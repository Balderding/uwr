/* A level: read from data\levels\levelN.txt, turned into triangles for the renderer, and asked for
 * floor heights and walls by the movement code.
 *
 * World space: x east, z north, y up; one tile = 1.0. Floor heights are 0-15 steps of STEP and the
 * ceiling is at CEIL_STEPS steps (unconfirmed from UW.EXE; see NOTES.md). */
#pragma once
#include "base.h"

#define STEP 0.25f
#define CEIL_STEPS 16

enum { T_SOLID, T_OPEN, T_DIAG_SE, T_DIAG_SW, T_DIAG_NE, T_DIAG_NW, T_SLOPE_N, T_SLOPE_S, T_SLOPE_E, T_SLOPE_W };

typedef struct {
    u8 type, height, floor, wall, door, nomagic, unknown8;
    u16 first_object;
} Tile;

typedef struct {
    u16 item;
    u8 flags, z, heading, y, x, quality, owner;
    u16 next, link;
    u8 mobile[19];
} Object;

typedef struct {
    int number;
    u16 walls[48], floors[10];
    u8 doors[6];
    Tile tile[64 * 64]; /* [y * 64 + x], y 0 = south */
    Object obj[1024];
} Level;

/* One vertex, as the renderer takes it (laid out for the DLSS-ready pipeline: prev = last frame). */
typedef struct {
    float pos[3];
    float prev[3];
    float uv[2];      /* texels; repeats inside rect */
    float rect[4];    /* the texture's place in the atlas: x, y, w, h in texels */
    float nrm[3];
    float color[4];
} Vertex;

#define ATLAS_SIZE 1024   /* wide; the textures fill the top 1024 rows */
#define ATLAS_H 8192      /* below: object pictures (32 x 32 cells from row 1024), doors (row 1536), creatures (CRIT_ROW: both
                            pages of each creature this level has) */
#define CRIT_ROW 1664     /* the creatures' frames: 80 x 80 cells, 12 a row, for the kinds this level has */
#define CRIT_CELL 80
#define NATLAS_SIZE 4096  /* the normal maps: the same places as the atlas, 4x */
typedef struct {
    Vertex *vert;
    int vertices;
    u32 *index;       /* triangles: three vertex numbers each */
    int indices;
    int faces, relief_faces; /* the level's faces, and how many have their texture's relief as geometry */
    u8 *atlas;        /* ATLAS_SIZE x ATLAS_H RGBA8 */
    u8 *natlas;       /* NATLAS_SIZE^2 RGBA8: normal (RGB) + height (A) */
    u8 *snatlas;      /* ATLAS_SIZE x ATLAS_H RGBA8: the pictures' normal maps (objects, creatures, doors), at the
                       * same places as their colours (the user's Laigter maps; flat where there is none); alpha:
                       * their height maps (NAME_p.png; 1 where there is none) */
    u8 *saoatlas;     /* ATLAS_SIZE x ATLAS_H R8: the pictures' occlusion maps (NAME_o.png), the same places (1: none) */
    u8 grid[64 * 64 * 4]; /* per tile, for the shadow rays: type, floor height (steps), the floor's terrain (0, 1 water, 2 lava), 0 */
    int normal_maps;  /* how many were read */
    u8 animo_w[64], animo_h[64];  /* ANIMO.GR's frames' sizes (0 = none), at cell ANIMO_CELL + frame */
    u8 sprite_w[512], sprite_h[512]; /* the object pictures' sizes (0 = none); picture k is at (k % 32 * 32, 1024 + k / 32 * 32) */
    u8 sprite_div[512];              /* 2: picture k is one of ours at twice the detail (data\newsprites\objects, over 16 a
                                      * side): drawn at half a pixel's size, so it stands in the original's place */
    u8 door_w[16], door_h[16];       /* the door pictures (DOORS.GR): picture k at (k * 32, DOOR_ROW) */
    u8 door_top[16], door_l[16], door_r[16]; /* the door pictures' opaque part: first row, first and past-last column */
    /* the creatures this level has (object 0x40 + k): their page-0 frames in the atlas from cell `cell`, the
     * slots (animation -> segment) and segments (frame numbers) from data\sprites\creatures\NN\anim.txt */
    struct {
        bool loaded;
        int cell, nframes, nsegs;
        u8 w[256], h[256], hx[256], hy[256]; /* page 0's frames, then page 1's from base1 */
        u16 fx[256], fy[256];                /* where each frame is in the atlas (packed in rows from CRIT_ROW) */
        u8 slots[32], segs[16][8];           /* page 0: the sequences under 0x20 (combat, attacks, dying, casting) */
        int base1, nsegs1;
        u8 slots1[128], segs1[64][8];        /* page 1: slot 0x20 + ((sequence - 0x20) << 3) + direction, from 0x20 */
    } crit[64];
} Mesh;
#define DOOR_ROW 1536
#define ANIMO_CELL 576  /* the animated objects' pictures (ANIMO.GR's frames, data\ui\animo): 32 x 32 cells from row 1600 */
#define MAX_SPRITES 464
#define DOT_CELL 463    /* the particles' soft dot (uwview.c): the free cell after the last object picture (460); never the atlas's last column (x 992+), which does not show */
#define TMFLAT_CELL 464 /* the switches' pictures (TMFLAT.GR 0-15 = objects 0x170-0x17F) from here */
#define TMOBJ_CELL 480  /* the wall decorations' pictures (TMOBJ.GR 0-31) in the sprite cells from here */

/* false (with a message) when the file is missing or broken */
bool level_load(Level *lv, const char *data_dir, int number);
/* the game's own copies: levels as the player left them (a folder of levelN.txt, as the original keeps them in its
 * save); level_load reads one from there first. "" = none (the checks). */
void level_set_dir(const char *dir);
const char *level_dir(void);
/* the level as it is now, into dir\levelN.txt (the same text level_load reads) */
bool level_save(const Level *lv, const char *dir);
/* triangles + atlases (textures from data\textures\walls and floors, normal maps from walls_normal and
 * floors_normal) into the arena; each light gets a small glowing orb (colour alpha 2 = emissive) */
/* the textures' relief meshes as geometry (true, uw.exe) or the original's flat faces (the port's renderer) */
extern bool level_relief;
/* 1: object k's picture is ours when its file is there (data\newsprites\objects\NNN.png, up to 32 x 32); the Esc
 * menu's Sprites tab (uwview.c). Presentation only. */
extern u8 level_new_sprite[512];
bool level_mesh(const Level *lv, const char *data_dir, const float (*orbs)[6], int norbs, Mesh *m);
/* texture i's place in the atlas (0-209 the walls, 210-261 the floors): x, y, w, h */
void level_texture_rect(int i, float r[4]);

/* floor height (world y) at a point, and whether the point is inside rock */
float level_floor_y(const Level *lv, float x, float z);
bool level_solid_at(const Level *lv, float x, float z);
float level_ceil_y(void);
/* the tiles and objects from the original's level block (0x7c08 bytes, as LEV.ARK and the rules core's mapdata hold it) */
void level_from_block(Level *lv, const u8 *block);
/* one object of it */
void level_obj_from_block(const u8 *block, int i, Object *o);
