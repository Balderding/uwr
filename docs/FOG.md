# Fog and smoke

The fog and smoke in uwr are simulated air, not painted layers or particles. Cold, damp air lies over water
and creeps across the floor. Smoke climbs from campfires and torches and gathers under the ceiling. You, the
creatures, doors and blasts push it all aside, and every light in the level lights it through ray-traced shadows.

It needs the **Vulkan** renderer (the default). Its settings are on the Esc menu's **Fog and smoke** tab:
79 rows in 7 groups, with presets (Default, Light, Heavy, Faster, Finest). Every row is described in
[ESC_MENU.md](ESC_MENU.md). This page explains how it works.

The code: [src/volfog.inc](../src/volfog.inc) (the settings and what the game feeds the air each frame),
[src/render_vk_vol.h](../src/render_vk_vol.h) (the Vulkan side) and
[src/shaders/volume.hlsl](../src/shaders/volume.hlsl) (the simulation and the drawing, compute shaders).

## The air: a box of cells around you

The air is a 3D grid that follows you: by default 20 map squares wide (**Reach**), with 6 cells to a square
(**Fineness**; a map square is 2.4 m, so a cell is 40 cm). Each cell holds the air's velocity, its temperature,
and how much fog and smoke it carries. A cell keeps its place in the world as the box moves, so nothing jumps
when you walk; a cell that has just come into the box starts empty. Walls, floors and ceilings come from the
level's own map, so the air flows round pillars, up stairwells and through doorways, and never through rock.

60 times a second (**Simulation rate**) the air is moved on, one compute pass at a time:

1. **Curl.** How much the air turns in each cell, saved for step 3.
2. **Carry.** The air carries itself, its warmth, the fog and the smoke along (the semi-Lagrangian step of
   Stam's *Stable Fluids*).
3. **Correct and push.** The MacCormack correction (Selle et al. 2008) gives back what that step blurred,
   so wisps stay sharp. Then everything that acts on the air:
   - **Buoyancy** from temperature (Boussinesq: warm air rises and cold air sinks, with the real force of
     gravity times the temperature difference).
   - **Vorticity confinement** (Fedkiw, Stam and Jensen 2001), which puts back the small curls a coarse grid
     loses (**Curls kept**).
   - **The sources:** water's chill and haze, lava's heat and steam, the fires' smoke and heat.
   - **Bodies:** you, creatures, doors and missiles.
   - **The fog's droplets** settling (about 1 cm a second, as real fog does) and drying off in warmer air.
   - **Heat** lost to the air and to the rock, and the level's **draught**.
4. **Pressure.** How much air each cell gains or loses, then a pressure solve (20 Jacobi passes by default,
   **Pressure passes**). This makes the air flow round things instead of piling into them. Walls and floors
   are closed and the sides of the box are open.

### Where fog and smoke come from

- **Water** chills the air just above it (3 degrees by default). Cold air is heavy, so the haze lies low and
  runs out across the floor and down steps. It comes off in shifting wisps (**Mist rises in wisps**). Moving
  air takes up more of it (**Moving air raises more mist**). It dries away first from its thin edges
  (**Fog dries from its edges**).
- **Lava** heats the air above it to 120 degrees above the room, and the steam goes up with it.
- **Campfires and torches** give off hot smoke. It comes in puffs at a rate set by the fire's size, as real
  flames flicker (**Smoke comes in puffs**, **Puff rate by fire size**). Each puff also kicks the air
  (**Puffs kick the air**). The smoke rises, cools, and spreads out under the ceiling.
- **Explosions** throw a burst of hot smoke and push the air outward (**Blast force**).

### What pushes it

- **You:** fog parts in front of you and curls in behind you.
- **Creatures:** the same, by their size.
- **Doors:** a door that swings gives the air a shove along its swing.
- **Missiles:** they leave a wake.

Each has its own row, so it can be turned off.

## The light in it

Each cell works out its own light (**Shadows in fog and smoke**):

- Every light that reaches the cell is followed by a ray-traced shadow ray, past walls, pillars and creatures.
  The ray is also dimmed by the fog and smoke on the way. That is what makes shafts of torchlight.
- Light bounced off the walls (the ray tracer's light probes) lights the fog too (**Room light in it**).
- Thick fog also scatters light from droplet to droplet. This is approximated with Wrenninge's octaves
  (**Light scattered many times**), so dense fog glows softly instead of having dark cores.

Light is updated over several frames and kept between them, so it costs little.

Fog and smoke scatter light differently:

- **Fog** scatters by droplet size (the Mie fit of Jendersie and d'Eon 2023; **Fog droplet size**). That
  gives the bright glow round a lamp seen through mist.
- **Smoke** uses the Henyey-Greenstein model.

## Drawing it

For each pixel, the view is stepped through the box up to the nearest wall or object (48 steps,
**View steps**), adding up the light each cell sends towards the eye. The start of each step is jittered every
frame, and DLSS (or DLAA) blends the frames, which turns the jitter into a smooth picture.

Fine shapes smaller than a cell (veils, threads, round puffs) come from noise that drifts with the air:

- **Fog billows** and **Fog in veils** shape the fog; **Smoke in round puffs** shapes the smoke.
- The noise is stretched along the wind (**Wisps stretch with the air**).
- It shows most where the air shears (**Billows where the air shears**).
- It leaves the thick middle of a cloud solid (**Billows spare the thick middle**).

By default the view is stepped for one pixel in four and then scaled up (**Fog drawn at half size**). Pixels at
the edge of a wall or creature do their own full step, so edges stay sharp.

Outside the box, the older particle fog and smoke stand in (**Beyond the reach**).

## Each level's own fog: `DATA\UWRFOG.TXT`

A level can have its own settings in a text file in the game's `DATA` folder (Esc row
**Each level's own fog**, on by default). A file that is missing or empty changes nothing.

```
level 1
fog 1.5                          # this level's haze, times the menu's amount
smoke 0.8                        # this level's smoke from fires, times the menu's amount
draught 0.3 90                   # map squares a second, direction (0 north, 90 east); used when the menu's Draught is 0
temperature 8                    # the level's air in degrees C (instead of the menu's Room temperature)
fogspot 6.5 40.5 0.2 2 0.9       # fog placed on the map: x y height amount radius (map squares)
smokespot 5.5 44.5 0.3 1.5 0.5 60  # smoke placed on the map: x y height amount radius, and its heat
```

`level` starts a level's block (1 is the first level), and up to 48 spots go in each level. We write these
files with an extension to the Underworld Editor that we keep in our own copy; it isn't public yet. The file
can also be written by hand.

## Speed

On an RTX 4090 the frame takes about 14 ms with the fog on or off, so the fog isn't what
limits the frame there. On slower cards, the **Faster** preset uses fewer cells, a smaller box, fewer pressure
passes and view steps, and slower light updates. **Finest** goes the other way.

## Where it is going

- Simulating only the cells that hold fog or smoke, so the box can reach further at the same cost.
- Feeding the particle smoke into the grid.
- A test level built with the editor, with a room for each effect, so every change can be checked in the
  same places.
