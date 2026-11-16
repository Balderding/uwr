/* uwinv.h: the player's inventory as the original's own data holds it, copied on the game's thread (uwinv.c) into
   a plain struct for our panel (src\uwinv_draw.c, in uwview.dll). No game header and no Windows one here: the
   backend (window thread), the game's thread and the DLL all include it. */
#ifndef UWINV_H
#define UWINV_H

/* INVPANEL.C's display positions: 1..5 the armour (1 legs, 2 head, 3 torso, 4 gloves, 5 boots), 6..11 the shoulders,
   hands and rings (the left column 6, 8, 10 = slots 5, 7, 9; the right 7, 9, 11 = slots 6, 8, 10), 12..19 the
   pack's eight holes (or an open bag's eight objects), 20 the open bag, 21 and 22 its scroll arrows */
#define UWI_DISP 23

typedef struct {
    int item;            /* the object's item id (0-511), -1 for an empty hole */
    int qty;             /* the stack's count (1 for a single thing) */
    int weight;          /* ItemWeight, tenths of a stone, the whole stack */
    int container;       /* a bag, box or sack */
    int magic;           /* enchanted */
    int worth;           /* value times count */
    char name[64];       /* the game's name of it ("a leather sack"; the count with it) */
    char cond[28];       /* its condition's word ("excellent"), or "" */
    int wear;            /* the display position (1..11) it is worn or wielded in, -1 none: armour by its category, as
                            INVPANEL.C's ItemFitsSlot matches them, a ring in a free ring hole (else the first), a weapon
                            or launcher in the weapon hand, a shield in the other */
    int kind;            /* 1 armour (prot), 2 a weapon (dmg), 0 neither: OBJECTS.DAT's class records (struct Armour,
                            struct Weapon) as get_class_data gives them */
    int prot;            /* an armour's protection */
    int dmg[3];          /* a weapon's damage: slash, bash, stab */
    int runebag;         /* the rune bag (our rune page opens on it) */
} UwiThing;

/* everything carried in the pack and in the bags in it (and bags in those), for our Bagnon-like grid: each thing, its
   object (the player's object store, Obj_MemTPtr), the bag it is in (a node, -1 the pack itself) and, for the pack's
   own eight, its slot (11..18) */
#define UWI_NODES 128
typedef struct {
    UwiThing t;
    int obj;             /* the object's index (Obj_MemTPtr) */
    int parent;          /* the node of the bag it is in, -1: the pack */
    int depth;           /* 0 in the pack, 1 in a bag in it ... */
    int slot;            /* the pack's slot (11..18) for those in the pack, -1 else */
    int bag;             /* a container our grid can open (not the rune bag) */
} UwiNode;

typedef struct {
    int valid;           /* a snapshot was taken */
    int nnodes;          /* the tree (UwiNode), the pack's eight first, then each bag's contents after it */
    UwiNode node[UWI_NODES];
    int disp_obj[UWI_DISP]; /* the object at each display position now (-1 none) */
    int open_bags[16], nopen; /* our grid's open bags (objects), the backend's: copied in before the panel draws */
    int panel;           /* RightPanel (0: the inventory is the original's panel now) */
    int spot[UWI_DISP][2]; /* the 320 by 200 screen point that the original's FindInventoryHit takes for each display
                              position, -1 where none (position 0 is the body) */
    int rect[UWI_DISP][4]; /* each display position's rectangle on that screen: left, top, right, bottom (inclusive, y down) */
    UwiThing t[UWI_DISP];/* what each display position holds */
    int held;            /* an object is on the cursor */
    UwiThing h;          /* ... that object */
    int weight, max_weight; /* the carried weight and the most, tenths of a stone */
    int bag_open;        /* an open bag shows its objects */
    int up, down;        /* the scroll arrows' state (InvUpArrow, InvDownArrow) */
    int lefty;           /* the weapon hand is slot 8 - lefty */
    /* (every member an int or a char array of a multiple of 4: the game's side packs its structs, the DLL does not) */
    int runebag[3];      /* the runes the player has, a bit each (player->runebag: rune r is bit 7 - (r & 7) of byte r >> 3) */
    int shelf[3];        /* the runes on the shelf, in order (player->shelf), nrunes of them */
    int nrunes;
    int runes_open;      /* our rune page is open (the backend's, copied in before the panel draws) */
    int target;          /* the game wants a target for the thing on the cursor (input mode 2, 'Use <key> on what?', or
                            a missile spell aimed, 3): the thing is still where it was; no put or take till it is over */
    int mode;            /* the input mode (inplist->mode: 4 the barter) */
    int input_mode;      /* GameInputMode */
    /* the character */
    char name[32];
    int attr[3];         /* strength, dexterity, intelligence */
    int skill[20];
    int health, maxhealth, mana, maxmana, level, exp;
    int female, body;
    char cls[24];        /* the class's name */
    char lab[6][20];     /* the game's words: strength, dexterity, intelligence, vitality, mana, experience */
    char skn[20][24];    /* the skills' names */
    /* the full character sheet (the user, 2026-11-14: our first engine's sheet, more than the game's page shows): the
       game's own numbers, read only */
    int skill_points;    /* player->skill_points: to spend at a shrine */
    int hunger, fatigue, poison, easy, drawn; /* player->hunger, fatigue, poison (0-15), the easy setting, fight mode */
    int armour[4];       /* playerdat->armour by hit location: body, arms, legs, head (FixPlayerEquips' sums) */
    int defence;         /* playerdat->defence (FixPlayerEquips: the Defense skill, half the weapon's skill added) */
    int attack;          /* DoPlayerWeapon's to-hit skill with the weapon in hand (Attack / 2 + its skill + Dex / 7, + 7
                            easy, + a to-hit enchantment); -1 for a missile weapon (fired, not swung) */
    int atk_skill;       /* the skill that weapon uses (enum Skill: 2 bare hands, 3 sword, 4 axe, 5 mace, 6 missile) */
    int dmg[3];          /* its damage by swing (slash, bash, stab) with strength's bonus; fists the same in all three;
                            a missile weapon its Missile[] damage in [0] */
    int dlevel, day, period; /* the dungeon level (1..), the day of imprisonment (0-based, -1 past 100), the time of day 0..11 */
    char w_hunger[24], w_fatigue[24], w_level[16], w_period[24], w_day[24], w_weapon[64]; /* the game's words; the weapon's name */
} UwiSnap;

void uwi_capture(void);            /* the game's thread: take the snapshot (uwgame.c, at each 3D frame) */
int uwi_snapshot(UwiSnap *out);    /* the newest, 0 before one */
/* the player's health, mana and poison now, for our HUD's beakers (read only, any thread: the snapshot is taken only
   while the panel is open); v[0] health, v[1] max health, v[2] mana, v[3] max mana, v[4] poison (0 none) */
void uwi_vitals(int v[5]);
void uwi_want_panel(void);         /* ask the game's thread to show the inventory page (the next frame) */
void uwi_set_active(int on);       /* the panel is open: take the snapshot at each frame (else the game's thread leaves it) */
void uwi_reveal(int obj);
void uwi_look(int obj);            /* the original's look at an object carried (inv_look), at the next frame */
void uwi_show_pack(void);         /* the original's panel back on the pack (FixBagArea), at the next frame */
/* the backend's commands for the game's thread (uwi_cmd): run in order between the main loop's next passes
   (MAINLOOP.C's port_inv_hook), each by the original's own routines on its own data (uwinv.c). Those that work the inventory page wait for it when the original's panel shows
   another (the rune bag's), the page turned back first. */
enum {
    UWI_OP_PICK = 1,   /* a = the object (carried anywhere), b = how many of a stack: 0 all, -1 asked (the original's
                          'Move how many?'), n that many: onto the pointer (SetCursorObj) */
    UWI_OP_PUT_ON,     /* a = an object carried: the held thing put down on it (RearrangeInventory on its slot: a stack
                          joins, a bag takes it in, else they swap and the other comes onto the pointer) */
    UWI_OP_PUT_SLOT,   /* a = an inventory slot (the pack's 11..18): the held thing put there */
    UWI_OP_PUT_DISP,   /* a = a display position of the body (1..11): put there (worn, wielded) */
    UWI_OP_USE,        /* a = an object carried: used (UseObj: eat, read, light ...; the weapon hand's weapon: fight mode) */
    UWI_OP_LOOK,       /* a = an object carried: looked at (inv_look's words) */
    UWI_OP_DROP,       /* a, b = the original's screen point (mouse_getxy's, y up), c = the throw's speed class (0 its own):
                          the held thing let go into the world there (DoSpecialActions(DISP_WORLD): thrown when high in
                          the view, else put down in front) */
    UWI_OP_PACK,       /* the held thing into the first empty hole of the pack (a runestone into the rune bag) */
    UWI_OP_TAKE,       /* a, b = the original's screen point: what the view shows there taken onto the pointer, or looked at
                          (the port's n: player_3dget / player_3dlook) */
    UWI_OP_FIGHT,      /* fight mode on / off (toggle_fightmode: the original's click on the empty weapon hand) */
    UWI_OP_RUNE,       /* a = a rune (0..23) onto the shelf (the original's click on its rune page), -1 the shelf cleared */
    UWI_OP_RUNELOOK,   /* a = a rune: looked at (the original's right click on it) */
    UWI_OP_CAST,       /* the shelf cast (try_cast) */
    UWI_OP_TARGET,     /* target mode: a = an object carried, the target (the original's click on a thing of its panel:
                          ObjectActor); -1 cancelled */
    UWI_OP_TARGET3D,   /* target mode: a, b = the original's screen point (mouse_getxy's, y up): what the view shows there
                          (keys.c port_target_at, as mous_in_3d's click); -1, -1 the crosshair */
    UWI_OP_AIM3D,      /* aim mode (a missile spell, input mode 3): a, b = the point, or -1, -1 the crosshair: fired there
                          (keys.c port_aim_at: BlastFunction, as mous_in_3d's right click) */
    UWI_OP_SAVE,       /* a = a slot 1..4: saved there with uwi_save_desc's description (GAMEWRAP.C's DoSaveRest, no prompt) */
    UWI_OP_LOAD        /* a = a slot 1..4: restored (DoSaveRest, as the original's restore panel does it) */
};
void uwi_save_desc(const char *d);           /* the description the next UWI_OP_SAVE writes (our save screen) */
int uwi_input_mode(void);                   /* GameInputMode as last captured (every frame, our panel open or not) */
int uwi_cmd(int op, int a, int b, int c);   /* queued for the game's thread; 0 when the ring is full */
int uwi_cmd_pending(void);                  /* commands not yet run */          /* show this object in the original's panel (its bags opened, scrolled to it), by the
                                      original's own FixBagArea / OpenTheBag / ScrollItemsUp, at the next frame */
/* the grid's hit codes beyond the display positions (uwinv_draw.c's uwi_panel) */
#define UWI_HIT_NODE 100           /* + a node: a thing in our grid */
#define UWI_HIT_BAGBAR 300         /* + a node: a bag in the bag bar */
#define UWI_HIT_INTO 500           /* + a node: an empty hole of a bag's rows (a held thing goes into that bag) */
#define UWI_HIT_PACK 700           /* + 0..7: an empty hole of the pack's rows (its slot 11 + k) */
#define UWI_HIT_RUNE 900           /* + a rune (0..23): a rune of our rune page */
#define UWI_HIT_SHELF 930          /* the rune page's shelf strip: the shelf cleared */
#define UWI_HIT_CAST 931           /* the rune page's cast mark */
#define UWI_HIT_SCROLL 940         /* the slider of the open bags' rows (they do not all fit) */
#endif
