# Third-party code in uo-new

## Underworld Exhumed (MIT)

Some of uo-new's game-rule code is adapted from Underworld Exhumed's matched decompilation of UW.EXE
(https://github.com/abedegno/underworld-exhumed, the copy in underworld-exhumed\, uw1\src). Each adapted routine names
its source file and line in a comment ("underworld-exhumed COMBAT.C:830"). Every one is proved against UW.EXE in the
emulator before it counts (CLAUDE.md).

    MIT License

    Copyright (c) 2026 abedegno

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

The decompiled code is derived from UW.EXE; Ultima Underworld's copyright belongs to its owners (Electronic Arts).
Neither project includes game data: uo-new reads the user's own copy.

## Ultima Underworld Editor (MIT): testing only

Our test tool `uwcheck` (tools\uwcheck, a repository of its own, not part of the game) is built on the core library
of Karl Clinckspoor's Ultima Underworld Editor (https://github.com/KarlClinckspoor/UltimaUnderworldEditorAvalonia,
MIT License, Copyright (c) 2026 Karl Clinckspoor). It runs the editor's level checks on games saved by our port.
None of the editor's code is in the game or in this repository.

## Others

Nuked OPL3 and Munt (mt32emu) are LGPL, in external\ with their own licence files.

Munt (libmt32emu 2.8.6, LGPL 2.1 or later) is shipped as a shared library, on Windows mt32emu.dll and on Linux
libmt32emu.so.2 (tools\buildmunt_linux.py, from the same source in external\mt32emu). It is loaded at run time and can
be replaced by any compatible build.

## Exhume (MIT)

external/exhume holds Exhume's portability headers unchanged (https://github.com/abedegno/Exhume, commit
e6dba84517f11357f7bd15640d5cb782202f88f3; runtime/include/portable.h, runtime/port/compat.h, runtime/port/include,
runtime/port/sys/enhance.h and cheat.h), with which underworld-exhumed's C compiles here as our rules core
(tools/core.py). Licence: external/exhume/LICENSE (MIT, Copyright (c) 2026 abedegno).

## Laigter 1.14.0 (GPL v3) and CImg (CeCILL-C), in tools\laigter-cli
tools\laigter-cli\laigter_cli.cpp is a port of Laigter's map maker (https://github.com/azagaya/laigter,
Copyright (C) 2019 Pablo Ivan Fonovich) without Qt, so it is GPL v3 (tools\laigter-cli\LICENSE-GPL3.txt).
It is a separate program: uw.exe never links it, it only reads the PNG files it writes.
tools\laigter-cli\thirdparty\CImg.h is CImg 2.9.1 (David Tschumperle), CeCILL-C, as shipped in Laigter.

## OpenAL Soft 1.25.2 (LGPL 2.1) and Steam Audio 4.8.1 (Apache 2.0): the effects' audio (fx_audio, fx_acoustics)
Neither is linked: port\win32\uwaudio.c loads each DLL at run time (LoadLibrary, GetProcAddress) only when its
setting is on, and runs without it. Both are kept outside the repository, unpacked as released:
- OpenAL Soft 1.25.2 (Chris Robinson and contributors, LGPL 2.1: COPYING in the release), the 64-bit Windows binary
  release https://github.com/kcat/openal-soft/releases/download/1.25.2/openal-soft-1.25.2-bin.zip,
  SHA-256 67a0c4b800bd860c93c04f38caf8cbe4875f9c84700ac430efc451f70e265434, in
  O:\appsnotonc\openal-soft\1.25.2\ (archive\ the zip, extracted\ its contents). We use bin\Win64\soft_oal.dll
  (SHA-256 3963b06e319180700be0bcc0d027336664bb3a4975876b4a5442f10746fab5b4) and its headers include\AL\*.h
  (the types and constants only). Being a separate DLL it can be replaced by any OpenAL Soft build (LGPL 2.1 s. 6).
- Steam Audio 4.8.1 (Valve Corporation, Apache License 2.0), the C API release
  https://github.com/ValveSoftware/steam-audio/releases/download/v4.8.1/steamaudio_4.8.1.zip,
  SHA-256 4a0aa5ec1176f38f0b0993a37c2259d9e86f27e22d5e24f83ec4c3cb9a1d5449, in O:\appsnotonc\steam-audio\4.8.1\
  (archive\ the zip; extracted\ its lib\windows-x64, include and THIRDPARTY.md). We use
  lib\windows-x64\phonon.dll (SHA-256 ca3dbc01dbc24492717011e80f6a51404ca143ae344ca660971d2c983f1e058d) and
  include\phonon.h. Its third-party notices: steamaudio\THIRDPARTY.md in the release.
  Two of its headers ARE in the repository, unchanged, with Valve's Apache 2.0 notice at their head:
  port\win32\phonon.h and port\win32\phonon_version.h (copied 2026-11-14 from the release's include\; the types and
  constants port\win32\uwsteam.c is compiled against; phonon.dll itself is loaded at run time by that file, for
  "Acoustics: High", fx_acoustics 2).
tools\makedist.py copies the two DLLs (and those licence files) into dist\UW1\bin.
