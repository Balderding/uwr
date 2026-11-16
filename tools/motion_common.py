"""motion_common.py: what the Movement proofs (proof_collide, proof_motion, proof_objphys, proof_physics,
proof_playmove) share: a Pair with the game's globals set up the same on both sides (the player record, the
clock, the player object placed on a real level), generators of sound inputs (open tiles, real objects, movers),
and one full comparison of everything the movement routines can touch.

A proof script builds a World, then for every operation: sets up inputs on both sides (w.* helpers), calls the
routine with p.call, and runs w.check(ctx, result) which compares the return and the outside calls, Borland rand's
state, the whole level block, the physics records, the collision record and every global listed in STATE.
"""
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair  # noqa: E402
from emu import DGROUP  # noqa: E402

# plain data (same layout on both sides) compared after every operation: (name, bytes)
STATE = [('Ppd', 23), ('oCollisions', 48), ('PN', 40), ('CN1', 40), ('CN2', 40), ('CN3', 40), ('CN4', 40),
         ('PT', 8), ('CT1', 8), ('CT2', 8), ('CT3', 8), ('CT4', 8),
         ('XP', 2), ('YP', 2), ('nvokHgt', 2), ('nvokTerr', 2), ('stay_centered', 1),
         ('GrSq', 2), ('PlayerFacing', 2), ('PlayerHeading', 2), ('PlayerLevel', 2), ('lastTerr', 2), ('pFPS', 6),
         ('pTurn', 2), ('MaxPlayerAccel', 2), ('plyMoType', 2), ('motionbits', 1), ('fiz_update', 1),
         ('playerMod', 8), ('PlayerPitch', 2), ('PlayerBank', 2), ('PlayerInput', 2), ('PlayerTurn', 2),
         ('ForwInpRate', 2), ('TurnInpRate', 2), ('doMod', 1), ('combEfflen', 1), ('tremEfflen', 1),
         ('nextstep', 4), ('watertime', 4), ('water_eff', 1), ('PlayerDat', 0xd2), ('MapObj_X', 2), ('MapObj_Y', 2),
         ('failed', 1), ('control', 1), ('hitwall', 1), ('aligned', 1), ('didhitobj', 1), ('hitadoor', 1),
         ('doorx', 1), ('doory', 1), ('dontchangedz', 1), ('crit_terr', 2), ('tsteps', 1), ('pmouseHandled', 1),
         ('vort_rad', 2), ('vort_timer', 2), ('vort_theta', 2), ('camang', 6), ('campos', 6), ('MoveCrits', 1),
         ('res_to_terr', 18), ('animcount', 1), ('animlist', 384), ('animclassd', 64), ('AnimObjInPipe', 2), ('curBin', 2), ('DoAnimO', 1), ('hitx', 1), ('hity', 1)]
# near pointers (name) and far pointers
NEAR_PTRS = ['curP', 'CP', 'TP', 'player', 'playerdat']
FAR_PTRS = ['centptr', 'collobject', 'meptr', 'ThePlayer', 'UsPtr']
OBJ_PTRS = ['critptr', 'objptr', 'LastActiveMob', 'ActiveMob']
MP_AT = 4          # DOS offset of MP.frac; the host's is 12 more
MP_LEN = 0x37 - 4


CURRENT = None


class Skip(Exception):
    """the original did not finish a call (its own code loops on the state the proof gave it): the world is rebuilt"""


def obj_off(i):
    return 0x4000 + i * 27 if i < 256 else 0x5b00 + (i - 256) * 8


def chain(block, link, guard=1100):
    out = []
    while link and guard:
        out.append(link)
        link = struct.unpack_from('<H', block, obj_off(link) + 4)[0] >> 6
        guard -= 1
    return out


def phys(x=0, y=0, z=0, vel=(0, 0, 0), acc=(0, 0, 0), time=0, speed=0, bounce=0, flags=0, mass=0, light=0, hp=0,
         resist=0, heading=0, index=0, radius=0, height=0, b24=0, terrain=1, impact=0):
    def s(v):
        v &= 0xffff
        return v - 0x10000 if v & 0x8000 else v
    return struct.pack('<hhh3h3hhhBBhBBBBhhBBBBH', s(x), s(y), s(z), *[s(v) for v in vel], *[s(v) for v in acc], s(time),
                       s(speed), bounce, flags, s(mass), light, hp, resist, 0, s(heading), s(index), radius, height, b24,
                       terrain, impact & 0xffff)


def unphys(b):
    t = struct.unpack('<hhh3h3hhhBBhBBBBhhBBBBH', b)
    return dict(x=t[0], y=t[1], z=t[2], vel=t[3:6], acc=t[6:9], time=t[9], speed=t[10], bounce=t[11], flags=t[12],
                mass=t[13], light=t[14], hp=t[15], resist=t[16], heading=t[18], index=t[19], radius=t[20],
                height=t[21], b24=t[22], terrain=t[23], impact=t[24])


class World:
    def __init__(self, game, coretest, seed, name):
        self.p = p = Pair(game, coretest)
        p.files()                               # the game's files for both sides (strings read by the traps an object use sets off)
        p.call('init_strings', [], 'b')
        global CURRENT
        CURRENT = self
        self.name = name
        self.rng = random.Random(seed)
        self.counts = {}
        self.cov = {}
        self.total = 0
        self.ctx = ''
        self.state = [(n, k) for n, k in STATE if n in p.data and n in p.host]
        self.nears = [n for n in NEAR_PTRS if n in p.data]
        self.fars = [n for n in FAR_PTRS if n in p.data]
        # the clock: *Time (a far pointer at DS:2364) in the emulator, core_time on our side
        o, s = p.e.rw(DGROUP, p.data['Time']), p.e.rw(DGROUP, p.data['Time'] + 2)
        self.time_at = (s, o)
        self.set_time(self.rng.randint(0, 60000))
        com = open(os.path.join(game, 'DATA', 'COMOBJ.DAT'), 'rb').read()[2:2 + 512 * 11]
        self.com = [com[i * 11:i * 11 + 11] for i in range(512)]
        orig_call = p.call

        def guarded(name, args=(), ret='w'):
            snap = (p.e.read(DGROUP, 0, 0x10000), p.e.read(0xC000, 0, 0x8000))   # DGROUP and the level block, before the call
            try:
                return orig_call(name, args, ret)
            except Differ as ex:
                if 'did not finish' in str(ex):
                    # the original looped on the state the proof gave it (and wrote where it should not): put its memory
                    # back as it was before the call (our side never ran it) and drop the operation
                    p.e.write(DGROUP, 0, snap[0])
                    p.e.write(0xC000, 0, snap[1])
                    self.cover('original did not finish (operation dropped)')
                    raise Skip(str(ex))
                raise
        p.call = guarded
        if 'EtherealVoidSpecialEffects_seg00' in p.code:   # (symbols.tsv truncates the name)
            p.code['EtherealVoidSpecialEffects_seg008_150'] = p.code['EtherealVoidSpecialEffects_seg00']
        p.call('init_ai', [], None)
        # bounce_obj with a static object as the one pushed reads an uninitialised stack word (phys.terrain, which
        # get_phys_data sets only for mobile objects): the original's value is whatever its stack held and nothing
        # on our side can match it. An operation that reaches it is flagged here, not compared, and our side is made
        # equal to the original's afterwards (resync)
        from unicorn import UC_HOOK_CODE
        from unicorn.x86_const import UC_X86_REG_SP, UC_X86_REG_SS
        self.tainted = 0
        bs, bo = p.code['bounce_obj']

        def on_bounce(uc, address, size, user):
            sp, ss = uc.reg_read(UC_X86_REG_SP), uc.reg_read(UC_X86_REG_SS)
            if p.e.rw(ss, sp + 10) == 0xC000 and p.e.rw(ss, sp + 8) >= 0x5b00:
                self.tainted += 1
        p.e.uc.hook_add(UC_HOOK_CODE, on_bounce, begin=bs * 16 + bo, end=bs * 16 + bo)
        # get_pcoll reads oCollisions[MP.hit] with MP.hit -1 when a mover that cannot step stands on no object (and the
        # stale MP.item): in the original that is the six statics (xlow..yhigh) that precede oCollisions in DGROUP, in
        # the core whatever the host has there. The two instructions of get_pcoll that index oCollisions by MP.hit
        # (mov al,[bx+26C6h] and mov al,[bx+26C8h]) flag the operation when the index is outside 0..7
        from unicorn.x86_const import UC_X86_REG_BX
        gs, go = p.code['get_pcoll']
        for ip in (0x13bf, 0x1596):
            def on_idx(uc, address, size, user):
                bx = uc.reg_read(UC_X86_REG_BX)
                if bx >= 48:
                    self.tainted += 1
            p.e.uc.hook_add(UC_HOOK_CODE, on_idx, begin=gs * 16 + ip, end=gs * 16 + ip)
        # obj_coll_check lets found reach 9 (it stops only when found > 8): the ninth record is written past the 8-record
        # oCollisions, onto the statics that follow it in DGROUP (tiles[], which TerrainCheck rebuilds) in the original and
        # onto whatever follows the array in the core. An entry with found >= 8 flags the operation
        cs_, co_ = p.code['obj_coll_check']

        def on_coll(uc, address, size, user):
            ss_ = uc.reg_read(UC_X86_REG_SS)
            cur = p.e.rw(DGROUP, p.data['curP'])
            if p.e.rb(DGROUP, cur + 0x14) >= 8:
                self.tainted += 1
        p.e.uc.hook_add(UC_HOOK_CODE, on_coll, begin=cs_ * 16 + co_, end=cs_ * 16 + co_)
        # inplist (a near pointer the original initialises to a buffer; the stub's copy of its 2 bytes is no host pointer):
        # an Inplist of zeros (mode 0) in a spare buffer both sides have
        p.p('inplist', 0, '@chrbuf', near=True)
        self.block = None

    # --- clock --------------------------------------------------------------------------------------------
    def set_time(self, t):
        t &= 0xffff
        p = self.p
        p.e.write(self.time_at[0], self.time_at[1], struct.pack('<I', t))
        p._cmd('w core_time 0 %s' % struct.pack('<H', t).hex())
        self.time = t

    # --- writes to both sides --------------------------------------------------------------------------------
    def w16(self, name, v, off=0):
        self.p.w(name, off, struct.pack('<h' if v < 0 else '<H', v))

    def w8(self, name, v, off=0):
        self.p.w(name, off, bytes([v & 0xff]))

    def wl(self, name, v, off=0):
        self.p.w(name, off, struct.pack('<I', v & 0xffffffff))

    def np(self, name, spec, off=0):
        self.p.p(name, off, spec, near=True)

    def fp(self, name, spec, off=0):
        self.p.p(name, off, spec)

    # --- the level and the player ---------------------------------------------------------------------------
    def level(self, n):
        p, rng = self.p, self.rng
        self.lev = n
        blk = p.level(n)
        # the creatures' terrain code (b0A bits 4-6) is a table index in set_phys_data (res_to_terr[1 << code]): a few
        # creatures of the shipped levels carry 5-7, which index past the table (an out-of-bounds read of whatever
        # follows it in DGROUP, different in the core); those are given 0 here, on both sides
        for i in range(2, 256):
            o = 0x4000 + i * 27
            if (blk[o + 10] >> 4) & 7 > 4:
                p.w('L', o + 10, bytes([blk[o + 10] & 0x8f]))
        # triggers (items 0x1A0..0x1AF) made inert: with no trap behind them UseTrigger does nothing. A mover that
        # touches a live one would run the trap system (teleports and level changes, object creation, text, file reads),
        # which is not the movement code's and cannot be run in the emulator without DOS
        for i in list(range(2, 256)) + list(range(256, 256 + 768)):
            o = obj_off(i)
            if o + 8 > len(blk):
                break
            if 0x1A0 <= (blk[o] | blk[o + 1] << 8) & 0x1ff <= 0x1AF:
                ol = blk[o + 6] | blk[o + 7] << 8
                if ol >> 6:
                    p.w('L', o + 6, struct.pack('<H', ol & 0x3f))
        # every mobile object on a tile list is given that tile as its home (and a mobile non-creature its exact position
        # in the tile): set_phys_data moves an object between the tile lists by that position, and where a shipped level
        # has the two apart the list it is taken from and the one it is put into are different ones, which can link an
        # object to itself (a cyclic list). The core then runs off the end of oCollisions where the original quietly
        # writes over its neighbouring statics
        for t in range(4096):
            link = struct.unpack_from('<H', blk, t * 4 + 2)[0] >> 6
            for i in chain(blk, link):
                if i < 256 and i != 1:
                    o = obj_off(i)
                    tx, ty = t & 63, t >> 6
                    home = struct.unpack_from('<H', blk, o + 0x16)[0]
                    nh = (home & 0x000f) | (tx << 10) | (ty << 4)
                    if nh != home:
                        p.w('L', o + 0x16, struct.pack('<H', nh))
                    if (struct.unpack_from('<H', blk, o)[0] & 0x1ff) >> 6 != 1:
                        pos = struct.unpack_from('<H', blk, o + 2)[0]
                        z = pos & 0x7f
                        gx = (tx << 8) + (((pos >> 13) & 7) << 5) + 0xF
                        gy = (ty << 8) + (((pos >> 10) & 7) << 5) + 0xF
                        p.w('L', o + 0x0b, struct.pack('<HHH', gx, gy, z << 3))
        self.w16('PlayerLevel', n)
        self.np('player', '@PlayerDat')
        self.np('playerdat', '@Creature+bc0')
        # the creature table: a few bytes of the player's entry the physics reads (noise, visibility)
        self.p.w('Creature', 63 * 48 + 0x1d, bytes([rng.randint(0, 255)]))
        pd = bytearray(0xd2)
        for i in range(0x1e, 0x21):
            pd[i] = rng.randint(8, 30)
        for i in range(0x21, 0x35):
            pd[0x21 + (i - 0x21)] = rng.randint(0, 30)
        struct.pack_into('<HH', pd, 0x4a, rng.randint(0, 900), rng.choice([0, rng.randint(200, 1200)]))
        pd[0x6d] = rng.randint(0, 6)
        struct.pack_into('<H', pd, 0x6e, rng.getrandbits(16))
        struct.pack_into('<H', pd, 0x5f, rng.getrandbits(16))  # drawn bit
        struct.pack_into('<H', pd, 0x62, rng.getrandbits(16))  # talisman_ok
        pd[0xb8] = rng.choice([0, 0, 0, 1, 2, 8, 0x20, 0x40, 0x61])
        pd[0xb9] = rng.choice([0, 0x10, 0x60])
        struct.pack_into('<H', pd, 0xb6, rng.randint(0, 6) | (rng.getrandbits(8) << 3))
        struct.pack_into('<I', pd, 0xce, rng.getrandbits(24))
        self.p.w('PlayerDat', 0, bytes(pd))
        self.w8('plyNotice', rng.randint(1, 15))
        self.w8('plyNotice', rng.randint(1, 15), 1)
        self.w8('DragonSkinBoots_dseg_5c99_1B01', rng.randint(0, 1))
        for nm in ('Hasted', 'TimeStop', 'WizEye', 'MoveCrits', 'UsingPole'):   # (bytes: a 16-bit write would clobber DS neighbours)
            if nm in p.data:
                self.w8(nm, 0)
        self.w8('DoAnimO', 0)
        self.w8('MoveCrits', rng.randint(0, 1))
        # the current critter of the AI (crit_die plays the death cry by mycst->death): some creature type
        self.np('mycst', '@Creature+%x' % (48 * rng.randint(0, 62)))
        self.w16('myxpost', rng.randint(0, 500))
        self.w16('myypost', rng.randint(0, 500))
        self.w16('GrSq', 0xffff)
        # the player object (object 1): an adventurer, off the lists until player_setup links it
        po = bytearray(27)
        struct.pack_into('<HHHH', po, 0, 0x7f, 0, 0, 0)
        po[8] = rng.randint(1, 255)
        self.p.w('L', 0x4000 + 27, bytes(po))
        self.refresh()
        self.stat_clean()

    def recover(self):
        """after a Skip (the original's memory was put back before the call, our side never ran it)"""
        self.tainted = 0
        self.refresh()

    def stat_clean(self):
        self.w16('XP', 0)
        self.w16('YP', 0)

    def refresh(self, lazy=False):
        """re-read the level block from the emulator (the truth both sides agree on) and index it"""
        b = self.p.e.read(0xC000, 0, 0x7c08)
        if lazy and b == self.block:
            return b
        self.block = b
        self.tiles = {}
        for t in range(4096):
            tw = struct.unpack_from('<HH', b, t * 4)
            self.tiles[t] = (tw[0] & 15, (tw[0] >> 4) & 15, tw[1])
        self.dense = []
        for t in range(4096):
            if self.tiles[t][2] >> 6:
                c = chain(b, self.tiles[t][2] >> 6)
                if len(set(c)) != len(c):
                    # a list that links an object to itself or back (the original's own code did that with the state
                    # the proof gave it, the same on both sides): cut it, on both sides, so the lists stay sound (the
                    # core runs off the end of oCollisions on a looped list where the original quietly writes over its
                    # neighbours)
                    seen, prev, link = set(), None, self.tiles[t][2] >> 6
                    while link not in seen:
                        seen.add(link)
                        prev, link = link, struct.unpack_from('<H', b, obj_off(link) + 4)[0] >> 6
                    o = obj_off(prev) + 4
                    q = struct.unpack_from('<H', b, o)[0]
                    self.p.w('L', o, struct.pack('<H', q & 0x3f))
                    self.cover('cyclic list cut')
                    return self.refresh()
                if c[1:]:
                    self.dense.append(t)
        return b

    def tile_type(self, t):
        return self.tiles[t][0]

    def open_tiles(self, border=2):
        out = []
        for t in range(4096):
            x, y = t & 63, t >> 6
            if border <= x < 64 - border and border <= y < 64 - border and self.tiles[t][0] != 0:
                out.append(t)
        return out

    def objects_on(self, t):
        return chain(self.block, self.tiles[t][2] >> 6)

    def all_objects(self, mobile=None):
        """the (index, tile) of objects on the map lists"""
        out = []
        for t in range(4096):
            for i in chain(self.block, self.tiles[t][2] >> 6):
                if mobile is None or (i < 256) == mobile:
                    out.append((i, t))
        return out

    def obj_word(self, idx, off):
        return struct.unpack_from('<H', self.block, obj_off(idx) + off)[0]

    # --- sound inputs ------------------------------------------------------------------------------------------
    def pos_near(self, border=2, tall=False):
        """x, y (1/8 tiles) and z (object units) at a random place: mostly on an open tile, often near an object"""
        rng = self.rng
        ots = self.open_tiles(border)
        if rng.random() < 0.6:
            objs = self.all_objects()
            if self.dense and rng.random() < 0.5:
                t = rng.choice(self.dense)
                objs = [(i, t) for i in self.objects_on(t)]
            if tall:   # objects ObjectCheck does not skip (a static one of no height is)
                objs = [(i, t) for i, t in objs if i < 256 or self.com_height(self.obj_word(i, 0) & 0x1ff) > 0]
            if objs:
                i, t = rng.choice(objs)
                x, y = t & 63, t >> 6
                if border <= x < 64 - border and border <= y < 64 - border:
                    w = self.obj_word(i, 2)
                    fx, fy = (w >> 13) & 7, (w >> 10) & 7
                    z = w & 0x7f
                    return x * 8 + fx, y * 8 + fy, min(z + rng.choice([0, 0, 0, 1, 4, 8, 16]), 0x7f), t
        t = rng.choice(ots)
        x, y = t & 63, t >> 6
        fl = self.tiles[t][1] * 8
        return x * 8 + rng.randint(0, 7), y * 8 + rng.randint(0, 7), min(fl + rng.choice([0, 0, 0, 0, 1, 3, 8]), 0x7f), t

    def setup_player(self, tile=None):
        """player_setup on both sides at an open tile (links the player into its list)"""
        t = tile if tile is not None else self.rng.choice(self.open_tiles(3))
        r = self.p.call('player_setup', [('w', t & 63), ('w', t >> 6)], None)
        self.check('player_setup', r)
        self.refresh()
        return t

    # --- objects -----------------------------------------------------------------------------------------------------
    def com_radius(self, item):
        return (self.com[item][1] & 7)

    def com_height(self, item):
        return self.com[item][0]

    def com_flags(self, item):
        c = self.com[item]
        w3, w = c[3], c[6] | c[7] << 8
        return dict(solid=(w3 >> 1) & 1, c3_2=(w3 >> 2) & 1, no_hit=(w3 >> 3) & 1, touch=w & 1, usable=(w >> 1) & 1,
                    bounce=(w >> 5) & 15, fate=(w >> 9) & 15, mass=(c[1] | c[2] << 8) >> 4 & 0xfff)

    def pick_item(self):
        """an item for a footprint: from the level's real objects, or at random"""
        rng = self.rng
        if rng.random() < 0.7:
            objs = self.all_objects()
            if objs:
                i, t = rng.choice(objs)
                return self.obj_word(i, 0) & 0x1ff
        return rng.randint(0, 0x1ff)

    def new_object(self, mobile, item=None):
        """Obj_Alloc on both sides, the object made a sound unlinked one of a real item; returns its spec or None"""
        p, rng = self.p, self.rng
        res = p.call('Obj_Alloc', [('w', mobile)], 'f')
        p.same('Obj_Alloc', res, self.ctx)
        spec = res[0][0].split()[1]
        if spec == '0':
            return None
        off = int(spec[2:], 16)
        if item is None:
            item = self.pick_item()
            if mobile:
                item = rng.choice([item, rng.randint(0x40, 0x7e), rng.randint(0x20, 0x3f)])
        if mobile:
            rec = bytearray(27)
            struct.pack_into('<HHHH', rec, 0, item, rng.getrandbits(16) & 0xff80 | rng.randint(0, 0x7f), 0, 0)
            rec[8] = rng.randint(1, 255)
            rec[9] = rng.getrandbits(8)
            rec[0x0a] = rng.getrandbits(8)
            struct.pack_into('<H', rec, 0x0b, rng.getrandbits(16))
            struct.pack_into('<H', rec, 0x0d, rng.getrandbits(16))
            struct.pack_into('<H', rec, 0x0f, rng.getrandbits(16))
            rec[0x13] = rng.getrandbits(8)
            rec[0x14] = rng.getrandbits(8)
            rec[0x15] = rng.getrandbits(8) & 0x7f
            p.w('L', off, bytes(rec))
        else:
            item &= 0x1ff
            q = rng.getrandbits(6)
            # a link word only means something as a quantity or special property (is_quant): a plain object's would be
            # a chain of contents, and a random one a garbage chain
            quant = rng.random() < 0.2
            link = rng.choice([1, 2, 7, 0x200, 0x205]) if quant else 0
            p.w('L', off, struct.pack('<HHHH', item | (0x8000 if quant else 0), rng.getrandbits(16), q, link << 6))
        return spec

    def make_mobile(self, item, x8, y8, z, speed=None):
        """a new mobile non-creature (a missile or a thrown object) of item at x8, y8 (1/8 tiles) and z (object units),
        linked into its tile with its fine position in the goal and attitude words (mob_init's way); returns its index"""
        p, rng = self.p, self.rng
        spec = self.new_object(1, item)
        if spec is None:
            return None
        off = int(spec[2:], 16)
        tx, ty = x8 >> 3, y8 >> 3
        rec = bytearray(27)
        pos = (z & 0x7f) | rng.randint(0, 7) << 7 | (y8 & 7) << 10 | (x8 & 7) << 13
        struct.pack_into('<HHHH', rec, 0, item, pos, 0, 0)
        rec[8] = rng.choice([0x3f, 0x3f, 0x20, 1, 0])      # hit points
        rec[9] = rng.getrandbits(8)                        # heading
        rec[0x0a] = (rng.randint(0, 4) << 4) | 1           # terrain code and the bin
        struct.pack_into('<H', rec, 0x0b, (tx << 8) + ((x8 & 7) << 5) + rng.randint(0, 31))
        struct.pack_into('<H', rec, 0x0d, (ty << 8) + ((y8 & 7) << 5) + rng.randint(0, 31))
        struct.pack_into('<H', rec, 0x0f, (z << 3) + rng.randint(0, 7))
        spd = rng.randint(0, 0x7f) if speed is None else speed
        rec[0x13] = spd | rng.choice([0, 0x80])
        rec[0x14] = (rng.choice([16, 16, rng.randint(0, 31)]) << 3) | 2
        rec[0x15] = rng.choice([0, 0, 1])
        struct.pack_into('<H', rec, 0x16, (tx << 10) | (ty << 4))
        p.w('L', off, bytes(rec))
        t = ty * 64 + tx
        r = p.call('Obj_Add', [('f', 'L+%x' % (t * 4 + 2)), ('f', spec)], None)
        p.same('Obj_Add', r, self.ctx)
        return (off - 0x4000) // 27

    # --- the comparison -----------------------------------------------------------------------------------------------
    def check(self, ctx, res=None, count=None):
        p = self.p
        c = '%s level %d op %d %s' % (self.name, self.lev, self.total, ctx)
        if os.environ.get('PROOF_TRACE'):
            print(c, flush=True)
        if self.tainted:
            self.tainted = 0
            self.resync()
            self.cover('tainted (not compared)')
            self.refresh(lazy=True)
            return c
        if res is not None:
            p.same('return', res, c)
            parts = res[0][1].split(' ', 2)
            if len(parts) > 2:
                for nm in set(parts[2].split(',')):
                    self.cover('call ' + nm.split('(')[0])
        for nm, k in self.state:
            p.same_bytes(nm, 0, k, c)
        p.same_bytes('L', 0, 0x7c08, c)
        a = p.r('MP', 4, 0x1d - 4, hoff=16)       # frac .. hit (the host's pointers are 8 bytes: +12)
        p.same('MP', a, c)
        a = p.r('MP', 0x1d, 0x37 - 0x1d, hoff=42)  # item .. headings (the host aligns item: +13)
        p.same('MP', a, c)
        p.same('MP.vel', self.qn('MP', 0), c)
        p.same('MP.pos', self.qn('MP', 2, hoff=8), c)
        for nm in self.nears:
            a, b = self.qn(nm, 0)
            if nm == 'curP':   # left pointing at a routine's stack record (gone on return): only 'Ppd', none or elsewhere
                a, b = [v if v in ('@Ppd+0', '0') else 'stack' for v in (a, b)]
            p.same(nm, (a, b), c)
        for nm in self.fars:
            p.same(nm, p.q(nm, 0), c)
        for nm in OBJ_PTRS:
            p.same(nm, p.q(nm, 0), c)
        for nm in ('PT', 'CT1', 'CT2', 'CT3', 'CT4'):
            p.same(nm + '.special', p.q(nm, 8, hoff=8), c)
        p.same('rand', p.gseed(), c)
        self.refresh(lazy=True)
        return c

    def qn(self, name, off=0, hoff=None):
        """a near pointer on both sides (a null one is DS:0 in the emulator, which q cannot name)"""
        a, b = self.p.q(name, off, near=True, hoff=hoff)
        if a == '?' and self.p.e.rw(DGROUP, self.p.data[name] + off) == 0:
            a = '0'
        return a, b

    def resync(self):
        """our side made equal to the original's: the level block, the plain state, the pointers and rand's state"""
        p = self.p
        p.w('L', 0, p.e.read(0xC000, 0, 0x7c08))
        for nm, k in self.state:
            p.w(nm, 0, p.e.read(DGROUP, p.data[nm], k))
        p.w('MP', 4, p.e.read(DGROUP, p.data['MP'] + 4, 0x1d - 4), hoff=16)
        p.w('MP', 0x1d, p.e.read(DGROUP, p.data['MP'] + 0x1d, 0x37 - 0x1d), hoff=42)
        for nm in self.nears:
            a, b = self.qn(nm, 0)
            if a != b and a.startswith(('@', '0')) and nm != 'curP':
                p.p(nm, 0, a, near=True)
        a, b = self.qn('MP', 0)
        if a != b and a.startswith(('@', '0')):
            p.p('MP', 0, a, near=True)
        a, b = self.qn('MP', 2, hoff=8)
        if a != b and a.startswith('@'):
            p.p('MP', 2, a, near=True, hoff=8)
        for nm in self.fars + OBJ_PTRS:
            a, b = p.q(nm, 0)
            if a != b and a != '?':
                p.p(nm, 0, a)
        p.seed(p.e.seed())

    def cover(self, key, cond=True):
        """coverage counters (printed with PROOF_COV=1)"""
        if cond:
            self.cov[key] = self.cov.get(key, 0) + 1

    def ppd(self):
        b = self.p.e.read(DGROUP, self.p.data['Ppd'], 23)
        t = struct.unpack('<hhhHBBhhhBBBBBBb', b)
        return dict(x=t[0], y=t[1], z=t[2], heading=t[3], radius=t[4], height=t[5], index=t[6], hits0=t[7], hits1=t[8],
                    floor=t[9], top=t[10], slope=t[11], open=t[12], found=t[13], count=t[14], first=t[15])

    def done(self, kind, n=1):
        self.counts[kind] = self.counts.get(kind, 0) + n
        self.total += 1

    def summary(self, label):
        txt = ', '.join('%s %d' % kv for kv in sorted(self.counts.items()))
        if self.cov.get('tainted (not compared)'):
            txt += '; %d operations not compared (they read memory the original leaves undefined)' % self.cov['tainted (not compared)']
        print('%-15s %d operations on 9 levels  same (%s)' % (label, self.total, txt))
        if os.environ.get('PROOF_COV'):
            print('coverage:', ', '.join('%s %d' % kv for kv in sorted(self.cov.items())))
        print('all the same')


def run(label, main):
    try:
        main()
    except Differ as ex:
        print('%s DIFFER:' % label, ex)
        sys.exit(1)
    except Exception as ex:   # an unsound input, a crashed emulator: say where
        print('%s ERROR: %s: %s (%s)' % (label, type(ex).__name__, ex, CURRENT.ctx if CURRENT else ''))
        sys.exit(2)
