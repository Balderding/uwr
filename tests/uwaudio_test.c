/* uwaudio_test.c: port\win32\uwaudio.c over OpenAL Soft's loopback device (ALC_SOFT_loopback: rendered into memory,
   nothing played on the machine). tools\fxcheck.py audio builds and runs it:
       uwaudio_test.exe BINDIR WORKDIR
   BINDIR holds soft_oal.dll (absent: "SKIP"), WORKDIR gets sfx\007.wav (a generated 440 Hz tone). Cases:
     east   the listener facing north, the effect 3 tiles east: the right channel louder (> 1.5x), alGetError 0
     west   3 tiles west: the left louder
     far    7 tiles away: not played (the original plays nothing beyond six tiles)
     muted  FX_MUTED: not played
     none   an effect with no sample file: not played, no error
     hrtf   ALC_SOFT_HRTF on by default (status 1); an effect ahead and one behind render differently; with
            UWPORT_FX_HRTF=0 the status is 0 (OpenAL Soft's plain stereo still tells front from back a little: not checked)
   Prints one PASS / FAIL line a case; the exit code is the failures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include "../port/win32/uwaudio.h"

#define RATE 48000
static int fails;
static void check(const char *name, int ok, const char *why)
{
    printf("%s %s%s%s\n", name, ok ? "PASS" : "FAIL", ok ? "" : ": ", ok ? "" : why);
    fails += !ok;
}
static void w16(FILE *f, int v) { fputc(v & 255, f), fputc(v >> 8 & 255, f); }
static void w32(FILE *f, unsigned v) { w16(f, (int)(v & 0xFFFF)), w16(f, (int)(v >> 16)); }
static short b[RATE / 5 * 2];   /* rms's last rendering (the east case's is kept as the reference of "acoustics off") */
static void rms(double *l, double *r)
{
    double sl = 0, sr = 0;
    uwaudio_render(b, RATE / 5);
    for (int i = 0; i < RATE / 5; i++) sl += (double)b[2 * i] * b[2 * i], sr += (double)b[2 * i + 1] * b[2 * i + 1];
    *l = sqrt(sl / (RATE / 5)), *r = sqrt(sr / (RATE / 5));
}
static void quiet(void);
/* effect 7 three tiles north of the listener (facing north), then three tiles south: *front the front's stereo RMS,
   *diff the RMS of the sample-by-sample difference between the two renderings (0 when they are the same) */
static void front_back(double *front, double *diff)
{
    static short a[RATE / 5 * 2], b[RATE / 5 * 2];
    double sa = 0, sd = 0;
    uwaudio_listener(10.5f, 1.0f, 10.5f, 0.0f);
    uwaudio_event(1, 10.5f, 1.0f, 13.5f, 7, -1);
    uwaudio_render(a, RATE / 5);
    quiet();
    uwaudio_event(1, 10.5f, 1.0f, 7.5f, 7, -1);
    uwaudio_render(b, RATE / 5);
    quiet();
    for (int i = 0; i < RATE / 5 * 2; i++) sa += (double)a[i] * a[i], sd += (double)(a[i] - b[i]) * (a[i] - b[i]);
    *front = sqrt(sa / (RATE / 5 * 2)), *diff = sqrt(sd / (RATE / 5 * 2));
}
static void quiet(void)   /* until every source has finished */
{
    static short b[RATE * 2];
    for (int k = 0; k < 10 && uwaudio_playing(); k++) uwaudio_render(b, RATE);
}

/* --- the acoustics (the plan's Task 4): reverb by the room, muffling by what stands between ---
   The fixture is tests\uwacoustic_test.c's: room A 4 x 4 (x 10-13, z 10-13), room B 12 x 12 (x 15-26, z 10-21), the
   wall column x 14 between them with a doorway at (14, 11) holding a door across it. Materials by the shipped
   table's order: 0 stone, 1 wood, 2 carpet. Samples: 007 0.5 s of 440 Hz, 009 0.5 s of 4 kHz, 010 3 s of 440 Hz,
   011 3 s of 4 kHz. */
enum { STONE, WOOD, CARPET, METAL, WATER };
static const char *g_work;
static UwaTile grid[64 * 64];
static void rooms(int a_mat, int ceil_mat, int door_shut, int door_mat)
{
    UwaDoor door = {1, 14, 11, 2, door_mat, door_shut};
    for (int i = 0; i < 64 * 64; i++) grid[i].type = UWAT_SOLID, grid[i].height = 15, grid[i].wall_mat = grid[i].floor_mat = STONE;
    for (int z = 9; z <= 14; z++)
        for (int x = 9; x <= 14; x++) grid[z * 64 + x].wall_mat = (unsigned char)a_mat;
    for (int z = 10; z <= 13; z++)
        for (int x = 10; x <= 13; x++) grid[z * 64 + x].type = UWAT_OPEN, grid[z * 64 + x].height = 0, grid[z * 64 + x].floor_mat = (unsigned char)a_mat;
    for (int z = 10; z <= 21; z++)
        for (int x = 15; x <= 26; x++) grid[z * 64 + x].type = UWAT_OPEN, grid[z * 64 + x].height = 0;
    grid[11 * 64 + 14].type = UWAT_OPEN, grid[11 * 64 + 14].height = 0;
    for (int z = 10; z <= 21; z++) grid[z * 64 + 30].type = UWAT_OPEN, grid[z * 64 + 30].height = 0;   /* a corridor north, */
    for (int x = 31; x <= 36; x++) grid[21 * 64 + x].type = UWAT_OPEN, grid[21 * 64 + x].height = 0;   /* its arm east: an L */
    uwaudio_acoustics(grid, ceil_mat, &door, 1);
}
static void settings(const char *text)   /* WORKDIR\settings.txt, whole */
{
    char p[1024];
    snprintf(p, sizeof p, "%s\\settings.txt", g_work);
    FILE *f = fopen(p, "wb");
    if (f) fputs(text, f), fclose(f);
}
static void tone_wav(int number, double hz, double seconds)
{
    char p[1024];
    int n = (int)(22050 * seconds);
    snprintf(p, sizeof p, "%s\\sfx\\%03d.wav", g_work, number);
    FILE *f = fopen(p, "wb");
    if (!f) return;
    fwrite("RIFF", 1, 4, f), w32(f, 36 + (unsigned)n * 2), fwrite("WAVEfmt ", 1, 8, f), w32(f, 16), w16(f, 1), w16(f, 1);
    w32(f, 22050), w32(f, 44100), w16(f, 2), w16(f, 16), fwrite("data", 1, 4, f), w32(f, (unsigned)n * 2);
    for (int i = 0; i < n; i++) w16(f, (int)(12000.0 * sin(6.2831853 * hz * i / 22050.0)) & 0xFFFF);
    fclose(f);
}
static short big[RATE * 2 * 2];          /* up to two seconds rendered */
static double span(int ms, int *jump)   /* the next ms rendered: its RMS over both channels; *jump: the largest step between two samples of a channel */
{
    int n = RATE / 1000 * ms, j = 0;
    double s = 0;
    uwaudio_render(big, n);
    for (int i = 0; i < 2 * n; i++) {
        s += (double)big[i] * big[i];
        if (i >= 2 && abs(big[i] - big[i - 2]) > j) j = abs(big[i] - big[i - 2]);
    }
    if (jump) *jump = j;
    return sqrt(s / (2 * n));
}
static double tone_of(int ms, double hz)   /* the amplitude of hz in the last span (both channels' mean) */
{
    int n = RATE / 1000 * ms;
    double s = 0, c = 0;
    for (int i = 0; i < n; i++) {
        double m = (big[2 * i] + big[2 * i + 1]) / 2.0, a = 6.2831853 * hz * i / RATE;
        s += m * sin(a), c += m * cos(a);
    }
    return 2.0 * sqrt(s * s + c * c) / n;
}
static void ears(int ms, double *l, double *r)   /* the last span's RMS, each ear */
{
    int n = RATE / 1000 * ms;
    double sl = 0, sr = 0;
    for (int i = 0; i < n; i++) sl += (double)big[2 * i] * big[2 * i], sr += (double)big[2 * i + 1] * big[2 * i + 1];
    *l = sqrt(sl / n), *r = sqrt(sr / n);
}
static void hush(void) { quiet(); span(1500, NULL); span(1500, NULL); }   /* every source done, and the reverb's tail gone */
/* the sample played a tile east of the listener in a room: the RMS of 300 ms, from 0.6 s after its half second (right
   after it a dead room and a live one are as loud: what tells them apart is how long it lasts) */
static double tail_in(float x, float z)
{
    uwaudio_listener(x, 1.0f, z, 0.0f);
    span(300, NULL);                              /* (the room found, the reverb settled) */
    uwaudio_event(1, x + 1.0f, 1.0f, z, 7, -1);
    span(500, NULL);
    span(600, NULL);
    double t = span(300, NULL);
    hush();
    return t;
}
/* the sample played at (sx, sz), the listener at (lx, lz): the RMS of the 300 ms after its half second (the room's ring alone) */
static double ring_from(float lx, float lz, float sx, float sz)
{
    uwaudio_listener(lx, 1.0f, lz, 0.0f);
    span(300, NULL);
    uwaudio_event(1, sx, 1.0f, sz, 7, -1);
    span(500, NULL);
    double t = span(300, NULL);
    hush();
    return t;
}
/* effect fx from (sx, sz) heard at (lx, lz): the amplitude of hz in its first 100 ms */
static double heard(int fx, double hz, float lx, float lz, float sx, float sz)
{
    uwaudio_listener(lx, 1.0f, lz, 0.0f);
    span(120, NULL);
    uwaudio_event(1, sx, 1.0f, sz, fx, -1);
    span(100, NULL);
    double a = tone_of(100, hz);
    hush();
    return a;
}
static double g_tail0;   /* the tail with no acoustics at all (acoustics() measures it) */
static void acoustics(const char *bin, const char *work)
{
    char why[400];
    int jump = 0;
    g_work = work;
    tone_wav(9, 4000.0, 0.5), tone_wav(10, 440.0, 3.0), tone_wav(11, 4000.0, 3.0);
    {   /* the materials: the table the code ships (a table left here by an older run would be read instead) */
        char tp[1024];
        snprintf(tp, sizeof tp, "%s\\acoustics.txt", work);
        FILE *tf = fopen(tp, "wb");
        if (tf) fputs(uwacmat_default(), tf), fclose(tf);
        snprintf(tp, sizeof tp, "%s\\sfx\\001-water.wav", work);   /* Task 6's fixture: a footstep's sound of the user's own for water (4 kHz) */
        tf = fopen(tp, "wb");
        if (tf) {
            int n = 6615;
            fwrite("RIFF", 1, 4, tf), w32(tf, 36 + (unsigned)n * 2), fwrite("WAVEfmt ", 1, 8, tf), w32(tf, 16), w16(tf, 1), w16(tf, 1);
            w32(tf, 22050), w32(tf, 44100), w16(tf, 2), w16(tf, 16), fwrite("data", 1, 4, tf), w32(tf, (unsigned)n * 2);
            for (int i = 0; i < n; i++) w16(tf, (int)(12000.0 * sin(6.2831853 * 4000.0 * i / 22050.0)) & 0xFFFF);
            fclose(tf);
        }
    }

    /* without them (UWPORT_FX_ACOUSTICS=0): what the same sounds are with no acoustics at all */
    _putenv("UWPORT_FX_ACOUSTICS=0");
    settings("");
    if (!uwaudio_init_loopback(bin, work, RATE)) { printf("FAIL: the loopback device did not open (acoustics off)\n"); fails++; return; }
    rooms(STONE, STONE, 1, WOOD);
    double tail0 = tail_in(21.0f, 16.0f);
    g_tail0 = tail0;
    uwaudio_listener(10.2f, 1.0f, 12.0f, 0.0f);   /* against A's west wall */
    uwaudio_event(1, -1.0f, 1.0f, -1.0f, 7, -1);
    double here0 = span(100, NULL);
    hush();
    uwaudio_shutdown();

    _putenv("UWPORT_FX_ACOUSTICS=1");
    if (!uwaudio_init_loopback(bin, work, RATE)) { printf("FAIL: the loopback device did not open (acoustics on)\n"); fails++; return; }
    snprintf(why, sizeof why, "acoustics %d, error %d", uwaudio_acoustics_on(), uwaudio_error());
    check("acoustics on", uwaudio_acoustics_on() == 1 && uwaudio_error() == 0, why);

    /* tail: a stone hall rings on after the sound; a carpeted room far less; without the acoustics nothing */
    rooms(STONE, STONE, 1, WOOD);
    double tail_b = tail_in(21.0f, 16.0f), decay_b = uwaudio_acoustic_decay();
    rooms(CARPET, CARPET, 1, WOOD);
    double tail_a = tail_in(12.0f, 12.0f), decay_a = uwaudio_acoustic_decay();
    snprintf(why, sizeof why, "0.6 to 0.9 s after the sound: stone hall B %.1f (decay %.2f s), carpeted room A %.1f (decay %.2f s), acoustics off %.1f", tail_b, decay_b, tail_a, decay_a, tail0);
    printf("  tail: %s\n", why);
    check("tail", tail_b > 2 * tail_a && tail_b > 20 * tail0 && tail_b > 20, why);

    /* occluded tail (the review, 2026-11-14): the listener in room A; the sample three tiles off in A, then three
       tiles off behind the rock in B. What rings in A in the 300 ms after the sound: far less for the hidden one (its
       way into the reverb is muffled as the sound itself is; unmuffled, the two rang alike) */
    rooms(STONE, STONE, 1, WOOD);
    double ring_clear = ring_from(13.5f, 13.0f, 10.5f, 13.0f), ring_rock = ring_from(13.5f, 13.0f, 16.5f, 13.0f);
    snprintf(why, sizeof why, "the 300 ms after the sound, heard in room A: from within A %.1f, from behind the rock %.1f", ring_clear, ring_rock);
    printf("  occluded tail: %s\n", why);
    check("occluded tail", ring_clear > 20 && ring_rock < ring_clear / 3, why);

    /* dry: acoustics_wet 0 is no reverb at all */
    rooms(STONE, STONE, 1, WOOD);
    settings("acoustics_wet 0\n"), uwaudio_tune(work);
    double tail_dry = tail_in(21.0f, 16.0f);
    snprintf(why, sizeof why, "the tail with acoustics_wet 0: %.1f; acoustics off: %.1f", tail_dry, tail0);
    check("dry", tail_dry <= tail0 * 1.05 + 2, why);

    /* (the reverb stays off for the muffling's cases: the direct sound alone is measured) */
    /* occluded, muffled: the same distance (3 tiles) in one room and through the rock between A and B */
    double clear_lo = heard(7, 440.0, 18.0f, 16.0f, 21.0f, 16.0f), rock_lo = heard(7, 440.0, 12.5f, 13.0f, 15.5f, 13.0f);
    double clear_hi = heard(9, 4000.0, 18.0f, 16.0f, 21.0f, 16.0f), rock_hi = heard(9, 4000.0, 12.5f, 13.0f, 15.5f, 13.0f);
    snprintf(why, sizeof why, "440 Hz: clear %.0f, behind rock %.0f (%.3f); 4 kHz: clear %.0f, behind rock %.0f (%.3f)", clear_lo, rock_lo,
             rock_lo / clear_lo, clear_hi, rock_hi, rock_hi / clear_hi);
    printf("  occluded: %s\n", why);
    check("occluded", clear_lo > 100 && rock_lo < clear_lo / 4, why);
    check("muffled", clear_hi > 100 && rock_hi / clear_hi < 0.7 * (rock_lo / clear_lo), why);

    /* door: behind the shut wood door, then the door opened: the sound comes back without a click */
    rooms(STONE, STONE, 0, WOOD);
    uwaudio_listener(12.5f, 1.0f, 11.5f, 0.0f);
    span(120, NULL);
    uwaudio_event(1, 15.5f, 1.0f, 11.5f, 10, -1);
    span(1900, NULL);
    span(100, NULL);
    double open_ref = tone_of(100, 440.0);
    hush();
    rooms(STONE, STONE, 1, WOOD);
    span(120, NULL);
    uwaudio_event(1, 15.5f, 1.0f, 11.5f, 10, -1);
    span(300, NULL);
    double shut = tone_of(300, 440.0);
    rooms(STONE, STONE, 0, WOOD);                   /* the door opens while its sound plays */
    span(1600, &jump);
    span(100, NULL);
    double opened = tone_of(100, 440.0);
    hush();
    snprintf(why, sizeof why, "shut %.0f, 1.6 s after it opened %.0f, open from the start %.0f; the largest step between samples %d", shut, opened, open_ref, jump);
    printf("  door: %s\n", why);
    check("door", shut < 0.5 * open_ref && fabs(opened - open_ref) < 0.1 * open_ref && jump < 4000, why);

    /* here: an effect at the listener, the listener against a wall: never muffled */
    rooms(STONE, STONE, 1, WOOD);
    uwaudio_listener(10.2f, 1.0f, 12.0f, 0.0f);
    span(120, NULL);
    uwaudio_event(1, -1.0f, 1.0f, -1.0f, 7, -1);
    double here1 = span(100, NULL);
    hush();
    snprintf(why, sizeof why, "the first 100 ms: %.1f with the acoustics, %.1f without", here1, here0);
    check("here", here0 > 100 && fabs(here1 - here0) < 0.02 * here0, why);

    /* steal: every source busy behind rock (4 kHz), a new sound beside the listener (440 Hz) takes the oldest:
       it starts with its own path, not the stolen source's */
    double near_ref = heard(7, 440.0, 12.5f, 12.5f, 12.9f, 12.5f);
    uwaudio_listener(12.5f, 12.5f - 11.5f, 12.5f, 0.0f);
    span(120, NULL);
    for (int i = 0; i < 32; i++) uwaudio_event(1, 15.5f, 1.0f, 13.0f, 11, -1);
    span(100, NULL);
    int busy = uwaudio_playing();
    uwaudio_event(1, 12.9f, 1.0f, 12.5f, 7, -1);
    span(50, NULL);
    double stolen = tone_of(50, 440.0);
    hush();
    span(120, NULL);
    uwaudio_event(1, 12.9f, 1.0f, 12.5f, 7, -1);
    span(50, NULL);
    double alone = tone_of(50, 440.0);
    hush();
    snprintf(why, sizeof why, "%d sources busy; the new sound's 440 Hz in its first 50 ms: %.0f, alone %.0f (100 ms alone %.0f)", busy, stolen, alone, near_ref);
    printf("  steal: %s\n", why);
    check("steal", busy == 32 && alone > 100 && fabs(stolen - alone) < 0.1 * alone, why);

    /* round corner (Task 5): a sound round the L's corner, the listener in the corridor facing north, the source two
       tiles up the arm (north-east through the rock): with "Sound round corners" it is louder than behind rock and
       quieter than in the open, and comes from the corner ahead (the two ears alike), not from the right */
    double cl, cr, fl, fr;
    uwaudio_listener(30.5f, 1.0f, 19.5f, 0.0f);
    span(120, NULL);
    uwaudio_event(1, 32.5f, 1.0f, 21.5f, 7, -1);
    span(100, NULL);
    double corner = tone_of(100, 440.0);
    ears(100, &cl, &cr);
    hush();
    settings("acoustics_wet 0\nacoustics_detour 0\n"), uwaudio_tune(work);
    span(120, NULL);
    uwaudio_event(1, 32.5f, 1.0f, 21.5f, 7, -1);
    span(100, NULL);
    double blocked = tone_of(100, 440.0);
    ears(100, &fl, &fr);
    hush();
    settings("acoustics_wet 0\n"), uwaudio_tune(work);
    double open_same = heard(7, 440.0, 18.0f, 16.0f, 20.0f, 18.0f);
    snprintf(why, sizeof why, "round the corner %.0f (left %.0f right %.0f); with acoustics_detour 0 %.0f (left %.0f right %.0f); the same distance in the open %.0f",
             corner, cl, cr, blocked, fl, fr, open_same);
    printf("  round corner: %s\n", why);
    check("round corner", corner > 2 * blocked && corner < 0.9 * open_same && fabs(cl - cr) < 0.15 * (cl + cr) / 2 && fr > 1.2 * fl, why);

    /* step wav (Task 6): a footstep (effect 1) on water plays the user's 001-water.wav; on stone it does not (here
       effect 1 has no sound of its own without the Roland: nothing plays); with "Footsteps by floor" off, nothing */
    rooms(WATER, STONE, 1, WOOD);
    uwaudio_listener(12.0f, 1.0f, 12.0f, 0.0f);
    span(120, NULL);
    uwaudio_event(1, -1.0f, 1.0f, -1.0f, 1, -1);
    span(100, NULL);
    double on_water = tone_of(100, 4000.0);
    hush();
    uwaudio_event(1, 16.5f, 0.0f, 12.0f, 1, -1);   /* a creature's step in B, on stone, heard from A */
    int on_stone = uwaudio_playing();
    hush();
    settings("acoustics_wet 0\nfx_footsteps 0\n"), uwaudio_tune(work);
    uwaudio_event(1, -1.0f, 1.0f, -1.0f, 1, -1);
    int off_water = uwaudio_playing();
    hush();
    settings("acoustics_wet 0\n"), uwaudio_tune(work);
    rooms(STONE, STONE, 1, WOOD);
    snprintf(why, sizeof why, "on water: 4 kHz at %.0f; on stone: %d playing; on water with fx_footsteps 0: %d playing", on_water, on_stone, off_water);
    check("step wav", on_water > 100 && on_stone == 0 && off_water == 0, why);

    /* jump: the listener from A into B at once (a teleport): the reverb eases to the new room */
    settings(""), uwaudio_tune(work);
    uwaudio_listener(12.0f, 1.0f, 12.0f, 0.0f);
    span(1500, NULL);
    double d0 = uwaudio_acoustic_decay();
    uwaudio_listener(21.0f, 1.0f, 16.0f, 0.0f);
    span(100, NULL);
    double d1 = uwaudio_acoustic_decay();
    span(100, NULL);
    double d2 = uwaudio_acoustic_decay();
    span(1500, NULL);
    double d3 = uwaudio_acoustic_decay();
    snprintf(why, sizeof why, "the decay in A %.2f s; 100 ms after the jump %.2f, 200 ms %.2f, settled in B %.2f; error %d", d0, d1, d2, d3, uwaudio_error());
    printf("  jump: %s\n", why);
    check("jump", d0 < d1 && d1 < d2 && d2 < d3 && uwaudio_error() == 0, why);

    /* tune: the three settings changed while it plays, taken at once with no restart */
    uwaudio_event(1, 22.0f, 1.0f, 16.0f, 11, -1);   /* three seconds of 4 kHz in B (a source that stays playing) */
    uwaudio_event(1, 22.0f, 1.0f, 16.0f, 7, -1);    /* and the half second of 440 Hz, whose tail is listened for */
    span(1100, NULL);
    span(100, NULL);
    double wet = tone_of(100, 440.0);
    int playing = uwaudio_playing();
    settings("acoustics_wet 0\n"), uwaudio_tune(work);
    span(200, NULL);
    span(100, NULL);
    double dry = tone_of(100, 440.0);
    int playing2 = uwaudio_playing();
    hush();
    double occ = heard(7, 440.0, 12.5f, 13.0f, 15.5f, 13.0f);
    settings("acoustics_wet 0\nacoustics_occlusion 0\n"), uwaudio_tune(work);
    double unocc = heard(7, 440.0, 12.5f, 13.0f, 15.5f, 13.0f);
    settings("acoustics_tile_m 1.5\n"), uwaudio_tune(work);
    uwaudio_listener(12.0f, 1.0f, 12.0f, 0.0f);
    span(1500, NULL);
    double small = uwaudio_acoustic_decay();
    settings("acoustics_tile_m 4.0\n"), uwaudio_tune(work);
    span(1500, NULL);
    double large = uwaudio_acoustic_decay();
    snprintf(why, sizeof why, "a 440 Hz tail in B %.1f, 0.2 s after acoustics_wet 0 %.1f (%d then %d playing); behind rock %.0f, with acoustics_occlusion 0 %.0f (clear %.0f); "
             "A's decay at 1.5 m a tile %.2f s, at 4.0 m %.2f s", wet, dry, playing, playing2, occ, unocc, clear_lo, small, large);
    printf("  tune: %s\n", why);
    check("tune", wet > 20 && dry < 0.05 * wet && playing == 1 && playing2 == 1 && fabs(unocc - clear_lo) < 0.05 * clear_lo && occ < clear_lo / 4 && large > 1.5 * small, why);
    settings("");
    uwaudio_shutdown();
    _putenv("UWPORT_FX_ACOUSTICS=");
}

/* --- Acoustics High (the plan's Task 7; UWPORT_FX_ACOUSTICS=2): Steam Audio's reflections in the EFX reverb's place,
   our muffling kept. UWPORT_FX_PHONON names phonon.dll's folder (not set, or no DLL there: the cases are skipped). --- */
static void acoustics_high(const char *bin, const char *work)
{
    char why[400], pp[1024];
    const char *ph = getenv("UWPORT_FX_PHONON");
    snprintf(pp, sizeof pp, "%s\\phonon.dll", ph ? ph : "");
    if (!ph || GetFileAttributesA(pp) == INVALID_FILE_ATTRIBUTES) { printf("  high: skipped (no phonon.dll: UWPORT_FX_PHONON)\n"); return; }
    g_work = work;
    _putenv("UWPORT_FX_ACOUSTICS=2");
    settings("");
    if (!uwaudio_init_loopback(bin, work, RATE)) { printf("FAIL: the loopback device did not open (acoustics high)\n"); fails++; return; }
    snprintf(why, sizeof why, "acoustics %d, error %d", uwaudio_acoustics_on(), uwaudio_error());
    check("high on", uwaudio_acoustics_on() == 2 && uwaudio_error() == 0, why);
    rooms(STONE, STONE, 1, WOOD);
    double tail_b = tail_in(21.0f, 16.0f);
    rooms(CARPET, CARPET, 1, WOOD);
    double tail_a = tail_in(12.0f, 12.0f);
    snprintf(why, sizeof why, "0.6 to 0.9 s after the sound: stone hall B %.1f, carpeted room A %.1f, acoustics off %.1f; the EFX reverb's decay %.2f (0: not used)",
             tail_b, tail_a, g_tail0, uwaudio_acoustic_decay());
    printf("  high tail: %s\n", why);
    check("high tail", tail_b > 2 * tail_a && tail_b > 20 * g_tail0 && tail_b > 20 && uwaudio_acoustic_decay() == 0, why);
    rooms(STONE, STONE, 1, WOOD);
    settings("acoustics_wet 0\n"), uwaudio_tune(work);
    double dry = tail_in(21.0f, 16.0f);
    double clear_lo = heard(7, 440.0, 18.0f, 16.0f, 21.0f, 16.0f), rock_lo = heard(7, 440.0, 12.5f, 13.0f, 15.5f, 13.0f);
    snprintf(why, sizeof why, "the tail with acoustics_wet 0: %.1f (off %.1f); 440 Hz clear %.0f, behind rock %.0f", dry, g_tail0, clear_lo, rock_lo);
    printf("  high dry: %s\n", why);
    check("high dry", dry <= g_tail0 * 1.05 + 2, why);
    check("high occluded", clear_lo > 100 && rock_lo < clear_lo / 4, why);
    settings(""), uwaudio_tune(work);
    uwaudio_listener(21.0f, 1.0f, 16.0f, 0.0f);
    uwaudio_event(1, 22.0f, 1.0f, 16.0f, 7, -1);
    span(500, NULL);
    double ringing = span(300, NULL);
    for (int k = 0; k < 4; k++) span(1500, NULL);
    double after = span(300, NULL);
    snprintf(why, sizeof why, "right after the sound %.1f; six seconds on %.1f; error %d", ringing, after, uwaudio_error());
    check("high drain", ringing > 20 && after < 3 && uwaudio_error() == 0, why);
    uwaudio_shutdown();
    _putenv("UWPORT_FX_ACOUSTICS=");
}

int main(int argc, char **argv)
{
    char p[1024], why[256];
    double l, r;
    if (argc < 3) { fprintf(stderr, "usage: uwaudio_test BINDIR WORKDIR\n"); return 2; }
    snprintf(p, sizeof p, "%s\\soft_oal.dll", argv[1]);
    FILE *f = fopen(p, "rb");
    if (!f) { printf("SKIP: no soft_oal.dll in %s\n", argv[1]); return 0; }
    fclose(f);
    /* the sample: 0.5 s of 440 Hz, mono, 16 bits, 22050 a second */
    snprintf(p, sizeof p, "%s\\sfx\\007.wav", argv[2]);
    if (!(f = fopen(p, "wb"))) { printf("FAIL: cannot write %s\n", p); return 1; }
    int n = 11025;
    fwrite("RIFF", 1, 4, f), w32(f, 36 + (unsigned)n * 2), fwrite("WAVEfmt ", 1, 8, f), w32(f, 16), w16(f, 1), w16(f, 1);
    w32(f, 22050), w32(f, 44100), w16(f, 2), w16(f, 16), fwrite("data", 1, 4, f), w32(f, (unsigned)n * 2);
    for (int i = 0; i < n; i++) w16(f, (int)(12000.0 * sin(6.2831853 * 440.0 * i / 22050.0)) & 0xFFFF);
    fclose(f);

    if (!uwaudio_init_loopback(argv[1], argv[2], RATE)) { printf("FAIL: the loopback device did not open\n"); return 1; }
    uwaudio_listener(10.5f, 1.0f, 10.5f, 0.0f);   /* facing north */
    uwaudio_event(1, 13.5f, 1.0f, 10.5f, 7, -1);       /* FX_SOUND 7, three tiles east */
    rms(&l, &r);
    snprintf(why, sizeof why, "left %.0f right %.0f, error %d", l, r, uwaudio_error());
    printf("  east: %s\n", why);
    check("east", r > 100 && r > 1.5 * l && uwaudio_error() == 0, why);
    {   /* off: without fx_acoustics the sound is sample for sample what it was before the acoustics were written
           (UWAUDIO_REF: tests\data\uwaudio_east_ref.raw, this case's rendering by the code of 2026-11-14 morning) */
        static short ref[RATE / 5 * 2];
        const char *rp = getenv("UWAUDIO_REF");
        FILE *rf = rp && *rp ? fopen(rp, "rb") : NULL;
        int got = rf ? (int)fread(ref, 1, sizeof ref, rf) : -1, differ = 0;
        if (rf) fclose(rf);
        for (int i = 0; i < RATE / 5 * 2; i++) differ += ref[i] != b[i];
        snprintf(why, sizeof why, "%s: %d bytes read, %d samples differ, acoustics %d", rp ? rp : "(UWAUDIO_REF not set)", got, differ, uwaudio_acoustics_on());
        check("off", got == (int)sizeof ref && differ == 0 && !uwaudio_acoustics_on(), why);
    }
    {   /* UWAUDIO_REF_WRITE=FILE: the east case's samples saved (tests\data\uwaudio_east_ref.raw was written so on
           2026-11-14 from the code before the acoustics: the "off" case below compares with it) */
        const char *rw = getenv("UWAUDIO_REF_WRITE");
        FILE *rf = rw && *rw ? fopen(rw, "wb") : NULL;
        if (rf) fwrite(b, sizeof b, 1, rf), fclose(rf), printf("  east: reference written to %s\n", rw);
    }
    quiet();
    uwaudio_event(1, 7.5f, 1.0f, 10.5f, 7, -1);        /* three tiles west */
    rms(&l, &r);
    snprintf(why, sizeof why, "left %.0f right %.0f", l, r);
    printf("  west: %s\n", why);
    check("west", l > 100 && l > 1.5 * r, why);
    quiet();
    uwaudio_event(1, 17.5f, 1.0f, 10.5f, 7, -1);       /* seven tiles east */
    snprintf(why, sizeof why, "%d playing", uwaudio_playing());
    check("far", uwaudio_playing() == 0, why);
    uwaudio_event(1 | 0x100, 11.5f, 1.0f, 10.5f, 7, -1);
    snprintf(why, sizeof why, "%d playing", uwaudio_playing());
    check("muted", uwaudio_playing() == 0, why);
    uwaudio_event(1, 11.5f, 1.0f, 10.5f, 8, -1);   /* no 008.wav */
    snprintf(why, sizeof why, "%d playing, error %d", uwaudio_playing(), uwaudio_error());
    check("none", uwaudio_playing() == 0 && uwaudio_error() == 0, why);
    /* HRTF: asked for by default (fx_hrtf absent = 1): the context reports it on; an effect three tiles ahead and the
       same three tiles behind (both dead centre, the same distance: identical under plain pan) render differently */
    snprintf(why, sizeof why, "status %d (%s)", uwaudio_hrtf(), uwaudio_hrtf_name());
    printf("  hrtf: %s\n", why);
    check("hrtf on", uwaudio_hrtf() == 1, why);
    front_back(&l, &r);
    snprintf(why, sizeof why, "front rms %.0f, front-back difference rms %.0f", l, r);
    printf("  hrtf front/back: %s\n", why);
    check("hrtf front/back differ", l > 100 && r > 0.2 * l, why);
    uwaudio_shutdown();
    /* fx_hrtf 0 (UWPORT_FX_HRTF=0 for the checks): off, and front and back come out the same */
    _putenv("UWPORT_FX_HRTF=0");
    if (!uwaudio_init_loopback(argv[1], argv[2], RATE)) { printf("FAIL: the loopback device did not open (hrtf off)\n"); return fails + 1; }
    snprintf(why, sizeof why, "status %d", uwaudio_hrtf());
    check("hrtf off", uwaudio_hrtf() == 0, why);
    uwaudio_shutdown();
    _putenv("UWPORT_FX_HRTF=");
    acoustics(argv[1], argv[2]);
    acoustics_high(argv[1], argv[2]);
    /* roland (a third argument DATADIR, the game's data folder: its parent holds mt32emu.dll and roms\): the effects
       are Munt's own renderings; effect 11 (a door) three tiles east comes out right, and the module reports it as
       taken (the game's copy silent) until shutdown */
    if (argc > 3) {
        int waited;
        if (!uwaudio_init_loopback(argv[1], argv[3], RATE)) { printf("FAIL: the loopback device did not open (roland)\n"); return fails + 1; }
        for (waited = 0; waited < 400 && !(uwaudio_taken() >> 11 & 1); waited++) Sleep(50);
        snprintf(why, sizeof why, "taken %016llx after %d ms", uwaudio_taken(), waited * 50);
        printf("  roland: %s\n", why);
        check("roland rendered", (uwaudio_taken() >> 11 & 1) != 0, why);
        uwaudio_listener(10.5f, 1.0f, 10.5f, 0.0f);
        uwaudio_event(1, 13.5f, 1.0f, 10.5f, 11, -1);
        rms(&l, &r);
        snprintf(why, sizeof why, "left %.0f right %.0f, %d playing, error %d", l, r, uwaudio_playing(), uwaudio_error());
        printf("  roland east: %s\n", why);
        check("roland east", r > 100 && r > 1.5 * l && uwaudio_error() == 0, why);
        quiet();
        /* stop: the water (effect 0, 25 s) started at the listener plays until the game's kill_effect (FX_STOP 8) */
        for (waited = 0; waited < 600 && !(uwaudio_taken() & 1); waited++) Sleep(50);
        uwaudio_event(1, -1.0f, 1.0f, -1.0f, 0, -1);
        rms(&l, &r);
        snprintf(why, sizeof why, "taken %016llx, %d playing, left %.0f right %.0f", uwaudio_taken(), uwaudio_playing(), l, r);
        printf("  roland water: %s\n", why);
        check("roland water plays", (uwaudio_taken() & 1) && uwaudio_playing() == 1 && l + r > 100, why);
        uwaudio_event(8, -1.0f, -1.0f, -1.0f, 0, -1);
        rms(&l, &r);
        snprintf(why, sizeof why, "%d playing, left %.0f right %.0f after the stop", uwaudio_playing(), l, r);
        printf("  roland stop: %s\n", why);
        check("roland stop", uwaudio_playing() == 0 && l + r < 10, why);
        /* the velocity: a blow (effect 4, SOUNDS.DAT velocity 72) at the game's 0x7F (a hard hit) is louder than at
           its own, by its own rendering (the worker renders the blows' velocities after the base set) */
        for (waited = 0; waited < 600 && !uwaudio_rendered(4, 0x7F); waited++) Sleep(50);
        uwaudio_event(1, -1.0f, 1.0f, -1.0f, 4, 72);
        rms(&l, &r);
        quiet();
        {
            double soft = l + r;
            uwaudio_event(1, -1.0f, 1.0f, -1.0f, 4, 0x7F);
            rms(&l, &r);
            quiet();
            snprintf(why, sizeof why, "rendered %d after %d ms; at 72: %.0f, at 0x7F: %.0f", uwaudio_rendered(4, 0x7F), waited * 50, soft, l + r);
            printf("  roland velocity: %s\n", why);
            check("roland velocity", uwaudio_rendered(4, 0x7F) && soft > 50 && l + r > 1.2 * soft, why);
        }
        {   /* Task 6, footsteps by the floor: effect 1 at the listener. On stone it is the original's note with the
               row on or off, sample for sample; on wood (step 3 in the shipped table) its own rendering, another
               sound, higher (more zero crossings in the same time) */
            static short stone_on[RATE / 5 * 2], stone_off[RATE / 5 * 2], wood[RATE / 5 * 2];
            int vel = 0, differ = 0, zs = 0, zw = 0, dw = 0;
            g_work = argv[2];
            for (waited = 0; waited < 600 && !(uwaudio_taken() >> 1 & 1); waited++) Sleep(50);
            rooms(STONE, STONE, 1, WOOD);
            uwaudio_listener(12.0f, 1.0f, 12.0f, 0.0f);
            uwaudio_event(1, -1.0f, 1.0f, -1.0f, 1, 64);   /* (velocity 64's own rendering asked for, and waited for: both */
            quiet();                                        /* steps compared below then play that one) */
            for (waited = 0; waited < 600 && !uwaudio_rendered(1, 64); waited++) Sleep(50);
            uwaudio_event(1, -1.0f, 1.0f, -1.0f, 1, 64);
            uwaudio_render(stone_on, RATE / 5);
            quiet();
            settings("fx_footsteps 0\n"), uwaudio_tune(argv[2]);
            uwaudio_event(1, -1.0f, 1.0f, -1.0f, 1, 64);
            uwaudio_render(stone_off, RATE / 5);
            quiet();
            settings(""), uwaudio_tune(argv[2]);
            /* (OpenAL Soft dithers its 16-bit output, and the dither runs on from sound to sound: the same sound twice
               differs by a step of one here and there, so "the same" is within 2) */
            int most = 0;
            for (int i = 0; i < RATE / 5 * 2; i++) {
                int dd = abs(stone_on[i] - stone_off[i]);
                differ += dd > 2;
                if (dd > most) most = dd;
            }
            snprintf(why, sizeof why, "%d samples differ by more than the dither's 2 between the row on and off (the largest difference %d)", differ, most);
            printf("  step stone: %s\n", why);
            check("step stone", differ == 0, why);
            rooms(WOOD, STONE, 1, WOOD);
            uwaudio_event(1, -1.0f, 1.0f, -1.0f, 1, 64);   /* the first step on wood asks for its rendering */
            quiet();
            vel = 64;
            for (waited = 0; waited < 600 && !uwaudio_rendered(32 + WOOD, vel); waited++) Sleep(50);
            uwaudio_event(1, -1.0f, 1.0f, -1.0f, 1, 64);
            uwaudio_render(wood, RATE / 5);
            quiet();
            for (int i = 2; i < RATE / 5 * 2; i += 2) {
                zs += (stone_on[i] >= 0) != (stone_on[i - 2] >= 0), zw += (wood[i] >= 0) != (wood[i - 2] >= 0);
                dw += wood[i] != stone_on[i];
            }
            snprintf(why, sizeof why, "the wood step rendered %d after %d ms; %d samples differ from stone's; zero crossings: stone %d, wood %d", uwaudio_rendered(32 + WOOD, vel), waited * 50, dw, zs, zw);
            printf("  step wood: %s\n", why);
            check("step wood", uwaudio_rendered(32 + WOOD, vel) && dw > 1000 && zw > zs * 1.05, why);
            rooms(STONE, STONE, 1, WOOD);
        }
        uwaudio_shutdown();
        check("roland off", uwaudio_taken() == 0, "taken after shutdown");
    }
    return fails;
}
