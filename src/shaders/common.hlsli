// What the scene and the light probes share: the level's textures and tile grid, light, the probes.
#include "cbuf.hlsli"

Texture2D<float4> atlas : register(t0);   // textures (1024)
Texture2D<float4> natlas : register(t1);  // normal maps + height (4096: the same places, 4x)
Texture2D<uint4> grid : register(t2);     // per tile: type, floor height (steps)
Texture2D<float4> snatlas : register(t9); // the pictures' normal maps (objects, creatures, doors): the atlas's places, 1x
Texture2D<float> saoatlas : register(t11); // their occlusion maps (1: none), the same places
// the walls' and floors' upscaled textures (the Esc menu's Textures, the user 2026-11-14): the atlas's rows 0-959 at
// texres.x times the size; a texel whose alpha is below 1/2 is one of the cycling colours (water, lava): alpha x 255 is
// its palette entry, whose colour now uwview.c keeps in the atlas's row 1023 (x = the entry)
Texture2D<float4> hatlas : register(t12);

static const float STEP = 0.25, CEIL = 4.0;

float3 lin(float3 c) { return pow(max(c, 0), 2.2); }
float lum(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

// a texel of the atlas / the normal atlas, the texture repeating inside its rectangle (texel units)
float4 atlas_at(float4 rect, float2 q) {
    float2 w = q - rect.zw * floor(q / rect.zw);
    if (texres.z > 0 && rect.y < 960) {
        float f = texres.x;
        float4 h = hatlas.Load(int3(int2(rect.xy * f + min(w * f, rect.zw * f - 0.001)), 0));
        if (h.a < 0.5) return float4(atlas.Load(int3(int(h.a * 255 + 0.5), 1023, 0)).rgb, 1);
        return h;
    }
    return atlas.Load(int3(int2(rect.xy + min(w, rect.zw - 0.001)), 0));
}
float4 snatlas_at(float4 rect, float2 q) {
    float2 w = q - rect.zw * floor(q / rect.zw);
    return snatlas.Load(int3(int2(rect.xy + min(w, rect.zw - 0.001)), 0));
}
float saoatlas_at(float4 rect, float2 q) {
    float2 w = q - rect.zw * floor(q / rect.zw);
    return saoatlas.Load(int3(int2(rect.xy + min(w, rect.zw - 0.001)), 0));
}
// the upscaled textures' own relief maps (the user, 2026-11-15: made from the 256 / 512 textures at their size,
// tools\make_upscaled_maps.py; texres.y > 0: there): in hatlas to the right of the colours (1024 x texres.x texels
// along), the same places: r, g the normal's x, y (its z worked out), b the occlusion, a the height
float4 hmaps_at(float4 rect, float2 w) {
    float f = texres.x;
    return hatlas.Load(int3(int2(rect.xy * f + min(w * f, rect.zw * f - 0.001)) + int2(int(1024 * f), 0), 0));
}
float4 natlas_at(float4 rect, float2 q) {
    float2 w = q - rect.zw * floor(q / rect.zw);
    if (texres.y > 0 && rect.y < 960) {
        float4 h = hmaps_at(rect, w);
        float2 n = h.rg * 2 - 1;
        return float4(h.rg, sqrt(saturate(1 - dot(n, n))) * 0.5 + 0.5, h.a);
    }
    // (the atlas's blue is the occlusion, texao_at: the normal's z from its x, y)
    float4 t = natlas.Load(int3(int2(rect.xy * 4 + min(w * 4, rect.zw * 4 - 0.001)), 0));
    float2 n = t.rg * 2 - 1;
    return float4(t.rg, sqrt(saturate(1 - dot(n, n))) * 0.5 + 0.5, t.a);
}
// a wall's or a floor's occlusion (NNN_o.png in the pictures' AO atlas; the upscaled textures' own when there)
float texao_at(float4 rect, float2 q) {
    if (texres.y > 0 && rect.y < 960) return hmaps_at(rect, q - rect.zw * floor(q / rect.zw)).b;
    // the original textures': the normal atlas's blue, at its 4x (the user, 2026-11-16: 256 maps on the 64 texel walls)
    float2 w = q - rect.zw * floor(q / rect.zw);
    return natlas.Load(int3(int2(rect.xy * 4 + min(w * 4, rect.zw * 4 - 0.001)), 0)).b;
}

// --- the tile grid ---------------------------------------------------------------------------------
bool open_at(uint k, float2 l) {
    if (k == 0) return false;
    if (k == 2) return l.y <= l.x;
    if (k == 3) return l.x + l.y <= 1;
    if (k == 4) return l.x + l.y >= 1;
    if (k == 5) return l.y >= l.x;
    return true;
}
float floor_at(uint4 t, float2 l) {
    float h = t.y;
    if (t.x == 6) h += l.y; else if (t.x == 7) h += 1 - l.y; else if (t.x == 8) h += l.x; else if (t.x == 9) h += 1 - l.x;
    return h * STEP;
}
uint4 tile(int2 c) { return any(c < 0) || any(c > 63) ? uint4(0, 0, 0, 0) : grid.Load(int3(c, 0)); }
bool blocked(float3 p) {
    int2 c = int2(floor(p.xz));
    uint4 t = tile(c);
    float2 l = p.xz - c;
    return !open_at(t.x, l) || p.y < floor_at(t, l) - 0.002 || p.y > CEIL;
}
// a shadow ray marched through the grid (when there is no hardware ray tracing)
float visible_grid(float3 a, float3 b) {
    float3 d = b - a;
    float len = length(d);
    int n = min(160, (int)(len / 0.06) + 1);
    [loop] for (int k = 1; k < n; k++) {
        float t = (float)k / (float)n;
        if (t * len > len - 0.06) break;
        if (blocked(a + d * t)) return 0;
    }
    return 1;
}

// --- light -------------------------------------------------------------------------------------------
// The lights: 0 is the player's torch (col.w = 1 when lit), the rest the placed ones. Each tile has a
// list of the (up to 8) placed lights that reach it, the ones it can see first.
// position + range; colour x intensity, w = on; info.x puffs (a flame), y their emission's sum; puff[]: position,
// w emission; pcol[]: colour (render.c's render_flame)
struct GLight { float4 pr; float4 col; float4 info; float4 puff[4]; float4 pcol[4]; };
StructuredBuffer<GLight> lights : register(t6);
StructuredBuffer<uint> tile_lights : register(t7);
static const uint NO_LIGHT = 0xFFFFFFFFu;

// light m (0 = the torch, 1-8 = the tile's list) for a point just off a surface (inside its open tile)
uint light_at(float3 p, uint m) {
    if (m == 0) return lights[0].col.w > 0 ? 0 : NO_LIGHT - 1;
    int2 c = int2(floor(p.xz));
    if (any(c < 0) || any(c > 63)) return NO_LIGHT;
    return tile_lights[(c.y * 64 + c.x) * 8 + m - 1];
}

// what one light gives a surface (before the shadow): colour x falloff x the angle. A light has a size
// (SOFT in the falloff), so it does not blow up right next to it. The player's torch (light 0) is the
// one a wall can come right up to: its size is larger, and scaled so a surface a tile away gets what it
// did with the small size; right against a wall it is at most about 2.5 times that, not 8.
// The original's light (map\LIGHTING.C set_light; 3d\SMOOTH.ASM smooth_over, which shades the textured walls,
// floors and ceilings; 3d\VIEW3D.C preset_grid, which decides which cells are drawn at all). smooth_over: each
// corner's shade from its distance to the eye in eighths of a tile (the view vector >> 5), (8 d * smooth_div >> 6)
// + smooth_lowpass, at least 0, + smooth_base, at most 15, blended across the face (Gouraud); shade k is LIGHT.DAT's
// map k. preset_grid: cells farther than curvrad tiles (11.3137 d over the grid) are not drawn. SHADES.DAT's rows
// (smooth_div, smooth_base, smooth_lowpass, curvrad) for light levels 0-7; LIGHTDAT how bright each of LIGHT.DAT's
// 16 maps leaves the palette (measured: the luminance of every mapped colour over the colour's own).
static const float4 SHADES[8] = {float4(56, 5, -3, 3), float4(46, 4, -3, 3), float4(39, 2, -2, 4), float4(30, 1, 0, 4),
                                 float4(28, 0, 0, 5), float4(21, 0, 0, 6), float4(18, 0, 0, 7), float4(18, 0, 0, 7)};
static const float LIGHTDAT[16] = {1.000, 0.934, 0.885, 0.826, 0.764, 0.710, 0.650, 0.591,
                                   0.535, 0.478, 0.422, 0.365, 0.315, 0.261, 0.206, 0.140};
// d3: the distance to the eye; dh: across the floor (the grid's cells)
// The user's reach (as our own version had it): the light dies smoothly by 3 tiles across the floor from the eye,
// and the bounced light with it (the light's range); no cell steps (the original's whole-cell cut-off left the
// floor ending in a hard line before the walls). dh: the distance across the floor.
static const float TORCH_REACH = 3.0;
float original_light(float d3, float dh, uint level) {
    // level 0, no light carried: the player's eyes (the user, 2026-11-11), a fifth shorter: 2.4 tiles, not 3
    if ((level & 7) == 0) { d3 *= 1.25; dh *= 1.25; }
    float4 s = SHADES[level & 7];
    float R = s.w;   // the original's reach for the light level (SHADES.DAT's curvrad: 3 3 4 4 5 6 7 7 tiles; a torch 4)
    if (dh >= R) return 0;
    float shade = min(max(0, 8 * d3 * s.x / 64 + s.z) + s.y, 15);
    uint i = min((uint)shade, 14);
    return lerp(LIGHTDAT[i], LIGHTDAT[i + 1], saturate(shade - i)) * (1 - smoothstep(R - 0.75, R, dh));   // (the last 3/4 tile eased: no hard line)
}

float rnd(inout uint s);
// a flame's puff, picked in proportion to its emission (the Monte Carlo estimate of the light from the whole flame:
// the pick's light times the sum of the emissions); 0 and W 1 for a point light
uint pick_puff(uint k, inout uint seed, out float W) {
    W = lights[k].info.y;
    if (lights[k].info.x < 0.5 || W <= 1e-6) { W = 1; return 4; }
    float r = rnd(seed) * W, acc = 0;
    [loop] for (uint j = 0; j < 3; j++) {
        acc += lights[k].puff[j].w;
        if (r < acc) return j;
    }
    return 3;
}
float3 light_term(uint k, float3 p, float3 Ng, float3 N, out float3 L, out float d, inout uint seed, out float3 src) {
    float4 pr = lights[k].pr;
    float W;
    uint pj = pick_puff(k, seed, W);
    src = pj < 4 ? lights[k].puff[pj].xyz : pr.xyz;
    float3 Lv = pr.xyz - p;
    float d2 = dot(Lv, Lv);
    d = sqrt(d2);
    L = Lv / max(d, 1e-4);
    if (k == 0 && lights[0].col.w > 1.5) {
        // the player's light as the original's (render_set_torch_original): col.rgb the eye, col.w 2 + the light
        // level + the output's exponent / 10. The brightness by the distance to the eye across the floor (the grid's
        // cells), raised to the exponent so that on the screen it is LIGHT.DAT's; the normal maps lean it about the
        // torch's direction without changing it on a flat face; every face lit, as the original lights them
        float lv = lights[0].col.w - 2, ex = frac(lv) * 10;
        if (rtlim.w < 0) {   // full bright (Shift+L): every surface fully lit, the normal maps still leaning it
            float ng0 = dot(Ng, L);
            return ng0 > 0.15 ? clamp(saturate(dot(N, L)) / ng0, 0, 1.6) : 1;
        }
        float dh = length(p.xz - lights[0].col.xz);
        // the distance as smooth_over measures it: the depth along the view scaled by the zoom (rtlim.w),
        // the rest as it is (INSTANCE.ASM's scale_matrix)
        float3 v = p - lights[0].col.xyz;
        float depth = mul(vp_nojit, float4(p, 1)).w, side2 = max(dot(v, v) - depth * depth, 0);
        float k = rtlim.w > 0 ? rtlim.w : 1;
        float b = pow(original_light(sqrt(side2 + depth * depth * k * k), dh, (uint)lv), ex);
        if (pj < 4) {   // the flame: the picked puff's light against the centre's (the original's falloff kept on average)
            float3 Lj = src - p;
            float dj2 = dot(Lj, Lj);
            Lj *= rsqrt(max(dj2, 1e-8));
            // the puff nearer or farther than the centre, by distance alone: the original lights every face the same
            // whatever its angle, so no cosine here (a wall beside the player, grazed by the centre, blew out to 3x)
            float ratio = clamp((d2 + 0.6) / (dj2 + 0.6), 0.75, 1.3);
            L = Lj;
            float ngj = dot(Ng, L), leanj = ngj > 0.15 ? clamp(saturate(dot(N, L)) / ngj, 0, 1.6) : 1;
            return b * leanj * ratio * W * lights[0].pcol[pj].rgb;
        }
        float ng = dot(Ng, L), lean = ng > 0.15 ? clamp(saturate(dot(N, L)) / ng, 0, 1.6) : 1;
        return b * lean * pr.w * torchc.rgb;   // pr.w: the flame's flicker (uwview.c); torchc: its colour (black body)
    }
    if (d > pr.w) return 0;
    float w = saturate(1 - pow(d / pr.w, 4));
    float soft = k == 0 ? 0.6 : 0.15, scale = k == 0 ? (1 + 0.6) / (1 + 0.15) : 1;
    if (pj < 4) {   // a flame: the picked puff's light, cosine and inverse square from where it is, times the sum
        float3 Lj = src - p;
        float dj2 = dot(Lj, Lj);
        L = Lj * rsqrt(max(dj2, 1e-8));
        if (dot(Ng, L) <= 0) return 0;
        return lights[k].pcol[pj].rgb * W * saturate(dot(N, L)) * w * w * scale / (dj2 + soft);
    }
    if (dot(Ng, L) <= 0) return 0;
    return lights[k].col.rgb * saturate(dot(N, L)) * w * w * scale / (d2 + soft);
}

// How far inside some light's reach a point is: 1 near a light, falling to 0 at the edge of its reach the way
// its own light does. Bounced light is kept to this: in the original nothing past a light's reach is seen (the
// torch: SHADES.DAT, 4 tiles), and light bounced on down a corridor frame after frame lit it all the way along.
float reach_window(float3 p) {
    float r = 0;
    [loop] for (uint m = 0; m <= 8; m++) {
        uint k = light_at(p, m);
        if (k == NO_LIGHT) break;
        if (k == NO_LIGHT - 1) continue;
        if (k == 0 && lights[0].col.w > 1.5 && rtlim.w < 0) { r = 1; continue; }   // full bright: everywhere
        if (k == 0 && lights[0].col.w > 1.5) {   // the player's light: 3 tiles across the floor from the eye, as it lights
            float R = SHADES[(uint)(lights[0].col.w - 2) & 7].w;   // its level's reach, as it lights
            r = max(r, 1 - smoothstep(R - 0.75, R, length(p.xz - lights[0].col.xz)));
            continue;
        }
        float d = length(lights[k].pr.xyz - p) / max(lights[k].pr.w, 1e-3);
        r = max(r, saturate(1 - d * d * d * d));
    }
    return r;
}

// --- the light probes: bounced light ------------------------------------------------------------------
// One column per tile, PL layers from just above the floor; each holds the light arriving from six
// directions (+x -x +y -y +z -z), as light per unit of surface colour (what a white wall facing that way
// would send back).
static const int PL = 6;
static const float PROBE_BASE = 0.35, PROBE_GAP = 0.7;
float2 probe_local(uint k) { // inside a diagonal tile: the middle of its open half
    if (k == 2) return float2(0.67, 0.33);
    if (k == 3) return float2(0.33, 0.33);
    if (k == 4) return float2(0.67, 0.67);
    if (k == 5) return float2(0.33, 0.67);
    return float2(0.5, 0.5);
}
float probe_floor(uint4 t) { return floor_at(t, probe_local(t.x)); }
bool probe_ok(uint4 t, int layer) { return t.x != 0 && probe_floor(t) + PROBE_BASE + PROBE_GAP * layer < CEIL - 0.05; }
float3 probe_pos(int2 c, int layer) {
    uint4 t = tile(c);
    float2 l = probe_local(t.x);
    return float3(c.x + l.x, probe_floor(t) + PROBE_BASE + PROBE_GAP * layer, c.y + l.y);
}
uint probe_index(int2 c, int layer) { return ((uint)(c.y * 64 + c.x) * PL + layer) * 6; }

float3 cube_eval(StructuredBuffer<float4> buf, uint base, float3 n) {
    float3 n2 = n * n;
    return n2.x * buf[base + (n.x > 0 ? 0 : 1)].rgb + n2.y * buf[base + (n.y > 0 ? 2 : 3)].rgb + n2.z * buf[base + (n.z > 0 ? 4 : 5)].rgb;
}

// the bounced light at p (normal n): the probes of the four tile columns round it, two layers each,
// leaving out rock and probes behind the surface
float3 bounce_at(StructuredBuffer<float4> buf, float3 p, float3 n) {
    float2 f = p.xz - 0.5;
    int2 c0 = int2(floor(f));
    float2 a = f - c0;
    float3 sum = 0;
    float wsum = 0;
    [unroll] for (int j = 0; j < 4; j++) {
        int2 c = c0 + int2(j & 1, j >> 1);
        uint4 t = tile(c);
        if (t.x == 0) continue;
        float wxz = ((j & 1) ? a.x : 1 - a.x) * ((j >> 1) ? a.y : 1 - a.y);
        float fl = (p.y - (probe_floor(t) + PROBE_BASE)) / PROBE_GAP;
        int k0 = clamp((int)floor(fl), 0, PL - 1), k1 = min(k0 + 1, PL - 1);
        float ak = saturate(fl - k0);
        [unroll] for (int m = 0; m < 2; m++) {
            int k = m ? k1 : k0;
            if (!probe_ok(t, k)) continue;
            float3 pp = probe_pos(c, k);
            float facing = saturate(dot(n, normalize(pp - p + n * 0.001)) * 0.5 + 0.6);
            float w = max(wxz, 1e-3) * (m ? ak : 1 - ak) * facing;
            if (w <= 0) continue;
            sum += cube_eval(buf, probe_index(c, k), n) * w;
            wsum += w;
        }
    }
    return wsum > 1e-5 ? sum / wsum : 0;
}

// --- noise --------------------------------------------------------------------------------------------
uint pcg(uint v) {
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
float rnd(inout uint s) { s = pcg(s); return (s & 0xFFFFFF) / 16777216.0; }
