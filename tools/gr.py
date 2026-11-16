"""gr.py: Ultima Underworld's .GR picture files (objects, UI, inventory, ...), the reference for the engine.

    python -I gr.py sheet NAME.GR [palette]     writes extracted/gr/NAME.png: every picture on a sheet

.GR: u8 1, u16 count, u32 offset[count] (an offset at or past the file's end = no picture). A picture:
  u8 type: 4 = 8-bit, 8 = 4-bit run-length, 10 = 4-bit; u8 width, u8 height;
  4-bit types: u8 small palette (ALLPALS.DAT: 32 x 16 indices into the game palette), then u16 size;
  8-bit: u16 size; then the pixels. Palette index 0 is transparent.
4-bit run-length: nibbles, high first. A count is one nibble, or (when 0) two, or (when those are 0) three.
Records alternate: a repeat record (count, then one nibble repeated count times; count 1 = no repeat, go to a
run; count 2 = the next count - 1 repeat records follow), then a run record (count, then count nibbles).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import assets  # noqa: E402


def allpals():
    d = assets.rd('DATA/ALLPALS.DAT')
    return [list(d[i * 16:(i + 1) * 16]) for i in range(32)]


class Nibbles:
    def __init__(self, d, at):
        self.d, self.i = d, at * 2

    def get(self):
        b = self.d[self.i >> 1] if (self.i >> 1) < len(self.d) else 0
        v = (b >> 4) if (self.i & 1) == 0 else (b & 15)
        self.i += 1
        return v

    def count(self):
        n = self.get()
        if n:
            return n
        n = self.get() << 4 | self.get()
        if n:
            return n
        return self.get() << 8 | self.get() << 4 | self.get()


def rle4(d, at, npix):
    nb = Nibbles(d, at)
    out = []
    stage, repeats = 0, 0
    guard = 0
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


def pictures(d, aux=None):
    """[(width, height, indices into the game palette or None)]"""
    aux = aux or allpals()
    n = struct.unpack_from('<H', d, 1)[0]
    offs = struct.unpack_from('<%dI' % n, d, 3)
    out = []
    for o in offs:
        if o + 3 > len(d):
            out.append((0, 0, None))
            continue
        t, w, h = d[o], d[o + 1], d[o + 2]
        if t == 4:
            px = list(d[o + 5:o + 5 + w * h])
        elif t in (8, 10):
            pal = aux[d[o + 3] & 31]
            if t == 8:
                nib = rle4(d, o + 6, w * h)
            else:
                nb = Nibbles(d, o + 6)
                nib = [nb.get() for _ in range(w * h)]
            px = [pal[v] for v in nib]
        else:
            out.append((w, h, None))
            continue
        out.append((w, h, px + [0] * (w * h - len(px))))
    return out


def sheet(name, palno=0):
    pal = assets.palettes(assets.rd('DATA/PALS.DAT'))[palno]
    pics = pictures(assets.rd('DATA/' + name))
    cols = 16
    cw = max([w for w, h, p in pics if p] + [1]) + 2
    ch = max([h for w, h, p in pics if p] + [1]) + 2
    rows = (len(pics) + cols - 1) // cols
    W, H = cols * cw, rows * ch
    img = bytearray([40, 0, 40] * W * H)
    for k, (w, h, px) in enumerate(pics):
        if not px:
            continue
        ox, oy = (k % cols) * cw + 1, (k // cols) * ch + 1
        for y in range(h):
            for x in range(w):
                i = px[y * w + x]
                if i:
                    p = ((oy + y) * W + ox + x) * 3
                    img[p:p + 3] = bytes(pal[i])
    out = os.path.join(assets.OUT, 'gr', name.replace('.GR', '.png'))
    assets.png(out, W, H, img)
    print(name, len(pics), 'pictures ->', out)


if __name__ == '__main__':
    if sys.argv[1] == 'sheet':
        sheet(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 0)
