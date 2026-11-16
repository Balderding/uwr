"""verify.py: UW.EXE's routines against uo-new's ports of them. Each case runs the original in the emulator (tools/emu.py)
and our port in build/rtest.exe with the same inputs and the same random seed; the results and the seed after must
be the same.

    uv run --no-project --with unicorn==2.0.1.post1 --with capstone python -I tools/verify.py game build/rtest.exe [cases]

(unicorn 2.1.x crashes on this machine in mem_map; 2.0.1.post1 works.)
"""
import os
import random
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import DGROUP, UWEmu  # noqa: E402

OBJ_AT = 0xF000  # a scratch object in DGROUP for routines that take an object's far pointer


def main():
    game, rtest = sys.argv[1], sys.argv[2]
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 2000
    e = UWEmu(os.path.join(game, 'UW.EXE'))
    # COMOBJ.DAT as the game loads it (11 bytes a kind from offset 2) at DS 0x5b6e
    com = open(os.path.join(game, 'DATA', 'COMOBJ.DAT'), 'rb').read()[2:2 + 512 * 11]
    e.write(DGROUP, 0x5b6e, com)
    rng = random.Random(1)
    cases = []  # (routine name for rtest, rtest args, emulator call)

    def add(name, rargs, seg, off, eargs, prep=None, state=None):
        cases.append((name, rargs, seg, off, eargs, prep, state))

    for _ in range(n):
        add('rand', [], 0xec5, 0xde7, [])
        c, s = rng.randint(-2, 12), rng.randint(-2, 30)
        add('dice', [c, s], 0x35bd, 0x11e, [c, s])
        sk, df = rng.randint(0, 40), rng.randint(0, 40)
        add('check', [sk, df], 0x30f9, 0xc, [sk, df])
        item, harm, kind = rng.choice([0x4a, 0x61, 0x63, 0x78, 0x7c, rng.randint(0x40, 0x7f), rng.randint(0, 511)]), rng.randint(0, 255), rng.choice([1, 2, 3, 8, 0x0b, 0x10, 0x13, 0x40, 0x80, rng.randint(0, 255)])
        res = com[item * 11 + 8]

        def prep(e, item=item):
            e.ww(DGROUP, OBJ_AT, item)
        add('resist', [res, harm, kind], 0x229b, 0x2ff, [OBJ_AT, DGROUP, harm, kind], prep)

    # --- experience (SKILLCHK.C player_get_exp 0x341d8; COMBAT.C player_killed_a 0x25b35) ---------------------------
    # the player's record at DS PLAYER (DS 0x7270 points at it), the dungeon level DS 0x7278, the critters' table
    # (OBJECTS.DAT 0x132, 48 bytes each) at DS 0x4a52; advance (0x8138a, SKILLS.C:119) stands in by its state change
    # only (level + n, skill points + n: its message and the maxima are other routines'); the panel redraw
    # (0x34349) and the frame dragons (0x31f7f) do nothing here
    PLAYER = 0xF100
    od = open(os.path.join(game, 'DATA', 'OBJECTS.DAT'), 'rb').read()
    e.write(DGROUP, 0x4a52, od[0x132:0x132 + 64 * 48])
    e.ww(DGROUP, 0x7270, PLAYER)

    def advance(e):
        n = e.arg(0) & 0xff
        e.wb(DGROUP, PLAYER + 0x3d, (e.rb(DGROUP, PLAYER + 0x3d) + n) & 0xff)
        e.wb(DGROUP, PLAYER + 0x52, (e.rb(DGROUP, PLAYER + 0x52) + n) & 0xff)
        return 0
    e.hook_file(0x8138a, advance)
    e.hook(0x30f9, 0x1b9, lambda e: 0)
    e.hook(0x2e9a, 0x3df, lambda e: 0)

    def player_state(e):
        return '%d,%d,%d,%d' % (struct.unpack('<I', e.read(DGROUP, PLAYER + 0x4e, 4))[0], e.rb(DGROUP, PLAYER + 0x3d),
                                e.rb(DGROUP, PLAYER + 0x52), e.rb(DGROUP, PLAYER + 0x53))

    for _ in range(n):
        exp = rng.choice([0, rng.randint(0, 5000), rng.randint(0, 100000), rng.randint(95000, 97000), rng.randint(0, 0xffffffff)])
        lvl = rng.randint(1, 16)
        sp, earned = rng.randint(0, 255), rng.choice([exp // 3000 & 0xff, rng.randint(0, 255)])
        gain = rng.choice([rng.randint(-32768, 32767), rng.randint(0, 600), rng.randint(-2000, 0), -32768, 0])
        dungeon = rng.randint(1, 9)

        def prep(e, exp=exp, lvl=lvl, sp=sp, earned=earned, dungeon=dungeon):
            e.write(DGROUP, PLAYER + 0x4e, struct.pack('<I', exp))
            e.wb(DGROUP, PLAYER + 0x3d, lvl), e.wb(DGROUP, PLAYER + 0x52, sp), e.wb(DGROUP, PLAYER + 0x53, earned)
            e.ww(DGROUP, 0x7278, dungeon)
        add('getexp', [exp, lvl, sp, earned, gain, dungeon], 0x30f9, 0x48, [gain & 0xffff], prep, player_state)
        k = rng.randint(0, 63)
        powerful = rng.random() < 0.3

        def prep2(e, k=k, powerful=powerful, prep=prep):
            prep(e)
            e.ww(DGROUP, OBJ_AT, 0x40 + k)
            e.ww(DGROUP, OBJ_AT + 0xd, 0x400 if powerful else 0)
        add('killexp', [exp, lvl, sp, earned, struct.unpack_from('<H', od, 0x132 + k * 48 + 0x28)[0], int(powerful), dungeon],
            0x2121, 0x1725, [OBJ_AT, DGROUP], prep2, player_state)

    seeds = [rng.getrandbits(32) for _ in cases]
    lines = ''.join('%08x %s %s\n' % (sd, c[0], ' '.join(str(a) for a in c[1])) for sd, c in zip(seeds, cases))
    ours = subprocess.run([rtest], input=lines, capture_output=True, text=True).stdout.split('\n')
    bad = {}
    count = {}
    for i, (sd, c) in enumerate(zip(seeds, cases)):
        name, rargs, seg, off, eargs, prep, state = c
        if prep:
            prep(e)
        e.set_seed(sd)
        ax, dx = e.call(seg, off, eargs)
        res = ax - 0x10000 if ax & 0x8000 else ax
        if name in ('resist',):
            res = ax & 0xff
        orig = '%s %08x' % (state(e) if state else res, e.seed())
        mine = ours[i].strip()
        count[name] = count.get(name, 0) + 1
        if orig != mine:
            bad.setdefault(name, []).append('%s %s seed %08x: original %s, ours %s' % (name, rargs, sd, orig, mine))
    ok = True
    for name in count:
        b = bad.get(name, [])
        print('%-8s %5d cases  %s' % (name, count[name], 'same' if not b else '%d DIFFER' % len(b)))
        for line in b[:5]:
            print('    ' + line)
        ok = ok and not b
    print('all the same' if ok else 'DIFFERENCES')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
