# menucheck.py: our Esc menu (docs\superpowers\plans\2026-11-14-acoustics-ours.md, Task 0): legible text, the whole
# name of the language model, headings, a foot saying what a row does, and the settings that had no row. Off-screen
# runs of the test exe as fxcheck.py makes them (its run(): a fresh copy of the test home, never a visible window,
# never a sound); the renderer writes what it drew (UWVIEW_MENU_DUMP=FILE: a line a row, the widths in window
# pixels) and says each row chosen ("menu: sel N head H ..."); the options a check changes are kept in a scratch file
# (UWVIEW_SETTINGS), never in the user's data\settings.txt.
#   smooth    the menu opened with the smooth font: the font found, every value within its room, no label over its
#             value, the letters' own case, the headings, the chosen row never a heading, the panel inside the window,
#             the language model's name whole in its value (a 60-character name: both its ends drawn), the new rows
#   original  the same with menu_font original: the game's letters; the two pictures differ on the panel
#   keys      Down 60 times never rests on a heading; then each new row stepped with Right: its value changes and its
#             key is in the settings file; on the Language model row the foot gives the name whole
#   kept      a settings file with menu_font original: the menu starts in the game's letters
# Usage: python -I tools\menucheck.py [smooth|original|keys|kept ...]. Exit code: the failures.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fxcheck as fx

fx.WORK = os.path.join(fx.ROOT, 'work', 'menucheck')
os.makedirs(fx.WORK, exist_ok=True)
LONG = 'A-Very-Long-Language-Model-Name-For-The-Menu-Check-7B-Instruct-Q4_K_M'   # 69 characters
MODEL = 'X:\\none\\' + LONG + '.gguf'
HEADS = ['Game', 'Display', 'Graphics', 'Effects', 'Sound', 'Controls', 'AI']
ESC, UP, DOWN, RIGHT = 0x01, 0x148, 0x150, 0x14D
LEFT = 0x14B

def tap(t, code): return ['%d:kd@%d/0' % (t, code), '%d:ku@%d/0' % (t + 80, code)]

def menu_run(name, keys, exit_ms, env=None, shot_ms=None, settings=''):
    """a run into play (fxcheck's MENU script: about 15 s), then the window's key events; returns (lines, dump, the
    settings file's text, the window shot)"""
    dump = os.path.join(fx.WORK, name + '.dump')
    sett = os.path.join(fx.WORK, name + '.settings')
    for p in (dump,):
        if os.path.exists(p): os.remove(p)
    open(sett, 'w').write(settings)
    e = dict(UWVIEW_MENU_DUMP=dump, UWVIEW_SETTINGS=sett, UWPORT_LLM_MODEL=MODEL, UWPORT_MOUSETEST=','.join(keys))
    e.update(env or {})
    lines, shot = fx.run(name, fx.MENU, exit_ms, e, shot_ms)
    rows, panel, foot, sel = [], None, ['', ''], -1
    if os.path.exists(dump):
        for l in open(dump, encoding='latin-1').read().split('\n'):
            c = l.split('\t')
            if c[0] == 'panel': panel = dict(x=int(c[1]), y=int(c[2]), w=int(c[3]), h=int(c[4]), ww=int(c[5]), wh=int(c[6]), font=c[7], face=c[8], title=c[9])
            elif c[0] == 'row': rows.append(dict(i=int(c[1]), head=c[2] == '1', shown=c[3] == '1', label=c[4], value=c[5], drawn=c[6], vw=int(c[7]), room=int(c[8]), lw=int(c[9])))
            elif c[0] == 'sel': sel = int(c[1])
            elif c[0] == 'foot1': foot[0] = c[1] if len(c) > 1 else ''
            elif c[0] == 'foot2': foot[1] = c[1] if len(c) > 1 else ''
    return lines, dict(rows=rows, panel=panel, foot=foot, sel=sel), open(sett).read(), shot

def row(d, label):
    for r in d['rows']:
        if r['label'] == label and not r['head']: return r
    return None

def common(tag, d, font):
    p, rows = d['panel'], d['rows']
    fx.check('%s: the menu was drawn (the renderer wrote its rows)' % tag, p is not None and len(rows) > 20, 'rows %d' % len(rows))
    if not p: return False
    print('  %s: panel %dx%d at %d,%d in a %dx%d window, font %s (%s), %d rows' % (tag, p['w'], p['h'], p['x'], p['y'], p['ww'], p['wh'], p['font'], p['face'], len(rows)))
    fx.check('%s: the font is %s' % (tag, font), p['font'] == font, p['font'])
    bad = [(r['label'], r['vw'], r['room']) for r in rows if not r['head'] and (r['vw'] > r['room'] or r['room'] <= 0)]
    fx.check('%s: fits: every value within the room beside its label' % tag, not bad, str(bad[:4]))
    fx.check('%s: narrow: the panel lies inside the window' % tag, p['x'] >= 0 and p['y'] >= 0 and p['x'] + p['w'] <= p['ww'] and p['y'] + p['h'] <= p['wh'], str(p))
    heads = [r['label'] for r in rows if r['head']]
    fx.check('%s: headings: %s' % (tag, ', '.join(HEADS)), [h for h in HEADS if h in heads] == heads and len(heads) >= 5, str(heads))
    fx.check('%s: no heading is the last row or followed by a heading' % tag,
             all(not (rows[i]['head'] and (i + 1 == len(rows) or rows[i + 1]['head'])) for i in range(len(rows))))
    fx.check('%s: the row chosen at the start is not a heading' % tag, 0 <= d['sel'] < len(rows) and not rows[d['sel']]['head'], 'sel %d' % d['sel'])
    m = row(d, 'Language model')
    fx.check('%s: full name: the Language model row has the whole name, without .gguf' % tag, m is not None and m['value'] == LONG, str(m))
    if m:
        dr = m['drawn']
        print('  %s: the model as drawn: %s (%d of %d pixels)' % (tag, dr, m['vw'], m['room']))
        whole = dr.upper() == LONG.upper()
        ends = ('..' in dr) and dr[:6].upper() == LONG[:6].upper() and dr[-6:].upper() == LONG[-6:].upper()
        fx.check('%s: full name: drawn whole, or its middle left out with both ends there' % tag, whole or ends, dr)
    for lab in ('Menu text', 'Headphone 3D sound', "Effects' sides", 'Conversation text', 'Positional sound'):
        fx.check('%s: rows: "%s" is there' % (tag, lab), row(d, lab) is not None)
    fx.check('%s: the foot says what the chosen row does' % tag, d['foot'][0] != '' and d['foot'][1] != '', str(d['foot']))
    return True

def test_smooth():
    lines, d, sett, shot = menu_run('smooth', tap(20000, ESC), 26000, dict(UWVIEW_MENUFONT='smooth'), 23500)
    if not common('smooth', d, 'smooth'): return
    fx.check('smooth: the font is Segoe UI (or Tahoma where it is missing)', d['panel']['face'] in ('Segoe UI', 'Tahoma'), d['panel']['face'])
    fx.check('smooth: case: the labels are drawn in their own case ("Language model")', row(d, 'Language model') is not None)
    fx.check('smooth: the foot keeps the case', d['foot'][0][:6] == 'Resume', d['foot'][0])
    fx.check('smooth: the window\'s picture was taken', shot is not None and os.path.exists(shot))

def test_original():
    lines, d, sett, shot = menu_run('original', tap(20000, ESC), 26000, dict(UWVIEW_MENUFONT='original'), 23500)
    if not common('original', d, 'original'): return
    fx.check('original: the values are the game\'s capitals', row(d, 'Display') is not None and row(d, 'Display')['drawn'] == row(d, 'Display')['drawn'].upper())
    a = os.path.join(fx.WORK, 'smooth-w.png')
    if not (shot and os.path.exists(shot) and os.path.exists(a)):
        fx.check('original: both pictures are there (run smooth first)', False); return
    pts, w, h = fx.differ(a, shot, 40)
    p = d['panel']
    sx, sy = w / p['ww'], h / p['wh']
    x0, y0, x1, y1 = p['x'] * sx, p['y'] * sy, (p['x'] + p['w']) * sx, (p['y'] + p['h']) * sy
    inside = sum(1 for (x, y) in pts if x0 <= x < x1 and y0 <= y < y1)
    area_in = (x1 - x0) * (y1 - y0); area_out = w * h - area_in
    fin, fout = inside / area_in, (len(pts) - inside) / area_out
    print('  the two fonts\' pictures: %d pixels differ on the panel (%.1f%% of it), %d elsewhere (%.2f%%)' % (inside, fin * 100, len(pts) - inside, fout * 100))
    fx.check('original: smooth and original differ on the panel (over 2% of it)', fin > 0.02, '%.2f%%' % (fin * 100))
    fx.check('original: and hardly anywhere else (under a tenth of that share)', fout < fin / 10, '%.2f%% outside' % (fout * 100))

def test_keys():
    # the rows as a first run shows them (the same settings): where each row is
    _, d0, _, _ = menu_run('keys0', tap(20000, ESC), 24000, dict(UWVIEW_MENUFONT='smooth'))
    rows = d0['rows']
    if not rows: fx.check('keys: the menu was drawn', False); return
    selectable = [r['i'] for r in rows if not r['head']]
    def idx(label): return selectable.index(row(d0, label)['i'])
    keys, t, pos = tap(20000, ESC), 21000, selectable.index(d0['sel'])
    def go(to):
        nonlocal t, pos, keys
        while pos != to:
            keys += tap(t, DOWN); t += 200; pos = (pos + 1) % len(selectable)
    for k in range(60):
        keys += tap(t, DOWN); t += 200; pos = (pos + 1) % len(selectable)
    for lab in ('Headphone 3D sound', "Effects' sides"):
        go(idx(lab)); keys += tap(t, RIGHT); t += 400
    go(idx('Language model')); t += 600
    end1 = t
    lines, d, sett, _ = menu_run('keys', keys, end1 + 2500, dict(UWVIEW_MENUFONT='smooth'))
    sels = [l for l in lines if l.startswith('menu: sel ')]
    onhead = [l for l in sels if ' head 1 ' in l]
    seen = set(l.split()[2] for l in sels)
    print('  keys: %d rows chosen in turn, %d different, of %d that can be' % (len(sels), len(seen), len(selectable)))
    fx.check('keys: headings: Down never rests on a heading', sels and not onhead, str(onhead[:2]))
    fx.check('keys: Down went through every row that can be chosen', len(seen) >= len(selectable), '%d of %d' % (len(seen), len(selectable)))
    fx.check('keys: the row chosen is always in view', not [l for l in sels if ' shown 0 ' in l], str([l for l in sels if ' shown 0 ' in l][:2]))
    kv = dict(l.split(None, 1) for l in sett.splitlines() if len(l.split(None, 1)) == 2)
    fx.check('keys: rows: Headphone 3D sound stepped: fx_hrtf 0 kept (On -> Off)', kv.get('fx_hrtf', '').strip() == '0' and row(d, 'Headphone 3D sound')['value'] == 'Off', '%s / %s' % (kv.get('fx_hrtf'), row(d, 'Headphone 3D sound')))
    fx.check('keys: rows: Effects\' sides stepped: fx_pan_fix 0 kept', kv.get('fx_pan_fix', '').strip() == '0' and row(d, "Effects' sides")['value'] == 'As the MT-32 plays them', str(kv.get('fx_pan_fix')))
    fx.check('keys: nothing else was written', set(kv) <= {'fx_hrtf', 'fx_pan_fix'}, str(sorted(kv)))
    f1 = d['foot'][0]
    print('  keys: the foot on the Language model row: %s' % f1)
    fx.check('keys: full name: the foot gives the model\'s name whole (or both its ends)', f1.startswith('Language model: ') and (LONG in f1 or (f1[16:22] == LONG[:6] and f1[-6:] == LONG[-6:])), f1)
    # Menu text: stepped, the menu is in the game's letters at once, and the key is kept
    keys2, t, pos = tap(20000, ESC), 21000, selectable.index(d0['sel'])
    keys = keys2
    go(idx('Menu text')); keys += tap(t, RIGHT); t += 600
    lines, d, sett, _ = menu_run('keys2', keys, t + 2500, dict(UWVIEW_MENUFONT='smooth'))
    fx.check('keys: rows: Menu text stepped: the game\'s letters at once, menu_font original kept',
             d['panel'] and d['panel']['font'] == 'original' and 'menu_font original' in sett, '%s / %r' % (d['panel'] and d['panel']['font'], sett))

def test_kept():
    lines, d, sett, _ = menu_run('kept', tap(20000, ESC), 24000, None, None, 'menu_font original\nfx_hrtf 2\n')
    fx.check('kept: a settings file with menu_font original: the menu starts in the game\'s letters', d['panel'] and d['panel']['font'] == 'original', str(d['panel']))
    r = row(d, 'Headphone 3D sound')
    fx.check('kept: fx_hrtf 2 read: Headphone 3D sound is Automatic', r and r['value'] == 'Automatic', str(r))

# ---- Task 0B: the DLSS and ray tracing rows, by their names, changed during play ----
SCALE = [('DLAA', 1.0), ('Quality', 0.6667), ('Balanced', 0.58), ('Performance', 0.5), ('Ultra Performance', 1.0 / 3.0)]
def size_for(w, h, k): return max(64, int(w * k + 0.5)), max(64, int(h * k + 0.5))

class Nav:
    """the key events that walk the menu: the rows as a first run showed them (d0)"""
    def __init__(self, d0, t0=20000):
        self.sel = [r['i'] for r in d0['rows'] if not r['head']]
        self.d0, self.keys, self.t, self.pos = d0, tap(t0, ESC), t0 + 1000, self.sel.index(d0['sel'])
    def go(self, label):
        to = self.sel.index(row(self.d0, label)['i'])
        while self.pos != to:
            self.keys += tap(self.t, DOWN); self.t += 200; self.pos = (self.pos + 1) % len(self.sel)
        self.t += 300
    def press(self, code, wait=1500):
        self.keys += tap(self.t, code); self.t += wait

def mean_outside(path, p):
    w, h, px = fx.png_rgb(path)
    sx, sy = w / p['ww'], h / p['wh']
    x0, x1 = int(p['x'] * sx), int((p['x'] + p['w']) * sx)
    tot = n = 0
    for y in range(0, h, 4):
        for x in list(range(0, x0, 4)) + list(range(x1, w, 4)):
            j = (y * w + x) * 3; tot += px[j] + px[j + 1] + px[j + 2]; n += 3
    return tot / max(n, 1)

def test_dlss():
    base = dict(UWVIEW_MENUFONT='smooth')
    _, d0, _, _ = menu_run('dlss0', tap(20000, ESC), 24500, base, 23500)
    p = d0['panel']
    if not p or not row(d0, 'DLSS mode'):
        r0 = row(d0, 'DLSS Super Resolution') if p else None
        print('SKIP dlss: no DLSS mode row here (DLSS Super Resolution: %s)' % (r0 and r0['value']))
        return
    W, H = p['ww'], p['wh']
    print('  dlss: the window is %d x %d; at 2560 x 1440 the modes draw at %s' % (W, H, ', '.join('%s %d x %d' % ((n,) + size_for(2560, 1440, k)) for n, k in SCALE)))
    for lab in ('Resolution', 'DLSS Super Resolution', 'DLSS mode', 'Ray tracing', 'DLSS Ray Reconstruction'):
        fx.check('dlss: rows: "%s" is there' % lab, row(d0, lab) is not None)
    order = [r['label'] for r in d0['rows']]
    fx.check('dlss: rows in order: Resolution, Super Resolution, mode, Ray tracing, Ray Reconstruction',
             order.index('Resolution') < order.index('DLSS Super Resolution') < order.index('DLSS mode') < order.index('Ray tracing') < order.index('DLSS Ray Reconstruction'))
    fx.check('dlss: off: it starts in DLAA: "DLAA (%d x %d)" and Resolution "%d x %d -> %d x %d"' % (W, H, W, H, W, H),
             row(d0, 'DLSS mode')['value'] == 'DLAA (%d x %d)' % (W, H) and row(d0, 'Resolution')['value'] == '%d x %d -> %d x %d' % (W, H, W, H),
             '%s / %s' % (row(d0, 'DLSS mode')['value'], row(d0, 'Resolution')['value']))
    # sizes: the mode stepped through its five values, each made at once
    nv = Nav(d0)
    nv.go('DLSS mode')
    for k in range(5): nv.press(RIGHT)
    lines, d, sett, _ = menu_run('dlss-sizes', nv.keys, nv.t + 1500, base)
    got = [l.split(' = ', 1)[1] for l in lines if l.startswith('menu: sel ') and ' DLSS mode = ' in l]
    want = ['%s (%d x %d)' % ((n,) + size_for(W, H, k)) for n, k in SCALE]
    want = want + want[:1]
    print('  dlss: the mode row in turn: %s' % ' | '.join(got))
    fx.check('dlss: sizes: the mode row gives DLAA, Quality, Balanced, Performance, Ultra Performance and DLAA again, each with its size', got == want, '%s, wanted %s' % (got, want))
    made = [l for l in lines if 'ngx: DLSS created' in l]
    wantm = ['%dx%d -> %dx%d' % (size_for(W, H, k) + (W, H)) for n, k in SCALE] + ['%dx%d -> %dx%d' % (W, H, W, H)]
    gotm = [l.split('created ')[1].split(' (')[0] for l in made]
    print('  dlss: NGX made: %s' % ' | '.join(gotm))
    fx.check('dlss: sizes: NGX made Super Resolution at each size (the start\'s, then the five)', gotm == wantm[-1:] + wantm[1:], str(gotm))
    fx.check('dlss: live: no failure, the same process to its end', not [l for l in lines if 'could not be made' in l or 'evaluate failed' in l or 'create failed' in l] and fx.last_rc == 0,
             'exit %s; %s' % (fx.last_rc, [l for l in lines if 'failed' in l or 'could not' in l][:2]))
    fx.check('dlss: kept: the mode is in the settings file (back at dlaa)', 'port_dlss_quality dlaa' in sett, repr(sett))
    # live: Performance chosen, the menu shut: the picture is there, and near the DLAA picture
    nv = Nav(d0); nv.go('DLSS mode')
    for k in range(3): nv.press(RIGHT)
    nv.press(ESC, 2500)
    lines, d, sett, shot = menu_run('dlss-perf', nv.keys + tap(nv.t, ESC), nv.t + 2500, base, nv.t + 1500)
    r = row(d, 'Resolution')
    pw, ph = size_for(W, H, 0.5)
    fx.check('dlss: sizes: Resolution reads "%d x %d -> %d x %d" in Performance' % (pw, ph, W, H), r and r['value'] == '%d x %d -> %d x %d' % (pw, ph, W, H), str(r))
    a = os.path.join(fx.WORK, 'dlss0-w.png')
    if shot and os.path.exists(shot) and os.path.exists(a):
        m0, m1 = mean_outside(a, p), mean_outside(shot, p)
        print('  dlss: the picture beside the panel: mean %.1f in DLAA, %.1f in Performance' % (m0, m1))
        fx.check('dlss: live: the picture in Performance is there (not black) and as bright as DLAA\'s within a third', m1 > 4 and abs(m1 - m0) < max(m0, m1) / 3, '%.1f / %.1f' % (m0, m1))
    else: fx.check('dlss: live: both pictures were taken', False)
    fx.check('dlss: kept: port_dlss_quality performance', 'port_dlss_quality performance' in sett, repr(sett))
    # repeat: 30 changes in a row
    nv = Nav(d0); nv.go('DLSS mode')
    for k in range(30): nv.press(RIGHT, 500)
    lines, d, sett, _ = menu_run('dlss-repeat', nv.keys, nv.t + 2000, base)
    n = len([l for l in lines if 'render: DLSS mode ' in l])
    bad = [l for l in lines if 'could not be made' in l or 'evaluate failed' in l or 'create failed' in l or 'device removed' in l.lower()]
    fx.check('dlss: repeat: 30 mode changes, each made, no failure, a clean exit', n == 30 and not bad and fx.last_rc == 0, '%d made, exit %s, %s' % (n, fx.last_rc, bad[:2]))
    # Ray Reconstruction, ray tracing off, Super Resolution off
    if row(d0, 'Ray tracing')['value'] != 'On':
        print('SKIP dlss rr: ray tracing is %s here' % row(d0, 'Ray tracing')['value'])
    else:
        nv = Nav(d0); nv.go('DLSS mode'); nv.press(RIGHT)            # Quality
        nv.go('DLSS Ray Reconstruction'); nv.press(RIGHT, 3000)     # On (or Off -> On)
        was_on = row(d0, 'DLSS Ray Reconstruction')['value'] == 'On'
        if was_on: nv.press(RIGHT, 3000)
        nv.t += 1500
        lines, d, sett, _ = menu_run('dlss-rr', nv.keys, nv.t + 1500, base)
        qw, qh = size_for(W, H, 0.6667)
        rr = [l for l in lines if 'ngx: Ray Reconstruction created' in l]
        print('  dlss: %s' % (rr[-1:] or ['no "Ray Reconstruction created" line']))
        fx.check('dlss: rr: On in Quality: NGX made Ray Reconstruction at %dx%d' % (qw, qh), any('%dx%d -> %dx%d' % (qw, qh, W, H) in l for l in rr) and row(d, 'DLSS Ray Reconstruction')['value'] == 'On',
                 '%s / %s' % (rr[-1:], row(d, 'DLSS Ray Reconstruction')))
        fx.check('dlss: kept: rt_quality high', 'rt_quality high' in sett, repr(sett))
        nv = Nav(d0); nv.go('Ray tracing'); nv.press(RIGHT, 1500)
        lines, d, sett, _ = menu_run('dlss-rtoff', nv.keys, nv.t + 1500, base)
        fx.check('dlss: rr: ray tracing Off: the Ray Reconstruction row reads "Needs ray tracing"', row(d, 'Ray tracing')['value'] == 'Off' and row(d, 'DLSS Ray Reconstruction')['value'] == 'Needs ray tracing',
                 '%s / %s' % (row(d, 'Ray tracing')['value'], row(d, 'DLSS Ray Reconstruction')['value']))
    nv = Nav(d0); nv.go('DLSS Super Resolution'); nv.press(RIGHT, 1500)
    lines, d, sett, _ = menu_run('dlss-off', nv.keys, nv.t + 1500, base)
    rr = row(d, 'DLSS Ray Reconstruction')
    fx.check('dlss: off: Super Resolution Off: Resolution "%d x %d -> %d x %d", no mode row, Ray Reconstruction "Needs DLSS"' % (W, H, W, H),
             row(d, 'DLSS Super Resolution')['value'] == 'Off' and row(d, 'Resolution')['value'] == '%d x %d -> %d x %d' % (W, H, W, H) and row(d, 'DLSS mode') is None and (rr is None or rr['value'] == 'Needs DLSS'),
             '%s / %s / %s / %s' % (row(d, 'DLSS Super Resolution')['value'], row(d, 'Resolution')['value'], row(d, 'DLSS mode'), rr and rr['value']))
    # kept: a settings file with the mode: the game starts in it
    bw, bh = size_for(W, H, 0.58)
    lines, d, sett, _ = menu_run('dlss-kept', tap(20000, ESC), 24000, base, None, 'port_dlss_quality balanced\n')
    fx.check('dlss: kept: port_dlss_quality balanced read at the start: "Balanced (%d x %d)", NGX made it so' % (bw, bh),
             row(d, 'DLSS mode') and row(d, 'DLSS mode')['value'] == 'Balanced (%d x %d)' % (bw, bh) and any('ngx: DLSS created %dx%d' % (bw, bh) in l for l in lines), str(row(d, 'DLSS mode')))
    # DLSS 5's two rows, where the card has it
    if row(d0, 'DLSS 5 intensity') is None: print('SKIP dlss5: no DLSS 5 rows on this card / in this build')
    else:
        v0 = row(d0, 'DLSS 5 intensity')['value']
        nv = Nav(d0); nv.go('DLSS 5 intensity'); nv.press(RIGHT, 600) if v0 != '100%' else nv.press(0x14B, 600)
        lines, d, sett, _ = menu_run('dlss5', nv.keys, nv.t + 1500, base)
        fx.check('dlss5: intensity stepped by five (%s -> %s) and kept' % (v0, row(d, 'DLSS 5 intensity')['value']),
                 abs(int(row(d, 'DLSS 5 intensity')['value'][:-1]) - int(v0[:-1])) == 5 and 'dlss5_intensity' in sett, repr(sett))

# ---- the acoustics plan's Task 4: the acoustics' rows (every part of it can be turned off from the menu) ----
def test_acoustics_rows():
    DIALS = ('Reverb level', 'Room size', 'Muffling by walls', 'Sound round corners')
    _, d0, _, _ = menu_run('ac0', tap(20000, ESC), 24000, dict(UWVIEW_MENUFONT='smooth'))
    r = row(d0, 'Acoustics')
    fx.check('acoustics rows: off by default: the Acoustics row says Off', bool(r) and r['value'] == 'Off', str(r))
    fx.check('acoustics rows: off: its three dials are not shown', not [l for l in DIALS if row(d0, l)], str([l for l in DIALS if row(d0, l)]))
    # on: the three rows with their defaults; Right on the first two, Left on the third; each kept in settings.txt
    _, d1, _, _ = menu_run('ac1', tap(20000, ESC), 24000, dict(UWVIEW_MENUFONT='smooth'), None, 'fx_acoustics 1\n')
    vals = {l: (row(d1, l) or {}).get('value') for l in ('Acoustics',) + DIALS}
    print('  acoustics rows: %s' % vals)
    fx.check('acoustics rows: on: Acoustics On, Reverb level 100%, Room size 2.4 m a tile, Muffling by walls 100%',
             vals == {'Acoustics': 'On', 'Reverb level': '100%', 'Room size': '2.4 m a tile', 'Muffling by walls': '100%', 'Sound round corners': 'On'}, str(vals))
    if not all(row(d1, l) for l in DIALS): return
    selectable = [x['i'] for x in d1['rows'] if not x['head']]
    keys, t, pos = tap(20000, ESC), 21000, selectable.index(d1['sel'])
    for lab, key in zip(DIALS, (RIGHT, RIGHT, LEFT, RIGHT)):
        to = selectable.index(row(d1, lab)['i'])
        while pos != to:
            keys += tap(t, DOWN); t += 200; pos = (pos + 1) % len(selectable)
        keys += tap(t, key); t += 400
    lines, d, sett, _ = menu_run('ac2', keys, t + 2500, dict(UWVIEW_MENUFONT='smooth'), None, 'fx_acoustics 1\n')
    kv = dict(l.split(None, 1) for l in sett.splitlines() if len(l.split(None, 1)) == 2)
    vals = {l: (row(d, l) or {}).get('value') for l in DIALS}
    print('  acoustics rows: stepped: %s; settings %s' % (vals, {k: v for k, v in kv.items() if k.startswith('acoustics')}))
    fx.check('acoustics rows: Reverb level stepped: 110%, acoustics_wet 110 kept', vals['Reverb level'] == '110%' and kv.get('acoustics_wet', '').strip() == '110', str(kv))
    fx.check('acoustics rows: Room size stepped: 2.5 m a tile, acoustics_tile_m 2.5 kept', vals['Room size'] == '2.5 m a tile' and kv.get('acoustics_tile_m', '').strip() == '2.5', str(kv))
    fx.check('acoustics rows: Muffling by walls stepped: 90%, acoustics_occlusion 90 kept', vals['Muffling by walls'] == '90%' and kv.get('acoustics_occlusion', '').strip() == '90', str(kv))
    fx.check('acoustics rows: Sound round corners stepped: Off, acoustics_detour 0 kept', vals['Sound round corners'] == 'Off' and kv.get('acoustics_detour', '').strip() == '0', str(kv))
    fx.check('acoustics rows: nothing else was written', set(kv) <= {'fx_acoustics', 'acoustics_wet', 'acoustics_tile_m', 'acoustics_occlusion', 'acoustics_detour'}, str(sorted(kv)))
    print('  acoustics rows: the foot on Sound round corners: %s | %s' % (d['foot'][0], d['foot'][1]))
    fx.check('acoustics rows: the row chosen has its foot line', 'Sound round corners' in d['foot'][0] + d['foot'][1] and 'opening' in d['foot'][0] + d['foot'][1], str(d['foot']))
    # Task 6's row: with Positional sound on, "Footsteps by floor" (On by default); stepped: Off, fx_footsteps 0 kept
    _, d4, _, _ = menu_run('ac4', tap(20000, ESC), 24000, dict(UWVIEW_MENUFONT='smooth'), None, 'fx_audio 1\n')
    r4 = row(d4, 'Footsteps by floor')
    fx.check('acoustics rows: Footsteps by floor is shown with Positional sound on, On by default; not without it',
             bool(r4) and r4['value'] == 'On' and not row(d0, 'Footsteps by floor'), '%s / %s' % (r4, row(d0, 'Footsteps by floor')))
    if r4:
        sel4 = [x['i'] for x in d4['rows'] if not x['head']]
        keys, t, pos = tap(20000, ESC), 21000, sel4.index(d4['sel'])
        while pos != sel4.index(r4['i']):
            keys += tap(t, DOWN); t += 200; pos = (pos + 1) % len(sel4)
        keys += tap(t, RIGHT); t += 400
        _, d5, sett5, _ = menu_run('ac5', keys, t + 2500, dict(UWVIEW_MENUFONT='smooth'), None, 'fx_audio 1\n')
        fx.check('acoustics rows: Footsteps by floor stepped: Off, fx_footsteps 0 kept',
                 (row(d5, 'Footsteps by floor') or {}).get('value') == 'Off' and 'fx_footsteps 0' in sett5, '%s / %r' % (row(d5, 'Footsteps by floor'), sett5))
    # Task 7's value: the Acoustics row goes Off -> On -> High -> Off; from On a Right press gives High, fx_acoustics 2 kept
    sel1 = [x['i'] for x in d1['rows'] if not x['head']]
    keys, t, pos = tap(20000, ESC), 21000, sel1.index(d1['sel'])
    while pos != sel1.index(row(d1, 'Acoustics')['i']):
        keys += tap(t, DOWN); t += 200; pos = (pos + 1) % len(sel1)
    keys += tap(t, RIGHT); t += 400
    _, d6, sett6, _ = menu_run('ac6', keys, t + 2500, dict(UWVIEW_MENUFONT='smooth'), None, 'fx_acoustics 1\n')
    fx.check('acoustics rows: Acoustics stepped from On: High, fx_acoustics 2 kept; its dials still shown',
             (row(d6, 'Acoustics') or {}).get('value') == 'High' and 'fx_acoustics 2' in sett6 and bool(row(d6, 'Reverb level')), '%s / %r' % (row(d6, 'Acoustics'), sett6))
    # each part can be turned off from the menu: the dials at 0 read Off
    _, d3, _, _ = menu_run('ac3', tap(20000, ESC), 24000, dict(UWVIEW_MENUFONT='smooth'), None, 'fx_acoustics 1\nacoustics_wet 0\nacoustics_occlusion 0\n')
    vals = {l: (row(d3, l) or {}).get('value') for l in DIALS}
    fx.check('acoustics rows: acoustics_wet 0 and acoustics_occlusion 0 read Off (each part switched off alone)',
             vals['Reverb level'] == 'Off' and vals['Muffling by walls'] == 'Off', str(vals))

def test_texture_rows():
    """the walls' and floors' rows (the user, 2026-11-14): Normal maps, Normal map strength, Joint shadows (AO), Relief
    shadows; the two new ones read their settings, step and are kept; and they work with the normal maps off"""
    LABS = ('Normal maps (F6)', 'Normal map strength', 'Joint shadows (AO)', 'Relief shadows')
    base = 'texture_ao_strength 1\ntexture_shadow 3\n'
    _, d0, _, _ = menu_run('tx0', tap(20000, ESC), 24000, dict(UWVIEW_MENUFONT='smooth'), None, base)
    vals = {l: (row(d0, l) or {}).get('value') for l in LABS}
    print('  texture rows: %s' % vals)
    fx.check('texture rows: all four there; Joint shadows (AO) 100%, Relief shadows Depth 3',
             all(vals.values()) and vals['Joint shadows (AO)'] == '100%' and vals['Relief shadows'] == 'Depth 3', str(vals))
    if not (row(d0, 'Joint shadows (AO)') and row(d0, 'Relief shadows')): return
    sel = [x['i'] for x in d0['rows'] if not x['head']]
    keys, t, pos = tap(20000, ESC), 21000, sel.index(d0['sel'])
    for lab, key in (('Joint shadows (AO)', RIGHT), ('Relief shadows', LEFT)):
        while pos != sel.index(row(d0, lab)['i']):
            keys += tap(t, DOWN); t += 200; pos = (pos + 1) % len(sel)
        keys += tap(t, key); t += 400
    _, d1, sett, _ = menu_run('tx1', keys, t + 2500, dict(UWVIEW_MENUFONT='smooth'), None, base)
    kv = dict(l.split(None, 1) for l in sett.splitlines() if len(l.split(None, 1)) == 2)
    vals = {l: (row(d1, l) or {}).get('value') for l in LABS[2:]}
    print('  texture rows: stepped: %s; settings %s' % (vals, kv))
    fx.check('texture rows: AO stepped right: 125%, texture_ao_strength 1.25 kept',
             vals['Joint shadows (AO)'] == '125%' and kv.get('texture_ao_strength', '').strip() == '1.25', str(kv))
    fx.check('texture rows: Relief shadows stepped left: Depth 2, texture_shadow 2 kept',
             vals['Relief shadows'] == 'Depth 2' and kv.get('texture_shadow', '').strip() == '2', str(kv))
    _, d2, _, _ = menu_run('tx2', tap(20000, ESC), 24000, dict(UWVIEW_MENUFONT='smooth'), None, 'texture_ao_strength 0\ntexture_shadow 0\n')
    vals = {l: (row(d2, l) or {}).get('value') for l in LABS[2:]}
    fx.check('texture rows: 0 reads Off on both', vals == {'Joint shadows (AO)': 'Off', 'Relief shadows': 'Off'}, str(vals))
    # the picture with the normal maps off, held still (uwview's comparison "texture"): AO + relief shadows at their
    # most, then none; the same with both at 0 gives the noise of the held view (each its own switch)
    res = {}
    for name, ao, sh in (('txon', '2', '8'), ('txnone', '0', '0')):
        # (the project's data: the test build's own data folder has no Laigter maps)
        on, off, _ = fx.compare(name, fx.MENU, 30000, 26000, 'texture', dict(UWVIEW_DATA=os.path.join(fx.ROOT, 'data'), UWVIEW_NORMALS='0', UWVIEW_TEXTURE_AO=ao, UWVIEW_TEXTURE_SHADOW=sh))
        res[name] = len(fx.differ(on, off, 12)[0]) if os.path.exists(on) and os.path.exists(off) else None
    print('  texture rows: normal maps off, held view: %s pixels differ with AO + shadows on/off, %s with both at 0' % (res['txon'], res['txnone']))
    fx.check('texture rows: with the normal maps off, AO and relief shadows still change the picture',
             res['txon'] is not None and res['txnone'] is not None and res['txon'] > max(500, 5 * res['txnone']), str(res))

TESTS = {'textures': test_texture_rows, 'acoustics': test_acoustics_rows, 'smooth': test_smooth, 'original': test_original, 'keys': test_keys, 'kept': test_kept, 'dlss': test_dlss}

if __name__ == '__main__':
    for n in sys.argv[1:] or list(TESTS):
        TESTS[n]()
    print('menucheck: %d failure(s)' % fx.fails)
    sys.exit(fx.fails)
