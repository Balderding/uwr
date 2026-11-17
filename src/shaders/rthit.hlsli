// What a ray finds: the level's triangles (their vertices and vertex numbers), the light reaching the
// surface a ray hit. Shared by the light probes and the scene's per-pixel bounced light (ray tracing High).
// Needs common.hlsli and scene_as (t3) declared first.
struct Vtx { float3 pos; float3 prev; float2 uv; float4 rect; float3 nrm; float4 col; };
StructuredBuffer<Vtx> verts : register(t5);
StructuredBuffer<uint> tri_index : register(t8); // three vertex numbers a triangle

bool lit_by(float3 a, float3 b) {
    float3 d = b - a;
    float len = length(d);
    if (len < 0.08) return true;
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE> q;
    RayDesc r;
    r.Origin = a, r.Direction = d / len, r.TMin = 0.0, r.TMax = len - 0.06;
    q.TraceRayInline(scene_as, RAY_FLAG_NONE, 0x01, r);
    q.Proceed();
    return q.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
}

// reach_window, but a light's reach counts only where the light itself sees the point (the user: past the torch's
// 3 tiles nothing is seen; a campfire round the corner let bounced torchlight light the far end of a hall), fading
// over the outer 40% of its range as the torch's does
float reach_window_seen(float3 p) {
    float r = 0;
    [loop] for (uint m = 0; m <= 8; m++) {
        uint k = light_at(p, m);
        if (k == NO_LIGHT) break;
        if (k == NO_LIGHT - 1) continue;
        if (k == 0 && lights[0].col.w > 1.5 && rtlim.w < 0) { r = 1; continue; }
        if (k == 0 && lights[0].col.w > 1.5) {
            float R = SHADES[(uint)(lights[0].col.w - 2) & 7].w * LIGHT_REACH;   // its level's reach (the original's), as it lights
            if (hdrout.w > 0.5) r = max(r, fade_window(length(p.xz - lights[0].col.xz) * 3.0 * LIGHT_REACH / R));
            else r = max(r, 1 - smoothstep(R - 0.75, R, length(p.xz - lights[0].col.xz)));
            continue;
        }
        float d = length(lights[k].pr.xyz - p) / max(lights[k].pr.w, 1e-3);
        const bool fade = hdrout.w > 0.5;   // (the user's fade, common.hlsli light_fade: over at 4/3 of the range)
        if ((fade ? d * LIGHT_REACH >= FADE_END : d >= 1) || r >= 1) continue;
        float w = fade ? fade_window(d * 3.0 * LIGHT_REACH) : 1 - smoothstep(0.6, 1.0, d);
        if (w > r && lit_by(p, lights[k].pr.xyz)) r = w;
    }
    return r;
}

// A ray from P along dir: the light it brings back (the surface's colour x the light reaching that surface:
// the lights through shadow rays, and bounce x the probes' bounced light there). one_light: only the torch
// and one of the tile's lights picked at random (counted as all of them), for a ray per pixel.
float3 ray_light(float3 P, float3 dir, StructuredBuffer<float4> pr, float bounce, bool one_light, inout uint seed, out float hit_t) {
    RayQuery<RAY_FLAG_FORCE_OPAQUE> rq;
    RayDesc rd;
    rd.Origin = P, rd.Direction = dir, rd.TMin = 0.005, rd.TMax = rtlim.x; // the bounce rays' reach (rt_ray_length)
    rq.TraceRayInline(scene_as, RAY_FLAG_NONE, 0x01, rd);
    rq.Proceed();
    hit_t = 48;
    if (rq.CommittedStatus() != COMMITTED_TRIANGLE_HIT) return 0;
    hit_t = rq.CommittedRayT();
    uint prim = rq.CommittedPrimitiveIndex();
    float2 b = rq.CommittedTriangleBarycentrics();
    Vtx v0 = verts[tri_index[prim * 3]], v1 = verts[tri_index[prim * 3 + 1]], v2 = verts[tri_index[prim * 3 + 2]];
    float3 w = float3(1 - b.x - b.y, b.x, b.y);
    float3 n = normalize(v0.nrm);
    if (dot(n, dir) >= 0) return 0; // the back of a surface: the ray left the open space
    float3 hp = P + dir * hit_t;
    float2 uv = v0.uv * w.x + v1.uv * w.y + v2.uv * w.z;
    float3 alb = lin((v0.rect.z > 0 ? atlas_at(v0.rect, uv).rgb : float3(1, 1, 1)) * v0.col.rgb);
    float3 inc = 0;
    float3 hl = hp + n * 0.01;
    if (one_light) {
        float3 Ld;
        float d;
        if (light_at(hl, 0) == 0) {
            float3 src;
            float3 e = light_term(0, hp, n, n, Ld, d, seed, src);
            if (lum(e) > 1e-5 && lit_by(hl, src)) inc += e;
        }
        uint count = 0;
        [loop] for (uint m = 1; m <= 8; m++) {
            if (light_at(hl, m) == NO_LIGHT) break;
            count++;
        }
        if (count > 0) {
            uint k = light_at(hl, 1 + min(count - 1, (uint)(rnd(seed) * count)));
            float3 src;
            float3 e = light_term(k, hp, n, n, Ld, d, seed, src);
            if (lum(e) > 1e-5 && lit_by(hl, src)) inc += e * count;
        }
    } else {
        [loop] for (uint m = 0; m <= 8; m++) {
            uint k = light_at(hl, m);
            if (k == NO_LIGHT) break;
            if (k == NO_LIGHT - 1) continue;
            float3 Ld;
            float d;
            float3 src;
            float3 e = light_term(k, hp, n, n, Ld, d, seed, src);
            if (lum(e) > 1e-5 && lit_by(hl, src)) inc += e;
        }
    }
    // the light that had already bounced there (the probes): a second bounce and on, unless one is the limit
    if (rtlim.y > 1.5) inc += bounce_at(pr, hp + n * 0.05, n) * bounce * reach_window_seen(hl);
    return alb * inc;
}
