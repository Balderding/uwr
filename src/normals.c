#include "normals.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* the way the maps are made; a map the engine made with an older way (and nobody changed since) is made again */
#define NORMALS_VERSION 3 /* 3: the same depth in the world for floors (32 texels a tile) as walls */

static int wrap(int v, int n) { return ((v % n) + n) % n; }

/* drops mortar patches smaller than min_size pixels (dark specks are not mortar: mortar forms lines) */
static void drop_specks(u8 *g, int w, int h, int min_size) {
    size_t mark = arena_mark();
    int n = w * h;
    int *stack = arena_alloc(sizeof(int) * (size_t)n), *member = arena_alloc(sizeof(int) * (size_t)n);
    u8 *seen = arena_alloc((size_t)n);
    for (int s = 0; s < n; s++) {
        if (!g[s] || seen[s]) continue;
        int sp = 0, count = 0;
        stack[sp++] = s, seen[s] = 1;
        while (sp) {
            int i = stack[--sp];
            member[count++] = i;
            int x = i % w, y = i / w;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int j = wrap(y + dy, h) * w + wrap(x + dx, w);
                    if (g[j] && !seen[j]) seen[j] = 1, stack[sp++] = j;
                }
        }
        if (count < min_size)
            for (int k = 0; k < count; k++) g[member[k]] = 0;
    }
    arena_reset(mark);
}

void normals_make(const u8 *rgba, int w, int h, float texels_per_tile, float bevel, u8 *out, float *heights) {
    size_t mark = arena_mark();
    int n = w * h;
    float *lum = arena_alloc(sizeof(float) * (size_t)n), *loc = arena_alloc(sizeof(float) * (size_t)n);
    float *ht = arena_alloc(sizeof(float) * (size_t)n), *ht2 = arena_alloc(sizeof(float) * (size_t)n);
    u8 *groove = arena_alloc((size_t)n);
    float mean = 0;
    for (int i = 0; i < n; i++) {
        const u8 *p = rgba + (size_t)i * 4;
        lum[i] = (0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]) / 255.0f;
        mean += lum[i];
    }
    mean /= (float)n;
    /* the brightness round each pixel (a 9 x 9 box, the texture repeating) */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float s = 0;
            for (int dy = -4; dy <= 4; dy++)
                for (int dx = -4; dx <= 4; dx++) s += lum[wrap(y + dy, h) * w + wrap(x + dx, w)];
            loc[y * w + x] = s / 81.0f;
        }
    /* mortar: clearly darker than the surroundings, in lines (and not in a texture dark all over, like the
     * Void's black: that stays flat) */
    for (int i = 0; i < n; i++) groove[i] = mean > 0.06f && lum[i] < loc[i] * 0.62f && lum[i] < 0.45f;
    drop_specks(groove, w, h, MAX(6, n / 150));
    int grooves = 0;
    for (int i = 0; i < n; i++) grooves += groove[i];
    /* height: the distance to the nearest mortar, rising like a cushion (steep at the edge, rounding off
     * toward the middle of the stone) */
    int r = (int)ceilf(bevel) + 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            float d = (float)r;
            if (groove[i]) d = 0;
            else if (grooves)
                for (int dy = -r; dy <= r; dy++)
                    for (int dx = -r; dx <= r; dx++)
                        if (groove[wrap(y + dy, h) * w + wrap(x + dx, w)]) {
                            float e = sqrtf((float)(dx * dx + dy * dy)) - 0.5f;
                            if (e < d) d = e;
                        }
            float t = d / bevel;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            ht[i] = grooves ? 1 - (1 - t) * (1 - t) * (1 - t) : 0.6f;
        }
    /* softened a little (3 x 3), then the speckle painted inside a stone: lighter a touch higher */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float s = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) s += ht[wrap(y + dy, h) * w + wrap(x + dx, w)] * (dx || dy ? 1.0f : 4.0f);
            int i = y * w + x;
            ht2[i] = s / 12.0f + (lum[i] - loc[i]) * 0.25f * (groove[i] ? 0.3f : 1.0f);
            /* the geometry takes the stones' shapes only: the painted speckle stays in the normal map */
            if (heights) heights[i] = s / 12.0f < 0 ? 0 : s / 12.0f > 1 ? 1 : s / 12.0f;
        }
    /* 4x up, smoothly (bilinear, repeating), then the normals from the slopes */
    int W = w * NORMAL_SCALE, H = h * NORMAL_SCALE;
    float *hi = arena_alloc(sizeof(float) * (size_t)W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float fx = ((float)x + 0.5f) / NORMAL_SCALE - 0.5f, fy = ((float)y + 0.5f) / NORMAL_SCALE - 0.5f;
            int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
            float ax = fx - (float)x0, ay = fy - (float)y0;
            float a = ht2[wrap(y0, h) * w + wrap(x0, w)], b = ht2[wrap(y0, h) * w + wrap(x0 + 1, w)];
            float c = ht2[wrap(y0 + 1, h) * w + wrap(x0, w)], d = ht2[wrap(y0 + 1, h) * w + wrap(x0 + 1, w)];
            hi[y * W + x] = (a * (1 - ax) + b * ax) * (1 - ay) + (c * (1 - ax) + d * ax) * ay;
        }
    /* depth of the relief in this texture's texels (2.2 on a 64-texel wall, 1.1 on a 32-texel floor) */
    const float depth = NORMAL_DEPTH_TILES * texels_per_tile;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            /* slopes per texel (central differences over two fine pixels = half a texel) */
            float dx = (hi[y * W + wrap(x + 1, W)] - hi[y * W + wrap(x - 1, W)]) * (NORMAL_SCALE / 2.0f) * depth;
            float dy = (hi[wrap(y + 1, H) * W + x] - hi[wrap(y - 1, H) * W + x]) * (NORMAL_SCALE / 2.0f) * depth;
            /* image y runs down; the normal's y runs up */
            float nx = -dx, ny = dy, nz = 1;
            float l = sqrtf(nx * nx + ny * ny + nz * nz);
            u8 *o = out + ((size_t)y * W + x) * 4;
            o[0] = (u8)lrintf((nx / l * 0.5f + 0.5f) * 255);
            o[1] = (u8)lrintf((ny / l * 0.5f + 0.5f) * 255);
            o[2] = (u8)lrintf((nz / l * 0.5f + 0.5f) * 255);
            float hv = hi[y * W + x];
            o[3] = (u8)lrintf((hv < 0 ? 0 : hv > 1 ? 1 : hv) * 255);
        }
    arena_reset(mark);
}

/* --- the relief as geometry ----------------------------------------------------------------------------
 * The full detail: a point at every texel corner (65 x 65 for a 64-texel wall), two triangles a texel, the
 * height from the stones' shapes. Kept in game: that mesh simplified by edge collapses (Garland & Heckbert's
 * quadric error): the pair of neighbouring points whose merging changes the shape least is merged first, as
 * long as the shape stays within RELIEF_ERROR texels; a straight mortar line ends as a few long triangles.
 * The border points are all kept (the texture repeats: its repeats, and the next tile's face, meet them
 * exactly), no triangle may turn over. OBJ: "v u v h" (u, v in the texture's units, h 0 mortar .. 1 a stone's
 * face), "f a b c". */
#define RELIEF_ERROR 0.8f  /* texels: the outline and bulge of each stone; the normal map shades the curves */
#define RELIEF_VERSION "# uo-new relief v7"

static float height_at(const float *ht, int w, int h, float x, float y) { /* texel units, bilinear, repeating */
    x -= 0.5f, y -= 0.5f;
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float ax = x - (float)x0, ay = y - (float)y0;
    float a = ht[wrap(y0, h) * w + wrap(x0, w)], b = ht[wrap(y0, h) * w + wrap(x0 + 1, w)];
    float c = ht[wrap(y0 + 1, h) * w + wrap(x0, w)], d = ht[wrap(y0 + 1, h) * w + wrap(x0 + 1, w)];
    return (a * (1 - ax) + b * ax) * (1 - ay) + (c * (1 - ax) + d * ax) * ay;
}

/* a quadric: the sum of squared distances to planes, as the symmetric 4x4 matrix's 10 numbers */
typedef struct {
    double q[10];
} Quadric;
static void quadric_plane(Quadric *Q, double a, double b, double c, double d, double wgt) {
    double p[4] = {a, b, c, d};
    int k = 0;
    for (int i = 0; i < 4; i++)
        for (int j = i; j < 4; j++) Q->q[k++] += p[i] * p[j] * wgt;
}
static double quadric_eval(const Quadric *Q, const float *v) {
    double p[4] = {v[0], v[1], v[2], 1}, s = 0;
    int k = 0;
    for (int i = 0; i < 4; i++)
        for (int j = i; j < 4; j++, k++) s += Q->q[k] * p[i] * p[j] * (i == j ? 1 : 2);
    return s;
}

#define MAX_DEG 48
typedef struct {
    float (*p)[3];     /* u, v (texels), height (texels) */
    int (*t)[3];
    u8 *tdead, *vdead, *border;
    int *ntri, (*tris)[MAX_DEG]; /* each point's triangles */
    u32 *stamp;
    Quadric *Q;
    int nv, nt;
} Simp;

typedef struct {
    double cost;
    int a, b;    /* a merges into b */
    u32 sa, sb;
} Cand;
typedef struct {
    Cand *c;
    int n, cap;
} Heap;
static void heap_push(Heap *h, Cand c) {
    if (h->n >= h->cap) return;
    int i = h->n++;
    while (i > 0 && h->c[(i - 1) / 2].cost > c.cost) h->c[i] = h->c[(i - 1) / 2], i = (i - 1) / 2;
    h->c[i] = c;
}
static Cand heap_pop(Heap *h) {
    Cand top = h->c[0], last = h->c[--h->n];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        double mc = last.cost;
        if (l < h->n && h->c[l].cost < mc) m = l, mc = h->c[l].cost;
        if (r < h->n && h->c[r].cost < mc) m = r;
        if (m == i) break;
        h->c[i] = h->c[m], i = m;
    }
    if (h->n) h->c[i] = last;
    return top;
}

static double tri_area2(const float *a, const float *b, const float *c) {
    return (double)(b[0] - a[0]) * (c[1] - a[1]) - (double)(c[0] - a[0]) * (b[1] - a[1]);
}

/* the cost of merging a into b, or -1 when it may not be done */
static double collapse_cost(const Simp *s, int a, int b) {
    if (s->border[a] || s->vdead[a] || s->vdead[b]) return -1;
    if (s->ntri[b] + s->ntri[a] > MAX_DEG) return -1;
    /* no triangle of a that stays may turn over or collapse in the (u, v) plane */
    for (int k = 0; k < s->ntri[a]; k++) {
        int t = s->tris[a][k];
        if (s->tdead[t]) continue;
        const int *v = s->t[t];
        if (v[0] == b || v[1] == b || v[2] == b) continue;
        float q[3][3];
        for (int j = 0; j < 3; j++) memcpy(q[j], s->p[v[j] == a ? b : v[j]], 12);
        double before = tri_area2(s->p[v[0]], s->p[v[1]], s->p[v[2]]), after = tri_area2(q[0], q[1], q[2]);
        if (after * before <= 0 || fabs(after) < 1e-4 * fabs(before) + 1e-6) return -1;
    }
    Quadric Q = s->Q[a];
    for (int k = 0; k < 10; k++) Q.q[k] += s->Q[b].q[k];
    return quadric_eval(&Q, s->p[b]);
}

/* a merges into b: the triangles on edge a-b go, a's others take b; returns how many triangles went */
static int merge(Simp *s, int a, int b) {
    int gone = 0;
    for (int k = 0; k < s->ntri[a]; k++) {
        int t = s->tris[a][k];
        if (s->tdead[t]) continue;
        int *v = s->t[t];
        if (v[0] == b || v[1] == b || v[2] == b) {
            s->tdead[t] = 1, gone++;
            continue;
        }
        for (int j = 0; j < 3; j++)
            if (v[j] == a) v[j] = b;
        if (s->ntri[b] < MAX_DEG) s->tris[b][s->ntri[b]++] = t;
    }
    int m = 0;
    for (int k = 0; k < s->ntri[b]; k++)
        if (!s->tdead[s->tris[b][k]]) s->tris[b][m++] = s->tris[b][k];
    s->ntri[b] = m;
    for (int k = 0; k < 10; k++) s->Q[b].q[k] += s->Q[a].q[k];
    s->vdead[a] = 1;
    s->stamp[b]++;
    for (int k = 0; k < s->ntri[b]; k++)
        for (int j = 0; j < 3; j++) s->stamp[s->t[s->tris[b][k]][j]]++;
    return gone;
}

static void push_edges_of(Simp *s, Heap *h, int v) {
    for (int k = 0; k < s->ntri[v]; k++) {
        int t = s->tris[v][k];
        if (s->tdead[t]) continue;
        for (int j = 0; j < 3; j++) {
            int o = s->t[t][j];
            if (o == v) continue;
            double c1 = collapse_cost(s, v, o), c2 = collapse_cost(s, o, v);
            if (c1 >= 0) heap_push(h, (Cand){c1, v, o, s->stamp[v], s->stamp[o]});
            if (c2 >= 0) heap_push(h, (Cand){c2, o, v, s->stamp[o], s->stamp[v]});
        }
    }
}

/* is the relief mesh there and current: missing, or the engine's own from before v2 (the first line says), it is
 * made (again); any other file is someone's own work and is kept */
static bool relief_current(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[80] = {0};
    size_t n = fread(line, 1, sizeof line - 1, f);
    fclose(f);
    line[n] = 0;
    if (!strncmp(line, RELIEF_VERSION, strlen(RELIEF_VERSION))) return true;
    return strncmp(line, "# relief of a texture as geometry, made by uo-new", 49) != 0 && strncmp(line, "# uo-new relief v", 17) != 0;
}

bool relief_mesh_write(const char *path, const float *ht, int w, int h, float units) {
    if (w != h || w < 4) return true; /* square pictures only (all the game's are) */
    size_t mark = arena_mark();
    int g = w + 1;
    Simp s = {0};
    s.nv = g * g, s.nt = 2 * w * w;
    s.p = arena_alloc(sizeof *s.p * (size_t)s.nv);
    s.t = arena_alloc(sizeof *s.t * (size_t)s.nt);
    s.tdead = arena_alloc((size_t)s.nt), s.vdead = arena_alloc((size_t)s.nv), s.border = arena_alloc((size_t)s.nv);
    s.ntri = arena_alloc(sizeof(int) * (size_t)s.nv);
    s.tris = arena_alloc(sizeof *s.tris * (size_t)s.nv);
    s.stamp = arena_alloc(sizeof(u32) * (size_t)s.nv);
    s.Q = arena_alloc(sizeof(Quadric) * (size_t)s.nv);
    /* the relief's depth in texels of this picture (the same in the world for every texture) */
    float depth = NORMAL_DEPTH_TILES * (float)w; /* one repeat of every texture covers one tile */
    for (int y = 0; y < g; y++)
        for (int x = 0; x < g; x++) {
            int i = y * g + x;
            s.p[i][0] = (float)x, s.p[i][1] = (float)y, s.p[i][2] = height_at(ht, w, h, (float)x, (float)y) * depth;
            s.border[i] = x == 0 || y == 0 || x == w || y == w;
        }
    int nt = 0;
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            int a = y * g + x, b = a + 1, c = a + g, d = c + 1;
            /* the diagonal along the smaller height change */
            bool alt = fabsf(s.p[a][2] - s.p[d][2]) > fabsf(s.p[b][2] - s.p[c][2]);
            int t1[3] = {a, b, alt ? c : d}, t2[3] = {alt ? b : a, d, c};
            memcpy(s.t[nt++], t1, sizeof t1), memcpy(s.t[nt++], t2, sizeof t2);
        }
    for (int t = 0; t < nt; t++) {
        const int *v = s.t[t];
        float *p0 = s.p[v[0]], *p1 = s.p[v[1]], *p2 = s.p[v[2]];
        double e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]}, e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        double l = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (l < 1e-12) continue;
        for (int k = 0; k < 3; k++) n[k] /= l;
        double d = -(n[0] * p0[0] + n[1] * p0[1] + n[2] * p0[2]);
        for (int j = 0; j < 3; j++) {
            quadric_plane(&s.Q[v[j]], n[0], n[1], n[2], d, l * 0.5);
            if (s.ntri[v[j]] < MAX_DEG) s.tris[v[j]][s.ntri[v[j]]++] = t;
        }
    }
    int alive = nt;
    /* 1. the borders as lines: each border's height profile keeps the points that matter (Douglas-Peucker within
     * RELIEF_ERROR); the opposite border keeps the same ones (its heights are the same: the texture repeats),
     * the others merge into their neighbour along the border */
    u8 *keep = arena_alloc((size_t)g);
    for (int axis = 0; axis < 2; axis++) {
        memset(keep, 0, (size_t)g);
        /* the profile along x at y = 0 (axis 0), along y at x = 0 (axis 1) */
#define PROF(i) (axis ? s.p[(i) * g][2] : s.p[(i)][2])
        int stack[256][2], sp = 0;
        keep[0] = keep[w] = 1;
        stack[sp][0] = 0, stack[sp++][1] = w;
        while (sp) {
            int i0 = stack[--sp][0], i1 = stack[sp][1], best = -1;
            float worst = RELIEF_ERROR;
            for (int i = i0 + 1; i < i1; i++) {
                float line = PROF(i0) + (PROF(i1) - PROF(i0)) * (float)(i - i0) / (float)(i1 - i0);
                float e = fabsf(PROF(i) - line);
                if (e > worst) worst = e, best = i;
            }
            if (best >= 0 && sp < 254) {
                keep[best] = 1;
                stack[sp][0] = i0, stack[sp++][1] = best;
                stack[sp][0] = best, stack[sp++][1] = i1;
            }
        }
#undef PROF
        /* both borders of this axis: points not kept merge into the next point along the border */
        for (int side = 0; side < 2; side++)
            for (int i = 1; i < w; i++) {
                if (keep[i]) continue;
                int a = axis ? (i * g + side * w) : (side * w * g + i);
                int b = axis ? ((i + 1) * g + side * w) : (side * w * g + i + 1);
                if (s.vdead[b]) continue;
                s.border[a] = 0;
                if (collapse_cost(&s, a, b) >= 0) alive -= merge(&s, a, b);
                s.border[a] = 1;
            }
    }
    /* 2. the inside: the cheapest merges first, until the budget (about one triangle per 14 texels) */
    int budget = MAX(64, w * w / 14);
    Heap hp = {arena_alloc(sizeof(Cand) * (size_t)s.nt * 400), 0, s.nt * 400};
    for (int v = 0; v < s.nv; v++) push_edges_of(&s, &hp, v);
    while (hp.n && alive > budget) {
        Cand c = heap_pop(&hp);
        if (s.vdead[c.a] || s.vdead[c.b] || s.stamp[c.a] != c.sa || s.stamp[c.b] != c.sb) continue;
        if (collapse_cost(&s, c.a, c.b) < 0) continue;
        int b = c.b;
        alive -= merge(&s, c.a, b);
        push_edges_of(&s, &hp, b);
        for (int k = 0; k < s.ntri[b]; k++)
            for (int j = 0; j < 3; j++)
                if (s.t[s.tris[b][k]][j] != b) push_edges_of(&s, &hp, s.t[s.tris[b][k]][j]);
        if (hp.n > hp.cap - 4096) break; /* (never in practice) */
    }
    /* the OBJ */
    int *index = arena_alloc(sizeof(int) * (size_t)s.nv);
    size_t cap = (size_t)s.nv * 48 + (size_t)s.nt * 32 + 256, tn = 0;
    char *text = arena_alloc(cap);
    tn += (size_t)snprintf(text + tn, cap - tn, RELIEF_VERSION ": a texture's relief as geometry: v u v h (u, v in the texture's units, "
                                                "h 0 mortar .. 1 a stone's face); delete to have it made again\n");
    float scale = units / (float)w;
    int nv = 0, kept = 0;
    for (int v = 0; v < s.nv; v++)
        if (!s.vdead[v]) {
            index[v] = ++nv;
            tn += (size_t)snprintf(text + tn, cap - tn, "v %.3f %.3f %.4f\n", s.p[v][0] * scale, s.p[v][1] * scale, s.p[v][2] / depth);
        }
    for (int t = 0; t < s.nt; t++) {
        if (s.tdead[t]) continue;
        const int *v = s.t[t];
        if (v[0] == v[1] || v[1] == v[2] || v[0] == v[2]) continue;
        tn += (size_t)snprintf(text + tn, cap - tn, "f %d %d %d\n", index[v[0]], index[v[1]], index[v[2]]);
        kept++;
    }
    (void)kept;
    make_dirs_for(path);
    bool ok = file_write(path, text, tn);
    arena_reset(mark);
    return ok;
}

/* --- the maps on disk, and the list of the ones the engine made ---------------------------------- */
static u32 file_hash(const char *path) {
    size_t mark = arena_mark();
    Bytes b = file_read(path);
    Fnv f;
    fnv_init(&f);
    fnv_bytes(&f, b.data, b.size);
    arena_reset(mark);
    return b.size ? f.h : 0;
}

int normals_missing(const char *data_dir) {
    /* normal_maps.txt: "version N", then "NAME HASH" for each map the engine wrote. A map whose bytes still
     * have that hash is the engine's own and is made again when the version changes; any other map is
     * someone's own work and is kept. (Before the list existed, every map there was the engine's.) */
    char list[700];
    snprintf(list, sizeof list, "%s\\textures\\normal_maps.txt", data_dir);
    static char names[262][32];
    static u32 hashes[262];
    int version = 0, listed = 0;
    bool have_list = file_exists(list);
    {
        size_t mark = arena_mark();
        Bytes f = file_read(list);
        const char *p = (const char *)f.data, *end = p + f.size;
        while (p && p < end) {
            char a[64];
            unsigned hv;
            if (sscanf(p, "version %d", &version) == 1) {
            } else if (sscanf(p, "%63s %x", a, &hv) == 2 && a[0] != '#' && listed < 262) {
                str_copy(names[listed], sizeof names[0], a);
                hashes[listed++] = hv;
            }
            const char *nl = memchr(p, '\n', (size_t)(end - p));
            p = nl ? nl + 1 : NULL;
        }
        arena_reset(mark);
    }
    int made = 0, kept_own = 0;
    char src[700], dst[700], name[32];
    static char out_names[262][32];
    static u32 out_hashes[262];
    int nout = 0;
    for (int k = 0; k < 2; k++) {
        const char *kind = k ? "floors" : "walls";
        int count = k ? 52 : 210;
        for (int i = 0; i < count; i++) {
            snprintf(name, sizeof name, "%s_normal\\%03d.png", kind, i);
            snprintf(dst, sizeof dst, "%s\\textures\\%s", data_dir, name);
            char obj[700];
            snprintf(obj, sizeof obj, "%s\\textures\\%s_mesh\\%03d.obj", data_dir, kind, i);
            bool exists = file_exists(dst), remake = !exists, need_obj = !relief_current(obj);
            if (exists && version < NORMALS_VERSION) {
                if (!have_list) remake = true;
                else
                    for (int j = 0; j < listed; j++)
                        if (!strcmp(names[j], name)) remake = hashes[j] == file_hash(dst);
            }
            if (exists && !remake) {
                /* still listed when it is the engine's own (unchanged) */
                for (int j = 0; j < listed; j++)
                    if (!strcmp(names[j], name) && hashes[j] == file_hash(dst)) {
                        str_copy(out_names[nout], 32, name), out_hashes[nout++] = hashes[j];
                        kept_own++;
                    }
                if (!need_obj) continue;
            }
            if (remake) need_obj = true;
            snprintf(src, sizeof src, "%s\\textures\\%s\\%03d.png", data_dir, kind, i);
            size_t mark = arena_mark();
            Image img;
            if (!png_read(src, &img)) {
                arena_reset(mark);
                logf_("normals: cannot read %s\n", src);
                return -1;
            }
            /* stones rise over about a third of a stone's height: 4.5 texels of a 64-texel wall */
            float bevel = 4.5f * (float)img.w / 64.0f;
            if (bevel < 1.5f) bevel = 1.5f;
            u8 *out = arena_alloc((size_t)img.w * img.h * NORMAL_SCALE * NORMAL_SCALE * 4);
            /* walls show 64 texels a tile, floors 32, whatever size the picture is */
            float tpt = (float)(k ? 32 : 64);
            float *heights = arena_alloc(sizeof(float) * (size_t)img.w * img.h);
            normals_make(img.rgba, img.w, img.h, tpt * (float)img.w / (k ? 32.0f : 64.0f), bevel, out, heights);
            bool ok = !remake || png_write(dst, out, img.w * NORMAL_SCALE, img.h * NORMAL_SCALE, 4);
            /* the relief as geometry: in the texture's own units (64 a wall tile, 32 a floor tile) */
            if (ok && need_obj) ok = relief_mesh_write(obj, heights, img.w, img.h, tpt);
            arena_reset(mark);
            if (!ok) return logf_("normals: cannot write %s or %s\n", dst, obj), -1;
            if (remake) {
                str_copy(out_names[nout], 32, name), out_hashes[nout++] = file_hash(dst);
                made++;
            }
        }
    }
    if (made || version != NORMALS_VERSION) {
        FILE *f = fopen(list, "wb");
        if (f) {
            fprintf(f, "# Normal maps made by uo-new. A map whose bytes still match its line is the engine's own and is made\r\n"
                       "# again when the way they are made improves. Change or replace a map and it is kept as yours.\r\n"
                       "# Delete a map to have it made again from its texture.\r\nversion %d\r\n", NORMALS_VERSION);
            for (int i = 0; i < nout; i++) fprintf(f, "%s %08x\r\n", out_names[i], out_hashes[i]);
            fclose(f);
        }
    }
    (void)kept_own;
    return made;
}
