"""xmidi_verify.py: every song through UW's own MT32MPU.ADV (in the emulator, tools/adv.py) and through our port
(build/xmidi_test.exe); the records of bytes sent to the MPU-401 must be identical.

    uv run --no-project --with unicorn==2.0.1.post1 python -I tools/xmidi_verify.py [ticks] [song...]
"""
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from adv import Driver, gtl_index  # noqa: E402


def original(xmi, seqnum, ticks, gtl):
    d = Driver(os.path.join(ROOT, 'game', 'SOUND', 'MT32MPU.ADV'))
    d.call(102, 0x330, 2, 1, 1)
    size, _ = d.call(150)
    state = d.alloc(size)
    data = open(xmi, 'rb').read()
    xseg = d.alloc(data)
    seq, _ = d.call(151, (xseg, 0), seqnum, (state, 0), (0, 0))
    if seq == 0xffff:
        return None
    while True:
        req, _ = d.call(155, seq)
        if req == 0xffff:
            break
        t = gtl.get((req & 0xff, req >> 8))
        tseg = d.alloc(t if t else b'\0\0')
        d.call(156, req >> 8, req & 0xff, (tseg, 0) if t else (0, 0))
    d.call(170, seq)
    d.serve(ticks)
    return ['%d vr' % t if p == 'vr' else '%d %x %02x' % (t, p, v) for t, p, v in d.out]


def ours(xmi, seqnum, ticks):
    out = os.path.join(ROOT, 'work', 'xmidi_ours.txt')
    r = subprocess.run([os.path.join(ROOT, 'build', 'xmidi_test.exe'), os.path.join(ROOT, 'game', 'SOUND', 'UW.MT'), xmi,
                        str(seqnum), str(ticks), out], capture_output=True, text=True)
    if r.returncode:
        return None
    return open(out).read().split('\n')[:-1]


def main():
    ticks = int(sys.argv[1]) if len(sys.argv) > 1 else 3000
    songs = sys.argv[2:] or sorted(glob.glob(os.path.join(ROOT, 'game', 'SOUND', '*.XMI')))
    gtl = gtl_index(os.path.join(ROOT, 'game', 'SOUND', 'UW.MT'))
    bad = 0
    for xmi in songs:
        for seqnum in range(8):
            a = original(xmi, seqnum, ticks, gtl)
            if a is None:
                break
            b = ours(xmi, seqnum, ticks)
            name = '%s #%d' % (os.path.basename(xmi), seqnum)
            if a == b:
                print('%-14s same (%d bytes and waits over %d ticks)' % (name, len(a), ticks))
                continue
            bad += 1
            n = next((i for i in range(min(len(a), len(b or []))) if a[i] != b[i]), min(len(a), len(b or [])))
            print('%-14s DIFFERS at entry %d: original %s, ours %s' % (name, n, a[n:n + 3], (b or [])[n:n + 3]))
    print('all the same' if not bad else '%d differ' % bad)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
