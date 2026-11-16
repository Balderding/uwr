"""rle_probe.py: tries readings of the creatures' run-length scheme; the right one uses up each frame's data
exactly (the frame header's length, in words)."""
import struct
import sys

sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
import assets  # noqa: E402
import crit  # noqa: E402


def decode(d, at, npix, size, v):
    nb = crit.Bits(d, at, size)
    out, stage, repeats, guard = 0, 0, 0, 0
    while out < npix and guard < npix * 4:
        guard += 1
        if stage == 0:
            c = nb.count()
            if c == 1 and v['one_skips']:
                stage = 1
                continue
            if c == 2 and v['two_multi']:
                repeats = nb.count() - v['minus']
                continue
            nb.get()
            out += c
            if repeats <= 0:
                stage = 1
                repeats = 0
            else:
                repeats -= 1
        else:
            c = nb.count()
            for _ in range(c):
                nb.get()
            out += c
            stage = 0
    return out, nb.i // size - at * 8 // size


def score(v, files):
    bad = total = 0
    for nn, pg in files:
        try:
            d = assets.rd('CRIT/CR%02oPAGE.N%02d' % (nn, pg))
        except OSError:
            continue
        at = 2 + d[1]
        nseg = d[at]
        at += 1 + 8 * nseg
        npal = d[at]
        at += 1 + 32 * npal
        nfr = d[at]
        at += 2
        offs = struct.unpack_from('<%dH' % nfr, d, at)
        for o in offs:
            w, h, t = d[o], d[o + 1], d[o + 4]
            ln = struct.unpack_from('<H', d, o + 5)[0]
            size = 5 if t == 6 else 4
            n, used = decode(d, o + 7, w * h, size, v)
            total += 1
            bad += abs(used - ln) > 1 or n != w * h
    return bad, total


files = [(n, p) for n in range(0, 8) for p in range(2)]
for one_skips in (True, False):
    for two_multi in (True, False):
        for minus in (0, 1):
            v = dict(one_skips=one_skips, two_multi=two_multi, minus=minus)
            print(v, score(v, files))
