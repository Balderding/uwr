/* songs.c: UW.EXE's choice of music (seg 1bd8), routine by routine; see songs.h. tools\songs_verify.py runs the
 * original routines in the emulator against these on the same inputs. */
#include "songs.h"

Songs g_songs = {0, 0, -1, false, false, true, 0, 0};
static const SongsAil *A;
#define M g_songs

/* the songs that start again when they end, by number (the table at 561b:0000, file 0x593b0) */
static const u8 REPEATS[16] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0};

void songs_init(const SongsAil *ail, bool roland) {
    A = ail;
    M.cur = M.next = 0, M.handle = -1, M.roland = roland, M.combat_t = M.switch_t = 0;
    M.loaded = M.on = true; /* (0x1f26e: the driver and the library found) */
}

/* 0x1f39c: song n, its file read and its timbres installed unless it is the one there; started when asked (from
 * its beginning) at tempo 100 % and volume 96 %. A missing file or timbre turns the music off for good (0x138). */
bool songs_play(int n, bool start) {
    if (!M.loaded || !M.on) return false;
    if ((u8)n != M.cur) {
        char name[9] = "uw00.xmi";
        if (!M.roland) name[0] = 'a';
        name[2] = (char)(((u8)n >> 3) + '0');
        name[3] = (char)(((u8)n & 7) + '0');
        if (!A->file(name)) goto fail;
        A->stop(M.handle);
        A->seq_release(M.handle);
        M.handle = A->seq_register();
        if (M.handle == -1) goto fail;
        for (;;) {
            int t = A->timbre_request(M.handle);
            if (t == -1) break;
            if (!A->install((t >> 8) & 0xff, t & 0xff)) goto fail;
        }
    }
    if (start) {
        A->start(M.handle);
        A->set_tempo(M.handle, 100, 0);
        A->set_volume(M.handle, 96, 0);
    }
    M.cur = (u8)n, M.next = 0;
    return true;
fail:
    M.handle = -1, M.loaded = false;
    return false;
}

/* 0x1f58d: the song again where it was, unless it plays */
void songs_resume(void) {
    if (!M.loaded || !M.on || M.handle == -1) return;
    if (A->status(M.handle) != 1) A->start(M.handle);
    A->set_volume(M.handle, 96, 0);
}

int songs_current(void) { return M.cur; }

/* 0x1f6e8: on: a wandering song if none is there, then (again) from its start; off: stopped (only with a song) */
void songs_enable(bool on) {
    if (!M.loaded) return;
    if (on && !M.on) {
        M.on = true;
        if (M.handle == -1) songs_next_random(), songs_play(M.next, true);
        A->start(M.handle);
    } else if (!on && M.on && M.handle != -1) {
        M.on = false;
        A->stop(M.handle);
    }
}
void songs_toggle(void) { /* 0x1f75f */
    if (M.loaded) songs_enable(!M.on);
}

void songs_set_next(int n) { M.next = (u8)n; }                /* 0x2053a */
void songs_next_random(void) { M.next = (u8)(A->rand() % 3 + 2); } /* 0x20545 */

/* 0x20741: no song, or it does not play */
bool songs_ended(void) { return M.handle == -1 || A->status(M.handle) != 1; }

/* 0x2055c */
void songs_restart_if_over(void) {
    int n = M.cur;
    if (!songs_ended()) return;
    if (M.cur == 1) n = 4;
    songs_play(n, true);
}

static bool fight_song(int n) { return n >= 5 && n <= 7; }

/* 0x2058d: the music's turn */
void songs_update(u32 clock, bool fight_mode, bool main_screen) {
    if (!M.loaded || !M.on) return;
    if ((M.cur == 9 || M.cur == 11) && !songs_ended()) return; /* these play to their end */
    if (fight_song(M.cur) && clock > M.combat_t + 0xa00) { /* 10 s without a blow: the fight is over */
        if (fight_mode) M.next = 8;
        else songs_next_random();
    }
    if (M.next != 0 && M.next != M.cur) {
        if (fight_song(M.cur) && fight_song(M.next)) {
            if (clock > M.switch_t + 0x800) { /* a fight's song changes at most every 8 s */
                songs_play(M.next, true);
                M.switch_t = clock;
            } else M.next = M.cur;
        } else songs_play(M.next, true);
        if (fight_song(M.next)) M.switch_t = clock;
        return;
    }
    if (!songs_ended()) return;
    /* the song is over: in the main screen a wandering song follows a wandering or a single one; else the one
     * asked for again (none: a wandering one); Fight mode: its own */
    bool pick = (!REPEATS[M.cur & 15] || (M.cur >= 2 && M.cur <= 4)) && main_screen;
    if (pick || M.next == 0) songs_next_random();
    if (fight_mode) M.next = 8;
    songs_play(M.next, true);
    M.switch_t = 0;
}

/* --- the callers' parts ----------------------------------------------------------------------------------------- */
void songs_fight(u32 clock, int song) {
    songs_set_next(song);
    M.combat_t = clock;
}
/* 0x1b652: a creature whose goal is the player (attack): 6 unless a fight's song plays; the time kept either way */
void songs_engaged(u32 clock) {
    if (!fight_song(songs_current())) songs_set_next(6);
    M.combat_t = clock;
}
/* 0x1c447: the damage routine, after a blow that left its target alive: the player's on a creature, 5 if it is
 * below a quarter of its life (hp x 64 / (max + 1) < 16), else 6; a creature's on the player, 7 if the player is,
 * else 6 */
void songs_blow(u32 clock, bool by_player, int hp, int max_hp) {
    int q = (hp & 0xff) * 64 / ((max_hp & 0xff) + 1);
    songs_fight(clock, q < 16 ? (by_player ? 5 : 7) : 6);
}
/* 0x274c5 (an icon chosen) / 0x27621 (Fight mode by a key): on: 8 unless a fight's song plays; off (the bit cleared):
 * if 8 plays, a wandering song */
void songs_fight_mode(bool on) {
    if (on) {
        if (!fight_song(songs_current())) songs_set_next(8);
    } else if (songs_current() == 8) songs_next_random();
}
