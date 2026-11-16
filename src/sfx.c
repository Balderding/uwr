/* sfx.c: see sfx.h. Each routine keeps the original's order of tests and its 16-bit arithmetic. */
#include "sfx.h"

#include <math.h>
#include <string.h>

Sfx g_sfx;
static SfxAil A;

/* --- the math library ------------------------------------------------------------------------------------------- */
static i16 g_sin[320]; /* DS 0x5624:0x4e0: round(32768 sin(2 pi i / 256)), at most 32767; 320 so cos = sin + 64 */
static void sin_table(void) {
    if (g_sin[64]) return;
    for (int i = 0; i < 320; i++) {
        double v = floor(32768.0 * sin(6.283185307179586 * i / 256.0) + 0.5);
        g_sin[i] = (i16)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
    }
}
void uw_sincos(u16 angle, int *s, int *c) {
    sin_table();
    int i = angle >> 8;
    *s = g_sin[i], *c = g_sin[i + 64];
}
int uw_isqrt(u32 v) {
    u16 cx = (u16)(v >> 16), bx = (u16)v;
    if (cx >> 8) { /* ch */
        u32 di = cx >= 0x4000 ? 0xffff : 0x4000;
        for (int k = 0; k < 5; k++) di = (di + (v / di & 0xffff)) >> 1;
        return (int)di;
    }
    if (cx) { /* cl */
        u32 di = 0x400;
        for (int k = 0; k < 5; k++) di = (di + (v / di & 0xffff)) >> 1;
        return (int)di;
    }
    if (bx >> 8) {
        u32 c = 0x40;
        for (int k = 0; k < 5; k++) c = (c + bx / c) >> 1;
        return (int)c;
    }
    if (bx) {
        u32 c = 4;
        for (int k = 0; k < 5; k++) c = (c + (bx / c & 0xff)) >> 1;
        return (int)c;
    }
    return 0;
}

/* --- the effects ------------------------------------------------------------------------------------------------ */
void sfx_init(const SfxAil *ail, const u8 *sounds_dat, size_t size, int device) {
    A = *ail;
    memset(&g_sfx, 0, sizeof g_sfx);
    g_sfx.device = (u8)device;
    g_sfx.on = g_sfx.ready = true;
    sin_table();
    if (!sounds_dat || !size) return;
    g_sfx.count = sounds_dat[0];
    for (int i = 0; i < g_sfx.count && i < 64 && 1 + (size_t)i * 5 + 5 <= size; i++) memcpy(g_sfx.table[i], sounds_dat + 1 + i * 5, 5);
}

int sfx_start(int id, int timbre, int note, int vel, int pan, int duration) { /* 0x1fee9 */
    (void)id;
    int slot = 0;
    u8 bit = 1;
    while ((g_sfx.active & bit) && slot < 4) slot++, bit = (u8)(bit << 1);
    if (slot == 4) return 0xff;
    u8 ch;
    i16 dur = (i16)duration;
    if (g_sfx.device == 1) return 0xff; /* the PC speaker (device 1, the original's jump table at cs:0x116d): not a device
                                         * uo-new has, so not ported (ROUTINES.md: replaced) */
    if (!A.timbre_status(1, timbre & 0xff) && !A.install_timbre(1, timbre & 0xff)) return 0xff;
    ch = (u8)A.lock_channel();
    if (!ch) return 0xff;
    g_sfx.chans |= (u16)(1u << ch);
    g_sfx.active |= bit;
    g_sfx.chan[slot] = ch;
    g_sfx.time[slot] = dur == -1 ? -1 : (i16)(dur / 16);
    g_sfx.note[slot] = (u8)note;
    g_sfx.timbre[slot] = (u8)timbre;
    A.send_cv(0xaf + ch, 0x72, 1);              /* bank 1 (PATCH_BANK_SEL) */
    A.send_cv(0xbf + ch, timbre & 0xff, 0);     /* the timbre */
    A.send_cv(0xaf + ch, 0x79, 0);              /* reset all controllers */
    A.send_cv(0xaf + ch, 7, 0x7f);              /* volume */
    A.send_cv(0xaf + ch, 0x0b, 0x7f);           /* expression */
    A.send_cv(0xaf + ch, 0x0a, pan & 0xff);     /* pan */
    A.send_cv(0x8f + ch, note & 0xff, vel & 0xff); /* the note */
    return slot;
}

int sfx_play(int id, int pan, int vol_delta) { /* 0x1fa9b */
    if (!g_sfx.on || !g_sfx.ready) return 0xff;
    const u8 *e = g_sfx.table[id & 0xff];
    int v = e[2] + (i8)(u8)vol_delta;
    v = v > 0x7f ? 0x7f : v < 0 ? 0 : v;
    return sfx_start(id & 0xff, e[0], e[1], v, pan & 0xff, e[3] << 8 | e[4]);
}

int sfx_play_at(int id, int x, int y, int vol_delta, int px, int py, int heading8) { /* 0x1f82e */
    if (!g_sfx.on || !g_sfx.ready) return 0xff;
    const u8 *e = g_sfx.table[id & 0xff];
    i32 dx = (i16)(x - px), dy = (i16)(y - py);
    int dist = uw_isqrt((u32)(dx * dx + dy * dy)) & 0xffff;
    int vol, pan;
    if (dist == 0) {
        vol = e[2] + (i8)(u8)vol_delta, pan = 0x40;
    } else {
        int ny, nx; /* the direction, x 128 (the original: exactly -dist gives +128) */
        if (dy == dist) ny = 0x7f;
        else if (-dy == dist) ny = 0x80;
        else ny = (int)((dy * 128) / dist);
        if (dx == dist) nx = 0x7f;
        else if (-dx == dist) nx = 0x80;
        else nx = (int)((dx * 128) / dist);
        u16 ang = (u16)(0x4000 - (u16)((heading8 & 0xff) << 8));
        int s, c;
        uw_sincos(ang, &s, &c);
        s = (i16)s >> 8, c = (i16)c >> 8;
        i16 d = (i16)((i16)(ny * c) - (i16)(nx * s));
        d = (i16)(d >> 8);
        pan = 0x40 - d;
        pan = pan > 0x7f ? 0x7f : pan < 0 ? 0 : pan;
        vol = e[2] + (i8)(u8)vol_delta;
        if ((u16)dist > 0x30) return 0xff; /* out of hearing */
        if ((u16)dist >= 8) vol = (u16)((u16)(vol * (0x30 - dist)) / 0x28);
    }
    vol = (i16)vol > 0x7f ? 0x7f : (i16)vol < 0 ? 0 : vol;
    return sfx_start(id & 0xff, e[0], e[1], vol, pan, e[3] << 8 | e[4]);
}

static void end_slot(int slot, bool note_off) {
    g_sfx.active &= (u8)~(1u << slot);
    if (g_sfx.device == 1) A.send_cv(0xaf + g_sfx.chan[slot], 0x7b, 0);
    else {
        if (note_off) A.send_cv(0x7f + g_sfx.chan[slot], g_sfx.note[slot], 0);
        A.release_channel(g_sfx.chan[slot]);
    }
    g_sfx.chans &= (u16)~(1u << g_sfx.chan[slot]);
}

void sfx_tick(void) { /* 0x1fc95 */
    if (!g_sfx.active) return;
    for (int slot = 0; slot < 4; slot++) {
        if (!(g_sfx.active & (1u << slot)) || g_sfx.time[slot] == -1) continue;
        if (--g_sfx.time[slot] != 0) continue;
        end_slot(slot, true);
    }
}
void sfx_stop(int slot) { /* 0x1fb7d: the original tests the slot's bit of the byte at DS 0x25f6 with no range check;
                            * 0-7 are that byte's bits (only 0-3 are ever set: four slots), the test here guards the shift */
    if (slot < 0 || slot > 7 || !(g_sfx.active & (1u << slot))) return;
    end_slot(slot, false);
}
void sfx_stop_all(void) { /* 0x1fbfc */
    for (int slot = 0; slot < 4; slot++)
        if (g_sfx.active & (1u << slot)) end_slot(slot, false);
}
