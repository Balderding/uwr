"""magic_common.py: what the magic and trap proofs (proof_spells, proof_runes, proof_trigger, proof_trigsave,
proof_worldev) share. It builds on motion_common.World (the game's files on both sides, init_ai, the player record,
the clock, the player object placed on a real level, and one full comparison: return, outside calls, rand, the
whole level block, the player record, the physics records, the pointers) and adds: the data the magic reads, the
globals the magic and the traps write, a level that keeps its real triggers and traps, a player put near what is
being tested, and the checks of those globals.
"""
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import motion_common as mc  # noqa: E402
from motion_common import World, obj_off, chain, run, Skip  # noqa: E402
from coreproof import Differ  # noqa: E402
from emu import DGROUP  # noqa: E402

# plain globals the magic and trap code reads or writes: (name, bytes)
EXTRA = [('changed', 2), ('GameInputMode', 2), ('ObjectActorArg', 2), ('mspell_mused', 1), ('inanmMapX', 1),
         ('inanmMapY', 1), ('NewPlayerLevel', 2), ('NewPlayerX', 2), ('NewPlayerY', 2), ('EndGameMode_dseg_1C8F', 1),
         ('spell_delay', 1), ('lstime', 4), ('CreatedObjectFound_dseg_5c99_1D9A', 1), ('Triggers', 16),
         ('curvrad', 4), ('nextSpellTime', 4), ('Hasted', 1), ('TimeStop', 1), ('WizEye', 1), ('curBin', 2),
         ('Inventory', 0x38), ('stay_centered', 1), ('DoAnimO', 1)]
EXTRA_PTRS = ['ObjectActing', 'ObjectActor', 'npp_func', 'CursorObjPtr', 'ActiveObj', 'UsPtr']


def u16(b, o):
    return struct.unpack_from('<H', b, o)[0]


class MWorld(World):
    def __init__(self, game, coretest, seed, name, cover=False):
        World.__init__(self, game, coretest, seed, name)
        p = self.p
        if cover:
            p.coverage()   # from before init_objects (which runs trap_init)
        r = p.call('init_objects', [], 'w')
        p.same('init_objects', r)
        # a failing call leaves its level block and player record in work/crash_*.bin (to be set up again alone)
        guarded = p.call

        def keep(name, args=(), ret='w'):
            blk = p.e.read(0xC000, 0, 0x7c08)
            pdv = p.e.read(DGROUP, p.data['PlayerDat'], 0xd2)
            if os.environ.get('PROOF_DUMP'):   # before every call (for a crash that leaves no message)
                root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
                open(os.path.join(root, 'work', 'crash_block.bin'), 'wb').write(blk)
                open(os.path.join(root, 'work', 'crash_player.bin'), 'wb').write(pdv)
                open(os.path.join(root, 'work', 'crash_dg.bin'), 'wb').write(p.e.read(DGROUP, 0, 0x10000))
                open(os.path.join(root, 'work', 'crash_call.txt'), 'w').write('%s %r %r  level %s' % (name, args, ret, getattr(self, 'lev', 0)))
            try:
                return guarded(name, args, ret)
            except Differ:
                root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
                open(os.path.join(root, 'work', 'crash_block.bin'), 'wb').write(blk)
                open(os.path.join(root, 'work', 'crash_player.bin'), 'wb').write(pdv)
                open(os.path.join(root, 'work', 'crash_call.txt'), 'w').write('%s %r %r  level %s' % (name, args, ret, self.lev))
                raise
        p.call = keep
        self.state += [(n, k) for n, k in EXTRA if n in p.data and n in p.host and (n, k) not in self.state]
        self.xptrs = [n for n in EXTRA_PTRS if n in p.data and n in p.host]
        self.player_in = False

    # --- comparing -------------------------------------------------------------------------------------------
    def check(self, ctx, res=None, count=None):
        self.ctx = ctx
        c = World.check(self, ctx, res, count)
        p = self.p
        for n in self.xptrs:
            if n in ('ObjectActor', 'npp_func'):
                b = p.q(n, 0)[1]
                a2 = p.e.rw(DGROUP, p.data[n]), p.e.rw(DGROUP, p.data[n] + 2)
                # a function pointer: both sides say null or not the same way
                if (a2 == (0, 0)) != (b == '0'):
                    raise Differ('%s differs (%s): original %s ours %s' % (n, c, a2, b))
                continue
            p.same(n, p.q(n, 0), c)
        return c

    # --- the level ---------------------------------------------------------------------------------------------
    def level(self, n):
        """World.level, but the real triggers keep their traps (the trap system is what is being proved)"""
        p, rng = self.p, self.rng
        self.lev = n
        blk = p.level(n)
        for i in range(2, 256):
            o = 0x4000 + i * 27
            if (blk[o + 10] >> 4) & 7 > 4:
                p.w('L', o + 10, bytes([blk[o + 10] & 0x8f]))
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
        self.p.w('Creature', 63 * 48 + 0x1d, bytes([rng.randint(0, 255)]))
        self.fill_player()
        for nm in ('Hasted', 'TimeStop', 'WizEye', 'MoveCrits', 'UsingPole'):
            if nm in p.data:
                self.w8(nm, 0)
        self.w8('DoAnimO', 0)
        self.w8('MoveCrits', rng.randint(0, 1))
        self.np('mycst', '@Creature+%x' % (48 * rng.randint(0, 62)))
        self.w16('myxpost', rng.randint(0, 500))
        self.w16('myypost', rng.randint(0, 500))
        self.w16('GrSq', 0xffff)
        self.w16('GameInputMode', 0)
        po = bytearray(27)
        struct.pack_into('<HHHH', po, 0, 0x7f, 0, 0, 0)
        po[8] = rng.randint(20, 255)
        self.p.w('L', 0x4000 + 27, bytes(po))
        self.player_in = False
        self.refresh()
        self.stat_clean()

    def fill_player(self):
        """a random player record, sound for the magic: skills, level, mana, spells, runes, variables"""
        p, rng = self.p, self.rng
        pd = bytearray(rng.getrandbits(8) for _ in range(0xd2))
        pd[0:0x1e] = bytes(0x1e)
        for i in range(0x1e, 0x21):
            pd[i] = rng.randint(8, 30)
        for i in range(0x21, 0x35):
            pd[i] = rng.choice([0, rng.randint(0, 30), rng.randint(0, 63)])
        struct.pack_into('<HH', pd, 0x4a, rng.randint(0, 900), rng.choice([0, rng.randint(200, 1200)]))
        pd[0x6d] = rng.randint(0, 6)
        struct.pack_into('<H', pd, 0x5f, rng.getrandbits(16) & ~0x3c)
        pd[0x61] &= ~0x0c
        # active spells of the classes the game's casting makes (class 13 minor 4 is the maze spell, which on level 7 swaps in
        # a texture file when FixPlayerEquips runs), as many as the record says
        n = rng.randint(0, 3)
        for k in range(3):
            struct.pack_into('<H', pd, 0x3e + k * 2, rng.choice([0, 1, 1, 2, 3, 11]) | (rng.randint(0, 7) << 4) | (rng.randint(1, 40) << 8))
        struct.pack_into('<H', pd, 0x5f, (struct.unpack_from('<H', pd, 0x5f)[0] & ~(0xf << 6)) | (n << 6))
        pd[0x62] &= ~0x10   # no maze spell: on level 7 FixPlayerEquips would swap in a texture file (set_maze)
        pd[0xb8] = rng.choice([0, 0, 0, 1, 2, 8, 0x20, 0x40, 0x61])
        pd[0xb9] = rng.choice([0, 0x10, 0x60])
        pd[0xb6] = (pd[0xb6] & 0xf8) | rng.randint(0, 6)
        struct.pack_into('<I', pd, 0xce, rng.getrandbits(24))
        struct.pack_into('<I', pd, 0xbc, rng.getrandbits(24))
        p.w('PlayerDat', 0, bytes(pd))
        self.w8('plyNotice', rng.randint(1, 15))
        self.w8('plyNotice', rng.randint(1, 15), 1)
        return pd

    # --- the player and what is around it ------------------------------------------------------------------------------
    def put_player(self, t=None, near=None):
        """the player (object 1) linked into tile t (any open tile, or one near the tile `near`), by the proven
        player_setup on both sides; returns the tile"""
        rng = self.rng
        if t is None:
            ots = self.open_tiles(3)
            if near is not None:
                nx, ny = near & 63, near >> 6
                close = [x for x in ots if abs((x & 63) - nx) <= 6 and abs((x >> 6) - ny) <= 6]
                ots = close or ots
            t = rng.choice(ots)
        r = self.p.call('player_setup', [('w', t & 63), ('w', t >> 6)], None)
        self.check('player_setup', r)
        self.player_in = True
        self.refresh()
        return t

    TILE_WALLS = [30, 0, 19, 21, 11, 13, 32, 32, 32, 32, 0, 0, 0, 0, 0, 30]

    def ring0_or_solid(self, idx, x, y):
        """find_good_x_and_y's search reads its 'visited' array, of which the original clears only the first nine of
        eighteen bytes (WORLDEV.C memset(visited, 0, 9)): the rest is the stack's old contents, zero in the emulator's
        harness and whatever the host's stack held on our side (a core difference, reported: tools/core.py should
        zero it). The search is run only where it does not reach them: where the object idx fits on the square itself
        or the square is a wall (no square is queued). Returns whether it is such a case (can_place is run here on both
        sides as the check)."""
        if x < 1 or x > 62 or y < 1 or y > 62:
            return False
        typ, hgt, _ = self.tiles[y * 64 + x]
        if typ == 0:
            return True
        item = self.obj_word(idx, 0) & 0x1ff
        z = (hgt << 3) + (4 if self.TILE_WALLS[typ] & 0x20 else 0)
        r = self.p.call('can_place', [('w', item), ('w', idx), ('w', x * 8 + 3), ('w', y * 8 + 3), ('w', z), ('w', 0), ('w', 8)], 'b')
        self.check('can_place', r)
        return r[0][0] == 'ret 1'

    def objs_of(self, major=None, minor=None, cls=None):
        """(index, tile) of the objects on the level's lists of a major class (a trap is major 6), and optionally
        minor and the in-class (the item's low four bits)"""
        out = []
        for t in range(4096):
            for i in chain(self.block, self.tiles[t][2] >> 6):
                it = self.obj_word(i, 0) & 0x1ff
                if major is not None and it >> 6 != major:
                    continue
                if minor is not None and (it >> 4) & 3 != minor:
                    continue
                if cls is not None and (it & 15) != cls:
                    continue
                out.append((i, t))
        return out


# --- files by hand: a FILE the two sides each have (the game's own Borland calls; the core's is a host FILE) --------
def raw_call(p, name, words, host, hostname=None):
    """a call with the DOS words and the host's arguments given separately (a FILE pointer is a fake near pointer in
    the emulator and a host pointer on our side); returns the DOS (ax, dx) and the host's return (an int)"""
    s, o = p.code[name]
    p.calls = []
    p.e.write(DGROUP, 0xEC00, b'\0' * (0xFFF0 - 0xEC00))
    ax, dx = p.e.call(s, o, [w & 0xffff for w in words], limit=p.limit)
    ours = p._cmd('call %s %s' % (hostname or name, ' '.join(host)), True)
    p.c.stdout.readline()
    return (ax, dx), int(ours.split()[1], 16)


def fopen_both(p, path, mode):
    """fopen on both sides: returns (the emulator's handle, the host pointer as an argument string)"""
    p.w('S', 0, path.encode() + b'\0')
    p.w('S', 0x20, mode.encode() + b'\0')
    (ax, dx), h = raw_call(p, 'fopen', [0xE000, 0xE020], ['S+0', 'S+20'], 'bc_fopen')
    return ax, '0x%x' % h

SPELLS_ROUTINES = ['anti_magic_p', 'cast', 'do_spell', 'restore_mana', 'restore_hp', 'get_hp_back', 'healing', 'backfire',
                   'release_missile', 'build_new_obj', 'sp_true_sight', 'sp_sheet_light', 'sp_meteor', 'sp_ward_undead',
                   'sp_poison', 'hit_critter_goal', 'sp_charm', 'sp_confusion', 'sp_fear', 'sp_hold', 'process_area',
                   'gronk_area', 'gronk_whoami', 'nail_area', 'nail_1area', 'creat_spell', 'tremor_area', 'print_monster',
                   'mdetect', 'obj_spells', 'xt_spells', 'damage_square']
