/* models.c: the original's 3D models run from their bytecode (models.h). The programs are UW.EXE's own (the render
 * interpreter's language: underworld-exhumed INTERP.ASM, TMAPOPS.ASM; UnderworldAdventures' ModelDecoder was read as a
 * map for the colour operands). Points are built the way the interpreter builds them (a triple transformed as it is
 * defined, offsets along the model's axes, sums of points), so a sub-program turned by an angle (a door's leaf on its
 * hinge) comes out where the original puts it; faces become triangle fans. Coordinates are 8.8 fixed, 1.0 one tile;
 * the wire order of a triple is (x, z, y) with y up (INTERP.ASM's do_defres); angles are 1/65536 of a turn. */
#include "models.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- the tables of DRAWOBJ.C ---- */
static const unsigned char rect_cols[30][4] = {
    {0x01, 0xEC, 0x00, 0x00}, {0x21, 0xEB, 0x00, 0x00}, {0x11, 0xEC, 0x00, 0x3E}, {0x01, 0xE4, 0x00, 0x00},
    {0x02, 0xB6, 0xB0, 0x00}, {0x02, 0x64, 0x6C, 0x00}, {0x02, 0x64, 0x6C, 0x00}, {0x02, 0x64, 0x6C, 0x00},
    {0x42, 0xE8, 0xB8, 0x00}, {0x01, 0xE4, 0x00, 0x00}, {0x19, 0xE4, 0x00, 0x60}, {0x03, 0xA3, 0xA4, 0xA6},
    {0x01, 0x68, 0x00, 0x00}, {0x01, 0x68, 0x00, 0x00}, {0x11, 0xEC, 0x00, 0x00}, {0x21, 0xEC, 0x00, 0x00},
    {0x51, 0xB0, 0x00, 0xE4}, {0x51, 0xB0, 0x00, 0xEC}, {0x11, 0xB0, 0x00, 0xF4}, {0x11, 0x6A, 0x00, 0x3C},
    {0x51, 0xB0, 0x00, 0x00}, {0x11, 0xB0, 0x00, 0x00}, {0x21, 0xB0, 0x00, 0x00}, {0x83, 0x00, 0x02, 0x04},
    {0x02, 0xE4, 0x68, 0x00}, {0x02, 0xE6, 0x68, 0x00}, {0x01, 0xE4, 0x00, 0x00}, {0x02, 0xE4, 0x6A, 0x00},
    {0x03, 0xE6, 0x6A, 0x71}, {0x03, 0xE2, 0x62, 0xC4}};
static const short rect_sub[32] = {0x03, 0x08, 0x08, 0x07, 0x07, 0x06, 0x05, 0x0B, 0x18, 0x09, 0x17, 0x1B, 0x1C, 0x19, 0x1A, 0x04,
                                   0x0A, 0x10, 0x11, -1,   0x02, 0x13, 0x12, -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1};

int models_for_item(int item) {
    int k = (item & 0x3F) - 0x10;
    if (item < 0x150 || item > 0x16F || k < 0 || k >= 32) return -1;
    return rect_sub[k];
}
const unsigned char *models_cols(int model) {
    static const unsigned char none[4] = {0, 0, 0, 0};
    return model >= 0 && model < 30 ? rect_cols[model] : none;
}

/* ---- the file ---- */
static Bytes exe;                 /* game\UW.EXE */
static size_t mbase;              /* the models' base: the table's offset + 0x8E */
static u16 moffs[MODELS_N];
static float pal[256][3];
static bool ready;

bool models_ready(void) { return ready; }
void models_palette(int index, float rgb[3]) { memcpy(rgb, pal[index & 255], 12); }

static u16 w16(size_t at) { return at + 1 < exe.size ? (u16)(exe.data[at] | exe.data[at + 1] << 8) : 0; }

bool models_load(const char *game_dir) {
    static const size_t TABLES[4] = {0x4e910, 0x4ccd0, 0x4e370, 0x4ec70};
    char path[1024];
    if (ready) return true;
    snprintf(path, sizeof path, "%s\\UW.EXE", game_dir);
    exe = file_read(path);
    if (!exe.size) { logf_("models: %s is missing\n", path); return false; }
    mbase = 0;
    for (int i = 0; i < 4; i++)
        if (TABLES[i] + 4 <= exe.size && w16(TABLES[i]) == 0x4ab6 && w16(TABLES[i] + 2) == 0x4006) {
            for (int n = 0; n < MODELS_N; n++) moffs[n] = w16(TABLES[i] + 2 * n);
            mbase = TABLES[i] + 0x8E;
            break;
        }
    if (!mbase) { logf_("models: no model table in %s\n", path); return false; }
    snprintf(path, sizeof path, "%s\\DATA\\PALS.DAT", game_dir);
    Bytes pals = file_read(path);
    if (pals.size < 768) { logf_("models: %s is missing\n", path); return false; }
    for (int i = 0; i < 256; i++)
        for (int c = 0; c < 3; c++) {
            u8 v = pals.data[i * 3 + c];
            pal[i][c] = (float)(v << 2 | v >> 4) / 255.0f;
        }
    ready = true;
    logf_("models: %d models from UW.EXE (table at 0x%zx)\n", MODELS_N, mbase - 0x8E);
    return true;
}

/* ---- the interpreter ---- */
#define NPTS 512
#define VAR0 0x2920           /* Clk(0)'s address: the variables lie at 0x2920 + 2n */
#define DELTA_PT 256          /* the 0x4C delta point (0x800 / 8) */

typedef struct {
    const ModelInst *mi;
    float pt[NPTS][3];        /* in the model's frame, after the current rotation and origin */
    u8 shade[NPTS];
    bool have[NPTS];
    float org[3];             /* the origin, moved by 4A (translate) */
    float c, s;               /* the current rotation about y (cos, sin) */
    float col[3];             /* the flat colour set last (BC, 2E, 5C) */
    int gour_var;             /* D4's variable: the Gouraud colour's base */
    bool gouraud;             /* D6 chose the standard (Gouraud) surface */
    float nrm[3];             /* the normal of the face to come (a jnorm test's plane), or none */
    bool have_nrm;
    int cur_tex;              /* A8's texture operand (informational) */
    float vscale;             /* the texture's height slot (bmhgtoff + cTmBm * 8) as a share of its full height: v x this */
    Vertex *out;
    int cap, n;
    int depth;
    bool stop;
} Ctx;

static float fx(u16 w) { return (float)(short)w / 256.0f; }
static float mi_vscale(const ModelInst *mi) { return mi->tex_vscale > 0 ? mi->tex_vscale : 1.0f; }

static int var_of(u16 addr) { int v = ((int)addr - VAR0) / 2; return v >= 0 && v < MODEL_VARS && !((addr - VAR0) & 1) ? v : -1; }
static int var_val(const Ctx *c, u16 addr) { int v = var_of(addr); return v < 0 ? 0 : c->mi->vars[v]; }

/* a model-frame vector from an (x, y, z) in the program (its axes), through the current rotation */
static void rot(const Ctx *c, float x, float y, float z, float out[3]) {
    out[0] = x * c->c + z * c->s;
    out[1] = y;
    out[2] = -x * c->s + z * c->c;
}
static void set_pt(Ctx *c, int p, const float v[3]) {
    if (p < 0 || p >= NPTS) return;
    memcpy(c->pt[p], v, 12), c->have[p] = true;
}
/* a point defined by a triple: rotated and offset by the origin */
static void def_pt(Ctx *c, int p, float x, float y, float z) {
    float v[3];
    rot(c, x, y, z, v);
    v[0] += c->org[0], v[1] += c->org[1], v[2] += c->org[2];
    set_pt(c, p, v);
}
/* a point as another plus an offset along the model's axes */
static void rel_pt(Ctx *c, int src, int dst, float dx, float dy, float dz) {
    float v[3], d[3];
    if (src < 0 || src >= NPTS) return;
    rot(c, dx, dy, dz, d);
    v[0] = c->pt[src][0] + d[0], v[1] = c->pt[src][1] + d[1], v[2] = c->pt[src][2] + d[2];
    set_pt(c, dst, v);
}

static void colour_index(Ctx *c, int index) {
    const ModelInst *mi = c->mi;
    /* flag 0x20: variable 0 is the texture's own colour, given as RGB; its shades (BC's + n) darken it a little */
    if (mi->var0_rgb[0] >= 0 && index >= mi->vars[0] && index < mi->vars[0] + 8) {
        float k = 1.0f - 0.07f * (float)(index - mi->vars[0]);
        c->col[0] = mi->var0_rgb[0] * k, c->col[1] = mi->var0_rgb[1] * k, c->col[2] = mi->var0_rgb[2] * k;
        return;
    }
    memcpy(c->col, pal[index & 255], 12);
}

/* one face: a fan of the points, flat (the colour, with the points' Gouraud shades when D6 chose them) or textured
 * (uv 0..1 across the instance's texture rect) */
static void face(Ctx *c, int n, const int *pts, const float (*uv)[2]) {
    const ModelInst *mi = c->mi;
    float nrm[3] = {0, 1, 0};
    if (n < 3) return;
    if (c->have_nrm) memcpy(nrm, c->nrm, 12);
    else {                    /* Newell's normal from the polygon */
        float nx = 0, ny = 0, nz = 0;
        for (int i = 0; i < n; i++) {
            const float *a = c->pt[pts[i]], *b = c->pt[pts[(i + 1) % n]];
            nx += (a[1] - b[1]) * (a[2] + b[2]), ny += (a[2] - b[2]) * (a[0] + b[0]), nz += (a[0] - b[0]) * (a[1] + b[1]);
        }
        float l = sqrtf(nx * nx + ny * ny + nz * nz);
        if (l > 1e-6f) nrm[0] = nx / l, nrm[1] = ny / l, nrm[2] = nz / l;
    }
    c->have_nrm = false;
    bool tex = uv != NULL && mi->rect[2] > 0;
    float kind = tex ? mi->kind_tex : mi->kind_flat;
    int base_col = c->gouraud && c->gour_var >= 0 ? mi->vars[c->gour_var] : -1;
    /* the polygon cut into triangles by ear clipping in the plane of its normal: the original's filler takes any
     * simple polygon, and the models use concave ones (the ankh's loop is one path round the outside and back
     * along the hole); a fan would fill the hole */
    int tris[64 * 3], ntri = 0;
    {
        int ax = fabsf(nrm[0]) > fabsf(nrm[1]) && fabsf(nrm[0]) > fabsf(nrm[2]) ? 0 : fabsf(nrm[1]) > fabsf(nrm[2]) ? 1 : 2;
        int ua = ax == 0 ? 1 : 0, va = ax == 2 ? 1 : 2;
        float P[64][2];
        int rem[64], nr = n > 64 ? 64 : n;
        float area = 0;
        for (int i = 0; i < nr; i++) P[i][0] = c->pt[pts[i]][ua], P[i][1] = c->pt[pts[i]][va];
        for (int i = 0; i < nr; i++) { int j = (i + 1) % nr; area += P[i][0] * P[j][1] - P[j][0] * P[i][1]; }
        for (int i = 0; i < nr; i++) rem[i] = area >= 0 ? i : nr - 1 - i;   /* counter-clockwise in (u, v) */
        int guard = 0;
        while (nr > 3 && guard++ < 4096) {
            bool cut = false;
            for (int i = 0; i < nr && !cut; i++) {
                int a = rem[(i + nr - 1) % nr], b = rem[i], d = rem[(i + 1) % nr];
                float ax_ = P[b][0] - P[a][0], ay = P[b][1] - P[a][1], bx = P[d][0] - P[b][0], by = P[d][1] - P[b][1];
                if (ax_ * by - ay * bx <= 1e-9f) continue;                /* a reflex corner: no ear */
                bool inside = false;
                for (int k = 0; k < nr && !inside; k++) {
                    int q = rem[k];
                    if (q == a || q == b || q == d) continue;
                    float s1 = (P[b][0] - P[a][0]) * (P[q][1] - P[a][1]) - (P[b][1] - P[a][1]) * (P[q][0] - P[a][0]);
                    float s2 = (P[d][0] - P[b][0]) * (P[q][1] - P[b][1]) - (P[d][1] - P[b][1]) * (P[q][0] - P[b][0]);
                    float s3 = (P[a][0] - P[d][0]) * (P[q][1] - P[d][1]) - (P[a][1] - P[d][1]) * (P[q][0] - P[d][0]);
                    if (s1 >= 0 && s2 >= 0 && s3 >= 0) inside = true;
                }
                if (inside) continue;
                tris[ntri * 3] = a, tris[ntri * 3 + 1] = b, tris[ntri * 3 + 2] = d, ntri++;
                for (int k = i; k + 1 < nr; k++) rem[k] = rem[k + 1];
                nr--;
                cut = true;
            }
            if (!cut) break;                                                  /* degenerate: the rest as a fan */
        }
        if (nr >= 3) for (int i = 1; i + 1 < nr; i++) tris[ntri * 3] = rem[0], tris[ntri * 3 + 1] = rem[i], tris[ntri * 3 + 2] = rem[i + 1], ntri++;
    }
    for (int i = 0; i < ntri; i++) {
        if (c->n + 3 > c->cap) { c->stop = true; return; }
        int idx[3] = {tris[i * 3], tris[i * 3 + 1], tris[i * 3 + 2]};
        for (int k = 0; k < 3; k++) {
            int p = pts[idx[k]];
            Vertex *v = &c->out[c->n++];
            memset(v, 0, sizeof *v);
            float w[3] = {c->pt[p][0] + mi->pos[0], c->pt[p][1] + mi->pos[1], c->pt[p][2] + mi->pos[2]};
            memcpy(v->pos, w, 12), memcpy(v->prev, w, 12);
            memcpy(v->nrm, nrm, 12);
            if (tex) {
                memcpy(v->rect, mi->rect, 16);
                v->uv[0] = uv[idx[k]][0] * (mi->rect[2] - 0.002f) + 0.001f;
                v->uv[1] = uv[idx[k]][1] * c->vscale * (mi->rect[3] - 0.002f) + 0.001f;   /* (past 1: the texture repeats, as on the walls) */
                v->color[0] = v->color[1] = v->color[2] = 1;
            } else {
                float col[3];
                if (base_col >= 0) {
                    /* Gouraud: the base colour darkened by the point's shade (0 full, up to 0xE) */
                    float k = 1.0f - 0.045f * (float)(c->shade[p] > 14 ? 14 : c->shade[p]);
                    if (mi->var0_rgb[0] >= 0 && c->gour_var == 0) {
                        memcpy(col, mi->var0_rgb, 12);
                        col[0] *= k, col[1] *= k, col[2] *= k;
                    } else {
                        /* the original's Gouraud surface: the colour is the palette entry base + shade (the boulders'
                           0x64 + 0..0xE runs down the grey-blue ramp to 0x6F; a plain darkening left them pale and
                           flat beside the original's: level 4's 33,6 and level 6's 56,43, 2026-11-15) */
                        int s = c->shade[p] > 14 ? 14 : c->shade[p];
                        memcpy(col, pal[(base_col + s) & 255], 12);
                    }
                } else memcpy(col, c->col, 12);
                memcpy(v->color, col, 12);
            }
            v->color[3] = kind;
        }
    }
}

static void run(Ctx *c, size_t at);

/* a sub-program at `at`, turned by `turns` about the current origin */
static void sub_turned(Ctx *c, size_t at, float turns) {
    float oc = c->c, os = c->s;
    float a = turns * 6.2831853f;
    float nc = cosf(a), ns = sinf(a);
    /* compose: the new rotation applied before the current one */
    c->c = oc * nc - os * ns;
    c->s = os * nc + oc * ns;
    run(c, at);
    c->c = oc, c->s = os;
}

static void run(Ctx *c, size_t at) {
    size_t p = at;
    if (c->depth > 24) return;
    c->depth++;
    while (!c->stop && p + 2 <= exe.size) {
        u16 op = w16(p);
        p += 2;
        switch (op) {
        case 0x0000: c->depth--; return;                                           /* do_eof */
        case 0x0006: case 0x000C: case 0x000E: case 0x0010: {                      /* sort planes: both halves */
            p += op == 0x0006 ? 12 : 8;
            u16 a = w16(p); p += 2; size_t pa = p + (short)a;
            u16 b = w16(p); p += 2; size_t pb = p + (short)b;
            run(c, pa);
            run(c, pb);
            break;
        }
        case 0x0012: { u16 o = w16(p); p += 2; run(c, p + (short)o); break; }      /* do_sfcal */
        case 0x0014: case 0x0016: {                                                 /* do_ifle / do_ifge */
            u16 skip = w16(p), addr = w16(p + 2); short val = (short)w16(p + 4);
            p += 6;
            int v = var_val(c, addr);
            if (op == 0x0014 ? v <= val : v >= val) p += (short)skip;
            break;
        }
        case 0x0002: { u16 addr = w16(p); short val = (short)w16(p + 2); p += 4; int v = var_of(addr);
                       if (v >= 0) ((ModelInst *)c->mi)->vars[v] = val; break; }   /* do_movec (not in the models) */
        case 0x0040: case 0x0044: break;                                            /* fill routine choices */
        case 0x0048: { u16 o = w16(p); p += 2; p += (short)o; break; }               /* do_ijmp: a jump */
        case 0x004A: {                                                              /* do_rorg: translate */
            float d[3];
            rot(c, fx(w16(p)), fx(w16(p + 2)), fx(w16(p + 4)), d);
            c->org[0] += d[0], c->org[1] += d[1], c->org[2] += d[2];
            p += 6;
            break;
        }
        case 0x0058: {                                                              /* jnorm: skip, (n,p) x, y, z */
            float n[3] = {fx(w16(p + 2)), fx(w16(p + 6)), fx(w16(p + 10))};
            rot(c, n[0], n[1], n[2], c->nrm);
            c->have_nrm = true;
            p += 14;
            break;
        }
        case 0x005E: case 0x0060: case 0x0062: {                                    /* jnorm with one axis missing */
            float n[3] = {0, 0, 0};
            float a = fx(w16(p + 2)), b = fx(w16(p + 6));
            if (op == 0x005E) n[1] = a, n[2] = b;
            else if (op == 0x0060) n[0] = a, n[2] = b;
            else n[0] = a, n[1] = b;
            rot(c, n[0], n[1], n[2], c->nrm);
            c->have_nrm = true;
            p += 10;
            break;
        }
        case 0x0064: case 0x0066: case 0x0068: {                                    /* jnorm along one axis */
            float n[3] = {0, 0, 0};
            n[op == 0x0064 ? 0 : op == 0x0066 ? 1 : 2] = fx(w16(p + 2));
            rot(c, n[0], n[1], n[2], c->nrm);
            c->have_nrm = true;
            p += 6;
            /* the original runs only the block of the side facing the eye (the jnorm skips the other), so each side's
               block starts with the texture height do_door set; we draw both: each starts from it again (the door
               frame's 0068 blocks: the lintel at that height, then BE restores the full height for the posts) */
            c->vscale = mi_vscale(c->mi);
            break;
        }
        case 0x0078: p += 10; break;                                                /* parapiped: a view test */
        case 0x007A: { float x = fx(w16(p)), z = fx(w16(p + 2)), y = fx(w16(p + 4)); int pt = w16(p + 6) >> 3; p += 8;
                       def_pt(c, pt, x, y, z); break; }
        case 0x0082: {                                                              /* multires: count, first, triples */
            int n = w16(p), first = w16(p + 2) >> 3;
            p += 4;
            for (int i = 0; i < n; i++, p += 6) def_pt(c, first + i, fx(w16(p)), fx(w16(p + 4)), fx(w16(p + 2)));
            break;
        }
        case 0x0086: case 0x0088: case 0x008A: {                                    /* x_rel, y_rel, z_rel */
            int src = w16(p) >> 3; float d = fx(w16(p + 2)); int dst = w16(p + 4) >> 3;
            p += 6;
            rel_pt(c, src, dst, op == 0x0086 ? d : 0, op == 0x0088 ? d : 0, op == 0x008A ? d : 0);
            break;
        }
        case 0x008C: {                                                              /* addres: dest = a + b */
            int a = w16(p) >> 3, b = w16(p + 2) >> 3, dst = w16(p + 4) >> 3;
            p += 6;
            if (a >= 0 && a < NPTS && b >= 0 && b < NPTS) {
                float v[3] = {c->pt[a][0] + c->pt[b][0], c->pt[a][1] + c->pt[b][1], c->pt[a][2] + c->pt[b][2]};
                set_pt(c, dst, v);
            }
            break;
        }
        case 0x0090: case 0x0092: case 0x0094: {                                    /* two offsets, then src, dst */
            float a = fx(w16(p)), b = fx(w16(p + 2)); int src = w16(p + 4) >> 3, dst = w16(p + 6) >> 3;
            p += 8;
            if (op == 0x0090) rel_pt(c, src, dst, a, b, 0);                       /* dx, dy */
            else if (op == 0x0092) rel_pt(c, src, dst, a, 0, b);                  /* dx, dz */
            else rel_pt(c, src, dst, 0, b, a);                                    /* 94h: dz then dy (INTERP.ASM's note) */
            break;
        }
        case 0x007E: case 0x0022: {                                                 /* polyres: a flat polygon */
            int n = w16(p), pts[64];
            p += 2;
            if (n > 64) n = 64;
            for (int i = 0; i < n; i++) pts[i] = w16(p + 2 * i) >> 3;
            p += 2 * (size_t)w16(p - 2);
            face(c, n, pts, NULL);
            break;
        }
        case 0x00A0: {                                                              /* compact tmap: slot, 4 byte points */
            static const float UV[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            int pts[4] = {exe.data[p + 2], exe.data[p + 3], exe.data[p + 4], exe.data[p + 5]};
            p += 6;
            face(c, 4, pts, UV);
            break;
        }
        case 0x00A8: case 0x00B4: case 0x00CE: {                                    /* textured faces: (point, u, v) */
            int pts[64]; float uv[64][2];
            if (op == 0x00A8) c->cur_tex = w16(p), p += 2;
            int n = w16(p);
            p += 2;
            if (n > 64) n = 64;
            for (int i = 0; i < n; i++, p += 6) pts[i] = w16(p) >> 3, uv[i][0] = w16(p + 2) / 65535.0f, uv[i][1] = w16(p + 4) / 65535.0f;
            face(c, n, pts, uv);
            break;
        }
        case 0x00BA: {                                                              /* ihcall_ind: angle from a variable */
            u16 var = w16(p); u16 o = w16(p + 2); p += 4;
            sub_turned(c, p + (short)o, (float)var_val(c, var) / 65536.0f);
            break;
        }
        case 0x0050: {                                                              /* ihcall: angle, offset */
            short ang = (short)w16(p); u16 o = w16(p + 2); p += 4;
            sub_turned(c, p + (short)o, (float)(u16)ang / 65536.0f);
            break;
        }
        case 0x00BC: {                                                              /* colour = [var] + n */
            u16 var = w16(p); short n = (short)w16(p + 2); p += 4;
            colour_index(c, var_val(c, var) + n);
            c->gouraud = false;
            break;
        }
        case 0x00BE: {                                                              /* movei: *[b] = [a] (INTERP.ASM _do_movei) */
            /* the door frame's BE Clk(7) -> *Clk(6): do_door put the texture's full height (cTmDm^2 - 1) in Clk(7) and
               the address of its height slot in Clk(6), so this restores the full height after the lintel, drawn at
               (cTmDm^2 >> 8) x h - 1 (DRAWOBJ.C do_door) */
            if (w16(p) == VAR0 + 2 * 7 && w16(p + 2) == VAR0 + 2 * 6) c->vscale = 1.0f;
            p += 4;
            break;
        }
        case 0x002E: { colour_index(c, w16(p)); c->gouraud = false; p += 2; break; }
        case 0x005C: { colour_index(c, var_val(c, w16(p)) + (short)w16(p + 2)); c->gouraud = false; p += 4; break; }
        case 0x00C8: { colour_index(c, w16(p)); p += 2; break; }
        case 0x00CA: { colour_index(c, var_val(c, w16(p))); p += 2; break; }
        case 0x00D4: {                                                              /* uwshade: count, var, (point, shade) */
            int n = w16(p); u16 var = w16(p + 2); p += 4;
            c->gour_var = var_of(var);
            for (int i = 0; i < n; i++, p += 3) { int pt = w16(p) >> 3; if (pt >= 0 && pt < NPTS) c->shade[pt] = exe.data[p + 2]; }
            if (n & 1) p += 1;
            break;
        }
        case 0x00D6: c->gouraud = c->gour_var >= 0; break;                          /* the standard surface */
        case 0x00D8: break;
        default:
            logf_("models: model %d: unknown opcode %04X at 0x%zx\n", c->mi->model, op, p - 2);
            c->stop = true;
            c->depth--;
            return;
        }
    }
    c->depth--;
}

int models_place(const ModelInst *mi, Vertex *out, int cap) {
    static Ctx c;             /* large: the points */
    if (!ready || mi->model < 0 || mi->model >= MODELS_N || cap < 3) return 0;
    memset(&c, 0, sizeof c);
    c.mi = mi, c.out = out, c.cap = cap;
    c.gour_var = -1;
    c.vscale = mi_vscale(mi);
    {
        float a = mi->turn * 6.2831853f;
        c.c = cosf(a), c.s = sinf(a);
    }
    {
        float d[3] = {0, mi->delta_y, 0};
        set_pt(&c, DELTA_PT, d);
        /* the portcullis (DRAWOBJ.C do_door) has a second 0x4C block, point 0x400 / 8 = 128, (0xD0 - 0x30 x steps) high
         * (our delta_y for model 12): the height its vertical bars' tops are added to (8C a + b); without it the bars
         * collapsed to flat slivers and only the cross bars showed */
        if (mi->model == 12) set_pt(&c, 128, d);
    }
    colour_index(&c, mi->vars[0]);
    size_t at = mbase + moffs[mi->model] + 4 + 6;   /* the header dword and the three extents */
    run(&c, at);
    return c.n;
}
