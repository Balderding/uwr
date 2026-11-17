/* The things in the world drawn as pictures standing upright and turning to face the player, as the original
 * draws them: the level's objects (OBJECTS.GR pictures) and the burning torches on the walls. */
#pragma once
#include "level.h"
#include "lights.h"

#define SPRITE_SCALE (1.0f / 48.0f) /* tiles a picture pixel */
#define MAX_SPRITE_VERTS (6 * 4096)

/* is an object drawn as a picture (an item lying about, not a door, trap, trigger, creature or wall decoration) */
bool world_item_shown(const Object *o);
/* the cards (6 vertices each) of the level's items and the wall torches; returns the vertex count */
int world_sprites(const Level *lv, const Mesh *m, const Light *l, int nl, Vertex *out, int cap);
/* where an object stands in the world (x, y of its bottom, z) */
void world_object_pos(const Level *lv, int tile_x, int tile_z, const Object *o, float pos[3]);
/* wall decorations (writing, gravestones): their picture's sprite cell (TMOBJ.GR), or -1; and where they hang:
 * the middle c and the way they face */
#define DECAL_SCALE (1.0f / 64.0f) /* tiles a picture pixel: as the walls' textures (64 a tile) */
int world_decal_cell(const Object *o);
void world_decal_place(const Level *lv, int tile_x, int tile_z, const Object *o, float c[3], float nrm[3]);

/* a door: across the middle of its tile, half a tile wide (the pictures are 32 x 64: a tile high), framed by the
 * tile's wall texture; open 0 shut .. 1 swung 90 degrees on its left hinge. heading 0/4: the passage runs
 * north-south (the door spans x); 2/6: east-west (it spans z). */
typedef struct {
    int tx, tz, heading, item, obj;
    float open;
} Door;
#define DOOR_WIDTH 0.5f
/* the door's quads (colour alpha 5: world quads) into out; returns the vertex count */
int world_door_quads(const Level *lv, const Mesh *m, const Door *d, Vertex *out, int cap);
/* the same door through the original's models (models.h: the frame model 1 in the tile's wall texture, the leaf
 * model 14 swung on its hinge by the door's open steps, 15 the secret door, 12 the portcullis), as DRAWOBJ.C's
 * do_door draws it; o the door's object (its flags: the open steps, the way it opens) */
int world_door_model_quads(const Level *lv, const Mesh *m, const Door *d, const Object *o, Vertex *out, int cap);
/* the level's 3D objects (items 0x150-0x16F: benches, boulders, the shrine's ankh, tables, chests, barrels, chairs,
 * pillars, the bridge, levers...) as the original's models, placed as do_rect places them; returns the vertex count */
int world_model_quads(const Level *lv, const Mesh *m, Vertex *out, int cap);
/* the same objects for the sound (2026-11-16): each one's item and the box round its model as it stands in the world
   (tiles: lo, hi); arrows, wall plates and panels left out. The count (cap at most). */
int world_model_boxes(const Level *lv, int *item, float (*lo)[3], float (*hi)[3], int cap);
/* a creature's card: frame fr of kind k (0-63) standing with its hotspot (its feet) at pos; s tiles a pixel */
#define CREATURE_SCALE (1.0f / 60.0f)
int world_creature_card(const Mesh *m, int k, int fr, const float pos[3], Vertex *out);
/* the same with where it stood last frame (prev: the motion vectors DLSS follows a moving creature by) */
int world_creature_card_mv(const Mesh *m, int k, int fr, const float pos[3], const float prev[3], Vertex *out);
/* an object's picture card standing at pos (a missile in flight): 6 vertices, 0 when it has no picture */
int world_item_card(const Mesh *m, int item, const float pos[3], Vertex *out);
/* an animated object's frame (ANIMO.GR) standing at pos: 6 vertices, 0 when there is no picture */
int world_animo_card(const Mesh *m, int frame, const float pos[3], Vertex *out, bool glow);
/* the door's floor and top (world y) */
void world_door_span(const Level *lv, const Door *d, float *y0, float *y1);
