/* collide.h: UW.EXE's terrain and object collision (segment 0x2529; underworld-exhumed motion/COLLIDE.C), ported
 * routine by routine. Everything works on a collision record (MotionCalc) as the original's curP: the caller sets x,
 * y (1/8 tiles), z (object units), the heading, radius and height; the checks fill the rest. The map is the original's
 * raw tile words (LEV.ARK's level block: word 0 type 0-3, height 4-7, light 8-9, floor 10-13, no-magic 14, door 15;
 * word 1 the wall texture 0-5 and the first object from bit 6), [x + y * 64]. */
#pragma once
#include "base.h"

typedef struct {
    i16 x, y, z;      /* 0x00, in 1/8 tiles (z: object units) */
    u16 heading;      /* 0x06: bits 13-15 the eighth of a turn */
    u8 radius, height;/* 0x08 */
    i16 index;        /* 0x0a: the mover's object */
    i16 hits0, hits1; /* 0x0c: the centre's state bits, the footprint's */
    u8 floor, top;    /* 0x10: the floor under the centre, the highest under the footprint */
    u8 slope, open;   /* 0x12: the blocking side's and the open side's direction (0-7, 9 none) */
    u8 found, count;  /* 0x14: collisions found, those in the way */
    i8 first;         /* 0x16: the first of them */
} MotionCalc;

/* the world the checks read: the level's block as LEV.ARK has it and the game keeps it (0x7c08 bytes: the 64 x 64
 * tiles of two words, the 256 mobile objects of 27 bytes at 0x4000 (critdata), the 768 static ones of 8 bytes at
 * 0x5b00 (objdata)), the floors' terrain words (TxmTerr: TERRAIN.DAT's word at 0x200 + the level's floor texture x 2,
 * 10 of them) and COMOBJ.DAT's 11-byte records (ComObjData, from its byte 2) */
typedef struct {
    u8 *block;
    u16 terr[10];
    const u8 *com;
} CollWorld;

/* a thing in the mover's way (struct Collision, 6 bytes) */
typedef struct {
    u8 top, bottom;
    u16 link;   /* bits 6-15 the object, 0-5 flags: 9, + 0x10 the mover's centre inside it */
    i16 offset; /* its tile from the centre tile, in the map (x + y x 64) */
} Collision;
extern Collision coll_objects[9]; /* oCollisions (DS 0x26c6; the original's 8, a ninth spilling into what follows) */
extern i16 coll_place_height, coll_place_terrain; /* can_place's nvokHgt, nvokTerr */

void coll_world(const CollWorld *w);
u8 coll_get_hgt(int n, i8 *steep);         /* GetHgt (0x28494) */
int coll_get_slope_hgt(int x, int y);       /* GetSlopeHgt (0x28635) */
void coll_terrain_check(MotionCalc *c, u8 range); /* TerrainCheck (0x28809) (with SolvePnt 0x2869d, SolveCenter 0x28754) */
void coll_compute_heading(MotionCalc *c);   /* ComputeHeading (0x28c86), after a TerrainCheck */
void coll_object_check(MotionCalc *c, u8 flat, u8 useflag); /* ObjectCheck (0x29086) with obj_coll_check (0x28ef0) */
void coll_process_objlist(MotionCalc *c);   /* process_objlist (0x2934f) with oCswap (0x292ea) */
/* can_place (0x29498): whether an object of kind item (its index skipped) fits at x, y, z; sets coll_place_height /
 * coll_place_terrain */
u8 coll_can_place(int item, int index, int x, int y, int z, u8 flier, u8 range);
