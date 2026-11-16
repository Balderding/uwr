r"""makeflat.py: the game as ONE folder with the original game's files as they are (the user, 2026-11-16):

    dist\uwr\            UW.EXE, DATA\, CRIT\, CUTS\, SOUND\   the original game, untouched, not extracted
                         uwr.exe, the renderer's DLLs, dlss\, bin\, roms\, fonts\, freefonts\   ours
                         uwr\                                  our own files: settings.txt, laigter\, upscaled\, home\

Copies only (nothing deleted, nothing moved); dist\UW1 stays as it is. Sources: the fresh build (underworld-exhumed's
build\port, build\), uo-new\game, and dist\UW1 for what is not built here (dlss\, bin\, roms\, fonts, the user's home
and settings). llm\ (2.6 GB, the model for the off-by-default hunters) is not copied unless --llm is given.
Refused while a program of the folder runs; the total is checked before anything is written.
"""
import os, sys, shutil, subprocess
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(HERE, 'underworld-exhumed', 'uw1', 'build', 'port')
BUILD = os.path.join(HERE, 'build')
OLD = os.path.join(HERE, 'dist', 'UW1')
GAME = os.path.join(HERE, 'game')
OUT = os.path.join(HERE, 'dist', 'uwr')
LIMIT = 1200 * 1024 * 1024 + (3000 * 1024 * 1024 if '--llm' in sys.argv else 0)

jobs = []   # (source file, destination file)
def tree(src, dst, skip=()):
    for r, ds, fs in os.walk(src):
        ds[:] = [d for d in ds if d.lower() not in skip]
        for f in fs:
            a = os.path.join(r, f)
            jobs.append((a, os.path.join(dst, os.path.relpath(a, src))))
def one(src, dst):
    if os.path.exists(src): jobs.append((src, dst))
    else: print('makeflat.py: not there, left out:', src)

# the original game, as it is (names as in uo-new\game)
for n in os.listdir(GAME):
    p = os.path.join(GAME, n)
    if os.path.isdir(p): tree(p, os.path.join(OUT, n))
    else: one(p, os.path.join(OUT, n))
# ours: the program and the renderers (fresh builds), then what dist\UW1 carries
one(os.path.join(PORT, 'uw1port.exe'), os.path.join(OUT, 'uwr.exe'))
for n in ('uwview.dll', 'uwview_vk.dll'):
    one(os.path.join(BUILD, n), os.path.join(OUT, n))
one(os.path.join(PORT, 'uw_nvngx.dll'), os.path.join(OUT, 'uw_nvngx.dll'))
for n in ('mt32emu.dll', 'uwr-vulkan.exe', 'keybindings.txt'):
    one(os.path.join(OLD, n), os.path.join(OUT, n))
for d in ('dlss', 'bin', 'roms', 'fonts', 'freefonts') + (('llm',) if '--llm' in sys.argv else ()):
    if os.path.isdir(os.path.join(OLD, d)): tree(os.path.join(OLD, d), os.path.join(OUT, d))
# our own files: uwr\ (dist\UW1\data's own, not the extracted copies), and the user's home
OURS = os.path.join(OUT, 'uwr')
for d in ('laigter', 'upscaled', 'sfx', 'lights'):
    if os.path.isdir(os.path.join(OLD, 'data', d)): tree(os.path.join(OLD, 'data', d), os.path.join(OURS, d))
for n in ('settings.txt', 'keys.txt', 'acoustics.txt', 'lights.txt'):
    if os.path.exists(os.path.join(OLD, 'data', n)): one(os.path.join(OLD, 'data', n), os.path.join(OURS, n))
if os.path.isdir(os.path.join(OLD, 'home')): tree(os.path.join(OLD, 'home'), os.path.join(OURS, 'home'), skip=('logs',))

total = sum(os.path.getsize(a) for a, _ in jobs)
print('makeflat.py: %d files, %d MB' % (len(jobs), total >> 20))
if total > LIMIT: sys.exit('makeflat.py: more than %d MB: something is wrong, nothing written' % (LIMIT >> 20))
r = subprocess.run(['powershell', '-NoProfile', '-Command',
                    "Get-Process | Where-Object { $_.Path -and $_.Path.ToLower().StartsWith('%s') } | ForEach-Object { $_.Path }"
                    % (os.path.abspath(OUT).lower().replace("'", "''") + os.sep)], capture_output=True, text=True)
if r.stdout.strip(): sys.exit('makeflat.py: a program of the folder runs, nothing written:\n' + r.stdout)
copied = same = 0
for a, z in jobs:
    if os.path.exists(z) and os.path.getsize(z) == os.path.getsize(a) and int(os.path.getmtime(z)) == int(os.path.getmtime(a)):
        same += 1
        continue
    os.makedirs(os.path.dirname(z), exist_ok=True)
    shutil.copy2(a, z)
    copied += 1
print('makeflat.py: %s: %d copied, %d already there' % (OUT, copied, same))
