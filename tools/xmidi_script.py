"""xmidi_script.py: driver call scripts (see tests/xmidi_test.c's script mode) on UW's own MT32MPU.ADV in the emulator
and on our port; the records (every byte to the MPU-401, every wait, every returned value) must be identical.

    uv run --no-project --with unicorn==2.0.1.post1 python -I tools/xmidi_script.py tests/xmidi_scripts/*.txt
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from adv import Driver, gtl_index  # noqa: E402


def original(path, gtl):
    d = Driver(os.path.join(ROOT, 'game', 'SOUND', 'MT32MPU.ADV'))
    d.call(102, 0x330, 2, 1, 1)
    size, _ = d.call(150)
    handles = []
    rec = d.out
    for line in open(path):
        p = line.split()
        if not p or p[0].startswith('#'):
            continue
        c, a = p[0], [int(x, 0) for x in p[1:] if not x.endswith('.XMI') and not x.endswith('.xmi')]
        h = lambda: handles[a[0]] if a and 0 <= a[0] < len(handles) else 0xffff  # noqa: E731
        r = None
        if c == 'at':
            while d.tick < a[0]:
                d.serve()
        elif c == 'reg':
            data = open(p[1], 'rb').read()
            state = d.alloc(size)
            xseg = d.alloc(data)
            r, _ = d.call(151, (xseg, 0), int(p[2]), (state, 0), (0, 0))
            handles.append(r)
        elif c == 'start':
            d.call(170, h())
        elif c == 'stop':
            d.call(171, h())
        elif c == 'resume':
            d.call(173, h())
        elif c == 'release':
            d.call(152, h())
        elif c == 'vol':
            d.call(177, h(), a[1], a[2])
        elif c == 'tempo':
            d.call(178, h(), a[1], a[2])
        elif c == 'lock':
            r, _ = d.call(191)
        elif c == 'relch':
            d.call(193, a[0])
        elif c == 'inst':
            t = gtl.get((a[1], a[0]))
            tseg = d.alloc(t if t else b'\0\0')
            d.call(156, a[0], a[1], (tseg, 0) if t else (0, 0))
        elif c == 'cv':
            d.call(186, a[0], a[1], a[2])
        elif c == 'branch':
            d.call(181, h(), a[1])
        elif c == 'status':
            r, _ = d.call(174, h())
        elif c == 'notes':
            r, _ = d.call(185, h(), a[1])
        elif c == 'tstat':
            r, _ = d.call(159, a[0], a[1])
        elif c == 'beat':
            r, _ = d.call(179, h())
        elif c == 'bar':
            r, _ = d.call(180, h())
        elif c == 'ctl':
            r, _ = d.call(182, h(), a[1], a[2])
        elif c == 'setctl':
            d.call(183, h(), a[1], a[2], a[3])
        elif c == 'protect':
            d.call(157, a[0], a[1])
        elif c == 'unprotect':
            d.call(158, a[0], a[1])
        elif c == 'display':
            d.call(188, (d.alloc(b'Ultima Underworld\0'), 0))
        elif c == 'sysex':
            d.call(187, a[0], a[1], a[2], (d.alloc(bytes([1, 2, 3, 4])), 0), 4, 2)
        elif c == 'shutdown':
            d.call(104, (d.alloc(b'Goodbye\0'), 0))
        if r is not None:
            rec.append((d.tick, 'ret', r & 0xffff))
    return ['%d vr' % t if p == 'vr' else ('%d ret %d' % (t, v) if p == 'ret' else '%d %x %02x' % (t, p, v)) for t, p, v in rec]


def ours(path):
    out = os.path.join(ROOT, 'work', 'xmidi_script_ours.txt')
    subprocess.run([os.path.join(ROOT, 'build', 'xmidi_test.exe'), os.path.join(ROOT, 'game', 'SOUND', 'UW.MT'), '--script', path, out])
    return open(out).read().split('\n')[:-1]


def main():
    gtl = gtl_index(os.path.join(ROOT, 'game', 'SOUND', 'UW.MT'))
    bad = 0
    paths = []
    for p in sys.argv[1:]:  # files, or folders of *.txt scripts
        paths += sorted(os.path.join(p, f) for f in os.listdir(p) if f.endswith('.txt')) if os.path.isdir(p) else [p]
    for path in paths:
        a, b = original(path, gtl), ours(path)
        name = os.path.basename(path)
        if a == b:
            print('%-24s same (%d entries)' % (name, len(a)))
            continue
        bad += 1
        n = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
        print('%-24s DIFFERS at entry %d of %d/%d: original %s, ours %s' % (name, n, len(a), len(b), a[max(0, n - 2):n + 4], b[max(0, n - 2):n + 4]))
    print('all the same' if not bad else '%d differ' % bad)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
