/* smoothtext.h: text in a smooth Windows font (Segoe UI through GDI; Tahoma where it is missing), antialiased, drawn
 * at the window's own resolution straight into an RGBA layer (bytes R, G, B, A: the interface layer's). For our
 * menus: the game's small pixel letters enlarged three times were hard to read (the user, 2026-11-14).
 * Glyphs are made once a face and size and kept. Presentation only. */
#pragma once

enum { ST_REGULAR, ST_SEMIBOLD };
/* the width of s in pixels at letter height em (pixels) */
int st_width(int face, int em, const char *s);
/* s with its top-left at x, y into the w by h layer, in colour rgb (0xRRGGBB); returns the pen's end (x) */
int st_draw(unsigned *rgba, int w, int h, int x, int y, int face, int em, const char *s, unsigned rgb);
/* the line's height in pixels at em (ascent + descent) */
int st_height(int face, int em);
/* the font found: "Segoe UI" or "Tahoma" (the checks) */
const char *st_font_name(void);
/* the one text size of our interface (the user, 2026-11-14: "the font size needs to be universal ... the game's
   resolution"): letter height in pixels for a window window_h rows tall, 19 at 1080, in proportion, 12 at least.
   Fixed for a resolution, so its glyphs are made once (a new size costs a GDI pass over 224 glyphs). */
int st_game_em(int window_h);
/* the Esc menu's Font and Font size: the faces offered (st_face_label 0..st_face_count()-1), the one chosen (every
   set made is dropped and made again in it), and the size as a percentage of st_game_em's (50..200) */
int st_face_count(void);
const char *st_face_label(int i);
void st_set_face(int i);
/* the face named (st_face_label's), -1 if not offered; and every .ttf / .otf in dir loaded for this program only (FR_PRIVATE)
   and offered by its family name (call before the faces are listed) */
int st_face_find(const char *name);
void st_load_font_dir(const char *dir);
void st_set_size_pct(int pct);
int st_size_pct(void);
