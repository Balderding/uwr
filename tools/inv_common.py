"""inv_common.py: the common ground of the inventory / player / map / system proofs (proof_bags, proof_invdata, ...).
It wraps coreproof.Pair and adds:
  - build_exe(): build/coretest_inv.exe, the proofs' own copy of tests/coretest.c with two more commands (qn: a pointer as
    a number, rm: bytes at an address) so the open bags (farmalloc'd records) can be read on our side; rebuilt when the
    core is newer. (tests/coretest.c itself is not touched.)
  - Inv: a Pair with the game's files, the class tables, the strings and the player set up; objects made on both sides by
    the proven Obj_Alloc / Obj_AddEnd; random but sound inventories; and check(): the return, the outside calls, rand,
    the whole level block, the player record, the inventory slots, the cursor object, the open bags (walked on both
    sides), and every global both sides have (raw bytes, up to 0x100 each, minus the pointer variables).
"""
import os
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)
import random
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair, ROOT, MAPSEG  # noqa: E402
from emu import DGROUP  # noqa: E402

SLOT_TO_DISPLAY = [2, 3, 4, 1, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                   12, 13, 14, 15, 16, 17, 18, 19]
DISPLAY_TO_SLOT = [1, 3, 0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]
PLAYERDAT = 0xD2


def build_exe():
    """build/coretest_inv.exe from work/coretest_inv.c and the current core (when stale); returns its path"""
    exe = os.path.join(ROOT, 'build', 'coretest_inv.exe')
    src = os.path.join(ROOT, 'work', 'coretest_inv.c')
    deps = [src, os.path.join(ROOT, 'build', 'core', 'core.a'), os.path.join(ROOT, 'build', 'gen', 'core_tab.c'),
            os.path.join(ROOT, 'build', 'gen', 'core_stubs.c')]
    if os.path.exists(exe) and all(os.path.getmtime(exe) >= os.path.getmtime(d) for d in deps):
        return exe
    env = dict(os.environ)
    env['PATH'] = TOOLS + r'\llvm\23.1.4\bin;' + env['PATH']
    env['TMP'] = env['TEMP'] = env['TMPDIR'] = TOOLS + r'\llvm\tmp'
    new = exe + '.new'
    cmd = [TOOLS + r'\llvm\23.1.4\bin\clang.exe','--target=x86_64-w64-windows-gnu', '--sysroot=' + TOOLS.replace('\\', '/') + '/gcc/mingw64', '-fuse-ld=lld', '-O1', '-w',
           '-o', new, src, deps[2], deps[3], deps[1]]
    r = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('build of coretest_inv.exe failed: ' + r.stderr[:2000])
    try:
        os.replace(new, exe)
    except OSError:  # in use by another run: take it under its own name
        exe = exe[:-4] + '_%d.exe' % os.getpid()
        os.replace(new, exe)
    return exe


def u16(b, o):
    return struct.unpack_from('<H', b, o)[0]


def obj_off(i):
    return 0x4000 + i * 27 if i < 256 else 0x5b00 + (i - 256) * 8


class ScenarioDone(Exception):
    """a call left the carried objects in a state the game never has (a cycle, an object twice): the call itself was compared
    equal; the scenario ends"""


class Inv:
    # variables whose bytes are not comparable raw (pointers: 4 bytes in DOS, 8 on the host; far data both sides keep by
    # other means) or that are the harness's own; found by the first runs
    PTRS = {'CursorObjPtr': False, 'ActiveObj': False, 'centptr': False, 'newPlObj': False, 'CP': True, 'Obj_Find_Head': False,
            'meptr': False, 'mytarget': False, 'collobject': False, 'mycst': True, 'tp_act': True, 'pn_act': True, 'UsPtr': False,
            'ObjectActing': False}  # name: near?
    SIZES = {'BagSaveHandles': 16, 'level': 2, 'invArmorObj': 6, 'invArmorQ': 6, 'PlayerDat': 0xD2, 'PT': 8, 'CT1': 8, 'CT2': 8, 'CT3': 8, 'CT4': 8}  # real sizes of the names the headers do not declare
    FNPTRS = {'PT@8': False, 'ObjectActor': False, 'curP': True, 'TP': True, 'mous_dispatch': True, 'key_dispatch': True}  # function pointers and pointers into the front end's data: compared as set / not set (name: near?)
    SKIP = set()

    def __init__(self, game, seed, name, per_name_limit=0x100, extra_files=True):
        self.game = game
        self.name = name
        exe = build_exe()
        self.p = Pair(game, exe)
        self.rng = random.Random(seed + int(os.environ.get('PROOF_SEED', '0')) * 1000003)
        self.counts = {}
        self.p.limit = 60_000_000
        self.ctx = ''
        p = self.p
        p.coverage()
        if extra_files:
            p.files()
            p.same('init_objects', p.call('init_objects', [], 'w'))
            p.same('init_strings', p.call('init_strings', [], 'b'))
        self.com = open(os.path.join(game, 'DATA', 'COMOBJ.DAT'), 'rb').read()
        self.pickup = [i for i in range(512) if (self.com[2 + i * 11 + 3] >> 5) & 1]
        self.pending = []
        # the panel's drawing routines are stood in for at their stub entries (Pair); a call from another routine of the
        # same overlay goes to the loaded code and not through the stub, so the stand-ins follow the overlay in
        names = ('load_inventory_pix', 'DisplayInvSpecial', 'DisplayInvObject', 'DisplayInventory', 'displayInventoryArray',
                 'DisplayOpenBag')
        self._ovl_hooked = set()
        # DOS file calls the core has as silent stubs (bc_unlink, bc_rename, bc_mkdir, bc_chdir: not logged on the original's
        # side either, as Pair names stubs by the core's names): the original's would call DOS, which the emulator does not
        # have. They do nothing on both sides (so a file put_arc moves to a temporary name stays where it was)
        for n in ('unlink', 'rename', 'mkdir', 'chdir', 'rmdir'):
            if n in p.code:
                p.e.hook(*p.code[n], lambda e_: (0, 0))
        # realloc (INPUT.C's dispatch tables): Pair stands in for malloc, calloc and free but not realloc, whose original runs on
        # the DOS heap bookkeeping the emulator does not have (it writes into DGROUP): a new block from the same near heap, the
        # old bytes copied over
        if 'realloc' in p.code:
            def realloc(e_):
                old, n = e_.arg(0), e_.arg(1)
                if n == 0:
                    return 0, 0
                sizes = p.__dict__.setdefault('_ra_sizes', {})
                if old in sizes and (old + sizes[old] + 15) & ~15 == p.heap[0]:  # the last block: grown in place
                    if old + n > 0xE000:
                        return 0, 0
                    p.heap[0] = (old + n + 15) & ~15
                    sizes[old] = n
                    return old, 0
                a = p.heap[0]
                if a + n > 0xE000:
                    return 0, 0
                p.heap[0] = (a + n + 15) & ~15
                sizes[a] = n
                if old:
                    e_.write(DGROUP, a, e_.read(DGROUP, old, min(n, sizes.get(old, n), max(0, 0xE000 - old))))
                return a, 0
            p.e.hook(*p.code["realloc"], realloc)
        oc = p.call

        def call(name, args=(), ret='w', _oc=oc):
            self.overlay_hooks(names)
            if self.trace:
                print('   call', name, args, flush=True)
            r = _oc(name, args, ret)
            return (r[0], self.drop_port(r[1]))
        p.call = call
        # the raw global comparison: every name both sides have, the bytes up to the next symbol (at most the limit)
        ds = sorted((a, n) for n, a in p.data.items())
        sizes = {}
        import json
        jp = os.path.join(ROOT, 'work', 'inv_sizes.json')
        if not os.path.exists(jp):
            import inv_sizes
            inv_sizes.sizes()
        real = json.load(open(jp))
        for i, (a, n) in enumerate(ds):
            nxt = ds[i + 1][0] if i + 1 < len(ds) else a + 2
            sizes[n] = min(real.get(n, 2), nxt - a, per_name_limit)  # a name the headers do not declare: a word (SIZES has the others)
        sizes.update(self.SIZES)
        self.SKIP = set(self.PTRS) | set(k.partition('@')[0] for k in self.FNPTRS) | {'OpenBag', 'OpenBagList', 'Inventory'}  # the bags: Inv.bags() walks them
        self.ptrnames = []
        self.gnames = [(n, sizes[n]) for n in p.known if n in sizes and sizes[n] > 0 and n not in self.SKIP]
        self.trace = bool(os.environ.get('PROOF_TRACE'))
        self.diag = []

    @staticmethod
    def drop_port(ours):
        """the core's outside-call list without the port's own additions (port_*: the modern-keys and mouse-look hooks the
        port adds to the rules, which the original does not have) and the silent file stubs"""
        ret, calls = ours
        parts = calls.split(' ', 2)
        if len(parts) < 3:
            return ours
        names = [c for c in parts[2].split(',') if not c.startswith('port_') and c not in ('bc_unlink', 'bc_rename', 'bc_mkdir', 'bc_chdir')]
        return (ret, 'calls %d%s' % (len(names), (' ' + ','.join(names)) if names else ''))

    def overlay_hooks(self, names):
        p = self.p
        e = p.e
        for n in names:
            if n not in p.code:
                continue
            s, o = p.code[n]
            st = e.stubs.get(s)
            if not st:
                continue
            k = (o - 0x20) // 5
            para = e.loaded.get(s)
            if para is None:
                continue
            addr = para * 16 + st[3][k]
            if (addr, n) not in self._ovl_hooked:
                self._ovl_hooked.add((addr, n))
                e.hooks[addr] = lambda e_: (0, 0)

    # ---- the world --------------------------------------------------------------------------------------------
    def has(self, n):
        return n in self.p.data and n in self.p.host

    def setw(self, name, v, fmt='<h', off=0):
        if self.has(name):
            self.p.w(name, off, struct.pack(fmt, v))

    def level(self, n, scrmode=None):
        p = self.p
        self.lev = n
        block = p.level(n)
        self.setw('PlayerLevel', n)
        p.p('player', 0, '@PlayerDat', near=True)
        p.p('playerdat', 0, '@Creature+%x' % (63 * 48), near=True)
        p.p('inplist', 0, 'S+0', near=True)
        p.w('S', 0, struct.pack('<5h', 0, 0, 0, 0, 1))  # inplist: x y ? ? mode
        p.p('UsPtr', 0, '0')
        self.setw('scrmode', 1)
        self.setw('RightPanel', 0)
        self.setw('GameInputMode', 0)
        self.setw('GrSq', -1)
        return block

    def block(self):
        return self.p.e.read(MAPSEG, 0, 0x7c08)

    def player_record(self, lefty=None, female=None):
        """random sound player: skills and stats, the weight, a handedness"""
        rng, p = self.rng, self.p
        rec = bytearray(rng.getrandbits(8) for _ in range(PLAYERDAT))
        rec[0:0x1e] = b'Avatar' + bytes(0x18)
        for o in (0x1e, 0x1f, 0x20):
            rec[o] = rng.randint(8, 40)
        for i in range(20):
            rec[0x21 + i] = rng.randint(0, 30)
        rec[0x35] = rng.randint(5, 60)
        rec[0x36] = rec[0x35] + rng.randint(0, 20)
        rec[0x5f] = rec[0x5f] & ~0xfc  # no poison, no active spells (bits 2-5 poison, 6-9 the spell count)
        rec[0x60] = rec[0x60] & ~0x03
        rec[0x61] &= ~0x0c  # no mushrooms: set_drugged would load a palette
        rec[0x62] &= ~0x10  # no maze
        lefty = rng.randint(0, 1) if lefty is None else lefty
        female = rng.randint(0, 1) if female is None else female
        rec[0x64] = (rec[0x64] & ~3) | lefty | (female << 1)
        rec[0xb4] = rng.choice([0, 1])
        rec[0xb6] = (rec[0xb6] & 0xf8) | rng.randint(0, 6)
        struct.pack_into('<HH', rec, 0x4a, 0, rng.choice([rng.randint(200, 1200), rng.randint(0, 1200)]))
        struct.pack_into('<I', rec, 0xce, rng.randint(0, 0x20000))
        struct.pack_into('<I', rec, 0xbc, rng.randint(0, 0x20000))
        p.w('PlayerDat', 0, bytes(rec))
        return rec

    def reset_inventory(self, lefty=None, on_map=True):
        """an empty inventory: the player object with nothing in it, the slots, the cursor, no open bags"""
        p = self.p
        self.player_record(lefty)
        po = bytearray(27)
        struct.pack_into('<HHHH', po, 0, 0x7f, 0, 0, 0)
        po[8] = self.rng.randint(10, 60)
        self.on_map = False
        p.w('L', 0x4000 + 27, bytes(po))
        if on_map:
            self.place_player()
        p.w('Inventory', 0, bytes(0x38))
        p.p('CursorObjPtr', 0, '0')
        p.p('OpenBag', 0, '0')
        p.p('OpenBagList', 0, '0')
        p.p('ActiveObj', 0, '0')
        p.w('BagSaveHandles', 0, bytes(16))
        p.w('SaveHandles', 0, bytes(46))
        p.w('InvUpArrow', 0, b'\0')
        p.w('InvDownArrow', 0, b'\0')
        p.w('invArmorObj', 0, bytes(6))
        p.w('invArmorQ', 0, bytes(6))
        p.w('SlotToDisplay', 0, bytes(SLOT_TO_DISPLAY + DISPLAY_TO_SLOT))
        self.setw('GameInputMode', 0)

    def place_player(self):
        """the player object (1) on a random open tile away from the map's edge, by the proven Obj_Add: his place and home
        words set, as the drop and throw code (ReturnObject) and the collision scans read them"""
        p, rng = self.p, self.rng
        b = self.block()
        tiles = []
        for ty in range(2, 62):
            for tx in range(2, 62):
                w0 = u16(b, (ty * 64 + tx) * 4)
                if w0 & 15 == 1 and (w0 >> 4) & 15 < 14:
                    tiles.append((tx, ty, (w0 >> 4) & 15))
        tx, ty, h = rng.choice(tiles)
        pos = ((h << 3) & 0x7f) | (rng.randint(0, 7) << 7) | (rng.randint(0, 7) << 10) | (rng.randint(0, 7) << 13)
        p.w('L', 0x4000 + 27 + 2, struct.pack('<H', pos))
        p.w('L', 0x4000 + 27 + 0x16, struct.pack('<H', (tx << 10) | (ty << 4)))
        r = p.call('Obj_Add', [('f', 'L+%x' % ((ty * 64 + tx) * 4 + 2)), ('f', 'L+401b')], None)
        p.same('Obj_Add', r, self.ctx + ' place_player')
        self.on_map = True
        self.tile = (tx, ty)
        self.setw('GrSq', ty * 64 + tx)  # the tile the player is on (do_teleport and friends take him off its list)

    # ---- objects ----------------------------------------------------------------------------------------------
    def new_obj(self, item, quality=None, qty=None, link=0, special=False, extra_id=0):
        """Obj_Alloc on both sides, the object made a sound unlinked one of item (static); returns (spec, index) or None.
        qty makes it a stack of that many (is_quant); link is a contents head (an object index)"""
        p, rng = self.p, self.rng
        r = p.call('Obj_Alloc', [('w', 0)], 'f')
        p.same('Obj_Alloc', r, self.ctx)
        spec = r[0][0].split()[1]
        if spec == '0':
            return None
        off = int(spec[2:], 16)
        idx = 256 + (off - 0x5b00) // 8
        q = rng.getrandbits(6) if quality is None else quality
        idw = item | extra_id
        if qty is not None:
            idw |= 0x8000
            lk = qty
        else:
            lk = link
        p.w('L', off, struct.pack('<HHHH', idw, rng.getrandbits(16) & 0xff80 | rng.randint(0, 0x7f), q, lk << 6))
        return spec, idx

    def random_item(self, kind=None):
        # never the bedroll (0x121): using it from the panel sleeps, which lets hours of the level pass (the critters'
        # updates: hundreds of millions of instructions in the emulator); sleeping is proved with SKILLS.C
        i = self.random_item1(kind)
        return i if i != 0x121 else 0x122

    def random_item1(self, kind=None):
        rng = self.rng
        kind = kind or rng.choice(['any', 'any', 'pickup', 'pickup', 'pickup', 'bag', 'bag', 'light', 'food', 'armour',
                                   'missile', 'weapon', 'rune', 'scroll'])
        if kind == 'any':
            return rng.randint(0, 0x1ff)
        if kind == 'pickup':
            return rng.choice(self.pickup)
        if kind == 'bag':
            return rng.choice([0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
                               0x80, 0x82, 0x8d, 0x8e, 0x8f, 0x88])
        if kind == 'light':
            return rng.choice(list(range(0x90, 0x98)))
        if kind == 'food':
            return rng.choice(list(range(0xb0, 0xc0)) + [0x92, 0xce, 0xcf, 0x125, 0x11b, 0xd9])
        if kind == 'armour':
            return rng.randint(0x20, 0x3f)
        if kind == 'missile':
            return rng.choice([0x10, 0x11, 0x12, 0x18, 0x19, 0x1a, 0x1f])
        if kind == 'weapon':
            return rng.randint(0, 0x0f)
        if kind == 'rune':
            return rng.choice(list(range(0xe8, 0x100)) + list(range(0x120, 0x130)))
        if kind == 'scroll':
            return rng.choice(list(range(0x130, 0x140)))
        return rng.randint(0, 0x1ff)

    def random_obj(self, kind=None, item=None, allow_stack=True):
        """a new random object (a stack when its item stacks and luck has it)"""
        rng = self.rng
        item = self.random_item(kind) if item is None else item
        stack = (self.com[2 + item * 11 + 3] >> 6) & 3
        qty = None
        if allow_stack and stack in (0, 2) and rng.random() < 0.35:
            qty = rng.choice([1, 2, 3, 5, 12, 40, 200, 600, 998])
        return self.new_obj(item, qty=qty)

    def add_end(self, head, obj_spec):
        r = self.p.call('Obj_AddEnd', [('f', head), ('f', obj_spec)], None)
        self.p.same('Obj_AddEnd', r, self.ctx)

    def fill_contents(self, cont, depth=0):
        """random objects into a container's contents (by Obj_AddEnd); nested bags to depth 2"""
        rng = self.rng
        cspec, cidx = cont
        n = rng.choice([0, 1, 1, 2, 3, 4, 5, 6, 9, 11])
        for k in range(n):
            kind = None
            if depth < 2 and rng.random() < 0.2:
                kind = 'bag'
            o = self.random_obj(kind)
            if not o:
                continue
            if kind == 'bag' and (self.item_of(o) >> 4) == 8:
                self.fill_contents(o, depth + 1)
            # an invisible object now and then (the open bag skips them)
            if rng.random() < 0.1:
                off = int(o[0][2:], 16)
                w = u16(self.p.e.read(MAPSEG, off, 2), 0)
                self.p.w('L', off, struct.pack('<H', w | 0x4000))
            self.add_end('L+%x' % (int(cspec[2:], 16) + 6), o[0])

    def item_of(self, o):
        return u16(self.p.e.read(MAPSEG, int(o[0][2:], 16), 2), 0) & 0x1ff

    def build_inventory(self, nobj=None):
        """random items into random slots through the proven AddToInventory (which keeps ItemFitsSlot's rules); the
        bags among them filled; the carried weight set from what is carried"""
        rng, p = self.rng, self.p
        nobj = rng.randint(3, 16) if nobj is None else nobj
        for k in range(nobj):
            kind = rng.choice([None, None, 'bag', 'bag', 'armour', 'light', 'food', 'missile'])
            o = self.random_obj(kind)
            if not o:
                break
            slot = rng.choice(list(range(0, 19)) * 2 + list(range(11, 19)) * 3)
            if p.e.rw(DGROUP, p.addr('Inventory') + slot * 2):
                continue
            r = p.call('AddToInventory', [('f', o[0]), ('w', slot)], 'b')
            p.same('AddToInventory', r, self.ctx + ' build')
            if r[0][0] == 'ret 1' and (self.item_of(o) >> 4) == 8:
                self.fill_contents(o)
        self.set_weight()
        self.p.same('FixPlayerEquips', p.call('FixPlayerEquips', [], None), self.ctx + ' build')

    def player_contents(self):
        """the object indexes of the player's contents list (top level), from the emulator's block"""
        b = self.block()
        out = []
        i = u16(b, 0x4000 + 27 + 6) >> 6
        while i and len(out) < 300:
            out.append(i)
            i = u16(b, obj_off(i) + 4) >> 6
        return out

    def open_objs(self):
        """the object indexes of the open bags (the chain OpenBagList..OpenBag), from the emulator's records"""
        a, _ = self.bags()
        return set(x[0] >> 6 for x in a[1] if x != 'loop')

    def bag_open(self):
        a = self.p.addr('OpenBag')
        return bool(self.p.e.rw(DGROUP, a) or self.p.e.rw(DGROUP, a + 2))

    def id_at(self, idx):
        return u16(self.p.e.read(MAPSEG, obj_off(idx), 2), 0)

    def item_at(self, idx):
        return u16(self.p.e.read(MAPSEG, obj_off(idx), 2), 0) & 0x1ff

    def carried(self):
        """every object index the player carries: the contents list and (recursively) what the containers hold"""
        b = self.block()
        out = []

        def walk(i, depth):
            while i and len(out) < 400:
                out.append(i)
                w0 = u16(b, obj_off(i))
                if not w0 & 0x8000 and depth < 5:
                    walk(u16(b, obj_off(i) + 6) >> 6, depth + 1)
                i = u16(b, obj_off(i) + 4) >> 6
        walk(u16(b, 0x4000 + 27 + 6) >> 6, 0)
        return out

    def check_lists(self, ctx):
        """the player's contents lists (and what the containers hold) have no cycle and no object twice (a harness check:
        a broken list hangs both sides' loops)"""
        b = self.block()
        seen = {}

        def walk(i, owner, depth):
            n = 0
            while i:
                if i in seen or depth > 6 or n > 400:
                    raise ScenarioDone('object %d on two lists / a cycle (%s): first under %s, again under %s' % (i, ctx, seen.get(i), owner))
                seen[i] = owner
                if not u16(b, obj_off(i)) & 0x8000:
                    walk(u16(b, obj_off(i) + 6) >> 6, i, depth + 1)
                i = u16(b, obj_off(i) + 4) >> 6
                n += 1
        walk(u16(b, 0x4000 + 27 + 6) >> 6, 'player', 0)
        # the open bag's slots 20..27 name objects of that bag's contents (the panel redraws after what takes one out)
        a, _ = self.bags()
        if a[2] != -1 and a[1] and a[2] != 'x':
            bagobj = a[1][a[2]][0] >> 6
            chain = set()
            i = u16(b, obj_off(bagobj) + 6) >> 6 if bagobj else 0
            while i and len(chain) < 300:
                chain.add(i)
                i = u16(b, obj_off(i) + 4) >> 6
            for s_, idx in enumerate(self.slots()[20:28]):
                if idx and idx not in chain:
                    raise ScenarioDone('slot %d shows object %d, not in the open bag (%s)' % (20 + s_, idx, ctx))
        # none of what is carried is on the free stack (a freed object still linked: the next Obj_Alloc hands it out again)
        top = self.p.e.rw(DGROUP, self.p.addr('objptr'))
        free = set(u16(b, o) for o in range(0x74fc, top, 2))
        for i in seen:
            if i >= 256 and i in free:
                raise ScenarioDone('object %d is carried and on the free stack (%s)' % (i, ctx))

    def slots(self):
        """Inventory[0..27] as object indexes (from the emulator's record)"""
        a = self.p.addr('Inventory')
        return [self.p.e.rw(DGROUP, a + s * 2) >> 6 for s in range(28)]

    def set_weight(self):
        """PlayerDat's carried weight: ItemWeight (both sides, compared) of what the player's contents list holds"""
        p = self.p
        tot = 0
        for i in self.player_contents():
            r = p.call('ItemWeight', [('f', 'L+%x' % obj_off(i))], 'w')
            p.same('ItemWeight', r, self.ctx + ' set_weight')
            tot += int(r[0][0].split()[1], 16)
        p.w('PlayerDat', 0x4a, struct.pack('<H', tot & 0xffff))

    # ---- the open bags ----------------------------------------------------------------------------------------
    def bags(self):
        """the open bag chain as both sides hold it: [(obj, weight, index of prev, index of next)], OpenBag's index"""
        p = self.p
        e = p.e
        # the original: far pointers in DS
        def far_at(name):
            a = p.addr(name)
            return e.rw(DGROUP, a), e.rw(DGROUP, a + 2)
        mine = []
        seen = {}
        lst = []
        o, s = far_at('OpenBagList')
        while (o or s) and len(lst) < 20:
            lin = s * 16 + o
            if lin in seen:
                lst.append(('loop',))
                break
            seen[lin] = len(lst)
            nx = (e.rw(s, o), e.rw(s, o + 2))
            pv = (e.rw(s, o + 4), e.rw(s, o + 6))
            lst.append((lin, nx, pv, e.rw(s, o + 8), e.rw(s, o + 10)))
            o, s = nx
        def norm(lst, ptr_at):
            res = []
            idx = dict((x[0], k) for k, x in enumerate(lst))
            for x in lst:
                if x[0] == 'loop':
                    res.append('loop')
                    continue
                res.append((x[3] & 0xffc0, x[4], idx.get(x[2], -1 if not x[2] else 'x'), idx.get(x[1], -1 if not x[1] else 'x')))
            return res
        # normalise the DOS records: linear addresses of next and prev
        lst2 = []
        for x in lst:
            if x[0] == 'loop':
                lst2.append(x)
                continue
            lin, nx, pv, ob, wt = x
            lst2.append((lin, nx[1] * 16 + nx[0] if nx != (0, 0) else 0, pv[1] * 16 + pv[0] if pv != (0, 0) else 0, ob, wt))
        mine = norm(lst2, None)
        ob = far_at('OpenBag')
        mine_ob = (ob[1] * 16 + ob[0]) if ob != (0, 0) else 0
        mine_ob = dict((x[0], k) for k, x in enumerate(lst2) if x[0] != 'loop').get(mine_ob, -1 if not mine_ob else 'x')
        # ours: host pointers
        def hp(name):
            return int(p._cmd('qn %s 0' % name, True), 16)
        ours_l = []
        a = hp('OpenBagList')
        while a and len(ours_l) < 20:
            if any(a == y[0] for y in ours_l):
                ours_l.append(('loop',))
                break
            raw = bytes.fromhex(p._cmd('rm %x 14' % a, True))
            nx, pv = struct.unpack_from('<QQ', raw, 0)
            ob_, wt = struct.unpack_from('<Hh', raw, 16)
            ours_l.append((a, nx, pv, ob_, wt & 0xffff))
            a = nx
        ours = norm(ours_l, None)
        ao = hp('OpenBag')
        ours_ob = dict((x[0], k) for k, x in enumerate(ours_l) if x[0] != 'loop').get(ao, -1 if not ao else 'x')
        return ('bags', mine, mine_ob), ('bags', ours, ours_ob)

    # ---- the comparison ---------------------------------------------------------------------------------------
    def read_many(self, reqs):
        p = self.p
        mine, lines = [], []
        for name, off, n in reqs:
            if name == 'L':
                mine.append(p.e.read(MAPSEG, off, n).hex())
                lines.append('r L+0 %x %x' % (off, n))
            else:
                mine.append(p.e.read(DGROUP, p.addr(name) + off, n).hex())
                lines.append('r %s %x %x' % (name, off, n))
        p.c.stdin.write('\n'.join(lines) + '\n')
        p.c.stdin.flush()
        ours = [p.c.stdout.readline().rstrip('\n') for _ in lines]
        return mine, ours

    def globals_diff(self, skip=()):
        reqs = [(n, 0, s) for n, s in self.gnames if n not in skip and n not in self.SKIP]
        mine, ours = self.read_many(reqs)
        out = []
        for (n, _, s), a, b in zip(reqs, mine, ours):
            if a != b:
                i = next(k for k in range(0, len(a), 2) if a[k:k + 2] != b[k:k + 2]) // 2
                out.append((n, i, a[max(0, i * 2 - 8):i * 2 + 24], b[max(0, i * 2 - 8):i * 2 + 24]))
        return out

    def baseline(self, skip=()):
        """the names that differ now, with nothing run: the pointer variables set by the setup (a pointer is 4 bytes
        in DOS, 8 on the host); each is compared through Pair.q (as a name or place both sides know) from here on"""
        d = self.globals_diff(skip)
        self.ptrnames = [x[0] for x in d]
        self.SKIP = self.SKIP | set(self.ptrnames)
        self.gnames = [(n, s) for n, s in self.gnames if n not in self.SKIP]
        return self.ptrnames

    def check(self, fn, res, ctx=None, extra=(), objs=True, quiet_globals=False):
        p = self.p
        ctx = ctx or self.ctx
        if fn:
            p.same(fn, res, ctx)
        p.same_bytes('L', 0, 0x7c08, ctx)
        p.same('rand', p.gseed(), ctx)
        p.same_bytes('PlayerDat', 0, PLAYERDAT, ctx)
        ia, ib = p.r('Inventory', 0, 0x38)
        # Inventory[19].low: CloseTheBag copies OpenBag->obj whole, whose low 6 bits farmalloc left unset (heap garbage
        # in DOS and on the host: never read), so they are not compared
        ia, ib = [x[:76] + '%02x' % (int(x[76:78], 16) & 0xc0) + x[78:] for x in (ia, ib)]
        if ia != ib:
            raise Differ('Inventory differs (%s):\n  original %s\n  ours     %s' % (ctx, ia, ib))
        p.same_bytes('SlotToDisplay', 0, 49, ctx)  # with DisplayToSlot behind it (one array on the host)
        for n, near in self.PTRS.items():
            if self.has(n):
                p.same(n, p.q(n, 0, near=near), ctx)
        for n, near in self.FNPTRS.items():
            n, _, hoff = n.partition('@')
            hoff = int(hoff or 0)
            if self.has(n):
                a_ = bool(p.e.rw(DGROUP, p.addr(n) + hoff) or (not near and p.e.rw(DGROUP, p.addr(n) + hoff + 2)))
                b_ = int(p._cmd('qn %s %x' % (n, hoff), True), 16) != 0
                if a_ != b_:
                    raise Differ('%s differs (%s): original %s ours %s' % (n, ctx, a_, b_))
        a, b = self.bags()
        if a != b:
            raise Differ('open bags differ (%s):\n  original %s\n  ours     %s' % (ctx, a, b))
        d = self.globals_diff()
        if d and os.environ.get('PROOF_ALLDIFF'):
            for x in d:
                if x[0] not in self.diag:
                    self.diag.append(x[0])
                    print('DIFF', x[0], 'at', x[1], 'orig', x[2], 'ours', x[3], '|', ctx, flush=True)
            d = []
        if d:
            raise Differ('globals differ (%s): ' % ctx + '; '.join('%s+%x orig %s ours %s' % x for x in d[:6]))
        if fn:
            self.counts[fn] = self.counts.get(fn, 0) + 1
        self.check_lists(ctx)

    def summary(self, label, routines, what='operations'):
        p = self.p
        tot = sum(self.counts.values())
        print('%-14s %d %s  same (%s)' % (label, tot, what, ', '.join('%s %d' % kv for kv in sorted(self.counts.items()))))
        ran, notrun, nosym = p.ran(routines)
        print('  routines run: %d of %d%s%s' % (len(ran), len(routines), ('; not run: ' + ', '.join(notrun)) if notrun else '',
              ('; no address: ' + ', '.join(nosym)) if nosym else ''))
        print('all the same')


def main_wrap(label, fn):
    try:
        fn()
    except Differ as ex:
        print('%s DIFFER: %s' % (label, ex))
        sys.exit(1)


# ---- the shared workspace (TMPALLOC.C's 64 KB scratch segment) ---------------------------------------------------
WS_SEG = 0xF010  # in the emulator: linear F0100 up to the end of its memory (the sentinel is at F000:0)


def ws_hooks(w):
    """get_workspace and its friends stood in for in the emulator (the original's maps EMS pages): a fixed segment"""
    p = w.p
    e = p.e
    e.hook(*p.code['get_workspace'], lambda e_: (WS_SEG, 0))
    for n in ('release_workspace', 'set_workspace'):
        if n in p.code:
            e.hook(*p.code[n], lambda e_: (0, 0))


def ws_bytes(w, n, off=0):
    """the first n bytes of the workspace on both sides: (original, ours) hex"""
    p = w.p
    mine = p.e.read(WS_SEG, off, n).hex()
    seg = int(p._cmd('call get_workspace', True).split()[1], 16)
    p.c.stdout.readline()  # the calls line
    addr = int(p._cmd('call port_mk_fp %d %d' % (seg, off), True).split()[1], 16)
    p.c.stdout.readline()
    ours = p._cmd('rm %x %x' % (addr, n), True)
    return mine, ours


def sound_cursor(w, carried):
    """the cursor object, when it is also carried (a call that left it so: the game never does) is let go of, on both sides"""
    p = w.p
    a = p.addr('CursorObjPtr')
    off, seg = p.e.rw(DGROUP, a), p.e.rw(DGROUP, a + 2)
    if (off or seg) and seg == 0xC000 + 0:  # in the level block
        idx = (0x100 + (off - 0x5b00) // 8) if off >= 0x5b00 else (off - 0x4000) // 27
        if idx in carried:
            p.p('CursorObjPtr', 0, '0')
            w.setw('GameInputMode', 0)


def scenarios(w, per, op, nobj=(2, 14), ops_per=(8, 25), setup=None, levels=range(1, 10), on_map=True):
    """the common loop: per operations on each of the nine levels, in scenarios (the level reloaded, a player record and an
    inventory built, then random operations). op(w, level, scen, k, ctx) makes one operation (and its w.check); a
    ScenarioDone from it ends the scenario."""
    p, rng = w.p, w.rng
    based = False
    for level in levels:
        done = 0
        scen = 0
        while done < per:
            scen += 1
            w.level(level)
            w.ctx = 'level %d scenario %d' % (level, scen)
            w.reset_inventory(on_map=on_map)
            w.build_inventory(rng.randint(*nobj))
            if not based:
                w.baseline()
                based = True
            if setup:
                setup(w, level, scen)
            try:
                for k in range(rng.randint(*ops_per)):
                    done += 1
                    p.seed(rng.getrandbits(32))
                    p.time(rng.getrandbits(32))
                    w.ctx = 'level %d scenario %d op %d' % (level, scen, k)
                    op(w, level, scen, k, w.ctx)
            except ScenarioDone as ex:
                if w.trace:
                    print('   scenario ends:', ex, flush=True)


SOUND_SPELLS = [(0, range(1, 8)), (1, range(1, 7)), (2, range(1, 16)), (3, range(1, 10)), (11, (0, 1, 2, 3, 14, 15)), (13, (4,))]


def set_spells(w, n=None, poison=None, shrooms=0):
    """the player's active spells (class, subclass, stability) and poison, from real spell kinds; both sides"""
    p, rng = w.p, w.rng
    n = rng.choice([0, 0, 1, 2, 3]) if n is None else n
    poison = rng.choice([0, 0, 0, 1, 3, 8]) if poison is None else poison
    words = []
    for k in range(3):
        cls, subs = rng.choice(SOUND_SPELLS)
        sub = rng.choice(list(subs))
        stab = rng.choice([1, 1, 2, 3, 5, 9, 40])
        words.append(cls | sub << 4 | stab << 8)
    p.w('PlayerDat', 0x3e, struct.pack('<3H', *words))
    w5f = struct.unpack('<H', bytes.fromhex(p.r('PlayerDat', 0x5f, 2)[0]))[0]
    w5f = (w5f & ~0x3fc) | (poison << 2) | (n << 6)
    p.w('PlayerDat', 0x5f, struct.pack('<H', w5f))
    w61 = struct.unpack('<H', bytes.fromhex(p.r('PlayerDat', 0x61, 2)[0]))[0]
    p.w('PlayerDat', 0x61, struct.pack('<H', (w61 & ~0x0c) | (shrooms << 2)))


def set_pn(w):
    """PN (the player's physics record) at the player's place, and the facing: sound for the code that reads them"""
    p, rng = w.p, w.rng
    b = w.block()
    pos = u16(b, 0x4000 + 27 + 2)
    home = u16(b, 0x4000 + 27 + 0x16)
    tx, ty = (home >> 10) & 63, (home >> 4) & 63
    fx, fy = pos >> 13, (pos >> 10) & 7
    z = pos & 0x7f
    x = (tx << 8) + (fx << 5) + 16
    y = (ty << 8) + (fy << 5) + 16
    p.w('PN', 0, struct.pack('<3h', x, y, z << 3))
    face = rng.getrandbits(16)
    w.setw('PlayerFacing', face if face < 0x8000 else face - 0x10000)
    w.setw('PlayerHeading', (pos >> 7) & 7)


def put_ptr_s(w, dos_off, host_off, spec, far=True):
    """a pointer into the scratch block: on the original at dos_off (4 bytes, or 2 near), on the core at host_off (8 bytes)"""
    p = w.p
    o, s = p._dos_ptr(spec)
    p.e.ww(DGROUP, 0xE000 + dos_off, o)
    if far:
        p.e.ww(DGROUP, 0xE000 + dos_off + 2, s)
    p._cmd('ps %x %s' % (host_off, spec))


def wfar(w, name, off, data):
    """bytes into a far data array (a name with a segment:offset symbol: stdat) on both sides"""
    p = w.p
    seg, o0 = p.code[name]
    p.e.write(seg, o0 + off, bytes(data))
    p._cmd('w %s %x %s' % (name, off, bytes(data).hex()))


def cstr(w, off, text):
    """a C string into the scratch block (both sides)"""
    w.p.w('S', off, text.encode('latin-1') + b'\0')


def save0(w):
    """SAVE0\LEV.ARK as the game has it after start-up: the workspace stand-ins in the emulator, the directories in each side's
    write folder, the texture buffers of load_txtmaps at the workspace (dseg_5c99_7178: both sides' own segment of it), and
    LEV.ARK copied by the game's own copy_file (compared)"""
    p = w.p
    ws_hooks(w)
    for d in p.write_dirs:
        os.makedirs(os.path.join(d, 'SAVE0'), exist_ok=True)
        for k in range(1, 5):
            os.makedirs(os.path.join(d, 'SAVE%d' % k), exist_ok=True)
    seg = int(p._cmd('call get_workspace', True).split()[1], 16)
    p.c.stdout.readline()
    p.e.ww(DGROUP, p.addr('dseg_5c99_7178'), WS_SEG)
    p._cmd('w dseg_5c99_7178 0 %s' % struct.pack('<H', seg & 0xffff).hex())
    cstr(w, 0, 'DATA\lev.ark')
    cstr(w, 0x40, 'SAVE0\lev.ark')
    r = p.call('copy_file', [('n', 'S+0'), ('n', 'S+40')], 'b')
    p.same('copy_file', r, 'save0')
    return r


def files_same(w, rel, ctx=''):
    """the file rel (under each side's write folder) is the same on both sides; returns its size"""
    ed, cd = w.p.write_dirs
    a = open(os.path.join(ed, rel), 'rb').read()
    b = open(os.path.join(cd, rel), 'rb').read()
    if a != b:
        i = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]), min(len(a), len(b)))
        raise Differ('%s differs at byte %x of %d / %d (%s):\n  original %s\n  ours     %s' % (rel, i, len(a), len(b), ctx, a[max(0, i - 8):i + 24].hex(), b[max(0, i - 8):i + 24].hex()))
    return len(a)
