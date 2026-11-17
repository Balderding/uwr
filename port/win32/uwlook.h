/* uwlook.h: the crosshair's readout (uwlook.c) */
#ifndef UWLOOK_H
#define UWLOOK_H
void uwl_enable(int on);          /* any thread: the readout is wanted (full-screen play); off, the game is not touched */
int uwl_get(char *out, int n);    /* the window's thread: the name the crosshair is on ("" when nothing); its length */
void uwl_capture(void);           /* the game's thread, after the 3D frame was copied to the screen */
/* what the crosshair is on, as the game's own data has it (for the crosshair's picture and the hint line) */
typedef struct {
    char name[96];   /* its name ("" for nothing, a wall, a floor, a closed secret door) */
    int kind;        /* 0 nothing named, 1 a creature, 2 a door, 3 another object */
    int hostile;     /* a creature hostile to the player (its attitude 0, CritterLook's word) */
    int reach;       /* in the player's reach: INTERACT.C's InPickRange(PickDist) and no BlockingTerrain */
    int take;        /* it can be taken (pick_3d's releaseable: pickable, not a mobile) */
    int open;        /* a door, open (0x148-0x14F) */
    int sw;          /* a switch (0x170-0x17F) */
    int charging;    /* a blow is being charged (COMBAT.C's pQatt < 0) */
    int held;        /* an object is on the cursor (CursorObjPtr) */
    int drawn;       /* the weapon is drawn: the original's fight mode (player->drawn, INTERACT.C's pick_fightmode) */
    int stack;       /* a stack (player_3dget asks how many of it to take) */
    int held_item;   /* the item id of the object on the cursor, -1 none (every frame) */
    int carry;       /* GameInputMode is GIM_CARRY: the object on the cursor waits to be put somewhere */
} UwlInfo;
void uwl_info(UwlInfo *out);      /* the window's thread: the newest */
#define UWL_TARGETS 16
void uwl_set_targets(const int *t, int n);   /* the window's thread: what our view shows at the pointer (uwv_pick), -1 none */
extern int uwl_door_targets;      /* 1: a door may be picked by its drawn frame and leaf when nothing else is there */
extern int port_interact_take;    /* port/ui/keys.c: e on a thing that can be taken takes it (our display) */
extern int port_grab_armed[3];    /* port/ui/keys.c (uw-new): n takes, o throws, u puts away; armed before each is sent */
extern int port_throw_x, port_throw_y;   /* ... the screen point (mouse_getxy's) the throw is let go at */
extern int port_throw_speed;      /* ... and its speed class (1..15, the original's 0xF) */
extern int port_throw_class;      /* the speed MISSILE.C's ReturnObject throws with now (0: its own 0xF) */
int uwl_music_on(void);           /* the original's music_is_on / fx_is_on: read only */
int uwl_fx_on(void);
int uwl_left_panel(void);         /* LeftPanel: 1 while the original's options panel is up */
/* our HUD's combat pictures: the gargoyle's eyes (PANELS.C setting[SCR_EYES]: 1..3 while they blink after a blow, the
   creature healthy .. near death; 0 at rest) and the blow's charge (COMBAT.C play_pow 0..100 while a swing charges and
   has not struck, else -1). Read only. */
void uwl_combat(int *eyes, int *pow);
#endif
