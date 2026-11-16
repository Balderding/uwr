"""crit.py: the creatures' animations (CRIT\\ASSOC.ANM, CRIT\\CRxxPAGE.Nyy), the reference for the engine.

    python -I crit.py sheet NN [page]     writes extracted/crit/crNN_pY.png: every frame of the page

ASSOC.ANM: 32 animation names (8 bytes each), then 64 creatures x (u8 animation, u8 palette).
A page: u8 slot base, u8 slot count, the slots (u8 segment or 0xFF), u8 segment count, segments of 8 frame
numbers (0xFF = none), u8 palette count, palettes of 32 game-palette indices, u8 frame count, u8 compression,
u16 offset of each frame. A frame: u8 width, height, hotspot x, hotspot y, u8 type (6 = 5-bit run-length,
8 = 4-bit), u16 length, then the data: the run-length scheme of the .GR pictures with 5-bit (or 4-bit) units; counts of more than one unit
combine 4 bits a unit even in 5-bit data.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import assets  # noqa: E402


class Bits:
    def __init__(self, d, at, size):
        self.d, self.i, self.size = d, at * 8, size

    def get(self):
        v = 0
        for _ in range(self.size):
            b = self.d[self.i >> 3] if (self.i >> 3) < len(self.d) else 0
            v = (v << 1) | ((b >> (7 - (self.i & 7))) & 1)
            self.i += 1
        return v

    def count(self):
        n = self.get()
        if n:
            return n
        # longer counts combine 4 bits a unit, even for 5-bit data (found on the creatures' frames)
        n = self.get() << 4 | self.get()
        if n:
            return n
        return (self.get() << 4 | self.get()) << 4 | self.get()


def rle(d, at, npix, size):
    nb = Bits(d, at, size)
    out, stage, repeats, guard = [], 0, 0, 0
    while len(out) < npix and guard < npix * 4:
        guard += 1
        if stage == 0:
            c = nb.count()
            if c == 1:
                stage = 1
                continue
            if c == 2:
                repeats = nb.count() - 1
                continue
            v = nb.get()
            out += [v] * c
            if repeats == 0:
                stage = 1
            else:
                repeats -= 1
        else:
            c = nb.count()
            out += [nb.get() for _ in range(c)]
            stage = 0
    return (out + [0] * npix)[:npix]


def assoc():
    d = assets.rd('CRIT/ASSOC.ANM')
    names = [d[i * 8:i * 8 + 8].split(b'\0')[0].decode('latin-1') for i in range(32)]
    creatures = [(d[256 + 2 * i], d[257 + 2 * i]) for i in range(64)]
    return names, creatures


def page(nn, pg):
    d = assets.rd('CRIT/CR%02oPAGE.N%02d' % (nn, pg))
    at = 0
    base, nslots = d[0], d[1]
    slots = list(d[2:2 + nslots])
    at = 2 + nslots
    nseg = d[at]
    at += 1
    segs = [list(d[at + 8 * i:at + 8 * i + 8]) for i in range(nseg)]
    at += 8 * nseg
    npal = d[at]
    at += 1
    pals = [list(d[at + 32 * i:at + 32 * i + 32]) for i in range(npal)]
    at += 32 * npal
    nfr, comp = d[at], d[at + 1]
    at += 2
    offs = struct.unpack_from('<%dH' % nfr, d, at)
    frames = []
    for o in offs:
        w, h, hx, hy, typ = d[o:o + 5]
        size = 5 if typ == 6 else 4
        px = rle(d, o + 7, w * h, size)
        frames.append((w, h, hx, hy, px))
    return dict(base=base, slots=slots, segs=segs, pals=pals, comp=comp, frames=frames)


def sheet(nn, pg=0, palno=0):
    pal = assets.palettes(assets.rd('DATA/PALS.DAT'))[0]
    p = page(nn, pg)
    aux = p['pals'][palno] if p['pals'] else list(range(32))
    fr = p['frames']
    cols = 16
    cw = max([f[0] for f in fr] + [1]) + 2
    ch = max([f[1] for f in fr] + [1]) + 2
    rows = (len(fr) + cols - 1) // cols
    W, H = cols * cw, rows * ch
    img = bytearray([40, 0, 40] * W * H)
    for k, (w, h, hx, hy, px) in enumerate(fr):
        ox, oy = (k % cols) * cw + 1, (k // cols) * ch + 1
        for y in range(h):
            for x in range(w):
                i = px[y * w + x]
                if i:
                    pi = aux[i] if i < len(aux) else 0
                    q = ((oy + y) * W + ox + x) * 3
                    img[q:q + 3] = bytes(pal[pi])
    out = os.path.join(assets.OUT, 'crit', 'cr%02d_p%d.png' % (nn, pg))
    assets.png(out, W, H, img)
    print('cr%02d page %d: %d slots %d segments %d palettes %d frames (compression %d) -> %s' % (
        nn, pg, len(p['slots']), len(p['segs']), len(p['pals']), len(fr), p['comp'], out))
    print(' slots', p['slots'])
    print(' segments', p['segs'][:12])


if __name__ == '__main__':
    if sys.argv[1] == 'sheet':
        sheet(int(sys.argv[2]), int(sys.argv[3]) if len(sys.argv) > 3 else 0)
    elif sys.argv[1] == 'assoc':
        n, c = assoc()
        print(n)
        print(c)
