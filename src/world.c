#include "world.h"
#include "models.h"

#include <math.h>
#include <string.h>

/* object numbers (item ids): 0x40-0x7F creatures, 0x140-0x15F doors and 3D things, 0x160-0x17F wall
 * decorations, switches and special things, 0x180-0x1FF traps and triggers (invisible) */
bool world_item_shown(const Object *o) {
    int id = o->item;
    /* (item 0 is the hand axe, a thing like any other: `id == 0` here hid level 1's at 26,12 and two more; a tile's
     * list never holds an unused object. work\soundudit.py, 2026-11-15) */
    if (id >= 0x140) return false;
    if (id >= 0x40 && id < 0x80) return false; /* creatures: drawn from their own animations (to come) */
    if ((o->flags >> 5) & 1) return false;     /* invisible */
    return true;
}

/* a wall decoration's picture (its sprite cell, TMOBJ.GR from TMOBJ_CELL), or -1: writing 0x166 = 20 + flags
 * & 7, gravestone 0x165 = 28 + flags & 1 */
int world_decal_cell(const Object *o) {
    if ((o->flags >> 5) & 1) return -1;
    if (o->item == 0x166) return TMOBJ_CELL + 20 + (o->flags & 7);
    if (o->item == 0x165) return TMOBJ_CELL + 28 + (o->flags & 1);
    if (o->item >= 0x170 && o->item <= 0x17F) return TMFLAT_CELL + (o->item & 15); /* switches: TMFLAT.GR */
    return -1;
}

/* where it hangs: the original draws it as a 3D model at the object's position turned by its heading (DRAWOBJ.C
 * do_rect), and the level data has the heading name the wall: 0 the north wall (+z), 2 east (+x), 4 south, 6 west
 * (every writing in LEV.ARK: work\writings.py), the picture facing into the room. On that wall, just off it, at
 * the object's place along it; a diagonal heading (a diagonal wall, a gravestone standing free) at its position */
void world_decal_place(const Level *lv, int tx, int tz, const Object *o, float c[3], float nrm[3]) {
    world_object_pos(lv, tx, tz, o, c);
    const float off = 0.045f; /* in front of the stones' relief (0.035) */
    float a = o->heading * 0.785398f, dx = sinf(a), dz = cosf(a); /* towards the wall it hangs on */
    nrm[0] = -dx, nrm[1] = 0, nrm[2] = -dz;
    if (dx > 0.7f) c[0] = tx + 1 - off, nrm[0] = -1, nrm[2] = 0;
    else if (dx < -0.7f) c[0] = tx + off, nrm[0] = 1, nrm[2] = 0;
    else if (dz > 0.7f) c[2] = tz + 1 - off, nrm[0] = 0, nrm[2] = -1;
    else if (dz < -0.7f) c[2] = tz + off, nrm[0] = 0, nrm[2] = 1;
}

void world_object_pos(const Level *lv, int tx, int tz, const Object *o, float pos[3]) {
    pos[0] = (float)tx + ((float)o->x + 0.5f) / 8.0f;
    pos[2] = (float)tz + ((float)o->y + 0.5f) / 8.0f;
    pos[1] = (float)o->z / 32.0f; /* 0-127: the floor of height h is at 8h, the ceiling at 128 */
    /* On a slope the game sets a creature down at the tile's base height (AI.C move_mobile: SET_Z(to->height << 3);
     * CRITTIME.C's placements), up to a step under the slope's surface; the original's renderer has no depth test, so
     * the picture shows all the same, where our depth-tested floor would bury it (a slug on level 1's ramps vanished
     * while it moved; the user, 2026-11-13). So a thing under the floor's surface stands on it (presentation only). */
    if (lv) {
        float fy = level_floor_y(lv, pos[0], pos[2]);
        if (pos[1] < fy && fy < level_ceil_y()) pos[1] = fy;
    }
}

/* a card: its bottom middle at pos, the picture k of the atlas, s tiles a pixel; kind 3 lit, 4 glowing */
static int card(Vertex *out, const float pos[3], int k, int w, int h, float s, float kind) {
    float rect[4] = {(float)(k % 32 * 32), (float)(1024 + k / 32 * 32), (float)w, (float)h};
    float hw = w * s * 0.5f, ht = h * s;
    static const float CX[6] = {-1, 1, 1, -1, 1, -1}, CY[6] = {0, 0, 1, 0, 1, 1};
    for (int i = 0; i < 6; i++) {
        Vertex *v = &out[i];
        memset(v, 0, sizeof *v);
        memcpy(v->pos, pos, 12), memcpy(v->prev, pos, 12);
        v->nrm[0] = CX[i] * hw, v->nrm[1] = CY[i] * ht, v->nrm[2] = 0;
        v->uv[0] = CX[i] > 0 ? (float)w - 0.001f : 0.0f, v->uv[1] = CY[i] > 0 ? 0.0f : (float)h - 0.001f;
        memcpy(v->rect, rect, 16);
        v->color[0] = v->color[1] = v->color[2] = 1, v->color[3] = kind;
    }
    return 6;
}

/* an animated object's frame (ANIMO.GR, at ANIMO_CELL) standing at pos: 6 vertices, 0 when there is no picture */
int world_animo_card(const Mesh *m, int frame, const float pos[3], Vertex *out, bool glow) {
    if (frame < 0 || frame >= 64 || !m->animo_w[frame]) return 0;
    /* glow: self-lit (kind 4, as a burning torch on a wall): sparks, explosions, spell effects */
    return card(out, pos, ANIMO_CELL + frame, m->animo_w[frame], m->animo_h[frame], SPRITE_SCALE, glow ? 4 : 3);
}

int world_item_card(const Mesh *m, int item, const float pos[3], Vertex *out) {
    if (item < 0 || item >= MAX_SPRITES || !m->sprite_w[item]) return 0;
    return card(out, pos, item, m->sprite_w[item], m->sprite_h[item], SPRITE_SCALE, 3);
}

/* a world quad: corners a, b (bottom) and their tops at height top; u along a -> b in texels, v from the top */
static int quad(Vertex *out, const float a[3], const float b[3], float top_a, float top_b, float u0, float u1, float v_top, float v_bot,
                const float rect[4], const float nrm[3]) {
    float p[4][3] = {{a[0], a[1], a[2]}, {b[0], b[1], b[2]}, {b[0], top_b, b[2]}, {a[0], top_a, a[2]}};
    float uv[4][2] = {{u0, v_bot}, {u1, v_bot}, {u1, v_top}, {u0, v_top}};
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) {
        Vertex *v = &out[i];
        memset(v, 0, sizeof *v);
        memcpy(v->pos, p[T[i]], 12), memcpy(v->prev, p[T[i]], 12);
        memcpy(v->uv, uv[T[i]], 8), memcpy(v->rect, rect, 16), memcpy(v->nrm, nrm, 12);
        v->color[0] = v->color[1] = v->color[2] = 1, v->color[3] = 5;
    }
    return 6;
}

void world_door_span(const Level *lv, const Door *d, float *y0, float *y1) {
    const Tile *t = &lv->tile[d->tz * 64 + d->tx];
    *y0 = t->height * STEP;
    *y1 = MIN(*y0 + 1.0f, level_ceil_y());
}

/* any quad: corners p[4] in order round it, their texel coordinates uv[4], the normal n; kind 1 lit like the level
 * (with its normal map), 5 a door's (seen from either side, see-through where the picture is) */
static int quad4(Vertex *out, float p[4][3], const float uv[4][2], const float rect[4], const float n[3], float kind) {
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++) {
        Vertex *v = &out[i];
        memset(v, 0, sizeof *v);
        memcpy(v->pos, p[T[i]], 12), memcpy(v->prev, p[T[i]], 12);
        memcpy(v->uv, uv[T[i]], 8), memcpy(v->rect, rect, 16), memcpy(v->nrm, n, 12);
        v->color[0] = v->color[1] = v->color[2] = 1, v->color[3] = kind;
    }
    return 6;
}

/* A door in its frame. The original's frame is its model 1 (UW.EXE, tools\models.py): a slab across the tile with
 * an opening half a tile wide and 13/16 high, the wall's texture on its faces and a flat colour inside the opening,
 * only 1/32 of a tile thick. Ours keeps its opening (the door picture's opaque part, the same size) and gives the
 * slab depth (DOOR_FRAME_DEPTH) with stone reveals, and the door a thickness, swung on its hinge (a portcullis
 * rises). Pictures only: what blocks and what is pointed at are game.c's, as before. */
#define DOOR_FRAME_DEPTH 0.125f
#define DOOR_THICK 0.04f
int world_door_quads(const Level *lv, const Mesh *m, const Door *d, Vertex *out, int cap) {
    if (cap < 14 * 6) return 0;
    const Tile *t = &lv->tile[d->tz * 64 + d->tx];
    float cx = d->tx + 0.5f, cz = d->tz + 0.5f, y0, y1, ceil = level_ceil_y();
    world_door_span(lv, d, &y0, &y1);
    bool spans_x = d->heading == 0 || d->heading == 4;
    /* al: across the passage (the door's width); nr: along the passage */
    float al[3] = {spans_x ? 1.0f : 0.0f, 0, spans_x ? 0.0f : 1.0f}, nr[3] = {spans_x ? 0.0f : 1.0f, 0, spans_x ? 1.0f : 0.0f};
    int wt = lv->walls[t->wall % 48];
    float wrect[4] = {(float)(wt % 16 * 64), (float)(wt / 16 * 64), 64, 64};
    float hw = DOOR_WIDTH * 0.5f, f = DOOR_FRAME_DEPTH * 0.5f;
    int k = d->item & 7, n = 0;
    int pic = k < 6 ? lv->doors[k] : (k == 6 ? 12 : -1); /* 0x146 a portcullis, 0x147 a secret door (the wall) */
    bool has_pic = pic >= 0 && pic < 16 && m->door_w[pic];
    /* the opening: the door picture's opaque part (DOORS.GR leaves its top 12-13 rows and a side column clear) */
    float lx = -hw, rx = hw, ytop = y1;
    if (has_pic) {
        lx = -hw + m->door_l[pic] / 32.0f * DOOR_WIDTH, rx = -hw + m->door_r[pic] / 32.0f * DOOR_WIDTH;
        ytop = y1 - m->door_top[pic] / 64.0f * (y1 - y0);
    }
    ytop = MIN(ytop, ceil);
#define PT(P, s, tt, y) (P)[0] = cx + al[0] * (s) + nr[0] * (tt), (P)[1] = (y), (P)[2] = cz + al[2] * (s) + nr[2] * (tt)
    /* the frame: front (-f) and back (+f) faces beside and above the opening, the wall's texture as on the walls
     * (u: texels across, v: from the ceiling down) */
    for (int side = 0; side < 2; side++) {
        float tt = side ? f : -f, nn[3] = {side ? nr[0] : -nr[0], 0, side ? nr[2] : -nr[2]};
        float box[3][4] = {{-0.5f, lx, y0, ceil}, {rx, 0.5f, y0, ceil}, {lx, rx, ytop, ceil}};
        for (int b = 0; b < 3; b++) {
            float s0 = box[b][0], s1 = box[b][1], ya = box[b][2], yb = box[b][3];
            if (s1 - s0 < 1e-4f || yb - ya < 1e-4f) continue;
            float p[4][3], uv[4][2] = {{s0 * 64, (ceil - ya) * 64}, {s1 * 64, (ceil - ya) * 64}, {s1 * 64, (ceil - yb) * 64}, {s0 * 64, (ceil - yb) * 64}};
            PT(p[0], s0, tt, ya), PT(p[1], s1, tt, ya), PT(p[2], s1, tt, yb), PT(p[3], s0, tt, yb);
            n += quad4(out + n, p, uv, wrect, nn, 1);
        }
    }
    /* the reveals inside the opening: its two sides and the lintel's underside (stone, the wall's texture) */
    {
        float p[4][3], u0 = -f * 64, u1 = f * 64;
        float uv[4][2] = {{u0, (ceil - y0) * 64}, {u1, (ceil - y0) * 64}, {u1, (ceil - ytop) * 64}, {u0, (ceil - ytop) * 64}};
        float nl[3] = {al[0], 0, al[2]}, nrr[3] = {-al[0], 0, -al[2]}, nd[3] = {0, -1, 0};
        PT(p[0], lx, -f, y0), PT(p[1], lx, f, y0), PT(p[2], lx, f, ytop), PT(p[3], lx, -f, ytop);
        n += quad4(out + n, p, uv, wrect, nl, 1);
        PT(p[0], rx, -f, y0), PT(p[1], rx, f, y0), PT(p[2], rx, f, ytop), PT(p[3], rx, -f, ytop);
        n += quad4(out + n, p, uv, wrect, nrr, 1);
        if (ceil > ytop + 1e-3f) {
            float uvl[4][2] = {{lx * 64, u0 + 32}, {rx * 64, u0 + 32}, {rx * 64, u1 + 32}, {lx * 64, u1 + 32}};
            PT(p[0], lx, -f, ytop), PT(p[1], rx, -f, ytop), PT(p[2], rx, f, ytop), PT(p[3], lx, f, ytop);
            n += quad4(out + n, p, uvl, wrect, nd, 1);
        }
    }
    /* the door: a board DOOR_THICK thick filling the opening, turned about its hinge toward +nr by open x 90
     * degrees; a portcullis rises into the frame instead. Every door picture has its hinges at its right (the rx
     * edge): the board runs from there (s' 0) to the lx edge (s' W). */
    float rect[4], tu0, tu1, tv0, tv1;
    if (has_pic) rect[0] = (float)(pic * 32), rect[1] = DOOR_ROW, rect[2] = 32, rect[3] = 64, tu0 = m->door_l[pic], tu1 = m->door_r[pic],
                 tv0 = m->door_top[pic], tv1 = 63.999f;
    else memcpy(rect, wrect, 16), tu0 = lx * 64, tu1 = rx * 64, tv0 = (ceil - ytop) * 64, tv1 = (ceil - y0) * 64;
    bool portcullis = k == 6;
    float th = portcullis ? 0.02f : DOOR_THICK, W = rx - lx, a = portcullis ? 0.0f : d->open * 1.5707963f;
    float lift = portcullis ? d->open * (ytop - y0) * 0.95f : 0.0f, ya = y0 + lift, yb = ytop + lift;
    float u[3] = {-al[0] * cosf(a) + nr[0] * sinf(a), 0, -al[2] * cosf(a) + nr[2] * sinf(a)}; /* along the board */
    float w[3] = {al[0] * sinf(a) + nr[0] * cosf(a), 0, al[2] * sinf(a) + nr[2] * cosf(a)};   /* through it */
    float H[3];
    PT(H, rx, th * 0.5f, 0);
#define DP(P, s, tt, y) (P)[0] = H[0] + u[0] * (s) + w[0] * (tt), (P)[1] = (y), (P)[2] = H[2] + u[2] * (s) + w[2] * (tt)
    {
        float t0 = tu0;
        tu0 = tu1, tu1 = t0; /* s' 0 is the picture's right edge */
        float p[4][3], uv[4][2] = {{tu0, tv1}, {tu1, tv1}, {tu1, tv0}, {tu0, tv0}};
        float nb[3] = {-w[0], 0, -w[2]}, nf[3] = {w[0], 0, w[2]};
        DP(p[0], 0, -th, ya), DP(p[1], W, -th, ya), DP(p[2], W, -th, yb), DP(p[3], 0, -th, yb);
        n += quad4(out + n, p, uv, rect, nb, 5);
        DP(p[0], 0, 0, ya), DP(p[1], W, 0, ya), DP(p[2], W, 0, yb), DP(p[3], 0, 0, yb);
        n += quad4(out + n, p, uv, rect, nf, 5);
        if (!portcullis) { /* its edges: a column and a row of the picture drawn out across the board's thickness, in
                              from its border: the outermost columns are its dark outline, the lock plate and moss, which
                              drew the edge as a streaky grey-green band (the user). The free (lock) edge a quarter of
                              the width in, past the lock plate: planks, the iron bands wrapping round; the hinge edge
                              and the top three texels in */
            float ce = tu1 + (tu0 - tu1) * 0.25f, ch = tu0 - 3.5f, rt = tv0 + 3.5f; /* (tu0 > tu1 now) */
            float uve[4][2] = {{ce, tv1}, {ce, tv1}, {ce, tv0}, {ce, tv0}}, uvh[4][2] = {{ch, tv1}, {ch, tv1}, {ch, tv0}, {ch, tv0}};
            float uvt[4][2] = {{tu0, rt}, {tu1, rt}, {tu1, rt}, {tu0, rt}};
            float ne[3] = {u[0], 0, u[2]}, nh[3] = {-u[0], 0, -u[2]}, nt[3] = {0, 1, 0};
            DP(p[0], W, -th, ya), DP(p[1], W, 0, ya), DP(p[2], W, 0, yb), DP(p[3], W, -th, yb);
            n += quad4(out + n, p, uve, rect, ne, 5);
            DP(p[0], 0, -th, ya), DP(p[1], 0, 0, ya), DP(p[2], 0, 0, yb), DP(p[3], 0, -th, yb);
            n += quad4(out + n, p, uvh, rect, nh, 5);
            DP(p[0], 0, -th, yb), DP(p[1], W, -th, yb), DP(p[2], W, 0, yb), DP(p[3], 0, 0, yb);
            n += quad4(out + n, p, uvt, rect, nt, 5);
        }
    }
#undef DP
#undef PT
    return n;
}

int world_door_quads_flat(const Level *lv, const Mesh *m, const Door *d, Vertex *out, int cap);
int world_door_quads_flat(const Level *lv, const Mesh *m, const Door *d, Vertex *out, int cap) {
    if (cap < 24) return 0;
    const Tile *t = &lv->tile[d->tz * 64 + d->tx];
    float cx = d->tx + 0.5f, cz = d->tz + 0.5f, y0, y1, ceil = level_ceil_y();
    world_door_span(lv, d, &y0, &y1);
    bool spans_x = d->heading == 0 || d->heading == 4;
    /* along: the unit vector across the passage (the door's width); nrm: along the passage */
    float al[3] = {spans_x ? 1.0f : 0.0f, 0, spans_x ? 0.0f : 1.0f}, nrm[3] = {spans_x ? 0.0f : 1.0f, 0, spans_x ? 1.0f : 0.0f};
    float wrect[4] = {(float)(t->wall % 16 * 64), (float)(t->wall / 16 * 64), 64, 64};
    int wt = lv->walls[t->wall % 48];
    wrect[0] = (float)(wt % 16 * 64), wrect[1] = (float)(wt / 16 * 64);
    int n = 0;
    float hw = DOOR_WIDTH * 0.5f;
#define AT(s, P) P[0] = cx + al[0] * (s), P[1] = y0, P[2] = cz + al[2] * (s)
    float p0[3], p1[3];
    int k = d->item & 7;
    int pic = k < 6 ? lv->doors[k] : (k == 6 ? 12 : -1); /* 0x146 a portcullis, 0x147 a secret door (the wall) */
    /* the opening is the door picture's opaque part (DOORS.GR leaves its top 12-13 rows and a side column
     * see-through): the frame comes in to it, so nothing shows through round a shut door */
    float lx = -hw, rx = hw, ytop = y1;
    if (pic >= 0 && pic < 16 && m->door_w[pic]) {
        lx = -hw + m->door_l[pic] / 32.0f * DOOR_WIDTH, rx = -hw + m->door_r[pic] / 32.0f * DOOR_WIDTH;
        ytop = y1 - m->door_top[pic] / 64.0f * (y1 - y0);
    }
    /* the frame: the wall's texture beside the opening, and above it up to the ceiling */
    AT(-0.5f, p0), AT(lx, p1);
    n += quad(out + n, p0, p1, ceil, ceil, -0.5f * 64, lx * 64, 0, (ceil - y0) * 64, wrect, nrm);
    AT(rx, p0), AT(0.5f, p1);
    n += quad(out + n, p0, p1, ceil, ceil, rx * 64, 0.5f * 64, 0, (ceil - y0) * 64, wrect, nrm);
    if (ceil > ytop + 1e-3f) {
        AT(lx, p0), AT(rx, p1);
        p0[1] = p1[1] = ytop;
        n += quad(out + n, p0, p1, ceil, ceil, lx * 64, rx * 64, 0, (ceil - ytop) * 64, wrect, nrm);
    }
    /* the door: swung on its hinge (the -hw side) toward +nrm by open x 90 degrees */
    float rect[4];
    if (pic >= 0 && pic < 16 && m->door_w[pic]) rect[0] = (float)(pic * 32), rect[1] = DOOR_ROW, rect[2] = 32, rect[3] = 64;
    else memcpy(rect, wrect, 16);
    float a = d->open * 1.5707963f, hinge[3], tip[3];
    AT(-hw, hinge);
    tip[0] = hinge[0] + (al[0] * cosf(a) + nrm[0] * sinf(a)) * DOOR_WIDTH;
    tip[2] = hinge[2] + (al[2] * cosf(a) + nrm[2] * sinf(a)) * DOOR_WIDTH;
    tip[1] = y0;
    float dn[3] = {-(al[2] * cosf(a) + nrm[2] * sinf(a)), 0, al[0] * cosf(a) + nrm[0] * sinf(a)};
    if (rect[2] == 32) n += quad(out + n, hinge, tip, y1, y1, 0, 32, 0, 63.999f, rect, dn);
    else n += quad(out + n, hinge, tip, y1, y1, -hw * 64, hw * 64, (ceil - y1) * 64, (ceil - y0) * 64, rect, dn);
#undef AT
    return n;
}

int world_creature_card(const Mesh *m, int k, int fr, const float pos[3], Vertex *out) {
    return world_creature_card_mv(m, k, fr, pos, pos, out);
}
int world_creature_card_mv(const Mesh *m, int k, int fr, const float pos[3], const float prev[3], Vertex *out) {
    if (k < 0 || k >= 64 || !m->crit[k].loaded || fr < 0 || fr >= m->crit[k].nframes) return 0;
    int w = m->crit[k].w[fr], h = m->crit[k].h[fr];
    float rect[4] = {(float)m->crit[k].fx[fr], (float)m->crit[k].fy[fr], (float)w, (float)h};
    float s = CREATURE_SCALE, hx = m->crit[k].hx[fr], hy = m->crit[k].hy[fr];
    /* the corners round the hotspot: x from -hx to w - hx, y from -(h - 1 - hy) (below the feet) to hy + 1 */
    float x0 = -hx * s, x1 = (w - hx) * s, y0 = -(h - 1 - hy) * s, y1 = (hy + 1) * s;
    static const int CX[6] = {0, 1, 1, 0, 1, 0}, CY[6] = {0, 0, 1, 0, 1, 1};
    for (int i = 0; i < 6; i++) {
        Vertex *v = &out[i];
        memset(v, 0, sizeof *v);
        memcpy(v->pos, pos, 12), memcpy(v->prev, prev, 12);
        v->nrm[0] = CX[i] ? x1 : x0, v->nrm[1] = CY[i] ? y1 : y0;
        v->uv[0] = CX[i] ? (float)w - 0.001f : 0.0f, v->uv[1] = CY[i] ? 0.0f : (float)h - 0.001f;
        memcpy(v->rect, rect, 16);
        v->color[0] = v->color[1] = v->color[2] = 1, v->color[3] = 3;
    }
    return 6;
}

int world_sprites(const Level *lv, const Mesh *m, const Light *l, int nl, Vertex *out, int cap) {
    int n = 0;
    for (int tz = 0; tz < 64; tz++)
        for (int tx = 0; tx < 64; tx++) {
            int i = lv->tile[tz * 64 + tx].first_object, guard = 0;
            while (i > 0 && i < 1024 && guard++ < 1024) {
                const Object *o = &lv->obj[i];
                /* an arrow or a bolt (0x11, 0x12) is a 3D model in the original (render type 2: world_model_quads) */
                if (world_item_shown(o) && o->item != 0x11 && o->item != 0x12 && o->item < MAX_SPRITES && m->sprite_w[o->item] && n + 6 <= cap) {
                    /* do_obj: the rune stones 0xE8-0xFF all draw as the one rune stone (ITEM_RUNESTONE 0xE0) in the
                     * world; their own letters are the inventory's pictures */
                    int pic = (o->item & 0x1E0) == 0xE0 && (o->item & 0x18) && m->sprite_w[0xE0] ? 0xE0 : o->item;
                    float pos[3];
                    world_object_pos(lv, tx, tz, o, pos);
                    /* what burns glows (self-lit, kind 4): a lit lantern, torch, candle or taper (0x94-0x97), a campfire */
                    bool glow = (o->item >= 0x94 && o->item <= 0x97) || o->item == 0x12A || o->item == 0xAD;   /* the ankh (0xAD): a holy gold glow (the user) */
                    /* glass (ale, the potions, water, port, wine 0x0BA-0x0BF, an oil flask 0x12D): light through it,
                       tinted (the shadow rays, kind 3.3) */
                    bool glass = (o->item >= 0x0BA && o->item <= 0x0BF) || o->item == 0x12D;
                    n += card(out + n, pos, pic, m->sprite_w[pic], m->sprite_h[pic], SPRITE_SCALE, glow ? 4 : glass ? 3.3f : 3);
                    if (o->item == 0xAD)   /* the ankh's picture brighter, gold (its pixels are dark) */
                        for (int k = n - 6; k < n; k++) out[k].color[0] = 2.2f, out[k].color[1] = 1.9f, out[k].color[2] = 1.1f;
                }
                i = o->next;
            }
        }
    /* the wall decorations: flat on the wall they touch */
    for (int tz = 0; tz < 64; tz++)
        for (int tx = 0; tx < 64; tx++)
            for (int i = lv->tile[tz * 64 + tx].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = lv->obj[i].next, guard++) {
                const Object *o = &lv->obj[i];
                int cell = world_decal_cell(o);
                if (cell < 0 || cell >= 512 || !m->sprite_w[cell] || n + 6 > cap) continue;
                float c[3], nrm[3];
                world_decal_place(lv, tx, tz, o, c, nrm);
                float w = m->sprite_w[cell] * DECAL_SCALE, h = m->sprite_h[cell] * DECAL_SCALE;
                float r[3] = {-nrm[2], 0, nrm[0]}; /* right, as seen by someone facing the wall */
                float a[3] = {c[0] - r[0] * w * 0.5f, c[1] - h * 0.5f, c[2] - r[2] * w * 0.5f};
                float b[3] = {c[0] + r[0] * w * 0.5f, c[1] - h * 0.5f, c[2] + r[2] * w * 0.5f};
                float rect[4] = {(float)(cell % 32 * 32), (float)(1024 + cell / 32 * 32), (float)m->sprite_w[cell], (float)m->sprite_h[cell]};
                n += quad(out + n, a, b, c[1] + h * 0.5f, c[1] + h * 0.5f, 0, m->sprite_w[cell] - 0.001f, 0, m->sprite_h[cell] - 0.001f, rect, nrm);
            }
    /* the burning torches on the walls: the lit torch picture (0x95) below the flame's light */
    const int lit_torch = 0x95; /* "a_lit torch" (strings block 4: 0x94 is the lit lantern) */
    for (int k = 0; k < nl; k++) {
        if (l[k].kind != LIGHT_WALL_TORCH || !m->sprite_w[lit_torch] || n + 6 > cap) continue;
        /* only a torch hung on a wall has a facing (lights.c); the burning things' lights (a blow's spark, an
           explosion, an ankh, a lantern lying lit) flicker as LIGHT_WALL_TORCH too but are drawn by themselves */
        if (l[k].facing[0] == 0.0f && l[k].facing[1] == 0.0f) continue;
        float s = SPRITE_SCALE * 1.6f, pos[3] = {l[k].pos[0], l[k].pos[1] - m->sprite_h[lit_torch] * s * 0.8f, l[k].pos[2]};
        n += card(out + n, pos, lit_torch, m->sprite_w[lit_torch], m->sprite_h[lit_torch], s, 4);
    }
    return n;
}

/* ---- the original's 3D models (models.h) ---- */

/* the colour of a texture: its first pixel (DRAWOBJ.C's do_rect for flag 0x20 models: seg009_38C's first byte),
 * from our atlas, RGB 0..1 */
static void atlas_colour(const Mesh *m, const float rect[4], float rgb[3]) {
    int x = (int)rect[0], y = (int)rect[1];
    if (!m->atlas || x < 0 || y < 0 || x >= ATLAS_SIZE || y >= ATLAS_H) { rgb[0] = rgb[1] = rgb[2] = 0.5f; return; }
    const u8 *p = m->atlas + ((size_t)y * ATLAS_SIZE + (size_t)x) * 4;
    rgb[0] = p[0] / 255.0f, rgb[1] = p[1] / 255.0f, rgb[2] = p[2] / 255.0f;
}
static void wall_rect(const Level *lv, const Tile *t, float r[4]) {
    int wt = lv->walls[t->wall % 48];
    r[0] = (float)(wt % 16 * 64), r[1] = (float)(wt / 16 * 64), r[2] = 64, r[3] = 64;
}
static void inst_init(ModelInst *mi, int model, const float pos[3], float turn) {
    memset(mi, 0, sizeof *mi);
    mi->model = model;
    memcpy(mi->pos, pos, 12);
    mi->turn = turn;
    mi->var0_rgb[0] = -1;
    mi->kind_tex = 5, mi->kind_flat = 1;
    const unsigned char *cols = models_cols(model);
    for (int i = 0; i < MCOL_NCOLS(cols[0]) && i < 3; i++) mi->vars[i] = cols[1 + i];
}

int world_door_model_quads(const Level *lv, const Mesh *m, const Door *d, const Object *o, Vertex *out, int cap) {
    if (!models_ready()) return 0;
    const Tile *t = &lv->tile[d->tz * 64 + d->tx];
    int k = d->item & 7, steps = o->flags & 7;
    bool portcullis = k == 6, secret = k == 7, moving = o->item == 0x1CF;
    float pos[3], wrect[4], ceil = level_ceil_y(), turn = (float)d->heading / 8.0f;
    world_object_pos(lv, d->tx, d->tz, o, pos);
    /* do_door: an open door's object stands 0x18 higher (OpenDoor's SET_Z + 0x18); the drawing takes it off again
     * (objyloc -= 0xC0), a moving door's always; a portcullis is raised by its steps instead */
    if (!portcullis && (steps || moving)) pos[1] -= 0.75f;
    wall_rect(lv, t, wrect);
    int n = 0;
    /* the frame: model 1, the wall's texture, its colour the texture's own; variable 3 the open steps (the portcullis
     * counts as open: its bars are the opening); the delta point reaches the ceiling from 0xD0 up */
    {
        /* a portcullis's object rises 0x30 a step as it opens (its leaf drawn there); do_door draws the frame at
         * oldz = objyloc - steps * 0x30, where the door stands shut, and its delta from that (h = 0x400 - oldz - 0xD0):
         * the frame drawn at the object's height lifted the whole wall with the bars (the user's save, 2026-11-13) */
        float fpos[3] = { pos[0], pos[1], pos[2] };
        if (portcullis) fpos[1] -= (float)steps * 0.1875f;
        ModelInst mi;
        inst_init(&mi, 1, fpos, turn);
        memcpy(mi.rect, wrect, 16);
        atlas_colour(m, wrect, mi.var0_rgb);
        mi.vars[3] = portcullis ? steps + 1 : (d->open > 0.001f ? 1 : 0);   /* do_door: Clk(3) = steps + 1 for a portcullis */
        mi.delta_y = ceil - fpos[1] - 0.8125f;
        /* do_door's texture height for the lintel: (cTmDm^2 >> 8) x h - 1, h = 0x400 - objyloc - 0xD0 (the way from the
           opening's top to the ceiling, 0x100 a tile): the texture runs h / 0x100 times down it, hung from the ceiling
           as the walls' are (it was the whole texture squeezed into the strip); BE restores the full height for the posts */
        mi.tex_vscale = mi.delta_y > 0.001f ? mi.delta_y : 0.001f;
        n += models_place(&mi, out + n, cap - n);
    }
    /* the leaf */
    {
        ModelInst mi;
        int model = portcullis ? 12 : secret ? 15 : 14;
        inst_init(&mi, model, pos, turn);
        mi.vars[3] = portcullis ? steps + 1 : (d->open > 0.001f ? 1 : 0);
        if (portcullis) {
            /* raised by its steps (0 to 4, USEITEMS.C's moveDoor): variable 5 the steps, the delta point the bars'
             * height left, 0xD0 less 0x30 a step. The steps are the object's own (do_door: OBJ_FLAGS & 7), which
             * EFFECT.C's do_animobj counts up a frame at a time as it raises the object 6 a frame: our own swing
             * timer (d->open) ran ahead of the object's height, so a rising portcullis was stubs at once, carried
             * up bare (the user, 2026-11-15: "when they go up the bars are not rendered") */
            int st = steps > 4 ? 4 : steps;
            mi.vars[5] = st;
            mi.delta_y = 0.8125f - 0.1875f * (float)st;
        } else {
            /* swung on its hinge by dir sixteenths of a turn: the steps (4 fully open) toward the side its door-dir
             * bit says, as do_door's Clk(5) = dir << 12 */
            int doordir = (o->flags >> 4) & 1;
            float dir = d->open * 4.0f * (doordir ? 1.0f : -1.0f);
            mi.vars[5] = (int)(dir * 4096.0f);
            if (secret) { memcpy(mi.rect, wrect, 16); atlas_colour(m, wrect, mi.var0_rgb); }
            else {
                int pic = k < 6 ? lv->doors[k] : -1;
                if (pic >= 0 && pic < 16 && m->door_w[pic]) mi.rect[0] = (float)(pic * 32), mi.rect[1] = DOOR_ROW, mi.rect[2] = 32, mi.rect[3] = 64;
                else memcpy(mi.rect, wrect, 16);
            }
        }
        n += models_place(&mi, out + n, cap - n);
    }
    return n;
}

static void floor_rect_of(int i, float r[4]) { r[0] = (float)(i % 32 * 32), r[1] = (float)(896 + i / 32 * 32), r[2] = r[3] = 32; }

int world_model_quads(const Level *lv, const Mesh *m, Vertex *out, int cap) {
    static const int rect_flagcol[2] = {0xE4, 0x66};
    int n = 0;
    if (!models_ready()) return 0;
    for (int t = 0; t < 64 * 64; t++)
        for (int i = lv->tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = lv->obj[i].next, guard++) {
            const Object *o = &lv->obj[i];
            /* an arrow or a bolt lying about or in flight (0x11, 0x12; COMOBJ.DAT render type 2): do_obj takes
             * item & 0x3F - 0x10 into rect_sub as for 0x151 / 0x152, model 8 (we drew the inventory's picture) */
            int arrow = o->item == 0x11 || o->item == 0x12;
            if ((!arrow && (o->item < 0x150 || o->item > 0x16F)) || ((o->flags >> 5) & 1)) continue;
            if (o->item == 0x165 || o->item == 0x166) continue;    /* gravestones and writings: our decals (world_decal_cell) */
            int model = arrow ? models_for_item(0x140 + o->item) : models_for_item(o->item);
            /* the special wall panels (0x16D, 0x16E, 0x16F; DRAWOBJ.C do_obj's type 3, not minor 3): model 0x16 with the
             * level's wall texture number `owner` (txtwal(0, sqmod, 4, owner); do_rect(0x16, o, -1, owner)). rect_sub
             * has no model for them, so we drew none: the archway pictures at the foot of the stairs between the
             * levels were missing (the user, 2026-11-15: "it teleports me to the second level but there is no door or
             * anything here"; level 1's 7,34 and 27,20, owner 47 = texture 137, the dark arch) */
            int panel = o->item >= 0x16D && o->item <= 0x16F;   /* (0x16D too: level 9's 112, render type 3) */
            if (panel) model = 0x16;
            if (model < 0) continue;
            const unsigned char *cols = models_cols(model);
            int oflags = o->flags & 0xF;
            float pos[3];
            world_object_pos(lv, t % 64, t / 64, o, pos);
            if (model == 16 || model == 17 || panel) {
                /* the tmap lever and switch plates (0x161, 0x162): one face on the wall's base plane, under our stones'
                 * relief (0.035), which showed it as slivers through the mortar (the user, 2026-11-13: level 1's levers
                 * at 56-59,45). Moved off the wall into the room as the decals are (world_decal_place's 0.045);
                 * presentation only */
                float a = o->heading * 0.785398f;
                pos[0] -= sinf(a) * 0.045f, pos[2] -= cosf(a) * 0.045f;
            }
            ModelInst mi;
            /* every model turned by the object's heading (DRAWOBJ.C do_rect, heading -1: head from OBJ_HEADING
             * whatever the flags; 0x40 adds only the pitch and fine heading of an untextured model). A bridge (flags
             * 0x11) drawn unturned sat an eighth of a tile off its tile (the user, 2026-11-13: level 1's high bridge) */
            inst_init(&mi, model, pos, (float)o->heading / 8.0f);
            if (cols[0] & MCOL_DELTA) mi.delta_y = level_ceil_y() - pos[1];
            if (panel) {
                int wt = lv->walls[o->owner % 48];
                mi.rect[0] = (float)(wt % 16 * 64), mi.rect[1] = (float)(wt / 16 * 64), mi.rect[2] = 64, mi.rect[3] = 64;
            } else if (cols[0] & MCOL_TEXTURED) {
                int first = cols[3], count = (first >> 5) + 1;
                if (model == 2 && oflags >= count) {
                    /* the bridge over water: the level's floor texture oflags - count (do_rect's txtflr) */
                    floor_rect_of(lv->floors[(oflags - count) % 10], mi.rect);
                } else {
                    int q = (first & 0x1F) + oflags % count, cell = TMOBJ_CELL + q;
                    if (q < 32 && cell < 512 && m->sprite_w[cell]) {
                        mi.rect[0] = (float)(cell % 32 * 32), mi.rect[1] = (float)(1024 + cell / 32 * 32);
                        mi.rect[2] = m->sprite_w[cell], mi.rect[3] = m->sprite_h[cell];
                    }
                }
                if (model == 2) mi.vars[11] = rect_flagcol[oflags & 1];
            }
            if ((cols[0] & MCOL_TEXCOL) && mi.rect[2] > 0) atlas_colour(m, mi.rect, mi.var0_rgb);
            n += models_place(&mi, out + n, cap - n);
            if (n + 3 > cap) return n;
        }
    return n;
}
