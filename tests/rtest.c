/* rtest: runs uo-new's ports of UW.EXE routines on lines from stdin, for tools/verify.py (and tools/sfx_verify.py) to
 * compare with the original run in an emulator.
 *   default mode, each line "<seed hex> <routine> <args...>": "<result> <seed after, hex>"
 *   "rtest sfx SOUNDS.DAT": the effects (sfx.c); each line one operation ("play ID PAN VOL", "at ID X Y VOL PX PY
 *   HEADING", "tick", "stop SLOT", "stopall", "tstat N" (what the next timbre status answers), "lock N" (what the
 *   next channel lock answers)); every call to the driver is written as it happens, then "ret R" and the state. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "orig.h"
#include "sfx.h"
#include "songs.h"
#include "collide.h"
#include "songs.h"
#include "collide.h"

static int g_tstat, g_lock;
static int ail_tstat(int bank, int patch) { return printf("tstat %d %d\n", bank, patch), g_tstat; }
static bool ail_install(int bank, int patch) { return printf("install %d %d\n", bank, patch), true; }
static int ail_lock(void) { return printf("lock\n"), g_lock; }
static void ail_release(int ch) { printf("release %d\n", ch); }
static void ail_cv(int st, int d1, int d2) { printf("cv %d %d %d\n", st & 0xffff, d1 & 0xffff, d2 & 0xffff); }

static int sfx_mode(const char *sounds) {
    static unsigned char dat[4096];
    FILE *f = fopen(sounds, "rb");
    if (!f) return 1;
    size_t n = fread(dat, 1, sizeof dat, f);
    fclose(f);
    SfxAil ail = {ail_tstat, ail_install, ail_lock, ail_release, ail_cv};
    sfx_init(&ail, dat, n, 6);
    char line[256], cmd[32];
    int a[8];
    while (fgets(line, sizeof line, stdin)) {
        memset(a, 0, sizeof a);
        if (sscanf(line, "%31s %d %d %d %d %d %d %d", cmd, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]) < 1) continue;
        int r = -1;
        if (!strcmp(cmd, "tstat")) {
            g_tstat = a[0];
            continue;
        }
        if (!strcmp(cmd, "lock")) {
            g_lock = a[0];
            continue;
        }
        if (!strcmp(cmd, "play")) r = sfx_play(a[0], a[1], a[2]);
        else if (!strcmp(cmd, "at")) r = sfx_play_at(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
        else if (!strcmp(cmd, "tick")) sfx_tick();
        else if (!strcmp(cmd, "stop")) sfx_stop(a[0]);
        else if (!strcmp(cmd, "stopall")) sfx_stop_all();
        else continue;
        const Sfx *s = &g_sfx;
        printf("ret %d state %d %d", r & 0xff, s->active, s->chans);
        for (int k = 0; k < 4; k++) printf(" %d %d %d %d", s->chan[k], s->time[k] & 0xffff, s->note[k], s->timbre[k]);
        printf("\n");
    }
    return 0;
}

/* "rtest songs": the music choice (songs.c); each line one operation (see tools/songs_verify.py): the driver's calls
 * as they happen, then "state" */
static struct {
    int file, reg, inst, status, timbres[8], ntimbres;
} g_ans = {1, 0, 1, 1, {0}, 0};
static bool s_file(const char *name) { return printf("file %s\n", name), g_ans.file != 0; }
static int s_register(void) { return printf("register\n"), g_ans.reg; }
static void s_release(int seq) { printf("release %d\n", seq & 0xffff); }
static int s_request(int seq) {
    printf("request %d\n", seq & 0xffff);
    if (!g_ans.ntimbres) return -1;
    int t = g_ans.timbres[0];
    memmove(g_ans.timbres, g_ans.timbres + 1, sizeof g_ans.timbres - sizeof g_ans.timbres[0]);
    g_ans.ntimbres--;
    return t;
}
static bool s_install(int bank, int patch) { return printf("install %d %d\n", bank, patch), g_ans.inst != 0; }
static void s_start(int seq) { printf("start %d\n", seq & 0xffff); }
static void s_stop(int seq) { printf("stop %d\n", seq & 0xffff); }
static int s_status(int seq) { return printf("status %d\n", seq & 0xffff), g_ans.status; }
static void s_volume(int seq, int pct, int ms) { printf("volume %d %d %d\n", seq & 0xffff, pct, ms); }
static void s_tempo(int seq, int pct, int ms) { printf("tempo %d %d %d\n", seq & 0xffff, pct, ms); }
static int songs_mode(void) {
    static const SongsAil ail = {s_file, s_register, s_release, s_request, s_install, s_start, s_stop, s_status, s_volume, s_tempo, uw_rand};
    songs_init(&ail, true);
    char line[256], cmd[32];
    long long a[8];
    while (fgets(line, sizeof line, stdin)) {
        memset(a, 0, sizeof a);
        int n = sscanf(line, "%31s %lld %lld %lld %lld %lld %lld %lld", cmd, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]);
        if (n < 1) continue;
        if (!strcmp(cmd, "seed")) {
            unsigned s;
            if (sscanf(line + 5, "%x", &s) == 1) uw_set_seed(s);
            continue;
        }
        if (!strcmp(cmd, "loaded")) {
            g_songs.loaded = a[0] != 0;
            continue;
        }
        if (!strcmp(cmd, "file")) {
            g_ans.file = (int)a[0];
            continue;
        }
        if (!strcmp(cmd, "reg")) {
            g_ans.reg = (int)a[0];
            continue;
        }
        if (!strcmp(cmd, "inst")) {
            g_ans.inst = (int)a[0];
            continue;
        }
        if (!strcmp(cmd, "status")) {
            g_ans.status = (int)a[0];
            continue;
        }
        if (!strcmp(cmd, "timbres")) {
            g_ans.ntimbres = n - 1;
            for (int i = 0; i < n - 1; i++) g_ans.timbres[i] = (int)a[i];
            continue;
        }
        if (!strcmp(cmd, "combat")) {
            g_songs.combat_t = (u32)a[0];
            continue;
        }
        if (!strcmp(cmd, "play")) songs_play((int)a[0], a[1] != 0);
        else if (!strcmp(cmd, "resume")) songs_resume();
        else if (!strcmp(cmd, "enable")) songs_enable(a[0] != 0);
        else if (!strcmp(cmd, "toggle")) songs_toggle();
        else if (!strcmp(cmd, "next")) songs_set_next((int)a[0]);
        else if (!strcmp(cmd, "random")) songs_next_random();
        else if (!strcmp(cmd, "restart")) songs_restart_if_over();
        else if (!strcmp(cmd, "update")) songs_update((u32)a[0], a[1] != 0, a[2] != 0);
        else continue;
        const Songs *s = &g_songs;
        printf("state %d %d %d %d %d %u %u %u\n", s->cur, s->next, s->handle & 0xffff, s->loaded, s->on, (unsigned)s->combat_t,
               (unsigned)s->switch_t, (unsigned)uw_seed());
    }
    return 0;
}

/* "rtest collide LEV.ARK TERRAIN.DAT": the collision (collide.c); lines "level L", "tc X Y Z HEADING RADIUS RANGE"
 * (TerrainCheck then ComputeHeading: the record after), "slope X Y" (GetSlopeHgt after the last check) */
static int collide_mode(const char *ark_path, const char *terrain_path, const char *comobj_path) {
    static unsigned char ark[600000], ter[2048], com[8192], block[0x7c08];
    FILE *f = fopen(ark_path, "rb");
    if (!f) return 1;
    size_t an = fread(ark, 1, sizeof ark, f);
    fclose(f);
    f = fopen(terrain_path, "rb");
    if (!f) return 1;
    fread(ter, 1, sizeof ter, f);
    fclose(f);
    f = fopen(comobj_path, "rb");
    if (!f) return 1;
    fread(com, 1, sizeof com, f);
    fclose(f);
    static CollWorld w;
    char line[256], cmd[32];
    long a[8];
    while (fgets(line, sizeof line, stdin)) {
        memset(a, 0, sizeof a);
        int n = sscanf(line, "%31s %ld %ld %ld %ld %ld %ld %ld", cmd, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]);
        if (n < 1) continue;
        if (!strcmp(cmd, "level")) {
            unsigned lb = ark[2 + (a[0] - 1) * 4] | ark[3 + (a[0] - 1) * 4] << 8 | (unsigned)ark[4 + (a[0] - 1) * 4] << 16;
            unsigned tb = ark[2 + (17 + a[0]) * 4] | ark[3 + (17 + a[0]) * 4] << 8 | (unsigned)ark[4 + (17 + a[0]) * 4] << 16;
            if (lb + 0x7c08 > an || tb + 116 > an) return 2;
            memcpy(block, ark + lb, sizeof block);
            for (int i = 0; i < 10; i++) {
                int id = ark[tb + 96 + i * 2] | ark[tb + 97 + i * 2] << 8;
                w.terr[i] = (u16)(ter[0x200 + id * 2] | ter[0x201 + id * 2] << 8);
            }
            w.block = block, w.com = com + 2;
            coll_world(&w);
            continue;
        }
        if (!strcmp(cmd, "oc")) { /* ObjectCheck then process_objlist: X Y Z RADIUS HEIGHT INDEX FLAT|USE<<1 */
            MotionCalc c = {0};
            c.x = (i16)a[0], c.y = (i16)a[1], c.z = (i16)a[2], c.radius = (u8)a[3], c.height = (u8)a[4], c.index = (i16)a[5];
            coll_object_check(&c, (u8)(a[6] & 1), (u8)(a[6] >> 1));
            coll_process_objlist(&c);
            printf("oc %d %d %d", c.found, c.count, c.first);
            for (int i = 0; i < c.found && i < 9; i++)
                printf(" %d/%d/%d/%d", coll_objects[i].top, coll_objects[i].bottom, coll_objects[i].link, coll_objects[i].offset & 0xffff);
            printf("\n");
            continue;
        }
        if (!strcmp(cmd, "cp")) { /* can_place: ITEM INDEX X Y Z FLIER RANGE */
            int ok = coll_can_place((int)a[0], (int)a[1], (int)a[2], (int)a[3], (int)a[4], (u8)a[5], (u8)a[6]);
            printf("cp %d %d %d\n", ok, coll_place_height & 0xffff, coll_place_terrain & 0xffff);
            continue;
        }
        if (!strcmp(cmd, "tc")) {
            MotionCalc c = {0};
            c.x = (i16)a[0], c.y = (i16)a[1], c.z = (i16)a[2], c.heading = (u16)a[3], c.radius = (u8)a[4];
            coll_terrain_check(&c, (u8)a[5]);
            coll_compute_heading(&c);
            printf("tc %d %d %d %d %d %d %d %d\n", c.hits0 & 0xffff, c.hits1 & 0xffff, c.floor, c.top, c.slope, c.open, c.x & 0xffff,
                   c.y & 0xffff);
        } else if (!strcmp(cmd, "slope")) printf("slope %d\n", coll_get_slope_hgt((int)a[0], (int)a[1]) & 0xffff);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 5 && !strcmp(argv[1], "collide")) return collide_mode(argv[2], argv[3], argv[4]);
    if (argc >= 3 && !strcmp(argv[1], "sfx")) return sfx_mode(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "songs")) return songs_mode();
    char line[512], name[64];
    unsigned seed;
    int a[8] = {0};
    while (fgets(line, sizeof line, stdin)) {
        int n = sscanf(line, "%x %63s %d %d %d %d %d %d", &seed, name, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
        if (n < 2) continue;
        uw_set_seed(seed);
        if (!strcmp(name, "getexp") || !strcmp(name, "killexp")) { /* exp lvl sp earned (gain | crit_exp powerful) dungeon */
            unsigned long long v[7] = {0};
            sscanf(line, "%*x %*s %llu %llu %llu %llu %lld %lld %lld", &v[0], &v[1], &v[2], &v[3], (long long *)&v[4], (long long *)&v[5], (long long *)&v[6]);
            UwExp p = {(u32)v[0], (u8)v[1], (u8)v[2], (u8)v[3]};
            if (name[0] == 'g') uw_player_get_exp(&p, (int)(long long)v[4], (int)(long long)v[5]);
            else uw_player_get_exp(&p, uw_kill_exp((int)v[4], v[5] != 0), (int)(long long)v[6]);
            printf("%u,%d,%d,%d %08x\n", (unsigned)p.exp, p.level, p.skill_points, p.earned, (unsigned)uw_seed());
            continue;
        }
        int r = 0;
        if (!strcmp(name, "rand")) r = uw_rand();
        else if (!strcmp(name, "dice")) r = uw_dice(a[0], a[1]);
        else if (!strcmp(name, "check")) r = uw_skill_check(a[0], a[1]);
        else if (!strcmp(name, "resist")) r = uw_resist(a[0], a[1], a[2]);
        else if (!strcmp(name, "isqrt")) r = uw_isqrt((unsigned)a[0] << 16 | (unsigned)a[1]);
        else if (!strcmp(name, "sin")) {
            int s, c;
            uw_sincos((unsigned short)a[0], &s, &c);
            r = (s & 0xffff) | (c & 0xffff) << 16;
        } else r = -99999;
        printf("%d %08x\n", r, (unsigned)uw_seed());
    }
    return 0;
}
