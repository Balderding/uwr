/* orig.h: UW.EXE's shared routines, ported one to one (each with its file offset). The game's rules call these, so
 * every roll, check and formula goes the way the original's does; tools/verify.py runs the original routine in an
 * emulator beside each of these and compares them. */
#pragma once
#include "base.h"

/* Borland C's rand (UW.EXE 0x12c37): seed = seed * 0x015A4E35 + 1, the result (seed >> 16) & 0x7FFF; srand (0x12c26) */
void uw_srand(u16 seed);
int uw_rand(void);
u32 uw_seed(void);
void uw_set_seed(u32 seed);
/* rand() % n: the original's most common form (cdq; idiv) */
int uw_rand_mod(int n);
/* rand() x n / 0x8000 (long multiply and divide, 0x35fb / 0x3636): 0 .. n - 1 */
int uw_rand_scaled(int n);
/* dice (0x38eee): count + the sum of count rolls of rand() x sides / 0x8000 (each 1 .. sides); none when either <= 0 */
int uw_dice(int count, int sides);
/* the skill check (0x3419c): skill - difficulty + rand() % 31; above 28: 2 (critical), above 15: 1, above 2: 0, else
 * -1 (a fumble) */
int uw_skill_check(int skill, int difficulty);
/* harm of a kind against a resistance byte (0x25eaf): the magic part (bits 0-1) resisted when rand() % 3 < the
 * resistance's bits 0-1 (else dropped from the kind); any other bit in both: nothing. The harm left. */
int uw_resist(int res, int harm, int kind);

/* the player's experience record (player bytes 0x3D level, 0x4E exp (in tenths: the panel shows exp / 10), 0x52 skill
 * points, 0x53 skill points earned) */
typedef struct {
    u32 exp;
    u8 level, skill_points, earned;
} UwExp;
/* player_get_exp (0x341d8, underworld-exhumed SKILLCHK.C:72): n (a 16-bit int) tenths of experience. A loss is taken
 * off, floored at 0, nothing else. A gain: none once exp is past 0x17700; halved + 1 when the level is above twice the
 * dungeon level + 2; a skill point for every 3000 of (exp + n) not yet earned; then the levels exp / 500 reaches in
 * the table, at most 16, each (advance, SKILLS.C:119) + 1 level and + 1 skill point. Returns the levels gained. */
int uw_player_get_exp(UwExp *p, int n, int dungeon_level);
/* player_killed_a (0x25b35, COMBAT.C:830): the experience for a kill: exp x 4 + 2d(exp) as a 16-bit int; a powerful
 * one (attitude word bit 10) x (24 + rand() % 24) / 16, the product wrapping at 16 bits as the original's does */
int uw_kill_exp(int crit_exp, bool powerful);
