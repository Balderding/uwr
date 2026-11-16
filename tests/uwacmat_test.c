/* uwacmat_test.c: port\win32\uwacmat.c, the acoustic materials table (the acoustics plan's Task 1). tools\fxcheck.py
   materials builds and runs it:
       uwacmat_test.exe WORKDIR
   WORKDIR gets the fixture folders (ok\, unknown\, badline\, none\, fresh\), each with its own acoustics.txt. Cases:
     count      the plan's ten lines plus "wall 012 wood": six presets
     wall       wall 12 is wood, wall 13 is stone (the '*'), a number past every texture is the '*' too
     door       door item 0x140 is wood (the '*'); wood's transmit is its three bands, 0.07 0.014 0.005
     values     stone's three bands and wood's as written; step 0 when absent, 3 when "step 3" is written; a
                transmit written as one number is that number in all three bands (two numbers: a bad line)
     last       a '*' line after a numbered line takes the number back; a numbered line after it sets it again
     doors      uwacmat_door: the item's line, else the picture's, else the '*'
     ceiling    a ceiling is a floor picture: its own line, else the floor's line for that picture, else the '*'
     unknown    a texture line naming a preset that was never defined: load returns 0, nothing is loaded
     badline    a line that is neither a preset nor a kind: load returns 0
     missing    no acoustics.txt in the folder: load returns 0
     default    the table in the code loads, has the six presets by name, and every texture resolves
     write      uwacmat_write_default writes the file when missing (1), leaves it alone when there (0), and the
                written file is the default text byte for byte
   Prints one PASS / FAIL line a case; the exit code is the failures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include "../port/win32/uwacmat.h"

static int fails;
static void check(const char *name, int ok, const char *why)
{
    printf("%s %s%s%s\n", name, ok ? "PASS" : "FAIL", ok ? "" : ": ", ok ? "" : why);
    fails += !ok;
}
static const char *PLAN =
    "preset stone   absorb 0.02 0.03 0.04  transmit 0.00\n"
    "preset wood    absorb 0.10 0.08 0.10  transmit 0.07 0.014 0.005\n"
    "preset carpet  absorb 0.05 0.25 0.55  transmit 0.00\n"
    "preset metal   absorb 0.02 0.02 0.03  transmit 0.00\n"
    "preset water   absorb 0.01 0.01 0.02  transmit 0.00\n"
    "preset dirt    absorb 0.15 0.30 0.45  transmit 0.00\n"
    "wall * stone\n"
    "floor * stone\n"
    "ceiling * stone\n"
    "door * wood\n";
static char root[400];
/* WORKDIR\name\acoustics.txt holding the texts one after another (NULL: the folder alone); returns the folder */
static const char *fixture(const char *name, const char *a, const char *b)
{
    static char dir[4][512];
    static int k;
    char p[600], *d = dir[k++ & 3];
    snprintf(d, 512, "%s\\%s", root, name);
    _mkdir(d);
    snprintf(p, sizeof p, "%s\\acoustics.txt", d);
    remove(p);
    if (a) {
        FILE *f = fopen(p, "wb");
        if (!f) { printf("FAIL: cannot write %s\n", p); exit(99); }
        fputs(a, f);
        if (b) fputs(b, f);
        fclose(f);
    }
    return d;
}
static int named(int preset, const char *name)
{
    const UwaMaterial *m = uwacmat_get(preset);
    return m && !strcmp(m->name, name);
}

int main(int argc, char **argv)
{
    char why[300];
    if (argc < 2) { printf("usage: uwacmat_test WORKDIR\n"); return 99; }
    snprintf(root, sizeof root, "%s", argv[1]);
    _mkdir(root);

    int ok = uwacmat_load(fixture("ok", PLAN, "wall 012 wood\n"));
    snprintf(why, sizeof why, "load %d, count %d", ok, uwacmat_count());
    check("count", ok == 1 && uwacmat_count() == 6, why);
    snprintf(why, sizeof why, "wall 12 -> %d, wall 13 -> %d, wall 9999 -> %d", uwacmat_for(UWA_WALL, 12), uwacmat_for(UWA_WALL, 13), uwacmat_for(UWA_WALL, 9999));
    check("wall", named(uwacmat_for(UWA_WALL, 12), "wood") && named(uwacmat_for(UWA_WALL, 13), "stone") &&
          named(uwacmat_for(UWA_WALL, 9999), "stone") && named(uwacmat_for(UWA_WALL, -1), "stone"), why);
    const UwaMaterial *wood = uwacmat_get(uwacmat_for(UWA_DOOR, 0x140)), *stone = uwacmat_get(uwacmat_for(UWA_FLOOR, 0));
    snprintf(why, sizeof why, "door 0x140 -> %s, transmit %g %g %g", wood ? wood->name : "(none)", wood ? wood->transmit[0] : -1.0,
             wood ? wood->transmit[1] : -1.0, wood ? wood->transmit[2] : -1.0);
    check("door", wood && !strcmp(wood->name, "wood") && wood->transmit[0] == 0.07f && wood->transmit[1] == 0.014f && wood->transmit[2] == 0.005f, why);
    check("values", stone && wood && stone->absorb[0] == 0.02f && stone->absorb[1] == 0.03f && stone->absorb[2] == 0.04f &&
          stone->transmit[0] == 0.0f && stone->transmit[1] == 0.0f && stone->transmit[2] == 0.0f && wood->absorb[0] == 0.10f && wood->absorb[1] == 0.08f && wood->absorb[2] == 0.10f &&
          stone->step == 0 && !uwacmat_get(6) && !uwacmat_get(-1), "a preset's numbers are not the file's");

    ok = uwacmat_load(fixture("last", PLAN,
        "preset plank absorb 0.1 0.1 0.1 transmit 0.2 step 3\n"
        "preset mud absorb 0.1 0.1 0.1 transmit 0 step -5\n"
        "wall 7 wood\nwall * metal\nwall 8 dirt\n# a comment\n\nfloor 0x10 water   # sixteen\nfloor 010 carpet\n"));
    const UwaMaterial *plank = uwacmat_get(6);
    snprintf(why, sizeof why, "load %d: wall 7 -> %d, wall 8 -> %d, floor 16 -> %d, floor 10 -> %d, floor 8 -> %d", ok, uwacmat_for(UWA_WALL, 7),
             uwacmat_for(UWA_WALL, 8), uwacmat_for(UWA_FLOOR, 16), uwacmat_for(UWA_FLOOR, 10), uwacmat_for(UWA_FLOOR, 8));
    check("last", ok == 1 && uwacmat_count() == 8 && uwacmat_get(7)->step == -5 && named(uwacmat_for(UWA_WALL, 7), "metal") && named(uwacmat_for(UWA_WALL, 8), "dirt") &&
          named(uwacmat_for(UWA_FLOOR, 16), "water") && named(uwacmat_for(UWA_FLOOR, 10), "carpet") && named(uwacmat_for(UWA_FLOOR, 8), "stone") &&
          plank && plank->step == 3 && plank->transmit[0] == 0.2f && plank->transmit[1] == 0.2f && plank->transmit[2] == 0.2f, why);

    ok = uwacmat_load(fixture("doors", PLAN, "door 5 metal\ndoor 9 stone\ndoor 0x147 stone\n"));
    snprintf(why, sizeof why, "load %d: (0x140, 5) -> %d, (0x141, 0) -> %d, (0x147, 5) -> %d, (0x142, -1) -> %d, (0x143, 9) -> %d", ok,
             uwacmat_door(0x140, 5), uwacmat_door(0x141, 0), uwacmat_door(0x147, 5), uwacmat_door(0x142, -1), uwacmat_door(0x143, 9));
    check("doors", ok == 1 && named(uwacmat_door(0x140, 5), "metal") && named(uwacmat_door(0x141, 0), "wood") &&
          named(uwacmat_door(0x147, 5), "stone") && named(uwacmat_door(0x142, -1), "wood") && named(uwacmat_door(0x143, 9), "stone"), why);

    ok = uwacmat_load(fixture("ceiling", PLAN, "floor 6 dirt\nfloor 7 dirt\nceiling 7 carpet\nceiling * metal\nfloor 8 water\nceiling 9 wood\n"));
    snprintf(why, sizeof why, "load %d: ceiling 6 -> %d, 7 -> %d, 8 -> %d, 9 -> %d, 10 -> %d", ok, uwacmat_for(UWA_CEILING, 6),
             uwacmat_for(UWA_CEILING, 7), uwacmat_for(UWA_CEILING, 8), uwacmat_for(UWA_CEILING, 9), uwacmat_for(UWA_CEILING, 10));
    check("ceiling", ok == 1 && named(uwacmat_for(UWA_CEILING, 6), "dirt") && named(uwacmat_for(UWA_CEILING, 7), "dirt") &&
          named(uwacmat_for(UWA_CEILING, 8), "water") && named(uwacmat_for(UWA_CEILING, 9), "wood") &&
          named(uwacmat_for(UWA_CEILING, 10), "metal") && named(uwacmat_for(UWA_FLOOR, 9), "stone"), why);

    ok = uwacmat_load(fixture("unknown", PLAN, "wall 012 oak\n"));
    snprintf(why, sizeof why, "load %d, count %d", ok, uwacmat_count());
    check("unknown", ok == 0 && uwacmat_count() == 0 && !uwacmat_get(0), why);
    ok = uwacmat_load(fixture("badline", PLAN, "roof 3 stone\n"));
    int ok2 = uwacmat_load(fixture("badnum", PLAN, "wall twelve stone\n"));
    int ok3 = uwacmat_load(fixture("badpreset", "preset stone absorb 0.02 0.03 transmit 0\nwall * stone\n", NULL));
    int ok4 = uwacmat_load(fixture("badtransmit", "preset stone absorb 0.02 0.03 0.04 transmit 0.1 0.2\nwall * stone\n", NULL));
    snprintf(why, sizeof why, "an unknown kind %d, a number that is none %d, a preset short of a band %d, a transmit of two numbers %d", ok, ok2, ok3, ok4);
    check("badline", ok == 0 && ok2 == 0 && ok3 == 0 && ok4 == 0, why);
    ok = uwacmat_load(fixture("none", NULL, NULL));
    snprintf(why, sizeof why, "load %d", ok);
    check("missing", ok == 0 && uwacmat_count() == 0, why);

    ok = uwacmat_load(fixture("default", uwacmat_default(), NULL));
    static const char *six[] = {"stone", "wood", "carpet", "metal", "water", "dirt"};
    int all = ok == 1 && uwacmat_count() >= 6, bad = 0;
    for (int i = 0; i < 6 && all; i++) all = named(i, six[i]);
    const UwaMaterial *dwood = uwacmat_get(1);   /* the shipped doors are dull as well as quiet: low > mid > high */
    all = all && dwood && dwood->transmit[0] > dwood->transmit[1] && dwood->transmit[1] > dwood->transmit[2] && dwood->transmit[2] > 0.0f;
    for (int kind = UWA_FLOOR; kind <= UWA_DOOR; kind++)
        for (int t = 0; t < 0x150; t++) bad += !uwacmat_get(uwacmat_for(kind, t));
    snprintf(why, sizeof why, "load %d, count %d, the six first %d, %d numbers without a preset", ok, uwacmat_count(), all, bad);
    check("default", all && bad == 0, why);
    /* the table in the code without a file (uwacmat_load_default: the footsteps' materials when no acoustics.txt
       is there, nothing written), with the footsteps' notes: stone its own (0), wood up, water down */
    int okd = uwacmat_load_default();
    const UwaMaterial *ds = uwacmat_get(0), *dw = uwacmat_get(1), *dwa = uwacmat_get(4);
    snprintf(why, sizeof why, "load %d, count %d; step: stone %d, wood %d, water %d (%s)", okd, uwacmat_count(), ds ? ds->step : 99, dw ? dw->step : 99,
             dwa ? dwa->step : 99, dwa ? dwa->name : "?");
    check("default in memory", okd == 1 && uwacmat_count() >= 7 && ds->step == 0 && dw->step > 0 && dwa && !strcmp(dwa->name, "water") && dwa->step < 0, why);

    const char *fresh = fixture("fresh", NULL, NULL);
    int w1 = uwacmat_write_default(fresh), w2 = uwacmat_write_default(fresh), same = 0;
    {
        char p[600];
        static char buf[65536];
        snprintf(p, sizeof p, "%s\\acoustics.txt", fresh);
        FILE *f = fopen(p, "rb");
        if (f) {
            size_t n = fread(buf, 1, sizeof buf - 1, f);
            fclose(f);
            buf[n] = 0;
            same = !strcmp(buf, uwacmat_default());
        }
    }
    snprintf(why, sizeof why, "first write %d, second %d, the file the default text %d, loads %d", w1, w2, same, uwacmat_load(fresh));
    check("write", w1 == 1 && w2 == 0 && same && uwacmat_load(fresh) == 1, why);
    return fails;
}
