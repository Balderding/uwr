/* uwacmat.c: the acoustic materials table (uwacmat.h; the acoustics plan's Task 1). Presentation only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rlog.h"
#include "uwacmat.h"
#ifdef _WIN32                          /* (the path separator: Windows' as before; "/" for the Linux build) */
#define ACMAT_SEP "\\"
#else
#define ACMAT_SEP "/"
#endif

#define MAXPRESET 32
#define MAXTEX 512          /* walls 0-209, floors 0-51, door pictures 0-12 and door items 0x140-0x14F all fit */
#define KINDS 4

/* The table we ship. The presets' numbers are the plan's (2026-11-13-sound-and-acoustics.md); "hanging" is ours: a
   cloth that covers about a third of a stone wall (0.35 carpet + 0.65 stone). Which texture is what: the game's own
   names for them (STRINGS.PAK block 10: "a finished oak wall", "a tapestry", "a dirt floor", "water", ...) and a look
   at each picture (2026-11-14, work\acoustics\*.png). Judgements, where the name does not settle it:
   - a wall that is stone with a small thing on it (a grating, a drain, a pipe, a thin waterfall) stays stone;
   - stucco, brick and marble are hard and smooth enough to count as stone;
   - lava has no preset of its own: a dense liquid surface, taken as water;
   - walls 38 and 39 (the locked doors of the Abyss) are riveted plates in the picture: metal;
   - door pictures 5, 6, 10 and 12 are iron (10 and 12 corroded green), 9 is a stone slab, the rest planks.
   transmit by band (2026-11-14, the user's choice after reading up on it): wood and stone are Steam Audio's "wood" and
   "rock" presets (low, mid, high: ValveSoftware/steam-audio issue 330 quotes them); metal is OUR ESTIMATE, an iron
   door about twice a plank door's weight, so about 6 dB less than wood by the mass law (Steam Audio's "metal" is a
   thin sheet, which lets more through than wood). Only shut doors are passed through (the plan: through rock
   nothing), so the presets no door is made of keep 0.
   absorb, stone (2026-11-14, after the user's first listen: "the walking sound is quite long"): the Abyss is rough-hewn
   rock, not dressed blocks. Dressed stone's 0.02 0.03 0.04 made the first room of level 1 ring 8.9 / 6.0 / 4.5 s
   (a footstep still clear 3 s on); rough rock's 0.13 0.20 0.24 gives it 1.4 / 0.9 / 0.7 s.
   step (the plan's Task 6, to be judged by ear): a footstep's note on that floor, in semitones from the original's
   (stone 0): wood +3, water -5, dirt -2, carpet -2, metal +7. */
static const char DEFAULT_TEXT[] =
    "# acoustics.txt: what the level's surfaces are made of, for the sound (port\\win32\\uwacmat.h has the format).\n"
    "# Edit freely; delete it to have this table written again. The last matching line wins.\n"
    "# absorb: the share of the sound taken at a bounce (low, mid, high); transmit: the share passing through\n"
    "# a shut door (low, mid, high; one number: all three bands). step: a footstep on such a floor sounds this many\n"
    "# semitones higher (or lower) than on stone, the original's note.\n"
    "preset stone   absorb 0.13 0.20 0.24  transmit 0.015 0.002 0.001\n"
    "preset wood    absorb 0.10 0.08 0.10  transmit 0.070 0.014 0.005 step 3\n"
    "preset carpet  absorb 0.05 0.25 0.55  transmit 0.00 step -2\n"
    "preset metal   absorb 0.02 0.02 0.03  transmit 0.018 0.004 0.001 step 7\n"
    "preset water   absorb 0.01 0.01 0.02  transmit 0.00 step -5\n"
    "preset dirt    absorb 0.15 0.30 0.45  transmit 0.00 step -2\n"
    "preset hanging absorb 0.03 0.11 0.22  transmit 0.00\n"
    "wall * stone\n"
    "floor * stone\n"
    "ceiling * stone\n"
    "door * wood\n"
    "# walls: finished oak (112 with a grating, 113 with a pipe)\n"
    "wall 108 wood\nwall 109 wood\nwall 110 wood\nwall 111 wood\nwall 112 wood\nwall 113 wood\n"
    "# walls: a tapestry, banner or standard hanging on the stone\n"
    "wall 005 hanging\nwall 022 hanging\nwall 041 hanging\nwall 077 hanging\nwall 081 hanging\nwall 103 hanging\n"
    "wall 120 hanging\nwall 133 hanging\nwall 172 hanging\n"
    "wall 006 hanging\nwall 059 hanging\nwall 155 hanging\n"
    "wall 009 hanging\nwall 010 hanging\nwall 018 hanging\nwall 062 hanging\nwall 091 hanging\nwall 123 hanging\n"
    "wall 124 hanging\nwall 125 hanging\nwall 159 hanging\n"
    "# walls: the locked doors of the Abyss (riveted plates)\n"
    "wall 038 metal\nwall 039 metal\n"
    "# walls: a waterfall, a lavafall (the whole face), a wall of moss\n"
    "wall 198 water\nwall 206 water\nwall 138 dirt\n"
    "# floors: dirt\n"
    "floor 005 dirt\nfloor 006 dirt\nfloor 007 dirt\nfloor 008 dirt\nfloor 009 dirt\nfloor 010 dirt\nfloor 011 dirt\n"
    "floor 018 dirt\nfloor 019 dirt\nfloor 020 dirt\nfloor 021 dirt\nfloor 022 dirt\nfloor 035 dirt\n"
    "floor 037 dirt\nfloor 038 dirt\nfloor 039 dirt\nfloor 040 dirt\n"
    "# floors: water; lava and rivulets of lava (no preset of its own: as water)\n"
    "floor 016 water\nfloor 017 water\nfloor 032 water\nfloor 033 water\nfloor 034 water\n"
    "floor 023 water\nfloor 024 water\nfloor 025 water\nfloor 041 water\n"
    "# floors: a straw bed\n"
    "floor 042 carpet\n"
    "# doors by their picture (data\\sprites\\doors): iron, and a stone slab; the rest are planks\n"
    "door 5 metal\ndoor 6 metal\ndoor 10 metal\ndoor 12 metal\ndoor 9 stone\n"
    "# doors by their item: the portcullis (shut 0x146, open 0x14E), the secret door (0x147, 0x14F: it is the wall)\n"
    "door 0x146 metal\ndoor 0x14E metal\ndoor 0x147 stone\ndoor 0x14F stone\n";

static UwaMaterial presets[MAXPRESET];
static int npresets;
static signed char table[KINDS][MAXTEX];     /* the preset of each number */
static unsigned char own[KINDS][MAXTEX];     /* 1: a numbered line set it (0: a '*' line, or nothing) */
static signed char fallback[KINDS];

const char *uwacmat_default(void) { return DEFAULT_TEXT; }
int uwacmat_count(void) { return npresets; }
const UwaMaterial *uwacmat_get(int preset) { return preset >= 0 && preset < npresets ? &presets[preset] : NULL; }

int uwacmat_for(int kind, int tex)
{
    if (!npresets || kind < 0 || kind >= KINDS) return -1;
    if (tex < 0 || tex >= MAXTEX) return fallback[kind];
    /* the ceiling is a floor picture (the level's floor slot 9, GRIDDB.C:606): without a ceiling line of its own
       it is what that picture is as a floor (an earth ceiling on levels 5 and 6), else the ceilings' '*' */
    if (kind == UWA_CEILING && !own[UWA_CEILING][tex] && own[UWA_FLOOR][tex]) return table[UWA_FLOOR][tex];
    return table[kind][tex];
}

int uwacmat_door(int item, int picture)
{
    if (!npresets) return -1;
    if (item >= 0 && item < MAXTEX && own[UWA_DOOR][item]) return table[UWA_DOOR][item];
    if (picture >= 0 && picture < MAXTEX && own[UWA_DOOR][picture]) return table[UWA_DOOR][picture];
    return fallback[UWA_DOOR];
}

static void clear(void)
{
    npresets = 0;
    memset(table, 0, sizeof table), memset(own, 0, sizeof own), memset(fallback, 0, sizeof fallback);
}
static int preset_named(const char *name)
{
    for (int i = 0; i < npresets; i++)
        if (!strcmp(presets[i].name, name)) return i;
    return -1;
}
static int in01(float v) { return v >= 0.0f && v <= 1.0f; }

/* one line (its comment and line end already cut); NULL when it is good, else what is wrong with it */
static const char *line_read(const char *s)
{
    static const char *kinds[KINDS] = {"floor", "ceiling", "wall", "door"};   /* in the enum's order */
    char a[16], b[16], c[16], extra;
    int n = sscanf(s, "%15s %15s %15s", a, b, c);
    if (n <= 0) return NULL;                                   /* an empty line */
    if (!strcmp(a, "preset")) {
        UwaMaterial m;
        int used = 0, step = 0, more = 0, got;
        memset(&m, 0, sizeof m);
        if (sscanf(s, " preset %15s absorb %f %f %f transmit %f%n", m.name, &m.absorb[0], &m.absorb[1], &m.absorb[2], &m.transmit[0], &used) != 5 || !used)
            return "a preset is \"preset NAME absorb LOW MID HIGH transmit LOW MID HIGH [step N]\"";
        got = sscanf(s + used, " %f %f%n", &m.transmit[1], &m.transmit[2], &more);   /* three bands, or one number for all */
        if (got == 2 && more) used += more;
        else if (got <= 0) m.transmit[1] = m.transmit[2] = m.transmit[0];
        else return "a transmit is one number or three (low, mid, high)";
        if (!in01(m.absorb[0]) || !in01(m.absorb[1]) || !in01(m.absorb[2]) || !in01(m.transmit[0]) || !in01(m.transmit[1]) || !in01(m.transmit[2]))
            return "a preset's numbers run from 0 to 1";
        if (sscanf(s + used, " %15s", a) == 1) {               /* something after the transmit: only "step N" */
            if (sscanf(s + used, " step %d %c", &step, &extra) != 1 || step < -24 || step > 24) return "after the transmit only \"step N\" may follow (N from -24 to 24)";
        }
        m.step = step;
        int at = preset_named(m.name);
        if (at < 0) {
            if (npresets >= MAXPRESET) return "too many presets (32 at most)";
            at = npresets++;
        }
        presets[at] = m;
        return NULL;
    }
    int kind = -1;
    for (int k = 0; k < KINDS; k++)
        if (!strcmp(a, kinds[k])) kind = k;
    if (kind < 0) return "a line starts with preset, wall, floor, ceiling or door";
    if (n != 3 || sscanf(s, "%15s %15s %15s %c", a, b, c, &extra) != 3) return "a texture line is \"KIND NUMBER PRESET\" (NUMBER may be *)";
    int p = preset_named(c);
    if (p < 0) return "a preset that no line above defines";
    if (!strcmp(b, "*")) {
        memset(table[kind], p, MAXTEX), memset(own[kind], 0, MAXTEX);
        fallback[kind] = (signed char)p;
        return NULL;
    }
    char *end;
    int hex = b[0] == '0' && (b[1] == 'x' || b[1] == 'X');
    long t = strtol(hex ? b + 2 : b, &end, hex ? 16 : 10);     /* "012" is twelve: the file's name, never octal */
    if (end == (hex ? b + 2 : b) || *end) return "the number is neither digits, 0x and hex digits, nor *";
    if (t < 0 || t >= MAXTEX) return "the number is out of range (0 to 511)";
    table[kind][t] = (signed char)p, own[kind][t] = 1;
    return NULL;
}

/* the table's text (changed in place: cut into lines) read; name: what the log calls it */
static int text_read(char *text, const char *name)
{
    int no = 0;
    clear();
    for (char *s = text; *s; ) {
        char *e = s + strcspn(s, "\r\n"), *next = e + strspn(e, "\r"), *h;
        if (*next == '\n') next++;
        *e = 0, no++;
        if ((h = strchr(s, '#'))) *h = 0;
        const char *why = line_read(s);
        if (why) {
            RLOG_W("fx", "acoustics: %s line %d: %s", name, no, why);
            clear();
            return 0;
        }
        s = next;
    }
    if (!npresets) { RLOG_W("fx", "acoustics: %s defines no preset", name); return 0; }
    return 1;
}

int uwacmat_load(const char *datadir)
{
    static char text[65536];
    char path[600];
    clear();
    snprintf(path, sizeof path, "%s" ACMAT_SEP "acoustics.txt", datadir ? datadir : ".");
    FILE *f = fopen(path, "rb");
    if (!f) { RLOG_W("fx", "acoustics: no %s", path); return 0; }
    size_t len = fread(text, 1, sizeof text - 1, f);
    int whole = feof(f);
    fclose(f);
    text[len] = 0;
    if (!whole) { RLOG_W("fx", "acoustics: %s is longer than %d bytes", path, (int)sizeof text - 1); return 0; }
    return text_read(text, path);
}

int uwacmat_load_default(void)
{
    static char text[sizeof DEFAULT_TEXT];
    memcpy(text, DEFAULT_TEXT, sizeof text);
    return text_read(text, "the table in the code");
}

int uwacmat_write_default(const char *datadir)
{
    char path[600];
    snprintf(path, sizeof path, "%s" ACMAT_SEP "acoustics.txt", datadir ? datadir : ".");
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 0; }
    if (!(f = fopen(path, "wb"))) { RLOG_E("fx", "acoustics: cannot write %s: %s", path, rlog_errno()); return -1; }
    size_t n = strlen(DEFAULT_TEXT), w = fwrite(DEFAULT_TEXT, 1, n, f);
    return fclose(f) == 0 && w == n ? 1 : -1;
}
