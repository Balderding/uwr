"""One-off: eating and drinking (UW.EXE 0x374ee), hunger and fatigue (the player tick 0x2acbd: a counter 1-24,
reset at 24 when hunger falls 3-6 and fatigue rises), the status line; objects.txt v3 with the food table."""
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


# --- objects.txt v3: food lines ---------------------------------------------------------------------------
patch('src/extract.c', [
    ('"# uo-new objects v2: what the original knows', '"# uo-new objects v3: what the original knows'),
    ('!file_starts_with(path, "# uo-new objects v2")', '!file_starts_with(path, "# uo-new objects v3")'),
    ('"#   container N  capacity (tenths of a stone, 0 no limit) accepts (-1 anything, an object, 512 runes,\\r\\n"\n'
     '                "#          513 ammunition, 514 maps and scrolls, 515 food)\\r\\n");',
     '"#   container N  capacity (tenths of a stone, 0 no limit) accepts (-1 anything, an object, 512 runes,\\r\\n"\n'
     '                "#          513 ammunition, 514 maps and scrolls, 515 food)\\r\\n"\n'
     '                "#   food N  nutrition (what eating it adds to hunger, 0-255; below 0: a drink)\\r\\n");'),
    ('                n += (size_t)snprintf(text + n, cap - n, "container %3d  capacity %d accepts %d\\r\\n", 0x80 + k, e[0], rd16s(e + 1));\n'
     '            }',
     '                n += (size_t)snprintf(text + n, cap - n, "container %3d  capacity %d accepts %d\\r\\n", 0x80 + k, e[0], rd16s(e + 1));\n'
     '            }\n'
     '            for (int k = 0; k < 16; k++) /* OBJECTS.DAT 0xd82: a signed byte for 0xB0-0xBF */\n'
     '                n += (size_t)snprintf(text + n, cap - n, "food %3d  nutrition %d\\r\\n", 0xb0 + k, (int)(i8)od.data[0xd82 + k]);'),
])
patch('src/objdata.h', [
    ('    bool container;\n} ObjInfo;', '    bool container;\n    i16 nutrition;   /* food: what eating it adds to hunger (OBJECTS.DAT 0xd82, signed; 0 when not food) */\n} ObjInfo;'),
])
patch('src/objdata.c', [
    ('                else if (!strcmp(key, "accepts")) o->accepts = (i16)v;',
     '                else if (!strcmp(key, "accepts")) o->accepts = (i16)v;\n'
     '                else if (!strcmp(key, "nutrition")) o->nutrition = (i16)v;'),
])

# --- inventory.c: vitals, the tick, eating ------------------------------------------------------------------
patch('src/inventory.c', [
    ('    u8 tick;\n', '    u8 tick;            /* the player tick\'s counter: 1-24, back to 0 at 24 (UW.EXE [0x3f9]) */\n'
                     '    int hunger;         /* 0-255, a new character 192 (player byte 0x39) */\n'
                     '    int fatigue;        /* 0-255 (player byte 0x3a), up 1 every 24 ticks */\n'
                     '    int drunk, poison;  /* player 0x61 bits 4-9; 0x5f bits 2-5 */\n'
                     '    u32 rng;\n'),
    ('    I.hover = -1;\n    load_light_table();', '    I.hover = -1, I.hunger = 192, I.rng = 0x1234567;\n    load_light_table();'),
    ('static void burn_tick(void) {\n    I.tick++;', 'static void burn_tick(void) {'),
    ('    if (I.tick_secs > 20) I.tick_secs = 0, burn_tick();',
     '    if (I.tick_secs > 20) I.tick_secs = 0, player_tick();'),
    ('void inv_tick(float dt) {',
     '''static int rnd(int n) {
    I.rng = I.rng * 1103515245u + 12345u;
    return (int)((I.rng >> 16) % (u32)n);
}

/* The player tick (UW.EXE file 0x2acbd), every 21 seconds: the counter goes up; the lights burn; every 3rd
 * tick a point of poison wears off; every 24th hunger falls 3-6, drunkenness 1, fatigue rises 1, and the
 * counter starts again from 0 (so a lantern, duration 10, burns at 10 and 20 of each 24: about a point every
 * 12 ticks). */
static void player_tick(void) {
    I.tick++;
    burn_tick();
    if (I.tick % 3 == 0 && I.poison > 0) I.poison--;
    if (I.tick % 24 == 0) {
        I.hunger = MAX(0, I.hunger - (3 + rnd(4)));
        if (I.drunk > 0) I.drunk--;
        I.fatigue = MIN(255, I.fatigue + 1);
        I.tick = 0;
    }
}

/* "You are well fed and wide awake." (file 0x26122: block 1, 104 + hunger / 30, 118 - fatigue / 23 up to 5) */
void inv_status(char *out, size_t n) {
    snprintf(out, n, "You are %s and %s.", str_get(1, 104 + MIN(8, I.hunger / 30)), str_get(1, 118 - MIN(5, I.fatigue / 23)));
}

void inv_tick(float dt) {'''),
    ('''    if (obj_info(o->item)->container) {
        if (I.nopen < 8) I.open[I.nopen++] = p, I.scroll = 0;
        return;
    }
    game_message("You cannot use %s yet.", name);''',
     '''    if (obj_info(o->item)->container) {
        if (I.nopen < 8) I.open[I.nopen++] = p, I.scroll = 0;
        return;
    }
    int eaten = eat(p);
    if (eaten > 0) consume(p);
    if (eaten >= 0) return;
    game_message("You cannot use %s yet.", name);'''),
    ('static void use_thing(int p) {',
     '''/* one of thing p is gone (eaten, drunk): a count goes down, else it leaves the inventory */
static void consume(int p) {
    Object *o = &I.pool[p];
    if (obj_quantity(o) > 1) {
        o->link--;
        return;
    }
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] == p) I.slot[s] = 0;
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] && holds(&I.pool[I.slot[s]])) unlink_from(I.slot[s], p);
    if (I.held == p) I.held = 0;
    free_tree(p);
}

/* Eating and drinking (UW.EXE file 0x374ee): -1 not food, 0 not eaten, 1 eaten. Food (0xB0-0xBF) adds its
 * nutrition to hunger (refused past 255: "You are too full to eat that now.") and says how it tasted (block 1,
 * 172 + (0-19 + quality) / 16, up to 4); the special ones say their own: leeches 229, candle 230, toadstool
 * 231 (poisons), mushroom 232, plants 233/236 (nutrition 23/4), rotworm 234 (4), stew 235 (64), water 237,
 * port 238, ale 239, potions 240. A drink below -1 (ale, port) makes drunk; wine: "You are unable to open the
 * wine bottle." */
static int eat(int p) {
    Object *o = &I.pool[p];
    int item = o->item & 511, say = 0, food = 255;
    if ((item >> 4) == 0xb) food = obj_info(item)->nutrition;
    switch (item) {
    case 0x125: say = 229; break;
    case 0x92: say = 230; break;
    case 0xb9: say = 231; break;
    case 0xb8: say = 232; break;
    case 0xcf: say = 233, food = 23; break;
    case 0xd9: say = 234, food = 4; break;
    case 0x11b: say = 235, food = 64; break;
    case 0xce: say = 236, food = 4; break;
    case 0xbd: say = 237; break;
    case 0xbe: say = 238; break;
    case 0xba: say = 239; break;
    case 0xbb: case 0xbc: say = 240; break;
    case 0xbf: return game_message("%s", str_get(1, 127)), 0;
    default:
        if (food == 255) return -1;
    }
    if (food > 0 && food != 255) {
        if (I.hunger + food > 255) return game_message("%s", str_get(1, 126)), 0;
        I.hunger += food;
    }
    if (!say) {
        char noun[64];
        obj_noun(o, noun, sizeof noun);
        game_message("The %s%s", noun, str_get(1, 172 + MIN(4, (rnd(20) + o->quality) >> 4)));
    } else game_message("%s", str_get(1, say));
    if (item == 0xb9) I.poison = I.poison < 4 ? 4 : MIN(15, I.poison + 2);
    if (food < -1 && food > -127) I.drunk = MIN(63, I.drunk - food);
    return 1;
}

static void use_thing(int p) {'''),
])
patch('src/inventory.h', [
    ('bool inv_light(float *intensity, float *range, float color[3]);',
     'bool inv_light(float *intensity, float *range, float color[3]);\n'
     '/* "You are well fed and wide awake." */\nvoid inv_status(char *out, size_t n);'),
])
print('ok')
