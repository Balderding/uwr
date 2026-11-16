/* The game: the player's inventory and light, picking things up and dropping them, the messages and the
 * interface (HUD, inventory panel), drawn into the UI overlay. */
#pragma once
#include "level.h"
#include "lights.h"

typedef struct {
    bool use;              /* pick up / use what is under the crosshair */
    bool inventory;        /* open / close the inventory */
    bool torch;            /* light / put out the carried light (the first light source in the pack) */
    bool attack;           /* the attack button pressed this frame (on a tagged thing: a look instead) */
    bool attack_held;      /* ... and still held: the attack charges; let go, it strikes */
    int number;            /* 1-9 pressed (a reply in a conversation), else 0 */
    bool escape;           /* leave the conversation */
    bool look;             /* look at what the crosshair (or, in the pack, the mouse) is on */
    bool fight;            /* C: fight mode on / off (the original's Fight icon) */
    bool character;        /* ,: the character panel on / off */
    int text;              /* a character typed this frame (32-126), 8 backspace, 13 enter; 0 none */
    bool sleep;            /* Z: sleep (make camp) */
    bool track;            /* K: the Track skill */
    bool runes;            /* R: the rune panel (the shelf of three runes) */
    bool cast;             /* Q: cast the spell on the shelf */
    bool shift;            /* a shift key is down (shift + right click drops; shift shows both tooltips) */
    bool mouse_dir_ok;     /* mouse_dir: the view ray through the mouse pointer (the mouse is free) */
    float mouse_dir[3];
    bool click_left, click_right;
    int mouse_x, mouse_y;  /* in overlay pixels, when the mouse is free */
} GameInput;

bool game_start(const char *data_dir, bool give_torch);
/* a new level: the game keeps these and changes the level's objects (picked up, dropped) */
void game_level(Level *lv, Mesh *m, const Light *lights, int nlights);
/* one step; eye and fwd: the view. Returns true while the mouse should be free (inventory open) */
bool game_update(float dt, const float eye[3], float yaw, const float fwd[3], const GameInput *in);
void game_draw_ui(float yaw);
/* the carried light, when one is lit: its intensity, range and colour */
bool game_light(float *intensity, float *range, float color[3]);
void game_message(const char *fmt, ...);
/* a book, scroll or anything with words to read: shown in the reading panel until clicked away */
void game_read(const char *title, const char *text);
/* is the circle (x, z, radius r) inside a door's frame, or a door that is shut */
bool game_blocked(float x, float z, float r);
/* a move trigger sent the player elsewhere: true once, with the level (1-9) and the tile to stand in */
bool game_travel(int *level, float *x, float *z);
/* Esc: closes one thing (reading, a conversation, a held thing back in the pack, the inventory); false when
 * nothing was open */
bool game_escape(void);
/* the player tick (every 21 s, counter 1-24): the regeneration and poison of UW.EXE 0x2acbd */
void game_player_tick(int tick, int poison);
void game_converse(int n); /* the checks: conversation N opens */
/* the character's numbers: skill k (0-19 in UW.EXE's order: attack, defense, unarmed, sword, axe, mace, missile,
 * mana, lore, casting, traps, search, track, sneak, repair, charm, picklock, acrobat, appraise, swimming; the
 * game's names are strings block 2, 31 + k), attribute k (0 strength, 1 dexterity, 2 intelligence), easy */
int game_skill(int k);
int game_attr(int k);
bool game_easy(void);
/* character creation (UW.EXE's overlay 5991): needed when character.txt holds no created character; on starts it */
bool game_needs_character(void);
void game_chargen(bool on);
bool game_chargen_on(void);
/* a save: the character, health, mana, fight mode, the quest bits and variables, what each conversation remembers
 * and the inventory, into / from dir (the levels and where the player stands: main.c) */
bool game_save(const char *dir);
bool game_load(const char *dir);
/* the level's living state back into its objects before it is written (creatures' hit points) */
void game_store_level(void);
/* a new game: everyone forgets, the quest starts over */
void game_new(void);
void game_init_char(bool blank); /* the original's new character (init_char): random, or blank before the creation screens */
/* sleep (UW.EXE 0x81f8f): how 1 a bedroll or the sleep key, -2 drink's collapse */
void game_sleep(int how);
/* where, when and how the player is (the original's status lines, strings block 1: 64-92) */
void game_status(void);
/* the silver seed planted where the player stands (true: it was, the seed is gone) */
bool game_plant_seed(void);
void game_hurt(float damage); /* the checks (--hurt-at) */
/* after death with no silver tree: 1 load the last save, 2 a new game (asked once), 0 nothing yet */
int game_death_choice(void);
/* held used on target, both in the pack: 0 no use (it is put there as usual), 1 used (held back to the pack),
 * 2 used up, 3 the target destroyed */
int game_use_on_item(Object *held, Object *target);
void game_fish(void);
bool game_leeches(void);
void game_play(void);
/* what is underfoot at (x, z): 0 floor, 1 water, 2 lava (TERRAIN.DAT) */
int game_terrain(float x, float z);
/* the player landed falling at speed v (tiles / s): the fall's harm (UW.EXE 0x1d0c3) */
void game_landed(float v);
/* magic: the light a spell gives (level 0-7, 0 none); the movement spells' flags (1 leap, 2 slow fall, 4 levitate,
 * 8 water walk, 16 fly, 32 speed); an enchantment (category, strength: potions, scrolls, wands) taking effect */
int game_spell_light(void);
int game_motion(void);
bool game_enchant_effect(int cat, int strength);
/* the checks: runes by name ("In Lor") onto the shelf, each given to the pack first; the number of runes put */
int game_set_shelf(const char *runes);
void game_set_char_level(int level); /* the checks: the character at this level, mana and skills for casting */
/* a trap changed the level's tiles: true once, the geometry is to be made again (game_remesh after) */
bool game_rebuild_wanted(void);
/* the quest bits (UW.EXE player record 0x65) */
u32 game_quests(void);
/* the level's geometry made again: the new mesh and lights (doors, creatures, the pack stay) */
void game_remesh(Mesh *m, const Light *lights, int nlights);
/* the conversation screen is up */
bool game_talking(void);
