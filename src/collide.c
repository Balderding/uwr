/* collide.c: see collide.h. Adapted from underworld-exhumed's motion/COLLIDE.C (MIT, THIRD-PARTY-NOTICES.md), the
 * C that compiles to UW.EXE's segment 0x2529 byte for byte; proved against UW.EXE by tools/collide_verify.py. The
 * types are the original's (char signed, int 16 bits), kept where they change a result. */
#include "collide.h"

#include <string.h>

enum { TILE_SOLID, TILE_OPEN, TILE_DIAG_SE, TILE_DIAG_SW, TILE_DIAG_NE, TILE_DIAG_NW, TILE_SLOPE_N, TILE_SLOPE_S,
       TILE_SLOPE_E, TILE_SLOPE_W };

typedef struct {
    u8 tile, x, y;
    u16 flags;
} Pnt;

static const CollWorld *W;
static i16 tiles[9];  /* the 3 x 3 tiles' terrain words, 0x1111 unread */
static Pnt pnt[5];    /* the four corners of the footprint, then its centre */
static int centre;    /* the centre tile's index (centptr) */
static i8 firstsolve;
static MotionCalc *curP;
/* tile_off: the 3 x 3 tiles from the centre, rows of 64 (COLLIDE.C:104) */
static const i8 TILE_OFF[9] = {-65, -64, -63, -1, 0, 1, 63, 64, 65};
/* tile_walls (DS 0x1d8a, read from UW.EXE): which sides of each tile type are wall (event.h: 1 diagonal, 2 west,
 * 4 east, 8 south, 0x10 north, 0x20 a slope) */
static const u8 TILE_WALLS[16] = {0x1e, 0x00, 0x13, 0x15, 0x0b, 0x0d, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e};

void coll_world(const CollWorld *w) { W = w; }

static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
/* an object by its index (Obj_IntTMem: critdata + index below 256, objdata + index - 256 above) */
static u8 *obj_at(int index) { return index < 256 ? W->block + 0x4000 + index * 27 : W->block + 0x5b00 + (index - 256) * 8; }
static int obj_item(const u8 *o) { return le16(o) & 511; }
static int obj_major(const u8 *o) { return (le16(o) & 0x1c0) >> 6; }
static const u8 *com_of(int item) { return W->com + item * 11; }

/* TILE_TERR: type + height << 4 + the floor's terrain byte << 4 (UW1: the whole byte) */
static i16 tile_terr(int t) {
    u16 w0 = le16(W->block + (t & 4095) * 4);
    int type = w0 & 15, height = (w0 >> 4) & 15, floor = (w0 >> 10) & 15;
    return (i16)(type + (height << 4) + ((W->terr[floor % 10] & 0xff) << 4));
}

u8 coll_get_hgt(int n, i8 *steep) { /* 0x28494 */
    u8 h = (u8)((tiles[pnt[n].tile] & 0xf0) >> 1);
    *steep = 0;
    switch (tiles[pnt[n].tile] & 0xf) {
    case TILE_SOLID: h = 0x80; break;
    case TILE_DIAG_SE:
        if (pnt[n].y >= pnt[n].x) h = 0x80;
        *steep = 1;
        break;
    case TILE_DIAG_SW:
        if (pnt[n].x + pnt[n].y >= 7) h = 0x80;
        *steep = 1;
        break;
    case TILE_DIAG_NE:
        if (pnt[n].x + pnt[n].y <= 7) h = 0x80;
        *steep = 1;
        break;
    case TILE_DIAG_NW:
        if (pnt[n].y <= pnt[n].x) h = 0x80;
        *steep = 1;
        break;
    case TILE_SLOPE_N: h = (u8)(h + (pnt[n].y & 7)); break;
    case TILE_SLOPE_S: h = (u8)(h + (7 - (pnt[n].y & 7))); break;
    case TILE_SLOPE_E: h = (u8)(h + (pnt[n].x & 7)); break;
    case TILE_SLOPE_W: h = (u8)(h + (7 - (pnt[n].x & 7))); break;
    }
    return h;
}

int coll_get_slope_hgt(int x, int y) { /* 0x28635 */
    int h = 0;
    y &= 0xff, x &= 0xff;
    switch (tiles[4] & 0xf) {
    case TILE_SLOPE_N: h = y; break;
    case TILE_SLOPE_S: h = 0xff - y; break;
    case TILE_SLOPE_E: h = x; break;
    case TILE_SLOPE_W: h = 0xff - x; break;
    }
    h >>= 2;
    h += (tiles[4] & 0xf0) << 2;
    return (i16)h;
}

static i8 solve_pnt(int n, u8 range) { /* SolvePnt 0x2869d */
    i8 steep;
    u16 flags = 0;
    u8 h = coll_get_hgt(n, &steep);
    if (h == 0x80) flags |= 0x200;
    else if (curP->z + range < h) flags |= 0x100;
    else if (curP->z - range > h) flags |= 0x800;
    else flags |= (u16)(8 << ((tiles[pnt[n].tile] & 0x300) >> 8));
    pnt[n].flags = flags;
    if (curP->top < h) curP->top = h;
    return (i8)!steep;
}

static u8 solve_center(u8 range) { /* SolveCenter 0x28754 */
    i8 steep;
    curP->hits0 = (i16)((tiles[4] & 0x300) >> 8);
    curP->floor = coll_get_hgt(4, &steep);
    if (curP->floor == 0x80) curP->hits0 |= 0x200;
    else if (curP->z + range < curP->floor) curP->hits0 |= 0x100;
    else if (curP->z - range > curP->floor) curP->hits0 |= 0x800;
    else {
        curP->hits0 |= 4;
        curP->hits0 |= (i16)(8 << ((tiles[4] & 0x300) >> 8));
    }
    if ((tiles[4] & 0xf) >= TILE_SLOPE_N) curP->hits0 |= 0x2000;
    return (u8)!steep;
}

void coll_terrain_check(MotionCalc *c, u8 range) { /* 0x28809 */
    curP = c;
    memset(tiles, 0x11, sizeof tiles);
    centre = ((c->x >> 3) & 63) + ((c->y >> 3) & 63) * 64; /* Map_GetAddr(x >> 3, y >> 3) */
    int x = c->x & 7, y = c->y & 7;
    i8 off = 4;
    pnt[4].tile = (u8)off, pnt[4].x = (u8)x, pnt[4].y = (u8)y;
    if (tiles[4] == 0x1111) tiles[4] = tile_terr(centre);
    solve_center(range);
    c->hits1 = c->hits0;
    c->top = c->floor;
    if (c->radius == 0) return;
    for (y -= c->radius; y < 0; y += 8) off = (i8)(off - 3);
    for (x -= c->radius; x < 0; x += 8) off -= 1;
    pnt[0].tile = (u8)off, pnt[0].x = (u8)x, pnt[0].y = (u8)y;
    for (x += c->radius * 2; x > 7; x -= 8) off = (i8)(off + 1);
    pnt[1].tile = (u8)off, pnt[1].x = (u8)x, pnt[1].y = (u8)y;
    for (y += c->radius * 2; y > 7; y -= 8) off = (i8)(off + 3);
    pnt[2].tile = (u8)off, pnt[2].x = (u8)x, pnt[2].y = (u8)y;
    for (x -= c->radius * 2; x < 0; x += 8) off -= 1;
    pnt[3].tile = (u8)off, pnt[3].x = (u8)x, pnt[3].y = (u8)y;
    for (int k = 0; k < 4; k++)
        if (tiles[pnt[k].tile] == 0x1111) tiles[pnt[k].tile] = tile_terr(centre + TILE_OFF[pnt[k].tile]);
    firstsolve = 1;
    for (i8 i = 0; i < 4; i++) {
        if (!solve_pnt(i, range) && !(pnt[i].flags & 0x300)) {
            static const u8 WALLS[5] = {4, 0x10, 2, 8, 4};
            for (i8 j = 0; j < 2; j++)
                if (pnt[(i + (j << 1) + 1) & 3].tile != pnt[i].tile && (TILE_WALLS[tiles[pnt[i].tile] & 0xf] & WALLS[j + i])) {
                    pnt[i].flags = 0x200;
                    c->top = 0x80;
                    j = 2;
                }
            firstsolve = 0;
        }
    }
    c->hits1 = (i16)(c->hits1 | pnt[0].flags | pnt[1].flags | pnt[2].flags | pnt[3].flags);
}

void coll_compute_heading(MotionCalc *c) { /* 0x28c86 */
    static const i8 SIDE[4] = {1, -1, -1, 1};
    static const u8 DIRS[3][3] = {{5, 4, 3}, {6, 9, 2}, {7, 0, 1}};
    i8 ox = 0, oy = 0, sx = 0, sy = 0;
    int nopen = 0, nsteep = 0;
    for (i8 i = 0; i < 4; i++) {
        if ((pnt[i].flags & 0xf8) == 0) oy = (i8)(oy + SIDE[i]), ox = (i8)(ox + SIDE[(i + 3) & 3]), nopen++;
        if (pnt[i].flags & 0x300) sx = (i8)(sx + SIDE[i]), sy = (i8)(sy + SIDE[(i + 3) & 3]), nsteep++;
    }
    if (nsteep) {
        c->slope = DIRS[sx / nsteep + 1][sy / nsteep + 1];
        if (nsteep == 1 && c->slope % 2 && firstsolve) {
            u8 want = (u8)((9 - c->slope) & 7), head = (u8)(c->heading >> 13), k, a = 0, b = 0;
            switch ((head - want) & 7) {
            case 0:
            case 1: c->slope = 9; break;
            case 2:
            case 3: c->slope = (u8)((c->slope + 1) & 7); break;
            case 4:
            case 5:
                k = (u8)((c->slope - 1) >> 1);
                c->slope--;
                switch (k) {
                case 0: a = pnt[k].x, b = pnt[k].y; break;
                case 1: a = pnt[k].x, b = (u8)(8 - pnt[k].y); break;
                case 2: a = pnt[k].y, b = pnt[k].x; break;
                case 3: a = (u8)(8 - pnt[k].x), b = pnt[k].y; break;
                }
                if (a < b) c->slope = (u8)((c->slope + 2) & 7);
                if (a == b) c->slope++;
                break;
            case 6:
            case 7: c->slope = (u8)((c->slope + 7) & 7); break;
            }
        }
    } else c->slope = 9;
    if (nopen == 1 || nopen == 2) c->open = DIRS[oy / -nopen + 1][ox / -nopen + 1];
    else c->open = 9;
}

/* --- the objects in the way ----------------------------------------------------------------------------------- */
Collision coll_objects[9];
i16 coll_place_height, coll_place_terrain;
static i8 xlow, xhigh, ypos, ylow, yhigh, pos_x; /* the mover's square within its tile (char: signed) */

/* obj_coll_check (0x28ef0): obj (link index), in tile x, y from the centre, into coll_objects when its square (a
 * whole tile for radius 4) overlaps the mover's; creatures shrink each other's radius by one */
static void obj_coll_check(const u8 *obj, int link, i8 x, i8 y, i8 isnpc) {
    if (curP->found > 8) return;
    const u8 *com = com_of(obj_item(obj));
    u8 radius = com[1] & 7, height = com[0];
    i8 x0, y0, x1, y1;
    u16 pos = le16(obj + 2);
    if (radius == 4) x1 = (i8)((x0 = (i8)(x << 3)) + 7), y1 = (i8)((y0 = (i8)(y << 3)) + 7);
    else {
        x0 = (i8)((x << 3) + ((pos >> 13) & 7));
        y0 = (i8)((y << 3) + ((pos >> 10) & 7));
        int r = radius;
        if (obj_major(obj) == 1 && r > 0 && isnpc) r--;
        x1 = (i8)(x0 + r), y1 = (i8)(y0 + r), x0 = (i8)(x0 - r), y0 = (i8)(y0 - r);
    }
    if (x1 < xlow || x0 > xhigh || y1 < ylow || y0 > yhigh) return;
    Collision *c = &coll_objects[curP->found++];
    c->bottom = (u8)(pos & 0x7f);
    c->top = (u8)(c->bottom + height);
    if (height == 0) c->top = (u8)(c->top + 1);
    c->link = (u16)(link << 6 | 9);
    if (pos_x >= x0 && pos_x <= x1 && ypos >= y0 && ypos <= y1) c->link |= 0x10;
    c->offset = (i16)(x + (y << 6));
}

void coll_object_check(MotionCalc *c, u8 flat, u8 useflag) { /* 0x29086 */
    curP = c;
    i8 isnpc = 0, radius;
    int tile = (c->x >> 3) + (c->y >> 3) * 64; /* Map_GetAddr */
    if (c->index != 0) isnpc = obj_major(obj_at(c->index)) == 1;
    c->found = 0;
    pos_x = (i8)(c->x & 7), ypos = (i8)(c->y & 7);
    if (flat && c->height == 0) radius = 0;
    else {
        radius = (i8)c->radius;
        if (isnpc && radius > 0) radius--;
    }
    xhigh = (i8)(pos_x + radius), yhigh = (i8)(ypos + radius), xlow = (i8)(pos_x - radius), ylow = (i8)(ypos - radius);
    i8 x0 = (i8)((xlow - 11) / 8), y0 = (i8)((ylow - 11) / 8), x1 = (i8)((xhigh + 4) / 8), y1 = (i8)((yhigh + 4) / 8);
    for (i8 i = x0; i <= x1; i++)
        for (i8 j = y0; j <= y1; j++) {
            int n = 0, off = (j << 6) + i;
            /* the tile's list: its head in word 1, each object's next in its word 2 (both from bit 6) */
            for (int link = le16(W->block + (tile + off) * 4 + 2) >> 6; link != 0 && n < 0x40; link = le16(obj_at(link) + 4) >> 6, n++) {
                if (link == c->index) continue;
                const u8 *obj = obj_at(link), *com = com_of(obj_item(obj));
                if (isnpc && (com[3] & 4)) continue;                                /* c3_2 */
                if (com[0] == 0 && link >= 256) continue;                           /* flat static */
                if (link < 256 && obj_major(obj) != 1 && (obj[0x15] & 0x80)) continue; /* b15 bit 7 */
                if (!(i8)useflag || (com[6] & 1)) obj_coll_check(obj, link, i, j, isnpc);
            }
            if (n == 0x40) return;
        }
}

static void ocswap(u8 i) { /* oCswap 0x292ea */
    Collision t = coll_objects[i];
    coll_objects[i] = coll_objects[i + 1];
    coll_objects[i + 1] = t;
}

void coll_process_objlist(MotionCalc *c) { /* 0x2934f */
    i8 k, m, n, flat = 0;
    if (c->height == 0) flat = 1;
    for (n = 0; c->found > n; n++) {
        for (k = (i8)(c->found - 2); k >= n; k--)
            if (coll_objects[k].top > coll_objects[k + 1].top) ocswap((u8)k);
        if (coll_objects[n].top > c->z) break;
    }
    for (k = n; c->found > k; k++)
        for (m = (i8)(c->found - 2); m >= k; m--)
            if (coll_objects[m].bottom > coll_objects[m + 1].bottom) ocswap((u8)m);
    c->first = n;
    for (c->count = 0; n + c->count < c->found && c->z + c->height + flat > coll_objects[n + c->count].bottom; c->count++)
        ;
}

u8 coll_can_place(int item, int index, int x, int y, int z, u8 flier, u8 range) { /* 0x29498 */
    MotionCalc *old = curP, calc;
    memset(&calc, 0, sizeof calc);
    MotionCalc *c = &calc;
    u8 ok;
    c->index = (i16)index;
    c->radius = com_of(item)[1] & 7;
    c->height = com_of(item)[0];
    c->x = (i16)x, c->y = (i16)y, c->z = (i16)z;
    if (c->height != 0x80 && c->height + c->z > 0x7f) {
        ok = 0;
        goto out;
    }
    coll_terrain_check(c, range);
    if ((c->hits0 | c->hits1) & 0x300) {
        ok = 0;
        goto out;
    }
    coll_place_height = c->z + range >= c->top ? c->top : c->floor;
    int radius = c->radius;
    if (range > radius) radius = range;
    if (c->z <= c->floor + radius) coll_place_terrain = (i16)(1 << (c->hits0 & 3));
    else coll_place_terrain = 0x10; /* FOOT_AIR */
    coll_object_check(c, (u8)(coll_place_terrain != 0x10 && index >= 256), 1);
    if (c->found != 0) {
        int best = -1;
        coll_process_objlist(c);
        if (c->count != 0) {
            ok = 0;
            goto out;
        }
        if (c->found != 0)
            for (int i = 0; i < c->first; i++)
                if (coll_objects[i].top > coll_place_height) coll_place_height = coll_objects[best = i].top;
        if (best > -1) {
            if (!(com_of(obj_item(obj_at(coll_objects[best].link >> 6)))[3] & 2)) { /* solid */
                ok = 0;
                goto out;
            }
            coll_place_terrain = 1; /* FOOT_FLOOR */
        }
    }
    if (!(i8)flier && ((c->hits0 | c->hits1) & 0x800) && c->z - range > coll_place_height) ok = 0;
    else ok = 1;
out:
    curP = old;
    return ok;
}
