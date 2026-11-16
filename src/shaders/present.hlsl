// The final picture: the scene (or DLSS's / DLSS 5's) back from linear light, bright parts rolled off
// toward white instead of clipped, then the light sources' orbs drawn on top at the output size, so
// neither DLSS nor DLSS 5 ever sees them (a tiny very bright dot made them ring and invent colour).
#include "cbuf.hlsli"

Texture2D<float4> src : register(t0);
Texture2D<float> depth : register(t1); // the scene's depth, at the render size
Texture2D<float4> ui : register(t2);   // the interface, post.w window pixels a pixel

float4 vs(uint id : SV_VertexID) : SV_Position {
    float2 p = float2((id << 1) & 2, id & 2);
    return float4(p * float2(2, -2) + float2(-1, 1), 0, 1);
}

float lum(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }
float depth_of(float w) { return FAR_Z / (FAR_Z - NEAR_Z) - NEAR_Z * FAR_Z / (FAR_Z - NEAR_Z) / w; }
float scene_depth(float2 px) {
    float2 q = clamp(px, 0, outp.xy - 1) * outp.w;
    return depth.Load(int3(int2(q), 0));
}

float4 ps(float4 pos : SV_Position) : SV_Target {
    float3 c = max(src.Load(int3(pos.xy, 0)).rgb * post.x, 0);
    float l = lum(c);
    c = max(lerp(l.xxx, c, post.y), 0);
    // a shoulder: up to 0.6 as it is, above it eased toward the brightest the screen shows: white (1) in SDR; in HDR
    // the monitor's peak (hdrout.z times paper white, as the scene's linear light: the screen shows c^(1/gamma), so
    // its limit is that to the gamma, 1 / (2.2 post.z))
    const bool hdr = hdrout.x > 0.5;
    float top = hdr ? pow(hdrout.z, 1.0 / (2.2 * post.z)) : 1.0;
    l = lum(c);
    if (l > 0.6) c *= (0.6 + (top - 0.6) * (1 - exp(-(l - 0.6) / (top - 0.6)))) / l;
    c = hdr ? max(pow(c, post.z), 0) : saturate(pow(c, post.z));   // (HDR: past 1, brighter than paper white)
    if (tint.w > 0) c *= tint.rgb; // the original's palette, faded or flashed (uwview.c)
    // the orbs
    int nl = (int)linfo.x;
    for (int k = 0; k < nl; k++) {
        float4 cc = mul(vp_nojit, float4(lpos[k].xyz, 1));
        if (cc.w < 0.05) continue;
        float2 sp = float2((cc.x / cc.w * 0.5 + 0.5) * outp.x, (0.5 - cc.y / cc.w * 0.5) * outp.y);
        float R = 0.05, rpx = R / cc.w * outp.z * outp.y * 0.5;
        float d = distance(pos.xy, sp);
        if (d > rpx * 10 + 6) continue;
        float zf = depth_of(max(cc.w - R, NEAR_Z * 1.5));
        float seen = zf <= scene_depth(sp) + 1e-6 ? 1 : 0;      // its middle, for the glow
        float here = zf <= scene_depth(pos.xy) + 1e-6 ? 1 : 0;  // this pixel, for its disc
        float3 col = lcol[k].rgb / max(max(lcol[k].r, lcol[k].g), max(lcol[k].b, 1e-4));
        float core = saturate(rpx + 0.75 - d) * here;
        float glow = exp(-max(d - rpx, 0) / (rpx * 1.2 + 1.5)) * 0.5 * seen;
        c = lerp(c, lerp(col, 1, 0.7), core);
        c = c + col * glow * (1 - core);
        c = hdr ? max(c, 0) : saturate(c);
    }
    // the interface over everything
    float4 u = ui.Load(int3(int2(pos.xy / max(post.w, 1)), 0));
    c = lerp(c, u.rgb, u.a);
    // HDR: the picture as the SDR screen would show it (its 2.2 gamma undone: linear light, 1 = paper white), in
    // scRGB (1.0 = 80 nits): paper white's nits / 80; the interface's white lands on paper white
    if (hdr) c = pow(max(c, 0), 2.2) * hdrout.y;
    return float4(c, 1);
}
