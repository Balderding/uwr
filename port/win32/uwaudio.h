/* uwaudio.h: our positional sound effects (uwaudio.c) over OpenAL Soft, loaded at run time (soft_oal.dll), for the
   effect events the game reports (underworld-exhumed's src/port/sys/fxevent.c). Presentation only: the game's own
   effects (the MT-32 / CM-32L through Munt) are rendered once each by a Munt of our own and placed by OpenAL where
   the game placed the effect, the music driver's copy silenced meanwhile (uwaudio_taken -> port_fx_control); a
   data\sfx\NNN.wav (NNN the SOUNDS.DAT entry) takes precedence where present. Off unless fx_audio 1. */
#ifndef UWAUDIO_H
#define UWAUDIO_H

/* world units as the renderer's: tiles, x east, y up, z north; yaw 0 looks north, east positive (radians) */
int  uwaudio_init(const char *bindir, const char *datadir);   /* 1 on; 0 off (one log line says why) */
void uwaudio_listener(float x, float y, float z, float yaw);
/* kind FX_* (| FX_MUTED: nothing; FX_STOP: the game silenced effect fx, so our copy stops); x < 0: at the listener;
   vol the velocity the game gave the note (0..0x7F; -1: the effect's own) */
void uwaudio_event(int kind, float x, float y, float z, int fx, int vol);
int  uwaudio_rendered(int fx, int vel);   /* 1 once effect fx is rendered at velocity vel (the checks) */
void uwaudio_shutdown(void);
void uwaudio_game_dir(const char *dir);   /* the game's own folder (its SOUND\), before uwaudio_init: read when data\sound is not there */
int  uwaudio_on(void);
unsigned long long uwaudio_taken(void);   /* bit n: effect n is ours to play now (the game's copy to be silent) */
void uwaudio_volume(float gain);          /* the menu's Sound volume, 0..1 */
void uwaudio_pump(void);                  /* once a frame (the hidden checks' loopback WAV) */
int  uwaudio_setting(const char *datadir, const char *key, int dflt);   /* data\settings.txt's "key N" */
/* HRTF (ALC_SOFT_HRTF; fx_hrtf 1 asks for it, the default): the context's ALC_HRTF_STATUS_SOFT (0 off, 1 on, 2 denied,
   3 required, 4 on by headphones detected, 5 unsupported format; -1 no context) and the data set's name ("" none) */
int  uwaudio_hrtf(void);
const char *uwaudio_hrtf_name(void);

/* acoustics (docs\superpowers\plans\2026-11-14-acoustics-ours.md, Task 4; on with fx_acoustics 1 and OpenAL's EFX):
   the room round the listener rings as its size and surfaces say (one EAX reverb, from uwacoustic.c's uwac_room), and
   each sound is quietened and dulled by the rock and the shut doors between it and the listener (uwac_path, a filter
   on its source). The level is handed over by the backend whenever it changes: the tiles (NULL: as they were) with
   the ceiling's material, and the doors. Settings (data\settings.txt, each a row of the Esc menu): acoustics_wet
   (the reverb's level, a percentage, 100; 0: none), acoustics_tile_m (the metres a tile, 2.4: the rooms' size),
   acoustics_occlusion (the muffling's strength, a percentage, 100; 0: no filter at all), acoustics_detour (1: a
   sound round a corner comes along the way a person would walk, louder and from the opening; 0: muffled only);
   uwaudio_tune reads them again and applies them at once. */
#include "uwacoustic.h"
void uwaudio_acoustics(const UwaTile *t, int ceil_mat, const UwaDoor *d, int nd);
void uwaudio_tune(const char *datadir);   /* (also fx_footsteps: the footsteps by their floor, 1 unless written 0) */
int  uwaudio_acoustics_on(void);
float uwaudio_acoustic_decay(void);       /* the reverb's decay time now, seconds (0: none yet; the checks) */

/* the checks (tests\uwaudio_test.c): OpenAL Soft's loopback device (ALC_SOFT_loopback) in place of a sound card,
   rendered into a buffer: nothing is played on the machine's speakers */
int  uwaudio_init_loopback(const char *bindir, const char *datadir, int rate);
int  uwaudio_render(short *stereo, int frames);   /* interleaved left, right; 1 ok */
int  uwaudio_error(void);                         /* alGetError (0: none) */
int  uwaudio_playing(void);                       /* sources playing now */
#endif
