// The decals (render_decals.c; the effects plan's Task 4): marks lying on the floor (blood by who bled, scorch, ice,
// acid), drawn after the level and its pictures as quads a hair above the floor, depth-tested against what is drawn
// (a creature standing on one hides it), no depth written, multiplying the floor's colour: they darken and tint what
// is there, so they take its light and its shadows as they are.
#pragma pack_matrix(row_major)
cbuffer DC : register(b0) {
    float4x4 vp;
    float4 misc;   // x the time (s)
};
struct VI { float3 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; float seed : TEXCOORD1; };
struct VO { float4 pos : SV_Position; float2 uv : TEXCOORD0; nointerpolation float4 col : COLOR; nointerpolation float seed : TEXCOORD1; };

VO vs(VI i) {
    VO o;
    o.pos = mul(vp, float4(i.pos, 1));
    o.uv = i.uv, o.col = i.col, o.seed = i.seed;
    return o;
}

float hash(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
float noise(float2 p) {
    float2 i = floor(p), f = frac(p);
    f = f * f * (3 - 2 * f);
    return lerp(lerp(hash(i), hash(i + float2(1, 0)), f.x), lerp(hash(i + float2(0, 1)), hash(i + 1), f.x), f.y);
}

// col.rgb the mark's colour, col.a its strength (fading with its age); the floor multiplied by lerp(1, colour, cover)
float4 ps(VO i) : SV_Target {
    float2 p = i.uv * 2 - 1;
    float r = length(p);
    // a splash: a ragged blob (its edge moved by noise) with a few drops round it, each mark its own (seed)
    float edge = 0.62 + 0.28 * noise(p * 3.1 + i.seed * 17.3) + 0.12 * noise(p * 7.3 - i.seed * 5.1);
    float cover = smoothstep(edge, edge - 0.12, r);
    float2 q = p * 4.5 + i.seed * 9.7;
    float drops = step(0.86, noise(q)) * step(r, 0.98);
    cover = saturate(max(cover, drops)) * i.col.a;
    if (cover < 0.01) discard;
    return float4(lerp(float3(1, 1, 1), i.col.rgb, cover), 1);
}
