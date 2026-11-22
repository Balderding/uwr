# The Esc menu

Everything you can change in uwr while you play is in one menu, opened with **Esc**. This page explains every row
of it: what it does, which values it takes, and what it needs in order to work.

## Using the menu

| Do this | To |
| --- | --- |
| Esc | Open the menu while you are in the 3D view; close it again. On the Save and Load pages Esc goes back to the options. |
| Tab or Page Down | Open the next tab. |
| Shift+Tab or Page Up | Open the tab before. |
| A left click on a tab's name | Open that tab. |
| Up / Down | Move from row to row (headings are skipped). |
| Right, or a left click | Change the row's value: the next one, or more. |
| Left, or a right click | Change the row's value: the one before, or less. |
| Enter or Space | Choose the row (the same as Right). |
| Mouse wheel, or the scrollbar on the right | Scroll the list. |

The game is paused while the menu is open. At the foot of the menu the row you are on is described: its
value in full, and under it what the row does (up to three lines).

**Tabs.** The rows are sorted into tabs, one for each section of this page: Game, Display, Graphics, Sprites, Effects,
Fog and smoke, Sound, Sound tuning, Controls, Keys, AI and Enhancements. Their names stand in a strip under the title, on one or two lines;
the open tab is the one in gold with a gold bar under it, and only its rows are listed. A tab that has nothing to
show at the moment is left out of the strip. The menu opens on the tab you were on last, until you close the game.

**Preset and Default settings.** Every tab except Game begins with the same two rows:

| Row | What it does |
| --- | --- |
| **Preset** | A ready-made set of the tab's settings. Left, Right, Enter or a click go to the next one (or the one before) and apply it at once. The row always shows the preset that the tab's rows match right now. As soon as you change a row so that no preset matches, it reads **Custom**: your own mix. Nothing is lost by that; pick a preset again whenever you like. |
| **Default settings** | Puts every row of the tab back to its default, the value the game uses when your settings file says nothing. It acts on Enter, Space or a left click only, so Left, Right and a right click cannot set it off by accident. It is the same as choosing the preset called Default. |

Some rows are never touched by a preset or by Default settings, because they need a restart or depend on your
machine: Display, Full screen, Renderer, Font, Music on / off and Sound on / off, the two sound device rows, HDR
peak brightness and HDR paper white, Language model and Model runs on, the AI log rows, and the rows that only
give information. Music volume and Sound volume are put back to 100% by Default only; the other Sound presets
leave them as they are. A row that is not shown at the moment, or that your graphics card does not offer, is
passed over.

| Tab | Presets | What each one sets |
| --- | --- | --- |
| Display | Default | Field of view 85, Smooth motion On, Message fade 5 s, the three door rows On, Font size 100%, Menu text, Conversation text and Message text Smooth, Menu background Solid colour, Quick menu drawing On, Pointer in menus System, Character sheet Full. |
| Graphics | Default | DLSS Super Resolution On in DLAA mode, Ray tracing On, Ray Reconstruction Off, DLSS 5 Off (style B - Natural, intensity and structure 100%), Bounced light On, Object shadows On, Textures Original, the three map detail rows As the textures, Normal maps On at strength 1.00, Joint shadows 100%, Relief shadows Depth 3, Light colour CAT16, Light fade 2 / 3 / 4 tiles, Fire breathing 20%, Light adaptation Automatic, HDR output On. |
| | Fastest | As Default, but DLSS mode Performance, Bounced light Off, Object shadows Off, Relief shadows Off. |
| | Balanced | As Default, but DLSS mode Quality and Textures Upscaled 256. |
| | Best picture | As Default (DLAA), but DLSS Ray Reconstruction On, Textures Upscaled 512 and Relief shadows Depth 8. |
| Sprites | (no presets) | Each row is switched on its own. |
| Effects | Default | Particles, Decals and Water Off; Campfire smoke, Water fog and Glowing stones On; Moonstone light 10%, Fire elemental light 30%. |
| | All off | Every row of the tab Off. |
| | All on | As Default, with Particles, Decals and Water On. |
| Fog and smoke | Default | Every row of the tab at its default (the main Fog and smoke switch is left as it is). |
| | Light | Fog and smoke half as thick, and less of both given off. |
| | Heavy | Fog and smoke twice as thick, and more of both given off. |
| | Faster | Coarser, smaller and cheaper: fewer cells, a shorter reach, fewer pressure passes and view steps, light refreshed a quarter at a time, no self-shadowing. |
| | Finest | Finer and wider: more cells, a longer reach, more pressure passes and view steps, two light rays a cell. |
| Sound | Default | Music and Sound volume 100%, Positional sound On, Acoustics High (as the game comes), and every other row of the tab at its default (Headphone 3D sound On, Effects' sides Corrected, Hearing range Original, Footstep width Original, Footstep volume 80%, Footstep echo 50%, Footsteps by floor On, Echo detail Medium, Reverb level 100%, Map square size 2.4 m, Muffling by walls 100%, Sound round corners On). |
| | Original | Positional sound Off: the game's own plain sound. |
| | Positional | Positional sound On and Acoustics On. |
| | Full | Positional sound On, Acoustics High, Headphone 3D sound On. |
| Sound tuning | Default | Every dial that is shown back at the default its row names. |
| Controls | Default | Mouse sensitivity 1.0. |
| Keys | (no presets) | Its own **Default keys** row puts every command back on the key it came with. |
| AI | Default | Creature AI Devious, every part's row On, AI messages On, Level hunters 2 a level, Decisions by Rules, Model answers by Scoring, Model questions 20 a second, Model answer too old 3 s, Model unloaded after 10 min. |
| | Original | Creature AI Original. |
| | Devious | Creature AI Devious, with every part of that level On. |
| | Monstrous | Creature AI Monstrous, with every part On. |

The Sound presets other than Default, and the AI presets, also put the tab's other rows at their defaults, as
"As Default, but" says for the Graphics ones. The public download's settings file differs from Default in a few
rows (see Defaults below), so a fresh copy may show Custom on some tabs.

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
| **Start-up settings (F11)** | Opens the settings screen the game has at its start, over the game: mouse capture (Follow / Lock), mouse-look speed, the master volume, the window (full screen, its scale, 4:3, whole-number scaling), the game's and the Roland ROMs' folders, recording sessions, and whether that screen shows at each start. F11 closes it again (or opens it straight from the game). |
| **Quick fades** | On (default): the map and the game's screens come up at once. Off: the original's half-second fades. Kept in uw1port.cfg (quick_fades). |
| **Map what I saw** | On (default): the map marks every place you have seen in the 3D view: in your field of view, in plain sight (rock and shut doors block it, a portcullis does not), and lit by your light, a light of the level or glowing lava. A door stays on the map once seen. Kept with your saves (SAVE0\UWRMAP.DAT, copied into each save slot with the rest). Off: the original's map alone; it only marks what its own narrow view (16 tiles deep, about 90 degrees, well lit) saw. The original's map is still kept underneath and saved as before either way, and the experience for exploring is the original's. Kept in uw1port.cfg (map_seen). (Added 2026-11-21.) |
| **Map level arrows** | Levels 1 to 8 (default): the map's up and down arrows (and the modern keys' S and W) stop at level 8, the last level with a map. Original (to 99): they go on to level 99, as the original's do, every page past 8 blank. Kept in uw1port.cfg (map_arrows8). (Added 2026-11-21.) |
| **Map notes kept right** | On (default): the notes you write on the map are saved as you leave them. Off: the original's faults: when you erase the last note left, it comes back the next time you open the map; of two erased notes next to each other in the list, the second stays. Kept in uw1port.cfg (map_notes_fixed). (Added 2026-11-21.) |
| **Log detail** | How much the game writes to uwr.log. Normal is enough unless you are chasing a problem. Errors only: just what went wrong. Warnings: also what the game worked round. Normal (default): also what it is doing. Everything: also the fine detail. Takes hold at once, for the game and for the renderer. Kept in uw1port.cfg (log_detail). If UWR_LOGLEVEL is set when the game starts, it wins at the start. |
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
| **Display** | Ours / The original's | The one switch between the two games. **Ours** is the new full-screen 3D view with its own interface and everything this menu offers, each row as you last set it. **The original's** is the 1992 game as it was: its own screen and its own 3D picture, its own keys and mouse, its own sound (no positional sound or acoustics), its own creatures (Creature AI Original, no hunters), its own door rules and half-second fades, and every enhancement off. While it is chosen the menu shows only the rows that still do something: the Game rows, Display, Full screen, Menu text, Menu background, Quick menu drawing, and the music and sound rows the original had. Nothing you set is lost: choose Ours again and it all comes back. |
| **Full screen** | On / Off | On fills the whole screen; Off plays in a window. |
| **Renderer** | Vulkan (default) / Direct3D 12 | The graphics system that draws the picture. Takes hold at the next start; until then the row also shows which one is running now. Windows only. |
| **Field of view** | 50 to 120 degrees, steps of 5. Default 85 | How wide an angle you see. A higher number shows more at the sides. |
| **Smooth motion** | On (default) / Off | The game's rules move you in small steps. On, the camera glides between those steps; Off, it jumps from one to the next. The rules themselves do not change. |
| **Vertical sync** | On (default) / Off | On, each picture waits for the screen's refresh, so there is no tearing. Off, pictures go out as soon as they are made. Takes hold at the next start (the row says so until then). |
| **Message fade** | 2, 3, 5, 8, 10, 15 or 30 s, or Never. Default 5 s | How long the message box stays on screen after its newest message. Never keeps it there. |
| **Shut doors from close up** | On (default) / Off | In the original, a door you shut while standing just inside its swing hits you and swings open again. On, such a door (up to a quarter of a square inside its space) shuts all the same, and you are not moved. Off is the original's rule. |
| **Easier door targeting** | On (default) / Off | On, you can point at a door by its frame and by its open leaf from either side, when nothing else is under the crosshair. Off, as the original, only the leaf itself counts. What using the door does is unchanged. |
| **Door blocked message** | On (default) / Off | On, a line in the message box tells you when a door you shut swings open again because you stand in its way. Step back and shut it again. |
| **Font** | The game's own, or one of the fonts found | The letters of the whole interface. The list holds the readable fonts your system has, plus any font files placed in the game's fonts folder. |
| **Font size** | Steps of 5% (shown in pixels and percent). Default 100% | The size of the interface's letters. 100% is the size the game picks for your screen resolution. The smallest is 6 pixels, the largest 60. |
| **Menu text** | Smooth (default) / The original's | This menu's letters: a smooth font at your screen's full sharpness, or the original game's small capitals. |
| **Menu background** | Solid colour (default) / Leather | What lies behind this menu's rows and text: one dark colour, the easiest to read, or the game's own leather. |
| **Quick menu drawing** | On (default) / Off | On: this menu's picture is kept and drawn again only when something in it changes (a row, a value, the row under the pointer, the picture behind it), and the pointer is laid over its own small box, so the pointer moves at the game's full frame rate. Off: the whole menu is drawn again every frame, as before (at 1920x1080 that took about 19 ms a frame, and more on a bigger screen). (Added 2026-11-22.) |
| **Pointer in menus** | System (default) / Drawn | System: in this menu and the inventory the pointer is the system's own cursor (made from the game's own cross, or from the thing you carry and its count), so it moves at the monitor's refresh whatever the game's frame rate. Drawn: the game draws the pointer into its picture as before, so it moves only as often as a frame is drawn. The world's pointer and conversations keep the original's own. (Added 2026-11-22.) |
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
| **DLSS 5 tone** | 0.00 to 2.00, steps of 0.05. Default 1.00 | DLSS 5's tone. Takes hold at the next start. | Card with DLSS 5 |
| **DLSS 5 saturation** | 0.00 to 2.00, steps of 0.05. Default 1.15 | How strong DLSS 5 makes the colours. Takes hold at the next start. | Card with DLSS 5 |
| **Bounced light** | On (default) / Off | Light that hits a wall or floor bounces on into the shadows, so they are less dark. | |
| **Light bounces** | 1 (default) to 4, or As many as build up | How many times bounced light bounces again. Above 1 the light probes feed their light back. | Card with ray tracing |
| **Bounce ray reach** | 2 to 32 tiles, steps of 1. Default 8 | How far a bounced light ray goes. Longer finds more light and costs a little more. | Card with ray tracing |
| **Object shadows (.)** | On (default) / Off | Objects and creatures cast shadows. The `.` key switches it in the game too. | Ray tracing. |
| **Textures** | Original (default), Upscaled 256, Upscaled 512 | The pictures on walls and floors: the game's own, or sharper copies enlarged to 256 or 512 pixels a side, in the same colours. | |
| **Normal map detail** | As the textures (default), Original, 256, 512 | A normal map tells the light which way each point of a surface faces, which gives flat stone its bumps. This row picks how detailed those maps are: the same as the Textures row, the 64-pixel ones, or those made from the 256 or 512 pixel textures. | |
| **Joint shadow map detail** | As the textures (default), Original, 256, 512 | The same choice for the maps that darken the joints between stones (see Joint shadows). | |
| **Relief height map detail** | As the textures (default), Original, 256, 512 | The same choice for the height maps that the stones' own small shadows are cast from (see Relief shadows). | |
| **Normal maps (F6)** | On (default) / Off | On, light picks out the bumps and grooves of the stone, so walls and floors look less flat. F6 switches it in the game too. | |
| **Normal map strength** | 0.25 to 4.00, steps of 0.25. Default 1.00 | How deep the stone's bumps look. | Normal maps on. |
| **Stone sheen** | 0.00 (default) to 1.00, steps of 0.05 | A shine on the stone walls and floors. 0 is none. | |
| **Joint shadows (AO)** | Off, or 25% to 200% in steps of 25. Default 100% | Darkens the joints and hollows between the stones of walls and floors, where little light would reach ("ambient occlusion", AO). Higher is darker. | |
| **Relief shadows** | Off, or Depth 1 to 8. Default Depth 3 | The stones cast small shadows of their own across walls and floors, away from each light. A higher depth treats the stone's relief as deeper, so the shadows are stronger. | |
| **Sprite normal strength** | 0.00 to 1.00, steps of 0.05. Default 0.50 | How strongly the objects' and creatures' pictures are lit by their normal maps. 0 is flat. | |
| **Sprite joint shadows** | 0.00 to 1.00, steps of 0.05. Default 0.50 | How dark the objects' and creatures' occlusion maps make their creases. 0 is none. | |
| **Sprite relief depth** | 0 (default) to 4 texels, steps of 0.25 | How deep the objects' and creatures' height maps look. 0 is off. | |
| **Light colour** | CAT16 (default) / Bradford | Firelight is orange, but an eye that has got used to it sees it as nearly white. These are two standard formulas for working out that shift; CAT16 is the newer one. The difference is subtle. | |
| **Light fade** | 2 / 3 / 4 tiles (default) / Original | How light dims with distance. With 2 / 3 / 4 tiles a light is at half strength 2 tiles away, 30% at 3 and gone at 4 (a brighter light reaches further in the same proportion). Original uses the game's own shading by distance. | |
| **Your light** | Over the head (default) / At the eye | Where your torch or lantern is held. | |
| **Fire breathing** | Off, or 10% to 40% in steps of 10. Default 20% | Now and then a fire sags or flares for a few seconds, as real flames do. The value is how deep that swing goes. | |
| **Light adaptation** | Automatic (default), or 0% to 100% in steps of 10 | How far the eye gets used to firelight. 0% leaves the fire's raw orange on everything; 100% makes it white. Automatic works out the amount for a torch-lit dungeon and shows it in brackets. | |
| **Exposure** | 0.50 to 4.00, steps of 0.25. Default 2.00 | The picture's overall brightness before tone mapping. Takes hold at the next start. | |
| **Light gamma** | 0.50 to 2.00, steps of 0.05. Default 1.00 | The lights' falloff curve. Higher keeps rooms brighter away from the lights. | |
| **HDR output** | On (default) / Off | On an HDR monitor, with HDR switched on in Windows, fire and bright light go brighter than white, up to the monitor's peak. On a monitor that is not in HDR the row reads "On (this monitor: SDR)" and the picture is unchanged. | An HDR monitor. |
| **HDR peak brightness** | The monitor's (default), or 400 to 4000 nits in steps of 100 | The brightest the highlights go. "The monitor's" uses the figure the monitor reports. (A nit is the unit of screen brightness.) | HDR. |
| **HDR paper white** | 80 to 500 nits, steps of 10. Default 200 | How bright plain white is in HDR: the picture's normal range and the interface. | HDR. |

## Sprites

Items whose picture has been redrawn have a row here, named after the item (Hand axe, Battle axe ...). The tab is
only there when at least one redrawn picture is installed, and an item without one has no row.

| Row | Values | What it does |
| --- | --- | --- |
| **All redrawn pictures** | All original / All new / some of them new | Turns every redrawn picture on at once, or all of them off again. It is on if any picture is still off, otherwise it turns them all off. (Added 2026-11-20.) |
| **(the item's name)** | Original (default) / New | The item's picture where it lies in the world, in your pack, in the hand, shoulder and ring holes of the character panel, and when you hold it on the crosshair. Original is the game's own 16 x 16 picture. New is the redrawn one: the item stands upright, drawn at twice the detail in the same space. It changes at once. |

The redrawn pictures are PNG files in the folder `uwr\newsprites\objects`, named by the item's number (`000.png` is
the hand axe). A picture may be up to 32 x 32 pixels; one larger than 16 x 16 is shown at half size, so it takes the
same room as the original. The game looks for them when it starts. The original game's own inventory panel
(Character sheet: the game's page, or Display: The original's) always shows the original pictures.

## Effects

| Row | Values | What it does |
| --- | --- | --- |
| **Particles** | On / Off | Sparks, smoke and dust in the air. |
| **Decals** | On / Off | Marks left on floors and walls, such as blood. |
| **Water** | On / Off | Water that moves and bends the view of what lies under it. |
| **Fog and light shafts** | Off (default) / Low / Medium / High | Fog in the air, and shafts of light through it. (Its row did not show until 2026-11-18.) |
| **Campfire smoke** | On (default) / Off | Grey smoke rises from every campfire, spreads under the ceiling and thins out over a few seconds. Needs Particles. |
| **Water fog** | On (default) / Off | A thin pale fog lies over the water near you and drifts slowly. Needs Particles. |
| **Glowing stones** | On (default) / Off | Glowing rocks, orbs and orb rocks shine and light the floor about a foot round them. Off, they are plain objects, as in the original. |
| **Moonstone light** | Off, or 5% to 50% of a candle, steps of 5. Default 10% | The moonstone fills the room it lies in with a deep blue light, the same everywhere in the room, round corners too. It only shines while it lies on the floor, not in your pack. |
| **Fire elemental light** | Off, or 10% to 100% of a candle, steps of 10. Default 30% | Fire elementals cast a smouldering orange light that moves with them. |
| **Campfire swell** | 0.00 to 0.60, steps of 0.05. Default 0.35 | How much the campfires swell and shrink as they burn. 0 is steady. |
| **Flame swell** | 0.00 to 0.50, steps of 0.05. Default 0.15 | How much the other fires (torches, braziers, burning things) swell and shrink. 0 is steady. |

Particles, Decals and Water are off unless your settings file turns them on (the public download's does).

## Fog and smoke

Fog and smoke have a tab of their own, next to Effects (shown when the renderer has them). It works like the
other tabs: Up and Down move, the wheel and the scrollbar scroll, and the foot describes the row you are on.

Most of its rows are **sliders**: a number on the right, and a bar in the middle of the row with a knob that
shows where the number lies between its lowest and its highest value. There are three ways to set one:

| Do this | To |
| --- | --- |
| Left / Right (or a right click / a left click on the row) | Move the slider one step down or up. |
| Press the left mouse button on the bar and drag | Put the knob where you press; it follows the pointer until you let go. |
| Type a number, then Enter | Set the value exactly. Digits, `.` and `-` work, on the main keys and the number pad. What you type is shown in place of the value, with a `_` after it. Backspace takes the last character away; Esc drops what you typed and keeps the old value (it does not close the menu); Up or Down drop it and move on. A number outside the slider's range becomes the nearest value it allows. |

The other rows are switches (On / Off) and choices, which change as everywhere else in the menu. The tab's
**Preset** row offers Default, Light, Heavy, Faster and Finest, and **Default settings** puts every row of the tab
back to its default (see Using the menu); change any slider afterwards and Preset reads Custom.

Every change is kept in `settings.txt`, as the other rows' are.

### What the tab does

Fog and smoke are simulated as real clouds: the air round you is divided into small cells, and many times a second
the game works out how the air in each cell moves, what it carries, and how much light reaches it. Water gives off a
low haze, lava gives off steam, fires give off smoke that rises and gathers under the ceiling. You, the creatures,
doors, missiles and explosions push the air, so the fog parts and curls. With Ray tracing on, walls, pillars and
creatures cast shadows into it.

Limits to know about:

- It needs the Vulkan renderer (the default). With Direct3D 12 the tab's rows are kept but nothing is drawn.
- Only the air in a box round you is simulated (the **Reach** row). Beyond it the older particle fog and smoke stand in, or nothing (the **Beyond the reach** row).
- A shut door does not hold fog or smoke back; walls, floors and ceilings do.
- Fog and smoke do not change what creatures can see.

### The rows

**Fog and smoke**

| Row | Values | What it does |
|---|---|---|
| **Fog and smoke** | On / Off (default On) | On: fog and smoke are real clouds in the air. They drift, rise, sink, thin out, catch the light, and you push them aside. Vulkan renderer only. |
| **Each level's own fog** | On / Off (default On) | On: a level can have its own fog and smoke, made in the Underworld Editor (Level, Fog and smoke): more or less haze and smoke, a draught, its air's temperature, and fog and smoke placed on the map. Kept in DATA\UWRFOG.TXT. Off: every level by these rows alone. |

**Fog (haze over water)**

| Row | Values | What it does |
|---|---|---|
| **Fog** | On / Off (default On) | On: a haze lies over water and steam rises over lava. Off: no fog; smoke is not changed. |
| **Fog thickness** | Slider: 0.00 to 4.00 x, steps of 0.05 (default 1.00) | How thick the fog looks for the same amount of it in the air: 0 (unseen) to 4. Default 1. |
| **Fog given off** | Slider: 0.00 to 3.00 x, steps of 0.05 (default 1.00) | How much haze water and lava give off: 0 (none) to 3. Default 1. |
| **Haze over water** | On / Off (default On) | On: every water square gives off haze. Off: water gives none. |
| **Steam over lava** | On / Off (default On) | On: lava gives off hot steam that rises. Off: none. |
| **Water chills the air** | Slider: 0.0 to 10.0 K, steps of 0.5 (default 3.0) | How much colder than the room the air just over water is: 0 to 10 degrees. Cold air is heavy, so the fog lies low and creeps out across the floor. Default 3. |
| **Fog dries off in warm air** | Slider: 0.00 to 1.00 /s K, steps of 0.01 (default 0.20) | How fast fog vanishes for each degree the air is warmer than the room: real droplets are gone within a second in air a few degrees warmer. 0 (never) to 1. Default 0.2. |
| **Fog droplets settle** | Slider: 0.0 to 5.0 cm/s, steps of 0.1 (default 1.0) | How fast the fog's droplets fall through still air: 0 to 5 cm a second. Real fog droplets of 20 thousandths of a millimetre fall about 1. Default 1. |
| **Lava heats the air** | Slider: 0 to 400 K, steps of 10 (default 120) | How much hotter than the room the air just over lava is: 0 to 400 degrees. Hot air rises and takes the steam up with it. Default 120. |
| **Fog spreads** | Slider: 0.00 to 0.50, steps of 0.01 (default 0.05) | How fast fog seeps sideways into clear air by itself: 0 (holds its shape) to 0.5. Default 0.05. |
| **Fog thins out** | Slider: 0.00 to 2.00 /s, steps of 0.01 (default 0.10) | How fast fog fades away: the share lost each second, 0 (never) to 2. Default 0.10. |
| **Fog layer height** | Slider: 0.10 to 2.00 sq, steps of 0.05 (default 0.45) | How high above the water the haze forms: 0.1 to 2 map squares. Default 0.45. |
| **Fog colour: warm** | Slider: 0.00 to 1.00, steps of 0.01 (default 0.97) | The fog's colour, first of three sliders (the warm part of the light it sends back): 0 to 1. Default 0.97. |
| **Fog colour: green** | Slider: 0.00 to 1.00, steps of 0.01 (default 0.98) | The fog's colour, second of three sliders: 0 to 1. Default 0.98. |
| **Fog colour: blue** | Slider: 0.00 to 1.00, steps of 0.01 (default 1.00) | The fog's colour, third of three sliders: 0 to 1. Default 1.00. |
| **Fog brightness** | Slider: 0.00 to 4.00 x, steps of 0.05 (default 1.00) | How brightly fog shows in light: 0 (it only dims what is behind it) to 4. Default 1. |
| **Fog blocks light** | Slider: 0.0 to 8.0, steps of 0.1 (default 1.6) | How much a square's depth of full fog hides what is behind it: 0 (clear) to 8. Default 1.6. |
| **Fog droplet size** | Slider: 5 to 50 um, steps of 1 (default 15) | The size of the fog's water droplets: 5 to 50 thousandths of a millimetre. It sets how fog scatters light: a bright glow round lights seen through it. Default 15. |
| **Fog billows** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.70) | Fine cloud shapes in the fog, smaller than the simulation can hold: wisps at the edges and uneven clumps that drift with the air. 0 (smooth, even haze) to 1. Default 0.7. |
| **Mist rises in wisps** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.60) | 0: water gives off its haze evenly. Higher: it comes off in separate, shifting wisps that climb a little before they sink back, as mist does over a real pond. The same amount of haze overall. 0 to 1. Default 0.6. |
| **Cold air warms up** | Slider: 0.02 to 3.00 /s, steps of 0.01 (default 0.05) | How fast the cold air over water warms to the room's temperature: 0.02 (it stays cold and heavy, so the fog runs far across the floor and down steps) to 3 (it warms at once and the fog stays by the water). Default 0.05. |
| **Fog in veils** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.40) | The form of the fog's fine shapes: 0 (soft round clumps) to 1 (thin drifting veils and threads, as mist has). Needs the Fog billows row above 0. Default 0.4. |
| **Moving air raises more mist** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.50) | Air that moves across water takes up more haze than still air, as a light breeze does over a pond: your own passing and any draught thicken the mist. 0 (the same in still and moving air) to 1. Default 0.5. |
| **Fog dries from its edges** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.50) | 0: thin and thick fog fade at the same rate. Higher: the thin outer parts, mixed with drier air, vanish sooner and the thick middle lasts longer, so a bank of fog keeps a clearer shape. 0 to 1. Default 0.5. |
| **Fog edge softness** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.20) | 0: thin fog shows as it is. Higher: thin wisps at the edges are hidden, so the fog has a softer, later start. 0 to 1. Default 0.2. |

**Smoke**

| Row | Values | What it does |
|---|---|---|
| **Smoke** | On / Off (default On) | On: fires give off smoke that rises, gathers under the ceiling and thins out. Off: no smoke; fog is not changed. |
| **Smoke thickness** | Slider: 0.00 to 4.00 x, steps of 0.05 (default 1.00) | How thick the smoke looks for the same amount of it in the air: 0 (unseen) to 4. Default 1. |
| **Smoke given off** | Slider: 0.00 to 3.00 x, steps of 0.05 (default 1.00) | How much smoke the fires give off: 0 (none) to 3. Default 1. |
| **Campfires smoke** | On / Off (default On) | On: campfires give off smoke. Off: they give none. |
| **Torches and lanterns smoke** | On / Off (default On) | On: wall torches, lanterns and other small flames give off a thin smoke. Off: none. |
| **Blasts smoke** | On / Off (default On) | On: an explosion leaves a puff of hot smoke. Off: none. |
| **Campfire heat** | Slider: 50 to 800 K, steps of 10 (default 400) | How much hotter than the room the air over a campfire is: 50 to 800 degrees. Hotter: the smoke climbs faster and thinner. A real campfire (about 30 kW): 400 just over the flames, 140 a metre up. Default 400. |
| **Torch heat** | Slider: 20 to 500 K, steps of 10 (default 120) | How much hotter than the room the air over a torch or lantern flame is: 20 to 500 degrees. Default 120. |
| **Smoke's own weight** | Slider: -1.00 to 1.00, steps of 0.01 (default 0.00) | Real smoke weighs next to nothing and goes where the warm air goes: 0. Below 0 it sinks by itself, above 0 it floats up. -1 to 1. Default 0. |
| **Heat lifts the air** | Slider: 0.00 to 3.00 x, steps of 0.05 (default 1.00) | How strongly warm air rises and cold air sinks, against the real force (gravity x the difference in temperature): 0 to 3. Default 1 (real). |
| **Smoke spreads** | Slider: 0.00 to 0.50, steps of 0.01 (default 0.03) | How fast smoke seeps sideways into clear air by itself: 0 (holds its shape) to 0.5. Default 0.03. |
| **Smoke thins out** | Slider: 0.00 to 2.00 /s, steps of 0.01 (default 0.20) | How fast smoke fades away: the share lost each second, 0 (never) to 2. Default 0.20. |
| **Air loses its heat** | Slider: 0.02 to 3.00 /s, steps of 0.01 (default 0.25) | How fast warm air cools to the room's temperature (faster still beside rock): 0.02 (stays hot, climbs far) to 3 (cools at once). Default 0.25. |
| **Smoke colour: warm** | Slider: 0.00 to 1.00, steps of 0.01 (default 0.80) | The smoke's colour, first of three sliders (the warm part of the light it sends back): 0 to 1. Default 0.80. |
| **Smoke colour: green** | Slider: 0.00 to 1.00, steps of 0.01 (default 0.79) | The smoke's colour, second of three sliders: 0 to 1. Default 0.79. |
| **Smoke colour: blue** | Slider: 0.00 to 1.00, steps of 0.01 (default 0.77) | The smoke's colour, third of three sliders: 0 to 1. Default 0.77. |
| **Smoke brightness** | Slider: 0.00 to 4.00 x, steps of 0.05 (default 1.00) | How brightly smoke shows in light: 0 (black smoke that only hides) to 4. Default 1. |
| **Smoke blocks light** | Slider: 0.0 to 8.0, steps of 0.1 (default 3.0) | How much a square's depth of full smoke hides what is behind it: 0 (clear) to 8. Default 3. |
| **Smoke glow toward lights** | Slider: 0.00 to 0.90, steps of 0.05 (default 0.60) | How much brighter smoke is when you look through it toward a light: 0 (the same every way) to 0.9. Wood smoke is about 0.6. Default 0.6. |
| **Smoke billows** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.85) | Fine cloud shapes in the smoke, smaller than the simulation can hold: wisps at the edges and uneven clumps that drift with the air. 0 (smooth, even smoke) to 1. Default 0.85. |
| **Smoke comes in puffs** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.80) | 0: every fire gives off smoke evenly, as a smooth ball. Higher: it comes off in uneven puffs, as from a real fire. The same amount of smoke overall. 0 to 1. Default 0.8. |
| **Puff rate by fire size** | On / Off (default On) | On: a fire puffs at the rate real flames do, set by its width: a torch about three times a second, a campfire about twice. Off: every fire puffs slowly and unevenly. Needs the Smoke comes in puffs row above 0. |
| **Smoke in round puffs** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.40) | The form of the smoke's fine shapes: 0 (soft clumps) to 1 (round puffs with dark creases between them, as rolling smoke has). Needs the Smoke billows row above 0. Default 0.4. |
| **Puffs kick the air** | On / Off (default On) | On: a fire's heat comes in the same beats as its puffs, so each puff is thrown up as a rolling ring, as a real flame sheds them. Off: steady heat. Needs the Smoke comes in puffs row above 0. |
| **Smoke edge softness** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.10) | 0: thin smoke shows as it is. Higher: thin wisps at the edges are hidden. 0 to 1. Default 0.1. |

**The air**

| Row | Values | What it does |
|---|---|---|
| **Room temperature** | Slider: 0 to 30 C, steps of 1 (default 12) | The temperature of the dungeon's still air: 0 to 30 degrees Celsius. Warm air rises faster in a cold room. Default 12. |
| **Billow size** | Slider: 0.20 to 2.00 sq, steps of 0.05 (default 0.60) | How large the fine cloud shapes of the Fog billows and Smoke billows rows are, in map squares (a square is 2.4 m). Small: fine and wispy. Large: big soft clumps. 0.2 to 2. Default 0.6. |
| **Billows spare the thick middle** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.50) | The fine cloud shapes eat into the thin outer parts of fog and smoke and leave the thick middle whole, as in a real cloud. 0 (the whole cloud is broken up) to 1 (only its edges). Default 0.5. |
| **Wisps stretch with the air** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.60) | Where the air moves, the fine cloud shapes are drawn out into streaks along its way; still air keeps its round puffs. 0 (round everywhere, a little faster) to 1. Default 0.6. |
| **Billows where the air shears** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.50) | 0: the fine cloud shapes are equally strong everywhere. Higher: stronger where layers of air slide past each other (the rolls on top of a moving sheet of fog, the edge of a rising column) and weaker in still air. 0 to 1. Default 0.5. |
| **Sharp wisps** | On / Off (default On) | On: the moving air keeps fine wisps and edges (a second, correcting pass each step). Off: the simpler method, softer and blurred, a little faster. |
| **Air comes to rest** | Slider: 0.00 to 3.00 /s, steps of 0.05 (default 0.10) | How soon moving air slows down by itself: 0 (it keeps going, as real air nearly does) to 3 (it stops at once). Default 0.1. |
| **Curls kept** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.35) | A coarse grid of air loses its small curls and eddies; this puts them back (vorticity confinement): 0 (smooth, lazy smoke) to 1 (lively). Default 0.35. |
| **Draught** | Slider: 0.00 to 1.00, steps of 0.01 (default 0.00) | A steady draught through the whole level that carries fog and smoke along: 0 (still air) to 1 map square a second. Default 0. |
| **Draught direction** | Slider: 0 to 355 deg, steps of 5 (default 0) | Which way the draught blows: 0 north, 90 east, 180 south, 270 west. Default 0. |

**Pushing it**

| Row | Values | What it does |
|---|---|---|
| **You push it** | On / Off (default On) | On: your body moves the air, so fog and smoke part in front of you and curl in behind. Off: you pass through it. |
| **Push strength** | Slider: 0.00 to 3.00 x, steps of 0.05 (default 1.00) | How hard a moving body shoves the air aside: 0 to 3. Default 1. |
| **Wake** | Slider: 0.00 to 1.00, steps of 0.05 (default 0.60) | How much of the air a moving body drags along behind it: 0 (none) to 1 (all of it). Default 0.6. |
| **Creatures push it** | On / Off (default On) | On: creatures move the air as you do. Off: they pass through fog and smoke. |
| **Doors push it** | On / Off (default On) | On: a door that swings open or shut gives the air a shove. Off: none. |
| **Missiles push it** | On / Off (default On) | On: arrows, thrown things and spell bolts leave a trail through fog and smoke. Off: none. |
| **Blast force** | Slider: 0.0 to 5.0 x, steps of 0.1 (default 1.0) | How hard an explosion throws the air outward: 0 (not at all) to 5. Default 1. |

**Light in it**

| Row | Values | What it does |
|---|---|---|
| **Shadows in fog and smoke** | On / Off (default On) | On: walls, pillars and creatures cast shadows into the fog, so light falls through it in shafts. Off: lit evenly, faster. |
| **Light rays a cell** | Slider: 1 to 4, steps of 1 (default 1) | Shadow rays for each light at each cell of air: 1 to 4. More: smoother shafts from wide flames, slower. Default 1. |
| **Light shafts strength** | Slider: 0.00 to 4.00 x, steps of 0.05 (default 1.00) | How brightly torches, fires and other placed lights show in fog and smoke: 0 (not at all) to 4. Default 1. |
| **Your light in it** | Slider: 0.00 to 2.00 x, steps of 0.05 (default 1.00) | How brightly the light you carry shows in fog and smoke round you: 0 (not at all) to 2. 1 is real; too high hides the view. Default 1. |
| **Placed lights light it** | On / Off (default On) | On: torches, fires and glowing things light fog and smoke. Off: only the light you carry does. |
| **It shadows itself** | Slider: 0.00 to 2.00 x, steps of 0.05 (default 1.00) | How much thick fog and smoke keep light from the fog and smoke behind them: 0 (off, faster) to 2. Default 1. |
| **Light scattered many times** | Slider: 0.00 to 1.00, steps of 0.05 (default 1.00) | In thick fog light bounces from droplet to droplet and gets deeper in than a straight beam: 0 (straight beams only: hard, dark cores) to 1 (real). Default 1. |
| **Room light in it** | Slider: 0.00 to 2.00 x, steps of 0.05 (default 1.00) | How much of the light bounced off the walls also lights fog and smoke: 0 (only direct light) to 2. Needs Ray tracing and Bounced light. Default 1. |

**Quality and speed**

| Row | Values | What it does |
|---|---|---|
| **Fineness** | 3, 4, 6, 8 (default 6) | Cells of air to a map square: 3, 4, 6 or 8. More: finer wisps, more work for the graphics card. A change starts the air again. Default 6. |
| **Reach** | 12, 16, 20, 24, 32 (default 20) | The simulated air is a box round you this many map squares wide: 12 to 32. Beyond it there is no fog or smoke. Default 20. |
| **Simulation rate** | 30, 60, 120, Every picture (default 60) | How often the air is moved on: 30, 60 or 120 times a second, or once every picture. Default 60. |
| **Pressure passes** | Slider: 4 to 80, steps of 2 (default 20) | How exactly air flows round walls and bodies instead of into them: 4 to 80 passes. More: truer swirls, slower. Default 20. |
| **View steps** | Slider: 8 to 128, steps of 4 (default 48) | Samples taken along each line of sight through the fog: 8 to 128. More: smoother, slower. Default 48. |
| **Fog drawn at half size** | On / Off (default On) | On: the fog and smoke are worked out for a quarter of the screen's pixels and brought up to full size by distance, with full detail kept at the edges of walls and creatures: much faster, and DLSS smooths it. Off: every pixel worked out, slower. |
| **Light refresh** | All, Half, Quarter (default Half) | The share of the air's cells whose light is worked out again each picture: all, half or a quarter. Less is faster, the light follows later. |
| **Light smoothing** | Slider: 0.00 to 0.95, steps of 0.05 (default 0.80) | How much of the light from earlier pictures is kept in each cell: 0 (none: grainy, instant) to 0.95 (smooth, slow to follow). Default 0.8. |
| **Beyond the reach** | Particles, Nothing (default Particles) | What stands in for fog and smoke outside the simulated box: the older Particles (Water fog, Campfire smoke rows), or Nothing. |

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
| **Headphone 3D sound** | On (default), Off, Automatic | For headphones. Ordinary stereo can only say left or right; this filters each sound the way your head and ears would (an "HRTF"), so you can also tell ahead from behind, above from below. Automatic turns it on when the system reports headphones. Leave it Off for loudspeakers. The value also says whether it is **working now** (what the sound device made of it): "working now", "not in use now", or "this output cannot" (surround speakers, for one). The log says the same: `fx: HRTF on (Built-In HRTF)`. | Positional sound. |
| **Effects' sides** | Corrected (default) / As the MT-32 plays them | The Roland MT-32 swaps left and right, so the original's effects came out on the wrong side. Corrected puts each sound on its true side. | |
| **Hearing range** | Original (6 tiles), 9, 12, 16 or 24 tiles | How far away a sound can be heard. A sound is at full strength within one tile and fades evenly to nothing at the range. | Positional sound. |
| **Footstep width** | Middle, then 50% to 800% in steps of 50; 100% reads Original | How far to the left and right your own two feet sound. Original is the game's own, barely apart. Middle puts both in the centre. 400% is half-way to each side. | Positional sound. |
| **Footstep volume** | Off, or 10% to 200% in steps of 10. Default 80% | How loud footsteps are, yours and the creatures', next to the other sounds. | Positional sound. |
| **Your own footsteps** | 0% to 200% in steps of 10. Default 100% | How loud your own footsteps are, on top of Footstep volume, so you can turn yours down and leave the creatures' as they are. 0% silences your own steps. | Positional sound. |
| **Footsteps at your feet** | On (default) / Off | Your own footsteps come from below you, where your feet are (about 1.3 m under your ears, a little ahead), so with Headphone 3D sound they sound under you. Off: from just in front of you at ear height, as before 2026-11-19. | Positional sound. |
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
| **Music volume, clean** | On (default) / Off | On, the Roland plays the music at the original level and the Music volume turns the finished sound down, at full precision: each step is as loud as before. Off, the volume is turned down inside the Roland, as before 2026-11-18: the Roland then worked 38 dB down at Music volume 15, its reverb included (in a quiet passage 36 different sample values against 2,617 at full volume). Only while our positional sound plays the effects (otherwise they come from the same Roland and the old way stays). |
| **Roland voices** | 32 to 256, steps of 32. Default 256 | How many notes the Roland can sound at once for the music. The real MT-32 and CM-32L have 32 ("32 (original)"), and in a busy passage the unit cuts sounding notes short to start new ones (the main theme loses some 36 notes between 9.7 and 13 seconds). More voices: no note is cut. Heard from the next start. |
| **Music in the menu** | On (default) / Off | On, the music plays on while this menu is open, though the game is paused, and while a new level is being built (the game waits for its picture; in DOS the music played on through a level's load too). Off, the music stops with the game. |

### With Positional sound

| Row | Values | What it does |
| --- | --- | --- |
| **Heat shimmer** | 0% to 300%, steps of 10. Default 100% | Hot air is uneven, and sound travels faster through its warmer pockets, so a sound that crosses the rising air over a fire, lava or a blast is bent this way and that and flickers a little in loudness, the high notes most. The hot places are the ones the fog and smoke use (flames, lava, blasts, at the Fog and smoke page's temperatures), whether the fog and smoke are shown or not. A sound only ever dips (by up to about 4.5 dB in the high notes through a campfire's plume at 100%), never gets louder; your own sounds and a sound whose fire is its own source are left alone. 0% turns it off. Fog and smoke themselves are not added: damp air and smoke change nothing you could hear over a dungeon's distances. Shown while Acoustics is on. |
| **Loudness limiter** | On (default) / Off | OpenAL's own limiter on everything our positional sound plays. With it off, a moment where many loud sounds land together can add up past full scale, and on a sound card that takes float samples (as most do now) Windows then cuts the peaks flat, which you hear as a crackle. On, the whole is turned down for that moment instead; below full scale it does nothing at all. The sound starts again when you change it, which takes a moment. |
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
| **Material table** | Researched (default) / Earlier | What the level's walls, floors and doors are made of for the sound. Researched (since 2026-11-19, `acoustics.txt`): rough rock, dressed stone, brick, marble, plaster (the stucco walls), wood, straw, metal, water and dirt, each from published measurements (docs/SOUND_RESEARCH.md): a marble hall now rings longer than a rock cave. Earlier (`acoustics-2026-11-14.txt`): every stone surface rough rock, as until then. The sound starts again when you change it. An `acoustics.txt` you have edited yourself is never written over. |

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
| **Fast echo rays** | On (default) / Off | The sound rays behind the echoes are traced by Intel's Embree, which comes inside Steam Audio, on several of the processor's threads at once (a quarter of them, 2 to 8): about six times quicker than Steam Audio's own ray tracer on two threads, which Off goes back to. The echoes are the same either way; quicker, they follow you more closely and leave the processor freer. Steam Audio is started again when you change it, which takes a moment. |
| **Sounds with echoes** | 1 to 32 sounds. Default 16 | How many sounds at once have echoes of their own. Up to 64 sounds play at once, every one shaped by Steam Audio; the newest this many also echo. More echoes cost processor time; fewer leaves the processor freer. |
| **Muffling on its own thread** | On (default) / Off | Steam Audio's muffling and sound paths are worked out on a thread of their own, every 8 ms, so the sound is never late waiting for them (they took about a quarter of each block's time in the mixing). Off: worked out while the sound is mixed, as before. |
| **Steam Audio on speakers** | On (default) / Off | With speakers or Headphone 3D off: Steam Audio still shapes each sound, with its muffling and its paths round corners, panned between the two speakers. Off: speakers get OpenAL's sound and our own muffling, as before (the echoes stay Steam Audio's either way). |
| **Surround speakers** | On (default) / Off | With 4, 5.1 or 7.1 speakers (as Windows is set up): Steam Audio's sound and echoes are mixed for every speaker around you, so a sound behind you comes from behind. Off: they come from the front two only, as before. The game's other sounds use every speaker either way. Headphones are not changed: they keep the 3D sound made for two ears. The sound starts again when you change it. |
| **Head shape** | Steam Audio's own (default), or a measured head | The head your headphone sound is made for. Every head hears a little differently; when sounds behind or above you seem to come from in front, try another. Measured heads are `.sofa` files in `uwr\hrtf` (SADIE II's KU100 and KEMAR dummy heads to start with; any SOFA "SimpleFreeFieldHRIR" file works). The echoes start again when you change it. |
| **Echo direction detail** | Plain (default) / Fine | Plain: the echoes come from 4 broad directions (first-order ambisonics). Fine: 9 (second order), so each echo comes more clearly from its own wall; about twice the work. The echoes start again when you change it. |
| **Sound by Steam Audio** | On (default) / Off | With Headphone 3D sound (and, with Steam Audio on speakers, on speakers too, panned): each sound itself is made for your two ears by Steam Audio's binaural rendering, from the place where it happens. Off, OpenAL places it. |
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
| **Roughness by material** | On (default) / Off | Each material scatters its echoes as it would: rough rock, dirt and cloth a lot, dressed stone and brick less, marble, metal and water hardly at all (`acoustics.txt`'s `scatter`). Wall roughness is used for any material without its own. Off: Wall roughness for every surface, as before. |
| **Echo buffer** | 2 to 6 blocks. Default 4 | How many blocks of echoes (about 11 ms each) are kept ready ahead. Too few and the echoes crackle; more and they come a little later. |
| **Echoes straight to the ears** | On (default) / Off | The echoes, already made for two ears, go to the headphones as they are. Off, they are placed a second time by Headphone 3D sound, which makes them duller. |

## Controls

| Row | Values | What it does |
| --- | --- | --- |
| **Mouse sensitivity** | 0.1 to 4.0, steps of 0.1. Default 1.0 | How fast the mouse turns the view. Higher is faster. |
| **Invert mouse** | On / Off (default) | On, moving the mouse forward looks down, as a flight stick. |

The keys themselves are listed in [KEYBINDS.md](KEYBINDS.md), and can be changed on the Keys tab.

## Keys

Every keyboard command of our display, with the key it has now. Change any of them to the key you want.

| Row | Values | What it does |
| --- | --- | --- |
| **Default keys** | Enter | Every command goes back to the key it came with. |
| **A command's row** (Walk forward, Use, Map ...) | A key, a key with Shift or Ctrl, or none | Enter (or a click) asks for the new key: the next key you press becomes that command's key. Hold Shift or Ctrl while pressing it for a combination. Shift on its own is taken when you let it go. Esc gives up, Delete leaves the command with no key. Left (or a right click) puts the row's own default key back. |

- A key can belong to one command only. Giving a command a key that another one had takes it from that one, which
  then shows **none** until you give it a key.
- A key that has lost its command does nothing in play. It never falls back to an older meaning.
- Some commands come with two keys (Walk forward: W and Up arrow). Each has its own row, marked "second key".
- Esc, Tab, F1, F11 and the key under Esc belong to the menus and cannot be given to a command.
- The mouse buttons, the map's and the conversations' keys, and the rune keys (Ctrl + Alt + letter) are not on this
  tab and stay as they are.
- Your keys are kept in the settings file as `key_<name>` lines.
- The original's display uses the 1992 keys, so the tab is not shown there.

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
| **Rival creatures fight** | Off, Rare (default), Sometimes | Now and then two creatures of rival kinds that have not noticed you fight each other: the two goblin tribes (who keep to opposite ends of level 1, so theirs is the rarest), trolls and the mountain folk against goblins, and giant spiders hunting giant rats (the game's own kin groups, its race numbers). Only creatures without a conversation of their own, never your friends, never on level 7 or in the Ethereal Void. A fight begins only between two that see each other 3 to 6 squares apart; one at a time on a level; Rare tries at most once a minute and waits 5 minutes after a fight, Sometimes every 20 seconds and 2 minutes. It ends at the first death, when either notices you (you come within 3 squares, or you attack one), after two minutes, or when they are far apart. Their kills give you no experience. Off in the Original AI level. In the AI log: "feud: ...". (Added 2026-11-21.) | Devious or Monstrous. |
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

## Enhancements

Changes the original game does not have, and fixes for its faults, each Off until turned on. They are kept in
uw1port.cfg's `enhance=` line, the same line `--enhance NAME` writes, and act at once. The ones our display always
has on (mouse-look, the modern keys, the rune keys, combined moves, the wide look up and down) are set by the
Display row and have no row here; invert-look follows Controls > Invert mouse; smart hunting follows AI > Creature AI.
The tab is not shown with the original's display, which has every enhancement off.

| Row | What it does |
| --- | --- |
| **Skip the introduction** | The game starts at its main menu, without the title and the introduction (from the next start). |
| **Menus wrap around** | The start menu and the saved games wrap from the last item to the first. |
| **Fast panels** | The right-hand panel slides twice as fast. |
| **Free heading** | Sliding along a wall no longer turns your view. |
| **Cutscene subtitles** | Cutscenes show their text while the speech plays. |
| **Skill messages** | Says which skill a skill point or a trainer raised, and to what. |
| **Straight walls when looking up** | In the original's 3D picture, floors, ceilings and walls stay straight when the view pitches. |
| **Full-size sprites when looking up** | In the original's 3D picture, creatures and objects keep their size when the view pitches. |
| **Key ring** | A key that does not fit tries the other keys you carry, bags included, and uses the one that fits. |
| **Fix: lost object slots** | A stack too heavy to take part of no longer loses an object slot, and lost slots are freed. Changes play. |
| **Fix: restore at rest** | A restored game starts at rest, without the motion or fall it was saved in. Changes play. |
| **Fix: broken enchanted things** | An enchanted object that breaks loses its spell, so its debris casts nothing. Changes play. |
| **Fix: armour spells** | Resist Blows, Thick Skin and Iron Flesh add to your armour, as they were meant to. Changes play. |
| **Fix: enchantment names** | A melee weapon's enchantment is named for what it does (Damage or Accuracy); the original names them the other way round. |
