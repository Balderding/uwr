"""One-off: the create object trap (UW.EXE file 0x8418f) and the delete object trap (0x845dc)."""
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


patch('src/game.c', [
    ('''static void creatures_load(void) {
    g_ncr = 0;
    for (int t = 0; t < 64 * 64; t++)
        for (int i = G.lv->tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = G.lv->obj[i].next, guard++) {
            const Object *o = &G.lv->obj[i];
            if (o->item < 0x40 || o->item >= 0x80 || g_ncr >= 256) continue;
            Creature *c = &g_cr[g_ncr++];
            memset(c, 0, sizeof *c);
            float p[3];
            world_object_pos(G.lv, t % 64, t / 64, o, p);
            c->obj = i, c->kind = o->item - 0x40, c->x = p[0], c->z = p[2], c->y = p[1];
            c->hp = (i < 256 && (o->mobile[0] & 0x7F)) ? (float)(o->mobile[0] & 0x7F) : 8.0f;
            c->hostile = !(i < 256 && o->mobile[18]);
            c->t = (float)rnd(100) / 37.0f;
        }
}''', '''/* object i (a creature, 0x40-0x7F) on tile t joins the living */
static void creature_add(int i, int t) {
    const Object *o = &G.lv->obj[i];
    if (o->item < 0x40 || o->item >= 0x80 || g_ncr >= 256) return;
    Creature *c = &g_cr[g_ncr++];
    memset(c, 0, sizeof *c);
    float p[3];
    world_object_pos(G.lv, t % 64, t / 64, o, p);
    c->obj = i, c->kind = o->item - 0x40, c->x = p[0], c->z = p[2], c->y = p[1];
    c->hp = (i < 256 && (o->mobile[0] & 0x7F)) ? (float)(o->mobile[0] & 0x7F) : 8.0f;
    c->hostile = !(i < 256 && o->mobile[18]);
    c->t = (float)rnd(100) / 37.0f;
}

static void creatures_load(void) {
    g_ncr = 0;
    for (int t = 0; t < 64 * 64; t++)
        for (int i = G.lv->tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = G.lv->obj[i].next, guard++) creature_add(i, t);
}'''),
    ('''    case 0x190: /* a text string trap (file 0x847bb): strings block 9 [(quality << 5) | owner] */''',
     '''    case 0x187: { /* a create object trap (file 0x8418f): a roll of 0-62 below the quality does nothing; else a
                   * copy of the object the trap links to (a creature with its mobile bytes) goes on the target
                   * tile, at the template's place in the tile and its height */
        if (rnd(63) < o->quality || (o->flags & 0x40) || o->link <= 0 || o->link >= 1024) break;
        const Object *tpl = &G.lv->obj[o->link];
        bool creature = tpl->item >= 0x40 && tpl->item < 0x80;
        int n = -1;
        if (creature) { /* a mobile slot (1-255) nothing uses */
            for (int k = 1; k < 256 && n < 0; k++)
                if (inv_level_free(G.lv, k) && G.lv->obj[k].item == 0) n = k;
        } else n = inv_level_alloc(G.lv);
        if (n <= 0 || tx < 0 || tx > 63 || tz < 0 || tz > 63) break;
        G.lv->obj[n] = *tpl;
        G.lv->obj[n].link = 0;
        Tile *t = &G.lv->tile[tz * 64 + tx];
        G.lv->obj[n].next = t->first_object;
        t->first_object = (u16)n;
        if (creature) creature_add(n, tz * 64 + tx);
        rebuild_sprites();
        logf_("create object trap %d: object %d (0x%03x) on %d,%d\\n", i, n, tpl->item, tx, tz);
        break;
    }
    case 0x18b: { /* a delete object trap (file 0x845dc): the object it links to leaves the tile the trap's own
                   * quality and owner name */
        int dx = o->quality, dz = o->owner, v = o->link;
        if (dx > 63 || dz > 63 || v <= 0 || v >= 1024) break;
        Tile *t = &G.lv->tile[dz * 64 + dx];
        u16 *at = &t->first_object;
        for (int guard = 0; *at && guard < 1024; guard++, at = &G.lv->obj[*at].next)
            if (*at == v) {
                *at = G.lv->obj[v].next;
                for (int k = 0; k < g_ncr; k++)
                    if (g_cr[k].obj == v) g_cr[k] = g_cr[--g_ncr];
                memset(&G.lv->obj[v], 0, sizeof G.lv->obj[v]);
                rebuild_sprites();
                logf_("delete object trap %d: object %d off %d,%d\\n", i, v, dx, dz);
                break;
            }
        break;
    }
    case 0x190: /* a text string trap (file 0x847bb): strings block 9 [(quality << 5) | owner] */'''),
])
print('ok')
