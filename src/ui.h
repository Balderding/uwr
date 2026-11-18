/* The interface: drawn by the CPU into an RGBA overlay a whole factor smaller than the window (about 300 lines
 * high, so the original's pixel pictures keep their look), laid over the finished picture by the present pass.
 * Pictures and fonts come from data\ui (the game's own: .GR pictures as PNGs, the .SYS fonts). */
#pragma once
#include "base.h"

typedef struct {
    int w, h, scale;   /* the overlay's size and how many window pixels one of its pixels covers */
    u8 *px;            /* w * h * 4 */
} Overlay;

typedef struct {
    int w, h;
    u8 *rgba;
} Pic;

enum { FONT_SMALL, FONT_NORMAL, FONT_BIG, FONTS };

bool ui_start(const char *data_dir, int window_w, int window_h);
/* the game's folder (game\DATA's fonts are copied into data\ui\fonts when missing there): before ui_start */
void ui_game_dir(const char *game_dir);
Overlay *ui_overlay(void);
void ui_clear(void);
/* a picture from data\ui or data\sprites, e.g. ("ui\\flasks", 75); NULL when missing (loaded once, kept) */
const Pic *ui_pic(const char *dir, int k);
const Pic *ui_pic_file(const char *rel);
/* the mouse pointer (CURSORS.GR picture n) with its point on (x, y): the cross pictures' centre pixel, the others'
 * middle */
void ui_cursor(int n, int x, int y);
void ui_draw(const Pic *p, int x, int y, int scale);
/* an object's picture for the interface: ours (data\newsprites\objects, the Esc menu's Sprites tab: ui_new_sprite[item]
 * 1) when its file is there, else the original's. *div: 2 when it is at twice the detail (over 16 a side), else 1 */
extern u8 ui_new_sprite[512];
const Pic *ui_item_pic(int item, int *div);
/* a picture at 1 / div of its size, top left at the overlay's (x, y): div 2 at the window's own pixels (the text
 * layer), so that none of its detail is lost */
void ui_draw_fine(const Pic *p, int x, int y, int div);
void ui_set_scale(int s);
void ui_fill(int x, int y, int w, int h, u8 r, u8 g, u8 b, u8 a);
/* text in the game's font; returns its width; colour rgb */
int ui_text(int font, const char *s, int x, int y, u8 r, u8 g, u8 b);
int ui_text_width(int font, const char *s);
/* text wrapped at word breaks to width w; returns the height used (draw false: measure only) */
int ui_text_wrap(int font, const char *s, int x, int y, int w, u8 r, u8 g, u8 b, bool draw);
int ui_font_height(int font);
/* the interface's font (ui.c): 0 the game's own letters, 1.. a smooth face (smoothtext.c st_face_label(f - 1)) at the
 * size for the window's height; its text is drawn into a layer of the window's pixels, which ui_text_apply lays on
 * the layer d (w x h) wherever the overlay is laid on it (after the overlay) */
void ui_set_font(int f);
int ui_font(void);
void ui_text_apply(unsigned *d, int w, int h);
void ui_text_apply_a(unsigned *d, int w, int h, unsigned alpha256);   /* the same at an opacity (0..256) */
/* a character's bitmap in the game's font (rows of *row bytes, *h rows, *w wide); NULL for a space or a missing one */
const u8 *ui_glyph(int font, int ch, int *w, int *h, int *row);
/* lines of ui_text_wrap drawn only between y0 and y1 (ui_clip(-1, -1): everywhere) */
void ui_clip(int y0, int y1);

/* The panels are the original's parchment: the colours of its conversation scroll (CONV.BYT): the paper, its two
 * darker shades and the ink. The other inks are ours, dark enough to read on the paper. Use as ui_text(.., UI_INK). */
#define UI_PAPER 142, 105, 85
#define UI_PAPER_MID 125, 89, 69
#define UI_PAPER_DARK 97, 69, 52
#define UI_INK 60, 40, 32
#define UI_INK_FAINT 88, 62, 48
/* (no red text, a rule of the port since 2026-11-14: the
   heading was a red-brown, UI_INK_RED a red; now a deep navy and a dark teal) */
#define UI_INK_HEAD 16, 40, 116 /* headings: a deep navy */
#define UI_INK_BLUE 26, 42, 104
#define UI_INK_GREEN 22, 84, 18
#define UI_INK_RED 0, 84, 86    /* (its name kept: vitality's ink, now a dark teal) */
#define UI_INK_MAGIC 86, 28, 112
#define UI_INK_GOLD 100, 60, 0
/* a panel of paper with a raised edge */
void ui_parchment(int x, int y, int w, int h);
/* a hole pressed into the paper (hot: under the pointer) */
void ui_inset(int x, int y, int w, int h, bool hot);
void ui_blit(const Pic *p, int sx, int sy, int w, int h, int dx, int dy);
void ui_leather(int x, int y, int w, int h);
void ui_button(int x, int y, int w, bool lit);
/* a bright colour (meant for a dark panel) turned into a dark ink of the same hue */
void ui_ink(u8 *r, u8 *g, u8 *b);
/* a tooltip on paper by the pointer (mx, my): a heading and wrapped text ('\n' starts a line), at most w wide */
void ui_tip(int mx, int my, const char *title, const char *body, int w);
