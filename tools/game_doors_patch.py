"""One-off patch: doors, move triggers (stairs, traps) and collision with doors in game.c / game.h / main.c."""
import sys

ROOT = sys.argv[1]


def edit(p, reps):
    p = ROOT + '/' + p
    s = open(p, encoding='utf-8').read()
    for a, b in reps:
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)


edit('src/game.h', [
    ("void game_message(const char *fmt, ...);",
     "void game_message(const char *fmt, ...);\n"
     "/* is the circle (x, z, radius r) inside a door's frame, or a door that is shut */\n"
     "bool game_blocked(float x, float z, float r);\n"
     "/* a move trigger sent the player elsewhere: true once, with the level (1-9) and the tile to stand in */\n"
     "bool game_travel(int *level, float *x, float *z);"),
])

edit('src/game.c', [
    ("    int mouse_x, mouse_y, hover_slot;\n    char names[512][64];\n} G;",
     "    int mouse_x, mouse_y, hover_slot;\n    char names[512][64];\n"
     "    /* doors */\n    Door doors[128];\n    float door_target[128];\n    int ndoors, hover_door;\n"
     "    /* the move triggers: the tile the player was in; where a teleport sends them */\n"
     "    int last_tx, last_tz;\n    bool travel;\n    int travel_level;\n    float travel_x, travel_z;\n"
     "    /* the cards of the items and torches (rebuilt when they change), the doors' quads added every frame */\n"
     "    Vertex *cards;\n    int ncards;\n} G;"),
    ("static void rebuild_sprites(void) {\n    static Vertex cards[RENDER_MAX_SPRITE_VERTS];\n    int n = world_sprites(G.lv, G.mesh, G.lights, G.nlights, cards, RENDER_MAX_SPRITE_VERTS);\n    render_set_sprites(cards, n);\n}",
     "static Vertex g_cards[RENDER_MAX_SPRITE_VERTS];\n"
     "static void rebuild_sprites(void) { G.ncards = world_sprites(G.lv, G.mesh, G.lights, G.nlights, g_cards, RENDER_MAX_SPRITE_VERTS - 24 * 128); }\n"
     "static void upload_sprites(void) {\n"
     "    int n = G.ncards;\n"
     "    for (int k = 0; k < G.ndoors; k++) n += world_door_quads(G.lv, G.mesh, &G.doors[k], g_cards + n, RENDER_MAX_SPRITE_VERTS - n);\n"
     "    render_set_sprites(g_cards, n);\n"
     "}"),
    ("void game_level(Level *lv, Mesh *m, const Light *lights, int nlights) {\n    G.lv = lv, G.mesh = m, G.lights = lights, G.nlights = nlights;\n    rebuild_sprites();\n}",
     "void game_level(Level *lv, Mesh *m, const Light *lights, int nlights) {\n"
     "    G.lv = lv, G.mesh = m, G.lights = lights, G.nlights = nlights;\n"
     "    /* the doors (0x140-0x147 shut, 0x148-0x14F open) */\n"
     "    G.ndoors = 0, G.hover_door = -1, G.last_tx = G.last_tz = -1;\n"
     "    for (int t = 0; t < 64 * 64 && G.ndoors < 128; t++)\n"
     "        for (int i = lv->tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = lv->obj[i].next, guard++) {\n"
     "            const Object *o = &lv->obj[i];\n"
     "            if (o->item < 0x140 || o->item > 0x14F || G.ndoors >= 128) continue;\n"
     "            Door *d = &G.doors[G.ndoors];\n"
     "            d->tx = t % 64, d->tz = t / 64, d->heading = o->heading & 6, d->item = o->item, d->obj = i;\n"
     "            d->open = o->item >= 0x148 ? 1.0f : 0.0f;\n"
     "            G.door_target[G.ndoors++] = d->open;\n"
     "        }\n"
     "    rebuild_sprites();\n"
     "    upload_sprites();\n"
     "}\n\n"
     "/* --- doors ------------------------------------------------------------------------------------------- */\n"
     "static float seg_dist2(float px, float pz, float ax, float az, float bx, float bz) {\n"
     "    float dx = bx - ax, dz = bz - az, l = dx * dx + dz * dz, t = l > 0 ? ((px - ax) * dx + (pz - az) * dz) / l : 0;\n"
     "    t = t < 0 ? 0 : t > 1 ? 1 : t;\n"
     "    float qx = ax + dx * t - px, qz = az + dz * t - pz;\n"
     "    return qx * qx + qz * qz;\n"
     "}\n\n"
     "bool game_blocked(float x, float z, float r) {\n"
     "    for (int k = 0; k < G.ndoors; k++) {\n"
     "        const Door *d = &G.doors[k];\n"
     "        if (fabsf(x - (d->tx + 0.5f)) > 1.0f || fabsf(z - (d->tz + 0.5f)) > 1.0f) continue;\n"
     "        bool sx = d->heading == 0 || d->heading == 4;\n"
     "        float cx = d->tx + 0.5f, cz = d->tz + 0.5f, ax = sx ? 1.0f : 0.0f, az = sx ? 0.0f : 1.0f, hw = DOOR_WIDTH * 0.5f;\n"
     "        /* the frames beside the door */\n"
     "        if (seg_dist2(x, z, cx - ax * 0.5f, cz - az * 0.5f, cx - ax * hw, cz - az * hw) < r * r) return true;\n"
     "        if (seg_dist2(x, z, cx + ax * hw, cz + az * hw, cx + ax * 0.5f, cz + az * 0.5f) < r * r) return true;\n"
     "        /* the door while it is mostly shut */\n"
     "        if (d->open < 0.6f && seg_dist2(x, z, cx - ax * hw, cz - az * hw, cx + ax * hw, cz + az * hw) < r * r) return true;\n"
     "    }\n"
     "    return false;\n"
     "}\n\n"
     "/* is the door's lock still locked: its link is a lock (0x10F) */\n"
     "static bool locked(const Door *d) {\n"
     "    int l = G.lv->obj[d->obj].link;\n"
     "    return l > 0 && l < 1024 && G.lv->obj[l].item == 0x10F && !(G.lv->obj[l].flags & 1);\n"
     "}\n\n"
     "/* the door the view ray meets within reach */\n"
     "static void pick_door(const float eye[3], const float fwd[3], float *best) {\n"
     "    G.hover_door = -1;\n"
     "    for (int k = 0; k < G.ndoors; k++) {\n"
     "        const Door *d = &G.doors[k];\n"
     "        bool sx = d->heading == 0 || d->heading == 4;\n"
     "        float cx = d->tx + 0.5f, cz = d->tz + 0.5f, y0, y1;\n"
     "        world_door_span(G.lv, d, &y0, &y1);\n"
     "        /* the doorway's plane: z = cz (spans x) or x = cx */\n"
     "        float den = sx ? fwd[2] : fwd[0];\n"
     "        if (fabsf(den) < 1e-4f) continue;\n"
     "        float t = ((sx ? cz : cx) - (sx ? eye[2] : eye[0])) / den;\n"
     "        if (t < 0.05f || t > *best) continue;\n"
     "        float hx = eye[0] + fwd[0] * t, hy = eye[1] + fwd[1] * t, hz = eye[2] + fwd[2] * t;\n"
     "        float across = sx ? hx - cx : hz - cz;\n"
     "        if (fabsf(across) > DOOR_WIDTH * 0.5f + 0.05f || hy < y0 || hy > y1) continue;\n"
     "        *best = t, G.hover_door = k;\n"
     "    }\n"
     "}\n\n"
     "static void use_door(int k) {\n"
     "    Door *d = &G.doors[k];\n"
     "    if ((d->item & 7) == 7 && G.door_target[k] < 0.5f) return game_message(\"There is nothing to open there.\");\n"
     "    if (G.door_target[k] < 0.5f && locked(d)) return game_message(\"The door is locked.\");\n"
     "    G.door_target[k] = G.door_target[k] < 0.5f ? 1.0f : 0.0f;\n"
     "    d->item = (d->item & ~8) | (G.door_target[k] > 0.5f ? 8 : 0);\n"
     "    G.lv->obj[d->obj].item = (u16)d->item;\n"
     "}\n\n"
     "/* --- move triggers ---------------------------------------------------------------------------------- */\n"
     "static void run_trap(int i, int depth) {\n"
     "    if (i <= 0 || i >= 1024 || depth > 8) return;\n"
     "    const Object *o = &G.lv->obj[i];\n"
     "    switch (o->item) {\n"
     "    case 0x181: /* a teleport trap: the tile (quality, owner), on level z when that is not 0 */\n"
     "        G.travel = true, G.travel_level = o->z ? o->z : G.lv->number;\n"
     "        G.travel_x = o->quality + 0.5f, G.travel_z = o->owner + 0.5f;\n"
     "        break;\n"
     "    case 0x180: /* a damage trap */\n"
     "        G.hp -= o->quality, game_message(\"Ouch!\");\n"
     "        if (G.hp < 1) G.hp = 1;\n"
     "        break;\n"
     "    default: break;\n"
     "    }\n"
     "}\n\n"
     "static void enter_tile(int tx, int tz) {\n"
     "    for (int i = G.lv->tile[tz * 64 + tx].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = G.lv->obj[i].next, guard++)\n"
     "        if (G.lv->obj[i].item == 0x1A0) run_trap(G.lv->obj[i].link, 0);\n"
     "}\n\n"
     "bool game_travel(int *level, float *x, float *z) {\n"
     "    if (!G.travel) return false;\n"
     "    G.travel = false;\n"
     "    *level = G.travel_level, *x = G.travel_x, *z = G.travel_z;\n"
     "    return true;\n"
     "}"),
    ("    } else {\n        pick(eye, fwd);\n        if (in->use && G.hover_obj >= 0) take(G.hover_tx, G.hover_tz, G.hover_obj);\n    }\n    return G.open;",
     "    } else {\n"
     "        pick(eye, fwd);\n"
     "        float best = REACH;\n"
     "        if (G.hover_obj >= 0) {\n"
     "            float p[3];\n"
     "            world_object_pos(G.lv, G.hover_tx, G.hover_tz, &G.lv->obj[G.hover_obj], p);\n"
     "            best = sqrtf((p[0] - eye[0]) * (p[0] - eye[0]) + (p[2] - eye[2]) * (p[2] - eye[2]));\n"
     "        }\n"
     "        pick_door(eye, fwd, &best);\n"
     "        if (G.hover_door >= 0) G.hover_obj = -1;\n"
     "        if (in->use && G.hover_obj >= 0) take(G.hover_tx, G.hover_tz, G.hover_obj);\n"
     "        else if (in->use && G.hover_door >= 0) use_door(G.hover_door);\n"
     "    }\n"
     "    /* the doors swing (half a second) */\n"
     "    for (int k = 0; k < G.ndoors; k++) {\n"
     "        float *o = &G.doors[k].open, t = G.door_target[k];\n"
     "        if (*o < t) *o = MIN(t, *o + dt * 2);\n"
     "        else if (*o > t) *o = MAX(t, *o - dt * 2);\n"
     "    }\n"
     "    upload_sprites();\n"
     "    /* the move triggers of the tile the player has just walked into */\n"
     "    int tx = (int)eye[0], tz = (int)eye[2];\n"
     "    if ((tx != G.last_tx || tz != G.last_tz) && tx >= 0 && tx < 64 && tz >= 0 && tz < 64) {\n"
     "        if (G.last_tx >= 0) enter_tile(tx, tz);\n"
     "        G.last_tx = tx, G.last_tz = tz;\n"
     "    }\n"
     "    return G.open;"),
    ("        if (G.hover_obj >= 0) {\n            char name[96];\n            item_name(&G.lv->obj[G.hover_obj], name, sizeof name);",
     "        if (G.hover_door >= 0) {\n"
     "            const Door *d = &G.doors[G.hover_door];\n"
     "            if ((d->item & 7) != 7 || d->open > 0.5f) {\n"
     "                const char *t = d->open > 0.5f ? \"an open door\" : \"a door\", *h = d->open > 0.5f ? \"E - CLOSE\" : \"E - OPEN\";\n"
     "                ui_text(FONT_NORMAL, t, o->w / 2 - ui_text_width(FONT_NORMAL, t) / 2, o->h / 2 + 10, 255, 230, 160);\n"
     "                ui_text(FONT_SMALL, h, o->w / 2 - ui_text_width(FONT_SMALL, h) / 2, o->h / 2 + 20, 200, 200, 200);\n"
     "            }\n"
     "        }\n"
     "        if (G.hover_obj >= 0) {\n            char name[96];\n            item_name(&G.lv->obj[G.hover_obj], name, sizeof name);"),
])

edit('src/main.c', [
    ("        if (level_solid_at(lv, px, pz)) return false;",
     "        if (level_solid_at(lv, px, pz)) return false;\n        if (game_blocked(px, pz, 0.02f)) return false;"),
])
print('ok')
