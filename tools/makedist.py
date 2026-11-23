r"""makedist.py: the game as one folder that runs anywhere (another PC, a laptop): dist\UW1 under uo-new.

    O:\appsnotonc\sentinel\uv\python\cpython-3.13.17-windows-x86_64-none\python.exe -I tools\makedist.py

What goes in, and nothing else:
  uwr.exe                the game with all our changes (underworld-exhumed, branch uw-new: tools\portwin32.py)
  uw-original.exe        the byte-matched original, untouched (exhumed-original: tools\portwin32.py --original):
                         its own screen and controls, no renderer
  uwview.dll             our D3D12 renderer (the Esc menu's Renderer row, and when Vulkan cannot start); without it,
                         or when it cannot start either, the game draws with GDI
  uw_nvngx.dll           the DLSS 5 bridge; without it (or on a card without DLSS 5) DLSS alone
  uwview_vk.dll          the Vulkan renderer (docs\superpowers\plans\2026-11-15-vulkan-renderer.md), the default (the user, 2026-11-16)
  uwr-vulkan.exe          starts uwr.exe with the Vulkan renderer for that one run (tools\uw_vulkan_launch.c;
                         built by tools\buildview.cmd into build\)
  mt32emu.dll, roms\     Munt and the user's own Roland ROMs: the CM-32L's music and effects (Play.cmd names roms\)
  dlss\                  NVIDIA's DLSS libraries and their licence; without them, no DLSS (plain rendering)
  bin\                   OpenAL Soft's soft_oal.dll and Steam Audio's phonon.dll (the effects' audio, fx_audio /
                         fx_acoustics, off by default) with their licence files
  game\                  the game's own files (uo-new\game, the md5-checked copy of the user's UW1)
  home\                  saved games and settings (made on the first run)
  Play.cmd, uwr.cmd   start uwr.exe; UW-Original.cmd starts uw-original.exe (game\, home\ shared)
Only these named files and folders are copied, and the total is checked before anything is written (a
whole-tree copy once filled O:). Files already there and the same are left alone; nothing is deleted.

Every name in the folder is lower case (the user, 2026-11-15: "make all file names lower case: it doesn't matter in
Windows but it does in Linux"): the names written, and once, by a rename only (nothing deleted; refused when two names
would become one), what was already there. The one exception: libSDL3.so.0, the name the Linux program asks for.
The Linux files (tools\makelinuxdist.py's release\linux: uwr, uwr.sh, libSDL3.so.0, bin\libphonon.so,
readme-linux.txt) are placed beside the Windows ones when they are staged; game\, data\, home\, roms\ are shared.
Refused while a program of the folder runs.
"""
import os, sys, shutil, subprocess
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)

KEEP_CASE = {'libSDL3.so.0'}   # (a name the dynamic loader matches exactly)


def lower_rel(path, top):
    """path under top with every name below top in lower case (but KEEP_CASE's)"""
    rel = os.path.relpath(path, top)
    return os.path.join(top, *[p if p in KEEP_CASE else p.lower() for p in rel.split(os.sep)])


def running_in(top):
    """the programs running from top (Windows: their paths, by PowerShell)"""
    r = subprocess.run(['powershell', '-NoProfile', '-Command',
                        "Get-Process | Where-Object { $_.Path -and $_.Path.ToLower().StartsWith('%s') } | "
                        "ForEach-Object { $_.Path }" % (os.path.abspath(top).lower().replace("'", "''") + os.sep)],
                       capture_output=True, text=True)
    return [l for l in r.stdout.splitlines() if l.strip()]


def lower_tree(top):
    """every name under top in lower case, by renames only; nothing done when two would become one (they are listed)"""
    todo, clash = [], []
    for root, dirs, files in os.walk(top, topdown=False):
        names = dirs + files
        seen = {}
        for n in names:
            seen.setdefault(n if n in KEEP_CASE else n.lower(), []).append(n)
        for low, ns in seen.items():
            if len(ns) > 1:
                clash.append('%s: %s' % (root, ', '.join(ns)))
            elif ns[0] != low:
                todo.append((os.path.join(root, ns[0]), os.path.join(root, low)))
    if clash:
        sys.exit('makedist.py: names that would become one in lower case (nothing renamed):\n  ' + '\n  '.join(clash))
    for a, z in todo:
        os.rename(a, z)   # (a case-only rename: NTFS keeps the one file)
    return len(todo)

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(HERE, 'underworld-exhumed', 'uw1', 'build', 'port')
PORT_ORIGINAL = os.path.join(HERE, 'exhumed-original', 'uw1', 'build', 'port')
GAME = os.path.join(HERE, 'game')
OUT = os.path.join(HERE, 'dist', 'UW1')
LIMIT = 1000 * 1024 * 1024       # (+ the model runner's DLLs, ~80 MB; + data\upscaled, ~50 MB: the user 2026-11-14)
#        # about 290 MB: NVIDIA's DLSS libraries 270 MB, the game 11 MB, ours 6 MB; more
                                 # means something is wrong

GAME_FILES = ['UW.EXE']
GAME_DIRS = ['CRIT', 'CUTS', 'DATA', 'SOUND']

pairs = [(os.path.join(PORT, f), os.path.join(OUT, f)) for f in ('uwview.dll', 'uw_nvngx.dll', 'mt32emu.dll', 'uwview_vk.dll')]
pairs += [(os.path.join(HERE, 'build', 'uwr-vulkan.exe'), os.path.join(OUT, 'uwr-vulkan.exe'))]
pairs += [(os.path.join(PORT, 'uw1port.exe'), os.path.join(OUT, 'uwr.exe')),
          (os.path.join(PORT_ORIGINAL, 'uw1port.exe'), os.path.join(OUT, 'uw-original.exe'))]
# the user's own Roland ROM images (the CM-32L / MT-32 for the music and effects; Play.cmd names the folder)
for f in ('cm32l_ctrl_1_02.rom', 'cm32l_pcm.rom'):  # the pair our engine played (src\sound.c's), those two only
    pairs.append((os.path.join(PORT, 'roms', f), os.path.join(OUT, 'roms', f)))
for f in sorted(os.listdir(os.path.join(PORT, 'dlss'))):
    pairs.append((os.path.join(PORT, 'dlss', f), os.path.join(OUT, 'dlss', f)))
# the language model's runner (LLM_NPC_AI.md): llama.cpp's DLLs, every CPU variant, Vulkan, and our uwmind.dll
# (tools\buildllama.cmd vulkan, tools\buildmind.cmd) into llm\bin; the models already sit in llm\models (staged
# by hand, sha-checked: not copied from here)
LLAMA = os.path.join(HERE, 'build', 'llama', 'bin')
if os.path.isdir(LLAMA):
    for f in sorted(os.listdir(LLAMA)):
        if f.lower().endswith('.dll'):
            pairs.append((os.path.join(LLAMA, f), os.path.join(OUT, 'llm', 'bin', f)))
# the effects' audio (THIRD-PARTY-NOTICES.md; port\win32\uwaudio.c loads them from bin\ when fx_audio / fx_acoustics
# are on): OpenAL Soft 1.25.2 (LGPL 2.1, its COPYING beside it) and Steam Audio 4.8.1 (Apache 2.0, its notices)
OPENAL = TOOLS + r'\openal-soft\1.25.2\extracted\openal-soft-1.25.2-bin'
STEAMAUDIO = TOOLS + r'\steam-audio\4.8.1\extracted\steamaudio'
pairs += [(os.path.join(OPENAL, 'bin', 'Win64', 'soft_oal.dll'), os.path.join(OUT, 'bin', 'soft_oal.dll')),
          (os.path.join(OPENAL, 'COPYING'), os.path.join(OUT, 'bin', 'OpenAL-Soft-COPYING.txt')),
          (os.path.join(STEAMAUDIO, 'lib', 'windows-x64', 'phonon.dll'), os.path.join(OUT, 'bin', 'phonon.dll')),
          (os.path.join(STEAMAUDIO, 'THIRDPARTY.md'), os.path.join(OUT, 'bin', 'SteamAudio-THIRDPARTY.md'))]
# every key and what the folder holds (the user, 2026-11-13: a keybindings.txt in the package)
pairs.append((os.path.join(HERE, 'tools', 'keybindings.txt'), os.path.join(OUT, 'keybindings.txt')))
for f in GAME_FILES:
    pairs.append((os.path.join(GAME, f), os.path.join(OUT, 'game', f)))
# the user's own normal maps (made with Laigter: tools\laigter-cli), which nothing can make again from the game's
# files; the rest of data\ the renderer extracts from game\ on the first run
for root, dirs, files in os.walk(os.path.join(HERE, 'data', 'laigter')):
    for f in files:
        a = os.path.join(root, f)
        pairs.append((a, os.path.join(OUT, 'data', os.path.relpath(a, os.path.join(HERE, 'data')))))
# the walls' and floors' upscaled textures (the Esc menu's Textures: tools\make_upscaled_data.py from the user's
# ComfyUI run, Balderding/uw-new-upscaled), likewise not made again here
for root, dirs, files in os.walk(os.path.join(HERE, 'data', 'upscaled')):
    for f in files:
        a = os.path.join(root, f)
        pairs.append((a, os.path.join(OUT, 'data', os.path.relpath(a, os.path.join(HERE, 'data')))))
for d in GAME_DIRS:
    for root, dirs, files in os.walk(os.path.join(GAME, d)):
        for f in files:
            a = os.path.join(root, f)
            pairs.append((a, os.path.join(OUT, 'game', os.path.relpath(a, GAME))))

# the Linux game (tools\makelinuxdist.py's staging), beside the Windows one, sharing the rest
LINUX = os.path.join(HERE, 'release', 'linux')
if os.path.isfile(os.path.join(LINUX, 'uwr')):
    for root, dirs, files in os.walk(LINUX):
        for f in files:
            if f in ('uw-new', 'uw-new.sh'):   # (the staging's files from before the game was named uwr)
                continue
            a = os.path.join(root, f)
            pairs.append((a, os.path.join(OUT, os.path.relpath(a, LINUX))))
# every name in lower case (the Linux game finds them as the Windows one does)
pairs = [(a, lower_rel(z, OUT)) for a, z in pairs]

for a, _ in pairs:
    if not os.path.isfile(a):
        sys.exit('makedist.py: missing %s (build first: tools\\portwin32.py)' % a)
busy = running_in(OUT)
if busy:
    sys.exit('makedist.py: refused, running from the folder: %s' % ', '.join(busy))
renamed = lower_tree(OUT) if os.path.isdir(OUT) else 0
if renamed:
    print('makedist.py: %d names in %s made lower case' % (renamed, OUT))
total = sum(os.path.getsize(a) for a, _ in pairs)
if total > LIMIT:
    sys.exit('makedist.py: %d MB to copy, more than the %d MB limit: refused' % (total >> 20, LIMIT >> 20))

copied = 0
for a, z in pairs:
    os.makedirs(os.path.dirname(z), exist_ok=True)
    if os.path.exists(z) and os.path.getsize(z) == os.path.getsize(a) and os.path.getmtime(z) >= os.path.getmtime(a):
        continue
    shutil.copy2(a, z)
    copied += 1
os.makedirs(os.path.join(OUT, 'home'), exist_ok=True)
NEW = ('@echo off\n'
       'rem uwr: Ultima Underworld, ours (uwr.exe): game\\ and home\\ (saved games, settings) beside this file.\n'
       'rem Everything it prints (the AI log, Ctrl+L, and any crash report) is added to home\\logs\\uwr.log.\n'
       'rem The log is rotated: past 5 MB it becomes uwr.1.log (replacing the older one) and a new one starts.\n'
       'if not exist "%~dp0home\\logs" mkdir "%~dp0home\\logs"\n'
       'if exist "%~dp0home\\logs\\uwr.log" for %%F in ("%~dp0home\\logs\\uwr.log") do'
       ' if %%~zF GTR 5242880 move /y "%%~fF" "%~dp0home\\logs\\uwr.1.log" >nul\n'
       'start "" cmd /c ""%~dp0uwr.exe" --data "%~dp0game" --home "%~dp0home" --mt32-roms "%~dp0roms" --enhance mouse-look,modern-keys --autoload %* >> "%~dp0home\\logs\\uwr.log" 2>&1"\n')
for name in ('play.cmd', 'uwr.cmd'):
    with open(os.path.join(OUT, name), 'w', newline='\r\n') as f:
        f.write(NEW)
with open(os.path.join(OUT, 'uw-original.cmd'), 'w', newline='\r\n') as f:
    f.write('@echo off\n'
            'rem Ultima Underworld as it was (uw-original.exe, the byte-matched source untouched): its own screen and\n'
            'rem controls, the same game\\ and home\\ (saved games) as uwr.exe, our enhancements off.\n'
            'start "" "%~dp0uw-original.exe" --data "%~dp0game" --home "%~dp0home" --mt32-roms "%~dp0roms" --no-enhance mouse-look,modern-keys %*\n')
with open(os.path.join(OUT, 'readme.txt'), 'w', newline='\r\n') as f:
    f.write('uwr: Ultima Underworld: The Stygian Abyss (1992), the original game on Windows and Linux.\n\n'
            'Start play.cmd (on Linux: sh uwr.sh; readme-linux.txt). Saved games and settings go in home\\. F11 opens the settings.\n\n'
            'The picture goes through a Vulkan renderer (uwview_vk.dll); Esc, then Renderer, chooses the\n'
            'Direct3D 12 one (uwview.dll), which is also used when Vulkan cannot start. If neither can, the game\n'
            'draws the same picture with plain Windows drawing instead. DLSS and DLSS 5 are used only\n'
            'where the graphics card and its driver have them; without them the game runs the same.\n\n'
            'Up to two clever creatures a level hunt you: they notice your noises and lights and plan\n'
            'with the small language model in llm\\. Esc, then Level hunters: off, 1 or 2 a level.\n\n'
            'dlss\\: NVIDIA DLSS, under NVIDIA-DLSS-SDK-LICENSE.txt. The game data in game\\ is the\n'
            "owner's own copy of UW1.\n")
print('makedist.py: %s: %d files (%.1f MB), %d copied now' % (OUT, len(pairs), total / 1048576.0, copied))
