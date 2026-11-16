/* Conversations: the game's conversation programs (CNV.ARK, extracted to data\conversations\NNN.txt) run by a
 * small stack machine, as the original runs them. Layout of a program (from the file and the documented UW1
 * format): header (u16 0x0828, 0, code words, 0, 0, string block, variables, imports), the imports (name,
 * id = address or function number, 1, type 0x010F variable / 0x0111 function, return type), the code (words).
 * Opcodes 0x00-0x29: arithmetic and tests, jumps (JMP absolute; BEQ/BNE/BRA relative to the operand), CALL/RET,
 * CALLI (a built-in by number), PUSHI, PUSHI_EFF (bp + offset), stack frame ops, FETCHM/STO (memory = the
 * stack array), OFFSET (arrays), SAVE_REG/PUSH_REG (the result register), SAY_OP (a string), EXIT_OP. */
#pragma once
#include "base.h"

#define CONV_MAX_LINES 64
#define CONV_MAX_OPTIONS 10

typedef struct {
    bool running;          /* a conversation is going on */
    bool waiting;          /* waiting for the player to choose one of options */
    char lines[CONV_MAX_LINES][640]; /* what has been said (the newest last) */
    int nlines;
    char options[CONV_MAX_OPTIONS][160];
    int noptions;
    char who[64];
} Conv;

/* starts conversation n (0-319) with an NPC: its name, attitude, whether it has talked before */
bool conv_start(const char *data_dir, int n, const char *npc_name, const char *player_name);
/* the speaker's attitude for the next conversation (0 hostile, 1 upset, 2 mellow, 3 friendly: the npc_attitude
 * variable), and what it is when the conversation ends (set_attitude may change it) */
void conv_set_attitude(int a);
int conv_attitude(void);
const Conv *conv_state(void);
void conv_choose(int option);   /* 0-based, when waiting */
void conv_end(void);
/* Each conversation's own variables (the first BABGLOBS.DAT count of its memory: what the NPC remembers between
 * talks; the original keeps them in its save, BGLOBALS.DAT): kept here, written to and read from dir\conversations.txt */
bool conv_save(const char *dir);
bool conv_load(const char *dir);
void conv_forget(void); /* a new game: everyone forgets */
/* writes data\conversations\NNN.txt for every conversation in CNV.ARK that is missing; returns how many */
int conv_extract(const char *data_dir, const char *game_dir);
