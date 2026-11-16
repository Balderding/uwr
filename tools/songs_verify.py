"""songs_verify.py: UW.EXE's music choice (segment 1bd8: play 0x1f39c, resume 0x1f58d, on / off 0x1f6e8, toggle
0x1f75f, ask 0x2053a, a wandering one 0x20545, again if over 0x2055c, the turn 0x2058d) run in the emulator beside our
port (src/songs.c through build/rtest.exe) on the same random operations. The driver's calls, the file and timbre
loads and the state after each operation (the song playing and asked for, the handle, the two flags, the two times,
the random seed) must be the same.

    uv run --no-project --with unicorn==2.0.1.post1 python -I tools/songs_verify.py game build/rtest.exe [operations]
"""
import os
import random
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import DGROUP, UWEmu  # noqa: E402

PLAYER = 0xF100  # the player's record (DS 0x7270 points here): byte 0x5f bit 1, Fight mode
CLOCK = 0xF200   # the game clock (DS 0x2364 points here), 32 bits


def main():
    game, rtest = sys.argv[1], sys.argv[2]
    nops = int(sys.argv[3]) if len(sys.argv) > 3 else 4000
    rng = random.Random(11)
    e = UWEmu(os.path.join(game, 'UW.EXE'))
    for a, v in ((0x138, 1), (0x135, 1), (0x13a, 6), (0x261e, 0), (0x261f, 0)):
        e.wb(DGROUP, a, v)
    e.ww(DGROUP, 0x13c, 0), e.ww(DGROUP, 0x2646, 0xffff)
    for a in (0x2482, 0x2484, 0x2634, 0x2636):
        e.ww(DGROUP, a, 0)
    e.ww(DGROUP, 0x7270, PLAYER)
    e.ww(DGROUP, 0x2364, CLOCK), e.ww(DGROUP, 0x2366, DGROUP)
    e.write(DGROUP, CLOCK, bytes(4))
    e.set_seed(0x1234567)

    rec = []
    ans = {'file': 1, 'reg': 0, 'inst': 1, 'status': 1, 'timbres': []}

    def string_at(off):
        s = bytearray()
        while len(s) < 80:
            b = e.rb(DGROUP, off + len(s))
            if not b:
                break
            s.append(b)
        return s.decode('latin-1')

    def h_file(e):
        name = string_at(e.arg(0)).split('\\')[-1]
        rec.append('file %s' % name)
        return ans['file']

    def h_register(e):
        rec.append('register')
        return ans['reg'] & 0xffff

    def h_release(e):
        rec.append('release %d' % e.arg(1))
        return 0

    def h_request(e):
        rec.append('request %d' % e.arg(1))
        return ans['timbres'].pop(0) if ans['timbres'] else 0xffff

    def h_install(e):  # 0x1f341(bank, patch)
        rec.append('install %d %d' % (e.arg(0) & 0xff, e.arg(1) & 0xff))
        return ans['inst']

    def h_start(e):
        rec.append('start %d' % e.arg(1))
        return 0

    def h_stop(e):
        rec.append('stop %d' % e.arg(1))
        return 0

    def h_status(e):
        rec.append('status %d' % e.arg(1))
        return ans['status']

    def h_vol(e):
        rec.append('volume %d %d %d' % (e.arg(1), e.arg(2), e.arg(3)))
        return 0

    def h_tempo(e):
        rec.append('tempo %d %d %d' % (e.arg(1), e.arg(2), e.arg(3)))
        return 0

    e.hook(0x1bd8, 0x6db, h_file)
    e.hook(0x1bd8, 0x3c1, h_install)
    for off, fn in ((0xbf5, h_register), (0xbfb, h_release), (0xc0d, h_request), (0xc2b, h_start), (0xc31, h_stop),
                    (0xc3d, h_status), (0xc4f, h_vol), (0xc55, h_tempo)):
        e.hook(0x2042, off, fn)

    ops = ['seed 1234567']
    clock = 0
    for _ in range(nops):
        k = rng.random()
        if k < 0.03:
            ops.append('loaded 1')
        elif k < 0.04:
            ops.append('file %d' % (rng.random() < 0.9))
        elif k < 0.08:
            ops.append('reg %d' % rng.choice([0, 4, 8, 12, 12, -1]))
        elif k < 0.11:
            ops.append('inst %d' % (rng.random() < 0.9))
        elif k < 0.18:
            ops.append('status %d' % rng.choice([0, 1, 1, 2]))
        elif k < 0.22:
            ops.append('timbres %s' % ' '.join(str(rng.randint(0, 2) << 8 | rng.randint(0, 127)) for _ in range(rng.randint(0, 3))))
        elif k < 0.32:
            ops.append('play %d %d' % (rng.choice(list(range(0, 16)) + [1, 2, 5, 6, 7, 8, 9, 13]), rng.random() < 0.8))
        elif k < 0.34:
            ops.append('resume')
        elif k < 0.37:
            ops.append('enable %d' % (rng.random() < 0.6))
        elif k < 0.38:
            ops.append('toggle')
        elif k < 0.48:
            ops.append('next %d' % rng.choice([0, 2, 4, 5, 6, 7, 8, 9, 11, 13]))
        elif k < 0.51:
            ops.append('random')
        elif k < 0.54:
            ops.append('restart')
        elif k < 0.58:
            ops.append('combat %d' % max(0, clock - rng.randint(0, 0x1400)))
        else:
            clock += rng.choice([0, 1, 5, 40, 300, 0x400, 0x900, 0xb00])
            clock &= 0xffffffff
            ops.append('update %d %d %d' % (clock, rng.random() < 0.3, rng.random() < 0.8))

    orig = []
    for op in ops:
        p = op.split()
        c, a = p[0], [int(x) for x in p[1:]]
        rec.clear()
        if c == 'seed':
            e.set_seed(int(p[1], 16))
            continue
        if c == 'loaded':  # the start-up's flag again (0x1f26e): the music there
            e.wb(DGROUP, 0x138, a[0])
            continue
        if c in ('file', 'reg', 'inst', 'status'):
            ans[c] = a[0]
            continue
        if c == 'timbres':
            ans['timbres'] = list(a)
            continue
        if c == 'combat':
            e.ww(DGROUP, 0x2482, a[0] & 0xffff), e.ww(DGROUP, 0x2484, a[0] >> 16)
            continue
        if c == 'play':
            e.call(0x1bd8, 0x41c, [a[0], a[1]])
        elif c == 'resume':
            e.call(0x1bd8, 0x60d)
        elif c == 'enable':
            e.call(0x1bd8, 0x768, [a[0]])
        elif c == 'toggle':
            e.call(0x1bd8, 0x7df)
        elif c == 'next':
            e.call(0x1bd8, 0x15ba, [a[0]])
        elif c == 'random':
            e.call(0x1bd8, 0x15c5)
        elif c == 'restart':
            e.call(0x1bd8, 0x15dc)
        elif c == 'update':
            e.ww(DGROUP, CLOCK, a[0] & 0xffff), e.ww(DGROUP, CLOCK + 2, a[0] >> 16)
            e.wb(DGROUP, PLAYER + 0x5f, 2 if a[1] else 0)
            e.ww(DGROUP, 0x565e, 1 if a[2] else 2)
            e.call(0x1bd8, 0x160d)
        st = [e.rb(DGROUP, 0x261e), e.rb(DGROUP, 0x261f), e.rw(DGROUP, 0x2646), e.rb(DGROUP, 0x138), e.rb(DGROUP, 0x135),
              e.rw(DGROUP, 0x2482) | e.rw(DGROUP, 0x2484) << 16, e.rw(DGROUP, 0x2634) | e.rw(DGROUP, 0x2636) << 16, e.seed()]
        orig += rec + ['state %s' % ' '.join(str(v) for v in st)]

    out = subprocess.run([rtest, 'songs'], input='\n'.join(ops) + '\n', capture_output=True, text=True).stdout.split('\n')[:-1]
    n = next((i for i in range(min(len(orig), len(out))) if orig[i] != out[i]), None)
    nstate = sum(1 for x in orig if x.startswith('state'))
    if n is None and len(orig) == len(out):
        print('songs           %d operations  same (%d driver calls and states)' % (nstate, len(orig)))
        print('all the same')
        sys.exit(0)
    n = n if n is not None else min(len(orig), len(out))
    print('songs DIFFER at entry %d:\n  original %s\n  ours     %s' % (n, orig[max(0, n - 4):n + 3], out[max(0, n - 4):n + 3]))
    sys.exit(1)


if __name__ == '__main__':
    main()
