/* uwsteam_test.c: port\win32\uwsteam.c, Steam Audio's reflections over our tile grid (the acoustics plan's Task 7).
   tools\fxcheck.py steam builds and runs it:
       uwsteam_test.exe BINDIR [DUMP]
   BINDIR holds phonon.dll (absent: "SKIP"). DUMP: the renderer's grid of a level (work\fx\grid-dump.txt): one
   simulation on it is timed. Nothing is played: the reflections are mixed into memory.
   The fixture is the acoustics' (tests\uwacoustic_test.c): room A 4 x 4 (x 10-13, z 10-13), room B 12 x 12 (x 15-26,
   z 10-21), the wall column x 14 between them with a doorway at (14, 11) holding a door. Cases:
     load       uwst_init 1 with phonon.dll; 0 and "fx: acoustics high off (no phonon.dll)" from a folder without it
     scene      the fixture's tiles become triangles (some thousands); a door shut adds two
     tail       a half second of 440 Hz in stone room B: its reflections ring on after it; in carpeted room A far less
     side       the source one tile from B's east wall, the listener in the middle facing north: the first echoes
                are louder in the right ear
     next room  the source in A, the door open, the listener in B: the reflections are heard there
     drain      some seconds after the sound no voice is busy
     swap       the level given again twenty times between simulations: no crash, the scene stands
   One PASS / FAIL line each; the exit code is the failures. */
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
static void check(const char *name, int ok, const char *why)
{
    printf("%s %s%s%s\n", name, ok ? "PASS" : "FAIL", ok ? "" : ": ", ok ? "" : why);
    fails += !ok;
}
static UwaTile grid[64 * 64];
static void rooms(int a_mat, int ceil_mat, int door_shut)
{
    UwaDoor door = {1, 14, 11, 2, WOOD, door_shut};
    for (int i = 0; i < 64 * 64; i++) grid[i].type = UWAT_SOLID, grid[i].height = 15, grid[i].wall_mat = grid[i].floor_mat = STONE;
    for (int z = 9; z <= 14; z++)
        for (int x = 9; x <= 14; x++) grid[z * 64 + x].wall_mat = (unsigned char)a_mat;
    for (int z = 10; z <= 13; z++)
        for (int x = 10; x <= 13; x++) grid[z * 64 + x].type = UWAT_OPEN, grid[z * 64 + x].height = 0, grid[z * 64 + x].floor_mat = (unsigned char)a_mat;
    for (int z = 10; z <= 21; z++)
        for (int x = 15; x <= 26; x++) grid[z * 64 + x].type = UWAT_OPEN, grid[z * 64 + x].height = 0;
    grid[11 * 64 + 14].type = UWAT_OPEN, grid[11 * 64 + 14].height = 0;
    uwst_level(grid, ceil_mat, &door, 1, 2.4f);
}
static short tone[RATE / 2];
static short buf[RATE * 2 * 2];
/* the next ms of the reflections: the RMS of each ear */
static int span(int ms, double *l, double *r)
{
    int n = RATE / 1000 * ms, busy = uwst_mix(buf, n, 1.0f, 1);
    double sl = 0, sr = 0;
    for (int i = 0; i < n; i++) sl += (double)buf[2 * i] * buf[2 * i], sr += (double)buf[2 * i + 1] * buf[2 * i + 1];
    if (l) *l = sqrt(sl / n);
    if (r) *r = sqrt(sr / n);
    return busy;
}
static void silence(void)   /* until every voice has rung out (6 s at most) */
{
    for (int k = 0; k < 12 && span(500, NULL, NULL); k++) {}
}
/* the tone from (sx, sz) heard at (lx, lz) facing north: the reflections' RMS (both ears' mean) while it sounds
   (*during, its first 80 ms each ear in *l80 / *r80) and from 0.2 to 0.5 s after it ends (returned) */
static double ring(float lx, float lz, float sx, float sz, double *during, double *l80, double *r80)
{
    double l, r, a, b;
    uwst_listener(lx, 1.0f, lz, 0.0f);
    int v = uwst_begin(sx, 1.0f, sz, tone, RATE / 2, RATE, 1.0f);
    uwst_simulate();
    span(80, &l, &r);
    if (l80) *l80 = l;
    if (r80) *r80 = r;
    span(420, &a, &b);
    if (during) *during = (a + b) / 2;
    span(200, NULL, NULL);
    span(300, &a, &b);
    (void)v;
    silence();
    return (a + b) / 2;
}

int main(int argc, char **argv)
{
    char why[400], p[1024];
    double d, l, r;
    if (argc < 2) { printf("usage: uwsteam_test BINDIR [DUMP]\n"); return 99; }
    snprintf(p, sizeof p, "%s\\phonon.dll", argv[1]);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) { printf("SKIP: no phonon.dll in %s\n", argv[1]); return 0; }
    {   /* the cases' own table, in a folder beside the test (2026-11-14: not the shipped one, whose stone became rough
           rock; "tail" compares a hard room with a soft one, so its stone is dressed stone whatever we ship) */
        static const char *TABLE =
            "preset stone   absorb 0.02 0.03 0.04  transmit 0.015 0.002 0.001\n"
            "preset wood    absorb 0.10 0.08 0.10  transmit 0.070 0.014 0.005\n"
            "preset carpet  absorb 0.05 0.25 0.55  transmit 0.00\n"
            "preset metal   absorb 0.02 0.02 0.03  transmit 0.018 0.004 0.001\n"
            "wall * stone\nfloor * stone\nceiling * stone\ndoor * wood\n";
        char dir[1024], *e;
        FILE *f;
        GetModuleFileNameA(NULL, dir, sizeof dir - 32);
        if ((e = strrchr(dir, '\\')) != NULL) *e = 0;
        snprintf(p, sizeof p, "%s\\acoustics.txt", dir);
        if (!(f = fopen(p, "wb"))) { printf("FAIL: cannot write %s\n", p); return 1; }
        fputs(TABLE, f), fclose(f);
        if (!uwacmat_load(dir)) { printf("FAIL: the cases' materials did not load\n"); return 1; }
    }
    for (int i = 0; i < RATE / 2; i++) tone[i] = (short)(12000.0 * sin(6.2831853 * 440.0 * i / RATE));

    int none = uwst_init("X:\\none", RATE), ok = uwst_init(argv[1], RATE);
    snprintf(why, sizeof why, "a folder without it %d, the real one %d, on %d", none, ok, uwst_on());
    check("load", none == 0 && ok == 1 && uwst_on() == 1, why);
    if (!ok) return fails;

    rooms(STONE, STONE, 0);
    uwst_simulate();
    int open_tris = uwst_triangles();
    rooms(STONE, STONE, 1);
    uwst_simulate();
    snprintf(why, sizeof why, "%d triangles with the door open, %d with it shut", open_tris, uwst_triangles());
    printf("  scene: %s\n", why);
    check("scene", open_tris > 500 && uwst_triangles() == open_tris + 2, why);

    double tail_b = ring(21.0f, 16.0f, 22.0f, 16.0f, &d, NULL, NULL), during_b = d;
    rooms(CARPET, CARPET, 1);
    uwst_simulate();
    double tail_a = ring(12.0f, 12.0f, 13.0f, 12.0f, &d, NULL, NULL);
    snprintf(why, sizeof why, "stone hall B: the reflections %.1f while it sounds, %.1f from 0.2 to 0.5 s after; carpeted room A: %.1f and %.1f", during_b, tail_b, d, tail_a);
    printf("  tail: %s\n", why);
    check("tail", during_b > 20 && tail_b > 5 && tail_b > 2 * tail_a, why);

    rooms(STONE, STONE, 1);
    uwst_simulate();
    ring(21.0f, 16.0f, 26.0f, 16.0f, &d, &l, &r);
    snprintf(why, sizeof why, "the first 80 ms: left %.1f, right %.1f", l, r);
    printf("  side: %s\n", why);
    check("side", r > 1.1 * l && r > 5, why);

    rooms(STONE, STONE, 0);
    uwst_simulate();
    double through = ring(17.0f, 11.5f, 12.0f, 11.5f, &d, NULL, NULL);
    snprintf(why, sizeof why, "a sound in A heard in B through the open door: the reflections %.1f while it sounds, %.1f after", d, through);
    printf("  next room: %s\n", why);
    check("next room", d > 5, why);

    uwst_listener(21.0f, 1.0f, 16.0f, 0.0f);
    uwst_begin(22.0f, 1.0f, 16.0f, tone, RATE / 2, RATE, 1.0f);
    uwst_simulate();
    int busy0 = span(500, NULL, NULL), busy = busy0, ms = 500;
    for (; busy && ms < 6000; ms += 250) busy = span(250, NULL, NULL);
    snprintf(why, sizeof why, "%d busy while it sounds, none after %d ms", busy0, ms);
    check("drain", busy0 >= 1 && busy == 0 && ms <= 4000, why);

    for (int i = 0; i < 20; i++) {
        rooms(i & 1 ? CARPET : STONE, STONE, i & 1);
        uwst_begin(22.0f, 1.0f, 16.0f, tone, RATE / 8, RATE, 1.0f);
        uwst_simulate();
        span(40, NULL, NULL);
    }
    silence();
    snprintf(why, sizeof why, "%d triangles after twenty levels", uwst_triangles());
    check("swap", uwst_triangles() > 500, why);

    if (argc > 2) {   /* a real level's grid, every material stone: one simulation with one voice timed */
        static UwaTile lv[64 * 64];
        char line[200];
        int n = 0, x, z, type, h, wall, flr, blocks = 0;
        float at[3] = {0, 0, 0};
        FILE *f = fopen(argv[2], "r");
        while (f && fgets(line, sizeof line, f)) {
            if (!strncmp(line, "acoustic", 8) && ++blocks > 1) break;
            if (sscanf(line, "tile %d %d %d %d %d %d", &x, &z, &type, &h, &wall, &flr) == 6 && x >= 0 && x < 64 && z >= 0 && z < 64) {
                lv[z * 64 + x].type = (unsigned char)type, lv[z * 64 + x].height = (unsigned char)h, n++;
                if (type == UWAT_OPEN && h < 12 && !at[0]) at[0] = x + 0.5f, at[1] = h * 0.25f + 1.0f, at[2] = z + 0.5f;
            }
        }
        if (f) fclose(f);
        if (n == 4096 && at[0]) {
            LARGE_INTEGER a, b, q;
            uwst_level(lv, STONE, NULL, 0, 2.4f);
            uwst_simulate();
            uwst_listener(at[0], at[1], at[2], 0);
            uwst_begin(at[0], at[1], at[2], tone, RATE / 2, RATE, 1.0f);
            QueryPerformanceFrequency(&q), QueryPerformanceCounter(&a);
            uwst_simulate();
            QueryPerformanceCounter(&b);
            double sim_ms = (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)q.QuadPart;
            QueryPerformanceCounter(&a);
            for (int i = 0; i < 20; i++) uwst_mix(buf, UWST_FRAME, 1.0f, 1);
            QueryPerformanceCounter(&b);
            printf("timing: the level's %d triangles; one simulation with one voice %.1f ms; one block of %d frames mixed %.2f ms (a block lasts %.1f ms)\n",
                   uwst_triangles(), sim_ms, UWST_FRAME, (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)q.QuadPart / 20, UWST_FRAME * 1000.0 / RATE);
        } else printf("timing: %s is not a level's grid\n", argv[2]);
    }
    uwst_shutdown();
    return fails;
}
