uwr: Ultima Underworld: The Stygian Abyss (1992) on Windows and Linux
======================================================================

Preview of 2026-11-16.  Source, pictures and news: https://github.com/Balderding/uwr

uwr plays the 1992 game by its own rules, with a new renderer, sound and interface. Every new part has a switch in
the Esc menu, back to the 1992 screen as it was.

YOU NEED YOUR OWN COPY OF THE GAME. None of its files are in this package.


1. Put your game in
-------------------
Copy these from your own Ultima Underworld (the folder that holds UW.EXE) into the game\ folder here:

    UW.EXE    CRIT\    CUTS\    DATA\    SOUND\

More in game\put-your-game-here.txt.

2. Start
--------
Windows:  play.cmd
Linux:    sh uwr.sh        (read readme-linux.txt first: it lists the packages to install)

The first start takes about ten seconds longer: the game makes its relief maps (the shading of walls, floors and
sprites) from your copy's pictures, into data\laigter\. That happens once.

Everything is switched on as it comes: the Vulkan renderer with ray tracing, DLSS and HDR, the effects (particles,
decals, water), and the 3D sound with room acoustics. What your machine cannot do is left out by the game itself, and
each has its own row in the Esc menu to turn it off (data\settings.txt keeps your choices).
There are no saved games in the package: the game starts at its menu.

Saved games and settings go in home\. F11 opens the settings. Esc opens the menu. The keys are in keybindings.txt.

3. Roland MT-32 music (optional)
--------------------------------
With the two CM-32L ROM files in roms\ the music and effects are the Roland's. They are not included; see
roms\put-your-roms-here.txt. Without them the game plays its other sound.


What you need
-------------
- Windows 10 or 11, 64-bit, or Linux Mint 22 (other Linux systems are untried).
- A graphics card with Vulkan. Ray tracing, DLSS and HDR are used where the card and its driver have them; without
  them the game runs the same, plainer.
- The Linux game has so far only been start-checked, not played through.

What this preview leaves out
----------------------------
- The upscaled wall and floor textures (the Esc menu's Textures row stays on the game's own).
- The small language model for the creatures (the Esc menu's creature levels Original, Devious and Monstrous work
  without it, by rules).
- DLSS 5.

In the folder
-------------
  uwr.exe, play.cmd       the game, and its starter
  uwview_vk.dll           the Vulkan renderer (the default); uwview.dll the Direct3D 12 one (Esc, Renderer)
  mt32emu.dll             Munt, the Roland emulator (LGPL)
  bin\                    OpenAL Soft and Steam Audio, for the 3D sound, with their licences
  dlss\                   NVIDIA DLSS, under nvidia-dlss-sdk-license.txt
  freefonts\              the fonts, with their licences
  laigter\                the program that makes the relief maps on the first start (GPL v3; laigter\readme.txt)
  uwr, uwr.sh, *.so*      the Linux game

Licences
--------
Our own code: MIT (LICENSE.txt). The rules core is Underworld Exhumed by abedegno (MIT). Everything used from
others, with the licences: THIRD-PARTY-NOTICES.md.

Ultima Underworld belongs to its owners (Electronic Arts). This project is not affiliated with or endorsed by them.

Something plays differently from the original? That is a bug: https://github.com/Balderding/uwr/issues
