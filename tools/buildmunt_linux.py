r"""buildmunt_linux.py: Munt's libmt32emu 2.8.6 (external\mt32emu, LGPL 2.1+; the same source and config.h as the Windows
mt32emu.dll of tools\buildmunt.cmd) for the Linux build: build\munt-linux\libmt32emu.so.2 (SONAME libmt32emu.so.2),
cross-compiled with our clang against the Debian sysroot (tools\linux_sysroot.py; C++: its libstdc++-14-dev). No cmake:
buildmunt.cmd's own source list and defines. Its headers are build\munt\include's (buildmunt.cmd: <mt32emu/...>).
tools\portlinux.py answers pkg-config "mt32emu" with these; tools\makelinuxdist.py ships the .so beside uw-new.

    <python> -I tools\buildmunt_linux.py"""
import os, subprocess, sys
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(HERE, 'external', 'mt32emu')
OUT = os.path.join(HERE, 'build', 'munt-linux')
CXX = TOOLS + r'\llvm\23.1.4\bin\clang++.exe'
SYSROOT = os.environ.get('LINUX_SYSROOT', TOOLS + r'\linux-sysroot')
FILES = ('Analog.cpp BReverbModel.cpp Display.cpp File.cpp FileStream.cpp LA32FloatWaveGenerator.cpp LA32Ramp.cpp '
         'LA32WaveGenerator.cpp MidiStreamParser.cpp Part.cpp Partial.cpp PartialManager.cpp Poly.cpp ROMInfo.cpp '
         'Synth.cpp Tables.cpp TVA.cpp TVF.cpp TVP.cpp sha1/sha1.cpp SampleRateConverter.cpp c_interface/c_interface.cpp '
         'srchelper/srctools/src/FIRResampler.cpp srchelper/srctools/src/SincResampler.cpp '
         'srchelper/srctools/src/IIR2xResampler.cpp srchelper/srctools/src/LinearResampler.cpp '
         'srchelper/srctools/src/ResamplerModel.cpp srchelper/InternalResampler.cpp').split()

os.makedirs(OUT, exist_ok=True)
os.environ['TMP'] = os.environ['TEMP'] = TOOLS + r'\llvm\tmp'
cmd = [CXX, '--target=x86_64-linux-gnu', '--sysroot=' + SYSROOT.replace('\\', '/'), '-fuse-ld=lld', '-O2', '-std=c++11',
       '-fPIC', '-shared',
       # (libstdc++-14-dev's headers by hand: clang on this Windows host does not add them for the sysroot's GCC 14)
       '-nostdinc++', '-isystem', SYSROOT + r'\usr\include\c++\14', '-isystem', SYSROOT + r'\usr\include\x86_64-linux-gnu\c++\14',
       '-isystem', SYSROOT + r'\usr\include\c++\14\backward', '-L', SYSROOT + r'\usr\lib\gcc\x86_64-linux-gnu\14',
       '-DMT32EMU_WITH_INTERNAL_RESAMPLER=1', '-Dmt32emu_EXPORTS', '-I', '.', '-I',
       os.path.join('srchelper', 'srctools', 'include'), '-Wl,-soname,libmt32emu.so.2', '-Wl,--no-undefined',
       '-o', os.path.join(OUT, 'libmt32emu.so.2')] + [f.replace('/', os.sep) for f in FILES] + ['-pthread']
r = subprocess.run(cmd, cwd=SRC, capture_output=True, text=True)
sys.stdout.write(r.stdout + r.stderr)
if r.returncode:
    sys.exit('buildmunt_linux.py: the build failed')
print('buildmunt_linux.py: %s (%d bytes)' % (os.path.join(OUT, 'libmt32emu.so.2'), os.path.getsize(os.path.join(OUT, 'libmt32emu.so.2'))))
