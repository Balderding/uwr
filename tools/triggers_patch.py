"""One-off: triggers on objects (UW.EXE 0x83b3b): look (event 5, a search check against the trigger's z), pick up (2),
use (4), open (7); the event table of OBJECTS.DAT 0xd92; special tmap objects (0x16e) on walls looked at."""
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


TRIG = r'''
/* Triggers on events (UW.EXE 0x83b3b). Each trigger kind answers one event (OBJECTS.DAT 0xd92: 0x1A0 move 0,
 * 0x1A1 pick up 2, 0x1A2 use 4, 0x1A3 look 5, 0x1A4 step on 1, 0x1A5 open 7, 0x1A6 unlock 6). The player sets it
 * off only when its word-0 bit 11 (our flags bit 2) is set; a look trigger with a z needs a search check against
 * z. It runs the trap it links to, for the tile its quality and owner name. */
enum { EV_MOVE = 0, EV_STEP = 1, EV_PICKUP = 2, EV_USE = 4, EV_LOOK = 5, EV_UNLOCK = 6, EV_OPEN = 7 };
static const u8 TRIGGER_EVENT[16] = {0, 2, 4, 5, 1, 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int skill_check(int skill, int difficulty);
static int search_skill(void);
static bool trigger_event(int t, int event) {
    if (t <= 0 || t >= 1024) return false;
    const Object *tr = &G.lv->obj[t];
    if (tr->item < 0x1A0 || tr->item > 0x1AF || TRIGGER_EVENT[tr->item & 15] != event || !(tr->flags & 0x04)) return false;
    if (event == EV_LOOK && (tr->z & 0x7f) && skill_check(search_skill(), tr->z & 0x7f) <= 0) return false;
    logf_("trigger %d (0x%03x) fires: trap %d for %d,%d\n", t, tr->item, tr->link, tr->quality, tr->owner);
    run_trap(tr->link, tr->quality, tr->owner, 0);
    return true;
}
'''

patch('src/game.c', [
    ('''static void enter_tile(int tx, int tz) {''', TRIG + '''
static void enter_tile(int tx, int tz) {'''),
    # look: an object's look trigger; a wall: the special tmap objects on that wall
    ('''    if (G.hover_obj >= 0) {
        obj_describe(&G.lv->obj[G.hover_obj], line, sizeof line);
        return game_message("%s", line);
    }''', '''    if (G.hover_obj >= 0) {
        obj_describe(&G.lv->obj[G.hover_obj], line, sizeof line);
        game_message("%s", line);
        trigger_event(G.lv->obj[G.hover_obj].link, EV_LOOK);
        return;
    }'''),
    ('''        if (level_solid_at(G.lv, x, z)) {
            const Tile *o = last >= 0 ? &G.lv->tile[last] : &G.lv->tile[ti];
            wall_describe(G.lv->walls[o->wall % 48], line, sizeof line);
            return game_message("%s", line);
        }''', '''        if (level_solid_at(G.lv, x, z)) {
            const Tile *o = last >= 0 ? &G.lv->tile[last] : &G.lv->tile[ti];
            wall_describe(G.lv->walls[o->wall % 48], line, sizeof line);
            game_message("%s", line);
            /* special tmap objects (0x16E) on this wall of the open tile: their look triggers (hidden things) */
            if (last >= 0) {
                int lx = last % 64, lz = last / 64;
                for (int j = G.lv->tile[last].first_object, guard = 0; j > 0 && j < 1024 && guard < 1024; j = G.lv->obj[j].next, guard++) {
                    const Object *d = &G.lv->obj[j];
                    if (d->item != 0x16E && d->item != 0x16F) continue;
                    bool on = (d->x == 0 && x < lx) || (d->x == 7 && x >= lx + 1) || (d->y == 0 && z < lz) || (d->y == 7 && z >= lz + 1);
                    if (on) trigger_event(d->link, EV_LOOK);
                }
            }
            return;
        }'''),
    # use on what cannot be taken: its use / open trigger
    ('''        } else if (obj_info(it)->pick) {
            if (inv_take(G.lv, G.hover_tx, G.hover_tz, G.hover_obj)) rebuild_sprites();
        } else look_world(eye, dir);''', '''        } else if (obj_info(it)->pick) {
            int link = G.lv->obj[G.hover_obj].link; /* a pick-up trigger goes off as it is taken */
            if (inv_take(G.lv, G.hover_tx, G.hover_tz, G.hover_obj)) {
                trigger_event(link, EV_PICKUP);
                rebuild_sprites();
            }
        } else {
            int link = G.lv->obj[G.hover_obj].link;
            if (!trigger_event(link, EV_USE) && !trigger_event(link, EV_OPEN)) look_world(eye, dir);
        }'''),
    ('''static int skill_check(int skill, int difficulty) {''', '''static int search_skill(void);
static int skill_check(int skill, int difficulty) {'''),
    ('''static int character_vitality(void) { return P.vitality; }''', '''static int character_vitality(void) { return P.vitality; }
static int search_skill(void) { return P.skill[SK_SEARCH]; }'''),
])
print('ok')
