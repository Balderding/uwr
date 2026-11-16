# The original walls' and floors' normal maps at 4x (256 for the 64 texel walls; the user, 2026-11-16: "too blocky").
# The same Laigter normals the user chose (tools\laigter-cli\texture.preset), only smooth between their texels:
# each map enlarged 4x by a bicubic curve that wraps round the tile's edges, the normals brought back to length 1.
# level.c lays a 4x map straight into the 4x normal atlas (texel for texel).
#
# Usage: upsize_normals.py SRC OUT      SRC\walls\NNN_n.png (the texture-sized Laigter maps) -> OUT\walls\NNN_n.png
import sys, os
import numpy as np
from PIL import Image

UP, PAD = 4, 8
src, out = sys.argv[1], sys.argv[2]
made = 0
for kind in ("walls", "floors"):
    os.makedirs(os.path.join(out, kind), exist_ok=True)
    for f in sorted(os.listdir(os.path.join(src, kind))):
        if not f.endswith("_n.png"):
            continue
        a = np.asarray(Image.open(os.path.join(src, kind, f)).convert("RGB")).astype(np.float32)
        h, w = a.shape[:2]
        p = np.pad(a, ((PAD, PAD), (PAD, PAD), (0, 0)), mode="wrap")
        big = np.stack([np.asarray(Image.fromarray(p[..., c], "F").resize(((w + 2 * PAD) * UP, (h + 2 * PAD) * UP), Image.BICUBIC))
                        for c in range(3)], axis=2)
        big = big[PAD * UP:(PAD + h) * UP, PAD * UP:(PAD + w) * UP]
        n = big / 127.5 - 1.0
        n /= np.maximum(np.linalg.norm(n, axis=2, keepdims=True), 1e-6)
        Image.fromarray(np.clip((n + 1.0) * 127.5 + 0.5, 0, 255).astype(np.uint8), "RGB").save(os.path.join(out, kind, f), optimize=True)
        made += 1
print("made %d normal maps at %dx in %s" % (made, UP, out))
