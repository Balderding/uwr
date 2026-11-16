/* uwacoustic_test.c: port\win32\uwacoustic.c, our sound rays over the tile grid (the acoustics plan's Tasks 3 and 4).
   tools\fxcheck.py acoustics builds and runs it:
       uwacoustic_test.exe [DUMP]
   DUMP: the renderer's grid of a level (UWVIEW_ACOUSTIC_DUMP's file, work\fx\grid-dump.txt): one uwac_room on it is
   timed. The fixture, built here (all stone unless said; floors at height 0, so y 0; the ceiling at 4.0):
     room A      4 x 4 tiles, x 10-13, z 10-13
     room B      12 x 12, x 15-26, z 10-21; the tile column x 14 between A and B is their shared wall, with a
                 doorway tile at (14, 11) holding a wood door across it (heading 2: spanning z)
     corridor    1 x 12, x 30, z 10-21, with an arm east from its north end (x 31-36, z 21): an L
     steps       (40, 10) height 2, (41, 10) height 8, (42, 10) height 0
     diagonal    (45, 10) T_DIAG_SE (its open half the south-east one), (46, 10) open
     corner      (0, 0) open
   Cases, one PASS / FAIL line each; the exit code is the failures:
     ray wall, ray floor, ray ceiling, ray step, ray door, ray diagonal, inside, edge, room size, room mfp,
     room material, room open door (Task 3); path clear, path rock, path door, path own door, path corner, path here (Task 4);
     detour straight, detour corner, detour door, detour step, waypoint, path round (Task 5). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../port/win32/uwacoustic.h"

static int fails;
static void check(const char *name, int ok, const char *why)
{
    printf("%s %s%s%s\n", name, ok ? "PASS" : "FAIL", ok ? "" : ": ", ok ? "" : why);
    fails += !ok;
}

enum { STONE, WOOD, CARPET, METAL };
static const char *TABLE =
    "preset stone   absorb 0.02 0.03 0.04  transmit 0.015 0.002 0.001\n"
    "preset wood    absorb 0.10 0.08 0.10  transmit 0.070 0.014 0.005\n"
    "preset carpet  absorb 0.05 0.25 0.55  transmit 0.00\n"
    "preset metal   absorb 0.02 0.02 0.03  transmit 0.018 0.004 0.001\n"
    "wall * stone\nfloor * stone\nceiling * stone\ndoor * wood\n";

static UwaTile grid[64 * 64];
static UwaDoor door = {1, 14, 11, 2, WOOD, 1};
static void open_tile(int x, int z, int type, int height, int mat)
{
    UwaTile *t = &grid[z * 64 + x];
    t->type = (unsigned char)type, t->height = (unsigned char)height, t->wall_mat = t->floor_mat = (unsigned char)mat;
}
static void fixture(int a_mat, int door_shut, int door_mat)
{
    for (int i = 0; i < 64 * 64; i++) grid[i].type = UWAT_SOLID, grid[i].height = 15, grid[i].wall_mat = grid[i].floor_mat = STONE;
    for (int z = 10; z <= 13; z++)
        for (int x = 10; x <= 13; x++) open_tile(x, z, UWAT_OPEN, 0, a_mat);
    if (a_mat != STONE)   /* the walls round A too (a wall's material is the rock tile's) */
        for (int z = 9; z <= 14; z++)
            for (int x = 9; x <= 14; x++)
                if (grid[z * 64 + x].type == UWAT_SOLID) grid[z * 64 + x].wall_mat = (unsigned char)a_mat;
    for (int z = 10; z <= 21; z++)
        for (int x = 15; x <= 26; x++) open_tile(x, z, UWAT_OPEN, 0, STONE);
    open_tile(14, 11, UWAT_OPEN, 0, STONE);
    for (int z = 10; z <= 21; z++) open_tile(30, z, UWAT_OPEN, 0, STONE);
    for (int x = 31; x <= 36; x++) open_tile(x, 21, UWAT_OPEN, 0, STONE);   /* the corridor's east arm at its north end: an L */
    open_tile(40, 10, UWAT_OPEN, 2, STONE), open_tile(41, 10, UWAT_OPEN, 8, STONE), open_tile(42, 10, UWAT_OPEN, 0, STONE);
    open_tile(45, 10, UWAT_DIAG_SE, 0, STONE), open_tile(46, 10, UWAT_OPEN, 0, STONE);
    open_tile(0, 0, UWAT_OPEN, 0, STONE);
    uwac_grid(grid, a_mat, 2.4f);
    door.shut = door_shut, door.mat = door_mat;
    uwac_doors(&door, 1);
}
static int about(float a, float b) { return fabsf(a - b) <= 0.01f; }

static int write_table(const char *dir)
{
    char p[600];
    CreateDirectoryA(dir, NULL);
    snprintf(p, sizeof p, "%s\\acoustics.txt", dir);
    FILE *f = fopen(p, "wb");
    if (!f) return 0;
    fputs(TABLE, f);
    fclose(f);
    return uwacmat_load(dir);
}

/* the renderer's dump (the first level in it), every material stone: one room probe timed */
static void timing(const char *dump)
{
    static UwaTile lv[64 * 64];
    char line[200];
    int n = 0, x, z, type, h, wall, flr, blocks = 0;
    float at[3] = {0, 0, 0};
    FILE *f = fopen(dump, "r");
    if (!f) { printf("timing: no %s\n", dump); return; }
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "acoustic", 8) && ++blocks > 1) break;
        if (sscanf(line, "tile %d %d %d %d %d %d", &x, &z, &type, &h, &wall, &flr) == 6 && x >= 0 && x < 64 && z >= 0 && z < 64) {
            UwaTile *t = &lv[z * 64 + x];
            t->type = (unsigned char)type, t->height = (unsigned char)h, t->wall_mat = t->floor_mat = STONE;
            if (type == UWAT_OPEN && !at[0]) at[0] = x + 0.5f, at[1] = h * 0.25f + 1.0f, at[2] = z + 0.5f;
            n++;
        }
    }
    fclose(f);
    if (n != 4096 || !at[0]) { printf("timing: %s is not a level's grid (%d tiles)\n", dump, n); return; }
    uwac_grid(lv, STONE, 2.4f);
    uwac_doors(NULL, 0);
    UwaRoom r;
    LARGE_INTEGER a, b, q;
    int ok = 0;
    QueryPerformanceFrequency(&q), QueryPerformanceCounter(&a);
    for (int i = 0; i < 1000; i++) ok += uwac_room(at, &r);
    QueryPerformanceCounter(&b);
    printf("timing: one uwac_room on the level's grid at %.1f %.2f %.1f: %.4f ms (1000 probes, %d good; rt60 %.2f %.2f %.2f s, mean free path %.1f m)\n",
           at[0], at[1], at[2], (double)(b.QuadPart - a.QuadPart) * 1000.0 / (double)q.QuadPart / 1000.0, ok, r.rt60[0], r.rt60[1], r.rt60[2], r.mfp_m);
}

int main(int argc, char **argv)
{
    char why[400], dir[600];
    UwaHit h, h2;
    UwaRoom ra, rb, rc, rk, ro;
    snprintf(dir, sizeof dir, "%s", argc > 2 ? argv[2] : ".");
    if (!write_table(dir)) { printf("the fixture's acoustics.txt could not be written or read in %s\n", dir); return 99; }

    fixture(STONE, 1, WOOD);
    const float E[3] = {1, 0, 0}, W[3] = {-1, 0, 0}, UP[3] = {0, 1, 0}, DOWN[3] = {0, -1, 0};
    float a_mid[3] = {12.0f, 1.0f, 12.0f};
    int r = uwac_ray(a_mid, E, 16, &h);
    snprintf(why, sizeof why, "returned %d: dist %.3f, kind %d, mat %d", r, h.dist, h.kind, h.mat);
    check("ray wall", r == 1 && about(h.dist, 2.0f) && h.kind == UWA_WALL && h.mat == STONE, why);

    float on2[3] = {40.5f, 1.0f, 10.5f};
    r = uwac_ray(on2, DOWN, 16, &h);
    snprintf(why, sizeof why, "returned %d: dist %.3f, kind %d", r, h.dist, h.kind);
    check("ray floor", r == 1 && about(h.dist, 0.5f) && h.kind == UWA_FLOOR, why);
    r = uwac_ray(on2, UP, 16, &h);
    snprintf(why, sizeof why, "returned %d: dist %.3f, kind %d", r, h.dist, h.kind);
    check("ray ceiling", r == 1 && about(h.dist, 3.0f) && h.kind == UWA_CEILING, why);

    float hi[3] = {40.5f, 3.0f, 10.5f};
    r = uwac_ray(on2, E, 16, &h);
    int r2 = uwac_ray(hi, E, 16, &h2);
    snprintf(why, sizeof why, "at y 1.0: %d, dist %.3f, kind %d; at y 3.0: %d, dist %.3f, kind %d", r, h.dist, h.kind, r2, h2.dist, h2.kind);
    check("ray step", r == 1 && about(h.dist, 0.5f) && h.kind == UWA_WALL && r2 == 1 && about(h2.dist, 2.5f) && h2.kind == UWA_WALL, why);

    float a_door[3] = {12.0f, 1.0f, 11.5f};
    r = uwac_ray(a_door, E, 16, &h);
    fixture(STONE, 0, WOOD);
    r2 = uwac_ray(a_door, E, 32, &h2);
    snprintf(why, sizeof why, "shut: %d, dist %.3f, kind %d, mat %d; open: %d, dist %.3f, kind %d", r, h.dist, h.kind, h.mat, r2, h2.dist, h2.kind);
    check("ray door", r == 1 && about(h.dist, 2.5f) && h.kind == UWA_DOOR && h.mat == WOOD && r2 == 1 && about(h2.dist, 15.0f) && h2.kind == UWA_WALL, why);
    fixture(STONE, 1, WOOD);

    float dg[3] = {46.5f, 1.0f, 10.25f};
    r = uwac_ray(dg, W, 16, &h);
    snprintf(why, sizeof why, "returned %d: dist %.3f, kind %d", r, h.dist, h.kind);
    check("ray diagonal", r == 1 && about(h.dist, 1.25f) && h.kind == UWA_WALL, why);

    float rock[3] = {5.5f, 1.0f, 5.5f}, under[3] = {12.0f, -0.5f, 12.0f}, out[3] = {-3.0f, 1.0f, 12.0f};
    r = uwac_ray(rock, E, 16, &h);
    snprintf(why, sizeof why, "in rock: ray %d, room %d; under the floor: ray %d; outside the map: ray %d, room %d", r, uwac_room(rock, &ra),
             uwac_ray(under, E, 16, &h), uwac_ray(out, E, 16, &h), uwac_room(out, &ra));
    check("inside", r == -1 && uwac_room(rock, &ra) == 0 && uwac_ray(under, E, 16, &h) == -1 && uwac_ray(out, E, 16, &h) == -1 && uwac_room(out, &ra) == 0, why);

    float c0[3] = {0.5f, 1.0f, 0.5f};
    const float SW[3] = {-1, 0, -1};
    r = uwac_ray(c0, W, 16, &h);
    r2 = uwac_ray(c0, SW, 16, &h2);
    snprintf(why, sizeof why, "west: %d, dist %.3f, kind %d; south-west: %d, dist %.3f; the room there %d", r, h.dist, h.kind, r2, h2.dist, uwac_room(c0, &ra));
    check("edge", r == 1 && about(h.dist, 0.5f) && h.kind == UWA_WALL && r2 == 1 && about(h2.dist, 0.7071f) && uwac_room(c0, &ra) == 1, why);

    float b_mid[3] = {21.0f, 1.0f, 16.0f}, c_mid[3] = {30.5f, 1.0f, 16.0f};
    int oa = uwac_room(a_mid, &ra), ob = uwac_room(b_mid, &rb), oc = uwac_room(c_mid, &rc);
    snprintf(why, sizeof why, "A %d: rt60 %.2f %.2f %.2f s, mfp %.2f m; B %d: %.2f %.2f %.2f, %.2f m; the corridor %d: %.2f %.2f %.2f, %.2f m",
             oa, ra.rt60[0], ra.rt60[1], ra.rt60[2], ra.mfp_m, ob, rb.rt60[0], rb.rt60[1], rb.rt60[2], rb.mfp_m, oc, rc.rt60[0], rc.rt60[1], rc.rt60[2], rc.mfp_m);
    printf("  %s\n", why);
    check("room size", oa && ob && oc && rb.rt60[1] > ra.rt60[1] && rc.rt60[1] < rb.rt60[1] && ra.hits == 64, why);

    /* the room's own mean free path, 4 V / S, wherever the point stands in it (A: 4 x 4 tiles, 4 high: 4 * 64 / 96 =
       2.67 tiles, 6.4 m): from the middle and from near a corner alike, within a tenth */
    UwaRoom rn;
    float a_corner[3] = {10.3f, 0.4f, 13.7f};
    int on = uwac_room(a_corner, &rn);
    snprintf(why, sizeof why, "A's middle: mfp %.2f m (first surface %.2f m); near its corner %d: mfp %.2f m (first %.2f m); 4V/S 6.40 m",
             ra.mfp_m, ra.first_m, on, rn.mfp_m, rn.first_m);
    printf("  %s\n", why);
    check("room mfp", on && fabsf(ra.mfp_m - 6.4f) < 0.64f && fabsf(rn.mfp_m - 6.4f) < 0.64f && rn.first_m < ra.first_m, why);

    fixture(CARPET, 1, WOOD);
    int ok = uwac_room(a_mid, &rk);
    snprintf(why, sizeof why, "carpeted A %d: rt60 %.2f %.2f %.2f s against stone A's %.2f %.2f %.2f", ok, rk.rt60[0], rk.rt60[1], rk.rt60[2], ra.rt60[0], ra.rt60[1], ra.rt60[2]);
    printf("  %s\n", why);
    check("room material", ok && rk.rt60[2] < ra.rt60[2] / 4, why);

    fixture(STONE, 0, WOOD);
    ok = uwac_room(a_door, &ro);
    fixture(STONE, 1, WOOD);
    UwaRoom rs;
    int ok2 = uwac_room(a_door, &rs);
    snprintf(why, sizeof why, "A at the door's row, door open %d: mfp %.2f m; shut %d: %.2f m", ok, ro.mfp_m, ok2, rs.mfp_m);
    check("room open door", ok && ok2 && ro.mfp_m > rs.mfp_m, why);

    /* Task 4: what stands between a sound and the listener (uwac_path: from the source, to the listener) */
    float g[3], lis[3] = {12.5f, 1.0f, 11.5f}, same[3] = {11.0f, 0.0f, 12.5f}, behind[3] = {15.5f, 1.0f, 11.5f};
    uwac_path(same, lis, g);
    snprintf(why, sizeof why, "a thing on the floor of the same room: %.3f %.3f %.3f", g[0], g[1], g[2]);
    check("path clear", g[0] == 1.0f && g[1] == 1.0f && g[2] == 1.0f, why);
    fixture(STONE, 1, STONE);            /* the door made stone: as the wall */
    float wall[3] = {16.5f, 1.0f, 13.0f};
    uwac_path(wall, lis, g);
    float gs[3];
    uwac_path(behind, lis, gs);
    snprintf(why, sizeof why, "behind the shared wall: %.3f %.3f %.3f; behind the stone door: %.3f %.3f %.3f", g[0], g[1], g[2], gs[0], gs[1], gs[2]);
    printf("  %s\n", why);
    check("path rock", about(g[0], 0.15f) && about(g[1], 0.10f) && about(g[2], 0.03f) && gs[1] <= 0.10f + 0.01f, why);
    fixture(STONE, 1, WOOD);
    uwac_path(behind, lis, g);
    snprintf(why, sizeof why, "behind the shut wood door: %.3f %.3f %.3f", g[0], g[1], g[2]);
    printf("  %s\n", why);
    check("path door", g[1] > 0.03f && g[1] < 0.3f && g[2] < g[1] && g[1] < g[0] && g[1] > 0.10f + 0.005f, why);
    /* the shut door's own sound (on the door, a little to its west side) heard from the east: not behind itself */
    float creak[3] = {14.44f, 1.0f, 11.5f}, east[3] = {16.5f, 1.0f, 11.5f};
    uwac_path(creak, east, gs);
    uwac_path(creak, lis, g);
    snprintf(why, sizeof why, "the shut door's own sound: from the east %.3f %.3f %.3f, from the west %.3f %.3f %.3f", gs[0], gs[1], gs[2], g[0], g[1], g[2]);
    check("path own door", gs[0] == 1.0f && gs[1] == 1.0f && gs[2] == 1.0f && g[1] == 1.0f, why);
    fixture(STONE, 0, WOOD);             /* the door open: a source in B half hidden by the doorway's corner */
    float lis2[3] = {13.5f, 1.0f, 11.5f}, half[3] = {15.6f, 1.0f, 12.1f};
    uwac_path(half, lis2, g);
    uwac_path(behind, lis, gs);
    snprintf(why, sizeof why, "half hidden: %.3f %.3f %.3f; through the open door: %.3f %.3f %.3f", g[0], g[1], g[2], gs[0], gs[1], gs[2]);
    printf("  %s\n", why);
    check("path corner", g[1] > 0.3f && g[1] < 0.9f && gs[1] == 1.0f, why);
    fixture(STONE, 1, STONE);
    float here[3] = {12.55f, 1.0f, 11.5f}, inrock[3] = {5.5f, 1.0f, 5.5f};
    uwac_path(here, lis, g);
    uwac_path(inrock, lis, gs);
    snprintf(why, sizeof why, "0.05 tile away: %.3f %.3f %.3f; a source in rock: %.3f %.3f %.3f", g[0], g[1], g[2], gs[0], gs[1], gs[2]);
    check("path here", g[1] == 1.0f && gs[0] == 1.0f && gs[1] == 1.0f && gs[2] == 1.0f, why);
    fixture(STONE, 1, WOOD);

    /* Task 5: the way round */
    float c_s[3] = {30.5f, 1.0f, 12.5f}, c_n[3] = {30.5f, 1.0f, 18.5f}, c_l[3] = {30.5f, 1.0f, 19.5f}, arm[3] = {32.5f, 1.0f, 21.5f}, far_arm[3] = {36.5f, 1.0f, 21.5f};
    float ds = uwac_detour(c_n, c_s), dc = uwac_detour(arm, c_l), dfar = uwac_detour(far_arm, c_s);
    snprintf(why, sizeof why, "along the corridor %.2f; round the L's corner %.2f; from its far end to the arm's end (15 tiles walked) %.2f", ds, dc, dfar);
    printf("  %s\n", why);
    check("detour straight", ds >= 0 && ds < 0.5f, why);
    check("detour corner", dc > 0.5f && dc < 2.0f && dfar == -1.0f, why);
    float dshut = uwac_detour(behind, lis);
    fixture(STONE, 0, WOOD);
    float dopen = uwac_detour(behind, lis);
    fixture(STONE, 1, WOOD);
    float down = uwac_detour(creak, east);
    snprintf(why, sizeof why, "from B into A: the door shut %.2f, open %.2f; the shut door's own sound into B %.2f", dshut, dopen, down);
    check("detour door", dshut == -1.0f && dopen >= 0 && dopen < 0.5f && down >= 0, why);
    float lo[3] = {40.5f, 1.0f, 10.5f}, hi8[3] = {41.5f, 2.5f, 10.5f}, past[3] = {42.5f, 1.0f, 10.5f};
    float up = uwac_detour(lo, hi8), dn = uwac_detour(hi8, lo), over = uwac_detour(lo, past);
    snprintf(why, sizeof why, "from height 2 up to height 8 %.2f; from 8 down to 2 %.2f; past the height-8 tile %.2f", up, dn, over);
    check("detour step", up == -1.0f && dn >= 0 && over == -1.0f, why);
    float wp[3] = {0, 0, 0}, wp2[3] = {0, 0, 0};
    int w1 = uwac_waypoint(arm, c_l, wp), w2 = uwac_waypoint(c_n, c_s, wp2), w3 = uwac_waypoint(behind, lis, wp2);
    snprintf(why, sizeof why, "round the L: %d, from (%.2f, %.2f, %.2f); in plain sight: %d; no way: %d", w1, wp[0], wp[1], wp[2], w2, w3);
    check("waypoint", w1 == 1 && about(wp[0], 30.5f) && about(wp[2], 21.5f) && about(wp[1], 1.0f) && w2 == 1 && w3 == 0, why);
    float g5[3], g4[3];
    uwac_path(arm, c_l, g4);
    int raised = uwac_path_round(arm, c_l, g5), raised2 = uwac_path_round(behind, lis, gs);
    snprintf(why, sizeof why, "round the L: straight %.3f %.3f %.3f, with the way round %.3f %.3f %.3f (%d); behind the shut door %.3f %.3f %.3f (%d)",
             g4[0], g4[1], g4[2], g5[0], g5[1], g5[2], raised, gs[0], gs[1], gs[2], raised2);
    printf("  %s\n", why);
    check("path round", raised == 1 && g5[1] > g4[1] + 0.2f && g5[1] < 1.0f && g5[2] < g5[1] && g5[1] < g5[0] && raised2 == 0 && about(gs[1], 0.118f), why);

    if (argc > 1) timing(argv[1]);
    return fails;
}
