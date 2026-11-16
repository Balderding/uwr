"""data\\upscaled\\{walls,floors}\\{256,512}\\NNN.png from work\\upscaled (upscale512.py's output).

The atlas's high-res copy has one scale for every texture (4x for "256", 8x for "512"), so:
  walls  256 = 256-NNN.png (4x of 64)        walls  512 = 512-NNN.png (8x of 64)
  floors 256 = pick(256-NNN.png) (128, 4x)   floors 512 = 256-NNN.png (256, 8x of 32)
pick = each 2x2 block's most common colour, a tie the block's top-left (as upscale512.py).
"""
import os
from collections import Counter
from PIL import Image

SRC = r'O:\uw\uo-new\work\upscaled'
DST = r'O:\uw\uo-new\data\upscaled'


def pick(im):
    im = im.convert('RGB')
    W, H = im.size
    px = im.load()
    out = Image.new('RGB', (W // 2, H // 2))
    po = out.load()
    for y in range(H // 2):
        for x in range(W // 2):
            b = (px[2*x, 2*y], px[2*x+1, 2*y], px[2*x, 2*y+1], px[2*x+1, 2*y+1])
            c, n = Counter(b).most_common(1)[0]
            po[x, y] = c if n > 1 else b[0]
    return out


for kind, count in (('walls', 210), ('floors', 52)):
    for res in ('256', '512'):
        os.makedirs(os.path.join(DST, kind, res), exist_ok=True)
    for i in range(count):
        n = '%03d' % i
        if kind == 'walls':
            a = Image.open(os.path.join(SRC, kind, '256-%s.png' % n)).convert('RGB')
            b = Image.open(os.path.join(SRC, kind, '512-%s.png' % n)).convert('RGB')
        else:
            b = Image.open(os.path.join(SRC, kind, '256-%s.png' % n)).convert('RGB')
            a = pick(b)
        a.save(os.path.join(DST, kind, '256', n + '.png'), optimize=True)
        b.save(os.path.join(DST, kind, '512', n + '.png'), optimize=True)
    print(kind, 'done')
