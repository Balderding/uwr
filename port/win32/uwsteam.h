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

#define UWST_VOICES 64       /* the sounds Steam Audio plays at once (its sound itself: one a source of uwaudio.c's,
                                which has as many) */
#define UWST_ECHOES 32       /* the echo slots: the sounds that can have echoes at once (uwst_echo_voices: the newest,
                                up to this) */
#define UWST_MAXCH 8         /* the most output channels the stream can have (7.1) */
#define UWST_FRAME 512       /* frames a block at the rate given to uwst_init (10.7 ms at 48 kHz; 1024 until
                                2026-11-15: a new sound's echoes came in behind three queued blocks, 60-100 ms) */

int  uwst_init(const char *bindir, int rate);     /* 1 on; 0 with one log line (no phonon.dll, ...) */
void uwst_shutdown(void);
int  uwst_on(void);
/* the level (tiles NULL: the same tiles, the doors anew); taken at the next uwst_simulate */
void uwst_level(const UwaTile t[64 * 64], int ceil_mat, const UwaDoor *d, int nd, float tile_m);
void uwst_quality(int rays, int bounces);         /* a run's rays (256..8192) and bounces (1..64): the menu's Echo detail */
void uwst_fast_rays(int on);                       /* (2026-11-18) Embree's rays and more threads (acoustics_embree), at uwst_init */
void uwst_tile_m(float tile_m);                    /* the metres a tile changed alone: the scene made again */
/* the dials (2026-11-16; settings.txt through uwaudio.c). uwst_length: the longest ring in seconds (0.25..6; 2 unless
   set), taken when Steam Audio starts: 1 when the running one has another, so the caller starts it again.
   uwst_scatter: the share of a bounce that goes every way (0..1; 0.25), the scene made again. uwst_ear: 1 (as it
   comes) keeps a response for the listener's own place ready, and a new sound rings with it until the rays have run
   for its own place; 0: a new sound's echoes wait for its own run, as until 2026-11-16. */
int  uwst_length(float seconds);
float uwst_length_now(void);
void uwst_scatter(float share);
void uwst_ear(int on);
void uwst_things(const UwaBox *b, int n);   /* the things standing in the level, as boxes (copied; the scene made again) */
void uwst_things_on(int on);                /* 0: they are left out of the scene */
void uwst_frames(int on);     /* 1 (as it comes): a door's frame is in the scene, the leaf in its opening when shut */
/* The sound itself through Steam Audio (2026-11-16). uwst_direct_mode: on 1 and a listener on headphones (uwst_mix's
   hrtf): every voice's own samples go through Steam Audio's direct effect and its binaural effect (its HRTF) into
   the mix beside the echoes, at uwst_direct_level (1.0: as the samples are); the caller then keeps its own copy of
   that sound silent. steam_occlusion 1: what reaches the listener is Steam Audio's (occlusion rays from a source
   radius_m across, `samples` of them; transmission through the surfaces between by their materials, `rays`);
   0: the gains given by uwst_direct. uwst_direct, for a voice: where it is heard from (its place unless told),
   gains by band, and whether those are a floor under Steam Audio's (1) or the gains themselves when Steam
   Audio's occlusion is off. */
void uwst_direct_mode(int on, int steam_occlusion, float radius_m, int samples, int rays);
int  uwst_direct_on(void);
void uwst_direct_level(float g);
void uwst_direct(int voice, const float heard[3], const float gain[3], int floor);
/* Steam Audio's pathing (2026-11-16; uwsteam.c has the whole of it above P). uwst_pathing: on 1 (as it comes), with
   the sound itself Steam Audio's and its occlusion on: a sound that the level hides is heard along the ways Steam
   Audio finds through the level's probes (a probe in every open tile, the ways between them baked once a level in
   a thread of its own), at `level` (1: as simulated); our own way round a corner then stands back for that sound.
   Until a level's bake has ended (under a second), and with it off, our way round stays. _probes: the probes in
   use (0 none yet); _busy: a bake is under way; _heard: a voice's last paths, for the checks (the three bands'
   gains and the first four of the directional coefficients; 0 when it has none). */
void uwst_pathing(int on, float level);
void uwst_level_change(void);   /* a level change under way: no pathing until the new level's tiles come */
int  uwst_pathing_probes(void);
int  uwst_pathing_busy(void);
int  uwst_pathing_heard(int voice, float eq[3], float sh[4]);
/* a voice heard from a place fixed to the listener's head, `right` and `ahead` of it in tiles (the player's own
   sounds: the two feet). Until 2026-11-16 the place stood in the world, and the walking player went through it. */
void uwst_relative(int voice, float right, float ahead);
void uwst_echo(int voice, float share);            /* a voice's echoes as a share of its own level (a footstep's) */
void uwst_listener(float x, float y, float z, float yaw);   /* yaw 0 looks north, east positive (radians) */
/* a sound starts at a place: its samples (mono, copied and brought to the module's rate) feed its reflections from
   their start. The voice (0..UWST_VOICES-1), or -1 when all are busy with nearer or newer sounds. */
int  uwst_begin(float x, float y, float z, const short *mono, int frames, int rate, float gain);
void uwst_end(int voice);                         /* the sound was cut (its echoes ring out) */
void uwst_simulate(void);                         /* one run of the rays for the listener and the voices as they are now */
/* The output's speakers: uwst_channels before uwst_init sets how many channels the stream has from the next start:
   2 stereo (the default; binaural when hrtf), 4 quad, 6 5.1 or 8 7.1 (any other number: 2), in OpenAL's and Steam
   Audio's order (front left, front right, centre, LFE, back left, back right, side left, side right). On more than
   2 channels the sound is panned over the speakers and decoded for them, never binaural. Returns 1 when the running
   start has another number, so the caller starts it again. uwst_channels_now: the running start's. */
int  uwst_channels(int n);
int  uwst_channels_now(void);
/* the next `frames` frames of every voice's sound and reflections, interleaved with uwst_channels_now() channels
   (binaural when hrtf and stereo); the voices still sounding or ringing. `wet` scales the echoes (1: as simulated). */
int  uwst_mix(short *out, int frames, float wet, int hrtf);
int  uwst_mix_float(float *out, int frames, float wet, int hrtf);   /* the same in float (1.0 full scale): the game's */
int  uwst_triangles(void);                        /* of the scene in use (the checks) */
/* (2026-11-19, the sound review: work\soundreview\REPORT-2026-11-19.md; each a row of the Esc menu's Sound tuning)
   uwst_echo_voices: how many sounds have echoes at once (1..UWST_ECHOES; 16 as it comes), heard at once.
   uwst_direct_thread: 1 (as it comes): the muffling's and the paths' sums are made by uwst_direct_tick, which the
   caller calls on a thread of its own every few ms; 0, or no tick for 100 ms: the mixing makes them, as before.
   uwst_speakers: 1 (as it comes): the sound itself is Steam Audio's on speakers too (panned, not by the HRTF), with
   its muffling and paths; uwst_direct_for(hrtf): the sound itself is Steam Audio's for that output.
   uwst_speaker_level: the sound itself on speakers at this times uwst_direct_level's (the makeups' ratio).
   uwst_order: the echoes' ambisonic order, 1 (4 channels) or 2 (9: their directions clearer, about twice the work);
   uwst_head: the head the binaural sound is made for, a SOFA file's path ("" or NULL: Steam Audio's own). Both are
   taken when Steam Audio starts: 1 when the running one differs, so the caller starts it again (uwst_restart_wanted).
   uwst_scatter_by_mat: 1 (as it comes): a material's own scattering (acoustics.txt "scatter S") where it has one,
   else uwst_scatter's; the scene made again. uwst_head_now: the head in use (a name, for the log and the menu). */
void uwst_echo_voices(int n);
void uwst_direct_thread(int on);
void uwst_direct_tick(void);
void uwst_speakers(int on);
int  uwst_direct_for(int hrtf);
void uwst_speaker_level(float ratio);
int  uwst_order(int order);
int  uwst_head(const char *sofa_path);
int  uwst_restart_wanted(void);
void uwst_scatter_by_mat(int on);
const char *uwst_head_now(void);
#endif
