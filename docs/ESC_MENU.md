# The Esc menu

Everything you can change in uwr while you play is in one menu, opened with **Esc**. This page explains every row
of it: what it does, which values it takes, and what it needs in order to work.

## Using the menu

| Do this | To |
| --- | --- |
| Esc | Open the menu while you are in the 3D view; close it again. On the Save and Load pages Esc goes back to the options. |
| Up / Down | Move from row to row (headings are skipped). |
| Right, or a left click | Change the row's value: the next one, or more. |
| Left, or a right click | Change the row's value: the one before, or less. |
| Enter or Space | Choose the row (the same as Right). |
| Mouse wheel, or the scrollbar on the right | Scroll the list. |

The game is paused while the menu is open. At the foot of the menu the row you are on is described: its
value in full, and under it what the row does (up to three lines).

**Changes take hold at once**, and you hear or see them behind the menu, unless the row says "from the next
start". Every change is kept: the game writes it to its settings file (`settings.txt`) or to its own config
file, and reads it back the next time you start.

**Rows come and go.** A row is shown only when its feature can run. A graphics card without ray tracing has no
ray-tracing rows; the sound-tuning rows for Steam Audio appear only while Acoustics is set to High; the AI
rows marked Monstrous appear only at that level, and the model tuning rows only while a language model decides. Each table below says what a row needs.

**Defaults.** "Default" below is the value the game uses when your settings file says nothing. The public
download comes with a settings file that turns the optional features on from the first start: ray tracing with
DLSS Ray Reconstruction, Normal map strength 0.75, Relief shadows at Depth 8, Particles, Decals, Water,
Positional sound, Acoustics High and Echo detail Very high. What your machine cannot do is left out by the game
itself.

A "tile" or "map square" is one square of the game's map: 8 feet, about 2.4 metres.

## Game

| Row | What it does |
| --- | --- |
| **Resume** | Closes the menu and goes on with the game. Esc does the same. |
| **Save game (F5)** | Opens the Save page (below). F5 opens it straight from the game. |
| **Load game (F9)** | Opens the Load page (below). F9 opens it straight from the game. |
| **New game (title screen)** | Goes back to the title screen so you can start a new character. Anything you have not saved is lost. |
| **Quit the game** | Closes the game at once, without asking. Anything you have not saved is lost. |

### The Save and Load pages

Both pages show the same five rows.

| Row | What it does |
| --- | --- |
| **Slot I** to **Slot IV** | The four save slots. A used slot shows its description (the dungeon level and the date and time it was saved); a free one shows "(empty)". On the Save page, choosing a slot saves the game there at once and closes the menu; a game already in that slot is overwritten. On the Load page, choosing a used slot loads it and closes the menu; an empty slot does nothing. |
| **Back** | Back to the options. Esc does the same. |

## Display

| Row | Values | What it does |
| --- | --- | --- |
| **Display** | Ours / The original's | The one switch between the two games. **Ours** is the new full-screen 3D view with its own interface and everything this menu offers, each row as you last set it. **The original's** is the 1992 game as it was: its own screen and its own 3D picture, its own keys and mouse, its own sound (no positional sound or acoustics), its own creatures (Creature AI Original, no hunters), its own door rules and half-second fades, and every enhancement off. While it is chosen the menu shows only the rows that still do something: the Game rows, Display, Full screen, Menu text, Menu background, and the music and sound rows the original had. Nothing you set is lost: choose Ours again and it all comes back. |
| **Full screen** | On / Off | On fills the whole screen; Off plays in a window. |
| **Renderer** | Vulkan (default) / Direct3D 12 | The graphics system that draws the picture. Takes hold at the next start; until then the row also shows which one is running now. Windows only. |
| **Field of view** | 50 to 120 degrees, steps of 5. Default 85 | How wide an angle you see. A higher number shows more at the sides. |
| **Smooth motion** | On (default) / Off | The game's rules move you in small steps. On, the camera glides between those steps; Off, it jumps from one to the next. The rules themselves do not change. |
| **Message fade** | 2, 3, 5, 8, 10, 15 or 30 s, or Never. Default 5 s | How long the message box stays on screen after its newest message. Never keeps it there. |
| **Shut doors from close up** | On (default) / Off | In the original, a door you shut while standing just inside its swing hits you and swings open again. On, such a door (up to a quarter of a square inside its space) shuts all the same, and you are not moved. Off is the original's rule. |
| **Easier door targeting** | On (default) / Off | On, you can point at a door by its frame and by its open leaf from either side, when nothing else is under the crosshair. Off, as the original, only the leaf itself counts. What using the door does is unchanged. |
| **Door blocked message** | On (default) / Off | On, a line in the message box tells you when a door you shut swings open again because you stand in its way. Step back and shut it again. |
| **Font** | The game's own, or one of the fonts found | The letters of the whole interface. The list holds the readable fonts your system has, plus any font files placed in the game's fonts folder. |
| **Font size** | Steps of 5% (shown in pixels and percent). Default 100% | The size of the interface's letters. 100% is the size the game picks for your screen resolution. The smallest is 6 pixels, the largest 60. |
| **Menu text** | Smooth (default) / The original's | This menu's letters: a smooth font at your screen's full sharpness, or the original game's small capitals. |
| **Menu background** | Solid colour (default) / Leather | What lies behind this menu's rows and text: one dark colour, the easiest to read, or the game's own leather. |
| **Conversation text** | Smooth (default) / Original | The letters in conversations. |
| **Message text** | Smooth (default) / The original's | The letters in the message box. Smooth is smaller and easier to read. |
| **Character sheet** | Full (default) / The game's page | The sheet in the inventory (Tab). Full shows every number the rules use; the other choice is the game's own statistics page. |

## Graphics

Rows marked "card" are shown only when the graphics card and its driver can do what the row needs.

| Row | Values | What it does | Needs |
| --- | --- | --- | --- |
| **Resolution** | Information only | Two sizes: the size the 3D view is drawn at, then the size it is shown at. The DLSS rows decide the first. | |
| **DLSS Super Resolution** | On (default) / Off | NVIDIA DLSS makes the picture sharp and steady. It can also draw the view smaller and enlarge it, which is faster (see DLSS mode). Off draws plainly at full size. | Card; otherwise the row reads "Not on this card". |
| **DLSS mode** | DLAA (default), Quality, Balanced, Performance, Ultra Performance | The size the view is drawn at before DLSS enlarges it; the row shows that size in pixels. DLAA draws at full size and only cleans the picture: the best picture. Each mode after it draws smaller and runs faster. | DLSS on. |
| **Ray tracing** | On (default) / Off | Light and shadows are worked out by tracing rays of light through the dungeon on the graphics card. | Card; otherwise "Not on this card". |
| **DLSS Ray Reconstruction** | On / Off (default) | Ray-traced light is grainy before it is cleaned. This is NVIDIA's cleaner (a "denoiser") for it; it cleans and enlarges the picture in one go, in place of Super Resolution. | Ray tracing and DLSS both on; the row tells you which one is missing. |
| **DLSS 5** | On / Off (default) | NVIDIA's neural rendering: a trained network reworks the finished DLSS picture. | Card. |
| **DLSS 5 style** | A - Default, B - Natural (default), C - Cinematic | The look DLSS 5 gives the picture. | Card with DLSS 5. |
| **DLSS 5 intensity** | 0% to 100%, steps of 5. Default 100% | How strongly DLSS 5 changes the picture. | Card with DLSS 5. |
| **DLSS 5 structure** | 0% to 100%, steps of 5. Default 100% | How much fine local detail DLSS 5 adds. | Card with DLSS 5. |
| **Bounced light** | On (default) / Off | Light that hits a wall or floor bounces on into the shadows, so they are less dark. | |
| **Object shadows (.)** | On (default) / Off | Objects and creatures cast shadows. The `.` key switches it in the game too. | Ray tracing. |
| **Textures** | Original (default), Upscaled 256, Upscaled 512 | The pictures on walls and floors: the game's own, or sharper copies enlarged to 256 or 512 pixels a side, in the same colours. | |
| **Normal map detail** | As the textures (default), Original, 256, 512 | A normal map tells the light which way each point of a surface faces, which gives flat stone its bumps. This row picks how detailed those maps are: the same as the Textures row, the 64-pixel ones, or those made from the 256 or 512 pixel textures. | |
| **Joint shadow map detail** | As the textures (default), Original, 256, 512 | The same choice for the maps that darken the joints between stones (see Joint shadows). | |
| **Relief height map detail** | As the textures (default), Original, 256, 512 | The same choice for the height maps that the stones' own small shadows are cast from (see Relief shadows). | |
| **Normal maps (F6)** | On (default) / Off | On, light picks out the bumps and grooves of the stone, so walls and floors look less flat. F6 switches it in the game too. | |
| **Normal map strength** | 0.25 to 4.00, steps of 0.25. Default 1.00 | How deep the stone's bumps look. | Normal maps on. |
| **Joint shadows (AO)** | Off, or 25% to 200% in steps of 25. Default 100% | Darkens the joints and hollows between the stones of walls and floors, where little light would reach ("ambient occlusion", AO). Higher is darker. | |
| **Relief shadows** | Off, or Depth 1 to 8. Default Depth 3 | The stones cast small shadows of their own across walls and floors, away from each light. A higher depth treats the stone's relief as deeper, so the shadows are stronger. | |
| **Light colour** | CAT16 (default) / Bradford | Firelight is orange, but an eye that has got used to it sees it as nearly white. These are two standard formulas for working out that shift; CAT16 is the newer one. The difference is subtle. | |
| **Light fade** | 2 / 3 / 4 tiles (default) / Original | How light dims with distance. With 2 / 3 / 4 tiles a light is at half strength 2 tiles away, 30% at 3 and gone at 4 (a brighter light reaches further in the same proportion). Original uses the game's own shading by distance. | |
| **Fire breathing** | Off, or 10% to 40% in steps of 10. Default 20% | Now and then a fire sags or flares for a few seconds, as real flames do. The value is how deep that swing goes. | |
| **Light adaptation** | Automatic (default), or 0% to 100% in steps of 10 | How far the eye gets used to firelight. 0% leaves the fire's raw orange on everything; 100% makes it white. Automatic works out the amount for a torch-lit dungeon and shows it in brackets. | |
| **HDR output** | On (default) / Off | On an HDR monitor, with HDR switched on in Windows, fire and bright light go brighter than white, up to the monitor's peak. On a monitor that is not in HDR the row reads "On (this monitor: SDR)" and the picture is unchanged. | An HDR monitor. |
| **HDR peak brightness** | The monitor's (default), or 400 to 4000 nits in steps of 100 | The brightest the highlights go. "The monitor's" uses the figure the monitor reports. (A nit is the unit of screen brightness.) | HDR. |
| **HDR paper white** | 80 to 500 nits, steps of 10. Default 200 | How bright plain white is in HDR: the picture's normal range and the interface. | HDR. |

## Effects

| Row | Values | What it does |
| --- | --- | --- |
| **Particles** | On / Off | Sparks, smoke and dust in the air. |
| **Decals** | On / Off | Marks left on floors and walls, such as blood. |
| **Water** | On / Off | Water that moves and bends the view of what lies under it. |
| **Campfire smoke** | On (default) / Off | Grey smoke rises from every campfire, spreads under the ceiling and thins out over a few seconds. Needs Particles. |
| **Water fog** | On (default) / Off | A thin pale fog lies over the water near you and drifts slowly. Needs Particles. |
| **Glowing stones** | On (default) / Off | Glowing rocks, orbs and orb rocks shine and light the floor about a foot round them. Off, they are plain objects, as in the original. |
| **Moonstone light** | Off, or 5% to 50% of a candle, steps of 5. Default 10% | The moonstone fills the room it lies in with a deep blue light, the same everywhere in the room, round corners too. It only shines while it lies on the floor, not in your pack. |
| **Fire elemental light** | Off, or 10% to 100% of a candle, steps of 10. Default 30% | Fire elementals cast a smouldering orange light that moves with them. |

Particles, Decals and Water are off unless your settings file turns them on (the public download's does).

## Sound

### The three levels of sound

The sound rows build on each other. It helps to know the three levels before reading the tables.

1. **Positional sound.** Without it, the game sounds as it did in 1992: each effect is played by the game's
   sound device, with only a little left and right. With it, every effect is placed in the 3D world by OpenAL
   Soft, so it comes from where it happens and fades with distance. It works with the Roland MT-32 as the music
   and effects device, because the effects are the Roland's own sounds, rendered once and then placed. Rows that
   belong to this level: Headphone 3D sound, Effects' sides, Hearing range, the Footstep rows, the two Loudness
   rows.
2. **Acoustics On.** The level's walls now matter. A few dozen sound rays measure the room round you (how big
   it is, how far apart its surfaces are), and a reverb is set to match, so a hall rings longer than a
   corridor. Rock and shut doors between you and a sound muffle it, and a sound round a corner is heard from
   the opening it comes through. This uses OpenAL's built-in effects (a reverb and filters): cheap, and the same
   reverb for every sound in the room. Rows: Reverb level, Map square size, Muffling by walls, Sound round
   corners, and under Sound tuning the Ring time, delay, Reverb strength, Room change and Room ray rows.
3. **Acoustics High.** Everything in level 2, but the echoes are no longer one reverb for the room. Steam Audio
   traces thousands of sound rays from each sound through the level and builds that sound's own echoes from the
   surfaces they hit. With headphones it can also make the sound itself for your two ears (binaural sound),
   work out what walls and doors let through by their material (occlusion), and bring a hidden sound to you
   along the real routes through the level (pathing). Rows: Echo detail, Footstep echo, and under Sound tuning
   every row that names Steam Audio or echoes.

If a level cannot start (no sound device, or a missing library), the game falls back to the level below and
plays on.

### Sound rows

| Row | Values | What it does | Needs |
| --- | --- | --- | --- |
| **Music on / off** | On / Off | The game's music. | |
| **Music volume** | 0% to 100%, steps of 5. Default 100% | How loud the music is. | |
| **Sound on / off** | On / Off | The game's sound effects. | |
| **Sound volume** | 0% to 100%, steps of 5. Default 100% | How loud the sound effects are, the positional ones included. | |
| **Music and effects device** | None, PC speaker, Ad Lib, Sound Blaster, Sound Blaster Pro, Pro Audio Spectrum, Roland MT-32 | Which of the original game's sound devices plays the music and the effects. Takes hold at the next start (the row says so after a change). The Roland MT-32 needs its ROM files. With any other device the effects are that device's own, without positions or acoustics. | |
| **Speech device** | None, Sound Blaster, Sound Blaster Pro, Pro Audio Spectrum | Which device plays the game's recorded speech. Takes hold at the next start. | |
| **Positional sound** | On / Off | Level 1 above: each sound comes from the place where it happens. | Roland MT-32 as the device. |
| **Headphone 3D sound** | On (default), Off, Automatic | For headphones. Ordinary stereo can only say left or right; this filters each sound the way your head and ears would (an "HRTF"), so you can also tell ahead from behind. Automatic turns it on when the system reports headphones. Leave it Off for loudspeakers. | Positional sound. |
| **Effects' sides** | Corrected (default) / As the MT-32 plays them | The Roland MT-32 swaps left and right, so the original's effects came out on the wrong side. Corrected puts each sound on its true side. | |
| **Hearing range** | Original (6 tiles), 9, 12, 16 or 24 tiles | How far away a sound can be heard. A sound is at full strength within one tile and fades evenly to nothing at the range. | Positional sound. |
| **Footstep width** | Middle, then 50% to 800% in steps of 50; 100% reads Original | How far to the left and right your own two feet sound. Original is the game's own, barely apart. Middle puts both in the centre. 400% is half-way to each side. | Positional sound. |
| **Footstep volume** | Off, or 10% to 200% in steps of 10. Default 80% | How loud footsteps are, yours and the creatures', next to the other sounds. | Positional sound. |
| **Footstep echo** | Off, or 10% to 200% in steps of 10. Default 50% | How strongly footsteps echo in the room, next to other sounds' echoes. | Acoustics High. |
| **Footsteps by floor** | On (default) / Off | On, a step sounds like the floor under it: wood, water, earth, metal and so on. Stone keeps the original's sound. Off, the original's two step sounds everywhere. | Positional sound. |
| **Acoustics** | Off, On, High | Levels 2 and 3 above. | Positional sound. |
| **Echo detail** | Low (1,024 rays), Medium (2,048, default), High (4,096), Very high (8,192) | How many sound rays Steam Audio traces, and with them how many times each may bounce (8, 16, 32 or 64). More gives smoother echoes and costs more processor time. | Acoustics High. |
| **Reverb level** | Off, or 10% to 200% in steps of 10. Default 100% | How loud the rooms' echo is, at either Acoustics level. Off leaves the muffling alone. | Acoustics On or High. |
| **Map square size** | 1.5 to 4.0 metres a tile, steps of 0.1. Default 2.4 | How many metres one square of the map counts as for sound. The rays measure rooms in these squares, so a bigger square makes every room bigger and its echo longer. 2.4 m is the game's own 8 feet. | Acoustics On or High. |
| **Muffling by walls** | Off, or 10% to 100% in steps of 10. Default 100% | How much rock and shut doors quieten and dull a sound behind them. Off leaves the reverb alone. | Acoustics On or High. |
| **Sound round corners** | On (default) / Off | A sound round a corner is heard from the opening it comes through, along the way a person would walk, and not straight through the rock. | Acoustics On or High. |

## Sound tuning

These rows are the fine controls of the sound system: every number it uses is here, so that it can be tuned by
ear. Most players never need them. Each is heard at once.

### Always shown

| Row | Values | What it does |
| --- | --- | --- |
| **Music buffer** | 50 to 600 ms, steps of 25. Default 150 | How much music is kept ready ahead of the sound device. More means fewer gaps in the tune when the computer is busy; the music then answers a little later. |
| **Music thread first** | On (default) / Off | The part of the program that makes the music runs ahead of the game's other work, so a busy moment does not leave a gap in the tune. |
| **Music in the menu** | On (default) / Off | On, the music plays on while this menu is open, though the game is paused. Off, the music stops with the game. |

### With Positional sound

| Row | Values | What it does |
| --- | --- | --- |
| **Own footsteps plain** | On / Off (default) | Your own footsteps exactly as the Roland plays them: no echo, no floor sound, no placing for headphones, no muffling. A way to compare by ear with everything else left on. |
| **Roland's own reverb** | Automatic (default) / On / Off | The Roland sound module adds a small room echo of its own to every sound effect. Automatic turns it off while Acoustics is On or High, because the game then adds the echo of the real room you stand in; with Acoustics off it stays, as in the original. On keeps it always, Off never. The effects are made again when you change it, which takes a moment. |
| **Effects interrupt music** | On / Off (default) | The original took a music channel for every sound effect, so notes of the tune went missing while you walked or fought. Off, the music plays whole, because the positional sound plays the effects. On is the original's behaviour. |
| **Loudness, speakers** | 25% to 1000%, steps of 5. Default 215% | The loudness of the positional sound against the game's own mix, when Headphone 3D sound is off. |
| **Loudness, headphone 3D** | 25% to 2000%, steps of 10. Default 520% | The same when Headphone 3D sound is on (the headphone filter loses more level, so the figure is higher). |

### With Acoustics On or High

| Row | Values | What it does |
| --- | --- | --- |
| **Room measured here** | Information only | What the sound rays measure where you stand now: how long the room rings, in seconds, for low, middle and high notes, and how far sound travels between its surfaces, in metres. It changes from room to room. |
| **Furniture in the sound** | On (default) / Off | Tables, benches, barrels, chests, pillars and boulders stand in the way of sound and reflect it, each by what it is made of, so a room sounds by what is in it. Off, the rooms count as empty. |
| **Doorways their real size** | On (default) / Off | For sound, a doorway is an opening half a square wide in a wall, with the door in it when shut. Off, an open door leaves its whole square open, and two rooms joined by a door ring as one. |
| **Muffling change time** | 0 to 2000 ms, steps of 20. Default 300 | How quickly a sound turns muffled or clear when a wall or a door comes between you and it. |
| **Room rays** | 16 to 512, steps of 16. Default 64 | How many rays measure the room round you. |
| **Room ray bounces** | 3 to 48. Default 12 | How many bounces each of those rays is followed for. |
| **Room ray reach** | 4 to 64 map squares, steps of 2. Default 16 | How far a room ray may travel before its sound counts as gone. |
| **Corner reach** | 4 to 32 map squares. Default 12 | How far round corners a sound is followed, in squares walked. |

### With Acoustics On only

These shape the OpenAL reverb of level 2. At High the echoes come from Steam Audio and these rows are hidden.

| Row | Values | What it does |
| --- | --- | --- |
| **Ring time** | 10% to 400%, steps of 10. Default 100% | How long rooms ring, as a share of what the rays measure. 100% is as measured. |
| **Ring time, high notes** | 10% to 400%, steps of 10. Default 100% | The same for the high notes alone. |
| **Ring time, low notes** | 10% to 400%, steps of 10. Default 100% | The same for the low notes alone. |
| **First echo delay** | 0% to 400%, steps of 10. Default 100% | The wait before the first echoes, as a share of the measured distance to the nearest surfaces. |
| **Late ring delay** | 0% to 400%, steps of 10. Default 100% | The wait before the room's long ring follows the first echoes. |
| **Reverb strength** | 0% to 100%, steps of 2. Default 32% | The reverb's own loudness, before Reverb level scales it. |
| **Room change time** | 0 to 2000 ms, steps of 10. Default 170 | How quickly the ring changes when you walk into another room. The change is all but done after three times this. |

### With Acoustics High only

| Row | Values | What it does |
| --- | --- | --- |
| **Sound by Steam Audio** | On (default) / Off | With Headphone 3D sound: each sound itself is made for your two ears by Steam Audio's binaural rendering, from the place where it happens. Off, OpenAL places it. |
| **Muffling by Steam Audio** | On (default) / Off | With Sound by Steam Audio: what the walls, doors and floors between you and a sound let through is worked out by Steam Audio's rays, from what each surface is made of. Off, by the game's own rays. |
| **Sound level (Steam Audio)** | 0% to 100%, steps of 2. Default 30% | With Sound by Steam Audio: how loud those sounds are against the game's own mix. |
| **Sound paths by Steam Audio** | On (default) / Off | With the two rows above: a sound you cannot see reaches you along the routes Steam Audio finds through the level, round corners and through open doorways. It comes from the side it arrives and is duller the more it bends. A shut door closes its route. Off, the game's own way round a corner is used (Sound round corners). |
| **Sound paths level** | 0% to 400%, steps of 10. Default 100% | How loud a sound is along those routes. |
| **Sound source size** | 5 to 300 cm, steps of 5. Default 50 | How big a sound's source counts as for the muffling rays. Bigger, and a sound fades more gradually as it passes behind a corner. |
| **Echoes at once** | On (default) / Off | A new sound's echoes start with it, using the echoes of the place where you stand until its own have been worked out. Off, they wait for their own and can come late. |
| **Echo strength** | 0% to 60%. Default 5% | How loud the echoes are against the sound itself, before Reverb level scales them. |
| **Longest echo** | 0.5 to 6 s, steps of 0.25. Default 2 s | The longest an echo can ring. A change starts the echoes again. |
| **Echo bounces** | As Echo detail (default), or 4 to 64 in steps of 4 | How many times a sound ray bounces before it is dropped. Too few and an echo stops short. "As Echo detail" takes 8, 16, 32 or 64 from that row. |
| **Wall roughness** | 0% to 100%, steps of 5. Default 25% | How much of each bounce scatters in every direction and not as off a mirror. |
| **Echo buffer** | 2 to 6 blocks. Default 4 | How many blocks of echoes (about 11 ms each) are kept ready ahead. Too few and the echoes crackle; more and they come a little later. |
| **Echoes straight to the ears** | On (default) / Off | The echoes, already made for two ears, go to the headphones as they are. Off, they are placed a second time by Headphone 3D sound, which makes them duller. |

## Controls

| Row | Values | What it does |
| --- | --- | --- |
| **Mouse sensitivity** | 0.1 to 4.0, steps of 0.1. Default 1.0 | How fast the mouse turns the view. Higher is faster. |

The keys themselves are listed in [KEYBINDS.md](KEYBINDS.md).

## AI

The creatures' numbers (hit points, damage, speed, every dice roll) are the original's at every setting here.
These rows change only what a creature chooses to do, and a creature acts only on what it saw or heard itself or
was told by its kin: it never knows where you are by cheating. "Kin" are hostile creatures of the same race;
"in earshot" is that creature type's own hearing range. A named character (one with its own conversation) keeps
the original's behaviour, apart from a friendly one's nod or warning.

### Creature AI and its parts

**Devious** makes each creature cunning on its own. **Monstrous** adds creatures working together. Every part
below has its own row, on by default; a row is shown from its own level up. Once a part has been used in this
session its row shows how often: "On (used 3)".

| Row | Values | What it does | Needs |
| --- | --- | --- | --- |
| **Creature AI** | Original, Devious, Monstrous | How cunning creatures are. **Original** is the game as shipped. At **Devious** a creature that loses you walks to where it last knew you were and searches there (about 30 seconds); one that hears a blow, a door, a lever, a spell or a hard landing, or sees your light, goes to look; some lie in ambush by a corner or a door (up to a minute); a badly hurt one may run. The dimmest (worms, slugs, rats, lurkers, headlesses) just attack. At **Monstrous** creatures also call, fetch and wait for one another, as the rows marked Monstrous describe. | |
| **Friendly creatures** | On (default) / Off | Friendly creatures notice you when you come near: one may nod to you, follow you for a while if your weapon is drawn (half a minute at most, never more than 8 squares from its home), back away to its home, or turn to you and point out a hostile creature it can see. They never fight for you. Off, friendly creatures behave as in the original. | Devious or Monstrous. |
| **Hit and run** | On (default) / Off | A creature that shoots or casts spells holds its ground and lets you come to it (for up to 20 seconds, while it can see you), strikes when you are in range, and when you are close backs off a few steps before it strikes again. At Monstrous it falls back to its kin instead, when it has any in earshot. | Devious or Monstrous. |
| **Hears you run** | On (default) / Off | Your footsteps are heard when you run, by creatures close by (about a third of a creature's hearing range), and they come for the sound. Walking is silent to them, so walking past is a real choice. Off, footsteps are never heard, as before. | Devious or Monstrous. |
| **Searches the ways out** | On (default) / Off | A creature that has lost you and found nothing where it last knew you were goes on to the nearest door within 6 squares of that place, in case you left by it, before it gives up. The hunt lasts 15 seconds longer for that walk. | Devious or Monstrous. |
| **More ambushers** | On (default) / Off | Any clever creature (wits 10 and up) may lie in ambush. Off, only goblins, mountainmen, lizardmen, outcasts, fighters and giant spiders do. The fearless and mindless (skeletons, ghosts, ghouls, trolls, golems and the like) and creatures that live in water never ambush. | Devious or Monstrous. |
| **Wary after a fight** | On (default) / Off | A creature that hunted you and lost you, or ran from you, remembers it for five minutes. The next time it sees or hears you it does not charge: it lies in wait for you (once; if it cannot ambush, it attacks as usual). | Devious or Monstrous. |
| **Takes cover** | On (default) / Off | A creature that fights hand to hand, cannot get to you (you are across water, up a ledge) and was hurt in the last 8 seconds does not stand there to be shot: it goes to the nearest square within 5 that you cannot see, and waits. When it sees you again it comes out; hurt again with no way to you, it goes back. Not for the dimmest creatures or those that live in water. | Devious or Monstrous. |
| **Picks its ground** | On (default) / Off | A clever creature (wits 10 and up) that sees you from 4 squares or more does not charge across the open. One that fights hand to hand, when you stand in the open, takes a doorway or a passage one square wide within 3 squares of itself and lets you come to it; it goes for you when you are within 2 squares, when it is hurt, or after 15 seconds. A shooter or caster with nothing between you takes a square from which it sees you across water, lava or a drop, and shoots from there. Once a minute at most, and not when you look badly hurt. | Devious or Monstrous. |
| **Runs out of sight** | On (default) / Off | A creature that flees does not run straight away from you: it runs for the nearest square within 5 that you cannot see, and hides there for up to 45 seconds. If you find it, it fights. With no such square it flees as in the original. | Devious or Monstrous. |
| **Learns how you fight** | On (default) / Off | A creature that fights hand to hand and is hurt while you are 4 squares or more away (an arrow, a spell) stops walking into it: once a fight it goes out of your sight and waits 20 seconds for you to come, then comes on as before. A shooter or caster that has felt your blade and never your arrows backs off when you are within 4 squares, not 3. | Devious or Monstrous. |
| **Fetches kin** | On (default) / Off | A clever creature that loses you, or hears a noise you made, fetches the kin in its earshot and goes to the place with them. (Until 2026-11-17 this was part of Devious.) | Monstrous. |
| **Calls for help** | On (default) / Off | A creature that finds you while its kin are in earshot but not yet fighting calls them to the place. A creature calls once a minute at most. The dimmest creatures, and the fearless ones (skeletons, ghosts, ghouls, trolls, golems and the like), never call. | Monstrous. |
| **Calls passed on** | On (default) / Off | A clever creature that is called repeats the call to the kin in its own earshot, so the word spreads beyond the first caller's hearing. A call is passed on twice at most. | Monstrous. |
| **Strike together** | On (default) / Off | Creatures called to a fight do not arrive one by one: they gather three squares short of the place and go in together, after 10 seconds at the latest. | Monstrous. |
| **Cut off the retreat** | On (default) / Off | When two or more kin answer a call, one of them does not come to the place: it goes to the nearest door behind you (on the far side of you from the caller, within 6 squares), or failing a door to a square three beyond you, so that your way back is closed. It knows only where the caller met you. | Monstrous. |
| **The lure** | On (default) / Off | A clever creature that sees you from three squares or more, with kin in earshot who are not yet fighting, does not attack and does not call them to it. It tells them to lie in wait round the nearest of them, shows itself, and falls back there to turn and wait with them. If you follow, you walk in among them; if you do not, they come hunting after a minute. A creature tries this once in two minutes at most, and not when you look badly hurt. | Monstrous. |
| **Pack courage** | On (default) / Off | Spiders, bats and other creatures of low wits will not fight you alone when kin are in earshot: they fall back to them once, and then all attack together. With no kin about, with kin already beside it, or when you look badly hurt, it attacks at once. | Monstrous. |
| **Fall back when hurt** | On (default) / Off | A clever creature whose health drops low (under about three eighths; goblins, lizardmen, mountainmen and outcasts already at half) runs to its nearest kin, once a fight, and comes back with them. It does not run when you look badly hurt yourself. The fearless creatures never do this. | Monstrous. |
| **Surround** | On (default) / Off | When two of its kin already fight you hand to hand and a third is 2 to 5 squares off, the third does not queue behind them: once a fight it goes to a square beside or behind you (6 seconds at most to get there), then strikes. | Monstrous. |
| **Won't be drawn out** | On (default) / Off | A creature whose fight began with kin in earshot does not follow you more than 8 squares from that place when none of its kin are with it and you are not at hand: it leaves you, goes back and waits there. If you come back it fights again. At most once in 30 seconds, and not when you look badly hurt. | Monstrous. |
| **Death cry** | On (default) / Off | When a hostile creature dies, its kin at their posts within their own hearing of the place are roused: each goes to the body, which is all it knows of, and is wary and alarmed afterwards (the Wary and Alarm rows). The fearless and the dimmest neither cry nor answer. With AI messages on you read "The goblin's death cry echoes." | Monstrous. |
| **Fighting roles** | On (default) / Off | A shooter or caster whose kin are fighting you hand to hand keeps behind them: when you come within three squares of it, it backs off a few steps and shoots again, while the others hold you. | Monstrous. |
| **Alarm** | On (default) / Off | Creatures that were called to a fight stay roused for three minutes. When their hunt finds nothing they do not go back to their posts: each watches a door or corner near the place for up to a minute, hunts again, and so on until the alarm has run out. | Monstrous. |
| **Hunters bring the pack** | On (default) / Off | A level hunter that goes after you fetches its nearest kin first, every time. Off, and at Devious, hunters come alone. | Monstrous, and Level hunters on. |
| **AI messages** | On (default) / Off | Lines in the message box that tell you what a creature chose: "The goblin shrieks for help", "runs to its kin", "falls back to its kin", "backs away, watching you" (Monstrous), and a friendly creature's "nods to you", "eyes your weapon and follows you", "backs away from you", "points north: a ... is there". Off, the creatures do the same things without the lines. | Devious or Monstrous. |
| **Level hunters** | Off, 1 a level, 2 a level (default) | One or two of the cleverest hostile creatures on each level keep track of you across the whole level. A hunter notices signs of you (noises you made, your light or a light put out, doors opened, things taken, a sprung trap, its kin found dead), makes a short plan (go to where you were last seen, wait by a door, search, follow a light, go to water or rest when wounded, give up) and remembers it across a save and load. If hunters cannot run the row says why: "(no model)" means Decisions by is set to Language model and the model is not loaded. | Devious or Monstrous. |
| **AI log (Ctrl+L)** | On / Off (default) | Shows every decision a creature makes, with its reason, in the message box as it happens ("goblin #12 at 30,14: chose: ambush"). For watching how the AI works. Ctrl+L switches it in the game too. It is not kept: off again at the next start. | Devious or Monstrous. |
| **AI log in the log file** | On (default) / Off | Every decision a creature makes is also written to `home\logs\uwr.log` (and the terminal the game was started from), with the game time in seconds, whether Ctrl+L is on or not. A line that uses one of the parts ends with the part's name and how often it was used this session, "[Hit and run #3]"; when the game ends one line gives the whole count. | Devious or Monstrous. |

### Who decides

| Row | Values | What it does | Needs |
| --- | --- | --- | --- |
| **Decisions by** | Rules (default) / Language model | Who picks among the choices a creature has (hunt, investigate, ambush, fetch kin, go back, attack, flee; a friendly one's nod, follow, keep away, warn; a hunter's plan). **Rules** is the built-in rulebook: it answers at once and needs nothing. **Language model** asks an AI model file on your own computer; its answer comes a moment later (the rulebook's answer is used until then for a creature that sees you). The model is loaded only while this row is set to it. Nothing is sent over the internet. | Devious or Monstrous, and at least one model file. |
| **Language model** | The model files found | The model file used when Decisions by is Language model. Left and Right step through the files in the game's model folder (`.gguf` files). | At least one model file. |

### Language model tuning

Shown only while Decisions by is Language model. Most players never need these rows.

| Row | Values | What it does |
| --- | --- | --- |
| **Model runs on** | Graphics card (default) / Processor | Where the model is worked out. The graphics card is much faster and holds the model in its memory beside the game's picture. The processor is slower but leaves the graphics card to the picture. Takes hold the next time the model is loaded (at the next start, or after it was unloaded for being idle). |
| **Model answers by** | Scoring (default) / Grammar | How the answer is read from the model. Scoring gives the model the choices and rates each one in a single pass, up to eight creatures' questions at once: fast. Grammar lets the model write its answer out, held to the allowed words: slower. Takes hold at once. |
| **Model questions** | 1, 2, 5, 10, 20 (default), 30, 40 or 60 a second | The most questions creatures may put to the model each second. Any beyond that are not asked, which keeps a crowded level from swamping the model. |
| **Model answer too old** | 1, 2, 3 (default), 5, 8 or 12 s, or Never | An answer that arrives later than this after its question is thrown away, because the moment it was asked about has passed. |
| **Model unloaded after** | 1, 2, 5, 10 (default), 20, 30 or 60 min, or Never | After this long without a question the model is unloaded and its memory given back to the game; it is loaded again when the next question comes. |

The settings file has one more switch with no row: `llm=0` keeps the model from ever loading. Since Decisions by
is Rules by default and the model loads only when that row asks for it, the row does the same job.

More about the creatures' AI and the language model: [LLM_NPC_AI.md](../LLM_NPC_AI.md).
