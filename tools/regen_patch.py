"""One-off: the original's healing (UW.EXE 0x2adee..0x2af2d: mana + a mana check vs 10 every 3 ticks; poison hurts by
its level every 3 ticks; +1 hit point on a strength check vs 15 every 24 ticks), no combat messages (the original
shows none)."""
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


patch('src/inventory.c', [
    ('''static void player_tick(void) {
    I.tick++;
    burn_tick();
    if (I.tick % 3 == 0 && I.poison > 0) I.poison--;''', '''static void player_tick(void) {
    I.tick++;
    burn_tick();
    game_player_tick(I.tick, I.poison); /* healing, mana, poison's harm (the game has the numbers) */
    if (I.tick % 3 == 0 && I.poison > 0) I.poison--;'''),
])
patch('src/game.h', [
    ('bool game_escape(void);', 'bool game_escape(void);\n/* the player tick (every 21 s, counter 1-24): the regeneration and poison of UW.EXE 0x2acbd */\nvoid game_player_tick(int tick, int poison);'),
])
patch('src/game.c', [
    ('''    /* health comes back slowly; at nothing, the player wakes at the entrance of the Abyss */
    if (G.hp < G.hp_max) G.hp = MIN(G.hp_max, G.hp + dt * 0.08f);''', '''    /* at nothing, the player wakes at the entrance of the Abyss (the original's death: to decode) */'''),
    ('''static int character_vitality(void) { return P.vitality; }''', '''static int character_vitality(void) { return P.vitality; }

/* UW.EXE's player tick (0x2adee, 0x2af03): every 3rd tick mana + the result of a mana-skill check against 10 (when
 * it succeeds: 1 or 2) and poison hurts by its level; every 24th a strength check against 15 heals 1 hit point.
 * Resting and sleep heal more (to come). */
void game_player_tick(int tick, int poison) {
    if (tick % 3 == 0) {
        int r = skill_check(P.skill[SK_MANA], 10);
        if (r > 0) G.mana = MIN(G.mana_max, G.mana + (float)r);
        if (poison > 0) G.hp = MAX(1.0f, G.hp - (float)poison);
    }
    if (tick % 24 == 0 && skill_check(P.str, 15) > 0) G.hp = MIN(G.hp_max, G.hp + 1);
}'''),
    ('''    if (best < 0) return game_message("Thou swingest at the air.");''', '''    if (best < 0) return; /* the original says nothing in a fight (sounds only) */'''),
    ('''    if (r <= 0) return game_message("Thou missest the %s.", noun);''', '''    if (r <= 0) return;'''),
    ('''        game_message("Thou hast slain the %s!", noun);''', ''),
    ('''        game_message("Thou hittest the %s.", noun);''', ''),
    ('''    game_message("The %s hits thee.", noun ? noun + 1 : name);''', '''    (void)noun;'''),
])
print('ok')
