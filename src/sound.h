/* sound.h: the game's sound through UW's own Roland driver (xmidi.c, MT32MPU.ADV ported) into Munt (build\mt32emu.dll)
 * with the user's Roland ROMs (uo-new\roms: CM-32L preferred), out through Windows at 32 kHz. The driver is served
 * 120 times a second on the audio thread, as the original's timer did. Music and effects each get their own Munt
 * (a comfort: two volumes): every byte goes to both, except notes, which go by the channel: one locked for an effect
 * to the effects' Munt, the rest to the music's. Game code calls the driver (xmidi_*) between sound_lock and
 * sound_unlock. */
#pragma once
#include "base.h"

bool sound_start(const char *data_dir, float music_volume, float effect_volume); /* false: no ROMs / DLL / output */
bool sound_active(void);
void sound_stop(void);
void sound_music_volume(float v);  /* 0..1 */
void sound_effect_volume(float v); /* 0..1 */
void sound_music(bool on);         /* the music on / off (effects go on): UW.EXE 0x1f6e8 */
void sound_next_song(void);        /* a wandering song asked for (ours: a key) */
/* the game's music choice (songs.c, UW.EXE seg 1bd8): the game calls these where the original does */
void sound_song_play(int n);                    /* 0x1f39c: song n now (1 the title, 10 death) */
void sound_song_set_next(int n);                /* 0x2053a: asked for, played at the next turn (9 a kill, 13 ...) */
void sound_song_random(void);                   /* 0x20545: a wandering song asked for */
void sound_song_restart(void);                  /* 0x2055c: after a screen of its own, the song again if it ended */
void sound_songs_update(u32 clock, bool fight_mode); /* 0x2058d: each turn the 256 Hz clock moved */
void sound_song_engaged(u32 clock);             /* 0x1b652: a creature comes for the player */
void sound_song_blow(u32 clock, bool by_player, int hp, int max_hp); /* 0x1c447: a blow its target lived through */
void sound_song_fight_mode(bool on);            /* 0x274c5 / 0x27621 */
void sound_lock(void);
void sound_unlock(void);
/* the effects (SOUNDS.DAT 0-23; UW.EXE 0x1fa9b, 0x1f82e): the slot, or 0xff; at a place: (x, y) and the player's
 * (px, py) in eighths of a tile, the player's heading 0-255 */
int sound_effect(int id, int pan, int vol_delta);
int sound_effect_at(int id, int x, int y, int vol_delta, int px, int py, int heading8);
void sound_effect_stop(int slot);
void sound_effect_stop_all(void); /* 0x1fbfc */
const char *sound_device(void);    /* "Roland CM-32L" or "" */
/* the checks: the same chain for some seconds into a WAV file (no output device) */
/* (effect >= 0: that effect started after a second, the music off: the effects' chain alone) */
bool sound_render_wav(const char *data_dir, const char *wav, float seconds, float music_volume, float effect_volume, int effect);
