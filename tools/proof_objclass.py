"""proof_objclass.py: the object class data proved: OBJCLASS.C (ovr129: init_objects, get_class_data and its empty
neighbour), HACK.C (ovr116: the weapon, missile and armour tables), MISC.C (ovr125: containers, lights, food),
ANIMOBJ.C (ovr089: the animation classes) and the three empty class handlers OVR139.C, OVR144.C and OVR146.C, beside
the rules core. init_objects reads DATA\\OBJECTS.DAT and COMOBJ.DAT on both sides (Pair.files); every table it fills
must then be the same (CREATURE.C's critter table and TRIGGER.C's trigger table with them, which it also loads); then
get_class_data and each class's handler on every object of the nine real levels must give the same record.

    python tools/proof_objclass.py game build/coretest.exe
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair  # noqa: E402

ROUTINES = ['init_objects', 'get_class_data', 'Return0_ovr129_EE', 'hack_init', 'hack_class_data', 'misc_init',
            'misc_class_data', 'animobj_load', 'animobj_class_data', 'rect_class_data', 'spec_class_data',
            'stuff_class_data']
# the tables init_objects fills, with their DOS sizes
TABLES = [('Weapons', 16 * 8), ('Missile', 16 * 3), ('Armor', 32 * 4), ('Containers', 16 * 3), ('Lights', 16 * 2),
          ('Food', 16), ('animclassd', 16 * 4), ('ComObjData', 0x1600), ('Creature', 64 * 48), ('Triggers', 16)]


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    p = Pair(game, coretest)
    p.coverage()
    p.files()
    for name, n in TABLES:  # nothing loaded yet: both empty
        p.w(name, 0, b'\0' * n)
    r = p.call('init_objects', [], 'w')
    p.same('init_objects', r)
    if r[0][0] != 'ret 0':
        raise Differ('init_objects failed: %s' % (r,))
    for name, n in TABLES:
        p.same_bytes(name, 0, n, 'after init_objects')
    nobj = 0
    counts = {}
    for level in range(1, 10):
        block = p.level(level)
        for i in list(range(1, 256)) + list(range(256, 1024)):
            off = 0x4000 + i * 27 if i < 256 else 0x5b00 + (i - 256) * 8
            if not struct.unpack_from('<H', block, off)[0]:
                continue
            p.p('ActiveObj', 0, 'L+%x' % off)
            major = struct.unpack_from('<H', block, off)[0] >> 6 & 7
            calls = [('get_class_data', 'n')]
            calls.append(({0: 'hack_class_data', 2: 'misc_class_data', 7: 'animobj_class_data',
                           3: 'stuff_class_data', 4: 'spec_class_data', 5: 'rect_class_data'}.get(major, 'Return0_ovr129_EE'),
                          'n' if major in (0, 2, 7) else 'w'))
            for fn, ret in calls:
                res = p.call(fn, [], ret if fn != 'Return0_ovr129_EE' else 'b')
                p.same(fn, res, 'level %d object %d' % (level, i))
                counts[fn] = counts.get(fn, 0) + 1
            nobj += 1
    print('objclass        init_objects (%d tables the same) and %d objects on 9 levels  same (%s)' % (
        len(TABLES), nobj, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


if __name__ == '__main__':
    try:
        main()
    except Differ as ex:
        print('objclass DIFFER:', ex)
        sys.exit(1)
