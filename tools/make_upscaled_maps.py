"""The relief maps of the walls' and floors' upscaled textures (the user, 2026-11-15: "redo the normal, AO and height
maps to be 256x256 and 512x512 to match the higher res textures"; of the two trials the user chose the crisp one).

For each of data\\upscaled\\{walls,floors}\\{256,512}\\NNN.png, laigter-cli (tools\\laigter-cli, build\\laigter-cli.exe)
makes its normal, occlusion and height maps at the texture's own size, tiled, with texture.preset's settings scaled to
the larger texture (texture_x4.preset for the "256" folders, 4 times the original's size; texture_x8.preset for "512",
8 times): the occlusion's distance by the scale, the blurs and the normal's depth by half the scale (the crisp look:
the upscale's own grain shows in the relief). Then the three are packed into ONE picture a texture, which the port
reads (uwview.c's hires_apply) and the shader looks up beside the colours:

    data\\laigter\\upscaled\\{walls,floors}\\{256,512}\\NNN_m.png     RGBA: R, G the normal's x, y (its z is worked out),
                                                                   B the occlusion, A the height

The maps themselves stay in work\\upscaled_maps (scratch). Run with uv:
    uv run --no-project --with pillow python -I tools\\make_upscaled_maps.py
"""
import os
import subprocess
import sys
from PIL import Image

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(HERE, 'data', 'upscaled')
WORK = os.path.join(HERE, 'work', 'upscaled_maps')
DST = os.path.join(HERE, 'data', 'laigter', 'upscaled')
CLI = os.path.join(HERE, 'build', 'laigter-cli.exe')
PRESET = {'256': 'texture_x4.preset', '512': 'texture_x8.preset'}

made = 0
for kind, count in (('walls', 210), ('floors', 52)):
    for res in ('256', '512'):
        src, work, dst = os.path.join(SRC, kind, res), os.path.join(WORK, kind, res), os.path.join(DST, kind, res)
        os.makedirs(work, exist_ok=True)
        os.makedirs(dst, exist_ok=True)
        r = subprocess.run([CLI, '-q', '-n', '-o', '-p', '--tile', '--occlusion-thresh-median',
                            '-r', os.path.join(HERE, 'tools', 'laigter-cli', PRESET[res]), '-d', src, '-l', work])
        if r.returncode:
            sys.exit('make_upscaled_maps.py: laigter-cli failed on %s' % src)
        for i in range(count):
            n = '%03d' % i
            if not os.path.exists(os.path.join(src, n + '.png')):
                continue
            nrm = Image.open(os.path.join(work, n + '_n.png')).convert('RGB')
            occ = Image.open(os.path.join(work, n + '_o.png')).convert('L')
            hgt = Image.open(os.path.join(work, n + '_p.png')).convert('L')
            if occ.size != nrm.size or hgt.size != nrm.size:
                sys.exit('make_upscaled_maps.py: %s %s %s: the maps differ in size' % (kind, res, n))
            x, y, _ = nrm.split()
            Image.merge('RGBA', (x, y, occ, hgt)).save(os.path.join(dst, n + '_m.png'), optimize=True)
            made += 1
print('make_upscaled_maps.py: %d packed maps in %s' % (made, DST))
