"""ovl.py: where a far call through an overlay selector lands in UW.EXE (file offset).

    python -I tools/ovl.py game/UW.EXE SEL OFF      e.g. 478 61

The segment table (161 entries of seg, size, type, ?) sits at file 0x5c1c0; a selector is its index x 8. A type-3
entry is an overlay stub: CD 3F, +4 dword = the overlay's code offset from 0x65710, entries of 5 bytes from +0x20
(CD 3F + word = the routine's offset in the overlay). Any other entry is a plain segment (load image at 0x3200).
"""
import struct
import sys


def main():
    d = open(sys.argv[1], 'rb').read()
    sel, off = int(sys.argv[2], 16), int(sys.argv[3], 16)
    fbov = d.index(b'FBOV')
    tab, n = struct.unpack('<IH', d[fbov + 8:fbov + 14])
    seg, size, typ, _ = struct.unpack('<4H', d[tab + (sel // 8) * 8:tab + (sel // 8) * 8 + 8])
    print('selector %x -> table entry %d: seg %04x size %x type %d' % (sel, sel // 8, seg, size, typ))
    stub = 0x3200 + seg * 16
    if typ == 3 and d[stub:stub + 2] == b'\xcd\x3f':
        code = 0x65710 + struct.unpack('<I', d[stub + 4:stub + 8])[0]
        e = stub + off
        target = struct.unpack('<H', d[e + 2:e + 4])[0]
        print('overlay stub at file %x, code at %x; entry +%x -> routine +%x = file %x' % (stub, code, off, target, code + target))
    else:
        print('plain segment: file %x' % (stub + off))


main()
