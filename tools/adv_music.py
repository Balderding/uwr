"""adv_music.py: one XMI song through UW's own MT32MPU.ADV in the emulator, as the game plays it (register the sequence,
install every timbre it asks for from UW.MT, start, serve the driver at its service rate); writes what the driver sent
to the MPU-401 (the MIDI bytes, with the tick each came on) -- the reference for our port.

    uv run --no-project --with unicorn==2.0.1.post1 python -I tools/adv_music.py game/SOUND/MT32MPU.ADV game/SOUND/UW.MT game/SOUND/UW02.XMI 0 600 out.txt
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from adv import Driver, gtl_index  # noqa: E402


def main():
    adv, gtl, xmi, seqnum, ticks, outp = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), sys.argv[6]
    d = Driver(adv)
    ax, dx = d.call(100)  # describe: far pointer to the drvr_desc
    desc = d.read(dx, ax, 26)
    min_api, dtype = struct.unpack('<HH', desc[0:4])
    rate, = struct.unpack('<h', desc[20:22])
    print('driver type %d, service rate %d / s' % (dtype, rate))
    d.call(102, 0x330, 2, 1, 1)  # init
    size, _ = d.call(150)
    state = d.alloc(size)
    cache_size, _ = d.call(153)
    if cache_size:
        cache = d.alloc(cache_size)
        d.call(154, (cache, 0), cache_size)
    data = open(xmi, 'rb').read()
    xseg = d.alloc(data)
    seq, _ = d.call(151, (xseg, 0), seqnum, (state, 0), (0, 0))
    print('sequence handle', seq, 'state table', size, 'timbre cache', cache_size)
    timbres = gtl_index(gtl)
    while True:
        req, _ = d.call(155, seq)
        if req == 0xffff:
            break
        bank, patch = req >> 8, req & 0xff
        t = timbres.get((patch, bank))
        print('timbre request bank %d patch %d: %s' % (bank, patch, 'from the library' if t else 'NOT IN THE LIBRARY'))
        tseg = d.alloc(t if t else b'\0\0')
        d.call(156, bank, patch, (tseg, 0) if t else (0, 0))
    d.call(170, seq)  # start
    d.serve(ticks)
    with open(outp, 'w') as f:
        for tick, port, v in d.out:
            f.write(('%d vr\n' % tick) if port == 'vr' else ('%d %x %02x\n' % (tick, port, v)))
    print('%d port writes over %d ticks' % (len(d.out), ticks))


if __name__ == '__main__':
    main()
