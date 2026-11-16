/* songs.h: the game's choice of music, UW.EXE seg 1bd8 (0x1f39c-0x2076c) ported routine by routine. Song n is the
 * file UW<n / 8><n % 8>.XMI (two octal digits; AW.. for the AdLib), started through the AIL driver at tempo 100 %,
 * volume 96 %. What plays when:
 *   1 the title (UW01); after it, as after any song that ends in the game's main screen, a wandering song;
 *   2-4 the wandering songs, one at random (UW02-UW04);
 *   5-7 a fight: 6, or 5 when the player's blow leaves the foe below a quarter of its life, 7 when a creature's
 *       blow leaves the player below a quarter of the player's; a fight's song changes at most every 8 s (each try restarts
 *       that time) and ends 10 s after the last blow or engagement;
 *   8 Fight mode (UW10), the player record's byte 0x5f bit 1;
 *   9 a kill (UW11) and 11 (UW13): played to their end;
 *   10 death (UW12); 13 the map, a conversation, sleep (UW15).
 * The caller holds the sound lock: every routine may call the driver. */
#pragma once
#include "base.h"

typedef struct {
    bool (*file)(const char *name);                /* 0x1f6db: data\music\NAME (e.g. "uw02.xmi") into the buffer */
    int (*seq_register)(void);                     /* AIL 151 on that buffer: the handle, or -1 */
    void (*seq_release)(int seq);                  /* 152 */
    int (*timbre_request)(int seq);                /* 155: bank << 8 | patch, or -1 */
    bool (*install)(int bank, int patch);          /* 0x1f341: from the timbre library (UW.MT) into the driver */
    void (*start)(int seq);                        /* 170 */
    void (*stop)(int seq);                         /* 171 */
    int (*status)(int seq);                        /* 174: 1 playing */
    void (*set_volume)(int seq, int pct, int ms);  /* 177 */
    void (*set_tempo)(int seq, int pct, int ms);   /* 178 */
    int (*rand)(void);                             /* the game's rand (UW.EXE 0x12c37: the same stream as the rules) */
} SongsAil;

typedef struct {
    u8 cur, next;    /* DS 0x261e the song playing, 0x261f the one asked for (0: none) */
    int handle;      /* DS 0x2646: the sequence (-1 none) */
    bool loaded;     /* DS 0x138: the music driver and its timbres are there */
    bool on;         /* DS 0x135: the music is wanted (the options) */
    bool roland;     /* DS 0x13a == 6: the UW files, else the AW (AdLib) ones */
    u32 combat_t;    /* DS 0x2482: the clock (256 Hz) at the last blow or engagement */
    u32 switch_t;    /* DS 0x2634: the clock when a fight's song last changed (or was refused) */
} Songs;
extern Songs g_songs;

void songs_init(const SongsAil *ail, bool roland);
bool songs_play(int n, bool start);   /* 0x1f39c */
void songs_resume(void);              /* 0x1f58d */
int songs_current(void);              /* 0x1f6b8 */
void songs_enable(bool on);           /* 0x1f6e8: the music on / off */
void songs_toggle(void);              /* 0x1f75f */
void songs_set_next(int n);           /* 0x2053a */
void songs_next_random(void);         /* 0x20545: 2-4 */
void songs_restart_if_over(void);     /* 0x2055c: after a screen of its own (1 becomes 4) */
void songs_update(u32 clock, bool fight_mode, bool main_screen); /* 0x2058d: each turn the clock moved */
bool songs_ended(void);               /* 0x20741 */
/* the callers' parts (their addresses): */
void songs_fight(u32 clock, int song);              /* 0x1c2d2 / 0x1b138: a fight's song asked for, the time kept */
void songs_engaged(u32 clock);                      /* the block at 0x1b652 (its call of 0x2053a at 0x1b666): a creature comes for the player: 6 unless 5-7 plays */
void songs_blow(u32 clock, bool by_player, int hp, int max_hp); /* the block at 0x1c447 (the player's blow at 0x1c44d, a creature's at 0x1c48d): a blow that did not kill */
void songs_fight_mode(bool on);                     /* 0x274c5 / 0x27621: Fight mode chosen (8) or left (2-4) */
