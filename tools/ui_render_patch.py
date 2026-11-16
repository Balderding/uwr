"""One-off patch: the UI overlay in render.c / render.h / present.hlsl (kept as a script: heredocs mangle
backslashes)."""
import sys

ROOT = sys.argv[1]


def edit(p, reps):
    p = ROOT + '/' + p
    s = open(p, encoding='utf-8').read()
    for a, b in reps:
        assert s.count(a) == 1, (p, a[:70])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)


edit('src/render.c', [
    ("    ID3D12Resource *sprite_vb;\n",
     "    ID3D12Resource *sprite_vb;\n"
     "    /* the interface overlay (ui.c): a whole factor smaller than the window, uploaded every frame */\n"
     "    ID3D12Resource *ui_tex, *ui_up;\n"
     "    u8 *ui_ptr;\n"
     "    D3D12_PLACED_SUBRESOURCE_FOOTPRINT ui_fp;\n"
     "    int ui_w, ui_h, ui_s;\n"
     "    bool ui_dirty;\n"),
    ("    R.sprite_vb = make_buffer(sizeof(Vertex) * RENDER_MAX_SPRITE_VERTS, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);\n",
     "    R.sprite_vb = make_buffer(sizeof(Vertex) * RENDER_MAX_SPRITE_VERTS, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);\n"
     "    R.ui_s = MAX(1, (h + 150) / 300), R.ui_w = w / R.ui_s, R.ui_h = h / R.ui_s;\n"
     "    R.ui_tex = make_texture(R.ui_w, R.ui_h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL);\n"
     "    if (R.ui_tex) {\n"
     "        D3D12_RESOURCE_DESC ud = ID3D12Resource_GetDesc(R.ui_tex);\n"
     "        UINT64 ut = 0;\n"
     "        ID3D12Device_GetCopyableFootprints(R.dev, &ud, 0, 1, 0, &R.ui_fp, NULL, NULL, &ut);\n"
     "        R.ui_up = make_buffer((size_t)ut, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);\n"
     "    }\n"),
    ("        srv(R.depth, DXGI_FORMAT_R32_FLOAT, slot[i] + 2);\n",
     "        srv(R.ui_tex, DXGI_FORMAT_R8G8B8A8_UNORM, slot[i] + 2);\n"),
    ("void render_set_sprites(const Vertex *v, int n) {",
     "void render_ui_size(int *w, int *h, int *scale) { *w = R.ui_w, *h = R.ui_h, *scale = R.ui_s; }\n"
     "void render_set_ui(const u8 *rgba) {\n"
     "    if (!R.ui_up) return;\n"
     "    if (!R.ui_ptr) {\n"
     "        D3D12_RANGE none = {0, 0};\n"
     "        ID3D12Resource_Map(R.ui_up, 0, &none, (void **)&R.ui_ptr);\n"
     "    }\n"
     "    for (int y = 0; y < R.ui_h; y++) memcpy(R.ui_ptr + R.ui_fp.Offset + (size_t)y * R.ui_fp.Footprint.RowPitch, rgba + (size_t)y * R.ui_w * 4, (size_t)R.ui_w * 4);\n"
     "    R.ui_dirty = true;\n"
     "}\n\n"
     "void render_set_sprites(const Vertex *v, int n) {"),
    ("    c->post[0] = 1.0f / R.cfg.exposure, c->post[1] = 1.0f, c->post[2] = 1.0f / (2.2f * gamma);",
     "    c->post[0] = 1.0f / R.cfg.exposure, c->post[1] = 1.0f, c->post[2] = 1.0f / (2.2f * gamma), c->post[3] = (float)R.ui_s;"),
    ("    ID3D12DescriptorHeap *heaps[1] = {R.srv_heap};\n",
     "    if (R.ui_dirty && R.ui_tex) { /* this frame's interface */\n"
     "        barrier(R.ui_tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);\n"
     "        D3D12_TEXTURE_COPY_LOCATION ud = {R.ui_tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {.SubresourceIndex = 0}};\n"
     "        D3D12_TEXTURE_COPY_LOCATION us = {R.ui_up, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {.PlacedFootprint = R.ui_fp}};\n"
     "        ID3D12GraphicsCommandList_CopyTextureRegion(R.cl, &ud, 0, 0, 0, &us, NULL);\n"
     "        barrier(R.ui_tex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);\n"
     "        R.ui_dirty = false;\n"
     "    }\n"
     "    ID3D12DescriptorHeap *heaps[1] = {R.srv_heap};\n"),
])

edit('src/render.h', [
    ("void render_set_sprites(const Vertex *v, int n);",
     "void render_set_sprites(const Vertex *v, int n);\n"
     "/* the interface overlay: its size (a whole factor, scale, smaller than the window) and this frame's pixels */\n"
     "void render_ui_size(int *w, int *h, int *scale);\n"
     "void render_set_ui(const u8 *rgba);"),
])

edit('src/shaders/present.hlsl', [
    ("Texture2D<float> depth : register(t1); // the scene's depth, at the render size",
     "Texture2D<float> depth : register(t1); // the scene's depth, at the render size\n"
     "Texture2D<float4> ui : register(t2);   // the interface, post.w window pixels a pixel"),
    ("    return float4(c, 1);\n}",
     "    // the interface over everything\n"
     "    float4 u = ui.Load(int3(int2(pos.xy / max(post.w, 1)), 0));\n"
     "    c = lerp(c, u.rgb, u.a);\n"
     "    return float4(c, 1);\n}"),
])
edit('src/shaders/cbuf.hlsli', [
    ("    float4 post;        // x = 1 / exposure, y = colour given back after DLSS 5, z = 1 / (2.2 gamma)",
     "    float4 post;        // x = 1 / exposure, y = colour given back after DLSS 5, z = 1 / (2.2 gamma), w = UI scale"),
])
print('ok')
