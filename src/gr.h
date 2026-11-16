/* The game's .GR pictures and STRINGS.PAK text, decoded from the originals (tools\gr.py and
 * tools\strings_pak.py are the reference). Used by the extractor only: the engine reads the loose files. */
#pragma once
#include "base.h"

/* picture k of a .GR file: w x h palette indices into out (index 0 = transparent); the 4-bit kinds go through
 * the small palettes of ALLPALS.DAT (allpals: 32 x 16). false when k has no picture. out holds 255 x 255. */
bool gr_picture(Bytes gr, int k, const u8 *allpals, u8 *out, int *w, int *h);
int gr_count(Bytes gr);

/* a creature animation page (CRIT\CRxxPAGE.Nyy, tools\crit.py): the slots (animation -> segment), the segments
 * (8 frame numbers, 255 none), the small palettes (32 game-palette indices), the frames' offsets */
typedef struct {
    int nslots, nsegs, npals, nframes;
    u8 slots[256], segs[256][8], pals[16][32];
    u16 off[256];
} CritPage;
bool crit_page(Bytes page, CritPage *p);
/* frame k: w x h small-palette indices (0 transparent) into out (255 x 255 at most), and its hotspot (feet) */
bool crit_frame(Bytes page, const CritPage *p, int k, u8 *out, int *w, int *h, int *hx, int *hy);

/* every string of STRINGS.PAK: calls each(block, index, text, length) in order */
bool strings_each(Bytes pak, void (*each)(void *ctx, int block, int index, const char *s, int n), void *ctx);
