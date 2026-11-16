r"""creature_calls_mt32.py (2026-11-16): trial call sounds for the creatures, as our own Roland timbres, played through
the game's own Munt (dist\UW1\mt32emu.dll with the user's CM-32L ROMs in dist\UW1\roms) into WAV files to listen to.
Nothing of the game is changed; this only reads the DLL and the ROMs. Kept as a tool at the user's word; the sounds it
made were judged "just instruments going off" and are not in the game (see creature_calls_synth.py for the other try).

    python -I tools\creature_calls_mt32.py   -> work\creature-calls\wav\<creature>-<n>.wav, all-creature-calls.wav, index.txt

The unit is set up as the game sets it up (uwaudio.c mt_start: channel map, partial reserve, reverb 0/3/2, master
volume 90). Each sound is one timbre (two synthesiser partials, mixed or ring-modulated) written to timbre memory slot
0 and played on part 1 as one or more notes. Three variants a creature: 1 as designed, 2 higher and shorter, 3 lower
and longer. The partial's 58 bytes are the MT-32's partial parameters in their order (checked against the game's own
effect timbres in SOUND\UW.MT).
"""
import ctypes, os, sys, wave, array, math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, 'dist', 'UW1')
BASE = os.path.join(ROOT, 'work', 'creature-calls')
OUT = os.path.join(BASE, 'wav')

L = None
CTX = None
RATE = 32000

def load():
    global L
    L = ctypes.CDLL(os.path.join(DIST, 'mt32emu.dll'))
    L.mt32emu_create_context.restype = ctypes.c_void_p
    L.mt32emu_create_context.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    for f, a, r in (('mt32emu_add_rom_file', [ctypes.c_void_p, ctypes.c_char_p], ctypes.c_int),
                    ('mt32emu_open_synth', [ctypes.c_void_p], ctypes.c_int),
                    ('mt32emu_close_synth', [ctypes.c_void_p], None),
                    ('mt32emu_free_context', [ctypes.c_void_p], None),
                    ('mt32emu_play_msg_now', [ctypes.c_void_p, ctypes.c_uint], ctypes.c_int),
                    ('mt32emu_play_sysex_now', [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint], ctypes.c_int),
                    ('mt32emu_render_bit16s', [ctypes.c_void_p, ctypes.POINTER(ctypes.c_short), ctypes.c_uint], None),
                    ('mt32emu_get_actual_stereo_output_samplerate', [ctypes.c_void_p], ctypes.c_uint)):
        getattr(L, f).argtypes = a
        getattr(L, f).restype = r

def sysex(a, b, c, data):
    m = [0xF0, 0x41, 0x10, 0x16, 0x12, a, b, c] + list(data)
    s = (a + b + c + sum(data)) & 0x7F
    m += [(0x80 - s) & 0x7F, 0xF7]
    L.mt32emu_play_sysex_now(CTX, bytes(m), len(m))

def msg(status, d1, d2):
    L.mt32emu_play_msg_now(CTX, status | d1 << 8 | d2 << 16)

def start():
    global CTX, RATE
    load()
    CTX = L.mt32emu_create_context(None, None)
    r1 = L.mt32emu_add_rom_file(CTX, os.path.join(DIST, 'roms', 'cm32l_ctrl_1_02.rom').encode())
    r2 = L.mt32emu_add_rom_file(CTX, os.path.join(DIST, 'roms', 'cm32l_pcm.rom').encode())
    if r1 <= 0 or r2 <= 0: sys.exit('the ROMs were not taken (%d, %d)' % (r1, r2))
    if L.mt32emu_open_synth(CTX) != 0: sys.exit('Munt did not open')
    RATE = L.mt32emu_get_actual_stereo_output_samplerate(CTX)
    sysex(0x7F, 0, 0, [1])                                  # reset
    sysex(0x10, 0, 0x0D, [1, 2, 3, 4, 5, 6, 7, 8, 9])       # the channel map
    sysex(0x10, 0, 0x04, [3, 4, 3, 4, 3, 4, 3, 4, 4])       # the partial reserve
    sysex(0x10, 0, 0x01, [0, 3, 2])                         # the reverb
    sysex(0x10, 0, 0x16, [90])                              # the master volume
    render(RATE // 4)

def render(frames):
    buf = (ctypes.c_short * (frames * 2))()
    L.mt32emu_render_bit16s(CTX, buf, frames)
    return [(buf[2 * i] + buf[2 * i + 1]) // 2 for i in range(frames)]

SQU, SAW = 0, 1

def partial(wave=SAW, coarse=36, fine=50, pw=0, pdepth=0, ptimes=(0, 0, 0, 0), plevels=(50, 50, 50, 50, 50),
            lfo=(0, 0), cutoff=100, reso=0, fdepth=0, ftimes=(0, 0, 0, 0, 0), flevels=(100, 100, 100), fsus=100,
            level=100, atimes=(0, 20, 40, 60, 40), alevels=(100, 90, 80), asus=70, keyfollow=11):
    """one partial's 58 bytes: pitch (coarse, fine, key follow, bender, waveform, PCM wave, pulse width, its velocity
    sense), the pitch envelope (depth, velocity, key follow, 4 times, 5 levels), the LFO (rate, depth, modulation),
    the filter (cutoff, resonance, key follow, bias point and level, envelope depth, velocity, two key follows, 5
    times, 3 levels, sustain), the amplifier (level, velocity, two bias points and levels, two time follows, 5 times,
    3 levels, sustain)"""
    c = lambda v, lo, hi: max(lo, min(hi, int(round(v))))
    p = [c(coarse, 0, 96), c(fine, 0, 100), keyfollow, 0, wave, 0, c(pw, 0, 100), 7,
         c(pdepth, 0, 10), 0, 0] + [c(t, 0, 100) for t in ptimes] + [c(l, 0, 100) for l in plevels] + \
        [c(lfo[0], 0, 100), c(lfo[1], 0, 100), 0,
         c(cutoff, 0, 100), c(reso, 0, 30), 11, 0, 7, c(fdepth, 0, 100), 0, 0, 0] + [c(t, 0, 100) for t in ftimes] + \
        [c(l, 0, 100) for l in flevels] + [c(fsus, 0, 100),
         c(level, 0, 100), 70, 0, 12, 0, 12, 0, 0] + [c(t, 0, 100) for t in atimes] + [c(l, 0, 100) for l in alevels] + [c(asus, 0, 100)]
    assert len(p) == 58, len(p)
    return p

def install(name, structure, p1, p2):
    common = list(name.ljust(10)[:10].encode('ascii')) + [structure, 0, 3, 0]   # partials 1 and 2 sound; a sustaining envelope
    silent = partial(level=0)
    sysex(8, 0, 0x00, common)
    sysex(8, 0, 0x0E, p1)
    sysex(8, 0, 0x48, p2)
    sysex(8, 1, 0x02, silent)
    sysex(8, 1, 0x3C, silent)
    sysex(5, 0, 0, [2, 0])                                  # patch 0: timbre group 2 (memory), number 0
    msg(0xC1, 0, 0)
    msg(0xB1, 0x79, 0); msg(0xB1, 7, 0x7F); msg(0xB1, 0x0B, 0x7F); msg(0xB1, 0x0A, 0x40)

def play(notes, vel=110):
    """notes: (start seconds, note, length seconds); then the release until quiet (3 s at most)"""
    out, t = [], 0
    events = sorted([(int(s * RATE), 1, n) for s, n, d in notes] + [(int((s + d) * RATE), 0, n) for s, n, d in notes])
    for at, on, n in events:
        if at > t: out += render(at - t); t = at
        msg(0x91 if on else 0x81, max(0, min(127, n)), vel if on else 0)
    quiet = 0
    while len(out) - t < RATE * 3:
        blk = render(1024); out += blk
        quiet = quiet + 1 if max(abs(v) for v in blk) < 12 else 0
        if quiet >= 4: break
    msg(0xB1, 0x7B, 0)
    render(RATE // 8)
    while len(out) > RATE // 20 and max(abs(v) for v in out[-256:]) < 12: del out[-256:]
    return out

def write_wav(path, samples):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(array.array('h', samples).tobytes())

def loud(samples, peak=24000):
    m = max(1, max(abs(v) for v in samples))
    return [max(-32767, min(32767, v * peak // m)) for v in samples]

# ---- the sounds. Each: a function of (shift in semitones, length factor) giving (structure, partial 1, partial 2,
# notes). Structure 0: the two partials mixed; 1: partial 1 and its ring modulation with partial 2 (harsh, inharmonic).
MIX, RING = 0, 1
def N(*triples): return list(triples)

def worm(s, k):      # a wet, low gurgle
    a = partial(SQU, 36, pw=70, lfo=(78, 55), cutoff=38, reso=22, fdepth=40, ftimes=(5, 20, 30, 30, 30), flevels=(100, 40, 80), fsus=30,
                atimes=(8, 20, 30, 40, 45), alevels=(100, 70, 90), asus=50)
    b = partial(SAW, 31, fine=62, lfo=(64, 70), cutoff=30, reso=18, atimes=(10, 25, 30, 40, 45), alevels=(80, 100, 60), asus=40)
    return MIX, a, b, N((0, 38 + s, 0.55 * k))
def slug(s, k):      # a slow, falling bloop
    a = partial(SQU, 36, pw=40, pdepth=5, ptimes=(0, 55, 40, 40), plevels=(62, 50, 34, 30, 24), cutoff=34, reso=24,
                atimes=(15, 30, 40, 50, 55), alevels=(100, 85, 60), asus=30)
    b = partial(SQU, 24, pw=60, pdepth=4, ptimes=(0, 60, 40, 40), plevels=(58, 50, 38, 32, 28), lfo=(40, 30), cutoff=28, reso=14,
                atimes=(20, 30, 40, 50, 55), alevels=(90, 80, 50), asus=30)
    return MIX, a, b, N((0, 34 + s, 0.7 * k))
def bat(s, k):       # very high squeaks, three of them
    a = partial(SAW, 60, pdepth=6, ptimes=(0, 8, 10, 10), plevels=(40, 62, 50, 44, 40), cutoff=100,
                atimes=(0, 6, 10, 12, 10), alevels=(100, 70, 30), asus=0)
    b = partial(SQU, 67, fine=58, pw=30, pdepth=6, ptimes=(0, 8, 10, 10), plevels=(42, 64, 50, 44, 40),
                atimes=(0, 6, 10, 12, 10), alevels=(90, 60, 20), asus=0, level=70)
    return RING, a, b, N((0, 96 + s, 0.05 * k), (0.11 * k, 99 + s, 0.05 * k), (0.21 * k, 94 + s, 0.07 * k))
def lurker(s, k):    # a deep bubbling roar from the water
    a = partial(SAW, 24, lfo=(86, 60), cutoff=44, reso=20, fdepth=50, ftimes=(10, 30, 40, 40, 40), flevels=(100, 60, 90), fsus=40,
                atimes=(12, 30, 40, 50, 55), alevels=(100, 85, 95), asus=70)
    b = partial(SQU, 31, fine=44, pw=55, lfo=(92, 75), cutoff=50, atimes=(12, 30, 40, 50, 55), alevels=(100, 90, 90), asus=70)
    return RING, a, b, N((0, 33 + s, 0.9 * k))
def rat(s, k):       # a short rising squeal
    a = partial(SAW, 48, pdepth=6, ptimes=(0, 22, 20, 20), plevels=(38, 60, 66, 58, 50), cutoff=92, reso=10,
                atimes=(0, 10, 20, 25, 25), alevels=(100, 90, 70), asus=30)
    b = partial(SQU, 55, fine=57, pw=25, pdepth=6, ptimes=(0, 22, 20, 20), plevels=(38, 61, 67, 58, 50), level=60,
                atimes=(0, 10, 20, 25, 25), alevels=(100, 80, 60), asus=20)
    return MIX, a, b, N((0, 86 + s, 0.22 * k))
def spider(s, k):    # a chittering hiss
    a = partial(SAW, 72, lfo=(97, 85), cutoff=96, atimes=(3, 15, 25, 30, 30), alevels=(100, 85, 95), asus=75)
    b = partial(SQU, 79, fine=63, pw=20, lfo=(99, 90), atimes=(3, 15, 25, 30, 30), alevels=(100, 90, 95), asus=80)
    return RING, a, b, N((0, 84 + s, 0.5 * k))
def ghost(s, k):     # a hollow wail that rises and sinks
    a = partial(SQU, 36, pw=0, pdepth=4, ptimes=(0, 70, 75, 60), plevels=(44, 50, 60, 52, 38), lfo=(52, 14), cutoff=40, reso=8,
                atimes=(55, 50, 60, 70, 78), alevels=(70, 100, 90), asus=80)
    b = partial(SQU, 43, fine=53, pw=10, pdepth=4, ptimes=(0, 70, 75, 60), plevels=(44, 50, 61, 52, 38), lfo=(49, 18), cutoff=36,
                level=55, atimes=(60, 50, 60, 70, 80), alevels=(60, 100, 85), asus=70)
    return MIX, a, b, N((0, 67 + s, 1.6 * k))
def skeleton(s, k):  # a dry rattle of bone: quick clacks
    a = partial(SQU, 60, pw=15, cutoff=100, atimes=(0, 4, 8, 10, 8), alevels=(100, 40, 10), asus=0)
    b = partial(SAW, 71, fine=66, atimes=(0, 4, 8, 10, 8), alevels=(100, 30, 10), asus=0)
    t = [0, 0.07, 0.13, 0.22, 0.27, 0.36]
    return RING, a, b, N(*[(x * k, 80 + s + (i * 5) % 7, 0.03) for i, x in enumerate(t)])
def headless(s, k):  # a dumb, low groan
    a = partial(SAW, 30, pdepth=3, ptimes=(0, 60, 60, 50), plevels=(54, 50, 44, 40, 36), lfo=(30, 12), cutoff=36, reso=6,
                atimes=(25, 40, 50, 60, 60), alevels=(90, 100, 80), asus=60)
    b = partial(SQU, 30, fine=40, pw=45, pdepth=3, ptimes=(0, 60, 60, 50), plevels=(54, 50, 43, 40, 36), cutoff=30,
                atimes=(25, 40, 50, 60, 60), alevels=(90, 100, 80), asus=60, level=80)
    return MIX, a, b, N((0, 36 + s, 0.95 * k))
def goblin(s, k):    # a shriek: up fast, then wavering
    a = partial(SAW, 40, pdepth=6, ptimes=(0, 18, 25, 40), plevels=(36, 62, 56, 60, 44), lfo=(74, 30), cutoff=84, reso=16,
                atimes=(2, 15, 30, 40, 35), alevels=(100, 95, 85), asus=70)
    b = partial(SQU, 47, fine=56, pw=35, pdepth=6, ptimes=(0, 18, 25, 40), plevels=(36, 63, 56, 60, 44), lfo=(80, 40),
                atimes=(2, 15, 30, 40, 35), alevels=(100, 90, 80), asus=60, level=70)
    return RING, a, b, N((0, 72 + s, 0.55 * k))
def imp(s, k):       # a high cackle: four chirps going down
    a = partial(SQU, 48, pw=30, pdepth=5, ptimes=(0, 10, 12, 12), plevels=(60, 50, 44, 40, 36), cutoff=90, reso=12,
                atimes=(0, 6, 12, 14, 12), alevels=(100, 70, 30), asus=0)
    b = partial(SAW, 55, fine=54, pdepth=5, ptimes=(0, 10, 12, 12), plevels=(61, 50, 44, 40, 36),
                atimes=(0, 6, 12, 14, 12), alevels=(100, 60, 20), asus=0, level=60)
    return MIX, a, b, N(*[(i * 0.12 * k, 88 + s - 3 * i, 0.07 * k) for i in range(4)])
def golem(s, k):     # stone grinding: a long low rumble
    a = partial(SAW, 20, lfo=(58, 40), cutoff=34, reso=10, atimes=(35, 40, 50, 60, 65), alevels=(80, 100, 90), asus=80)
    b = partial(SQU, 27, fine=37, pw=65, lfo=(66, 50), cutoff=40, atimes=(35, 40, 50, 60, 65), alevels=(80, 100, 90), asus=80)
    return RING, a, b, N((0, 28 + s, 1.2 * k))
def troll(s, k):     # a roar that falls
    a = partial(SAW, 34, pdepth=4, ptimes=(0, 25, 55, 50), plevels=(44, 58, 50, 42, 34), lfo=(82, 45), cutoff=62, reso=14,
                fdepth=45, ftimes=(0, 25, 40, 50, 50), flevels=(100, 70, 40), fsus=30,
                atimes=(6, 20, 40, 55, 55), alevels=(100, 95, 85), asus=65)
    b = partial(SAW, 34, fine=30, pdepth=4, ptimes=(0, 25, 55, 50), plevels=(44, 58, 50, 41, 34), lfo=(88, 55), cutoff=58,
                atimes=(6, 20, 40, 55, 55), alevels=(100, 95, 85), asus=65)
    return RING, a, b, N((0, 40 + s, 1.0 * k))
def ghoul(s, k):     # a rasping moan that climbs
    a = partial(SAW, 36, pdepth=3, ptimes=(0, 70, 50, 50), plevels=(42, 50, 58, 54, 46), lfo=(70, 35), cutoff=54, reso=18,
                atimes=(30, 45, 55, 60, 65), alevels=(85, 100, 90), asus=70)
    b = partial(SQU, 42, fine=61, pw=50, pdepth=3, ptimes=(0, 70, 50, 50), plevels=(42, 50, 59, 54, 46), lfo=(76, 45), cutoff=60,
                atimes=(30, 45, 55, 60, 65), alevels=(85, 100, 90), asus=70)
    return RING, a, b, N((0, 46 + s, 1.15 * k))
def fire_elemental(s, k):   # a crackling roar that opens up
    a = partial(SAW, 48, lfo=(100, 95), cutoff=30, reso=20, fdepth=80, ftimes=(40, 40, 40, 40, 40), flevels=(60, 100, 80), fsus=60,
                atimes=(20, 30, 45, 55, 55), alevels=(80, 100, 90), asus=70)
    b = partial(SAW, 59, fine=68, lfo=(96, 100), cutoff=70, atimes=(20, 30, 45, 55, 55), alevels=(80, 100, 90), asus=70)
    return RING, a, b, N((0, 52 + s, 1.0 * k))
def lizardman(s, k): # two hissing barks
    a = partial(SAW, 36, pdepth=4, ptimes=(0, 14, 20, 20), plevels=(58, 50, 42, 38, 34), lfo=(94, 50), cutoff=78, reso=12,
                atimes=(0, 10, 20, 25, 22), alevels=(100, 85, 50), asus=10)
    b = partial(SQU, 45, fine=64, pw=25, lfo=(98, 70), atimes=(0, 10, 20, 25, 22), alevels=(100, 85, 50), asus=10)
    return RING, a, b, N((0, 58 + s, 0.16 * k), (0.28 * k, 55 + s, 0.22 * k))
def mountainman(s, k):   # a deep shout
    a = partial(SAW, 36, pdepth=2, ptimes=(0, 15, 40, 40), plevels=(44, 52, 50, 46, 40), lfo=(48, 8), cutoff=40, reso=22,
                fdepth=60, ftimes=(0, 18, 35, 40, 40), flevels=(100, 60, 40), fsus=30,
                atimes=(3, 15, 35, 45, 40), alevels=(100, 90, 75), asus=40)
    b = partial(SQU, 36, fine=44, pw=35, pdepth=2, ptimes=(0, 15, 40, 40), plevels=(44, 52, 50, 46, 40), cutoff=48, reso=10,
                atimes=(3, 15, 35, 45, 40), alevels=(100, 90, 75), asus=40, level=75)
    return MIX, a, b, N((0, 46 + s, 0.45 * k))
def outcast(s, k):   # a hoarse shout
    a = partial(SAW, 36, pdepth=3, ptimes=(0, 14, 35, 40), plevels=(42, 54, 50, 47, 40), lfo=(72, 18), cutoff=50, reso=20,
                fdepth=55, ftimes=(0, 16, 30, 40, 40), flevels=(100, 65, 45), fsus=30,
                atimes=(2, 14, 30, 45, 40), alevels=(100, 90, 70), asus=35)
    b = partial(SAW, 43, fine=59, lfo=(84, 30), cutoff=56, atimes=(2, 14, 30, 45, 40), alevels=(100, 85, 65), asus=30, level=60)
    return RING, a, b, N((0, 52 + s, 0.5 * k))
def fighter(s, k):   # a two-note battle cry
    a = partial(SAW, 36, pdepth=2, ptimes=(0, 12, 30, 40), plevels=(45, 52, 50, 48, 42), lfo=(50, 8), cutoff=46, reso=22,
                fdepth=60, ftimes=(0, 15, 30, 40, 40), flevels=(100, 70, 50), fsus=35,
                atimes=(2, 12, 30, 40, 38), alevels=(100, 92, 78), asus=45)
    b = partial(SQU, 36, fine=56, pw=40, pdepth=2, ptimes=(0, 12, 30, 40), plevels=(45, 52, 50, 48, 42), cutoff=52, reso=10,
                atimes=(2, 12, 30, 40, 38), alevels=(100, 92, 78), asus=45, level=75)
    return MIX, a, b, N((0, 50 + s, 0.22 * k), (0.26 * k, 55 + s, 0.4 * k))
def gazer(s, k):     # an eerie sweep that climbs
    a = partial(SQU, 36, pw=50, pdepth=7, ptimes=(0, 80, 60, 50), plevels=(30, 40, 62, 66, 58), lfo=(60, 20), cutoff=28, reso=28,
                fdepth=90, ftimes=(60, 60, 50, 50, 50), flevels=(40, 100, 80), fsus=60,
                atimes=(40, 45, 55, 60, 65), alevels=(80, 100, 90), asus=75)
    b = partial(SAW, 48, fine=52, pdepth=7, ptimes=(0, 80, 60, 50), plevels=(31, 41, 63, 67, 58), cutoff=34, reso=24, fdepth=80,
                ftimes=(60, 60, 50, 50, 50), flevels=(40, 100, 80), fsus=60, level=60,
                atimes=(40, 45, 55, 60, 65), alevels=(80, 100, 90), asus=75)
    return MIX, a, b, N((0, 60 + s, 1.2 * k))
def mage(s, k):      # a sharp two-tone call
    a = partial(SQU, 36, pw=20, pdepth=2, ptimes=(0, 10, 30, 30), plevels=(46, 52, 50, 50, 46), lfo=(56, 10), cutoff=58, reso=18,
                fdepth=50, ftimes=(0, 14, 30, 40, 40), flevels=(100, 75, 55), fsus=40,
                atimes=(1, 12, 30, 40, 36), alevels=(100, 90, 75), asus=45)
    b = partial(SAW, 48, fine=50, cutoff=60, level=50, atimes=(1, 12, 30, 40, 36), alevels=(100, 85, 70), asus=40)
    return MIX, a, b, N((0, 62 + s, 0.2 * k), (0.24 * k, 57 + s, 0.38 * k))
def wisp(s, k):      # a shimmering chime
    a = partial(SQU, 48, pw=0, lfo=(44, 10), cutoff=70, reso=6, atimes=(0, 30, 55, 70, 80), alevels=(100, 70, 50), asus=30)
    b = partial(SQU, 67, fine=43, pw=10, lfo=(38, 22), atimes=(0, 30, 55, 70, 82), alevels=(100, 75, 55), asus=35)
    return RING, a, b, N((0, 84 + s, 0.9 * k), (0.18 * k, 91 + s, 0.9 * k))

CREATURES = [('worm', worm), ('slug', slug), ('bat', bat), ('lurker', lurker), ('rat', rat), ('giant-spider', spider),
             ('ghost', ghost), ('skeleton', skeleton), ('headless', headless), ('goblin', goblin), ('imp', imp),
             ('golem', golem), ('troll', troll), ('ghoul', ghoul), ('fire-elemental', fire_elemental),
             ('lizardman', lizardman), ('mountainman', mountainman), ('outcast', outcast), ('fighter', fighter),
             ('gazer', gazer), ('mage', mage), ('wisp', wisp)]
VARIANTS = [(0, 1.0, 'as designed'), (4, 0.7, 'higher and shorter'), (-5, 1.4, 'lower and longer')]

def main():
    os.makedirs(OUT, exist_ok=True)
    start()
    whole, index, problems = [], [], []
    gap = [0] * int(RATE * 0.6)
    for name, fn in CREATURES:
        for vi, (shift, k, what) in enumerate(VARIANTS, 1):
            structure, p1, p2, notes = fn(shift, k)
            install(name[:10], structure, p1, p2)
            s = play(notes)
            peak = max(abs(v) for v in s) if s else 0
            rms = math.sqrt(sum(v * v for v in s) / max(1, len(s)))
            if peak < 400: problems.append('%s-%d: nearly silent (peak %d)' % (name, vi, peak))
            s = loud(s) if peak >= 400 else s
            fnm = '%s-%d.wav' % (name, vi)
            write_wav(os.path.join(OUT, fnm), s)
            at = len(whole) / RATE
            index.append('%6.1f s  %-20s %-20s %4.2f s  (raw peak %5d, rms %5d)' % (at, fnm, what, len(s) / RATE, peak, rms))
            whole += s + gap
        whole += gap
    write_wav(os.path.join(BASE, 'all-creature-calls.wav'), whole)
    with open(os.path.join(BASE, 'index.txt'), 'w', encoding='utf-8') as f:
        f.write('all-creature-calls.wav: where each sound starts (three a creature: 1 as designed, 2 higher and shorter, '
                '3 lower and longer)\n\n' + '\n'.join(index) + '\n')
    print('\n'.join(index))
    print('%d sounds, %.0f s in all-creature-calls.wav, rate %d' % (len(index), len(whole) / RATE, RATE))
    for p in problems: print('PROBLEM:', p)
    L.mt32emu_close_synth(CTX); L.mt32emu_free_context(CTX)

if __name__ == '__main__':
    main()
