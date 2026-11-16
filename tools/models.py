"""The 3D models inside UW.EXE (32 of them: 1 the door frame, 2 bridge, 3 bench, ...): their vertices and faces.

The model table and node format followed from UnderworldAdventures' ModelDecoder (GPL; used as a map only, this is
our own reader). Each model: a 4-byte header, extents (x, y, z as 8.8 fixed), then nodes (16-bit command words).
Coordinates are 8.8 fixed: 1.0 = one eighth of a tile? (see the note printed with the extents).

  python -I tools/models.py game/UW.EXE [model numbers...]
"""
import struct
import sys

TABLES = [(0x4e910, 0x40064ab6, 0x4e99e), (0x4ccd0, 0x40064ab6, 0x4cd5e), (0x4e370, 0x40064ab6, 0x4e3fe),
          (0x4ec70, 0x40064ab6, 0x4ecfe)]
NAMES = ["-", "door frame", "bridge", "bench", "car", "small boulder", "medium boulder", "large boulder", "arrow", "beam",
         "pillar", "shrine", "?", "painting", "?", "?", "tmap lever", "tmap switch", "tmap writing", "gravestone", "tmap 16e",
         "-", "tmap 16f", "moongate", "table", "chest", "nightstand", "barrel", "chair", "bed", "gem", "shelf"]


class Reader:
    def __init__(self, data, pos):
        self.d, self.p = data, pos

    def u16(self):
        v = struct.unpack_from('<H', self.d, self.p)[0]
        self.p += 2
        return v

    def u8(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def fx(self):
        v = struct.unpack_from('<h', self.d, self.p)[0]
        self.p += 2
        return v / 256.0

    def vn(self):
        return self.u16() >> 3


def parse(r, verts, faces, colour, depth=0, log=None):
    def put(n, v):
        while len(verts) <= n:
            verts.append(None)
        verts[n] = v

    while True:
        at = r.p
        cmd = r.u16()
        if log is not None:
            log.append('%s%05x %04x' % ('  ' * depth, at, cmd))
        if cmd == 0x0000:
            return
        elif cmd == 0x0078:  # origin
            r.vn(), r.fx(), r.fx(), r.fx(), r.u16()
        elif cmd == 0x004a:  # translate
            r.fx(), r.fx(), r.fx()
        elif cmd == 0x00ba:  # submodel
            c = r.u16()
            rel = r.u16()
            here = r.p
            r.p = here - ((-rel) & 0xffff)
            parse(r, verts, faces, c, depth + 1, log)
            r.p = here
        elif cmd == 0x007a:  # a vertex
            x, y, z = r.fx(), r.fx(), r.fx()
            put(r.vn(), (x, y, z))
        elif cmd == 0x0082:  # vertices
            n, first = r.u16(), r.u16()
            for i in range(n):
                put(first + i, (r.fx(), r.fx(), r.fx()))
        elif cmd in (0x0086, 0x0088, 0x008a):  # a vertex: another + an offset on x / z / y
            ref, d, n = r.vn(), r.fx(), r.vn()
            x, y, z = verts[ref]
            if cmd == 0x0086: x += d
            elif cmd == 0x0088: z += d
            else: y += d
            put(n, (x, y, z))
        elif cmd in (0x0090, 0x0092, 0x0094):  # two offsets
            a, b, ref, n = r.fx(), r.fx(), r.vn(), r.vn()
            x, y, z = verts[ref]
            if cmd == 0x0090: x, z = x + a, z + b
            elif cmd == 0x0092: x, y = x + a, y + b
            else: y, z = y + a, z + b
            put(n, (x, y, z))
        elif cmd == 0x008c:  # a vertex at the ceiling
            ref, unk, n = r.vn(), r.u16(), r.vn()
            x, y, z = verts[ref]
            put(n, (x, y, 'ceil'))
        elif cmd == 0x0058:
            r.u16(); [r.fx() for _ in range(6)]
        elif cmd in (0x0064, 0x0066, 0x0068):
            r.u16(), r.fx(), r.fx()
        elif cmd in (0x005e, 0x0060, 0x0062):
            r.u16(); [r.fx() for _ in range(4)]
        elif cmd == 0x007e:  # a face
            n = r.u16()
            faces.append(('flat', colour, [r.vn() for _ in range(n)]))
        elif cmd in (0x00a8, 0x00b4, 0x00ce):  # a textured face
            tex = r.u16() if cmd == 0x00a8 else 0
            n = r.u16()
            vs = []
            for _ in range(n):
                v = r.vn()
                u, w = r.u16() / 65535.0, r.u16() / 65535.0
                vs.append((v, u, w))
            faces.append(('tex', tex, vs))
        elif cmd == 0x00a0:  # a short face
            r.u16()
            faces.append(('short', colour, [r.u8() for _ in range(4)]))
        elif cmd in (0x0006, 0x000c, 0x000e, 0x0010):  # sort planes: both sides
            if cmd == 0x0006:
                r.fx(), r.fx()
            [r.fx() for _ in range(4)]
            left = r.u16(); left += r.p
            right = r.u16(); right += r.p
            here = r.p
            r.p = left
            parse(r, verts, faces, colour, depth + 1, log)
            r.p = right
            parse(r, verts, faces, colour, depth + 1, log)
            r.p = here
        elif cmd == 0x0014:
            r.vn(); colour = r.u16(); r.vn()
        elif cmd == 0x00bc:
            colour = r.u16(); r.u16()
        elif cmd == 0x00be:
            colour = r.u16(); r.u16()
        elif cmd == 0x00d4:
            n = r.u16(); colour = r.u16()
            for _ in range(n):
                r.vn(); r.u8()
            if n & 1:
                r.u8()
        elif cmd in (0x00d6, 0x0040):
            pass
        elif cmd == 0x0016:
            r.vn(); colour = r.u16(); r.u16()
        elif cmd == 0x0012:
            r.u16()
        else:
            if log is not None:
                log.append('  unknown command %04x: stop' % cmd)
            return


def models(data):
    for t, v, base in TABLES:
        if struct.unpack_from('<I', data, t)[0] == v:
            offs = struct.unpack_from('<32H', data, t)
            break
    else:
        raise SystemExit('no model table found')
    out = []
    for n in range(32):
        r = Reader(data, base + offs[n])
        head = struct.unpack_from('<I', data, r.p)[0]
        r.p += 4
        ext = (r.fx(), r.fx(), r.fx())
        verts, faces, log = [], [], []
        parse(r, verts, faces, 0, 0, log)
        out.append(dict(n=n, at=base + offs[n], head=head, ext=ext, verts=verts, faces=faces, log=log))
    return out


if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    want = [int(a, 0) for a in sys.argv[2:]] or list(range(32))
    for m in models(data):
        if m['n'] not in want:
            continue
        print('model %d %s at file 0x%x, header %08x, extents %s' % (m['n'], NAMES[m['n']], m['at'], m['head'], m['ext']))
        for i, v in enumerate(m['verts']):
            if v is not None:
                print('  v%-3d %s' % (i, v))
        for f in m['faces']:
            print('  face', f)
