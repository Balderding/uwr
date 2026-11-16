"""xmi.py: the game's music (.XMI, the Miles sound system's MIDI) -> standard MIDI files; the reference for the
engine's converter (src\\audio.c).

    python -I xmi.py info NAME.XMI         the programs (instruments) each channel selects, the length
    python -I xmi.py convert NAME.XMI OUT.mid

XMI: IFF. FORM XDIR (INFO: sequence count), CAT XMID, FORM XMID: TIMB, EVNT. EVNT: a delay is the sum of
the bytes below 0x80 before an event; events are MIDI without running status; a note-on (0x9n) is followed
by its length (a variable-length number) and has no note-off. 120 ticks a second. Out: a format 0 file,
60 ticks a quarter note at 500,000 us a quarter (= 120 ticks a second), the note-offs put back.
"""
import struct
import sys


def find_evnt(d):
    i = d.find(b'EVNT')
    if i < 0:
        return None
    n = struct.unpack_from('>I', d, i + 4)[0]
    return d[i + 8:i + 8 + n]


def vlq(d, i):
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i


def events(ev):
    """[(time, bytes)] with note-offs added, sorted by time (stable)"""
    out, i, t = [], 0, 0
    while i < len(ev):
        b = ev[i]
        if b < 0x80:
            t += b
            i += 1
            continue
        st = b
        i += 1
        if st == 0xFF:
            typ = ev[i]
            ln, j = vlq(ev, i + 1)
            data = ev[j:j + ln]
            i = j + ln
            if typ == 0x2F:
                break
            if typ == 0x51:
                continue  # tempo: XMI timing is fixed (120 a second)
            out.append((t, bytes([0xFF, typ]) + encode_vlq(ln) + data))
            continue
        if st in (0xF0, 0xF7):
            ln, j = vlq(ev, i)
            out.append((t, bytes([st]) + encode_vlq(ln) + ev[j:j + ln]))
            i = j + ln
            continue
        kind = st & 0xF0
        nd = 1 if kind in (0xC0, 0xD0) else 2
        data = ev[i:i + nd]
        i += nd
        out.append((t, bytes([st]) + data))
        if kind == 0x90:
            dur, i = vlq(ev, i)
            out.append((t + dur, bytes([0x80 | (st & 15), data[0], 0])))
    out.sort(key=lambda e: e[0])
    return out


def encode_vlq(v):
    b = [v & 0x7F]
    v >>= 7
    while v:
        b.append((v & 0x7F) | 0x80)
        v >>= 7
    return bytes(reversed(b))


def smf(evs):
    trk = bytearray(b'\x00\xff\x51\x03\x07\xa1\x20')  # 500,000 us a quarter
    last = 0
    for t, e in evs:
        trk += encode_vlq(t - last) + e
        last = t
    trk += b'\x00\xff\x2f\x00'
    return b'MThd' + struct.pack('>IHHH', 6, 0, 1, 60) + b'MTrk' + struct.pack('>I', len(trk)) + bytes(trk)


if __name__ == '__main__':
    d = open(sys.argv[2], 'rb').read()
    evs = events(find_evnt(d))
    if sys.argv[1] == 'info':
        progs = {}
        for t, e in evs:
            if e[0] & 0xF0 == 0xC0:
                progs.setdefault(e[0] & 15, set()).add(e[1])
        print(sys.argv[2], '%.1f s' % (evs[-1][0] / 120.0 if evs else 0), {c: sorted(p) for c, p in sorted(progs.items())})
    else:
        open(sys.argv[3], 'wb').write(smf(evs))
