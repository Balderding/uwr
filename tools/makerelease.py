"""makerelease.py: dist\\UW1 packed to carry to another PC: release\\UW1-<date> (a folder) and release\\UW1-<date>.zip.

Everything the game needs, beside the program: game\\ (the owner's UW1), the renderer and DLSS, the Roland ROMs
(roms\\), the language model's runner and models (llm\\), and home\\ (the saved games and settings) with this PC's
paths taken out of the settings (the program run bare finds game\\, home\\ and roms\\ beside itself). Not taken:
logs\\, videos\\, home\\recordings\\ (crash replays of this PC's sessions). Nothing is deleted: a folder or zip that
already exists is refused.

    python -I tools\\makerelease.py [NAME]        NAME default UW1-<today>
"""
import datetime, os, shutil, subprocess, sys, zipfile

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(HERE, 'dist', 'UW1')
name = sys.argv[1] if len(sys.argv) > 1 else 'UW1-' + datetime.date.today().isoformat()
OUT = os.path.join(HERE, 'release', name)
ZIP = OUT + '.zip'
if os.path.exists(OUT) or os.path.exists(ZIP):
    sys.exit('makerelease.py: %s or its zip already exists: refused (nothing is deleted)' % OUT)

SKIP_TOP = {'logs', 'videos'}
SKIP_HOME = {'recordings'}


def ignore(d, names):
    rel = os.path.relpath(d, DIST)
    if rel == '.':
        return [n for n in names if n in SKIP_TOP]
    if rel == 'home':
        return [n for n in names if n in SKIP_HOME]
    return []


shutil.copytree(DIST, OUT, ignore=ignore)

# the settings without this PC's paths (data=, mt32-roms=): run bare, the program takes the folders beside it
cfg = os.path.join(OUT, 'home', 'uw1port.cfg')
if os.path.isfile(cfg):
    lines = [l for l in open(cfg, encoding='utf-8', errors='replace').read().splitlines()
             if not l.startswith(('data=', 'mt32-roms='))]
    open(cfg, 'w', encoding='utf-8', newline='\r\n').write('\n'.join(lines) + '\n')


def head(repo):
    return subprocess.run(['git', '-C', repo, 'log', '--oneline', '-1'], capture_output=True, text=True).stdout.strip()


open(os.path.join(OUT, 'VERSION.txt'), 'w', newline='\r\n').write(
    'Ultima Underworld (uw-new), release %s\nBuilt from uo-new %s\nand underworld-exhumed uw-new %s\n'
    % (datetime.date.today().isoformat(), head(HERE), head(os.path.join(HERE, 'underworld-exhumed'))))

open(os.path.join(OUT, 'README.txt'), 'a', newline='\r\n').write('''
To start: run uw-new.exe (or Play.cmd). It finds game\\, home\\ and roms\\ beside itself and goes into the
newest saved game. Copy the whole folder anywhere; nothing is installed. Needs Windows 10 or 11 (64-bit) and a
graphics card with Direct3D 12.

Music and effects: roms\\ holds the Roland CM-32L ROM images (the owner's own); the game plays through them.

Creatures that hunt (smart-hunting, our own; on in home\\uw1port.cfg's enhance= line, off with
--no-enhance smart-hunting): a creature that loses you hunts where it last saw you, goes to look at noises and
light, and clever ones lie in ambush. A language model (llm\\models, run by llm\\bin's llama.cpp) chooses among
those as it happens: on the graphics card through Vulkan (any NVIDIA, AMD or Intel card's driver has it), else
on the processor. The first start on a new PC takes about 10 s more while the driver prepares the model's
shaders (once). Without llm\\, the rules decide alone. Settings: llm=0 turns the model off, llm_gpu=0 uses the
processor only.

Ctrl+L: the AI log: every creature decision and the model's questions and answers, in the message box and in
the console window (start uw-new.exe from a command prompt to keep them).

Keys (our display): F5 save, F9 load (our own pages), Esc the options, Tab the inventory, Shift+L full bright.
''')

# the zip: the models and DLLs stored as they are (they do not shrink), the rest compressed
STORE = ('.gguf', '.dll', '.exe', '.rom', '.png')
n = 0
with zipfile.ZipFile(ZIP, 'w', allowZip64=True) as z:
    for root, dirs, files in os.walk(OUT):
        for f in files:
            a = os.path.join(root, f)
            arc = os.path.join(name, os.path.relpath(a, OUT))
            z.write(a, arc, zipfile.ZIP_STORED if f.lower().endswith(STORE) else zipfile.ZIP_DEFLATED)
            n += 1
print('makerelease.py: %s (%d files), %s (%.2f GB)' % (OUT, n, ZIP, os.path.getsize(ZIP) / 1073741824.0))
