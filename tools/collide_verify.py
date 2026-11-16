"""collide_verify.py: UW.EXE's terrain collision (segment 2529, underworld-exhumed motion/COLLIDE.C: TerrainCheck
0x28809 with GetHgt, SolvePnt, SolveCenter; ComputeHeading 0x28c86; GetSlopeHgt 0x28635) run in the emulator on the
real levels beside our port (src/collide.c through build/rtest.exe). The world as the game has it: the level's block
from LEV.ARK at the map pointer (DS 0x19b4), the floors' terrain words from TERRAIN.DAT (TxmTerr, DS 0x7192), the
collision record through curP (DS 0x2708). Every result must be the same.

    uv run --no-project --with unicorn==2.0.1.post1 python -I tools/collide_verify.py game build/rtest.exe [per level]
"""
import os
import random
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import DGROUP, UWEmu  # noqa: E402

SEG = 0x2529     # the collision code's segment
MAPSEG = 0xC000  # where the level block goes
REC = 0xF300     # the collision record, in DGROUP


def main():
    game, rtest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 1500
    ark = open(os.path.join(game, 'DATA', 'LEV.ARK'), 'rb').read()
    ter = open(os.path.join(game, 'DATA', 'TERRAIN.DAT'), 'rb').read()
    com = open(os.path.join(game, 'DATA', 'COMOBJ.DAT'), 'rb').read()
    e = UWEmu(os.path.join(game, 'UW.EXE'))
    e.write(DGROUP, 0x5b6e, com[2:2 + 512 * 11])  # ComObjData
    rng = random.Random(5)
    ops, orig = [], []
    for level in range(1, 10):
        lb, = struct.unpack_from('<I', ark, 2 + (level - 1) * 4)
        tb, = struct.unpack_from('<I', ark, 2 + (17 + level) * 4)
        e.write(MAPSEG, 0, ark[lb:lb + 0x7c08])
        e.ww(DGROUP, 0x19b4, 0), e.ww(DGROUP, 0x19b6, MAPSEG)
        for i in range(10):
            fid, = struct.unpack_from('<H', ark, tb + 96 + i * 2)
            e.write(DGROUP, 0x7192 + i * 2, ter[0x200 + fid * 2:0x202 + fid * 2])
        e.ww(DGROUP, 0x2708, REC)
        e.ww(DGROUP, 0x272e, 0x4000), e.ww(DGROUP, 0x2730, MAPSEG)  # critdata: the mobile objects
        e.ww(DGROUP, 0x275a, 0x5b00), e.ww(DGROUP, 0x275c, MAPSEG)  # objdata: the static ones
        ops.append('level %d' % level)
        block = ark[lb:lb + 0x7c08]
        # the tiles with objects (away from the map's edge), each with its first object's fine place
        spots = []
        for t in range(64 * 64):
            tx, ty = t % 64, t // 64
            head = struct.unpack_from('<H', block, t * 4 + 2)[0] >> 6
            if head and 2 <= tx <= 61 and 2 <= ty <= 61:
                o = 0x4000 + head * 27 if head < 256 else 0x5b00 + (head - 256) * 8
                pos, = struct.unpack_from('<H', block, o + 2)
                spots.append((tx * 8 + (pos >> 13 & 7), ty * 8 + (pos >> 10 & 7), pos & 0x7f, head))
        for _ in range(per if spots else 0):
            sx, sy, sz, head = rng.choice(spots)
            x, y = sx + rng.randint(-6, 6), sy + rng.randint(-6, 6)
            z = max(0, sz + rng.randint(-12, 12))
            radius, height = rng.randint(0, 4), rng.choice([0, 0, rng.randint(1, 40)])
            index = rng.choice([0, 0, 1, head, rng.randint(1, 255), rng.randint(256, 1023)])
            mode = rng.randint(0, 3)
            rec = struct.pack('<hhhHBBh', x, y, z, 0, radius, height, index) + bytes(11)
            e.write(DGROUP, REC, rec)
            e.call(SEG, 0xbf6, [mode & 1, mode >> 1])
            e.call(SEG, 0xebf, [])
            r = e.read(DGROUP, REC, 0x17)
            found, count, first = r[0x14], r[0x15], r[0x16] - 256 if r[0x16] > 127 else r[0x16]
            cols = e.read(DGROUP, 0x26c6, 6 * 9)
            s = 'oc %d %d %d' % (found, count, first)
            for i in range(min(found, 9)):
                top, bot, link, off = struct.unpack_from('<BBHH', cols, i * 6)
                s += ' %d/%d/%d/%d' % (top, bot, link, off)
            ops.append('oc %d %d %d %d %d %d %d' % (x, y, z, radius, height, index, mode))
            orig.append(s)
            # can_place: a real kind there
            item = rng.choice([rng.randint(0, 0x7f), rng.randint(0x80, 0x1ff)])
            idx = rng.choice([0, 1, head, rng.randint(256, 1023)])
            flier, rge = rng.randint(0, 1), rng.choice([0, 1, 2, 4, 8])
            ax, _ = e.call(SEG, 0x1008, [item, idx, x & 0xffff, y & 0xffff, z & 0xffff, flier, rge])
            ops.append('cp %d %d %d %d %d %d %d' % (item, idx, x, y, z, flier, rge))
            orig.append('cp %d %d %d' % (ax & 0xff, e.rw(DGROUP, 0x2728), e.rw(DGROUP, 0x272c)))
        for _ in range(per):
            # inside the map, a tile from its edge (the 3 x 3 tiles read around the centre)
            x, y = rng.randint(8, 62 * 8 + 7), rng.randint(8, 62 * 8 + 7)
            z = rng.choice([rng.randint(0, 127), rng.randint(-20, 160), 0, 8, 16, 24, 64])
            heading = rng.randint(0, 0xffff)
            radius = rng.choice([0, 1, 2, 3, 4, rng.randint(0, 7)])
            rng_ = rng.choice([0, 1, 2, 3, 4, 8, 16, rng.randint(0, 255)])
            rec = struct.pack('<hhhHBBh', x, y, z, heading, radius, 0, 0) + bytes(11)
            e.write(DGROUP, REC, rec)
            e.call(SEG, 0x379, [rng_])
            e.call(SEG, 0x7f6, [])
            r = e.read(DGROUP, REC, 0x17)
            h0, h1 = struct.unpack_from('<HH', r, 0x0c)
            ops.append('tc %d %d %d %d %d %d' % (x, y, z, heading, radius, rng_))
            orig.append('tc %d %d %d %d %d %d %d %d' % (h0, h1, r[0x10], r[0x11], r[0x12], r[0x13], x & 0xffff, y & 0xffff))
            if rng.random() < 0.3:
                sx, sy = rng.randint(-300, 300), rng.randint(-300, 300)
                ax, _ = e.call(SEG, 0x1a5, [sx & 0xffff, sy & 0xffff])
                ops.append('slope %d %d' % (sx, sy))
                orig.append('slope %d' % ax)
    out = subprocess.run([rtest, 'collide', os.path.join(game, 'DATA', 'LEV.ARK'), os.path.join(game, 'DATA', 'TERRAIN.DAT'),
                          os.path.join(game, 'DATA', 'COMOBJ.DAT')],
                         input='\n'.join(ops) + '\n', capture_output=True, text=True).stdout.split('\n')[:-1]
    n = next((i for i in range(min(len(orig), len(out))) if orig[i] != out[i]), None)
    if n is None and len(orig) == len(out):
        tcs = [list(map(int, o.split()[1:])) for o in orig if o.startswith('tc')]
        cov = 'walls %d, too high %d, drops %d, slopes %d, wall directions %d, open directions %d, water/lava %d' % (
            sum(t[1] & 0x200 > 0 for t in tcs), sum(t[1] & 0x100 > 0 for t in tcs), sum(t[1] & 0x800 > 0 for t in tcs),
            sum(t[0] & 0x2000 > 0 for t in tcs), sum(t[4] != 9 for t in tcs), sum(t[5] != 9 for t in tcs),
            sum(t[0] & 3 > 0 for t in tcs))
        ocs = [o.split() for o in orig if o.startswith('oc')]
        cps = [o.split() for o in orig if o.startswith('cp')]
        cov += '; object checks %d (found something %d, in the way %d), can_place %d (fits %d)' % (
            len(ocs), sum(int(o[1]) > 0 for o in ocs), sum(int(o[2]) > 0 for o in ocs), len(cps), sum(o[1] == '1' for o in cps))
        print('collide         %d checks on 9 levels  same (%s)' % (len(orig), cov))
        print('all the same')
        sys.exit(0)
    n = n if n is not None else min(len(orig), len(out))
    q = [o for o in ops if not o.startswith('level')]
    print('collide DIFFER at %d (%s): original %s, ours %s' % (n, q[n] if n < len(q) else '?', orig[n:n + 1], out[n:n + 1]))
    sys.exit(1)


if __name__ == '__main__':
    main()
