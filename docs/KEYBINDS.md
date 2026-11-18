# uwr keybinds

These are the keys the game comes with. The keyboard commands of play can be changed in the Esc menu, on the **Keys** tab
(see [ESC_MENU.md](ESC_MENU.md)).

Two programs read keys differently, so there are two tables.

- **uw-new.exe** (the port, `dist\UW1\uw-new.exe`): the fixed table in `port/win32/uwinput.c`, plus F11 / Esc / Tab. Not rebindable.
- **uw.exe** (`src/main.c`): the table in `data/keys.txt`, rebindable (up to four keys per action; delete the file for the defaults).

Sources: `port/win32/uwinput.c` (key table), `port/win32/plat_win32.c` (our_action), `data/keys.txt`.
If a key is not listed, the game itself (the original's keys) gets it, e.g. O alone, S, A, D, E, the digits.

## uw-new.exe

### Movement and view

| Key | Action |
| --- | --- |
| W, Up | Walk forward (hold Shift: run) |
| Down | Walk back (S also does) |
| A / D | Slide left / right |
| Left / Right | Turn |
| PageUp / PageDown | Look up / down |
| Shift (either) | Run |
| Space | Jump |
| M | Map |

### Actions

| Key | Action |
| --- | --- |
| E | Use / take what the crosshair is on |
| L | Look (left click also looks) |
| C | Fight mode on / off (a left click attacks only in fight mode) |
| Q | Cast the spell on the shelf |
| R | Runes panel |
| , | Character panel |
| K | Track skill |
| Z | Sleep (make camp) |
| I | Our inventory panel |
| Tab | Our panel (the original's interface in the original display) |

### Save, load, capture

| Key | Action |
| --- | --- |
| F5 | Our save page (quick slot) |
| F9 | Our load page |
| P | Screenshot to the clipboard |
| Shift+P | 15 seconds of video |

### Graphics (ours)

| Key | Action |
| --- | --- |
| ' | DLSS 5 on / off |
| ; | Ray tracing High / Normal / Off |
| ] | DLSS 5 style: Default / Natural / Cinematic |
| F6 | Normal maps (stone relief) on / off |
| . or Shift+O | Objects' shadows on / off |
| Shift+L | Full bright |

### Debug / system

| Key | Action |
| --- | --- |
| Ctrl+L | AI log on / off (smart-hunting decisions) |
| F11 | Settings screen |
| Esc | Our menu (and releases the mouse) |
| Ctrl+F10 | Mouse lock (when locked; see `plat_win32.c`) |
| Alt+F4 | Close the window |

## uw.exe (data/keys.txt defaults)

| Action | Keys |
| --- | --- |
| forward | W, Up |
| back | S, Down |
| left / right | A / D |
| turn_left / turn_right | Left / Right |
| look_up / look_down | PageUp / PageDown |
| run | Shift |
| jump | Space |
| release_mouse | Escape |
| use | E |
| look | L |
| fight | C |
| inventory | I, Tab |
| character | , |
| runes | R |
| cast | Q |
| track | K |
| sleep | Z |
| torch | T |
| quicksave / quickload | F5 / F9 |
| screenshot | P |
| dlss | ' |
| raytracing | ; |
| dlss5_style | ] |
| normal_maps | F6 |
| prev_level / next_level | F2 / F3 (exploring aid) |
| fly | F4 (exploring aid) |

Key names for keys.txt: A-Z 0-9 Space Shift Ctrl Alt Tab Enter Escape Backspace Up Down Left Right PageUp PageDown
Home End Insert Delete F1-F12 Mouse1-Mouse5, or a Windows key code.

## Not verified

Written from reading the source and keys.txt on 2026-11-13; the game was not run to confirm each key.
The torch key (T) appears only in keys.txt, not in uwinput.c's table, so it may be uw.exe-only.
Keys the original game itself handles (digits, O, etc.) are not listed here.
