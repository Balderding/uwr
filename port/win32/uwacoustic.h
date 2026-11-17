/* uwacoustic.h: our sound rays over the level's tile grid (docs\superpowers\plans\2026-11-14-acoustics-ours.md, Tasks 3
   and 4): what a ray hits, how the room round a point rings, and how much of a sound reaches the listener past rock
   and shut doors. Presentation only: nothing here is read by the game. Plain C, no OpenAL, no DLL:
   tests\uwacoustic_test.c runs it alone (tools\fxcheck.py acoustics).

   Space: the renderer's (x east, y up, z north; one tile = 1.0; a floor at height * 0.25, the ceiling at 4.0).
   Distances are tiles; uwac_grid's tile_m (metres a tile) turns them into metres for the room's times.
   Materials are preset indices of uwacmat.h (uwacmat_get). */
#ifndef UWACOUSTIC_H
#define UWACOUSTIC_H
#include "uwacmat.h"

/* a tile's type: src\level.h's T_* (the same numbers) */
enum { UWAT_SOLID, UWAT_OPEN, UWAT_DIAG_SE, UWAT_DIAG_SW, UWAT_DIAG_NE, UWAT_DIAG_NW, UWAT_SLOPE_N, UWAT_SLOPE_S, UWAT_SLOPE_E, UWAT_SLOPE_W };

typedef struct { unsigned char type, height, wall_mat, floor_mat; } UwaTile;   /* mats: preset indices */
typedef struct { int id, tx, tz, heading, mat, shut; } UwaDoor;
typedef struct { float lo[3], hi[3]; int mat; } UwaBox;   /* a thing standing in the level (a table, a pillar): its box in tiles, its preset */
typedef struct { float dist; int mat, kind; float n[3]; } UwaHit;   /* kind UWA_* (uwacmat.h); dist in tiles; n the
                                                                     surface's unit normal, toward the ray's side */
typedef struct { float rt60[3], mfp_m, first_m; int hits; } UwaRoom;   /* seconds, low / mid / high; the room's mean
                                                                     free path, and the mean distance from the point
                                                                     to the first surface (the early echoes' delay),
                                                                     in metres; hits: the 64 first rays that hit */

void uwac_grid(const UwaTile t[64 * 64], int ceil_mat, float tile_m);   /* [z * 64 + x]; copied */
void uwac_doors(const UwaDoor *d, int n);                               /* copied (128 at most); only shut ones stop a ray */
void uwac_tile_m(float tile_m);                                         /* the metres a tile, changed alone */
/* the dials: the room probe's rays (64) and the legs each is followed for (12), how far a leg may go (16 tiles), how
   far the way round a corner may be (12 tiles) */
void uwac_tune(int rays, int legs, float reach, float way);
void uwac_frames(int on);     /* 1 (as it comes): a door's frame stands for a ray, its opening half a tile wide and 13/16 high */
/* the things standing in the level (copied, 256 at most): a ray stops on a box as on a wall of its material.
   uwac_things_on(0): they are not there for a ray (the Esc menu's "Furniture in the sound"). */
void uwac_things(const UwaBox *b, int n);
void uwac_things_on(int on);
void uwac_lift(float p[3]);   /* a thing's foot lifted 0.3 tile off its floor, kept under the ceiling */

/* a ray from o along dir (any length but 0), max tiles long: 1 and *hit when it hits, 0 when nothing within max, -1
   when o is not in open air (in rock, under the floor, above the ceiling, outside the map). It hits the floor, the
   ceiling, a wall on entering rock or a tile whose floor is above the ray there (the entered tile's wall material),
   a diagonal tile's wall across it, and a shut door: a plane across the middle of its tile, wall to wall and floor
   to ceiling (heading 0 / 4: spanning x; 2 / 6: spanning z). Outside the 64 x 64 grid is rock. */
int uwac_ray(const float o[3], const float dir[3], float max, UwaHit *hit);

/* the room round a point: 64 rays (a Fibonacci sphere), 16 tiles a leg at most, each scattered on from where it hits
   (a rough surface's cosine law, the same way every time) for 12 legs or until it leaves. The mean free path is the
   mean leg from the third on (the room's own, 4 V / S, wherever the point stands in it), the absorption the mean over
   every surface hit; Sabine's RT60 = 0.161 (V / S) / a. A leg that hits nothing within 16 tiles ends the ray as an
   open surface (absorb 1). 1 and *out; 0 when at is not in open air. */
int uwac_room(const float at[3], UwaRoom *out);

/* how much of a sound at `from` reaches `to`, by band (low, mid, high; 1: all of it): 1 always. Nine rays from `to`:
   one to the source and eight to the corners of a cube 0.3 tile round it (those of them in open air). A ray is 1
   when clear; through shut doors only, the product of their materials' transmit[band]; through rock, a floor or the
   ceiling, 0. The mean over the rays, never under {0.15, 0.10, 0.03} (round a corner a sound is muffled, not gone).
   A shut door in the source's own tile does not count (a door's own creak is not behind the door).
   A source within 0.1 tile of `to`, or either point not in open air: {1, 1, 1}. */
int uwac_path(const float from[3], const float to[3], float gain[3]);

/* Task 5, sound round corners: the way a sound takes from `from` to `to` when rock stands on the straight line, as a
   person would walk it: over the open tiles (8 neighbours, a diagonal step only when both tiles beside it are open),
   not through a shut door (but a sound made in a shut door's own tile leaves it), not up a step of more than 4
   height steps (a wall to the sound that comes from below it), 12 tiles at most. The walked tiles less the straight
   ones (both between the tiles' middles); -1 when there is no way within 12. */
float uwac_detour(const float from[3], const float to[3]);
/* where that sound seems to come from at `to`: the last place on its way that `to` sees in a straight line (the
   opening it comes through), a tile's middle at to's height. 1 and *out; 0 when there is no way (out untouched). */
int uwac_waypoint(const float from[3], const float to[3], float out[3]);
/* uwac_path with the way round: when the middle band is under 0.5 and there is a way, each band is at least
   {1, 0.8, 0.5} * (1 - detour / 4) (nothing beyond a detour of 4 tiles). 1 when the way raised it, else 0. */
int uwac_path_round(const float from[3], const float to[3], float gain[3]);

/* Task 6, footsteps by the floor: the preset of the floor under a point; -1 none (rock, outside the map, no level) */
int uwac_floor_mat(const float at[3]);

#endif
