/* render_particles.c: the GPU particles (the effects plan's Task 3; src\shaders\particles.hlsl). One structured buffer
 * of RP_MAX particles (RParticle: 64 bytes), a ring: what the frame spawns (render_particles_spawn, from uwview.c's
 * emitters: the flames' embers, the blows' sparks and blood, the splashes) is copied into the next slots by a compute
 * pass, a second moves every particle (gravity, drag, the floor, its end), and one draw of six vertices a particle
 * (the dead ones collapsed off the screen) puts them into the scene's colour after the level and the pictures,
 * depth-tested (never seen through a wall), writing no depth and no motion. Off (fx_particles 0): none of it runs.
 * The live count is kept on the CPU from each spawn's life (no readback): render_particles_live. */
#include "render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ngx.h" /* d3d12.h with COBJMACROS */
#include <windows.h>

#include "particles_cs_spawn.h"
#include "particles_cs_update.h"
#include "particles_ps.h"
#include "particles_vs.h"

#define RP_SPAWN_MAX 8192

typedef struct {
    float vp[16];
    float eye[4], right[4], up[4], sim[4], torch[4], torchc[4], misc[4];
} RpConst;

static struct {
    bool ok;
    ID3D12Device *dev;
    ID3D12RootSignature *croot, *groot;
    ID3D12PipelineState *spawn_pso, *update_pso, *draw_pso;
    ID3D12Resource *parts, *spawn_up, *cbuf;
    RpConst *cb;
    RParticle *spawn_ptr;
    int nspawn;          /* waiting for the next frame */
    unsigned head;       /* the next slot */
    bool clear;          /* all cleared at the next frame (a level change) */
    bool drawable;       /* the compute ran this frame: the buffer is readable */
    double death[RP_MAX];/* each slot's end (s, the clock below), for the live count */
    double now;
} P;

/* UWVIEW_PART_DT=MS (the checks only: tools\vkcompare.py): the particles' clock counts frames, MS milliseconds each
   (rfx_particles_frame steps it), not the machine's, so that the ring after N frames is the same in every run and in
   both renderers. Unset (always, outside a check): the machine's clock, as before. */
static double g_part_clock;
static double part_dt_check(void)
{
    static int got;
    static double v;
    if (!got) { const char *e = getenv("UWVIEW_PART_DT"); got = 1; if (e && *e) v = atof(e) / 1000.0; }
    return v;
}
static double clock_s(void)
{
    if (part_dt_check() > 0) return g_part_clock;
    return (double)GetTickCount64() / 1000.0;
}

static ID3D12Resource *buffer(size_t size, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags)
{
    D3D12_HEAP_PROPERTIES hp = {heap, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};
    D3D12_RESOURCE_DESC d;
    memset(&d, 0, sizeof d);
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, d.Width = size, d.Height = d.DepthOrArraySize = d.MipLevels = 1;
    d.SampleDesc.Count = 1, d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR, d.Flags = flags;
    ID3D12Resource *r = NULL;
    if (FAILED(ID3D12Device_CreateCommittedResource(P.dev, &hp, D3D12_HEAP_FLAG_NONE, &d, state, NULL, &IID_ID3D12Resource, (void **)&r)))
        return NULL;
    return r;
}

static ID3D12RootSignature *root(const D3D12_ROOT_PARAMETER *p, int n)
{
    D3D12_ROOT_SIGNATURE_DESC rd = {(UINT)n, p, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ID3DBlob *blob = NULL, *err = NULL;
    ID3D12RootSignature *rs = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) return NULL;
    ID3D12Device_CreateRootSignature(P.dev, 0, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), &IID_ID3D12RootSignature, (void **)&rs);
    return rs;
}

bool rfx_particles_init(void *devp)
{
    ID3D12Device *dev = (ID3D12Device *)devp;
    memset(&P, 0, sizeof P);
    P.dev = dev;
    /* compute: b0, u0 the particles, t0 the spawns; graphics: b0, t0 the particles */
    D3D12_ROOT_PARAMETER cp[3], gp[2];
    memset(cp, 0, sizeof cp), memset(gp, 0, sizeof gp);
    cp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    cp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    cp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    gp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    gp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    P.croot = root(cp, 3), P.groot = root(gp, 2);
    if (!P.croot || !P.groot) return logf_("render: particles: no root signature\n"), false;
    D3D12_COMPUTE_PIPELINE_STATE_DESC cd;
    memset(&cd, 0, sizeof cd);
    cd.pRootSignature = P.croot;
    cd.CS.pShaderBytecode = PARTICLES_CS_SPAWN, cd.CS.BytecodeLength = sizeof PARTICLES_CS_SPAWN;
    ID3D12Device_CreateComputePipelineState(dev, &cd, &IID_ID3D12PipelineState, (void **)&P.spawn_pso);
    cd.CS.pShaderBytecode = PARTICLES_CS_UPDATE, cd.CS.BytecodeLength = sizeof PARTICLES_CS_UPDATE;
    ID3D12Device_CreateComputePipelineState(dev, &cd, &IID_ID3D12PipelineState, (void **)&P.update_pso);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
    memset(&d, 0, sizeof d);
    d.pRootSignature = P.groot;
    d.VS.pShaderBytecode = PARTICLES_VS, d.VS.BytecodeLength = sizeof PARTICLES_VS;
    d.PS.pShaderBytecode = PARTICLES_PS, d.PS.BytecodeLength = sizeof PARTICLES_PS;
    D3D12_RENDER_TARGET_BLEND_DESC *b = &d.BlendState.RenderTarget[0];
    b->BlendEnable = TRUE;   /* premultiplied: src + dst (1 - src alpha) */
    b->SrcBlend = D3D12_BLEND_ONE, b->DestBlend = D3D12_BLEND_INV_SRC_ALPHA, b->BlendOp = D3D12_BLEND_OP_ADD;
    b->SrcBlendAlpha = D3D12_BLEND_ZERO, b->DestBlendAlpha = D3D12_BLEND_ONE, b->BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    d.SampleMask = 0xFFFFFFFFu;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID, d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.DepthStencilState.DepthEnable = TRUE, d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1, d.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    d.SampleDesc.Count = 1;
    ID3D12Device_CreateGraphicsPipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&P.draw_pso);
    if (!P.spawn_pso || !P.update_pso || !P.draw_pso) return logf_("render: particles: no pipeline\n"), false;
    P.parts = buffer(sizeof(RParticle) * RP_MAX, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    P.spawn_up = buffer(sizeof(RParticle) * RP_SPAWN_MAX, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
    P.cbuf = buffer(256, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
    if (!P.parts || !P.spawn_up || !P.cbuf) return logf_("render: particles: out of GPU memory\n"), false;
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Map(P.spawn_up, 0, &none, (void **)&P.spawn_ptr);
    ID3D12Resource_Map(P.cbuf, 0, &none, (void **)&P.cb);
    P.clear = true;   /* the buffer starts as whatever the memory held: cleared at the first frame */
    P.ok = true;
    return true;
}

void render_particles_spawn(const RParticle *p, int n)
{
    if (!P.ok) return;
    double t = clock_s();
    for (int i = 0; i < n && P.nspawn < RP_SPAWN_MAX; i++) {
        P.spawn_ptr[P.nspawn++] = p[i];
        P.death[(P.head + (unsigned)P.nspawn - 1) % RP_MAX] = t + p[i].life;
    }
}

void render_particles_clear(void)
{
    if (!P.ok) return;
    P.clear = true, P.nspawn = 0;
    memset(P.death, 0, sizeof P.death);
}

int render_particles_live(void)
{
    double t = clock_s();
    int n = 0;
    for (int i = 0; i < RP_MAX; i++) n += P.death[i] > t;
    return n;
}

/* the frame's simulation (before the scene is drawn): the constants, the spawns into the ring, every particle moved */
void rfx_particles_frame(void *clp, const float vp[16], const float eye[3], float exposure, const float right[3], const float up[3],
                     const float torch[4], const float torchc[3])
{
    ID3D12GraphicsCommandList *cl = (ID3D12GraphicsCommandList *)clp;
    P.drawable = false;
    if (!P.ok) return;
    if (part_dt_check() > 0) g_part_clock += part_dt_check();   /* (a check's clock: one step a frame) */
    double t = clock_s();
    float dt = P.now > 0 ? (float)(t - P.now) : 0;
    P.now = t;
    if (dt > 0.05f) dt = 0.05f;
    RpConst *c = P.cb;
    memcpy(c->vp, vp, 64);
    memcpy(c->eye, eye, 12), c->eye[3] = exposure;
    memcpy(c->right, right, 12), memcpy(c->up, up, 12);
    c->sim[0] = dt, c->sim[1] = (float)P.nspawn, c->sim[2] = (float)P.head, c->sim[3] = P.clear ? 1.0f : 0.0f;
    memcpy(c->torch, torch, 16), memcpy(c->torchc, torchc, 12);
    c->misc[0] = (float)RP_MAX;
    D3D12_GPU_VIRTUAL_ADDRESS cb = ID3D12Resource_GetGPUVirtualAddress(P.cbuf);
    ID3D12GraphicsCommandList_SetComputeRootSignature(cl, P.croot);
    ID3D12GraphicsCommandList_SetComputeRootConstantBufferView(cl, 0, cb);
    ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(cl, 1, ID3D12Resource_GetGPUVirtualAddress(P.parts));
    ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(cl, 2, ID3D12Resource_GetGPUVirtualAddress(P.spawn_up));
    /* the update first (the clear kills the old), then the new ones placed (moved from the next frame) */
    ID3D12GraphicsCommandList_SetPipelineState(cl, P.update_pso);
    ID3D12GraphicsCommandList_Dispatch(cl, (RP_MAX + 255) / 256, 1, 1);
    D3D12_RESOURCE_BARRIER b;
    memset(&b, 0, sizeof b);
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV, b.UAV.pResource = P.parts;
    ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &b);
    if (P.nspawn) {
        ID3D12GraphicsCommandList_SetPipelineState(cl, P.spawn_pso);
        ID3D12GraphicsCommandList_Dispatch(cl, (UINT)(P.nspawn + 63) / 64, 1, 1);
    }
    memset(&b, 0, sizeof b);
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, b.Transition.pResource = P.parts;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS, b.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &b);
    P.head = (P.head + (unsigned)P.nspawn) % RP_MAX;
    P.nspawn = 0, P.clear = false, P.drawable = true;
}

/* the draw, into the scene's colour (rtv) with its depth (dsv) bound, the scene's viewport set; the buffer back to
 * the compute pass's state after */
void rfx_particles_draw(void *clp, size_t rtv_ptr, size_t dsv_ptr)
{
    ID3D12GraphicsCommandList *cl = (ID3D12GraphicsCommandList *)clp;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = {rtv_ptr}, dsv = {dsv_ptr};
    if (!P.ok || !P.drawable) return;
    ID3D12GraphicsCommandList_OMSetRenderTargets(cl, 1, &rtv, FALSE, &dsv);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(cl, P.groot);
    ID3D12GraphicsCommandList_SetPipelineState(cl, P.draw_pso);
    ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(cl, 0, ID3D12Resource_GetGPUVirtualAddress(P.cbuf));
    ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(cl, 1, ID3D12Resource_GetGPUVirtualAddress(P.parts));
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D12GraphicsCommandList_DrawInstanced(cl, RP_MAX * 6, 1, 0, 0);
    D3D12_RESOURCE_BARRIER b;
    memset(&b, 0, sizeof b);
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, b.Transition.pResource = P.parts;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, b.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    ID3D12GraphicsCommandList_ResourceBarrier(cl, 1, &b);
    P.drawable = false;
}

