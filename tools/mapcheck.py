# mapcheck.py: the automap under wide-pitch (underworld-exhumed GRIDDB.C port_map_how, VIEW3D.C port_view_passes).
# The original maps what its one pass walks (PlayersMap) and counts the newly mapped squares for experience (pipeexp).
# Beyond the original's pitch (0x1000) the port draws the view in two passes with opened rays; the map and the
# experience must still come out as the original's one pass leaves them.
# For each place and heading: off-screen runs of the test exe from a fresh copy of the test home, the restore, then
# `cheat pitch`, `cheat turn`, `cheat goto`, some seconds of frames, `cheat mapdump`. Compared:
#   level: pitch 0 twice (the noise floor: must be the same bytes)
#   steep: pitch 0x2800 and -0x2800 against pitch 0 (PASS: the same map bytes and the same experience)
#   old:   steep with UW1PORT_MAP_AS_DRAWN=1 (the map as it was before the fix; reported, expected to differ)
# Usage: python -I tools\mapcheck.py [quick]      Exit code: the failures. Never while uw-new.exe runs.
import os, sys, shutil, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.environ.get('MAPCHECK_EXE') or os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port', 'uw1port.exe')
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'mapcheck')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"
PLACES = [('level2', 2, 28, 28), ('door', 1, 21, 5), ('steeltoe', 2, 9, 14), ('hall', 1, 6, 42), ('corner', 1, 36, 7)]
HEADINGS = [0, 0x2000, 0x4000, 0x7000, 0xA000, 0xE000]

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''), flush=True)
    if not ok: fails += 1

def playing():
    # the user's own game: uw-new.exe / uw-vulkan.exe started without a test's --input-script or --hidden
    # (other sessions' checks run the dist exe with those)
    try:
        out = subprocess.run(['wmic', 'process', 'where', "name='uw-new.exe' or name='uw-vulkan.exe'", 'get', 'CommandLine'],
                             capture_output=True, text=True, errors='replace').stdout
    except OSError:
        out = ''
    for l in out.splitlines():
        l = l.strip().lower()
        if ('uw-new.exe' in l or 'uw-vulkan.exe' in l) and '--input-script' not in l and '--hidden' not in l: return True
    return False

def run(name, level, x, y, heading, pitch, env=None):
    if playing():   # never while the user plays: stop at once
        print('mapcheck: uw-new.exe started (the user is playing): stopped', flush=True); sys.exit(99)
    home = os.path.join(WORK, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f:
        f.write('settings-at-start=0\nquick_fades=0\n')
    script = MENU + ("17000 cheat pitch %d\n17200 cheat turn %d\n17400 cheat goto %d %d %d\n"
                     "19000 cheat pitch %d\n19200 cheat turn %d\n23000 cheat mapdump\n" % (pitch, heading, level, x, y, pitch, heading))
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', **(env or {}))
    cmd = [EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio', '--input-script', sp,
           '--exit-after', '25000']
    r = subprocess.run(cmd, env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    rows, head = {}, None
    for l in r.stderr.splitlines():
        i = l.find('mapdump: ')
        if i >= 0: head = l[i:].split()
        i = l.find('maprow ')
        if i >= 0:
            p = l[i:].split()
            rows[int(p[1])] = bytes.fromhex(p[2])
    if head is None or len(rows) != 64: return None
    return dict(exp=int(head[2]), pitch=int(head[4]), facing=int(head[6]), map=b''.join(rows[y] for y in range(64)))

def diff(a, b):
    d = [(i % 64, i // 64, a['map'][i], b['map'][i]) for i in range(4096) if a['map'][i] != b['map'][i]]
    return d

def show(d):
    return ', '.join('(%d,%d) %02X->%02X' % t for t in d[:8]) + (' ...' if len(d) > 8 else '')

if __name__ == '__main__':
    if playing():
        print('mapcheck: uw-new.exe is running (the user is playing): not run'); sys.exit(99)
    os.makedirs(WORK, exist_ok=True)
    quick = 'quick' in sys.argv[1:]
    places = PLACES[:1] if quick else PLACES
    only = [a for a in sys.argv[1:] if a != 'quick']
    if only: places = [p for p in PLACES if p[0] in only]
    heads = HEADINGS[:2] if quick else HEADINGS
    old_diffs = 0
    for pn, lev, x, y in places:
        for h in heads:
            tag = '%s %d,%d heading %04X' % (pn, x, y, h)
            a = run('a', lev, x, y, h, 0)
            if a is None: check(tag + ': a dump', False, 'no mapdump (see work\mapcheck\a.err)'); continue
            if h == heads[0]:
                a2 = run('a2', lev, x, y, h, 0)
                check(tag + ': level twice, the same map and experience', a2 is not None and not diff(a, a2) and a['exp'] == a2['exp'])
            for pitch in (0x2800, -0x2800):
                b = run('b', lev, x, y, h, pitch)
                if b is None: check(tag + ' pitch %d: a dump' % pitch, False); continue
                d = diff(a, b)
                check('%s pitch %d (pitch now %d): the map as the level look\'s (%d squares differ), experience %d / %d' % (
                    tag, pitch, b['pitch'], len(d), a['exp'], b['exp']), not d and a['exp'] == b['exp'] and abs(b['pitch']) > 0x1000, show(d))
                o = run('o', lev, x, y, h, pitch, dict(UW1PORT_MAP_AS_DRAWN='1'))
                if o is not None:
                    do = diff(a, o)
                    old_diffs += len(do)
                    print('    before the fix (UW1PORT_MAP_AS_DRAWN): %d squares differ, experience %d / %d%s' % (
                        len(do), a['exp'], o['exp'], (': ' + show(do)) if do else ''))
    print('mapcheck: before the fix %d squares differed in all' % old_diffs)
    print('mapcheck: %s' % ('PASS' if not fails else '%d FAIL' % fails))
    sys.exit(fails)
