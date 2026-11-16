/* What the original knows about each kind of object, and how it puts it into words. From the loose files
 * data\objects\objects.txt (COMOBJ.DAT and the tables of OBJECTS.DAT, extracted) and the game's strings
 * (data\strings: block 1 messages, 4 names, 5 quality words, 8 writings, 10 textures). The rules are the ones
 * UW.EXE follows (see NOTES.md, "Objects"). */
#pragma once
#include "level.h"

typedef struct {
    u16 mass;        /* tenths of a stone, for one */
    u16 value;       /* gold, for one */
    u8 height, radius;
    u8 pick;         /* can be picked up */
    u8 kind;         /* 0 a thing (stacks when its name has a plural), 1 a weapon or armour, 3 a container */
    u8 qclass, qtype;/* quality class (3: always the last word), quality words: block 5 from qtype * 6 */
    u8 look;         /* described with "You see ..." (else a decoration with its own text) */
    u8 material;     /* a critter's record byte 0x10: bits 6-7 its weapon, 4-5 its hide (1: metal), 0-3 its walk sound */
    u8 death;        /* a critter's record byte 8 bits 0-2: 1 = effect 6 as it dies */
    u8 resist;       /* COMOBJ byte 8 (UW.EXE DS 0x5b76): bits 0-1 magic resistance 0-3 (resists a magic hit when d3 - 1 <
                      * it), other bits immunity to that kind of harm (8 fire, 0x10 poison, 0x40 missiles), 0x80 undead */
    u8 owner_ok;     /* can belong to someone */
    i8 armour;        /* the body slot it is worn in: 0 helm, 1 chest, 2 gloves, 3 leggings, 4 boots, 9 a ring; -1 */
    u8 protection, durability;
    u8 capacity;     /* a container: tenths of a stone it holds (0 = no limit) */
    i16 accepts;     /* a container: -1 anything, an object number, or 0x200-0x203: runes, ammunition, maps, food */
    bool container;
    i16 nutrition;   /* food: what eating it adds to hunger (OBJECTS.DAT 0xd82, signed; 0 when not food) */
    bool weapon;     /* a hand weapon (OBJECTS.DAT 2): its damage by attack, skill, durability */
    u8 slash, bash, stab, skill, wdurability;
    u8 power_min, rate, power_max; /* the swing (UW.EXE file 0x2574b, 0x257c8) */
    /* a critter (OBJECTS.DAT 0x132, 48 bytes; UW.EXE file 0x24b68, 0x259ee) */
    bool critter;
    u8 hp, carmour[4], attack, defense, strength;
    i8 a_bonus[3];
    u8 a_damage[3], a_chance[3];
    u16 xp;
    i16 ammo;        /* a launcher: the object it shoots (0 none) */
} ObjInfo;

bool objdata_load(const char *data_dir);
const ObjInfo *obj_info(int item);
/* a string of the game's (block, index); "" when there is none. "\n" in the files is a line break */
const char *str_get(int block, int index);
int obj_quantity(const Object *o);
/* "a torch", "3 torches", "some blood" */
void obj_name(const Object *o, char *out, size_t n);
/* the noun alone: "torch" */
void obj_noun(const Object *o, char *out, size_t n);
/* Look at an object, as the original says it: "You see a somewhat used torch." or for writing and
 * gravestones "The sign reads: Keep Out" */
void obj_describe(const Object *o, char *out, size_t n);
/* Look at a wall (its global texture number), a floor (floor texture) */
void wall_describe(int tex, char *out, size_t n);
void floor_describe(int tex, char *out, size_t n);
/* can object o go into a container of kind `container` already holding `held` tenths of a stone: 0 yes, else
 * the message to show (written into msg) */
bool obj_fits(int container, const Object *o, int held, char *msg, size_t n);
int obj_mass(const Object *o); /* tenths of a stone, with the quantity (not the contents) */
bool obj_stacks(int item);     /* can several lie as one (a count): a thing whose name has a plural */
