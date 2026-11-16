"""scan16.py: linear 16-bit disassembly of UW.EXE (load image + overlays), printing windows where all the given
instruction fragments occur within N instructions.

    uv run --no-project --with capstone python -I tools/scan16.py game/UW.EXE WINDOW "frag1" "frag2" ...

A fragment matches the text "mnemonic op_str" of an instruction (substring). Fragments may be alternatives
separated by '|'.
"""
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, Cs


def main():
    img = open(sys.argv[1], 'rb').read()
    window = int(sys.argv[2])
    frags = [f.split('|') for f in sys.argv[3:]]
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.skipdata = True
    ins = [(i.address, '%s %s' % (i.mnemonic, i.op_str)) for i in md.disasm(img[0x3200:], 0x3200)]
    hits = 0
    last = -1
    for k in range(len(ins)):
        if not any(a in ins[k][1] for a in frags[0]):
            continue
        seg = ins[max(0, k - window):k + window]
        if all(any(any(a in t for a in f) for _, t in seg) for f in frags[1:]):
            if ins[k][0] - last < 64:
                continue
            last = ins[k][0]
            hits += 1
            print('== %06x' % ins[k][0])
            for a, t in seg:
                print('   %06x %s' % (a, t))
    print(hits, 'hits')


if __name__ == '__main__':
    main()
