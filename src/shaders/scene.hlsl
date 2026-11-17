// The level, lit only by the light sources. Built three times: with RT defined (hardware ray tracing: shadow
// rays against the scene + bounced light from the probes), with RT and RTHIGH (ray tracing High: a bounce ray
// from every pixel instead of the probes, and the guides DLSS Ray Reconstruction denoises it with), and
// without (shadow rays marched through the tile grid).
#include "common.hlsli"

#ifdef RT
RaytracingAccelerationStructure scene_as : register(t3);
StructuredBuffer<float4> probes : register(t4);
// the cards (objects, creatures, doors) in the ray tracing: their vertices, as render.c's Vertex
struct SVtx { float3 pos; float3 prev; float2 uv; float4 rect; float3 nrm; float4 col; };
StructuredBuffer<SVtx> sverts : register(t10);
#include "rthit.hlsli"   // (ray tracing Normal too: lit_by for reach_window_seen)
#endif

struct VI {
    float3 pos : POSITION0; float3 prev : POSITION1; float2 uv : TEXCOORD0; float4 rect : TEXCOORD1;
    float3 nrm : NORMAL; float4 col : COLOR;
};
struct VO {
    float4 pos : SV_Position; float2 uv : TEXCOORD0; nointerpolation float4 rect : TEXCOORD1;
    float3 nrm : NORMAL; float4 cur : TEXCOORD2; float4 prev : TEXCOORD3; float3 wpos : TEXCOORD4; float4 col : COLOR;
};

// colour alpha: 1 the level, 3 a picture card (an object), 4 a glowing card (a burning torch). A card's point
// is its bottom middle, nrm.xy the corner's offset (tiles, right and up); it stands upright facing the eye.
bool is_card(float a) { return a > 2.5 && a < 4.5; }

VO vs(VI i) {
    VO o;
    float3 P = i.pos, PP = i.prev, N = i.nrm;
    if (is_card(i.col.a)) {
        float3 d = eye.xyz - i.pos;
        d.y = 0;
        d = normalize(d + float3(1e-4, 0, 0));
        float3 right = float3(-d.z, 0, d.x), off = right * i.nrm.x + float3(0, i.nrm.y, 0);
        P = i.pos + off, PP = i.prev + off, N = d;
    }
    o.pos = mul(vp, float4(P, 1));
    o.cur = mul(vp_nojit, float4(P, 1));
    o.prev = mul(vp_prev, float4(PP, 1));
    o.uv = i.uv, o.rect = i.rect, o.nrm = N, o.wpos = P, o.col = i.col;
    return o;
}

// can the point a see the point b
// The light that gets from a to b: 0 behind a wall; through the cards (objects, creatures, doors: not opaque in the
// ray tracing) only where their pictures are clear; glass (kind 3.3: bottles, potions, flasks) lets it through tinted
// by the glass's colour; what glows (kind 4) casts no shadow
float3 shadow(float3 a, float3 b) {
#ifdef RT
    float3 d = b - a;
    float len = length(d);
    if (len < 0.08) return 1;
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> q;
    RayDesc r;
    r.Origin = a, r.Direction = d / len, r.TMin = 0.03, r.TMax = len - 0.06;   // (0.03: not the card being lit itself)
    q.TraceRayInline(scene_as, RAY_FLAG_NONE, 0xFF, r);
    float3 T = 1;
    while (q.Proceed()) {
        if (q.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE) continue;
        uint p = q.CandidatePrimitiveIndex();
        SVtx v0 = sverts[p * 3], v1 = sverts[p * 3 + 1], v2 = sverts[p * 3 + 2];
        float k = v0.col.a;
        if (k > 3.5 && k < 4.5) continue;                      // a flame, a spark: no shadow
        float2 bc = q.CandidateTriangleBarycentrics();
        float2 uv = v0.uv * (1 - bc.x - bc.y) + v1.uv * bc.x + v2.uv * bc.y;
        float4 c = v0.rect.z > 0 ? atlas_at(v0.rect, uv) : float4(1, 1, 1, 1);
        if (c.a < 0.5) continue;                               // the picture clear here
        if (k > 3.25 && k < 3.35) {                            // glass: its colour, as thin coloured glass passes it
            float3 g = lin(c.rgb);
            float m = max(max(g.r, g.g), max(g.b, 1e-3));
            T *= lerp(g / m, 1, 0.15) * 0.8;
            if (max(T.r, max(T.g, T.b)) < 0.02) q.CommitNonOpaqueTriangleHit();
            continue;
        }
        q.CommitNonOpaqueTriangleHit();                        // opaque: the light stops
    }
    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0 : T;
#else
    return visible_grid(a, b);
#endif
}

// The relief's own shadow on a wall or a floor (the user, 2026-11-14): how much of a light reaches the texel at uv
// past the texture's relief, by its height map (the normal atlas's alpha, NNN_p.png: 1 the stones' faces, low the
// joints). Lt: the way to the light in the texture's axes (x along u, y along v, z out of the face); depth: how far
// the map's white stands proud of its black, in texels. The ray toward the light climbs out of the relief; where the
// height map stands above it, the light is cut (softly: 1/6 of the map's range from clear to dark). Six texels far.
float relief_shadow(float4 rect, float2 uv, float3 Lt, float depth) {
    float len = length(Lt.xy);
    if (Lt.z <= 0 || len < 1e-3 || depth <= 0) return 1;
    float2 d = Lt.xy / len;
    float rise = Lt.z / len / depth;   // the ray's climb a texel, in the height map's units
    if (rise * 0.75 >= 1) return 1;    // too steep for anything to stand in its way
    float h0 = natlas_at(rect, uv).a, lit = 1;
    [unroll] for (int k = 1; k <= 8; k++) {
        float t = k * 0.75;
        lit = min(lit, 1 - saturate((natlas_at(rect, uv + d * t).a - h0 - rise * t) * 6));
    }
    return lit;
}

struct PO {
    float4 color : SV_Target0; float2 motion : SV_Target1;
#ifdef RTHIGH
    // Ray Reconstruction's guides: diffuse albedo, specular albedo (none: nothing here shines), world normal + roughness
    float4 albedo : SV_Target2; float4 spec : SV_Target3; float4 nrough : SV_Target4;
#endif
};

// the pond's bed: the texture seen through the water, softer the deeper (four taps r texels apart, in linear light)
float4 bed_at(float4 rect, float2 q, float r) {
    float4 a = atlas_at(rect, q + float2(-r, -r * 0.4)), b = atlas_at(rect, q + float2(r * 0.4, -r));
    float4 c = atlas_at(rect, q + float2(r, r * 0.4)), d = atlas_at(rect, q + float2(-r * 0.4, r));
    return float4(sqrt((a.rgb * a.rgb + b.rgb * b.rgb + c.rgb * c.rgb + d.rgb * d.rgb) * 0.25), a.a);
}

PO ps(VO i) {
    PO o;
    float2 ma = i.cur.xy / i.cur.w, mb = i.prev.xy / i.prev.w;
    o.motion = (mb - ma) * float2(0.5, -0.5) * size.xy;

    // the surface: its texture coordinates change across the world by T (u) and B (v), in texels per tile
    float3 Ng = normalize(i.nrm), N = Ng;
    float3 dp1 = ddx(i.wpos), dp2 = ddy(i.wpos);
    float2 du1 = ddx(i.uv), du2 = ddy(i.uv);
    float3 p2 = cross(dp2, Ng), p1 = cross(Ng, dp1);
    float3 T = p2 * du1.x + p1 * du2.x, B = p2 * du1.y + p1 * du2.y;
    float det = dot(dp1, p2);
    det = abs(det) > 1e-20 ? det : 1e-20;
    T /= det, B /= det;
    // the relief is geometry now (the textures' relief meshes): no parallax; the normal map adds the fine detail
    float2 uv = i.uv;
    bool card = is_card(i.col.a), wquad = i.col.a > 4.5 && i.col.a < 5.5; // a door, a door frame
    // a door frame (and a secret door) is drawn with the wall's own texture rectangle (atlas rows below 896): it takes the
    // WALL normal map like the stones round it, not the pictures' atlas (which has nothing at those places)
    bool wallpic = wquad && i.rect.y < 896;
    bool relief = linfo.y > 0 && i.rect.z > 0 && !card && (!wquad || wallpic);
    // a wall, a floor, a ceiling (not water, below): its occlusion map and its relief's shadows, with or without its
    // normal map (each its own Esc row)
    bool texrelief = i.rect.z > 0 && !card && (!wquad || wallpic);
    if (wquad && dot(Ng, eye.xyz - i.wpos) < 0) Ng = -Ng, N = -N; // seen from either side
    float4 c = (i.rect.z > 0 ? atlas_at(i.rect, uv) : float4(1, 1, 1, 1)) * float4(i.col.rgb, 1);
    if ((card || wquad) && c.a < 0.5) discard;
    // the floors' terrain (level.c: the vertex colour's alpha 1.2 water, 1.3 lava)
    bool water = i.col.a > 1.15 && i.col.a < 1.25, lava = i.col.a > 1.25 && i.col.a < 1.35;
    float tsec = eye.w / 60.0;   // the frame count as time
    // what glows of itself: lava (the original's texture, pulsing slowly, uneven across it) and the glowing pictures
    // (alpha 4: a burning torch, lantern, candle, campfire, an ankh, a spark; doors, frames and wall pictures, alpha 5,
    // are lit like the walls). Both go through the lighting with their normal maps (the user: everything normal-mapped)
    // and their glow is added on top, so their relief shows; they used to return here flat
    bool glowcard = i.col.a > 3.5 && i.col.a < 4.5;
    float3 emit = 0;
    if (lava) {
        float pulse = 0.8 + 0.2 * sin(tsec * 1.7 + i.wpos.x * 2.9 + i.wpos.z * 2.3) + 0.1 * sin(tsec * 3.1 - i.wpos.z * 5.0);
        emit = lin(c.rgb) * 2.0 * pulse;
    } else if (glowcard) emit = lin(c.rgb) * 1.3;
#ifdef RTHIGH
    o.albedo = float4(lin(c.rgb), 1), o.nrough = float4(Ng, 1);
    o.spec = lava || glowcard ? float4(0, 0, 0, 1) : float4(look.w * 0.5, look.w * 0.5, look.w * 0.5, 1);
#endif
    float4 nm = natlas_at(i.rect, uv);
    float shine = look.w, shine_pow = 24;   // the sheen: the stone's, or the water's (tight, strong)
    // (the user, 2026-11-16: the water is the original texture, flat and still, and only the rings of what touches
    // it move on it: no normal map on it, no swell, no travelling waves bending the bed, no caustics; its sheen and
    // its reflection only on the rings. false: the water as it was before)
    // (the user, 2026-11-17: "we need those water effects and the water needs to be opaque but seem like we are looking
    // into it, not the surface but below it": with the Water row on (fxw.x) the travelling waves and the caustics are
    // back, on the pond below: the waves bend the view of the bed, more the deeper it lies, the caustics play on the
    // bed, and the surface itself shows little (a faint glint, the Fresnel reflection). Water off: still, as above)
    static const bool WATER_PLAIN = true;
    float wdeep = 0;         // the bed's depth under the surface here, tiles
    float wring = 0;   // how much of a ring is here (0: still water)
    // (the user, 2026-11-16: "like looking at a pond: the surface reflects but you are seeing into the water too"):
    // the texture is the pond's BED, a quarter of a tile under a flat surface: seen by the view ray bent into the
    // water (so it slides under the surface as the eye moves), dimmed by the water it is seen through (red first,
    // Beer-Lambert), and the surface takes the reflection by Fresnel (little straight down, much at a low angle)
    // (later that day: "a tiny brown tint layer", and the bed further under: the water itself a little murky, a
    // brown that gathers with the water looked through, lit like the bed; the bed 0.4 of a tile down, was 0.25)
    static const float WATER_DEPTH = 0.5;
    static const float3 WATER_MURK = float3(0.34, 0.22, 0.10);
    float wsoft = 0;         // the bed's softness, texels
    float wmurk = 0;         // how much of the water's own brown lies over the bed here
    float2 wbed = 0;         // the bed's shift under the surface, texels
    float3 wdim = 0.6;       // what of the bed's colour comes through
    float wfr = 0;           // the surface's Fresnel reflectance
    if (water) {
        // water: dark and opaque (the original's texture, darkened), its surface waved by three moving sines (the
        // normal from their slopes), the lights' highlights tight on it, a reflection traced in the ray-traced builds
        relief = false;
        if (WATER_PLAIN) {
            float3 Vw = normalize(eye.xyz - i.wpos);
            float3 Rr = refract(-Vw, Ng, 0.75);
            // (the user: "the texture doesn't look like it is sitting below the surface"): a pond's bed slopes: at
            // the shore (the nearest tile that is not water, the grid's z) it meets the surface, and sinks to
            // WATER_DEPTH 0.7 of a tile out; and what lies deeper is seen softer (four taps, wider with the depth)
            float2 pw = i.wpos.xz;
            int2 c0w = int2(floor(pw));
            float sd = 1;
            [unroll] for (int sz = -1; sz <= 1; sz++)
                [unroll] for (int sx = -1; sx <= 1; sx++) {
                    int2 cw = c0w + int2(sx, sz);
                    if (tile(cw).z != 1) sd = min(sd, length(max(abs(pw - (float2(cw) + 0.5)) - 0.5, 0)));
                }
            float shore = smoothstep(0.0, 0.7, sd);
            float wd = WATER_DEPTH * (0.06 + 0.94 * shore);
            wdeep = wd;
            float down = max(-dot(Rr, Ng), 0.3), path = wd / down;
            float3 s = (Rr - Ng * dot(Rr, Ng)) * path;
            wbed = float2(dot(s, T) / max(dot(T, T), 1e-12), dot(s, B) / max(dot(B, B), 1e-12));
            wfr = 0.02 + 0.98 * pow(1 - saturate(dot(Ng, Vw)), 5);
            wdim = 0.9 * exp(-path * float3(1.2, 0.55, 0.38)) * (1 - wfr);
            wmurk = 1 - exp(-path * 0.32);
            wsoft = 0.12 + 0.3 * shore;
            if (i.rect.z > 0) c = bed_at(i.rect, uv + wbed, wsoft) * float4(i.col.rgb, 1);
            c.rgb *= wdim / 0.6;   // (the 0.6 below)
        }
        // (the user: "too glassy ... I'd rather it be incorporated into the water texture"): the normal is the
        // texture's own (its Laigter normal map, as the floors'), so the highlights and the reflection follow the
        // texture's ripples; only a gentle slow swell from the sines on top of it, and the highlight brighter on the
        // texture's light pixels (their crests), dimmer in its dark ones
        c.rgb *= 0.6;
        float2 w = i.wpos.xz;
        float dxw = 6.0 * cos(w.x * 6.0 + tsec * 1.3) + 4.5 * cos((w.x + w.y) * 9.0 + tsec * 2.1);
        float dzw = 5.0 * cos(w.y * 5.0 - tsec * 1.1) + 4.5 * cos((w.x + w.y) * 9.0 + tsec * 2.1);
        float3 Nw = Ng;
        if (!WATER_PLAIN && linfo.y > 0 && i.rect.z > 0) {
            // (2026-11-16: the original textures' normal maps are smooth at 4x now; the water keeps its ripples by
            // whole texels, as it had them: smooth, its sparkles and the bed seen through it smeared into blobs)
            if (texres.y <= 0) nm = natlas_at(i.rect, floor(uv) + 0.5);
            float3 tn = nm.xyz * 2 - 1;
            tn.xy *= max(linfo.y, 1.0) * 2.5;   /* its Laigter map is faint (half a floor's): made stronger */
            Nw = normalize(normalize(T) * tn.x - normalize(B) * tn.y + Ng * tn.z);
        }
        N = WATER_PLAIN ? Ng : normalize(Nw + float3(-dxw * 0.01, 0, -dzw * 0.01));
        shine = WATER_PLAIN ? 0 : 0.3 * saturate(0.3 + 1.4 * lum(c.rgb) / 0.35), shine_pow = 28;
        if (fxw.x > 0.5) {
            // the effects' water (fx_water): waves travelling across it (four, each its own way and length) and the
            // rings of the splashes (FX_SPLASH: up to 16, two seconds each) bend the surface; what lies under it (the
            // original's texture, its colour cycling kept as the base) is seen through it displaced by that bend
            // (refraction, with or without ray tracing); the caustics are the ray-traced builds' (below)
            float t = fxw.z;
            float2 g = 0;
            static const float4 WV[4] = {float4(0.8, 0.6, 7.0, 1.1), float4(-0.5, 0.86, 11.0, 1.7), float4(0.2, -0.98, 17.0, 2.3), float4(-0.95, -0.3, 23.0, 2.9)};
            [unroll] for (int k = 0; k < 4; k++) g += WV[k].xy * cos(dot(WV[k].xy, w) * WV[k].z + t * WV[k].w) * (0.9 / (k + 1));
            int nr = (int)fxw.y;
            float crest = 0;   // the rings' crests, lighter (the light caught on their slopes)
            for (int j = 0; j < nr && j < 16; j++) {
                float2 dv = w - ripple[j].xy;
                float d = length(dv) + 1e-4, R = 0.7 * ripple[j].z, x = (d - R) / 0.14;
                float env = exp(-ripple[j].z * 1.4) * exp(-x * x) * ripple[j].w;
                g += dv / d * env * cos((d - R) * 45.0) * 5.0;
                crest += env * max(sin((d - R) * 45.0), 0);
            }
            N = normalize(N + float3(-g.x, 0, -g.y) * 0.2);
            // the bed seen through the bent surface: the texture displaced (refraction), a little lighter where the
            // waves face the eye and on the rings' crests
            c = (i.rect.z > 0 ? bed_at(i.rect, uv + wbed + N.xz * (WATER_PLAIN ? 28.0 * wdeep : 14.0), wsoft) : float4(1, 1, 1, 1)) * float4(i.col.rgb, 1);
            c.rgb *= wdim * (1.0 + 0.35 * dot(g, normalize(eye.xz - w + 1e-4)) / 2.0) + 0.0;
            c.rgb *= 1.0 + 1.5 * saturate(crest);
            wring = saturate(crest * 2 + length(g) * 0.3);
            if (WATER_PLAIN) shine = 0.08 + 0.5 * wring; else shine *= 1.6;
        }
    }
    if (water && WATER_PLAIN) c.rgb = lerp(c.rgb, WATER_MURK * i.col.rgb * (1 - wfr), wmurk);
    if (water) texrelief = false;
    if (relief) {
        float3 tn = nm.xyz * 2 - 1;
        tn.xy *= linfo.y;
        N = normalize(normalize(T) * tn.x - normalize(B) * tn.y + Ng * tn.z);
    } else if (linfo.y > 0 && (card || wquad) && i.rect.z > 0) {
        // a picture (an object, a creature, a door): its own normal map, at the same place as its colours
        float2 nuv = uv;
        if (sprite.y > 0) {
            // its height map (the normal atlas's alpha, NAME_p.png): the normal looked up where the view ray meets the
            // relief below the card, (1 - height) x sprite.y texels deep (a small parallax offset; the colours stay)
            float3 Vs = normalize(eye.xyz - i.wpos);
            float3 Vt = float3(dot(Vs, normalize(T)), dot(Vs, normalize(B)), dot(Vs, Ng));
            float depth = 1 - snatlas_at(i.rect, uv).a;
            nuv = uv - Vt.xy / max(abs(Vt.z), 0.3) * depth * sprite.y;
        }
        float3 tn = snatlas_at(i.rect, nuv).xyz * 2 - 1;
        tn.xy *= rtlim.z > 0 ? rtlim.z : linfo.y;
        N = normalize(normalize(T) * tn.x - normalize(B) * tn.y + Ng * tn.z);
        relief = true; // its sheen, as the walls'
    }
    // the checks (render_set_debug 1): the shading normal as a colour, and how far it leans off the surface's
    if (linfo.w > 0.5 && linfo.w < 1.5) { o.color = float4(lin(N * 0.5 + 0.5) * size.z, 1); return o; }
    if (linfo.w > 1.5 && linfo.w < 2.5) { o.color = float4((water ? 1.0 : 0.0).xxx * size.z * 4, 1); return o; }   // the checks: the water's mask

    // the lights, each seen through a shadow ray (aimed at a random point of the light, so the edges of
    // shadows are soft once DLSS has gathered a few frames); on relief, a stone edge toward the light shades
    uint seed = pcg((uint)i.pos.x * 1973u + (uint)i.pos.y * 9277u + (uint)eye.w * 26699u);
    float3 acc = 0, sheen = 0;
    float3 V = normalize(eye.xyz - i.wpos);
    float3 pl = i.wpos + Ng * 0.01;
    [loop] for (uint m = 0; m <= 8; m++) {
        uint k = light_at(pl, m);
        if (k == NO_LIGHT) break;
        if (k == NO_LIGHT - 1) continue;
        float3 L;
        float d;
        float3 src;
        float3 e = light_term(k, i.wpos, Ng, N, L, d, seed, src);
        if (lum(e) < 1e-5) continue;
        float3 j = float3(rnd(seed), rnd(seed), rnd(seed)) * 2 - 1;
        float3 target = src + normalize(j + 1e-4) * look.x * (lights[k].info.x > 0.5 ? 0.3 : 1) * rnd(seed);   // a flame: its puff
        bool flood = k != 0 && lights[k].info.z > 0.5;   // a flooding light (the moonstone): no shadow, no relief shadow
        float3 vis = (k == 0 && rtlim.w < 0) || flood ? 1 : shadow(pl, target);   // full bright: no shadow from the torch
        // the texture's relief in the light's way (sprite.w: its depth for a 64 texel texture; a floor's 32 half that)
        if (sprite.w > 0 && texrelief && !(k == 0 && rtlim.w < 0) && !flood)
            vis *= relief_shadow(i.rect, uv, float3(dot(L, normalize(T)), dot(L, normalize(B)), dot(L, Ng)), sprite.w * i.rect.z / 64.0);
        acc += e * vis;
        // the sheen: a soft highlight (Blinn-Phong) of the light that reaches here, by the mapped normal; it moves
        // with every bevel and crack, where the diffuse light only dims them a little
        if (shine > 0 && (relief || water)) {
            float3 H = normalize(L + V);
            sheen += e / max(saturate(dot(N, L)), 0.2) * pow(saturate(dot(N, H)), shine_pow) * shine * vis;
        }
    }
    {   // the lava round the point (the grid's z: 2): its glow, a warm unshadowed light falling off with distance,
        // from the middle of each lava tile within two tiles
        int2 c0 = int2(floor(i.wpos.xz));
        float3 lg = 0;
        [loop] for (int dz = -2; dz <= 2; dz++)
            [loop] for (int dx = -2; dx <= 2; dx++) {
                int2 cc = c0 + int2(dx, dz);
                uint4 tt = tile(cc);
                if (tt.z != 2) continue;
                float3 lp = float3(cc.x + 0.5, floor_at(tt, float2(0.5, 0.5)) + 0.05, cc.y + 0.5);
                float3 d = lp - i.wpos;
                float dd = max(dot(d, d), 0.2);
                float3 Ld = d * rsqrt(dd);
                lg += float3(1.0, 0.42, 0.10) * saturate(dot(N, Ld) * 0.7 + 0.3) / dd * 0.24;   // (0.3 until 2026-11-15: the user, "less light, say 20%")
            }
        acc += lg;
    }
    float3 refl = 0;
#ifdef RT
    if (water) {   // the reflection: what the mirrored view ray sees, lit as it is, by the water's Fresnel
        // a rough surface: the mirrored ray scattered a little each frame (DLSS gathers it into a blur), and weak
        float3 Rd = normalize(reflect(-V, N) + (float3(rnd(seed), rnd(seed), rnd(seed)) * 2 - 1) * (WATER_PLAIN ? 0.07 : 0.18));
        if (Rd.y < 0.02) Rd = normalize(Rd - float3(0, Rd.y - 0.02, 0));
        float fr = WATER_PLAIN ? 0.02 + 0.98 * pow(1 - saturate(dot(N, V)), 5) : 0.04 + 0.96 * pow(1 - saturate(dot(N, V)), 5);
        float ht;
        refl = ray_light(i.wpos + Ng * 0.02, Rd, probes, look.y, false, seed, ht) * fr * (WATER_PLAIN ? 1.0 : 0.35);
    }
#endif
#if defined(RTHIGH)
    // one bounce ray, cosine-distributed about the normal: what it brings back is this point's bounced light
    // (the probes give the bounces after that); Ray Reconstruction takes the noise out
    {
        float u1 = rnd(seed), u2 = rnd(seed) * 6.2831853, r = sqrt(u1);
        float3 t1 = normalize(cross(N, abs(N.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0))), t2 = cross(N, t1);
        float3 dir = normalize(t1 * (r * cos(u2)) + t2 * (r * sin(u2)) + N * sqrt(saturate(1 - u1)));
        if (dot(dir, Ng) < 0.01) dir = normalize(dir - Ng * (2 * dot(dir, Ng) - 0.02));
        float ht;
        acc += ray_light(i.wpos + Ng * 0.02, dir, probes, look.y, true, seed, ht) * look.y * reach_window_seen(pl);
    }
    o.nrough = float4(N, 1);
#elif defined(RT)
    acc += bounce_at(probes, i.wpos + Ng * 0.05, N) * look.y * reach_window_seen(pl);
#endif
#ifdef RT
    // the effects' water: the caustics, the lights that reach it focused by its waves into bright threads on what lies
    // under (the ray-traced builds only: the light arriving is the traced light)
    if (water && fxw.x > 0.5) {
        float2 q = (i.wpos.xz + N.xz * wdeep) * 3.1 + float2(fxw.z * 0.35, fxw.z * 0.27);
        float k = abs(sin(q.x + sin(q.y * 1.3 + fxw.z * 0.8))) * abs(sin(q.y * 1.1 + sin(q.x * 0.9 - fxw.z * 0.6)));
        acc *= WATER_PLAIN ? lerp(1.0, 0.8 + 1.1 * pow(1 - k, 6), saturate(wdeep * 4)) : 0.75 + 1.1 * pow(1 - k, 6);   // (none at the shore's edge)
    }
#endif
    // (water had a faint light of its own here, 0.03, "never black": taken out 2026-11-13 at the user's word: it must
    // darken with distance like every other texture, not be seen far down a dark passage)
    // the glow of what glows: lava a little brighter where its relief faces up (the crust's tops cooler, the cracks
    // hotter would need the map's height; the facing is the normal map's), the glowing pictures by their own colour
    if (lava) emit *= 0.7 + 0.3 * saturate(N.y);
    // a picture's occlusion map (NAME_o.png): on all the light it takes, the lights' and the bounced (not its glow
    // nor its sheen); sprite.x its strength (0: none, the pictures as before)
    if (sprite.x > 0 && (card || wquad) && !wallpic && i.rect.z > 0) acc *= lerp(1.0, saoatlas_at(i.rect, uv), sprite.x);
    // a wall's or a floor's occlusion map (NNN_o.png: its joints, at the texture's own place in the pictures' AO atlas):
    // on all the light it takes, as the pictures'; sprite.z its strength
    if (sprite.z > 0 && texrelief) acc *= lerp(1.0, texao_at(i.rect, uv), sprite.z);
    o.color = float4((lin(c.rgb) * acc + sheen + refl + emit) * size.z, 1);
    return o;
}
