"""callsites.py: show the instructions before far calls to SEG:OFF across UW.EXE (load image and overlays).

    uv run --no-project --with capstone python -I tools/callsites.py game/UW.EXE SEG OFF [count] [before]
"""
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

IMG, OVL_BASE = 0x3200, 0x65710


def main():
    d = open(sys.argv[1], 'rb').read()
    want = (int(sys.argv[2], 16), int(sys.argv[3], 16))
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 8
    before = int(sys.argv[5]) if len(sys.argv) > 5 else 5
    fbov = d.index(b'FBOV')
    tab, n = struct.unpack('<IH', d[fbov + 8:fbov + 14])
    segs = [struct.unpack('<4H', d[tab + k * 8:tab + k * 8 + 8]) for k in range(n)]
    regions = []
    for seg, size, typ, _ in segs:
        if typ in (0, 1) and seg < 0x5AAC and size:
            regions.append((IMG + seg * 16, IMG + seg * 16 + size, False))
        if typ == 3:
            a = IMG + seg * 16
            fo, = struct.unpack('<I', d[a + 4:a + 8])
            cs, = struct.unpack('<H', d[a + 8:a + 10])
            regions.append((OVL_BASE + fo, OVL_BASE + fo + cs, True))
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.skipdata = True
    shown = 0
    for start, end, ovl in sorted(regions):
        insns = list(md.disasm(d[start:end], start))
        for i, ins in enumerate(insns):
            if ins.mnemonic != 'lcall' or ins.size != 5:
                continue
            o, s = struct.unpack('<HH', ins.bytes[1:5])
            if ovl and s % 8 == 0 and s // 8 < n:
                s = segs[s // 8][0]
            if (s, o) != want:
                continue
            print('== %06x' % ins.address)
            for j in insns[max(0, i - before):i + 2]:
                print('   %06x %s %s' % (j.address, j.mnemonic, j.op_str))
            shown += 1
            if shown >= count:
                return


if __name__ == '__main__':
    main()
