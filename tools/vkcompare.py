r"""vkcompare.py: the Vulkan renderer's pictures against the Direct3D 12 renderer's (docs\superpowers\plans\
2026-11-15-vulkan-renderer.md, M1 T10). Off-screen runs of build\port\uw1port.exe (the D3D12 picture is the reference):

  title  the title screen, 5 s into a run with an empty home
  menu   our Esc menu (F1) open over a saved game (work\in2-4's), the scene tinted black in both (UWVIEW_NOSCENE: the
         Vulkan renderer has no scene before M2)

Each picture is taken twice with D3D12 (the noise floor: what differs between two runs of the same renderer) and once
with Vulkan, with the validation layer on (the SDK's, VK_ADD_LAYER_PATH); counted: the pixels differing by more than
8 of 255 in any channel, the largest difference, and the box the differing pixels lie in. Then a Vulkan run that
stretches the window (a resize) and turns full screen on and off through the Esc menu, its log checked for the
renderer's frames going on and the validation layer's messages.

Run: python -I tools\vkcompare.py [--only title|menu|switch] [--visible]   (pictures and logs in work\vk\compare;
--visible: the windows on the screen, not off it: full screen really fills the monitor)
"""
import ctypes, os, shutil, subprocess, sys, threading, time, zlib, struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# (VKCOMPARE_EXE: another copy of the program with its DLLs, for when another session's runs hold build\port's files)
EXE = os.environ.get('VKCOMPARE_EXE') or os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port', 'uw1port.exe')
DATA = os.path.join(ROOT, 'dist', 'UW1', 'game')
SAVES = os.path.join(ROOT, 'work', 'in2-4')
OUT = os.path.join(ROOT, 'work', 'vk', 'compare')
LAYERS = r'O:\appsnotonc\vulkan-sdk\1.4.363.1\Bin'
THRESHOLD = 8
# the start menu's restore of the saved game (work\convshot.py's), then F1: our menu
RESTORE = ['3000 key esc', '5000 key esc', '7000 key esc', '9000 key end', '9300 key enter', '12000 key home',
           '12600 key enter', '14000 key enter']
CASES = {
    'title': dict(home=None, script=[], shot=5000),
    'menu': dict(home=SAVES, script=RESTORE + ['20000 key f1'], shot=23000),
}


def png_load(p):
    d = open(p, 'rb').read()
    i, idat, w, h, ct = 8, b'', 0, 0, 0
    while i < len(d):
        n, t = struct.unpack('>I4s', d[i:i + 8])
        c = d[i + 8:i + 8 + n]
        i += 12 + n
        if t == b'IHDR':
            w, h, _, ct = struct.unpack('>IIBB', c[:10])
        elif t == b'IDAT':
            idat += c
    bpp = {2: 3, 6: 4}[ct]
    raw, s, out, prev, o = zlib.decompress(idat), w * bpp, bytearray(), bytearray(w * bpp), 0
    for _ in range(h):
        f, line = raw[o], bytearray(raw[o + 1:o + 1 + s])
        o += 1 + s
        for x in range(s):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += line
        prev = line
    return w, h, bpp, bytes(out)


CELLS = {}   # the last comparison's differing pixels by cell of an 8 x 6 grid over the picture (column, row)


def cells_text():
    return ', '.join('%d,%d: %d' % (c[0], c[1], n) for c, n in sorted(CELLS.items(), key=lambda t: -t[1])[:6]) or 'none'


def compare(a, b, masks=()):
    """(pixels over the threshold, the largest difference, their box x0 y0 x1 y1 or None); masks: boxes (x0, y0, x1, y1,
    inclusive) left out of the count"""
    wa, ha, ba, pa = png_load(a)
    wb, hb, bb, pb = png_load(b)
    if (wa, ha) != (wb, hb):
        return None
    n = mx = 0
    box = [wa, ha, -1, -1]
    CELLS.clear()
    skip = bytearray(wa * ha)
    for (x0, y0, x1, y1) in masks:
        for y in range(max(y0, 0), min(y1, ha - 1) + 1):
            skip[y * wa + max(x0, 0):y * wa + min(x1, wa - 1) + 1] = b'\1' * (min(x1, wa - 1) + 1 - max(x0, 0))
    for k in range(wa * ha):
        if skip[k]:
            continue
        i, j = k * ba, k * bb
        d = max(abs(pa[i] - pb[j]), abs(pa[i + 1] - pb[j + 1]), abs(pa[i + 2] - pb[j + 2]))
        mx = max(mx, d)
        if d > THRESHOLD:
            n += 1
            x, y = k % wa, k // wa
            box = [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
            c = (x * 8 // wa, y * 6 // ha)
            CELLS[c] = CELLS.get(c, 0) + 1
    return n, mx, (box if n else None)


# ---- M2: the scene without ray tracing (the plan's M2 proof). Both renderers with the same options, read over the
# user's own (UWVIEW_SETTINGS: never written to theirs): ray tracing, DLSS, DLSS 5, HDR and the effects off.
M2_SETTINGS = 'raytracing 0\ndlss 0\ndlss5 0\nhdr 0\nfx_particles 0\nfx_decals 0\nfx_fog 0\nfx_water 0\n'
# What cannot be held and is masked (said in every report): the two flasks are cut from the original game's own screen,
# and the save's player is poisoned: the health flask bubbles by the game's clock.
HUD_FLASK = (16, 920, 111, 1063)     # our display: the left flask's piece (24 x 36 of the original's, 4 x, margin 16)
ORIG_FLASK = (1356, 675, 1463, 869)  # the original's display at 1920 x 1080: its left flask (x 248-271, rows 125-160)
# M3: the maps off (the settings' defaults have them on: M2's views are the "on" side)
NOMAPS = dict(UWVIEW_NORMALS='0', UWVIEW_TEXTURE_AO='0', UWVIEW_TEXTURE_SHADOW='0', UWVIEW_SPRITE_AO='0', UWVIEW_SPRITE_HEIGHT='0')
TORCH_DATA = os.path.join(ROOT, 'work', 'vk', 'torchdata')   # the same with eight wall torches (work\vk\m3\torch-lights.txt)
ORB_DATA = os.path.join(ROOT, 'work', 'vk', 'orbdata')   # a copy of the data folder (links) with its own lights.txt


def level1():
    """level 1 of the game's LEV.ARK: (tiles: [(type, height, floor texture number)] by y * 64 + x, creatures' tiles)"""
    d = open(os.path.join(DATA, 'data', 'lev.ark'), 'rb').read()
    n = struct.unpack('<H', d[:2])[0]
    offs = struct.unpack('<%dI' % n, d[2:2 + 4 * n])
    b = d[offs[0]:offs[0] + 31752]
    tm = d[offs[18]:offs[18] + 122]
    floor = struct.unpack('<10H', tm[96:116])
    tiles, crit = [], []
    for t in range(4096):
        w0, w1 = struct.unpack('<HH', b[t * 4:t * 4 + 4])
        tiles.append((w0 & 15, w0 >> 4 & 15, floor[w0 >> 10 & 15]))
        i, g = w1 >> 6, 0
        while i and g < 1024:
            g += 1
            o = 0x4000 + (i * 27 if i < 256 else 256 * 27 + (i - 256) * 8)
            w = struct.unpack('<4H', b[o:o + 8])
            if 0x40 <= (w[0] & 0x1FF) < 0x80:
                crit.append((t % 64, t // 64))
            i = w[2] >> 6
    return tiles, crit


def m2_cases():
    """the views: the saved game's start, a corridor, things on the floor, a creature's place, the roomiest hall, the
    water's edge (cycling colours: render_update_atlas), the original's display (display=original in the run's
    settings: the view in its 3D window, render_set_view_rect / render_set_lens), and the corridor full bright (no flame's time in it: an exact comparison)"""
    tiles, crit = level1()
    terr = open(os.path.join(DATA, 'data', 'terrain.dat'), 'rb').read()
    fl = struct.unpack('<256H', terr[512:1024])

    def opened(x, y):
        return 0 <= x < 64 and 0 <= y < 64 and tiles[y * 64 + x][0] == 1

    def wet(x, y):
        return opened(x, y) and (fl[tiles[y * 64 + x][2]] & 0xF0) == 0x10
    # the roomiest dry place (in water the player bobs: no two runs alike)
    hall = max(((x, y) for y in range(64) for x in range(64) if opened(x, y) and not any(wet(x + dx, y + dy) for dx in range(-3, 4) for dy in range(-3, 4))),
               key=lambda p: sum(opened(p[0] + dx, p[1] + dy) for dx in range(-3, 4) for dy in range(-3, 4)))
    shore = next(((x, y) for y in range(64) for x in range(64) if opened(x, y) and not wet(x, y) and
                  sum(wet(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))) >= 1 and
                  sum(wet(x + dx, y + dy) for dx in range(-2, 3) for dy in range(-2, 3)) >= 6), None)
    beast = next(((x + dx, y + dy) for (x, y) in crit for dx, dy in ((2, 0), (-2, 0), (0, 2), (0, -2), (1, 0), (-1, 0), (0, 1), (0, -1))
                  if opened(x + dx, y + dy) and tiles[(y + dy) * 64 + x + dx][1] == tiles[y * 64 + x][1]), None)

    def at(p, more=(), shot=21000, env=None, **kw):
        return dict(home=SAVES, script=RESTORE + ['16000 cheat goto 1 %d %d' % p] + list(more), shot=shot, env=env or {},
                    **dict(dict(mask=[HUD_FLASK]), **kw))
    cases = {'start': dict(home=SAVES, script=RESTORE, shot=18000, env={}, mask=[HUD_FLASK]),
             'corridor': at((26, 21)),
             'things': at((27, 12)),
             'hall': at(hall),
             'interface': dict(home=SAVES, script=RESTORE, shot=20000, env={}, cfg={'display': 'original'}, mask=[ORIG_FLASK]),
             'fullbright': at((26, 21), env={'UWVIEW_FULLBRIGHT': '1'})}
    if beast:   # UWVIEW_CRIT_FORCE: every creature in one sequence and frame (its own goes by the game's clock);
        cases['creature'] = at(beast, env={'UWVIEW_CRIT_FORCE': '20,0'}, frames=True)   # frames: see main's retries
    if shore:
        cases['water'] = at(shore)
    if '--m3' in sys.argv:   # M3: each feature on and off (M2's views are the maps on, the effects off)
        # (the maps shade only the placed lights: the player's own light is the original's, flat; so the maps are
        # compared in the orb ring's light. Fog: render.c stores fx_fog and draws nothing with it: nothing to port.)
        held = {'UWVIEW_HOLD_SECS': '100'}   # the flames' and the water's time (render_hold_time), every run the same
        m3 = {'orbs-maps-off': at(hall, data=ORB_DATA, env=NOMAPS),
              'corridor-tex256': at((26, 21), env={'UWVIEW_TEXRES': '256'}),
              'corridor-tex512': at((26, 21), env={'UWVIEW_TEXRES': '512'}),
              'corridor-decal': at((26, 21), env={'UWVIEW_FX_DECALS': '1', 'UWVIEW_FX_TEST': 'decal'}),
              # the water: a calm shore (11, 28) facing the pool (no creature near), the effect on and off
              'pool-on': at((11, 28), env=dict(held, UWVIEW_FX_WATER='1')),
              'pool-off': at((11, 28), env=dict(held)),
              'torches': at(hall, data=TORCH_DATA, env=dict(held)),
              # the particles: level 1's campfire (8, 42, as fxcheck's), embers, from nothing at the comparison's start
              # (UWVIEW_STEADY), 64 frames of 16 ms each (UWVIEW_PART_DT), the picture at that frame (UWVIEW_COMPARE)
              'embers': at((6, 42), env=dict(held, UWVIEW_FX_PARTICLES='1', UWVIEW_PART_DT='16', UWVIEW_CRIT_FORCE='20,0'),
                           cmp='particles', mask=[HUD_FLASK, (0, 520, 70, 720)])}   # (a goblin walking at the left edge)
        cases = m3
    if '--m4' in sys.argv:   # M4: ray tracing on (UWVIEW_RT=1 over the settings' 0); the probes converge (their seed held)
        rt = {'UWVIEW_RT': '1'}
        cases = {'rt-corridor': at((26, 21), env=dict(rt)),
                 'rt-orbs': at(hall, data=ORB_DATA, env=dict(rt)),
                 'rt-torches': at(hall, data=TORCH_DATA, env=dict(rt, UWVIEW_HOLD_SECS='100')),
                 'rt-things': at((27, 12), env=dict(rt)),
                 'rt-things-nocards': at((27, 12), env=dict(rt, UWVIEW_CARDSHADOW='0')),
                 'rt-pool': at((11, 28), env=dict(rt, UWVIEW_HOLD_SECS='100', UWVIEW_FX_WATER='1'))}
        if beast:
            cases['rt-creature'] = at(beast, env=dict(rt, UWVIEW_CRIT_FORCE='20,0'), frames=True)
    if '--m5' in sys.argv:   # M5: DLSS (DLAA, then quality 2/3), Ray Reconstruction (ray tracing High)
        dl = 'dlss 1\nport_dlss_quality %s\n'
        jit = {'UWVIEW_JITTER_HOLD': '1'}   # (eb8f692: one jitter phase in both renderers)
        cases = {'dlaa-corridor': at((26, 21), env=dict(jit), settings=dl % 'dlaa'),
                 'dlaa-orbs': at(hall, data=ORB_DATA, env=dict(jit), settings=dl % 'dlaa'),
                 'dlssq-orbs': at(hall, data=ORB_DATA, env=dict(jit), settings=dl % 'quality'),
                 'rr-orbs': at(hall, data=ORB_DATA, env=dict(jit, UWVIEW_RT='1', UWVIEW_RTHIGH='1'), settings=dl % 'dlaa')}
    if '--m6' in sys.argv:   # M6: HDR on (when the monitor runs it): the window shot is the SDR copy in both renderers
        cases = {'hdr-corridor': at((26, 21), settings='hdr 1\n'), 'hdr-orbs': at(hall, data=ORB_DATA, settings='hdr 1\n')}
    if '--orbs' in sys.argv:   # the placed lights' orbs (present's depth path): one before a wall, one half behind a corner
        cases = {'orbs': at(hall, data=ORB_DATA)}   # (its ring of eight: one before the far wall, one cut by the near corner)
    print('views: hall %s, the water\'s edge %s, by a creature %s' % (hall, shore, beast))
    return cases


def run(name, renderer, case, validate=False, extra_ms=1500, during=None):
    home = os.path.join(OUT, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)   # (our own scratch home of the previous run)
    if case['home']:
        shutil.copytree(case['home'], home, ignore=shutil.ignore_patterns('recordings', 'logs'))
    else:
        os.makedirs(home)
    cp = os.path.join(home, 'uw1port.cfg')
    lines = open(cp).read().splitlines() if os.path.exists(cp) else []
    for k, val in dict(case.get('cfg', {}), **{'settings-at-start': '0'}).items():   # (the run's own copy of the settings)
        lines = [l for l in lines if not l.startswith(k + '=')] + ['%s=%s' % (k, val)]
    open(cp, 'w').write(''.join(l + '\n' for l in lines))
    sp = os.path.join(OUT, name + '.script')
    with open(sp, 'w') as f:
        f.write(''.join(l + '\n' for l in case['script']))
    e = dict(os.environ, UWPORT_OFFSCREEN='1', UWPORT_LLM='0', UWPORT_RENDERER=renderer, UWVIEW_NOSCENE='1')
    if 'env' in case:   # an M2 view: the scene itself, the cheats for its place, its own options
        del e['UWVIEW_NOSCENE']
        sf = os.path.join(OUT, name + '.settings')
        open(sf, 'w').write(M2_SETTINGS + case.get('settings', ''))   # (a case's own options after the common ones)
        # UWVIEW_STEADY: the player's flame held steady (it goes by the clock: no two runs alike with it)
        # UWVIEW_VIT_TEST: the flasks' levels given (the save's player is poisoned: the health's level fell a step in
        # one run and not in the other). No UWPORT_FOCUSTEST (the user, 2026-11-15: nothing about focus in a run).
        e.update(EXHUME_TEST_CHEATS='1', UWVIEW_SETTINGS=sf, UWVIEW_STEADY='1', UWVIEW_VIT_TEST='20,34,2,2,0')
        e.update(case['env'])
        if case.get('data'):
            e['UWVIEW_DATA'] = case['data']
        if case.get('cmp'):   # uwview's own comparison: the picture N frames after the shot's time, not the clock's
            e['UWVIEW_COMPARE'], e['UWVIEW_COMPARE_FX'] = os.path.join(OUT, name + '-cmp'), case['cmp']
            extra_ms = max(extra_ms, 8000)
    # --visible: on the screen (the user, 2026-11-15 late: "use my desktop and focus", after an earlier "do not take my
    # focus"): only for what a hidden run cannot show (full screen, HDR on the monitor), never while the game runs.
    # The resize and full-screen case (switch) runs only when asked for by name.
    if '--visible' in sys.argv:
        if os.system('tasklist | findstr /i "uwr.exe uwr-vulkan.exe uw-new.exe uw-vulkan.exe" >nul') == 0:
            sys.exit('vkcompare: the game is running: no visible run now')
        del e['UWPORT_OFFSCREEN']
    if validate:   # (the SDK's installer registers the layer; VK_ADD_LAYER_PATH only when it is not found so)
        e['UWVK_VALIDATE'] = '1'
        if os.environ.get('VKCOMPARE_LAYER_PATH'):
            e['VK_ADD_LAYER_PATH'] = LAYERS
    png = os.path.join(OUT, name + '.png')
    if os.path.exists(png):
        os.replace(png, png + '.old')
    cpng = os.path.join(OUT, name + '-cmp-on.png')
    if os.path.exists(cpng):
        os.replace(cpng, cpng + '.old')
    cmd = [EXE, '--data', DATA, '--home', home, '--no-audio', '--input-script', sp,
           '--screenshot', os.path.join(OUT, name + '-fb.png'), '--window-shot', png,
           '--screenshot-after', str(case['shot']), '--exit-after', str(case['shot'] + extra_ms)]
    p = subprocess.Popen(cmd, env=e, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', cwd=OUT,
                         creationflags=0x08000000)   # CREATE_NO_WINDOW: no console of its own either
    t = threading.Thread(target=during, args=(p.pid,)) if during else None
    if t:
        t.start()
    log, _ = p.communicate()
    if t:
        t.join()
    open(os.path.join(OUT, name + '.log'), 'w').write(log)
    if case.get('cmp'):
        png = os.path.join(OUT, name + '-cmp-on.png')
    return p.returncode, png if os.path.exists(png) else None, log


class RECT(ctypes.Structure):
    _fields_ = [('l', ctypes.c_long), ('t', ctypes.c_long), ('r', ctypes.c_long), ('b', ctypes.c_long)]


def window_of(pid):
    """the largest top-level window of process pid (the game's; off-screen it is not 'visible'), 0 none"""
    u = ctypes.windll.user32
    u.EnumWindows.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    found = []
    proto = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def each(h, _):
        p = ctypes.c_ulong()
        u.GetWindowThreadProcessId(ctypes.c_void_p(h), ctypes.byref(p))
        if p.value == pid:
            r = RECT()
            u.GetWindowRect(ctypes.c_void_p(h), ctypes.byref(r))
            found.append(((r.r - r.l) * (r.b - r.t), h))
        return True
    cb = proto(each)
    u.EnumWindows(cb, None)
    return max(found)[1] if found else 0


def stretch(pid):
    """at 17 s (in play) the window made 1280 x 720, at 19 s 1700 x 1000 (the renderer keeps its size; the swap chain
    follows the window)"""
    u = ctypes.windll.user32
    t0 = time.time()
    for at, (w, h) in ((17, (1280, 720)), (19, (1700, 1000))):
        while time.time() - t0 < at:
            time.sleep(0.1)
        hw = window_of(pid)
        if hw:
            u.SetWindowPos(ctypes.c_void_p(hw), None, 0, 0, w, h, 0x0002 | 0x0004 | 0x0010)   # NOMOVE NOZORDER NOACTIVATE
            print('  window %d x %d at %d s' % (w, h, at))
        else:
            print('  no window found at %d s' % at)


def validation_of(log):
    msgs = [l for l in log.splitlines() if l.startswith('render: Vulkan error') or l.startswith('render: Vulkan warning')]
    on = 'render: Vulkan validation on' in log
    total = [l for l in log.splitlines() if 'Vulkan validation:' in l]
    return on, msgs, total


def main():
    only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else None
    os.makedirs(OUT, exist_ok=True)
    ok = True
    cases = dict(CASES)
    if '--m2' in sys.argv or '--m3' in sys.argv or '--m4' in sys.argv or '--m5' in sys.argv or '--m6' in sys.argv or '--orbs' in sys.argv:   # the scene's views (unless --only names another)
        cases = m2_cases() if not only or only not in CASES else cases
    for cname, case in cases.items():
        if only and only != cname:
            continue
        print('%s:' % cname)
        if False:   # (side by side they are not run: of two programs started at once one faulted at its start, a
            pass    # "host fault" before any renderer; D3D12 with D3D12 and D3D12 with Vulkan alike)
        else:
            _, a, _ = run(cname + '-d3d12-a', 'd3d12', case)
            _, b, _ = run(cname + '-d3d12-b', 'd3d12', case)
            rc, v, log = run(cname + '-vulkan', 'vulkan', case, validate=True)
        if not (a and b and v):
            print('  a picture is missing (d3d12 %s %s, vulkan %s)' % (bool(a), bool(b), bool(v)))
            ok = False
            continue
        masks = case.get('mask', ())
        fl = compare(a, b, masks)
        print('  noise floor (D3D12 vs D3D12): %d pixels over %d/255, largest %d, box %s; cells %s' % (fl[0], THRESHOLD, fl[1], fl[2], cells_text()))
        vk = compare(a, v, masks)
        vcells, against, tries = cells_text(), 'a', 1
        # a view whose state at the shot is one of a few (frames): Vulkan against the D3D12 run that has the same one,
        # and run again (4 at most) until it meets one; the tries are printed
        while case.get('frames') and vk[0]:
            vb = compare(b, v, masks)
            if vb[0] < vk[0]:
                vk, vcells, against = vb, cells_text(), 'b'
            if not vk[0] or tries >= 4:
                break
            tries += 1
            rc, v, log = run(cname + '-vulkan', 'vulkan', case, validate=True)
            vk, vcells, against = compare(a, v, masks), cells_text(), 'a'
        print('  Vulkan vs D3D12:              %d pixels over %d/255, largest %d, box %s; cells %s%s' %
              (vk[0], THRESHOLD, vk[1], vk[2], vcells, '; against run %s, try %d' % (against, tries) if case.get('frames') else ''))
        if masks:
            print('  masked: %s' % ', '.join('x %d-%d y %d-%d' % (m[0], m[2], m[1], m[3]) for m in masks))
        on, msgs, total = validation_of(log)
        dev = [l for l in log.splitlines() if l.startswith('render: Vulkan:')]
        print('  %s; exit %d; validation %s: %d messages %s' % (dev[0] if dev else 'no device line', rc, 'on' if on else 'OFF',
                                                               len(msgs), total[0].strip() if total else ''))
        for m in msgs[:10]:
            print('    ' + m[:300])
        ok = ok and on and not msgs and rc == 0
    if only == 'switch':
        # the full-screen row: F1, six rows down (Resume, Save, Load, New, Quit, Display: the headings are passed), Enter;
        # again 3 s later (back to the window); Esc closes the menu
        case = dict(home=SAVES, shot=26000,
                    script=RESTORE + ['20000 key f1', '20500 key down', '20700 key down', '20900 key down', '21100 key down',
                                      '21300 key down', '21500 key down', '22000 key enter', '24000 key enter', '25000 key esc'])
        print('switch (resize to 1280x720 and 1700x1000, full screen on and off):')
        rc, v, log = run('switch-vulkan', 'vulkan', case, validate=True, during=stretch)
        for l in log.splitlines():
            if l.startswith('render: Vulkan swap chain'):
                print('  ' + l)
        on, msgs, total = validation_of(log)
        print('  exit %d, picture %s; validation %s: %d messages %s' % (rc, 'written' if v else 'MISSING', 'on' if on else 'OFF',
                                                                    len(msgs), total[0].strip() if total else ''))
        for m in msgs[:10]:
            print('    ' + m[:300])
        ok = ok and on and not msgs and rc == 0 and v is not None
    print('vkcompare: %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
