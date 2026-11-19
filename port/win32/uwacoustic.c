/* uwacoustic.c: casts our sound rays over the level's tile grid (see uwacoustic.h): a ray's first hit, the room's
   size and reverb time from a ball of rays, what stands between a sound and the listener (doors, things), and the
   way round rock a person would walk. Presentation only, never read by the rules. The shapes follow src\level.c: a
   diagonal tile's open half (open_at), a slope's floor (level_floor_y: one step up toward the side it is named
   after), the ceiling at 16 steps. */
#include <math.h>
#include <string.h>
#include "uwacoustic.h"

#define N 64
#define STEP 0.25f
#define CEIL 4.0f            /* CEIL_STEPS * STEP */
#define EPS 1e-4f
#define MAXDOORS 128
/* the dials, set by uwac_tune from settings.txt (through uwaudio.c): the room probe's rays, the legs each is followed
   for, how far a leg may go (tiles), and how far the way round a corner may be (tiles). Defaults: 64, 12, 16, 12. */
static int room_rays = 64, room_legs = 12;
static float room_max = 16.0f, way_max = 12.0f;

static UwaTile tiles[N * N];
static int have_grid, ceil_mat_, ndoors;
static float tile_m_ = 2.4f;
static UwaDoor doors[MAXDOORS];
static short door_at[N * N];   /* 1 + the first shut door of a tile, 0 none */
/* A door's frame: without it an open door would be a hole a whole tile wide and as high as the room, and two rooms
   joined by a door would ring as one. The original's frame is a slab across the middle of the tile with an opening
   half a tile wide and 13/16 high (src\world.c): the slab is always there for a ray, the leaf in its opening when
   the door is shut. frame_at: 1 + the first door of a tile, shut or open. */
static short frame_at[N * N];
static int door_frames = 1;
#define DOOR_LEFT 0.25f
#define DOOR_RIGHT 0.75f
#define DOOR_HIGH 0.8125f
static unsigned level_gen;     /* moves whenever the tiles or the doors are given again (the way round's cache) */
/* turns the door frames on or off */
void uwac_frames(int on) { if ((on != 0) != door_frames) door_frames = on != 0, level_gen++; }

/* sets the tile size in metres */
void uwac_tile_m(float tile_m) { tile_m_ = tile_m > 0.1f ? tile_m : 2.4f; }
/* sets the dials above, kept inside sane limits */
void uwac_tune(int rays, int legs, float reach, float way)
{
    room_rays = rays < 8 ? 8 : rays > 1024 ? 1024 : rays;
    room_legs = legs < 3 ? 3 : legs > 64 ? 64 : legs;       /* (three at least: the first two are not counted) */
    room_max = reach < 2.0f ? 2.0f : reach > 64.0f ? 64.0f : reach;
    way = way < 2.0f ? 2.0f : way > 64.0f ? 64.0f : way;
    if (way != way_max) way_max = way, level_gen++;         /* (the way round's field made again) */
}
/* gives the level's tile grid and its ceiling material, and sets the tile size in metres */
void uwac_grid(const UwaTile t[N * N], int ceil_mat, float tile_m)
{
    memcpy(tiles, t, sizeof tiles);
    ceil_mat_ = ceil_mat, have_grid = 1, level_gen++;
    uwac_tile_m(tile_m);
}
/* gives the level's doors, shut or open */
void uwac_doors(const UwaDoor *d, int n)
{
    memset(door_at, 0, sizeof door_at), memset(frame_at, 0, sizeof frame_at);
    ndoors = 0, level_gen++;
    for (int i = 0; d && i < n && ndoors < MAXDOORS; i++) {
        if (d[i].tx < 0 || d[i].tx >= N || d[i].tz < 0 || d[i].tz >= N) continue;
        doors[ndoors] = d[i];
        if (d[i].shut && !door_at[d[i].tz * N + d[i].tx]) door_at[d[i].tz * N + d[i].tx] = (short)(ndoors + 1);
        if (!frame_at[d[i].tz * N + d[i].tx]) frame_at[d[i].tz * N + d[i].tx] = (short)(ndoors + 1);
        ndoors++;
    }
}

/* The things standing in the level, so that sounds change with what is in the room: boxes round the original's 3D
   models, each of a material. box_hit: where a ray first enters one (BIG none; a ray that starts inside one passes
   out of it). */
#define MAXBOXES 256
static UwaBox boxes[MAXBOXES];
static int nboxes, boxes_on = 1;
/* gives the boxes of the things standing in the level */
void uwac_things(const UwaBox *b, int n)
{
    nboxes = 0;
    for (int i = 0; b && i < n && nboxes < MAXBOXES; i++) boxes[nboxes++] = b[i];
    level_gen++;
}
/* turns the boxes on or off */
void uwac_things_on(int on) { if ((on != 0) != boxes_on) boxes_on = on != 0, level_gen++; }
static float box_hit(const float o[3], const float d[3], int *mat, int *axis)
{
    float best = 1e30f;
    for (int i = 0; boxes_on && i < nboxes; i++) {
        float t0 = 0, t1 = 1e30f;
        int ax = -1, ok = 1;
        for (int k = 0; k < 3 && ok; k++) {
            if (fabsf(d[k]) < 1e-9f) { ok = o[k] >= boxes[i].lo[k] && o[k] <= boxes[i].hi[k]; continue; }
            float a = (boxes[i].lo[k] - o[k]) / d[k], b = (boxes[i].hi[k] - o[k]) / d[k];
            if (a > b) { float w = a; a = b, b = w; }
            if (a > t0) t0 = a, ax = k;
            if (b < t1) t1 = b;
            ok = t0 <= t1;
        }
        if (ok && ax >= 0 && t0 > 1e-4f && t0 < best) best = t0, *mat = boxes[i].mat, *axis = ax;
    }
    return best;
}
static const UwaTile *tile_at(int x, int z) { return x < 0 || x >= N || z < 0 || z >= N ? NULL : &tiles[z * N + x]; }

/* a diagonal tile's wall as a line: > 0 in its open half, < 0 in its rock half (lx, lz the place in the tile);
   1 everywhere for the other types (level.c's open_at) */
static float diag_side(const UwaTile *t, float lx, float lz)
{
    switch (t->type) {
    case UWAT_DIAG_SE: return lx - lz;
    case UWAT_DIAG_SW: return 1.0f - lx - lz;
    case UWAT_DIAG_NE: return lx + lz - 1.0f;
    case UWAT_DIAG_NW: return lz - lx;
    default: return 1.0f;
    }
}
/* the floor's y in a tile at a place in it (level.c's level_floor_y) */
static float floor_y(const UwaTile *t, float lx, float lz)
{
    float h = t->height;
    switch (t->type) {
    case UWAT_SLOPE_N: h += lz; break;
    case UWAT_SLOPE_S: h += 1.0f - lz; break;
    case UWAT_SLOPE_E: h += lx; break;
    case UWAT_SLOPE_W: h += 1.0f - lx; break;
    default: break;
    }
    return h * STEP;
}
/* whether the point is in open air of the grid (not rock, under the floor or over the ceiling) */
static int open_air(const float p[3])
{
    int tx = (int)floorf(p[0]), tz = (int)floorf(p[2]);
    const UwaTile *t = tile_at(tx, tz);
    if (!have_grid || !t || t->type == UWAT_SOLID) return 0;
    float lx = p[0] - (float)tx, lz = p[2] - (float)tz;
    return diag_side(t, lx, lz) >= -EPS && p[1] >= floor_y(t, lx, lz) - 1e-3f && p[1] <= CEIL + 1e-3f;
}

/* sets the hit's normal to (x, y, z) made unit length */
static void set_n(UwaHit *h, float x, float y, float z)
{
    float l = sqrtf(x * x + y * y + z * z);
    h->n[0] = x / l, h->n[1] = y / l, h->n[2] = z / l;
}
/* the floor's normal in a tile (a slope rises one step across it toward the side it is named after) */
static void floor_n(const UwaTile *t, UwaHit *h)
{
    float gx = t->type == UWAT_SLOPE_E ? STEP : t->type == UWAT_SLOPE_W ? -STEP : 0;
    float gz = t->type == UWAT_SLOPE_N ? STEP : t->type == UWAT_SLOPE_S ? -STEP : 0;
    set_n(h, -gx, 1, -gz);
}
/* a diagonal wall's normal: diag_side's gradient, toward the open half */
static void diag_n(const UwaTile *t, UwaHit *h)
{
    switch (t->type) {
    case UWAT_DIAG_SE: set_n(h, 1, 0, -1); break;
    case UWAT_DIAG_SW: set_n(h, -1, 0, -1); break;
    case UWAT_DIAG_NE: set_n(h, 1, 0, 1); break;
    default: set_n(h, -1, 0, 1); break;
    }
}

/* walks one ray tile by tile from o along dir for up to max: 1 hit (filled in), 0 nothing within max, -1 o is not in
   open air */
int uwac_ray(const float o[3], const float dir[3], float max, UwaHit *hit)
{
    float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (!open_air(o)) return -1;
    if (len < 1e-9f || max <= 0) return 0;
    const float dx = dir[0] / len, dy = dir[1] / len, dz = dir[2] / len;
    int tx = (int)floorf(o[0]), tz = (int)floorf(o[2]);
    const int sx = dx > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;
    const float BIG = 1e30f;
    float nx = fabsf(dx) > 1e-9f ? ((float)(tx + (dx > 0)) - o[0]) / dx : BIG, ax = fabsf(dx) > 1e-9f ? fabsf(1.0f / dx) : BIG;
    float nz = fabsf(dz) > 1e-9f ? ((float)(tz + (dz > 0)) - o[2]) / dz : BIG, az = fabsf(dz) > 1e-9f ? fabsf(1.0f / dz) : BIG;
    float t = 0;
    const float dn[3] = {dx, dy, dz};
    int box_mat = 0, box_axis = 0;
    const float t_box = box_hit(o, dn, &box_mat, &box_axis);   /* the first thing standing in the ray's way */
    for (int guard = 0; guard < 4 * N; guard++) {
        const UwaTile *T = tile_at(tx, tz);   /* never NULL here: the walk stops on leaving the grid */
        float t_out = nx < nz ? nx : nz, t1 = t_out < max ? t_out : max;
        float best = BIG;
        int kind = -1, mat = 0, door_plane = 0;   /* door_plane: the hit is on a door's slab (1: its plane is z, 2: x) */
#define LX(s) (o[0] + dx * (s) - (float)tx)
#define LZ(s) (o[2] + dz * (s) - (float)tz)
#define Y(s) (o[1] + dy * (s))
        /* the floor (a plane in the tile, so its height along the ray is a line) */
        float g0 = Y(t) - floor_y(T, LX(t), LZ(t)), g1 = Y(t1) - floor_y(T, LX(t1), LZ(t1));
        if (g1 < 0) {
            float s = g0 <= 0 ? t : t + (t1 - t) * g0 / (g0 - g1);
            if (s < best) best = s, kind = UWA_FLOOR, mat = T->floor_mat;
        }
        /* the ceiling */
        if (dy > 0 && Y(t1) > CEIL) {
            float s = (CEIL - o[1]) / dy;
            if (s < t) s = t;
            if (s < best) best = s, kind = UWA_CEILING, mat = ceil_mat_;
        }
        /* a diagonal tile's wall */
        if (T->type >= UWAT_DIAG_SE && T->type <= UWAT_DIAG_NW) {
            float f0 = diag_side(T, LX(t), LZ(t)), f1 = diag_side(T, LX(t1), LZ(t1));
            if (f1 < 0) {
                float s = f0 <= 0 ? t : t + (t1 - t) * f0 / (f0 - f1);
                if (s < best) best = s, kind = UWA_WALL, mat = T->wall_mat;
            }
        }
        /* a shut door across the middle of the tile */
        if (door_frames ? frame_at[tz * N + tx] : door_at[tz * N + tx]) {
            const UwaDoor *d = &doors[(door_frames ? frame_at[tz * N + tx] : door_at[tz * N + tx]) - 1];
            int span_x = (d->heading & 2) == 0;   /* heading 0 / 4: the door spans x, so its plane is z = tz + 0.5 */
            float den = span_x ? dz : dx, s = BIG;
            if (fabsf(den) > 1e-9f) s = span_x ? ((float)tz + 0.5f - o[2]) / dz : ((float)tx + 0.5f - o[0]) / dx;
            if (s > t + EPS && s <= t1 && s < best) {
                /* with frames: the slab round the opening is the wall's stone; the opening holds the leaf when shut */
                float u = span_x ? LX(s) : LZ(s), up = Y(s) - (float)T->height * STEP;
                int opening = !door_frames || (u >= DOOR_LEFT && u <= DOOR_RIGHT && up <= DOOR_HIGH);
                if (!opening) best = s, kind = UWA_WALL, mat = T->wall_mat, door_plane = span_x ? 1 : 2;
                else if (d->shut) best = s, kind = UWA_DOOR, mat = d->mat, door_plane = span_x ? 1 : 2;
            }
        }
        if (t_box <= t1 && t_box < best) {   /* a thing in this tile's stretch, before anything of the tile's own */
            if (hit) {
                hit->dist = t_box, hit->kind = UWA_WALL, hit->mat = box_mat;
                set_n(hit, box_axis == 0 ? (dx > 0 ? -1.0f : 1.0f) : 0, box_axis == 1 ? (dy > 0 ? -1.0f : 1.0f) : 0, box_axis == 2 ? (dz > 0 ? -1.0f : 1.0f) : 0);
            }
            return 1;
        }
        if (kind >= 0) {
            if (hit) {
                hit->dist = best, hit->kind = kind, hit->mat = mat;
                if (door_plane == 1) set_n(hit, 0, 0, dz > 0 ? -1.0f : 1.0f);
                else if (door_plane == 2) set_n(hit, dx > 0 ? -1.0f : 1.0f, 0, 0);
                else if (kind == UWA_FLOOR) floor_n(T, hit);
                else if (kind == UWA_CEILING) set_n(hit, 0, -1, 0);
                else diag_n(T, hit);
            }
            return 1;
        }
        if (t_out >= max) return 0;
        /* into the next tile: rock, the map's edge, a diagonal's rock half or a floor above the ray are a wall there */
        int wall_mat = T->wall_mat, stepx = nx < nz;
        if (stepx) tx += sx, nx += ax; else tz += sz, nz += az;
        t = t_out;
        const UwaTile *M = tile_at(tx, tz);
        int stop = !M || M->type == UWAT_SOLID;
        if (!stop) {
            float lx = LX(t), lz = LZ(t);
            lx = lx < 0 ? 0 : lx > 1 ? 1 : lx, lz = lz < 0 ? 0 : lz > 1 ? 1 : lz;
            wall_mat = M->wall_mat;
            stop = diag_side(M, lx, lz) < -EPS || Y(t) < floor_y(M, lx, lz) - EPS;
        } else if (M) wall_mat = M->wall_mat;
        if (stop) {
            if (hit) {
                hit->dist = t, hit->kind = UWA_WALL, hit->mat = wall_mat;
                if (stepx) set_n(hit, (float)-sx, 0, 0); else set_n(hit, 0, 0, (float)-sz);
            }
            return 1;
        }
#undef LX
#undef LZ
#undef Y
    }
    return 0;
}

#define ROOM_SKIP 2       /* legs not counted in the mean free path: those still near where the rays began */

/* a number in (0, 1) from two integers and a salt (lowbias32's mixing) */
static float hash01(unsigned a, unsigned b, unsigned salt)
{
    unsigned x = a * 0x9e3779b9u ^ (b + salt) * 0x85ebca6bu ^ salt;
    x ^= x >> 16, x *= 0x7feb352du, x ^= x >> 15, x *= 0x846ca68bu, x ^= x >> 16;
    return ((float)(x >> 8) + 0.5f) / 16777216.0f;
}
/* a direction off a surface of normal n by the cosine law (a rough wall's scattering), from u1, u2 in [0, 1) */
static void scatter(const float n[3], float u1, float u2, float d[3])
{
    float t[3], b[3], r = sqrtf(u1), phi = 6.28318531f * u2, c = sqrtf(1.0f - u1);
    float a[3] = {fabsf(n[0]) < 0.9f ? 1.0f : 0.0f, fabsf(n[0]) < 0.9f ? 0.0f : 1.0f, 0};
    t[0] = a[1] * n[2] - a[2] * n[1], t[1] = a[2] * n[0] - a[0] * n[2], t[2] = a[0] * n[1] - a[1] * n[0];
    float l = sqrtf(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
    t[0] /= l, t[1] /= l, t[2] /= l;
    b[0] = n[1] * t[2] - n[2] * t[1], b[1] = n[2] * t[0] - n[0] * t[2], b[2] = n[0] * t[1] - n[1] * t[0];
    for (int i = 0; i < 3; i++) d[i] = t[i] * r * cosf(phi) + b[i] * r * sinf(phi) + n[i] * c;
}

/* probes the room round a point with a ball of rays: the mean first-hit and free-path distances (metres) and a reverb
   time per band; 0 when the point is not in open air */
int uwac_room(const float at[3], UwaRoom *out)
{
    if (!open_air(at)) return 0;
    double first = 0, legs = 0, ab[3] = {0, 0, 0};
    int hits = 0, nlegs = 0, nsurf = 0;
    for (int i = 0; i < room_rays; i++) {
        /* a Fibonacci sphere: the same directions every time */
        float y = 1.0f - 2.0f * ((float)i + 0.5f) / (float)room_rays, r = sqrtf(1.0f - y * y), phi = (float)i * 2.39996323f;
        float d[3] = {cosf(phi) * r, y, sinf(phi) * r}, o[3] = {at[0], at[1], at[2]};
        for (int k = 0; k < room_legs; k++) {
            UwaHit h;
            const UwaMaterial *m = NULL;
            int got = uwac_ray(o, d, room_max, &h);
            if (got < 0) break;                       /* (a bounce that ended in a corner's crack: the ray stops there) */
            if (got == 1) m = uwacmat_get(h.mat);
            else h.dist = room_max;                   /* nothing within the reach (16 tiles): the sound left */
            if (k == 0) first += h.dist, hits += got;
            if (k >= ROOM_SKIP) legs += h.dist, nlegs++;
            for (int b = 0; b < 3; b++) ab[b] += m ? m->absorb[b] : 1.0;
            nsurf++;
            if (got != 1) break;
            /* on from just off the surface, scattered by a hash of the ray and the leg (the same every time). A plain
               sequence over i * 12 + k gives neighbouring rays nearly the same scatter leg by leg, and the mean free
               path comes out 16 % short in a closed box */
            float u1 = hash01((unsigned)i, (unsigned)k, 0x68e31da4u), u2 = hash01((unsigned)i, (unsigned)k, 0xb5297a4du);
            for (int c = 0; c < 3; c++) o[c] += d[c] * h.dist + h.n[c] * 2e-3f;
            scatter(h.n, u1, u2, d);
        }
    }
    if (out) {
        out->hits = hits, out->first_m = (float)(first / room_rays) * tile_m_;
        out->mfp_m = nlegs ? (float)(legs / nlegs) * tile_m_ : out->first_m;
        for (int b = 0; b < 3; b++) {
            double a = nsurf ? ab[b] / nsurf : 1.0, t = a > 1e-6 ? 0.161 * (out->mfp_m / 4.0) / a : 10.0;
            out->rt60[b] = (float)(t < 0.1 ? 0.1 : t > 10.0 ? 10.0 : t);
        }
    }
    return 1;
}

/* --- what stands between a sound and the listener --- */
#define PATH_NEAR 0.1f
#define PATH_SPREAD 0.3f
static const float PATH_FLOOR[3] = {0.15f, 0.10f, 0.03f};

/* one ray from `to` to p: its gain by band. A shut door in the source's own tile (stx, stz) does not count: a door's
   own creak, or a knock on it, is made on the door's plane and would be behind it from one side only. A material's
   transmit is the share of the sound's energy that passes (Steam Audio's meaning, whose numbers the table carries),
   so the gain on the sound itself is its square root. */
static void path_ray(const float to[3], const float p[3], int stx, int stz, float g[3])
{
    float d[3] = {p[0] - to[0], p[1] - to[1], p[2] - to[2]}, o[3] = {to[0], to[1], to[2]};
    float left = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - 2e-3f;   /* (short of p: a point against a wall is not behind it) */
    g[0] = g[1] = g[2] = 1.0f;
    for (int k = 0; k < 16 && left > 0; k++) {
        UwaHit h;
        int r = uwac_ray(o, d, left, &h);
        if (r == 0) return;
        const UwaMaterial *m = r == 1 && h.kind == UWA_DOOR ? uwacmat_get(h.mat) : NULL;
        float adv = h.dist + 2e-3f, len = left + 2e-3f;        /* on from just past the door */
        float hx = o[0] + d[0] / len * h.dist, hz = o[2] + d[2] / len * h.dist;
        int own = (int)floorf(hx) == stx && (int)floorf(hz) == stz;
        /* (the frame of the source's own door: the slab's stone round the opening is that door's plane
           too; a hit on the middle plane of the source's tile, where a door stands, is passed like its leaf) */
        int own_frame = r == 1 && h.kind == UWA_WALL && own && door_frames && stx >= 0 && stx < N && stz >= 0 && stz < N && frame_at[stz * N + stx] &&
                        (fabsf(hx - (float)stx - 0.5f) < 2e-3f || fabsf(hz - (float)stz - 0.5f) < 2e-3f);
        if (own_frame) {
            for (int i = 0; i < 3; i++) o[i] += d[i] / len * adv;
            d[0] = p[0] - o[0], d[1] = p[1] - o[1], d[2] = p[2] - o[2];
            left -= adv;
            continue;
        }
        if (!m) { g[0] = g[1] = g[2] = 0; return; }           /* rock, a floor, the ceiling (or a door of no material) */
        for (int b = 0; b < 3 && !own; b++) g[b] *= sqrtf(m->transmit[b]);
        for (int i = 0; i < 3; i++) o[i] += d[i] / len * adv;
        d[0] = p[0] - o[0], d[1] = p[1] - o[1], d[2] = p[2] - o[2];
        left -= adv;
    }
}

/* the gain per band of the sound from `from` heard at `to`, averaged over rays to a small cube round the source;
   the gain is 1 when there is no grid, the points are not in open air, or the two are closer than PATH_NEAR. Always returns 1 */
int uwac_path(const float from[3], const float to[3], float gain[3])
{
    float c[3] = {from[0], from[1], from[2]}, sum[3] = {0, 0, 0}, g[3];
    float dx = from[0] - to[0], dy = from[1] - to[1], dz = from[2] - to[2];
    int n = 0, tx = (int)floorf(c[0]), tz = (int)floorf(c[2]);
    const UwaTile *t = tile_at(tx, tz);
    gain[0] = gain[1] = gain[2] = 1.0f;
    if (!have_grid || !t || dx * dx + dy * dy + dz * dz < PATH_NEAR * PATH_NEAR) return 1;
    /* the source lifted off the floor it stands on (a thing's place is its foot: a ray to the floor itself would
       graze it), and kept under the ceiling */
    if (t->type != UWAT_SOLID) {
        float f = floor_y(t, c[0] - (float)tx, c[2] - (float)tz);
        if (c[1] < f + 0.3f) c[1] = f + 0.3f;
        if (c[1] > CEIL - 0.1f) c[1] = CEIL - 0.1f;
    }
    if (!open_air(c) || !open_air(to)) return 1;
    path_ray(to, c, tx, tz, g);
    for (int b = 0; b < 3; b++) sum[b] += g[b];
    n++;
    for (int k = 0; k < 8; k++) {
        float p[3] = {c[0] + (k & 1 ? PATH_SPREAD : -PATH_SPREAD), c[1] + (k & 2 ? PATH_SPREAD : -PATH_SPREAD), c[2] + (k & 4 ? PATH_SPREAD : -PATH_SPREAD)};
        if (!open_air(p)) continue;                            /* a corner of the cube in rock or under the floor: not a ray */
        path_ray(to, p, tx, tz, g);
        for (int b = 0; b < 3; b++) sum[b] += g[b];
        n++;
    }
    for (int b = 0; b < 3; b++) {
        gain[b] = sum[b] / (float)n;
        if (gain[b] < PATH_FLOOR[b]) gain[b] = PATH_FLOOR[b];
    }
    return 1;
}

/* a thing's place is its foot: lifted off the floor there and kept under the ceiling, as uwac_path does for its rays
   (a ray to the floor's own plane would graze it) */
void uwac_lift(float p[3])
{
    int tx = (int)floorf(p[0]), tz = (int)floorf(p[2]);
    const UwaTile *t = tile_at(tx, tz);
    if (!have_grid || !t || t->type == UWAT_SOLID) return;
    float f = floor_y(t, p[0] - (float)tx, p[2] - (float)tz);
    if (p[1] < f + 0.3f) p[1] = f + 0.3f;
    if (p[1] > CEIL - 0.1f) p[1] = CEIL - 0.1f;
}

/* --- the way round (a sound that rock hides from the listener, heard along the way a person would walk) --- */
#define WAY_STEP_UP 4          /* height steps: a floor this much above the one the sound comes from is a wall to it */
static float way_cost[N * N];  /* the walked tiles from the listener's tile to each (WAY_NONE: not reached) */
static short way_from[N * N];  /* the tile before it on the way from the listener (-1: the listener's own) */
static int way_tile = -1;      /* the listener's tile the two above are for */
static unsigned way_gen;       /* ... and the level's generation */
#define WAY_NONE 1e9f

/* may the sound pass from tile b into tile a (the listener's side)? Both open; not up a high step. */
static int way_open(int a, int b)
{
    return tiles[a].type != UWAT_SOLID && tiles[b].type != UWAT_SOLID && (int)tiles[a].height - (int)tiles[b].height <= WAY_STEP_UP;
}
/* the walked distance from the listener's tile to every tile within WAY_MAX (Dijkstra over a small heap), kept
   until the listener changes tile or the level changes. A shut door's tile is reached (a sound made in it leaves
   it) but not walked through. */
static void way_field(int ltx, int ltz)
{
    static int heap[N * N * 8];
    static float hkey[N * N * 8];
    int nh = 0, start = ltz * N + ltx;
    if (way_tile == start && way_gen == level_gen) return;
    way_tile = start, way_gen = level_gen;
    for (int i = 0; i < N * N; i++) way_cost[i] = WAY_NONE, way_from[i] = -1;
    way_cost[start] = 0;
    heap[nh] = start, hkey[nh++] = 0;
    while (nh) {
        /* the nearest tile out of the heap */
        int t = heap[0];
        float c = hkey[0];
        nh--;
        for (int i = 0, last = heap[nh]; ; ) {
            float lk = hkey[nh];
            int k = 2 * i + 1;
            if (k < nh && k + 1 < nh && hkey[k + 1] < hkey[k]) k++;
            if (k >= nh || hkey[k] >= lk) { heap[i] = last, hkey[i] = lk; break; }
            heap[i] = heap[k], hkey[i] = hkey[k], i = k;
        }
        if (c > way_cost[t]) continue;                       /* an older entry of a tile since reached sooner */
        if (door_at[t] && t != start) continue;              /* a shut door: reached, not passed */
        int x = t % N, z = t / N;
        for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, nz = z + dz;
                if ((!dx && !dz) || nx < 0 || nx >= N || nz < 0 || nz >= N) continue;
                int n = nz * N + nx;
                if (!way_open(t, n)) continue;
                if (dx && dz) {                              /* a diagonal step: both tiles beside it open and free of a shut door */
                    int s1 = z * N + nx, s2 = nz * N + x;
                    if (tiles[s1].type == UWAT_SOLID || tiles[s2].type == UWAT_SOLID || door_at[s1] || door_at[s2]) continue;
                }
                float nc = c + (dx && dz ? 1.41421356f : 1.0f);
                if (nc > way_max || nc >= way_cost[n]) continue;
                way_cost[n] = nc, way_from[n] = (short)t;
                int i = nh++;
                for (; i > 0 && hkey[(i - 1) / 2] > nc; i = (i - 1) / 2) heap[i] = heap[(i - 1) / 2], hkey[i] = hkey[(i - 1) / 2];
                heap[i] = n, hkey[i] = nc;
            }
    }
}
/* the two ends' tiles, the field made for the listener's: 1 when both are open tiles of the map */
static int way_ends(const float from[3], const float to[3], int *st, int *lt)
{
    int sx = (int)floorf(from[0]), sz = (int)floorf(from[2]), lx = (int)floorf(to[0]), lz = (int)floorf(to[2]);
    const UwaTile *a = tile_at(sx, sz), *b = tile_at(lx, lz);
    if (!have_grid || !a || !b || a->type == UWAT_SOLID || b->type == UWAT_SOLID) return 0;
    *st = sz * N + sx, *lt = lz * N + lx;
    way_field(lx, lz);
    return 1;
}

/* how much longer the walked way from `from` to `to` is than the straight line (tiles); -1 when none within reach */
float uwac_detour(const float from[3], const float to[3])
{
    int st, lt;
    if (!way_ends(from, to, &st, &lt) || way_cost[st] >= WAY_NONE) return -1.0f;
    float dx = (float)(st % N - lt % N), dz = (float)(st / N - lt / N), d = way_cost[st] - sqrtf(dx * dx + dz * dz);
    return d < 0 ? 0 : d;
}

/* the first point on the walked way, from the source toward the listener, that the listener can hear directly;
   0 when there is no way */
int uwac_waypoint(const float from[3], const float to[3], float out[3])
{
    int st, lt;
    if (!way_ends(from, to, &st, &lt) || way_cost[st] >= WAY_NONE || !open_air(to)) return 0;
    /* from the source back toward the listener: the first tile whose middle the listener sees */
    for (int t = st, guard = 0; t >= 0 && guard < N * N; t = way_from[t], guard++) {
        float p[3] = {(float)(t % N) + 0.5f, to[1], (float)(t / N) + 0.5f}, g[3];
        float f = floor_y(&tiles[t], 0.5f, 0.5f);
        if (p[1] < f + 0.3f) p[1] = f + 0.3f;
        if (p[1] > CEIL - 0.1f) p[1] = CEIL - 0.1f;
        if (t == lt) { out[0] = p[0], out[1] = p[1], out[2] = p[2]; return 1; }
        path_ray(to, p, st % N, st / N, g);
        if (g[1] > 0.99f) { out[0] = p[0], out[1] = p[1], out[2] = p[2]; return 1; }
    }
    return 0;
}

/* the path gain with the way round added where rock blocks the direct sound; 1 when the way round raised it */
int uwac_path_round(const float from[3], const float to[3], float gain[3])
{
    static const float K[3] = {1.0f, 0.8f, 0.5f};
    int raised = 0;
    uwac_path(from, to, gain);
    if (gain[1] >= 0.5f) return 0;
    float d = uwac_detour(from, to);
    if (d < 0 || d >= 4.0f) return 0;
    for (int b = 0; b < 3; b++) {
        float k = K[b] * (1.0f - d / 4.0f);
        if (k > gain[b]) gain[b] = k, raised = 1;
    }
    return raised;
}

/* what the floor under a point is made of (a preset index); -1 outside the map, in rock, or before a level */
int uwac_floor_mat(const float at[3])
{
    const UwaTile *t = tile_at((int)floorf(at[0]), (int)floorf(at[2]));
    return !have_grid || !t || t->type == UWAT_SOLID ? -1 : t->floor_mat;
}
