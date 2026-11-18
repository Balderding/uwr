// The fog and smoke (the user, 2026-11-17: "physical simulation of smoke and water haze fog ... displaced by you when
// you move through it"; later that day: "as real as possible", after the papers): a box of air cells round the eye
// (dims cells, each 1 / org.w of a tile; a tile 2.4 m), stepped each frame:
//   cs_curl     the air's curl (A -> WV), for the vorticity confinement below
//   cs_advect   the air carries itself, its warmth, the fog and the smoke along: the plain semi-Lagrangian step of
//               Stam's Stable Fluids (A -> B)
//   cs_mac      the MacCormack correction of Selle, Fedkiw, Kim, Liu and Rossignac 2008 (what the step lost, found by
//               carrying B back again, half given back, kept within what was there: sharp wisps instead of a blur);
//               then what acts on the air: buoyancy from its temperature (g dT / T0, Boussinesq), the vorticity
//               confinement of Fedkiw, Stam and Jensen 2001 (the small curls the grid loses, put back), the water's
//               chill and haze, the lava's heat and steam, the fires, the bodies, the fog's droplets settling and
//               drying off in warm air, the heat lost to the air and the rock, the draught (-> C)
//   cs_div      how much air each cell gains or loses (C -> the pressure's y)
//   cs_jac_ab / cs_jac_ba   the pressure that undoes it, a pass at a time (walls and floors closed, the box's sides open)
//   cs_project  the air made to flow round things instead of into them (C, pressure -> A)
//   cs_light    each cell's light: every light that reaches it, through a shadow ray (the walls', the pictures') and
//               through the fog and smoke in between (the light scattered more than once let further in, after
//               Wrenninge's octaves), the room's bounced light (the probes); kept over frames; what the cell sends on
//   vs / ps     the view stepped through the box to the scene's depth, drawn over the scene's colour; fog's droplets
//               scatter as Mie's (the HG + Draine blend of Jendersie and d'Eon 2023, by the droplets' size), smoke as
//               Henyey-Greenstein's
// The cells are kept at their place in the world modulo the box's size (a cell never moves in memory as the box
// follows the eye; a cell that has just come into the box starts empty).
// Set 0: the scene's own (common.hlsli: the constants, the grid, the lights, the top level). Set 1: these.
#include "common.hlsli"

#ifdef RT
RaytracingAccelerationStructure scene_as : register(t3);
StructuredBuffer<float4> probes : register(t4);
struct SVtx { float3 pos; float3 prev; float2 uv; float4 rect; float3 nrm; float4 col; };
StructuredBuffer<SVtx> sverts : register(t10);
#endif

[[vk::binding(0, 1)]] cbuffer VC : register(b0, space1) {
    float4 vorg;     // the box's first cell in the world (cells), w cells to a tile
    float4 vdims;    // cells along x, y, z; w a cell's size (tiles)
    float4 vprev;    // the box's first cell the frame before; w 1: there was one
    float4 vtm;      // x the step (s), y the time (s), z the frame, w the light's share (a cell every w-th frame)
    float4 vcam;     // the eye; w 1 / the projection's x scale
    float4 vcamr;    // the camera's right; w 1 / the projection's y scale
    float4 vcamu;    // its up; w the exposure
    float4 vcamf;    // its forward; w the view's steps
    float4 vmisc;    // x the emitters, y, z the projection's centre (x, y), w spare
    float4 air;      // x drag (1/s), y the vorticity confinement, z, w the draught (its wind, tiles/s along x, z)
    float4 fogA;     // x the water's chill (K under the room's air), y drying off (1/s a K of warmth), z spread, w decay
    float4 fogB;     // the layer's height, given off, water on, lava on
    float4 smkA;     // x its own weight, y the heat's lift (x the real), z spread, w decay
    float4 smkB;     // x cooling (1/s), y 1: a fire puffs at its flame's own rate, z cold air's warming (1/s), w the billows spare the thick middle (0..1)
    float4 fogL;     // colour (linear), brightness
    float4 fogM;     // what it blocks a tile (0: off), its billows (0..1), softness, the puffs of what gives off (0..1)
    float4 smkL;
    float4 smkM;     // what it blocks a tile, its glow toward the light (Henyey-Greenstein's g), softness, its billows (0..1)
    float4 lightp;   // x shadows, y rays a light, z the placed lights' strength, w self shadow
    float4 lightq;   // x the player's light's strength, y placed lights on, z kept of the old light, w many scatterings
    float4 fogC;     // x the droplets' settling (tiles/s), y the lava's air (K over the room's), z the water's haze in wisps (0..1), w the billows drawn out along the air's way (0..1)
    float4 fogP;     // the droplets' phase function (Jendersie and d'Eon): g of HG, g of Draine, Draine's alpha, Draine's weight
    float4 vextra;   // x 1: the MacCormack correction, y the room's bounced light in it, z the room's air (K), w the billows' size (tiles)
    float4 vmore;    // x the billows by the air's shear (0..1), y fog dries from its edges (0..1), z moving air raises more mist (0..1), w 1: the puffs' heat in beats
    float4 vshape;   // the fine shapes: x fog in veils (0..1), y smoke in round puffs (0..1)
    float4 vscr;     // the scene's picture (pixels: x, y) and the half-size one's (z, w)
};
struct Emit { float4 pr; float4 vh; float4 q; float4 k; };
[[vk::binding(1, 1)]] StructuredBuffer<Emit> emits : register(t0, space1);
[[vk::binding(2, 1)]] SamplerState vsamp : register(s0, space1);              // linear; x, z repeat, y clamped
[[vk::binding(3, 1)]] Texture3D<float4> UAs : register(t1, space1);           // the air: velocity (tiles/s), w its warmth (K over the room's)
[[vk::binding(4, 1)]] Texture3D<float4> DAs : register(t2, space1);           // x fog, y smoke, z the air's curl there (1/s)
[[vk::binding(5, 1)]] Texture3D<float4> UBs : register(t3, space1);
[[vk::binding(6, 1)]] Texture3D<float4> DBs : register(t4, space1);
[[vk::binding(7, 1)]] Texture3D<float4> Rs : register(t5, space1);
[[vk::binding(8, 1)]] Texture3D<float4> Gs : register(t6, space1);
[[vk::binding(9, 1)]] Texture2D<float> zbuf : register(t7, space1);           // the scene's depth
[[vk::binding(10, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> UA : register(u0, space1);
[[vk::binding(11, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> DA : register(u1, space1);
[[vk::binding(12, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> UB : register(u2, space1);
[[vk::binding(13, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> DB : register(u3, space1);
[[vk::binding(14, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> UC : register(u4, space1);
[[vk::binding(15, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> DC : register(u5, space1);
[[vk::binding(16, 1)]] [[vk::image_format("rgba32f")]] RWTexture3D<float4> PA : register(u6, space1);   // x pressure, y what the cell gains
[[vk::binding(17, 1)]] [[vk::image_format("rgba32f")]] RWTexture3D<float4> PB : register(u7, space1);
[[vk::binding(18, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> LA : register(u8, space1);  // the cell's light, a 1: there is one
[[vk::binding(19, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> RW : register(u9, space1);  // what it sends the eye a tile, a what it blocks
[[vk::binding(20, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> GW : register(u10, space1); // the way to its light x how much comes that way, w the fog's share
[[vk::binding(21, 1)]] [[vk::image_format("rgba16f")]] RWTexture3D<float4> WV : register(u11, space1); // the air's curl (1/s), w its size
[[vk::binding(22, 1)]] [[vk::image_format("rgba16f")]] RWTexture2D<float4> LOWW : register(u12, space1); // the picture at half size (cs_march)
[[vk::binding(23, 1)]] Texture2D<float4> LOWs : register(t8, space1);

static const float VCEIL = 4.0;
static const float TILE_M = 2.4;                 // a tile in metres (the game's own, as the acoustics have it)
static const float GRAV = 9.81 / TILE_M;         // tiles / s2

// ---- the cells ----
int3 dimsi() { return int3(vdims.xyz); }
int3 wrap(int3 wc) {
    int3 d = dimsi();
    return int3(((wc.x % d.x) + d.x) % d.x, clamp(wc.y, 0, d.y - 1), ((wc.z % d.z) + d.z) % d.z);
}
bool in_box(int3 wc, float4 o) { int3 l = wc - int3(o.xyz); return all(l >= 0) && all(l < dimsi()); }
bool was_there(int3 wc) { return vprev.w > 0.5 && in_box(wc, vprev); }
float3 cell_pos(int3 wc) { return (float3(wc) + 0.5) * vdims.w; }
bool solid(int3 wc) { return blocked(cell_pos(wc)); }
// a place's coordinates in the textures (they repeat across x and z: the cells' places modulo the box)
float3 tex_at(float3 p) { return p * vorg.w / vdims.xyz; }
// is a place well inside the box the fields were made for (o: its first cell), margin cells from its sides
bool inside(float3 p, float4 o, float margin) {
    float3 l = p * vorg.w - o.xyz;
    return all(l.xz >= margin) && all(l.xz <= vdims.xz - margin) && l.y >= 0 && l.y <= vdims.y;
}
uint cell_of(uint id, out int3 wc) {
    int3 d = dimsi();
    int3 l = int3(id % d.x, (id / d.x) % d.y, id / (d.x * d.y));
    wc = int3(vorg.xyz) + l;
    return id < (uint)(d.x * d.y * d.z) ? 1 : 0;
}
static const int3 NB[6] = {int3(1, 0, 0), int3(-1, 0, 0), int3(0, 1, 0), int3(0, -1, 0), int3(0, 0, 1), int3(0, 0, -1)};

float hash3(float3 p) {
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.x + p.y) * p.z);
}
// a slow uneven field, 0..1 (the water not giving off the same over every square)
float lumps(float3 p) {
    float3 i = floor(p), f = frac(p);
    f = f * f * (3 - 2 * f);
    float a = lerp(lerp(hash3(i), hash3(i + float3(1, 0, 0)), f.x), lerp(hash3(i + float3(0, 1, 0)), hash3(i + float3(1, 1, 0)), f.x), f.y);
    float b = lerp(lerp(hash3(i + float3(0, 0, 1)), hash3(i + float3(1, 0, 1)), f.x), lerp(hash3(i + float3(0, 1, 1)), hash3(i + float3(1, 1, 1)), f.x), f.y);
    return lerp(a, b, f.z);
}

// ---- the step ----
// the air's curl where the step starts (central differences; rock and what is outside the box: the cell's own)
float3 vel_a(int3 n, float3 own) { return was_there(n) && !solid(n) ? UA[wrap(n)].xyz : own; }
[numthreads(64, 1, 1)]
void cs_curl(uint id : SV_DispatchThreadID) {
    int3 wc;
    if (!cell_of(id, wc)) return;
    int3 ic = wrap(wc);
    if (!was_there(wc) || solid(wc)) { WV[ic] = 0; return; }
    float3 c = UA[ic].xyz;
    float3 dx = vel_a(wc + NB[0], c) - vel_a(wc + NB[1], c), dy = vel_a(wc + NB[2], c) - vel_a(wc + NB[3], c), dz = vel_a(wc + NB[4], c) - vel_a(wc + NB[5], c);
    float3 w = float3(dy.z - dz.y, dz.x - dx.z, dx.y - dy.x) * 0.5 / vdims.w;
    WV[ic] = float4(w, length(w));
}

// the plain step: what was a step upstream is here now
[numthreads(64, 1, 1)]
void cs_advect(uint id : SV_DispatchThreadID) {
    int3 wc;
    if (!cell_of(id, wc)) return;
    int3 ic = wrap(wc);
    if (solid(wc)) { UB[ic] = 0, DB[ic] = 0; return; }
    float3 p = cell_pos(wc);
    float4 u0 = was_there(wc) ? UA[ic] : 0;
    float3 pb = p - u0.xyz * vtm.x;
    float4 U = 0, D = 0;
    if (vprev.w > 0.5 && inside(pb, vprev, 1.49)) {
        float3 t = tex_at(pb);
        U = UAs.SampleLevel(vsamp, t, 0), D = DAs.SampleLevel(vsamp, t, 0);
    }
    UB[ic] = U, DB[ic] = D;
}

float curl_at(int3 n) { return in_box(n, vorg) && n.y >= 0 && n.y < (int)vdims.y ? WV[wrap(n)].w : 0; }
[numthreads(64, 1, 1)]
void cs_mac(uint id : SV_DispatchThreadID) {
    int3 wc;
    if (!cell_of(id, wc)) return;
    int3 ic = wrap(wc);
    if (solid(wc)) { UC[ic] = 0, DC[ic] = 0; return; }
    float dt = vtm.x, h = vdims.w;
    float3 p = cell_pos(wc);
    bool there = was_there(wc);
    float4 U = UB[ic], D = DB[ic];
    float4 u0 = there ? UA[ic] : 0, d0 = there ? DA[ic] : 0;
    // MacCormack: the step taken back again shows what it lost; half of that given back, and the answer kept within
    // what the eight cells it came from held (no new peaks: stable whatever the step)
    float3 pb = p - u0.xyz * dt, pf = p + u0.xyz * dt;
    if (vextra.x > 0.5 && there && inside(pb, vprev, 2.49) && inside(pf, vorg, 2.49)) {
        float3 t = tex_at(pf);
        float4 Ur = UBs.SampleLevel(vsamp, t, 0), Dr = DBs.SampleLevel(vsamp, t, 0);
        float4 Un = U + 0.5 * (u0 - Ur), Dn = D + 0.5 * (d0 - Dr);
        int3 b0 = int3(floor(pb * vorg.w - 0.5));
        float4 ulo = 1e9, uhi = -1e9, dlo = 1e9, dhi = -1e9;
        [unroll] for (int k = 0; k < 8; k++) {
            int3 n = wrap(b0 + int3(k & 1, (k >> 1) & 1, k >> 2));
            float4 a = UA[n], b = DA[n];
            ulo = min(ulo, a), uhi = max(uhi, a), dlo = min(dlo, b), dhi = max(dhi, b);
        }
        U = clamp(Un, ulo, uhi), D = clamp(Dn, dlo, dhi);
    }
    D = max(D, 0);
    // it seeps into the clear air beside it (the six neighbours; rock gives and takes nothing), and the rock beside it
    // takes the air's warmth
    int rock = 0;
    if (there) {
        float4 c0 = max(d0, 0), sum = 0;
        [unroll] for (int k = 0; k < 6; k++) {
            int3 n = wc + NB[k];
            bool sn = solid(n);
            rock += sn ? 1 : 0;
            sum += was_there(n) && !sn ? max(DA[wrap(n)], 0) : c0;
        }
        float2 kd = min(float2(fogA.z, smkA.z) * dt / (h * h), 0.15);
        D.xy += kd * (sum.xy - 6 * c0.xy);
        // the fog's droplets settle: what the cell above holds comes down at their speed (Stokes: about a centimetre a
        // second for 20 micrometre droplets)
        int3 up = wc + NB[2];
        float above = was_there(up) && !solid(up) ? max(DA[wrap(up)].x, 0) : 0;
        D.x += min(fogC.x * dt / h, 0.5) * (above - c0.x);
    }
    float3 v = U.xyz;
    // (cold air by its own dial: it lies as a heavy sheet for a good while, and runs out across the floor)
    float T = U.w * exp(-(U.w < 0 ? smkB.z : smkB.x) * (1 + 0.6 * rock) * dt);
    // the small curls the grid loses, put back (vorticity confinement: a push round each curl, toward where it is
    // strongest)
    if (air.y > 0 && there) {
        float4 w = WV[ic];
        float3 eta = float3(curl_at(wc + NB[0]) - curl_at(wc + NB[1]), curl_at(wc + NB[2]) - curl_at(wc + NB[3]),
                            curl_at(wc + NB[4]) - curl_at(wc + NB[5]));
        float el = length(eta);
        if (el > 1e-5) v += air.y * h * cross(eta / el, w.xyz) * dt;
    }
    // the water chills the air just over it and gives off its haze there (steam fog: the cold damp air then flows out
    // across the floor by its own weight); the lava heats it and gives off steam
    int2 tc = int2(floor(p.xz));
    uint4 tt = tile(tc);
    if (tt.z == 1 || tt.z == 2) {
        float up = p.y - floor_at(tt, p.xz - tc);
        bool lava = tt.z == 2;
        float layer = lava ? fogB.x * 0.6 : fogB.x;
        if (up >= 0 && up < layer) {
            float k = 1 - up / layer;
            // (a cell that has just come into the box starts as the water has it by now, not empty: no fog growing
            // in ahead of a walking player)
            if (lava) T = there ? lerp(T, max(T, fogC.y * k), saturate(dt * 2)) : fogC.y * k;
            else T = there ? lerp(T, min(T, -fogA.x * k), saturate(dt * 0.8)) : -fogA.x * k;
            if (fogB.y > 0 && (lava ? fogB.w : fogB.z) > 0.5) {
                float want = fogB.y * k * (0.2 + 0.8 * lumps(float3(p.xz * 0.9, vtm.y * 0.05)) + 0.6 * lumps(float3(p.x * 2.7, p.y * 4.0 + vtm.y * 0.11, p.z * 2.7))) * (lava ? 0.7 : 1.0);
                // (in wisps: shifting spots that give off most of it and send it up a little, steam fog's streamers;
                // the same amount on the whole)
                float spot = smoothstep(0.5, 0.78, lumps(float3(p.x * 3.1 + vtm.y * 0.07, vtm.y * 0.22, p.z * 3.1 - vtm.y * 0.05)));
                want *= lerp(1, 0.35 + 2.6 * spot, saturate(fogC.z));
                if (!lava) v.y += saturate(fogC.z) * spot * k * 0.15 * dt;
                want *= 1 + 1.5 * saturate(vmore.z) * saturate(length(v.xz) / 0.4);   // (air moving over it takes more up)
                if (!there) D.x = want;
                else if (D.x < want) D.x += (want - D.x) * saturate(dt * (lava ? 1.5 : 0.6));
            }
        }
    }
    // what gives off, heats and pushes: the fires, the blasts, the bodies
    int ne = (int)vmisc.x;
    [loop] for (int e = 0; e < ne; e++) {
        Emit m = emits[e];
        float3 a = m.pr.xyz, q = p;
        q.y = clamp(q.y, a.y, a.y + m.vh.w);              // (a body: the nearest point of its height)
        float3 dv = p - float3(a.x, q.y, a.z);
        float d = length(dv), r = max(m.pr.w, 1.2 * h);   // (never smaller than a cell: a torch fell between them)
        if (d >= r * 2) continue;
        float w = saturate(1 - d / r), w2 = saturate(1 - d / (r * 2));
        // (given off in puffs: an uneven field that climbs past the source and changes, the same amount on the whole)
        float pt = lumps(float3(vtm.y * 1.7 + e * 5.0, e * 3.0, 0.5));
        if (smkB.y > 0.5) {
            // (a flame sheds a puff about 1.5 / sqrt(its width in metres) times a second: a torch three, a campfire
            // two; each fire in its own step, a little uneven)
            float eh = hash3(floor(a * 8.0) + 0.5);
            float ph = vtm.y * 1.5 / sqrt(max(m.pr.w * TILE_M, 0.05)) + eh * 7.0 + 0.5 * lumps(float3(vtm.y * 0.5, eh * 31.0, 0.5));
            float s1 = 0.5 + 0.5 * sin(6.2832 * ph);
            pt = s1 * s1 * 1.33;
        }
        float pf = lumps(float3(p.x * 2.6 / max(r, 0.2), p.y * 2.6 / max(r, 0.2) - vtm.y * 2.3, p.z * 2.6 / max(r, 0.2)) + e * 17.0) * pt;
        float puff = lerp(1, saturate(pf * 4.0 - 0.15) * 2.2, saturate(fogM.w));
        D.x += m.q.x * w * puff * dt, D.y += m.q.y * w * puff * dt;
        // (its heat: the air in it held at that warmth, K over the room's: a plume that climbs, not a jet)
        float heat = m.q.z * (vmore.w > 0.5 ? lerp(1, 0.55 + 0.9 * pt, saturate(fogM.w)) : 1);   // (each puff a buoyant ring the flame sheds)
        T = heat < 0 ? lerp(T, min(T, heat), saturate(w * dt * 10)) : lerp(T, max(T, heat), saturate(w * dt * 10));   // (below 0: a cold source)
        v += dv / max(d, 1e-3) * m.q.w * w2 * saturate(dt * 8);          // shoved aside
        if (m.k.x > 0) v = lerp(v, m.vh.xyz, saturate(m.k.x * w2 * dt * 12));   // carried along
    }
    // buoyancy: warm air is lighter than the room's by dT / T0 of its weight (Boussinesq), cold air heavier; the
    // smoke's own weight if it is given one; the draught; the air coming to rest; the thinning out; fog dries off in
    // warm air
    v.y += (GRAV * T / max(vextra.z, 200.0) * smkA.y + smkA.x * D.y) * dt;
    if (dot(air.zw, air.zw) > 1e-6) v.xz += (air.zw - v.xz) * saturate(dt * 0.4);   // the draught: a wind the air is brought to
    v *= exp(-air.x * dt);
    // (thin fog, mixed with the drier air beside it, goes sooner; the thick middle lasts)
    float edry = lerp(1, 2.2 - 1.8 * saturate(D.x / 0.6), saturate(vmore.y));
    D.x *= exp(-(fogA.w * edry + fogA.y * max(T, 0)) * dt), D.y *= exp(-smkA.w * dt);
    float sp = length(v), top = 1.8 * h / max(dt, 1e-4);   // (never more than a couple of cells a step)
    if (sp > top) v *= top / sp;
    UC[ic] = float4(v, clamp(T, -30, 900));
    DC[ic] = float4(clamp(D.xy, 0, 6), there ? WV[ic].w : 0, 0);   // (z: the curl's size, for the picture's billows)
}

// a neighbour's velocity as the flow sees it: rock sends it back (nothing through a wall), the box's side lets it out
float3 vel_nb(int3 n, float3 own, int axis) {
    if (solid(n)) { float3 r = own; r[axis] = -r[axis]; return r; }
    if (!in_box(n, vorg)) return own;
    return UC[wrap(n)].xyz;
}
[numthreads(64, 1, 1)]
void cs_div(uint id : SV_DispatchThreadID) {
    int3 wc;
    if (!cell_of(id, wc)) return;
    int3 ic = wrap(wc);
    if (solid(wc)) { PA[ic] = 0; return; }
    float3 v = UC[ic].xyz;
    float d = vel_nb(wc + int3(1, 0, 0), v, 0).x - vel_nb(wc - int3(1, 0, 0), v, 0).x
            + vel_nb(wc + int3(0, 1, 0), v, 1).y - vel_nb(wc - int3(0, 1, 0), v, 1).y
            + vel_nb(wc + int3(0, 0, 1), v, 2).z - vel_nb(wc - int3(0, 0, 1), v, 2).z;
    // (the pressure of the step before kept where the cell was there: the passes start near the answer)
    PA[ic] = float4(was_there(wc) ? PA[ic].x : 0, d * 0.5 * vdims.w, 0, 0);
}
float pr_a(int3 n, float own) { return solid(n) ? own : !in_box(n, vorg) ? 0 : PA[wrap(n)].x; }
float pr_b(int3 n, float own) { return solid(n) ? own : !in_box(n, vorg) ? 0 : PB[wrap(n)].x; }
[numthreads(64, 1, 1)]
void cs_jac_ab(uint id : SV_DispatchThreadID) {
    int3 wc;
    if (!cell_of(id, wc)) return;
    int3 ic = wrap(wc);
    if (solid(wc)) { PB[ic] = 0; return; }
    float2 c = PA[ic].xy;
    float s = pr_a(wc + int3(1, 0, 0), c.x) + pr_a(wc - int3(1, 0, 0), c.x) + pr_a(wc + int3(0, 1, 0), c.x) + pr_a(wc - int3(0, 1, 0), c.x)
            + pr_a(wc + int3(0, 0, 1), c.x) + pr_a(wc - int3(0, 0, 1), c.x);
    PB[ic] = float4((s - c.y) / 6, c.y, 0, 0);
}
[numthreads(64, 1, 1)]
void cs_jac_ba(uint id : SV_DispatchThreadID) {
    int3 wc;
    if (!cell_of(id, wc)) return;
    int3 ic = wrap(wc);
    if (solid(wc)) { PA[ic] = 0; return; }
    float2 c = PB[ic].xy;
    float s = pr_b(wc + int3(1, 0, 0), c.x) + pr_b(wc - int3(1, 0, 0), c.x) + pr_b(wc + int3(0, 1, 0), c.x) + pr_b(wc - int3(0, 1, 0), c.x)
            + pr_b(wc + int3(0, 0, 1), c.x) + pr_b(wc - int3(0, 0, 1), c.x);
    PA[ic] = float4((s - c.y) / 6, c.y, 0, 0);
}
[numthreads(64, 1, 1)]
void cs_project(uint id : SV_DispatchThreadID) {
    int3 wc;
    if (!cell_of(id, wc)) return;
    int3 ic = wrap(wc);
    if (solid(wc)) { UA[ic] = 0, DA[ic] = 0; return; }
    float4 u = UC[ic];
    float pc = PA[ic].x;
    float3 g = float3(pr_a(wc + int3(1, 0, 0), pc) - pr_a(wc - int3(1, 0, 0), pc), pr_a(wc + int3(0, 1, 0), pc) - pr_a(wc - int3(0, 1, 0), pc),
                      pr_a(wc + int3(0, 0, 1), pc) - pr_a(wc - int3(0, 0, 1), pc)) * 0.5 / vdims.w;
    float3 v = u.xyz - g;
    // nothing into the rock beside it
    if (v.x > 0 && solid(wc + int3(1, 0, 0))) v.x = 0;
    if (v.x < 0 && solid(wc - int3(1, 0, 0))) v.x = 0;
    if (v.y > 0 && solid(wc + int3(0, 1, 0))) v.y = 0;
    if (v.y < 0 && solid(wc - int3(0, 1, 0))) v.y = 0;
    if (v.z > 0 && solid(wc + int3(0, 0, 1))) v.z = 0;
    if (v.z < 0 && solid(wc - int3(0, 0, 1))) v.z = 0;
    UA[ic] = float4(v, u.w);
    DA[ic] = DC[ic];
}

// ---- the light ----
// what a cell blocks a tile, and how much of it sends light on (each kind by its own dials; thin wisps hidden by its
// softness)
void matter(float2 d, out float ext, out float3 sca, out float glow) {
    float f = d.x * smoothstep(0, max(fogM.z * 0.5, 1e-4), d.x), s = d.y * smoothstep(0, max(smkM.z * 0.5, 1e-4), d.y);
    float ef = f * fogM.x, es = s * smkM.x;
    ext = ef + es;
    // (against a wall: the scene's light is what a white wall sends back, 1 / pi of what falls on it; fog and smoke
    // send 1 / 4 pi of it each way: a quarter of the wall's)
    sca = 0.25 * (ef * fogL.rgb * fogL.w + es * smkL.rgb * smkL.w);
    glow = ef / max(ext, 1e-5);   // the fog's share of it (the picture: the droplets' phase function for that share)
}
// the light that gets from a to b past the walls and the pictures (scene.hlsl's shadow, the glass left clear)
float seen(float3 a, float3 b) {
    float3 d = b - a;
    float len = length(d);
    if (len < 0.1) return 1;
#ifdef RT
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> q;
    RayDesc r;
    r.Origin = a, r.Direction = d / len, r.TMin = 0.0, r.TMax = len - 0.06;
    q.TraceRayInline(scene_as, RAY_FLAG_NONE, 0xFF, r);
    while (q.Proceed()) {
        if (q.CandidateType() != CANDIDATE_NON_OPAQUE_TRIANGLE) continue;
        uint p = q.CandidatePrimitiveIndex();
        SVtx v0 = sverts[p * 3], v1 = sverts[p * 3 + 1], v2 = sverts[p * 3 + 2];
        float k = v0.col.a;
        if (k > 3.25 && k < 4.5) continue;                     // glass, a flame, a spark: no shadow
        float2 bc = q.CandidateTriangleBarycentrics();
        float2 uv = v0.uv * (1 - bc.x - bc.y) + v1.uv * bc.x + v2.uv * bc.y;
        float4 c = v0.rect.z > 0 ? atlas_at(v0.rect, uv) : float4(1, 1, 1, 1);
        if (c.a < 0.5) continue;
        q.CommitNonOpaqueTriangleHit();
    }
    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0 : 1;
#else
    return visible_grid(a, b);
#endif
}
// through the fog and smoke between a cell and its light (six steps, three tiles at most)
float through(float3 a, float3 b) {
    float3 d = b - a;
    float len = min(length(d), 3.0);
    d = normalize(d) * len / 6;
    float tau = 0;
    [unroll] for (int k = 1; k <= 6; k++) {
        float3 p = a + d * (k - 0.5);
        if (!inside(p, vorg, 0.5)) break;
        float ext, glow;
        float3 sca;
        matter(max(DAs.SampleLevel(vsamp, tex_at(p), 0).xy, 0), ext, sca, glow);
        tau += ext;
    }
    tau *= len / 6 * lightp.w;
    // the light scattered on from cell to cell gets further in than the straight beam (Wrenninge's octaves: each a
    // beam of half the strength that sees half the fog); lightq.w: how much of that there is
    float one = exp(-tau), many = (one + 0.5 * exp(-0.5 * tau) + 0.25 * exp(-0.25 * tau)) / 1.75;
    return lerp(one, many, saturate(lightq.w));
}
[numthreads(64, 1, 1)]
void cs_light(uint id : SV_DispatchThreadID) {
    int3 wc;
    if (!cell_of(id, wc)) return;
    int3 ic = wrap(wc);
    float2 dn = max(DA[ic].xy, 0);
    float ext, glow;
    float3 sca;
    matter(dn, ext, sca, glow);
    bool fresh = !was_there(wc);
    if (ext < 1e-4 || solid(wc)) {   // clear air and rock send nothing; the light it had is kept for when the fog comes back
        RW[ic] = 0, GW[ic] = 0;
        if (fresh) LA[ic] = 0;
        return;
    }
    float4 old = fresh ? 0 : LA[ic];
    int share = max((int)vtm.w, 1);
    uint hsh = pcg((uint)wc.x * 73856093u ^ (uint)wc.y * 19349663u ^ (uint)wc.z * 83492791u);
    float4 Lc = old;
    float3 way = GW[ic].xyz;
    if (old.a < 0.5 || ((hsh + (uint)vtm.z) % (uint)share) == 0) {
        uint seed = pcg(hsh + (uint)vtm.z * 26699u);
        float3 p = cell_pos(wc), sum = 0, dsum = 0;
        float lsum = 0;
        int rays = clamp((int)lightp.y, 1, 4);
        [loop] for (uint m = 0; m <= 8; m++) {
            uint k = light_at(p, m);
            if (k == NO_LIGHT) break;
            if (k == NO_LIGHT - 1) continue;
            float gain = k == 0 ? lightq.x : lightp.z;
            if (gain <= 0) continue;
            float3 toL = normalize(lights[k].pr.xyz - p + 1e-4);
            float3 e = 0;
            float3 L = toL;
            [loop] for (int r = 0; r < rays; r++) {
                float3 Lr, src;
                float d;
                float3 er = light_term(k, p, toL, toL, Lr, d, seed, src);
                if (lum(er) < 1e-5) continue;
                bool flood = k != 0 && lights[k].info.z > 0.5;
                float vis = 1;
                if (lightp.x > 0.5 && !flood && !(k == 0 && rtlim.w < 0)) {
                    float3 j = float3(rnd(seed), rnd(seed), rnd(seed)) * 2 - 1;
                    vis = seen(p, src + normalize(j + 1e-4) * look.x * rnd(seed));
                }
                if (vis > 0 && lightp.w > 0 && !flood) vis *= through(p, src);
                e += er * vis, L = Lr;
            }
            e *= gain / rays;
            sum += e;
            float w = lum(e);
            dsum += L * w, lsum += w;
        }
#ifdef RT
        // the room's bounced light (the probes: what arrives from all six sides, evenly)
        if (vextra.y > 0) {
            static const float3 DG = float3(0.57735, 0.57735, 0.57735);
            sum += 0.5 * (bounce_at(probes, p, DG) + bounce_at(probes, p, -DG)) * look.y * vextra.y;
        }
#endif
        float keep = old.a > 0.5 ? saturate(lightq.z) : 0;
        Lc = float4(lerp(sum, old.rgb, keep), 1);
        float3 nw = lsum > 1e-6 ? dsum / lsum : 0;
        way = old.a > 0.5 ? lerp(nw, way, keep) : nw;
        LA[ic] = Lc;
    }
    // (an even share of the light every way: 1 / 4 pi of what is scattered; kept as the light's own number, so that fog
    // as bright as the wall behind it reads as the wall's light)
    RW[ic] = float4(sca * Lc.rgb, ext);
    GW[ic] = float4(way, glow);
}

// ---- the picture ----
// phase functions over an even share (x 4 pi): Henyey-Greenstein's, and Draine's (alpha 0: HG; alpha 1: Cornette and
// Shanks's), which with HG fits the droplets of fog (Jendersie and d'Eon 2023)
float hg(float c, float g) { return (1 - g * g) / pow(max(1 + g * g - 2 * g * c, 1e-4), 1.5); }
float draine(float c, float g, float a) { return hg(c, g) * (1 + a * c * c) / (1 + a * (1 + 2 * g * g) / 3); }
struct VVO { float4 pos : SV_Position; float2 ndc : TEXCOORD0; };
VVO vs(uint id : SV_VertexID) {
    VVO o;
    float2 t = float2((id << 1) & 2, id & 2);
    o.ndc = t * 2 - 1;
    o.pos = float4(o.ndc, 0, 1);
    return o;
}
// the place a depth stands for, along the view (render_vk_cpu.h view_proj's)
float view_z(float depth) {
    float A = FAR_Z / (FAR_Z - NEAR_Z), B = NEAR_Z * FAR_Z / (FAR_Z - NEAR_Z);
    return depth >= 1 ? FAR_Z : B / max(A - depth, 1e-6);
}
// the nearest of the four scene pixels a half-size one stands for (its depth as stored)
float block_depth(int2 q) {
    int2 hi = int2(vscr.xy) - 1, p0 = min(q * 2, hi), p1 = min(q * 2 + 1, hi);
    return min(min(zbuf.Load(int3(p0, 0)), zbuf.Load(int3(p1.x, p0.y, 0))), min(zbuf.Load(int3(p0.x, p1.y, 0)), zbuf.Load(int3(p1, 0))));
}
// the fog and smoke along one line of sight: ndc its place on the screen, depth the scene's there
float4 march(float2 ndc, float depth, uint seed) {
    float zv = view_z(depth);
    float3 dir = vcamf.xyz + vcamr.xyz * ((ndc.x - vmisc.y) * vcam.w) + vcamu.xyz * ((ndc.y - vmisc.z) * vcamr.w);
    float dl = length(dir);
    float3 rd = dir / dl;
    float tmax = zv * dl;
    // the stretch of the view inside the box (its sides a tile and a half in: the fog thins out toward them)
    float3 lo = vorg.xyz * vdims.w, hi = (vorg.xyz + vdims.xyz) * vdims.w;
    float3 inv = 1 / float3(abs(rd.x) > 1e-6 ? rd.x : 1e-6, abs(rd.y) > 1e-6 ? rd.y : 1e-6, abs(rd.z) > 1e-6 ? rd.z : 1e-6);
    float3 t0 = (lo - vcam.xyz) * inv, t1 = (hi - vcam.xyz) * inv;
    float3 tn = min(t0, t1), tf = max(t0, t1);
    float ta = max(max(tn.x, tn.y), max(tn.z, 0)), tb = min(min(tf.x, tf.y), min(tf.z, tmax));
    if (tb <= ta) return 0;
    int n = clamp((int)vcamf.w, 4, 160);
    // (steps of a third of a cell at the shortest; a long way through gets them all)
    float len = tb - ta, ds = max(len / n, vdims.w * 0.33);
    n = min(n, (int)(len / ds) + 1);
    ds = len / n;
    float t = ta + ds * rnd(seed);
    float3 acc = 0;
    float T = 1;
    float2 mid = (lo.xz + hi.xz) * 0.5, halfw = (hi.xz - lo.xz) * 0.5;
    [loop] for (int k = 0; k < n; k++, t += ds) {
        float3 p = vcam.xyz + rd * t;
        float3 tc = tex_at(p);
        float4 R = Rs.SampleLevel(vsamp, tc, 0);
        if (R.a < 1e-4) continue;
        R.rgb /= R.a;   // (what a tile's worth of it sends on: the billows below change how much there is, not its light)
        float2 e = (halfw - abs(p.xz - mid)) / 1.5;
        float edge = saturate(min(e.x, e.y));
        float4 G = Gs.SampleLevel(vsamp, tc, 0);
        // the billows finer than a cell: a noise carried along by the air there (where it was half a second ago), slowly
        // changing, that wears the thin parts into wisps and gathers the rest into clumps; the thick middle keeps more
        float bs = lerp(smkM.w, fogM.y, saturate(G.w));
        if (bs > 0.001) {
            float3 va = UAs.SampleLevel(vsamp, tc, 0).xyz;
            float3 q = (p - clamp(va, -1.5, 1.5) * 0.5) / max(vextra.w, 0.05);
            float3 d1 = float3(0.013, -0.021, 0.017) * vtm.y * 3.0, d2 = float3(7.1, 3.3 - vtm.y * 0.09, 1.7);
            float nz = 0.62 * lumps(q + d1) + 0.38 * lumps(q * 2.7 + d2);
            // drawn out along the way the air moves: the noise taken a little up and down the flow as well (moving
            // fog and smoke streak; still air keeps its round puffs)
            float sp = length(va);
            if (fogC.w > 0.001 && sp > 0.03) {
                float3 o = va / sp * (saturate(sp / 0.6) * 0.8 * saturate(fogC.w));
                float n2 = 0.62 * (lumps(q + d1 + o) + lumps(q + d1 - o)) + 0.38 * (lumps(q * 2.7 + d2 + o) + lumps(q * 2.7 + d2 - o));
                nz = 0.5 + ((nz + n2) / 3 - 0.5) * (1 + 0.5 * saturate(length(o) * 1.6));   // (the mean of three is flatter: its contrast given back)
            }
            // the shapes by kind: fog in thin veils (the noise's middle level, a sheet), smoke in round puffs with
            // creases between them (the same level left out)
            float sh = lerp(vshape.y, vshape.x, saturate(G.w));
            if (sh > 0.001) {
                float gz = (nz - 0.5) / 0.12;
                float veil = 0.25 + 0.6 * exp(-gz * gz * 4.0), puffs = 0.3 + 0.5 * saturate(abs(gz) / 1.4);
                nz = lerp(nz, lerp(puffs, veil, saturate(G.w)), saturate(sh));
            }
            // (more of them where the air shears, rolls as on top of a moving sheet of fog; less in still air)
            if (vmore.x > 0.001) bs *= lerp(1, 0.6 + 0.8 * saturate(DAs.SampleLevel(vsamp, tc, 0).z / 1.5), saturate(vmore.x));
            float thick = saturate(R.a * 0.35);
            float thr = 0.5 + 0.12 * (1 - thick) - 0.2 * thick;
            // (the thick middle spared: the fine shapes carve the thin outer parts, as fine noise does a cloud's edge)
            float carve = saturate(bs) * (1 - saturate(smkB.w) * smoothstep(0.35, 1.0, thick));
            R.a *= lerp(1, 2.1 * smoothstep(thr - 0.2, thr + 0.2, nz), carve);
        }
        float dom = saturate(length(G.xyz));   // how much of the light comes one way
        float ph = 1;
        if (dom > 0.02) {   // brighter toward the light, each kind by its own phase function (over an even share)
            float c = dot(G.xyz / max(length(G.xyz), 1e-4), rd);
            float pf = (1 - fogP.w) * hg(c, fogP.x) + fogP.w * draine(c, fogP.y, fogP.z);
            ph = lerp(1, min(lerp(hg(c, smkM.y), pf, saturate(G.w)), 5.0), dom);
        }
        float ext = R.a * edge, a = 1 - exp(-ext * ds);
        acc += T * a * R.rgb * ph;
        T *= 1 - a;
        if (T < 0.01) break;
    }
    // over the scene: its colour x what gets through, plus the fog's own (the scene's exposure)
    return float4(acc * vcamu.w, 1 - T);
}
// the half-size picture: each of its pixels the line of sight through the middle of the four it stands for, to the
// nearest of their surfaces
[numthreads(8, 8, 1)]
void cs_march(uint3 id : SV_DispatchThreadID) {
    if (id.x >= (uint)vscr.z || id.y >= (uint)vscr.w) return;
    float2 pc = float2(id.xy) * 2 + 1;
    float2 ndc = float2(pc.x / vscr.x * 2 - 1, 1 - pc.y / vscr.y * 2);
    uint seed = pcg(id.x * 1973u + id.y * 9277u + (uint)vtm.z * 26699u);
    LOWW[id.xy] = march(ndc, block_depth(int2(id.xy)), seed);
}
float4 ps(VVO i) : SV_Target0 {
    float depth = zbuf.Load(int3(int2(i.pos.xy), 0));
    if (vmisc.w > 0.5) {
        // the half-size picture brought up: its four nearest pixels, each by how near its surface is to this one's
        float zc = view_z(depth);
        float2 lp = i.pos.xy * 0.5 - 0.5;
        int2 b = int2(floor(lp));
        float2 f = lp - b;
        float4 sum = 0;
        float ws = 0, wb = 0;
        [unroll] for (int k = 0; k < 4; k++) {
            int2 o = int2(k & 1, k >> 1), q = clamp(b + o, int2(0, 0), int2(vscr.zw) - 1);
            float bw = (o.x ? f.x : 1 - f.x) * (o.y ? f.y : 1 - f.y);
            float wz = saturate(1 - abs(view_z(block_depth(q)) - zc) / (0.04 * zc + 0.05));
            sum += LOWs.Load(int3(q, 0)) * (bw * wz), ws += bw * wz, wb += bw;
        }
        if (ws > 0.3 * wb) return sum / ws;
        // (an edge: none of them at this depth: this pixel's own steps)
    }
    return march(i.ndc, depth, pcg((uint)i.pos.x * 1973u + (uint)i.pos.y * 9277u + (uint)vtm.z * 26699u));
}
