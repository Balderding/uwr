# followcheck.py: mouse-look's follower (plat_win32.c's mouse_look_follow) sends its ` key only while the game says
# it is on its 3D screen (PS_GAME = 2). Off-screen runs of the test exe from a fresh copy of work\in2-4, UWPORT_LOOKDBG
# on: every ` the follower sends is logged with the screen state ("follower: ` in screen N at MS ms").
#   death   the death's cutscene with the focus lost during it (the mouse review's case): no ` outside screen 2
#   play    plain play with the focus lost and back in PS_GAME: no ` sent (no follower line, no "in: key 29" in the
#           UWPORT_INPUTTRACE trace: the follower is gone since the input rewrite, Task 3), and the game's own
#           "mouse-look off (want)" then "on (want)" around the focus test
# Usage: python -I tools\followcheck.py [death play]. Exit code: the failures.
import os, sys, re, shutil, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port', 'uw1port.exe')
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'followcheck')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"

def run(name, script, exit_ms, focus):
    home = os.path.join(WORK, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f: f.write('settings-at-start=0\nllm_model=Qwen3.5-2B\\Qwen3.5-2B-Q4_K_M.gguf\n')
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', UWPORT_LOOKDBG='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', UWPORT_FOCUSTEST=focus,
             UWPORT_INPUTTRACE='1')
    r = subprocess.run([EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio',
                        '--input-script', sp, '--exit-after', str(exit_ms)], env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    run.err = r.stderr.splitlines()
    return [(int(m.group(1)), int(m.group(2))) for m in re.finditer(r'follower: ` in screen (\d+) at (\d+) ms', r.stderr)]

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''))
    if not ok: fails += 1

def test_death():
    sends = run('death', MENU + "18000 cheat die\n", 40000, '20000,25000')
    bad = [s for s in sends if s[0] != 2]
    check('death: focus lost during the cutscene: no ` outside PS_GAME', not bad, str(bad))

def test_play():
    sends = run('play', MENU, 30000, '20000,22000')
    err = run.err
    graves = [l for l in err if l in ('in: key 29', 'in: key A9')]
    check('play: focus lost and back in PS_GAME: no ` sent (no follower line, no "in: key 29")', not sends and not graves,
          'follower %s, ` bytes %d' % (sends, len(graves)))
    away = next((i for i, l in enumerate(err) if 'focus test: away' in l), None)
    back = next((i for i, l in enumerate(err) if 'focus test: back' in l), None)
    wants = [(i, l.split('mouse-look ')[1].split()[0]) for i, l in enumerate(err) if '(want)' in l and 'mouse-look ' in l]
    off = next((i for i, w in wants if w == 'off' and away is not None and i > away), None)
    on = next((i for i, w in wants if w == 'on' and back is not None and i > back), None)
    check("play: 'mouse-look off (want)' then 'on (want)' around the focus test",
          away is not None and back is not None and off is not None and on is not None and off < on,
          'away line %s, back line %s, wants %s' % (away, back, wants))

os.makedirs(WORK, exist_ok=True)
for t in sys.argv[1:] or ['death', 'play']: globals()['test_' + t]()
print('%d failures' % fails)
sys.exit(fails)
