/* uwsteam_path_test.c: Steam Audio's pathing in port\win32\uwsteam.c, alone (2026-11-16). Built and run by hand,
   not by fxcheck: clang as fxcheck.py's clang() with port\win32's uwsteam.c, uwacoustic.c and uwacmat.c.
       uwsteam_path_test.exe BINDIR [DUMP]
   The fixture is uwsteam_test.c's: room A 4 x 4 (x 10-13, z 10-13), room B 12 x 12 (x 15-26, z 10-21), the wall column
   x 14 between them, a doorway at (14, 11) with a door. Prints what is measured; PASS / FAIL lines. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../port/win32/uwsteam.h"

#define RATE 48000
enum { STONE, WOOD, CARPET };
static int fails;
static void check(const char *name, int ok) { printf("%s %s\n", name, ok ? "PASS" : "FAIL"); fails += !ok; }
static UwaTile grid[64 * 64];
static void rooms(int door_shut)
{
    UwaDoor door = {1, 14, 11, 2, WOOD, door_shut};
    for (int i = 0; i < 64 * 64; i++) grid[i].type = UWAT_SOLID, grid[i].height = 15, grid[i].wall_mat = grid[i].floor_mat = STONE;
    for (int z = 10; z <= 13; z++)
        for (int x = 10; x <= 13; x++) grid[z * 64 + x].type = UWAT_OPEN, grid[z * 64 + x].height = 0;
    for (int z = 10; z <= 21; z++)
        for (int x = 15; x <= 26; x++) grid[z * 64 + x].type = UWAT_OPEN, grid[z * 64 + x].height = 0;
    grid[11 * 64 + 14].type = UWAT_OPEN, grid[11 * 64 + 14].height = 0;
    uwst_level(grid, STONE, &door, 1, 2.4f);
}
static short tone[RATE];
static float blk[UWST_FRAME * 2];
static double now_ms(void)
{
    LARGE_INTEGER a, q;
    QueryPerformanceFrequency(&q), QueryPerformanceCounter(&a);
    return (double)a.QuadPart * 1000.0 / (double)q.QuadPart;
}
static void wait_bake(const char *what)
{
    double t0 = now_ms();
    uwst_simulate();
    while (uwst_pathing_busy()) Sleep(20), uwst_simulate();
    printf("  %s: %d probes after %.2f s\n", what, uwst_pathing_probes(), (now_ms() - t0) / 1000.0);
}
/* a tone from (sx, sz) heard at (lx, lz) facing north: 30 blocks; the two ears' RMS of the last 20 (the echoes off:
   wet 0), the voice's last paths */
static int hear(float lx, float lz, float sx, float sz, double *l, double *r, float eq[3], float sh[4], double *ms)
{
    double sl = 0, sr = 0, t0;
    int v, ok;
    uwst_listener(lx, 0.64f, lz, 0.0f);
    v = uwst_begin(sx, 0.3f, sz, tone, RATE, RATE, 1.0f);
    uwst_simulate();
    t0 = now_ms();
    for (int b = 0; b < 30; b++) {
        uwst_mix_float(blk, UWST_FRAME, 0.0f, 1);
        if (b >= 10) for (int i = 0; i < UWST_FRAME; i++) sl += (double)blk[2 * i] * blk[2 * i], sr += (double)blk[2 * i + 1] * blk[2 * i + 1];
    }
    if (ms) *ms = (now_ms() - t0) / 30;
    *l = sqrt(sl / (20 * UWST_FRAME)), *r = sqrt(sr / (20 * UWST_FRAME));
    ok = uwst_pathing_heard(v, eq, sh);
    uwst_end(v);
    for (int b = 0; b < 400; b++) if (!uwst_mix_float(blk, UWST_FRAME, 0.0f, 1)) break;
    return ok;
}
static void show(const char *what, int ok, double l, double r, const float eq[3], const float sh[4], double ms)
{
    printf("  %s: paths %d, eq %.3f %.3f %.3f, sh %.4f %.4f %.4f %.4f; left %.4f right %.4f; a block %.2f ms\n",
           what, ok, eq[0], eq[1], eq[2], sh[0], sh[1], sh[2], sh[3], l, r, ms);
}

int main(int argc, char **argv)
{
    char p[1024], dir[1024], *e;
    float eq[3], sh[4];
    double l, r, l0, r0, ms;
    int ok;
    FILE *f;
    static const char *TABLE =
        "preset stone   absorb 0.13 0.20 0.24  transmit 0.015 0.002 0.001\n"
        "preset wood    absorb 0.10 0.08 0.10  transmit 0.070 0.014 0.005\n"
        "preset carpet  absorb 0.05 0.25 0.55  transmit 0.00\n"
        "wall * stone\nfloor * stone\nceiling * stone\ndoor * wood\n";
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) { printf("usage: pathtest BINDIR [DUMP [CACHEDIR]]\n"); return 99; }
    GetModuleFileNameA(NULL, dir, sizeof dir - 32);
    if ((e = strrchr(dir, '\\')) != NULL) *e = 0;
    snprintf(p, sizeof p, "%s\\acoustics.txt", dir);
    if (!(f = fopen(p, "wb"))) return 1;
    fputs(TABLE, f), fclose(f);
    if (!uwacmat_load(dir)) { printf("FAIL: materials\n"); return 1; }
    for (int i = 0; i < RATE; i++) tone[i] = (short)(12000.0 * sin(6.2831853 * 440.0 * i / RATE));
    if (!uwst_init(argv[1], RATE)) { printf("FAIL: no Steam Audio\n"); return 1; }
    uwst_direct_mode(1, 1, 0.5f, 16, 8);

    /* the paths off: the hidden sound's level (transmission alone) */
    uwst_pathing(0, 1.0f);
    rooms(0);
    uwst_simulate();
    hear(20.0f, 18.0f, 12.0f, 11.5f, &l0, &r0, eq, sh, &ms);
    printf("  paths off, the sound in A, the listener in B out of sight: left %.4f right %.4f; a block %.2f ms\n", l0, r0, ms);

    uwst_pathing(1, 1.0f);
    wait_bake("the fixture's bake");
    check("bake", uwst_pathing_probes() == 16 + 144 + 1);

    ok = hear(20.0f, 18.0f, 12.0f, 11.5f, &l, &r, eq, sh, &ms);
    show("door open, out of sight (the doorway is to the listener's west-south-west)", ok, l, r, eq, sh, ms);
    check("round the corner", ok && l + r > 4 * (l0 + r0) && l > 1.15 * r);

    ok = hear(16.0f, 11.5f, 12.0f, 11.5f, &l, &r, eq, sh, &ms);
    show("door open, in plain sight through the doorway (4 tiles west)", ok, l, r, eq, sh, ms);
    {
        double ls, rs;
        uwst_pathing(0, 1.0f);
        hear(16.0f, 11.5f, 12.0f, 11.5f, &ls, &rs, eq, sh, &ms);
        printf("  the same with the paths off: left %.4f right %.4f\n", ls, rs);
        check("in sight not twice", l + r < 1.25 * (ls + rs));
        uwst_pathing(1, 1.0f);
        uwst_simulate();
    }

    /* how the level falls with the way's length: the listener further into B */
    for (int k = 0; k < 3; k++) {
        float lz = 14.0f + 3.0f * k;
        ok = hear(25.0f, lz, 12.0f, 11.5f, &l, &r, eq, sh, &ms);
        snprintf(p, sizeof p, "the listener at 25, %.0f", lz);
        show(p, ok, l, r, eq, sh, ms);
    }

    rooms(1);
    uwst_simulate();
    wait_bake("door shut (no new bake wanted)");
    ok = hear(20.0f, 18.0f, 12.0f, 11.5f, &l, &r, eq, sh, &ms);
    show("door shut, out of sight", ok, l, r, eq, sh, ms);
    check("shut door", l + r < 3 * (l0 + r0));

    if (argc > 2) {   /* a real level: the bake's time, a block's time with eight sounds */
        static UwaTile lv[64 * 64];
        char line[200];
        int n = 0, x, z, type, h, wall, flr, blocks = 0, open = 0;
        float at[8][3];
        f = fopen(argv[2], "r");
        while (f && fgets(line, sizeof line, f)) {
            if (!strncmp(line, "acoustic", 8) && ++blocks > 1) break;
            if (sscanf(line, "tile %d %d %d %d %d %d", &x, &z, &type, &h, &wall, &flr) == 6 && x >= 0 && x < 64 && z >= 0 && z < 64) {
                lv[z * 64 + x].type = (unsigned char)type, lv[z * 64 + x].height = (unsigned char)h, n++;
                if (type != UWAT_SOLID) {
                    open++;
                }
            }
        }
        if (f) fclose(f);
        if (n == 4096 && open > 320) {
            {   /* the listener on an open tile near the middle, seven sounds on open tiles within 6 of it */
                int k = 0, lx = 0, lz = 0;
                for (int z = 28; z < 40 && !lx; z++)
                    for (int x = 28; x < 40 && !lx; x++) if (lv[z * 64 + x].type == UWAT_OPEN && lv[z * 64 + x].height < 12) lx = x, lz = z;
                at[0][0] = lx + 0.5f, at[0][1] = lv[lz * 64 + lx].height * 0.25f, at[0][2] = lz + 0.5f;
                for (int z = lz - 6; z <= lz + 6 && k < 7; z += 2)
                    for (int x = lx - 6; x <= lx + 6 && k < 7; x += 3)
                        if ((x != lx || z != lz) && lv[z * 64 + x].type == UWAT_OPEN && lv[z * 64 + x].height < 12)
                            k++, at[k][0] = x + 0.5f, at[k][1] = lv[z * 64 + x].height * 0.25f, at[k][2] = z + 0.5f;
                for (; k < 7; k++) at[k + 1][0] = at[0][0], at[k + 1][1] = at[0][1], at[k + 1][2] = at[0][2];
                printf("  the listener at %.1f, %.1f\n", at[0][0], at[0][2]);
            }
            uwst_level(lv, STONE, NULL, 0, 2.4f);
            snprintf(p, sizeof p, "the level's bake (%d open tiles)", open);
            wait_bake(p);
            uwst_listener(at[0][0], at[0][1] + 0.64f, at[0][2], 0);
            for (int k = 1; k < 8; k++) uwst_begin(at[k][0], at[k][1] + 0.3f, at[k][2], tone, RATE, RATE, 1.0f);
            uwst_simulate();
            double t0 = now_ms(), worst = 0;
            for (int b = 0; b < 40; b++) {
                double a = now_ms();
                uwst_mix_float(blk, UWST_FRAME, 0.0f, 1);
                a = now_ms() - a;
                if (a > worst) worst = a;
            }
            printf("  the level, seven sounds within 6 tiles: a block %.2f ms mean, %.2f ms worst (a block lasts %.1f ms)\n",
                   (now_ms() - t0) / 40, worst, UWST_FRAME * 1000.0 / RATE);
            for (int k = 0; k < 7; k++) {
                int got = uwst_pathing_heard(k, eq, sh);
                printf("    voice %d: paths %d eq %.3f %.3f %.3f sh0 %.4f\n", k, got, eq[0], eq[1], eq[2], sh[0]);
            }
            uwst_pathing(0, 1.0f);
            t0 = now_ms();
            for (int b = 0; b < 40; b++) uwst_mix_float(blk, UWST_FRAME, 0.0f, 1);
            printf("  the same with the paths off: a block %.2f ms mean\n", (now_ms() - t0) / 40);
        } else printf("  %s is not a level's grid (%d tiles, %d open)\n", argv[2], n, open);
    }
    uwst_shutdown();
    printf("%d failed\n", fails);
    return fails;
}
