# hud_art.py: our HUD's two pictures from the user's art (2026-11-14), each into a header the renderer carries.
#   gargoyle SRC   the gargoyle face (the picture's left half; "just scale it to take up a small area at the top centre
#                  ... a glow effect that shows the enemy's health") -> src\hud_gargoyle.h: RGBA, the eyes' glow mask
#                  (the dark of the eye sockets) and the halo (alpha blurred: the glow round the face)
#   charge SRC     the stone pedestal with the fire basin ("this could be used for the swing indicator ... resize it and
#                  make it fit ... next to the compass") -> src\hud_charge.h: RGBA, the fire's mask (its orange and yellow)
#                  and each fire pixel's distance from the basin's centre (0 centre .. 255 rim): the glow spreads out
#                  from the centre as a blow charges
# The black surround (the black reached from the picture's edge) is cut away, the black inside kept.
# Usage: uv run --no-project --with pillow --with numpy python -I tools\hud_art.py gargoyle|charge SRC
import sys, os
from collections import deque
import numpy as np
from PIL import Image, ImageFilter

CLOSE = 13   # (odd) the widest outline gap closed, in the source's pixels

def surround(a):
    bg = surround_mask(a)
    ys, xs = np.nonzero(~bg)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    return a[y0:y1, x0:x1], bg[y0:y1, x0:x1]

def surround_mask(a):
    """the black reached from the picture's edge, the outline's gaps closed: True where cut away (the picture's size)"""
    h, w, _ = a.shape
    dark = a.max(axis=2) < 28
    bg = np.zeros((h, w), bool)
    q = deque()
    for x in range(w):
        for y in (0, h - 1):
            if dark[y, x] and not bg[y, x]: bg[y, x] = True; q.append((y, x))
    for y in range(h):
        for x in (0, w - 1):
            if dark[y, x] and not bg[y, x]: bg[y, x] = True; q.append((y, x))
    while q:
        y, x = q.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and dark[ny, nx] and not bg[ny, nx]:
                bg[ny, nx] = True; q.append((ny, nx))
    # the outline's gaps closed (the pedestal's tiers: their dark seams reach the surround and were cut with it): the
    # picture's mask grown and shrunk by CLOSE pixels, so a seam narrower than that is kept
    fg = Image.fromarray((~bg).astype(np.uint8) * 255, 'L')
    fg = fg.filter(ImageFilter.MaxFilter(CLOSE)).filter(ImageFilter.MinFilter(CLOSE))
    return np.asarray(fg) < 128

def scale(a, bg, planes, h_out, pad):
    """the picture (premultiplied while scaled) and its planes (0..255) at h_out rows, pad round; RGBA straight"""
    fh, fw = a.shape[:2]
    alpha = np.where(bg, 0, 255).astype(np.uint8)
    ow = round(fw * h_out / fh)
    pre = np.dstack([a * (alpha[..., None] / 255.0), alpha]).astype(np.uint8)
    pre = np.asarray(Image.fromarray(pre, 'RGBA').resize((ow, h_out), Image.LANCZOS)).astype(np.float32)
    W2, H2 = ow + 2 * pad, h_out + 2 * pad
    out = np.zeros((H2, W2, 4), np.uint8)
    al = pre[..., 3]
    rgb = np.where(al[..., None] > 0, pre[..., :3] * 255 / np.maximum(al[..., None], 1), 0)
    out[pad:pad + h_out, pad:pad + ow, :3] = np.clip(rgb, 0, 255)
    out[pad:pad + h_out, pad:pad + ow, 3] = np.clip(al, 0, 255)
    res = []
    for p in planes:
        z = np.zeros((H2, W2), np.uint8)
        z[pad:pad + h_out, pad:pad + ow] = np.asarray(Image.fromarray(p.astype(np.uint8), 'L').resize((ow, h_out), Image.LANCZOS))
        res.append(z)
    return out, res

def arr(name, data):
    b = data.reshape(-1)
    lines = [','.join(str(int(v)) for v in b[i:i + 40]) for i in range(0, len(b), 40)]
    return 'static const unsigned char %s[%d] = {\n%s\n};\n' % (name, len(b), ',\n'.join(lines))

def write(path, prefix, what, src, out, pad, named):
    with open(path, 'w') as f:
        f.write('/* made by tools\\hud_art.py from %s (the user\'s picture, 2026-11-14): %s; do not edit */\n' % (src, what))
        f.write('#define %s_W %d\n#define %s_H %d\n#define %s_PAD %d\n' % (prefix, out.shape[1], prefix, out.shape[0], prefix, pad))
        for n, d in named: f.write(arr(n, d))
    Image.fromarray(out, 'RGBA').save(os.path.join('work', 'hud', prefix.lower() + '-cut.png'))

os.makedirs(os.path.join('work', 'hud'), exist_ok=True)
kind, path = sys.argv[1], sys.argv[2]
a = np.asarray(Image.open(path).convert('RGB')).astype(np.int32)
if kind == 'gargoyle':
    a = a[:, : a.shape[1] // 2]
    a, bg = surround(a)
    fh, fw = a.shape[:2]
    luma = (a[..., 0] * 3 + a[..., 1] * 6 + a[..., 2]) / 10
    yy, xx = np.mgrid[0:fh, 0:fw]
    band = (yy > fh * 0.28) & (yy < fh * 0.52) & (np.abs(xx - fw / 2) > fw * 0.06) & (np.abs(xx - fw / 2) < fw * 0.36)
    eye = np.clip((70 - luma) / 50, 0, 1) * band * (~bg) * 255
    eye = np.asarray(Image.fromarray(eye.astype(np.uint8), 'L').filter(ImageFilter.GaussianBlur(2)))
    pad = 12
    out, (eyes,) = scale(a, bg, [eye], 112, pad)
    halo = np.asarray(Image.fromarray(out[..., 3], 'L').filter(ImageFilter.GaussianBlur(6))).astype(np.float32)
    halo = np.clip(halo * 1.6, 0, 255).astype(np.uint8)
    write(os.path.join('src', 'hud_gargoyle.h'), 'GARG', 'the gargoyle face, RGBA (straight alpha), the eyes\' glow mask and the halo',
          path.replace('/', '\\'), out, pad, [('garg_rgba', out), ('garg_eyes', eyes), ('garg_halo', halo)])
elif kind == 'charge':
    a, bg = surround(a)
    fh, fw = a.shape[:2]
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    fire = ((r > 150) & (r > b + 90) & (r >= g)) & ~bg
    fy, fx = np.nonzero(fire)
    cy, cx = (fy.min() + fy.max()) / 2, (fx.min() + fx.max()) / 2
    ry, rx = max((fy.max() - fy.min()) / 2, 1), max((fx.max() - fx.min()) / 2, 1)
    yy, xx = np.mgrid[0:fh, 0:fw]
    inside = (((yy - cy) / ry) ** 2 + ((xx - cx) / rx) ** 2) <= 1.0
    mask = (fire | (inside & (r > g * 0.9) & ~bg)) * 255   # the basin's fire, its darker browns inside the ellipse too
    mask = np.asarray(Image.fromarray(mask.astype(np.uint8), 'L').filter(ImageFilter.GaussianBlur(1.5)))
    rad = np.clip(np.sqrt(((yy - cy) / ry) ** 2 + ((xx - cx) / rx) ** 2), 0, 1) * 255
    pad = 2
    out, (m, d) = scale(a, bg, [mask, rad], 72, pad)
    write(os.path.join('src', 'hud_charge.h'), 'CHG', 'the swing indicator\'s pedestal, RGBA, the fire mask and its radius',
          path.replace('/', '\\'), out, pad, [('chg_rgba', out), ('chg_fire', m), ('chg_rad', d)])
elif kind == 'panel':
    # the bottom panel ("for the compass and text box ... the left shelf for buffs, the right for the rune shelf"):
    # opaque, the whole picture at half size (its pixel art's grain is about 4 source pixels); the renderer places the
    # compass, spells, runes and the message lines by the source's 1024 x 282 coordinates (uwview.c PNL_*)
    im = Image.fromarray(a.astype(np.uint8), 'RGB').resize((512, 141), Image.LANCZOS)
    out = np.dstack([np.asarray(im), np.full((141, 512), 255, np.uint8)])
    write(os.path.join('src', 'hud_panel.h'), 'PNL', 'the bottom panel, RGBA (opaque)', path.replace('/', '\\'), out, 0,
          [('pnl_rgba', out)])
elif kind == 'panel2':
    # the second bottom panel (the user, 2026-11-14: "use this for the graphic ... the swing indicator into the top
    # thing as a glowing light ... the side tanks are health and mana"): full size (its grain is about 2 source
    # pixels), the black above its ends cut away; the tanks' plane marks the dark of their glass (x 15-90 and 932-1012,
    # rows 107-265), where the liquid shows, so the glass's frame and marks stay over it
    h, w = a.shape[:2]
    alpha = np.where(surround_mask(a), 0, 255).astype(np.uint8)   # (kept at the picture's size: the coordinates hold)
    luma = (a[..., 0] * 3 + a[..., 1] * 6 + a[..., 2]) / 10
    yy, xx = np.mgrid[0:h, 0:w]
    rect = ((xx >= 15) & (xx <= 90) | (xx >= 932) & (xx <= 1012)) & (yy >= 107) & (yy <= 265)
    tank = (rect & (luma < 60) & (alpha > 0)).astype(np.uint8) * 255
    out = np.dstack([a.astype(np.uint8), alpha])
    write(os.path.join('src', 'hud_panel.h'), 'PNL', 'the bottom panel with its tanks, RGBA, and the tanks\' glass plane',
          path.replace('/', '\\'), out, 0, [('pnl_rgba', out), ('pnl_tank', tank)])
    Image.fromarray(tank, 'L').save(os.path.join('work', 'hud', 'pnl-tank.png'))
elif kind == 'gargoyle2':
    # the second gargoyle (the user, 2026-11-14: "use this for the gargoyle ... sit flush with the top of the screen
    # centred and use the eye holes for the enemy health"): the bar with the face, full size, the black below it cut
    # away; the eyes' plane the dark of the eye holes (rows 85-112, x 225-322 and 350-447), the halo that plane
    # blurred a little (the glow's spill onto the brow and cheeks)
    h, w = a.shape[:2]
    alpha = np.where(surround_mask(a), 0, 255).astype(np.uint8)
    luma = (a[..., 0] * 3 + a[..., 1] * 6 + a[..., 2]) / 10
    yy, xx = np.mgrid[0:h, 0:w]
    box = (yy >= 85) & (yy <= 112) & (((xx >= 225) & (xx <= 322)) | ((xx >= 350) & (xx <= 447)))
    eyes = (np.clip((70 - luma) / 40, 0, 1) * box * (alpha > 0) * 255).astype(np.uint8)
    halo = np.asarray(Image.fromarray(eyes, 'L').filter(ImageFilter.GaussianBlur(5))).astype(np.float32)
    halo = np.clip(halo * 1.8, 0, 255).astype(np.uint8)
    out = np.dstack([a.astype(np.uint8), alpha])
    write(os.path.join('src', 'hud_gargoyle.h'), 'GARG', 'the gargoyle bar, RGBA, the eye holes\' glow plane and its spill',
          path.replace('/', '\\'), out, 0, [('garg_rgba', out), ('garg_eyes', eyes), ('garg_halo', halo)])
    Image.fromarray(eyes, 'L').save(os.path.join('work', 'hud', 'garg2-eyes.png'))
print('hud_art: %s %dx%d' % (kind, out.shape[1], out.shape[0]))
