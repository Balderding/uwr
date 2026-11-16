# Soft occlusion (NNN_o) and height (NNN_p) maps for the ORIGINAL walls and floors (the user, 2026-11-16: the
# Laigter ones were too abrupt: the occlusion fell from white to dark in a texel or two, the height was nearly a
# step at each joint with the faces' speckle left in). Ours, not Laigter's; the normal maps (NNN_n) are not touched.
#
#   height: the texture's grey, 4x, blurred round the tile's edges (it repeats), then bent softly about the
#           texture's median grey (tanh): joints sink by a rounded shoulder, the faces' speckle nearly flat.
#           Saved at 4x the texture's size (level.c takes any size, smooth into the 4x normal atlas's alpha).
#   occlusion: from that height: how far a place lies under its surroundings (the joints), plus the same spread
#           wide (the stones' edges darken gently toward the joint). Saved at 4x as well (256 for the walls).
#
# Usage: make_soft_maps.py OUT [--variant NAME] [--only 000,030,...] [--kinds walls,floors]
#   OUT\walls\NNN_o.png, NNN_p.png, OUT\floors\...   (data\laigter to put them in the game)
import sys, os, argparse
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
UO = os.path.dirname(HERE)

# name: (height blur, texels; AO near blur; AO wide blur; AO near strength; AO wide strength; darkest AO)
VARIANTS = {
    "A": (0.70, 1.2, 1.6, 3.6, 2.6, 0.45),   # gentle: a little softer than Laigter's
    "B": (1.00, 1.6, 2.4, 3.4, 3.4, 0.48),   # soft
    "C": (1.40, 2.2, 3.4, 3.2, 4.4, 0.50),   # very soft, wide
}
UP = 4


def blur(a, sigma):
    """Gaussian blur that wraps round the picture's edges (the textures tile)."""
    if sigma <= 0:
        return a
    h, w = a.shape
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.rfftfreq(w)[None, :]
    g = np.exp(-2.0 * (np.pi * sigma) ** 2 * (fx * fx + fy * fy))
    return np.fft.irfft2(np.fft.rfft2(a) * g, s=a.shape)


def grey(path):
    rgb = np.asarray(Image.open(path).convert("RGB")).astype(np.float64)
    return (rgb[..., 0] * 11 + rgb[..., 1] * 16 + rgb[..., 2] * 5) / 32.0 / 255.0   # Qt's qGray, as Laigter


def make(g, v):
    hb, an, aw, kn, kw, floor_ = VARIANTS[v]
    med = float(np.median(g))
    big = np.kron(g, np.ones((UP, UP)))
    b = blur(big, hb * UP)
    w = max(0.14, 0.6 * med)                      # the bend's width: by the texture's own brightness
    d = (b - med) / w
    t = np.where(d < 0, np.tanh(d), 0.35 * np.tanh(d))   # joints sink fully, highlights rise a little
    hgt = 0.62 + 0.36 * t                         # faces about 0.62, the deepest joints about 0.27
    near = np.clip(blur(hgt, an * UP) - hgt, 0, None)     # under its surroundings: the joint itself
    wide = blur(near, aw * UP)                            # and spread: the stones' edges
    ao = 1.0 - kn * near - kw * wide
    ao = np.clip(blur(ao, 0.35 * UP), floor_, 1.0)
    return ao, hgt     # both at 4x: level.c lays both smooth into the 4x normal atlas (blue, alpha)


def save(a, path):
    Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), "L").save(path, optimize=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--variant", default="B")
    ap.add_argument("--only", default="")
    ap.add_argument("--kinds", default="walls,floors")
    a = ap.parse_args()
    only = set(a.only.split(",")) if a.only else None
    made = 0
    for kind in a.kinds.split(","):
        src = os.path.join(UO, "data", "textures", kind)
        dst = os.path.join(a.out, kind)
        os.makedirs(dst, exist_ok=True)
        for f in sorted(os.listdir(src)):
            n, e = os.path.splitext(f)
            if e.lower() != ".png" or (only and n not in only):
                continue
            g = grey(os.path.join(src, f))
            if g.shape[0] < 3 or g.shape[1] < 3:
                continue
            ao, hgt = make(g, a.variant)
            save(ao, os.path.join(dst, n + "_o.png"))
            save(hgt, os.path.join(dst, n + "_p.png"))
            made += 1
    print("made %d textures' maps (variant %s) in %s" % (made, a.variant, a.out))


if __name__ == "__main__":
    main()
