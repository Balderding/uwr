#include "level.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- reading the text ------------------------------------------------------------------------- */
static long num(const char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
    char *e;
    long v = strtol(*p, &e, 10);
    *p = e;
    return v;
}
static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

static char g_dir[600];
void level_set_dir(const char *dir) { str_copy(g_dir, sizeof g_dir, dir ? dir : ""); }
const char *level_dir(void) { return g_dir; }

bool level_save(const Level *lv, const char *dir) {
    char path[700];
    snprintf(path, sizeof path, "%s\\level%d.txt", dir, lv->number);
    size_t cap = 600 * 1024, n = 0, mark = arena_mark();
    char *out = arena_alloc(cap);
    n += (size_t)snprintf(out + n, cap - n, "# Ultima Underworld level %d as the player left it (uo-new)\r\nwalls", lv->number);
    for (int i = 0; i < 48; i++) n += (size_t)snprintf(out + n, cap - n, " %d", lv->walls[i]);
    n += (size_t)snprintf(out + n, cap - n, "\r\nfloors");
    for (int i = 0; i < 10; i++) n += (size_t)snprintf(out + n, cap - n, " %d", lv->floors[i]);
    n += (size_t)snprintf(out + n, cap - n, "\r\ndoors");
    for (int i = 0; i < 6; i++) n += (size_t)snprintf(out + n, cap - n, " %d", lv->doors[i]);
    n += (size_t)snprintf(out + n, cap - n, "\r\n");
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            const Tile *t = &lv->tile[y * 64 + x];
            n += (size_t)snprintf(out + n, cap - n, "tile %d %d %d %d %d %d %d %d %d %d\r\n", x, y, t->type, t->height, t->floor, t->wall, t->door,
                                  t->nomagic, t->unknown8, t->first_object);
        }
    for (int i = 0; i < 1024 && n + 128 < cap; i++) {
        const Object *o = &lv->obj[i];
        n += (size_t)snprintf(out + n, cap - n, "object %d %d %d %d %d %d %d %d %d %d %d", i, o->item, o->flags, o->z, o->heading, o->y, o->x,
                              o->quality, o->next, o->owner, o->link);
        if (i < 256) {
            n += (size_t)snprintf(out + n, cap - n, " ");
            for (int k = 0; k < 19; k++) n += (size_t)snprintf(out + n, cap - n, "%02x", o->mobile[k]);
        }
        n += (size_t)snprintf(out + n, cap - n, "\r\n");
    }
    make_dirs_for(path);
    bool ok = file_write(path, out, n);
    arena_reset(mark);
    return ok;
}

bool level_load(Level *lv, const char *data_dir, int number) {
    memset(lv, 0, sizeof *lv);
    lv->number = number;
    char path[700];
    snprintf(path, sizeof path, "%s\\level%d.txt", g_dir, number);
    if (!g_dir[0] || !file_exists(path)) snprintf(path, sizeof path, "%s\\levels\\level%d.txt", data_dir, number);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    if (!f.size) return logf_("level: cannot read %s\n", path), false;
    int tiles = 0;
    const char *s = (const char *)f.data, *end = s + f.size;
    while (s < end) {
        const char *eol = memchr(s, '\n', (size_t)(end - s));
        if (!eol) eol = end;
        const char *p = s;
        while (p < eol && (*p == ' ' || *p == '\t')) p++;
        if (!strncmp(p, "walls ", 6)) {
            p += 5;
            for (int i = 0; i < 48; i++) lv->walls[i] = (u16)num(&p);
        } else if (!strncmp(p, "floors ", 7)) {
            p += 6;
            for (int i = 0; i < 10; i++) lv->floors[i] = (u16)num(&p);
        } else if (!strncmp(p, "doors ", 6)) {
            p += 5;
            for (int i = 0; i < 6; i++) lv->doors[i] = (u8)num(&p);
        } else if (!strncmp(p, "tile ", 5)) {
            p += 4;
            long x = num(&p), y = num(&p);
            if (x >= 0 && x < 64 && y >= 0 && y < 64) {
                Tile *t = &lv->tile[y * 64 + x];
                t->type = (u8)num(&p), t->height = (u8)num(&p), t->floor = (u8)num(&p), t->wall = (u8)num(&p);
                t->door = (u8)num(&p), t->nomagic = (u8)num(&p), t->unknown8 = (u8)num(&p), t->first_object = (u16)num(&p);
                if (t->type > 9 || t->height > 15 || t->floor > 15 || t->wall > 63) {
                    logf_("level: %s: tile %ld %ld has a value out of range\n", path, x, y);
                    return arena_reset(mark), false;
                }
                tiles++;
            }
        } else if (!strncmp(p, "object ", 7)) {
            p += 6;
            long i = num(&p);
            if (i >= 0 && i < 1024) {
                Object *o = &lv->obj[i];
                o->item = (u16)num(&p), o->flags = (u8)num(&p), o->z = (u8)num(&p), o->heading = (u8)num(&p);
                o->y = (u8)num(&p), o->x = (u8)num(&p), o->quality = (u8)num(&p), o->next = (u16)num(&p);
                o->owner = (u8)num(&p), o->link = (u16)num(&p);
                if (i < 256) {
                    while (p < eol && *p == ' ') p++;
                    for (int k = 0; k < 19 && p + 1 < eol; k++, p += 2) o->mobile[k] = (u8)(hexval(p[0]) << 4 | hexval(p[1]));
                }
            }
        }
        s = eol + 1;
    }
    arena_reset(mark);
    if (tiles != 4096) return logf_("level: %s has %d tiles, not 4096\n", path, tiles), false;
    return true;
}

/* --- shape of a tile ------------------------------------------------------------------------------ */
/* corners: 0 SW (x, z), 1 SE (x+1, z), 2 NE (x+1, z+1), 3 NW (x, z+1) */
static const float CX[4] = {0, 1, 1, 0}, CZ[4] = {0, 0, 1, 1};

static const Tile *tile_at(const Level *lv, int x, int z) {
    if (x < 0 || x > 63 || z < 0 || z > 63) return NULL;
    return &lv->tile[z * 64 + x];
}

/* floor height of a corner, in steps (slopes rise one step toward the side they are named after;
 * unconfirmed) */
static float corner_h(const Tile *t, int c) {
    float h = t->height;
    switch (t->type) {
    case T_SLOPE_N: return h + (c == 2 || c == 3);
    case T_SLOPE_S: return h + (c == 0 || c == 1);
    case T_SLOPE_E: return h + (c == 1 || c == 2);
    case T_SLOPE_W: return h + (c == 0 || c == 3);
    default: return h;
    }
}

/* edges: 0 S (SW-SE), 1 E (SE-NE), 2 N (NE-NW), 3 W (NW-SW); does the tile's open part reach the edge */
static bool open_on_edge(const Tile *t, int e) {
    if (!t || t->type == T_SOLID) return false;
    switch (t->type) {
    case T_DIAG_SE: return e == 0 || e == 1;
    case T_DIAG_SW: return e == 0 || e == 3;
    case T_DIAG_NE: return e == 2 || e == 1;
    case T_DIAG_NW: return e == 2 || e == 3;
    default: return true;
    }
}

/* is the local point (0..1, 0..1) in the tile's open part */
static bool open_at(const Tile *t, float lx, float lz) {
    switch (t->type) {
    case T_SOLID: return false;
    case T_DIAG_SE: return lz <= lx;
    case T_DIAG_SW: return lx + lz <= 1;
    case T_DIAG_NE: return lx + lz >= 1;
    case T_DIAG_NW: return lz >= lx;
    default: return true;
    }
}

float level_ceil_y(void) { return CEIL_STEPS * STEP; }

bool level_solid_at(const Level *lv, float x, float z) {
    int tx = (int)floorf(x), tz = (int)floorf(z);
    const Tile *t = tile_at(lv, tx, tz);
    return !t || !open_at(t, x - (float)tx, z - (float)tz);
}

float level_floor_y(const Level *lv, float x, float z) {
    int tx = (int)floorf(x), tz = (int)floorf(z);
    const Tile *t = tile_at(lv, tx, tz);
    if (!t || t->type == T_SOLID) return level_ceil_y();
    float lx = x - (float)tx, lz = z - (float)tz;
    float h = t->height;
    switch (t->type) {
    case T_SLOPE_N: h += lz; break;
    case T_SLOPE_S: h += 1 - lz; break;
    case T_SLOPE_E: h += lx; break;
    case T_SLOPE_W: h += 1 - lx; break;
    }
    return h * STEP;
}

/* --- building the triangles -------------------------------------------------------------------- */
typedef struct {
    float p[4][3], uv[4][2], rect[4], nrm[3];
    int n, tex;      /* corners; the texture: 0-209 walls, 210 + floor number */
    u8 taper[4];     /* edge k (corner k to k+1): the relief eases to the base there (no face carries it on) */
} Face;

typedef struct {
    Vertex *v;
    int n, cap;
    u32 *idx;
    int ni, icap;
    Face *f;
    int nf, fcap;
} Build;

/* what is underfoot on each floor texture (data\levels\terrain.txt, from the game's TERRAIN.DAT): 0 floor, 1 water,
 * 2 lava; the vertices of such floors carry it in their colour's alpha (1.2 water, 1.3 lava: the shaders' material) */
static u8 terr[64];
static float face_kind = 1;
static void terrain_load(const char *data_dir) {
    char path[700], line[128];
    memset(terr, 0, sizeof terr);
    snprintf(path, sizeof path, "%s\\levels\\terrain.txt", data_dir);
    size_t mark = arena_mark();
    Bytes f = file_read(path);   /* (the disk's, or the one kept in memory) */
    for (size_t at = 0; at < f.size;) {
        size_t n = 0;
        while (at < f.size && f.data[at] != '\n') {
            if (n + 1 < sizeof line) line[n++] = (char)f.data[at];
            at++;
        }
        at++, line[n] = 0;
        int t;
        char w[16];
        if (sscanf(line, "floor %d %15s", &t, w) == 2 && t >= 0 && t < 64) terr[t] = (u8)(w[0] == 'w' ? 1 : w[0] == 'l' ? 2 : 0);
    }
    arena_reset(mark);
}
static int tex_terrain(int tex) { return tex >= 210 && tex - 210 < 64 ? terr[tex - 210] : 0; }

static int add_vertex(Build *b, const float p[3], const float uv[2], const float rect[4], const float nrm[3]) {
    if (b->n >= b->cap) return -1;
    Vertex *o = &b->v[b->n];
    memset(o, 0, sizeof *o);
    memcpy(o->pos, p, 12);
    memcpy(o->prev, p, 12);
    memcpy(o->uv, uv, 8);
    memcpy(o->rect, rect, 16);
    memcpy(o->nrm, nrm, 12);
    o->color[0] = o->color[1] = o->color[2] = 1, o->color[3] = face_kind;
    return b->n++;
}
static void add_tri(Build *b, int i0, int i1, int i2) {
    if (i0 < 0 || i1 < 0 || i2 < 0 || b->ni + 3 > b->icap) return;
    b->idx[b->ni++] = (u32)i0, b->idx[b->ni++] = (u32)i1, b->idx[b->ni++] = (u32)i2;
}
static void tri(Build *b, const float p[3][3], const float uv[3][2], const float rect[4], const float nrm[3]) {
    int i[3];
    for (int k = 0; k < 3; k++) i[k] = add_vertex(b, p[k], uv[k], rect, nrm);
    add_tri(b, i[0], i[1], i[2]);
}

/* a face of the level before it is made into triangles: a flat convex polygon with its texture coordinates */
static void add_face(Build *b, const float (*p)[3], const float (*uv)[2], int n, const float rect[4], const float nrm[3], int tex) {
    if (b->nf >= b->fcap) return;
    Face *f = &b->f[b->nf++];
    memset(f, 0, sizeof *f);
    for (int k = 0; k < n; k++) memcpy(f->p[k], p[k], 12), memcpy(f->uv[k], uv[k], 8);
    f->n = n, f->tex = tex;
    memcpy(f->rect, rect, 16), memcpy(f->nrm, nrm, 12);
}

/* atlas places: walls 64 px, 16 a row from the top; floors 32 px, 32 a row below them */
static void wall_rect(int i, float r[4]) { r[0] = (float)(i % 16 * 64), r[1] = (float)(i / 16 * 64), r[2] = r[3] = 64; }
static void floor_rect(int i, float r[4]) { r[0] = (float)(i % 32 * 32), r[1] = (float)(896 + i / 32 * 32), r[2] = r[3] = 32; }
void level_texture_rect(int i, float r[4]) { if (i < 210) wall_rect(i, r); else floor_rect(i - 210, r); }

/* a vertical quad along the edge from a to c (x, z), bottom heights bot0/bot1, tops top0/top1 (world y);
 * ua, uc: the texture's u at a and c (it must grow to the right as seen from the side the wall faces);
 * vlim: the texture's v along the bottom edge, in texels (v is 0 along the top edge) */
static void wall_strip(Build *b, const float a[2], const float c[2], float bot0, float bot1, float top0, float top1, const float rect[4],
                       const float nrm[3], float ua, float uc, int tex, float vlim) {
    float p[4][3] = {{a[0], bot0, a[1]}, {c[0], bot1, c[1]}, {c[0], top1, c[1]}, {a[0], top0, a[1]}};
    float uv[4][2] = {{ua, vlim}, {uc, vlim}, {uc, 0}, {ua, 0}};
    /* a wall whose top and bottom meet (a riser that comes to nothing at one end) is a triangle */
    if (top0 - bot0 < 1e-4f) {
        float q[3][3], w[3][2];
        for (int k = 0; k < 3; k++) memcpy(q[k], p[k + 1], 12), memcpy(w[k], uv[k + 1], 8);
        add_face(b, (const float(*)[3])q, (const float(*)[2])w, 3, rect, nrm, tex);
    } else if (top1 - bot1 < 1e-4f) {
        float q[3][3] = {{p[0][0], p[0][1], p[0][2]}, {p[1][0], p[1][1], p[1][2]}, {p[3][0], p[3][1], p[3][2]}};
        float w[3][2] = {{uv[0][0], uv[0][1]}, {uv[1][0], uv[1][1]}, {uv[3][0], uv[3][1]}};
        add_face(b, (const float(*)[3])q, (const float(*)[2])w, 3, rect, nrm, tex);
    } else add_face(b, (const float(*)[3])p, (const float(*)[2])uv, 4, rect, nrm, tex);
}

/* The texture as the original maps a face (GRIDDB.C's txtwal with TMAPOPS.ASM's do_compact_wtmap): the whole
 * texture over the face, u 0 at its start corner and 63 (the texture's width) at its end, v 0 along its top edge
 * and the same limit sh * 16 texels (a step is 16) along its bottom edge, sh the face's highest top less its
 * lowest bottom (thgt): so the stones hang from the top of the face and a slope's bottom edge squeezes them.
 * Where the ends' heights differ the map is bilinear, not affine: the face is cut in strips that follow it. */
static void wall_quad(Build *b, const float a[2], const float c[2], float bot0, float bot1, float top0, float top1, const float rect[4],
                      const float nrm[3], float ua, float uc, int tex) {
    float vlim = (MAX(top0, top1) - MIN(bot0, bot1)) * 64;
    if (fabsf(bot0 - bot1) < 1e-4f && fabsf(top0 - top1) < 1e-4f) return wall_strip(b, a, c, bot0, bot1, top0, top1, rect, nrm, ua, uc, tex, vlim);
    enum { N = 8 };
    for (int i = 0; i < N; i++) {
        float t0 = (float)i / N, t1 = (float)(i + 1) / N;
        float a2[2] = {a[0] + (c[0] - a[0]) * t0, a[1] + (c[1] - a[1]) * t0}, c2[2] = {a[0] + (c[0] - a[0]) * t1, a[1] + (c[1] - a[1]) * t1};
        wall_strip(b, a2, c2, bot0 + (bot1 - bot0) * t0, bot0 + (bot1 - bot0) * t1, top0 + (top1 - top0) * t0, top0 + (top1 - top0) * t1, rect, nrm,
                   ua + (uc - ua) * t0, ua + (uc - ua) * t1, tex, vlim);
    }
}

/* a horizontal polygon (3 or 4 corners of the tile, in order) at the corner heights */
static void flat(Build *b, int tx, int tz, const int *cs, int n, const float *ys, const float rect[4], float ny, int tex) {
    float p[4][3], uv[4][2];
    for (int k = 0; k < n; k++) {
        float x = (float)tx + CX[cs[k]], z = (float)tz + CZ[cs[k]];
        p[k][0] = x, p[k][1] = ys[k], p[k][2] = z;
        /* TMAPOPS.ASM's do_tmap corner words with GRIDDB.C's qudecode: a floor's texture has u 0 at the west edge and
         * v 0 at the south edge (v grows north: SW 0, SE u, NW v); a ceiling's v 0 is at the north edge (v grows south) */
        uv[k][0] = x * 32, uv[k][1] = (ny > 0 ? z : 64 - z) * 32;
    }
    /* the face's own normal (a slope leans: its corners are on one plane), up for floors, down for ceilings */
    float e1[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
    float e2[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
    float nrm[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
    float l = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (l < 1e-9f) return;
    float s = (nrm[1] * ny < 0 ? -1.0f : 1.0f) / l;
    for (int j = 0; j < 3; j++) nrm[j] *= s;
    add_face(b, (const float(*)[3])p, (const float(*)[2])uv, n, rect, nrm, tex);
}

/* --- the relief as geometry -------------------------------------------------------------------------- */
/* a texture's relief mesh (normals.c, data\textures\*_mesh\NNN.obj): u, v, h per vertex, triangles */
typedef struct {
    float *v; /* u, v, h */
    int *t;
    int nv, nt;
    float units; /* the texture's size in texture coordinates: 64 walls, 32 floors */
} Relief;

static bool relief_load(const char *path, float units, Relief *r) {
    memset(r, 0, sizeof *r);
    Bytes f = file_read(path); /* stays in the arena with the level */
    if (!f.size) return false;
    int nv = 0, nt = 0;
    for (size_t i = 0; i + 1 < f.size; i++)
        if (i == 0 || f.data[i - 1] == '\n') nv += f.data[i] == 'v', nt += f.data[i] == 'f';
    r->v = arena_alloc(sizeof(float) * 3 * (size_t)(nv + 1));
    r->t = arena_alloc(sizeof(int) * 3 * (size_t)(nt + 1));
    r->units = units;
    const char *p = (const char *)f.data, *end = p + f.size;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        if (!nl) nl = end;
        if (p[0] == 'v' && r->nv < nv) {
            char *e;
            float *o = r->v + r->nv * 3;
            o[0] = strtof(p + 1, &e), o[1] = strtof(e, &e), o[2] = strtof(e, &e);
            r->nv++;
        } else if (p[0] == 'f' && r->nt < nt) {
            char *e;
            int *o = r->t + r->nt * 3;
            o[0] = (int)strtol(p + 1, &e, 10) - 1, o[1] = (int)strtol(e, &e, 10) - 1, o[2] = (int)strtol(e, &e, 10) - 1;
            if (o[0] >= 0 && o[1] >= 0 && o[2] >= 0) r->nt++;
        }
        p = nl + 1;
    }
    for (int i = 0; i < r->nt * 3; i++)
        if (r->t[i] >= r->nv) return r->nt = 0, false;
    return r->nt > 0;
}

/* a polygon in texture coordinates, at most 16 corners */
typedef struct {
    float p[16][2];
    int n;
} Poly;

/* keeps the part of the polygon where (a, b) . (u, v) + c >= 0 */
static void clip_half(const Poly *in, float a, float b, float c, Poly *out) {
    out->n = 0;
    for (int k = 0; k < in->n; k++) {
        const float *p = in->p[k], *q = in->p[(k + 1) % in->n];
        float dp = a * p[0] + b * p[1] + c, dq = a * q[0] + b * q[1] + c;
        if (dp >= 0 && out->n < 16) out->p[out->n][0] = p[0], out->p[out->n][1] = p[1], out->n++;
        if ((dp >= 0) != (dq >= 0) && out->n < 16) {
            float t = dp / (dp - dq);
            out->p[out->n][0] = p[0] + (q[0] - p[0]) * t, out->p[out->n][1] = p[1] + (q[1] - p[1]) * t, out->n++;
        }
    }
    if (out->n < 3) out->n = 0;
}

#define RELIEF_DEPTH 0.035f /* tiles: the same as the normal maps' (normals.h NORMAL_DEPTH_TILES) */

/* the face as triangles: its texture's relief mesh repeated over it, cut to its outline, raised along its
 * normal by the heights (eased to the base within TAPER texels of its tapered edges); flat when no relief */
static int g_stamp_key[1 << 15], g_stamp_val[1 << 15], g_stamp_gen[1 << 15], g_gen;
static void emit_face(Build *b, const Face *f, const Relief *r) {
    if (!r || !r->nt) { /* flat */
        int i[4];
        for (int k = 0; k < f->n; k++) i[k] = add_vertex(b, f->p[k], f->uv[k], f->rect, f->nrm);
        for (int k = 1; k + 1 < f->n; k++) add_tri(b, i[0], i[k], i[k + 1]);
        return;
    }
    /* texture coordinates -> world: P0 + A (u - u0) + B (v - v0) */
    int j1 = 1, j2 = 2;
    float du1 = f->uv[1][0] - f->uv[0][0], dv1 = f->uv[1][1] - f->uv[0][1], du2 = f->uv[2][0] - f->uv[0][0], dv2 = f->uv[2][1] - f->uv[0][1];
    float det = du1 * dv2 - du2 * dv1;
    if (fabsf(det) < 1e-6f && f->n == 4) {
        j2 = 3, du2 = f->uv[3][0] - f->uv[0][0], dv2 = f->uv[3][1] - f->uv[0][1], det = du1 * dv2 - du2 * dv1;
    }
    if (fabsf(det) < 1e-6f) return;
    float A[3], B[3];
    for (int c = 0; c < 3; c++) {
        float e1 = f->p[j1][c] - f->p[0][c], e2 = f->p[j2][c] - f->p[0][c];
        A[c] = (dv2 * e1 - dv1 * e2) / det, B[c] = (-du2 * e1 + du1 * e2) / det;
    }
    /* the outline: which side is inside */
    float area = 0;
    for (int k = 0; k < f->n; k++) area += f->uv[k][0] * f->uv[(k + 1) % f->n][1] - f->uv[(k + 1) % f->n][0] * f->uv[k][1];
    float side = area > 0 ? 1.0f : -1.0f;
    float ea[4], eb[4], ec[4]; /* edge k: inside where ea u + eb v + ec >= 0, the value the distance in texels */
    for (int k = 0; k < f->n; k++) {
        const float *p = f->uv[k], *q = f->uv[(k + 1) % f->n];
        float dx = q[0] - p[0], dy = q[1] - p[1], l = sqrtf(dx * dx + dy * dy);
        if (l < 1e-6f) l = 1e-6f;
        ea[k] = -dy / l * side, eb[k] = dx / l * side, ec[k] = -(ea[k] * p[0] + eb[k] * p[1]);
    }
    const float TAPER = 1.5f * r->units / 64.0f;
    float umin = 1e9f, umax = -1e9f, vmin = 1e9f, vmax = -1e9f;
    for (int k = 0; k < f->n; k++)
        umin = MIN(umin, f->uv[k][0]), umax = MAX(umax, f->uv[k][0]), vmin = MIN(vmin, f->uv[k][1]), vmax = MAX(vmax, f->uv[k][1]);
    float W = r->units, depth = RELIEF_DEPTH;
    g_gen++;
    for (int cj = (int)floorf(vmin / W); cj <= (int)floorf((vmax - 1e-4f) / W); cj++)
        for (int ci = (int)floorf(umin / W); ci <= (int)floorf((umax - 1e-4f) / W); ci++)
            for (int t = 0; t < r->nt; t++) {
                const float *v0 = r->v + r->t[t * 3] * 3, *v1 = r->v + r->t[t * 3 + 1] * 3, *v2 = r->v + r->t[t * 3 + 2] * 3;
                float ou = ci * W, ov = cj * W;
                float tu0 = MIN(v0[0], MIN(v1[0], v2[0])) + ou, tu1 = MAX(v0[0], MAX(v1[0], v2[0])) + ou;
                float tv0 = MIN(v0[1], MIN(v1[1], v2[1])) + ov, tv1 = MAX(v0[1], MAX(v1[1], v2[1])) + ov;
                if (tu1 < umin || tu0 > umax || tv1 < vmin || tv0 > vmax) continue;
                /* in the template's triangle: heights by barycentric weights */
                float x0 = v0[0] + ou, y0 = v0[1] + ov, x1 = v1[0] + ou, y1 = v1[1] + ov, x2 = v2[0] + ou, y2 = v2[1] + ov;
                float td = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
                if (fabsf(td) < 1e-9f) continue;
                Poly a, c;
                a.n = 3;
                a.p[0][0] = x0, a.p[0][1] = y0, a.p[1][0] = x1, a.p[1][1] = y1, a.p[2][0] = x2, a.p[2][1] = y2;
                for (int k = 0; k < f->n && a.n; k++) {
                    clip_half(&a, ea[k], eb[k], ec[k], &c);
                    a = c;
                }
                if (!a.n) continue;
                /* split at the taper lines, so the easing starts exactly there */
                Poly parts[8];
                int np = 1;
                parts[0] = a;
                for (int k = 0; k < f->n; k++) {
                    if (!f->taper[k]) continue;
                    int cur = np;
                    for (int q = 0; q < cur && np < 8; q++) {
                        Poly in = parts[q], o1, o2;
                        clip_half(&in, ea[k], eb[k], ec[k] - TAPER, &o1);
                        clip_half(&in, -ea[k], -eb[k], -(ec[k] - TAPER), &o2);
                        if (o1.n && o2.n) parts[q] = o1, parts[np++] = o2;
                    }
                }
                for (int q = 0; q < np; q++) {
                    int id[16];
                    for (int k = 0; k < parts[q].n; k++) {
                        float u = parts[q].p[k][0], v = parts[q].p[k][1];
                        /* this face's vertex at (u, v), shared by its triangles */
                        int ku = (int)lrintf(u * 64), kv = (int)lrintf(v * 64);
                        u32 hsh = ((u32)ku * 73856093u ^ (u32)kv * 19349663u) & ((1 << 15) - 1);
                        int found = -1;
                        while (g_stamp_gen[hsh] == g_gen) {
                            if (g_stamp_key[hsh] == (ku * 65599 ^ kv) && b->v[g_stamp_val[hsh]].uv[0] == u) {
                                found = g_stamp_val[hsh];
                                break;
                            }
                            hsh = (hsh + 1) & ((1 << 15) - 1);
                        }
                        if (found < 0) {
                            float w0 = ((y1 - y2) * (u - x2) + (x2 - x1) * (v - y2)) / td, w1 = ((y2 - y0) * (u - x2) + (x0 - x2) * (v - y2)) / td;
                            float h = v0[2] * w0 + v1[2] * w1 + v2[2] * (1 - w0 - w1);
                            float ease = 1;
                            for (int e = 0; e < f->n; e++)
                                if (f->taper[e]) {
                                    float d = (ea[e] * u + eb[e] * v + ec[e]) / TAPER;
                                    d = d < 0 ? 0 : d > 1 ? 1 : d;
                                    ease = MIN(ease, d * d * (3 - 2 * d));
                                }
                            float pos[3], uv[2] = {u, v};
                            for (int c3 = 0; c3 < 3; c3++)
                                pos[c3] = f->p[0][c3] + A[c3] * (u - f->uv[0][0]) + B[c3] * (v - f->uv[0][1]) + f->nrm[c3] * h * depth * ease;
                            found = add_vertex(b, pos, uv, f->rect, f->nrm);
                            if (found < 0) return;
                            g_stamp_gen[hsh] = g_gen, g_stamp_key[hsh] = ku * 65599 ^ kv, g_stamp_val[hsh] = found;
                        }
                        id[k] = found;
                    }
                    for (int k = 1; k + 1 < parts[q].n; k++) add_tri(b, id[0], id[k], id[k + 1]);
                }
            }
}

/* edges carried on by another face (the same edge, the same plane, the same texture) keep their relief; the
 * rest ease to the base (corners, wall ends, a floor's edge over a step) */
static void set_tapers(Build *b) {
    for (int i = 0; i < b->nf; i++) {
        Face *f = &b->f[i];
        for (int k = 0; k < f->n; k++) f->taper[k] = 1;
    }
    /* a hash of edges by their two ends (either order) */
    enum { HS = 1 << 17 };
    int *slot = arena_alloc(sizeof(int) * HS);
    for (int i = 0; i < HS; i++) slot[i] = -1;
#define EKEY(P, Q) (((u32)lrintf(((P)[0] + (Q)[0]) * 512) * 73856093u) ^ ((u32)lrintf(((P)[1] + (Q)[1]) * 512) * 19349663u) ^ ((u32)lrintf(((P)[2] + (Q)[2]) * 512) * 83492791u))
    for (int i = 0; i < b->nf; i++) {
        Face *f = &b->f[i];
        for (int k = 0; k < f->n; k++) {
            const float *p = f->p[k], *q = f->p[(k + 1) % f->n];
            u32 h = EKEY(p, q) & (HS - 1);
            for (int probe = 0; probe < 64 && slot[h] >= 0; probe++, h = (h + 1) & (HS - 1)) {
                int j = slot[h] >> 2, kk = slot[h] & 3;
                Face *g = &b->f[j];
                const float *gp = g->p[kk], *gq = g->p[(kk + 1) % g->n];
                bool same = (fabsf(gp[0] - q[0]) + fabsf(gp[1] - q[1]) + fabsf(gp[2] - q[2]) < 1e-4f &&
                             fabsf(gq[0] - p[0]) + fabsf(gq[1] - p[1]) + fabsf(gq[2] - p[2]) < 1e-4f) ||
                            (fabsf(gp[0] - p[0]) + fabsf(gp[1] - p[1]) + fabsf(gp[2] - p[2]) < 1e-4f &&
                             fabsf(gq[0] - q[0]) + fabsf(gq[1] - q[1]) + fabsf(gq[2] - q[2]) < 1e-4f);
                if (same && g->tex == f->tex && f->nrm[0] * g->nrm[0] + f->nrm[1] * g->nrm[1] + f->nrm[2] * g->nrm[2] > 0.999f)
                    f->taper[k] = 0, g->taper[kk] = 0;
            }
            if (slot[h] < 0) slot[h] = i << 2 | k;
        }
    }
#undef EKEY
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* a light's orb: a small sphere, glowing in the light's colour (colour alpha 2 = emissive) */
static void orb(Build *b, const float *o) {
    const float R = 0.05f, PI = 3.14159265f;
    float rect[4] = {0, 0, 0, 0}, uv[3][2] = {{0, 0}, {0, 0}, {0, 0}};
    for (int a = 0; a < 12; a++)
        for (int s = 0; s < 6; s++) {
            float p[4][3];
            for (int k = 0; k < 4; k++) {
                float th = (float)(a + (k & 1)) * 2 * PI / 12, ph = (float)(s + (k >> 1)) * PI / 6 - PI / 2;
                p[k][0] = o[0] + R * cosf(ph) * cosf(th), p[k][1] = o[1] + R * sinf(ph), p[k][2] = o[2] + R * cosf(ph) * sinf(th);
            }
            static const int T[2][3] = {{0, 1, 3}, {0, 3, 2}};
            for (int t = 0; t < 2; t++) {
                float q[3][3], nrm[3] = {0, 1, 0};
                for (int k = 0; k < 3; k++) memcpy(q[k], p[T[t][k]], 12);
                int at = b->n;
                tri(b, q, uv, rect, nrm);
                for (int k = 0; k < 3; k++) {
                    Vertex *v = &b->v[at + k];
                    v->color[0] = o[3], v->color[1] = o[4], v->color[2] = o[5], v->color[3] = 2;
                }
            }
        }
}

/* an image into its cell of an atlas (nearest texel when the sizes differ) */
static void put_cell(u8 *atlas, int atlas_size, const Image *img, int x0, int y0, int cell) {
    if (img->w == cell && img->h == cell) {   /* (2026-11-17: the cell's own size, each texel its own: whole rows) */
        for (int y = 0; y < cell; y++) memcpy(atlas + ((size_t)(y0 + y) * atlas_size + (size_t)x0) * 4, img->rgba + (size_t)y * cell * 4, (size_t)cell * 4);
        return;
    }
    for (int y = 0; y < cell; y++)
        for (int x = 0; x < cell; x++) {
            const u8 *s = img->rgba + ((size_t)(y * img->h / cell) * img->w + (size_t)(x * img->w / cell)) * 4;
            memcpy(atlas + ((size_t)(y0 + y) * atlas_size + (size_t)(x0 + x)) * 4, s, 4);
        }
}

/* a picture's normal map into the pictures' normal atlas at the picture's place: the user's own made with Laigter,
 * data\laigter\sprites\...\NAME_n.png for data\sprites\...\NAME.png, at the picture's size (else flat). Beside it
 * (tools\laigter-cli\sprite_op.preset, make-normals.cmd): NAME_p.png, its height map, into the normal atlas's alpha
 * (its "height", 1 where there is none), and NAME_o.png, its occlusion map, into the pictures' AO atlas (1 where
 * there is none). Laigter's grey maps: their red channel. Made without "Use Alpha": the picture's own alpha masks
 * them (the shader discards where it is transparent). */
/* (2026-11-17: the three maps' paths apart, 0 the normal, 1 the height, 2 the occlusion, for level_mesh to name them
 * to png_prefetch as well; false: the picture is not one that has them) */
static bool sprite_relief_path(const char *picture, int k, char *path, size_t n) {
    const char *at = strstr(picture, "\\sprites\\");
    if (!at) at = strstr(picture, "\\ui\\animo\\");   /* the effects (blood, sparks ...): data\laigter\ui\animo */
    const char *dot = strrchr(picture, '.');
    if (!at || !dot || dot < at) return false;
    static const char SFX[3] = {'n', 'p', 'o'};
    snprintf(path, n, "%.*s\\laigter%.*s_%c.png", (int)(at - picture), picture, (int)(dot - at), at, SFX[k]);
    return true;
}
static void sprite_normal(Mesh *m, const char *picture, int x0, int y0, int w, int h) {
    char path[600];
    for (int k = 0; k < 3; k++) {
        if (!sprite_relief_path(picture, k, path, sizeof path)) return;
        if (!png_exists(path)) continue;
        size_t mark = arena_mark();
        Image img;
        if (png_read(path, &img) && img.w == w && img.h == h)
            for (int y = 0; y < h; y++) {
                const u8 *s = img.rgba + (size_t)y * img.w * 4;
                size_t row = (size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0;
                if (k == 0) {   /* the normal (the alpha is the height's: 1 unless NAME_p.png says) */
                    u8 *d = m->snatlas + row * 4;
                    for (int x = 0; x < w; x++) d[x * 4] = s[x * 4], d[x * 4 + 1] = s[x * 4 + 1], d[x * 4 + 2] = s[x * 4 + 2];
                } else if (k == 1)
                    for (int x = 0; x < w; x++) m->snatlas[(row + (size_t)x) * 4 + 3] = s[x * 4];
                else
                    for (int x = 0; x < w; x++) m->saoatlas[row + (size_t)x] = s[x * 4];
            }
        arena_reset(mark);
    }
}

/* the textures' relief meshes as geometry (uw.exe); off: the original's flat faces, the normal maps alone */
bool level_relief = true;
u8 level_new_sprite[512];
/* a wall's or floor's grey map (its height, its occlusion) stretched smooth to the normal atlas's cell (4x the
 * texture's, the texture repeating at its edges) into one channel of it. 2026-11-17: worked out once for each texture
 * and kept for the run (outside the arena), copied in for every level after: they are the same every level, and
 * working them out again was 1.1 s of each level's build. */
typedef struct {
    int n, w, h;
    u8 *px;
} ReliefKept;
static ReliefKept relief_kept[210 + 52][2];
static void relief_stretch(Mesh *m, const float r[4], int cell, const Image *img, int channel, ReliefKept *k) {
    int n = cell * 4;
    if (!k->px || k->n != n || k->w != img->w || k->h != img->h) {
        u8 *px = realloc(k->px, (size_t)n * (size_t)n);
        if (!px) return;
        k->px = px, k->n = n, k->w = img->w, k->h = img->h;
        /* (2026-11-17: this was 1.1 s of the first level's build, every texel working out its own places and weights.
         * A column's two texels and its weight are the same in every row, and a row's in every column: worked out
         * once, by the same sums, the texel's value by the same sum from them: the same bytes. And where every
         * texel falls on one of the map's own with no weight to its neighbours (a map of the cell's own size, as
         * the user's are) the sum gives that texel's value itself: copied.) */
        size_t mark = arena_mark();
        int *xa = arena_alloc(sizeof(int) * (size_t)n * 4), *xb = xa + n, *ya = xb + n, *yb = ya + n;
        float *ax = arena_alloc(sizeof(float) * (size_t)n * 2), *ay = ax + n;
        bool same = img->w == n && img->h == n;
        for (int x = 0; x < n; x++) {
            float fx = (x + 0.5f) * (float)img->w / (float)n - 0.5f;
            int x0 = (int)floorf(fx);
            ax[x] = fx - (float)x0;
            xa[x] = ((x0 % img->w) + img->w) % img->w, xb[x] = (xa[x] + 1) % img->w;
            if (xa[x] != x || ax[x] != 0) same = false;
        }
        for (int y = 0; y < n; y++) {
            float fy = (y + 0.5f) * (float)img->h / (float)n - 0.5f;
            int y0 = (int)floorf(fy);
            ay[y] = fy - (float)y0;
            ya[y] = ((y0 % img->h) + img->h) % img->h, yb[y] = (ya[y] + 1) % img->h;
            if (ya[y] != y || ay[y] != 0) same = false;
        }
        if (same)
            for (size_t i = 0; i < (size_t)n * (size_t)n; i++) px[i] = img->rgba[i * 4];
        else
            for (int y = 0; y < n; y++) {
                const u8 *ra = img->rgba + (size_t)ya[y] * img->w * 4, *rb = img->rgba + (size_t)yb[y] * img->w * 4;
                float wy = ay[y];
                for (int x = 0; x < n; x++) {
                    float wx = ax[x];
                    float h = (ra[xa[x] * 4] * (1 - wx) + ra[xb[x] * 4] * wx) * (1 - wy) + (rb[xa[x] * 4] * (1 - wx) + rb[xb[x] * 4] * wx) * wy;
                    px[(size_t)y * n + x] = (u8)(h + 0.5f);
                }
            }
        arena_reset(mark);
    }
    for (int y = 0; y < n; y++) {
        u8 *d = m->natlas + ((size_t)((int)r[1] * 4 + y) * NATLAS_SIZE + (size_t)((int)r[0] * 4)) * 4 + channel;
        const u8 *q = k->px + (size_t)y * n;
        for (int x = 0; x < n; x++) d[x * 4] = q[x];
    }
}
bool level_mesh(const Level *lv, const char *data_dir, const float (*orbs)[6], int norbs, Mesh *m) {
    memset(m, 0, sizeof *m);
    clock_t lm_t[8];   /* (2026-11-17: the parts timed, UWVIEW_BUILD_LOG=1) */
    int lm_n = 0;
    lm_t[lm_n++] = clock();
    /* the atlas: every wall and floor texture (any size is scaled to its cell, nearest texel) */
    m->atlas = arena_alloc((size_t)ATLAS_SIZE * ATLAS_H * 4);
    m->natlas = arena_alloc((size_t)NATLAS_SIZE * NATLAS_SIZE * 4);
    m->snatlas = arena_alloc((size_t)ATLAS_SIZE * ATLAS_H * 4);
    for (size_t i = 0; i < (size_t)ATLAS_SIZE * ATLAS_H; i++) {
        u8 *d = m->snatlas + i * 4;
        d[0] = d[1] = 128, d[2] = 255, d[3] = 255;
    }
    m->saoatlas = arena_alloc((size_t)ATLAS_SIZE * ATLAS_H);   /* no occlusion where a picture has no map */
    memset(m->saoatlas, 255, (size_t)ATLAS_SIZE * ATLAS_H);
    /* flat where a normal map is missing: (0.5, 0.5, 1) up out of the surface, height 1 */
    for (size_t i = 0; i < (size_t)NATLAS_SIZE * NATLAS_SIZE; i++) {
        u8 *d = m->natlas + i * 4;
        d[0] = d[1] = 128, d[2] = 255, d[3] = 255;
    }
    char path[600];
    int missing = 0;
    {   /* (2026-11-17) the pictures the loops below ask for, named to png_prefetch first: it reads and unpacks them on
         * several threads at once (png.c): on a run's first build the relief maps too, which are kept from then on;
         * on every build the textures and the pictures themselves, read again each time as before. The walls' and
         * floors' textures and three maps, the objects', switches' and decorations' pictures and their three maps,
         * the effects' and doors' maps; the creatures' pictures and maps, and the effects' and doors' pictures, are
         * named further down, when the creatures' tables are read. A picture not named, or not read ahead, is read
         * when asked for, as before. */
        enum { PF_MAX = ((210 + 52) + MAX_SPRITES + 16 + 32 + 64 + 16) * 4 };
        size_t mark = arena_mark();
        char (*pf)[600] = arena_alloc((size_t)PF_MAX * 600);
        const char **pp = arena_alloc((size_t)PF_MAX * sizeof *pp);
        int n = 0;
        for (int i = 0; i < 210 + 52; i++) {
            for (int k = 0; k < 3; k++)
                snprintf(pf[n++], 600, "%s\\laigter\\%s\\%03d_%c.png", data_dir, i < 210 ? "walls" : "floors", i < 210 ? i : i - 210, "npo"[k]);
            snprintf(pf[n++], 600, "%s\\textures\\%s\\%03d.png", data_dir, i < 210 ? "walls" : "floors", i < 210 ? i : i - 210);
        }
        for (int i = 0; i < MAX_SPRITES + 16 + 32 + 64 + 16; i++) {
            int j = i - MAX_SPRITES;
            if (j < 0) {
                if (level_new_sprite[i]) continue;   /* (one of ours in its place: no maps) */
                snprintf(path, sizeof path, "%s\\sprites\\objects\\%03d.png", data_dir, i);
            } else if (j < 16) snprintf(path, sizeof path, "%s\\sprites\\flat_objects\\%03d.png", data_dir, j);
            else if (j < 48) snprintf(path, sizeof path, "%s\\sprites\\wall_objects\\%03d.png", data_dir, j - 16);
            else if (j < 112) snprintf(path, sizeof path, "%s\\ui\\animo\\%03d.png", data_dir, j - 48);
            else snprintf(path, sizeof path, "%s\\sprites\\doors\\%03d.png", data_dir, j - 112);
            if (j < 48) snprintf(pf[n++], 600, "%s", path);
            for (int k = 0; k < 3; k++)
                if (sprite_relief_path(path, k, pf[n], 600)) n++;
        }
        for (int i = 0; i < n; i++) pp[i] = pf[i];
        png_prefetch(pp, n);
        arena_reset(mark);
    }
    for (int i = 0; i < 210 + 52; i++) {
        bool wall = i < 210;
        int k = wall ? i : i - 210;
        size_t mark = arena_mark();
        Image img;
        float r[4];
        if (wall) wall_rect(k, r);
        else floor_rect(k, r);
        int cell = (int)r[2];
        /* the normal map: the user's own made with Laigter (data\laigter\walls|floors\NNN_n.png, at the texture's size,
         * the same axes as ours: tools\laigter-cli) when there is one, else the engine's (textures\walls_normal ...) */
        snprintf(path, sizeof path, "%s\\laigter\\%s\\%03d_n.png", data_dir, wall ? "walls" : "floors", k);
        if (!png_exists(path))   /* (2026-11-17: png_exists, the disk asked once a run as for the other maps, not at every build) */
            snprintf(path, sizeof path, "%s\\textures\\%s_normal\\%03d.png", data_dir, wall ? "walls" : "floors", k);
        if (png_read(path, &img)) {
            put_cell(m->natlas, NATLAS_SIZE, &img, (int)r[0] * 4, (int)r[1] * 4, cell * 4);
            m->normal_maps++;
        }
        arena_reset(mark);
        /* beside it (tools\laigter-cli\texture.preset, make-normals.cmd; the user, 2026-11-14): NNN_p.png, the
         * texture's height map, into the normal atlas's alpha (the shader casts the relief's shadows from it: smooth
         * between its texels, the texture repeating, at the atlas's 4x; 1 where there is none: no shadow), and
         * NNN_o.png, its occlusion map (the joints), into the pictures' AO atlas at the texture's own place (rows
         * the pictures do not use; 1 where there is none). Laigter's grey maps: their red channel. */
        snprintf(path, sizeof path, "%s\\laigter\\%s\\%03d_p.png", data_dir, wall ? "walls" : "floors", k);
        if (png_exists(path) && png_read(path, &img) && img.w > 0 && img.h > 0) {
            relief_stretch(m, r, cell, &img, 3, &relief_kept[i][0]);
        }
        arena_reset(mark);
        snprintf(path, sizeof path, "%s\\laigter\\%s\\%03d_o.png", data_dir, wall ? "walls" : "floors", k);
        /* the occlusion the shader reads is the normal atlas's BLUE (the user, 2026-11-16: occlusion maps larger
         * than the texture, 256 for the 64 texel walls: the atlas's 4x; the normal's z is worked out from its x, y
         * in the shader): 1 where there is no map, else the map smooth between its texels, the texture repeating */
        for (int y = 0; y < cell * 4; y++)
            for (int x = 0; x < cell * 4; x++)
                m->natlas[((size_t)((int)r[1] * 4 + y) * NATLAS_SIZE + (size_t)((int)r[0] * 4 + x)) * 4 + 2] = 255;
        if (png_exists(path) && png_read(path, &img) && img.w > 0 && img.h > 0) {
            for (int y = 0; y < cell; y++)
                for (int x = 0; x < cell; x++)
                    m->saoatlas[(size_t)((int)r[1] + y) * ATLAS_SIZE + (size_t)((int)r[0] + x)] =
                        img.rgba[((size_t)(y * img.h / cell) * img.w + (size_t)(x * img.w / cell)) * 4];
            relief_stretch(m, r, cell, &img, 2, &relief_kept[i][1]);
        }
        arena_reset(mark);
        snprintf(path, sizeof path, "%s\\textures\\%s\\%03d.png", data_dir, wall ? "walls" : "floors", k);
        if (png_read(path, &img)) {
            put_cell(m->atlas, ATLAS_SIZE, &img, (int)r[0], (int)r[1], cell);
        } else {
            missing++;
            for (int y = 0; y < cell; y++) /* magenta check: missing texture */
                for (int x = 0; x < cell; x++) {
                    u8 *d = m->atlas + ((size_t)((int)r[1] + y) * ATLAS_SIZE + (size_t)((int)r[0] + x)) * 4;
                    bool on = ((x / 8) ^ (y / 8)) & 1;
                    d[0] = on ? 255 : 40, d[1] = 0, d[2] = on ? 255 : 40, d[3] = 255;
                }
        }
        arena_reset(mark);
    }
    if (missing) logf_("level: %d textures could not be read (drawn magenta)\n", missing);
    lm_t[lm_n++] = clock();
    /* the object pictures, at their own size in 32 x 32 cells below the textures */
    for (int k = 0; k < MAX_SPRITES; k++) {
        size_t mark = arena_mark();
        Image img;
        /* one of ours in its place (the Esc menu's Sprites tab: level_new_sprite): up to 32 x 32, over 16 a side at
         * twice the detail (sprite_div 2: world.h's sprite_unit draws it at half a pixel's size). It has no relief
         * maps of its own: flat, as any picture without them */
        bool ours = false;
        if (level_new_sprite[k]) {
            snprintf(path, sizeof path, "%s\\newsprites\\objects\\%03d.png", data_dir, k);
            ours = png_exists(path) && png_read(path, &img) && img.w > 0 && img.h > 0 && img.w <= 32 && img.h <= 32;
        }
        if (ours) {
            int x0 = k % 32 * 32, y0 = 1024 + k / 32 * 32;
            for (int y = 0; y < img.h; y++) memcpy(m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0) * 4, img.rgba + (size_t)y * img.w * 4, (size_t)img.w * 4);
            m->sprite_w[k] = (u8)img.w, m->sprite_h[k] = (u8)img.h, m->sprite_div[k] = img.w > 16 || img.h > 16 ? 2 : 1;
            arena_reset(mark);
            continue;
        }
        snprintf(path, sizeof path, "%s\\sprites\\objects\\%03d.png", data_dir, k);
        if (png_read(path, &img) && img.w <= 32 && img.h <= 32) {
            int x0 = k % 32 * 32, y0 = 1024 + k / 32 * 32;
            for (int y = 0; y < img.h; y++) memcpy(m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0) * 4, img.rgba + (size_t)y * img.w * 4, (size_t)img.w * 4);
            sprite_normal(m, path, x0, y0, img.w, img.h);
            m->sprite_w[k] = (u8)img.w, m->sprite_h[k] = (u8)img.h;
        }
        arena_reset(mark);
    }
    lm_t[lm_n++] = clock();
    /* the switches' pictures (TMFLAT.GR: 0x170 + k is picture k) in the free cells from TMFLAT_CELL */
    for (int j = 0; j < 16; j++) {
        size_t mark = arena_mark();
        Image img;
        int k = TMFLAT_CELL + j;
        snprintf(path, sizeof path, "%s\\sprites\\flat_objects\\%03d.png", data_dir, j);
        if (png_read(path, &img) && img.w <= 32 && img.h <= 32) {
            int x0 = k % 32 * 32, y0 = 1024 + k / 32 * 32;
            for (int y = 0; y < img.h; y++) memcpy(m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0) * 4, img.rgba + (size_t)y * img.w * 4, (size_t)img.w * 4);
            sprite_normal(m, path, x0, y0, img.w, img.h);
            m->sprite_w[k] = (u8)img.w, m->sprite_h[k] = (u8)img.h;
        }
        arena_reset(mark);
    }
    /* the wall decorations' pictures (TMOBJ.GR 0-31: dials, writings, gravestones) in the free cells from
     * TMOBJ_CELL (sprite_w / sprite_h there too) */
    for (int j = 0; j < 32; j++) {
        size_t mark = arena_mark();
        Image img;
        int k = TMOBJ_CELL + j;
        snprintf(path, sizeof path, "%s\\sprites\\wall_objects\\%03d.png", data_dir, j);
        if (k < 512 && png_read(path, &img) && img.w <= 32 && img.h <= 32) {
            int x0 = k % 32 * 32, y0 = 1024 + k / 32 * 32;
            for (int y = 0; y < img.h; y++) memcpy(m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0) * 4, img.rgba + (size_t)y * img.w * 4, (size_t)img.w * 4);
            sprite_normal(m, path, x0, y0, img.w, img.h);
            m->sprite_w[k] = (u8)img.w, m->sprite_h[k] = (u8)img.h;
        }
        arena_reset(mark);
    }
    lm_t[lm_n++] = clock();
    /* the creatures on this level: anim.txt and the page-0 frames into 80 x 80 cells from CRIT_ROW */
    {
        bool want[64] = {0};
        for (int t = 0; t < 64 * 64; t++)
            for (int i = lv->tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = lv->obj[i].next, guard++)
                if (lv->obj[i].item >= 0x40 && lv->obj[i].item < 0x80) want[lv->obj[i].item - 0x40] = true;
        int cell = 0, shelf_x = 0, shelf_y = CRIT_ROW, shelf_h = 0;
        for (int k = 0; k < 64; k++) {
            if (!want[k]) continue;
            size_t mark = arena_mark();
            snprintf(path, sizeof path, "%s\\sprites\\creatures\\%02d\\anim.txt", data_dir, k);
            Bytes f = file_read(path);
            m->crit[k].cell = cell;
            m->crit[k].base1 = -1;
            memset(m->crit[k].slots1, 255, sizeof m->crit[k].slots1);
            for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
                int pg, a, b2, c, d, e, n = -1;
                /* (%n is set only when the whole pattern matched: "page 0 frame ..." also gives 1 conversion) */
                if (sscanf(p, "page %d slots%n", &pg, &n) == 1 && n > 0 && pg == 0) {
                    const char *q = p + n;
                    for (int s = 0; s < 32; s++) {
                        char *end;
                        long v = strtol(q, &end, 10);
                        if (end == q) break;
                        m->crit[k].slots[s] = (u8)v, q = end;
                    }
                } else if (sscanf(p, "page %d segment %d%n", &pg, &a, &n) == 2 && pg == 0 && a < 16) {
                    const char *q = p + n;
                    for (int s = 0; s < 8; s++) {
                        char *end;
                        long v = strtol(q, &end, 10);
                        if (end == q) break;
                        m->crit[k].segs[a][s] = (u8)v, q = end;
                    }
                    if (a + 1 > m->crit[k].nsegs) m->crit[k].nsegs = a + 1;
                } else if (sscanf(p, "page %d frame %d %d %d %d %d", &pg, &a, &b2, &c, &d, &e) == 6 && pg == 0 && a < 64) {
                    m->crit[k].w[a] = (u8)b2, m->crit[k].h[a] = (u8)c, m->crit[k].hx[a] = (u8)d, m->crit[k].hy[a] = (u8)e;
                    if (a + 1 > m->crit[k].nframes) m->crit[k].nframes = a + 1;
                } else if ((n = -1, sscanf(p, "page %d slots%n", &pg, &n)) == 1 && n > 0 && pg == 1) { /* (n afresh: the
                    * segment test above sets it on a segment line) */
                    /* page 1 (standing and walking by the angle to the viewer, the original's slots from 0x20) */
                    const char *q = p + n;
                    for (int s = 0; s < 128; s++) {
                        char *end;
                        long v = strtol(q, &end, 10);
                        if (end == q) break;
                        m->crit[k].slots1[s] = (u8)v, q = end;
                    }
                } else if (sscanf(p, "page %d segment %d%n", &pg, &a, &n) == 2 && pg == 1 && a < 64) {
                    const char *q = p + n;
                    for (int s = 0; s < 8; s++) {
                        char *end;
                        long v = strtol(q, &end, 10);
                        if (end == q) break;
                        m->crit[k].segs1[a][s] = (u8)v, q = end;
                    }
                    if (a + 1 > m->crit[k].nsegs1) m->crit[k].nsegs1 = a + 1;
                } else if (sscanf(p, "page %d frame %d %d %d %d %d", &pg, &a, &b2, &c, &d, &e) == 6 && pg == 1) {
                    if (m->crit[k].base1 < 0) m->crit[k].base1 = m->crit[k].nframes;
                    int g = m->crit[k].base1 + a;
                    if (g < 256) {
                        m->crit[k].w[g] = (u8)b2, m->crit[k].h[g] = (u8)c, m->crit[k].hx[g] = (u8)d, m->crit[k].hy[g] = (u8)e;
                        if (g + 1 > m->crit[k].nframes) m->crit[k].nframes = g + 1;
                    }
                }
                const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
                p = nl ? nl + 1 : NULL;
            }
            arena_reset(mark);
            /* each frame at its own size, packed in rows (shelves) from CRIT_ROW; a kind that does not fit whole is
             * left out */
            {
                int sx = shelf_x, sy = shelf_y, sh = shelf_h;
                bool fits = true;
                for (int fr = 0; fr < m->crit[k].nframes && fits; fr++) {
                    int w = m->crit[k].w[fr] + 1, h = m->crit[k].h[fr] + 1;
                    if (sx + w > ATLAS_SIZE) sx = 0, sy += sh, sh = 0;
                    if (sy + h > ATLAS_H) fits = false;
                    else m->crit[k].fx[fr] = (u16)sx, m->crit[k].fy[fr] = (u16)sy, sx += w, sh = MAX(sh, h);
                }
                if (!fits) {
                    m->crit[k].nframes = 0;
                    continue;
                }
                shelf_x = sx, shelf_y = sy, shelf_h = sh;
            }
            cell += m->crit[k].nframes;
        }
        /* (2026-11-17) the kinds' frames are read in a second round, after every kind's table and places are known (as
         * before: a kind's frames change neither): so that all their pictures and relief maps, and the effects' and
         * doors' pictures read after them, are named to png_prefetch at once, as the walls' above */
        {
            int total = 64 + 16, n = 0;
            for (int k = 0; k < 64; k++)
                if (want[k]) total += m->crit[k].nframes * 4;
            size_t mark = arena_mark();
            char (*pf)[600] = arena_alloc((size_t)total * 600);
            const char **pp = arena_alloc((size_t)total * sizeof *pp);
            for (int k = 0; k < 64; k++)
                for (int fr = 0; want[k] && fr < m->crit[k].nframes; fr++) {
                    bool p1 = m->crit[k].base1 >= 0 && fr >= m->crit[k].base1;
                    snprintf(path, sizeof path, "%s\\sprites\\creatures\\%02d\\p%d_%03d.png", data_dir, k, p1 ? 1 : 0,
                             p1 ? fr - m->crit[k].base1 : fr);
                    snprintf(pf[n++], 600, "%s", path);
                    for (int j = 0; j < 3; j++)
                        if (sprite_relief_path(path, j, pf[n], 600)) n++;
                }
            for (int k = 0; k < 64; k++) snprintf(pf[n++], 600, "%s\\ui\\animo\\%03d.png", data_dir, k);
            for (int k = 0; k < 16; k++) snprintf(pf[n++], 600, "%s\\sprites\\doors\\%03d.png", data_dir, k);
            for (int i = 0; i < n; i++) pp[i] = pf[i];
            png_prefetch(pp, n);
            arena_reset(mark);
        }
        for (int k = 0; k < 64; k++) {
            if (!want[k]) continue;
            size_t mark;
            for (int fr = 0; fr < m->crit[k].nframes; fr++) {
                mark = arena_mark();
                Image img;
                bool p1 = m->crit[k].base1 >= 0 && fr >= m->crit[k].base1;
                snprintf(path, sizeof path, "%s\\sprites\\creatures\\%02d\\p%d_%03d.png", data_dir, k, p1 ? 1 : 0,
                         p1 ? fr - m->crit[k].base1 : fr);
                if (png_read(path, &img) && img.w <= m->crit[k].w[fr] + 1 && img.h <= m->crit[k].h[fr] + 1) {
                    int x0 = m->crit[k].fx[fr], y0 = m->crit[k].fy[fr];
                    for (int y = 0; y < img.h; y++)
                        memcpy(m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0) * 4, img.rgba + (size_t)y * img.w * 4, (size_t)img.w * 4);
                    sprite_normal(m, path, x0, y0, img.w, img.h);
                }
                arena_reset(mark);
            }
            m->crit[k].loaded = m->crit[k].nframes > 0;
        }
    }
    terrain_load(data_dir);
    lm_t[lm_n++] = clock();
    {   /* the particles' picture (uwview.c's sparks, drops, embers, smoke): a soft round dot, white at the middle,
           dark at its edge, in the free cell after the object pictures (DOT_CELL) */
        int c = DOT_CELL, x0 = c % 32 * 32, y0 = 1024 + c / 32 * 32;
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++) {
                float d = sqrtf(((float)x - 15.5f) * ((float)x - 15.5f) + ((float)y - 15.5f) * ((float)y - 15.5f)) / 15.5f;
                float a = d < 1 ? 1 - d : 0;
                u8 *px = m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)(x0 + x)) * 4;
                px[0] = px[1] = px[2] = (u8)(255.0f * powf(a, 0.7f));
                px[3] = a > 0.3f ? 255 : 0;
            }
    }
    /* the animated objects' pictures (ANIMO.GR: blood, mist, explosions, splashes, smoke, a fountain's water...), in
     * 32 x 32 cells from ANIMO_CELL (rows 1600-1663, between the doors and the creatures) */
    for (int k = 0; k < 64; k++) {
        size_t mark = arena_mark();
        Image img;
        snprintf(path, sizeof path, "%s\\ui\\animo\\%03d.png", data_dir, k);
        if (png_read(path, &img) && img.w <= 32 && img.h <= 32) {
            int c = ANIMO_CELL + k, x0 = c % 32 * 32, y0 = 1024 + c / 32 * 32;
            for (int y = 0; y < img.h; y++) memcpy(m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0) * 4, img.rgba + (size_t)y * img.w * 4, (size_t)img.w * 4);
            m->animo_w[k] = (u8)img.w, m->animo_h[k] = (u8)img.h;
            sprite_normal(m, path, x0, y0, img.w, img.h);   /* the user's normal map (pixelart.preset, as the items') */
        }
        arena_reset(mark);
    }
    /* the door pictures (32 x 64), in a row below the object pictures */
    for (int k = 0; k < 16; k++) {
        size_t mark = arena_mark();
        Image img;
        snprintf(path, sizeof path, "%s\\sprites\\doors\\%03d.png", data_dir, k);
        if (png_read(path, &img) && img.w <= 32 && img.h <= 64) {
            int x0 = k * 32, y0 = DOOR_ROW;
            for (int y = 0; y < img.h; y++) memcpy(m->atlas + ((size_t)(y0 + y) * ATLAS_SIZE + (size_t)x0) * 4, img.rgba + (size_t)y * img.w * 4, (size_t)img.w * 4);
            sprite_normal(m, path, x0, y0, img.w, img.h);
            m->door_w[k] = (u8)img.w, m->door_h[k] = (u8)img.h;
            /* the door itself: the pictures' top 12-13 rows and a column at a side are see-through (the frame
             * fills them): its opaque box */
            int top = img.h, l = img.w, r = 0;
            for (int y = 0; y < img.h; y++)
                for (int x = 0; x < img.w; x++)
                    if (img.rgba[((size_t)y * img.w + x) * 4 + 3] > 127) top = MIN(top, y), l = MIN(l, x), r = MAX(r, x + 1);
            m->door_top[k] = (u8)(top < img.h ? top : 0), m->door_l[k] = (u8)(r > l ? l : 0), m->door_r[k] = (u8)(r > l ? r : img.w);
        }
        arena_reset(mark);
    }

    png_prefetch(NULL, 0);   /* (2026-11-17: the build's pictures are in: what was read ahead and not asked for is dropped) */
    Build b = {0};
    b.cap = 3 << 20, b.icap = 12 << 20, b.fcap = 64 * 64 * 8;
    b.v = arena_alloc(sizeof(Vertex) * (size_t)b.cap);
    b.idx = arena_alloc(sizeof(u32) * (size_t)b.icap);
    b.f = arena_alloc(sizeof(Face) * (size_t)b.fcap);
    lm_t[lm_n++] = clock();
    /* the textures' relief meshes (only those this level uses are read) */
    static Relief relief[262];
    static bool tried[262];
    memset(relief, 0, sizeof relief), memset(tried, 0, sizeof tried);
    float ceil = level_ceil_y();
    /* the ceiling texture: floor slot 9 (unconfirmed) */
    float crect[4];
    floor_rect(clampi(lv->floors[9], 0, 51), crect);
    for (int tz = 0; tz < 64; tz++)
        for (int tx = 0; tx < 64; tx++) {
            const Tile *t = tile_at(lv, tx, tz);
            if (t->type == T_SOLID) continue;
            float frect[4], wrect[4];
            floor_rect(clampi(lv->floors[t->floor % 10], 0, 51), frect);
            int wtex = clampi(lv->walls[t->wall % 48], 0, 209), ftex = 210 + clampi(lv->floors[t->floor % 10], 0, 51);
            wall_rect(wtex, wrect);
            int ctex = 210 + clampi(lv->floors[9], 0, 51);
            /* floor and ceiling */
            static const int poly[6][4] = {{0, 1, 2, 3}, {0, 1, 2, 3}, {0, 1, 2, -1}, {0, 1, 3, -1}, {1, 2, 3, -1}, {0, 2, 3, -1}};
            int pk = t->type >= T_DIAG_SE && t->type <= T_DIAG_NW ? t->type : 0;
            int n = poly[pk][3] < 0 ? 3 : 4;
            float fy[4], cy[4];
            for (int k = 0; k < n; k++) fy[k] = corner_h(t, poly[pk][k]) * STEP, cy[k] = ceil;
            flat(&b, tx, tz, poly[pk], n, fy, frect, 1, ftex);
            flat(&b, tx, tz, poly[pk], n, cy, crect, -1, ctex);
            /* the diagonal wall */
            if (pk) {
                float a[2], c[2], nrm[3] = {0, 0, 0};
                const float r2 = 0.70710678f;
                if (t->type == T_DIAG_SE || t->type == T_DIAG_NW) { /* SW - NE */
                    a[0] = (float)tx, a[1] = (float)tz, c[0] = (float)tx + 1, c[1] = (float)tz + 1;
                    nrm[0] = t->type == T_DIAG_SE ? r2 : -r2, nrm[2] = -nrm[0];
                } else { /* SE - NW */
                    a[0] = (float)tx + 1, a[1] = (float)tz, c[0] = (float)tx, c[1] = (float)tz + 1;
                    nrm[0] = t->type == T_DIAG_SW ? -r2 : r2, nrm[2] = t->type == T_DIAG_SW ? -r2 : r2;
                }
                float h = t->height * STEP;
                /* seen from the open half, u grows to the right: toward c for SE and NE, toward a for NW and SW */
                float L = 64; /* the whole texture along the diagonal too (do_compact_wtmap): 64 texels, not 90 */
                bool to_c = t->type == T_DIAG_SE || t->type == T_DIAG_NE;
                wall_quad(&b, a, c, h, h, ceil, ceil, wrect, nrm, to_c ? 0 : L, to_c ? L : 0, wtex);
            }
            /* the four edges */
            static const int EC[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
            static const int DX[4] = {0, 1, 0, -1}, DZ[4] = {-1, 0, 1, 0};
            static const float NX[4] = {0, -1, 0, 1}, NZ[4] = {1, 0, -1, 0}; /* into this tile */
            for (int e = 0; e < 4; e++) {
                if (!open_on_edge(t, e)) continue;
                const Tile *nb = tile_at(lv, tx + DX[e], tz + DZ[e]);
                int ca = EC[e][0], cb = EC[e][1];
                float a[2] = {(float)tx + CX[ca], (float)tz + CZ[ca]}, c[2] = {(float)tx + CX[cb], (float)tz + CZ[cb]};
                float nrm[3] = {NX[e], 0, NZ[e]};
                float ha = corner_h(t, ca) * STEP, hb = corner_h(t, cb) * STEP;
                /* u runs along the wall, continuous across tiles, growing to the right as seen from this tile:
                 * facing south (the S edge) right is west, facing east right is south: u = -x or -z there */
                float sgn = (e == 0 || e == 1) ? -64.0f : 64.0f;
                float ua = sgn * ((e == 0 || e == 2) ? a[0] : a[1]), uc = sgn * ((e == 0 || e == 2) ? c[0] : c[1]);
                int back = (e + 2) % 4;
                if (!open_on_edge(nb, back)) {
                    wall_quad(&b, a, c, ha, hb, ceil, ceil, wrect, nrm, ua, uc, wtex);
                } else {
                    /* the neighbour's corners on this edge: ours a (corner ca) is its corner across */
                    static const int OPP[4][2] = {{3, 2}, {0, 3}, {1, 0}, {2, 1}}; /* nb corners matching ca, cb */
                    float na = corner_h(nb, OPP[e][0]) * STEP, nbh = corner_h(nb, OPP[e][1]) * STEP;
                    if (na > ha + 1e-4f || nbh > hb + 1e-4f) wall_quad(&b, a, c, ha, hb, MAX(na, ha), MAX(nbh, hb), wrect, nrm, ua, uc, wtex);
                }
            }
        }
    lm_t[lm_n++] = clock();
    /* the faces as triangles, raised by their textures' relief */
    set_tapers(&b);
    int relief_faces = 0;
    for (int i = 0; i < b.nf; i++) {
        const Face *f = &b.f[i];
        int t = f->tex;
        if (!tried[t]) {
            tried[t] = true;
            if (!level_relief) goto flat; /* the original's flat faces: the normal maps alone give the stone its depth */
            snprintf(path, sizeof path, "%s\\textures\\%s_mesh\\%03d.obj", data_dir, t < 210 ? "walls" : "floors", t < 210 ? t : t - 210);
            relief_load(path, t < 210 ? 64.0f : 32.0f, &relief[t]);
        }
    flat:
        face_kind = tex_terrain(t) == 1 ? 1.2f : tex_terrain(t) == 2 ? 1.3f : 1.0f;   /* water, lava: the shaders' material; flat */
        emit_face(&b, f, relief[t].nt && face_kind == 1.0f ? &relief[t] : NULL);
        face_kind = 1;
        relief_faces += relief[t].nt > 0;
        if (b.n >= b.cap || b.ni + 3 > b.icap) {
            logf_("level: too many triangles; the rest are flat\n");
            for (int j = i + 1; j < b.nf; j++) emit_face(&b, &b.f[j], NULL);
            break;
        }
    }
    for (int i = 0; i < norbs; i++) orb(&b, orbs[i]);
    m->vert = b.v;
    m->vertices = b.n;
    m->index = b.idx;
    m->indices = b.ni;
    m->faces = b.nf, m->relief_faces = relief_faces;
    for (int i = 0; i < 64 * 64; i++) {
        m->grid[i * 4] = lv->tile[i].type;
        m->grid[i * 4 + 1] = lv->tile[i].height;
        m->grid[i * 4 + 2] = lv->tile[i].type == T_SOLID ? 0 : (u8)terr[clampi(lv->floors[lv->tile[i].floor % 10], 0, 51)];   /* 1 water, 2 lava: the lava's glow */
    }
    lm_t[lm_n++] = clock();
    if (getenv("UWVIEW_BUILD_LOG"))
        fprintf(stderr, "uwview: the mesh's parts: walls and floors %ld, objects %ld, switches and decorations %ld, creatures %ld, effects and doors %ld, tiles %ld, triangles %ld ms\n",
                (long)(lm_t[1] - lm_t[0]) * 1000 / CLOCKS_PER_SEC, (long)(lm_t[2] - lm_t[1]) * 1000 / CLOCKS_PER_SEC, (long)(lm_t[3] - lm_t[2]) * 1000 / CLOCKS_PER_SEC,
                (long)(lm_t[4] - lm_t[3]) * 1000 / CLOCKS_PER_SEC, (long)(lm_t[5] - lm_t[4]) * 1000 / CLOCKS_PER_SEC, (long)(lm_t[6] - lm_t[5]) * 1000 / CLOCKS_PER_SEC,
                (long)(lm_t[7] - lm_t[6]) * 1000 / CLOCKS_PER_SEC);
    /* (2026-11-17: the atlases' sums, to prove a change to the build left every byte as it was: with UWVIEW_BUILD_LOG=2
     * only, the summing takes 140 ms that uwview.c's "its mesh and textures" then counts) */
    if (getenv("UWVIEW_BUILD_LOG") && atoi(getenv("UWVIEW_BUILD_LOG")) >= 2) {
        Fnv fa, fn, fs, fo, ft;
        fnv_init(&fa), fnv_init(&fn), fnv_init(&fs), fnv_init(&fo), fnv_init(&ft);
        fnv_bytes(&ft, m->crit, sizeof m->crit), fnv_bytes(&ft, m->sprite_w, sizeof m->sprite_w), fnv_bytes(&ft, m->sprite_h, sizeof m->sprite_h);
        fnv_bytes(&ft, m->sprite_div, sizeof m->sprite_div), fnv_bytes(&ft, m->animo_w, sizeof m->animo_w), fnv_bytes(&ft, m->animo_h, sizeof m->animo_h);
        fnv_bytes(&ft, m->door_w, sizeof m->door_w), fnv_bytes(&ft, m->door_h, sizeof m->door_h), fnv_bytes(&ft, m->door_top, sizeof m->door_top);
        fnv_bytes(&ft, m->door_l, sizeof m->door_l), fnv_bytes(&ft, m->door_r, sizeof m->door_r);
        fnv_bytes(&fa, m->atlas, (size_t)ATLAS_SIZE * ATLAS_H * 4);
        fnv_bytes(&fn, m->natlas, (size_t)NATLAS_SIZE * NATLAS_SIZE * 4);
        fnv_bytes(&fs, m->snatlas, (size_t)ATLAS_SIZE * ATLAS_H * 4);
        fnv_bytes(&fo, m->saoatlas, (size_t)ATLAS_SIZE * ATLAS_H);
        fprintf(stderr, "uwview: the atlases' sums: pictures %08x, normals %08x, pictures' normals %08x, pictures' occlusion %08x, the pictures' and creatures' tables %08x\n", fa.h, fn.h, fs.h, fo.h, ft.h);
    }
    return true;
}

/* --- the level from the original's own block (LEV.ARK's layout, as the rules core keeps it in mapdata): the
 * tiles and the 1024 objects decoded as tools\extract wrote them into levelN.txt (the walls, floors and doors of
 * the texture block are left as they are) */
void level_from_block(Level *lv, const u8 *b) {
    for (int i = 0; i < 4096; i++) {
        u32 w0 = rd16(b + 4 * i), w1 = rd16(b + 4 * i + 2);
        Tile *t = &lv->tile[i];
        t->type = (u8)(w0 & 15), t->height = (u8)((w0 >> 4) & 15), t->floor = (u8)((w0 >> 10) & 15), t->wall = (u8)(w1 & 63);
        t->door = (u8)(w0 >> 15), t->nomagic = (u8)((w0 >> 14) & 1), t->unknown8 = (u8)((w0 >> 8) & 3), t->first_object = (u16)(w1 >> 6);
    }
    for (int i = 0; i < 1024; i++) {
        size_t off = 0x4000 + (i < 256 ? (size_t)i * 27 : 256 * 27 + (size_t)(i - 256) * 8);
        u32 w0 = rd16(b + off), w1 = rd16(b + off + 2), w2 = rd16(b + off + 4), w3 = rd16(b + off + 6);
        Object *o = &lv->obj[i];
        o->item = (u16)(w0 & 511), o->flags = (u8)(w0 >> 9), o->z = (u8)(w1 & 127), o->heading = (u8)((w1 >> 7) & 7);
        o->y = (u8)((w1 >> 10) & 7), o->x = (u8)(w1 >> 13), o->quality = (u8)(w2 & 63), o->next = (u16)(w2 >> 6);
        o->owner = (u8)(w3 & 63), o->link = (u16)(w3 >> 6);
        if (i < 256) memcpy(o->mobile, b + off + 8, 19);
        else memset(o->mobile, 0, 19);
    }
}

/* one object (0-1023) of the original's level block, decoded as level_from_block does */
void level_obj_from_block(const u8 *b, int i, Object *o) {
    size_t off = 0x4000 + (i < 256 ? (size_t)i * 27 : 256 * 27 + (size_t)(i - 256) * 8);
    u32 w0 = rd16(b + off), w1 = rd16(b + off + 2), w2 = rd16(b + off + 4), w3 = rd16(b + off + 6);
    o->item = (u16)(w0 & 511), o->flags = (u8)(w0 >> 9), o->z = (u8)(w1 & 127), o->heading = (u8)((w1 >> 7) & 7);
    o->y = (u8)((w1 >> 10) & 7), o->x = (u8)(w1 >> 13), o->quality = (u8)(w2 & 63), o->next = (u16)(w2 >> 6);
    o->owner = (u8)(w3 & 63), o->link = (u16)(w3 >> 6);
    if (i < 256) memcpy(o->mobile, b + off + 8, 19);
    else memset(o->mobile, 0, 19);
}
