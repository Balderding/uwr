/* render_vk_cpu.h: render.c's CPU side, COPIED for the Vulkan renderer (render_vk.c includes it; nothing else does).
 * MADE BY tools\render_vk_cpu.py FROM render.c: do not edit it here. render.c is not to be changed while D3D12 is the
 * default (the Vulkan plan's "beside, not instead"), so what both renderers must work out the same way is copied word
 * for word: the effects' state, the water's rings and the held time; the constants and a light as the shaders take
 * them; the flame (render_flame); the matrices; the view's rectangle, the shade scale, full bright; the black body and
 * its adaptation (render_blackbody); the torch's colour, the lens, the tint, the debug view.
 * The one change: GetTickCount64 is rv_ticks. tools\buildview.cmd checks this file against render.c at every build
 * (render_vk_cpu.py --check): when render.c changes in these parts, run the tool and read the difference. */
static const RenderFxPasses *g_fxp;
static struct { int particles, decals, water, fog; } g_fx;   /* render_set_fx: all off unless the settings say */
void render_set_fx(int particles, int decals, int water, int fog) { g_fx.particles = particles, g_fx.decals = decals, g_fx.water = water, g_fx.fog = fog; }
static bool g_rp_ok, g_rd_ok;   /* the particles' and the decals' passes started */
/* the effects' water (fx_water; the scene's shader): the splashes' rings, up to 16, two seconds each */
static struct { float x, z, strength; double born; } g_ripple[16];
static int g_ripples_shown = 1;   /* (the checks: 0 hides them, the water's waves kept) */
void render_water_ripple(float x, float z, float strength) {
    int k = 0;
    for (int i = 1; i < 16; i++) if (g_ripple[i].born < g_ripple[k].born) k = i;
    g_ripple[k].x = x, g_ripple[k].z = z, g_ripple[k].strength = strength;
    g_ripple[k].born = (double)rv_ticks() / 1000.0;
}
void render_water_ripples_shown(bool on) { g_ripples_shown = on; }
/* the checks' comparisons of an effect on and off (uwview.c, UWVIEW_COMPARE_FX): the flames' time and the frame count
 * (the water's and the lava's motion) held, so that only the effect differs between the two pictures */
static bool g_hold;
static float g_hold_secs;
static double g_hold_now;
static u32 g_hold_frame;
void render_hold_time(bool on) {
    if (on && !g_hold) g_hold_secs = (float)(rv_ticks() % 100000000ull) / 1000.0f, g_hold_now = (double)rv_ticks() / 1000.0;
    if (on && !g_hold) {   /* UWVIEW_HOLD_SECS=N (the checks only: tools\vkcompare.py): the held time is N seconds, not
                              the clock's, so that two runs (D3D12 and Vulkan) hold the same flames and water */
        static int got;
        static float fixed = -1;
        if (!got) { const char *e = getenv("UWVIEW_HOLD_SECS"); got = 1; if (e && *e) fixed = (float)atof(e); }
        if (fixed >= 0) g_hold_secs = fixed, g_hold_now = (double)fixed;
    }
    g_hold = on;
}

/* the frame's constants: src\shaders\cbuf.hlsli, the same order */
typedef struct {
    float vp[16], vp_nojit[16], vp_prev[16];
    float eye[4], look[4], size[4], post[4];
    float lpos[16][4], lcol[16][4], linfo[4], outp[4];
    float tint[4];
    float rtlim[4];
    float torchc[4];
    float sprite[4];    /* the pictures' Laigter maps: x = occlusion strength, y = height (parallax, texels) */
    float fxw[4];       /* the effects' water: x on, y the ripples, z the time (s) */
    float ripple[16][4];/* the splashes' rings: x, z, age (s), strength */
    float texres[4];    /* the walls' and floors' high-res copy: x its scale (0: off) */
    float hdr[4];       /* HDR output: x on, y paper white / 80 (scRGB's 1.0 is 80 nits), z peak / paper white */
} Constants;

/* a light as the shaders take it (common.hlsli GLight) */
typedef struct {
    float pr[4], col[4];
    float info[4];      /* x: puffs (0: a point light), y: their emission's sum this frame */
    float puff[4][4];   /* the flame's puffs: position, w its emission (render_flame) */
    float pcol[4][4];   /* their colours (unit luminance, the black body at each one's temperature) */
} GpuLight;

/* ---- the flame (ours, from fire physics; the user's): a column of NPUFF glowing puffs. A pool fire puffs at
 * f = 1.5 / sqrt(D) a second (D its width in metres: Cetegen & Ahmed); its visible height is Heskestad's
 * L = 0.235 Q^0.4 - 1.02 D (Q its heat release in kW). A puff is born at the base each 1/f s and rises through the
 * height in that time (phase phi 0..1); it glows by its soot's incandescence, sin(pi phi)^1.5 x (T / Tmax)^4
 * (Stefan-Boltzmann), the temperature falling toward the tip, its colour the black body's (render_blackbody); it
 * meanders sideways (the flame's sway, wider higher up) and the whole column leans back from the way the flame moves,
 * tan(theta) = 0.7 sqrt(U^2 / g L) (a Froude number). The emissions are scaled so their mean sum is 1: the flicker is
 * the puffs' own coming and going. Positions in tiles: TILE_M metres a tile. ---- */
#define NPUFF 4
#define TILE_M 1.2f
static float bb_table[201][3];   /* the black body's colour, 1000-3000 K in 10 K steps */
static bool bb_ready;   /* (false again when the method or the adaptation changes) */
static void bb_at(float kelvin, float out[3]) {
    if (!bb_ready) {
        for (int i = 0; i <= 200; i++) render_blackbody(1000.0f + 10.0f * (float)i, bb_table[i]);
        bb_ready = true;
    }
    float x = (kelvin - 1000.0f) / 10.0f;
    if (x < 0) x = 0;
    if (x > 199.999f) x = 199.999f;
    int i = (int)x;
    float t = x - (float)i;
    for (int j = 0; j < 3; j++) out[j] = bb_table[i][j] * (1 - t) + bb_table[i + 1][j] * t;
}
static float puff_glow(float phi, float T, float Tmax) {
    float s = sinf(3.14159265f * phi);
    float r = T / Tmax;
    return (s > 0 ? powf(s, 1.5f) : 0) * r * r * r * r;
}
/* the puffs of a flame of width D (m) and heat Q (kW), hottest Tmax (K), at base (tiles), moving at vel (tiles a
   second, x and z), at time t (s), seed its own; the emissions times scale */
static float g_puff_depth = 0.15f;   /* the flame's coherent swell (UWVIEW_PUFF for checks, settings flame_swell) */
void render_set_puff_depth(float d) { g_puff_depth = d < 0 ? 0 : d > 0.5f ? 0.5f : d; }
/* a campfire's own swell (the user, 2026-11-14: deeper pulses, a more dramatic fire): the flames 0.4 m across and
 * wider (a campfire is 0.5) swell and neck this deep instead (settings campfire_swell, UWVIEW_CAMPFIRE for checks;
 * 0.35: the light about half as bright again at a puff's crest and half at its neck); the torches keep flame_swell */
static float g_camp_depth = 0.35f;
void render_set_campfire_depth(float d) { g_camp_depth = d < 0 ? 0 : d > 0.6f ? 0.6f : d; }
/* the flame's slow breath (the user, 2026-11-16: "a deeper slow breathing ... once in a while"). What is measured
 * of real flames: besides the puffing (f = 1.5 / sqrt(D), Cetegen and Ahmed 1993) their light carries slow, aperiodic
 * changes below about 1 Hz, from the flame's shape changing with the air round it and the fuel reaching it (a lull
 * starves it: shorter, dimmer, cooler; a fresh pocket of fuel flares). So not a steady pulse: now and then, each flame
 * at its own times, one swell 3 to 6 s long, two in three a sag and one a flare; about half of every 9 s has none.
 * Returns -1 (the deepest sag) .. 0.6 (a flare); render_flame takes g_breath_depth of it (settings flame_breath, the
 * Esc menu's Fire breathing row; UWVIEW_BREATH for checks), a campfire half as much again, a candle's or a lantern's
 * flame a quarter (sheltered, small) */
static float g_breath_depth = 0.20f;
void render_set_breath_depth(float d) { g_breath_depth = d < 0 ? 0 : d > 0.4f ? 0.4f : d; }
float render_breath_depth(void) { return g_breath_depth; }
static float flame_breath(float t, float seed) {
    float u = t / 9.0f + seed * 17.0f, b = 0;
    for (int k = -1; k <= 0; k++) {   /* (a swell begun in the window before can still be running) */
        float w = floorf(u) + (float)k;
        unsigned h = (unsigned)(long long)w * 2654435761u ^ (unsigned)(seed * 1e6f) * 40503u;
        h ^= h >> 15, h *= 2246822519u, h ^= h >> 13, h *= 3266489917u, h ^= h >> 16;
        if ((h & 0xFF) >= 128) continue;
        float start = w + (float)((h >> 8) & 0xFF) / 255.0f, len = (3.0f + 3.0f * (float)((h >> 16) & 0xFF) / 255.0f) / 9.0f;
        float x = (u - start) / len;
        if (x <= 0 || x >= 1) continue;
        float s = sinf(3.14159265f * x);
        b += (((h >> 24) % 3) ? -1.0f : 0.6f) * s * s;
    }
    return b < -1 ? -1 : b > 0.6f ? 0.6f : b;
}
static void render_flame(GpuLight *g, const float base[3], float D, float Q, float Tmax, float seed, const float vel[2], float t,
                         float scale) {
    float f = 1.5f / sqrtf(D), L = 0.235f * powf(Q, 0.4f) - 1.02f * D;
    if (L < 2.0f * D) L = 2.0f * D;
    /* its slow breath (flame_breath): the flame shorter, dimmer and cooler in a sag, the other way in a flare */
    float br = g_breath_depth * (D >= 0.4f ? 1.5f : D < 0.03f ? 0.25f : 1.0f) * flame_breath(t, seed);
    L *= 1.0f + 0.5f * br, Tmax += 150.0f * br, scale *= 1.0f + br;
    /* the mean sum of the emissions over a period (the puffs evenly spaced in phase) */
    float mean = 0;
    for (int s = 0; s < 64; s++) {
        float phi = (float)s / 64.0f;
        mean += puff_glow(phi, Tmax - 400.0f * powf(phi, 1.3f), Tmax);
    }
    mean = mean / 64.0f * NPUFF;
    if (mean < 1e-6f) mean = 1;
    /* the lean */
    float U = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]) * TILE_M, tl = 0.7f * sqrtf(U * U / (9.81f * L)), ux = 0, uz = 0;
    if (tl > 1.5f) tl = 1.5f;
    if (U > 1e-3f) ux = -vel[0] * TILE_M / U, uz = -vel[1] * TILE_M / U;
    float small = D < 0.03f ? 0.25f : 1.0f;   /* a candle's or a lantern's flame hardly sways in still air */
    float sum = 0, csum[3] = {0, 0, 0};
    for (int j = 0; j < NPUFF; j++) {
        float phase = t * f + (float)j / NPUFF + seed, phi = phase - floorf(phase);
        float T = Tmax - 400.0f * powf(phi, 1.3f);
        float turb = 1.0f + 0.25f * small * sinf(6.2832f * (t * f * 2.3f + (float)j * 0.37f + seed));
        /* each shed puff its own size (the fuel it took in: real fires' light varies 10-20%): a strength 0.55-1.45 for
           this puff's life, from its birth's number; a small flame's less */
        unsigned hb = (unsigned)(long long)floorf(phase) * 2654435761u ^ (unsigned)(j * 40503) ^ (unsigned)(seed * 1e6f);
        hb ^= hb >> 15, hb *= 2246822519u, hb ^= hb >> 13;
        float strength = 1.0f + small * 0.9f * ((float)(hb & 0xFFFF) / 65535.0f - 0.5f);
        /* the whole flame swells and necks once a puff (its luminous area at the puffing frequency), coherent across the
           puffs, so it does not average out: g_puff_depth deep (the user: 12-16% on the screen) */
        float swell = 1.0f + small * (D >= 0.4f ? g_camp_depth : g_puff_depth) * (sinf(6.2832f * (t * f + seed)) + 0.4f * sinf(6.2832f * (t * f * 2.7f + seed * 1.7f)));
        float w = puff_glow(phi, T, Tmax) * turb * strength * swell / mean * scale;
        float y = phi * L, A = small * (0.18f * D + 0.06f * L * phi);
        float ox = A * sinf(6.2832f * (t * f * 0.31f + seed * 7.1f + phi * 0.9f));
        float oz = A * sinf(6.2832f * (t * f * 0.27f + seed * 3.7f + phi * 1.3f) + 1.1f);
        g->puff[j][0] = base[0] + (ox + ux * y * tl) / TILE_M;
        g->puff[j][1] = base[1] + y / TILE_M;
        g->puff[j][2] = base[2] + (oz + uz * y * tl) / TILE_M;
        g->puff[j][3] = w;
        bb_at(T, g->pcol[j]);
        g->pcol[j][3] = T;
        sum += w;
        for (int c = 0; c < 3; c++) csum[c] += w * g->pcol[j][c];
    }
    g->info[0] = NPUFF, g->info[1] = sum;
    for (int c = 0; c < 3; c++) g->col[c] = csum[c];   /* (the sum, for anything that takes the light as a point) */
}
static float g_torch_vel[2];
void render_set_torch_motion(float vx, float vz) { g_torch_vel[0] = vx, g_torch_vel[1] = vz; }

static void mat_mul(const float *a, const float *b, float *o) {
    float t[16];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) t[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + a[r * 4 + 3] * b[12 + c];
    memcpy(o, t, sizeof t);
}

/* world: x east, y up, z north. view: right, up, forward rows; depth = distance along forward.
 * jx, jy: a shift of the whole picture in clip space (the DLSS jitter) */
/* the part of the output the camera's view fills (fractions of it, from the top left): the whole by default; the
 * original game's port puts the view in its 3D window, the interface layer covering the rest */
static float g_vr[4] = {0, 0, 1, 1};
/* INSTANCE.ASM's scale_matrix: the original's view depth scaled by its zoom / 8000h before SMOOTH.ASM measures a
 * corner's distance for its shade (0.76 at the usual zoom); 1 until the game says */
static float g_shade_k = 0.7629f;
void render_set_shade_scale(float k) { if (k > 0.1f && k < 4) g_shade_k = k; }
/* full bright (Shift+L, the user): every surface lit fully, at any distance; the shader is told by rtlim.w < 0 */
static bool g_fullbright;
void render_set_fullbright(bool on) { g_fullbright = on; }

/* A flame's colour from its temperature (the physics: its light is mostly soot glowing as a black body): Planck's
 * law through the CIE 1931 functions to XYZ; the eye's adaptation by a chromatic adaptation transform (CAT16, CAM16's,
 * the default; or Bradford), a degree D of the way from a wood fire's white (1800 K) to D65's; then linear sRGB, a
 * colour outside it brought in by lowering its OKLCh chroma (its lightness and hue kept); unit luminance.
 * D: CAM16's D = F (1 - exp((-La - 42) / 92) / 3.6) for a torch-lit dungeon (La about 5 cd/m2, a dim surround F 0.9:
 * 0.75), or the settings' fixed share. Until 2026-11-15 the negative sRGB blue was clipped to 0 first and the
 * adaptation done channel by channel after: firelight had no blue at any adaptation (removed, the user). */
static float g_adapt = -1.0f;   /* the eye's adaptation, 0..1; < 0 automatic (settings light_adaptation, "auto") */
static int g_cat;                /* 0 CAT16, 1 Bradford (settings light_colour) */
void render_set_adaptation(float a) { g_adapt = a < 0 ? -1.0f : a > 1 ? 1 : a; bb_ready = false; }
float render_adaptation(void) { return g_adapt; }
void render_set_light_cat(int cat) { g_cat = cat ? 1 : 0; bb_ready = false; }
int render_light_cat(void) { return g_cat; }
static double cam16_degree(double La, double F) {
    double d = F * (1.0 - exp((-La - 42.0) / 92.0) / 3.6);
    return d < 0 ? 0 : d > 1 ? 1 : d;
}
float render_adaptation_auto(void) { return (float)cam16_degree(5.0, 0.9); }
static float g3(float x, float mu, float s1, float s2) { float t = (x - mu) / (x < mu ? s1 : s2); return expf(-0.5f * t * t); }
/* the black body at kelvin as CIE XYZ, scaled to Y = 1 (Planck's law through Wyman, Sloan & Shirley's fit) */
static void bb_xyz(double kelvin, double out[3])
{
    double X = 0, Y = 0, Z = 0;
    for (int nm = 380; nm <= 780; nm += 5) {
        float l = (float)nm;
        double lm = nm * 1e-9, b = 1.0 / (lm * lm * lm * lm * lm * (exp(0.0143877735 / (lm * kelvin)) - 1.0));
        X += b * (1.056 * g3(l, 599.8f, 37.9f, 31.0f) + 0.362 * g3(l, 442.0f, 16.0f, 26.7f) - 0.065 * g3(l, 501.1f, 20.4f, 26.2f));
        Y += b * (0.821 * g3(l, 568.8f, 46.9f, 40.5f) + 0.286 * g3(l, 530.9f, 16.3f, 31.1f));
        Z += b * (1.217 * g3(l, 437.0f, 11.8f, 36.0f) + 0.681 * g3(l, 459.0f, 26.0f, 13.8f));
    }
    out[0] = X / Y, out[1] = 1.0, out[2] = Z / Y;
}
static void mat3_inv(const double m[3][3], double o[3][3])
{
    double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                 m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            int r0 = (j + 1) % 3, r1 = (j + 2) % 3, c0 = (i + 1) % 3, c1 = (i + 2) % 3;   /* the cofactor, transposed */
            o[i][j] = (m[r0][c0] * m[r1][c1] - m[r0][c1] * m[r1][c0]) / det;
        }
}
/* OKLab (Ottosson 2020) from linear sRGB and back */
static void oklab_from(const double c[3], double o[3])
{
    double l = 0.4122214708 * c[0] + 0.5363325363 * c[1] + 0.0514459929 * c[2];
    double m = 0.2119034982 * c[0] + 0.6806995451 * c[1] + 0.1073969566 * c[2];
    double s = 0.0883024619 * c[0] + 0.2817188376 * c[1] + 0.6299787005 * c[2];
    l = cbrt(l), m = cbrt(m), s = cbrt(s);
    o[0] = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s;
    o[1] = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s;
    o[2] = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s;
}
static void oklab_to(double L, double A, double B, double c[3])
{
    double l = L + 0.3963377774 * A + 0.2158037573 * B, m = L - 0.1055613458 * A - 0.0638541728 * B,
           s = L - 0.0894841775 * A - 1.2914855480 * B;
    l = l * l * l, m = m * m * m, s = s * s * s;
    c[0] = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    c[1] = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    c[2] = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
}
static void blackbody_adapted(float kelvin, float rgb[3])
{
    static const double MS[2][3][3] = {
        {{0.401288, 0.650173, -0.051461}, {-0.250268, 1.204414, 0.045854}, {-0.002079, 0.048952, 0.953127}},   /* CAT16 */
        {{0.8951, 0.2664, -0.1614}, {-0.7502, 1.7135, 0.0367}, {0.0389, -0.0685, 1.0296}}};                   /* Bradford */
    static const double D65[3] = {0.95047, 1.0, 1.08883};
    static double MI[2][3][3], wsrc[2][3], wdst[2][3];
    static bool ready;
    const double (*M)[3] = MS[g_cat];
    double c[3], l[3], x[3], D = g_adapt < 0 ? cam16_degree(5.0, 0.9) : g_adapt;
    if (!ready) {
        double w[3];
        bb_xyz(1800.0, w);
        for (int k = 0; k < 2; k++) {
            mat3_inv(MS[k], MI[k]);
            for (int i = 0; i < 3; i++) wsrc[k][i] = MS[k][i][0] * w[0] + MS[k][i][1] * w[1] + MS[k][i][2] * w[2],
                                        wdst[k][i] = MS[k][i][0] * D65[0] + MS[k][i][1] * D65[1] + MS[k][i][2] * D65[2];
        }
        ready = true;
    }
    bb_xyz(kelvin, c);
    for (int i = 0; i < 3; i++) l[i] = (M[i][0] * c[0] + M[i][1] * c[1] + M[i][2] * c[2]) * pow(wdst[g_cat][i] / wsrc[g_cat][i], D);
    for (int i = 0; i < 3; i++) x[i] = MI[g_cat][i][0] * l[0] + MI[g_cat][i][1] * l[1] + MI[g_cat][i][2] * l[2];
    double v[3] = {3.2406 * x[0] - 1.5372 * x[1] - 0.4986 * x[2], -0.9689 * x[0] + 1.8758 * x[1] + 0.0415 * x[2],
                   0.0557 * x[0] - 0.2040 * x[1] + 1.0570 * x[2]};
    if (v[0] < 0 || v[1] < 0 || v[2] < 0) {   /* outside sRGB: the most chroma (OKLCh) that is inside, hue and lightness kept */
        double lab[3], t[3], lo = 0, hi = 1;
        oklab_from(v, lab);
        for (int it = 0; it < 30; it++) {
            double k = 0.5 * (lo + hi);
            oklab_to(lab[0], lab[1] * k, lab[2] * k, t);
            if (t[0] >= 0 && t[1] >= 0 && t[2] >= 0) lo = k; else hi = k;
        }
        oklab_to(lab[0], lab[1] * lo, lab[2] * lo, v);
        for (int i = 0; i < 3; i++) if (v[i] < 0) v[i] = 0;   /* (rounding) */
    }
    double y = 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2];
    if (y <= 0) { rgb[0] = rgb[1] = rgb[2] = 1; return; }
    rgb[0] = (float)(v[0] / y), rgb[1] = (float)(v[1] / y), rgb[2] = (float)(v[2] / y);
}
void render_blackbody(float kelvin, float rgb[3]) { blackbody_adapted(kelvin, rgb); }
static float g_torch_rgb[3] = {1, 1, 1};
void render_set_torch_colour(const float rgb[3]) { memcpy(g_torch_rgb, rgb, 12); }
static bool g_torch_steady;
void render_set_torch_steady(bool on) { g_torch_steady = on; }
bool render_fullbright(void) { return g_fullbright; }
/* the lens as the tangents of the half angles across and up (0: the camera's fov_y and the rectangle's shape) */
static float g_lens[2];
static int g_debug; /* the checks: 1 the shading normals as colours */
/* the scene's tint (render_set_tint), and the atlas's top rows to rewrite (render_update_atlas: two upload buffers
 * in turn, the copy made at the next frame's start) */
static float g_tint[4];
void render_set_tint(float r, float g, float b, bool on) { g_tint[0] = r, g_tint[1] = g, g_tint[2] = b, g_tint[3] = on ? 1.0f : 0.0f; }

void render_set_debug(int what) { g_debug = what; }
void render_set_lens(float tan_x, float tan_y) { g_lens[0] = tan_x, g_lens[1] = tan_y; }
void render_set_view_rect(float x0, float y0, float x1, float y1) {
    if (x1 - x0 < 0.01f || y1 - y0 < 0.01f) x0 = y0 = 0, x1 = y1 = 1;
    g_vr[0] = x0, g_vr[1] = y0, g_vr[2] = x1, g_vr[3] = y1;
}
static void view_proj(const Camera *cam, int w, int h, float jx, float jy, float *out, float *view_out, float *proj_out) {
    float cy = cosf(cam->yaw), sy = sinf(cam->yaw), cp = cosf(cam->pitch), sp = sinf(cam->pitch);
    float f[3] = {sy * cp, sp, cy * cp};
    float r[3] = {cy, 0, -sy};
    float u[3] = {f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2], f[0] * r[1] - f[1] * r[0]};
    if (cam->roll != 0) { /* rolled about the view direction */
        float c = cosf(cam->roll), s = sinf(cam->roll), r2[3], u2[3];
        for (int k = 0; k < 3; k++) r2[k] = r[k] * c + u[k] * s, u2[k] = u[k] * c - r[k] * s;
        memcpy(r, r2, sizeof r2), memcpy(u, u2, sizeof u2);
    }
    const float *p = cam->pos;
    float v[16] = {r[0], r[1], r[2], -(r[0] * p[0] + r[1] * p[1] + r[2] * p[2]),
                   u[0], u[1], u[2], -(u[0] * p[0] + u[1] * p[1] + u[2] * p[2]),
                   f[0], f[1], f[2], -(f[0] * p[0] + f[1] * p[1] + f[2] * p[2]),
                   0, 0, 0, 1};
    /* the view's rectangle: the frustum fitted to it (its shape), its centre moved there */
    float rw = g_vr[2] - g_vr[0], rh = g_vr[3] - g_vr[1], cx = g_vr[0] + g_vr[2] - 1.0f, cyc = 1.0f - (g_vr[1] + g_vr[3]);
    float aspect = (float)w * rw / ((float)h * rh), n = 0.02f, fa = 200.0f;
    float ys = 1.0f / tanf(cam->fov_y * 0.5f), xs = ys / aspect;
    if (g_lens[0] > 0 && g_lens[1] > 0) xs = 1.0f / g_lens[0], ys = 1.0f / g_lens[1];
    float pr[16] = {xs * rw, 0, jx + cx, 0, 0, ys * rh, jy + cyc, 0, 0, 0, fa / (fa - n), -n * fa / (fa - n), 0, 0, 1, 0};
    mat_mul(pr, v, out);
    if (view_out) memcpy(view_out, v, sizeof v);
    if (proj_out) memcpy(proj_out, pr, sizeof pr);
}
static float fwd_of(const Camera *cam, int k) {
    return k == 0 ? sinf(cam->yaw) * cosf(cam->pitch) : k == 1 ? sinf(cam->pitch) : cosf(cam->yaw) * cosf(cam->pitch);
}
static void transpose(const float *a, float *o) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) o[c * 4 + r] = a[r * 4 + c];
}

static float halton(int i, int base) {
    float f = 1, r = 0;
    for (; i > 0; i /= base) {
        f /= (float)base;
        r += f * (float)(i % base);
    }
    return r;
}
