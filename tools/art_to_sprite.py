"""Turn a big piece of art (a picture on a white background) into a UW object sprite at any scale, in the original
sprite's own colours. The production line the user picked on 2026-11-15 (skull key, object 270). See ART_TO_SPRITE.md.

    python tools/art_to_sprite.py ART.png OBJ [--out DIR] [--sizes 1,2,4,8,16] [--sprites DIR]
    python tools/art_to_sprite.py ART.png NAME --sprite FRAME.png --bg corner     (creatures; a non-white background)

  ART.png  the new picture: the item on a plain white background (any size, e.g. 1024x576 from an image generator)
  OBJ      the object number, e.g. 270 (its sprite: <sprites>/objects/270.png, 16x16)

Steps:
  1. cut-out: white connected to the border is background; an enclosed white patch over 200 px is a hole (the key's
     bit) and is background too; small white specks inside stay (highlights)
  2. fit: the cut-out is scaled (Lanczos, premultiplied so no white fringe) to fit the ORIGINAL sprite's opaque box
     times the factor, centred in it, on a (16*factor)^2 canvas: so it lands where the original item lies
  3. alpha cut at 128 (on/off, as all UW sprites)
  4. nearest colour: every opaque pixel set to the nearest (RGB distance) of the original sprite's own colours
     (key 270: 9 colours). No dithering. This is the version the user liked ("the nearest is neat").

Writes, in --out (default work/art/<OBJ>):
  <OBJ>-cutout.png                        step 1, full size
  <OBJ>-x<f>-<16f>px.png                  steps 2-3, full colour
  <OBJ>-x<f>-<16f>px-origcolours.png      step 4  <- the one to use
  <OBJ>-sheet.png                         original | full colour | nearest colours, each size shown 320 px
Nothing is overwritten: an existing output folder gets the new files with a -N suffix folder instead.
"""
import argparse, os
from collections import deque

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def cutout(path, bg_col=None, tol=24, shadow=False):
    a = np.asarray(Image.open(path).convert('RGB')).astype(int)
    H, W = a.shape[:2]
    if bg_col is None:      # a white background
        white = (a.min(axis=2) > 225) & (a.max(axis=2) - a.min(axis=2) < 25)
    else:                   # a plain background of that colour ('white' below: background-coloured)
        white = np.abs(a - np.array(bg_col)).max(axis=2) <= tol
    bg = np.zeros((H, W), bool)
    seen = np.zeros((H, W), bool)
    for y in range(H):
        for x in range(W):
            if not white[y, x] or seen[y, x]:
                continue
            comp, q, edge = [], deque([(y, x)]), False
            seen[y, x] = True
            while q:
                v, u = q.popleft()
                comp.append((v, u))
                edge |= v in (0, H - 1) or u in (0, W - 1)
                for dv, du in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    vv, uu = v + dv, u + du
                    if 0 <= vv < H and 0 <= uu < W and white[vv, uu] and not seen[vv, uu]:
                        seen[vv, uu] = True
                        q.append((vv, uu))
            if edge or len(comp) > 200:          # outside, or a hole through the item
                ys, xs = zip(*comp)
                bg[list(ys), list(xs)] = True
    if shadow:   # also a pale grey drop shadow touching the background (generators draw one under the item)
        grey = (a.min(axis=2) > 150) & (a.max(axis=2) - a.min(axis=2) < 20)
        q = deque(zip(*np.nonzero(bg)))
        while q:
            v, u = q.popleft()
            for dv, du in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                vv, uu = v + dv, u + du
                if 0 <= vv < H and 0 <= uu < W and grey[vv, uu] and not bg[vv, uu]:
                    bg[vv, uu] = True
                    q.append((vv, uu))
    im = Image.fromarray(np.dstack([a, np.where(bg, 0, 255)]).astype(np.uint8), 'RGBA')
    return im.crop(im.getbbox())


def fit(cut, orig, f):
    bx0, by0, bx1, by1 = orig.getbbox()
    bw, bh = (bx1 - bx0) * f, (by1 - by0) * f
    s = min(bw / cut.width, bh / cut.height)
    w, h = max(1, round(cut.width * s)), max(1, round(cut.height * s))
    p = np.asarray(cut).astype(float)
    p[..., :3] *= p[..., 3:] / 255
    rgb = Image.fromarray(p[..., :3].astype(np.uint8)).resize((w, h), Image.LANCZOS)
    al = np.asarray(cut.getchannel('A').resize((w, h), Image.LANCZOS)).astype(float)
    c = np.asarray(rgb).astype(float) * 255 / np.maximum(al, 1)[..., None]
    k = np.dstack([np.clip(c, 0, 255), np.where(al >= 128, 255, 0)]).astype(np.uint8)
    k[k[..., 3] == 0] = 0
    canvas = Image.new('RGBA', (orig.width * f, orig.height * f), (0, 0, 0, 0))
    canvas.alpha_composite(Image.fromarray(k, 'RGBA'), (bx0 * f + (bw - w) // 2, by0 * f + (bh - h) // 2))
    return canvas


def nearest(im, orig):
    oa = np.asarray(orig)
    cols = np.unique(oa[oa[..., 3] >= 128][:, :3], axis=0).astype(int)
    a = np.array(im)
    m = a[..., 3] >= 128
    px = a[m][:, :3].astype(int)
    a[m, :3] = cols[((px[:, None, :] - cols[None]) ** 2).sum(-1).argmin(1)]
    return Image.fromarray(a, 'RGBA')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('art')
    ap.add_argument('obj', help='object number (sprites/objects/NNN.png), or a name when --sprite is given')
    ap.add_argument('--sprite', help='the original sprite to fit to (e.g. a creature frame), instead of objects/OBJ')
    ap.add_argument('--bg', help='background colour R,G,B or "corner" (the top-left pixel) instead of white')
    ap.add_argument('--tol', type=int, default=24, help='how far (per channel) from --bg still counts as background')
    ap.add_argument('--shadow', action='store_true', help='also cut a pale grey drop shadow touching the background')
    ap.add_argument('--out')
    ap.add_argument('--sizes', default='1,2,4,8,16', help='factors of the 16x16 sprite')
    ap.add_argument('--sprites', default=os.path.join(ROOT, 'dist', 'UW1', 'data', 'sprites'))
    a = ap.parse_args()
    out = a.out or os.path.join(ROOT, 'work', 'art', str(a.obj))
    base, n = out, 1
    while os.path.exists(out) and os.listdir(out):
        n += 1
        out = f'{base}-{n}'
    os.makedirs(out, exist_ok=True)
    sp = a.sprite or os.path.join(a.sprites, 'objects', f'{int(a.obj):03d}.png')
    orig = Image.open(sp).convert('RGBA')
    bg = None
    if a.bg == 'corner':
        bg = Image.open(a.art).convert('RGB').getpixel((0, 0))
    elif a.bg:
        bg = tuple(int(v) for v in a.bg.split(','))
    cut = cutout(a.art, bg, a.tol, a.shadow)
    cut.save(os.path.join(out, f'{a.obj}-cutout.png'))
    rows = []
    for f in [int(v) for v in a.sizes.split(',')]:
        full = fit(cut, orig, f)
        near = nearest(full, orig)
        tag = f'{a.obj}-x{f}-{orig.width * f}px'
        full.save(os.path.join(out, tag + '.png'))
        near.save(os.path.join(out, tag + '-origcolours.png'))
        rows.append((orig.width * f, full, near))
    P = 320
    sh = Image.new('RGBA', ((len(rows) + 1) * (P + 20) + 10, 2 * (P + 40) + 40), (70, 80, 95, 255))
    d = ImageDraw.Draw(sh)
    d.text((10, 6), f'original {a.obj} {orig.width}x{orig.height}', fill=(240, 220, 130))
    def zoom(im):   # as big as fits P x P, whole pixels, shape kept
        z = max(1, P // max(im.size))
        return im.resize((im.width * z, im.height * z), Image.NEAREST)
    sh.alpha_composite(zoom(orig), (10, 24))
    for r, (lab, y0) in enumerate((('full colour', 24), ('nearest original colours', P + 64))):
        for c, (sz, full, near) in enumerate(rows):
            x = 10 + (c + 1) * (P + 20)
            d.text((x, y0 - 18), f'{full.width}x{full.height} {lab}', fill=(240, 220, 130))
            d.rectangle((x - 1, y0 - 1, x + P, y0 + P), outline=(110, 120, 135))
            sh.alpha_composite(zoom(full if r == 0 else near), (x, y0))
    sh.convert('RGB').save(os.path.join(out, f'{a.obj}-sheet.png'))
    print('->', out)


if __name__ == '__main__':
    main()
