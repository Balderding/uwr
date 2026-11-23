r"""build_laigter_linux.py: tools\laigter-cli for Linux, cross-built on this Windows machine as tools\build_hdrprobe.py
builds hdrprobe (our LLVM clang and lld, the Debian sysroot), into build\linux\laigter-cli. The same sources and
switches as tools\laigter-cli\build.cmd; libstdc++ and libgcc are linked in, so the player's system needs only glibc.

    <python> -I tools\build_laigter_linux.py
"""
import os, sys, subprocess
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LLVM_BIN = TOOLS + r'\llvm\23.1.4\bin'
LLVM_TMP = TOOLS + r'\llvm\tmp'
SYSROOT = os.environ.get('LINUX_SYSROOT', TOOLS + r'\linux-sysroot')
OBJ = os.path.join(HERE, 'build', 'linux', 'laigter-obj')
OUT = os.path.join(HERE, 'build', 'linux', 'laigter-cli')
SRC = os.path.join(HERE, 'src')
TOOL = os.path.join(HERE, 'tools', 'laigter-cli')

if not os.path.isdir(os.path.join(SYSROOT, 'usr', 'include')):
    sys.exit(r'build_laigter_linux.py: no sysroot at %s (make it: tools\linux_sysroot.py)' % SYSROOT)
for k in ('TMP', 'TEMP', 'TMPDIR'):
    os.environ[k] = LLVM_TMP
os.makedirs(OBJ, exist_ok=True)
TARGET = ['--target=x86_64-linux-gnu', '--sysroot=' + SYSROOT.replace('\\', '/')]


def run(cmd):
    if subprocess.run(cmd).returncode:
        sys.exit('build_laigter_linux.py: the build failed')


objs = []
# (extract.c and its three: --uwr-game; os_posix: the Win32 calls they use, as tools\buildview_linux.py builds them)
for c in ('base.c', 'png.c', 'gr.c', 'audio.c', 'conv.c', 'extract.c', 'os_posix.c'):
    o = os.path.join(OBJ, c[:-2] + '.o')
    run([os.path.join(LLVM_BIN, 'clang.exe')] + TARGET + ['-std=c17', '-O2', '-w', '-include', os.path.join(SRC, 'os_posix.h'), '-I', SRC, '-c', os.path.join(SRC, c), '-o', o])
    objs.append(o)
o = os.path.join(OBJ, 'laigter_cli.o')
# -O3 like Laigter's release; no fused multiply-add, so the float maths rounds as Laigter's does (build.cmd)
S = SYSROOT.replace('\\', '/')
# (this clang does not find the sysroot's libstdc++ headers by itself)
CXX_INC = ['-cxx-isystem', S + '/usr/include/c++/14', '-cxx-isystem', S + '/usr/include/x86_64-linux-gnu/c++/14',
           '-cxx-isystem', S + '/usr/include/c++/14/backward']
run([os.path.join(LLVM_BIN, 'clang++.exe')] + TARGET + CXX_INC + ['-std=c++17', '-O3', '-ffp-contract=off', '-w', '-c',
     os.path.join(TOOL, 'laigter_cli.cpp'), '-o', o])
run([os.path.join(LLVM_BIN, 'clang++.exe')] + TARGET + ['-fuse-ld=lld', '-static-libstdc++', '-static-libgcc', '-o', OUT, o] + objs +
    ['-lpthread', '-lm'])
print('built %s (%d bytes)' % (OUT, os.path.getsize(OUT)))
