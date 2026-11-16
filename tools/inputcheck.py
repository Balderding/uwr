# inputcheck.py: the input rewrite's checks (docs\superpowers\specs\2026-11-12-input-rewrite-design.md). Off-screen
# runs of the test exe (underworld-exhumed\uw1\build\port\uw1port.exe) from a fresh copy of the test home work\in2-4,
# each asserted from its stderr (UWPORT_LOOKDBG on: the backend's "screen N at MS ms" lines and the game's
# "mouse-look on|off (want)" lines, ui\MOUSE.C).
#   look  mouse-look is the mouse's own mode: the menu restore, a death, the restore again. No ` is typed by the
#         backend (no "follower" line); each entry to PS_GAME (screen 2) is followed by exactly one "mouse-look on"
#         before the next screen change; PS_DEATH (screen 7) by "mouse-look off" before the next screen change.
#   trace the key trace comparison (the spec's proof that gameplay does not change): one script in play using the
#         controls of our key table once each and the clicks (left tap, left drag, right click, C's attack held and let
#         go, Z held through its sleep and let go after), run on the old build (work\inputrw\old: b9ba607's backend with
#         the same trace, tools' "kd/ku" test events) and on the new one with UWPORT_INPUTTRACE=1: every byte and
#         pointer event the window's thread gives the game ("in: ..."). The two sequences equal once the old build's `
#         bytes (29, A9: its mouse-look follower) are taken out, but for the old build's split press (the map's
#         Esc: its release swallowed); each difference printed; no key left held in the game ("key_on" lines). Left out: Ctrl+L (the live
#         keyboard's Ctrl, GetKeyState, cannot be made by a test), P and Shift+P (they write the user's clipboard, a
#         video; neither sends the game anything in either build).
#   charge  the Space of a fight-mode left press held into the death (ruling 5): C, the left button held, cheat die,
#         the button let go during PS_DEATH; then a new character (Create Character, the default choices, name "a")
#         and a right click in play. The Space's up reaches the game at the death (in the trace), no "key_on: 0x39
#         held" line (port\sys\keyqueue.c), and the right click's use (e: 12, 92) reaches the game after.
#   sleep   a new character (Create Character, the defaults, "a"), Z held 9 s: one dream (one PS_CUT_FULL), play again;
#         the old build's result printed as evidence.
#   display our display never shows the original's 3D-screen picture in PS_GAME (the design's "The display"): a level
#         change (cheat goto 2 28 28, then back: cheat goto 1 33 19), a conversation (cheat talk 243, answered
#         with 1 until it ends), the map (M, then Esc, through the window's input). No "display: original picture
#         in PS_GAME" line (one per frame composed so); the "display: kept our last frame (why)" lines printed
#         (at least one expected: the frames after the conversation and the map do not match).
#   original  the ` key: in the original's display (display=original) the game's toggle, left alone by the backend
#         ("no wish"): on, and on to the end, no "(want)" line; in our display it reaches the game not at all.
#   focusheld  the left button held when the focus is lost (a pending take, a thing carried, a throw from our panel):
#         the early release takes the let-go path: no look, no throw, no packing, nothing left held (see test_focusheld).
#   ring    the keyboard's ring (port\sys\keyqueue.c): W held, our Esc menu open 20 s over it, closed, W let go: no ring
#         overrun line, no "key_on: 0x11 held", and the player stops (the test cheat where's position, from 2 s after).
#   fightwalk  C tapped six times while W walks: each toggles fight mode (our taps held until the game takes them).
#   visible  (its own subcommand: inputcheck.py visible [play menu panel conv death newchar alttab sleep]) the real
#         capture in shown runs on the user's screen; refused while uw-new.exe runs. See "visible" below.
# Usage: python -I tools\inputcheck.py [look trace charge sleep display original ring focusheld levelfail fightwalk ...] (all but visible when none);
#        python -I tools\inputcheck.py visible [scenario ...]. Exit code: the failures.
import os, sys, re, shutil, subprocess, difflib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port', 'uw1port.exe')
OLD_EXE = os.path.join(ROOT, 'work', 'inputrw', 'old', 'uw1port.exe')
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'inputcheck')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"
# after the death the start menu is up: the same restore without the Introduction's Escapes
MENU2 = "33000 key end\n33300 key enter\n36000 key home\n36600 key enter\n38000 key enter\n"

def run(name, script, exit_ms, env, exe=EXE, visible=False, cfg=''):
    home = os.path.join(WORK, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f:
        f.write('settings-at-start=0\n')
        # (visible: the first start's language-model picker, our menu 3 s into play, is not opened: off-screen it never
        # is; llm=0 as a player who turned the model off)
        if visible: f.write('llm=0\n')
        f.write(cfg)
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', UWPORT_LOOKDBG='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', **env)
    if visible: del e['UWPORT_OFFSCREEN']   # (visible: the window shown on the user's screen, the real ClipCursor)
    cmd = [exe, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio', '--input-script', sp,
           '--exit-after', str(exit_ms)]
    r = subprocess.run(cmd, env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    return r.stderr.splitlines()

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''))
    if not ok: fails += 1

SCREEN = re.compile(r': screen (\d+) at')
WANT = re.compile(r'mouse-look (on|off) \(want\)')

def events(err):
    """the screen changes and the game's applied wants, in order: ('screen', N) / ('want', 'on'|'off')"""
    ev = []
    for l in err:
        m = SCREEN.search(l)
        if m: ev.append(('screen', int(m.group(1)))); continue
        m = WANT.search(l)
        if m: ev.append(('want', m.group(1)))
    return ev

def spans(ev, ps):
    """for each entry to screen ps: the wants applied before the next screen change"""
    out = []
    for i, (k, v) in enumerate(ev):
        if k == 'screen' and v == ps:
            w = []
            for k2, v2 in ev[i + 1:]:
                if k2 == 'screen': break
                w.append(v2)
            out.append(w)
    return out

def test_look():
    err = run('look', MENU + "18000 cheat die\n" + MENU2, 46000, dict(UWPORT_FOCUSTEST='1000,1500'))
    fol = [l for l in err if 'follower' in l]
    check('look: no ` typed by the backend (no "follower" line)', not fol, '%d lines, first: %s' % (len(fol), fol[:1]))
    ev = events(err)
    games = spans(ev, 2)
    check('look: PS_GAME entered twice (the restore, the restore after the death)', len(games) >= 2,
          'screens %s' % [v for k, v in ev if k == 'screen'])
    check('look: each PS_GAME entry followed by exactly one "mouse-look on" (and no off) before the next screen',
          games and all(w == ['on'] for w in games), 'wants per PS_GAME entry: %s' % games)
    deaths = spans(ev, 7)
    check('look: PS_DEATH followed by "mouse-look off" first, before the next screen', deaths and all(w[:1] == ['off'] for w in deaths),
          'wants per PS_DEATH entry: %s' % deaths)

# the trace's events (UWPORT_MOUSETEST: wall-clock ms from the start; kd / ku @CODE/0 a key, its set-1 code in decimal
# with 256 for E0; ld / lu / rd / ru the buttons at the window's middle; raw DX/DY a captured mouse's motion; sd / su Shift)
def keytap(t, code, hold=100):
    return ['%d:kd@%d/0' % (t, code), '%d:ku@%d/0' % (t + hold, code)]

def trace_events():
    ev = []
    ev += keytap(16000, 0x11, 400)                     # W: walk
    ev += keytap(16800, 0x148, 400)                    # Up: walk
    ev += keytap(17600, 0x150, 400)                    # Down: back
    ev += keytap(18400, 0x149, 400)                    # PageUp: look up
    ev += keytap(19200, 0x151, 400)                    # PageDown: look down
    ev += keytap(20000, 0x39, 200)                     # Space: jump
    ev += keytap(21000, 0x26)                          # L: look
    ev += ['22000:sd'] + keytap(22100, 0x26) + ['22300:su']   # Shift+L: full bright
    ev += keytap(23000, 0x10)                          # Q: cast
    ev += keytap(24000, 0x25)                          # K: track
    ev += keytap(25000, 0x3F) + keytap(26000, 0x01) + keytap(26600, 0x01)   # F5: our save page; Esc back; Esc closed
    ev += keytap(27500, 0x43) + keytap(28200, 0x01) + keytap(28800, 0x01)   # F9: our load page
    ev += keytap(29500, 0x34)                          # .: shadows
    ev += ['30000:sd'] + keytap(30100, 0x18) + ['30300:su']   # Shift+O: shadows
    ev += keytap(31000, 0x28) + keytap(31500, 0x27) + keytap(32000, 0x1B) + keytap(32500, 0x40)   # ' ; ] F6
    ev += keytap(33000, 0x17) + keytap(34000, 0x17)    # I: our panel open, closed
    ev += keytap(35000, 0x0F) + keytap(36000, 0x0F)    # Tab: the same
    ev += keytap(37000, 0x57) + keytap(38500, 0x57)    # F11: the settings screen open, closed
    ev += keytap(39500, 0x01) + keytap(40500, 0x01)    # Esc: our menu open, closed
    ev += ['45000:ld', '45150:lu']                     # a left tap
    ev += ['46500:ld', '46600:raw 40/0', '46700:raw 40/0', '46800:raw 40/0', '47000:lu']   # a left drag
    ev += ['48500:rd', '48600:ru']                     # a right click
    ev += keytap(50000, 0x2E) + ['52000:ld', '53000:lu'] + keytap(54500, 0x2E)   # C: fight mode, a blow charged, C again
    # R, then , : the interface (interface_toggle) and their taps; each given back by our menu's Display row twice
    # (Esc, Down, Enter: the original's, Enter: ours again, which ends the interface; Esc)
    back = lambda t: keytap(t, 0x01) + keytap(t + 500, 0x150) + keytap(t + 1000, 0x1C) + keytap(t + 1600, 0x1C) + keytap(t + 2200, 0x01)
    ev += keytap(55500, 0x13) + back(56500)            # R: the runes
    ev += keytap(59500, 0x33) + back(60500)            # ,: the character
    ev += keytap(63500, 0x32) + keytap(65500, 0x01)    # M: the map; Esc (after the fight: the old build's Esc split)
    ev += ['67000:kd@%d/0' % 0x2C, '101000:ku@%d/0' % 0x2C]   # Z: held (this character dies asleep), let go after
    return ','.join(ev)

IN = re.compile(r'^in: (key [0-9A-F]{2}|ptr .*)$')

def in_lines(err, drop_grave):
    out = []
    for l in err:
        m = IN.match(l)
        if not m: continue
        if drop_grave and m.group(1) in ('key 29', 'key A9'): continue
        out.append(m.group(1))
    return out

def test_trace():
    env = dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_INPUTTRACE='1', UWPORT_MOUSETEST=trace_events())
    old = run('trace-old', MENU, 108000, env, OLD_EXE)
    new = run('trace-new', MENU, 108000, env)
    a, b = in_lines(old, True), in_lines(new, False)
    graves = len(in_lines(old, False)) - len(a)
    print('trace: old %d events (%d ` bytes taken out), new %d' % (len(a), graves, len(b)))
    diff = [d for d in difflib.unified_diff(a, b, 'old', 'new', n=2, lineterm='')]
    for d in diff: print('  ' + d)
    # the differences expected, each a press the old build split (its release sent to another owner than its press):
    # - the map's Esc: the down went to the game (on the map), the map closed, and the old menu_key swallowed the
    #   release (the 3D view live again: "Esc opens our menu"), so the game's key_on[01] stayed set for the rest of the
    #   run ("key_on: 0x01 held"; before the map was moved after the fight it also kept C's fight mode from coming
    #   on). The new build sends the release (81) to the owner of the press, the game.
    # - R and ,: our_keys took the press (interface_toggle, then the tap 13 93 / 21 A1); the interface was then up,
    #   so the old build sent the key's own release to the game (an extra 93 after R's tap, a B3 with no 33 before it
    #   after , 's). The new build's release goes to the press's owner (ours: nothing).
    unexplained = []
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        if tag == 'equal': continue
        if tag == 'insert' and b[j1:j2] == ['key 81'] and j1 > 0 and b[j1 - 1] == 'key 01': continue
        if tag == 'delete' and a[i1:i2] == ['key 93'] and a[i1 - 2:i1] == ['key 13', 'key 93']: continue
        if tag == 'delete' and a[i1:i2] == ['key B3'] and a[i1 - 2:i1] == ['key 21', 'key A1']: continue
        unexplained.append((tag, a[i1:i2], b[j1:j2]))
    check('trace: the in: sequences equal (the old one\'s ` bytes out) but for the old build\'s split presses (the map\'s Esc, R, ,)',
          not unexplained, 'unexplained: %s' % unexplained)
    check('trace: the new build sends no ` (29, A9)', not [l for l in in_lines(new, False) if l in ('key 29', 'key A9')])
    held = [l for l in new if l.startswith('key_on:')]
    print('trace: "key_on: ... held" lines: old %d, new %d' % (len([l for l in old if l.startswith('key_on:')]), len(held)))
    check('trace: the new build leaves no key held in the game (no "key_on: 0xNN held": the old one held Z\'s F10 '
          'through the death into the menu and the map\'s Esc for good)', not held, str(held[:3]))

# a new character (it sleeps without dying): the start menu's Create Character, the default choices, the name "a";
# then Z held 9 s (the controller's work\death2\z2.txt: the old dist exe slept, dreamed and slept again: screen 6 at
# 41, 64 and 83 s)
SLEEP = ("3000 key esc\n5000 key esc\n7000 key esc\n9000 key home\n9300 key right\n9600 key enter\n" +
         ''.join('%d key enter\n' % (13000 + 1200 * i) for i in range(12)) +
         "27400 key a\n27900 key enter\n29400 key enter\n41400 down z\n50400 up z\n")

def sleep_screens(err):
    ev = []
    for l in err:
        m = SCREEN.search(l)
        if m: ev.append(int(m.group(1)))
        if l == 'in: key 44' and 'Z' not in ev: ev.append('Z')   # (Z's press: the game's F10 down)
    return ev

def test_sleep():
    """Z held into the dream: one sleep (one screen 6, PS_CUT_FULL), then play again (screen 2)"""
    env = dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_INPUTTRACE='1')
    for name, exe in (('sleep-old', OLD_EXE), ('sleep-new', EXE)):
        err = run(name, SLEEP, 110000, env, exe)
        ev = sleep_screens(err)
        z = ev.index('Z') if 'Z' in ev else len(ev)
        after = [v for v in ev[z:] if v != 'Z']
        cuts = after.count(6)
        held = len([l for l in err if l.startswith('key_on: 0x44')])
        what = 'screens after the Z: %s, "key_on: 0x44 held" lines %d' % (after, held)
        if name == 'sleep-old':
            print('sleep (the old build, evidence only): %s' % what)
            continue
        check('sleep: Z held: exactly one dream (one screen 6 after the Z press), then play again (screen 2)',
              cuts == 1 and 6 in after and 2 in after[after.index(6):], what)

def test_charge():
    # the menu restore; C, the left button held from 17 s; the death; the button let go at 26 s (PS_DEATH: the skulls);
    # the start menu: Create Character, twelve Enters for the default choices, the name "a", Enter, Enter; a right click
    mt = ['16000:kd@46/0', '16100:ku@46/0', '17000:ld', '26000:lu', '%d:rd' % 95000, '%d:ru' % 95150]
    script = MENU + "21000 cheat die\n37000 key home\n37300 key right\n37600 key enter\n"
    t = 40000
    for i in range(12): script += '%d key enter\n' % t; t += 1200
    script += '%d key a\n%d key enter\n%d key enter\n' % (t, t + 600, t + 1800)
    err = run('charge', script, 100000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_INPUTTRACE='1', UWPORT_MOUSETEST=','.join(mt)))
    screens = [int(m.group(1)) for m in (SCREEN.search(l) for l in err) if m]
    check('charge: PS_GAME, PS_DEATH, the start menu, character creation, PS_GAME again', screens[:1] and 7 in screens and
          9 in screens and screens[-1:] == [2], 'screens %s' % screens)
    fight = any('fight mode on asked' in l for l in err)
    check('charge: fight mode asked (C)', fight)
    # the Space's down after the press and its up before the new game
    seq = []
    for l in err:
        m = IN.match(l)
        if m: seq.append(m.group(1))
        m = SCREEN.search(l)
        if m: seq.append('screen %s' % m.group(1))
    try:
        d = seq.index('key 39'); u = seq.index('key B9', d)
        between = seq[d:u]
        ok = 'screen 7' in between and 'screen 9' not in between
    except ValueError:
        ok, between = False, []
    check('charge: the attack\'s Space down, its up at the death (in PS_DEATH, before the new character)', ok,
          'from the 39: %s' % between[:12])
    held = [l for l in err if 'key_on: 0x39 held' in l]
    check('charge: no "key_on: 0x39 held" (the game took no Space as held)', not held, '%d lines, first %s' % (len(held), held[:1]))
    last2 = len(seq) - 1 - seq[::-1].index('screen 2') if 'screen 2' in seq else len(seq)
    after = seq[last2:]
    check('charge: the right click in the new game reaches the game as its use (e: 12, 92)',
          'key 12' in after and 'key 92' in after and after.count('key 39') == 0, 'after the last PS_GAME: %s' % after[:20])

DISPLAY = (MENU + "18000 cheat goto 2 28 28\n24000 cheat goto 1 33 19\n26000 cheat talk 243\n" +
           ''.join('%d key 1\n' % t for t in range(29000, 45000, 1500)))

# the ` key (0x29): in the original's display (display=original) the game's own toggle, which the backend leaves alone
# (it publishes "no wish"): mouse-look on stays on; in our display it reaches the game not at all (the module swallows
# it: the want rules there)
def test_original():
    mt = '18000:kd@41/0,18100:ku@41/0'
    err = run('original', MENU, 24000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_INPUTTRACE='1', UWPORT_MOUSETEST=mt),
              cfg='display=original\n')
    t = next((i for i, l in enumerate(err) if 'mouse test at' in l and ': kd 41' in l), None)
    after = err[t:] if t is not None else []
    caps = [l for l in after if "mouse-look's capture" in l]
    wants = [l for l in after if '(want)' in l]
    for l in caps + wants: print('  ' + l)
    check('original: ` reaches the game (in: key 29)', 'in: key 29' in after)
    check('original: ` turns mouse-look on and it stays on to the end (6 s; capture on, no off)',
          len(caps) == 1 and 'capture on' in caps[0], str(caps))
    check('original: no "(want)" line after the ` (the backend has no wish in the original\'s display)', not wants, str(wants[:3]))
    nw = [l for l in err if 'no wish' in l]
    check('original: the backend says "no wish" in the original\'s display', nw, '')
    err = run('grave-ours', MENU, 24000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_INPUTTRACE='1', UWPORT_MOUSETEST=mt))
    t = next((i for i, l in enumerate(err) if 'mouse test at' in l and ': kd 41' in l), None)
    after = err[t:] if t is not None else []
    caps = [l for l in after if "mouse-look's capture" in l]
    check('original (our display): ` sends the game nothing (no in: key 29 / A9)', t is not None and not [l for l in after if l in ('in: key 29', 'in: key A9')])
    check('original (our display): mouse-look stays on (no capture line after the `)', t is not None and not caps, str(caps))
    # the switch at run time: in play in our display (mouse-look on by the want), Esc, Down, Enter (the Display row:
    # the original's), Esc; then ` twice. The want's mouse-look is turned off first (a "mouse-look off (want)" line,
    # the capture off), then the backend has no wish and ` toggles freely (on, then off)
    mt = ','.join(['18000:kd@1/0', '18100:ku@1/0', '18600:kd@336/0', '18700:ku@336/0', '19200:kd@28/0', '19300:ku@28/0',
                   '20000:kd@1/0', '20100:ku@1/0', '23000:kd@41/0', '23100:ku@41/0', '26000:kd@41/0', '26100:ku@41/0'])
    err = run('switch', MENU, 29000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_INPUTTRACE='1', UWPORT_MOUSETEST=mt))
    t = next((i for i, l in enumerate(err) if 'mouse test at' in l and ': kd 1 ' in l), None)
    after = err[t:] if t is not None else []
    g = next((i for i, l in enumerate(after) if 'mouse test at' in l and ': kd 41' in l), len(after))
    sw, tog = after[:g], after[g:]
    ev = [l for l in after if "capture" in l or '(want)' in l or 'no wish' in l or 'in: key 29' in l or ': kd 41' in l]
    for l in ev: print('  ' + l)
    check('switch: before the first `: "mouse-look off (want)", the capture off, then "no wish"',
          any('mouse-look off (want)' in l for l in sw) and any("capture off" in l for l in sw) and any('no wish' in l for l in sw))
    caps = [l.split("capture ")[1].split(' ')[0] for l in tog if "mouse-look's capture" in l]
    check('switch: then ` toggles freely: capture on at the first, off at the second', caps == ['on', 'off'], str(caps))
    check('switch: both ` reach the game and no "(want)" line after them',
          tog.count('in: key 29') == 2 and not [l for l in tog if '(want)' in l], str([l for l in tog if '(want)' in l][:3]))

def test_display():
    mt = ['50000:kd@50/0', '50100:ku@50/0', '53000:kd@1/0', '53100:ku@1/0']   # M: the map; Esc: closed
    err = run('display', DISPLAY, 58000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_MOUSETEST=','.join(mt)))
    screens = [int(m.group(1)) for m in (SCREEN.search(l) for l in err) if m]
    lv = [l for l in err if 'cheat: goto' in l]
    check('display: the level change and back (two "cheat: goto" lines)', len(lv) == 2, str(lv))
    seq = [v for i, v in enumerate(screens) if i == 0 or screens[i - 1] != v]
    ok = 4 in seq and 3 in seq and 2 in seq[seq.index(4):] and 2 in seq[seq.index(3):]
    check('display: a conversation (screen 4) and the map (screen 3), each back to PS_GAME (screen 2)', ok, 'screens %s' % seq)
    orig = [l for l in err if 'display: original picture in PS_GAME' in l]
    kept = [l for l in err if 'display: kept our last frame' in l]
    print('display: %d "original picture in PS_GAME" lines, %d "kept our last frame" lines' % (len(orig), len(kept)))
    for l in (orig + kept)[:20]: print('  ' + l)
    check('display: no frame of the original\'s picture in PS_GAME in our display', not orig, '%d lines, first: %s' % (len(orig), orig[:1]))
    check('display: our last frame kept where a frame did not match ("kept our last frame" lines)', kept)
    # a new game after a death: the death, Create Character (the defaults, the name "a") and the first seconds of play.
    # The last game's frame is never kept into the new one: no "kept our last frame" line between the new PS_GAME and
    # our first view of the new game ("display: our first view"); none of the original's picture either
    script = MENU + "18000 cheat die\n34000 key home\n34300 key right\n34600 key enter\n"
    t = 37000
    for i in range(12): script += '%d key enter\n' % t; t += 1200
    script += '%d key a\n%d key enter\n%d key enter\n' % (t, t + 600, t + 1800)
    err = run('display-new', script, t + 14000, dict(UWPORT_FOCUSTEST='1000,1500'))
    ev = [l for l in err if SCREEN.search(l) or 'display:' in l]
    last2 = max([i for i, l in enumerate(ev) if SCREEN.search(l) and SCREEN.search(l).group(1) == '2'] or [-1])
    seen = [int(SCREEN.search(l).group(1)) for l in ev if SCREEN.search(l)]
    check('display (new game): PS_DEATH, character creation (screen 9), then PS_GAME', 7 in seen and 9 in seen and seen[-1:] == [2] and last2 >= 0,
          'screens %s' % seen)
    after = ev[last2 + 1:] if last2 >= 0 else []
    first = next((i for i, l in enumerate(after) if 'display: our first view' in l), None)
    early = [l for l in (after[:first] if first is not None else after) if 'kept our last frame' in l]
    for l in after[:8]: print('  ' + l)
    check('display (new game): our first view of the new game shown ("display: our first view" after the last PS_GAME)', first is not None)
    check('display (new game): no "kept our last frame" between the new PS_GAME and our first view', not early, str(early[:3]))
    orig = [l for l in err if 'display: original picture in PS_GAME' in l]
    check('display (new game): no frame of the original\'s picture in PS_GAME', not orig, '%d lines, first: %s' % (len(orig), orig[:1]))

# the keyboard's ring (Task 6, port\sys\keyqueue.c: a ring that cannot overrun): W held in play, our Esc menu open 20 s
# over it, closed, W let go. No "keyqueue: ring overrun" line, no "key_on: 0x11 held" line, and the player stops: the
# position (the test cheat where: PN's x, y, z, heading) changes while W is held before the menu, and no longer from 2 s
# after the release. Any "keyqueue: ring full, N bytes queued" line is printed as evidence.
WHERE = re.compile(r'^where: x (-?\d+) y (-?\d+) z (-?\d+) heading (\d+)')

def test_ring():
    script = (MENU + "17000 down w\n18500 cheat where\n19500 cheat where\n20000 key esc\n40000 key esc\n41000 up w\n"
              "43000 cheat where\n44000 cheat where\n45000 cheat where\n")
    err = run('ring', script, 47000, dict(UWPORT_FOCUSTEST='1000,1500'))
    wh = [tuple(int(g) for g in m.groups()) for m in (WHERE.match(l) for l in err) if m]
    over = [l for l in err if 'keyqueue: ring overrun' in l]
    full = [l for l in err if 'keyqueue: ring full' in l]
    held = [l for l in err if 'key_on: 0x11 held' in l]
    print('ring: where %s; %d "ring full" lines %s' % (wh, len(full), full[:3]))
    check('ring: five "where" lines (two while W is held, three after the release)', len(wh) == 5, str(wh))
    check('ring: no "keyqueue: ring overrun" line', not over, '%d lines' % len(over))
    check('ring: no "key_on: 0x11 held" line (W not left held in the game)', not held, '%d lines, first %s' % (len(held), held[:1]))
    if len(wh) == 5:
        check('ring: the player walks while W is held (the position changes)', wh[0][:2] != wh[1][:2], str(wh[:2]))
        check('ring: the player stopped 2 s after the release (the position the same at 43, 44 and 45 s)',
              wh[2] == wh[3] == wh[4], str(wh[2:]))

# C pressed while W walks (the user, 2026-11-13: "while moving forward and i press c to start combat it sometimes
# doesn't ... i have to spam the button"): the game's key handlers act on MOUSE.C's do_keyarray_input (a scan of the keys
# held, at most every 30 ticks since the last key it gave), never on key()'s result, so a tap whose make and break came
# together (C's F5) counted only when no key had been given for 30 ticks; W's walk (X) is given every 30 ticks. Now a tap
# is held until the game gives it (uwfront.c send_key, MOUSE.C port_key_taken). W held from 16 s to 24 s, C tapped six
# times 1.2 s apart: each C toggles fight mode (the "fight mode on/off asked" lines, which read the game's weapon state
# before the press, alternate on, off, on, ...), and each F5 make reaches the game with its break later (the trace).
def test_fightwalk():
    taps = [16700 + 1200 * i for i in range(6)]
    mt = ['16000:kd@17/0'] + ['%d:kd@46/0,%d:ku@46/0' % (t, t + 90) for t in taps] + ['24000:ku@17/0']
    err = run('fightwalk', MENU, 25000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_INPUTTRACE='1', UWPORT_GRABDBG='1',
                                             UWPORT_MOUSETEST=','.join(mt)))
    asked = [m.group(1) for m in (re.search(r'fight mode (on|off) asked', l) for l in err) if m]
    held = [l for l in err if ': tap 3F held' in l]
    for l in held: print('  ' + l)
    check('fightwalk: six C presses asked (six "fight mode ... asked" lines)', len(asked) == 6, str(asked))
    check('fightwalk: each C toggled fight mode while W walked (on, off, on, off, on, off)',
          asked == ['on', 'off'] * 3, str(asked))
    ins = [m.group(1) for m in (IN.match(l) for l in err) if m]
    check('fightwalk: W\'s walk (X, key 2D) held through the presses (no break AD before the last F5)',
          'key 2D' in ins and 'key AD' not in ins[:max((i for i, v in enumerate(ins) if v == 'key BF'), default=0)], str(ins[:12]))
    check('fightwalk: each F5 make (3F) reached the game and was let go (BF) after the game took it (a "tap 3F held" line each)',
          ins.count('key 3F') == 6 and ins.count('key BF') == 6 and len(held) == 6, '%d makes, %d breaks, %d tap lines'
          % (ins.count('key 3F'), ins.count('key BF'), len(held)))

# the attack's kind in mouse-look (the user, 2026-11-14: "I cannot do an overhead attack or a stab due to the mouse
# being in the center of the screen"): the mouse's motion in the 100 ms before the fight-mode left press picks it, as
# the original's high / low pointer did: up the bash (the port's p, 19), down the stab (the port's ., 34), still or
# sideways the slash (the port's Space, 39, while the last kind sent was the slash; else its ;, 27, which sets the
# port's last attack back to the slash). Each key's break at the button's release.
def test_flick():
    mt = ['16000:kd@46/0', '16100:ku@46/0',
          '17000:ld', '17400:lu',                           # still: Space
          '19000:raw@0/-300', '19020:ld', '19400:lu',       # up: p (bash)
          '21000:ld', '21400:lu',                           # still after a bash: ;
          '23000:raw@0/300', '23020:ld', '23400:lu',        # down: . (stab)
          '25000:raw@300/40', '25020:ld', '25400:lu',       # sideways after a stab: ;
          '27000:ld', '27400:lu']                           # still after a slash: Space
    err = run('flick', MENU, 29000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_INPUTTRACE='1', UWPORT_MOUSETEST=','.join(mt)))
    check('flick: fight mode asked (C)', any('fight mode on asked' in l for l in err))
    ins = [m.group(1) for m in (IN.match(l) for l in err) if m]
    atk = [v for v in ins if v in ('key 39', 'key B9', 'key 19', 'key 99', 'key 27', 'key A7', 'key 34', 'key B4')]
    want = ['key 39', 'key B9', 'key 19', 'key 99', 'key 27', 'key A7', 'key 34', 'key B4', 'key 27', 'key A7', 'key 39', 'key B9']
    check('flick: still Space, up p (bash), still ;, down . (stab), sideways ;, still Space, each let go', atk == want, str(atk))
    held = [l for l in err if 'key_on: 0x' in l and 'held' in l]
    check('flick: no key left held in the game', not held, str(held[:2]))

# the left button held when the focus is lost (the final review's finding 1): uwi_release_all at WM_KILLFOCUS gives
# the world (and our panel) the press's release early; it must take the let-go path (world_let_go: nothing sent but
# the matched releases), never world_click's or inv_pointer's release (full_play does not depend on the focus). Twenty
# sacks dropped around the player (cheat give 0x80 drop: random spots within 6/8 of a tile), PageDown held 0.8 s so
# that the crosshair rests on one (UWPORT_GRABDBG's "crosshair on" lines say so), then:
#   take    the left press on it and the focus lost 100 ms later (before HOLD_PICK_MS: the take still pending): no look
#           (q: in: key 10), no grab key, no throw; nothing on the cursor after
#   carry   the left press held 0.8 s (the take done: grab key n, the sack on the cursor, carried), then the focus
#           lost: no throw ("throw:" / grab key o) and no packing (grab key u); the sack stays on the cursor (a
#           "crosshair on ... held 1" line after)
#   panel   a right click on it (e: the sack onto the cursor, our panel opened for it), the left press at the
#           window's middle (the world in our panel: let go there, thrown), then the focus lost: no throw
# and in each no key left held in the game ("key_on" lines).
XH = re.compile(r'crosshair on "([^"]*)" kind (\d+) take (\d) reach (\d) stack \d+, held (\d)')
FH_AIM = ''.join('%d cheat give 0x80 drop\n' % (15000 + 100 * i) for i in range(20))

def fh_after(err, mark):
    i = next((k for k, l in enumerate(err) if mark in l), None)
    return (err[:i], err[i:]) if i is not None else (err, [])

def test_focusheld():
    aim = ['17500:kd@337/0', '18300:ku@337/0']
    runs = {
        'take':  (aim + ['19000:ld', '20500:lu'], '1000,1500,19100,19600'),
        'carry': (aim + ['19000:ld', '21000:lu'], '1000,1500,19800,20300'),
        'panel': (aim + ['18800:kd@23/0', '18900:ku@23/0', '19500:ld', '21500:lu'], '1000,1500,20000,20500'),
    }
    for name, (mt, ft) in runs.items():
        # (the sacks' spots are the game's rand: a run whose crosshair did not come to rest on one is run again, up to
        # four times; the aim is a precondition, printed, not the finding's check)
        for attempt in range(4):
            err = run('focusheld-' + name, MENU + FH_AIM, 24000,
                      dict(UWPORT_FOCUSTEST=ft, UWPORT_GRABDBG='1', UWPORT_INPUTTRACE='1', UWPORT_MOUSETEST=','.join(mt)))
            before, after = fh_after(err, ': ld 5000 5000')
            xs = [m for m in (XH.search(l) for l in before) if m]
            on = xs[-1] if xs else None
            aimed = on is not None and on.group(3) == '1' and on.group(4) == '1'
            if aimed: break
            print('focusheld %s: run %d: the crosshair not on a sack at the press (%s): again' % (name, attempt + 1,
                  on.group(0) if on else 'no crosshair line'))
        check('focusheld %s: the crosshair on a thing to take at the left press' % name, aimed)
        pre, gone = fh_after(after, 'focus: lost to window')
        for l in [l for l in after if 'grab key' in l or 'throw:' in l or 'focus test' in l or 'mouse test' in l
                  or 'crosshair' in l or l in ('in: key 10', 'in: key 31', 'in: key 18', 'in: key 16', 'in: key 12')]:
            print('  ' + l)
        if name == 'carry':
            check('focusheld carry: the take done before the focus went (grab key n)', any('grab key n' in l for l in pre))
        if name == 'panel':
            check('focusheld panel: the sack onto the cursor in our panel before the focus went (a "held 1" crosshair line)',
                  any((m := XH.search(l)) and m.group(5) == '1' for l in pre))
        check('focusheld %s: the focus lost while the button is held' % name, gone != [])
        # (the window's side: q, n, o, u sent, our grab keys, a throw's point; the game's: port_key_grab 1 / 2, the throw
        # or the packing carried out)
        bad = [l for l in gone if l in ('in: key 10', 'in: key 31', 'in: key 18', 'in: key 16') or 'grab key' in l or 'throw:' in l
               or l.startswith('port_key_grab 1') or l.startswith('port_key_grab 2')]
        if name == 'take': bad += [l for l in pre if l == 'in: key 10' or 'grab key' in l]
        check('focusheld %s: after the focus went nothing sent for the press (no look q, no grab key, no throw)' % name,
              not bad, str(bad[:4]))
        held = [l for l in err if l.startswith('key_on:')]
        check('focusheld %s: no key left held in the game (no "key_on" line)' % name, not held, str(held[:2]))
        xg = [m for m in (XH.search(l) for l in gone) if m]
        if name == 'take':
            check('focusheld take: nothing taken onto the cursor (no "held 1" after)', not [m for m in xg if m.group(5) == '1'],
                  str([m.group(0) for m in xg][:3]))
        if name == 'carry':
            last = [m for m in (XH.search(l) for l in err) if m][-1:]
            check('focusheld carry: the sack still on the cursor (the last "crosshair on" line says held 1; evidence only: '
                  'the line is not written again until the crosshair changes)',
                  last and last[0].group(5) == '1', last[0].group(0) if last else 'none')

# a level whose scene cannot be built (the final review's finding 2): uwfront.c's view_world keeps the last frame's
# camera and level ("level" fallback); uwview's level_build resets its arena before it fails, so each try of the new
# level costs a rebuild of the old one. Our renderer's data (UWVIEW_DATA) copied once into work\inputcheck\data-lv2 with
# levels\level2.txt empty (uwview: "level 2: cannot read our level data"); cheat goto 2 28 28 at 18 s, 6 s on level 2.
# The tries of level 2 and the rebuilds of level 1 after the goto: at most one a second each (+1), not one a frame.
LVBUILD = re.compile(r'^uwview: level (\d+): (\d+ triangles|cannot read our level data)')

def test_levelfail():
    src = os.path.join(os.path.dirname(EXE), 'data')
    dst = os.path.join(WORK, 'data-lv2')
    if not os.path.isdir(dst):
        shutil.copytree(src, dst)
    open(os.path.join(dst, 'levels', 'level2.txt'), 'w').close()
    err = run('levelfail', MENU + "18000 cheat goto 2 28 28\n", 24000, dict(UWPORT_FOCUSTEST='1000,1500', UWVIEW_DATA=dst))
    g = next((i for i, l in enumerate(err) if 'cheat: goto' in l), None)
    after = err[g:] if g is not None else []
    tries = [l for l in after if (m := LVBUILD.match(l)) and m.group(1) == '2']
    rebuilds = [l for l in after if (m := LVBUILD.match(l)) and m.group(1) == '1']
    kept = [l for l in after if 'kept our last frame (level)' in l]
    print('levelfail: after the goto %d tries of level 2, %d builds of level 1, %d "kept our last frame (level)" lines'
          % (len(tries), len(rebuilds), len(kept)))
    check('levelfail: the goto to level 2 made (a "cheat: goto" line)', g is not None)
    check('levelfail: level 2 tried and failed (uwview: "level 2: cannot read our level data")', tries)
    check('levelfail: our last frame kept for it ("kept our last frame (level)")', kept)
    check('levelfail: level 2 tried at most once a second (6 s: at most 7)', len(tries) <= 7, '%d tries' % len(tries))
    check('levelfail: level 1 rebuilt at most once a second (6 s: at most 7)', len(rebuilds) <= 7, '%d builds' % len(rebuilds))

# ---- visible: the real capture on the user's screen (Task 5; the user allowed visible runs for this work) ----
# Each run as the hidden ones but shown (no UWPORT_OFFSCREEN), so set_capture really calls ClipCursor and ShowCursor;
# under UWPORT_LOOKDBG each set_capture logs "clip: set N: clip L,T,R,B client L,T,R,B screen L,T,R,B cursor C"
# (plat_win32.c: GetClipCursor, the client's rectangle on the screen, the virtual screen, and C whether the cursor
# shows: GetCursorInfo's CURSOR_SHOWING, 1 shown, 0 hidden, -1 not read; before the final review's finding 4 C was
# ShowCursor's display count, below 0 hidden, read by calling ShowCursor). "held" = the clip is the client's rectangle
# (cut to the screen); "free" = the clip is the whole virtual screen; the cursor hidden = C 0. The release at the exit (after "N presents") is left out.
CLIP = re.compile(r'clip: set (\d): clip (-?\d+),(-?\d+),(-?\d+),(-?\d+) client (-?\d+),(-?\d+),(-?\d+),(-?\d+) '
                  r'screen (-?\d+),(-?\d+),(-?\d+),(-?\d+) cursor (-?\d+) at (\d+) ms')
CAP = re.compile(r"mouse-look's capture (on|off) at (\d+) ms")

def uw_new_running():
    r = subprocess.run(['tasklist'], capture_output=True, text=True, errors='replace')
    return [l for l in r.stdout.splitlines() if 'uw-new.exe' in l.lower()]

def vtimeline(err):
    """('screen', N) / ('want', on|off) / ('cap', on|off) / ('clip', set, 'held'|'free'|'other', cursor, ms) /
    ('in', 'key XX'|'ptr ...') / ('focus', away|back), up to the exit's "N presents" line"""
    tl = []
    for l in err:
        if re.search(r': \d+ presents$', l): break
        m = SCREEN.search(l)
        if m: tl.append(('screen', int(m.group(1)))); continue
        m = WANT.search(l)
        if m: tl.append(('want', m.group(1))); continue
        m = CAP.search(l)
        if m: tl.append(('cap', m.group(1))); continue
        m = CLIP.search(l)
        if m:
            v = [int(x) for x in m.groups()]
            on, c, r, s, cur, ms = v[0], v[1:5], v[5:9], v[9:13], v[13], v[14]
            cut = [max(r[0], s[0]), max(r[1], s[1]), min(r[2], s[2]), min(r[3], s[3])]
            state = 'held' if c == cut else 'free' if c == s else 'other'
            tl.append(('clip', on, state, cur, ms)); continue
        m = IN.match(l)
        if m: tl.append(('in', m.group(1))); continue
        m = re.search(r'focus test: (away|back)', l)
        if m: tl.append(('focus', m.group(1)))
    return tl

def vclips(tl):
    """the clip lines as words: 'held' (set 1, the clip the window's, the cursor hidden), 'free' (set 0, the clip the
    screen, the cursor shown) or the line's own values when it is neither"""
    out = []
    for e in tl:
        if e[0] != 'clip': continue
        _, on, state, cur, ms = e
        if on and state == 'held' and cur == 0: out.append('held')
        elif not on and state == 'free' and cur == 1: out.append('free')
        else: out.append('set %d %s cursor %d at %d ms' % (on, state, cur, ms))
    return out

def vshow(name, tl):
    print('  %s: %s' % (name, ' '.join(('S%d' % e[1]) if e[0] == 'screen' else ('want-' + e[1]) if e[0] == 'want' else
                                         ('cap-' + e[1]) if e[0] == 'cap' else ('CLIP-%s/%s/%d' % ('on' if e[1] else 'off', e[2], e[3]))
                                         if e[0] == 'clip' else ('focus-' + e[1]) for e in tl if e[0] != 'in')))

def vafter(tl, kind, value, nth=-1):
    """the timeline from the nth (default the last) ('kind', value) entry on"""
    idx = [i for i, e in enumerate(tl) if e[0] == kind and e[1] == value]
    return tl[idx[nth]:] if idx else []

def vrun(name, script, exit_ms, env):
    if uw_new_running():
        print('visible %s: refused: uw-new.exe is running (the user may be playing)' % name)
        return None
    err = run('v-' + name, script, exit_ms, dict(env, UWPORT_INPUTTRACE='1'), visible=True)
    tl = vtimeline(err)
    vshow(name, tl)
    return tl

def vcheck_play(tl):
    games = vafter(tl, 'screen', 2, 0)
    c = vclips(games)
    check('visible play: PS_GAME, then mouse-look on (want) and the capture on', ('want', 'on') in games and ('cap', 'on') in games)
    check('visible play: the pointer really held to the window, the cursor hidden (clip lines: held only)', c == ['held'], str(c))

V_ESC = lambda t: '%d:kd@1/0,%d:ku@1/0' % (t, t + 100)
V_KEY = lambda t, code: '%d:kd@%d/0,%d:ku@%d/0' % (t, code, t + 100, code)

def visible_play():
    tl = vrun('play', MENU, 26000, dict(UWPORT_FOCUSTEST='1000,1500'))
    if tl is not None: vcheck_play(tl)

def visible_menu():
    tl = vrun('menu', MENU, 30000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_MOUSETEST=V_ESC(20000) + ',' + V_ESC(24000)))
    if tl is None: return
    c = vclips(vafter(tl, 'screen', 2, 0))
    check('visible menu: in play held; Esc (our menu): the pointer free, the cursor shown; Esc again: held (clips held free held)',
          c == ['held', 'free', 'held'], str(c))
    # the game is paused under our menu, so it applies no want there (its mouse-look stays on, the capture's line
    # with it); the backend's want goes off at the Esc and on at the second (the raw motion follows it: the view still)
    err = open(os.path.join(WORK, 'v-menu.err')).read().splitlines()
    pub = [l.split('input: mouse-look ')[1].split(',')[0] for l in err if 'input: mouse-look ' in l and 'screen 2' in l]
    check('visible menu: the backend\'s want off at the Esc, on at the second Esc ("input: mouse-look" lines in PS_GAME)',
          pub == ['wanted', 'not wanted', 'wanted'], str(pub))

def visible_panel():
    # I: our panel open, closed; M: the map, Esc: closed
    mt = ','.join([V_KEY(20000, 0x17), V_KEY(23000, 0x17), V_KEY(26000, 0x32), V_ESC(29000)])
    tl = vrun('panel', MENU, 33000, dict(UWPORT_FOCUSTEST='1000,1500', UWPORT_MOUSETEST=mt))
    if tl is None: return
    g = vafter(tl, 'screen', 2, 0)
    c = vclips(g)
    check('visible panel: I (our panel): free, I: held; M (the map): free, Esc: held (clips held free held free held)',
          c == ['held', 'free', 'held', 'free', 'held'], str(c))
    seq = [e[1] for e in g if e[0] == 'screen']
    check('visible panel: the map is screen 3, then PS_GAME again', 3 in seq and 2 in seq[seq.index(3):] if 3 in seq else False, str(seq))

def visible_conv():
    script = MENU + "20000 cheat talk 243\n" + ''.join('%d key 1\n' % t for t in range(23000, 40000, 1500))
    tl = vrun('conv', script, 46000, dict(UWPORT_FOCUSTEST='1000,1500'))
    if tl is None: return
    g = vafter(tl, 'screen', 2, 0)
    c = vclips(g)
    seq = [e[1] for e in g if e[0] == 'screen']
    check('visible conv: a conversation (screen 4), then PS_GAME again', 4 in seq and 2 in seq[seq.index(4):] if 4 in seq else False, str(seq))
    check('visible conv: held in play, free in the conversation, held after (clips held free held)', c == ['held', 'free', 'held'], str(c))
    # the free one in the conversation, the second held after its PS_GAME
    try:
        i4 = g.index(('screen', 4)); back = next(i for i in range(i4, len(g)) if g[i] == ('screen', 2))
        ok = vclips(g[back:]) == ['held'] and vclips(g[i4:back]) in (['free'], [])
        ok = ok and 'free' in vclips(g[:back])
    except (ValueError, StopIteration):
        ok = False
    check('visible conv: let go by the conversation, held again only after it (in PS_GAME)', ok)

def visible_death():
    tl = vrun('death', MENU + "18000 cheat die\n" + MENU2, 46000, dict(UWPORT_FOCUSTEST='1000,1500'))
    if tl is None: return
    g = vafter(tl, 'screen', 2, 0)
    c = vclips(g)
    check('visible death: held, free at the death, held after the restore (clips held free held)', c == ['held', 'free', 'held'], str(c))
    d = vafter(g, 'screen', 7, 0)
    nxt = next((i for i, e in enumerate(d[1:], 1) if e[0] == 'screen'), len(d))
    check('visible death: the pointer let go during PS_DEATH (before the next screen)', 'free' in vclips(d[:nxt]), str(vclips(d[:nxt])))
    last = vafter(tl, 'screen', 2)
    check('visible death: held again after the restore\'s PS_GAME', vclips(last) == ['held'], str(vclips(last)))

NEWCHAR = (MENU + "18000 cheat die\n34000 key home\n34300 key right\n34600 key enter\n" +
           ''.join('%d key enter\n' % (37000 + 1200 * i) for i in range(12)) +
           "51400 key a\n52000 key enter\n53200 key enter\n")

def visible_newchar():
    tl = vrun('newchar', NEWCHAR, 66000, dict(UWPORT_FOCUSTEST='1000,1500'))
    if tl is None: return
    seq = [e[1] for e in tl if e[0] == 'screen']
    check('visible newchar: PS_DEATH, character creation (screen 9), PS_GAME', 7 in seq and 9 in seq and seq[-1:] == [2], str(seq))
    c = vclips(vafter(tl, 'screen', 2, 0))
    check('visible newchar: held, free at the death, held in the new game (clips held free held)', c == ['held', 'free', 'held'], str(c))
    last = vafter(tl, 'screen', 2)
    caps = [e[1] for e in last if e[0] == 'cap']; wants = [e[1] for e in last if e[0] == 'want']
    check('visible newchar: in the new game mouse-look on once (want on, capture on, one clip held)',
          caps == ['on'] and wants == ['on'] and vclips(last) == ['held'], 'caps %s wants %s clips %s' % (caps, wants, vclips(last)))

def visible_alttab():
    # W and the right button held from 20 s; the focus away at 22 s; both let go at 23 / 23.5 s (no owner: nothing
    # sent); the focus back at 25 s
    mt = '20000:kd@17/0,20500:rd,23000:ku@17/0,23500:ru'
    tl = vrun('alttab', MENU, 32000, dict(UWPORT_FOCUSTEST='1000,1500,22000,25000', UWPORT_MOUSETEST=mt))
    if tl is None: return
    g = vafter(tl, 'screen', 2, 0)
    c = vclips(g)
    check('visible alttab: held, free while away, held after (clips held free held)', c == ['held', 'free', 'held'], str(c))
    away = vafter(g, 'focus', 'away')
    caps = [e[1] for e in away if e[0] == 'cap']
    check('visible alttab: the capture off while away and on after the focus is back (capture off on)', caps == ['off', 'on'], str(caps))
    ins = [e[1] for e in g if e[0] == 'in']
    print('  alttab: the in: lines from PS_GAME: %s' % ins)
    held = set()
    for k in ins:
        if not k.startswith('key '): continue
        b = int(k[4:], 16)
        if b < 0x80: held.add(b)
        else: held.discard(b & 0x7F)
    check('visible alttab: W\'s press reached the game (key 2D: the walk, as Up\'s) and every key pressed was let go (no make without its break)',
          'key 2D' in ins and not held, 'held %s' % sorted('%02X' % b for b in held))
    back = vafter(g, 'focus', 'back')
    sent = [e[1] for e in back if e[0] == 'in' and e[1].startswith('key ')]
    check('visible alttab: nothing of W or the right button sent after the focus is back (their releases had no owner)',
          not [k for k in sent if k in ('key 2D', 'key AD', 'key 12', 'key 92')], str(sent))
    lost = [l for l in open(os.path.join(WORK, 'v-alttab.err')).read().splitlines() if 'key_on:' in l]
    check('visible alttab: no "key_on: ... held" line', not lost, str(lost[:3]))

def visible_sleep():
    tl = vrun('sleep', SLEEP, 70000, dict(UWPORT_FOCUSTEST='1000,1500'))
    if tl is None: return
    ins = [i for i, e in enumerate(tl) if e == ('in', 'key 44')]
    z = tl[ins[0]:] if ins else []
    seq = [e[1] for e in z if e[0] == 'screen']
    check('visible sleep: Z held: exactly one dream (one screen 6), then play again (screen 2)',
          seq.count(6) == 1 and 2 in seq[seq.index(6):] if 6 in seq else False, 'screens after the Z: %s' % seq)
    c = vclips(vafter(tl, 'screen', 2, 0))
    check('visible sleep: held, free in the dream, held after (clips held free held)', c == ['held', 'free', 'held'], str(c))

VISIBLE = ['play', 'menu', 'panel', 'conv', 'death', 'newchar', 'alttab', 'sleep']

def test_visible(which=None):
    for v in which or VISIBLE:
        if uw_new_running():
            print('visible: refused: uw-new.exe is running (the user may be playing): %s' % uw_new_running()[0])
            global fails
            fails += 1
            return
        globals()['visible_' + v]()

if __name__ == '__main__':
    os.makedirs(WORK, exist_ok=True)
    if sys.argv[1:2] == ['visible']:
        test_visible(sys.argv[2:])
        print('inputcheck: %d failures' % fails)
        sys.exit(fails)
    for t in sys.argv[1:] or ['look', 'trace', 'charge', 'sleep', 'display', 'original', 'ring', 'focusheld', 'levelfail', 'fightwalk', 'flick']: globals()['test_' + t]()
    print('inputcheck: %d failures' % fails)
    sys.exit(fails)
