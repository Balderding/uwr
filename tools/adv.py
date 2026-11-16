"""adv.py: run one of UW's Miles AIL 2 sound drivers (game/SOUND/*.ADV) in a CPU emulator and record everything it
sends to the sound hardware: the reference our C port of the driver must match byte for byte.

    from adv import Driver
    d = Driver('game/SOUND/MT32MPU.ADV')
    d.call(102, 0x330, 2, 1, 1)          # AIL_init_driver(IO, IRQ, DMA, DRQ) (the handle is passed for you)
    d.out                                 # [(tick, port, value), ...]

A driver is a flat binary loaded at offset 0 of its own segment; its function table (after the copyright line and
0x1A) pairs AIL function numbers with offsets. AIL's call_driver jumps into a function with the caller's far frame,
so a function finds the handle at [bp+6] and its arguments after it (cdecl). An MPU-401 is modelled as always
ready: status port 0x331 reads 0x80 (nothing to read) except right after a command, when the 0xFE acknowledgement
waits at 0x330. Port writes are recorded with the tick (the number of AIL_serve_driver calls so far).
"""
import struct

from unicorn import UC_ARCH_X86, UC_HOOK_CODE, UC_HOOK_INSN, UC_HOOK_INTR, UC_MODE_16, Uc, UcError
from unicorn.x86_const import (UC_X86_INS_IN, UC_X86_INS_OUT, UC_X86_REG_AX, UC_X86_REG_BP, UC_X86_REG_BX,
                               UC_X86_REG_CS, UC_X86_REG_CX, UC_X86_REG_DI, UC_X86_REG_DS, UC_X86_REG_DX,
                               UC_X86_REG_ES, UC_X86_REG_IP, UC_X86_REG_SI, UC_X86_REG_SP, UC_X86_REG_SS)

DRV_SEG = 0x1000   # the driver
STACK_SEG = 0x2000  # the caller's stack
DATA_SEG = 0x3000   # memory for the caller's buffers (XMI files, state tables, timbre cache): 0x30000-0x8ffff
SENT_SEG = 0xF000


class Driver:
    def __init__(self, path):
        self.code = open(path, 'rb').read()
        i = self.code.index(b'\x1a') + 1
        self.funcs = {}
        while i + 4 <= len(self.code):
            fn, off = struct.unpack('<HH', self.code[i:i + 4])
            i += 4
            if fn in (0, 0xffff):
                break
            self.funcs[fn] = off
        self.uc = Uc(UC_ARCH_X86, UC_MODE_16)
        self.uc.mem_map(0, 0x110000)
        self.uc.mem_write(DRV_SEG * 16, self.code)
        self.uc.mem_write(SENT_SEG * 16, b'\xf4')
        self.uc.mem_write(0x463, struct.pack('<H', 0x3D4))  # BIOS data: the CRTC at 0x3D4 (colour), status 0x3DA
        self.retrace = 0
        self.uc.hook_add(UC_HOOK_INSN, self._in, None, 1, 0, UC_X86_INS_IN)
        self.uc.hook_add(UC_HOOK_INSN, self._out, None, 1, 0, UC_X86_INS_OUT)
        self.uc.hook_add(UC_HOOK_INTR, self._intr)
        self.out = []
        self.tick = 0
        self.mpu_ack = 0
        self.free = DATA_SEG * 16
        self.done = False
        self.uc.hook_add(UC_HOOK_CODE, self._code, None, SENT_SEG * 16, SENT_SEG * 16)

    # memory for buffers the caller passes: returns (segment, offset 0)
    def alloc(self, data_or_size):
        data = data_or_size if isinstance(data_or_size, (bytes, bytearray)) else b'\0' * data_or_size
        para = (self.free + 15) // 16
        self.uc.mem_write(para * 16, bytes(data))
        self.free = para * 16 + len(data) + 16
        return para

    def read(self, seg, off, n):
        return bytes(self.uc.mem_read(seg * 16 + off, n))

    def _in(self, uc, port, size, user):
        if port == 0x331:
            if self.mpu_ack:
                return 0x00  # data waiting (the acknowledgement); ready to receive
            return 0x80  # nothing to read; ready to receive
        if port == 0x3DA:  # vertical retrace: bit 3 flips on every read; each rising edge is logged ('vr')
            self.retrace ^= 8
            if self.retrace:
                self.out.append((self.tick, 'vr', 0))
            return self.retrace
        if port == 0x330:
            if self.mpu_ack:
                self.mpu_ack -= 1
                return 0xFE
            return 0xFE
        return 0

    def _out(self, uc, port, size, value, user):
        self.out.append((self.tick, port, value & 0xff))
        if port == 0x331:
            self.mpu_ack = 1

    def _intr(self, uc, intno, user):
        raise RuntimeError('the driver used int %02xh' % intno)

    def _code(self, uc, addr, size, user):
        self.done = True
        uc.emu_stop()

    def call(self, fn, *args, limit=20_000_000):
        """AIL function fn with (handle = 0, args...) as words; far pointers are given as (seg, off) tuples"""
        words = [0]  # the driver handle
        for a in args:
            if isinstance(a, tuple):
                words += [a[1], a[0]]  # off, seg (a far pointer: offset at the lower address)
            else:
                words.append(a & 0xffff)
        uc = self.uc
        sp = 0xFFF0
        for w in reversed(words):
            sp -= 2
            uc.mem_write(STACK_SEG * 16 + sp, struct.pack('<H', w))
        sp -= 4
        uc.mem_write(STACK_SEG * 16 + sp, struct.pack('<HH', 0, SENT_SEG))
        for r, v in ((UC_X86_REG_SS, STACK_SEG), (UC_X86_REG_SP, sp), (UC_X86_REG_DS, STACK_SEG), (UC_X86_REG_ES, STACK_SEG),
                     (UC_X86_REG_AX, fn), (UC_X86_REG_BX, 0), (UC_X86_REG_CX, 0), (UC_X86_REG_DX, 0), (UC_X86_REG_SI, 0),
                     (UC_X86_REG_DI, 0), (UC_X86_REG_BP, 0), (UC_X86_REG_CS, DRV_SEG)):
            uc.reg_write(r, v)
        self.done = False
        try:
            uc.emu_start(DRV_SEG * 16 + self.funcs[fn], 0x110000, count=limit)
        except UcError as ex:
            raise RuntimeError('%s in function %d at %04x:%04x' % (ex, fn, uc.reg_read(UC_X86_REG_CS), uc.reg_read(UC_X86_REG_IP)))
        if not self.done:
            raise RuntimeError('function %d did not return within %d instructions' % (fn, limit))
        return uc.reg_read(UC_X86_REG_AX), uc.reg_read(UC_X86_REG_DX)

    def serve(self, n=1):
        for _ in range(n):
            self.call(103)
            self.tick += 1


def gtl_index(path):
    """an AIL Global Timbre Library: (patch, bank) -> the timbre's bytes (its length word first)"""
    d = open(path, 'rb').read()
    out, i = {}, 0
    while i + 6 <= len(d):
        p, b = d[i], d[i + 1]
        if b == 0xff:
            break
        off, = struct.unpack('<I', d[i + 2:i + 6])
        n, = struct.unpack('<H', d[off:off + 2])
        out[(p, b)] = d[off:off + n]
        i += 6
    return out
