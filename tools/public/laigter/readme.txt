laigter-cli: the program that makes uwr's relief maps (normal, shadow and depth maps for walls, floors and sprites).

The game runs it by itself, once, on its first start. It reads the pictures from YOUR OWN copy of the game in game\
and writes the maps to data\laigter\. That takes about ten seconds. To have the maps made again, delete the
data\laigter folder and start the game.

    laigter-cli.exe     for Windows
    laigter-cli         for Linux
    *.preset            the settings the maps are made with (Laigter's own preset format)

It is a port of Laigter 1.14.0 by Pablo Ivan Fonovich (https://github.com/azagaya/laigter), without Qt, and is free
software under the GNU General Public License version 3: license-gpl3.txt. It is a program of its own; the game only
reads the pictures it writes.

Its source code: https://github.com/Balderding/uwr (tools/laigter-cli, with src/base.c, png.c, gr.c, audio.c, conv.c
and extract.c).
