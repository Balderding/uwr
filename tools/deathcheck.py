# deathcheck.py: the death in our display (docs\superpowers\plans\2026-11-12-death-scene.md, Task 1). Off-screen runs of
# the test exe (underworld-exhumed\uw1\build\port\uw1port.exe) with a fresh copy of the test home work\in2-4, each
# asserted from its stderr and its window shot:
#   A  mouse-look on, cheat die, pointer moves on the menu: mouse-look's capture let go BEFORE the first pointer move
#      (at the death, not on a raw event); the menu up (PS_MENU) after PS_DEATH; the pointer drawn where it was moved
#      and nowhere else (the game's crosshair cursor colour counted in the window shot)
#   B  a key during the skulls: the skulls end, the menu's four buttons intact (the 320x200 frame equals A's)
#   C  focus lost and back during the skulls: no "capture on" after the death
# Usage: python -I tools\deathcheck.py [A|B|C ...] (all when none). Exit code: the failures.
import os, sys, shutil, subprocess, struct, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port', 'uw1port.exe')
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'deathcheck')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"

def run(name, script, shot_ms, exit_ms, env):
    home = os.path.join(WORK, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f:
        f.write('settings-at-start=0\n')
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', UWPORT_LOOKDBG='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', **env)
    shot = os.path.join(WORK, name + '.png'); wshot = os.path.join(WORK, name + '-w.png')
    cmd = [EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio', '--input-script', sp,
           '--screenshot-after', str(shot_ms), '--screenshot', shot, '--window-shot', wshot, '--exit-after', str(exit_ms)]
    r = subprocess.run(cmd, env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    return r.stderr.splitlines(), shot, wshot

def png_rgb(path):
    """a PNG's pixels as (w, h, bytes rgb) (8-bit RGB or RGBA, non-interlaced: what the port writes)"""
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', path
    pos, idat, w = 8, b'', 0
    while pos < len(d):
        n, = struct.unpack('>I', d[pos:pos + 4]); t = d[pos + 4:pos + 8]; c = d[pos + 8:pos + 8 + n]
        if t == b'IHDR': w, h, depth, ctype = struct.unpack('>IIBB', c[:10]); assert depth == 8 and ctype in (2, 6)
        elif t == b'IDAT': idat += c
        pos += 12 + n
    raw = zlib.decompress(idat); bpp = 3 if ctype == 2 else 4; stride = w * bpp
    out = bytearray(); prev = bytearray(stride); p = 0
    for y in range(h):
        f = raw[p]; line = bytearray(raw[p + 1:p + 1 + stride]); p += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0; b = prev[i]; cc = prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - cc), abs(a - cc), abs(a + b - 2 * cc)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else cc)) & 255
        prev = line
        out += bytes(line[j] for j in range(stride) if bpp == 3 or j % 4 != 3) if bpp == 4 else line
    return w, h, bytes(out)

def cursor_pixels(wshot):
    """the game's crosshair cursor in the window shot: its bright gold pixels (the menu's own golds are darker and
    larger blobs; the crosshair is 1-2 pixels wide), as (x, y) list"""
    w, h, px = png_rgb(wshot)
    pts = []
    for y in range(h):
        row = px[y * w * 3:(y + 1) * w * 3]
        for x in range(w):
            r, g, b = row[x * 3], row[x * 3 + 1], row[x * 3 + 2]
            if (r, g, b) == (255, 207, 85): pts.append((x, y))   # the crosshair's own gold (the title has some)
    return w, h, pts

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''))
    if not ok: fails += 1

def test_A():
    err, shot, wshot = run('A', MENU + "18000 cheat die\n", 68000, 70000,
                           dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_MOUSETEST='60000:mv@2000/2000,66000:mv@5000/5000'))
    off = next((i for i, l in enumerate(err) if "capture off" in l), None)
    mt = next((i for i, l in enumerate(err) if "mouse test" in l), None)
    check('A capture let go at the death, before the first pointer move', off is not None and mt is not None and off < mt,
          'capture off line %s, mouse test line %s' % (off, mt))
    st = [int(l.split('screen ')[1].split()[0]) for l in err if ': screen ' in l]
    check('A screens: PS_GAME, PS_DEATH, then PS_MENU', st[-2:] == [7, 1] and 2 in st, str(st))
    w, h, pts = cursor_pixels(wshot)
    near = [p for p in pts if abs(p[0] - w // 2) < 60 and abs(p[1] - h // 2) < 60]
    check('A the pointer drawn at the last move (the middle), once', 50 <= len(near) <= 400 and len(pts) - len(near) < 400,
          '%d crosshair pixels near the middle, %d elsewhere' % (len(near), len(pts) - len(near)))
    return shot

def test_B(shot_a):
    err, shot, wshot = run('B', MENU + "18000 cheat die\n22000 key enter\n", 68000, 70000, dict(UWPORT_FOCUSTEST='1000,1500'))
    st = [int(l.split('screen ')[1].split()[0]) for l in err if ': screen ' in l]
    same = 0
    if shot_a and os.path.exists(shot):   # the menu's palette cycles (the rock): most pixels equal, not all
        a, b = png_rgb(shot)[2], png_rgb(shot_a)[2]
        same = sum(1 for i in range(0, len(a), 3) if a[i:i + 3] == b[i:i + 3]) / (len(a) / 3)
    check('B a key during the skulls: the menu with its four buttons, as A', st[-2:] == [7, 1] and same > 0.9,
          'screens %s, %.1f%% of the frame as A' % (st, same * 100))

def test_C():
    err, shot, wshot = run('C', MENU + "18000 cheat die\n", 68000, 70000, dict(UWPORT_FOCUSTEST='20000,25000'))
    die = next((i for i, l in enumerate(err) if 'cheat: die' in l), None)
    on_after = [l for l in err[die + 1:] if 'capture on' in l] if die is not None else ['no death']
    check('C focus lost during the skulls: no capture after the death', die is not None and not on_after, str(on_after))

def test_menu():
    """Task 2, the start menu's choosing in our code: a run from the title with the window's own pointer moves and
    clicks (UWPORT_MOUSETEST) on Journey Onward then the first saved game restores a game; a keyboard-only run
    restores too; Escape in the saved games list brings the four buttons back"""
    esc = "3000 key esc\n"
    err, shot, wshot = run('M1', esc, 18000, 20000, dict(UWPORT_FOCUSTEST='1000,1500',
        UWPORT_MOUSETEST='8000:mv@5000/8190,8500:ld@5000/8190,8700:lu@5000/8190,11000:mv@5000/6390,11500:ld@5000/6390,11700:lu@5000/6390'))
    st = [int(l.split('screen ')[1].split()[0]) for l in err if ': screen ' in l]
    check('menu: clicks on Journey Onward and the first save restore a game', any('uwview: level' in l for l in err) and st[-1:] == [2], str(st))
    err, shot, wshot = run('M2', esc + "9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n", 18000, 20000,
                           dict(UWPORT_FOCUSTEST='1000,1500'))
    st = [int(l.split('screen ')[1].split()[0]) for l in err if ': screen ' in l]
    check('menu: the keys alone restore a game', any('uwview: level' in l for l in err) and st[-1:] == [2], str(st))
    err0, shot0, w0 = run('M3', esc, 16000, 17000, dict(UWPORT_FOCUSTEST='1000,1500'))
    err, shot, wshot = run('M4', esc + "9000 key end\n9300 key enter\n12000 key esc\n", 16000, 17000, dict(UWPORT_FOCUSTEST='1000,1500'))
    st = [int(l.split('screen ')[1].split()[0]) for l in err if ': screen ' in l]
    a, b = png_rgb(shot)[2], png_rgb(shot0)[2]
    same = sum(1 for i in range(0, len(a), 3) if a[i:i + 3] == b[i:i + 3]) / (len(a) / 3)
    check('menu: Escape in the saved games list brings the four buttons back', st[-1:] == [1] and same > 0.9,
          'screens %s, %.1f%% of the frame as the plain menu' % (st, same * 100))

if __name__ == '__main__':
    os.makedirs(WORK, exist_ok=True)
    which = sys.argv[1:] or ['A', 'B', 'C', 'menu']
    shot_a = test_A() if 'A' in which or 'B' in which else None
    if 'B' in which: test_B(shot_a)
    if 'C' in which: test_C()
    if 'menu' in which: test_menu()
    print('deathcheck: %d failures' % fails)
    sys.exit(fails)
