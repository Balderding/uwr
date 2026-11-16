"""loose.py: the reference for the engine's loose data files (uo-new/data). The engine reads only
these; when one is missing it writes it itself from uo-new/game (src/extract.c). This script writes
the same files byte for byte, so the C extractor can be checked against it.

    python -I loose.py write [out folder]    default uo-new/extracted/data-ref
    python -I loose.py roundtrip             rebuild every level block from its text, compare bytes

Layout:
  textures/walls/NNN.png    W64.TR, 64x64, 8-bit indexed PNG with palette 0 (index = the original's)
  textures/floors/NNN.png   F32.TR, 32x32, the same
  levels/levelN.txt         LEV.ARK blocks N-1 (level), 8+N (overlay block), 17+N (texture map), lossless

PNGs are written with stored (uncompressed) deflate blocks: simple to write identically in C and Python.
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import assets  # noqa: E402

OUT = os.path.join(assets.ROOT, 'extracted', 'data-ref')


# --- PNG, indexed, stored deflate -------------------------------------------------------------
def adler32(b):
    a, s = 1, 0
    for x in b:
        a = (a + x) % 65521
        s = (s + a) % 65521
    return (s << 16) | a


def zlib_stored(raw):
    out = bytearray(b'\x78\x01')
    i = 0
    while True:
        n = min(65535, len(raw) - i)
        final = 1 if i + n >= len(raw) else 0
        out += bytes((final,)) + struct.pack('<HH', n, n ^ 0xFFFF) + raw[i:i + n]
        i += n
        if final:
            break
    out += struct.pack('>I', adler32(raw))
    return bytes(out)


def chunk(t, data):
    c = t + data
    return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)


def png_indexed(w, h, pix, pal):
    """pix: w*h palette indices; pal: 256 (r,g,b) 0..255."""
    raw = b''.join(b'\0' + bytes(pix[y * w:(y + 1) * w]) for y in range(h))
    plte = b''.join(bytes(c) for c in pal)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0))
            + chunk(b'PLTE', plte) + chunk(b'IDAT', zlib_stored(raw)) + chunk(b'IEND', b''))


# --- level text ---------------------------------------------------------------------------------
def hexline(b):
    return ''.join('%02x' % x for x in b)


def level_text(n, lev, ovl, tmap):
    """n: 1-9. lev: the 31,752-byte level block, ovl: the overlay block (bytes or None), tmap: 122 bytes."""
    L = ['# Ultima Underworld level %d (LEV.ARK blocks %d, %d, %d), written by uo-new' % (n, n - 1, n + 8, n + 17),
         '# Edit freely: the engine reads this file. Delete it to have it extracted again from game\\DATA\\LEV.ARK.']
    walls = struct.unpack_from('<48H', tmap, 0)
    floors = struct.unpack_from('<10H', tmap, 96)
    L.append('walls ' + ' '.join(str(v) for v in walls))
    L.append('floors ' + ' '.join(str(v) for v in floors))
    L.append('doors ' + ' '.join(str(v) for v in tmap[116:122]))
    L.append('# tile x y type height floor wall door nomagic unknown8 first_object  (y 0 = south; type 0 solid 1 open'
             ' 2-5 diagonal open SE SW NE NW 6-9 slope N S E W)')
    for i in range(4096):
        w0, w1 = struct.unpack_from('<HH', lev, 4 * i)
        L.append('tile %d %d %d %d %d %d %d %d %d %d' % (
            i % 64, i // 64, w0 & 15, (w0 >> 4) & 15, (w0 >> 10) & 15, w1 & 63, w0 >> 15, (w0 >> 14) & 1,
            (w0 >> 8) & 3, w1 >> 6))
    L.append('# object index item flags z heading y x quality next owner link [19 more bytes for 0-255, hex]')
    for i in range(1024):
        o = 0x4000 + (i * 27 if i < 256 else 256 * 27 + (i - 256) * 8)
        w0, w1, w2, w3 = struct.unpack_from('<4H', lev, o)
        s = 'object %d %d %d %d %d %d %d %d %d %d %d' % (
            i, w0 & 511, w0 >> 9, w1 & 127, (w1 >> 7) & 7, (w1 >> 10) & 7, w1 >> 13, w2 & 63, w2 >> 6, w3 & 63, w3 >> 6)
        if i < 256:
            s += ' ' + hexline(lev[o + 8:o + 27])
        L.append(s)
    L.append('# the rest of the level block (not decoded yet): offset, then bytes in hex')
    for o in range(0x7300, len(lev), 32):
        L.append('rest %d %s' % (o, hexline(lev[o:o + 32])))
    L.append('# LEV.ARK block %d (not decoded yet; 384 bytes when present)' % (n + 8))
    if ovl:
        for o in range(0, len(ovl), 32):
            L.append('overlay %d %s' % (o, hexline(ovl[o:o + 32])))
    return ('\r\n'.join(L) + '\r\n').encode('ascii')


def level_from_text(txt):
    """The inverse: (level block, overlay block or None, texture map) from the text."""
    lev = bytearray(31752)
    ovl = bytearray()
    tmap = bytearray(122)
    for line in txt.decode('ascii').split('\r\n'):
        f = line.split()
        if not f or f[0].startswith('#'):
            continue
        k = f[0]
        if k == 'walls':
            struct.pack_into('<48H', tmap, 0, *map(int, f[1:49]))
        elif k == 'floors':
            struct.pack_into('<10H', tmap, 96, *map(int, f[1:11]))
        elif k == 'doors':
            tmap[116:122] = bytes(map(int, f[1:7]))
        elif k == 'tile':
            x, y, ty, h, fl, wa, door, nm, u8, ob = map(int, f[1:11])
            w0 = ty | h << 4 | u8 << 8 | fl << 10 | nm << 14 | door << 15
            struct.pack_into('<HH', lev, 4 * (y * 64 + x), w0, wa | ob << 6)
        elif k == 'object':
            v = list(map(int, f[1:12]))
            i = v[0]
            o = 0x4000 + (i * 27 if i < 256 else 256 * 27 + (i - 256) * 8)
            struct.pack_into('<4H', lev, o, v[1] | v[2] << 9, v[3] | v[4] << 7 | v[5] << 10 | v[6] << 13,
                             v[7] | v[8] << 6, v[9] | v[10] << 6)
            if i < 256:
                lev[o + 8:o + 27] = bytes.fromhex(f[12])
        elif k == 'rest':
            o = int(f[1])
            b = bytes.fromhex(f[2])
            lev[o:o + len(b)] = b
        elif k == 'overlay':
            o = int(f[1])
            b = bytes.fromhex(f[2])
            ovl[o:o + len(b)] = b
    return bytes(lev), (bytes(ovl) if ovl else None), bytes(tmap)


# --- driver -------------------------------------------------------------------------------------
def all_files():
    """{relative path: bytes} of every loose file."""
    pal = assets.palettes(assets.rd('DATA/PALS.DAT'))[0]
    files = {}
    for tr, sub in (('W64.TR', 'walls'), ('F32.TR', 'floors')):
        size, texs = assets.textures(assets.rd('DATA/' + tr))
        for i, t in enumerate(texs):
            files['textures/%s/%03d.png' % (sub, i)] = png_indexed(size, size, t, pal)
    blocks = assets.ark(assets.rd('DATA/LEV.ARK'))
    for n in range(1, 10):
        files['levels/level%d.txt' % n] = level_text(n, blocks[n - 1], blocks[n + 8], blocks[n + 17])
    return files


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'write'
    if cmd == 'write':
        out = sys.argv[2] if len(sys.argv) > 2 else OUT
        files = all_files()
        for rel, b in files.items():
            p = os.path.join(out, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, 'wb') as f:
                f.write(b)
        print(len(files), 'files written to', out)
    elif cmd == 'roundtrip':
        blocks = assets.ark(assets.rd('DATA/LEV.ARK'))
        for n in range(1, 10):
            t = level_text(n, blocks[n - 1], blocks[n + 8], blocks[n + 17])
            lev, ovl, tmap = level_from_text(t)
            ok = lev == blocks[n - 1] and ovl == blocks[n + 8] and tmap == blocks[n + 17]
            print('level', n, 'identical' if ok else 'DIFFERENT', len(t), 'bytes of text')
            assert ok


if __name__ == '__main__':
    main()
