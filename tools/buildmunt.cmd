@echo off
rem buildmunt.cmd: Munt's libmt32emu (external\mt32emu, LGPL 2.1+) for the original game's port: build\munt\mt32emu.dll
rem and its import library build\munt\libmt32emu.dll.a, and build\munt\include\mt32emu (a junction to the source) for
rem <mt32emu/c_interface/c_interface.h>. Apart from uw.exe's build\mt32emu.dll. Built once; tools\portwin32.py runs it.
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0..\external\mt32emu"
if not exist ..\..\build\munt mkdir ..\..\build\munt
if not exist ..\..\build\munt\include mkdir ..\..\build\munt\include
if not exist ..\..\build\munt\include\mt32emu mklink /J ..\..\build\munt\include\mt32emu . >nul
if exist ..\..\build\munt\mt32emu.dll if exist ..\..\build\munt\libmt32emu.dll.a goto done
set S=.
set SRC=%S%\Analog.cpp %S%\BReverbModel.cpp %S%\Display.cpp %S%\File.cpp %S%\FileStream.cpp %S%\LA32FloatWaveGenerator.cpp %S%\LA32Ramp.cpp %S%\LA32WaveGenerator.cpp %S%\MidiStreamParser.cpp %S%\Part.cpp %S%\Partial.cpp %S%\PartialManager.cpp %S%\Poly.cpp %S%\ROMInfo.cpp %S%\Synth.cpp %S%\Tables.cpp %S%\TVA.cpp %S%\TVF.cpp %S%\TVP.cpp %S%\sha1\sha1.cpp %S%\SampleRateConverter.cpp %S%\c_interface\c_interface.cpp %S%\srchelper\srctools\src\FIRResampler.cpp %S%\srchelper\srctools\src\SincResampler.cpp %S%\srchelper\srctools\src\IIR2xResampler.cpp %S%\srchelper\srctools\src\LinearResampler.cpp %S%\srchelper\srctools\src\ResamplerModel.cpp %S%\srchelper\InternalResampler.cpp
clang++ %CLANG_FLAGS% -O2 -std=c++11 -shared -DMT32EMU_WITH_INTERNAL_RESAMPLER=1 -Dmt32emu_EXPORTS -I . -I srchelper\srctools\include -o ..\..\build\munt\mt32emu.dll %SRC% -static -lpthread -Wl,--out-implib,..\..\build\munt\libmt32emu.dll.a || exit /b 1
:done
echo built munt
