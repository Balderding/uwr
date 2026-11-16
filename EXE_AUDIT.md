# UW.EXE run-through: everything the original does, and whether uo-new has it

Built 2026-11-08 from every message UW.EXE prints (strings block 1, `data\strings\block0001.txt`; `tools\coverage.py`
-> `re\coverage.txt` ties 90 of them to the routines printing them), the routine map (`re\uw_map.txt`) and the
rules already decoded (`MECHANICS.md`). Message numbers are block 1 indexes. **have** = in uo-new, **partial**,
**missing**.

## World and movement
| Feature | Original | Status |
|---|---|---|
| Walking, running, turning, looking up/down, mouse look | | have (ours: WASD + mouse) |
| Jumping (Acrobat skill for distance) | | partial: jump without the skill |
| Falling damage: d from the fall's speed, an Acrobat check (vs 2d) takes (30 - Acrobat) / 30 of it, above 3 it hurts | 0x1d0c3 | have (our speed to d: estimated) |
| Swimming: water from TERRAIN.DAT; tiring 96 on entering; each tick a Swimming check vs the load, failures tire (3 - r)d4, past 120 drowning 2d(4 - r) | 0x2b02e, 0x1c5f3 | have (slower in water, no jumping: ours) |
| Lava hurts (TERRAIN.DAT lava floors) | ? | partial: 1d4 a half second (estimate; the rule is not found yet) |
| Doors: open, close, locked, keys (2-6) | 0x36afd, 0x3831b | have (E finds the key) |
| Lock with a key (4 "The key locks the lock"): hold the key, click the door | 0x36afd | have |
| Lockpicking (120-122): hold the lockpick, click the door; Picklock vs the lock's z x 3 (z 15 never, z 14 needs 31) | 0x36a78, 0x3831b | have |
| Spiking doors (128-131): quality 63, looked at "is spiked", not opened by hand | 0x36c42, 0x7c6c5 | have |
| Bashing doors: weapon wears, noise | 0x24b06 | missing |
| Switches, levers, buttons, chains and their messages (194-209) | | have (triggers); their messages missing |
| Traps: damage, teleport, door, create/delete, text, variables, inventory, change terrain | 0x83d96 | have |
| Traps: arrow (a missile of kind (q << 5) | owner at the player) | 0x83dc3 | have; do (special events by quality 2-5, 0x18, 0x28-0x2a, 0x32-0x3f: the quest's), spell, ward / tell, pit: missing |
| Searching for traps, disarming (97, 98, 244; Traps / Search skill) | | missing |
| Look mode's Search roll (hidden things) | 0x26e50 | missing |
| Moongates (280), between worlds (159) | | missing |
| Bridges (171 "You see a bridge") | | have (look) |
| Boulders broken with the rock hammer (135): smaller pieces, the smallest sling stones | 0x3797e | have |
| Level changes by stairs/ladders | | have (F2/F3 for testing; the level links: check) |
| Ethereal Void (level 9), the endgame (188-190, 278-279) | | missing |

## The character
| Feature | Original | Status |
|---|---|---|
| Attributes, 20 skills, vitality, mana, level, experience, skill points | 0x341d8 | have (stand-in numbers) |
| Character creation: sex, handedness, class (8), skill picks, face, difficulty, name, keep (block 2: 1-30) | 0x6c830, 0x6cdb2, 0x6ca88, SKILLS.DAT | have |
| Level up (147) | 0x8138a | have |
| Shrines and mantras (24-30): use a shrine (0x157), type the mantra; 20 skill mantras (a skill point, two advances 0x814e1: not past 30 or twice the attribute, +1, +1 more for dex/int skills below half, + rnd % 25/40/10 < attr - skill), SUMM RA / MU AHM / OM CAH (random skills of the group, 3 / 2 / 4 tries) | 0x816fd, 0x814e1 | have; INSAHN (the Cup's direction) and FANLO (the Key of Truth): with the quest |
| Hunger, fatigue and their words (104-118) | | have |
| Eating, drinking (126, 172-176, 229-243): food, water, port, ale, potions, leeches, toadstool, mushroom, candle, plants, rotworm, stew | 0x374ee | partial: food and its taste words; drinks, potions and the special foods' effects missing |
| Drunkenness (241, 243) | 0x37809 | partial (counter only) |
| Poison: state, words (84-88, 91-92), leeches cure it (224) | 0x2adee | partial: poison harms; words and cures missing |
| Healing and mana regeneration | 0x2acbd | have |
| Sleep and camp (14-21): Z or a bedroll; not on level 9; hostile creatures within 2 tiles stop it; 2-6 hours, poison runs its course, a hostile creature within 8 tiles may wake you (fatigue -32, hunger -12..27, drink -16); else 7-10 hours in all, vitality and mana back by sixteenths (more after a fed sleep), fatigue 0, hunger -24..55, drink -32, rested / uneasy; starving: 2 damage | 0x81f8f | have (dreams: missing) |
| Death (0x82880): an eighth of the experience lost, the pointer's thing falls, a pile of bones where the player fell; the silver tree planted (not level 9): wake beside it healed; else the game is over (ours: the panel offers the newest save or a new game) | 0x82880 | have (Garamon's dream: missing) |
| Status: vitality, mana, poison (89-92) | | have (character panel, the figure's status) |
| Where am I / what day / what time (64-92, 411-511): click the figure in the pack; the game clock (1/256 s, from 19:00 on day 1), saved | 0xce | have |
| Track skill (36-43, 59-62): K; creatures within 8 tiles, a Track check each (vs 15 - how hidden), the busiest of 8 directions told | 0x356ee | have (the kinds' hiddenness: 0 for now) |
| Easy setting | 0xb4 | have |

## Items
| Feature | Original | Status |
|---|---|---|
| Pick up / weight / reach (93-96, 252) | 0x26b8e | have (93 "too far away" missing) |
| Pack, bags, containers, what fits (248, 258) | 0x7aaeb | have |
| Stacks (119 "use those individually") | | partial |
| Throwing / dropping (253) | 0x27ab5 | partial: dropping |
| Light sources: equip, burn, used up (123, 124) | 0x2af36 | have |
| Oil: refuel the lantern, refresh a torch, make a torch of wood (177-184) | 0x37b60 | have |
| Reading books and scrolls | 0x37fa0 | have |
| The map scroll (151) and the automap with notes | | missing |
| Wands, charges, breaking (125, 215) | 0x38d20 | have: the spell object travels with the wand; charges on the tooltip; an empty wand snaps 4 times in 10 |
| Potions | 0x38b5c | have: their enchantment (heal, mana, harm, spells by number) |
| Instruments (250, 251) | 0x37ce9 | partial: the mode (Esc ends it); the notes need sound |
| Fishing (99-102): water ahead (TERRAIN.DAT), one in five, a fish | 0x37d8e, 0x73f34 | have |
| Bowl and stew (148-150) | | missing |
| Repair with the anvil (140-145, 216-223): hold the anvil, click the thing in the pack (asks, then tries) | 0x747ac, 0x748c4 | have |
| Bones on a gravestone (134, 259): Garamon's on his grave start his conversation | 0x36cf9 | have |
| Pole on a switch (157, 158) | 0x36e62 | have |
| Silver seed: plant it (10, 12), taking the tree back gives the seed (9); one tree, kept in saves | 0x37d45 | have |
| The bedroll (sleep) | 0x37cd2 | have |
| Leeches (224): the poison gone, 2 vitality | 0x37cfe | have |
| Campfire, moonstone, glowing rock, medallion | | missing (most do nothing in the original) |
| Enchantments: weapons (Accuracy, Damage), armour (Protection, Toughness), worn effects | 0x38b5c, 0x254ed, 0x7e6e5 | have: the reader exact; weapon accuracy / damage + (v & 7) + 1 exact; worn rings and armour as lasting effects exact; armour Protection + (v & 7) + 1 and Toughness: estimate (Toughness not applied: no item wear yet); names on the tooltip |
| Item wear (weapons on doors, armour) and repair | 0x24b06 | missing |
| Weapon art and swings | WEAPONS.GR | have |

## Combat
| Feature | Original | Status |
|---|---|---|
| Charge, power, attack score, damage, criticals, hit location, angle, armour, shields | | have |
| Fight mode | 0x26fef | have (C) |
| Missile weapons: bows, crossbows, slings in fight mode; their ammunition from the pack; speed = Missile x 8 + 0xC0 (fumble -0x80, critical +0xC0), damage = ammunition x speed / 256; firing room (254); misses lie where they fall | 0x2b2bd, OBJECTS.DAT 0x82 | have (the impact's hit check: ours, the melee one) |
| Creature attacks | 0x259ee | have |
| Creature AI: attitude from the object (word 0x0d bits 14-15: 0 hostile, 1 upset, 2 mellow, 3 friendly; only the hostile attack; struck: hostile; conversations get it as npc_attitude and keep set_attitude's change) | 0x7120c | partial: attitude have; goals, following, fleeing, noise (146) missing |
| Angering friends (225-227: angered / annoyed / notes your action) | | missing |
| Enraged creatures (more XP, harder hits) | 0x25b5f | missing |

## Magic
| Feature | Original | Status |
|---|---|---|
| The spell table: 48 spells, runes, category, aim, strength | overlay entry 73 (file 0x59ef0) | have: extracted to data\magic\spells.txt |
| Runes, the rune bag (247), the spell shelf | player 0x47-0x49 | have: R opens the rune panel (owned runes lit), a shelf of three, Cast button; the shelf is saved |
| Casting: circles, mana, failure, backfire (210-214) | 0x78ec8 | have, exact: circle n / 6 + 1 against (level + 1) / 2; 3 x circle mana; Casting + 5 against 2 x circle; fumble: harm circle / 2 d8 |
| The effect dispatcher by category | 0x34418 | have: lasting (0-3), heal, missiles, area, near creatures, making, harm, mana, specials |
| Lasting spells: at most 3, durations 2d3 / 2d8 + 6 / 3d20 + 24 ticks | 0x2b109, 0x7e008 | have, exact; listed at the top right; saved |
| Light, Night Vision, Daylight | 0x7e008 | have (the light level; our reach and colour) |
| Leap, Slow Fall, Levitate, Water Walk, Fly | 0x7e0a8, 0x1c62f, 0x1d2c2 | have, exact in proportion: leap halves gravity through the jump; slow fall holds the fall to 94/141 of a jump and halves its harm; levitate / fly: no gravity, the rise fading by 1/5 a tick; water walk: water is floor (the up / down keys: ours) |
| Speed, Freeze Time, Telekinesis, Roaming Sight (specials 2, 0, 3, 1 as lasting) | 0x7e0e6, 0x30f74, 0x31144, 0x7dea4 | have: speed halves the world's time step (not the player faster); freeze time stops the creatures; telekinesis lifts the reach limit (DS 0x286 0x90 -> 0); Roaming Sight: lasting, no view yet |
| Resist Blows, Thick Skin, Iron Flesh (protection level), Protection (+3) | 0x7e028, 0x7e04e, 0x7e31d | have: level to every part's armour, Protection +3 off the attacker's score |
| Stealth, Conceal, Invisibility | 0x7dea4, 0x7e31d | have: noise 13 - Sneak / 3 (stealth - 16), visibility 15 - Sneak / 5 (conceal - 5, invisibility - 16) exact; how far creatures see / hear by them: our scale |
| Resistances: missile protection 0x40, flameproof 8, poison resistance 0x10, magic protection 1 / 2 | DS 0x1b00, 0x60eb, 0x25eaf | have: the resistance rule exact (magic: resisted when d3 - 1 < level; other bits: immune); creatures' from COMOBJ byte 8 (0x80 undead) |
| Heal (strength d8, 15 all), harm (d8, not below 3), mana ((v + d4 - 1) x max / 16 + 1) | 0x347cc, 0x34813, 0x34656 | have, exact |
| Magic Arrow, Electric Bolt, Fireball (and acid, items only) | DS 0x9ab | have (missiles 0x17, 0x15, 0x14, 0x16) |
| Area and creature spells: a 5 x 5 square 4 tiles ahead; area 3d4 targets, creature spells 1; random open tiles for strength 0x40 | 0x35017, 0x38e1d | have, exact |
| Flame Wind 10d6 fire on 3d4 tiles and their neighbours; Sheet Lightning 6d5 | 0x34a9b, 0x34a0e, 0x35bca, DS 0x9d3 | have, exact |
| Smite Undead (255 to the undead), Poison (5d4 poison), Cause Fear (goal 6), Paralyze (goal 7, attitude 1), Confusion (goal 2, attitude 1), Ally (friendly) | 0x34b8a, 0x34bd4, 0x34c29 | have: exact but for how long the goals hold (the AI's; ours 20-30 s) |
| Create Food (0xB0 + d7), Rune of Warding (a move trigger 0x1a0 and ward trap 0x189: d(level) + 3 to what steps on it, 245), Summon Monster (kind L..2L-1, an ally), 277 when no room | 0x351e5, 0x84c0a, 0x846e3 | have (a summon picks among kinds the level has pictures for) |
| Specials: Detect Monster (track 10 tiles at skill 45), Remove Trap (skill 45), Name Enchantment (and identified), Open (lockpick at 45: 270 / 271), Cure Poison, Tremor (3d8 rocks), Armageddon (278) | 0x359e5, 0x35900 | have (a falling rock's harm: estimate) |
| Strengthen Door | 0x35900 | have: UW.EXE does nothing with it (we say 271) |
| Gate Travel (273) | 0x35af9 | partial: no moonstone yet (the quest) |
| Hallucination potion (228) | 0x345f8 | have the message; the view's distortion missing |
| Spell scrolls | 0x37fa0 | have: cast once, then gone |
| Regeneration, Mana Regeneration (worn) | 0x7e008 | have: a point a tick (estimate) |

## Conversations and the quest
| Feature | Original | Status |
|---|---|---|
| Conversations (the VM) | CNV.ARK | have; their own variables now kept between talks and in saves |
| Bartering (186), giving and taking items, skill training by NPCs | conv built-ins | missing |
| The eight talismans and the Key of Truth (32, 260-269) | | missing |
| The Cup of Wonder pointer (35) | | missing |
| The princess (272), Tyball, the Slasher of Veils, the ending | | missing |
| Fountains (249) | | missing |

## The game itself
| Feature | Original | Status |
|---|---|---|
| Save and restore (161-170): slots 1-4 + quick (F5 / F9) + an autosave when the game closes (continued when it opens); each visited level as it was left, the character, the game state, the inventory and body, what each conversation remembers (BABGLOBS sizes), where the player stands | PLAYER.DAT, LEVx.ARK, BGLOBALS.DAT | have (our own text files in data\saves) |
| Options: music, sound, detail, quit | | have (ours: Esc menu) |
| Intro, cutscenes, endings (CUTS) | | missing |
| Sound effects (SOUNDS.DAT) | | missing |
| Music | | have |
| "Welcome to the Underworld." (13), "enters the Abyss" (256, 257) | | partial: 13 after creation |
