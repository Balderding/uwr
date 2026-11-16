"""One-off: NOTES.md (objects, look, inventory, exe map; DGROUP correction) and run-tests.cmd (new checks)."""
import sys

ROOT = sys.argv[1]
p = ROOT + '/NOTES.md'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'

a = '  - Light sources, from UW.EXE** (2026-11-07; tools\\dis16.py / scan16.py = capstone 16-bit disassembly by file\n  offset; DGROUP = paragraph 0x5AAC, file 0x5E2C0):'.replace('\n', nl)
if a not in s:
    a = '- **Light sources, from UW.EXE** (2026-11-07; tools\\dis16.py / scan16.py = capstone 16-bit disassembly by file\n  offset; DGROUP = paragraph 0x5AAC, file 0x5E2C0):'.replace('\n', nl)
assert s.count(a) == 1, 'light anchor'
s = s.replace(a, a.replace('file 0x5E2C0', 'file 0x5DCC0 (corrected: 0x5E2C0 was a slip)'))

ADD = '''
## UW.EXE map and the object rules (2026-11-07)
- **The exe's layout**: load image at file 0x3200 (segment S offset O = 0x3200 + S*16 + O); DGROUP paragraph 0x5AAC
  = **file 0x5DCC0** (the light slots table at DS 0x171e = 05 06 07 08, the Look strings at 0x18ff...). Borland
  VROOMM overlays: "FBOV" at 0x65700, segment table at file 0x5c1c0 (161 x {seg, size, type, ?}, type 3 = an
  overlay stub); a stub: CD 3F, +4 dword -> overlay code at file 0x65710 + it, +8 code size, +0xA fixups, +0xC
  entries; entries from +0x20, 5 bytes (CD 3F, word offset). Overlay code names far segments by selector = table
  index * 8 (0x150 -> 0x32A8). tools\\exe_map.py writes re\\uw_map.txt: 1909 routines, callers/calls, the block 1
  messages each prints (printer 0x32A8:0x494 takes the number), object ids compared. tools\\callsites.py shows
  the pushes before calls to a routine; string ids are block << 9 | index (0x150:0x8d gets one).
- **OBJECTS.DAT** in DS at 0x4dd8 (whole file): containers 0x5b0a (3 bytes), lights 0x5b3a. **COMOBJ.DAT** at DS
  0x5b6e (512 x 11). Byte 3: bit 5 = can be picked up (blood stains 0xDA-0xDF: no; the exe tests it as the last
  rule of "can it go in a container"), bits 6-7 = kind (0 a thing, 1 weapon/armour, 3 container; NOT stackable).
  Byte 6 bits 2-3 quality class; byte 10 bits 0-3 quality type, bit 4 "look" (else a decoration's own text);
  byte 7 bit 7 can have an owner. Extracted (all fields + armour slot/protection/durability + container
  capacity/accepts) to data\\objects\\objects.txt ("# uo-new objects v2"; the engine remakes its own older copy).
- **Look** (file 0x7ba10): "You see " + count or a/an (by the next word's first letter; quality type 13: none) +
  quality word (block 5 [qtype*6 + 0 at quality 0, 5 for qclass 3, else quality/16 + 1]) + name (plural for a
  count) + " belonging to" + block 1 [370 + owner] (owner 1-27, kinds that can have one) + ".". Decorations
  (file 0x7c0d1, jump table by item & 15): 0x164 bridge "You see a bridge."; 0x165 gravestone / 0x166 writing:
  block 8 lead-in [0x160 / 0x170 + (flags & 15)] ("The sign reads: ") then block 8 [link & 0x1ff when a count,
  else quality]; a gravestone with a picture reads GRAVE.DAT. Walls: block 10 [wall texture] (0-209), floors
  block 10 [510 - floor texture]. In game: L, or left click when no creature is in reach; E on what cannot be
  picked up looks at it.
- **Containers** (file 0x7aaeb): contents + item mass <= capacity (0 = none) else "The sack is too full.";
  accepts -1 anything pickable; an object number; 0x200 runes 0xE8-0xFF ("You can only put runes in the rune
  bag."), 0x201 ammunition 0x10-0x12, 0x202 maps/scrolls 0x138-0x13F, 0x203 food 0xB0-0xBF + plants 0xce/0xcf,
  candle 0x92, stew 0x11b, leeches 0x125, dead rotworm 0xd9; else "That item does not fit.".
- **Inventory** (inventory.c): pool of 512 objects, 19 slots: 0 helm 1 chest 2 gloves 3 leggings 4 boots 5/6
  shoulders 7/8 hands 9/10 rings 11-18 pack; armour by OBJECTS.DAT's armour table (byte 3: 1 chest, 3 leggings,
  4 gloves, 5 boots, 8 helm/crowns, 9 ring, 0 shield = a hand). Containers keep contents chained from link (as
  the levels do); taking/dropping moves the whole tree (level slots: free = not reachable from any tile through
  next/link). Lights only burn and light in 5-8 ("Lights may only be used if equipped."). Panel: grey board,
  sunk holes, the figure (BODIES.GR 0); left click takes/places/swaps/stacks (things with a plural name), on a
  container puts in; right click uses (light, open a container into the pack holes; "<" closes); L looks;
  click outside drops in front. Not yet: carry weight limit ("That is too heavy to take.").
- **Wall decorations drawn**: TMOBJ.GR pictures in sprite cells 480+ (TMOBJ_CELL): writing 20 + flags & 7,
  gravestone 28 + flags & 1, flat on the tile edge their x/y touches (0 or 7), 0.045 out of the wall, 1/64 tile a
  pixel. TMOBJ 4-19 are switches/levers/buttons (0x170-0x17F; the exact mapping waits for the switch code).
'''
s = s.rstrip() + nl + ADD.replace('\n', nl)
open(p, 'w', encoding='utf-8', newline='').write(s)

p = ROOT + '/run-tests.cmd'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
anchor = 'build\\uw.exe --shot=build\\shots\\dlss5.png'
i = s.index(anchor)
lines = ('rem objects: look at a wall writing (its text), blood cannot be taken, the starting sack opened in the panel' + nl +
         'build\\uw.exe --shot=build\\shots\\look_writing.png --level=1 --pos=30.75,2.69 --yaw=-90 --pitch=-10 --size=960x540 --ui=1 --frames=4 --look=2 || exit /b 1' + nl +
         'build\\uw.exe --shot=build\\shots\\blood.png --level=1 --pos=28.8,4.7 --yaw=0 --pitch=-50 --size=960x540 --ui=1 --frames=5 --use=3 || exit /b 1' + nl +
         'build\\uw.exe --shot=build\\shots\\sack.png --level=1 --pos=33.5,2.6 --yaw=-21 --pitch=-40 --size=960x540 --ui=1 --frames=8 --use=2 --inv=4 --mouse=357,131 --rclick=6 || exit /b 1' + nl)
s = s[:i] + lines + s[i:]
open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
