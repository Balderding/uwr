/* uwlog.c: the game's message scroll kept as text (uwlog.h), for our own message box over our display (src\uwview.c's
   uwv_log): every line the main scroll is given (SCROLLIO.C's scroll_print, its port hook), its escapes taken out,
   wrapped where the game wraps it (49 letters, at the last space). Written on the game's thread, read on the window's. */
#include <stdatomic.h>
#include <string.h>
#include "uwlog.h"

#include "uwos.h"   /* GetTickCount64 (kernel32 on Windows) */

enum { N = 200 };
static char lines[N][UWLOG_W];      /* the finished lines, a ring */
static int total;                   /* lines ever finished */
static char cur[UWLOG_W];           /* the line being written (no newline yet) */
static int curlen;
static unsigned long long last_ms;  /* when the newest text came */
static atomic_flag lock = ATOMIC_FLAG_INIT;

static void take(void) { while (atomic_flag_test_and_set_explicit(&lock, memory_order_acquire)) ; }
static void give(void) { atomic_flag_clear_explicit(&lock, memory_order_release); }

static void finish(void)
{
    memcpy(lines[total % N], cur, UWLOG_W);
    total++;
    curlen = 0, cur[0] = 0;
}

/* playing an instrument (the user, 2026-11-14: "when I play the mandolin I cannot get out of playing it"): SOUND.C's
   play_instrument loops until it reads Esc (0x1B), and in our display Esc opened our menu instead. Its messages mark
   it: string 0xFA "You play the instrument. (Use 0-9 to play, or ESC to return to game)" at the start, 0xFB "You put
   the instrument down." at the end (not printed when the right tune brings up the cup: the backend clears it at the
   Esc it passes on). While on, the backend gives Esc to the game (plat_win32.c input_state's esc_menu). */
static volatile int instrument;
int uwlog_instrument(void) { return instrument; }
void uwlog_instrument_clear(void) { instrument = 0; }

/* SCROLLIO.C's scroll_print (the main scroll only), port only */
void port_scroll_note(const char *s)
{
    if (!s) return;
    if (strstr(s, "You play the instrument")) instrument = 1;
    else if (strstr(s, "You put the instrument down")) instrument = 0;
    take();
    for (; *s; s++) {
        char c = *s;
        if (c == '\\') {           /* the scroll's escapes: \0..\6 a colour, \m [MORE], \p a pause */
            if (s[1]) s++;
            continue;
        }
        if (c == '\n') { finish(); continue; }
        if ((unsigned char)c < ' ') continue;
        if (curlen >= 49) {        /* the game's own wrap: back to the last space */
            int sp = curlen;
            while (sp > 0 && cur[sp - 1] != ' ') sp--;
            if (sp > 0 && sp < curlen) {
                char rest[UWLOG_W];
                int n = curlen - sp;
                memcpy(rest, cur + sp, (size_t)n);
                cur[sp - 1] = 0, curlen = sp - 1;
                finish();
                memcpy(cur, rest, (size_t)n), curlen = n, cur[n] = 0;
            } else finish();
        }
        if (curlen < UWLOG_W - 1) cur[curlen++] = c, cur[curlen] = 0;
    }
    last_ms = GetTickCount64();
    give();
}

int uwlog_get(char (*out)[UWLOG_W], int max, int back, unsigned long long *newest_ms, int *all)
{
    int n = 0, have;
    take();
    have = (total < N ? total : N) + (curlen > 0);
    if (all) *all = have;
    if (newest_ms) *newest_ms = last_ms;
    if (back > have - max) back = have - max;
    if (back < 0) back = 0;
    /* the lines from (have - back - max) to (have - back), oldest first; the partial line is the newest */
    for (int i = have - back - max; i < have - back; i++) {
        if (i < 0) continue;
        int fin = have - (curlen > 0);          /* finished lines held */
        if (i < fin) {
            int idx = total - fin + i;
            memcpy(out[n++], lines[idx % N], UWLOG_W);
        } else memcpy(out[n++], cur, UWLOG_W);
    }
    give();
    return n;
}
