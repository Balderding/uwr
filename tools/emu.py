"""emu.py: run UW.EXE's own routines, one call at a time, in a CPU emulator (Unicorn, 16-bit real mode) -- no DOS.

    from emu import UWEmu
    e = UWEmu('game/UW.EXE')
    e.set_seed(0x1234)
    ax, dx = e.call(0x35bd, 0x11e, [3, 6])        # far call seg:off with word arguments (Borland: pushed last first)

The load image is placed at linear 0 (so its segment numbers need no relocation: they are the image's own); DS = SS
= DGROUP (0x5aac, as the Borland start-up leaves them; the stack at its top). The VROOMM overlays: a far call into a
stub segment reaches its "int 3Fh"; here that loads the overlay (once) into memory above the image, applies its
fixups (the words after its code: each an offset in the overlay to a segment selector, the segment table's index x 8,
replaced by that entry's segment) and jumps to the routine. Any other interrupt stops the run (the routine touches
DOS or the hardware). hook(seg, off, fn): fn(emu) stands in for a routine (it gets the arguments with emu.arg(n) and
returns AX, or (AX, DX)).
"""
import struct

from unicorn import UC_ARCH_X86, UC_HOOK_CODE, UC_HOOK_INTR, UC_HOOK_MEM_UNMAPPED, UC_MODE_16, Uc, UcError
from unicorn.x86_const import (UC_X86_REG_AX, UC_X86_REG_BP, UC_X86_REG_BX, UC_X86_REG_CS, UC_X86_REG_CX,
                               UC_X86_REG_DI, UC_X86_REG_DS, UC_X86_REG_DX, UC_X86_REG_ES, UC_X86_REG_IP,
                               UC_X86_REG_SI, UC_X86_REG_SP, UC_X86_REG_SS)

IMG, OVL_BASE = 0x3200, 0x65710
DGROUP = 0x5AAC
SENT_SEG = 0xF000  # a far return here ends a call (a hlt at F000:0000)


class EmuError(Exception):
    pass


class UWEmu:
    def __init__(self, exe):
        self.d = d = open(exe, 'rb').read()
        hdr_paras, = struct.unpack('<H', d[8:10])
        last, pages = struct.unpack('<HH', d[2:6])
        end = pages * 512 - (512 - last if last else 0)
        self.img = d[hdr_paras * 16:end]
        fbov = d.index(b'FBOV')
        tab, count = struct.unpack('<IH', d[fbov + 8:fbov + 14])
        self.segs = [struct.unpack('<4H', d[tab + k * 8:tab + k * 8 + 8]) for k in range(count)]
        self.stubs = {}  # stub paragraph -> (overlay code file offset, code size, fixup bytes, [entry offsets])
        for seg, size, typ, _ in self.segs:
            if typ == 3:
                a = IMG + seg * 16
                fo, = struct.unpack('<I', d[a + 4:a + 8])
                cs, rs, ne = struct.unpack('<3H', d[a + 8:a + 14])
                ents = [struct.unpack('<H', d[a + 0x22 + k * 5:a + 0x24 + k * 5])[0] for k in range(ne)]
                self.stubs[seg] = (OVL_BASE + fo, cs, rs, ents)
        self.uc = Uc(UC_ARCH_X86, UC_MODE_16)
        self.uc.mem_map(0, 0x110000)
        self.reset_memory()
        self.hooks = {}  # linear address -> fn
        self.loaded = {}  # stub paragraph -> overlay paragraph
        self.next_para = 0x7000
        self.uc.hook_add(UC_HOOK_INTR, self._intr)
        self.uc.hook_add(UC_HOOK_CODE, self._code)
        self.uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._unmapped)
        self.pending = None
        self.messages = []  # (block, index) the routines printed, when the printer is hooked
        self.preload()

    def preload(self):
        """every overlay loaded now and each stub entry patched to a far jump to its routine, as the overlay manager
        leaves a stub once its overlay is in (the int 3Fh path, loading from inside a run, can bring Unicorn down)"""
        for stub, (fo, cs, rs, ents) in sorted(self.stubs.items()):
            para = self.overlay_para(stub)
            for k, off in enumerate(ents):
                self.uc.mem_write(stub * 16 + 0x20 + 5 * k, b'\xea' + struct.pack('<HH', off, para))

    # --- memory ---------------------------------------------------------------------------------------------
    def reset_memory(self):
        self.uc.mem_write(0, b'\0' * 0x110000)
        self.uc.mem_write(0, self.img)
        self.uc.mem_write(SENT_SEG * 16, b'\xf4')

    def rb(self, seg, off):
        return self.uc.mem_read(seg * 16 + off, 1)[0]

    def rw(self, seg, off):
        return struct.unpack('<H', bytes(self.uc.mem_read(seg * 16 + off, 2)))[0]

    def wb(self, seg, off, v):
        self.uc.mem_write(seg * 16 + off, bytes([v & 0xff]))

    def ww(self, seg, off, v):
        self.uc.mem_write(seg * 16 + off, struct.pack('<H', v & 0xffff))

    def write(self, seg, off, data):
        self.uc.mem_write(seg * 16 + off, bytes(data))

    def read(self, seg, off, n):
        return bytes(self.uc.mem_read(seg * 16 + off, n))

    def set_seed(self, seed):  # Borland rand's state, DS 0x207e (a dword)
        self.write(DGROUP, 0x207e, struct.pack('<I', seed & 0xffffffff))

    def seed(self):
        return struct.unpack('<I', self.read(DGROUP, 0x207e, 4))[0]

    def arg(self, n):  # in a hook: the n-th word argument of the far call (0 first)
        sp = self.uc.reg_read(UC_X86_REG_SP)
        ss = self.uc.reg_read(UC_X86_REG_SS)
        return self.rw(ss, sp + 4 + 2 * n)

    # --- overlays ---------------------------------------------------------------------------------------------
    def overlay_para(self, stub):
        if stub in self.loaded:
            return self.loaded[stub]
        fo, cs, rs, ents = self.stubs[stub]
        code = bytearray(self.d[fo:fo + cs])
        fix = struct.unpack('<%dH' % (rs // 2), self.d[fo + cs:fo + cs + rs])
        for at in fix:
            sel, = struct.unpack('<H', code[at:at + 2])
            if sel // 8 < len(self.segs):
                code[at:at + 2] = struct.pack('<H', self.segs[sel // 8][0])
        para = self.next_para
        self.next_para += (cs + 15) // 16 + 1
        if self.next_para >= SENT_SEG:
            raise EmuError('out of room for overlays')
        self.uc.mem_write(para * 16, bytes(code))
        self.loaded[stub] = para
        return para

    def entry_of(self, file_off):
        """the stub and its entry offset through which an overlay routine (a file offset) is called"""
        for stub, (fo, cs, rs, ents) in self.stubs.items():
            if fo <= file_off < fo + cs and (file_off - fo) in ents:
                return stub, 0x20 + 5 * ents.index(file_off - fo)
        return None

    # --- hooks ----------------------------------------------------------------------------------------------
    def hook(self, seg, off, fn):
        self.hooks[seg * 16 + off] = fn

    def hook_file(self, file_off, fn):
        """a stand-in for a routine given by its file offset (load image or an overlay's entry)"""
        if file_off < OVL_BASE:
            self.hooks[file_off - IMG] = fn
        else:
            st = self.entry_of(file_off)
            if not st:
                raise EmuError('%x is not an overlay entry' % file_off)
            self.hooks[st[0] * 16 + st[1]] = fn

    def _intr(self, uc, intno, user):
        cs = uc.reg_read(UC_X86_REG_CS)
        ip = uc.reg_read(UC_X86_REG_IP)
        if intno == 0x3f and cs in self.stubs:
            entry = ip - 2
            routine, = struct.unpack('<H', self.read(cs, entry + 2, 2))
            self.pending = ('overlay', cs, routine)  # loaded once the run has stopped (writing code from inside a
            uc.emu_stop()                             # callback while it runs can bring Unicorn down)
            return
        self.pending = ('error', 'int %02xh at %04x:%04x' % (intno, cs, ip))
        uc.emu_stop()

    def _code(self, uc, addr, size, user):
        if addr == SENT_SEG * 16:
            self.pending = ('done',)
            uc.emu_stop()
            return
        fn = self.hooks.get(addr)
        if fn is None:
            # a far call into a stub that has a stand-in (its entry): the entry's linear address
            return
        r = fn(self)
        ax, dx = (r if isinstance(r, tuple) else (r if r is not None else 0, self.uc.reg_read(UC_X86_REG_DX)))
        uc.reg_write(UC_X86_REG_AX, ax & 0xffff)
        uc.reg_write(UC_X86_REG_DX, dx & 0xffff)
        sp = uc.reg_read(UC_X86_REG_SP)
        ss = uc.reg_read(UC_X86_REG_SS)
        ip, cs = self.rw(ss, sp), self.rw(ss, sp + 2)
        uc.reg_write(UC_X86_REG_SP, sp + 4)
        self.pending = (cs, ip)
        uc.emu_stop()

    def _unmapped(self, uc, access, addr, size, value, user):
        self.pending = ('error', 'memory %x' % addr)
        return False

    # --- a call -----------------------------------------------------------------------------------------------
    def call(self, seg, off, args=(), limit=5_000_000):
        """a far call to seg:off with word arguments; returns (AX, DX)"""
        uc = self.uc
        sp = 0xFFF0
        words = [a & 0xffff for a in args]
        for w in reversed(words):
            sp -= 2
            self.ww(DGROUP, sp, w)
        sp -= 4
        self.ww(DGROUP, sp, 0)          # the return: F000:0000
        self.ww(DGROUP, sp + 2, SENT_SEG)
        for r, v in ((UC_X86_REG_SS, DGROUP), (UC_X86_REG_DS, DGROUP), (UC_X86_REG_ES, DGROUP), (UC_X86_REG_SP, sp),
                     (UC_X86_REG_BP, 0), (UC_X86_REG_SI, 0), (UC_X86_REG_DI, 0), (UC_X86_REG_AX, 0), (UC_X86_REG_BX, 0),
                     (UC_X86_REG_CX, 0), (UC_X86_REG_DX, 0)):
            uc.reg_write(r, v)
        cs, ip = seg, off
        steps = 0
        while True:
            uc.reg_write(UC_X86_REG_CS, cs)
            uc.reg_write(UC_X86_REG_IP, ip)
            self.pending = None
            # a stand-in at the very start (no instruction has run yet to trigger the hook)
            fn_addr = cs * 16 + ip
            if fn_addr in self.hooks:
                self._code(uc, fn_addr, 1, None)
            else:
                try:
                    uc.emu_start(cs * 16 + ip, 0x110000, count=limit)
                except UcError as ex:
                    raise EmuError('%s at %04x:%04x' % (ex, uc.reg_read(UC_X86_REG_CS), uc.reg_read(UC_X86_REG_IP)))
            p = self.pending
            if p is None:
                raise EmuError('the routine ran past %d instructions' % limit)
            if p[0] == 'done':
                return uc.reg_read(UC_X86_REG_AX), uc.reg_read(UC_X86_REG_DX)
            if p[0] == 'error':
                raise EmuError(p[1])
            if p[0] == 'overlay':
                p = (self.overlay_para(p[1]), p[2])
            cs, ip = p
            steps += 1
            if steps > 100000:
                raise EmuError('too many overlay / stand-in jumps')

    def call_file(self, file_off, args=(), seg=None):
        """a far call to a routine by its file offset: a load-image routine needs its segment (as the map names it)"""
        if file_off >= OVL_BASE:
            st = self.entry_of(file_off)
            if not st:
                raise EmuError('%x: not an overlay entry (only called near)' % file_off)
            return self.call(st[0], st[1], args)
        if seg is None:
            raise EmuError('a load-image routine needs its segment')
        return self.call(seg, file_off - IMG - seg * 16, args)
