"""One-off: NOTES.md for eating, hunger, fatigue and the player tick's 24-cycle (lantern correction)."""
import sys

p = sys.argv[1] + '/NOTES.md'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
ADD = '''- **The player tick** (file 0x2acbd, every 21 s): counter [0x3f9] + 1; spell effects; the lights burn (counter
  % duration); counter % 3 == 0: poison (player 0x5f bits 2-5) - 1 with damage; counter % 24 == 0: hunger
  (player byte 0x39) += -(3 + random 0-3) (via 0x82377, clamped at 0), drunkenness (0x61 bits 4-9) - 1,
  fatigue and two more bytes (0x3a-0x3c) + 1, and the **counter is reset to 0**. So the lights' durations run
  inside a 24-tick cycle: torch 3 -> every 3 (fresh 40 = ~42 min), candle 12 -> 12 and 24 (~2 h 48), **lantern
  10 -> 10 and 20 only = every 12 on average (~2 h 48, not 2 h 20)**, taper never. Hunger falls ~4.5 every
  8.4 min: a new character (0xC0 = 192, file 0x6c9c1) empties in ~6 hours.
- **Eating** (file 0x374ee): see inventory.c eat(). Food table OBJECTS.DAT 0xd82 (signed: meat 64, fish 48,
  corn 25, bread 16/12, cheese 12, apple 6, popcorn 2, toadstool 6, mushroom 0; drinks: water -1, ale -3,
  port -8, potions -127, wine 0) -> objects.txt v3 "food" lines. Adding past 255 is refused (msg 126); taste
  = block 1 [172 + min(4, (random 0-19 + quality) >> 4)]; specials 229-240; ale/port add drunkenness (then a
  check against the player that may knock them out: msg 241, TODO); toadstool poisons (4, or +2 up to 15).
  Sleep (file 0x82130) heals only when hunger > 0x40; at 0: "You are starving." (msg 17).
- **Status** (file 0x26122): block 1 [104 + hunger / 30] ("starving" .. "satiated") + " and " + block 1
  [118 - min(5, fatigue / 23)] ("wide awake" .. "fatigued"). In game: click the figure in the panel.
- **Command modes** (file 0x26fef: the 3D view's click handler; table of far routines at DS 0x28f, 5 entries in
  segment 0x22ef): talk, get, look, fight, use; with no mode chosen the click is index 2 = **look** (the
  original's default). Here: no modes, look on left click / L, use on E / right click.
'''
a = '- **Wall decorations drawn**'
i = s.index(a)
s = s[:i] + ADD.replace('\n', nl) + s[i:]
open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
