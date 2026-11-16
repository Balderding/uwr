"""assets.py: decoders for Ultima Underworld's data files (copied into uo-new/game), the
reference for the C engine's loaders.

    python -I assets.py check  [UW folder]      decode everything, print counts, write nothing
    python -I assets.py export [UW folder]      also write PNGs to uo-new/extracted

The game folder is only read. Output goes only to uo-new/extracted.
Format details are checked against the files (sizes, offsets, value ranges) and recorded in
uo-new/NOTES.md; anything not yet confirmed from UW.EXE is marked "unconfirmed" there.
"""
import os
import struct
import sys
import zlib

# Paths are relative to this folder (uo-new\tools), so uo-new can be moved anywhere.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UW = os.path.join(ROOT, 'game')          # copy of the user's original files
OUT = os.path.join(ROOT, 'extracted')


def rd(name):
    with open(os.path.join(UW, name), 'rb') as f:
        return f.read()


# --- palettes -------------------------------------------------------------------------------
def palettes(d):
    """PALS.DAT: 8 palettes of 256 VGA 6-bit (r,g,b); returned scaled to 0..255."""
    assert len(d) == 8 * 768 and max(d) <= 63
    pals = []
    for p in range(8):
        raw = d[p * 768:(p + 1) * 768]
        pals.append([tuple((c << 2) | (c >> 4) for c in raw[3 * i:3 * i + 3]) for i in range(256)])
    return pals


# --- textures (.TR) -------------------------------------------------------------------------
def textures(d):
    """.TR: u8 2, u8 size, u16 count, u32 offset[count]; each entry size*size palette indices."""
    kind, size, count = struct.unpack_from('<BBH', d, 0)
    assert kind == 2
    offs = struct.unpack_from('<%dI' % count, d, 4)
    texs = []
    for o in offs:
        assert o + size * size <= len(d)
        texs.append(d[o:o + size * size])
    return size, texs


# --- LEV.ARK --------------------------------------------------------------------------------
def ark(d):
    """u16 count, u32 offset[count] (0 = empty block). Block size = gap to the next offset."""
    n = struct.unpack_from('<H', d, 0)[0]
    offs = struct.unpack_from('<%dI' % n, d, 2)
    present = sorted(set(o for o in offs if o))
    blocks = []
    for o in offs:
        if not o:
            blocks.append(None)
            continue
        i = present.index(o)
        end = present[i + 1] if i + 1 < len(present) else len(d)
        blocks.append(d[o:end])
    return blocks


TILE_TYPES = ['solid', 'open', 'diag_se', 'diag_sw', 'diag_ne', 'diag_nw',
              'slope_n', 'slope_s', 'slope_e', 'slope_w']


def tilemap(b):
    """64x64 tiles, row 0 = south (y up), 2 u16 each:
    w0: bits 0-3 type, 4-7 floor height, 10-13 floor texture slot, 14 no-magic, 15 door
    w1: bits 0-5 wall texture slot, 6-15 first object index."""
    tiles = []
    for i in range(64 * 64):
        w0, w1 = struct.unpack_from('<HH', b, 4 * i)
        tiles.append(dict(type=w0 & 15, height=(w0 >> 4) & 15, bit8=(w0 >> 8) & 3,
                          floor=(w0 >> 10) & 15, nomagic=(w0 >> 14) & 1, door=w0 >> 15,
                          wall=w1 & 63, obj=w1 >> 6))
    return tiles


def objects(b):
    """1024 objects after the tile map (0x4000): 256 mobile of 27 bytes, then 768 static of 8.
    General 8 bytes, 4 u16:
      w0: 0-8 item id, 9-15 flags      w1: 0-6 z, 7-9 heading, 10-12 y in tile, 13-15 x in tile
      w2: 0-5 quality, 6-15 next       w3: 0-5 owner/special, 6-15 quantity or link."""
    objs = []
    for i in range(1024):
        o = 0x4000 + (i * 27 if i < 256 else 256 * 27 + (i - 256) * 8)
        w0, w1, w2, w3 = struct.unpack_from('<4H', b, o)
        objs.append(dict(item=w0 & 511, flags=w0 >> 9, z=w1 & 127, heading=(w1 >> 7) & 7,
                         y=(w1 >> 10) & 7, x=w1 >> 13, quality=w2 & 63, next=w2 >> 6,
                         owner=w3 & 63, link=w3 >> 6,
                         mobile=b[o + 8:o + 27] if i < 256 else None))
    return objs


def texmap(b):
    """Texture-mapping block (LEV.ARK 18-26): 48 u16 wall textures (W64), 10 u16 floor (F32),
    6 u8 door textures."""
    assert len(b) >= 122
    walls = list(struct.unpack_from('<48H', b, 0))
    floors = list(struct.unpack_from('<10H', b, 96))
    doors = list(b[116:122])
    return walls, floors, doors


# --- PNG ------------------------------------------------------------------------------------
def png(path, w, h, rgb):
    """rgb: bytes/bytearray of w*h*3."""
    raw = b''.join(b'\0' + bytes(rgb[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(t, data):
        c = t + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def sheet(path, size, texs, pal, cols):
    rows = (len(texs) + cols - 1) // cols
    w, h = cols * size, rows * size
    img = bytearray(w * h * 3)
    for n, t in enumerate(texs):
        ox, oy = (n % cols) * size, (n // cols) * size
        for y in range(size):
            for x in range(size):
                r, g, bl = pal[t[y * size + x]]
                p = ((oy + y) * w + ox + x) * 3
                img[p:p + 3] = bytes((r, g, bl))
    png(path, w, h, img)


def level_png(path, tiles, objs, walls, floors, w64, f32, pal):
    """Top view, north up, 16 px per tile: floor texture on open parts, wall texture on solid
    parts, darker for lower floors; red dots = creatures (items 64-127), yellow = other objects."""
    T = 16
    W = 64 * T
    img = bytearray(W * W * 3)
    for ty in range(64):
        for tx in range(64):
            t = tiles[ty * 64 + tx]
            ftex = f32[floors[t['floor']]] if t['floor'] < len(floors) and floors[t['floor']] < len(f32) else None
            wtex = w64[walls[t['wall']]] if t['wall'] < len(walls) and walls[t['wall']] < len(w64) else None
            shade = 0.45 + 0.55 * t['height'] / 15
            for py in range(T):
                for px in range(T):
                    # tile-local coords, y up
                    lx, ly = px / T, 1 - py / T
                    k = t['type']
                    if k == 0:
                        solid = True
                    elif k == 2:
                        solid = lx < ly        # open SE half
                    elif k == 3:
                        solid = lx + ly > 1    # open SW half
                    elif k == 4:
                        solid = lx + ly < 1    # open NE half
                    elif k == 5:
                        solid = lx > ly        # open NW half
                    else:
                        solid = False
                    if solid:
                        c = pal[wtex[(py * 4) * 64 + px * 4]] if wtex else (0, 0, 0)
                        c = tuple(int(v * 0.35) for v in c)
                    else:
                        c = pal[ftex[(py * 2) * 32 + px * 2]] if ftex else (255, 0, 255)
                        c = tuple(int(v * shade) for v in c)
                        if t['door'] and (px in (0, T - 1) or py in (0, T - 1)):
                            c = (60, 160, 255)
                    p = (((63 - ty) * T + py) * W + tx * T + px) * 3
                    img[p:p + 3] = bytes(c)
            # objects in this tile
            i, seen = t['obj'], 0
            while i and seen < 1024:
                o = objs[i]
                ox = tx * T + 1 + o['x'] * 2
                oy = (63 - ty) * T + T - 2 - o['y'] * 2
                c = (255, 40, 40) if 64 <= o['item'] < 128 else (255, 230, 60)
                for dy in (0, 1):
                    for dx in (0, 1):
                        p = ((oy + dy) * W + ox + dx) * 3
                        img[p:p + 3] = bytes(c)
                i, seen = o['next'], seen + 1
    png(path, W, W, img)


# --- driver ---------------------------------------------------------------------------------
def main():
    global UW
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'check'
    if len(sys.argv) > 2:
        UW = sys.argv[2]
    pals = palettes(rd('DATA/PALS.DAT'))
    print('palettes', len(pals))
    tex = {}
    for name in ('W64.TR', 'W16.TR', 'F32.TR', 'F16.TR'):
        size, t = textures(rd('DATA/' + name))
        tex[name] = t
        print(name, 'size', size, 'count', len(t))
    blocks = ark(rd('DATA/LEV.ARK'))
    sizes = {}
    for i, b in enumerate(blocks):
        if b is not None:
            sizes.setdefault(i // 9, set()).add(len(b))
    print('LEV.ARK blocks', len(blocks), 'sizes by group of 9:', {k: sorted(v) for k, v in sizes.items()})
    for lv in range(9):
        tiles = tilemap(blocks[lv])
        objs = objects(blocks[lv])
        walls, floors, doors = texmap(blocks[18 + lv])
        kinds = [0] * 16
        for t in tiles:
            kinds[t['type']] += 1
        assert max(t['type'] for t in tiles) <= 9, 'tile type > 9'
        assert max(walls) < len(tex['W64.TR']) and max(floors) < len(tex['F32.TR'])
        reach = 0
        for t in tiles:
            i, seen = t['obj'], 0
            while i and seen < 1024:
                reach += 1
                i, seen = objs[i]['next'], seen + 1
            assert seen < 1024, 'object chain loop'
        creatures = sum(1 for t in tiles for _ in [0] if False)
        print('level', lv + 1, 'types', kinds[:10], 'objects in tiles', reach,
              'doors', sum(t['door'] for t in tiles), 'walls', walls[:6], '... floors', floors)
        if cmd == 'export':
            level_png('%s/levels/level%d.png' % (OUT, lv + 1), tiles, objs, walls, floors,
                      tex['W64.TR'], tex['F32.TR'], pals[0])
    if cmd == 'export':
        sheet(OUT + '/textures/W64.png', 64, tex['W64.TR'], pals[0], 15)
        sheet(OUT + '/textures/F32.png', 32, tex['F32.TR'], pals[0], 13)
        print('written to', OUT)


if __name__ == '__main__':
    main()
