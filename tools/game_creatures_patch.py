"""One-off patch: creatures (animation, simple AI, combat) in game.c / game.h / main.c."""
import sys

ROOT = sys.argv[1]


def edit(p, reps):
    p = ROOT + '/' + p
    s = open(p, encoding='utf-8').read()
    for a, b in reps:
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)


CREATURES = r'''
/* --- creatures ----------------------------------------------------------------------------------------
 * Each mobile object 0x40-0x7F of the level. Animations by the page's slots: 0 idle, 1 attack, 5 walk (when
 * there), 7 hit, 12 death (unconfirmed names, read from how the frames look). Hostile unless it has a
 * conversation (its last mobile byte, the npc number). A simple behaviour for now: one that sees the player
 * within 7 tiles comes and strikes; the rules from UW.EXE come later. */
enum { CS_IDLE, CS_WALK, CS_ATTACK, CS_HIT, CS_DYING, CS_DEAD };
typedef struct {
    int obj, kind, state;
    float x, z, y, hp, t, cooldown;
    bool hostile, struck;
} Creature;
static Creature g_cr[256];
static int g_ncr;
static float g_swing; /* the player's next swing in */
static u32 g_rng = 12345;
static int rnd(int n) {
    g_rng = g_rng * 1103515245u + 12345u;
    return (int)((g_rng >> 16) % (u32)n);
}

static void creatures_load(void) {
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
}

static int seg_len(const u8 *seg) {
    int n = 0;
    while (n < 8 && seg[n] != 255) n++;
    return n;
}

/* the frame to show: the slot's segment, at 6 frames a second (held at its end for a dying one) */
static int creature_frame(const Creature *c) {
    const Mesh *m = G.mesh;
    if (!m->crit[c->kind].loaded) return -1;
    static const int SLOT[6] = {0, 5, 1, 7, 12, 12};
    int slot = SLOT[c->state], s = m->crit[c->kind].slots[slot];
    if (s == 255 || s >= m->crit[c->kind].nsegs) s = m->crit[c->kind].slots[0];
    if (s == 255 || s >= m->crit[c->kind].nsegs) return -1;
    const u8 *seg = m->crit[c->kind].segs[s];
    int n = seg_len(seg);
    if (!n) return -1;
    int i = (int)(c->t * 6);
    if (c->state >= CS_DYING) i = MIN(i, n - 1);
    else i %= n;
    return seg[i] < m->crit[c->kind].nframes ? seg[i] : -1;
}

static bool walkable(float x, float z, float y) {
    static const float OX[4] = {0.2f, -0.2f, 0, 0}, OZ[4] = {0, 0, 0.2f, -0.2f};
    for (int k = 0; k < 4; k++) {
        if (level_solid_at(G.lv, x + OX[k], z + OZ[k]) || game_blocked(x + OX[k], z + OZ[k], 0.02f)) return false;
        if (level_floor_y(G.lv, x + OX[k], z + OZ[k]) > y + 0.3f) return false;
    }
    return true;
}

static bool sees(float ax, float az, float bx, float bz) {
    float dx = bx - ax, dz = bz - az, d = sqrtf(dx * dx + dz * dz);
    for (float s = 0.2f; s < d; s += 0.2f)
        if (level_solid_at(G.lv, ax + dx * s / d, az + dz * s / d)) return false;
    return true;
}

static void creature_name(const Creature *c, char *out, size_t n) { item_name(&G.lv->obj[c->obj], out, n); }

static void creatures_update(float dt, const float eye[3]) {
    for (int k = 0; k < g_ncr; k++) {
        Creature *c = &g_cr[k];
        c->t += dt;
        if (c->state == CS_DEAD) continue;
        if (c->state == CS_DYING) {
            if (c->t > 1.2f) c->state = CS_DEAD;
            continue;
        }
        c->cooldown -= dt;
        if ((c->state == CS_ATTACK && c->t < 0.7f) || (c->state == CS_HIT && c->t < 0.35f)) {
            if (c->state == CS_ATTACK && !c->struck && c->t > 0.35f) {
                c->struck = true;
                float dx = eye[0] - c->x, dz = eye[2] - c->z;
                if (dx * dx + dz * dz < 1.2f * 1.2f) {
                    int dmg = 1 + rnd(3);
                    G.hp -= (float)dmg;
                    char name[96];
                    creature_name(c, name, sizeof name);
                    const char *noun = strchr(name, ' ');
                    game_message("The %s hits thee.", noun ? noun + 1 : name);
                }
            }
            continue;
        }
        float dx = eye[0] - c->x, dz = eye[2] - c->z, d = sqrtf(dx * dx + dz * dz);
        if (!c->hostile || d > 7 || !sees(c->x, c->z, eye[0], eye[2])) {
            if (c->state != CS_IDLE) c->state = CS_IDLE, c->t = 0;
            continue;
        }
        if (d > 0.85f) {
            float sp = 0.9f * dt, nx = c->x + dx / d * sp, nz = c->z + dz / d * sp;
            if (walkable(nx, c->z, c->y)) c->x = nx;
            if (walkable(c->x, nz, c->y)) c->z = nz;
            c->y = level_floor_y(G.lv, c->x, c->z);
            if (c->state != CS_WALK) c->state = CS_WALK, c->t = 0;
        } else if (c->cooldown <= 0) {
            c->state = CS_ATTACK, c->t = 0, c->struck = false, c->cooldown = 1.6f;
        } else if (c->state != CS_IDLE) c->state = CS_IDLE, c->t = 0;
    }
}

/* the player's swing: the living creature the view ray passes within reach */
static void player_attack(const float eye[3], const float fwd[3]) {
    if (g_swing > 0) return;
    g_swing = 0.7f;
    int best = -1;
    float bt = 1.7f;
    for (int k = 0; k < g_ncr; k++) {
        Creature *c = &g_cr[k];
        if (c->state >= CS_DYING) continue;
        float p[3] = {c->x - eye[0], c->y + 0.35f - eye[1], c->z - eye[2]};
        float t = p[0] * fwd[0] + p[1] * fwd[1] + p[2] * fwd[2];
        if (t < 0.1f || t > bt) continue;
        float q[3] = {p[0] - fwd[0] * t, p[1] - fwd[1] * t, p[2] - fwd[2] * t};
        if (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > 0.4f * 0.4f) continue;
        bt = t, best = k;
    }
    if (best < 0) return game_message("Thou swingest at the air.");
    Creature *c = &g_cr[best];
    char name[96];
    creature_name(c, name, sizeof name);
    const char *noun = strchr(name, ' ');
    noun = noun ? noun + 1 : name;
    if (rnd(5) == 0) return game_message("Thou missest the %s.", noun);
    c->hp -= (float)(2 + rnd(5));
    c->hostile = true;
    if (c->hp <= 0) {
        c->state = CS_DYING, c->t = 0;
        game_message("Thou hast slain the %s!", noun);
    } else {
        c->state = CS_HIT, c->t = 0;
        game_message("Thou hittest the %s.", noun);
    }
}

static int creature_cards(Vertex *out, int cap) {
    int n = 0;
    for (int k = 0; k < g_ncr && n + 6 <= cap; k++) {
        const Creature *c = &g_cr[k];
        int fr = creature_frame(c);
        if (fr < 0) continue;
        float pos[3] = {c->x, c->y, c->z};
        n += world_creature_card(G.mesh, c->kind, fr, pos, out + n);
    }
    return n;
}
'''

edit('src/game.c', [
    ("/* --- move triggers ----", CREATURES + "\n/* --- move triggers ----"),
    ("    for (int k = 0; k < G.ndoors; k++) n += world_door_quads(G.lv, G.mesh, &G.doors[k], g_cards + n, RENDER_MAX_SPRITE_VERTS - n);\n",
     "    for (int k = 0; k < G.ndoors; k++) n += world_door_quads(G.lv, G.mesh, &G.doors[k], g_cards + n, RENDER_MAX_SPRITE_VERTS - n);\n"
     "    n += creature_cards(g_cards + n, RENDER_MAX_SPRITE_VERTS - n);\n"),
    ("    rebuild_sprites();\n    upload_sprites();\n}",
     "    creatures_load();\n    rebuild_sprites();\n    upload_sprites();\n}"),
    ("    if (in->inventory) G.open = !G.open;",
     "    if (in->inventory) G.open = !G.open;\n"
     "    g_swing -= dt;\n"
     "    creatures_update(dt, eye);\n"
     "    if (in->attack && !G.open) player_attack(eye, fwd);\n"
     "    /* health comes back slowly; at nothing, the player wakes at the entrance of the Abyss */\n"
     "    if (G.hp < G.hp_max) G.hp = MIN(G.hp_max, G.hp + dt * 0.08f);\n"
     "    if (G.hp <= 0) {\n"
     "        G.hp = G.hp_max;\n"
     "        game_message(\"Thou hast fallen... and awakest at the entrance.\");\n"
     "        G.travel = true, G.travel_level = 1, G.travel_x = 32.0f, G.travel_z = 1.5f;\n"
     "    }"),
])

# the forward declaration: game_blocked is used by the creatures before its definition
edit('src/game.c', [
    ("static void rebuild_sprites(void)", "bool game_blocked(float x, float z, float r);\nstatic int creature_cards(Vertex *out, int cap);\nstatic void creatures_load(void);\nstatic void rebuild_sprites(void)"),
])

edit('src/game.h', [
    ("    bool torch;            /* light / put out the carried light (the first light source in the pack) */",
     "    bool torch;            /* light / put out the carried light (the first light source in the pack) */\n"
     "    bool attack;           /* swing at what is under the crosshair */"),
])

edit('src/main.c', [
    ("    case WM_LBUTTONDOWN:\n        if (g_inventory) {\n            g_click_l = true;\n            return 0;\n        }",
     "    case WM_LBUTTONDOWN:\n        if (g_inventory) {\n            g_click_l = true;\n            return 0;\n        }\n"
     "        if (g_captured) {\n            g_attack = true;\n            return 0;\n        }"),
    ("static bool g_focus, g_captured, g_inventory, g_click_l, g_click_r;",
     "static bool g_focus, g_captured, g_inventory, g_click_l, g_click_r, g_attack;"),
    ("            gi.click_left = g_click_l, gi.click_right = g_click_r;\n            g_click_l = g_click_r = false;",
     "            gi.click_left = g_click_l, gi.click_right = g_click_r, gi.attack = g_attack;\n            g_click_l = g_click_r = g_attack = false;"),
])
print('ok')
