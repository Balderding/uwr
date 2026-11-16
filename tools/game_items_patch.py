"""One-off: game.c uses objdata (names, look) and inventory (the pack, the lights); Look (L / left click on
what is not a creature); E on what cannot be picked up looks at it. main.c: the look key, right click = use."""
import sys

ROOT = sys.argv[1]


def load(p):
    s = open(ROOT + '/' + p, encoding='utf-8', newline='').read()
    return s, ('\r\n' if '\r\n' in s else '\n')


def save(p, s):
    open(ROOT + '/' + p, 'w', encoding='utf-8', newline='').write(s)


def rep(s, nl, a, b, count=1):
    a2, b2 = a.replace('\n', nl), b.replace('\n', nl)
    assert s.count(a2) == count, (a[:90], s.count(a2))
    return s.replace(a2, b2)


def cut(s, nl, start, end):
    """removes from the line `start` up to (not including) `end`"""
    a = s.index(start.replace('\n', nl))
    b = s.index(end.replace('\n', nl), a)
    return s[:a] + s[b:]


# ------------------------------------------------------------------------------------------------ game.c
s, nl = load('src/game.c')
s = rep(s, nl, '#include "conv.h"\n', '#include "conv.h"\n#include "inventory.h"\n#include "objdata.h"\n')
s = rep(s, nl, '''#define INV_SLOTS 40
#define REACH 2.2f /* tiles: how far away things can be picked up */

/* light sources (strings block 4): 144 lantern, 145 torch, 146 candle, 147 taper; +4 = lit */
enum { ITEM_LANTERN = 144, ITEM_TORCH = 145, ITEM_CANDLE = 146, ITEM_TAPER = 147, LIT = 4 };
''', '''#define REACH 2.2f /* tiles: how far away things can be picked up */
''')
s = rep(s, nl, '''    Object inv[INV_SLOTS];
    int ninv;
    bool open;
    float clock;           /* the part second toward the next whole one (the burning, below) */
    int tick_secs;         /* whole seconds since the last tick */
    u8 tick;               /* the tick counter (a byte, as in UW.EXE) */
''', '''    bool open;             /* the inventory panel */
''')
s = rep(s, nl, '''    int mouse_x, mouse_y, hover_slot;
    char names[512][64];
''', '''    int mouse_x, mouse_y;
''')
s = cut(s, nl, '/* --- names (data\\strings\\block0004.txt', 'void game_message(const char *fmt, ...) {')
s = rep(s, nl, '''static void item_name(''', '''static void item_name(''', 0) if False else s
# messages: drop a trailing line break (the game's strings end with one)
s = rep(s, nl, '''    vsnprintf(G.msg[0], sizeof G.msg[0], fmt, ap);
    va_end(ap);
''', '''    vsnprintf(G.msg[0], sizeof G.msg[0], fmt, ap);
    va_end(ap);
    for (size_t k = strlen(G.msg[0]); k > 0 && (G.msg[0][k - 1] == '\\n' || G.msg[0][k - 1] == ' '); k--) G.msg[0][k - 1] = 0;
''')
s = rep(s, nl, '''static void load_light_table(void);
bool game_start(const char *data_dir, bool give_torch) {
    memset(&G, 0, sizeof G);
    str_copy(G.data, sizeof G.data, data_dir);
    load_names();
    load_light_table();
    G.hp = G.hp_max = 30, G.mana = G.mana_max = 20;
    G.hover_obj = -1, G.hover_slot = -1;
    if (give_torch) { /* a lit torch to start with: the dungeon is dark */
        Object *t = &G.inv[G.ninv++];
        memset(t, 0, sizeof *t);
        t->item = ITEM_TORCH + LIT, t->quality = 40; /* a fresh torch, as they lie about */
    }
    return true;
}''', '''bool game_start(const char *data_dir, bool give_torch) {
    memset(&G, 0, sizeof G);
    str_copy(G.data, sizeof G.data, data_dir);
    objdata_load(data_dir);
    inv_start(data_dir, give_torch);
    G.hp = G.hp_max = 30, G.mana = G.mana_max = 20;
    G.hover_obj = -1;
    return true;
}

/* "a torch": the name the messages use */
static void item_name(const Object *o, char *out, size_t n) { obj_name(o, out, n); }''')
# the old inventory: unlink/free_object/take/drop/use/lights/burn/panel go (inventory.c has them)
s = cut(s, nl, '/* takes object i out of its tile\'s chain */', '/* the item the view ray meets first within reach')
s = cut(s, nl, '/* --- the inventory -----', 'bool game_update(float dt,')
s = rep(s, nl, 'bool game_update(float dt,', '''bool game_light(float *intensity, float *range, float color[3]) { return inv_light(intensity, range, color); }

/* --- Look ---------------------------------------------------------------------------------------------------
 * What the crosshair is on, put into words as the original does: a creature, a door, an object (its quality
 * and owner, a writing's text), else the wall, floor or ceiling the view ray meets (strings block 10). */
static void look_world(const float eye[3], const float fwd[3]) {
    char line[400];
    if (G.hover_creature >= 0) {
        char name[96];
        item_name(&G.lv->obj[g_cr[G.hover_creature].obj], name, sizeof name);
        return game_message("You see %s.", name);
    }
    if (G.hover_door >= 0) {
        const Door *d = &G.doors[G.hover_door];
        if ((d->item & 7) != 7 || d->open > 0.5f) return game_message("You see %s.", d->open > 0.5f ? "an open door" : "a door");
    }
    if (G.hover_obj >= 0) {
        obj_describe(&G.lv->obj[G.hover_obj], line, sizeof line);
        return game_message("%s", line);
    }
    /* the ray through the level: the first wall, floor or ceiling, up to 8 tiles away */
    int last = -1;
    for (float t = 0.02f; t < 8.0f; t += 0.02f) {
        float x = eye[0] + fwd[0] * t, y = eye[1] + fwd[1] * t, z = eye[2] + fwd[2] * t;
        if (x < 0 || z < 0 || x >= 64 || z >= 64) break;
        int ti = (int)z * 64 + (int)x;
        if (level_solid_at(G.lv, x, z)) {
            const Tile *o = last >= 0 ? &G.lv->tile[last] : &G.lv->tile[ti];
            wall_describe(G.lv->walls[o->wall % 48], line, sizeof line);
            return game_message("%s", line);
        }
        last = ti;
        if (y < level_floor_y(G.lv, x, z)) {
            floor_describe(G.lv->floors[G.lv->tile[ti].floor % 10], line, sizeof line);
            return game_message("%s", line);
        }
        if (y > level_ceil_y()) {
            floor_describe(G.lv->floors[9], line, sizeof line);
            return game_message("%s", line);
        }
    }
    game_message("You see nothing of note.");
}

bool game_update(float dt,''')
s = rep(s, nl, '''    if (!G.lv) return G.open;
    burn(dt);''', '''    if (!G.lv) return G.open;
    inv_tick(dt);''')
s = rep(s, nl, '''    if (in->inventory) G.open = !G.open;
    g_swing -= dt;
    creatures_update(dt, eye);
    if (in->attack && !G.open) player_attack(eye, fwd);''', '''    if (in->inventory) G.open = !G.open;
    g_swing -= dt;
    creatures_update(dt, eye);''')
s = rep(s, nl, '''    if (in->torch) {
        int k = -1;
        for (int i = 0; i < G.ninv && k < 0; i++)
            if (is_lit(G.inv[i].item)) k = i;
        for (int i = 0; i < G.ninv && k < 0; i++)
            if (is_light(G.inv[i].item) && G.inv[i].quality) k = i;
        if (k >= 0) use_slot(k);
        else game_message("Thou hast no light.");
    }
    G.hover_slot = -1;
    if (G.open) {
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        int px, py;
        panel_rect(&px, &py);
        int cx = (in->mouse_x - px) / CELL, cy = (in->mouse_y - py) / CELL;
        if (in->mouse_x >= px && in->mouse_y >= py && cx < COLS && cy * COLS + cx < INV_SLOTS) G.hover_slot = cy * COLS + cx;
        if (G.hover_slot >= G.ninv) G.hover_slot = -1;
        if (G.hover_slot >= 0 && in->click_left) use_slot(G.hover_slot);
        else if (G.hover_slot >= 0 && in->click_right) drop(G.hover_slot, eye, fwd);
        G.hover_obj = -1;
    } else {''', '''    if (in->torch) inv_toggle_light();
    if (G.open) {
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        InvInput ii = {in->click_left, in->click_right, in->look, in->mouse_x, in->mouse_y};
        if (inv_update(G.lv, &ii, eye, fwd) || in->click_left) rebuild_sprites();
        G.hover_obj = -1;
    } else {''')
s = rep(s, nl, '''        if (in->use && G.hover_creature >= 0) {
            if (!g_cr[G.hover_creature].hostile) talk(G.hover_creature);
            else game_message("It is in no mood to talk.");
            return G.talking;
        }
        if (in->use && G.hover_obj >= 0) take(G.hover_tx, G.hover_tz, G.hover_obj);
        else if (in->use && G.hover_door >= 0) use_door(G.hover_door);''', '''        if (in->use && G.hover_creature >= 0) {
            if (!g_cr[G.hover_creature].hostile) talk(G.hover_creature);
            else game_message("It is in no mood to talk.");
            return G.talking;
        }
        /* left click: a swing at a creature in reach, else a look (the original's look mode, built in) */
        if (in->attack) {
            if (G.hover_creature >= 0 && creature_at(eye, fwd, 1.7f) >= 0) player_attack(eye, fwd);
            else look_world(eye, fwd);
        }
        if (in->look) look_world(eye, fwd);
        if (in->use && G.hover_obj >= 0) {
            /* what cannot be picked up (writing, blood, a fountain) is looked at */
            if (obj_info(G.lv->obj[G.hover_obj].item)->pick) {
                if (inv_take(G.lv, G.hover_tx, G.hover_tz, G.hover_obj)) rebuild_sprites();
            } else look_world(eye, fwd);
        } else if (in->use && G.hover_door >= 0) use_door(G.hover_door);''')
# the HUD: the hover text for things that cannot be taken; the panel
s = rep(s, nl, '''            ui_text(FONT_NORMAL, name, o->w / 2 - w / 2, o->h / 2 + 10, 255, 230, 160);
            ui_text(FONT_SMALL, "E - TAKE", o->w / 2 - ui_text_width(FONT_SMALL, "E - TAKE") / 2, o->h / 2 + 20, 200, 200, 200);''',
            '''            ui_text(FONT_NORMAL, name, o->w / 2 - w / 2, o->h / 2 + 10, 255, 230, 160);
            const char *hint = obj_info(G.lv->obj[G.hover_obj].item)->pick ? "E - TAKE   L - LOOK" : "E / L - LOOK";
            ui_text(FONT_SMALL, hint, o->w / 2 - ui_text_width(FONT_SMALL, hint) / 2, o->h / 2 + 20, 200, 200, 200);''')
a = s.index('    if (!G.open) return;' + nl + '    /* the inventory */')
b = s.index(nl + '}', a)
s = s[:a] + '    if (G.open) inv_draw();' + s[b:]
save('src/game.c', s)

# ------------------------------------------------------------------------------------------------ game.h
s, nl = load('src/game.h')
s = rep(s, nl, '''    bool escape;           /* leave the conversation */''', '''    bool escape;           /* leave the conversation */
    bool look;             /* look at what the crosshair (or, in the pack, the mouse) is on */''')
save('src/game.h', s)

# ------------------------------------------------------------------------------------------------ main.c
s, nl = load('src/main.c')
s = rep(s, nl, 'A_SCREENSHOT, A_USE, A_INVENTORY, ACTIONS };', 'A_SCREENSHOT, A_USE, A_INVENTORY, A_LOOK, ACTIONS };')
s = rep(s, nl, '"torch", "screenshot", "use", "inventory"};', '"torch", "screenshot", "use", "inventory", "look"};')
s = rep(s, nl, '''                                              "inventory I Tab        # thy pack (the mouse is free while it is open)"};''',
        '''                                              "inventory I Tab        # thy pack (the mouse is free while it is open)",
                                              "look L                 # look at what the crosshair is on (left click too)"};''')
s = rep(s, nl, '''    case WM_RBUTTONDOWN:
        if (g_inventory) g_click_r = true;
        return 0;''', '''    case WM_RBUTTONDOWN:
        if (g_inventory) g_click_r = true;
        else if (g_captured) g_use_click = true; /* right click = use, like E */
        return 0;''')
s = rep(s, nl, 'static bool g_focus, g_captured, g_inventory, g_click_l, g_click_r, g_attack;',
        'static bool g_focus, g_captured, g_inventory, g_click_l, g_click_r, g_attack, g_use_click;')
s = rep(s, nl, '''            gi.use = pressed(A_USE), gi.inventory = pressed(A_INVENTORY), gi.torch = pressed(A_TORCH);''',
        '''            gi.use = pressed(A_USE) || g_use_click, gi.inventory = pressed(A_INVENTORY), gi.torch = pressed(A_TORCH);
            gi.look = pressed(A_LOOK);
            g_use_click = false;''')
s = rep(s, nl, '    int use_frame = -1, talk_arg = -1;',
        '''    int use_frame = -1, talk_arg = -1, look_frame = -1, click_frame = -1, attack_frame = -1, shot_mouse[2] = {0, 0};
    bool inv_open = false;''')
# headless: --look=N (L on frame N), --inv=1 (the panel open)
s = rep(s, nl, '''                gi.use = use_frame == f; /* --use=N: E pressed on frame N (the checks) */''',
        '''                gi.use = use_frame == f; /* --use=N: E pressed on frame N (the checks) */
                gi.look = look_frame == f, gi.inventory = inv_open && f == 0;
                gi.mouse_x = shot_mouse[0], gi.mouse_y = shot_mouse[1];
                gi.click_left = click_frame == f, gi.attack = attack_frame == f;''')
s = rep(s, nl, '''        else if (starts(a, "--use=")) use_frame = atoi(a + 6);''',
        '''        else if (starts(a, "--use=")) use_frame = atoi(a + 6);
        else if (starts(a, "--look=")) look_frame = atoi(a + 7);
        else if (starts(a, "--inv=")) inv_open = atoi(a + 6) != 0;
        else if (starts(a, "--mouse=")) sscanf(a + 8, "%d,%d", &shot_mouse[0], &shot_mouse[1]);
        else if (starts(a, "--click=")) click_frame = atoi(a + 8);
        else if (starts(a, "--attack=")) attack_frame = atoi(a + 9);''')
save('src/main.c', s)
print('ok')
