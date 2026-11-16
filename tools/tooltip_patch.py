"""One-off: item tooltips with comparisons (WoW-like), right click never drops (unequips, opens, uses, moves into a
bag), shift + right click drops; objects.txt v4 with the weapon tables of OBJECTS.DAT."""
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


# --- objects.txt v4: weapons ---------------------------------------------------------------------------------
patch('src/extract.c', [
    ('"# uo-new objects v3: what the original knows', '"# uo-new objects v4: what the original knows'),
    ('!file_starts_with(path, "# uo-new objects v3")', '!file_starts_with(path, "# uo-new objects v4")'),
    ('                "#   food N  nutrition (what eating it adds to hunger, 0-255; below 0: a drink)\\r\\n");',
     '                "#   food N  nutrition (what eating it adds to hunger, 0-255; below 0: a drink)\\r\\n"\n'
     '                "#   weapon N  slash bash stab (damage of each attack) skill (3 sword, 4 axe, 5 mace, 6 unarmed)\\r\\n"\n'
     '                "#          durability (255 never breaks)   # raw: the 8 bytes (3-5 not decoded yet)\\r\\n"\n'
     '                "#   launcher N  ammo (the object it shoots: 16 sling stones, 17 crossbow bolts, 18 arrows)\\r\\n");'),
    ('            for (int k = 0; k < 16; k++) /* OBJECTS.DAT 0xd82: a signed byte for 0xB0-0xBF */',
     '''            for (int k = 0; k < 16; k++) { /* OBJECTS.DAT 2: 16 hand weapons x 8 bytes */
                const u8 *e = od.data + 2 + k * 8;
                n += (size_t)snprintf(text + n, cap - n, "weapon %3d  slash %d bash %d stab %d skill %d durability %d   # raw %02x %02x %02x %02x %02x %02x %02x %02x\\r\\n",
                                      k, e[0], e[1], e[2], e[6], e[7], e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
            }
            for (int k = 8; k < 16; k++) { /* OBJECTS.DAT 0x82: 0x18-0x1F launchers, byte 2 = the ammunition */
                const u8 *e = od.data + 0x82 + k * 3;
                if (e[2] <= 2) n += (size_t)snprintf(text + n, cap - n, "launcher %3d  ammo %d\\r\\n", 0x10 + k, 0x10 + e[2]);
            }
            for (int k = 0; k < 16; k++) /* OBJECTS.DAT 0xd82: a signed byte for 0xB0-0xBF */'''),
])
patch('src/objdata.h', [
    ('    i16 nutrition;   /* food: what eating it adds to hunger (OBJECTS.DAT 0xd82, signed; 0 when not food) */',
     '    i16 nutrition;   /* food: what eating it adds to hunger (OBJECTS.DAT 0xd82, signed; 0 when not food) */\n'
     '    bool weapon;     /* a hand weapon (OBJECTS.DAT 2): its damage by attack, skill, durability */\n'
     '    u8 slash, bash, stab, skill, wdurability;\n'
     '    i16 ammo;        /* a launcher: the object it shoots (0 none) */'),
])
patch('src/objdata.c', [
    ('                else if (!strcmp(key, "nutrition")) o->nutrition = (i16)v;',
     '                else if (!strcmp(key, "nutrition")) o->nutrition = (i16)v;\n'
     '                else if (!strcmp(key, "slash")) o->slash = (u8)v, o->weapon = true;\n'
     '                else if (!strcmp(key, "bash")) o->bash = (u8)v;\n'
     '                else if (!strcmp(key, "stab")) o->stab = (u8)v;\n'
     '                else if (!strcmp(key, "skill")) o->skill = (u8)v;\n'
     '                else if (!strcmp(key, "ammo")) o->ammo = (i16)v;\n'
     '                else if (!strcmp(key, "durability") && !strcmp(kind, "weapon")) o->wdurability = (u8)v;'),
])

# --- input: shift ---------------------------------------------------------------------------------------------
patch('src/inventory.h', [
    ('    bool click_left, click_right, look;', '    bool click_left, click_right, look, shift;'),
])
patch('src/game.h', [
    ('    bool look;             /* look at what the crosshair (or, in the pack, the mouse) is on */',
     '    bool look;             /* look at what the crosshair (or, in the pack, the mouse) is on */\n'
     '    bool shift;            /* a shift key is down (shift + right click drops; shift shows both tooltips) */'),
])
patch('src/game.c', [
    ('        InvInput ii = {in->click_left, in->click_right, in->look, in->mouse_x, in->mouse_y};',
     '        InvInput ii = {in->click_left, in->click_right, in->look, in->shift, in->mouse_x, in->mouse_y};'),
])
patch('src/main.c', [
    ('            gi.look = pressed(A_LOOK);', '            gi.look = pressed(A_LOOK);\n            gi.shift = g_focus && (GetKeyState(VK_SHIFT) & 0x8000);'),
    ('                gi.look = look_frame == f, gi.inventory = inv_frame == f;',
     '                gi.look = look_frame == f, gi.inventory = inv_frame == f, gi.shift = shot_shift;'),
    ('    int inv_frame = -1;', '    int inv_frame = -1;\n    bool shot_shift = false;'),
    ('        else if (starts(a, "--inv=")) inv_frame = atoi(a + 6); /* the panel opened on frame N */',
     '        else if (starts(a, "--inv=")) inv_frame = atoi(a + 6); /* the panel opened on frame N */\n'
     '        else if (starts(a, "--shift")) shot_shift = true;'),
])

# --- inventory.c: right click, tooltips -------------------------------------------------------------------------
patch('src/inventory.c', [
    ('''    if (in->click_right && under) {
        use_thing(under);
        return true;
    }''', '''    if (in->click_right && under) {
        if (in->shift) { /* shift + right click: put it down in the world */
            take_out(under);
            I.held = under;
            drop_held(lv, eye, fwd);
            return true;
        }
        right_click(under);
        return true;
    }'''),
    ('bool inv_update(Level *lv, const InvInput *in, const float eye[3], const float fwd[3]) {',
     '''/* takes p out of wherever it is (a slot, a container, the open container) */
static void take_out(int p) {
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] == p) I.slot[s] = 0;
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] && holds(&I.pool[I.slot[s]])) unlink_from(I.slot[s], p);
    for (int k = 0; k < I.nopen; k++)
        if (I.open[k] == p) I.nopen = k;
}

/* Right click never drops. On something worn or held (the body): off to the pack (a free hole, else a bag it
 * fits in). On a bag: it opens. On something usable (a light, food, drink, a book, a scroll): used. On anything
 * else in the pack: into the first bag in the pack that takes it (else nothing). */
static void right_click(int p) {
    int s = slot_of(p);
    const Object *o = &I.pool[p];
    char name[96];
    obj_name(o, name, sizeof name);
    if (s >= 0 && s < INV_BODY) {
        if (is_light(o->item) && s >= 5 && s <= 8) return use_thing(p); /* a light in a hand: light / put out */
        take_out(p);
        if (place_new(p)) return game_message("You put %s in thy pack.", name);
        I.slot[s] = p;
        return game_message("There is no place to put that.");
    }
    int item = o->item & 511;
    if (obj_info(item)->container || is_light(item) || (item >> 4) == 0xb || (item >= 0x130 && item <= 0x13f) || item == 0x92 ||
        item == 0xce || item == 0xcf || item == 0xd9 || item == 0x11b || item == 0x125)
        return use_thing(p);
    /* into a bag: the first one in the pack (not itself, not the one it is in) that takes it */
    for (int k = INV_BODY; k < INV_SLOTS; k++) {
        int c = I.slot[k];
        if (!c || c == p || !obj_info(I.pool[c].item)->container) continue;
        if (I.nopen && I.open[I.nopen - 1] == c) continue;
        char msg[128];
        if (!obj_fits(I.pool[c].item, o, contents_mass(c), msg, sizeof msg)) continue;
        take_out(p);
        put_in(c, p, false);
        char bag[64];
        obj_noun(&I.pool[c], bag, sizeof bag);
        return game_message("You put %s in the %s.", name, bag);
    }
}

bool inv_update(Level *lv, const InvInput *in, const float eye[3], const float fwd[3]) {'''),
    # the hover text below the panel becomes a tooltip
    ('''    if (under && !I.held) {
        char name[96], line[160];
        obj_name(&I.pool[under], name, sizeof name);
        const Object *o = &I.pool[under];
        if (is_light(o->item)) snprintf(line, sizeof line, "%s  (right: %s)", name, is_lit(o->item) ? "put out" : "light");
        else if (obj_info(o->item)->container) snprintf(line, sizeof line, "%s  (right: open)", name);
        else snprintf(line, sizeof line, "%s", name);
        int tw = ui_text_width(FONT_NORMAL, line);
        ui_text(FONT_NORMAL, line, MAX(4, px + PW - tw), py + PH + 4, 255, 230, 160);
        ui_text(FONT_SMALL, "LEFT: TAKE  RIGHT: USE  L: LOOK", MAX(4, px + PW - ui_text_width(FONT_SMALL, "LEFT: TAKE  RIGHT: USE  L: LOOK")),
                py + PH + 16, 200, 200, 200);
    }''', '''    if (under && !I.held) tooltips(under, I.shift);
    ui_text(FONT_SMALL, "LEFT: TAKE  RIGHT: USE  SHIFT+RIGHT: DROP  L: LOOK", MAX(4, px + PW - ui_text_width(FONT_SMALL, "LEFT: TAKE  RIGHT: USE  SHIFT+RIGHT: DROP  L: LOOK")),
            py + PH + 4, 200, 200, 200);'''),
    ('void inv_draw(void) {', '''/* --- tooltips ------------------------------------------------------------------------------------------------
 * As in World of Warcraft: the name, its condition, where it is worn, what it does (damage, protection), its
 * durability, weight and value; for something that can be worn, what changes if it is put on in place of what is
 * there ("If you equip this:", gains green, losses red). With shift, the worn one's tooltip beside it, each
 * showing its differences from the other. Not while something is held. */
typedef struct {
    char text[64];
    u8 r, g, b;
    bool small;
} TipLine;
typedef struct {
    TipLine l[24];
    int n;
} Tip;
static void tip_add(Tip *t, bool small, u8 r, u8 g, u8 b, const char *fmt, ...) __attribute__((format(printf, 6, 7)));
static void tip_add(Tip *t, bool small, u8 r, u8 g, u8 b, const char *fmt, ...) {
    if (t->n >= 24) return;
    TipLine *l = &t->l[t->n++];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(l->text, sizeof l->text, fmt, ap);
    va_end(ap);
    l->r = r, l->g = g, l->b = b, l->small = small;
}

/* the numbers compared: slash, bash, stab, protection, durability */
typedef struct {
    int v[5];
} Stats;
static const char *const STAT_NAME[5] = {"Slash damage", "Bash damage", "Thrust damage", "Protection", "Durability"};
static Stats stats_of(int p) {
    Stats s = {{0}};
    const ObjInfo *in = obj_info(I.pool[p].item);
    if (in->weapon) s.v[0] = in->slash, s.v[1] = in->bash, s.v[2] = in->stab, s.v[4] = in->wdurability == 255 ? 0 : in->wdurability;
    if (in->armour >= 0 || (I.pool[p].item >= 0x37 && I.pool[p].item <= 0x3f)) s.v[3] = in->protection, s.v[4] = in->durability == 255 ? 0 : in->durability;
    return s;
}

static void tip_build(Tip *t, int p, int other, const char *header) {
    const Object *o = &I.pool[p];
    const ObjInfo *in = obj_info(o->item);
    int item = o->item & 511;
    char name[96];
    obj_name(o, name, sizeof name);
    t->n = 0;
    if (header) tip_add(t, true, 150, 150, 150, "%s", header);
    if (name[0] >= 'a' && name[0] <= 'z') name[0] = (char)(name[0] - 32);
    bool magic = (o->flags & 0x08) != 0; /* enchanted (item word bit 12) */
    tip_add(t, false, magic ? 190 : 255, magic ? 130 : 255, magic ? 255 : 255, "%s", name);
    /* its condition: the look's quality word */
    int bucket = o->quality ? (in->qclass == 3 ? 5 : (o->quality >> 4) + 1) : 0;
    const char *word = str_get(5, in->qtype * 6 + bucket);
    if (word[0] && in->qtype != 15) tip_add(t, false, 255, 210, 90, "%c%s", word[0] >= 'a' ? word[0] - 32 : word[0], word + 1);
    /* where it goes and what it does */
    static const char *const SLOT[11] = {"Head", "Chest", "Hands", "Legs", "Feet", "Shoulder", "Shoulder", "Weapon hand", "Off hand", "Finger", "Finger"};
    int s = body_slot_for(p);
    static const int SKILL[7] = {0, 0, 0, 34, 35, 36, 33}; /* block 2: Sword, Axe, Mace, Unarmed */
    if (in->weapon) {
        tip_add(t, false, 255, 255, 255, "%s   %s", SLOT[7], in->skill < 7 && SKILL[in->skill] ? str_get(2, SKILL[in->skill]) : "");
        tip_add(t, false, 255, 255, 255, "Slash %d   Bash %d   Thrust %d", in->slash, in->bash, in->stab);
    } else if (in->ammo) {
        char ammo[64];
        Object a = {0};
        a.item = (u16)in->ammo;
        obj_noun(&a, ammo, sizeof ammo);
        tip_add(t, false, 255, 255, 255, "Ranged weapon   uses %s", ammo);
    } else if (s >= 0) tip_add(t, false, 255, 255, 255, "%s", SLOT[s]);
    if (in->armour >= 0 || (item >= 0x37 && item <= 0x3f)) tip_add(t, false, 255, 255, 255, "%d Protection", in->protection);
    if (in->weapon || in->armour >= 0 || (item >= 0x37 && item <= 0x3f)) {
        int dur = in->weapon ? in->wdurability : in->durability;
        if (dur == 255) tip_add(t, false, 255, 255, 255, "Never wears out");
        else tip_add(t, false, 255, 255, 255, "Durability %d / %d", o->quality * dur / 63, dur);
    }
    if (in->container) {
        int count = 0;
        for (int c = o->link, guard = 0; c && guard < POOL; c = I.pool[c].next, guard++) count++;
        if (in->capacity) tip_add(t, false, 255, 255, 255, "Holds %d.%d stones (%d.%d in it)", in->capacity / 10, in->capacity % 10, contents_mass(p) / 10, contents_mass(p) % 10);
        tip_add(t, false, 255, 255, 255, "%d thing%s inside", count, count == 1 ? "" : "s");
    }
    if (is_light(item)) {
        int d = I.light_dur[(item & 3) + 4], b = I.light_bright[(item & 3) + 4];
        tip_add(t, false, 255, 255, 255, "Brightness %d of 4", b);
        if (!d) tip_add(t, false, 0, 230, 0, "Never burns out");
        else { /* a point every d ticks of 21 s, within the 24-tick cycle */
            int per = 24 / d, secs = o->quality * 24 * 21 / MAX(1, per);
            tip_add(t, false, 0, 230, 0, "Burns about %d min more%s", secs / 60, is_lit(item) ? " (lit)" : "");
        }
    }
    if ((item >> 4) == 0xb && in->nutrition > 0) tip_add(t, false, 0, 230, 0, "Use: eat (nourishment %d)", in->nutrition);
    if (item >= 0x130 && item <= 0x13f) tip_add(t, false, 0, 230, 0, "Use: read");
    tip_add(t, true, 200, 200, 200, "Weight %d.%d stones", obj_mass(o) / 10, obj_mass(o) % 10);
    if (in->value) tip_add(t, true, 200, 200, 200, "Worth %d gold", in->value * obj_quantity(o));
    /* the differences from the other one */
    if (other) {
        Stats a = stats_of(p), b = stats_of(other);
        bool any = false;
        for (int k = 0; k < 5; k++) {
            int d = a.v[k] - b.v[k];
            if (!d) continue;
            if (!any) tip_add(t, false, 255, 255, 255, "%s", header ? "Compared with the other:" : "If you equip this:"), any = true;
            tip_add(t, false, d > 0 ? 0 : 255, d > 0 ? 230 : 60, d > 0 ? 0 : 60, "%+d %s", d, STAT_NAME[k]);
        }
    }
}

static int tip_size(const Tip *t, int *h) {
    int w = 0;
    *h = 6;
    for (int k = 0; k < t->n; k++) {
        int f = t->l[k].small ? FONT_SMALL : FONT_NORMAL;
        w = MAX(w, ui_text_width(f, t->l[k].text));
        *h += ui_font_height(f) + 2;
    }
    return w + 10;
}
static void tip_draw(const Tip *t, int x, int y) {
    int h, w = tip_size(t, &h);
    ui_fill(x, y, w, h, 8, 10, 24, 235);
    ui_fill(x, y, w, 1, 150, 150, 160, 255), ui_fill(x, y + h - 1, w, 1, 150, 150, 160, 255);
    ui_fill(x, y, 1, h, 150, 150, 160, 255), ui_fill(x + w - 1, y, 1, h, 150, 150, 160, 255);
    int cy = y + 4;
    for (int k = 0; k < t->n; k++) {
        int f = t->l[k].small ? FONT_SMALL : FONT_NORMAL;
        ui_text(f, t->l[k].text, x + 5, cy, t->l[k].r, t->l[k].g, t->l[k].b);
        cy += ui_font_height(f) + 2;
    }
}

/* the tooltip of thing p by the mouse (to its left: the panel is at the right edge); compared with what is worn
 * where it would go (when that is something else); with shift, that one's tooltip too */
static void tooltips(int p, bool shift) {
    static Tip a, b;
    int s = body_slot_for(p), worn = s >= 0 && slot_of(p) != s ? I.slot[s] : 0;
    if (worn == p) worn = 0;
    tip_build(&a, p, worn, NULL);
    int ah, aw = tip_size(&a, &ah);
    int x = MAX(2, I.mouse_x - aw - 8), y = MAX(2, I.mouse_y - ah / 2);
    if (shift && worn) {
        tip_build(&b, worn, p, "Currently equipped");
        tip_build(&a, p, worn, "Looking at");
        aw = tip_size(&a, &ah);
        int bh, bw = tip_size(&b, &bh);
        x = MAX(2, I.mouse_x - aw - 8);
        tip_draw(&b, MAX(2, x - bw - 4), MAX(2, I.mouse_y - bh / 2));
    }
    tip_draw(&a, x, y);
}

void inv_draw(void) {'''),
    ('#include <math.h>\n#include <stdio.h>', '#include <math.h>\n#include <stdarg.h>\n#include <stdio.h>'),
    ('    int hover;          /* the hole under the mouse: 0-18 a slot, 19-26 a hole of the open container, -1 */',
     '    int hover;          /* the hole under the mouse: 0-18 a slot, 19-26 a hole of the open container, -1 */\n'
     '    bool shift;'),
    ('    I.mouse_x = in->mouse_x, I.mouse_y = in->mouse_y;\n    int px, py;\n    panel_origin(&px, &py);\n    bool on_panel',
     '    I.mouse_x = in->mouse_x, I.mouse_y = in->mouse_y, I.shift = in->shift;\n    int px, py;\n    panel_origin(&px, &py);\n    bool on_panel'),
])
print('ok')
