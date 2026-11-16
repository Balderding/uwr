# convcheck.py: the conversation record's check (docs\superpowers\plans\2026-11-13-conversation-and-map-screens.md,
# step A1). An off-screen run of the test exe (underworld-exhumed\uw1\build\port\uw1port.exe) from a fresh copy of the
# test home work\in2-4: the start menu's restore, then `cheat talk N` and the key 1 every 1.5 s until the conversation
# ends. With UWPORT_CONVLOG=1 the backend (port\win32\uwconvrec.c) writes each hook's event as a "conv: ..." line.
# The reference is STRINGS.PAK read by tools\strings_pak.py (not the game): the conversation's own block
# (0x0E00 + its number) holds every line the NPC says and every answer it offers.
#   - a conversation opened (open, player, npc lines) and closed, in that order;
#   - every NPC line ("\P" say), every narration ("\2" print) and every answer is a string of that block, the
#     substitutions (@SS1, @GI3 ... : convert_string's) matched as any text;
#   - every menu's count is the number of answers given before it; every "chosen k" names one of them and is
#     followed by the original's echo of it (play_say: "\1" + the answer + "\0\n" in the NPC's scroll);
#   - at least one menu answered.
# Usage: python -I tools\convcheck.py [N ...]   (N: a mobile object index for `cheat talk`; default 243). Exit code:
# the failures.
import os, sys, re, shutil, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import strings_pak  # noqa: E402

EXE = os.environ.get('CONVCHECK_EXE') or os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port', 'uw1port.exe')
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'convcheck')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''))
    if not ok: fails += 1

def run(name, script, exit_ms, env=None):
    home = os.path.join(WORK, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f:
        # (quick_fades off: a replacement is off in every proof. On, the map opens a second sooner and the bytes past a
        # note's text, memory the note code never writes, differ with that timing: work\mapvary.py, NOTES "quick fades")
        f.write('settings-at-start=0\nquick_fades=0\n')
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', UWPORT_CONVLOG='1', **(env or {}))
    cmd = [EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio', '--input-script', sp,
           '--exit-after', str(exit_ms)]
    r = subprocess.run(cmd, env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    return r.stderr.splitlines()

EV = re.compile(r'conv: (\w+)(?: (\w+))?(?: "(.*)")?$')
def events(err):
    out = []
    for l in err:
        i = l.find('conv: ')
        if i < 0: continue
        l = l[i:]
        m = re.match(r'conv: open (\d+) player "(.*)" head (\d+)$', l)
        if m: out.append(('open', int(m.group(1)), m.group(2), int(m.group(3)))); continue
        m = re.match(r'conv: npc "(.*)" (charhead|genhead) (\d+)$', l)
        if m: out.append(('npc', m.group(1), m.group(2), int(m.group(3)))); continue
        m = re.match(r'conv: text (npc|play|main) "(.*)"$', l)
        if m: out.append(('text', m.group(1), m.group(2).replace('\\n', '\n'))); continue
        m = re.match(r'conv: option (\d+) "(.*)"$', l)
        if m: out.append(('option', int(m.group(1)), m.group(2))); continue
        m = re.match(r'conv: (menu|chosen) (\d+)$', l)
        if m: out.append((m.group(1), int(m.group(2)))); continue
        m = re.match(r'conv: (close|ask|clear \w+|wait .*)$', l)
        if m: out.append((m.group(1),)); continue
        m = re.match(r'conv: asked "(.*)"$', l)
        if m: out.append(('asked', m.group(1))); continue
        out.append(('?', l))
    return out

SUB = re.compile(r'@[GSP][SIP]-?\d+(?:\[\d+\])?|@[GSP][SIP][A-Za-z0-9_]+')
def pattern(s):
    """a block string as a regex: the substitutions as any text"""
    parts = SUB.split(s)
    return re.compile('^' + '.*'.join(re.escape(p) for p in parts) + '$', re.S)

ESC = re.compile(r'\\[0-9Pp]')
def bare(s):
    return ESC.sub('', s).strip('\n')

def known(text, pats):
    t = bare(text)
    return not t or any(p.match(t) for p in pats)

def test(n):
    script = MENU + "20000 cheat talk %d\n" % n + ''.join('%d key 1\n' % t for t in range(23000, 52000, 1500))
    ev = events(run('talk%d' % n, script, 56000))
    # (strt_converse clears both scrolls before it draws the names: the clears and waits are left out of the order)
    ev = [e for e in ev if not (e[0].startswith('clear') or e[0].startswith('wait'))]
    kinds = [e[0] for e in ev]
    tag = 'talk %d' % n
    check(tag + ': no unread conv line', '?' not in kinds, str([e for e in ev if e[0] == '?'][:2]))
    check(tag + ': opened (open, npc), then closed', kinds[:2] == ['open', 'npc'] and 'close' in kinds,
          'first %s, close %s' % (kinds[:3], 'close' in kinds))
    if not ev or ev[0][0] != 'open': return
    cnv = ev[0][1]
    blk = strings_pak.blocks().get(0x0E00 + cnv)
    check(tag + ': conversation %d has its block %d' % (cnv, 0x0E00 + cnv), blk is not None)
    if blk is None: return
    pats = [pattern(s) for s in blk]
    print('  conversation %d: player "%s", npc "%s" (%s %d)' % (cnv, ev[0][2], ev[1][1], ev[1][2], ev[1][3]))
    said = [e[2] for e in ev if e[0] == 'text' and e[1] == 'npc' and (e[2].startswith('\\P') or e[2].startswith('\\2'))]
    bad = [s for s in said if not known(s, pats)]
    check(tag + ': each NPC line and narration (%d) is a string of its block' % len(said), said and not bad, repr(bad[:2]))
    opts, menus, answered, badm = {}, 0, 0, []
    for i, e in enumerate(ev):
        if e[0] == 'option':
            opts[e[1]] = e[2]
            if not known(e[2], pats): badm.append('unknown answer %r' % e[2])
        elif e[0] == 'menu':
            menus += 1
            if sorted(opts) != list(range(1, e[1] + 1)): badm.append('menu %d after answers %s' % (e[1], sorted(opts)))
            cur = dict(opts); opts = {}
        elif e[0] == 'chosen':
            answered += 1
            k = e[1]
            if k not in cur: badm.append('chosen %d of %s' % (k, sorted(cur))); continue
            echo = next((x for x in ev[i + 1:] if x[0] == 'text' and x[1] == 'npc'), None)
            want = '\\1' + cur[k] + '\\0\n'
            if not echo or echo[2] != want: badm.append('echo %r, want %r' % (echo and echo[2], want))
    check(tag + ': menus (%d) and choices (%d) agree with the answers and the echoes' % (menus, answered),
          menus and answered and not badm, '; '.join(badm[:3]))

# Our screen (step A2): a click on our second answer line gives the game the same choice as the key 2. Two runs of
# Hagbard's conversation: the key 2 at the first menu (23 s), and a left click on our answer 2 (the message scroll's
# second row: the original's x 60, y 178; in a 1920 x 1080 window at the scale 5, client 2396/8074 of 10000); then 1
# every 1.5 s in both. The two runs' conversation events (the backend's own "our answer" line taken out) are the
# same, and the click run has its "our answer 2 clicked" line. Run with each conv_font (UWVIEW_CONVFONT).
def test_click(font):
    keys = ''.join('%d key 1\n' % t for t in range(26000, 52000, 1500))
    tag = 'click (%s)' % font
    env = dict(UWVIEW_CONVFONT=font)
    a = run('key2-' + font, MENU + "20000 cheat talk 243\n23000 key 2\n" + keys, 56000, env)
    env_b = dict(env, UWPORT_MOUSETEST='23000:mv@2396/8074,23100:ld@2396/8074,23200:lu@2396/8074')
    b = run('click2-' + font, MENU + "20000 cheat talk 243\n" + keys, 56000, env_b)
    clicked = [l for l in b if 'our answer' in l]
    check(tag + ': our answer 2 clicked', any('our answer 2 clicked' in l for l in clicked), str(clicked[:2]))
    ea = [e for e in events(a) if e[0] != '?']
    eb = [e for e in events(b) if e[0] != '?']
    ch = [e[1] for e in ea if e[0] == 'chosen']
    check(tag + ': the key run chose 2 first (%s)' % ch[:3], ch[:1] == [2])
    diff = next((i for i, (x, y) in enumerate(zip(ea, eb)) if x != y), None)
    check(tag + ': the same conversation events as the key 2 (%d, %d)' % (len(ea), len(eb)),
          len(ea) == len(eb) and diff is None, 'first difference at %s: %s / %s' % (
              diff, ea[diff] if diff is not None else '', eb[diff] if diff is not None else ''))

# A typed answer (babl_ask, step A3): Steeltoe (level 2, object 250 on tile 9 14, conversation 21) asks after the
# answers 1, 1. Typed: d w a r f, Left twice, x: the original's wdialog (its own rules, our box only shows them) makes
# "dwaxrf"; the record's typing follows each key, its answer is that text, and the NPC replies after it.
def test_ask():
    keys = ['1', '1', 'd', 'w', 'a', 'r', 'f', 'left', 'left', 'x', 'enter']
    script = MENU + "18000 cheat goto 2 9 14\n22000 cheat talk 250\n" + \
        ''.join('%d key %s\n' % (25000 + 1500 * i if i < 2 else 28500 + 300 * i, k) for i, k in enumerate(keys))
    ev = [e for e in events(run('ask', script, 40000)) if not (e[0].startswith('clear') or e[0].startswith('wait'))]
    tag = 'ask (Steeltoe)'
    check(tag + ': conversation 21 opened', ev[:1] and ev[0][0] == 'open' and ev[0][1] == 21, str(ev[:1]))
    i = next((k for k, e in enumerate(ev) if e[0] == 'ask'), None)
    check(tag + ': babl_ask asked', i is not None)
    if i is None: return
    asked = next(((k, e[1]) for k, e in enumerate(ev) if e[0] == 'asked'), (None, None))
    check(tag + ': the answer given is "dwaxrf" (%r)' % asked[1], asked[1] == 'dwaxrf')
    if asked[0] is not None:
        after = [e for e in ev[asked[0]:] if e[0] == 'text' and e[1] == 'npc']
        check(tag + ': the NPC replies after it', bool(after))

# Our map (Part B): it only draws. The same session with our map and with the original's (UWPORT_ORIGMAP=1): M, a
# click on the map (the window's own path: map point 120, 150), the note HALL typed, Enter, Esc. SaveTheWords writes
# the note into SAVE0\LEV.ARK as the map closes: the two files are the same bytes, and the note is in them.
def test_map():
    import hashlib
    script = MENU + "18000 key m\n21000 key h\n21300 key a\n21600 key l\n21900 key l\n22200 key enter\n24000 key esc\n"
    mt = dict(UWPORT_MOUSETEST='20000:mv@3958/7315,20200:ld@3958/7315,20300:lu@3958/7315')
    out = {}
    for name, env in (('map-ours', mt), ('map-orig', dict(mt, UWPORT_ORIGMAP='1'))):
        run(name, script, 27000, env)
        p = os.path.join(WORK, 'h-' + name, 'SAVE0', 'LEV.ARK')
        if not os.path.exists(p): p = os.path.join(WORK, 'h-' + name, 'SAVE0', 'lev.ark')
        out[name] = open(p, 'rb').read() if os.path.exists(p) else b''
    a, b = out['map-ours'], out['map-orig']
    check('map: SAVE0\\LEV.ARK written (%d bytes)' % len(a), len(a) > 0)
    check('map: the note HALL is in it', b'HALL' in a)
    check('map: the same bytes with our map and the original\'s (%s / %s)' % (
        hashlib.md5(a).hexdigest()[:8], hashlib.md5(b).hexdigest()[:8]), a == b)

if __name__ == '__main__':
    os.makedirs(WORK, exist_ok=True)
    args = sys.argv[1:] or ['243']
    for a in args:
        if a == 'click':
            test_click('smooth')
            test_click('original')
        elif a == 'ask':
            test_ask()
        elif a == 'map':
            test_map()
        else:
            test(int(a))
    print('convcheck: %s' % ('PASS' if not fails else '%d FAIL' % fails))
    sys.exit(fails)
