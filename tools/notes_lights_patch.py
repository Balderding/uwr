"""One-off: NOTES.md for the lights as in the original."""
import sys

p = sys.argv[1] + '/NOTES.md'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'


def rep(a, b):
    global s
    a, b = a.replace('\n', nl), b.replace('\n', nl)
    assert s.count(a) == 1, a[:60]
    s = s.replace(a, b)


rep('''- **Wall torches** (user: "torches on the wall like the real game"; the UW1 data has none: only 4 torches, 3
  candles, 1 lantern lying about on level 1): data\\lights\\levelN.txt v2 = "walltorch" lines (header "# uo-new
  wall torches v2"; the old orb file is remade): room tiles beside a wall, 5 tiles apart, 0.8 tile above the
  floor, facing out; drawn with the lit torch picture 0x95, glowing, flickering (renderer per light).''',
    '''- **Wall torches: REMOVED 2026-11-07** (user: "lets get rid of them as it wasn't in the original game ... the
  only way to see was to have torches yourself"). The automatic data\\lights\\levelN.txt files are no longer made
  or read (the old ones are still on disk, unused: ask before deleting). The floating light of data\\lights.txt is
  commented out in the user's file and gone from the default. "walltorch"/"light" lines a person adds to
  data\\lights.txt still work (drawn with the lit torch picture 0x95, flickering).''')

rep('''from the tile chain; drop = a free static slot (256-1023) at the feet. Light sources 144-147 (+4 lit): one lit at
  a time = the carried light (lantern 1.5/7.5, torch 1.3/6, candle 0.7/3.5, taper 0.85/4), fuel = quality 0-63
  (torch 9.5 s a point ~10 min, candle 14 s, lantern 28 s: then out of oil, taper 7.5 s); settings torch 1 =
  start with a lit torch (the dungeon is dark). T = light / put out the first light in the pack.''',
    '''from the tile chain; drop = a free static slot (256-1023) at the feet. Light sources 144-147 (+4 lit): one lit at
  a time = the carried light; settings torch 1 = start with a lit torch, quality 40 (the dungeon is dark). T =
  light / put out the first light in the pack.
- **Light sources, from UW.EXE** (2026-11-07; tools\\dis16.py / scan16.py = capstone 16-bit disassembly by file
  offset; DGROUP = paragraph 0x5AAC, file 0x5E2C0):
  - OBJECTS.DAT at 0xd62: 16 x (duration, brightness) for 0x90-0x9F, in DS at 0x5B3A. Lit lantern 10/4, torch
    3/2, candle 12/1, taper 0/3 (never burns out); unlit ones 0/0. Extracted to data\\objects\\lights.txt.
  - Clock: AIL timer at 256 Hz (file 0x1ef9d registers 0x1ef8b, which adds 1 to the 32-bit clock at [0x2364]);
    clock >> 8 = real seconds (60 x 256 a game minute). Game time = real time.
  - file 0x26587: whole seconds added to a byte at [0x28d]; above 20 it is zeroed and the player tick runs
    (0x2acbd: spell effects, then burn(1, ++[0x3f9])).
  - burn (0x2af36): the 4 light slots (DS 0x171e: inventory slots 5-8, hands and shoulders); a lit light (class 9,
    index 4-7) with duration d loses n = (counter % d == 0) + (span > 1 ? span / d : 0) points; when quality <= n:
    quality 0 and item - 4 (goes out, stays). So torch 63 s a point (fresh 40 = ~42 min), lantern 210 s (~2 h 20),
    candle 252 s (~2 h 48), taper never.
  - use (0x37267): "Lights may only be used if equipped." (strings 1:123) when not in a light slot; "That light
    is already used up." (1:124) at quality 0; lighting = item + 4, putting out = item - 4.
  - Brightness in our light: intensity 0.5 + 0.4 b, range 3 + 1.5 b tiles (torch b 2 = 1.3 / 6 as before).
  - TODO with the paperdoll: lights only in the hand/shoulder slots, up to 4 lit; oil refuels (strings 1:178-184).''')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
