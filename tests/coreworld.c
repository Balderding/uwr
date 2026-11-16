/* coreworld.c: the rules core running the game world on its own, natively: the original's start-up (UWEDIT.C's
 * init_world, less the screen, sound and input), a new game on level 1 (MAINMENU.C's "Begin", less character
 * creation), then the world step (PLAYMOVE.C's check_physics) with the player walking forward, printing where the
 * player is. A smoke test of the core as the game's rules; compiled like the core (compat.h force-included).
 *   coreworld GAME_DIR WRITE_DIR [steps] */
#include <stdio.h>
#include "uw2.h"
#include "map.h"
#include "motion.h"
#include "object.h"
#include "player.h"
#undef main  /* compat.h renames the game's; this is the program's own */

extern char core_game_dir[260], core_write_dir[260];
extern uint32_t core_time;
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
void far check_physics(void);
char far copy_file(char *from, char *to);
extern int16 PlayerInput, ForwInpRate, TurnInpRate;

int main(int argc, char **argv)
{
    static unsigned char exe[0x90000];
    char path[600];
    if (argc < 3) return 2;
    snprintf(core_game_dir, sizeof core_game_dir, "%s", argv[1]);
    snprintf(core_write_dir, sizeof core_write_dir, "%s", argv[2]);
    int steps = argc > 3 ? atoi(argv[3]) : 200;
    snprintf(path, sizeof path, "UW.EXE"); /* fopen is Borland's here: under the game folder */
    FILE *f = fopen(path, "rb");
    long n = f ? (long)fread(exe, 1, sizeof exe, f) : 0;
    if (f) fclose(f);
    if (!core_math_init(exe, n)) { printf("no UW.EXE\n"); return 1; }
    printf("init_strings %d\n", init_strings());
    Map_Init();
    printf("init_objects %d\n", init_objects());
    init_txtlib();
    init_player();
    init_ai();
    init_char(0);
    init_lighting();
    init_combinables();
    printf("copy LEV.ARK %d\n", copy_file("DATA\\lev.ark", "SAVE0\\lev.ark"));
    printf("init_babl %d\n", init_babl());
    printf("GetLevel(1) %d\n", GetLevel(1));
    player_setup(0x20, 2, 1);
    do_level_hacks(1, 0);
    printf("start: tile %d,%d  PN %d,%d,%d heading %d\n", OBJ_HOMEX(ThePlayer), OBJ_HOMEY(ThePlayer), PN.x, PN.y, PN.z,
           (int)(uint16)PN.heading);
    for (int i = 0; i < steps; i++) {
        core_time += 8; /* 1/32 s a step */
        PlayerInput = PIN_FORWARD; /* as do_player_keyboard sets it for the forward key */
        ForwInpRate = 0x70;
        TurnInpRate = i >= 100 && i < 120 ? 0x5A : 0; /* a turn to the right on the way */
        check_physics();
        if (i % 40 == 39)
            printf("step %d: tile %d,%d  PN %d,%d,%d speed %d\n", i + 1, OBJ_HOMEX(ThePlayer), OBJ_HOMEY(ThePlayer),
                   PN.x, PN.y, PN.z, PN.speed);
    }
    return 0;
}
