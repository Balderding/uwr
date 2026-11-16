"""proof_trigsave.py: TRIGSAVE.C (trap_save: the trigger table written to a save file) proved beside the rules core,
with TRIGGER.C's trap_init, its twin (reads the 16 bytes back), which runs with it. A file of the game's folder
(OBJECTS.DAT, read with trap_init at random places) and a file the two sides write (trap_save, read back with fseek
and fread). Random trigger tables; the file bytes and the table must be the same afterwards.

    python tools/proof_trigsave.py game build/coretest.exe [count]
"""
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair  # noqa: E402
from magic_common import raw_call, fopen_both  # noqa: E402

ROUTINES = ['trap_save', 'trap_init']


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 60
    p = Pair(game, coretest)
    p.coverage()
    p.files()
    rng = random.Random(149)
    counts = {'trap_save': 0, 'trap_init': 0}
    size = os.path.getsize(os.path.join(game, 'DATA', 'OBJECTS.DAT'))
    for k in range(count):
        ctx = 'case %d' % k
        # trap_save: a random table, written to a file the two sides make, read back
        tab = bytes(rng.getrandbits(8) for _ in range(16))
        p.w('Triggers', 0, tab)
        fd, fh = fopen_both(p, 'TRIGTEST.TMP', 'w+b')
        r = raw_call(p, 'trap_save', [fd], [fh])
        (ax, dx), _ = raw_call(p, 'fseek', [fd, 0, 0, 0], [fh, '0', '0'], 'bc_fseek')
        p.w('S', 0x40, bytes(16))
        raw_call(p, 'fread', [0xE040, 1, 16, fd], ['S+40', '1', '16', fh], 'bc_fread')
        p.same_bytes('S', 0x40, 16, ctx)
        a, b = p.r('S', 0x40, 16)
        if bytes.fromhex(a) != tab:
            raise Differ('trap_save wrote %s, not the table %s (%s)' % (a, tab.hex(), ctx))
        p.same_bytes('Triggers', 0, 16, ctx)
        counts['trap_save'] += 1
        # trap_init: 16 bytes read from OBJECTS.DAT at a random place
        p.w('Triggers', 0, bytes(16))
        fd, fh = fopen_both(p, 'DATA\OBJECTS.DAT', 'rb')
        at = rng.choice([0, rng.randint(0, size - 16), size - 16, size - 10])
        raw_call(p, 'fseek', [fd, at & 0xffff, at >> 16, 0], [fh, str(at), '0'], 'bc_fseek')
        raw_call(p, 'trap_init', [fd], [fh])
        p.same_bytes('Triggers', 0, 16, ctx)
        counts['trap_init'] += 1
    print('trigsave        %d operations on 9 levels  same (%s)' % (sum(counts.values()), ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


if __name__ == '__main__':
    try:
        main()
    except Differ as ex:
        print('trigsave DIFFER:', ex)
        sys.exit(1)
