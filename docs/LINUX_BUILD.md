# Linux builds from this Windows machine

Both builds cross-compile with our clang (O:\appsnotonc\llvm\23.1.4, with lld) against a Debian sysroot on O:. No WSL
and no admin rights are needed, and nothing is installed. The programs are Linux ELF files: they cannot be run on
Windows.

## 1. The sysroot (once)

    O:\appsnotonc\sentinel\uv\python\cpython-3.13-windows-x86_64-none\python.exe -I tools\linux_sysroot.py

This writes O:\appsnotonc\linux-sysroot (about 110 MB, plus 25 MB of .debs in debs\):

- The packages come from Debian 13 "trixie" amd64 on deb.debian.org (https): libc6, libc6-dev, linux-libc-dev,
  libgcc-s1, libgcc-14-dev, libstdc++6, libstdc++-14-dev, libsdl3-0 and libsdl3-dev.
- glibc is 2.41, but the programs need only the symbol versions they use (GLIBC_2.34 at most so far). SDL3 is
  3.2.10.
- Every download is checked:
  - InRelease's OpenPGP signature is checked with Git for Windows' gpg against Debian's archive and release keys for
    13.
  - Packages.xz is checked against InRelease's SHA256.
  - Each .deb is checked against Packages' SHA256.
  - debs\MANIFEST.tsv lists the package, version, URL and SHA256 of each.
- The .debs are unpacked by the script itself.
- Links become copies of their targets. /lib and /lib64 are directory junctions to usr\lib and usr\lib64 (usrmerge;
  libc.so's linker script names /lib).
- Two lists are written: debs\case-collisions.txt (8 netfilter headers that differ only in case) and
  debs\dangling-links.txt (9 GCC runtime libraries we don't need).
- Running it again reuses the .debs already downloaded.

Debian 13's main has no Munt (mt32emu), so the Linux programs have no MT-32 for now.

## 2. A program by hand

    clang --target=x86_64-linux-gnu --sysroot=O:/appsnotonc/linux-sysroot -fuse-ld=lld -O2 -o hello hello.c -lSDL3

work\linux\hello.c is the first proof. It is ELF64 x86-64 PIE, needs libSDL3.so.0 and libc.so.6, uses the
interpreter /lib64/ld-linux-x86-64.so.2, and needs at most GLIBC_2.34.

## 3. The port

    <python> -I tools\portlinux.py --original

This builds upstream's UW1 port with nothing of ours: exhumed-original (underworld-exhumed at its upstream commit) and
the runtime's own plat_sdl3.c. The result is exhumed-original\uw1\build\port-linux\uw1port.

    <python> -I tools\portlinux.py

This builds uw-new itself (underworld-exhumed, branch uw-new) into underworld-exhumed\uw1\build\port-linux\uw1port.
It uses our backend: port\sdl3\plat_sdl3.c and port\win32's shared files (uwfront.c, uwgame.c, uwaudio.c and the
rest, but not plat_win32.c). They are copied into uw1\src\port\platform\sdl3, as `portwin32.py --backend sdl3`
does, and the runtime's own plat_sdl3.c is left out.

There is no 3D renderer on Linux (uwview.dll is Windows'), so the game shows the original's picture through SDL's
renderer, with our menu, input, panel and sound. The language model is off: uwmindhost.c is a stub off Windows,
because uwmind has no Linux build.

The program targets Linux Mint 21 and 22. Its file facts:

- ELF64 x86-64 PIE, needing libSDL3.so.0, libm.so.6 and libc.so.6, at most GLIBC_2.34.
- RUNPATH `$ORIGIN`, so libraries beside it are found.

What it needs at run time:

| Library | Where from | Notes |
|---|---|---|
| libSDL3.so.0 | Shipped beside the program | Mint's Ubuntu bases have no SDL3. Debian 13's copy needs GLIBC_2.38: Mint 22 only. |
| libopenal.so.1 | Mint's libopenal1 | Found by its name. |
| libphonon.so | The Steam Audio 4.8.1 SDK's lib\linux-x64 | Needs GLIBC_2.14 and libstdc++. |
| libmt32emu.so.2 | Not available yet | No Debian or Ubuntu package found here; without it the Roland effects are the music driver's. |

Our files load libraries through dlopen. They try the path they build first (the game's bin\, the data's parent),
then the bare name, which finds a library beside the program and then the system's.

portlinux.py runs Exhume's tools\portbuild.py unchanged:

- Every clang call gets `--target=x86_64-linux-gnu --sysroot=O:/appsnotonc/linux-sysroot`.
- A link also gets `-fuse-ld=lld`. uw-new's link adds `-Wl,--wrap=cFBtoScreen -Wl,--wrap=get_eye -Wl,-rpath,$ORIGIN
  -lm`, and its compiles add `-DUWPORT_GAMEVIEW`, as on Windows.
- pkg-config "sdl3" is answered with `-lSDL3`. The sysroot's usr\include is searched anyway.
- With `--original`, our files copied into uw1\src\port\platform\sdl3 are left out of the sources.

The shared files reach the operating system through port\win32\uwos.h. On Windows that is windows.h, so the calls are
exactly the ones made before. Elsewhere it is the same Win32 subset made of pthreads, `__atomic`, clock_gettime and
dlopen.

The Windows builds (tools\portwin32.py, and its `--backend sdl3`) are separate and are not touched.

## 4. The Linux game in the Windows game's folder

The Linux game lives in the same folder as the Windows one (dist\UW1). It shares game\, data\, home\, roms\, bin\,
llm\ and the rest. Nothing is duplicated.

    <python> -I tools\portlinux.py          the ELF
    <python> -I tools\makelinuxdist.py      stages the Linux files in release\linux\
    <python> -I tools\makedist.py           places them in dist\UW1 with the Windows files

makelinuxdist.py stages these:

| File | What it is |
|---|---|
| uw-new | the ELF |
| uw-new.sh | the start script: game/, home/ and roms/ beside it, the log in home/logs (shown in the terminal too), the program's "may run" mark set again after a copy from a Windows disk |
| libSDL3.so.0 | Debian 13's SDL3, which needs GLIBC_2.38, so Mint 22 only |
| bin\libphonon.so | Steam Audio for Linux, where uwsteam.c looks |
| readme-linux.txt | for the player: what was added, the one apt line, how to start, what works and what does not yet, what to send back |

makedist.py writes every name in lower case. It also renames what is already in dist\UW1 to lower case, once,
renaming only. It refuses if two names would become one, and refuses entirely while a program from the folder runs.
The one name kept as it is: libSDL3.so.0, which the dynamic loader matches exactly.

Why lower case works on Linux:

- The game's own files are found by the runtime's plat_resolve, case-insensitively, one path component at a time.
- Ours are opened by lower-case names: data\ui\fonts\font4x5p.sys and the rest, data/settings.txt, data/acoustics.txt,
  data/sound/*.

The home\ folder is shared with Windows:

- The runtime finds its folders in any case.
- On each start it writes data= and mt32-roms= into home\uw1port.cfg as absolute paths. These are Windows paths after
  a Windows start and Linux paths after a Linux start.
- Both start scripts pass --data, --home and --mt32-roms themselves, so neither system reads the other's paths.

## 5. Our sound on Linux: Munt and OpenAL

    <python> -I tools\buildmunt_linux.py    build\munt-linux\libmt32emu.so.2 (before tools\portlinux.py)

- **Munt:** libmt32emu 2.8.6 (external\mt32emu, the same source and config.h as the Windows mt32emu.dll) is
  cross-built with clang++. libstdc++'s headers are given by hand, because clang on this Windows host does not find
  them in the sysroot. The library:
  - SONAME libmt32emu.so.2, NEEDED libstdc++.so.6, libm, libgcc_s and libc;
  - at most GLIBC_2.38 and GLIBCXX_3.4;
  - all nine mt32emu_* functions uwaudio.c loads are exported.
- **The music:** portlinux.py answers pkg-config "mt32emu" with it, so the runtime's MT-32 music is compiled in, and
  the ELF NEEDS libmt32emu.so.2, found beside it through the RPATH.
- **One file, two users:** makelinuxdist.py stages libmt32emu.so.2 beside uw-new. The runtime's music uses it, and so
  does uwaudio.c's dlopen of our Roland effects (the data's parent folder + "libmt32emu.so.2").
- **OpenAL:** our effects need libopenal.so.1, Mint's libopenal1 (apt). Without it they are the music card's, as in
  the original.

Shown on Ubuntu 24.04 (WSL) by work\linux\mint\test_audio.sh: the shipped folder, Debian's libopenal.so.1 beside the
program for the test only, Xvfb, no sound device. The log lines:

- "mt32: ROMs given .../roms (ctrl_cm32l_1_02, pcm_cm32l)", and DATA\UW.CFG "6 2 330 -1 sound": the MT-32 music;
- "fx: Roland effects rendered by Munt (24 effects, 32000 Hz)";
- "fx: audio on (OpenAL Soft loopback ...)", "fx: HRTF on (Built-In HRTF)", "fx: acoustics on (EFX)", and the
  effects placed where they happen ("fx: sound N at X,Y").

The recordings: music.wav (--audio-wav) is 30 s, 47 % of its samples above 64; fx.wav (UWPORT_FX_LOOPBACK) is 30 s,
with sound at the effects.
