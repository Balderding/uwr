/* uwconv.c: lays out the original's full-screen pieces in our display (see uwconv.h): a conversation, the options
   panel, the automap, typed text, and the cutscene windows. It works out the whole-number scale and where each piece
   goes, hands the layer to the renderer DLL to draw (src\uwconv_draw.c's uwv_conv and friends, found by name so that
   this file needs no Windows header), draws our text from the conversation record (uwconvrec.c) and our map from the game's map data (PlayersMap,
   ATM_Strings) over it, and turns a window point back into the original's screen point for the mouse. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "uwconv.h"
#include "rlog.h"

extern int16_t scrnum;          /* UWEDIT.C: 0 the 3D view, 1 the automap, 2 a conversation */
#include "uwos.h"   /* GetProcAddress (kernel32's on Windows; dlsym elsewhere) */

typedef const unsigned char *(*ConvFn)(const unsigned *src, int sw, int sh, int uw, int uh, const int *pieces, int n,
                                       int scale, int dim);
static ConvFn conv_fn;
typedef const unsigned char *(*CutFn)(const unsigned *src, int sw, int sh, int uw, int uh, int x0, int y0, int w, int h,
                                      int dx, int dy, int scale, int dim);
static CutFn cut_fn;
typedef int (*TextFn)(const unsigned *src, int sw, int sh, int x, int y, int w, int h, int dx, int dy, int scale,
                      const unsigned *erase, int nerase, const char *text, const unsigned *inks, int text_x,
                      int line_rows, int back, unsigned paper, int *rows, int rows_n);
static TextFn text_fn;          /* the DLL's uwv_conv_text: our text in the conversation or NULL */
typedef int (*MapFn)(const unsigned *src, int sw, int sh, int dx, int dy, int k, const unsigned char *pm,
                     const unsigned *pal, const char *notes, const short *nx, const short *ny, int n, const char *typing,
                     int tx, int ty, int mx, int my, int cursor_on, int cx, int cytop, int cw, int ch, int cpic);
static MapFn map_fn;            /* the DLL's uwv_map: our map (the plan's Part B), or NULL */
/* the pieces: the original's screen rectangle (x, y, w, h; y down), and where it goes in the layer (dx, dy) */
enum { NP = 3, GAP = 6 };
static int P[NP][6] = {
    { 42, 0, 192, 137, 0, 0 },    /* the conversation area: names, portraits, trade slots, the NPC's scroll */
    { 234, 0, 86, 122, 0, 0 },    /* the right panel: the player's inventory */
    { 0, 166, 320, 34, 0, 0 },    /* the message scroll: the player's answers */
};
static int K = 1, shown, lw, lh, wcw, wch;

/* ---- our conversation text ----
   The original's screen layout and art stay, centred; the four places it writes text are drawn by us from the
   conversation's record (uwconvrec.c): its letters erased there, ours drawn at the window's resolution (settings.txt
   conv_font: smooth, or the original's font scaled), and the NPC's scroll keeps the whole conversation, scrolled back
   with the wheel (uwc_wheel). The input is still the original's: our text sits where the original's did, so a click
   on an answer is a click on the original's line. */
#include "uwconvrec.h"
unsigned plat_pal_rgb(int i);   /* plat_win32.c: palette colour i now, 0x00RRGGBB as the screen picture */
static UwcrState *rec;          /* the record's last copy (taken when its serial changes) */
static unsigned rec_serial = ~0u;
static int conv_back, conv_lines; /* the NPC's scroll: lines scrolled back by the wheel; lines it made last frame */
enum { MAIN_ROWS = 5 };            /* the message scroll's rows: (0x1E - 1 + 1) / 6 */
static int main_answer[MAIN_ROWS]; /* the answer (1..) whose line is drawn on each row of it, 0 none */
enum { STREAM = 64 * 1024 };
static char npc_text[STREAM], main_text[STREAM];

/* the original's escapes to our colour bytes: \0..\6 to 1..7; \P \p \m (waits, [MORE]) taken out; the rest kept */
static int add_text(char *out, int len, const char *s)
{
    for (; *s && len < STREAM - 2; s++) {
        if (*s == '\\' && s[1]) {
            if (s[1] >= '0' && s[1] <= '6') out[len++] = (char)(1 + s[1] - '0');
            s++;
            continue;
        }
        out[len++] = *s;
    }
    out[len] = 0;
    return len;
}

/* builds the two text streams (the NPC's scroll and the message scroll) from the conversation record */
static void build_streams(void)
{
    int nl = 0, nm = 0;
    npc_text[0] = main_text[0] = 0;
    if (!rec || !rec->open) return;
    int base = rec->total - rec->nlines;
    npc_text[nl++] = 1, main_text[nm++] = 1;   /* both scrolls start in \0's ink (font_color 0x2E) */
    for (int k = 0; k < rec->nlines; k++) {
        const UwcrLine *l = &rec->line[(rec->first + k) % UWCR_LINES];
        if (l->mode == UWCR_NPC) nl = add_text(npc_text, nl, l->text);
        else if (base + k >= rec->main_from) nm = add_text(main_text, nm, l->text);
        /* a typed answer (babl_ask) also into the NPC's scroll, as the original echoes a chosen one (play_say: \1) */
        if (l->mode == UWCR_ASKED && l->text[0] && nl < STREAM - UWCR_TEXT - 8) {
            npc_text[nl++] = 2;
            nl = add_text(npc_text, nl, l->text);
            npc_text[nl++] = 1, npc_text[nl++] = '\n', npc_text[nl] = 0;
        }
    }
    if (rec->waiting == 2 && nl < STREAM - 16) nl = add_text(npc_text, nl, "\n\\3[MORE]");   /* scroll_more's wait */
    npc_text[nl] = main_text[nm] = 0;
}

/* draws our text over the conversation layer: the NPC's scroll, the message scroll with the answer rows (and the
   typing bar), and the two names. Places are the original's screen (y down; the scrolls' fields in SCROLLIO.C are y
   up; the names' string_to_screen in strt_converse), each in its piece of P */
static void draw_text(const unsigned *src, int sw, int sh)
{
    if (!text_fn) return;
    if (!rec) rec = (UwcrState *)calloc(1, sizeof *rec);
    if (!rec) {
        static int said;
        if (!said) said = 1, RLOG_E("conv", "out of memory (%zu bytes)", sizeof *rec);
        return;
    }
    unsigned s = uwcr_serial();
    if (s != rec_serial) {
        int had = rec->total;
        uwcr_snapshot(rec);
        if (rec->total != had) conv_back = 0;   /* new text: back to the newest */
        rec_serial = s;
        build_streams();
    }
    static const int CODE[7] = {0x2E, 0x26, 0xF1, 0x60, 0xB4, 0xC4, 0xD4};   /* scroll_print3's \0..\6 */
    unsigned inks[7], name_ink[7];
    for (int i = 0; i < 7; i++) inks[i] = plat_pal_rgb(CODE[i]), name_ink[i] = plat_pal_rgb(0x65);
    /* npc_scroll { y0 0x94, y1 0x44, left 0x38, right 0xDB, text from 0x3B }: rows 199-0x94 .. 199-0x44 */
    int px = P[0][4], py = P[0][5];
#define AT(p, ox, oy) P[p][4] + ((ox) - P[p][0]) * K, P[p][5] + ((oy) - P[p][1]) * K
    unsigned paper = plat_pal_rgb(0x2A);   /* the scrolls' paper (scroll_clear's colour) */
    conv_lines = text_fn(src, sw, sh, 0x38, 199 - 0x94, 0xDB - 0x38 + 1, 0x94 - 0x44 + 1, AT(0, 0x38, 199 - 0x94), K,
                         inks, 7, npc_text, inks, 0x3B, 6, conv_back, paper, NULL, 0);
    /* main_scroll { y0 0x1E, y1 1, left 0x0F, right 0x131 }: rows 169 .. 198; each row's answer: the paragraph its
       line is in, when that starts "N. " (the menu's lines, conv_choice / conv_fmenu) */
    int rows[MAIN_ROWS];
    for (int r = 0; r < MAIN_ROWS; r++) rows[r] = -1;
    /* babl_ask's typing (wdialog: its text so far, after its ">"): ours, with a bar at its cursor */
    const char *mt = main_text;
    static char ask_text[STREAM + 80];
    if (rec->asking) {
        int n = 0, p = rec->typed_pos;
        while (main_text[n] && n < STREAM) ask_text[n] = main_text[n], n++;
        for (int i = 0; rec->typed[i] || i == p; i++) {
            if (i == p) ask_text[n++] = '|';
            if (!rec->typed[i]) break;
            ask_text[n++] = rec->typed[i];
        }
        ask_text[n] = 0;
        mt = ask_text;
    }
    text_fn(src, sw, sh, 0x0F, 199 - 0x1E, 0x131 - 0x0F + 1, 0x1E - 1 + 1, AT(2, 0x0F, 199 - 0x1E), K,
            inks, 7, mt, inks, 0x0F, 6, 0, paper, rows, MAIN_ROWS);
    for (int r = 0; r < MAIN_ROWS; r++) {
        main_answer[r] = 0;
        if (rows[r] < 0 || !rec->open || rec->nopts <= 0) continue;
        int a = rows[r];
        while (a > 0 && main_text[a - 1] != '\n') a--;
        while (main_text[a] >= 1 && main_text[a] <= 7) a++;
        if (main_text[a] >= '1' && main_text[a] <= '9' && main_text[a + 1] == '.' && main_text[a] - '0' <= rec->nopts)
            main_answer[r] = main_text[a] - '0';
    }
    /* the names: string_to_screen(name, 0x30 / 0x90, 0xC5) on the two plates (rows 1..8), in colour 0x65 */
    unsigned name_erase = plat_pal_rgb(0x65);
    char nb[UWCR_NAME + 2];
    nb[0] = 1;
    for (int i = 0; i < 2; i++) {
        const char *nm = rec->open ? (i ? rec->player : rec->npc) : "";
        int k = 0;
        while (nm[k] && k < UWCR_NAME) nb[1 + k] = nm[k], k++;
        nb[1 + k] = 0;
        int x = i ? 0x8B : 0x2D, tx = i ? 0x90 : 0x30;
        /* (rows 2..8: the original's letters start on row 2, 199 - 0xC5) */
        text_fn(src, sw, sh, x, 2, 0x5C, 7, AT(0, x, 2), K, &name_erase, 1, rec->open ? nb : "", name_ink, tx, 7, 0,
                0xFFFFFFFFu, NULL, 0);
    }
#undef AT
    (void)px, (void)py;
}

/* the answer under a window point: the message scroll's row there (6 of the original's rows each, from row 169) */
int uwc_answer_at(int cx, int cy)
{
    float x, y;
    if (!text_fn || !uwc_talking() || !uwc_shown() || !uwc_point(cx, cy, &x, &y)) return 0;
    if (x < 0x0F || x > 0x131 || y < 199 - 0x1E || y >= 199 - 0x1E + 6 * MAIN_ROWS) return 0;
    return main_answer[(int)(y - (199 - 0x1E)) / 6];
}

/* ---- our map (the original's look, sharper, no zoom or pan, the original's font) ----
   The original draws its map as ever (its rand() calls stay the game's); the DLL draws it again over the map's part of
   the screen from the same data: PlayersMap, the notes (ATM_Strings), the note being typed (port_map_typing), the
   player's marker and the pointer (port_map_state, port_mouse_drawn: underworld-exhumed AUTOMAP.C, MOUSE.C, read only).
   Read here on the window's thread while the game waits in the map screen's loop. */
extern unsigned char PlayersMap[64][64];          /* AUTOMAP.C */
struct PortATM { char text[0x32]; int16_t x, y; };   /* ui.h's struct ATM (0x36 bytes) */
extern struct PortATM ATM_Strings[100];            /* FARDATA */
int port_map_state(int *lev, int *n, int *mx, int *my);
int port_mouse_drawn(int *x0, int *ytop, int *w, int *h, int *pic);
static volatile long typing_seq;                   /* odd while port_map_typing writes */
static char typing_text[52];
static int typing_on, typing_x, typing_y;
/* the game's thread gives the note being typed on the map (NULL: none); read by draw_map on the window's thread */
void port_map_typing(const char *text, int x, int y)
{
    __atomic_add_fetch(&typing_seq, 1, __ATOMIC_ACQ_REL);
    typing_on = text != NULL;
    if (text) {
        int i = 0;
        for (; text[i] && i < 50; i++) typing_text[i] = text[i];
        typing_text[i] = 0;
        typing_x = x, typing_y = y;
    }
    __atomic_add_fetch(&typing_seq, 1, __ATOMIC_ACQ_REL);
}

/* draws our map over the map screen's layer from the game's map data */
static void draw_map(const unsigned *src, int sw, int sh, int dx, int dy, int k)
{
    if (!map_fn) return;
    static char notes[100 * 50];
    static short nx[100], ny[100];
    static unsigned pal[256];
    int lev, n, mx, my, cx, cy, cw, ch, cpic;
    port_map_state(&lev, &n, &mx, &my);
    if (n < 0) n = 0;
    if (n > 100) n = 100;
    for (int i = 0; i < n; i++) {
        for (int c = 0; c < 50; c++) notes[i * 50 + c] = ATM_Strings[i].text[c];
        nx[i] = ATM_Strings[i].x, ny[i] = ATM_Strings[i].y;
    }
    for (int i = 0; i < 256; i++) pal[i] = plat_pal_rgb(i);
    char typed[52];
    int ton = 0, tx = 0, ty = 0;
    for (int tries = 0; tries < 4; tries++) {
        long a = __atomic_load_n(&typing_seq, __ATOMIC_ACQUIRE);
        if (a & 1) continue;
        ton = typing_on, tx = typing_x, ty = typing_y;
        for (int i = 0; i < 52; i++) typed[i] = typing_text[i];
        if (__atomic_load_n(&typing_seq, __ATOMIC_ACQUIRE) == a) break;
    }
    int on = port_mouse_drawn(&cx, &cy, &cw, &ch, &cpic);
    map_fn(src, sw, sh, dx, dy, k, &PlayersMap[0][0], pal, notes, nx, ny, n, ton ? typed : NULL, tx, ty,
           mx, my, on, cx, cy, cw, ch, cpic);
}

/* the wheel over the conversation: notches up (positive) older lines, two lines a notch */
void uwc_wheel(int notches)
{
    conv_back += notches * 2;
    int most = conv_lines - (0x94 - 0x44 + 1) / 6;
    if (conv_back > most) conv_back = most;
    if (conv_back < 0) conv_back = 0;
}

/* the original's options panel (OPTIONS.C: the rectangle (4, 0x52) to (0x27, 0xBE), y up: rows 9..117), opened by our Esc
   menu's Save / Restore / Quit or the Ctrl shortcuts, and the message scroll under it (the saves' descriptions) */
extern int16_t LeftPanel;       /* OPTIONS.C: 1 while the options panel is up */
enum { NO = 2 };
static int O[NO][6] = { { 4, 9, 36, 109, 0, 0 }, { 0, 166, 320, 34, 0, 0 } };
static int KO = 1, olw, olh;
static int (*cur)[6] = P, ncur = NP, kcur = 1;
static void layout_options(int uw, int uh)
{
    int all_h = O[0][3] + GAP + O[1][3];
    int k1 = uw * 90 / 100 / O[1][2], k2 = uh * 94 / 100 / all_h;
    KO = k1 < k2 ? k1 : k2;
    if (KO < 1) KO = 1;
    int y0 = (uh - all_h * KO) / 2;
    O[0][4] = (uw - O[0][2] * KO) / 2, O[0][5] = y0;
    O[1][4] = (uw - O[1][2] * KO) / 2, O[1][5] = y0 + (O[0][3] + GAP) * KO;
}
/* whether the original's options panel is up */
int uwc_options(void) { return LeftPanel == 1; }

/* whether the game is in a conversation */
int uwc_talking(void) { return scrnum == 2; }
/* the automap (scrnum 1, AUTOMAP.C): its whole screen (the map, its level buttons, the notes, the scroll), scaled up
   whole, centred over our view's last frame dimmed; the world stops as the original's map stops it */
int uwc_mapping(void) { return scrnum == 1; }
static int MP[1][6] = { { 0, 0, 320, 200, 0, 0 } };
static int KM = 1;
static void layout_map(int uw, int uh)
{
    int k1 = uw * 94 / 100 / 320, k2 = uh * 94 / 100 / 200;
    KM = k1 < k2 ? k1 : k2;
    if (KM < 1) KM = 1;
    MP[0][4] = (uw - 320 * KM) / 2, MP[0][5] = (uh - 200 * KM) / 2;
}
/* whether the last frame showed one of our layers */
int uwc_shown(void) { return shown; }
#ifndef UWPORT_PLAIN
int port_screen_is(int ps);     /* port/sys/screen.c: the screen the game says it shows */
static int uwc_typing(void) { return port_screen_is(10 /* PS_TEXT, port/sys/screen.h */); }
#endif
/* finds the renderer DLL's drawing functions by name (the env vars UWPORT_ORIGCONV / UWPORT_ORIGMAP keep the
   original's text or map) */
void uwc_bind(void *dll)
{
    conv_fn = dll ? (ConvFn)(void (*)(void))GetProcAddress(dll, "uwv_conv") : NULL;
    cut_fn = dll ? (CutFn)(void (*)(void))GetProcAddress(dll, "uwv_cutscene") : NULL;
    text_fn = dll && !getenv("UWPORT_ORIGCONV") ? (TextFn)(void (*)(void))GetProcAddress(dll, "uwv_conv_text") : NULL;
    map_fn = dll && !getenv("UWPORT_ORIGMAP") ? (MapFn)(void (*)(void))GetProcAddress(dll, "uwv_map") : NULL;
}

/* the whole number scale that fits the pieces (320 wide, the top two above the scroll) in most of the layer, and the
   pieces centred: the conversation area and the panel side by side as on the original's screen, the scroll below */
static void layout(int uw, int uh)
{
    int top_w = P[0][2] + P[1][2], top_h = P[0][3], all_h = top_h + GAP + P[2][3];
    int k1 = uw * 96 / 100 / P[2][2], k2 = uh * 94 / 100 / all_h;
    K = k1 < k2 ? k1 : k2;
    if (K < 1) K = 1;
    int y0 = (uh - all_h * K) / 2, x0 = (uw - top_w * K) / 2;
    P[0][4] = x0, P[0][5] = y0;
    P[1][4] = x0 + P[0][2] * K, P[1][5] = y0;
    P[2][4] = (uw - P[2][2] * K) / 2, P[2][5] = y0 + (top_h + GAP) * K;
}

/* the layer for this frame (NULL: none), laid out for the screen the game shows: conversation, options panel, map or
   typed text */
const unsigned char *uwc_frame(const unsigned *src, int sw, int sh, int uw, int uh, int cw, int ch)
{
    const unsigned char *layer = NULL;
    if (conv_fn && src && uwc_talking() && sw == 320 && sh == 200 && uw > 0 && uh > 0) {
        { static int clw, clh; if (uw != clw || uh != clh) layout(uw, uh), clw = uw, clh = uh; }
        lw = uw, lh = uh;
        wcw = cw, wch = ch;
        layer = conv_fn(src, sw, sh, uw, uh, &P[0][0], NP, K, 150);
        if (layer) draw_text(src, sw, sh);
        cur = P, ncur = NP, kcur = K;
    } else if (conv_fn && src && uwc_options() && sw == 320 && sh == 200 && uw > 0 && uh > 0) {
        if (uw != olw || uh != olh) layout_options(uw, uh), olw = uw, olh = uh;
        wcw = cw, wch = ch, lw = uw, lh = uh;
        layer = conv_fn(src, sw, sh, uw, uh, &O[0][0], NO, KO, 150);
        cur = O, ncur = NO, kcur = KO;
    } else if (conv_fn && src && uwc_mapping() && sw == 320 && sh == 200 && uw > 0 && uh > 0) {
        static int mlw, mlh;
        if (uw != mlw || uh != mlh) layout_map(uw, uh), mlw = uw, mlh = uh;
        wcw = cw, wch = ch, lw = uw, lh = uh;
        layer = conv_fn(src, sw, sh, uw, uh, &MP[0][0], 1, KM, 150);
        if (layer) draw_map(src, sw, sh, MP[0][4], MP[0][5], KM);
        cur = MP, ncur = 1, kcur = KM;
    }
#ifndef UWPORT_PLAIN
    else if (conv_fn && src && uwc_typing() && sw == 320 && sh == 200 && uw > 0 && uh > 0) {
        /* typed text (PS_TEXT: a shrine's mantra, a quantity, a save's description): the original's message scroll,
           where the prompt and the typing are, scaled up in the lower part of the screen over our view dimmed (the
           user: the prompt was not to be seen in our display) */
        static int T[1][6] = { { 0, 166, 320, 34, 0, 0 } }, KT = 1, tlw, tlh;
        if (uw != tlw || uh != tlh) {
            int k1 = uw * 90 / 100 / 320, k2 = uh * 30 / 100 / 34;
            KT = k1 < k2 ? k1 : k2;
            if (KT < 1) KT = 1;
            T[0][4] = (uw - 320 * KT) / 2, T[0][5] = uh * 62 / 100;
            tlw = uw, tlh = uh;
        }
        wcw = cw, wch = ch, lw = uw, lh = uh;
        layer = conv_fn(src, sw, sh, uw, uh, &T[0][0], 1, KT, 110);
        cur = T, ncur = 1, kcur = KT;
    }
#endif
    shown = layer != NULL;
    return layer;
}

/* the game on its 3D screen with its 3D window not showing the view (a cutscene in the window: the death, the
   endings, a vision): that window alone, centred and scaled up over our view's last frame (dimmed), nothing of the rest of the screen */
#ifdef UWPORT_PLAIN
int uwc_on_view_screen(void) { return scrnum == 0; }
#else
int port_screen_is(int ps);     /* port/sys/screen.c: the screen the game says it shows */
int uwc_on_view_screen(void) { return port_screen_is(2 /* PS_GAME, port/sys/screen.h */); }
#endif
/* the 3D window alone, centred (a cutscene in the window) */
const unsigned char *uwc_frame_window(const unsigned *src, int sw, int sh, int uw, int uh)
{
    return uwc_frame_cut(src, sw, sh, uw, uh, 0);
}
/* death: the skulls' rectangle (runcutscene's 0x103: the window 9 rows lower and 11 taller) over black (dim 255) */
const unsigned char *uwc_frame_cut(const unsigned *src, int sw, int sh, int uw, int uh, int death)
{
    static int W[2][6] = {{53, 20, 170, 108, 0, 0},    /* place_3d_view(0x34, 0xB4, 0xAC, 0x71): x 52, top row 199-0xB4 */
                          {52, 10, 172, 123, 0, 0}};   /* 0x103: x 0x34, bottom 0xB4 + 9, w 0xAC, h 0x70 + 0xB */
    int k1, k2, k, *R = W[death ? 1 : 0], dim = death ? 255 : 150;
    if ((!conv_fn && !cut_fn) || !src || sw != 320 || sh != 200 || uw <= 0 || uh <= 0) return NULL;
    k1 = uw * 94 / 100 / R[2], k2 = uh * 94 / 100 / R[3];
    k = k1 < k2 ? k1 : k2;
    if (k < 1) k = 1;
    R[4] = (uw - R[2] * k) / 2, R[5] = (uh - R[3] * k) / 2;
    if (cut_fn)   /* only what the cutscene paints (the skulls, none of the interface) */
        return cut_fn(src, sw, sh, uw, uh, R[0], R[1], R[2], R[3], R[4], R[5], k, dim);
    return conv_fn(src, sw, sh, uw, uh, R, 1, k, dim);   /* over our view's last frame, dimmed */
}

/* a client point to the original's screen point (x, y); 0 when it is on none of the shown pieces */
int uwc_point(int cx, int cy, float *x, float *y)
{
    int i;
    if (!shown || wcw <= 0 || wch <= 0) return 0;
    float lx = (float)cx * (float)lw / (float)wcw, ly = (float)cy * (float)lh / (float)wch;   /* the layer's pixels */
    for (i = 0; i < ncur; i++) {
        float u = (lx - (float)cur[i][4]) / (float)kcur, v = (ly - (float)cur[i][5]) / (float)kcur;
        if (u >= 0 && v >= 0 && u < (float)cur[i][2] && v < (float)cur[i][3]) {
            *x = (float)cur[i][0] + u, *y = (float)cur[i][1] + v;
            return 1;
        }
    }
    if (cur == O) {   /* the options panel: a point off it held at the buttons' edge, where the game's cursor stops */
        float u = (lx - (float)O[0][4]) / (float)KO, v = (ly - (float)O[0][5]) / (float)KO;
        u = u < 0 ? 0 : u > (float)O[0][2] - 0.5f ? (float)O[0][2] - 0.5f : u;
        v = v < 0 ? 0 : v > (float)O[0][3] - 0.5f ? (float)O[0][3] - 0.5f : v;
        *x = (float)O[0][0] + u, *y = (float)O[0][1] + v;
        return 1;
    }
    return 0;
}
/* the options panel's buttons (O[0]) in the view's client pixels (uwc_point's inverse), for the pointer's clip */
int uwc_options_rect(int *x0, int *y0, int *x1, int *y1)
{
    if (!shown || cur != O || wcw <= 0 || wch <= 0 || lw <= 0 || lh <= 0) return 0;
    *x0 = O[0][4] * wcw / lw, *y0 = O[0][5] * wch / lh;
    *x1 = (O[0][4] + O[0][2] * KO) * wcw / lw, *y1 = (O[0][5] + O[0][3] * KO) * wch / lh;
    return 1;
}
