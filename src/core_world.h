/* core_world.h: the rules core as the game drives it (src/core_world.c): the original's world, moved by the
 * original's own main loop pass each frame. */
#pragma once

enum { CW_NONE, CW_FORWARD, CW_BACK, CW_LEFT, CW_RIGHT, CW_JUMP };

int cw_start(const char *game_dir, const char *write_dir); /* the start-up and a new game on level 1; 0 if it failed */
int cw_running(void);
void cw_step(float dt, int input, int run, float yaw);     /* one frame: dt seconds, the movement key, the facing */
void cw_player(float *x, float *z, float *feet, int *level); /* where the player is: our world units */
/* the original's clock from the game (1/256 s, running in real time as the DOS timer did): its waits then end */
void cw_clock(unsigned (*fn)(void));
/* the core's level block (0x7c08 bytes, LEV.ARK's layout: tiles, then the 256 mobile and 768 static objects) */
const unsigned char *cw_block(void);
/* the original's teleport: the player to tile (x, y) of level on the next step */
void cw_teleport(int level, int x, int y);
/* the player's vitals as the original keeps them */
void cw_vitals(int *hp, int *hpmax, int *mana, int *manamax);
/* fight mode on or off (the original's toggle_fightmode), and a swing: swing 1-9 starts one (the ninth of the view:
 * top row thrust, middle slash, bottom bash), 0 none; held: the attack button is down (the blow charges) */
void cw_fight(int on);
void cw_attack(int swing, int held);
/* the original ended the game (real_death): the front end shows its death screen */
int cw_dead(void);

/* the inventory and the world's objects by the original's routines: what is in slot k (0-18 body and pack, 19 the
 * open bag, 20-27 its things on show), in the hand; a click on a slot (drag: pick up / put down, else how 0 use,
 * -2 look); taking, using, looking at object idx on tile; the hand's thing into the world (dropped or thrown); the
 * count a stack's "Move how many?" is answered with (0: all) */
int cw_inv_slot(int k);
int cw_held(void);
void cw_inv_click(int k, int drag, int how);
int cw_take(int idx, int tile);
void cw_use_world(int idx, int tile);
void cw_look_world(int idx, int tile);
void cw_drop(int throw_it);
void cw_set_howmany(int n);
int cw_howmany(void);
/* a thing in a bag reached through the bags round it (chain[0] the thing, outermost bag last, in slot top): its hole 20-27 */
int cw_bag_reach(const int *chain, int n, int top);

/* magic by the original's RUNES.C: a rune onto the shelf (as a click on it in the rune bag), a look at it, the shelf
 * cleared, the shelf's runes (0-23; the count), whether the bag has a rune, the cast, a missile spell awaiting its aim
 * and its release where the player looks */
void cw_rune_add(int r);
void cw_rune_look(int r);
void cw_shelf_clear(void);
int cw_shelf(int *runes);
int cw_has_rune(int r);
void cw_cast(void);
int cw_aiming(void);
void cw_release_spell(void);
/* the checks only: rune r into the core's rune bag (no stone) */
void cw_debug_give_rune(int r);
/* the player's light level as the original keeps it: 0 dark to 7 */
int cw_light_level(void);
/* the setting "start with the torch lit": a lit torch in the core's right hand */
void cw_give_torch(void);
/* the conversation, run by the original (CONVERSE.C, BABL.C) on the core's fiber; the screen is ours */
void cw_talk_world(int idx, int tile);       /* talk to world object idx in tile (player_3dtalk) */
int cw_conv_active(void);                   /* the conversation screen is up */
int cw_conv_waiting(void);                  /* its menu is waiting for the player's choice */
int cw_conv_options(char out[][160], int max); /* the menu's options */
void cw_conv_choose(int i);                 /* option i (from 0) */
void cw_conv_skip(void);                    /* a click or a key: a pause ends */
void cw_conv_name(char *out, int n);        /* the one spoken to */
int cw_scroll_target(void);                 /* where scroll_print writes: 0 messages, 1 the conversation, 2 the player's */
int cw_conv_menu(void);                     /* the menu is being shown (the player's scroll holds its options) */
int cw_conv_serial(void);                   /* counts conversations begun */
void cw_wait_frame(void);                   /* on the core's fiber: the frame back to the game */
