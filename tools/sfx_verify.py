"""sfx_verify.py: UW.EXE's sound-effect routines (segment 1bd8: play 0x1fa9b, play at a place 0x1f82e, the 16 Hz timer
0x1fc95, stop 0x1fb7d / 0x1fbfc) and its math (isqrt 0x23018, sin / cos 0x23009) run in the emulator beside our ports
(src/sfx.c through build/rtest.exe). Every call to the sound driver, every returned value and the effects' state after
each operation must be the same.

    uv run --no-project --with unicorn==2.0.1.post1 python -I tools/sfx_verify.py game build/rtest.exe [operations]
"""
import os
import random
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import DGROUP, UWEmu  # noqa: E402

PLAYER = 0xF100  # the player's object, in DGROUP


def main():
    game, rtest = sys.argv[1], sys.argv[2]
    nops = int(sys.argv[3]) if len(sys.argv) > 3 else 3000
    rng = random.Random(7)
    bad = 0

    # --- the math: isqrt and sin / cos --------------------------------------------------------------------------
    e = UWEmu(os.path.join(game, 'UW.EXE'))
    lines, want = [], []
    vals = [rng.choice([rng.randint(0, 255), rng.randint(0, 65535), rng.randint(0, 0xffffff), rng.randint(0, 0x7fffffff)]) for _ in range(3000)]
    vals += list(range(0, 300)) + [0xffff, 0x10000, 0xffffff, 0x1000000, 0x3fffffff, 0x40000000]
    for v in vals:
        ax, _ = e.call(0x1f3a, 0xf3f, [v & 0xffff, v >> 16])
        lines.append('0 isqrt %d %d\n' % (v >> 16, v & 0xffff))
        want.append(ax)
    for ang in list(range(0, 65536, 97)) + [0, 0x4000, 0x8000, 0xc000, 0xffff]:
        e.call(0x1f3a, 0xe63, [ang, 0xF000, 0xF002])
        s, c = e.rw(DGROUP, 0xF000), e.rw(DGROUP, 0xF002)
        r = s | c << 16
        lines.append('0 sin %d\n' % ang)
        want.append(r - (1 << 32) if r & 0x80000000 else r)
    got = subprocess.run([rtest], input=''.join(lines), capture_output=True, text=True).stdout.split('\n')
    diffs = [(lines[i].strip(), want[i], got[i]) for i in range(len(want)) if int(got[i].split()[0]) != want[i]]
    print('isqrt, sin/cos  %d cases  %s' % (len(want), 'same' if not diffs else '%d DIFFER %s' % (len(diffs), diffs[:4])))
    bad += bool(diffs)

    # --- the effects ---------------------------------------------------------------------------------------------
    e = UWEmu(os.path.join(game, 'UW.EXE'))
    dat = open(os.path.join(game, 'SOUND', 'SOUNDS.DAT'), 'rb').read()
    table = bytearray()
    for i in range(dat[0]):  # the table as 0x1fd8e stores it: bytes 0-2, then the word byte 3 x 256 + byte 4 (little-endian)
        r = dat[1 + i * 5:6 + i * 5]
        table += bytes([r[0], r[1], r[2], r[4], r[3]])
    e.write(0x561c, 0, bytes(table))
    e.wb(DGROUP, 0x2644, dat[0])
    for a, v in ((0x137, 1), (0x136, 1), (0x13a, 6)):
        e.wb(DGROUP, a, v)
    e.ww(DGROUP, 0x13c, 0)
    e.ww(DGROUP, 0x7274, PLAYER), e.ww(DGROUP, 0x7276, DGROUP)
    rec = []
    ans = {'tstat': 0, 'lock': 0}

    def tstat(e):
        rec.append('tstat %d %d' % (e.arg(1) & 0xff, e.arg(2) & 0xff))
        return ans['tstat']

    def install(e):
        rec.append('install %d %d' % (e.arg(0) & 0xff, e.arg(1) & 0xff))
        return 1

    def lock(e):
        rec.append('lock')
        return ans['lock']

    def release(e):
        rec.append('release %d' % (e.arg(1) & 0xffff))
        return 0

    def cv(e):
        rec.append('cv %d %d %d' % (e.arg(1), e.arg(2), e.arg(3)))
        return 0

    e.hook(0x2042, 0xc25, tstat)
    e.hook(0x1bd8, 0x3c1, install)
    e.hook(0x2042, 0xc9d, lock)
    e.hook(0x2042, 0xca9, release)
    e.hook(0x2042, 0xc7f, cv)

    ops = []
    for _ in range(nops):
        k = rng.random()
        if k < 0.1:
            ops.append('tstat %d' % rng.choice([0, 1]))
        elif k < 0.2:
            ops.append('lock %d' % rng.choice([0, 2, 3, 5, 7, 8, 9, 9, 9]))
        elif k < 0.42:
            ops.append('play %d %d %d' % (rng.randint(0, 23), rng.randint(0, 255), rng.randint(-128, 127)))
        elif k < 0.67:
            px, py = rng.randint(0, 511), rng.randint(0, 511)
            near = rng.random() < 0.7
            x = max(0, min(511, px + rng.randint(-60, 60))) if near else rng.randint(0, 511)
            y = max(0, min(511, py + rng.randint(-60, 60))) if near else rng.randint(0, 511)
            if rng.random() < 0.05:
                x, y = px, py
            ops.append('at %d %d %d %d %d %d %d' % (rng.randint(0, 23), x, y, rng.randint(-128, 127), px, py, rng.randint(0, 255)))
        elif k < 0.94:
            ops.append('tick')
        elif k < 0.98:
            ops.append('stop %d' % rng.randint(0, 5))
        else:
            ops.append('stopall')

    orig = []
    for op in ops:
        p = op.split()
        c, a = p[0], [int(x) for x in p[1:]]
        rec.clear()
        r = None
        if c in ('tstat', 'lock'):
            ans[c] = a[0]
            continue
        if c == 'play':
            r, _ = e.call(0x1bd8, 0xb1b, [a[0], a[1], a[2]])
        elif c == 'at':
            idn, x, y, vol, px, py, h = a
            e.ww(DGROUP, PLAYER + 0x16, (px >> 3) << 10 | (py >> 3) << 4)
            e.ww(DGROUP, PLAYER + 2, (px & 7) << 13 | (py & 7) << 10 | (h >> 5) << 7)
            e.wb(DGROUP, PLAYER + 0x18, h & 0x1f)
            r, _ = e.call(0x1bd8, 0x8ae, [idn, x, y, vol])
        elif c == 'tick':
            e.call(0x1bd8, 0xd15)
        elif c == 'stop':
            e.call(0x1bd8, 0xbfd, [a[0]])
        elif c == 'stopall':
            e.call(0x1bd8, 0xc7c)
        st = [e.rb(DGROUP, 0x25f6), e.rw(DGROUP, 0x14c)]
        for k in range(4):
            st += [e.rb(DGROUP, 0x2612 + k), e.rw(DGROUP, 0x262c + 2 * k), e.rb(DGROUP, 0x25f7 + k), e.rb(DGROUP, 0x2606 + k)]
        orig += rec + ['ret %d state %s' % ((r if r is not None else 0xffff) & 0xff, ' '.join(str(v) for v in st))]

    sounds = os.path.join(os.path.dirname(os.path.abspath(rtest)), '..', 'data', 'sound', 'sounds.dat')
    out = subprocess.run([rtest, 'sfx', sounds], input='\n'.join(ops) + '\n', capture_output=True, text=True).stdout.split('\n')[:-1]
    # (ours prints ret 255 for operations without a result, as the original's AL is not looked at there)
    norm = lambda s: ('ret * ' + s.split(' ', 2)[2]) if s.startswith('ret ') else s  # noqa: E731
    o2 = [norm(x) if i + 1 < len(orig) else norm(x) for i, x in enumerate(orig)]
    u2 = [norm(x) for x in out]
    # the returned slot matters for play / at: compare those exactly
    ri = [i for i, x in enumerate(orig) if x.startswith('ret ')]
    ops_ret = [op for op in ops if not op.startswith(('tstat', 'lock'))]
    exact = [(orig[ri[j]].split()[1], out[[i for i, x in enumerate(out) if x.startswith('ret ')][j]].split()[1]) for j in range(min(len(ri), len(ops_ret))) if ops_ret[j].startswith(('play', 'at'))]
    n = next((i for i in range(min(len(o2), len(u2))) if o2[i] != u2[i]), None)
    rets_bad = [p for p in exact if p[0] != p[1]]
    if n is None and len(o2) == len(u2) and not rets_bad:
        print('effects         %d operations  same (%d driver calls and states)' % (len(ops_ret), len(orig)))
    else:
        bad += 1
        if n is not None or len(o2) != len(u2):
            n = n if n is not None else min(len(o2), len(u2))
            print('effects DIFFER at entry %d: original %s / ours %s' % (n, orig[max(0, n - 3):n + 3], out[max(0, n - 3):n + 3]))
        if rets_bad:
            print('effects: returned slots differ', rets_bad[:5])
    print('all the same' if not bad else 'DIFFERENCES')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
