r"""buildmind_linux.py: the language model's runner for Linux (the user, 2026-11-16: the model creatures in the Linux
build too). src\uwmind.c cross-built on this Windows machine as build\linux\llm\bin\libuwmind.so (our LLVM clang and
lld, the Debian sysroot, as tools\buildview_linux.py), against llama.cpp's OWN Linux build of the tag our Windows side
is built from (external\llama.cpp, b12131):

    https://github.com/ggml-org/llama.cpp/releases/download/b12131/llama-b12131-bin-ubuntu-vulkan-x64.tar.gz
    sha256 a9f6c2818734a7d364ff51ad21a06ab34cbc7aaa08583553b41545c5ef6fcc55
    unpacked in X:\tools\llama-linux\b12131\extracted (MIT, as the source; its licence is staged beside it)

and that build's libraries the game needs are put beside it by the names they are asked for: libllama.so.0,
libggml.so.0, libggml-base.so.0 (the three libuwmind.so needs, found beside it: RUNPATH $ORIGIN), every
libggml-cpu-*.so (ggml loads the best one this processor has) and libggml-vulkan.so (the graphics card, through the
driver's Vulkan). tools\makelinuxdist.py stages the folder as llm\bin.

libgomp.so.1 (GCC's OpenMP runtime, which llama.cpp's libraries need and a stock Ubuntu 24.04 / Linux Mint 22 may not
have) goes beside them too: Ubuntu 24.04's own libgomp1 14.2.0-4ubuntu2~24.04.1 (apt-get download in the WSL distro,
nothing installed; unpacked in X:\tools\llama-linux\libgomp; GPL 3 with the GCC Runtime Library Exception, which
lets it be shipped with a program). It needs glibc 2.38: Linux Mint 22, as the rest of the Linux build.

    <python> -I tools\buildmind_linux.py
"""
import os, sys, subprocess, shutil, glob
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LLVM_BIN = TOOLS + r'\llvm\23.1.4\bin'
LLVM_TMP = TOOLS + r'\llvm\tmp'
SYSROOT = os.environ.get('LINUX_SYSROOT', TOOLS + r'\linux-sysroot')
LLAMA = TOOLS + r'\llama-linux\b12131\extracted\llama-b12131'
GOMP = TOOLS + r'\llama-linux\libgomp\libgomp.so.1'
SRC = os.path.join(HERE, 'external', 'llama.cpp')
OUT = os.path.join(HERE, 'build', 'linux', 'llm', 'bin')

# the release's file -> the name it is loaded by (Windows' disks hold no symbolic links: the real file under its soname)
CORE = (('libllama.so.0.9.0', 'libllama.so.0'), ('libggml.so.0.29.0', 'libggml.so.0'), ('libggml-base.so.0.29.0', 'libggml-base.so.0'))

for a in [os.path.join(LLAMA, a) for a, _ in CORE] + [GOMP]:
    if not os.path.isfile(a):
        sys.exit('buildmind_linux.py: no %s (see this file\'s head)' % a)
os.makedirs(OUT, exist_ok=True)
for a, z in CORE:
    shutil.copy2(os.path.join(LLAMA, a), os.path.join(OUT, z))
backends = sorted(glob.glob(os.path.join(LLAMA, 'libggml-cpu-*.so'))) + [os.path.join(LLAMA, 'libggml-vulkan.so')]
for a in backends:
    shutil.copy2(a, os.path.join(OUT, os.path.basename(a)))
shutil.copy2(os.path.join(SRC, 'LICENSE'), os.path.join(OUT, 'license-llama.cpp.txt'))
shutil.copy2(GOMP, os.path.join(OUT, 'libgomp.so.1'))
with open(os.path.join(OUT, 'license-libgomp.txt'), 'w', newline='\n') as f:
    f.write("libgomp.so.1: the GNU Offloading and Multi Processing Runtime Library (GCC 14.2.0; Ubuntu 24.04's libgomp1\n"
            "14.2.0-4ubuntu2~24.04.1, unchanged). Copyright (C) Free Software Foundation, Inc. GNU General Public License\n"
            "version 3 or later, with the GCC Runtime Library Exception 3.1. Source: https://gcc.gnu.org and\n"
            "https://packages.ubuntu.com/noble/libgomp1\n")

for k in ('TMP', 'TEMP', 'TMPDIR'):
    os.environ[k] = LLVM_TMP
so = os.path.join(OUT, 'libuwmind.so')
cmd = [os.path.join(LLVM_BIN, 'clang.exe'), '--target=x86_64-linux-gnu', '--sysroot=' + SYSROOT.replace('\\', '/'),
       '-fuse-ld=lld', '-shared', '-fPIC', '-fvisibility=hidden', '-O2', '-Wall', '-D_DEFAULT_SOURCE',
       '-I', os.path.join(SRC, 'include'), '-I', os.path.join(SRC, 'ggml', 'include'),
       '-o', so, os.path.join(HERE, 'src', 'uwmind.c')] + [os.path.join(OUT, z) for _, z in CORE] + [
       '-Wl,-soname,libuwmind.so', '-Wl,-rpath,$ORIGIN']
if subprocess.run(cmd).returncode:
    sys.exit('buildmind_linux.py: the build failed')
print('built %s (%d bytes) with %d llama.cpp libraries and libgomp beside it' % (so, os.path.getsize(so), len(CORE) + len(backends)))
