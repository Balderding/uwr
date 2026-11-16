/* core_world.c: the game's way into the rules core (underworld-exhumed's C, tools/core.py): the original's start-up
 * and a new game, then one pass of the original's main loop (MAINLOOP.C: do_changes, whose always-set handlers run
 * PLAYMOVE.C's check_physics: the player's physics and everything else that moves) a frame, with the player's input
 * given as the original's keyboard handler gives it (PlayerInput and its rates) and the facing from our mouse look.
 * Compiled like the core (compat.h force-included); core_world.h is the plain C face the game includes. */
#include <stdio.h>
#include "uw2.h"
#include "map.h"
#include "motion.h"
#include "object.h"
#include "player.h"
#include "critter.h"
#include "ui.h"
#include "core_world.h"
#include <string.h>
#include <stdlib.h>
/* CONVERSE.C's (exhumed's conv.h: the name is our own conversation module's too) */
extern struct Object far *talking_to;
void far conv_play_menu(int option);
void far player_3dtalk(void);                                            /* INTERACT.C */
int far get_name(char far *dst, struct Object far *obj, char article, char plural); /* LOOK.C */

extern char core_game_dir[260], core_write_dir[260];
extern uint32_t core_time;
extern uint32_t (*core_clock_fn)(void);
int core_math_init(const unsigned char *exe, long size);
unsigned char far init_strings(void);
int far init_objects(void);
void far init_player(void);
void far init_ai(void);
void far init_lighting(void);
void far init_combinables(void);
int far init_babl(void);
int far GetLevel(int level);
void far player_setup(int a, int b, int c);
void far do_level_hacks(int a, int b);
void far do_changes(void);
char far copy_file(char *from, char *to);
extern int16 PlayerInput, ForwInpRate, TurnInpRate, PlayerFacing, PlayerHeading, PlayerLevel;
extern int16 changed, scrnum, change_state[3];

static int started, died;
extern void (*core_death_fn)(int how);
static void on_death(int how) { (void)how; died = 1; }
static uint32_t frac; /* the clock's part of a tick carried between frames */

int cw_start(const char *game_dir, const char *write_dir)
{
    static unsigned char exe[0x90000];
    snprintf(core_game_dir, sizeof core_game_dir, "%s", game_dir);
    snprintf(core_write_dir, sizeof core_write_dir, "%s", write_dir);
    FILE *f = fopen("UW.EXE", "rb"); /* Borland's fopen (compat.h): under the game folder */
    long n = f ? (long)fread(exe, 1, sizeof exe, f) : 0;
    if (f) fclose(f);
    if (!core_math_init(exe, n)) return 0;
    /* UWEDIT.C's init_world, less the screen, sound, input and the title */
    if (!init_strings()) return 0;
    Map_Init();
    if (init_objects() != 0) return 0;
    init_txtlib();
    init_player();
    init_ai();
    init_char(0);
    init_lighting();
    init_combinables();
    /* MAINMENU.C's "Begin": the working copy of the levels, the conversations, level 1, the player placed */
    if (!copy_file("DATA\\lev.ark", "SAVE0\\lev.ark")) return 0;
    if (init_babl() != 0) return 0;
    if (GetLevel(1) <= 0) return 0;
    player_setup(0x20, 2, 1);
    do_level_hacks(1, 0);
    /* the keyboard as the original reads it (SYSENTRY.ASM's key table and shift states): no key down, the front end
     * giving the movement as PlayerInput */
    {
        extern unsigned char *key_on, *Shift, *Ctrl, *Alt, *CapsLock; /* sys.h's */
        static unsigned char keys[0x200], shift, ctrl, alt, caps;
        key_on = keys, Shift = &shift, Ctrl = &ctrl, Alt = &alt, CapsLock = &caps;
    }
    core_death_fn = on_death; /* the death screen is the front end's */
    died = 0;
    scrnum = 0; /* the 3D view */
    changed |= change_state[0];
    started = 1;
    return 1;
}

int cw_running(void) { return started; }

static void step_job(void);
static int fib_busy_now(void);
static int fib_call(void (*job)(void));
static int fib_resume(void);
void cw_step(float dt, int input, int run, float yaw)
{
    if (!started) return;
    /* the game clock (GAME_TIME, 1/256 s): the game's real-time clock when it gave one (cw_clock), else dt */
    if (!core_clock_fn) {
        uint32_t t = (uint32_t)(dt * 256.0f * 256.0f) + frac;
        core_time += t >> 8;
        frac = t & 0xFF;
    }
    /* the core waiting for the player (a conversation's menu, its pauses): it goes on from where it waits, and
     * its own loop runs the world meanwhile (converse_event_loop's do_changes) */
    if (fib_resume()) return;
    /* the facing from our mouse look (a creature comfort; the original turns by the mouse's or the keys' rate) */
    int16 facing = (int16)(int32_t)(yaw * (65536.0f / 6.2831853f));
    PlayerFacing = PlayerHeading = facing;
    TurnInpRate = 0;
    switch (input) {
    case CW_FORWARD: PlayerInput = PIN_FORWARD; ForwInpRate = run ? 0x70 : 0x32; break; /* do_player_keyboard's */
    case CW_BACK: PlayerInput = PIN_BACK; break;
    case CW_LEFT: PlayerInput = PIN_LEFT; break;
    case CW_RIGHT: PlayerInput = PIN_RIGHT; break;
    case CW_JUMP: PlayerInput = run ? PIN_RUN_JUMP : PIN_JUMP; break;
    default: PlayerInput = PIN_NONE; ForwInpRate = 0; break;
    }
    /* turning in place: the original's input is forward at rate 0 (parse_playin with the pointer in the middle
     * band), which runs the player's physics and so turns the player's object (the heading a blow goes by) */
    {
        static int16 last_facing;
        static int have_last;
        if (input == CW_NONE && have_last && facing != last_facing) PlayerInput = PIN_FORWARD, ForwInpRate = 0;
        last_facing = facing, have_last = 1;
    }
    fib_call(step_job);
}
/* one pass of the main loop (input_dispatch, the original's reading of the keys, is ours) */
static void step_job(void)
{
    changed &= ~0x200; /* bit 9, RedispInv: the inventory panel's redraw, which the front end does */
    if (changed) do_changes();
}

void cw_player(float *x, float *z, float *feet, int *level)
{
    /* PN: x and y in 1/256 tiles (y north: our z), z in 1/8 of an object's height unit: a tile's height step (our
     * STEP, a quarter) is 8 units, so our y is z / 256 */
    *x = PN.x / 256.0f;
    *z = PN.y / 256.0f;
    *feet = PN.z / 256.0f;
    *level = PlayerLevel;
}

void cw_clock(unsigned (*fn)(void)) { core_clock_fn = (uint32_t (*)(void))fn; }

/* the level block the core keeps (mapdata: LEV.ARK's layout), for drawing; 0 before it starts */
const unsigned char *cw_block(void) { return started ? (const unsigned char *)mapdata : 0; }

/* the original's teleport (UWEDIT.C's new_player_pos, change handler 3, on the next pass): the player put on tile
 * (x, y) of level, at the nearest free square */
extern int16 NewPlayerX, NewPlayerY, NewPlayerLevel;
void far editchng(int bits);
void cw_teleport(int level, int x, int y)
{
    if (!started) return;
    NewPlayerX = (int16)x;
    NewPlayerY = (int16)y;
    NewPlayerLevel = (int16)level;
    editchng(8);
}

/* the player's vitals as the original keeps them: hit points in the player's object (byte 8), their most in the
 * player's creature record (playerdat->avghit), mana in the player record */
void cw_vitals(int *hp, int *hpmax, int *mana, int *manamax)
{
    *hp = *hpmax = *mana = *manamax = 0;
    if (!started || !ThePlayer) return;
    *hp = ThePlayer->hp;
    *hpmax = playerdat ? playerdat->avghit : 0;
    *mana = player ? player->play_mana : 0;
    *manamax = player ? player->max_mana : 0;
}

/* fight mode and the player's swing, as INTERACT.C has them: toggle_fightmode draws or sheathes the weapon; a swing
 * starts with player_attack(the ninth of the view, 1 to 9: the row picks thrust, slash or bash) and charges while the
 * right button is held (display_scr calls player_attack(0) every pass) */
void far toggle_fightmode(void);
void far player_attack(int swing);
extern int16_t core_mouse_buttons;
void cw_fight(int on)
{
    if (started && player && (player->drawn != 0) != (on != 0)) toggle_fightmode();
}
void cw_attack(int swing, int held)
{
    if (!started) return;
    core_mouse_buttons = (int16_t)(held ? 2 : 0);
    if (swing > 0) player_attack(swing);
    {
        extern int16 pQatt, weap_frame;
        static int last = -99;
        if (getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG") && (swing || held || pQatt || last != weap_frame))
            fprintf(stderr, "attack: swing %d held %d drawn %d pQatt %d weap_frame %d\n", swing, held, player ? player->drawn : -1, pQatt, weap_frame);
        last = weap_frame;
    }
}

/* the original ended the game (the player died with no silver tree to come back at): the front end's death screen */
int cw_dead(void) { return started && died; }

/* --- the inventory and the world's objects, by the original's routines ------------------------------------------
 * The panel is the front end's picture; what is in it and what a click does are the original's. */
#include "inv.h"
extern struct InvRect InvDisplay[];
extern char SlotToDisplay[];
extern union Link Inventory[];
extern struct Object far *CursorObjPtr, far *newPlObj;
extern struct Tile far *PickMap, far *mlowptr;
extern union Link far *releasePtr;
extern char releaseable;
extern int16 GameInputMode, core_mouse_x, core_mouse_y;
extern unsigned char RightPanel;
extern unsigned char core_mouse_dragged;
void far DoInventoryMouse(int how);
void far player_3dget(void);
void far player_3duse(void);
void far player_3dlook(void);
static int howmany;
struct Inplist { int16 x, y, mouse, cmd, mode; }; /* exhumed's ui.h (srcI.h is ours) */
extern struct Inplist *inplist;
unsigned char far IsMobElem(struct Object far *obj);

int cw_inv_slot(int k) { return started && k >= 0 && k < 28 ? Inventory[k].f.index : 0; }
int cw_held(void) { return started && CursorObjPtr ? Obj_MemTPtr(CursorObjPtr) : 0; }
void cw_set_howmany(int n) { howmany = n; }
int cw_howmany(void) { return howmany; }

/* the 3D view as the inventory and the world's routines expect it: screen mode 1, the inventory on the right */
static void view_mode(void)
{
    if (inplist) inplist->mode = 1;
    RightPanel = 0; /* PANEL_INV */
    mlowptr = (struct Tile far *)mapdata;
}

/* a click on inventory slot k (0-18 the body and pack, 20-27 an open bag's things): drag picks up or puts down,
 * otherwise a use (how 0) or a look (how -2), as DoInventoryMouse has them; the pointer at the slot's rectangle in
 * the original's 320 x 200 screen (InvDisplay) */
void cw_inv_click(int k, int drag, int how)
{
    if (getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG")) fprintf(stderr, "cw_inv_click(%d, %d, %d): started %d\n", k, drag, how, started);
    if (!started || k < 0 || k >= 28) return;
    view_mode();
    int hit = SlotToDisplay[k];
    if (hit <= 0) return;
    struct InvRect *r = &InvDisplay[hit];
    int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2;
    core_mouse_x = (int16)x, core_mouse_y = (int16)y;
    inplist->x = (int16)(x - 0xF0), inplist->y = (int16)(y - 0x52);
    core_mouse_dragged = (unsigned char)(drag != 0);
    core_mouse_buttons = 0;
    int before = Inventory[k].f.index, held_before = CursorObjPtr ? Obj_MemTPtr(CursorObjPtr) : 0;
    DoInventoryMouse(how);
    core_mouse_dragged = 0;
    if (getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG"))
        fprintf(stderr, "inv click: slot %d (display %d at %d,%d) drag %d how %d: slot %d -> %d, hand %d -> %d\n", k, hit, x, y, drag, how,
                before, Inventory[k].f.index, held_before, CursorObjPtr ? Obj_MemTPtr(CursorObjPtr) : 0);
}

/* the original's pick (pick_3d, from the renderer's colours) given by the front end: object idx on tile */
static int pick(int idx, int tile)
{
    if (!started || idx <= 0 || idx >= 1024 || tile < 0 || tile >= 4096) return 0;
    view_mode();
    newPlObj = Obj_IntTMem(idx);
    PickMap = (struct Tile far *)mapdata + tile;
    releasePtr = &PickMap->objects;
    releaseable = ComObjData[OBJ_ITEM(newPlObj)].pickable && !IsMobElem(newPlObj);
    return 1;
}

/* take object idx from tile into the hand (player_3dget: reach, terrain, weight, a stack's count, the owner) */
int cw_take(int idx, int tile)
{
    if (!pick(idx, tile)) return 0;
    core_mouse_buttons = 0;
    player_3dget();
    return CursorObjPtr != 0;
}
void cw_use_world(int idx, int tile) { if (!fib_busy_now() && pick(idx, tile)) fib_call(player_3duse); } /* may talk (a shrine) */
void cw_look_world(int idx, int tile) { if (pick(idx, tile)) player_3dlook(); }

/* the thing in hand into the world (DoSpecialActions' 3D view case): dropped in front, or thrown when the pointer is
 * low in the view (throw) */
void cw_drop(int throw)
{
    if (!started || !CursorObjPtr) return;
    view_mode();
    core_mouse_x = 0x8A, core_mouse_y = (int16)(throw ? 0xA0 : 0x50);
    inplist->x = core_mouse_x, inplist->y = core_mouse_y;
    DoSpecialActions(0x17); /* DISP_WORLD */
    if (!CursorObjPtr) GameInputMode = 0;
}

/* a thing in a bag, reached as the original's panel reaches it: chain[0] the thing, chain[1..n-1] the bags round it,
 * outermost last, in body or pack slot top; every bag closed (CloseAllBags), then each opened in turn (OpenTheBag),
 * scrolling its eight holes (ScrollItemsDown) until the next shows. Returns the thing's hole (20-27), or -1. */
void far CloseAllBags(void);
void far OpenTheBag(int slot);
void far ScrollItemsDown(void);
static int visible(int want)
{
    for (int tries = 0; tries < 64; tries++) {
        for (int k = 20; k < 28; k++)
            if (Inventory[k].f.index == want) return k;
        ScrollItemsDown();
    }
    return -1;
}
int cw_bag_reach(const int *chain, int n, int top)
{
    if (!started || n < 2) return -1;
    view_mode();
    CloseAllBags();
    int slot = top;
    for (int j = n - 1; j >= 1; j--) {
        OpenTheBag(slot);
        if ((slot = visible(chain[j - 1])) < 0) return -1;
    }
    return slot;
}

/* --- magic by the original's RUNES.C: the rune bag panel's clicks (mous_in_rune: a rune onto the shelf, the oldest
 * pushed off, a new shelf after a cast; the top strip clears it), the cast (try_cast: the spell looked up, then
 * player_cast's level, mana and skill checks), and a missile spell's release where the player looks (BlastFunction) */
void far mous_in_rune(void);
void far try_cast(int how);
void far BlastFunction(void);
static void rune_panel(int x, int y, int right)
{
    view_mode();
    inplist->x = (int16)x, inplist->y = (int16)y, inplist->cmd = (int16)(right ? 2 : 1);
    mous_in_rune();
    inplist->cmd = 0;
}
void cw_rune_add(int r)
{
    if (!started || r < 0 || r >= 24) return;
    /* rune = 0x14 - ((y - 0x12) / 0xF << 2) + (x - 3) / 0x12: four to a row */
    rune_panel(3 + (r & 3) * 0x12 + 1, 0x12 + (5 - r / 4) * 0xF + 1, 0);
}
void cw_rune_look(int r) { if (started && r >= 0 && r < 24) rune_panel(3 + (r & 3) * 0x12 + 1, 0x12 + (5 - r / 4) * 0xF + 1, 1); }
void cw_shelf_clear(void) { if (started) rune_panel(10, 4, 0); }
int cw_shelf(int *runes)
{
    if (!started || !player) return 0;
    int n = player->nrunes > 3 ? 3 : player->nrunes;
    for (int i = 0; i < n; i++) runes[i] = player->shelf[i];
    return n;
}
int cw_has_rune(int r) { return started && player && r >= 0 && r < 24 && (player->runebag[r >> 3] >> (7 - (r & 7)) & 1); }
void cw_cast(void)
{
    if (!started) return;
    view_mode();
    inplist->cmd = 0;
    if (getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG"))
        fprintf(stderr, "cast: input mode %d, runes %d: %d %d %d, mana %d\n", GameInputMode, player->nrunes, player->shelf[0], player->shelf[1], player->shelf[2], player->play_mana);
    try_cast(1);
}
/* a missile spell waits to be aimed (input mode 3: ObjectActor is BlastFunction); the player's attack releases it */
int cw_aiming(void) { return started && GameInputMode == 3; }
void cw_release_spell(void)
{
    if (!cw_aiming()) return;
    view_mode();
    BlastFunction();
    GameInputMode = 0;
}

/* the checks only (--runes): rune r into the rune bag without its stone */
void cw_debug_give_rune(int r) { if (started && player && r >= 0 && r < 24) player->runebag[r >> 3] |= (unsigned char)(1 << (7 - (r & 7))); }

/* the player's light level as the original keeps it (bits 4-7 of the player record's light byte: 0 dark to 7) */
int cw_light_level(void) { return started && player ? (player->light & 0xF0) >> 4 : 0; }

/* the setting "start with the torch lit" (a comfort of ours: the original starts with nothing): a lit torch made
 * (CreateObj) and put in the right hand (AddToInventory), and the light level set from it (FixPlayerEquips) */
unsigned char far AddToInventory(struct Object far *obj, int slot);
void far FixPlayerEquips(void);
void cw_give_torch(void)
{
    if (!started) return;
    struct Object far *t = CreateObj(0x95, 0); /* ITEM_LIT_TORCH */
    if (!t) return;
    t->qn.f.quality = 40;
    if (!AddToInventory(t, 7)) return;
    FixPlayerEquips();
}

/* --- the core on a fiber of its own. The original waits in loops of its own for the player (converse_event_loop's
 * menu, scroll_wait's pauses), reading the input as it goes (input_dispatch, mouse_get_input_sp); ours runs the core
 * on a fiber, and where it reads the input the fiber hands the frame back to the game: the next frame goes on from
 * there with what the player did. Everything that can start a conversation (a step: a creature that wants to talk,
 * a trigger; a use; a talk) runs on it. */
void *__stdcall ConvertThreadToFiber(void *param);
void *__stdcall CreateFiber(unsigned long long stack, void(__stdcall *start)(void *), void *param);
void __stdcall SwitchToFiber(void *fiber);
unsigned long long __stdcall GetTickCount64(void);
static void *fib_main, *fib_core;
static void (*fib_job)(void);
static int fib_busy;   /* a job under way: done, or waiting for the player */
static int on_core;    /* running on the core's fiber now */
static int fib_busy_now(void) { return fib_busy; }
static void __stdcall fib_proc(void *param)
{
    (void)param;
    for (;;) {
        on_core = 1;
        if (fib_job) fib_job();
        fib_job = 0, fib_busy = 0, on_core = 0;
        SwitchToFiber(fib_main);
    }
}
/* runs job on the core's fiber; 0 when the core is still in the middle of another (waiting for the player) */
static int fib_call(void (*job)(void))
{
    if (fib_busy) return 0;
    static int on = -1; /* UW_CORE_TALK: the fiber (the core's conversations) is still being made; off, as before */
    if (on < 0) { const char *e = getenv("UW_CORE_TALK"); on = e && *e; }
    if (!on) { job(); return 1; }
    if (!fib_main) fib_main = ConvertThreadToFiber(0);
    if (!fib_core) fib_core = CreateFiber(1 << 20, fib_proc, 0);
    if (!fib_main || !fib_core) { job(); return 1; } /* no fibers: as before (a wait then cannot hand back) */
    fib_job = job, fib_busy = 1;
    SwitchToFiber(fib_core);
    on_core = 0;
    return 1;
}
/* the core goes on from where it waits; 1 when it was waiting */
static int fib_resume(void)
{
    if (!fib_busy || on_core) return 0;
    SwitchToFiber(fib_core);
    on_core = 0;
    return 1;
}
/* the frame back to the game (on the core's fiber only; elsewhere nothing) */
static void core_yield(void)
{
    if (!on_core) return;
    on_core = 0;
    SwitchToFiber(fib_main);
    on_core = 1;
}
void cw_wait_frame(void) { core_yield(); }

/* --- the conversation (CONVERSE.C, BABL.C, BARTER.C as they are; the screen is ours) --- */
int core_conv_menu(void);
int core_conv_nopts(void);
char far *core_conv_opt(int i);
static int conv_pick;      /* the option the player chose (from 1), for the menu's handler */
static int conv_waiting;   /* the menu is up, waiting for the choice */
static int wait_skip;      /* a click or a key: the end of a pause */
static int scroll_to;      /* where scroll_print writes: 0 the messages, 1 the conversation (the NPC's scroll), 2 the
                            * player's (the menu, respond) */
static int conv_serial;    /* counts conversations begun (strt_converse loading its art) */
void far do_main_scroll(void) { scroll_to = 0; }
void far do_npc_scroll(void) { scroll_to = 1; }
void far do_play_scroll(void) { scroll_to = 2; }
void far scroll_clear(char redraw) { (void)redraw; }
int cw_scroll_target(void) { return scroll_to; }
int cw_conv_serial(void) { return conv_serial; }
int cw_conv_menu(void) { return started && core_conv_menu(); }
/* MOUSE.C's input queue, as input_dispatch reads it: on the core's fiber the frame goes back to the game first; the
 * player's choice then goes to the menu's handler (conv_play_menu: the keys 1-4 and a click on the option's row
 * both end there), and there is no other input (-1). Off the fiber: the stub's answer as before. */
int far mouse_get_input_sp(void)
{
    if (!on_core) return 0;
    conv_waiting = core_conv_menu() != 0;
    core_yield();
    conv_waiting = 0;
    if (conv_pick > 0) {
        int p = conv_pick;
        conv_pick = 0;
        conv_play_menu(p);
    }
    return -1;
}
/* SCROLLIO.C's scroll_wait: a pause of ticks (0x1F4 between a conversation's last line and its end, pause(n) n of
 * them) that a click or a key ends; ours counts 2 ms a tick */
void far scroll_wait(int ticks, char mouse)
{
    (void)mouse;
    if (!on_core) return;
    unsigned long long end = GetTickCount64() + (unsigned long long)(ticks > 0 ? ticks : 0) * 2;
    wait_skip = 0;
    while (!wait_skip && GetTickCount64() < end) core_yield();
    wait_skip = 0;
}
/* GRAPHICS's art loader: the conversation screen's art (frame, heads) is ours, so on the core's fiber it reports
 * the art loaded (strt_converse closes the screen when it cannot load it); off the fiber the stub's 0 as before */
char far gronk_gr(char *art, int start, int count, void far *(far *adr)(int), unsigned char (far *move)(void far *, int, int))
{
    (void)start, (void)count, (void)adr, (void)move;
    if (!on_core) return 0;
    if (art && !strcmp(art, "converse")) conv_serial++;
    return 1;
}
/* talk to world object idx in tile (player_3dtalk: TalkTo) */
void cw_talk_world(int idx, int tile) { if (!fib_busy && pick(idx, tile)) fib_call(player_3dtalk); }
/* a conversation going on: the conversation screen (UWEDIT.C's newscr(4): scrnum 2) */
int cw_conv_active(void) { return started && scrnum == 2; }
int cw_conv_waiting(void) { return cw_conv_active() && fib_busy && conv_waiting; }
int cw_conv_options(char out[][160], int max)
{
    int n = cw_conv_waiting() ? core_conv_nopts() : 0;
    if (n > max) n = max;
    for (int i = 0; i < n; i++) {
        const char far *t = core_conv_opt(i);
        snprintf(out[i], 160, "%s", t ? t : "");
    }
    return n;
}
void cw_conv_choose(int i) { if (cw_conv_waiting()) conv_pick = i + 1, fib_resume(); }
void cw_conv_skip(void) { wait_skip = 1; }
/* the one spoken to, as the screen names it (get_name) */
void cw_conv_name(char *out, int n)
{
    char name[0x28] = "";
    if (cw_conv_active() && talking_to) get_name(name, talking_to, 0, 0);
    snprintf(out, (size_t)n, "%s", name);
}
