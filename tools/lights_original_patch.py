"""One-off patch: lights as in the original. No wall torches, no floating light; the player's own lights burn
down by UW.EXE's rule (light table of OBJECTS.DAT, a tick every 21 s)."""
import sys

ROOT = sys.argv[1]


def edit(p, reps):
    p = ROOT + '/' + p
    s = open(p, encoding='utf-8', newline='').read()
    for a, b in reps:
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='').write(s)


# --- the extractor: OBJECTS.DAT's light table -> data\objects\lights.txt -------------------------------------
LIGHTS_EXTRACT = r'''    /* the light sources' table of OBJECTS.DAT (at 0xd62: 16 entries of duration, brightness for objects
     * 0x90-0x9F) -> data\objects\lights.txt */
    snprintf(path, sizeof path, "%s\\objects\\lights.txt", data_dir);
    if (!file_exists(path)) {
        char src[700];
        snprintf(src, sizeof src, "%s\\DATA\\OBJECTS.DAT", game_dir);
        mark = arena_mark();
        Bytes b = file_read(src);
        if (b.size >= 0xd62 + 32) {
            char text[2048];
            size_t n = (size_t)snprintf(text, sizeof text,
                                        "# uo-new light sources (from OBJECTS.DAT at 0xd62), one line per object 144-159:\r\n"
                                        "#   light OBJECT DURATION BRIGHTNESS\r\n"
                                        "# DURATION: a lit light loses one point of quality (its fuel, up to 63) every DURATION\r\n"
                                        "# ticks of 21 seconds (0 = never burns out). BRIGHTNESS 0-4 (0 = no light: the unlit ones).\r\n"
                                        "# Edit freely; delete to extract it again.\r\n");
            for (int k = 0; k < 16; k++)
                n += (size_t)snprintf(text + n, sizeof text - n, "light %d %d %d\r\n", 0x90 + k, b.data[0xd62 + k * 2],
                                      b.data[0xd62 + k * 2 + 1]);
            make_dirs_for(path);
            if (file_write(path, text, n)) total++;
        }
        arena_reset(mark);
    }
    return total;
}

int extract_missing('''

edit('src/extract.c', [
    ("        arena_reset(mark);\n    }\n    return total;\n}\n\nint extract_missing(",
     "        arena_reset(mark);\n    }\n" + LIGHTS_EXTRACT),
])

# --- game.c: the original's burning ---------------------------------------------------------------------------
BURN_OLD_START = "/* the lit light burns its fuel (quality): a torch about 10 minutes, a candle 15, a lantern 30, a taper 8 */"
BURN_OLD_END = "bool game_light(float *intensity, float *range, float color[3]) {"

BURN_NEW = r'''/* The light sources' table (data\objects\lights.txt, from OBJECTS.DAT): for objects 0x90-0x9F, how many ticks
 * a point of fuel lasts (0: never burns out) and the brightness (0-4). These are the original's values, used
 * when the file is missing. */
static u8 g_light_dur[16] = {0, 0, 0, 0, 10, 3, 12, 0}, g_light_bright[16] = {0, 0, 0, 0, 4, 2, 1, 3};
static void load_light_table(void) {
    char path[700];
    snprintf(path, sizeof path, "%s\\objects\\lights.txt", G.data);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        int item, dur, bright;
        if (sscanf(p, "light %d %d %d", &item, &dur, &bright) == 3 && item >= 0x90 && item <= 0x9F)
            g_light_dur[item - 0x90] = (u8)CLAMP(dur, 0, 255), g_light_bright[item - 0x90] = (u8)CLAMP(bright, 0, 15);
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
}

/* The burning, as UW.EXE does it (file 0x2acbd and 0x2af36). The game clock runs at 256 a second, in real
 * time; every time more than 20 whole seconds have gone by, a tick: a byte counter goes up, and each lit light
 * loses a point of quality when the counter is a multiple of its duration (a torch every 3 ticks = 63 s, so a
 * fresh one, quality 40, burns about 42 minutes; a lantern every 10 = 2 h 20 min; a candle every 12 = 2 h 48;
 * the taper never). At the last point it goes out, quality 0 ("burned out"), and stays in the pack: oil
 * refuels a lantern or a torch. */
static void burn_tick(void) {
    G.tick++;
    for (int k = 0; k < G.ninv; k++) {
        Object *o = &G.inv[k];
        if (!is_lit(o->item)) continue;
        int d = g_light_dur[o->item & 15];
        if (!d || G.tick % d) continue;
        if (o->quality > 1) {
            o->quality--;
            continue;
        }
        o->quality = 0, o->item -= LIT;
        if (o->item == ITEM_LANTERN) game_message("Thy lantern has run out of oil.");
        else {
            char name[96];
            item_name(o, name, sizeof name);
            const char *noun = strchr(name, ' ');
            game_message("Thy %s has burned out.", noun ? noun + 1 : name);
        }
    }
}
static void burn(float dt) {
    G.clock += dt;
    int secs = (int)G.clock;
    G.clock -= (float)secs;
    G.tick_secs += secs;
    if (G.tick_secs > 20) G.tick_secs = 0, burn_tick();
}

'''

GAME_LIGHT_OLD = '''        /* lantern, torch, candle, taper; a light low on fuel is a little dimmer */
        static const float I[4] = {1.5f, 1.3f, 0.7f, 0.85f}, R[4] = {7.5f, 6.0f, 3.5f, 4.0f};
        static const float C[4][3] = {{1.0f, 0.84f, 0.58f}, {1.0f, 0.70f, 0.40f}, {1.0f, 0.78f, 0.50f}, {1.0f, 0.80f, 0.55f}};
        int t = it - ITEM_LANTERN - LIT;
        float low = G.inv[k].quality < 8 ? 0.55f + 0.45f * G.inv[k].quality / 8.0f : 1.0f;
        *intensity = I[t] * low, *range = R[t];'''
GAME_LIGHT_NEW = '''        /* lantern, torch, candle, taper: how far and how bright from the table's brightness (torch 2 = intensity
         * 1.3, 6 tiles; each point more is 0.4 brighter and 1.5 tiles further); a light low on fuel is dimmer */
        static const float C[4][3] = {{1.0f, 0.84f, 0.58f}, {1.0f, 0.70f, 0.40f}, {1.0f, 0.78f, 0.50f}, {1.0f, 0.80f, 0.55f}};
        int t = it - ITEM_LANTERN - LIT;
        float b = (float)g_light_bright[it & 15];
        if (b <= 0) continue;
        float low = G.inv[k].quality < 8 ? 0.55f + 0.45f * G.inv[k].quality / 8.0f : 1.0f;
        *intensity = (0.5f + 0.4f * b) * low, *range = 3.0f + 1.5f * b;'''

p = ROOT + '/src/game.c'
s = open(p, encoding='utf-8', newline='').read()
a, b = s.index(BURN_OLD_START), s.index(BURN_OLD_END)
s = s[:a] + BURN_NEW + s[b:]
open(p, 'w', encoding='utf-8', newline='').write(s)

edit('src/game.c', [
    ("    float burn[INV_SLOTS]; /* seconds toward the next point of fuel */\n",
     "    float clock;           /* the part second toward the next whole one (the burning, below) */\n"
     "    int tick_secs;         /* whole seconds since the last tick */\n"
     "    u8 tick;               /* the tick counter (a byte, as in UW.EXE) */\n"),
    ("    G.burn[G.ninv] = 0;\n", ""),
    ("    for (int k = slot; k + 1 < G.ninv; k++) G.inv[k] = G.inv[k + 1], G.burn[k] = G.burn[k + 1];",
     "    for (int k = slot; k + 1 < G.ninv; k++) G.inv[k] = G.inv[k + 1];"),
    ('        if (o->quality == 0) return game_message("That is used up.");',
     '        if (o->quality == 0) return game_message("That light is already used up.");'),
    ("    load_names();\n", "    load_names();\n    load_light_table();\n"),
    ("        t->item = ITEM_TORCH + LIT, t->quality = 63;", "        t->item = ITEM_TORCH + LIT, t->quality = 40; /* a fresh torch, as they lie about */"),
    (GAME_LIGHT_OLD, GAME_LIGHT_NEW),
])

# --- lights.c: no wall torches, no floating light ---------------------------------------------------------------
p = ROOT + '/src/lights.c'
s = open(p, encoding='utf-8', newline='').read()
a = s.index("/* the automatic lights: burning torches on the walls of rooms.")
b = s.index("int lights_load(")
s = s[:a] + s[b:]
a = s.index("static u32 hash(u32 v) {")
b = s.index("int lights_load(")
assert b - a < 200, b - a
s = s[:a] + s[b:]
open(p, 'w', encoding='utf-8', newline='').write(s)

edit('src/lights.c', [
    ('''static const char DEFAULT_LIGHTS[] =
    "# uo-new lights placed by hand (each level also has data\\\\lights\\\\levelN.txt, placed automatically).\\r\\n"
    "#   light LEVEL X Z HEIGHT  R G B  INTENSITY RANGE\\r\\n"''',
     '''static const char DEFAULT_LIGHTS[] =
    "# uo-new lights placed by hand. There are none: as in the original, the only light is the one the player\\r\\n"
    "# carries. Lines added here put lights in the world:\\r\\n"
    "#   light LEVEL X Z HEIGHT  R G B  INTENSITY RANGE\\r\\n"'''),
    ('''    "# Delete this file to get this one back.\\r\\n"
    "#\\r\\n"
    "# floating where the first hallway leaves the entrance room of level 1 (warm, like a lantern)\\r\\n"
    "light 1 32.0 3.0 14.0  1.0 0.78 0.52  1.6 7.0\\r\\n";''',
     '''    "#   walltorch LEVEL X Z HEIGHT  R G B  INTENSITY RANGE  FX FZ   (a burning torch on a wall, facing FX FZ)\\r\\n"
    "# Delete this file to get this one back.\\r\\n";'''),
    ('''    int n = 0;
    read_file(path, lv->number, out, &n, cap);
    snprintf(path, sizeof path, "%s\\\\lights\\\\level%d.txt", data_dir, lv->number);
    size_t mark = arena_mark();
    if (!auto_current(path) && !write_auto(path, lv)) return arena_reset(mark), -1;
    arena_reset(mark);
    read_file(path, lv->number, out, &n, cap);
    return n;''',
     '''    int n = 0;
    read_file(path, lv->number, out, &n, cap);
    return n;'''),
])

# --- the user's own lights.txt: the floating light becomes a comment (kept, not deleted) -----------------------
p = ROOT + '/data/lights.txt'
s = open(p, encoding='utf-8', newline='').read()
old = 'light 1 32.0 3.0 14.0  1.0 0.78 0.52  1.6 7.0'
if s.count('\n' + old) == 1:
    s = s.replace('\n' + old, '\n# (off since 2026-11-07: the original has no lights but the player\'s own; remove the "# " to bring it back)\r\n# ' + old)
    open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
