"""strings_pak.py: STRINGS.PAK, the game's text (Huffman-coded), the reference for the engine.

    python -I strings_pak.py dump [block]     prints the blocks (or one) to the console

Layout: u16 node count, nodes of 4 bytes (char, parent, left, right); u16 block count, then per block
u16 block number + u32 offset. A block: u16 string count, u16 offsets (from after that table) of each
string; a string is a bit stream (most significant bit first) walked from the root (the last node): 1 =
right, 0 = left, until a node whose left is 0xFF (a leaf: its char); '|' ends the string.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import assets  # noqa: E402


def blocks(d=None):
    d = d or assets.rd('DATA/STRINGS.PAK')
    nn = struct.unpack_from('<H', d, 0)[0]
    nodes = [d[2 + 4 * i:6 + 4 * i] for i in range(nn)]
    at = 2 + 4 * nn
    nb = struct.unpack_from('<H', d, at)[0]
    at += 2
    heads = [struct.unpack_from('<HI', d, at + 6 * i) for i in range(nb)]
    out = {}
    for num, off in heads:
        ns = struct.unpack_from('<H', d, off)[0]
        soffs = struct.unpack_from('<%dH' % ns, d, off + 2)
        base = off + 2 + 2 * ns
        strs = []
        for so in soffs:
            p, bit, s = base + so, 7, []
            while p < len(d) and len(s) < 4096:
                node = nn - 1
                while nodes[node][2] != 0xFF:
                    b = (d[p] >> bit) & 1
                    bit -= 1
                    if bit < 0:
                        bit, p = 7, p + 1
                    node = nodes[node][3] if b else nodes[node][2]
                ch = nodes[node][0]
                if ch == ord('|'):
                    break
                s.append(ch)
            strs.append(bytes(s).decode('latin-1'))
        out[num] = strs
    return out


if __name__ == '__main__':
    b = blocks()
    want = int(sys.argv[2]) if len(sys.argv) > 2 else None
    for num in sorted(b):
        if want is not None and num != want:
            continue
        print('block %d: %d strings' % (num, len(b[num])))
        for i, s in enumerate(b[num][:600 if want is not None else 6]):
            print('  %d: %s' % (i, s.replace('\n', '\\n')))
