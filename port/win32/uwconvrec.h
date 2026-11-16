/* uwconvrec.h: the conversation's record (uwconvrec.c), filled by the port's read-only hooks in the original's
   CONVERSE.C and SCROLLIO.C (each under #ifndef __TURBOC__) on the game thread, read by our conversation screen on
   the window thread. The hooks only copy what the original prints and chooses; they change nothing the original
   does (the plan: docs\superpowers\plans\2026-11-13-conversation-and-map-screens.md, step A1).
   UWPORT_CONVLOG=1 also writes every event to stderr as a "conv: ..." line (tools\convcheck.py reads them). */
#ifndef UWCONVREC_H
#define UWCONVREC_H

enum {
    UWCR_LINES = 400,           /* the history kept (the original's NPC scroll shows a few lines only) */
    UWCR_TEXT = 1024,           /* one printed piece: npc_say builds it in conv_buffer (the screen buffer, no limit of
                                   its own); the Outcast's "Banner of Cabirus" line is over 300 (convcheck talk 239) */
    UWCR_OPTS = 10,             /* convo_line_option[10]: the answers a menu can have */
    UWCR_NAME = 40              /* strt_converse's name[0x28] */
};

/* scroll_mode as SCROLLIO.C sets it: where a piece was printed */
enum { UWCR_MAIN = 0, UWCR_NPC = 1, UWCR_PLAY = 2,
       UWCR_ASKED = 3 };        /* (ours) babl_ask's answer: drawn by wdialog in the message scroll, never printed */

typedef struct {
    int mode;                   /* UWCR_MAIN / _NPC / _PLAY */
    char text[UWCR_TEXT];       /* as given to scroll_print: the original's escapes (\P \1 \2 \0 ...) and '\n' kept */
} UwcrLine;

typedef struct {
    unsigned serial;            /* counts every change: a reader redraws when it differs */
    int open;                   /* strt_converse ran, free_converse not yet */
    int cnv;                    /* the conversation: CONVERSE.C cnv_id at the start */
    char npc[UWCR_NAME], player[UWCR_NAME];
    int npc_gr;                 /* the NPC's portrait: 0 CHARHEAD.GR, 1 GENHEAD.GR, -1 none */
    int npc_idx;                /* its picture there (whoami - 1, or the creature's OBJ_INMAJOR) */
    int player_idx;             /* HEADS.GR: female * 5 + body */
    int nlines, first;          /* the history: a ring of UWCR_LINES, first the oldest */
    int total;                  /* pieces printed in this conversation: the ring holds [total - nlines, total) */
    int main_from;              /* the first piece since the message scroll (UWCR_MAIN / _PLAY) was last cleared */
    UwcrLine line[UWCR_LINES];
    int nopts;                  /* the menu up (babl_menu / babl_fmenu): its answers 1..nopts, 0 none */
    char opt[UWCR_OPTS][UWCR_TEXT];
    int chosen;                 /* conv_play_menu's last choice, 0 none yet */
    int waiting;                /* scroll_wait is waiting: 1 for a time, 2 for a key or a click ([MORE], \P with no time) */
    int asking;                 /* babl_ask's wdialog is taking typed text */
    char typed[64];             /* wdialog's text so far (at most 50) and its cursor, while asking */
    int typed_pos;
} UwcrState;

/* the hooks (the game thread): see uwconvrec.c */
void port_conv_player(const char *name, int head);
void port_conv_npc(const char *name, int gr, int idx);
void port_conv_text(int mode, const char *s);
void port_conv_clear(int mode);
void port_conv_wait(int on, int ticks, int mouse);
void port_conv_option(int i, const char *s);
void port_conv_menu(int n);
void port_conv_chosen(int i);
void port_conv_ask(int on, const char *typed);
void port_conv_typing(const char *text, int pos);
void port_conv_close(void);

/* the reader (any thread): a copy taken under the record's lock */
void uwcr_snapshot(UwcrState *out);
unsigned uwcr_serial(void);

#endif
