r"""build_hdrprobe.py: tools\hdrprobe.c for Linux, cross-built on this Windows machine as tools\buildview_linux.py
builds libuwview.so (our LLVM clang and lld, the Debian sysroot), into build\linux\hdrprobe; it and hdrprobe.sh are
then put in release\linux beside the game (the probe uses the libSDL3.so.0 there: RPATH $ORIGIN).

    <python> -I tools\build_hdrprobe.py
"""
import os, sys, subprocess, shutil
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LLVM_BIN = TOOLS + r'\llvm\23.1.4\bin'
LLVM_TMP = TOOLS + r'\llvm\tmp'
SYSROOT = os.environ.get('LINUX_SYSROOT', TOOLS + r'\linux-sysroot')
VULKAN_INC = TOOLS + r'\vulkan-sdk\1.4.363.1\Include'
OUT = os.path.join(HERE, 'build', 'linux', 'hdrprobe')
REL = os.path.join(HERE, 'release', 'linux')

SH = '''#!/bin/sh
# hdrprobe.sh: asks this machine's graphics driver which HDR it offers, by each route (tools/hdrprobe.c), and
# writes the answer to hdrprobe.txt beside this file: send that file back.
#   ./hdrprobe.sh          the safe questions only
#   ./hdrprobe.sh take     also takes the monitor from the desktop for a moment (X11 only): the screen may go black
#                          for a few seconds; it ends itself after 15 seconds whatever happens
cd "$(dirname "$0")" || exit 1
chmod +x ./hdrprobe 2>/dev/null
if [ "$1" = "take" ]; then
    ./hdrprobe --take > hdrprobe.txt 2>&1
    # (the desktop's screens as they were, should X not have put one back)
    [ -n "$DISPLAY" ] && command -v xrandr >/dev/null 2>&1 && xrandr --auto >/dev/null 2>&1
else
    ./hdrprobe > hdrprobe.txt 2>&1
fi
cat hdrprobe.txt
echo
echo "Written to $(pwd)/hdrprobe.txt"
'''

if not os.path.isfile(os.path.join(SYSROOT, 'usr', 'include', 'SDL3', 'SDL.h')):
    sys.exit(r'build_hdrprobe.py: no sysroot at %s (make it: tools\linux_sysroot.py)' % SYSROOT)
for k in ('TMP', 'TEMP', 'TMPDIR'):
    os.environ[k] = LLVM_TMP
os.makedirs(os.path.dirname(OUT), exist_ok=True)
cmd = [os.path.join(LLVM_BIN, 'clang.exe'), '--target=x86_64-linux-gnu', '--sysroot=' + SYSROOT.replace('\\', '/'),
       '-fuse-ld=lld', '-std=c17', '-O2', '-Wall', '-Wextra', '-I', VULKAN_INC,
       '-o', OUT, os.path.join(HERE, 'tools', 'hdrprobe.c'), '-lSDL3', '-ldl', '-Wl,-rpath,$ORIGIN']
if subprocess.run(cmd).returncode:
    sys.exit('build_hdrprobe.py: the build failed')
print('built %s (%d bytes)' % (OUT, os.path.getsize(OUT)))
if os.path.isdir(REL):
    shutil.copy2(OUT, os.path.join(REL, 'hdrprobe'))
    with open(os.path.join(REL, 'hdrprobe.sh'), 'w', newline='\n') as f:
        f.write(SH)
    print('staged hdrprobe and hdrprobe.sh in %s' % REL)
