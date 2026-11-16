/* The original's 32 3D models (UW.EXE's bytecode: the door frame, the door leaves, the shrine's ankh, benches,
 * boulders, pillars, tables, chests, barrels, chairs, the bridge, the moongate...), decoded and placed as our
 * renderer's triangles. The bytecode is the render interpreter's language (underworld-exhumed INTERP.ASM, TMAPOPS.ASM);
 * tools\models.py walks the same programs for a look. Every model goes through this one path: what DRAWOBJ.C's
 * do_rect and do_door set up for a model (its variables Clk(n), the heading it is turned by, its texture, the
 * ceiling delta point) the caller puts in a ModelInst, and models_place runs the program. */
#pragma once
#include "level.h"

#define MODELS_N 32
#define MODEL_VARS 16

typedef struct {
    int model;        /* 0..31 (tools\models.py's numbering: 1 the door frame, 11 the shrine, 14 a door leaf...) */
    float pos[3];     /* the object's origin in the world: x east, y up (tiles), z north */
    float turn;       /* the whole model turned about y by this many turns (do_rect's OP_IHCALL head / 65536) */
    int vars[MODEL_VARS]; /* the model variables Clk(0..15) as do_rect / do_door set them (palette indices,
                             the door's open steps in 3, an angle in 5 (1/65536 turn), the hilite in 8) */
    float delta_y;    /* the 0x4C delta point (point 256), a vertical offset in tiles: a frame's or pillar's way to
                         the ceiling, a portcullis's bars' height; 0 when the model has none */
    float rect[4];    /* the texture of its textured faces: the atlas rect (x, y, w, h texels); w 0: none */
    float var0_rgb[3];/* flag 0x20 models (the door frame): colour variable 0 is the texture's own colour (its first
                         pixel), given as RGB 0..1 here; var0_rgb[0] < 0: use vars[0] as a palette index */
    float kind_tex, kind_flat; /* the vertex colour's alpha for textured and flat faces (world.c's kinds: 1 lit as the
                                  walls, 5 a door's: seen from either side, see-through where the picture is) */
    float tex_vscale; /* the texture's height slot as a share of the full height at the start (0: 1, the full texture):
                         the door frame's (cTmDm^2 >> 8) x h - 1 of cTmDm^2 - 1, h / 0x100 = the lintel's height in tiles */
} ModelInst;

/* game\UW.EXE (the model table) and DATA\PALS.DAT (palette 0) read; false (with a log line) when either is missing */
bool models_load(const char *game_dir);
bool models_ready(void);
/* the model's triangles into out (Vertex, 3 a triangle); returns the vertex count (0 when none fit) */
int models_place(const ModelInst *mi, Vertex *out, int cap);
/* the palette, RGB 0..1 */
void models_palette(int index, float rgb[3]);

/* DRAWOBJ.C's tables, as the original has them */
int models_for_item(int item);              /* rect_sub: the model of a 3D object (items 0x150-0x16F), -1 none */
const unsigned char *models_cols(int model); /* rect_cols[model]: flags, then up to three colours; byte 3 the first
                                                texture (bits 0-4) and the texture count less one (bits 5-7) */
#define MCOL_NCOLS(f) ((f) & 7)
#define MCOL_DELTA    0x08  /* an extra 0x4C delta point: i = 0x400 - objyloc (to the ceiling) */
#define MCOL_TEXTURED 0x10
#define MCOL_TEXCOL   0x20  /* colour 0 is the caller's texture's colour */
#define MCOL_HEADING  0x40  /* turned by the object's own heading */
#define MCOL_ANIMATED 0x80  /* colours animated by the clock */
