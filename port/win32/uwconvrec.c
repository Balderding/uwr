/* uwconvrec.c: the conversation's record (uwconvrec.h). Written on the game thread by the hooks, read by a copy on
   any thread. A spin lock (C11 atomic_flag): the hooks hold it only for a copy of a line. No Windows header. */
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uwconvrec.h"

extern uint16_t cnv_id;         /* CONVERSE.C: the conversation (its CNV.ARK slot; STRINGS.PAK block 0x0E00 + it) */

static UwcrState R = { .npc_gr = -1 };
static atomic_flag lk = ATOMIC_FLAG_INIT;
static int logon = -1;

static void lock(void) { while (atomic_flag_test_and_set_explicit(&lk, memory_order_acquire)) ; }
static void unlock(void) { atomic_flag_clear_explicit(&lk, memory_order_release); }

static int logging(void)
{
    if (logon < 0) { const char *e = getenv("UWPORT_CONVLOG"); logon = e && *e && *e != '0'; }
    return logon;
}

/* a text for the log: escapes kept as they are, a newline as \n, so one event is one line */
static void logtext(const char *what, int n, const char *s)
{
    if (!logging()) return;
    char b[UWCR_TEXT * 2 + 1];
    int k = 0;
    for (; s && *s && k < (int)sizeof b - 3; s++) {
        if (*s == '\n') b[k++] = '\\', b[k++] = 'n';
        else b[k++] = *s;
    }
    b[k] = 0;
    if (n >= 0) fprintf(stderr, "conv: %s %d \"%s\"\n", what, n, b);
    else fprintf(stderr, "conv: %s \"%s\"\n", what, b);
    fflush(stderr);
}

static void copyz(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; s && s[i] && i + 1 < n; i++) d[i] = s[i];
    d[i] = 0;
}

/* strt_converse: the player's name (string_to_screen at 0x90, 0xC5) and HEADS.GR's picture. Opens the record: a new
   conversation starts with an empty history. */
void port_conv_player(const char *name, int head)
{
    lock();
    R.open = 1;
    R.cnv = cnv_id;
    copyz(R.player, name, sizeof R.player);
    R.player_idx = head;
    R.npc[0] = 0, R.npc_gr = -1, R.npc_idx = 0;
    R.nlines = R.first = R.total = R.main_from = 0;
    R.nopts = R.chosen = R.waiting = R.asking = 0;
    R.serial++;
    unlock();
    if (logging()) { fprintf(stderr, "conv: open %u player \"%s\" head %d\n", (unsigned)cnv_id, name ? name : "", head); fflush(stderr); }
}

/* strt_converse: the NPC's portrait (the one gronk_gr loaded: CHARHEAD.GR, or GENHEAD.GR when that failed or the
   NPC has no whoami) and its name (get_name; "" when get_name gave none) */
void port_conv_npc(const char *name, int gr, int idx)
{
    lock();
    copyz(R.npc, name, sizeof R.npc);
    R.npc_gr = gr, R.npc_idx = idx;
    R.serial++;
    unlock();
    if (logging()) { fprintf(stderr, "conv: npc \"%s\" %s %d\n", name ? name : "", gr == 0 ? "charhead" : "genhead", idx); fflush(stderr); }
}

/* SCROLLIO.C scroll_print in the conversation screen: one piece of text into the scroll scroll_mode names */
void port_conv_text(int mode, const char *s)
{
    lock();
    int at = (R.first + R.nlines) % UWCR_LINES;
    if (R.nlines == UWCR_LINES) R.first = (R.first + 1) % UWCR_LINES;
    else R.nlines++;
    R.line[at].mode = mode;
    copyz(R.line[at].text, s, sizeof R.line[at].text);
    R.total++;
    R.serial++;
    unlock();
    logtext(mode == UWCR_NPC ? "text npc" : mode == UWCR_PLAY ? "text play" : "text main", -1, s);
}

/* scroll_clear in the conversation screen. The NPC's history is ours and stays; the message scroll (the answers) shows
   only what came after its clear, as the original's does */
void port_conv_clear(int mode)
{
    if (mode != UWCR_NPC) {
        lock();
        R.main_from = R.total;
        R.serial++;
        unlock();
    }
    if (logging()) { fprintf(stderr, "conv: clear %s\n", mode == UWCR_NPC ? "npc" : mode == UWCR_PLAY ? "play" : "main"); fflush(stderr); }
}

/* scroll_wait in the conversation screen: on at its start, off at its end */
void port_conv_wait(int on, int ticks, int mouse)
{
    lock();
    R.waiting = on ? (ticks ? 1 : 2) : 0;
    R.serial++;
    unlock();
    if (logging()) {
        if (on) fprintf(stderr, "conv: wait %s%s\n", ticks ? "time" : "key", mouse ? " more" : "");
        else fprintf(stderr, "conv: wait end\n");
        fflush(stderr);
    }
}

/* babl_menu / babl_fmenu: answer i (1..) as shown (babl_display[i]); port_conv_menu(n) when all n are given */
void port_conv_option(int i, const char *s)
{
    if (i < 1 || i > UWCR_OPTS) return;
    lock();
    copyz(R.opt[i - 1], s, sizeof R.opt[i - 1]);
    unlock();
    logtext("option", i, s);
}

void port_conv_menu(int n)
{
    lock();
    R.nopts = n < 0 ? 0 : n > UWCR_OPTS ? UWCR_OPTS : n;
    R.serial++;
    unlock();
    if (logging()) { fprintf(stderr, "conv: menu %d\n", n); fflush(stderr); }
}

/* conv_play_menu: the answer taken (a key's number or the clicked row's); the menu is gone */
void port_conv_chosen(int i)
{
    lock();
    R.chosen = i;
    R.nopts = 0;
    R.serial++;
    unlock();
    if (logging()) { fprintf(stderr, "conv: chosen %d\n", i); fflush(stderr); }
}

/* babl_ask: wdialog taking the player's typed text (on), and what it gave (off). The original drew the typing with
   string_to_screen, not scroll_print, so the record has it only from here: the answer given goes into the history as
   a piece of the message scroll, after wdialog's ">" (and its "-" when Esc gave nothing) */
void port_conv_ask(int on, const char *typed)
{
    if (on) {
        lock();
        R.asking = 1, R.typed[0] = 0, R.typed_pos = 0;
        R.serial++;
        unlock();
        if (logging()) { fprintf(stderr, "conv: ask\n"); fflush(stderr); }
        return;
    }
    lock();
    R.asking = 0;
    int at = (R.first + R.nlines) % UWCR_LINES;
    if (R.nlines == UWCR_LINES) R.first = (R.first + 1) % UWCR_LINES;
    else R.nlines++;
    R.line[at].mode = UWCR_ASKED;
    copyz(R.line[at].text, typed, sizeof R.line[at].text);
    R.total++;
    R.serial++;
    unlock();
    logtext("asked", -1, typed);
}

/* wdialog: its text and cursor after each key (the original's own rules took the key or refused it) */
void port_conv_typing(const char *text, int pos)
{
    lock();
    copyz(R.typed, text, sizeof R.typed);
    R.typed_pos = pos < 0 ? 0 : pos;
    R.serial++;
    unlock();
    if (logging()) {
        char b[80];
        copyz(b, text, sizeof b);
        fprintf(stderr, "conv: typing %d \"%s\"\n", pos, b);
        fflush(stderr);
    }
}

/* free_converse */
void port_conv_close(void)
{
    lock();
    R.open = 0;
    R.nopts = R.waiting = R.asking = 0;
    R.serial++;
    unlock();
    if (logging()) { fprintf(stderr, "conv: close\n"); fflush(stderr); }
}

void uwcr_snapshot(UwcrState *out)
{
    lock();
    *out = R;
    unlock();
}

unsigned uwcr_serial(void)
{
    lock();
    unsigned s = R.serial;
    unlock();
    return s;
}
