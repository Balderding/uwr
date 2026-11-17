// The particles (render_particles.c; the effects plan's Task 3): one buffer of particles on the GPU, moved by a
// compute pass a frame (gravity, drag, the floor, their end) and drawn as soft dots facing the camera into the
// scene's colour, depth-tested against the level (never through a wall), no depth written. Kinds: 1 glowing (sparks,
// embers: added light, cooling to red), 2 lit (blood, drops, dust: the torch's light and a little of their own),
// 3 smoke and mist (lit, growing, rising, alpha), 4 the fog over water (faint, wide, hardly growing, coming and going
// slowly), 5 a campfire's smoke (thinning out steadily as it rises and widens, lit from below by its fire while young;
// it spreads under the ceiling).
#pragma pack_matrix(row_major)
cbuffer PC : register(b0) {
    float4x4 vp;     // the scene's view-projection (jittered as the scene's)
    float4 eye;      // xyz; w the exposure
    float4 right;    // the camera's right (tiles)
    float4 up;       // its up
    float4 sim;      // x dt (s), y spawns this frame, z the first spawn's slot, w 1: all cleared
    float4 torch;    // the player's light: position, w its reach (tiles)
    float4 torchc;   // its colour x strength
    float4 misc;     // x the buffer's size
};
struct P {
    float3 pos; float life;
    float3 vel; float age;
    float3 col; float size;
    float floor_y; float kind; float drag; float grav;
};

#ifdef CS
RWStructuredBuffer<P> parts : register(u0);
StructuredBuffer<P> spawns : register(t0);

[numthreads(64, 1, 1)]
void cs_spawn(uint3 id : SV_DispatchThreadID) {
    if (id.x >= (uint)sim.y) return;
    parts[((uint)sim.z + id.x) % (uint)misc.x] = spawns[id.x];
}

[numthreads(256, 1, 1)]
void cs_update(uint3 id : SV_DispatchThreadID) {
    if (id.x >= (uint)misc.x) return;
    P p = parts[id.x];
    if (p.life <= 0) return;
    float dt = sim.x;
    p.age += dt;
    if (p.age >= p.life || sim.w > 0.5) { p.life = 0; parts[id.x] = p; return; }
    p.vel.y -= p.grav * dt;
    p.vel *= exp(-p.drag * dt);
    p.pos += p.vel * dt;
    if (p.kind > 4.5 && p.pos.y > 4.0 - p.size) { p.pos.y = 4.0 - p.size; p.vel.y = 0; }   // smoke under the ceiling (CEIL)
    if (p.kind < 2.5 && p.pos.y < p.floor_y + p.size) {
        if (p.kind < 1.5 && p.vel.y < -0.3) {   // a spark bounces off the floor, losing most of its speed
            p.pos.y = p.floor_y + p.size, p.vel.y = -p.vel.y * 0.35, p.vel.xz *= 0.6;
        } else if (p.kind < 1.5) {
            p.pos.y = p.floor_y + p.size, p.vel = float3(p.vel.x * 0.8, 0, p.vel.z * 0.8);
        } else p.life = 0;                         // a drop is gone on the floor
    }
    parts[id.x] = p;
}
#else
StructuredBuffer<P> parts : register(t0);
struct VO { float4 pos : SV_Position; float2 uv : TEXCOORD0; nointerpolation float4 col : COLOR; };

VO vs(uint id : SV_VertexID) {
    static const float2 C[6] = {float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, -1), float2(1, 1), float2(-1, 1)};
    VO o;
    P p = parts[id / 6];
    float2 c = C[id % 6];
    o.uv = c;
    if (p.life <= 0 || p.age >= p.life) { o.pos = float4(2, 2, 2, 1); o.col = 0; return o; }
    float f = p.age / p.life, fade = f < 0.7 ? 1 : (1 - f) / 0.3;
    float s = p.size * (p.kind > 2.5 ? 1 + f * 1.5 : 1);
    if (p.kind > 3.5 && p.kind < 4.5) fade = saturate(f / 0.2) * saturate((1 - f) / 0.35), s = p.size * (1 + f * 0.6);   // fog: in and out slowly
    if (p.kind > 4.5) fade = saturate(f / 0.06) * pow(1 - f, 1.5), s = p.size * (1 + f * 3.0);                          // smoke: thinning all the way
    float3 w = p.pos + (right.xyz * c.x + up.xyz * c.y) * s;
    o.pos = mul(vp, float4(w, 1));
    float3 col = p.col;
    if (p.kind < 1.5) {   // glowing: cooling as it goes, orange to a dull red, then gone
        col = lerp(col, col * float3(0.6, 0.15, 0.03), saturate(f * 1.3));
        o.col = float4(col * fade * 3.0 * eye.w, 0);
    } else {               // lit: the player's light by its distance, and a little ambient
        float d = distance(p.pos, torch.xyz);
        float l = 0.06 + saturate(1 - d / max(torch.w, 0.5)) * 1.0;
        float a = (p.kind > 4.5 ? 0.4 : p.kind > 3.5 ? 0.16 : p.kind > 2.5 ? 0.45 : 0.9) * fade;
        float3 own = p.kind > 4.5 ? float3(1.0, 0.5, 0.2) * 0.6 * pow(1 - f, 3) : 0;   // its fire's light from below
        o.col = float4(col * (torchc.rgb * l + 0.04 + own) * eye.w * a, a);
    }
    return o;
}

// premultiplied: the colour added, the scene behind kept by 1 - alpha (glowing ones: alpha 0, pure light)
float4 ps(VO i) : SV_Target {
    float r = dot(i.uv, i.uv);
    if (r >= 1) discard;
    float k = (1 - r) * (1 - r);
    return i.col * k;
}
#endif
