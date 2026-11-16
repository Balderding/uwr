# Ultima Underworld: the original's rules, and where uo-new stands

Every mechanic of UW.EXE, decoded from the executable (file offsets; `tools\dis16.py`, `re\uw_map.txt`). Status:
**done** (ours follows the original), **partial**, **todo**, **ours differs** (a choice made with the user; the
original's way is noted, ask before changing).

## Time
| Rule | UW.EXE | Status |
|---|---|---|
| Game clock 256 Hz = real seconds (AIL timer) | 0x1ef8b, 0x1ef9d | done |
| Player tick every 21 s; counter 1-24 reset at 24 | 0x26587, 0x2acbd | done |

## The character
| Rule | UW.EXE | Status |
|---|---|---|
| Player record: 0x1e str, 0x1f dex, 0x20 int, 0x21.. 20 skills (attack, defense, unarmed, sword, axe, mace, missile, mana, lore, casting, traps, search, track, sneak, repair, charm, picklock, acrobat, appraise, swimming), 0x35/0x36 vitality/max, 0x37/0x38 mana/max, 0x39 hunger, 0x3a fatigue, 0x4e exp (32-bit), 0x5f poison bits, 0x61 drunk bits, 0x65/0x67 quest bits, 0xb4 easy | many | partial: data\character.txt (stand-in numbers) |
| PLAYER.DAT xor 0xAA on save/load | 0x7dce0 | todo (save/load) |
| Character creation (overlay 5991): the new player 0x6c830 (level 1, 1 skill point, hunger 192, right-handed; skills 3d4 and attributes 10 + 2d10 as defaults); the class 0x6cdb2: SKILLS.DAT base str/dex/int, then its bonus points 1-4 at a time to a random attribute (at most 30), skills to 0; 5 skill sets per class 0x6ca88 (one skill: given; more: picked); each pick advances its skill (0x8141f); face (HEADS.GR 0-4 / 5-9), Standard / Easy, name, keep | 0x6c830.. | done |
| Experience: kill = 4 x critter value + value d2 (x1.5-3 if the creature is enraged) | 0x25b5f | done (enraged todo) |
| Experience gain halved (+1) when level > 2 x dungeon level + 2; cap 96000; a skill point per 3000 | 0x341d8 | done |
| Levels: exp/500 vs 0 1 2 3 4 6 8 12 16 24 32 48 64 96 128 192 (to 16); "You have attained experience level N"; +1 skill point each | 0x341d8, 0x8138a | done |
| Max HP = 30 + level x str / 5; max mana = (mana + 1) x int / 8; carry limit = str x 20 tenths of a stone | 0x81305 | done |
| Skill advancement: at 0: +3 + attr/9 + 0-2 + three checks of attr vs 20; else +1 + attr/13 + 0-1 + two checks; cap 30. Attribute (0x81402): str for skills 0-6, int for 7-9, dex for 10-19. Dice (0x38eee): n + n x (0..s-1) | 0x8141f, 0x81402 | done (creation; mantras todo) |
| Skill check: skill - difficulty + 0-30: >28 critical, >15 success, >2 failure, else fumble | 0x3419c | done |
| Hunger 192 at start; -3..6 every 24 ticks; food adds nutrition, refused past 255 | 0x2ae86, 0x82377 | done |
| Fatigue +1 every 24 ticks; status words | 0x2aee3, 0x26122 | done |
| Sleep (0x81f8f): see EXE_AUDIT.md; restore by sixteenths: vitality (0x346c2) v > 0: +1 + (v + 0-3) x max / 16, mana (0x34656) +(v + 0-3) x max / 16 + 1; v <= 0: -v points; an hour = 0xE1000 clock ticks | 0x81f8f | done (dreams todo) |
| Poison -1 every 3 ticks, hurting by its level | 0x2adee | done |
| Mana + a mana check vs 10 (1-2) every 3 ticks; +1 HP on a strength check vs 15 every 24 ticks; regeneration flags (DS 0x3f8) +1 every tick | 0x2ae43, 0x2af03, 0x2ad93 | done (regeneration enchantments todo) |
| No messages in combat (sounds) | - | done |
| Drunkenness | 0x37809 | partial (counter only) |
| Easy setting: +7 attack, half damage taken | 0x25454, 0x24e4a | done (character.txt easy 1) |

## Combat
| Rule | UW.EXE | Status |
|---|---|---|
| Charge: + weapon rate every 1/16 s, cap 100; power gem frame charge/12 + 1 | 0x2574b | done |
| Power = min + (max - min) x charge / 100 (weapon bytes 3, 5) | 0x257c8 | done |
| Player attack score = attack/2 + weapon skill + dex/7 (+7 easy) (+ enchantment) | 0x25406 | done (enchantment todo) |
| Player damage base = str/9 + weapon damage (slash/bash/thrust); unarmed: unarmed x 2/5 + str/6 + 4 | 0x25406 | done |
| Attack type: the view split 3 x 3, the pointer's row (DS 0x235: top thrust, middle slash, bottom bash) | 0x26fa9, 0x25406 | done (crosshair: the row is where it meets the creature) |
| Angle bonus 0 (face to face) .. 4 (from behind), added to score and damage | 0x25160 | done |
| Hit check: skill check (score + angle) vs defense (critter byte 0x12; player: defense) | 0x24b68 | done |
| Critical: damage x (48 + 0-31) / 32 | 0x24bbe | done |
| Damage roll: base/6 d6 + d(base%6), base min 2; x power/128 + angle | 0x24cb5 | done |
| Hit location by height: legs, head, else body/arms | 0x2441a | done |
| Armour: critter bytes 0-3 by body part (255: byte 0); player: worn armour reduces the attacker's score | 0x24dc1, 0x24b7f | done |
| Creature attacks: 3 per kind (bonus, damage, chance%); score = bonus + attack/2; damage + str/5; enraged +7-12 / +4-15 | 0x259ee | done (enraged todo) |
| Player armour (0x7e444): body parts zeroed, then helm -> head, chest -> body, gloves -> arms, leggings and boots -> legs (DS 0x1af8: 3 0 1 2 2), each worth protection x condition (quality & 63) / 64 + 1 (0x7e3ed; items below 0x20 nothing); a shield (0x3B-0x3F) in the off hand (slot 7 + handedness bit: the left for the right-handed) counts on body and arms both; defense = the Defense skill | 0x7e444 | done |
| Enchanted worn things (0x7e19a): Protection (bit 8) + (v&7)+1 to the armour of its part (a hand slot: body and arms); else Toughness to the per-part table 0x266a that is taken off an attacker's score | 0x7e19a | todo (enchantments) |
| Hitting a door wears the weapon: the weapon hand's thing damaged by a 2-4 roll (0x24b06, door objects 0x140-0x14f) | 0x24b06 | todo |
| Use X on Y (the target step 0x36b9a): keys on locks (0x36afd), lockpick on a door (0x36a78, "You succeed in picking the lock."), spike on a closed door (0x36c42), rock hammer on a boulder (0x3797e), oil flask on a lantern or wood ("you make a torch", 0x37b60), pole on a switch (0x36e62), anvil on a weapon / armour (repair, 0x36eaf), skull / bones on a gravestone (0x36cf9), the Key of Infinity on its place (0x370df), the orb (0x36ff2); the use switch 0x37ca4 (items 0x121-0x12d: bedroll sleep, seed plant, instruments play, leeches cure poison, fishing pole fish) | 0x36b9a, 0x37ca4 | partial: keys (E at the door finds the key, unlocks and opens) |
| Weapon wear (durability) | ? | todo |
| Weapon enchantment (0x333e:0x257c gives type/value): type 0xC, bit 8 set: damage + (v&7)+1, else attack score + (v&7)+1 (Accuracy / Damage) | 0x254ed | todo |
| Missile weapons, ammunition: launcher table DS 0x5942 (3 bytes a launcher; byte 2 0xC0 = fired by the player's skill); speed = missile x 8 + 0xC0, a missile check vs 10: fumble -0x80, critical +0xC0; damage x speed / 256 | 0x2b2bd | partial (decoding) |
| Weapon in the view: WEAPONS.GR 224 pictures (right hand 0-111, left 112-223; 28 a kind: sword, axe, mace, fist), WEAPONS.DAT positions (per hand 224 bytes, per kind 28 x + 28 y counted up from the 112-line view's bottom; hand = player 0x64 bit 0) | 0x32f0c, 0x33ffd | done |
| Weapon kind: hand weapon -> its skill (3-5) - 3; empty hand, ammunition, anything else -> fist; bow / crossbow / sling -> none | 0x7e580 | done |
| Swing: state 0-2 = slash / bash / thrust, picture 9 x state + f; f 0-3 drawn back, held at 3 while charging, 4-8 the swing, the blow at f 6, over at 9; rest picture 27; raise/lower states 5/3 use 18 + f | 0x330c9, 0x25628 | done (step rate 1/16 s: estimated, the update is a timed event) |
| Attack timing / reach of creatures and player | ? | todo (ours: placeholders) |

## Command modes (the icons left of the view: LFTI.GR, 2 pictures each, dim / lit)
| Rule | UW.EXE | Status |
|---|---|---|
| Icons: Options (menu, no mode), Talk, Get, Look, Fight, Use. The chosen one in [0x268c]: 0 none, 1 Use, 2 Fight, 3 Look, 4 Get, 5 Talk | 0x26fef | decoded |
| A click in the view (0x26fef): mode index = [0x268c] - 1 (none: 2); far-pointer table DS 0x28f: 0 Use 22ef:0e3f ("You are unable to use that from here"), 1 Fight 22ef:0eb9 (the attack: the pointer's row picks thrust / slash / bash), 2 Look 22ef:0d20 (overlay 5a23 describes; in Look mode proper also a Search-skill check, 0x5a76:0x7f), 3 Get 22ef:0a9e ("That is too heavy..." / "You cannot pick that up."), 4 Talk 22ef:0cec (conversation overlay 599c) | 0x270a0 | decoded |
| No icon chosen: a click looks. Only Fight mode attacks; every other mode first finds the thing under the pointer (0x269ba; none: 0x26ac0) | 0x2703a | done: C = Fight mode (off: a click looks; on: a click attacks); look / use / take / talk stay on L, E and the click as comforts |
| Look mode's extra Search check on what is looked at | 0x26e50 | todo |

## Creatures
| Rule | UW.EXE | Status |
|---|---|---|
| Critter table OBJECTS.DAT 0x132 (48 bytes) in DS 0x4a52 | many | partial (combat fields) |
| AI: attitude, goals, movement, fleeing, following | ? | todo (ours: placeholder chase) |
| Attitude field / who is hostile | ? | todo (ours: no conversation = hostile) |

## Light
| Rule | UW.EXE | Status |
|---|---|---|
| Light reach by level: SHADES.DAT word 3 = 3 3 4 4 5 6 7 7 tiles (torch 4, lantern 5, candle 3) | 0x810f8 | done |
| SHADES.DAT other words: the palette shading (LIGHT.DAT 16 tables) | 0x810c6 | ours: lit by real lights |

## Objects
| Rule | UW.EXE | Status |
|---|---|---|
| Can be picked up (COMOBJ byte 3 bit 5) | 0x7b08c | done |
| Look sentences, quality words, owners, writings, gravestones, textures | 0x7ba10, 0x7c0d1 | done |
| Containers: capacity, what they accept | 0x7aaeb | done |
| Light sources: durations, brightness, light slots | 0x2af36 | done |
| Eating/drinking | 0x374ee | done (potion effects todo) |
| Reading books/scrolls | 0x37fa0 | done (spell scrolls, map todo) |
| Keys and locks; lockpicks | 0x36b55, 0x3831b | done (lockpicking todo) |
| Carry weight limit (str x 2 stones): "That is too heavy for you to pick up." | 0x81305 | done |
| Repair, oil, fishing, instruments, spikes | ? | todo |

## The world
| Rule | UW.EXE | Status |
|---|---|---|
| Switches -> use trigger -> trap | 0x83d96 | done |
| Traps: damage, teleport, door, create object, delete object, text string | 0x83d96.. | done |
| Trap chains: after a trap, its link runs (a trap: same tile; a trigger: fires) unless the trap uses its link (door, create, delete) | 0x84803 | done |
| Set variable: z = variable (0: quest bits set/clear/toggle), heading = op (add sub set and or xor shl), value ((q<<5|owner)<<3)|y, 6 bits | 0x83ea4 | done |
| Check variable: vars z..z+heading summed (x set) or packed 3 bits; equal: chain on, else the link's next | 0x83fc5 | done |
| Inventory trap: chain on only when carrying object (q<<5|owner) (z of them) | 0x8463b | done |
| Damage trap: quality hit points; owner set heals | 0x840bd | done (type 2 vs 0 at 70%: todo) |
| Change terrain: rectangle (x+1 by y+1) from the target: height z/8, floor tex q/2, wall tex owner, type heading*2+q&1 (each only if in range) | 0x83e17, 0x73b68 | done (level geometry made again) |
| Traps: arrow 0x83dc3, do 0x84a7b, spell 0x84150, ward/tell 0x846e3; pit / combination do nothing | 0x83d96.. | todo |
| Trigger events (OBJECTS.DAT 0xd92): move 0, step on 1, pick up 2, use 4, look 5, unlock 6, open 7; player needs word-0 bit 11; look with z: search check vs z | 0x83b3b | done (move, pick up, use, look incl. walls' 0x16e objects, open on use); unlock, step-on landing, timer: todo |

## Magic, conversation, the rest
| Rule | UW.EXE | Status |
|---|---|---|
| Spell table: 48 x 4 bytes (category << 3 \| aim, 3 runes x 5 bits, strength); spell n is circle n / 6 + 1 | overlay entry 73 | done |
| Casting: (level + 1) / 2 >= circle (210); mana >= 3 x circle (211); check Casting + 5 vs 2 x circle: fail 212, fumble 214 = harm circle / 2 d8; mana -= 3 x circle; effect fails 213 | 0x78ec8 | done |
| Lasting effects: at most 3; time by strength & 0xC0: 0 2d3, 0x40 2d8 + 6, 0x80 3d20 + 24 player ticks | 0x2b109 | done |
| Enchantment reader: spell object (0x120-0x12F) in the link chain, or the thing itself (quantity + enchanted); major (bit 11) group l >> 6 (0 = spell by number, else category 12 + group, value l & 63); normal category l >> 4, value l & 15; names: block 6 index l (+ 256 major / weapon-armour 12-13) | 0x38b5c | done |
| Weapon enchantment: normal category 12, value 0-7 accuracy + (v & 7) + 1, 8-15 damage + (v & 7) + 1 | 0x254ed | done |
| Worn things (11 slots), normal form: applied as lasting effects while worn | 0x7e6e5 | done |
| Effect dispatcher: 15 categories; table at cs:0x298 of seg 0x311a (file 0x343a0) | 0x34418 | done |
| Lasting application (0x7e007, tables cs:0x542 / 0x562 at file 0x7dcc0): 0 light max; 1 motion bits; 2 protection level -> every part's armour; 3: 1 +3 off attacker score (0x266a), 2-4 stealth / conceal / invisibility bits, 5-9 resistances (DS 0x1b00); 9 curse; 11: 0 freeze 0x282, 1 roaming 0x284, 2 speed 0x283, 3 telekinesis 0x286 = 0, 14 / 15 regeneration 0x3f8 (+1 HP / mana a player tick, 0x2ad93); 12 armour enchantment: Protection (v 0-7) off attacker score, Toughness (8-15) to armour; 13: 4 maze navigation | 0x7e007 | done |
| Noise / visibility rebuilt each turn: 13 - Sneak / 3, 15 - Sneak / 5; stealth -16 noise, conceal -5 / invisibility -16 visibility | 0x7dea4, 0x7e31d | done (their use by creatures: our scale) |
| Resistance check: a kind against a resistance byte: magic part (bits 0-1) resisted when d3 - 1 < level, other shared bits: nothing | 0x25eaf | done |
| Spell areas: a 5 x 5 square 4 tiles ahead (0x38e1d), area spells 3d4 targets, creature spells 1; strength 0x40: open tiles at random (count / 28, up to 4 passes) | 0x35017 | done |
| Movement: jump 141, gravity 4 a tick; leap gravity 2; slow fall fall >= -94, harm accumulator halved; levitate / fly gravity 0, vertical speed x4/5 a tick | 0x1c62f, 0x1d2c2 | done |
| Open question: the player's per-part armour ([0x7272] parts) and how it reduces a blow's damage (ours takes armour off the attacker's score) | 0x24b68 | todo |
| Conversations (VM) | CNV.ARK | done; built-ins (barter, give/take, skills) todo |
| Save / load | ? | todo |
| Sound effects (SOUNDS.DAT) | ? | todo |
