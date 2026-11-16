# format2check.py: the creature questions the game sends are FORMAT 2, character for character (the user, 2026-11-13:
# the fine-tuned LFM2.5-350M-UW was trained on it). The reference is tools\mind_format.py (the fine-tune folder's own
# copy, O:\appsnotonc\finetune\mind_format.py). An off-screen run with the language model on (UWPORT_LLM=1, the
# fine-tuned model) and the AI log (UWPORT_AILOG=1): level 1 by the goblin #218 (huntercheck's spot), made hostile.
# Every question the game logs of kind 1 (lost sight: "asks the model") and kind 4 ("sees the player and asks the
# model") is read back into the builders' arguments (name, wits from "(int N)", distance and direction, the creature's
# health, the ambush choice, the adventurer's look, weapon and light, the facts) and built again with mind_format's
# lost_player / sees_player: the two strings must be equal. Every hunter's "you knew where the adventurer was" names a
# look of LOOKS when it says one. At least one kind 4 question asked.
# Usage: python -I tools\format2check.py   (loads the model: not in tools\verify.cmd; about a minute and a half)
import os, sys, re, shutil, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import mind_format as mf  # noqa: E402

EXE = os.environ.get('FORMAT2_EXE') or os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port', 'uw1port.exe')
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'format2check')
MODEL = os.path.join(ROOT, 'dist', 'UW1', 'llm', 'models', 'LFM2.5-350M-UW')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''))
    if not ok: fails += 1

DIRS = {d: i for i, d in enumerate(mf.DIRS)}
LIGHT = {'in the dark': 0, 'carrying a candle': 1, 'carrying a torch': 3, 'carrying a bright lantern': 4}
WHO = re.compile(r'^You are an? (?P<name>[a-z ]+?), (?:unhurt|wounded: health (?P<hp>\d+) of (?P<avg>\d+))\.')
WHERE = re.compile(r'(?P<d>\d+) squares to your (?P<dir>[a-z-]+)|right where you stand')
PLAYER = re.compile(r' (?:The adventurer looks|When you last saw them the adventurer looked) (?P<look>unhurt|hurt|badly hurt|near death), '
                    r'(?P<drawn>with a weapon drawn|no weapon in hand), (?P<light>in the dark|carrying a candle|carrying a torch|carrying a bright lantern)\.')

def rebuild(kind, wits, user):
    """the builders' string for what the logged question says (None when it cannot be read back)"""
    w, p = WHO.match(user), PLAYER.search(user)
    m = WHERE.search(user, w.end() if w else 0)
    if not (w and p and m): return None
    hp, avg = (int(w['hp']), int(w['avg'])) if w['hp'] else (None, None)
    d, direction = (int(m['d']), DIRS[m['dir']]) if m['d'] else (0, 0)
    known = user[p.end():user.index(' Choices: ')]
    look, drawn, light = mf.LOOKS.index(p['look']), p['drawn'] == 'with a weapon drawn', LIGHT[p['light']]
    if kind == 4:
        amb = 'ambush (pull back' in user
        return mf.sees_player(w['name'], wits, d, direction, hp, avg, ambusher=amb, known=known,
                              look=look, drawn=drawn, light=light)[0]
    amb, rally = 'ambush (hide in the dark' in user, 'rally (call your kin' in user
    # (rally is offered when kin are within hearing and wits >= 10: kin=1 stands for any number there)
    return mf.lost_player(w['name'], wits, d, direction, hp, avg, ambusher=amb or ('You are good at ambushes.' in user),
                          known=known, kin=1 if rally else 0, look=look, drawn=drawn, light=light)[0]

def main():
    os.makedirs(WORK, exist_ok=True)
    home = os.path.join(WORK, 'h')
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f:
        # (smart-hunting: hunt.c's questions, the user's own setting; llm_hunters=0: #218 not taken as a level hunter,
        # whose questions are plan steps; quick_fades off as in every proof)
        f.write('settings-at-start=0\nenhance=mouse-look,modern-keys,smart-hunting\nllm_hunters=0\nquick_fades=0\n')
    sp = os.path.join(WORK, 'run.script')
    # (#218 stands at 11,38: the player put on the open square beside it, 10,38, then it made hostile)
    open(sp, 'w').write(MENU + "15000 cheat goto 1 10 38\n15500 cheat anger 218\n16000 cheat where\n")
    gguf = [f for f in os.listdir(MODEL) if f.lower().endswith('.gguf')] if os.path.isdir(MODEL) else []
    gguf.sort(key=lambda f: ('q8' not in f.lower(), f))
    check('the fine-tuned model is there (%s)' % MODEL, bool(gguf))
    if not gguf: return
    e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='1', UWPORT_AILOG='1',
             UWPORT_FOCUSTEST='1000,1500', UWPORT_LLM_MODEL=os.path.join(MODEL, gguf[0]))
    r = subprocess.run([EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio',
                        '--input-script', sp, '--exit-after', '100000'], env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, 'run.out'), 'w').write(r.stdout)
    open(os.path.join(WORK, 'run.err'), 'w').write(r.stderr)
    asks = []
    for l in r.stdout.splitlines():
        m = re.search(r'\(int (\d+)\) (sees the player and )?asks the model: (.*)$', l)
        if m and '(int %s, friendly)' % m[1] not in l:
            kind = 4 if m[2] else (1 if 'lost sight of them' in m[3] else 2)
            asks.append((kind, int(m[1]), m[3]))
    k4 = [a for a in asks if a[0] == 4]
    k1 = [a for a in asks if a[0] == 1]
    check('a creature saw the player and asked (kind 4: %d, kind 1: %d)' % (len(k4), len(k1)), bool(k4),
          'see %s' % os.path.join(WORK, 'run.out'))
    bad = []
    for kind, wits, user in k4 + k1:
        want = rebuild(kind, wits, user)
        if want != user: bad.append((kind, user, want))
    check('every kind 1 and 4 question is mind_format\'s FORMAT 2 string (%d)' % (len(k4) + len(k1)), not bad,
          '\n game: %r\n want: %r' % (bad[0][1], bad[0][2]) if bad else '')
    looks = re.findall(r'you knew where the adventurer was: [^.]*?(?:, looking ([a-z ]+))?\.', r.stdout)
    check('the hunters\' "looking" words are LOOKS (%d said)' % sum(1 for x in looks if x),
          all(x in mf.LOOKS for x in looks if x))
    other = [a for a in asks if a[0] == 2]
    check('no adventurer\'s look in a noise or light question (%d)' % len(other),
          not any(PLAYER.search(u) for _, _, u in other))

if __name__ == '__main__':
    main()
    print('format2check: %s' % ('PASS' if not fails else '%d FAIL' % fails))
    sys.exit(fails)
