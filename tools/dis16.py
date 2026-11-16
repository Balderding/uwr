"""dis16.py: disassemble UW.EXE (16-bit real mode) at a file offset.

    uv run --no-project --with capstone python -I tools/dis16.py game/UW.EXE <file offset hex> [count] [back]

Prints file offsets. `back` starts that many bytes earlier (to see what leads up to a reference).
"""
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, Cs


def main():
    img = open(sys.argv[1], 'rb').read()
    off = int(sys.argv[2], 16)
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 60
    back = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    start = off - back
    for n, ins in enumerate(md.disasm(img[start:start + count * 8], start)):
        mark = '>>' if ins.address <= off < ins.address + ins.size else '  '
        print('%s%06x  %-16s %s %s' % (mark, ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
        if n + 1 >= count:
            break


if __name__ == '__main__':
    main()
