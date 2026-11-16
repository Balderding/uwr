r"""portlinux.py: underworld-exhumed's UW1 port cross-built for x86-64 Linux on this Windows machine (the Linux / SDL
plan, steps 4 and 5), with our LLVM clang and lld against the Debian sysroot tools\linux_sysroot.py makes. The program
cannot be run here: Windows does not run Linux programs. The target is Linux Mint (21.x: glibc 2.35; 22.x: 2.39).

Runs Exhume's own tools/portbuild.py unchanged, on uw1's exhume.toml unchanged, with:
  - every clang call given the Linux target and the sysroot (--target=x86_64-linux-gnu --sysroot=...; a link also
    -fuse-ld=lld);
  - pkg-config answered here: "sdl3" is the sysroot's SDL3 (Debian's libsdl3-dev); any other package (mt32emu, not in
    Debian 13's main) is not found, so that part of the port is built without it;
  - the program written to <tree>\uw1\build\port-linux\uw1port (an ELF).

    <python> -I tools\portlinux.py              uw-new (underworld-exhumed, branch uw-new) with OUR backend: port\sdl3's
                                                plat_sdl3.c and port\win32's shared files (uwfront.c, uwgame.c, uwaudio.c
                                                ... not plat_win32.c), copied into uw1\src\port\platform\sdl3 as
                                                portwin32.py --backend sdl3 copies them; the runtime's own plat_sdl3.c
                                                left out; -DUWPORT_GAMEVIEW and uwgame.c's two --wrap links as on Windows;
                                                RUNPATH $ORIGIN (SDL3 and the rest found beside the program). Without our
                                                3D renderer (uwview.dll is Windows'): the original's picture through SDL.
    <python> -I tools\portlinux.py --original   upstream's UW1 port with nothing of ours (exhumed-original, the worktree
                                                of underworld-exhumed at its upstream commit), the runtime's plat_sdl3.c
The two Windows builds (portwin32.py, and its --backend sdl3) are not touched.
"""
import os, sys, shutil, subprocess, runpy

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXHUME_TOOLS = r'O:\appsnotonc\exhume\tools'
PLAIN = '--original' in sys.argv
if PLAIN:
    sys.argv.remove('--original')
UW1 = os.path.join(HERE, 'exhumed-original' if PLAIN else 'underworld-exhumed', 'uw1')
LLVM_BIN = r'O:\appsnotonc\llvm\23.1.4\bin'
LLVM_TMP = r'O:\appsnotonc\llvm\tmp'
SYSROOT = os.environ.get('LINUX_SYSROOT', r'O:\appsnotonc\linux-sysroot')
TARGET = ['--target=x86_64-linux-gnu', '--sysroot=' + SYSROOT.replace('\\', '/'), '-fuse-ld=lld']
OUT_NAME = 'port-linux'
MUNT = os.path.join(HERE, 'build', 'munt-linux', 'libmt32emu.so.2')   # (tools/buildmunt_linux.py)
# uw-new's link: uwgame.c wraps the game's 3D frame's copy and its eye (as portwin32.py's WRAPS); the program looks for
# its libraries beside itself first (RUNPATH: lld's default --enable-new-dtags), then the system's
LINK_EXTRA = [] if PLAIN else ['-Wl,--wrap=cFBtoScreen', '-Wl,--wrap=get_eye', '-Wl,-rpath,$ORIGIN', '-Wl,--disable-new-dtags', '-lm']   # (DT_RPATH, not RUNPATH: a library's own NEEDED (SDL3's libsndio.so.7) is looked for beside the program too; libm: our
# files' float maths; glibc keeps it apart from libc)

if not os.path.isfile(os.path.join(SYSROOT, 'usr', 'include', 'SDL3', 'SDL.h')):
    sys.exit(r'portlinux.py: no sysroot at %s (make it: tools\linux_sysroot.py)' % SYSROOT)

# uw-new: our backend into place (portwin32.py --backend sdl3's copy: port\win32's files but plat_win32.c, port\sdl3's)
if not PLAIN:
    srcdir, dst = os.path.join(HERE, 'port', 'win32'), os.path.join(UW1, 'src', 'port', 'platform', 'sdl3')
    copy = [(srcdir, f) for f in os.listdir(srcdir) if f != 'plat_win32.c']
    copy += [(os.path.join(HERE, 'port', 'sdl3'), f) for f in os.listdir(os.path.join(HERE, 'port', 'sdl3'))]
    # our Esc menu without the renderer (uwmenu.c): src\'s drawing of it (the game's letters: smoothtext_none.c), in a
    # folder of its own (menu\): src's base.h is not the game's base.h
    copy += [(os.path.join(HERE, 'src'), os.path.join('menu', f)) for f in ('menu_draw.c', 'menu_draw.h', 'ui.c', 'ui.h',
                                                                            'base.c', 'base.h', 'png.c', 'smoothtext.h',
                                                                            'smoothtext_none.c')]
    for sd, f in copy:
        a, z = os.path.join(sd, os.path.basename(f)), os.path.join(dst, f)   # (f may name a folder there: menu\)
        os.makedirs(os.path.dirname(z), exist_ok=True)
        if not os.path.exists(z) or open(a, 'rb').read() != open(z, 'rb').read():
            if os.path.exists(z) and os.path.getmtime(z) > os.path.getmtime(a):
                sys.exit('portlinux.py: %s differs from %s and is newer: an edit made to the copy?' % (z, a))
            shutil.copy2(a, z)

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
            if names == ['sdl3']:   # Debian's sdl3.pc: -I${includedir} (the sysroot's usr/include, searched anyway), -lSDL3
                out = []
                if '--libs' in cmd:
                    out += ['-lSDL3']
                if '--modversion' in cmd:
                    out = ['3.2.10']
                return subprocess.CompletedProcess(cmd, 0, ' '.join(out) + '\n', '')
            if names == ['mt32emu'] and not PLAIN and os.path.isfile(MUNT):   # Munt for Linux (tools\buildmunt_linux.py):
                out = []                                                     # the runtime's MT-32 music compiled in
                if '--cflags' in cmd:
                    out += ['-I' + os.path.join(HERE, 'build', 'munt', 'include').replace('\\', '/')]
                if '--libs' in cmd:   # (by its path: NEEDED libmt32emu.so.2, found beside the program by RPATH $ORIGIN)
                    out += [MUNT.replace('\\', '/')]
                if '--modversion' in cmd:
                    out = ['2.8.6']
                return subprocess.CompletedProcess(cmd, 0, ' '.join(out) + '\n', '')
            return subprocess.CompletedProcess(cmd, 1, '', 'Package %s was not found\n' % ' '.join(names))
        if exe in ('clang', 'clang.exe'):
            link = '-c' not in cmd and '-E' not in cmd and '-dumpmachine' not in cmd   # (lld named on a link only)
            cmd = ([cmd[0]] + TARGET[:2] + (TARGET[2:] if link else []) + ([] if PLAIN else ['-DUWPORT_GAMEVIEW'])
                   + list(cmd[1:]) + (LINK_EXTRA if link and '-o' in cmd and not any(str(x).endswith(('.so', '.dll')) for x in cmd) else []))
    return real_run(cmd, *a, **k)


subprocess.run = run
sys.path.insert(0, EXHUME_TOOLS)
import portcfg  # noqa: E402

real_port, real_sources = portcfg.port, portcfg.port_sources


def port(cfg):
    p = real_port(cfg)
    p.backend = 'sdl3'
    p.out = os.path.join(os.path.dirname(p.out), OUT_NAME)
    p.out_name = OUT_NAME
    if not PLAIN:   # our plat_sdl3.c in place of the runtime's own (portwin32.py --backend sdl3's)
        p.runtime_exclude = list(p.runtime_exclude) + [os.path.join(p.runtime_port, 'platform', 'sdl3', 'plat_sdl3.c')]
        if os.path.join(p.dir, 'platform', 'sdl3') + os.sep not in p.optimise:
            p.optimise.append(os.path.join(p.dir, 'platform', 'sdl3') + os.sep)
    return p


def port_sources(cfg):
    """--original: upstream's sources only (exhumed-original has none of ours, but its platform\\sdl3 is left to
    upstream all the same); uw-new: as portbuild has them (our backend's files copied in above)"""
    if not PLAIN:
        return real_sources(cfg)
    ours = os.path.join(real_port(cfg).dir, 'platform', 'sdl3') + os.sep
    return [s for s in real_sources(cfg) if not s.startswith(ours)]


portcfg.port, portcfg.port_sources = port, port_sources
os.chdir(UW1)
sys.argv = [os.path.join(EXHUME_TOOLS, 'portbuild.py'), '--config', 'exhume.toml'] + sys.argv[1:]
try:
    runpy.run_path(sys.argv[0], run_name='__main__')
except SystemExit as e:
    if e.code not in (None, 0):
        raise
