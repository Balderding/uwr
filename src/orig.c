/* orig.c: UW.EXE's shared routines, ported one to one; see orig.h. */
#include "orig.h"

static u32 g_seed = 1; /* DS 0x207e (a dword) */

void uw_srand(u16 seed) { g_seed = seed; } /* 0x12c26: the high word cleared */
u32 uw_seed(void) { return g_seed; }
void uw_set_seed(u32 seed) { g_seed = seed; }

int uw_rand(void) { /* 0x12c37 */
    g_seed = g_seed * 0x015A4E35u + 1u;
    return (int)((g_seed >> 16) & 0x7FFF);
}

int uw_rand_mod(int n) { return n > 0 ? uw_rand() % n : 0; }

int uw_rand_scaled(int n) { return (int)((i32)uw_rand() * (i32)n / 0x8000); }

int uw_dice(int count, int sides) { /* 0x38eee */
    int sum = count;
    if (sides <= 0 || count <= 0) return count; /* the loop never runs: the count itself */
    while (count--) sum += uw_rand_scaled(sides);
    return sum;
}

int uw_skill_check(int skill, int difficulty) { /* 0x3419c */
    int r = skill - difficulty + uw_rand() % 31;
    if (r > 28) return 2;
    if (r > 15) return 1;
    if (r > 2) return 0;
    return -1;
}

int uw_resist(int res, int harm, int kind) { /* 0x25eaf */
    if (!(kind & res)) return harm;
    if (kind & 3) {
        if (uw_rand() % 3 < (res & 3)) return 0;
        kind &= 0xfc;
    }
    if (kind & res) return 0;
    return harm;
}

int uw_player_get_exp(UwExp *p, int n, int dungeon_level) { /* 0x341d8 */
    static const u8 LEVELS[15] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192};
    n = (i16)n;
    if (n < 0) {
        i16 neg = (i16)-n; /* (16-bit: -(-32768) stays negative, and as unsigned long is huge) */
        if ((u32)(i32)neg > p->exp) p->exp = 0;
        else p->exp += (u32)(i32)n;
        return 0;
    }
    if (p->exp > 0x17700u) return 0;
    if ((i16)(dungeon_level * 2 + 2) < p->level) n = n / 2 + 1;
    int points = (i16)((p->exp + (u32)n) / 3000);
    if (points > p->earned) p->skill_points = (u8)(p->skill_points + (points - p->earned)), p->earned = (u8)points;
    p->exp += (u32)n;
    int levels = 0;
    points = (i16)(p->exp / 500);
    /* (the original reads the table first: at level + levels = 16 the byte past it, then stops on the level test) */
    while (p->level + levels < 16 && p->level + levels - 1 >= 0 && LEVELS[p->level + levels - 1] <= points) levels++;
    if (levels) p->level = (u8)(p->level + levels), p->skill_points = (u8)(p->skill_points + levels); /* advance */
    return levels;
}

int uw_kill_exp(int crit_exp, bool powerful) { /* 0x25b35 */
    int e = (i16)(crit_exp * 4 + uw_dice(2, crit_exp));
    if (powerful) e = (i16)(e * (uw_rand() % 24 + 24)) / 16;
    return (i16)e;
}
