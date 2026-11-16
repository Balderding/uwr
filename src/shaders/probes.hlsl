// The light probes: each fires 32 rays a frame (directions turned at random each frame) and keeps the light
// arriving from six directions, blended with what it had (look.z of the old). A ray that hits a surface
// brings back that surface's colour x (the lights reaching it, through shadow rays + the probes' bounced
// light there, last frame's): light bounces as many times as frames go by.
#include "common.hlsli"

RaytracingAccelerationStructure scene_as : register(t3);
#include "rthit.hlsli"
StructuredBuffer<float4> prev_probes : register(t4);
RWStructuredBuffer<float4> out_probes : register(u0);

static const int RAYS = 32;

float3 rotate(float4 q, float3 v) { return v + 2 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }

[numthreads(64, 1, 1)]
void cs(uint id : SV_DispatchThreadID) {
    if (id >= 64 * 64 * PL) return;
    int layer = id % PL;
    int2 c = int2((id / PL) % 64, (id / PL) / 64);
    uint base = id * 6;
    uint4 t = tile(c);
    if (!probe_ok(t, layer)) {
        [unroll] for (int a = 0; a < 6; a++) out_probes[base + a] = 0;
        return;
    }
    float3 P = probe_pos(c, layer);
    // this frame's turn of the ray pattern: a random rotation (uniform quaternion)
    uint seed = pcg((uint)eye.w * 7919u + 17u);
    float u1 = rnd(seed), u2 = rnd(seed) * 6.2831853, u3 = rnd(seed) * 6.2831853;
    float4 q = float4(sqrt(1 - u1) * sin(u2), sqrt(1 - u1) * cos(u2), sqrt(u1) * sin(u3), sqrt(u1) * cos(u3));
    float3 E[6] = {float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0)};
    [loop] for (int r = 0; r < RAYS; r++) {
        // directions spread evenly over the sphere (Fibonacci), turned
        float cz = 1 - (2 * r + 1) / (float)RAYS, sz = sqrt(saturate(1 - cz * cz)), ph = r * 2.39996323;
        float3 dir = rotate(q, float3(sz * cos(ph), cz, sz * sin(ph)));
        float hit_t;
        float3 L = ray_light(P, dir, prev_probes, look.y, false, seed, hit_t);
        E[0] += L * max(dir.x, 0), E[1] += L * max(-dir.x, 0);
        E[2] += L * max(dir.y, 0), E[3] += L * max(-dir.y, 0);
        E[4] += L * max(dir.z, 0), E[5] += L * max(-dir.z, 0);
    }
    // light per unit of surface colour from each direction: (4 pi / RAYS) sum L cos / pi
    [unroll] for (int a = 0; a < 6; a++) {
        float3 now = E[a] * (4.0 / RAYS);
        float3 old = prev_probes[base + a].rgb;
        out_probes[base + a] = float4(lerp(now, old, look.z), 1);
    }
}
