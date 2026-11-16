"""One-off: switches (0x170-0x17F, TMFLAT.GR pictures) used in the world fire their use trigger (0x1A2) -> trap;
the trap runner takes the trigger's target tile; the door trap (UW.EXE file 0x843dd) and the text string trap
(0x847bb)."""
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


patch('src/level.h', [
    ('#define TMOBJ_CELL 480', '#define TMFLAT_CELL 464 /* the switches\' pictures (TMFLAT.GR 0-15 = objects 0x170-0x17F) from here */\n#define TMOBJ_CELL 480'),
])
patch('src/level.c', [
    ('    /* the wall decorations\' pictures (TMOBJ.GR 0-31: switches, writings, gravestones) in the free cells from',
     '''    /* the switches' pictures (TMFLAT.GR: 0x170 + k is picture k) in the free cells from TMFLAT_CELL */
    for (int j = 0; j < 16; j++) {
        size_t mark = arena_mark();
        Image img;
        int k = TMFLAT_CELL + j;
        snprintf(path, sizeof path, "%s\\\\sprites\\\\flat_objects\\\\%03d.png", data_dir, j);
        if (png_read(path, &img) && img.w <= 32 && img.h <= 32) {
            int x0 = k % 32 * 32, y0 = 1024 + k / 32 * 32;
            for (int y = 0; y < img.h; y++) memcpy(m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0) * 4, img.rgba + (size_t)y * img.w * 4, (size_t)img.w * 4);
            m->sprite_w[k] = (u8)img.w, m->sprite_h[k] = (u8)img.h;
        }
        arena_reset(mark);
    }
    /* the wall decorations' pictures (TMOBJ.GR 0-31: dials, writings, gravestones) in the free cells from'''),
])
patch('src/world.c', [
    ('    if (o->item == 0x165) return TMOBJ_CELL + 28 + (o->flags & 1);\n    return -1;',
     '    if (o->item == 0x165) return TMOBJ_CELL + 28 + (o->flags & 1);\n'
     '    if (o->item >= 0x170 && o->item <= 0x17F) return TMFLAT_CELL + (o->item & 15); /* switches: TMFLAT.GR */\n'
     '    return -1;'),
])
patch('src/game.c', [
    ('''static void run_trap(int i, int depth) {
    if (i <= 0 || i >= 1024 || depth > 8) return;
    const Object *o = &G.lv->obj[i];
    switch (o->item) {''', '''/* the door on tile (tx, tz), or -1 */
static int door_on(int tx, int tz) {
    for (int k = 0; k < G.ndoors; k++)
        if (G.doors[k].tx == tx && G.doors[k].tz == tz) return k;
    return -1;
}

/* A trap set off, for the target tile (tx, tz) its trigger names (UW.EXE's dispatcher, file 0x83d96, by item &
 * 0x3f through a table of 17). Done: damage, teleport, door, text string; the rest are logged. */
static void run_trap(int i, int tx, int tz, int depth) {
    if (i <= 0 || i >= 1024 || depth > 8) return;
    Object *o = &G.lv->obj[i];
    switch (o->item) {
    case 0x188: { /* a door trap (file 0x843dd): the door on the target tile loses its lock; a copy of the lock
                   * the trap links to (if any) goes on instead; then quality 1 opens, 2 closes, 3 toggles */
        int k = door_on(tx, tz);
        if (k < 0) break;
        Object *door = &G.lv->obj[G.doors[k].obj];
        if (door->link > 0 && door->link < 1024 && G.lv->obj[door->link].item == 0x10F) {
            int old = door->link;
            door->link = G.lv->obj[old].next; /* the lock out of the door's chain */
            memset(&G.lv->obj[old], 0, sizeof G.lv->obj[old]);
        }
        if (!(o->flags & 0x40) && o->link > 0 && o->link < 1024 && G.lv->obj[o->link].item == 0x10F) {
            int n = inv_level_alloc(G.lv);
            if (n > 0) {
                G.lv->obj[n] = G.lv->obj[o->link];
                G.lv->obj[n].next = door->link;
                door->link = (u16)n;
            }
        }
        float target = o->quality == 1 ? 1.0f : o->quality == 2 ? 0.0f : o->quality == 3 ? (G.door_target[k] < 0.5f ? 1.0f : 0.0f) : G.door_target[k];
        G.door_target[k] = target;
        Door *d = &G.doors[k];
        d->item = (d->item & ~8) | (target > 0.5f ? 8 : 0);
        door->item = (u16)d->item;
        break;
    }
    case 0x190: /* a text string trap (file 0x847bb): strings block 9 [(quality << 5) | owner] */
        game_message("%s", str_get(9, (o->quality << 5) | o->owner));
        break'''),
    ('''    default: break;
    }
}

static void enter_tile(int tx, int tz) {
    for (int i = G.lv->tile[tz * 64 + tx].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = G.lv->obj[i].next, guard++)
        if (G.lv->obj[i].item == 0x1A0) run_trap(G.lv->obj[i].link, 0);
}''', '''    default: logf_("trap 0x%03x (object %d) not done yet\\n", o->item, i); break;
    }
}

/* a trigger (0x1A0 move, 0x1A2 use, ...): its quality and owner name the target tile; its link the trap */
static void fire_trigger(int t) {
    if (t <= 0 || t >= 1024) return;
    const Object *tr = &G.lv->obj[t];
    if (tr->item < 0x1A0 || tr->item > 0x1AF) return run_trap(t, tr->quality, tr->owner, 0); /* a trap linked directly */
    run_trap(tr->link, tr->quality, tr->owner, 0);
}

static void enter_tile(int tx, int tz) {
    for (int i = G.lv->tile[tz * 64 + tx].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = G.lv->obj[i].next, guard++)
        if (G.lv->obj[i].item == 0x1A0) run_trap(G.lv->obj[i].link, G.lv->obj[i].quality, G.lv->obj[i].owner, 0);
}

/* a switch, lever, button or pull chain used: it flips (0x170-0x177 <-> 0x178-0x17F) and fires what it links */
static void use_switch(int i) {
    Object *o = &G.lv->obj[i];
    o->item ^= 8;
    fire_trigger(o->link);
}'''),
    ('''            if (obj_info(G.lv->obj[G.hover_obj].item)->pick) {''', '''            int it = G.lv->obj[G.hover_obj].item;
            if (it >= 0x170 && it <= 0x17F) {
                use_switch(G.hover_obj);
                rebuild_sprites();
            } else if (obj_info(it)->pick) {'''),
])
print('ok')
