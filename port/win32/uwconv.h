/* uwconv.h: a conversation in our display (display_mode 0), uwconv.c. While the game is on its conversation screen
   (UWEDIT.C's scrnum 2, set by change_screen(4) from CONVERSE.C's TalkTo -> newscr(4); strt_converse then runs the
   whole conversation), the window shows the original's own conversation pieces, cut from its 320 x 200 screen and
   scaled up by a whole number, centred over our 3D view's last frame (the world is stopped), dimmed:
     - the conversation area (strt_converse's frame: rectangle(0x2A, 0xC7, 0xC2, 0x99) and the "converse" art: both
       names and portraits, the trade slots of BARTER.C's play_slot_xy / npc_slot_xy, the NPC's scroll npc_scroll):
       x 42..233, rows 0..136;
     - beside it as on the original's screen, the right panel (the player's inventory, BeginInventory / RedispInv, for
       dragging things to and from the trade slots): x 234..319, rows 0..121;
     - below them, the message scroll (SCROLLIO.C's main_scroll, the player's numbered answers, its edges at x 0x0B and
       0x132): x 0..319, rows 166..199.
   The pointer over a piece goes to the game at the original's own point there (everything else is the game's own
   code: picking an answer, dragging into a trade slot, the keys). No game header and no Windows one here. */
#ifndef UWCONV_H
#define UWCONV_H

int uwc_talking(void);          /* the game is on its conversation screen (scrnum 2) */
/* this frame's layer (the DLL's uwv_conv) for an interface layer of uw by uh over a window cw by ch, src the game's
   picture (sw by sh, 0x00RRGGBB); NULL when no conversation (or no renderer): uwc_shown() then 0 */
const unsigned char *uwc_frame(const unsigned *src, int sw, int sh, int uw, int uh, int cw, int ch);
int uwc_shown(void);            /* the last frame was a conversation's (uwc_frame gave a layer) */
/* a window point (client pixels) to the original's screen: 1 over a piece (x, y set), 0 elsewhere */
int uwc_point(int cx, int cy, float *x, float *y);
/* our answer under a window point (client pixels) while the menu is up: 1.., 0 none (uwconv.c, step A2) */
int uwc_answer_at(int cx, int cy);
void uwc_wheel(int notches);   /* the NPC's scroll back (+) or on (-) */
int uwc_options(void);          /* the original's options panel is up (LeftPanel 1) */
int uwc_options_rect(int *x0, int *y0, int *x1, int *y1);   /* its buttons in the view's pixels, while shown */
void uwc_bind(void *dll);
int uwc_on_view_screen(void);   /* the game is on its 3D screen (scrnum 0) */
/* its 3D window alone (a cutscene in it: the death ...), centred and scaled over black; NULL without a renderer */
const unsigned char *uwc_frame_window(const unsigned *src, int sw, int sh, int uw, int uh);
const unsigned char *uwc_frame_cut(const unsigned *src, int sw, int sh, int uw, int uh, int death);       /* the renderer's DLL (its uwv_conv), or NULL */
#endif
