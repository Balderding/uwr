"""One-off: combat as UW.EXE does it (decoded 2026-11-08, see NOTES "Combat"): the player's attack score and damage,
the charge (weapon rate / power), the skill check, criticals, dice, hit location, armour, the angle bonus, the
creatures' attacks from the critter table, experience; the character's numbers in data\\character.txt."""
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


CHAR = r'''
/* --- the character (data\character.txt) ----------------------------------------------------------------------
 * The numbers the rules use: attributes, the skills (in UW.EXE's order: the player record from 0x1e: strength,
 * dexterity, intelligence, then attack, defense, unarmed, sword, axe, mace, missile, mana, lore, casting, traps,
 * search, track, sneak, repair, charm, picklock, acrobat, appraise, swimming), vitality, experience and the easy
 * setting (player byte 0xb4: +7 to attack, half the damage taken). Stand-ins until character creation is done. */
enum { SK_ATTACK, SK_DEFENSE, SK_UNARMED, SK_SWORD, SK_AXE, SK_MACE, SK_MISSILE, SK_MANA, SK_LORE, SK_CASTING, SK_TRAPS,
       SK_SEARCH, SK_TRACK, SK_SNEAK, SK_REPAIR, SK_CHARM, SK_PICKLOCK, SK_ACROBAT, SK_APPRAISE, SK_SWIMMING, SKILLS };
static const char *const SKILL_KEY[SKILLS] = {"attack", "defense", "unarmed", "sword", "axe", "mace", "missile", "mana", "lore", "casting",
                                              "traps", "search", "track", "sneak", "repair", "charm", "picklock", "acrobat", "appraise", "swimming"};
static struct {
    int str, dex, intl, skill[SKILLS], vitality;
    u32 exp;
    bool easy;
} P;
static void character_load(void) {
    static const int DEF[SKILLS] = {6, 6, 4, 6, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    P.str = 20, P.dex = 15, P.intl = 13, P.vitality = 30, P.exp = 0, P.easy = false;
    memcpy(P.skill, DEF, sizeof DEF);
    char path[700];
    snprintf(path, sizeof path, "%s\\character.txt", G.data);
    if (!file_exists(path)) {
        char text[2048];
        size_t n = (size_t)snprintf(text, sizeof text,
                                    "# uo-new character: the numbers the original's rules use (stand-ins until character creation is\r\n"
                                    "# done). Edit freely; delete to get these back.\r\n"
                                    "strength %d\r\ndexterity %d\r\nintelligence %d\r\nvitality %d\r\n", P.str, P.dex, P.intl, P.vitality);
        for (int k = 0; k < SKILLS; k++) n += (size_t)snprintf(text + n, sizeof text - n, "%s %d\r\n", SKILL_KEY[k], P.skill[k]);
        n += (size_t)snprintf(text + n, sizeof text - n, "experience 0\r\neasy 0                # 1: +7 to attack, half the damage taken (UW.EXE)\r\n");
        make_dirs_for(path);
        file_write(path, text, n);
    }
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        char key[32];
        int v;
        if (sscanf(p, "%31s %d", key, &v) == 2) {
            if (!strcmp(key, "strength")) P.str = v;
            else if (!strcmp(key, "dexterity")) P.dex = v;
            else if (!strcmp(key, "intelligence")) P.intl = v;
            else if (!strcmp(key, "vitality")) P.vitality = MAX(1, v);
            else if (!strcmp(key, "experience")) P.exp = (u32)MAX(0, v);
            else if (!strcmp(key, "easy")) P.easy = v != 0;
            else
                for (int k = 0; k < SKILLS; k++)
                    if (!strcmp(key, SKILL_KEY[k])) P.skill[k] = v;
        }
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
}
'''

COMBAT = r'''/* --- combat, as UW.EXE does it ---------------------------------------------------------------------------------
 * The skill check (file 0x3419c): r = skill - difficulty + a roll of 0-30; above 28 a critical (2), above 15 a
 * success (1), above 2 a failure (0), else a fumble (-1). Dice (0x38eee): n dice of 1-sides each. */
static int skill_check(int skill, int difficulty) {
    int r = skill - difficulty + rnd(31);
    return r > 28 ? 2 : r > 15 ? 1 : r > 2 ? 0 : -1;
}
static int dice(int n, int sides) {
    int s = 0;
    for (int k = 0; k < n && sides > 0; k++) s += 1 + rnd(sides);
    return s;
}
/* the damage roll (0x24cb5): at least 2; (base / 6) six-sided dice and one with (base % 6) sides */
static int roll_damage(int base) {
    base = MAX(base, 2);
    return dice(base / 6, 6) + (base % 6 ? dice(1, base % 6) : 0);
}
/* where a blow lands (0x2441a), by its height against the target's: 0 body, 1 arms, 2 legs, 3 head */
static int hit_part(float bottom, float top, float h) {
    float mid = (bottom + top) * 0.5f;
    if (h < bottom + 1.0f / 32) return 2;
    if (h > top - 1.0f / 32) return 3;
    if (h < mid) return rnd(2) ? 2 : (rnd(3) ? 0 : 1);
    return !rnd(3) ? 3 : (rnd(3) ? 0 : 1);
}
/* the angle bonus (0x25160): 0 face to face .. 4 from behind; headings in eighths */
static int angle_bonus(int target_dir, int attacker_dir) {
    int a = (target_dir + 12 - attacker_dir) & 7;
    return a > 4 ? 8 - a : a;
}
static int dir8(float dx, float dz) { return ((int)floorf(atan2f(dx, dz) / 0.785398f + 0.5f) % 8 + 8) % 8; }
#define CREATURE_HEIGHT 0.75f
#define EYE_HEIGHT 0.55f /* as main.c EYE */
'''

patch('src/game.c', [
    ('static void creature_name(const Creature *c, char *out, size_t n) { item_name(&G.lv->obj[c->obj], out, n); }',
     'static void creature_name(const Creature *c, char *out, size_t n) { item_name(&G.lv->obj[c->obj], out, n); }\n' + COMBAT + CHAR + r'''
/* the worn armour's protection for a body part (0 body: chest; 1 arms: gloves; 2 legs: leggings + boots;
 * 3 head: helm) */
static int player_armour(int part) {
    static const int SLOTS[4][2] = {{1, -1}, {2, -1}, {3, 4}, {0, -1}};
    int a = 0;
    for (int k = 0; k < 2; k++)
        if (SLOTS[part][k] >= 0) a += inv_protection(SLOTS[part][k]);
    return a;
}

/* a creature's blow at the player (0x259ee and 0x24b68): one of its three attacks by their chances; attack score =
 * the attack's bonus + the creature's attack skill / 2, less the player's armour where it lands; the check against
 * the player's defense; damage = the attack's damage + strength / 5, x1.5-2.5 on a critical, rolled as dice,
 * + the angle bonus; half on the easy setting */
static void creature_strikes(Creature *c, const float eye[3], float yaw) {
    const ObjInfo *ci = obj_info(0x40 + c->kind);
    int sum = ci->a_chance[0] + ci->a_chance[1] + ci->a_chance[2], pick = 0;
    if (sum > 0) {
        int r = rnd(sum);
        while (pick < 2 && r >= ci->a_chance[pick]) r -= ci->a_chance[pick], pick++;
    }
    int player_dir = dir8(sinf(yaw), cosf(yaw)), angle = angle_bonus(player_dir, c->dir);
    float feet = eye[1] - EYE_HEIGHT;
    int part = hit_part(feet, eye[1] + 0.1f, c->y + CREATURE_HEIGHT * 0.6f);
    int score = ci->a_bonus[pick] + ci->attack / 2 - player_armour(part);
    int r = skill_check(score + angle, P.skill[SK_DEFENSE]);
    if (r <= 0) return;
    int base = ci->a_damage[pick] + ci->strength / 5;
    if (r == 2) base = base * (48 + rnd(32)) / 32;
    int dmg = roll_damage(base) + angle;
    if (P.easy) dmg /= 2;
    G.hp -= (float)dmg;
    char name[96];
    creature_name(c, name, sizeof name);
    const char *noun = strchr(name, ' ');
    game_message("The %s hits thee.", noun ? noun + 1 : name);
}
'''),
    # creatures: their facing; their blows by the rules
    ('''    float x, z, y, hp, t, cooldown;
    bool hostile, struck;
} Creature;''', '''    float x, z, y, hp, t, cooldown;
    bool hostile, struck;
    int dir; /* the way it faces, in eighths (0 north) */
} Creature;'''),
    ('''    c->hp = (i < 256 && (o->mobile[0] & 0x7F)) ? (float)(o->mobile[0] & 0x7F) : 8.0f;''',
     '''    /* its hit points: its own (mobile byte 0), else its kind's (the critter table) */
    c->hp = (i < 256 && (o->mobile[0] & 0x7F)) ? (float)(o->mobile[0] & 0x7F) : (float)MAX(1, obj_info(o->item)->hp);
    c->dir = o->heading & 7;'''),
    ('static void creatures_update(float dt, const float eye[3]) {', 'static void creature_strikes(Creature *c, const float eye[3], float yaw);\nstatic void creatures_update(float dt, const float eye[3], float yaw) {'),
    ('''                c->struck = true;
                float dx = eye[0] - c->x, dz = eye[2] - c->z;
                if (dx * dx + dz * dz < 1.2f * 1.2f) {
                    int dmg = 1 + rnd(3);
                    G.hp -= (float)dmg;
                    char name[96];
                    creature_name(c, name, sizeof name);
                    const char *noun = strchr(name, ' ');
                    game_message("The %s hits thee.", noun ? noun + 1 : name);
                }''', '''                c->struck = true;
                float dx = eye[0] - c->x, dz = eye[2] - c->z;
                if (dx * dx + dz * dz < 1.2f * 1.2f) creature_strikes(c, eye, yaw);'''),
    ('''        if (d > 0.85f) {
            float sp = 0.9f * dt''', '''        c->dir = dir8(dx, dz); /* it faces the player it comes for */
        if (d > 0.85f) {
            float sp = 0.9f * dt'''),
    # the player's attack
    ('''/* the player's swing with power 0-1 (how long the button was held): the living creature the view ray passes
 * within reach. Damage scales from 40% (a quick click) to 100% (fully charged). The rules from UW.EXE (weapon
 * damage, skills, the charge rate) replace these with the combat code. */
static void player_attack(const float eye[3], const float fwd[3], float power) {
    if (g_swing > 0) return;
    g_swing = 0.5f;''', '''/* The player's blow (UW.EXE 0x25406, 0x24b68, 0x24cb5): charge 0-100. The weapon in the right hand (else the
 * fist, weapon 15) gives the power (power_min + (power_max - power_min) x charge / 100) and its skill (sword, axe,
 * mace; else unarmed). Attack score = attack / 2 + the weapon skill + dexterity / 7 (+ 7 easy); damage base =
 * strength / 9 + the weapon's damage for the attack (unarmed: unarmed x 2 / 5 + strength / 6 + 4). The check
 * (score + the angle bonus) against the creature's defense; a critical x1.5-2.5; dice; x power / 128 + the angle
 * bonus; less the creature's armour where it lands. A kill gives experience: 4 x its value + value d2. */
static void player_attack(const float eye[3], const float fwd[3], float charge) {
    if (g_swing > 0) return;
    g_swing = 0.5f;
    int witem = inv_weapon();
    const ObjInfo *wi = obj_info(witem >= 0 ? witem : 15);
    int power = wi->power_min + (wi->power_max - wi->power_min) * (int)(charge * 100) / 100;'''),
    ('''    if (best < 0) return game_message("Thou swingest at the air.");
    Creature *c = &g_cr[best];
    char name[96];
    creature_name(c, name, sizeof name);
    const char *noun = strchr(name, ' ');
    noun = noun ? noun + 1 : name;
    if (rnd(5) == 0) return game_message("Thou missest the %s.", noun);
    c->hp -= (float)(2 + rnd(5)) * (0.4f + 0.6f * power);
    c->hostile = true;
    if (c->hp <= 0) {
        c->state = CS_DYING, c->t = 0;
        game_message("Thou hast slain the %s!", noun);''', '''    if (best < 0) return game_message("Thou swingest at the air.");
    Creature *c = &g_cr[best];
    char name[96];
    creature_name(c, name, sizeof name);
    const char *noun = strchr(name, ' ');
    noun = noun ? noun + 1 : name;
    const ObjInfo *ci = obj_info(0x40 + c->kind);
    int skill = wi->skill >= 3 && wi->skill <= 5 ? wi->skill : 2; /* 3 sword 4 axe 5 mace, else unarmed */
    int score = P.skill[SK_ATTACK] / 2 + P.skill[SK_UNARMED + skill - 2] + P.dex / 7 + (P.easy ? 7 : 0);
    /* the attack: the weapon's strongest of slash, bash and thrust (how the original picks it is still open) */
    int dmg_type = wi->slash >= wi->bash && wi->slash >= wi->stab ? wi->slash : wi->bash >= wi->stab ? wi->bash : wi->stab;
    int base = skill == 2 ? P.skill[SK_UNARMED] * 2 / 5 + P.str / 6 + 4 : P.str / 9 + dmg_type;
    float hit_h = eye[1] + fwd[1] * bt; /* where the crosshair meets it */
    int part = hit_part(c->y, c->y + CREATURE_HEIGHT, hit_h);
    int player_dir = dir8(fwd[0], fwd[2]), angle = angle_bonus(c->dir, player_dir);
    int r = skill_check(score + angle, ci->defense);
    c->hostile = true;
    if (r <= 0) return game_message("Thou missest the %s.", noun);
    if (r == 2) base = base * (48 + rnd(32)) / 32;
    int dmg = roll_damage(base) * power / 128 + angle;
    int armour = ci->carmour[part] == 255 ? ci->carmour[0] : ci->carmour[part];
    dmg = MAX(0, dmg - armour);
    c->hp -= (float)dmg;
    if (c->hp <= 0) {
        c->state = CS_DYING, c->t = 0;
        int v = ci->xp, xp = 4 * v + dice(v, 2);
        P.exp += (u32)xp;
        game_message("Thou hast slain the %s!", noun);'''),
    ('''    creatures_update(dt, eye);''', '''    creatures_update(dt, eye, yaw);'''),
    ('''    (void)yaw;
    for (int i = 0; i < 4; i++) G.msg_age[i] += dt;''', '''    for (int i = 0; i < 4; i++) G.msg_age[i] += dt;'''),
    # the charge: the weapon's rate every 1/16 s up to 100 (0x2574b)
    ('''            if (in->attack_held) G.charge = MIN(1.0f, G.charge + dt / CHARGE_TIME);''',
     '''            if (in->attack_held) { /* the weapon's rate every 1/16 s, up to 100 (UW.EXE 0x2574b) */
                int w = inv_weapon();
                G.charge = MIN(1.0f, G.charge + dt * 16.0f * obj_info(w >= 0 ? w : 15)->rate / 100.0f);
            }'''),
    ('''#define CHARGE_TIME 0.8f /* seconds of holding the attack button for a full-power blow */\n''', ''),
    # the power gem: charge / 12 + 1 (0x25763)
    ('''        int g = G.charge < 0 ? 0 : G.charge >= 1.0f ? 9 + (int)(G.anim * 10) % 5 : 1 + (int)(G.charge * 7.99f);''',
     '''        int g = G.charge < 0 ? 0 : G.charge >= 1.0f ? 9 + (int)(G.anim * 10) % 5 : 1 + (int)(G.charge * 100) / 12; /* UW.EXE */'''),
    ('''    objdata_load(data_dir);
    inv_start(data_dir, give_torch);
    G.hp = G.hp_max = 30, G.mana = G.mana_max = 20;''', '''    objdata_load(data_dir);
    inv_start(data_dir, give_torch);
    character_load();
    G.hp = G.hp_max = (float)P.vitality, G.mana = G.mana_max = 20;'''),
])
patch('src/inventory.h', [
    ('bool inv_has_key(int id);', 'bool inv_has_key(int id);\n'
     '/* the weapon in the right hand (its object number), or -1 */\nint inv_weapon(void);\n'
     '/* the protection of what is worn in body slot s (0 helm .. 4 boots), 0 when nothing */\nint inv_protection(int s);'),
])
patch('src/inventory.c', [
    ('bool inv_has_key(int id) {', '''int inv_weapon(void) {
    int p = I.slot[7];
    if (!p) return -1;
    int item = I.pool[p].item & 511;
    return obj_info(item)->weapon ? item : -1;
}
int inv_protection(int s) {
    int p = s >= 0 && s < INV_BODY ? I.slot[s] : 0;
    return p ? obj_info(I.pool[p].item)->protection : 0;
}

bool inv_has_key(int id) {'''),
])
print('ok')
