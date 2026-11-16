/* uwsteam.h: Steam Audio's reflections for our acoustics (docs\superpowers\plans\2026-11-14-acoustics-ours.md, Task 7:
   "Acoustics: High", fx_acoustics 2). On top of our own system, not in its place: the muffling by walls and the way
   round corners stay uwacoustic.c's; what this adds is each sound's own echoes, simulated by Steam Audio (phonon.dll,
   Apache 2.0, loaded at run time from bin\ like soft_oal.dll, never linked) in the level's real shape: a sound rings
   with the room it is in, and its early echoes come from the walls near it. Presentation only.

   The level is the tiles the backend already hands the acoustics (uwacoustic.h's UwaTile / UwaDoor): this module makes
   the triangles from them (floors, the ceiling, walls, risers, diagonal walls, shut doors) with uwacmat.c's
   materials. Space as the renderer's (tiles; x east, y up, z north); Steam Audio's metres and right-handed frame
   are this module's own business.

   Threads: uwst_simulate is the slow part (rays), meant for a worker; uwst_mix is the audio's (one block's work);
   the rest only leave word for those two. */
#ifndef UWSTEAM_H
#define UWSTEAM_H
#include "uwacoustic.h"

#define UWST_VOICES 8        /* the sounds that get reflections at once (the others stay dry) */
#define UWST_FRAME 512       /* frames a block at the rate given to uwst_init (10.7 ms at 48 kHz; 1024 until
                                2026-11-15: a new sound's echoes came in behind three queued blocks, 60-100 ms) */

int  uwst_init(const char *bindir, int rate);     /* 1 on; 0 with one log line (no phonon.dll, ...) */
void uwst_shutdown(void);
int  uwst_on(void);
/* the level (tiles NULL: the same tiles, the doors anew); taken at the next uwst_simulate */
void uwst_level(const UwaTile t[64 * 64], int ceil_mat, const UwaDoor *d, int nd, float tile_m);
void uwst_quality(int rays, int bounces);         /* a run's rays (256..8192) and bounces (1..64): the menu's Echo detail */
void uwst_tile_m(float tile_m);                    /* the metres a tile changed alone: the scene made again */
void uwst_listener(float x, float y, float z, float yaw);   /* yaw 0 looks north, east positive (radians) */
/* a sound starts at a place: its samples (mono, copied and brought to the module's rate) feed its reflections from
   their start. The voice (0..UWST_VOICES-1), or -1 when all are busy with nearer or newer sounds. */
int  uwst_begin(float x, float y, float z, const short *mono, int frames, int rate, float gain);
void uwst_end(int voice);                         /* the sound was cut (its echoes ring out) */
void uwst_simulate(void);                         /* one run of the rays for the listener and the voices as they are now */
/* the next `frames` frames of every voice's reflections, mixed to stereo (interleaved; binaural when hrtf); the
   voices still sounding or ringing. `wet` scales it (1: as simulated). */
int  uwst_mix(short *stereo, int frames, float wet, int hrtf);
int  uwst_triangles(void);                        /* of the scene in use (the checks) */
#endif
