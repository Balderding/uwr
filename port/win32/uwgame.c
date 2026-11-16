/* uwgame.c: what our renderer (uwview.dll) needs from the original game as it runs, taken where the game shows
   a 3D frame. The game's cFBtoScreen (GRENTRY.ASM, the port's translated module) copies the 3D view's frame
   buffer (stdat:0, 0x4CCF bytes) to the page being drawn; the link wraps it (-Wl,--wrap=cFBtoScreen,
   tools\portwin32.py), so after each copy this keeps, under a lock: the frame buffer as copied (the window then
   tells the original's 3D pixels from what is drawn over them: the cursor), the player's place and facing (PN,
   PlayerFacing, PlayerPitch), the level (PlayerLevel, mapdata) and the view's size (xwid, xhgt). The last few
   frames are kept, since the page on the screen can be a frame or two behind the one being drawn. Reading only:
   nothing of the game is changed. Runs on the game's thread; uwg_snapshot on the window's. */
#define WIN32_LEAN_AND_MEAN
#include "uwos.h"   /* windows.h on Windows: SRWLOCK, GetTickCount64 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "uwgame.h"
#include "uwinv.h"
#include "uwlook.h"

/* the game's (declared here as the game's headers have them: int16 is a short) */
extern unsigned char stdat[];
extern unsigned char *mapdata;
extern struct { int16_t x, y, z; } PN;   /* struct Phys: x, y in 1/256 tiles, z in 1/8 of an object's z unit */
extern int16_t PlayerFacing, PlayerPitch, PlayerLevel, xwid, xhgt;
extern unsigned char cur_light_level;   /* LIGHTING.C: the level set_light last loaded */
extern uint16_t curZoom;                 /* VIEW3D.C: place_3d_view's zoom */

void __real_cFBtoScreen(void);
void __real_get_eye(void);

/* PLAYMOVE.C's get_eye places the 3D camera (cPlayer) for the frame: the player's eye 0xA4 above the feet with the
   effect offsets (playerMod, when doMod: a blow's jolt, a fall), a fixed camera (campos, camang), another mobile
   object's eyes, the view from behind the player, the moongate vortex. VIEW3D.C's setup_vars then rewrites cPlayer
   for the renderer (the position within the eye's tile, turned into the first quadrant), so it is kept here as
   get_eye leaves it (view3d.h's struct Camera: x 0x0A, z 0x0E, y 0x12, pitch 0x28, bank 0x2A, heading 0x2C). */
struct CameraRec { char pad0[0x0A]; int16_t x; char pad0C[2]; int16_t z; char pad10[2]; int16_t y; char pad14[0x28 - 0x14];
                   int16_t pitch, bank, heading; };
extern struct CameraRec *cPlayer;
static volatile int16_t eye_x, eye_y, eye_z, eye_pitch, eye_bank, eye_heading;
static volatile int eye_seen;
void __wrap_get_eye(void)
{
    __real_get_eye();
    if (!cPlayer) return;
    eye_x = cPlayer->x, eye_y = cPlayer->y, eye_z = cPlayer->z;
    eye_pitch = cPlayer->pitch, eye_bank = cPlayer->bank, eye_heading = cPlayer->heading;
    eye_seen = 1;
}

static SRWLOCK lock = SRWLOCK_INIT;
static UwgSnap ring[UWG_KEEP];
static unsigned frames;

void __wrap_cFBtoScreen(void)
{
    UwgSnap *s;
    __real_cFBtoScreen();
    AcquireSRWLockExclusive(&lock);
    s = &ring[frames % UWG_KEEP];
    memcpy(s->fb, stdat, UWG_FB_LEN);
    s->w = xwid, s->h = xhgt;
    if (eye_seen)   /* the eye as get_eye placed it for this frame */
        s->x = eye_x, s->y = eye_y, s->z = eye_z, s->facing = eye_heading, s->pitch = eye_pitch, s->bank = eye_bank, s->eye = 1;
    else
        s->x = PN.x, s->y = PN.y, s->z = PN.z, s->facing = PlayerFacing, s->pitch = PlayerPitch, s->bank = 0, s->eye = 0;
    s->level = PlayerLevel, s->light = cur_light_level;
    s->block = mapdata;
    s->zoom = curZoom;
    s->frames = ++frames;
    s->when = GetTickCount64();
    ReleaseSRWLockExclusive(&lock);
#ifndef UWPORT_GAMEVIEW   /* uw-new: at the main loop's frames only (gv_notify), never inside an effect */
    uwi_capture();   /* the inventory, for our panel (uwinv.c) */
    uwl_capture();   /* what the crosshair is on, for the readout (uwlook.c) */
#endif
}

#ifdef UWPORT_GAMEVIEW
#include "portview.h"   /* underworld-exhumed's src/port/portview.h (uw-new) */
static UwgView gv;
static int gv_any;

/* portview.c's port_view_notify, on the game's thread after each change */
static void gv_notify(int ev)
{
    static int dbg = -1;
    if (dbg < 0) dbg = getenv("UWPV_LOG") != NULL;
    if (dbg) fprintf(stderr, "pv: ev %d renders %u shown %u on %d regular %d effect %d at %llu\n", ev, port_view.renders,
                     port_view.shown, port_view.on_screen, port_view.regular, port_view.effect, (unsigned long long)GetTickCount64());
    AcquireSRWLockExclusive(&lock);
    gv.x = port_view.x, gv.y = port_view.y, gv.z = port_view.z;
    gv.facing = port_view.heading, gv.pitch = port_view.pitch, gv.bank = port_view.bank;
    gv.level = port_view.level, gv.light = port_view.light, gv.zoom = port_view.zoom;
    gv.w = port_view.w, gv.h = port_view.h, gv.block = port_view.block;
    gv.renders = port_view.renders, gv.shown = port_view.shown;
    gv.on_screen = port_view.on_screen, gv.regular = port_view.regular, gv.effect = port_view.effect;
    gv_any = port_view.renders > 0;
    ReleaseSRWLockExclusive(&lock);
    if (ev == PV_SHOWN && port_view.regular) {   /* a main-loop frame on the screen (send_db) */
        uwi_capture();   /* the inventory, for our panel (uwinv.c) */
        uwl_capture();   /* what the crosshair is on, for the readout (uwlook.c): the original's own pick pass */
    }
}

int uwg_gameview(UwgView *out)
{
    int ok;
    AcquireSRWLockShared(&lock);
    ok = gv_any;
    if (ok) *out = gv;
    ReleaseSRWLockShared(&lock);
    return ok;
}

void uwg_gameview_replace(int on)
{
    port_view_notify = gv_notify;
    port_view_replace = on;
}
#endif

/* the frame k back from the newest (0 the newest); 0 when there is none */
int uwg_snapshot_back(int k, UwgSnap *out)
{
    int ok;
    AcquireSRWLockShared(&lock);
    ok = k >= 0 && k < UWG_KEEP && (unsigned)k < frames;
    if (ok) *out = ring[(frames - 1 - (unsigned)k) % UWG_KEEP];
    ReleaseSRWLockShared(&lock);
    return ok;
}

int uwg_snapshot(UwgSnap *out) { return uwg_snapshot_back(0, out); }

/* the view's lens as the last frame had it (the zoom word and size), for mapping a click in our full-screen view to the original's */
int uwg_view_info(int *zoom, int *w, int *h)
{
    int ok;
    AcquireSRWLockShared(&lock);
    ok = frames > 0;
    if (ok) {
        const UwgSnap *s = &ring[(frames - 1) % UWG_KEEP];
        *zoom = s->zoom, *w = s->w, *h = s->h;
    }
    ReleaseSRWLockShared(&lock);
    return ok;
}
