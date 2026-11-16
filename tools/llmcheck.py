# llmcheck.py: smart-hunting on by default, off without the model files, the Esc menu's model picker
# (docs\superpowers\plans\2026-11-12-llm-default-and-picker.md). Off-screen runs of the test exe from fresh copies of
# work\in2-4; asserted from stderr / stdout and the home's uw1port.cfg.
#   a  enhance=mouse-look,modern-keys (as UW-Original.cmd leaves it): smart-hunting still on
#   b  an exe whose llm\models is empty: "smart-hunting off: no language model", no load tried, no menu row
#   c  smart-hunting=0: off, even with the files present
#   d  llm_model=sub dir\My Model.gguf (a subfolder, spaces; a fake file): "loading the language model My Model.gguf"
#   e  the first start, no llm_model: no picker (the user, 2026-11-13), the default written as llm_model and loaded:
#      our trained LFM2.5-350M-UW (the user, 2026-11-14); that file gone, Qwen3.5-2B; neither, the first found; an
#      llm_model already there wins
#   f  llm_model=lfm (the old value): the LFM file
#   g  the program closed during an uninterruptible load (UWPORT_LLM_STALLTEST): ended after port_mind_stop's 5 s, code 0
#   h  no calibration (the user, 2026-11-13): into VRAM, llm_calibrated neither written nor read; llm_gpu=0 the CPU
# (b d e f h run a copy of the exe with build\llama\bin\uwmind.dll, the runner as built now.)
# Usage: python -I tools\llmcheck.py [a b c d e f g h]. Exit code: the failures.
import os, sys, shutil, subprocess, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port')
EXE = os.path.join(PORT, 'uw1port.exe')
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'llmcheck')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"

def home_copy(name, cfg_edit=None, extra=''):
    home = os.path.join(WORK, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    p = os.path.join(home, 'uw1port.cfg')
    s = open(p).read()
    if cfg_edit: s = cfg_edit(s)
    open(p, 'w').write(s + 'settings-at-start=0\n' + extra)
    return home

def cfg_get(home, key):
    v = None
    for l in open(os.path.join(home, 'uw1port.cfg')):
        if l.startswith(key + '='): v = l[len(key) + 1:].rstrip('\r\n')
    return v

def run(name, home, script, exit_ms, env, exe=EXE):
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', UWPORT_FOCUSTEST='1000,1500', **env)
    for k in [k for k, v in e.items() if v is None]: del e[k]
    cmd = [exe, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio', '--input-script', sp,
           '--exit-after', str(exit_ms)]
    r = subprocess.run(cmd, env=e, capture_output=True, text=True, errors='replace', timeout=exit_ms // 1000 + 120)
    open(os.path.join(WORK, name + '.out'), 'w').write(r.stdout)
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    return r.stdout, r.stderr

def enh_line(err):
    return next((l for l in err.splitlines() if 'uw1port: enhance:' in l), '')

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''))
    if not ok: fails += 1

def exe_copy(name, models):
    """the test exe in a folder of its own: its files copied, its other folders (data, dlss, videos ...) as junctions
    to the build's (made once, never deleted by this script: a tree delete must not reach through them), llm\\bin
    copied, llm\\models as given ({relpath: bytes}, made fresh)"""
    d = os.path.join(WORK, 'exe-' + name)
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(PORT):
        a, z = os.path.join(PORT, f), os.path.join(d, f)
        if f in ('llm', 'logs') or f.endswith('.o'): continue
        if os.path.isdir(a):
            if not os.path.exists(z): subprocess.run(['cmd', '/c', 'mklink', '/J', z, a], capture_output=True)
        else:
            shutil.copy2(a, z)
    if not os.path.isdir(os.path.join(d, 'llm', 'bin')):
        shutil.copytree(os.path.join(PORT, 'llm', 'bin'), os.path.join(d, 'llm', 'bin'))
    built = os.path.join(ROOT, 'build', 'llama', 'bin', 'uwmind.dll')   # the runner as built from src\uwmind.c now
    if os.path.exists(built): shutil.copy2(built, os.path.join(d, 'llm', 'bin', 'uwmind.dll'))
    shutil.rmtree(os.path.join(d, 'llm', 'models'), ignore_errors=True)   # (our fake files only: no junction inside)
    os.makedirs(os.path.join(d, 'llm', 'models'))
    for rel, data in models.items():
        p = os.path.join(d, 'llm', 'models', rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(data)
    return os.path.join(d, 'uw1port.exe')

def test_a():
    home = home_copy('a', lambda s: s.replace('enhance=mouse-look,modern-keys,smart-hunting', 'enhance=mouse-look,modern-keys'))
    out, err = run('a', home, '', 4000, dict(UWPORT_LLM='0'))
    check('a: enhance= line without it (UW-Original.cmd): smart-hunting on', 'smart-hunting' in enh_line(err), enh_line(err))

def test_b():
    exe = exe_copy('b', {})
    home = home_copy('b')
    out, err = run('b', home, '', 4000, dict(UWPORT_LLM=None), exe)
    check('b: llm\\models empty: smart-hunting off, one line', 'smart-hunting off: no language model' in err
          and 'smart-hunting' not in enh_line(err), enh_line(err))
    check('b: no load tried', 'loading the language model' not in out and 'no language model' not in out, out[-400:])

def test_c():
    home = home_copy('c', extra='smart-hunting=0\n')
    out, err = run('c', home, '', 4000, dict(UWPORT_LLM='0'))
    check('c: smart-hunting=0: off with the files present', enh_line(err) and 'smart-hunting' not in enh_line(err), enh_line(err))

def test_d():
    exe = exe_copy('d', {'sub dir\\My Model.gguf': b'not a model\n' * 16, 'Other.gguf': b'not a model either\n'})
    home = home_copy('d', extra='llm_model=sub dir\\My Model.gguf\n')
    out, err = run('d', home, '', 15000, dict(UWPORT_LLM=None), exe)
    check('d: a subfolder and spaces: that file loaded', 'AI: loading the language model My Model.gguf' in out, out[-600:])
    check('d: (the fake file fails, the game goes on)', 'language model failed' in out, out[-600:])

TRAINED = 'LFM2.5-350M-UW\\LFM2.5-350M-UW-r5-f16.gguf'   # the default since 2026-11-14 (the user): the model we trained
QWEN = 'Qwen3.5-2B\\Qwen3.5-2B-Q4_K_M.gguf'             # the default before it, now the next when that file is gone

def test_e():
    """the first start (the user, 11-13: no question any more; 11-14: our trained LFM the default)"""
    home = home_copy('e1')
    out, err = run('e1', home, MENU, 20000, dict(UWPORT_LLM='0', UWPORT_PICKTEST='1'))
    check('e: the first start: no picker, even with UWPORT_PICKTEST', 'first start' not in out and 'pick the language model' not in out, out[-600:])
    check('e: llm_model written as the trained LFM', cfg_get(home, 'llm_model') == TRAINED, str(cfg_get(home, 'llm_model')))
    home = home_copy('e2')
    out, err = run('e2', home, '', 25000, dict(UWPORT_LLM=None))
    check('e: no llm_model: the trained LFM is the one loaded', 'AI: loading the language model LFM2.5-350M-UW-r5-f16.gguf' in out, out[-600:])
    check('e: it comes up', 'AI: language model ready' in out and 'language model failed' not in out, out[-600:])
    exe = exe_copy('e3', {QWEN: b'not a model\n' * 16, 'A.gguf': b'x'})
    home = home_copy('e3')
    out, err = run('e3', home, '', 15000, dict(UWPORT_LLM=None), exe)
    check('e: the trained LFM gone: Qwen is the one tried', 'AI: loading the language model Qwen3.5-2B-Q4_K_M.gguf' in out, out[-600:])
    check('e: and written as the choice', cfg_get(home, 'llm_model') == QWEN, str(cfg_get(home, 'llm_model')))
    exe = exe_copy('e4', {'Only.gguf': b'not a model\n'})
    home = home_copy('e4')
    out, err = run('e4', home, '', 15000, dict(UWPORT_LLM=None), exe)
    check('e: neither there: the first model found', 'AI: loading the language model Only.gguf' in out, out[-600:])
    home = home_copy('e5', extra='llm_model=' + QWEN + '\n')
    out, err = run('e5', home, '', 25000, dict(UWPORT_LLM=None))
    check('e: llm_model=Qwen (a choice made): it wins over the default', 'AI: loading the language model Qwen3.5-2B-Q4_K_M.gguf' in out
          and cfg_get(home, 'llm_model') == QWEN, '%s\n%s' % (cfg_get(home, 'llm_model'), out[-600:]))

def test_f():
    exe = exe_copy('f', {'A.gguf': b'x', 'LFM2.5-1.2B-Instruct\\LFM2.5-1.2B-Instruct-Q4_K_M.gguf': b'y'})
    home = home_copy('f', extra='llm_model=lfm\n')
    out, err = run('f', home, '', 15000, dict(UWPORT_LLM=None), exe)
    check('f: llm_model=lfm: the LFM file', 'AI: loading the language model LFM2.5-1.2B-Instruct-Q4_K_M.gguf' in out, out[-600:])

def test_g():
    """the third review's 1: the program closed while the model loads (a first start on this PC: two loads and the
    calibration): no wait past 5 s into the normal exit, the process ended at once with code 0"""
    home = home_copy('g', lambda s: s.replace('llm_calibrated=gpu\n', ''))
    sp = os.path.join(WORK, 'g.script')
    open(sp, 'w').write('')
    e = dict(os.environ, UWPORT_OFFSCREEN='1', UWPORT_LLM_STALLTEST='20000')   # a load of 20 s that cannot be interrupted
    e.pop('UWPORT_LLM', None)
    t0 = time.time()
    r = subprocess.run([EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio',
                        '--input-script', sp, '--exit-after', '4000'], env=e, capture_output=True, text=True, errors='replace', timeout=120)
    took = time.time() - t0
    open(os.path.join(WORK, 'g.out'), 'w').write(r.stdout)
    open(os.path.join(WORK, 'g.err'), 'w').write(r.stderr)
    check('g: quit during the load: ended after the 5 s wait, not the load, code 0',
          'the program ends without waiting for it' in r.stdout and r.returncode == 0 and took < 18,
          'code %d, %.1f s\n%s' % (r.returncode, took, r.stdout[-600:]))

def test_h():
    """no calibration any more (the user, 11-13): the model into the graphics card's memory unless llm_gpu=0; nothing
    timed, llm_calibrated neither written nor read"""
    exe = exe_copy('h', {'Fake.gguf': b'not a model\n'})
    home = home_copy('h', lambda s: s.replace('llm_calibrated=gpu\n', ''))
    out, err = run('h', home, '', 15000, dict(UWPORT_LLM=None), exe)
    check('h: no llm_calibrated: into VRAM, no calibration run', 'AI: loading the language model Fake.gguf (into VRAM)' in out
          and 'alibrat' not in out, out[-600:])
    check('h: llm_calibrated not written', cfg_get(home, 'llm_calibrated') is None, str(cfg_get(home, 'llm_calibrated')))
    home = home_copy('h2', lambda s: s.replace('llm_calibrated=gpu\n', 'llm_calibrated=cpu\n'))
    out, err = run('h2', home, '', 15000, dict(UWPORT_LLM=None), exe)
    check('h: llm_calibrated=cpu (an older run\'s): not read, into VRAM', 'AI: loading the language model Fake.gguf (into VRAM)' in out, out[-600:])
    home = home_copy('h3', extra='llm_gpu=0\n')
    out, err = run('h3', home, '', 15000, dict(UWPORT_LLM=None), exe)
    check('h: llm_gpu=0: the CPU', 'AI: loading the language model Fake.gguf (CPU)' in out, out[-600:])

os.makedirs(WORK, exist_ok=True)
which = sys.argv[1:] or ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h']
for t in which: globals()['test_' + t]()
print('%d failures' % fails)
sys.exit(fails)
