/* render_decals.c: the decals (the effects plan's Task 4; src\shaders\decal.hlsl): marks lying on the floor, a ring of
 * RD_MAX a level (the oldest replaced), each a square a hair above the floor turned by its yaw, drawn after the level
 * and its pictures, depth-tested, no depth written, multiplying the floor's colour. They are presentation only: not
 * saved (a load shows a clean floor: uwview.c clears them on a level change and on a jump of the eye), never read by
 * the game. Off (fx_decals 0): none drawn. Walls get none (a blow's mark on a wall is left to the game's own spark). */
#include "render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ngx.h" /* d3d12.h with COBJMACROS */
#include <windows.h>

#include "decal_ps.h"
#include "decal_vs.h"

#define RD_MAX 512
typedef struct { float pos[3], uv[2], col[4], seed; } DVert;   /* 40 bytes */
typedef struct { float pos[3], size, yaw, col[3], seed; double born; } Decal;

static struct {
    bool ok;
    ID3D12Device *dev;
    ID3D12RootSignature *root;
    ID3D12PipelineState *pso;
    ID3D12Resource *vb, *cbuf;
    DVert *vp;
    float *cb;
    Decal d[RD_MAX];
    int n, head;
} D;

static ID3D12Resource *upload(size_t size)
{
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};
    D3D12_RESOURCE_DESC d;
    memset(&d, 0, sizeof d);
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER, d.Width = size, d.Height = d.DepthOrArraySize = d.MipLevels = 1;
    d.SampleDesc.Count = 1, d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource *r = NULL;
    if (FAILED(ID3D12Device_CreateCommittedResource(D.dev, &hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                    &IID_ID3D12Resource, (void **)&r)))
        return NULL;
    return r;
}

bool rfx_decals_init(void *devp)
{
    memset(&D, 0, sizeof D);
    D.dev = (ID3D12Device *)devp;
    D3D12_ROOT_PARAMETER p[1];
    memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    D3D12_ROOT_SIGNATURE_DESC rd = {1, p, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    ID3DBlob *blob = NULL, *err = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) return logf_("render: decals: no root signature\n"), false;
    ID3D12Device_CreateRootSignature(D.dev, 0, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), &IID_ID3D12RootSignature, (void **)&D.root);
    static const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
    memset(&d, 0, sizeof d);
    d.pRootSignature = D.root;
    d.VS.pShaderBytecode = DECAL_VS, d.VS.BytecodeLength = sizeof DECAL_VS;
    d.PS.pShaderBytecode = DECAL_PS, d.PS.BytecodeLength = sizeof DECAL_PS;
    D3D12_RENDER_TARGET_BLEND_DESC *b = &d.BlendState.RenderTarget[0];
    b->BlendEnable = TRUE;   /* the floor times the mark */
    b->SrcBlend = D3D12_BLEND_DEST_COLOR, b->DestBlend = D3D12_BLEND_ZERO, b->BlendOp = D3D12_BLEND_OP_ADD;
    b->SrcBlendAlpha = D3D12_BLEND_ZERO, b->DestBlendAlpha = D3D12_BLEND_ONE, b->BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    d.SampleMask = 0xFFFFFFFFu;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID, d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.DepthStencilState.DepthEnable = TRUE, d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    d.InputLayout.pInputElementDescs = layout, d.InputLayout.NumElements = 4;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1, d.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    d.SampleDesc.Count = 1;
    ID3D12Device_CreateGraphicsPipelineState(D.dev, &d, &IID_ID3D12PipelineState, (void **)&D.pso);
    D.vb = upload(sizeof(DVert) * 6 * RD_MAX);
    D.cbuf = upload(256);
    if (!D.root || !D.pso || !D.vb || !D.cbuf) return logf_("render: decals: not started\n"), false;
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Map(D.vb, 0, &none, (void **)&D.vp);
    ID3D12Resource_Map(D.cbuf, 0, &none, (void **)&D.cb);
    D.ok = true;
    return true;
}

void render_decals_add(float x, float y, float z, int kind, float size, float yaw, const float rgb[3])
{
    static const float COL[RD_KINDS][3] = {{0.40f, 0.04f, 0.03f}, {0.22f, 0.19f, 0.16f}, {0.75f, 0.88f, 1.0f}, {0.45f, 0.75f, 0.12f}};
    Decal *d = &D.d[D.head];
    D.head = (D.head + 1) % RD_MAX;
    if (D.n < RD_MAX) D.n++;
    d->pos[0] = x, d->pos[1] = y, d->pos[2] = z, d->size = size, d->yaw = yaw;
    memcpy(d->col, rgb ? rgb : COL[kind < 0 || kind >= RD_KINDS ? 0 : kind], 12);
    d->seed = (float)((D.head * 2654435761u) % 1000u) / 1000.0f;
    d->born = (double)GetTickCount64() / 1000.0;
}

void render_decals_clear(void) { D.n = 0, D.head = 0; }
int render_decals_count(void) { return D.n; }

/* drawn into the scene's colour (rtv) with its depth (dsv), the scene's viewport set */
void rfx_decals_draw(void *clp, const float vp[16], size_t rtv_ptr, size_t dsv_ptr)
{
    ID3D12GraphicsCommandList *cl = (ID3D12GraphicsCommandList *)clp;
    if (!D.ok || !D.n) return;
    double now = (double)GetTickCount64() / 1000.0;
    int nv = 0;
    for (int i = 0; i < D.n; i++) {
        const Decal *d = &D.d[i];
        float a = (float)((now - d->born) / 0.25);   /* it spreads in a quarter of a second */
        a = a < 0 ? 0 : a > 1 ? 1 : a;
        float s = d->size * (0.6f + 0.4f * a), c = cosf(d->yaw) * s, sn = sinf(d->yaw) * s;
        static const float CU[6] = {0, 1, 1, 0, 1, 0}, CV[6] = {0, 0, 1, 0, 1, 1};
        for (int k = 0; k < 6; k++) {
            DVert *v = &D.vp[nv++];
            float u = CU[k] * 2 - 1, w = CV[k] * 2 - 1;
            v->pos[0] = d->pos[0] + u * c - w * sn, v->pos[1] = d->pos[1] + 0.006f, v->pos[2] = d->pos[2] + u * sn + w * c;
            v->uv[0] = CU[k], v->uv[1] = CV[k];
            memcpy(v->col, d->col, 12), v->col[3] = 0.85f * a;
            v->seed = d->seed;
        }
    }
    memcpy(D.cb, vp, 64);
    D.cb[16] = (float)now;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = {rtv_ptr}, dsv = {dsv_ptr};
    ID3D12GraphicsCommandList_OMSetRenderTargets(cl, 1, &rtv, FALSE, &dsv);
    ID3D12GraphicsCommandList_SetGraphicsRootSignature(cl, D.root);
    ID3D12GraphicsCommandList_SetPipelineState(cl, D.pso);
    ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(cl, 0, ID3D12Resource_GetGPUVirtualAddress(D.cbuf));
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D12_VERTEX_BUFFER_VIEW vbv = {ID3D12Resource_GetGPUVirtualAddress(D.vb), (UINT)(sizeof(DVert) * (size_t)nv), sizeof(DVert)};
    ID3D12GraphicsCommandList_IASetVertexBuffers(cl, 0, 1, &vbv);
    ID3D12GraphicsCommandList_DrawInstanced(cl, (UINT)nv, 1, 0, 0);
}
