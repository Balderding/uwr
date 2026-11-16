r"""makepublic.py: the public download, built from dist\UW1 (tools\makedist.py's folder, so run that first on a fresh
build) by copying only named things.

    <python> -I tools\makepublic.py <name>      -> release\<name>\uwr

Nothing in it comes from the game's art or files. Left out on purpose:
  game\ (the original's files), roms\ (Roland ROMs), home\, logs\, videos\, llm\ (models), fonts\,
  ALL of data\ (the game reads the player's own copy in memory; the relief maps are made on the first run by
  laigter\laigter-cli from that copy: src\mapsmake.h; the upscaled textures are derived from the original's art),
  dlss\nvngx_dlssnr.dll (DLSS 5: its licence to pass it on is not known to us),
  uw-original.exe (the untouched original's program: it no longer links against today's backend, 2026-11-16, and
  the one in dist is of 2026-11-10; the game's own menu still gives the 1992 screen).
Put in beside dist's files: laigter\ (the map maker for Windows and Linux from build\, its settings and licence),
the texts of tools\public\, LICENSE.txt, THIRD-PARTY-NOTICES.md and VERSION.txt (the commits it was built from).

Only copies; refuses when the target folder already exists, and when the result holds anything that looks like a
game file, a ROM, a model or a picture.
"""
import os, sys, shutil, subprocess

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(HERE, 'dist', 'UW1')
BUILD = os.path.join(HERE, 'build')
TOOL = os.path.join(HERE, 'tools', 'laigter-cli')
PUB = os.path.join(HERE, 'tools', 'public')
if len(sys.argv) != 2:
    sys.exit(__doc__)
NAME = sys.argv[1]
OUT = os.path.join(HERE, 'release', NAME, 'uwr')
if os.path.exists(os.path.join(HERE, 'release', NAME)):
    sys.exit('makepublic.py: refused: release\\%s exists' % NAME)

TOP = ['uwr.exe', 'uwr-vulkan.exe', 'uwview.dll', 'uwview_vk.dll', 'uw_nvngx.dll', 'mt32emu.dll',
       'play.cmd', 'uwr.cmd', 'keybindings.txt',
       'uwr', 'uwr.sh', 'libuwview.so', 'libSDL3.so.0', 'libmt32emu.so.2', 'libsndio.so.7', 'hdrprobe', 'hdrprobe.sh',
       'readme-linux.txt']
DIRS = ['bin', 'dlss', 'freefonts']
SKIP = {os.path.join('dlss', 'nvngx_dlssnr.dll')}
# what must never be in the result: the game's own files, ROMs, models, pictures of any kind
BAD_NAMES = {'uw.exe', 'uw2.exe', 'lev.ark', 'cnv.ark', 'strings.pak', 'pals.dat', 'sounds.dat', 'uw.mt', 'uw.ad'}
BAD_EXT = {'.ark', '.gr', '.tr', '.byt', '.pak', '.n00', '.n01', '.anm', '.voc', '.xmi', '.rom', '.gguf', '.png',
           '.jpg', '.jpeg', '.bmp', '.sav'}


def git(repo, *args):
    return subprocess.run(['git', '-C', repo] + list(args), capture_output=True, check=True).stdout


# the programs must be the committed source's: refuse on uncommitted changes to tracked files
dirty = [l for l in git(HERE, 'status', '--porcelain', '--untracked-files=no').decode().splitlines() if l.strip()]
if dirty and '--dirty' not in os.environ.get('MAKEPUBLIC', ''):
    sys.exit('makepublic.py: refused: uncommitted changes (the package would not match the source):\n  ' + '\n  '.join(dirty[:20]))

pairs = [(os.path.join(DIST, f), os.path.join(OUT, f)) for f in TOP]
# the map maker (GPL v3, a program of its own) with its settings and licence
pairs += [(os.path.join(BUILD, 'laigter-cli.exe'), os.path.join(OUT, 'laigter', 'laigter-cli.exe')),
          (os.path.join(BUILD, 'linux', 'laigter-cli'), os.path.join(OUT, 'laigter', 'laigter-cli')),
          (os.path.join(TOOL, 'LICENSE-GPL3.txt'), os.path.join(OUT, 'laigter', 'license-gpl3.txt'))]
for f in ('texture.preset', 'sprite_op.preset', 'creature.preset', 'item.preset'):
    pairs.append((os.path.join(TOOL, f), os.path.join(OUT, 'laigter', f)))
for d in DIRS:
    for root, dirs, files in os.walk(os.path.join(DIST, d)):
        for f in files:
            a = os.path.join(root, f)
            rel = os.path.relpath(a, DIST)
            if rel.lower() in SKIP:
                continue
            pairs.append((a, os.path.join(OUT, rel)))
for root, dirs, files in os.walk(PUB):
    for f in files:
        a = os.path.join(root, f)
        rel = os.path.relpath(a, PUB)
        if rel == 'put-your-roms-here.txt':   # (kept at the top of tools\public: .gitignore leaves any roms\ out)
            rel = os.path.join('roms', rel)
        pairs.append((a, os.path.join(OUT, rel)))
for a, z in pairs:
    if not os.path.isfile(a):
        sys.exit('makepublic.py: missing: ' + a)
    n = os.path.basename(z).lower()
    if n in BAD_NAMES or os.path.splitext(n)[1] in BAD_EXT:
        sys.exit('makepublic.py: refused: %s looks like a game file, a ROM, a model or a picture' % a)
total = sum(os.path.getsize(a) for a, z in pairs)
if total > 600 * 1024 * 1024:
    sys.exit('makepublic.py: refused: %d MB' % (total >> 20))
for a, z in pairs:
    os.makedirs(os.path.dirname(z), exist_ok=True)
    shutil.copy2(a, z)

open(os.path.join(OUT, 'LICENSE.txt'), 'wb').write(git(HERE, 'show', 'HEAD:LICENSE'))
open(os.path.join(OUT, 'THIRD-PARTY-NOTICES.md'), 'wb').write(git(HERE, 'show', 'HEAD:THIRD-PARTY-NOTICES.md'))
with open(os.path.join(OUT, 'VERSION.txt'), 'w', newline='\r\n') as f:
    f.write('uwr, %s\n' % NAME)
    f.write('Built from uo-new %s, underworld-exhumed uw-new %s.\n'
            % (git(HERE, 'rev-parse', '--short', 'HEAD').decode().strip(),
               git(os.path.join(HERE, 'underworld-exhumed'), 'rev-parse', '--short', 'HEAD').decode().strip()))
    f.write('Source: https://github.com/Balderding/uwr\n')
print('makepublic.py: %d files, %d MB -> %s' % (len(pairs) + 3, total >> 20, OUT))
