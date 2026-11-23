// The frame's constants (render.c Constants, same order).
#pragma pack_matrix(row_major)
cbuffer C : register(b0) {
    float4x4 vp;        // view-projection, jittered (DLSS)
    float4x4 vp_nojit;
    float4x4 vp_prev;   // last frame's, unjittered
    float4 eye;         // xyz; w = frame number
    float4 look;        // x = light size (soft shadows), y = bounce light strength, z = probe blend (0..1),
                        // w = the stone's sheen (a soft highlight that follows the normal maps; 0 none)
    float4 size;        // render w, h; z = exposure
    float4 post;        // x = 1 / exposure, y = colour given back after DLSS 5, z = 1 / (2.2 gamma), w = UI scale
    float4 lpos[16];    // the orbs drawn this frame (placed lights near the camera): position + range
    float4 lcol[16];    // their colour x intensity
    float4 linfo;       // x = orbs, y = normal-map strength (0 off), z = relief depth (tiles)
    float4 outp;        // output w, h; z = 1 / tan(fov / 2); w = render size / output size
    float4 tint;        // the scene multiplied by rgb after its gamma (the original's palette fades and flashes); w = on
    float4 rtlim;       // the bounce rays: x = how far they go (tiles); y = bounces (1: one; 2 and more: the probes'
                        // light fed back into them, as many as build up); z = the pictures' normal strength (0: as
                        // the walls')
    float4 torchc;      // the player's flame's colour now (render_blackbody: its temperature with the flicker), unit luminance
    float4 sprite;      // the pictures' Laigter maps: x = occlusion strength (0 none), y = height parallax (texels, 0 none);
                        // the walls' and floors': z = occlusion strength, w = the relief's depth its shadows are cast from (texels)
    float4 fxw;         // the effects' water (fx_water, render.c): x on, y the ripples, z the time (s)
    float4 ripple[16];  // the splashes' rings: x, y the place (tiles: x, z), z their age (s), w their strength
    float4 texres;      // the walls' and floors' high-res copy (hatlas, the Esc menu's Textures): x its scale (0: off),
                        // y > 0: relief maps beside it (common.hlsli hmaps_at), z > 0: its colours are there (0: only maps)
    float4 hdrout;      // HDR output (render.c render_set_hdr): x on, y paper white / 80 nits, z peak / paper white;
                        // w > 0.5: the lights' fade by distance (common.hlsli light_fade)
    float4 panel;       // our panel on the GPU (present.hlsl, Vulkan): x on, y its scale (window pixels a pixel), z, w its size
};
static const float NEAR_Z = 0.02, FAR_Z = 200.0;
