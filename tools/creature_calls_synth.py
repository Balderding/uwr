r"""creature_calls_synth.py (2026-11-16): creature calls built directly as sound, not through the Roland (the user: the
MT-32 drafts of creature_calls_mt32.py "just sound like instruments"). Each sound is modelled on how such a call is
made: a vibrating source (vocal folds: a pulse train whose pitch wanders and breaks up when it roars), shaped by the
resonances of a throat and mouth (formants, which may move during the call), with breath noise, hisses, clicks,
bubbles and rumble where the creature has no voice. numpy only; mono, 16 bit, 32000 Hz (the game's effects' rate).
Kept as a tool at the user's word ("it's ok"); its sounds are not in the game.

    O:\appsnotonc\finetune\python\python.exe -I tools\creature_calls_synth.py
        -> work\creature-calls\wav2\<creature>-<n>.wav, all-creature-calls-2.wav, index-2.txt

Three takes a creature: the same recipe with a different random seed, pitch and length, as an animal never calls the
same way twice.
"""
import os, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(ROOT, 'work', 'creature-calls')
OUT = os.path.join(BASE, 'wav2')
SR = 32000
R = np.random.default_rng(1)

# ---- building blocks ----
def curve(points, n):
    """a line through (position 0..1, value) points, n samples"""
    xs, ys = zip(*points)
    return np.interp(np.linspace(0, 1, n), xs, ys)

def env(n, attack=0.02, release=0.08, shape=None):
    """the loudness over the sound: attack and release in seconds, or a list of (position, level) points"""
    if shape: e = curve(shape, n)
    else:
        e = np.ones(n)
        a, r = max(1, min(n // 2, int(attack * SR))), max(1, min(n // 2, int(release * SR)))
        e[:a] *= np.linspace(0, 1, a) ** 0.7
        e[-r:] *= np.linspace(1, 0, r) ** 1.5
    return e

def smooth_noise(n, hz):
    """noise that changes about hz times a second, -1..1 (for wandering pitch and unsteady loudness)"""
    k = max(2, int(n / SR * hz) + 2)
    return np.interp(np.linspace(0, k - 1, n), np.arange(k), R.uniform(-1, 1, k))

def spectrum_filter(x, gain_of_f):
    """x through a filter given as gain by frequency (zero phase)"""
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    return np.fft.irfft(X * gain_of_f(f), len(x))

def formants(x, fs):
    """x through resonances [(centre Hz, width Hz, gain)], summed: a throat and mouth"""
    def g(f):
        h = np.zeros_like(f)
        for fc, bw, a in fs:
            h += a / np.sqrt(1 + ((f - fc) / (bw / 2)) ** 2)
        return h
    return spectrum_filter(x, g)

def moving_formants(x, sets, where=None):
    """the mouth changing shape during the call: x through each set, cross-faded in turn (where: the positions 0..1
    at which each set is fully reached)"""
    n = len(x)
    if len(sets) == 1: return formants(x, sets[0])
    where = where or list(np.linspace(0, 1, len(sets)))
    pos = np.linspace(0, 1, n)
    out = np.zeros(n)
    for i, s in enumerate(sets):
        w = np.interp(pos, where, [1.0 if j == i else 0.0 for j in range(len(sets))])
        out += formants(x, s) * w
    return out

def band(x, lo, hi, edge=0.25):
    """band-passed x, with soft edges"""
    def g(f):
        up = 1 / (1 + (lo / np.maximum(f, 1)) ** (2 / edge))
        dn = 1 / (1 + (f / hi) ** (2 / edge))
        return up * dn
    return spectrum_filter(x, g)

def noise(n):
    return R.normal(0, 1, n)

def voice(dur, f0, jitter=0.02, rough=0.0, sub=0.0, tilt=1.0, vibrato=(0, 0)):
    """vocal folds: a pulse train at pitch f0 (points) with pitch wander (jitter), break-up (rough: fast random pitch and
    loudness changes, as in a roar), every other pulse weaker (sub: the octave-below rattle of a growl), brightness
    (tilt: 1 bright, 3 dull), vibrato (rate Hz, depth as a fraction)"""
    n = int(dur * SR)
    t = np.arange(n) / SR
    f = curve(f0, n)
    f = f * (1 + jitter * smooth_noise(n, 25) + rough * 0.25 * smooth_noise(n, 110) + vibrato[1] * np.sin(2 * np.pi * vibrato[0] * t))
    ph = np.cumsum(f) / SR
    cyc = np.floor(ph)
    frac = ph - cyc
    src = np.exp(-frac * (6.0 / tilt)) * (1 - np.exp(-frac * 60))       # a sharp pulse each cycle, decaying
    src = src - src.mean()
    if sub: src *= 1 - sub * (cyc % 2)
    if rough: src *= 1 + rough * 0.8 * smooth_noise(n, 70)
    return src

def soft(x, drive):
    """the hard edge of a scream: soft clipping"""
    x = x / (np.abs(x).max() + 1e-9)
    return np.tanh(x * drive) / np.tanh(drive)

def call(dur, f0, mouth, breath=0.1, where=None, drive=1.5, shape=None, attack=0.02, release=0.1, **voice_kw):
    """a voiced call: voice + breath through the (moving) mouth"""
    n = int(dur * SR)
    src = voice(dur, f0, **voice_kw)
    src = src / (np.abs(src).max() + 1e-9) + breath * noise(n) * 0.5
    y = moving_formants(src, mouth, where)
    return soft(y, drive) * env(n, attack, release, shape)

def hiss(dur, lo, hi, shape=None, attack=0.03, release=0.1, flutter=0.0, flutter_hz=30):
    n = int(dur * SR)
    y = band(noise(n), lo, hi)
    if flutter: y *= 1 - flutter * 0.5 * (1 + smooth_noise(n, flutter_hz))
    y /= np.abs(y).max() + 1e-9
    return y * env(n, attack, release, shape)

def bubbles(dur, count, f_lo, f_hi, rise=1.6, length=(0.03, 0.09)):
    """bubbles bursting: each a short tone that climbs and dies"""
    n = int(dur * SR)
    y = np.zeros(n)
    for _ in range(count):
        L = int(R.uniform(*length) * SR)
        at = int(R.uniform(0, max(1, n - L)))
        f = R.uniform(f_lo, f_hi)
        tt = np.arange(L) / SR
        ph = 2 * np.pi * np.cumsum(f * (1 + (rise - 1) * tt / tt[-1])) / SR
        y[at:at + L] += np.sin(ph) * np.exp(-tt * R.uniform(25, 60)) * R.uniform(0.4, 1.0)
    return y

def clicks(dur, times, res, decay=90.0, spread=0.004):
    """dry knocks (bone, chitin): each a burst of noise ringing in a few resonances"""
    n = int(dur * SR)
    y = np.zeros(n)
    for tm in times:
        at = int((tm + R.uniform(-spread, spread)) * SR)
        L = int(0.05 * SR)
        if at < 0 or at + L > n: continue
        tt = np.arange(L) / SR
        k = sum(a * np.sin(2 * np.pi * f * R.uniform(0.94, 1.06) * tt + R.uniform(0, 6)) for f, a in res)
        y[at:at + L] += (k + 0.4 * R.normal(0, 1, L)) * np.exp(-tt * decay) * R.uniform(0.5, 1.0)
    return y

def mix(*parts):
    """(sound, start seconds, level) parts laid over one another"""
    n = max(int(s * SR) + len(x) for x, s, _ in parts)
    y = np.zeros(n)
    for x, s, lv in parts:
        a = int(s * SR)
        y[a:a + len(x)] += x / (np.abs(x).max() + 1e-9) * lv
    return y

# vowel shapes: (centre, width, gain) for the first three resonances
def V(f1, f2, f3, big=1.0):
    """big > 1: a longer throat (a larger creature): every resonance lower"""
    return [(f1 / big, 90, 1.0), (f2 / big, 120, 0.7), (f3 / big, 180, 0.35), (3600 / big, 300, 0.12)]
AH, EE, OO, EH, UH, AW = V(750, 1250, 2600), V(300, 2300, 3000), V(320, 800, 2400), V(550, 1800, 2600), V(450, 1100, 2500), V(600, 900, 2500)

# ---- the creatures: each a function of (pitch factor p, length factor k) ----
def worm(p, k):          # wet squelching: bubbles in mud
    d = 0.7 * k
    return mix((bubbles(d, 14, 180 * p, 520 * p), 0, 1.0), (hiss(d, 150, 900, flutter=0.9, flutter_hz=14), 0, 0.35))
def slug(p, k):          # a slow slimy squelch, lower
    d = 0.95 * k
    return mix((bubbles(d, 8, 110 * p, 300 * p, rise=1.3, length=(0.06, 0.14)), 0, 1.0),
               (hiss(d, 100, 600, flutter=1.0, flutter_hz=6, attack=0.15, release=0.3), 0, 0.5))
def bat(p, k):           # thin, very high squeaks
    parts = []
    t = 0.0
    for i in range(int(R.integers(3, 6))):
        d = R.uniform(0.035, 0.07) * k
        f = R.uniform(4200, 5600) * p
        c = call(d, [(0, f), (0.3, f * 1.25), (1, f * 0.7)], [[(f, 900, 1.0), (f * 1.9, 1500, 0.3)]], breath=0.25, drive=1.2,
                 attack=0.004, release=0.02, jitter=0.03, tilt=0.6)
        parts.append((c, t, R.uniform(0.6, 1.0)))
        t += d + R.uniform(0.04, 0.09) * k
    return mix(*parts)
def lurker(p, k):        # a deep gargling roar from under water
    d = 1.1 * k
    r = call(d, [(0, 62 * p), (0.3, 70 * p), (1, 48 * p)], [V(420, 900, 2200, 1.5)], breath=0.25, drive=2.5, rough=0.8, sub=0.5,
             tilt=1.6, attack=0.08, release=0.3)
    return mix((r, 0, 1.0), (bubbles(d, 18, 140, 420, length=(0.03, 0.08)), 0, 0.45))
def rat(p, k):           # sharp squeals
    parts, t = [], 0.0
    for i in range(2):
        d = R.uniform(0.09, 0.16) * k
        f = R.uniform(2300, 3000) * p
        parts.append((call(d, [(0, f * 0.8), (0.35, f * 1.2), (1, f)], [[(f * 1.05, 700, 1.0), (f * 2.1, 1200, 0.35)]], breath=0.2,
                           drive=2.0, rough=0.25, jitter=0.04, tilt=0.7, attack=0.006, release=0.04), t, 1.0))
        t += d + 0.07 * k
    return mix(*parts)
def spider(p, k):        # a hiss with the chitter of mouth-parts
    d = 0.75 * k
    ts = np.cumsum(R.uniform(0.018, 0.05, 30))
    ts = ts[ts < d - 0.06]
    return mix((hiss(d, 3200 * p, 9000, shape=[(0, 0), (0.12, 1), (0.6, 0.7), (1, 0)], flutter=0.7, flutter_hz=38), 0, 0.8),
               (clicks(d, ts, [(2400 * p, 1.0), (4100 * p, 0.6), (6300 * p, 0.3)], decay=220), 0, 1.0))
def ghost(p, k):         # a hollow, breathy wail
    d = 2.1 * k
    f = 310 * p
    a = call(d, [(0, f * 0.85), (0.35, f * 1.2), (0.7, f * 1.05), (1, f * 0.72)], [OO, UH, OO], breath=0.9, drive=1.0, jitter=0.012,
             vibrato=(5.2, 0.018), tilt=2.2, shape=[(0, 0), (0.25, 0.8), (0.5, 1), (0.8, 0.6), (1, 0)])
    b = call(d, [(0, f * 0.85 * 1.5), (0.35, f * 1.21 * 1.5), (0.7, f * 1.04 * 1.5), (1, f * 0.7 * 1.5)], [OO], breath=1.2, drive=1.0,
             jitter=0.02, vibrato=(4.6, 0.02), tilt=2.6, shape=[(0, 0), (0.4, 0.6), (0.7, 1), (1, 0)])
    return mix((a, 0, 1.0), (b, 0.05, 0.4), (hiss(d, 500, 3500, shape=[(0, 0), (0.3, 1), (1, 0)], flutter=0.5, flutter_hz=3), 0, 0.25))
def skeleton(p, k):      # bones rattling, a dry hollow breath
    d = 0.9 * k
    ts = np.cumsum(R.uniform(0.03, 0.11, 16))
    ts = ts[ts < d - 0.08]
    return mix((clicks(d, ts, [(1150 * p, 1.0), (2700 * p, 0.7), (4300 * p, 0.4)], decay=120), 0, 1.0),
               (hiss(d, 700, 2600, shape=[(0, 0), (0.2, 1), (0.7, 0.5), (1, 0)]), 0, 0.3))
def headless(p, k):      # a muffled, mindless groan from the chest (it has no mouth)
    d = 1.2 * k
    y = call(d, [(0, 86 * p), (0.3, 92 * p), (1, 66 * p)], [V(300, 620, 1900, 1.2)], breath=0.3, drive=1.6, rough=0.35, sub=0.25,
             tilt=2.6, attack=0.12, release=0.4)
    return band(y, 40, 900)
def goblin(p, k):        # a ragged shriek: "eeAAH"
    d = 0.7 * k
    f = 520 * p
    return call(d, [(0, f * 0.8), (0.18, f * 1.45), (0.6, f * 1.3), (1, f * 0.9)], [EE, AH, AH], where=[0, 0.3, 1], breath=0.3,
                drive=4.0, rough=0.7, sub=0.3, jitter=0.05, tilt=0.8, attack=0.01, release=0.12)
def imp(p, k):           # a nasty little cackle
    parts, t = [], 0.0
    f = 780 * p
    for i in range(5):
        d = 0.085 * k
        parts.append((call(d, [(0, f * 1.15), (1, f * 0.85)], [EH], breath=0.5, drive=2.5, rough=0.3, jitter=0.04, tilt=0.9,
                           attack=0.008, release=0.04), t, 1.0 - 0.1 * i))
        t += 0.13 * k
        f *= 0.9
    return mix(*parts)
def golem(p, k):         # stone grinding on stone
    d = 1.4 * k
    n = int(d * SR)
    g = band(noise(n), 30, 700) * (0.35 + 0.65 * np.abs(smooth_noise(n, 9)))
    g = formants(g, [(85 * p, 40, 1.0), (170 * p, 60, 0.8), (410 * p, 120, 0.5), (900 * p, 300, 0.2)])
    grit = band(noise(n), 1500, 5000) * np.abs(smooth_noise(n, 22)) ** 2
    return mix((g * env(n, 0.15, 0.4), 0, 1.0), (grit * env(n, 0.2, 0.4), 0, 0.18))
def troll(p, k):         # a huge roar
    d = 1.4 * k
    f = 105 * p
    return call(d, [(0, f * 0.85), (0.2, f * 1.3), (0.6, f * 1.1), (1, f * 0.62)], [V(650, 1050, 2400, 1.35), V(750, 1200, 2500, 1.35),
                V(500, 900, 2300, 1.35)], breath=0.35, drive=5.0, rough=1.0, sub=0.6, jitter=0.05, tilt=1.1,
                shape=[(0, 0), (0.08, 1), (0.6, 0.9), (1, 0)])
def ghoul(p, k):         # a rasping, hungry moan
    d = 1.4 * k
    f = 150 * p
    v = call(d, [(0, f * 0.9), (0.5, f * 1.2), (1, f * 0.8)], [UH, AH, UH], breath=0.9, drive=3.0, rough=0.8, sub=0.55, jitter=0.06,
             tilt=1.3, shape=[(0, 0), (0.25, 0.8), (0.6, 1), (1, 0)])
    return mix((v, 0, 1.0), (hiss(d, 1500, 6000, shape=[(0, 0), (0.3, 1), (1, 0)], flutter=0.8, flutter_hz=45), 0, 0.3))
def fire_elemental(p, k):    # a roaring, crackling blaze
    d = 1.3 * k
    n = int(d * SR)
    lo = band(noise(n), 60, 500) * (0.5 + 0.5 * np.abs(smooth_noise(n, 12)))
    hi = band(noise(n), 800 * p, 5000) * np.abs(smooth_noise(n, 30))
    up = curve([(0, 0.2), (0.4, 1), (1, 0.5)], n)
    ts = np.sort(R.uniform(0, d - 0.06, int(40 * k)))
    return mix((lo * env(n, 0.1, 0.4), 0, 1.0), (hi * up * env(n, 0.1, 0.4), 0, 0.7),
               (clicks(d, ts, [(3000, 1.0), (5200, 0.6)], decay=400, spread=0), 0, 0.5))
def lizardman(p, k):     # a hiss, then a barked "ssKAH"
    h = hiss(0.22 * k, 3500, 9000, shape=[(0, 0), (0.6, 1), (1, 0.6)])
    f = 190 * p
    b = call(0.26 * k, [(0, f * 1.3), (0.3, f), (1, f * 0.75)], [AH, EH], breath=0.5, drive=4.0, rough=0.7, sub=0.4, tilt=0.9,
             attack=0.006, release=0.1)
    return mix((h, 0, 0.6), (b, 0.2 * k, 1.0), (h * 0.5, 0.42 * k, 0.25))
def mountainman(p, k):   # a deep "HEY!"
    f = 125 * p
    v = call(0.5 * k, [(0, f * 0.9), (0.25, f * 1.3), (1, f * 0.85)], [V(600, 1500, 2500, 1.12), V(430, 2000, 2700, 1.12)], where=[0.15, 1],
             breath=0.2, drive=2.5, rough=0.25, jitter=0.02, tilt=1.2, attack=0.02, release=0.15)
    return mix((hiss(0.07, 800, 5000), 0, 0.25), (v, 0.04, 1.0))
def outcast(p, k):       # a hoarse "HAAH!"
    f = 165 * p
    v = call(0.55 * k, [(0, f), (0.2, f * 1.35), (1, f * 0.8)], [AH, AH, UH], breath=0.7, drive=3.5, rough=0.6, sub=0.3, jitter=0.04,
             tilt=1.0, attack=0.015, release=0.2)
    return mix((hiss(0.09, 900, 5000), 0, 0.35), (v, 0.05, 1.0))
def fighter(p, k):       # a battle cry: "YAAH!"
    f = 150 * p
    return call(0.65 * k, [(0, f * 0.9), (0.25, f * 1.45), (0.7, f * 1.3), (1, f * 0.9)], [EE, AH, AH], where=[0, 0.22, 1], breath=0.25,
                drive=3.0, rough=0.35, jitter=0.025, tilt=1.0, attack=0.015, release=0.18)
def gazer(p, k):         # an unearthly rising whine that pulses
    d = 1.5 * k
    n = int(d * SR)
    f = 210 * p
    v = call(d, [(0, f), (0.6, f * 2.2), (1, f * 2.9)], [[(900, 80, 1.0), (2700, 150, 0.6), (4200, 200, 0.3)]], breath=0.15, drive=1.5,
             jitter=0.01, tilt=1.4, shape=[(0, 0), (0.3, 0.8), (0.85, 1), (1, 0)])
    v *= 0.55 + 0.45 * np.sin(2 * np.pi * curve([(0, 9), (1, 26)], n).cumsum() / SR)
    drone = call(d, [(0, 70 * p), (1, 62 * p)], [V(300, 700, 2000, 1.3)], breath=0.1, drive=1.2, tilt=2.5, attack=0.2, release=0.4)
    return mix((v, 0, 1.0), (drone, 0, 0.5))
def mage(p, k):          # a sharp word of command: "KO-RAH!"
    f = 175 * p
    a = call(0.2 * k, [(0, f * 1.1), (1, f)], [AW, OO], breath=0.2, drive=2.0, jitter=0.02, tilt=1.2, attack=0.01, release=0.05)
    b = call(0.42 * k, [(0, f * 1.05), (0.3, f * 1.35), (1, f * 0.9)], [UH, AH], where=[0, 0.3], breath=0.25, drive=2.5, rough=0.2,
             tilt=1.1, attack=0.02, release=0.18)
    return mix((hiss(0.04, 1500, 6000), 0, 0.3), (a, 0.03, 0.9), (b, 0.27 * k, 1.0))
def wisp(p, k):          # a glassy shimmer, neither voice nor animal
    d = 1.8 * k
    n = int(d * SR)
    t = np.arange(n) / SR
    y = np.zeros(n)
    for ratio, a in ((1, 1.0), (2.76, 0.5), (5.4, 0.3), (8.93, 0.15), (1.007, 0.8), (2.79, 0.4)):
        y += a * np.sin(2 * np.pi * 1320 * p * ratio * t + R.uniform(0, 6)) * (0.6 + 0.4 * np.sin(2 * np.pi * R.uniform(2, 7) * t))
    return mix((y * env(n, shape=[(0, 0), (0.1, 1), (0.5, 0.6), (1, 0)]), 0, 1.0),
               (hiss(d, 5000, 12000, shape=[(0, 0), (0.2, 1), (1, 0)], flutter=0.6, flutter_hz=8), 0, 0.12))

CREATURES = [('worm', worm), ('slug', slug), ('bat', bat), ('lurker', lurker), ('rat', rat), ('giant-spider', spider),
             ('ghost', ghost), ('skeleton', skeleton), ('headless', headless), ('goblin', goblin), ('imp', imp),
             ('golem', golem), ('troll', troll), ('ghoul', ghoul), ('fire-elemental', fire_elemental),
             ('lizardman', lizardman), ('mountainman', mountainman), ('outcast', outcast), ('fighter', fighter),
             ('gazer', gazer), ('mage', mage), ('wisp', wisp)]
TAKES = [(1.0, 1.0, 'take 1'), (1.12, 0.85, 'take 2: a little higher and shorter'), (0.88, 1.2, 'take 3: a little lower and longer')]

def to_pcm(y, peak=0.8):
    y = y - y.mean()
    n = len(y)
    f = min(n // 2, int(0.01 * SR))
    y[:f] *= np.linspace(0, 1, f); y[-f:] *= np.linspace(1, 0, f)     # no click at either end
    y = y / (np.abs(y).max() + 1e-9) * peak
    return (y * 32767).astype('<i2')

def write_wav(path, pcm):
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(pcm.tobytes())

def main():
    global R
    os.makedirs(OUT, exist_ok=True)
    whole, index = [], []
    gap = np.zeros(int(0.6 * SR), '<i2')
    at = 0
    for ci, (name, fn) in enumerate(CREATURES):
        for ti, (p, k, what) in enumerate(TAKES, 1):
            R = np.random.default_rng(1000 + ci * 10 + ti)
            pcm = to_pcm(fn(p, k))
            fnm = '%s-%d.wav' % (name, ti)
            write_wav(os.path.join(OUT, fnm), pcm)
            rms = float(np.sqrt((pcm.astype(float) ** 2).mean()))
            index.append('%6.1f s  %-22s %-38s %4.2f s  (rms %5.0f)' % (at / SR, fnm, what, len(pcm) / SR, rms))
            whole += [pcm, gap]; at += len(pcm) + len(gap)
        whole.append(gap); at += len(gap)
    write_wav(os.path.join(BASE, 'all-creature-calls-2.wav'), np.concatenate(whole))
    with open(os.path.join(BASE, 'index-2.txt'), 'w', encoding='utf-8') as f:
        f.write('all-creature-calls-2.wav: where each sound starts (three takes a creature)\n\n' + '\n'.join(index) + '\n')
    print('\n'.join(index))
    print('%d sounds, %.0f s in all-creature-calls-2.wav' % (len(index), at / SR))

if __name__ == '__main__':
    main()
