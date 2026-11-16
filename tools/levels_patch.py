"""One-off: experience, levels, skill points, max hit points / mana and the carry limit as UW.EXE has them
(0x341d8, 0x8138a, 0x81305)."""
import sys

ROOT = sys.argv[1]


def patch(p, reps):
    s = open(ROOT + '/' + p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in s else '\n'
    for a, b in reps:
        a, b = a.replace('\n', nl), b.replace('\n', nl)
        assert s.count(a) == 1, (p, a[:70], s.count(a))
        s = s.replace(a, b)
    open(ROOT + '/' + p, 'w', encoding='utf-8', newline='').write(s)


LEVELS = r'''
/* Derived numbers (UW.EXE 0x81305): max hit points = 30 + level x strength / 5; max mana = (mana skill + 1) x
 * intelligence / 8; carry limit = strength x 20 tenths of a stone. */
static void character_derive(bool refill) {
    G.hp_max = (float)(30 + P.level * P.str / 5);
    G.mana_max = (float)((P.skill[SK_MANA] + 1) * P.intl / 8);
    if (refill) G.hp = G.hp_max, G.mana = G.mana_max;
    G.hp = MIN(G.hp, G.hp_max), G.mana = MIN(G.mana, G.mana_max);
    inv_set_capacity(P.str * 20);
}
/* Experience gained (0x341d8): halved (+1) when the level is above twice the dungeon level + 2; at most 96000.
 * A skill point for every 3000; levels by the table (exp / 500 against 0 1 2 3 4 6 8 12 16 24 32 48 64 96 128 192,
 * up to 16): each "You have attained experience level N" and a skill point (0x8138a), the derived numbers again. */
static void gain_exp(int v) {
    if (v <= 0) return;
    if (P.level > G.lv->number * 2 + 2) v = v / 2 + 1;
    u32 before = P.exp;
    P.exp = MIN(96000u, P.exp + (u32)v);
    P.skill_points += (int)(P.exp / 3000 - before / 3000);
    static const u8 TABLE[16] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192};
    int up = 0;
    while (P.level + up < 16 && TABLE[P.level + up] <= P.exp / 500) up++;
    if (up) {
        P.level += up, P.skill_points += up;
        game_message("%s%d", str_get(1, 147), P.level);
        character_derive(false);
    }
}
'''

patch('src/game.c', [
    ('''    int str, dex, intl, skill[SKILLS], vitality;
    u32 exp;
    bool easy;
} P;''', '''    int str, dex, intl, skill[SKILLS], vitality, level, skill_points;
    u32 exp;
    bool easy;
} P;'''),
    ('''    P.str = 20, P.dex = 15, P.intl = 13, P.vitality = 30, P.exp = 0, P.easy = false;''',
     '''    P.str = 20, P.dex = 15, P.intl = 13, P.vitality = 30, P.exp = 0, P.easy = false, P.level = 1, P.skill_points = 0;'''),
    ('''            else if (!strcmp(key, "experience")) P.exp = (u32)MAX(0, v);''',
     '''            else if (!strcmp(key, "experience")) P.exp = (u32)MAX(0, v);
            else if (!strcmp(key, "level")) P.level = MAX(1, MIN(16, v));
            else if (!strcmp(key, "skill_points")) P.skill_points = MAX(0, v);'''),
    ('''        n += (size_t)snprintf(text + n, sizeof text - n, "experience 0\\r\\neasy 0                # 1: +7 to attack, half the damage taken (UW.EXE)\\r\\n");''',
     '''        n += (size_t)snprintf(text + n, sizeof text - n, "experience 0\\r\\nlevel 1\\r\\nskill_points 0\\r\\neasy 0                # 1: +7 to attack, half the damage taken (UW.EXE)\\r\\n");'''),
    ('''    arena_reset(mark);
}

/* the worn armour's protection''', '''    arena_reset(mark);
}
''' + LEVELS + '''
/* the worn armour's protection'''),
    ('''        int v = ci->xp, xp = 4 * v + dice(v, 2);
        P.exp += (u32)xp;''', '''        int v = ci->xp;
        gain_exp(4 * v + dice(v, 2));'''),
    ('''    character_load();
    G.hp = G.hp_max = (float)character_vitality(), G.mana = G.mana_max = 20;''',
     '''    character_load();
    G.hp = G.hp_max = (float)character_vitality(), G.mana = G.mana_max = 20;
    character_derive(true);'''),
    ('static int character_vitality(void);\nbool game_start(', 'static int character_vitality(void);\nstatic void character_derive(bool refill);\nbool game_start('),
])
patch('src/inventory.h', [
    ('int inv_weapon(void);', 'int inv_weapon(void);\n/* the most the player can carry, tenths of a stone (strength x 20, UW.EXE 0x81305) */\nvoid inv_set_capacity(int tenths);'),
])
patch('src/inventory.c', [
    ('int inv_weapon(void) {', 'static int g_capacity = 1 << 30;\nvoid inv_set_capacity(int tenths) { g_capacity = tenths; }\nstatic int carried(void);\n\nint inv_weapon(void) {'),
    ('''    if (count_tree(lv, i, 0) > free) return game_message("%s", "That is too heavy to take."), false;''',
     '''    if (count_tree(lv, i, 0) > free) return game_message("%s", "That is too heavy to take."), false;
    /* the carry limit (strength x 2 stones): "That is too heavy for you to pick up." (strings 1:95) */
    {
        int m = obj_mass(o);
        for (int c = (o->flags & 0x40) ? 0 : o->link, guard = 0; obj_info(o->item)->container && c > 0 && c < 1024 && guard < 256; c = lv->obj[c].next, guard++)
            m += obj_mass(&lv->obj[c]);
        if (carried() + m > g_capacity) return game_message("%s", str_get(1, 95)), false;
    }'''),
    ('bool inv_holding(void) { return I.held != 0; }', '''bool inv_holding(void) { return I.held != 0; }
static int tree_mass(int p);
static int carried(void) {
    int m = 0;
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s]) m += tree_mass(I.slot[s]);
    return m + (I.held ? tree_mass(I.held) : 0);
}'''),
])
print('ok')
