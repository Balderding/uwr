/* uwgame.h: the original game's state at its last 3D frames (uwgame.c), for the window's renderer */
#ifndef UWGAME_H
#define UWGAME_H
#include <stdint.h>

/* The 3D view's frame buffer, as the game's own code lays it out:
   - stdat:0, 0x4CCF bytes (rpgame.h's FBUF_LEN);
   - rows of width + 2 bytes, the first starting at offset 2 (GRENTRY.ASM's setup_frame_buf, cPlaceFB:
     "rows of width + 2 bytes from offset 2"), the bottom row of the view first (cFBtoScreen copies from the
     last row, the top line of the screen, since y counts up);
   - shown at column 0x34 of the screen, its top row 199 - 0xB4 (UWEDIT.C's place_3d_view(0x34, 0xB4, 0xAC, 0x71):
     y is the bottom row counted up from the foot of the screen). */
#define UWG_FB_LEN 0x4CCF
#define UWG_FB_FIRST 2
#define UWG_FB_PITCH(w) ((w) + 2)
#define UWG_VIEW_X 0x34
#define UWG_VIEW_Y (199 - 0xB4)
#define UWG_KEEP 4               /* frames kept: the screen's page can be behind the one being drawn */

typedef struct {
    uint8_t fb[UWG_FB_LEN];
    int w, h;                    /* the view's size (xwid, xhgt: 172 by 113) */
    int x, y, z;                 /* PN: x, y in 1/256 tiles (y north), z in 1/8 object units */
    int facing, pitch;           /* a full turn is 0x10000 */
    int bank;                    /* the eye's roll (get_eye's cPlayer->bank), a full turn 0x10000 */
    int eye;                     /* 1: x, y, z, facing, pitch, bank are the eye itself (get_eye's), 0: PN (the feet) */
    int level;                   /* PlayerLevel, 1-9 */
    int light;                   /* LIGHTING.C's cur_light_level: SHADES.DAT's row, 0 darkest to 7 (5 the grey maps) */
    int zoom;                    /* VIEW3D.C's curZoom (0x7ED2 in play): the lens */
    const unsigned char *block;  /* mapdata: the level block (0x7C08 bytes) */
    unsigned frames;             /* 3D frames shown so far */
    unsigned long long when;     /* GetTickCount64 at the last */
} UwgSnap;

/* the frame buffer's pixel at column x, row y of the view (from its top left) */
static inline uint8_t uwg_fb_at(const UwgSnap *s, int x, int y)
{
    return s->fb[UWG_FB_FIRST + (s->h - 1 - y) * UWG_FB_PITCH(s->w) + x];
}

#ifdef UWPORT_GAMEVIEW
/* uw-new: the game's own word on its 3D view (the hooks of underworld-exhumed's src/port/portview.h, kept by
   portview.c, copied here under the lock at each change): no frame matching. The game no longer rasterizes its
   3D view (port_view_replace): its frame buffer holds UWG_MARK where the 3D picture would be, and whatever the
   game draws into it or over it on the screen afterwards (the weapon, the view's ornaments, the cursor) is
   another colour. */
#define UWG_MARK 0               /* portview.h's PORTVIEW_MARK */
typedef struct {
    int x, y, z, facing, pitch, bank;    /* get_eye's camera (as UwgSnap's with eye 1) */
    int level, light, zoom, w, h;
    const unsigned char *block;
    unsigned renders, shown;
    int on_screen;               /* the 3D window shows the frame buffer */
    int regular;                 /* the last copy was a main-loop frame */
    int effect;                  /* 0 none, 1 a flash, 2 a fade since the last frame drawn */
} UwgView;
int uwg_gameview(UwgView *out);  /* 0 before the first frame drawn */
void uwg_gameview_replace(int on); /* the game's rasterization left out (1) or drawn (0) */
#endif

int uwg_snapshot(UwgSnap *out);  /* the newest; 0 before the first 3D frame */
int uwg_snapshot_back(int k, UwgSnap *out); /* k frames before the newest */
int uwg_view_info(int *zoom, int *w, int *h); /* the last frame's zoom word and view size */
#endif
