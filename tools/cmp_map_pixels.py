"""cmp_pixels.py A B: every PNG under A (but upscaled\\) against the same name under B, by decoded pixels.
Prints per folder and map kind: files, files with any different pixel, different channel values, the largest difference."""
import os, sys, zlib, struct, collections


def png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', path
    at, idat, pal = 8, b'', None
    while at < len(d):
        n, t = struct.unpack('>I4s', d[at:at + 8])
        body = d[at + 8:at + 8 + n]
        if t == b'IHDR':
            w, h, depth, ct, _, _, il = struct.unpack('>IIBBBBB', body)
        elif t == b'IDAT':
            idat += body
        elif t == b'PLTE':
            pal = body
        at += 12 + n
    assert depth == 8 and il == 0, (path, depth, il)
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    raw = zlib.decompress(idat)
    row = w * ch
    out, prev = bytearray(), bytearray(row)
    for y in range(h):
        f = raw[y * (row + 1)]
        cur = bytearray(raw[y * (row + 1) + 1:(y + 1) * (row + 1)])
        if f == 1:
            for i in range(ch, row): cur[i] = (cur[i] + cur[i - ch]) & 255
        elif f == 2:
            for i in range(row): cur[i] = (cur[i] + prev[i]) & 255
        elif f == 3:
            for i in range(row): cur[i] = (cur[i] + (((cur[i - ch] if i >= ch else 0) + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(row):
                a = cur[i - ch] if i >= ch else 0
                b = prev[i]
                c = prev[i - ch] if i >= ch else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                cur[i] = (cur[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += cur
        prev = cur
    if ct == 3:
        out = bytearray(b''.join(pal[v * 3:v * 3 + 3] for v in out)); ch = 3
    return w, h, ch, bytes(out)


a, b = sys.argv[1], sys.argv[2]
stat = collections.defaultdict(lambda: [0, 0, 0, 0, 0])   # files, missing, files differing, values differing, max
hist = collections.Counter()
for root, dirs, files in os.walk(a):
    rel = os.path.relpath(root, a)
    if rel.split(os.sep)[0] == 'upscaled':
        continue
    for f in files:
        if not f.endswith('.png'):
            continue
        key = (os.sep.join(rel.split(os.sep)[:2]), f[-6:-4])
        s = stat[key]
        s[0] += 1
        q = os.path.join(b, rel, f)
        if not os.path.isfile(q):
            s[1] += 1
            continue
        pa, pb = png(os.path.join(root, f)), png(q)
        if pa == pb:
            continue
        if pa[:3] != pb[:3]:
            s[2] += 1; s[4] = 999
            continue
        n = 0
        for x, y in zip(pa[3], pb[3]):
            if x != y:
                n += 1
                hist[abs(x - y)] += 1
                if abs(x - y) > s[4]: s[4] = abs(x - y)
        s[2] += 1; s[3] += n
print('%-24s %-3s %6s %7s %7s %10s %4s' % ('folder', 'map', 'files', 'missing', 'differ', 'values', 'max'))
for k in sorted(stat):
    print('%-24s %-3s %6d %7d %7d %10d %4d' % (k + tuple(stat[k])))
print('differences by size:', dict(sorted(hist.items())))
