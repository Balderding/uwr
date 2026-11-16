"""coreproof.py: the proofs of the rules core. The same operations go to UW.EXE's own code in the emulator (emu.py)
and to the core (underworld-exhumed's C built by tools/core.py, driven through build/coretest.exe), and every answer
must be the same: the bytes of the names read, the pointers (said as the level block or a name both sides have),
the routines' returns, Borland rand's state and the sequence of outside calls (the routines neither side has here:
on our side the stubs of build/gen/core_stubs.c, in the emulator a stand-in at each one's address that records its
name and returns 0). Names and their addresses come from underworld-exhumed's uw1/symbols.tsv.

    from coreproof import Pair
    p = Pair(game_dir, coretest_exe)
    p.level(3)                                 # LEV.ARK's level 3 block as the level (mapdata and the object store)
    p.w('PN', 0, data); p.call('do_physics', [('n', '@PN'), ('n', '@PT')])
    p.same('PN', 0, 40)                        # raises Differ with the context when the two differ
"""
import os
import re
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import DGROUP, EmuError, UWEmu  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOSTNAME = {'rand': 'bc_rand', 'srand': 'bc_srand'}  # the C library's names the port renames (compat.h)
SCRATCH = 0xE000  # S+: a scratch block both sides have (DS:E000 to E7FF; the near heap ends at E000)
MAPSEG = 0xC000  # where the level block is in the emulator (offset 0, as the store's pointers compare by offset)


class Differ(Exception):
    pass


def symbols():
    data, code = {}, {}
    for line in open(os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'symbols.tsv')):
        p = line.rstrip('\n').split('\t')
        if len(p) < 2 or not p[0].startswith('_'):
            continue
        n, a = p[0][1:], p[1]
        if a.startswith('DS:'):
            data[n] = int(a[3:], 16)
        elif ':' in a:
            s, o = a.split(':')
            code[n] = (int(s, 16), int(o, 16))
    return data, code


class Pair:
    def __init__(self, game, coretest, log_ops=False):
        self.game = game
        self.data, self.code = symbols()
        self.e = UWEmu(os.path.join(game, 'UW.EXE'))
        self.c = subprocess.Popen([coretest], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=open(os.path.join(ROOT, 'work', 'coretest_%d.err' % os.getpid()), 'w'), text=True, bufsize=1)
        self.ops = []
        self.limit = 20_000_000  # instructions a call may run in the emulator before it is taken to be stuck
        self.log_ops = log_ops
        tab = open(os.path.join(ROOT, 'build', 'gen', 'core_tab.c')).read()
        self.host = dict((m.group(1), m.group(2) == '1') for m in re.finditer(r'\{ "(\w+)", \(void \*\)\w+, (\d) \}', tab))
        stubs = [l.split() for l in open(os.path.join(ROOT, 'build', 'gen', 'core_stubs.txt')) if l.strip()]
        self.stub_fns = [n for k, n in stubs if k == 'f']
        self.stub_data = [n for k, n in stubs if k == 'v']
        sk = os.path.join(ROOT, 'build', 'gen', 'core_sinks.txt')  # the outside calls compared with their arguments
        self.sinks = dict(l.split() for l in open(sk) if l.strip()) if os.path.exists(sk) else {}
        # names both sides have, for saying where a pointer points
        self.known = sorted(n for n in self.host if not self.host[n] and n in self.data)
        self.ds_sorted = sorted((self.data[n], n) for n in self.known)
        self._cmd('known ' + ' '.join(self.known))
        self._cmd('exe ' + os.path.join(game, 'UW.EXE'))
        # the outside calls: a stand-in at each one's address in UW.EXE
        self.calls = []
        for n in self.stub_fns:
            if n in self.code:
                s, o = self.code[n]
                self.e.hook(s, o, self._outside(n))
        # the C library's heaps (the original start-up that sets them up is not run here): the near heap in DGROUP from
        # DS:A000 to DS:E000 (its data ends near 8000), the far heap in paragraphs C800 to F000 (the level block is at
        # C000), both handed out in order and never reused; freeing is a no-op, as the core's pointers are its own
        self.heap = [0xA000, 0xC800]

        def near_alloc(n, zero):
            a = self.heap[0]
            if a + n > 0xE000:
                return 0, 0
            self.heap[0] = (a + n + 15) & ~15
            if zero:
                self.e.write(DGROUP, a, b'\0' * n)
            return a, 0

        def far_alloc(n, zero):
            s = self.heap[1]
            paras = (n + 15) // 16 + 1
            if s + paras > 0xF000:
                return 0, 0
            self.heap[1] = s + paras
            if zero:
                self.e.write(s, 0, b'\0' * n)
            return 0, s
        for n, fn in (('malloc', lambda e: near_alloc(e.arg(0), False)),
                      ('calloc', lambda e: near_alloc(e.arg(0) * e.arg(1), True)),
                      ('free', lambda e: (0, 0)),
                      ('farmalloc', lambda e: far_alloc(e.arg(0) | e.arg(1) << 16, False)),
                      ('farcalloc', lambda e: far_alloc((e.arg(0) | e.arg(1) << 16) * (e.arg(2) | e.arg(3) << 16), True)),
                      ('farfree', lambda e: (0, 0))):
            if n in self.code:
                self.e.hook(*self.code[n], fn)
        # the mouse pointer the rules read (mouse_getxy: src/core_glue.c's core_mouse_x, core_mouse_y): set by mouse()
        self.mouse_xy = [0, 0]

        def getxy(e):
            e.ww(DGROUP, e.arg(0), self.mouse_xy[0])
            e.ww(DGROUP, e.arg(1), self.mouse_xy[1])
            return 0, 0
        if 'mouse_getxy' in self.code:
            self.e.hook(*self.code['mouse_getxy'], getxy)
        # the glue's silent stand-ins: the debug print (DEBUG.C, ovr106) prints nothing in the core; the inventory
        # panel's drawing routines return at once in the core (the front end draws its panel: tools/core.py PATCHES),
        # so the original's do the same here
        for n in ('dprintf', 'load_inventory_pix', 'DisplayInvSpecial', 'DisplayInvObject', 'DisplayInventory',
                  'displayInventoryArray', 'DisplayOpenBag'):
            if n in self.code:
                self.e.hook(*self.code[n], lambda e: (0, 0))
        # the stubs' data starts as UW.EXE's DGROUP has it (the core's own data starts so already: its C is matched)
        bounds = sorted(self.data.values())
        for n in self.stub_data:
            if n in self.data:
                a = self.data[n]
                nxt = next((b for b in bounds if b > a), a + 2)
                k = min(nxt - a, 0x1000)
                self._cmd('w %s 0 %s' % (n, self.e.read(DGROUP, a, k).hex()))
        # stub variables that are far pointers (the front end's: the keyboard state, colours, font, camera): the bytes
        # copied above would make a host pointer of nonsense, so each points at zeroes on both sides (in the emulator
        # DGROUP from E900, on our side 0x100 into the stub's own block); the stray object pointers are null
        for k, n in enumerate(('Alt', 'CapsLock', 'Ctrl', 'Shift', 'key_on', 'background_color', 'foreground_color',
                               'cLightTabs', 'cPerror', 'cExitMessage', 'cur_font', 'cPlayer')):
            if n in self.data and n in self.stub_data:
                self.e.write(DGROUP, 0xE900 + k * 0x40, b'\0' * 0x40)
                self.e.ww(DGROUP, self.addr(n), 0xE900 + k * 0x40)
                self.e.ww(DGROUP, self.addr(n) + 2, DGROUP)
                self._cmd('w %s 100 %s' % (n, '00' * 0x40))
                self._cmd('p %s 0 @%s+100' % (n, n))
        for n in ('ObjectActing', 'UsPtr', 'curelem'):
            if n in self.data and n in self.stub_data:
                self.p(n, 0, '0')
        self.ark = open(os.path.join(game, 'DATA', 'LEV.ARK'), 'rb').read()

    # --- the two sides ------------------------------------------------------------------------------------
    def _outside(self, name):
        kinds = self.sinks.get(name)

        def fn(e):
            if kinds is None:
                self.calls.append(name)
                return 0, 0
            # with its arguments, as src/core_glue.c's core_stub_logv writes them
            out, w = [], 0
            for k in kinds:
                if k in 'FN':
                    off, seg = e.arg(w), (e.arg(w + 1) if k == 'F' else DGROUP)
                    w += 2 if k == 'F' else 1
                    s = ''
                    for i in range(200):
                        c = e.rb(seg, off + i) if (off or seg) else 0
                        if not c:
                            break
                        s += '\\%02x' % c if c < 32 or c > 126 or chr(c) in '",;\\' else chr(c)
                    out.append('"%s"' % s)
                elif k == 'b':
                    out.append('%d' % (e.arg(w) & 0xff))
                    w += 1
                elif k == 'w':
                    v = e.arg(w)
                    out.append('%d' % (v - 0x10000 if v & 0x8000 else v))
                    w += 1
                else:
                    out.append('*')
                    w += 2
            self.calls.append('%s(%s)' % (name, ';'.join(out)))
            return 0, 0
        return fn

    def _cmd(self, s, reply=False):
        if self.log_ops:
            self.ops.append(s)
        self.c.stdin.write(s + '\n')
        self.c.stdin.flush()
        if reply:
            line = self.c.stdout.readline()
            if not line:
                raise Differ('coretest ended at: %s' % s)
            return line.rstrip('\n')
        return None

    def addr(self, name):
        if name not in self.data:
            raise KeyError('no DS address for %s' % name)
        return self.data[name]

    def _dos_ptr(self, spec):
        """a SPEC as the DOS (offset, segment)"""
        if spec in ('0', 0, None):
            return 0, 0
        if spec.startswith('L+'):
            return int(spec[2:], 16), MAPSEG
        if spec.startswith('S+'):  # the scratch both sides have: DS:E000 to E7FF (coretest.c's scratch)
            return SCRATCH + int(spec[2:], 16), DGROUP
        if spec.startswith('@'):
            n, _, off = spec[1:].partition('+')
            if n not in self.data and n in self.code:  # a function: its far address (a function pointer)
                s, o = self.code[n]
                return o, s
            return self.addr(n) + (int(off, 16) if off else 0), DGROUP
        raise ValueError(spec)

    def _spec_of(self, off, seg, near=False):
        if near:
            seg = 0 if off == 0 else DGROUP  # a near null is null
        if off == 0 and seg == 0:
            return '0'
        if seg == MAPSEG:
            return 'L+%x' % off
        if seg == DGROUP and SCRATCH <= off < SCRATCH + 0x800:
            return 'S+%x' % (off - SCRATCH)
        if seg == DGROUP:
            best = None
            for a, n in self.ds_sorted:
                if a <= off:
                    best = (a, n)
                else:
                    break
            if best:
                return '@%s+%x' % (best[1], off - best[0])
        lin = seg * 16 + off
        if MAPSEG * 16 <= lin < MAPSEG * 16 + 0x10000:
            return 'L+%x' % (lin - MAPSEG * 16)
        return '?'

    # --- operations ---------------------------------------------------------------------------------------
    def seed(self, v):
        self.e.set_seed(v)
        self._cmd('seed %x' % (v & 0xffffffff))

    def w(self, name, off, data, hoff=None):
        if name == 'L':
            self.e.write(MAPSEG, off, data)
            self._cmd('w L+0 %x %s' % (off, bytes(data).hex()))
        elif name == 'S':
            self.e.write(DGROUP, SCRATCH + off, data)
            self._cmd('w S+0 %x %s' % (off, bytes(data).hex()))
        else:
            self.e.write(DGROUP, self.addr(name) + off, data)
            self._cmd('w %s %x %s' % (name, off if hoff is None else hoff, bytes(data).hex()))

    def p(self, name, off, spec, near=False, hoff=None):
        """a pointer into name + off: far (4 bytes in DOS) unless near (2). The host's pointers are 8 bytes, so in a
        struct that holds pointers (HOST_LAYOUT_BEGIN in exhumed's headers: MotionParams, Handler, ...) a field after
        the first pointer is at another offset on the host: give it as hoff (r, w and q take hoff too)."""
        o, s = self._dos_ptr(spec)
        a = self.addr(name) + off
        self.e.ww(DGROUP, a, o)
        if not near:
            self.e.ww(DGROUP, a + 2, s)
        self._cmd('p %s %x %s' % (name, off if hoff is None else hoff, spec))

    def q(self, name, off=0, near=False, hoff=None):
        a = self.addr(name) + off
        o = self.e.rw(DGROUP, a)
        s = 0 if near else self.e.rw(DGROUP, a + 2)
        mine = self._spec_of(o, s, near)
        ours = self._cmd('q %s %x' % (name, off if hoff is None else hoff), True)
        return mine, ours

    def r(self, name, off, n, hoff=None):
        if name == 'L':
            mine = self.e.read(MAPSEG, off, n).hex()
            ours = self._cmd('r L+0 %x %x' % (off, n), True)
        elif name == 'S':
            mine = self.e.read(DGROUP, SCRATCH + off, n).hex()
            ours = self._cmd('r S+0 %x %x' % (off, n), True)
        else:
            mine = self.e.read(DGROUP, self.addr(name) + off, n).hex()
            ours = self._cmd('r %s %x %x' % (name, off if hoff is None else hoff, n), True)
        return mine, ours

    def call(self, name, args=(), ret='w'):
        """a call: args are ('w', int) a word, ('l', int) a long, ('n', SPEC) a near pointer, ('f', SPEC) a far one;
        ret is 'w' (AX), 'b' (AL), 'l' (DX:AX), 'f' or 'n' a pointer, or None. Returns (original, ours)."""
        words, host = [], []
        for k, v in args:
            if k == 'w':
                words.append(v & 0xffff)
                host.append(str(v))
            elif k == 'l':
                words += [v & 0xffff, (v >> 16) & 0xffff]
                host.append(str(v))
            elif k == 'n':
                words.append(self._dos_ptr(v)[0])
                host.append(v)
            elif k == 'f':
                o, s = self._dos_ptr(v)
                words += [o, s]
                host.append(v)
        s, o = self.code[name]
        self.calls = []
        # what a routine finds in its uninitialised locals is the stack's old contents: zero on both sides (the
        # emulator's stack, DS:EC00 up to its top, here; the core's locals that DOS left unset, by tools/core.py's
        # PATCHES)
        self.e.write(DGROUP, 0xEC00, b'\0' * (0xFFF0 - 0xEC00))
        try:
            ax, dx = self.e.call(s, o, words, limit=self.limit)
        except EmuError as ex:
            raise Differ('the original %s(%s) did not finish: %s' % (name, ', '.join(host), ex))
        mine_calls = 'calls %d%s' % (len(self.calls), (' ' + ','.join(self.calls)) if self.calls else '')
        ours_ret = self._cmd('%s %s %s' % ('callp' if ret in ('f', 'n') else 'call', HOSTNAME.get(name, name), ' '.join(host)), True)
        ours_calls = self.c.stdout.readline().rstrip('\n')
        if ret in ('f', 'n'):
            mine_ret = 'ret ' + self._spec_of(ax, dx, ret == 'n')
        elif ret is None:
            mine_ret = ours_ret = 'ret -'
        else:
            v = int(ours_ret.split()[1], 16)
            if ret == 'w':
                mine_ret, ours_ret = 'ret %x' % ax, 'ret %x' % (v & 0xffff)
            elif ret == 'b':
                mine_ret, ours_ret = 'ret %x' % (ax & 0xff), 'ret %x' % (v & 0xff)
            else:
                mine_ret, ours_ret = 'ret %x' % (dx << 16 | ax), 'ret %x' % (v & 0xffffffff)
        return (mine_ret, mine_calls), (ours_ret, ours_calls)

    def coverage(self):
        """start recording which of UW.EXE's routines run (every basic block, mapped to the routine it is in, resident
        or overlay); covered() then gives their names"""
        if getattr(self, '_cov', None) is not None:
            return
        import bisect
        starts = [(s * 16 + o, n) for n, (s, o) in self.code.items()]
        # an overlay routine runs at its loaded paragraph: the stub entry's name for the overlay's code there
        ent = dict(((s * 16 + o), n) for n, (s, o) in self.code.items())
        for stub, (fo, cs, rs, ents) in self.e.stubs.items():
            para = self.e.loaded.get(stub)
            if para is None:
                continue
            for k, off in enumerate(ents):
                n = ent.get(stub * 16 + 0x20 + 5 * k)
                if n:
                    starts.append((para * 16 + off, n))
        starts.sort()
        keys = [a for a, _ in starts]
        names = [n for _, n in starts]
        self._cov = set()
        blocks = set()

        def blk(uc, addr, size, user):
            if addr not in blocks:
                blocks.add(addr)
                i = bisect.bisect_right(keys, addr) - 1
                if i >= 0:
                    self._cov.add(names[i])
        self.e.uc.hook_add(1 << 3, blk)  # UC_HOOK_BLOCK

    def trace(self, name, args=(), ret='w'):
        """the call on both sides with the routines each entered, in order: (original's names, ours). Ours needs the
        traced harness (build\\coretest_instr.exe: CORE_INSTR=1 tools/core.py). Names only the other side knows are
        left out of both."""
        import bisect
        stubs = set(st * 16 + 0x20 + 5 * k for st, (fo, cs, rs, ents) in self.e.stubs.items() for k in range(len(ents)))
        entry = {}
        for n, (s, o) in self.code.items():
            a = s * 16 + o
            if a not in stubs:
                entry[a] = n
        byname = dict((v, k) for k, v in entry.items())
        for stub, (fo, cs, rs, ents) in self.e.stubs.items():
            para = self.e.loaded.get(stub)
            for k, off in enumerate(ents):
                for n, (s, o) in self.code.items():
                    if s * 16 + o == stub * 16 + 0x20 + 5 * k:
                        entry[para * 16 + off] = n
        seq = []

        def blk(uc, addr, size, user):
            n = entry.get(addr)
            if n:
                seq.append(n)
        h = self.e.uc.hook_add(1 << 3, blk)
        self.e.uc.ctl_flush_tb()  # a hook added now reaches code already translated only after a flush
        self._cmd('traceon')
        res = self.call(name, args, ret)
        self.e.uc.hook_del(h)
        ours = self._cmd('traceoff', True).split()[2:]
        self.raw_trace = (seq, ours)
        both = set(seq) & set(ours)
        return [n for n in seq if n in both], [n for n in ours if n in both], res

    def covered(self):
        return set(getattr(self, '_cov', None) or ())

    def ran(self, routines):
        """which of a file's routines ran in the original during the proof: (ran, not ran, not in symbols.tsv)"""
        cov = self.covered()
        known = [r for r in routines if r in self.code]
        ran = [r for r in known if r in cov]
        # for tools/proved.py: the routines this proof ran, by the script that ran them
        script = os.path.basename(sys.argv[0]) if sys.argv and sys.argv[0] else 'proof'
        os.makedirs(os.path.join(ROOT, 'work', 'ran'), exist_ok=True)
        open(os.path.join(ROOT, 'work', 'ran', script + '.txt'), 'w').write('\n'.join(ran) + '\n')
        return ran, [r for r in known if r not in cov], [r for r in routines if r not in self.code]

    def time(self, t):
        """the game clock (GAME_TIME(): the original reads *Time, a far pointer to TICKS.ASM's 1/256 s count; the core
        rp_time(), src/core_glue.c's core_time)"""
        self.e.ww(DGROUP, self.addr('Time'), 0xE800)
        self.e.ww(DGROUP, self.addr('Time') + 2, DGROUP)
        self.e.write(DGROUP, 0xE800, struct.pack('<I', t & 0xffffffff))
        self._cmd('w core_time 0 %s' % struct.pack('<I', t & 0xffffffff).hex())

    def mouse(self, x, y):
        """the mouse pointer's screen position (320 x 200) as mouse_getxy gives it on both sides"""
        self.mouse_xy = [x & 0xffff, y & 0xffff]
        self._cmd('w core_mouse_x 0 %s' % struct.pack('<h', x).hex())
        self._cmd('w core_mouse_y 0 %s' % struct.pack('<h', y).hex())

    def gseed(self):
        return '%08x' % self.e.seed(), self._cmd('gseed', True)

    # --- checks ---------------------------------------------------------------------------------------------
    def same(self, what, pair, ctx=''):
        a, b = pair
        if a != b:
            raise Differ('%s differs%s:\n  original %s\n  ours     %s' % (what, (' (' + ctx + ')') if ctx else '',
                                                                       str(a)[:600], str(b)[:600]))

    def same_bytes(self, name, off, n, ctx='', hoff=None):
        a, b = self.r(name, off, n, hoff)
        if a != b:
            i = next(k for k in range(0, len(a), 2) if a[k:k + 2] != b[k:k + 2]) // 2
            raise Differ('%s+%x differs at byte %x%s:\n  original %s\n  ours     %s' % (
                name, off, off + i, (' (' + ctx + ')') if ctx else '', a[max(0, i * 2 - 16):i * 2 + 48],
                b[max(0, i * 2 - 16):i * 2 + 48]))

    # --- files (opt-in) ---------------------------------------------------------------------------------------
    def files(self):
        """the game's files for both sides, read from the game folder; what each side writes goes to a folder of its
        own (work/files_emu, work/files_core) and is read back from there first, as src/core_glue.c does. In the
        emulator Borland's fopen, fread, fwrite, fseek, ftell, fclose, open, read, write, lseek, close and access are
        stood in for here (DOS would be needed for them); a FILE is a fake near pointer."""
        emu_dir = os.path.join(ROOT, 'work', 'files_emu')
        core_dir = os.path.join(ROOT, 'work', 'files_core')
        os.makedirs(emu_dir, exist_ok=True)
        os.makedirs(core_dir, exist_ok=True)
        self.write_dirs = (emu_dir, core_dir)
        self._cmd('dirs %s %s' % (self.game.replace('\\', '/'), core_dir.replace('\\', '/')))
        e, game = self.e, self.game
        opened = {}  # handle -> python file (FILE pointers and handles alike)

        def cstr(off):
            s = b''
            while len(s) < 200:
                c = e.rb(DGROUP, off + len(s))
                if not c:
                    break
                s += bytes([c])
            return s.decode('latin-1').replace('\\', '/')

        def host_open(path, mode):
            writing = mode[0] != 'r' or '+' in mode
            for d in ([emu_dir] if writing else [emu_dir, game]):
                try:
                    return open(os.path.join(d, path), mode)
                except OSError:
                    pass
            return None

        def new_handle(f, first):
            k = first
            while k in opened:
                k += 1
            opened[k] = f
            return k

        def fopen(e):
            path, mode = cstr(e.arg(0)), cstr(e.arg(1)).replace('t', '')
            mode = mode if 'b' in mode else mode + 'b'
            f = host_open(path, mode)
            return (0, 0) if f is None else (new_handle(f, 0xE100) * 1, 0)

        def fread(e):
            buf, size, n, fp = e.arg(0), e.arg(1), e.arg(2), e.arg(3)
            f = opened.get(fp)
            if not f or not size:
                return 0, 0
            data = f.read(size * n)
            e.write(DGROUP, buf, data)
            return len(data) // size, 0

        def fwrite(e):
            buf, size, n, fp = e.arg(0), e.arg(1), e.arg(2), e.arg(3)
            f = opened.get(fp)
            if not f:
                return 0, 0
            f.write(e.read(DGROUP, buf, size * n))
            return n, 0

        def fseek(e):
            fp, off, whence = e.arg(0), e.arg(1) | e.arg(2) << 16, e.arg(3)
            f = opened.get(fp)
            if not f:
                return 0xffff, 0xffff
            f.seek(off - (1 << 32) if off & 0x80000000 else off, whence)
            return 0, 0

        def ftell(e):
            f = opened.get(e.arg(0))
            t = f.tell() if f else -1
            return t & 0xffff, (t >> 16) & 0xffff

        def fgetc(e):
            f = opened.get(e.arg(0))
            c = f.read(1) if f else b''
            return (c[0], 0) if c else (0xffff, 0xffff)

        def fgets(e):
            buf, n, fp = e.arg(0), e.arg(1), e.arg(2)
            f = opened.get(fp)
            if not f or n < 1:
                return 0, 0
            line = b''
            while len(line) < n - 1:
                c = f.read(1)
                if not c:
                    break
                line += c
                if c == b'\n':
                    break
            if not line:
                return 0, 0
            e.write(DGROUP, buf, line + b'\0')
            return buf, 0

        def fclose(e):
            f = opened.pop(e.arg(0), None)
            if f:
                f.close()
            return 0, 0

        def dopen(e):
            path, acc = cstr(e.arg(0)), e.arg(1)
            mode = 'w+b' if acc & 0x200 else 'r+b' if acc & 6 else 'rb'
            f = host_open(path, mode)
            if f is None and acc & 0x100:
                f = host_open(path, 'w+b')
            return (0xffff, 0xffff) if f is None else (new_handle(f, 5), 0)

        def dread(e):
            fd, buf, n = e.arg(0), e.arg(1), e.arg(2)
            f = opened.get(fd)
            if not f:
                return 0xffff, 0xffff
            data = f.read(n)
            e.write(DGROUP, buf, data)
            return len(data), 0

        def dwrite(e):
            fd, buf, n = e.arg(0), e.arg(1), e.arg(2)
            f = opened.get(fd)
            if not f:
                return 0xffff, 0xffff
            f.write(e.read(DGROUP, buf, n))
            return n, 0

        def dlseek(e):
            fd, off, whence = e.arg(0), e.arg(1) | e.arg(2) << 16, e.arg(3)
            f = opened.get(fd)
            if not f:
                return 0xffff, 0xffff
            f.seek(off - (1 << 32) if off & 0x80000000 else off, whence)
            t = f.tell()
            return t & 0xffff, (t >> 16) & 0xffff

        def access(e):
            f = host_open(cstr(e.arg(0)), 'rb')
            if f:
                f.close()
            return (0, 0) if f else (0xffff, 0xffff)

        def intdosx(e):
            # DOS read (3Fh) and write (40h) for a far buffer (MISCUTIL.C's loaders): REGS are ax bx cx dx si di
            # cflag flags, SREGS es cs ss ds
            rin, rout, sr = e.arg(0), e.arg(1), e.arg(2)
            ax, bx, cx, dx = struct.unpack('<4H', e.read(DGROUP, rin, 8))
            ds = e.rw(DGROUP, sr + 6)
            f = opened.get(bx)
            r = -1
            if f and ax >> 8 == 0x3F:
                data = f.read(cx)
                e.write(ds, dx, data)
                r = len(data)
            elif f and ax >> 8 == 0x40:
                f.write(e.read(ds, dx, cx))
                r = cx
            e.write(DGROUP, rout, e.read(DGROUP, rin, 16))
            e.ww(DGROUP, rout, 6 if r < 0 else r)
            e.ww(DGROUP, rout + 12, 1 if r < 0 else 0)
            return (6 if r < 0 else r), 0
        for n, fn in (('fopen', fopen), ('fread', fread), ('fwrite', fwrite), ('fseek', fseek), ('ftell', ftell),
                      ('fclose', fclose), ('open', dopen), ('read', dread), ('write', dwrite), ('lseek', dlseek),
                      ('close', fclose), ('access', access), ('intdosx', intdosx), ('fgetc', fgetc), ('fgets', fgets)):
            if n in self.code:
                e.hook(*self.code[n], fn)

    # --- the level --------------------------------------------------------------------------------------------
    def level(self, n, data=None):
        """level n's block from LEV.ARK (or the given bytes) as the level: mapdata, critdata and objdata at it, the
        free stacks and the active list from its counts (MAP.C's Map_Load), the floors' terrain (TxmTerr)"""
        lb, = struct.unpack_from('<I', self.ark, 2 + (n - 1) * 4)
        block = data if data is not None else self.ark[lb:lb + 0x7c08]
        self.w('L', 0, block)
        self.p('mapdata', 0, 'L+0')
        self.p('critdata', 0, 'L+4000')
        self.p('objdata', 0, 'L+5b00')
        nact, nmob, nsta = struct.unpack_from('<HHH', block, 0x7c00)
        self.p('critbot', 0, 'L+7300')
        self.p('crittop', 0, 'L+%x' % (0x7300 + 0xfd * 2))
        self.p('critptr', 0, 'L+%x' % (0x7300 + nmob * 2))
        self.p('objbot', 0, 'L+74fc')
        self.p('objtop', 0, 'L+%x' % (0x74fc + 0x2ff * 2))
        self.p('objptr', 0, 'L+%x' % (0x74fc + nsta * 2))
        self.p('ActiveMob', 0, 'L+7afc')
        self.p('LastActiveMob', 0, 'L+%x' % (0x7afc + nact))
        self.p('ThePlayer', 0, 'L+%x' % (0x4000 + 27))
        tb, = struct.unpack_from('<I', self.ark, 2 + (17 + n) * 4)
        ter = open(os.path.join(self.game, 'DATA', 'TERRAIN.DAT'), 'rb').read()
        tx = b''.join(ter[0x200 + struct.unpack_from('<H', self.ark, tb + 96 + i * 2)[0] * 2:][:2] for i in range(10))
        self.w('TxmTerr', 0, tx)
        com = open(os.path.join(self.game, 'DATA', 'COMOBJ.DAT'), 'rb').read()
        self.w('ComObjData', 0, com[2:2 + 512 * 11])
        return block
