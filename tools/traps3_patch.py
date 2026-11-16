"""One-off: trap chains (UW.EXE 0x84803), set / check variable (0x83ea4, 0x83fc5), inventory (0x8463b), damage
heals when the owner is set (0x840bd), change terrain (0x83e17 -> 0x73b68) with the level rebuilt while playing."""
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


NEW = r'''
/* the game's variables (UW.EXE: the player record's quest bits at 0x65 (32) and variables from 0x70 (6 bits each)) */
static u32 g_quest;
static u8 g_var[128];
static bool g_rebuild; /* the level's tiles changed: its geometry is to be made again */
bool game_rebuild_wanted(void) {
    bool r = g_rebuild;
    g_rebuild = false;
    return r;
}
static void run_trap(int i, int tx, int tz, int depth);
static void fire_trigger(int t);
/* what a trap's chain goes on to (0x84803): the object its link names when that is a trap (it runs for the same
 * tile) or a trigger (it fires) */
static void chain_on(int link, int tx, int tz, int depth) {
    if (link <= 0 || link >= 1024 || depth > 16) return;
    int item = G.lv->obj[link].item;
    if (item >= 0x180 && item < 0x1A0) run_trap(link, tx, tz, depth + 1);
    else if (item >= 0x1A0 && item < 0x1C0) fire_trigger(link);
}
/* the value a variable trap carries: ((quality << 5 | owner) << 3) | y */
static int trap_value(const Object *o) { return ((o->quality << 5 | o->owner) << 3) | (o->y & 7); }
'''

patch('src/game.c', [
    ('''/* the door on tile (tx, tz), or -1 */''', NEW + '''
/* the door on tile (tx, tz), or -1 */'''),
    ('''static void run_trap(int i, int tx, int tz, int depth) {
    if (i <= 0 || i >= 1024 || depth > 8) return;
    Object *o = &G.lv->obj[i];
    switch (o->item) {''', '''static void run_trap(int i, int tx, int tz, int depth) {
    if (i <= 0 || i >= 1024 || depth > 16) return;
    Object *o = &G.lv->obj[i];
    bool chain = true; /* the door, create and delete traps use their link themselves */
    switch (o->item) {
    case 0x18d: { /* set variable (0x83ea4): variable z; 0 = the quest bits: heading 1 clears, 5 toggles, else sets
                   * bit value; else variable op by heading: 0 add 1 sub 2 set 3 and 4 or 5 xor 6 shift left; 6 bits */
        int v = trap_value(o), var = o->z & 0x7f, op = o->heading & 7;
        if (!var) {
            u32 bit = v < 32 ? 1u << v : 0;
            if (op == 1) g_quest &= ~bit;
            else if (op == 5) g_quest ^= bit;
            else g_quest |= bit;
        } else {
            u8 *x = &g_var[var & 127];
            switch (op) {
            case 0: *x = (u8)(*x + v); break;
            case 1: *x = (u8)(*x - v); break;
            case 2: *x = (u8)v; break;
            case 3: *x &= (u8)v; break;
            case 4: *x |= (u8)v; break;
            case 5: *x ^= (u8)v; break;
            case 6: *x = (u8)(*x << (v & 7)); break;
            }
            *x &= 0x3f;
        }
        break;
    }
    case 0x18e: { /* check variable (0x83fc5): variables z .. z + heading, summed (x set) or packed 3 bits each;
                   * equal to the value: the chain goes on to the link; else to what follows the link */
        int first = o->z & 0x7f, last = first + (o->heading & 7), acc = 0;
        for (int k = first; k <= last && k < 128; k++) acc = o->x ? acc + g_var[k] : (acc << 3) | (g_var[k] & 7);
        if (acc != trap_value(o)) {
            chain = false;
            if (o->link > 0 && o->link < 1024) chain_on(G.lv->obj[o->link].next, tx, tz, depth);
        }
        break;
    }
    case 0x18c: { /* inventory (0x8463b): the chain goes on only when the player carries object (quality << 5 | owner),
                   * at least z of them when z is set */
        int item = o->quality << 5 | o->owner;
        if (inv_count(item) <= 0 || ((o->z & 0x7f) && inv_count(item) < (o->z & 0x7f))) return;
        break;
    }
    case 0x185: { /* change terrain (0x83e17, 0x73b68): the tiles from the target, x + 1 wide and y + 1 deep: floor
                   * height z / 8 (if <= 13; objects lifted with it), floor texture quality / 2 (if < 11), wall texture
                   * owner (if < 48), type heading x 2 + quality & 1 (if < 10; 15 -> 10) */
        int h = (o->z & 0x7f) >> 3, ftex = o->quality >> 1, wtex = o->owner, type = (o->heading & 7) * 2 + (o->quality & 1);
        if (type == 15) type = 10;
        for (int zz = tz; zz <= tz + (o->y & 7) && zz < 64; zz++)
            for (int xx = tx; xx <= tx + (o->x & 7) && xx < 64; xx++) {
                Tile *t = &G.lv->tile[zz * 64 + xx];
                if (h <= 13) {
                    for (int j = t->first_object, guard = 0; j > 0 && j < 1024 && guard < 1024; j = G.lv->obj[j].next, guard++)
                        if (G.lv->obj[j].z < h * 8) G.lv->obj[j].z = (u8)(h * 8);
                    t->height = (u8)h;
                }
                if (ftex < 11) t->floor = (u8)ftex;
                if (wtex < 48) t->wall = (u8)wtex;
                if (type < 10) t->type = (u8)type;
            }
        g_rebuild = true;
        logf_("change terrain trap %d: tiles from %d,%d (%dx%d)\\n", i, tx, tz, (o->x & 7) + 1, (o->y & 7) + 1);
        break;
    }'''),
    # the door, create and delete traps do not chain
    ('''        logf_("door trap %d: the door at %d,%d %s\\n", i, tx, tz, target > 0.5f ? "opens" : "closes");
        break;''', '''        logf_("door trap %d: the door at %d,%d %s\\n", i, tx, tz, target > 0.5f ? "opens" : "closes");
        chain = false;
        break;'''),
    ('''        logf_("create object trap %d: object %d (0x%03x) on %d,%d\\n", i, n, tpl->item, tx, tz);
        break;''', '''        logf_("create object trap %d: object %d (0x%03x) on %d,%d\\n", i, n, tpl->item, tx, tz);
        chain = false;
        break;'''),
    ('''        if (rnd(63) < o->quality || (o->flags & 0x40) || o->link <= 0 || o->link >= 1024) break;''',
     '''        chain = false;
        if (rnd(63) < o->quality || (o->flags & 0x40) || o->link <= 0 || o->link >= 1024) break;'''),
    ('''        int dx = o->quality, dz = o->owner, v = o->link;''', '''        chain = false;
        int dx = o->quality, dz = o->owner, v = o->link;'''),
    ('''    case 0x180: /* a damage trap */
        G.hp -= o->quality, game_message("Ouch!");
        if (G.hp < 1) G.hp = 1;
        break;''', '''    case 0x180: /* a damage trap (0x840bd): quality hit points; with an owner set it heals them instead */
        if (o->owner) G.hp = MIN(G.hp_max, G.hp + o->quality);
        else {
            G.hp -= o->quality, game_message("Ouch!");
            if (G.hp < 1) G.hp = 1;
        }
        break;'''),
    ('''    default: logf_("trap 0x%03x (object %d) not done yet\\n", o->item, i); break;
    }
}''', '''    default: logf_("trap 0x%03x (object %d) not done yet\\n", o->item, i); break;
    }
    if (chain) chain_on(o->link, tx, tz, depth);
}'''),
    ('''/* a trigger (0x1A0 move, 0x1A2 use, ...): its quality and owner name the target tile; its link the trap */
static void fire_trigger(int t) {''', '''/* a trigger (0x1A0 move, 0x1A2 use, ...): its quality and owner name the target tile; its link the trap */
static void fire_trigger(int t);
static void fire_trigger(int t) {'''),
])
patch('src/game.h', [
    ('bool game_escape(void);', 'bool game_escape(void);\n/* a trap changed the level\'s tiles: true once, the geometry is to be made again (game_remesh after) */\nbool game_rebuild_wanted(void);\n/* the level\'s geometry made again: the new mesh and lights (doors, creatures, the pack stay) */\nvoid game_remesh(Mesh *m, const Light *lights, int nlights);'),
])
patch('src/game.c', [
    ('void game_level(Level *lv, Mesh *m, const Light *lights, int nlights) {', '''void game_remesh(Mesh *m, const Light *lights, int nlights) {
    G.mesh = m, G.lights = lights, G.nlights = nlights;
    rebuild_sprites();
    upload_sprites();
}

void game_level(Level *lv, Mesh *m, const Light *lights, int nlights) {'''),
])
patch('src/inventory.h', [
    ('int inv_weapon(void);', 'int inv_weapon(void);\n/* how many of object kind item are carried (stacks counted), anywhere */\nint inv_count(int item);'),
])
patch('src/inventory.c', [
    ('int inv_weapon(void) {', '''static int count_in(int p, int item, int depth) {
    int n = 0;
    for (int guard = 0; p && guard < POOL && depth < 8; guard++, p = I.pool[p].next) {
        if ((I.pool[p].item & 511) == item) n += obj_quantity(&I.pool[p]);
        if (holds(&I.pool[p])) n += count_in(I.pool[p].link, item, depth + 1);
    }
    return n;
}
int inv_count(int item) {
    int n = 0;
    for (int s = 0; s < INV_SLOTS; s++) {
        int p = I.slot[s];
        if (!p) continue;
        if ((I.pool[p].item & 511) == item) n += obj_quantity(&I.pool[p]);
        if (holds(&I.pool[p])) n += count_in(I.pool[p].link, item, 0);
    }
    return n;
}

int inv_weapon(void) {'''),
])
# main: the level's geometry made again when a trap changed it
patch('src/main.c', [
    ('''static bool load_level(int n, const char *data_dir, Player *p, bool place) {''', '''/* a trap changed the tiles (change terrain): the geometry and lights made again from the level as it is now */
static bool remesh_level(const char *data_dir) {
    arena_reset(g_level_mark);
    static Light lights[MAX_LIGHTS];
    static u32 tiles[64 * 64 * TILE_LIGHTS];
    int nl = lights_load(data_dir, &g_level, lights, MAX_LIGHTS);
    if (nl < 0) nl = 0;
    lights_tiles(&g_level, lights, nl, tiles);
    static Mesh m;
    if (!level_mesh(&g_level, data_dir, NULL, 0, &m) || !render_upload(&m)) return false;
    render_set_lights(lights, nl, tiles);
    game_remesh(&m, lights, nl);
    logf_("level %d made again (a trap changed it)\\n", g_level.number);
    return true;
}

static bool load_level(int n, const char *data_dir, Player *p, bool place) {'''),
    ('''        /* a move trigger (stairs, a teleport) sends the player elsewhere */''', '''        if (game_rebuild_wanted()) remesh_level(data_dir);
        /* a move trigger (stairs, a teleport) sends the player elsewhere */'''),
])
print('ok')
