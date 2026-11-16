/* The player's things, as the original keeps them: 11 body slots (0 helm, 1 chest, 2 gloves, 3 leggings,
 * 4 boots, 5 right shoulder, 6 left shoulder, 7 right hand, 8 left hand, 9 right ring, 10 left ring) and the
 * 8 holes of the pack (11-18). Containers hold their contents (a chain from their link, as in the levels);
 * one is open at a time in the pack's holes. Lights burn only in the shoulders and hands (5-8, UW.EXE's light
 * slots). The panel: a grey board with holes, drawn into the UI overlay. */
#pragma once
#include "level.h"

#define INV_BODY 11
#define INV_SLOTS 19
/* enchantments (UW.EXE 0x38b5c): the worn things' (categories 0-3 and 11, lasting while worn); the wielded weapon's
 * accuracy and damage; the name of what is held or pointed at (block 6), "" for none */
int inv_worn_enchants(int *cat, int *val, int cap);
/* the worn armour enchantments by body part (0 body, 1 arms, 2 legs, 3 head; UW.EXE 0x7e15f): Protection (taken off
 * an attacker's score, DS 0x266a) and Toughness (added to the part's armour) */
void inv_enchant_armour(int prot[4], int tough[4]);
void inv_weapon_enchant(int *accuracy, int *damage);
const char *inv_enchant_name(int p);
int inv_held_index(void);

typedef struct {
    bool click_left, click_right, look, shift;
    int mouse_x, mouse_y;
} InvInput;

void inv_start(const char *data_dir, bool give_torch);
/* take object i from the level (out of tile tx, tz; with what it holds): into a free pack hole, else into
 * the first thing in the pack it fits in. False (and a message) when there is no room: nothing changes */
bool inv_take(Level *lv, int tx, int tz, int i);
/* the panel's input (it is open, the mouse is free); eye/fwd: where a dropped thing lands. True when the
 * click was on the panel (else a click in the world: dropped the held thing, or nothing held) */
bool inv_update(Level *lv, const InvInput *in, const float eye[3], const float fwd[3]);
void inv_draw(void);
/* is the point (overlay pixels) on the panel (else the world is under the mouse) */
bool inv_mouse_over(int x, int y);
/* T: light / put out the light in the hands or shoulders (one from the pack goes to a free hand first) */
void inv_toggle_light(void);
/* the clock: burns the lights (UW.EXE's rule) */
void inv_tick(float dt);
/* the brightest lit light in the light slots: intensity, range, colour */
bool inv_light(float *intensity, float *range, float color[3]);
/* "You are well fed and wide awake." */
void inv_status(char *out, size_t n);
bool inv_holding(void);       /* a thing is on the mouse pointer */
void inv_put_back(void);      /* the thing on the pointer back into the pack (else it stays) */
/* is a key with this number (its owner field) carried, anywhere in the pack or its containers */
bool inv_has_key(int id);
/* the weapon in the right hand (its object number), or -1 */
int inv_weapon(void);
/* a new thing of kind item into the right hand (what was there to the pack): for the checks (--wield) */
void inv_wield(int item);
/* a new thing of kind item into the pack (the silver seed back from its tree); false: no room */
bool inv_give(int item, int quality);
/* the thing on the mouse pointer: its kind (-1 none) and its owner field (a key's number); used up; dropped where
 * the view ray dir from eye points (as a left click in the world does) */
int inv_held_item(int *owner);
/* the kind of thing in body slot s (-1 none); one of kind item taken out of the pack for good (false: none) */
int inv_slot_item(int s);
bool inv_consume_one(int item);
void inv_held_consume(void);
bool inv_drop_held(Level *lv, const float eye[3], const float dir[3]);
/* how many of object kind item are carried (stacks counted), anywhere */
int inv_count(int item);
/* the most the player can carry, tenths of a stone (strength x 20, UW.EXE 0x81305) */
void inv_set_capacity(int tenths);
/* the protection of what is worn in body slot s (0 helm .. 4 boots), 0 when nothing */
int inv_protection(int s);
/* its worth as armour as the original counts it (condition included; slot 8: a shield only) */
int inv_armour(int s);
int inv_carried(void); /* what is carried, tenths of a stone */
/* everything carried and the body's state (hunger, fatigue, drink, poison, the clock): dir\inventory.txt */
bool inv_save(const char *dir);
/* the body's numbers (hunger, fatigue 0-255; drink, poison levels), for sleep */
void inv_body(int *hunger, int *fatigue, int *drunk, int *poison);
void inv_set_body(int hunger, int fatigue, int drunk, int poison);
bool inv_load(const char *dir);
/* the panel's checkbox: the character panel opens with the pack (Tab); main.c saves it */
bool inv_with_sheet(void);
void inv_set_with_sheet(bool on);
int inv_capacity(void); /* the most that can be, tenths of a stone */
bool inv_level_free(const Level *lv, int i); /* is level object slot i unused (by the tiles or what they hold) */
int inv_level_alloc(const Level *lv);
int inv_level_alloc_mobile(const Level *lv);
/* on the rules core: the panel's contents from it (inv_tick does it each frame); a pool object's core object */
void inv_core_sync(void);
int inv_core_of(int p);
