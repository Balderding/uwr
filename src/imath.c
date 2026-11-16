/* imath.c: UW.EXE's integer trigonometry and square root (seg019 module 14, IMATH.ASM, with C3DENTRY.ASM's C
 * entries; underworld-exhumed uw1/src/sys/IMATH.ASM, MIT), instruction for instruction in C. The tables are the
 * original's, read from the user's UW.EXE (seg063, paragraph 0x5624 of the load image: 0x4E0 the sine at each 1/256
 * turn, 257 words, so 0x560 is the cosine; 0x762 the arcsine and 0x864 the arccosine of 0 .. 1.0 in 128 steps).
 * Angles are words, 0x10000 to the turn; a sine is 1.15. Compiled with compat.h force-included, like the core. */
#include "uw2.h"

static uint8_t imath_tab[0xAB0];
static int16_t T(unsigned off) { return (int16_t)(imath_tab[off] | imath_tab[off + 1] << 8); }

/* the tables from UW.EXE's bytes (an MZ image: its header's paragraphs, then the load image) */
int core_math_init(const unsigned char *exe, long size)
{
    long img = (long)(exe[8] | exe[9] << 8) * 16, at = img + 0x5624L * 16;
    if (size < at + (long)sizeof imath_tab) return 0;
    for (unsigned i = 0; i < sizeof imath_tab; i++) imath_tab[i] = exe[at + i];
    void core_dos_init(const unsigned char *exe, long size);
    core_dos_init(exe, size); /* DGROUP's constant tables (src/core_dos.c) */
    void core_null_init(const unsigned char *exe, long size);
    core_null_init(exe, size); /* what a null far pointer reads (src/core_glue.c) */
    return 1;
}

/* imul cx with the 8.8 result taken from DX:AX's middle (mov al,ah; mov ah,dl) */
static uint16_t lerp(int16_t base, int16_t next, uint16_t frac)
{
    int32_t r = (int32_t)(int16_t)(uint16_t)(next - base) * (int16_t)frac;
    return (uint16_t)((uint32_t)r >> 8) + (uint16_t)base;
}

/* _A38 (sincos): the sine and cosine, interpolated by the angle's low byte */
static void a38(uint16_t bx, uint16_t *ax, uint16_t *bxo)
{
    uint16_t cx = bx & 0xFF, i = (bx >> 8) * 2;
    *ax = lerp(T(0x4E0 + i), T(0x4E2 + i), cx);
    *bxo = lerp(T(0x560 + i), T(0x562 + i), cx);
}

void cSinCos(int angle, int16 *x, int16 *y)
{
    uint16_t s, c;
    a38((uint16_t)angle, &s, &c);
    *x = (int16)s;
    *y = (int16)c;
}

/* _A69 (fast_sincos): the entries for the angle's high byte */
void cFstSinCos(int angle, int16 *a, int16 *b)
{
    uint16_t i = ((uint16_t)angle >> 8) * 2;
    *a = T(0x4E0 + i);
    *b = T(0x560 + i);
}

/* _A78 (lsqrt): five rounds of Newton's method from a guess chosen by the highest non-zero byte. (DOS faults on a
 * divide overflow, which only the largest values reach; the quotient is cut to 16 bits here.) */
static uint16_t round16(uint32_t v, uint16_t d) { uint16_t q = (uint16_t)(v / d); return (uint16_t)(((uint32_t)d + q) >> 1); }
int cSqRt(int32 v)
{
    uint32_t u = (uint32_t)v;
    uint16_t cx = (uint16_t)(u >> 16), bx = (uint16_t)u, di;
    if (cx >> 8) {
        di = cx >= 0x4000 ? 0xFFFF : 0x4000;
        for (int k = 0; k < 5; k++) di = round16(u, di);
        return di;
    }
    if (cx & 0xFF) {
        di = 0x400;
        for (int k = 0; k < 5; k++) di = round16(u, di);
        return di;
    }
    if (bx >> 8) {
        uint16_t c = 0x40;
        for (int k = 0; k < 5; k++) c = round16(bx, c);
        return c;
    }
    if (bx & 0xFF) {
        uint8_t cl = 4;
        for (int k = 0; k < 5; k++) { uint8_t q = (uint8_t)(bx / cl); cl = (uint8_t)(((unsigned)cl + q) >> 1); }
        return cl;
    }
    return 0;
}

/* _B78: the arcsine of ax, interpolated, with ax's sign; _BA2: the arccosine of bx, into the other half for a
 * negative bx */
static uint16_t b78(uint16_t ax)
{
    uint16_t dx = (int16_t)ax < 0 ? 0xFFFF : 0;
    ax = (ax ^ dx) - dx;
    uint16_t i = (ax >> 8) * 2, cx = lerp(T(0x762 + i), T(0x764 + i), ax & 0xFF);
    return (cx ^ dx) - dx;
}
static uint16_t ba2(uint16_t bx)
{
    uint16_t dx = (int16_t)bx < 0 ? 0xFFFF : 0, ax = (bx ^ dx) - dx;
    uint16_t i = (ax >> 8) * 2, cx = lerp(T(0x864 + i), T(0x866 + i), ax & 0xFF);
    cx = (cx ^ dx) - dx;
    return cx + (dx & 0x8000);
}

/* _BD4 (atan2): the angle of the unit vector (sine x, cosine y) */
int cAtan2(int x, int y)
{
    int16_t ax = (int16_t)x, bx = (int16_t)y;
    uint16_t cx;
    if (ax <= 0x5A82 && ax >= (int16_t)0xA57E) {
        cx = b78((uint16_t)ax);
        if (bx < 0) cx = (uint16_t)-(uint16_t)(cx - 0x8000);
        return (int16_t)cx;
    }
    uint16_t dx = ax < 0 ? 0xFFFF : 0;
    cx = ba2((uint16_t)bx);
    cx = (cx ^ dx) - dx;
    return (int16_t)cx;
}
