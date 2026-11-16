"""proof_runes.py: RUNES.C (ovr119: the rune bag and casting from runes: add_rune, clear_runes, ShowRune, RedispRune,
clear_shelf, mous_in_rune, not_a_spell, try_clear, try_cast, fail_spell, player_cast) proved beside the rules core,
on the nine real levels from LEV.ARK. The real spell table (spells[], 53 entries of the game's data), the player put
on an open tile by the proven player_setup, a random record (level, mana, Casting skill, runes in the bag, the shelf,
active spells), the clock and the last cast random, the mouse pointer record (inplist) random but sound for the panel
(a rune of the bag, the strip that clears the shelf, a right click that looks at it), and the input mode. Casting
runs the whole way through do_spell (the spells of SPELLS.C: mana, light, motion, missiles, areas, summoning,
object spells, Armageddon, gate travel ...). The runes the shelf spells are mostly real spells (a random real
spell's runes). After every call: the return, the outside calls (the sounds and messages with their arguments), rand's
state, the whole level block, the player record, and every global the magic writes.

    python tools/proof_runes.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from magic_common import MWorld, run, u16, Skip  # noqa: E402
import magic_common as mcm  # noqa: E402

ROUTINES = ['add_rune', 'clear_runes', 'ShowRune', 'RedispRune', 'clear_shelf', 'mous_in_rune', 'not_a_spell',
            'try_clear', 'try_cast', 'fail_spell', 'player_cast']
FIRST_RUNESTONE = 0xE8
PD = 0x4a   # PlayerDat: weight


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 250
    w = MWorld(game, coretest, 119, 'runes')
    p = w.p
    p.coverage()
    rng = w.rng
    # the spell table as the game has it: read back from the emulator's memory (far data at symbols' spells)
    seg, off = p.code['spells']
    stab = p.e.read(seg, off, 53 * 4)
    runes_of = [struct.unpack_from('<H', stab, i * 4 + 1)[0] for i in range(53)]   # cls (1), runes (2), sub (1)

    def pdw(off, data):
        p.w('PlayerDat', off, data)

    def pd(off, n=1):
        return bytes.fromhex(p.r('PlayerDat', off, n)[0])

    def alloc_static(item):
        r = p.call('Obj_Alloc', [('w', 0)], 'f')
        p.same('Obj_Alloc', r, w.ctx)
        spec = r[0][0].split()[1]
        if spec == '0':
            return None
        o = int(spec[2:], 16)
        p.w('L', o, struct.pack('<HHHH', item, 0, rng.randint(1, 63), 0))
        return spec

    def player_record():
        pdv = bytearray(pd(0, 0xd2))
        pdv[0x37] = rng.choice([0, 3, 6, 12, 18, 30, 60, rng.randint(0, 255)])        # mana
        pdv[0x38] = max(pdv[0x37], rng.choice([0, 20, 60, 99, rng.randint(0, 255)]))
        pdv[0x3d] = rng.choice([1, 2, 3, 5, 8, 12, 15, rng.randint(0, 30)])         # level
        pdv[0x21 + 9] = rng.choice([0, 5, 12, 20, 30, 45, rng.randint(0, 63)])      # Casting
        pdv[0x21 + 11] = rng.choice([0, 10, 30])                                     # Search (Reveal)
        pdv[0x21 + 8] = rng.randint(0, 30)
        # active spells: 0..3 of random class/sub/stability (words at 0x3e)
        n = rng.choice([0, 0, 1, 2, 3])
        for k in range(3):
            cls = rng.choice([0, 1, 1, 2, 3, 11, 0])
            sub = rng.choice([0, 1, 2, 3, 4, 5, 6, 7]) if cls != 11 else rng.choice([0, 1, 2, 3])
            struct.pack_into('<H', pdv, 0x3e + k * 2, cls | (sub << 4) | (rng.randint(1, 40) << 8))
        w16 = struct.unpack_from('<H', pdv, 0x5f)[0]
        w16 = (w16 & ~(0xf << 6)) | (n << 6)
        # no shrooms (they load palette files), nrunes 0..3
        nr = rng.randint(0, 3)
        w16 = (w16 & ~(3 << 2)) | (nr << 2)
        struct.pack_into('<H', pdv, 0x5f, w16)
        pdv[0x61] &= ~0x0c
        pdv[0x44:0x47] = bytes(rng.getrandbits(8) if rng.random() < 0.5 else 0xff for _ in range(3))
        shelf = [rng.choice([rng.randint(0, 23), 0x18]) for _ in range(3)]
        if rng.random() < 0.7:          # the runes of a real spell
            r = runes_of[rng.randrange(48)]
            shelf = [(r >> 10) & 31, (r >> 5) & 31, r & 31]
        pdv[0x47:0x4a] = bytes(shelf)
        struct.pack_into('<I', pdv, 0xce, rng.choice([0, rng.getrandbits(20), 0xffffff00]))
        pdv[0x5e] = (pdv[0x5e] & 0xf0) | rng.choice([0, 0, 3, 5, 9])               # moonstone level
        pdv[0xb0] = rng.randint(0, 99)
        pdw(0, bytes(pdv))
        gc = struct.unpack_from('<I', pdv, 0xce)[0]
        w.w8('spell_delay', rng.choice([0, 0x40, 0x60, 0xff]))
        w.wl('lstime', max(0, gc - rng.choice([0, 3, 100, 1000, 5000])) & 0xffffffff)

    counts = {}

    def check(name, res, ctx):
        w.ctx = ctx
        w.check(ctx, res)
        counts[name] = counts.get(name, 0) + 1

    for level in range(1, 10):
        w.level(level)
        done = 0
        episodes = 0
        while done < per:
            episodes += 1
            if episodes > 1:
                w.level(level)
            for k in range(30):
                if done >= per:
                    break
                done += 1
                op = rng.choice(['cast'] * 8 + ['pcast'] * 4 + ['click'] * 6 + ['clear'] * 4 + ['aspell'] * 6 + ['add'] * 4
                                + ['misc'] * 2 + ['fail'])
                ctx = 'level %d op %d %s' % (level, done, op)
                w.ctx = ctx
                if os.environ.get('PROOF_TRACE'):
                    print(ctx, flush=True)
                try:
                    if rng.random() < 0.5 or not w.player_in:
                        w.put_player()
                    player_record()
                    w.set_time(rng.randint(0, 60000))
                    # the mouse pointer record: x, y, mouse, cmd, mode
                    cmd = rng.choice([1, 1, 2, 3, 0])
                    inp = struct.pack('<5h', rng.randint(0, 100), rng.randint(0, 120), 1, cmd, 0)
                    p.w('chrbuf', 0, inp)
                    w.w16('GameInputMode', rng.choice([0, 0, 0, 0, 0, 1, 2, 3, 4, 8]))
                    w.w8('mspell_mused', rng.choice([0, 0, 3, 9]))
                    if op == 'add':
                        for _ in range(rng.randint(1, 3)):
                            item = rng.choice([FIRST_RUNESTONE + rng.randint(0, 24)] * 4 + [FIRST_RUNESTONE - 1, FIRST_RUNESTONE + 25, rng.randint(0, 0x1ff)])
                            spec = alloc_static(item)
                            if spec:
                                check('add_rune', p.call('add_rune', [('f', spec)], 'b'), ctx)
                    elif op == 'click':
                        # a click in the rune panel: a rune of the panel's grid (or the clear strip)
                        # the rune under the pointer is 0x14 - (row << 2) + col: choose a row and column that make a rune of the bag (0..23)
                        while True:
                            colx = rng.randint(0, 4)
                            rowy = rng.randint(0, 7)
                            r2 = 0x14 - (rowy << 2) + colx
                            if 0 <= r2 <= 23:
                                break
                        x = 3 + colx * 0x12 + rng.randint(0, 0x11)
                        y = 0x12 + rowy * 0xF + rng.randint(0, 0xe)
                        if rng.random() < 0.15:
                            y = rng.randint(0, 0x11)
                        p.w('chrbuf', 0, struct.pack('<5h', x, y, 1, cmd, 0))
                        # the click often lands on a rune the bag holds
                        if rng.random() < 0.7:
                            pdv = bytearray(pd(0x44, 3))
                            r2 = 0x14 - (((y - 0x12) // 0xF) << 2) + (x - 3) // 0x12 if y >= 0x12 else 0
                            if 0 <= r2 < 24:
                                pdv[r2 >> 3] |= 1 << (7 - (r2 & 7))
                                pdw(0x44, bytes(pdv))
                        for _ in range(rng.randint(1, 3)):
                            check('mous_in_rune', p.call('mous_in_rune', [], None), ctx)
                            if rng.random() < 0.3:
                                check('try_cast', p.call('try_cast', [('w', rng.randint(0, 1))], None), ctx)
                    elif op == 'cast':
                        for _ in range(rng.randint(1, 2)):
                            # the shelf spells a real spell: it is set from the table, or left random
                            if rng.random() < 0.8:
                                r = runes_of[rng.randrange(48)]
                                pdw(0x47, bytes([(r >> 10) & 31, (r >> 5) & 31, r & 31]))
                            check('try_cast', p.call('try_cast', [('w', rng.randint(0, 1))], None), ctx)
                    elif op == 'pcast':
                        idx = rng.choice([rng.randint(0, 47)] * 3 + [rng.randint(0, 52)])
                        c = p.call('player_cast', [('w', idx)], 'b')
                        check('player_cast', c, ctx)
                    elif op == 'clear':
                        which = rng.choice(['clear_shelf', 'clear_runes', 'RedispRune', 'not_a_spell'])
                        check(which, p.call(which, [], None), ctx)
                    elif op == 'aspell':
                        p.w('chrbuf', 0, struct.pack('<5h', rng.randint(0, 47), rng.randint(0, 20), 1, cmd, 0))
                        check('try_clear', p.call('try_clear', [], None), ctx)
                    elif op == 'misc':
                        check('ShowRune', p.call('ShowRune', [('w', rng.randint(0, 23))], None), ctx)
                    elif op == 'fail':
                        check('fail_spell', p.call('fail_spell', [('w', rng.randint(0, 3))], 'b'), ctx)
                except Skip as ex:
                    w.recover()
                    counts['skipped'] = counts.get('skipped', 0) + 1
    total = sum(v for k, v in counts.items() if k != 'skipped')
    print('runes           %d operations on 9 levels  same (%s)' % (total, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    if os.environ.get('PROOF_COV'):
        print('  (SPELLS.C routines reached from here: %s)' % ', '.join(p.ran(mcm.SPELLS_ROUTINES)[0]))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


if __name__ == '__main__':
    run('runes', main)
