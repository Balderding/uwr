r"""buildview_linux.py: the Vulkan renderer (buildview.cmd's uwview_vk.dll) for Linux, as build\linux\libuwview.so,
cross-built on this Windows machine with our LLVM clang and lld against the Debian sysroot (tools\linux_sysroot.py), as
tools\portlinux.py builds the program (docs\superpowers\plans\2026-11-15-vulkan-renderer.md, M7).

    <python> -I tools\buildview_linux.py        (after tools\buildview.cmd: the shaders' build\gen headers are its)

The same sources as uwview_vk.dll but the standalone engine's sound (sound.c, sfx.c, xmidi.c, songs.c: the renderer
calls none of them), plus os_posix.c; every file is given os_posix.h first (-include: Windows' names, the paths' '\' and
their case). NGX for Linux is linked in (the DLSS SDK's libnvsdk_ngx.a, with libstdc++); DLSS's own libraries
(libnvidia-ngx-dlss.so.*) are looked for in dlss\ at run time. SDL3 (the window's surface) and the C library are the
system's, found beside the program (its RPATH $ORIGIN). -z defs: nothing left unresolved but in those libraries.
"""
import os, sys, subprocess

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LLVM_BIN = r'O:\appsnotonc\llvm\23.1.4\bin'
LLVM_TMP = r'O:\appsnotonc\llvm\tmp'
SYSROOT = os.environ.get('LINUX_SYSROOT', r'O:\appsnotonc\linux-sysroot')
VULKAN_INC = r'O:\appsnotonc\vulkan-sdk\1.4.363.1\Include'
NGX_LIB = r'O:\appsnotonc\dlss-sdk\lib\Linux_x86_64\libnvsdk_ngx.a'
OUT = os.path.join(HERE, 'build', 'linux', 'libuwview.so')

SOURCES = ('uwview.c render_vk.c vk_load.c ngx_vk.c ui.c menu_draw.c smoothtext_stb.c uwinv_draw.c uwconv_draw.c uwrec_posix.c '
           'base.c png.c gr.c audio.c conv.c extract.c level.c normals.c lights.c world.c models.c objdata.c orig.c '
           'collide.c os_posix.c').split()

if not os.path.isfile(os.path.join(SYSROOT, 'usr', 'include', 'SDL3', 'SDL.h')):
    sys.exit(r'buildview_linux.py: no sysroot at %s (make it: tools\linux_sysroot.py)' % SYSROOT)
for h in ('present_vs_spv.h', 'scene_ps_spv.h'):
    if not os.path.isfile(os.path.join(HERE, 'build', 'gen', h)):
        sys.exit(r'buildview_linux.py: no build\gen\%s (run tools\buildview.cmd first)' % h)
if not os.path.isfile(NGX_LIB):
    sys.exit('buildview_linux.py: no %s (the DLSS SDK)' % NGX_LIB)

for k in ('TMP', 'TEMP', 'TMPDIR'):
    os.environ[k] = LLVM_TMP
os.makedirs(os.path.dirname(OUT), exist_ok=True)
src = os.path.join(HERE, 'src')
cmd = [os.path.join(LLVM_BIN, 'clang.exe'), '--target=x86_64-linux-gnu', '--sysroot=' + SYSROOT.replace('\\', '/'),
       '-fuse-ld=lld', '-shared', '-fPIC', '-fvisibility=hidden', '-std=c17', '-O2',
       '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-missing-field-initializers',
       '-include', os.path.join(src, 'os_posix.h'),
       '-I', src, '-I', os.path.join(HERE, 'port', 'win32'), '-I', os.path.join(HERE, 'dlss', 'include'), '-I', os.path.join(HERE, 'build', 'gen'), '-I', VULKAN_INC,
       '-o', OUT] + [os.path.join(src, f) for f in SOURCES] + [os.path.join(HERE, 'port', 'win32', 'rlog.c'),
       NGX_LIB, '-lstdc++', '-lSDL3', '-lm', '-ldl', '-lpthread',
       '-Wl,-z,defs', '-Wl,--exclude-libs,ALL', '-Wl,-soname,libuwview.so', '-Wl,-rpath,$ORIGIN']   # (NGX's own
# functions kept inside: only the uwv_* entry points are exported)
r = subprocess.run(cmd)
if r.returncode:
    sys.exit('buildview_linux.py: the build failed')
print('built %s (%d bytes)' % (OUT, os.path.getsize(OUT)))
