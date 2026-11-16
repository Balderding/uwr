/* sfx.h: UW.EXE's sound effects (segment 1bd8), ported routine by routine: SOUNDS.DAT's 24 effects played on channels
 * locked from the sound driver, each with its bank-1 timbre, ended by a 16-a-second timer; the volume and pan of an
 * effect at a place from the player's position and heading. The driver is reached through SfxAil (in the game:
 * xmidi.c and the timbre library; in tools/sfx_verify.py's check: a recorder), so the calls can be compared with the
 * original's own, run in the emulator. */
#pragma once
#include "base.h"

typedef struct {
    int (*timbre_status)(int bank, int patch);      /* AIL 159: nonzero when installed */
    bool (*install_timbre)(int bank, int patch);    /* UW.EXE 0x1f341: from the timbre library into the driver */
    int (*lock_channel)(void);                      /* AIL 191: 1-16, 0 none */
    void (*release_channel)(int chan);              /* AIL 193 */
    void (*send_cv)(int status, int d1, int d2);    /* AIL 186 */
} SfxAil;

/* the original's state, by its data addresses (DS 0x25f6 ...) */
typedef struct {
    u8 active;          /* DS 0x25f6: a bit for each of the 4 effect slots */
    u8 chan[4];         /* 0x2612: the locked channel (1-16) */
    i16 time[4];        /* 0x262c: 1/16 s left, -1 forever */
    u8 note[4];         /* 0x25f7 */
    u8 timbre[4];       /* 0x2606 */
    u16 chans;          /* 0x14c: a bit for each locked channel */
    u8 device;          /* 0x13a: the music device (1 the PC speaker, 6 the Roland ...) */
    bool on, ready;     /* 0x137, 0x136: effects on; the driver ready */
    int count;          /* 0x2644: SOUNDS.DAT's effects */
    u8 table[64][5];    /* SOUNDS.DAT: timbre, note, velocity, duration (big-endian word, 1/256 s) */
} Sfx;
extern Sfx g_sfx;

void sfx_init(const SfxAil *ail, const u8 *sounds_dat, size_t size, int device); /* 0x1fd8e + 0x1fe99 */
int sfx_play(int id, int pan, int vol_delta);                                     /* 0x1fa9b: the slot, or 0xff */
/* 0x1f82e: an effect at (x, y) (tile x 8 + fine), heard from the player at (px, py) facing heading8 (0-255) */
int sfx_play_at(int id, int x, int y, int vol_delta, int px, int py, int heading8);
int sfx_start(int id, int timbre, int note, int vel, int pan, int duration); /* 0x1fee9 */
void sfx_tick(void);                                                         /* 0x1fc95, 16 times a second */
void sfx_stop(int slot);                                                     /* 0x1fb7d */
void sfx_stop_all(void);                                                     /* 0x1fbfc */

/* the original's math library (segment 1f3a / DS 0x5624) */
int uw_isqrt(u32 v);                         /* 0x23018: Newton's, five steps from a guess by the size */
void uw_sincos(u16 angle, int *s, int *c);   /* 0x23009: the table of round(32768 sin), 256 steps a turn */
