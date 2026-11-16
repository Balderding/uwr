/* Light sources: the only light in the world. The player's torch (light 0, render_set_torch) is what lets
 * them explore; the rest are landmarks:
 *
 *   data\lights.txt           lights placed by hand (written with the first one, floating in the starting
 *                             hallway of level 1, when missing)
 *   (data\lights\levelN.txt, a file a level of lights placed automatically, was planned and is NOT read or
 *   written: lights_load reads data\lights.txt alone)
 *
 * Each line:  light LEVEL X Z HEIGHT  R G B  INTENSITY RANGE      (a floating light: an orb)
 *          walltorch LEVEL X Z HEIGHT  R G B  INTENSITY RANGE  FX FZ   (a torch on a wall, facing FX FZ)
 *   X, Z: tiles (x east, z north, as in the level files); HEIGHT: in floor-height steps (0-16, the ceiling
 *   is 16); R G B: 0-1; INTENSITY: brightness at one tile away; RANGE: tiles it reaches. */
#pragma once
#include "base.h"
#include "level.h"

#define MAX_LIGHTS 1024
#define TILE_LIGHTS 8  /* the most lights shading one tile */
#define NO_LIGHT 0xFFFFFFFFu

typedef struct {
    float pos[3];   /* world: x east, y up, z north */
    float color[3];
    float intensity, range;
    int kind;       /* LIGHT_ORB (a floating light) or LIGHT_WALL_TORCH (a burning torch on a wall: flickers) */
    float facing[2]; /* a wall torch: the way it faces (x, z), out of its wall */
    float flame;     /* the flame's width in metres (its flicker: a flame puffs at about 1.5 / sqrt(width) a second,
                        deeper the bigger it is); 0: a torch's, 0.05 */
    float kelvin;    /* a flame's temperature (its colour: render_blackbody, shifting with the flicker); 0: color[] */
} Light;
enum { LIGHT_ORB, LIGHT_WALL_TORCH };

/* this level's lights (hand-placed + automatic, at most cap); -1 when the files cannot be read or written */
int lights_load(const char *data_dir, const Level *lv, Light *out, int cap);
/* for every tile, the (up to TILE_LIGHTS) lights that reach it, the ones it can see first: out[(z*64+x)*8+k],
 * NO_LIGHT where the list ends */
void lights_tiles(const Level *lv, const Light *l, int n, u32 *out);
