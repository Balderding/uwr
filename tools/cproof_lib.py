"""cproof_lib.py: the common ground of the combat and creature proofs (proof_combat, proof_ai, proof_creature,
proof_crittime, proof_crpages, proof_pathfind). It wraps coreproof.Pair: loads the class tables of OBJECTS.DAT on both
sides, sets the player record and the critter globals, picks real critters of the level and puts the player near them,
scrambles a critter's AI fields soundly, and compares the whole state after a call (return, outside calls, Borland
rand's state, the level block, far buffers and a list of globals).
"""
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair  # noqa: E402
from emu import DGROUP  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE_DS = DGROUP

# plain (no pointer) globals the critter / combat code reads or writes: name -> bytes
CRIT_GLOBALS = {
    'crit_terr': 2, 'doorx': 1, 'doory': 1, 'tdx': 2, 'tdy': 2, 'control': 1, 'txpos': 1, 'aligned': 1,
    'dontchangedz': 1, 'txpost': 2, 'myid': 1, 'tdistsqr': 2, 'failed': 1, 'typos': 1, 'tdisttsqr': 4,
    'pathlen': 1, 'myxpos': 1, 'myypos': 1, 'myzpos': 1, 'hitwall': 1, 'typost': 2, 'myxhome': 1, 'myyhome': 1,
    'myheight': 1, 'didhitobj': 1, 'myxpost': 2, 'myypost': 2, 'didmove': 1, 'hitadoor': 1, 'myoldspeed': 1,
    'tzpos': 1, 'XP': 2, 'myoldfacing': 1, 'YP': 2, 'myoldheading': 1, 'freepaths': 2, 'lastXeye': 2, 'lastYeye': 2,
    'lastcombattime': 4, 'crithittime': 4, 'hitx': 1, 'hity': 1, 'curBin': 1, 'lastbin': 1, 'crithit': 1,
    'typehit': 1, 'CN1': 40, 'CN2': 40, 'CN3': 40, 'CN4': 40, 'PN': 40, 'Ppd': 23, 'oCollisions': 48,
}
COMBAT_GLOBALS = {
    'askill': 2, 'wsize': 2, 'towhere': 2, 'play_pow': 1, 'targx': 2, 'targy': 2, 'hitobj': 2, 'cmbModTH': 4,
    'hitz': 1, 'damage': 2, 'hitloc': 2, 'hitangle': 1, 'criti': 1, 'fromwho': 2, 'power': 1, 'pQatt': 2,
    'attackKey': 2, 'missile_try': 2, 'PlayerDat': 0xD2, 'GameInputMode': 2, 'Inventory': 0x38,
}
PTRS = {  # name: near?
    'meptr': False, 'mytarget': False, 'collobject': False, 'mycst': True, 'tp_act': True, 'pn_act': True,
    'ActiveObj': False, 'UsPtr': False, 'CursorObjPtr': False,
}
HANDLERS = ['CT1', 'CT2', 'CT3', 'CT4']  # 8 plain bytes then a function pointer
FAR_BUFFERS = {'pathsq': (0x55DD, 252), 'paths': (0x55ED, 448), 'stdat': (0x3F4B, 0x5000)}

MOBILE_FIRST = 2


def hexs(b):
    return bytes(b).hex()


class World:
    def __init__(self, game, coretest, seed=1):
        self.game = game
        self.p = Pair(game, coretest)
        self.rng = random.Random(seed + int(os.environ.get('PROOF_SEED', '0')) * 1000003)   # PROOF_SEED=n: another run
        self.level_no = 0
        # a critter with a conversation (whoami) runs its death conversation script when it dies (death_check): the
        # conversation needs the screen, so critters keep whoami 0, except where nothing kills (crittime reads names)
        self.allow_whoami = False
        self.pl_on = False
        self.names = set(self.p.host)
        # the class tables of OBJECTS.DAT (a 2 byte header, Weapons, Missile, Armor, Creature)
        d = open(os.path.join(game, 'DATA', 'OBJECTS.DAT'), 'rb').read()
        self.objects_dat = d
        self.tables = [('Weapons', 128), ('Missile', 48), ('Armor', 128), ('Creature', 64 * 48)]
        at = 2
        for name, n in self.tables:
            self.p.w(name, 0, d[at:at + n])
            at += n
        self.creature = bytearray(d[2 + 128 + 48 + 128:2 + 128 + 48 + 128 + 64 * 48])
        self.sizes = {}
        self.sizes.update(CRIT_GLOBALS)
        self.sizes.update(COMBAT_GLOBALS)
        self.skipped = [n for n in self.sizes if n not in self.p.data or n not in self.names]
        self.sizes = dict((n, s) for n, s in self.sizes.items() if n not in self.skipped)

    # ---- setup ------------------------------------------------------------------------------------------------
    def level(self, n):
        p = self.p
        self.level_no = n
        self.pl_on = False  # LEV.ARK's player object is on no tile list until it enters
        p.level(n)
        rng = self.rng
        p.p('player', 0, '@PlayerDat+0', near=True)
        p.p('playerdat', 0, '@Creature+%x' % (63 * 48), near=True)
        p.w('PlayerLevel', 0, struct.pack('<h', n))
        p.w('GameInputMode', 0, struct.pack('<h', 0))
        for nm, near in PTRS.items():
            if nm in p.data and nm in self.names:
                p.p(nm, 0, '0', near=near)
        p.p('UsPtr', 0, 'L+%x' % 0x401b)
        # inplist (the input mode record UseObj reads, a stub) points at eight zero bytes of DS
        p.w('Inventory', 0x30, bytes(8))
        if 'inplist' in p.data:
            p.p('inplist', 0, '@Inventory+30', near=True)
        p.call('init_ai', [], None)
        p.w('freepaths', 0, struct.pack('<H', 0xFFFF))
        # fixed far buffers
        self.zero_stdat()
        self.fill_player()
        self.relevel_state()

    def relevel_state(self):
        p = self.p
        for nm, v in (('lastXeye', 0), ('lastYeye', 0)):
            p.w(nm, 0, struct.pack('<h', v))
        p.w('curBin', 0, b'\0')
        p.w('lastbin', 0, b'\0')
        p.w('crithit', 0, b'\0')
        p.w('typehit', 0, b'\xff')

    def wfar(self, name, off, data):
        """write far data (a name with a segment:offset symbol) on both sides"""
        seg, o0 = self.p.code[name]
        self.p.e.write(seg, o0 + off, data)
        self.p._cmd('w %s %x %s' % (name, off, bytes(data).hex()))

    def zero_stdat(self):
        z = b'\0' * 0x5000
        self.wfar('stdat', 0, z)
        self.wfar('pathsq', 0, b'\0' * 252)
        self.wfar('paths', 0, b'\0' * 448)

    def fill_player(self):
        rng = self.rng
        p = self.p
        rec = bytearray(rng.getrandbits(8) for _ in range(0xD2))
        rec[0:0x1E] = b'\0' * 0x1E
        for o in (0x1E, 0x1F, 0x20):
            rec[o] = rng.randint(8, 40)
        for i in range(20):
            rec[0x21 + i] = rng.randint(0, 30)
        rec[0x35] = rng.randint(5, 60)
        rec[0x36] = rec[0x35] + rng.randint(0, 20)
        # the bits the combat code reads: drawn (word 0x5F bit 1), poison (bits 2-5), orb (word 0x60 bit 5),
        # lefty (0x64 bit 0), easy
        rec[0x5F] = (rec[0x5F] & ~0x3E) | (rng.choice([0, 2]) << 0) | (rng.choice([0, 0, 0, 1, 4, 9]) << 2)
        rec[0x5F] = (rec[0x5F] & ~2) | rng.choice([0, 2, 2])
        rec[0x60] = (rec[0x60] & ~0x20) | rng.choice([0, 0x20])
        rec[0x64] = (rec[0x64] & ~1) | rng.choice([0, 1])
        rec[0x61] &= ~0x0C   # no mushrooms: set_drugged would load a palette / light file by the C library's low-level open
        rec[0xB4] = rng.choice([0, 1])
        rec[0xB6] = (rec[0xB6] & 0xF8) | rng.randint(0, 6)   # fps (the movement state): newFPS indexes seven entries with it
        rec[0x69] = rng.choice([0, 1, 3, 4])
        # the carried weight and the most: a left over of -10000 or less (tenths of a stone) overflows displayEnc's
        # 5 byte buffer on DOS onto its result (the original's own bug), which no game reaches
        struct.pack_into('<HH', rec, 0x4a, rng.randint(0, 900), rng.choice([rng.randint(200, 1200), rng.randint(0, 1200)]))
        struct.pack_into('<I', rec, 0xCE, rng.randint(0, 0x20000))
        struct.pack_into('<I', rec, 0xBC, rng.randint(0, 0x20000))
        p.w('PlayerDat', 0, rec)
        # the player's creature record (63)
        c = bytearray(self.creature[63 * 48:64 * 48])
        c[4] = rng.randint(20, 80)
        for i in range(3):
            c[5 + i] = rng.randint(8, 40)
        c[0x12] = rng.randint(-5, 25) & 0xFF
        c[0x1D] = rng.randint(0, 15) << 0 | rng.randint(0, 15) << 4
        p.w('Creature', 63 * 48, c)
        # the player's object (object 1): an adventurer (LEV.ARK's is stale), off the lists until place_player links it
        po = bytearray(27)
        struct.pack_into('<HHHH', po, 0, 0x7f, 0, 0, 0)
        po[8] = rng.randint(10, 60)
        po[0x16:0x18] = bytes(2)
        if self.pl_on:
            p.w('L', 0x4000 + 27 + 8, bytes([po[8]]))   # linked: its place and list link stay
        else:
            p.w('L', 0x4000 + 27, bytes(po))
        # mobile objects parked on the map's edge tiles (the shipped levels have some): every scan that goes a tile
        # round them reads before the map's start, which is another thing in DOS and on the host. They are taken off
        # their lists and out of the active list with the proven routines
        blk0 = self.block()
        for i in range(2, 256):
            o = 0x4000 + i * 27
            idv, = struct.unpack_from('<H', blk0, o)
            if idv & 0x1ff:
                home, = struct.unpack_from('<H', blk0, o + 0x16)
                hx, hy = (home >> 10) & 63, (home >> 4) & 63
                if not (2 <= hx <= 61 and 2 <= hy <= 61):
                    if self.on_tile(blk0, i, hx, hy):
                        r = p.call('Obj_Rem', [('f', 'L+%x' % ((hy * 64 + hx) * 4 + 2)), ('f', 'L+%x' % o)], None)
                        p.same('edge Obj_Rem', r)
                    r = p.call('free_critter', [('w', i)], None)
                    p.same('edge free_critter', r)
                    p.w('L', o, bytes(2))
                    p.w('L', o + 8, bytes(1))
        # a few creatures of the shipped levels carry a terrain code (b0A bits 4-6) of 5-7, which indexes past
        # res_to_terr in set_phys_data (an out-of-bounds read, another thing in the core): given 0 on both sides
        blk = self.block()
        for i in range(2, 256):
            o = 0x4000 + i * 27
            if (blk[o + 10] >> 4) & 7 > 4:
                p.w('L', o + 10, bytes([blk[o + 10] & 0x8f]))
            # a conversation number (whoami) with no name string leaves get_name's caller printing an uninitialised
            # buffer (critter_get_told): the shipped levels carry a few of those; names exist for 1..28
            gw, = struct.unpack_from('<H', blk, o + 0x0B)
            if (gw & 0xF) == 10 and not self.allow_whoami:
                gw = (gw & 0xFFF0) | 8
                p.w('L', o + 0x0B, struct.pack('<H', gw))
            if (gw & 0xF) in (3, 5, 6, 9) and not (gw >> 4) & 0xFF:
                p.w('L', o + 0x0B, struct.pack('<H', gw | 0x10))   # the Slasher of Veils' follow goal has no target
            if blk[o + 0x1a] > (28 if self.allow_whoami else 0):
                p.w('L', o + 0x1a, bytes(1))

    def obj_off(self, i):
        return 0x4000 + i * 27 if i < 256 else 0x5b00 + (i - 256) * 8

    # ---- the level --------------------------------------------------------------------------------------------
    def block(self):
        return self.p.e.read(0xC000, 0, 0x7c08)

    def critters(self, block=None):
        """indices of the creatures (item 0x40..0x7f) that are on a tile list with an owned map position"""
        b = block or self.block()
        out = []
        for i in range(MOBILE_FIRST, 256):
            o = 0x4000 + i * 27
            idv, = struct.unpack_from('<H', b, o)
            if 0x40 <= (idv & 0x1ff) < 0x80:
                home, = struct.unpack_from('<H', b, o + 0x16)
                x, y = (home >> 10) & 63, (home >> 4) & 63
                # not on the map's edge: the collision scans read the tiles around (before the map's start, which
                # is another thing in DOS and on the host)
                if 2 <= x <= 61 and 2 <= y <= 61 and self.on_tile(b, i, x, y):
                    out.append(i)
        return out

    def mobiles(self, block=None):
        b = block or self.block()
        out = []
        for i in range(MOBILE_FIRST, 256):
            o = 0x4000 + i * 27
            idv, = struct.unpack_from('<H', b, o)
            if (idv & 0x1ff) != 0:
                home, = struct.unpack_from('<H', b, o + 0x16)
                x, y = (home >> 10) & 63, (home >> 4) & 63
                if 2 <= x <= 61 and 2 <= y <= 61 and self.on_tile(b, i, x, y):
                    out.append(i)
        return out

    def chain(self, b, link, guard=300):
        out = []
        while link and guard:
            out.append(link)
            off = 0x4000 + link * 27 if link < 256 else 0x5b00 + (link - 256) * 8
            link = struct.unpack_from('<H', b, off + 4)[0] >> 6
            guard -= 1
        return out

    def on_tile(self, b, i, x, y):
        t = (y * 64 + x) * 4
        head = struct.unpack_from('<H', b, t + 2)[0] >> 6
        return i in self.chain(b, head)

    def tile(self, b, x, y):
        w, = struct.unpack_from('<H', b, (y * 64 + x) * 4)
        return w & 15, (w >> 4) & 15  # type, height

    def open_tiles_near(self, b, x, y, r, margin=1):
        out = []
        for ty in range(max(margin, y - r), min(63 - margin, y + r) + 1):
            for tx in range(max(margin, x - r), min(63 - margin, x + r) + 1):
                t, h = self.tile(b, tx, ty)
                if t == 1:
                    out.append((tx, ty, h))
        return out

    def crit_xy(self, b, i):
        home, = struct.unpack_from('<H', b, self.obj_off(i) + 0x16)
        return (home >> 10) & 63, (home >> 4) & 63

    def place_player(self, tx, ty, fx, fy, z, heading=None):
        """the player (object 1) moved to a tile and fine place: off the old tile's list, onto the new one's (the
        proven Obj_Rem and Obj_Add on both sides), position and heading fields"""
        p = self.p
        b = self.block()
        ox, oy = self.crit_xy(b, 1)
        if not self.pl_on or (ox, oy) != (tx, ty):
            if self.pl_on:
                r = p.call('Obj_Rem', [('f', 'L+%x' % ((oy * 64 + ox) * 4 + 2)), ('f', 'L+%x' % 0x401b)], None)
                p.same('place Obj_Rem', r)
            self.pl_on = True
            r = p.call('Obj_Add', [('f', 'L+%x' % ((ty * 64 + tx) * 4 + 2)), ('f', 'L+%x' % 0x401b)], None)
            p.same('place Obj_Add', r)
        pos, = struct.unpack_from('<H', b, 0x401b + 2)
        if heading is None:
            heading = (pos >> 7) & 7
        pos = (z & 0x7f) | (heading << 7) | (fy << 10) | (fx << 13)
        home = (tx << 10) | (ty << 4)
        p.w('L', 0x401b + 2, struct.pack('<H', pos))
        p.w('L', 0x401b + 0x16, struct.pack('<H', home))

    def player_near(self, ci, radius=None, b=None, margin=1):
        """the player on an open tile within radius tiles of critter ci (more, if there is none); returns whether he is within
        16 tiles in both axes: the distance sums of the AI are 16 bit in DOS (23 tiles or more apart wrap) and the core's
        int is not, which no game reaches (the AI runs for critters near the player), so the proofs keep to it"""
        rng = self.rng
        b = b or self.block()
        x, y = self.crit_xy(b, ci)
        r = radius if radius is not None else rng.choice([0, 1, 1, 2, 2, 3, 4, 6, 9])
        tiles = []
        for rr in (r, r * 2 + 1, 8, 14):
            tiles = self.open_tiles_near(b, x, y, rr, margin)
            if tiles:
                break
        if not tiles:
            return False
        tx, ty, h = rng.choice(tiles)
        self.place_player(tx, ty, rng.randint(0, 7), rng.randint(0, 7), (h << 3) & 0x7f if h < 16 else 0,
                          rng.randint(0, 7))
        return abs(tx - x) <= 16 and abs(ty - y) <= 16

    def scramble_critter(self, ci, hostile=None, target=None):
        """soundly random AI fields of mobile ci (never its place on the lists)"""
        rng = self.rng
        p = self.p
        b = self.block()
        o = self.obj_off(ci)
        cur = bytearray(b[o:o + 27])
        ids, = struct.unpack_from('<H', cur, 0)
        item = ids & 0x1ff
        cr = self.creature[(item & 0x3f) * 48:(item & 0x3f) * 48 + 48]
        avg = cr[4] if cr[4] else 10
        cur[8] = rng.choice([1, 2, max(1, avg // 4), max(1, avg // 2), avg, avg, min(255, avg + avg // 2)])
        cur[9] = rng.getrandbits(8)
        cur[0x0A] = (cur[0x0A] & 0xF0) | rng.randint(0, 15)
        if rng.random() < 0.15:
            cur[0x0A] ^= 0x80  # loner
        goal = rng.choice([0, 1, 2, 3, 4, 5, 5, 5, 6, 8, 9, 10, 11, 12, 13])
        if goal == 10 and not self.allow_whoami:
            goal = 8   # talk: crit_talk starts a conversation (TalkTo) with the player, which needs the screen
        if target is None:
            target = rng.choice([1, 1, 1, 1, ci, 0, rng.randint(2, 255)])
        if target == 0 and goal in (3, 5, 6, 9):
            target = 1  # these goals call set_up_target, whose target 0 is a null far pointer (DOS reads its own low memory)
        frame = rng.choice([0, 1, 2, 3, 4, 0, 3])
        struct.pack_into('<H', cur, 0x0B, goal | (target << 4) | (frame << 12))
        att = rng.choice([0, 0, 1, 2, 3]) if hostile is None else (0 if hostile else rng.choice([1, 2, 3]))
        aw = rng.choice([0, 0, 4, 5, 6, 8]) | (rng.randint(0, 15) << 4) | (att << 14)
        if rng.random() < 0.15:
            aw |= 0x400  # powerful
        if rng.random() < 0.1:
            aw |= 0x200
        if rng.random() < 0.05:
            aw |= 0x100  # temporary
        struct.pack_into('<H', cur, 0x0D, aw)
        struct.pack_into('<H', cur, 0x0F, rng.randint(0, 63) | (rng.randint(0, 63) << 6) | (rng.randint(0, 15) << 12))
        cur[0x11] = rng.choice([0, 0, 0, 3, 40, 200])
        cur[0x12] = rng.choice([0, 0, 0, 1, 1, ci if rng.random() < 0.5 else rng.randint(2, 255)])
        cur[0x13] = rng.choice([0, 0, 1, 2, 5, 11, 20])
        cur[0x14] = rng.randint(1, 7) | (rng.choice([0x10, 0x10, 0x10, rng.randint(0, 31)]) << 3) & 0xF8
        seq = rng.choice([0x20, 0x2C, 0x20, 0x2C, 0, 1, 2, 3, 5, 7, 0xC, 0xD, 0x20, 0x2C, rng.randint(0, 0x3f)])
        cur[0x15] = seq | rng.choice([0, 0, 0, 0, 0x40])
        if seq in (5, 0xD) and not (target or 0):
            # firing and casting aim with set_up_target: target 0 is a null far pointer
            gw, = struct.unpack_from('<H', cur, 0x0B)
            struct.pack_into('<H', cur, 0x0B, gw | 0x10)
        # home low nibble: path slot (b15 bit 7 is only ever set by the routines, with a stored path)
        h, = struct.unpack_from('<H', cur, 0x16)
        struct.pack_into('<H', cur, 0x16, (h & 0xFFF0) | rng.randint(0, 15))
        cur[0x18] = rng.randint(0, 31) | rng.choice([0, 0, 0x20, 0x40])
        cur[0x19] = (rng.getrandbits(8) & rng.getrandbits(8)) | rng.choice([0, 0, 1])
        cur[0x1A] = rng.choice([0, 0, 0, 0, 5, 0x16]) if self.allow_whoami else 0
        # quality and owner are the AI's home tile (in the map); keep the list links in their words
        q, ow = rng.randint(1, 62), rng.randint(1, 62)
        if rng.random() < 0.6:
            x, y = self.crit_xy(b, ci)
            q, ow = max(1, min(62, x + rng.randint(-3, 3))), max(1, min(62, y + rng.randint(-3, 3)))
        qn, = struct.unpack_from('<H', b, o + 4)
        ol, = struct.unpack_from('<H', b, o + 6)
        struct.pack_into('<H', cur, 4, (qn & 0xFFC0) | q)
        struct.pack_into('<H', cur, 6, (ol & 0xFFC0) | ow)
        # id and z stay; the fine place within the tile is random
        pos0, = struct.unpack_from('<H', b, o + 2)
        pos = (pos0 & 0x7F) | (rng.randint(0, 7) << 7) | (rng.randint(0, 7) << 10) | (rng.randint(0, 7) << 13)
        struct.pack_into('<H', cur, 2, pos)
        cur[0:2] = b[o:o + 2]
        # the home word keeps its tile (y 4-9, x 10-15)
        h0, = struct.unpack_from('<H', b, o + 0x16)
        h, = struct.unpack_from('<H', cur, 0x16)
        struct.pack_into('<H', cur, 0x16, (h0 & 0xFFF0) | (h & 0xF))
        p.w('L', o, bytes(cur))

    # ---- comparing --------------------------------------------------------------------------------------------
    def read_many(self, reqs):
        """reqs: list of (name, off, n); returns (orig, ours) lists of hex strings, pipelined on our side"""
        p = self.p
        mine = []
        lines = []
        for name, off, n in reqs:
            if name == 'L':
                mine.append(p.e.read(0xC000, off, n).hex())
                lines.append('r L+0 %x %x' % (off, n))
            elif name not in p.data and name in p.code:
                seg, o0 = p.code[name]
                mine.append(p.e.read(seg, o0 + off, n).hex())
                lines.append('r %s %x %x' % (name, off, n))
            else:
                mine.append(p.e.read(DGROUP, p.addr(name) + off, n).hex())
                lines.append('r %s %x %x' % (name, off, n))
        p.c.stdin.write('\n'.join(lines) + '\n')
        p.c.stdin.flush()
        ours = [p.c.stdout.readline().rstrip('\n') for _ in lines]
        return mine, ours

    def compare(self, ctx, extra=(), far=('pathsq',), stdat=False, paths=True, level=True, ptrs=True, scratch=True):
        p = self.p
        reqs = []
        if level:
            reqs.append(('L', 0, 0x7c08))
        if scratch:
            reqs.append(('L', LSCR, 0x300))
        reqs += [(n, 0, s) for n, s in self.sizes.items()]
        for n in extra:
            if isinstance(n, tuple):
                reqs.append((n[0], 0, n[1]))
                continue
            if n in self.sizes:
                continue
            reqs.append((n, 0, self.xsize(n)))
        for n in HANDLERS:
            reqs.append((n, 0, 8))
        for n in far:
            reqs.append((n, 0, FAR_BUFFERS[n][1]))
        if paths:
            reqs.append(('paths', 0, 448))
        if stdat:
            reqs.append(('stdat', 0, 0x5000))
        mine, ours = self.read_many(reqs)
        for (n, off, sz), a, b in zip(reqs, mine, ours):
            if n == 'paths':
                # the bits of a stored path beyond its length are junk (store_path reads the path squares past the
                # end, whatever a stale one makes of them): compare what the path means
                for k in range(16):
                    ra, rb = bytes.fromhex(a)[k * 28:k * 28 + 28], bytes.fromhex(b)[k * 28:k * 28 + 28]
                    ok = ra[:4] == rb[:4]
                    for st in range(ra[3] if ok else 0):
                        ok = ok and (ra[4 + st // 4] >> (st % 4 * 2) & 3) == (rb[4 + st // 4] >> (st % 4 * 2) & 3)
                        ok = ok and (ra[20 + st // 8] >> (st % 8) & 1) == (rb[20 + st // 8] >> (st % 8) & 1)
                    if not ok:
                        raise Differ('paths[%d] differs (%s): original %s ours %s' % (k, ctx, ra.hex(), rb.hex()))
                continue
            if a != b:
                i = next(k for k in range(0, len(a), 2) if a[k:k + 2] != b[k:k + 2]) // 2
                raise Differ('%s+%x differs at byte %x (%s):\n  original %s\n  ours     %s' % (
                    n, off, off + i, ctx, a[max(0, i * 2 - 16):i * 2 + 48], b[max(0, i * 2 - 16):i * 2 + 48]))
        if level:
            check_lists(self, ctx)
        if ptrs:
            for n, near in PTRS.items():
                if n in p.data and n in self.names:
                    p.same(n, self.q(n, 0, near), ctx)
            for n in HANDLERS:
                p.same(n + '.special', p.q(n, 8, near=False), ctx)

    XS = {'ObjectActor': 2}

    def xsize(self, n):
        s = {'Hasted': 1, 'TimeStop': 1, 'wander_found': 1, 'hostile_found': 1, 'play_pow': 1, 'inanmMapX': 1, 'inanmMapY': 1,
             'mspell_mused': 1, 'MoveCrits': 1, 'stay_centered': 1}
        return s.get(n, 2)

    def q(self, name, off=0, near=False, hoff=None):
        """Pair.q with a null near pointer said as '0' on the DOS side too"""
        mine, ours = self.p.q(name, off, near=near, hoff=hoff)
        if near and self.p.e.rw(DGROUP, self.p.addr(name) + off) == 0:
            mine = '0'
        return mine, ours

    def rand_state(self, ctx):
        self.p.same('rand', self.p.gseed(), ctx)


def main_wrap(name, fn):
    try:
        fn()
    except Differ as ex:
        print('%s DIFFER:' % name, ex)
        sys.exit(1)


# ---- helpers shared by the path finding and AI proofs ------------------------------------------------------------
SCR = '@Inventory+28'      # 16 bytes of DS both sides have, for a near out parameter
SCR_OFF = 0x28
LSCR = 0x9000              # the free part of the level block: far out parameters and path records


def world_helpers():
    def set_vars(self, ci):
        r = self.p.call('set_critter_vars', [('f', 'L+%x' % self.obj_off(ci))], None)
        self.p.same('set_critter_vars', r)

    def rand_flags(self, control=None):
        rng = self.rng
        p = self.p
        flags = {'control': control if control is not None else rng.choice([0, 1, 1, 1, 1]),
                 'failed': rng.choice([0, 0, 1]), 'aligned': rng.choice([0, 0, 0, 1]),
                 'hitwall': rng.choice([0, 0, 1]), 'didhitobj': rng.choice([0, 0, 1]),
                 'hitadoor': rng.choice([0, 0, 1]), 'dontchangedz': rng.choice([0, 0, 1]),
                 'didmove': rng.choice([0, 0, 0, 1])}
        for k, v in flags.items():
            p.w(k, 0, bytes([v]))
        # a hit object (didhitobj) is what the handler left in collobject; a door when hitadoor
        b = self.block()
        ds = self.doors(b) if flags['hitadoor'] else []
        if flags['didhitobj'] or flags['hitadoor']:
            if flags['hitadoor'] and ds:
                o = rng.choice(ds)
            else:
                flags['hitadoor'] = 0
                p.w('hitadoor', 0, bytes(1))
                o = rng.choice(self.mobiles(b) or [1])
            p.p('collobject', 0, 'L+%x' % self.obj_off(o))

    def rand_ppd(self, ci, doors=False):
        """Ppd and oCollisions for the collision handlers: count objects near the critter"""
        rng = self.rng
        p = self.p
        b = self.block()
        x, y = self.crit_xy(b, ci)
        cnt = rng.choice([0, 1, 1, 2, 3])
        first = rng.randint(0, 4)
        ppd = bytearray(p.r('Ppd', 0, 23)[0] and bytes.fromhex(p.r('Ppd', 0, 23)[0]))
        struct.pack_into('<hh', ppd, 0, (x << 3) + rng.randint(0, 7), (y << 3) + rng.randint(0, 7))
        ppd[0x15] = cnt
        ppd[0x16] = first
        p.w('Ppd', 0, bytes(ppd))
        coll = bytearray(48)
        objs = self.chain_tile_objects(b, x, y) or [1]
        for i in range(8):
            o = rng.choice(objs)
            if doors and rng.random() < 0.7:
                ds = self.doors(b)
                if ds:
                    o = rng.choice(ds)
            dx, dy = rng.choice([(0, 0), (0, 0), (1, 0), (0, 1), (-1, 0), (0, -1)])
            struct.pack_into('<BBHh', coll, i * 6, rng.randint(0, 255), rng.randint(0, 255), (o << 6) | rng.randint(0, 63),
                             dy * 64 + dx)
        p.w('oCollisions', 0, bytes(coll))

    def chain_tile_objects(self, b, x, y):
        head = struct.unpack_from('<H', b, (y * 64 + x) * 4 + 2)[0] >> 6
        return self.chain(b, head)

    def doors(self, b):
        out = []
        for t in range(4096):
            head = struct.unpack_from('<H', b, t * 4 + 2)[0] >> 6
            for o in self.chain(b, head):
                off = 0x4000 + o * 27 if o < 256 else 0x5b00 + (o - 256) * 8
                item = struct.unpack_from('<H', b, off)[0] & 0x1ff
                if 0x140 <= item < 0x148:
                    out.append(o)
        return out

    for fn in (set_vars, rand_flags, rand_ppd, chain_tile_objects, doors):
        setattr(World, fn.__name__, fn)


world_helpers()


def diag_all(w, limit=0x80):
    """a diagnostic: every global both sides have, compared raw (pointers differ by nature) up to its symbol gap"""
    p = w.p
    names = []
    ds = sorted((a, n) for n, a in p.data.items())
    sizes = {}
    for i, (a, n) in enumerate(ds):
        nxt = ds[i + 1][0] if i + 1 < len(ds) else a + 2
        sizes[n] = min(nxt - a, limit)
    reqs = [(n, 0, sizes[n]) for n in p.known if n in sizes and sizes[n] > 0]
    mine, ours = w.read_many(reqs)
    out = []
    for (n, _, sz), a, b in zip(reqs, mine, ours):
        if a != b:
            out.append((n, sz, a[:48], b[:48]))
    return out


def check_lists(w, ctx):
    """the tile lists stay sound: no cycle, no object on two lists (a broken list hangs the original's loops)"""
    b = w.block()
    seen = {}
    for t in range(4096):
        link = struct.unpack_from('<H', b, t * 4 + 2)[0] >> 6
        n = 0
        while link:
            if link in seen:
                raise Differ('object %d on two lists / a cycle: tiles %d and %d (%s)' % (link, seen[link], t, ctx))
            seen[link] = t
            off = 0x4000 + link * 27 if link < 256 else 0x5b00 + (link - 256) * 8
            link = struct.unpack_from('<H', b, off + 4)[0] >> 6
            n += 1
            if n > 1100:
                raise Differ('tile %d list too long (%s)' % (t, ctx))
