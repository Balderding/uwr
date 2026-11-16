r"""makelinuxdist.py: what the Linux build adds to the game's one folder (dist\UW1), staged in release\linux\ (the Linux /
SDL plan, step 6). tools\makedist.py places these into dist\UW1 beside the Windows files; game\, data\, home\, roms\,
bin\'s Windows files and the rest are the same ones for both systems, nothing duplicated.

    <python> -I tools\makelinuxdist.py      (after tools\portlinux.py)

Staged (lower-case names, as every name in the folder):
  uwr               the game for Linux (underworld-exhumed\uw1\build\port-linux\uw1port: x86-64, glibc 2.34+)
  uwr.sh            starts it as play.cmd starts uwr.exe (game\, home\, roms\ beside it; the log in home\logs)
  libSDL3.so.0         SDL3 beside the program (its RPATH $ORIGIN), by the name the program asks for
  libuwview.so         our Vulkan renderer (tools\buildview_linux.py: build\linux), loaded beside the program as
                       uwview.dll is on Windows (docs\superpowers\plans\2026-11-15-vulkan-renderer.md, M7)
  freefonts\           the free fonts the renderer's text is drawn in on Linux (src\smoothtext_stb.c: Selawik for
                       Segoe UI, DejaVu Sans for Tahoma and Verdana, Carlito, Caladea, Gelasio, Inconsolata; the
                       user's yes 2026-11-15), each with its licence
  dlss\libnvidia-ngx-dlss.so.310.9.16, dlss\libnvidia-ngx-dlssd.so.310.9.16
                       NVIDIA's DLSS and Ray Reconstruction for Linux (the DLSS SDK's release builds), beside the
                       Windows ones in dlss\ (where the renderer tells NGX to look)
  libmt32emu.so.2      Munt (tools/buildmunt_linux.py): the runtime's MT-32 music and uwaudio.c's Roland effects
  libsndio.so.7        sndio, which SDL3 needs and a stock Mint 22 lacks (Debian 13's libsndio7.0)
  bin\libphonon.so     Steam Audio for Linux (where uwsteam.c looks: the game's bin\, as phonon.dll on Windows)
  llm\bin\*.so*        the language model's runner for Linux (tools\buildmind_linux.py: build\linux\llm\bin):
                       libuwmind.so, llama.cpp's own Linux libraries of our tag (libllama.so.0, libggml*.so) and
                       libgomp.so.1, beside the Windows DLLs in llm\bin; the models in llm\models are the same files
                       for both systems (the user, 2026-11-16)
  readme-linux.txt     for the player: what to install on Linux Mint 22, how to start, what works
Library names are the ones the program and the system ask for (libSDL3.so.0, libphonon.so): the dynamic loader
matches them exactly, so they keep their case."""
import os, sys, shutil

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ELF = os.path.join(HERE, 'underworld-exhumed', 'uw1', 'build', 'port-linux', 'uw1port')
SYSROOT = r'O:\appsnotonc\linux-sysroot'
SDL3 = os.path.join(SYSROOT, 'usr', 'lib', 'x86_64-linux-gnu', 'libSDL3.so.0')
MUNT = os.path.join(HERE, 'build', 'munt-linux', 'libmt32emu.so.2')   # (tools/buildmunt_linux.py: the music and our effects)
SNDIO = os.path.join(SYSROOT, 'usr', 'lib', 'x86_64-linux-gnu', 'libsndio.so.7')   # (SDL3 NEEDS it; Mint 22 lacks it)
PHONON = r'O:\appsnotonc\steam-audio\4.8.1\extracted\steamaudio\lib\linux-x64\libphonon.so'
UWVIEW = os.path.join(HERE, 'build', 'linux', 'libuwview.so')   # (tools\buildview_linux.py)
MIND = os.path.join(HERE, 'build', 'linux', 'llm', 'bin')        # (tools\buildmind_linux.py: libuwmind.so and llama.cpp's)
FREEFONTS = os.path.join(HERE, 'freefonts')
NGX_DLSS = [os.path.join(r'O:\appsnotonc\dlss-sdk\lib\Linux_x86_64\rel', f)
            for f in ('libnvidia-ngx-dlss.so.310.9.16', 'libnvidia-ngx-dlssd.so.310.9.16')]
OUT = os.path.join(HERE, 'release', 'linux')
# the packages a stock Linux Mint 22 (Ubuntu 24.04 "noble") may lack, for SDL3 (its NEEDED list) and OpenAL; apt
# skips the ones already there
APT = ('libopenal1 libdecor-0-0 libpipewire-0.3-0t64 libpulse0 libasound2t64 libxss1 libxcursor1 libxi6 '
       'libxrandr2 libxfixes3 libxkbcommon0 libwayland-client0 libwayland-cursor0 libwayland-egl1 libgbm1 libdrm2 libvulkan1')

SH = '''#!/bin/sh
# uwr.sh: Ultima Underworld, ours, on Linux: game/, home/ (saved games, settings) and roms/ beside this file, the same
# ones the Windows game uses. Everything it prints is shown here and added to home/logs/uwr.log (past 5 MB that
# becomes uwr.1.log). Start it with: sh uwr.sh
here=$(cd "$(dirname "$0")" && pwd)
cd "$here" || exit 1
chmod +x ./uwr 2>/dev/null   # (a copy from a Windows disk may have lost the program's "may run" mark)
mkdir -p home/logs
if [ -f home/logs/uwr.log ] && [ "$(wc -c < home/logs/uwr.log)" -gt 5242880 ]; then
    mv -f home/logs/uwr.log home/logs/uwr.1.log
fi
./uwr --data ./game --home ./home --mt32-roms ./roms --enhance mouse-look,modern-keys --autoload "$@" 2>&1 \\
    | tee -a home/logs/uwr.log
'''

README = '''uwr: Ultima Underworld: The Stygian Abyss (1992) on Linux (Linux Mint 22)
============================================================================

The Linux game sits in the same folder as the Windows one and uses the same game/, home/ (saved games, settings),
roms/ and data/. Copy (or mount) the whole folder; nothing else of the game is needed.

Added to the folder for Linux
  uwr            the game for Linux
  uwr.sh         starts it
  libSDL3.so.0      SDL3 (window, keys, mouse, sound); Linux Mint 22 has no SDL3 package
  libsndio.so.7     a sound library SDL3 needs, which Linux Mint 22 does not have
  libmt32emu.so.2   Munt, the Roland MT-32 / CM-32L (the music, and our effects), with the ROMs in roms/
  bin/libphonon.so  Steam Audio (Acoustics High), beside bin/phonon.dll
  llm/bin/*.so*     the small language model's runner (llama.cpp, MIT; libgomp), beside the Windows one; the models
                    in llm/models are the same files Windows uses
  readme-linux.txt  this file

Install once (a terminal). libopenal1 is REQUIRED for our sound effects (each from the place it happens, the
Roland's own sounds through Munt): without it the effects are the music card's, as in the original:
  sudo apt install APT_LINE

Start (a terminal in the folder):
  sh uwr.sh

What works
  The whole game, our menu (Esc: save, load, new game, quit, music and sound, the message box's fade), our keys
  and mouse-look, our inventory panel, the Roland music (Munt, the ROMs in roms/), our sound effects from where they
  happen (OpenAL: libopenal1), the message box.
  Our 3D renderer through Vulkan (libuwview.so): the textures, lights, relief and the effects as on Windows; ray
  tracing on a card that has it; DLSS and Ray Reconstruction on an NVIDIA card with NVIDIA's own driver (the
  "nvidia-driver" one in Linux Mint's Driver Manager, not "nouveau"). Without a Vulkan driver the game shows its own
  320 x 200 picture, scaled. The text in our menus is drawn in free fonts (freefonts/).
  The language-model creatures (Esc, then Level hunters), when the llm/ folder is there: the model runs on the
  graphics card through Vulkan, or on the processor when the card cannot take it.
What does not work yet
  - No HDR (Linux Mint's Cinnamon desktop runs X11, which has none).
  - Shift+P's video recording (P's screenshot to the clipboard works).
  - Linux Mint 21 cannot run it yet (its system is too old for this SDL3).

If something goes wrong, send back
  - home/logs/uwr.log
  - what the terminal shows, if the game does not start
  - if it says a library is missing: the output of   ldd ./uwr
'''.replace('APT_LINE', APT)

for a in (ELF, SDL3, PHONON, SNDIO, MUNT, UWVIEW, *NGX_DLSS, os.path.join(MIND, 'libuwmind.so')):
    if not os.path.isfile(a):
        sys.exit('makelinuxdist.py: missing %s (build first: tools\\portlinux.py; tools\\buildview_linux.py; '
                 'tools\\buildmind_linux.py; tools\\linux_sysroot.py)' % a)
os.makedirs(os.path.join(OUT, 'bin'), exist_ok=True)
for a, z in ((ELF, 'uwr'), (SDL3, 'libSDL3.so.0'), (PHONON, os.path.join('bin', 'libphonon.so')),
             (SNDIO, 'libsndio.so.7'), (MUNT, 'libmt32emu.so.2'), (UWVIEW, 'libuwview.so')):
    shutil.copy2(a, os.path.join(OUT, z))
os.makedirs(os.path.join(OUT, 'freefonts'), exist_ok=True)
for f in sorted(os.listdir(FREEFONTS)):
    shutil.copy2(os.path.join(FREEFONTS, f), os.path.join(OUT, 'freefonts', f))
os.makedirs(os.path.join(OUT, 'dlss'), exist_ok=True)
for a in NGX_DLSS:
    shutil.copy2(a, os.path.join(OUT, 'dlss', os.path.basename(a)))
os.makedirs(os.path.join(OUT, 'llm', 'bin'), exist_ok=True)
for f in sorted(os.listdir(MIND)):   # (libuwmind.so, llama.cpp's libraries, libgomp.so.1 and the two licences)
    shutil.copy2(os.path.join(MIND, f), os.path.join(OUT, 'llm', 'bin', f))
with open(os.path.join(OUT, 'uwr.sh'), 'w', newline='\n') as f:
    f.write(SH)
with open(os.path.join(OUT, 'readme-linux.txt'), 'w', newline='\n') as f:
    f.write(README)
for root, _, files in os.walk(OUT):
    for f in files:
        p = os.path.join(root, f)
        print('makelinuxdist.py: %-28s %10d bytes' % (os.path.relpath(p, OUT), os.path.getsize(p)))
print('makelinuxdist.py: staged in %s; tools\\makedist.py places it in dist\\UW1' % OUT)
