# The original's 3D models in our renderer (2026-11-13)

UW.EXE holds 32 3D models as bytecode of its render interpreter (underworld-exhumed INTERP.ASM, TMAPOPS.ASM):
1 the door frame, 2 the bridge, 3 the bench, 5-7 boulders, 8 the arrow, 9 the beam, 10 the pillar, 11 the shrine's
ankh, 12 the portcullis, 13 a painting, 14 the door leaf, 15 the secret door, 16-18 levers and switches, 19 the
gravestone, 22-23 moongate parts, 24 the table, 25 the chest, 26 the nightstand, 27 the barrel, 28 the chair.
Before this, our display drew only the door (a hand-made slab) and nothing at all for the other model objects (the
shrine was a bare glow: "the ankh is a sphere").

## Files
- `src\models.c/.h`: `models_load(game_dir)` reads the table out of `game\UW.EXE` (the four known table offsets)
  and palette 0 of `DATA\PALS.DAT`; `models_place(ModelInst, out, cap)` runs one model's program into our
  `Vertex`s. The ModelInst carries what DRAWOBJ.C's do_rect / do_door set up: the origin (the object's fine position),
  the turn (do_rect's OP_IHCALL head: 1/8 turn per heading for 0x40-flag models, 1/8 per door heading), the variables
  Clk(0..15) (palette indices from rect_cols; a door's open steps in 3; a leaf's swing angle or a portcullis's steps
  in 5), the 0x4C delta point (the way to the ceiling, or a portcullis's bar height), the texture's atlas rect, and for
  flag-0x20 models the texture's own colour as RGB.
- `src\world.c`: `world_door_model_quads` (frame + leaf, replacing `world_door_quads`; `UWVIEW_OLD_DOORS=1` keeps the
  old slab) and `world_model_quads` (items 0x150-0x16F through DRAWOBJ.C's rect_sub; gravestones and writings stay
  our decals). `src\uwview.c` calls both beside the sprites each frame; the ankh's glow is a glint plus a soft light
  above the loop now.
- `tests\models_test.c` (build.cmd builds and runs it): every model decodes to its end, no unknown opcode, a sane box.
- `tools\models.py`: the Python walker (a look at the programs). Its "colour" values are variable ADDRESSES
  (0x2920 + 2n = Clk(n)), not palette indices: BC's operands are (variable, +n), the colour is [Clk] + n.

## What the bytecode means (the parts that mattered)
- A coordinate triple on the wire is (x, z, y), y up, 8.8 fixed, 1.0 = one tile (the door frame is 1.0 wide).
- 7A / 82 define points (transformed as defined); 86 / 88 / 8A offsets along x / y / z; 90 (dx, dy), 92 (dx, dz),
  94 (dz then dy: INTERP.ASM's note); 8C a + b (b = point 256 is the delta point); 4A moves the origin; 50 / BA run a
  sub-program turned about y by an angle (1/65536 of a turn; BA takes it from a variable: the door leaf's hinge);
  12 a sub-program; 06 / 0C / 0E / 10 BSP splits (both halves drawn); 58, 5E-68 back-face tests (their normal is the
  face's normal); 14 / 16 conditional skips on a variable (<= / >=); 48 a jump; 78 a view test (ignored).
- Faces: 7E flat (the colour set last), A0 a textured quad with implied corners, A8 / B4 / CE textured with uv;
  BC colour = [var] + n; D4 per-point shades + the Gouraud base variable, D6 the Gouraud surface.
- Faces are concave polygons (the ankh's loop is one path round the outside and back along the hole): ear clipping,
  never a fan.

## Checks made
- `work\models\shots\`: the start door (lock plate on the left as the original's, where the old slab had it mirrored),
  the shrine face-on and edge-on, the pillar, barrels, a bench, each against the original renderer's own frame
  (`uw-original.exe --screenshot`, upscaled with `work\models\crop.py`).
- Not verified yet: the leaf's swing direction against the original (needs a door opened in both), the bridge's floor
  texture, model 23's animated colours (drawn with their base colours), the painting (13).
