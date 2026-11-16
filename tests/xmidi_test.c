/* xmidi_test: our port of MT32MPU.ADV (src/xmidi.c) plays a song exactly as tools/adv_music.py has the original
 * driver play it (init, register, every timbre asked for from UW.MT, start, serve N times) and writes the same record:
 * "tick port value" for each byte to the MPU-401 and "tick vr" for each retrace wait.
 *     xmidi_test UW.MT song.XMI seqnum ticks out.txt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmidi.h"

static FILE *g_out;
static int g_tick;
static void out(void *u, int port, int v) { fprintf(g_out, "%d %x %02x\n", g_tick, port, v & 0xff); }
static void vr(void *u) { fprintf(g_out, "%d vr\n", g_tick); }

static unsigned char *load(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *d = malloc((size_t)*n + 16);
    fread(d, 1, (size_t)*n, f);
    fclose(f);
    return d;
}

/* an AIL Global Timbre Library: (patch, bank) -> the timbre (its length word first) */
static const unsigned char *gtl_find(const unsigned char *g, long n, int bank, int patch) {
    for (long i = 0; i + 6 <= n; i += 6) {
        if (g[i + 1] == 0xff) break;
        if (g[i] == patch && g[i + 1] == bank) {
            unsigned long off = g[i + 2] | g[i + 3] << 8 | (unsigned long)g[i + 4] << 16 | (unsigned long)g[i + 5] << 24;
            return off < (unsigned long)n ? g + off : NULL;
        }
    }
    return NULL;
}

/* script mode: "xmidi_test UW.MT --script script.txt out.txt"; a line: "at TICK" (serve until then), "reg FILE.XMI N",
 * "start H", "stop H", "resume H", "release H", "vol H PCT MS", "tempo H PCT MS", "lock", "relch C", "inst BANK PATCH"
 * (from UW.MT), "cv STATUS D1 D2", "branch H MARKER", "status H", "notes H CHAN", "tstat BANK PATCH", "beat H", "bar H",
 * "ctl H CHAN CON", "setctl H CHAN CON VAL"; H is the n-th registered sequence (0, 1 ...). Return values go to the
 * record as "TICK ret VALUE". */
static int script(unsigned char *gtl, long gn, const char *path, const char *outp) {
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    g_out = fopen(outp, "w");
    XmidiIO io = {out, vr, NULL};
    xmidi_init(&io);
    int handles[8], nh = 0;
    static unsigned char *xmis[8];
    char line[256], cmd[32], a[200];
    int x, y, z;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%31s", cmd) != 1 || cmd[0] == '#') continue;
        int r = -12345;
        if (!strcmp(cmd, "at") && sscanf(line, "%*s %d", &x) == 1)
            for (; g_tick < x; g_tick++) xmidi_serve();
        else if (!strcmp(cmd, "reg") && sscanf(line, "%*s %199s %d", a, &x) == 2) {
            long xn;
            xmis[nh] = load(a, &xn);
            r = xmidi_register(xmis[nh], (size_t)xn, x, NULL);
            handles[nh++] = r;
        } else {
            x = y = z = 0;
            sscanf(line, "%*s %d %d %d", &x, &y, &z);
            int h = x >= 0 && x < nh ? handles[x] : -1;
            if (!strcmp(cmd, "start")) xmidi_start(h);
            else if (!strcmp(cmd, "stop")) xmidi_stop(h);
            else if (!strcmp(cmd, "resume")) xmidi_resume(h);
            else if (!strcmp(cmd, "release")) xmidi_release(h);
            else if (!strcmp(cmd, "vol")) xmidi_set_rel_volume(h, y, z);
            else if (!strcmp(cmd, "tempo")) xmidi_set_rel_tempo(h, y, z);
            else if (!strcmp(cmd, "lock")) r = xmidi_lock_channel();
            else if (!strcmp(cmd, "relch")) xmidi_release_channel(x);
            else if (!strcmp(cmd, "inst")) xmidi_install_timbre(x, y, gtl_find(gtl, gn, x, y));
            else if (!strcmp(cmd, "cv")) xmidi_send_cv(x, y, z);
            else if (!strcmp(cmd, "branch")) xmidi_branch_index(h, y);
            else if (!strcmp(cmd, "status")) r = xmidi_status(h);
            else if (!strcmp(cmd, "notes")) r = xmidi_channel_notes(h, y);
            else if (!strcmp(cmd, "tstat")) r = xmidi_timbre_status(x, y);
            else if (!strcmp(cmd, "beat")) r = xmidi_beat_count(h);
            else if (!strcmp(cmd, "bar")) r = xmidi_bar_count(h);
            else if (!strcmp(cmd, "ctl")) r = xmidi_control_value(h, y, z);
            else if (!strcmp(cmd, "setctl")) {
                int v = 0;
                sscanf(line, "%*s %*d %*d %*d %d", &v);
                xmidi_set_control_value(h, y, z, v);
            } else if (!strcmp(cmd, "protect")) xmidi_protect_timbre(x, y);
            else if (!strcmp(cmd, "unprotect")) xmidi_unprotect_timbre(x, y);
            else if (!strcmp(cmd, "display")) xmidi_write_display("Ultima Underworld");
            else if (!strcmp(cmd, "sysex")) {
                static const unsigned char data[4] = {1, 2, 3, 4};
                xmidi_send_sysex(x, y, z, data, 4, 2);
            } else if (!strcmp(cmd, "shutdown")) xmidi_shutdown("Goodbye");
        }
        if (r != -12345) fprintf(g_out, "%d ret %d\n", g_tick, r & 0xffff);
    }
    fclose(g_out);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 5 && !strcmp(argv[2], "--script")) {
        long gn;
        unsigned char *gtl = load(argv[1], &gn);
        return gtl ? script(gtl, gn, argv[3], argv[4]) : 1;
    }
    if (argc < 6) return fprintf(stderr, "xmidi_test UW.MT song.XMI seqnum ticks out.txt\n"), 2;
    long gn, xn;
    unsigned char *gtl = load(argv[1], &gn), *xmi = load(argv[2], &xn);
    if (!gtl || !xmi) return fprintf(stderr, "cannot read the files\n"), 1;
    int seqnum = atoi(argv[3]), ticks = atoi(argv[4]);
    g_out = fopen(argv[5], "w");
    XmidiIO io = {out, vr, NULL};
    xmidi_init(&io);
    int seq = xmidi_register(xmi, (size_t)xn, seqnum, NULL);
    if (seq < 0) return fprintf(stderr, "no sequence %d\n", seqnum), 1;
    for (;;) {
        int req = xmidi_timbre_request(seq);
        if (req == -1) break;
        xmidi_install_timbre(req >> 8, req & 0xff, gtl_find(gtl, gn, req >> 8, req & 0xff));
    }
    xmidi_start(seq);
    for (g_tick = 0; g_tick < ticks; g_tick++) xmidi_serve();
    fclose(g_out);
    return 0;
}
