"""portwin32.py: underworld-exhumed's UW1 port built with our LLVM clang and our Win32 backend, no SDL.

Runs Exhume's own tools/portbuild.py unchanged, on underworld-exhumed/uw1's exhume.toml unchanged, with
three things set from here:
  - the platform backend is "win32": port/win32/plat_win32.c (ours, kept in this repository) copied to
    uw1/src/port/platform/win32/, where portbuild looks for a backend;
  - pkg-config (not on this machine) is answered here: "win32" is the Windows libraries the backend
    links; any other package (mt32emu) is not found, so that part of the port is built silent;
  - clang is O:\\appsnotonc\\llvm's, given the MinGW target as CLANG_FLAGS gives it (env.cmd).
Nothing goes to C:: the temporary files go to the LLVM folder's tmp, as env.cmd sets.

    work\\venv\\Scripts\\python.exe -I tools\\portwin32.py [portbuild options: --debug, --release ...]
The program: underworld-exhumed\\uw1\\build\\port\\uw1port.exe
"""
import os, sys, shutil, subprocess, runpy

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXHUME_TOOLS = r'O:\appsnotonc\exhume\tools'
# --original: uw-original.exe, the byte-matched source untouched (exhumed-original, a worktree of underworld-exhumed
# at its upstream commit) with our backend in plain mode (UWPORT_PLAIN: no renderer, the original's display);
# else uw-new: underworld-exhumed (branch uw-new, our changes) with everything
PLAIN = '--original' in sys.argv
if PLAIN:
    sys.argv.remove('--original')
# --backend sdl3 (the Linux / SDL plan, step 3): our SDL3 backend (port\sdl3\plat_sdl3.c) in place of the Win32 one, with
# the same front end and game wrappers (port\win32's files but plat_win32.c), SDL3 from O:\appsnotonc\sdl3; the program
# goes to build\port-sdl3 with SDL3.dll beside it. The default stays win32 (build\port, the shipped one).
BACKEND = 'win32'
if '--backend' in sys.argv:
    i = sys.argv.index('--backend')
    BACKEND = sys.argv[i + 1] if i + 1 < len(sys.argv) else ''
    del sys.argv[i:i + 2]
    if BACKEND not in ('win32', 'sdl3'):
        sys.exit('portwin32.py: --backend win32 or sdl3')
SDL3 = r'O:\appsnotonc\sdl3\SDL3-3.4.18\x86_64-w64-mingw32'
OUT_NAME = 'port' if BACKEND == 'win32' else 'port-sdl3'
# UWR_AREA=NAME (the user, 2026-11-17: every session builds in its own private area): the program and its objects go
# to underworld-exhumed/uw1/build/port-NAME and the renderer to build/areas/NAME (tools/buildview.cmd reads the same
# variable), so sessions build at the same time and need no build lock. Unset: the shared build/port, as before.
AREA = os.environ.get('UWR_AREA', '').strip()
if AREA:
    OUT_NAME += '-' + AREA
UW1 = os.path.join(HERE, 'exhumed-original' if PLAIN else 'underworld-exhumed', 'uw1')
LLVM_BIN = r'O:\appsnotonc\llvm\23.1.4\bin'
LLVM_TMP = r'O:\appsnotonc\llvm\tmp'
TARGET = ['--target=x86_64-w64-windows-gnu', '--sysroot=O:/appsnotonc/gcc/mingw64', '-fuse-ld=lld']
WIN32_LIBS = ['-luser32', '-lgdi32', '-lwinmm', '-ldwmapi', '-lshell32', '-lole32']

# the backend into place: every file of port\win32 (plat_win32.c, uwgame.c/.h)
srcdir = os.path.join(HERE, 'port', 'win32')
dst = os.path.join(UW1, 'src', 'port', 'platform', BACKEND)
os.makedirs(dst, exist_ok=True)
# (sdl3: port\win32's shared files, not its plat_win32.c, and port\sdl3's own; the Exhume tree's sdl3\icon.h stays)
COPY = [(srcdir, f) for f in os.listdir(srcdir) if BACKEND == 'win32' or f != 'plat_win32.c']
if BACKEND == 'sdl3':
    COPY += [(os.path.join(HERE, 'port', 'sdl3'), f) for f in os.listdir(os.path.join(HERE, 'port', 'sdl3'))]
    # our Esc menu without the renderer (port\sdl3\uwmenu.c): the renderer's own drawing of it, from src\ (the game's
    # letters: smoothtext_none.c, no GDI), in a folder of its own (menu\): src's base.h is not the game's base.h
    COPY += [(os.path.join(HERE, 'src'), os.path.join('menu', f)) for f in ('menu_draw.c', 'menu_draw.h', 'ui.c', 'ui.h',
                                                                            'base.c', 'base.h', 'png.c', 'smoothtext.h',
                                                                            'smoothtext_none.c')]
# (the second review: the copy is untracked there; an edit made to it instead of to port\win32 would be lost
# silently. A copy that differs and is newer than its source is refused: edit port\win32, or pass --force-copy)
FORCE_COPY = '--force-copy' in sys.argv
if FORCE_COPY:
    sys.argv.remove('--force-copy')
for sd, f in COPY:
    a, z = os.path.join(sd, os.path.basename(f)), os.path.join(dst, f)   # (f may name a folder there: menu\)
    os.makedirs(os.path.dirname(z), exist_ok=True)
    if not os.path.exists(z) or open(a, 'rb').read() != open(z, 'rb').read():
        if os.path.exists(z) and os.path.getmtime(z) > os.path.getmtime(a) and not FORCE_COPY:
            sys.exit('portwin32.py: %s differs from port\\win32\\%s and is newer: an edit made to the copy? '
                     'Move it to port\\win32 (or pass --force-copy to overwrite it)' % (z, f))
        shutil.copy2(a, z)
# the game's routines the backend wraps (port\win32\uwgame.c): the 3D frame's copy to the screen
ROM_PAIR = ('cm32l_ctrl_1_02.rom', 'cm32l_pcm.rom')  # the Roland ROMs the port gets (src\sound.c's)
WRAPS = ['-Wl,--wrap=cFBtoScreen', '-Wl,--wrap=get_eye']

os.environ['PATH'] = LLVM_BIN + os.pathsep + os.environ.get('PATH', '')
for k in ('TMP', 'TEMP', 'TMPDIR'):
    os.environ[k] = LLVM_TMP
os.environ['CC'] = os.path.join(LLVM_BIN, 'clang.exe')

real_run = subprocess.run


def run(cmd, *a, **k):
    if isinstance(cmd, (list, tuple)) and cmd:
        exe = os.path.basename(str(cmd[0])).lower()
        if exe in ('pkg-config', 'pkg-config.exe'):
            names = [x for x in cmd[1:] if not x.startswith('-')]
            if names == ['win32']:
                out = []
                if '--libs' in cmd:
                    out += WIN32_LIBS
                if '--modversion' in cmd:
                    out = ['1']
                return subprocess.CompletedProcess(cmd, 0, ' '.join(out) + '\n', '')
            if names == ['sdl3']:   # (--backend sdl3) SDL3's mingw development files, and the Windows libraries
                out = []            # the shared files call (the same as win32's)
                if '--cflags' in cmd:
                    out += ['-I' + os.path.join(SDL3, 'include').replace('\\', '/')]
                if '--libs' in cmd:   # the import library by its path (-static would pass over a .dll.a found by -l)
                    out += [os.path.join(SDL3, 'lib', 'libSDL3.dll.a').replace('\\', '/')] + WIN32_LIBS
                if '--modversion' in cmd:
                    out = ['3.4.18']
                return subprocess.CompletedProcess(cmd, 0, ' '.join(out) + '\n', '')
            if names == ['mt32emu']:  # Munt, ours (tools\buildmunt.cmd): the MT-32 / CM-32L for the music and effects
                out = []
                if '--cflags' in cmd:
                    out += ['-I' + os.path.join(HERE, 'build', 'munt', 'include').replace('\\', '/')]
                if '--libs' in cmd:   # the import library by name: -static would pass over a .dll.a found by -l
                    out += [os.path.join(HERE, 'build', 'munt', 'libmt32emu.dll.a').replace('\\', '/')]
                if '--modversion' in cmd:
                    out = ['2.7.0']
                return subprocess.CompletedProcess(cmd, 0, ' '.join(out) + '\n', '')
            return subprocess.CompletedProcess(cmd, 1, '', 'Package %s was not found\n' % ' '.join(names))
        if exe in ('clang', 'clang.exe'):
            # uw-new: UWPORT_GAMEVIEW, the backend takes the 3D view from the game's own hooks (underworld-exhumed's
            # src/port/portview.h, uw-new only) and our renderer replaces the game's rasterization; uw-original
            # (exhumed-original has no such hooks) builds without it
            # (on since 2026-11-11, the fault fixed: portview.h's PortView lays out naturally on both sides; UWPV_OFF=1
            # builds uw-new with the old frame matching instead, for a comparison)
            cmd = [cmd[0]] + TARGET + (['-DUWPORT_PLAIN'] if PLAIN else ([] if os.environ.get('UWPV_OFF') else ['-DUWPORT_GAMEVIEW'])) + list(cmd[1:])
            if '-c' not in cmd and '-o' in cmd:  # a link: the MinGW runtime (winpthread) inside, so that the
                cmd.append('-static')            # program runs on a machine with nothing installed
                if any(str(x).endswith('.exe') for x in cmd):
                    cmd += WRAPS
    return real_run(cmd, *a, **k)


subprocess.run = run
sys.path.insert(0, EXHUME_TOOLS)
import portcfg  # noqa: E402

real_port = portcfg.port


def port(cfg):
    p = real_port(cfg)
    try:
        p.backend = BACKEND
    except AttributeError:
        p = p._replace(backend=BACKEND)
    if AREA and BACKEND != 'sdl3':   # a private area: a build directory of its own
        p.out = os.path.join(os.path.dirname(p.out), OUT_NAME)
        p.out_name = OUT_NAME
    if BACKEND == 'sdl3':
        # our plat_sdl3.c in place of the runtime's own (both under a platform\sdl3), into a program of its own
        p.runtime_exclude = list(p.runtime_exclude) + [os.path.join(p.runtime_port, 'platform', 'sdl3', 'plat_sdl3.c')]
        p.out = os.path.join(os.path.dirname(p.out), OUT_NAME)
        p.out_name = OUT_NAME
    # our backend compiled with -O2 like the port's translated modules ([port] optimise): unoptimised, the window's
    # per-frame work (the 3D view's state, the interface layer) took over 20 ms a frame at 1920x1080
    if os.path.join(p.dir, 'platform', BACKEND) + os.sep not in p.optimise:
        p.optimise.append(os.path.join(p.dir, 'platform', BACKEND) + os.sep)
    if not PLAIN:
        # (2026-11-16) our own copies of two of the runtime's files, kept in underworld-exhumed\uw1\src\port
        # (sound\ail.c, sys\pit.c: the music plays on while our menu holds the game), built in their place;
        # Exhume's clone stays as upstream has it
        p.runtime_exclude = list(p.runtime_exclude) + [os.path.join(p.runtime_port, 'sound', 'ail.c'),
                                                       os.path.join(p.runtime_port, 'sys', 'pit.c')]
        # (2026-11-17) and sys\borland.c: a handle's text-mode mark ends with the handle (error D004 at start)
        p.runtime_exclude = list(p.runtime_exclude) + [os.path.join(p.runtime_port, 'sys', 'borland.c')]
        # (2026-11-18) and sound\audio.c: the music's Roland gets more voices (Munt's partial count, the Esc menu)
        p.runtime_exclude = list(p.runtime_exclude) + [os.path.join(p.runtime_port, 'sound', 'audio.c')]
    return p


portcfg.port = port
os.chdir(UW1)
sys.argv = [os.path.join(EXHUME_TOOLS, 'portbuild.py'), '--config', 'exhume.toml'] + sys.argv[1:]

# our D3D12 renderer and the DLSS 5 bridge (tools\buildview.cmd), before the port
if not PLAIN and real_run(['cmd', '/c', os.path.join(HERE, 'tools', 'buildview.cmd')]).returncode:
    sys.exit(r'portwin32.py: the renderer (tools\buildview.cmd) did not build')
# Munt for the port's MT-32 / CM-32L (tools\buildmunt.cmd, once)
if real_run(['cmd', '/c', os.path.join(HERE, 'tools', 'buildmunt.cmd')]).returncode:
    sys.exit(r'portwin32.py: Munt (tools\buildmunt.cmd) did not build')


def deploy():
    r"""Everything the program needs, beside it in build\port: the renderer (uwview.dll; without it, or when it
    fails, the port draws with GDI), the DLSS 5 bridge (uw_nvngx.dll, looked for beside the program) and NVIDIA's
    DLSS libraries with their licence (dlss\, which uwview.dll looks for upward from itself)."""
    out = os.path.join(UW1, 'build', OUT_NAME)
    if not os.path.isdir(out):
        return
    b = os.path.join(HERE, 'build')
    v = os.path.join(b, 'areas', AREA) if AREA else b   # the renderer's (buildview.cmd's %B%)
    pairs = [(os.path.join(v, 'uwview.dll'), os.path.join(out, 'uwview.dll')),
             (os.path.join(v, 'uw_nvngx_port.dll'), os.path.join(out, 'uw_nvngx.dll'))]
    if BACKEND == 'sdl3':
        pairs.append((os.path.join(SDL3, 'bin', 'SDL3.dll'), os.path.join(out, 'SDL3.dll')))
    # the Vulkan renderer beside it (docs\superpowers\plans\2026-11-15-vulkan-renderer.md): loaded for renderer vulkan
    if os.path.isfile(os.path.join(v, 'uwview_vk.dll')):
        pairs.append((os.path.join(v, 'uwview_vk.dll'), os.path.join(out, 'uwview_vk.dll')))
    d = os.path.join(HERE, 'dlss')
    if PLAIN:   # uw-original: no renderer, no DLSS; the Roland (Munt and the ROMs) only
        pairs = []
    for f in ([] if PLAIN else os.listdir(d)):
        if f.lower().endswith(('.dll', '.txt')):
            pairs.append((os.path.join(d, f), os.path.join(out, 'dlss', f)))
    # Munt (mt32emu.dll, looked for beside the program) and the user's own Roland ROM images (roms\, which the port
    # finds beside itself and tells apart by their contents: the CM-32L's or MT-32's music and effects)
    pairs.append((os.path.join(b, 'munt', 'mt32emu.dll'), os.path.join(out, 'mt32emu.dll')))
    # the pair our engine played (src\sound.c's first: the CM-32L, control ROM 1.02), those two only (the user)
    r = os.path.join(HERE, 'roms')
    for f in ROM_PAIR:
        if os.path.isfile(os.path.join(r, f)):
            pairs.append((os.path.join(r, f), os.path.join(out, 'roms', f)))
    for a, z in pairs:
        os.makedirs(os.path.dirname(z), exist_ok=True)
        if not os.path.exists(z) or os.path.getmtime(z) < os.path.getmtime(a) or os.path.getsize(z) != os.path.getsize(a):
            shutil.copy2(a, z)
    os.makedirs(os.path.join(out, 'logs'), exist_ok=True)
    print(r'portwin32.py: deployed to %s: uwview.dll, uw_nvngx.dll, dlss\ (%d files)' % (out, len(pairs) - 2))


try:
    runpy.run_path(sys.argv[0], run_name='__main__')
except SystemExit as e:
    if e.code not in (None, 0):
        raise
deploy()
