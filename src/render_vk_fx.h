/* render_vk_fx.h: the effects' passes of the Vulkan renderer (milestone M3): the particles (render_particles.c's:
 * particles.hlsl's SPIR-V; a ring of RP_MAX on the GPU, a compute pass that moves them and one that places the frame's
 * spawns, one draw of six vertices a particle) and the decals (render_decals.c's: decal.hlsl's SPIR-V; up to RD_MAX
 * marks on the floor, a quad each). The CPU side is theirs line for line (the ring, the live count, the marks' spread
 * and colours); only the GPU calls differ. Included by render_vk.c after its helpers (V, make_buf, shader, rv_ticks).
 *
 * As render.c: the particles are moved before the scene is drawn (vfx_particles_frame, outside a rendering), and the
 * decals then the particles are drawn after the level and its cards into the scene's colour with its depth tested and
 * not written (vfx_draw, inside a rendering of colour + depth only: the motion is not theirs). Bindings as
 * buildview.cmd's SPV flags give them: b0 at 0, t0 at 16, u0 at 48. */
#include "decal_ps_spv.h"
#include "decal_vs_spv.h"
#include "particles_cs_spawn_spv.h"
#include "particles_cs_update_spv.h"
#include "particles_ps_spv.h"
#include "particles_vs_spv.h"

#define RP_SPAWN_MAX 8192
#define RD_MAX 512

typedef struct {
    float vp[16];
    float eye[4], right[4], up[4], sim[4], torch[4], torchc[4], misc[4];
} RpConst;
typedef struct { float pos[3], uv[2], col[4], seed; } DVert;   /* 40 bytes */
typedef struct { float pos[3], size, yaw, col[3], seed; double born; } Decal;

static struct {
    VkDescriptorPool pool;
    /* the particles */
    bool pok;
    VkDescriptorSetLayout pdsl;
    VkPipelineLayout ppl;
    VkPipeline spawn_pipe, update_pipe, draw_pipe;
    VkDescriptorSet cset, gset;      /* compute: b0, u0 the particles, t0 the spawns; the draw: b0, t0 the particles */
    Buf parts, spawn_up, pcb;
    int nspawn;          /* waiting for the next frame */
    unsigned head;       /* the next slot */
    bool clear;          /* all cleared at the next frame (a level change) */
    bool drawable;       /* the compute ran this frame: the buffer is readable */
    double death[RP_MAX];/* each slot's end (s, the clock below), for the live count */
    double now;
    /* the decals */
    bool dok;
    VkDescriptorSetLayout ddsl;
    VkPipelineLayout dpl;
    VkPipeline decal_pipe;
    VkDescriptorSet dset;
    Buf dvb, dcb;
    Decal d[RD_MAX];
    int n, dhead, nv;
} F;

/* UWVIEW_PART_DT=MS (the checks only), as render_particles.c's: the particles' clock counts frames */
static double g_part_clock;
static double part_dt_check(void)
{
    static int got;
    static double v;
    if (!got) { const char *e = getenv("UWVIEW_PART_DT"); got = 1; if (e && *e) v = atof(e) / 1000.0; }
    return v;
}
static double vfx_clock_s(void)
{
    if (part_dt_check() > 0) return g_part_clock;
    return (double)rv_ticks() / 1000.0;
}

/* what one pass wrote, readable (and writable) by the next: the compute passes' buffer */
static void vfx_mem_barrier(void)
{
    VkMemoryBarrier2 b = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                          .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT, .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                          .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
    VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &b};
    vkCmdPipelineBarrier2(V.cmd, &di);
}

/* a pipeline drawing into the scene's colour (R16G16B16A16F) with its depth tested LESS_OR_EQUAL and not written, no
   culling, blended as given (the alpha kept: source 0, destination 1), as render_particles.c's and render_decals.c's */
static VkPipeline vfx_pipeline(VkPipelineLayout pl, const unsigned char *vsc, size_t vsn, const unsigned char *psc, size_t psn,
                               const VkPipelineVertexInputStateCreateInfo *vi, VkBlendFactor src, VkBlendFactor dst)
{
    VkShaderModule vs = shader(vsc, vsn), fs = shader(psc, psn);
    if (!vs || !fs) return VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "vs"},
                                             {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "ps"}};
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL,
                                                 .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo dss = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_TRUE,
                                                 .depthWriteEnable = VK_FALSE, .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};
    VkPipelineColorBlendAttachmentState ba = {.blendEnable = VK_TRUE, .srcColorBlendFactor = src, .dstColorBlendFactor = dst,
                                              .colorBlendOp = VK_BLEND_OP_ADD, .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                                              .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .alphaBlendOp = VK_BLEND_OP_ADD, .colorWriteMask = 0xF};
    VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &ba};
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    VkFormat fmt = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkPipelineRenderingCreateInfo ri = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1, .pColorAttachmentFormats = &fmt,
                                        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT};
    VkGraphicsPipelineCreateInfo gi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2, .pStages = st,
                                       .pVertexInputState = vi, .pInputAssemblyState = &ia, .pViewportState = &vp, .pRasterizationState = &rs,
                                       .pMultisampleState = &ms, .pDepthStencilState = &dss, .pColorBlendState = &cbs, .pDynamicState = &ds,
                                       .layout = pl};
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(V.dev, VK_NULL_HANDLE, 1, &gi, NULL, &p) != VK_SUCCESS) p = VK_NULL_HANDLE;
    vkDestroyShaderModule(V.dev, vs, NULL);
    vkDestroyShaderModule(V.dev, fs, NULL);
    return p;
}

static VkPipeline vfx_compute(const unsigned char *code, size_t n, const char *entry)
{
    VkShaderModule m = shader(code, n);
    if (!m) return VK_NULL_HANDLE;
    VkComputePipelineCreateInfo ci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                .module = m, .pName = entry},
                                      .layout = F.ppl};
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(V.dev, VK_NULL_HANDLE, 1, &ci, NULL, &p) != VK_SUCCESS) p = VK_NULL_HANDLE;
    vkDestroyShaderModule(V.dev, m, NULL);
    return p;
}

static bool vfx_pool(void)
{
    if (F.pool) return true;
    VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}};
    VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 3, .poolSizeCount = 2, .pPoolSizes = ps};
    return vkCreateDescriptorPool(V.dev, &dpi, NULL, &F.pool) == VK_SUCCESS;
}

/* render_particles.c's rfx_particles_init */
static bool vfx_particles_start(void)
{
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const VkShaderStageFlags all = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (!vfx_pool()) return logf_("render: particles: no descriptor pool\n"), false;
    /* one layout for both sets: b0; t0 (16) the spawns for the compute passes, the particles for the draw; u0 (48)
       the particles for the compute passes */
    VkDescriptorSetLayoutBinding b[3] = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, all, NULL},
                                         {16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, all, NULL},
                                         {48, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, all, NULL}};
    VkDescriptorSetLayoutCreateInfo li = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 3, .pBindings = b};
    if (vkCreateDescriptorSetLayout(V.dev, &li, NULL, &F.pdsl) != VK_SUCCESS) return logf_("render: particles: no set layout\n"), false;
    VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &F.pdsl};
    if (vkCreatePipelineLayout(V.dev, &pli, NULL, &F.ppl) != VK_SUCCESS) return logf_("render: particles: no pipeline layout\n"), false;
    F.spawn_pipe = vfx_compute(PARTICLES_CS_SPAWN_SPV, sizeof PARTICLES_CS_SPAWN_SPV, "cs_spawn");
    F.update_pipe = vfx_compute(PARTICLES_CS_UPDATE_SPV, sizeof PARTICLES_CS_UPDATE_SPV, "cs_update");
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    /* premultiplied: src + dst (1 - src alpha) */
    F.draw_pipe = vfx_pipeline(F.ppl, PARTICLES_VS_SPV, sizeof PARTICLES_VS_SPV, PARTICLES_PS_SPV, sizeof PARTICLES_PS_SPV, &vi,
                               VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    if (!F.spawn_pipe || !F.update_pipe || !F.draw_pipe) return logf_("render: particles: no pipeline\n"), false;
    if (!make_buf(&F.parts, sizeof(RParticle) * RP_MAX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) ||
        !make_buf(&F.spawn_up, sizeof(RParticle) * RP_SPAWN_MAX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, host, 0) ||
        !make_buf(&F.pcb, 256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host, 0))
        return logf_("render: particles: out of GPU memory\n"), false;
    memset(F.pcb.ptr, 0, 256);
    VkDescriptorSetLayout two[2] = {F.pdsl, F.pdsl};
    VkDescriptorSet sets[2];
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = F.pool, .descriptorSetCount = 2, .pSetLayouts = two};
    if (vkAllocateDescriptorSets(V.dev, &dai, sets) != VK_SUCCESS) return logf_("render: particles: no descriptor sets\n"), false;
    F.cset = sets[0], F.gset = sets[1];
    VkDescriptorBufferInfo cb = {F.pcb.buf, 0, sizeof(RpConst)}, pb = {F.parts.buf, 0, VK_WHOLE_SIZE}, sb = {F.spawn_up.buf, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet w[6] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = F.cset, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &cb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = F.cset, .dstBinding = 16, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &sb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = F.cset, .dstBinding = 48, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &pb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = F.gset, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &cb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = F.gset, .dstBinding = 16, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &pb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = F.gset, .dstBinding = 48, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &pb}};
    vkUpdateDescriptorSets(V.dev, 6, w, 0, NULL);
    F.clear = true;   /* the buffer starts as whatever the memory held: cleared at the first frame */
    F.pok = true;
    return true;
}

void render_particles_spawn(const RParticle *p, int n)
{
    gpu_idle();
    if (!F.pok) return;
    double t = vfx_clock_s();
    RParticle *sp = F.spawn_up.ptr;
    for (int i = 0; i < n && F.nspawn < RP_SPAWN_MAX; i++) {
        sp[F.nspawn++] = p[i];
        F.death[(F.head + (unsigned)F.nspawn - 1) % RP_MAX] = t + p[i].life;
    }
}

void render_particles_clear(void)
{
    gpu_idle();
    if (!F.pok) return;
    F.clear = true, F.nspawn = 0;
    memset(F.death, 0, sizeof F.death);
}

int render_particles_live(void)
{
    double t = vfx_clock_s();
    int n = 0;
    for (int i = 0; i < RP_MAX; i++) n += F.death[i] > t;
    return n;
}

/* the frame's simulation (before the scene is drawn, outside a rendering): the constants, every particle moved, the
   spawns into the ring (render_particles.c's rfx_particles_frame) */
static void vfx_particles_frame(const float vp[16], const float eye[3], float exposure, const float right[3], const float up[3],
                                const float torch[4], const float torchc[3])
{
    F.drawable = false;
    if (!F.pok) return;
    if (part_dt_check() > 0) g_part_clock += part_dt_check();   /* (a check's clock: one step a frame) */
    double t = vfx_clock_s();
    float dt = F.now > 0 ? (float)(t - F.now) : 0;
    F.now = t;
    if (dt > 0.05f) dt = 0.05f;
    RpConst *c = F.pcb.ptr;
    memcpy(c->vp, vp, 64);
    memcpy(c->eye, eye, 12), c->eye[3] = exposure;
    memcpy(c->right, right, 12), memcpy(c->up, up, 12);
    c->sim[0] = dt, c->sim[1] = (float)F.nspawn, c->sim[2] = (float)F.head, c->sim[3] = F.clear ? 1.0f : 0.0f;
    memcpy(c->torch, torch, 16), memcpy(c->torchc, torchc, 12);
    c->misc[0] = (float)RP_MAX;
    vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, F.ppl, 0, 1, &F.cset, 0, NULL);
    /* the update first (the clear kills the old), then the new ones placed (moved from the next frame) */
    vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, F.update_pipe);
    vkCmdDispatch(V.cmd, (RP_MAX + 255) / 256, 1, 1);
    vfx_mem_barrier();
    if (F.nspawn) {
        vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, F.spawn_pipe);
        vkCmdDispatch(V.cmd, (uint32_t)(F.nspawn + 63) / 64, 1, 1);
        vfx_mem_barrier();
    }
    F.head = (F.head + (unsigned)F.nspawn) % RP_MAX;
    F.nspawn = 0, F.clear = false, F.drawable = true;
}

/* render_decals.c's rfx_decals_init */
static bool vfx_decals_start(void)
{
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!vfx_pool()) return logf_("render: decals: no descriptor pool\n"), false;
    VkDescriptorSetLayoutBinding b = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo li = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &b};
    if (vkCreateDescriptorSetLayout(V.dev, &li, NULL, &F.ddsl) != VK_SUCCESS) return logf_("render: decals: no set layout\n"), false;
    VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &F.ddsl};
    if (vkCreatePipelineLayout(V.dev, &pli, NULL, &F.dpl) != VK_SUCCESS) return logf_("render: decals: no pipeline layout\n"), false;
    /* POSITION, TEXCOORD0, COLOR, TEXCOORD1: the vertex shader's inputs in their order (locations 0..3) */
    VkVertexInputBindingDescription vb = {0, sizeof(DVert), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription va[4] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, {1, 0, VK_FORMAT_R32G32_SFLOAT, 12},
                                               {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 20}, {3, 0, VK_FORMAT_R32_SFLOAT, 36}};
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .vertexBindingDescriptionCount = 1,
                                               .pVertexBindingDescriptions = &vb, .vertexAttributeDescriptionCount = 4, .pVertexAttributeDescriptions = va};
    /* the floor times the mark */
    F.decal_pipe = vfx_pipeline(F.dpl, DECAL_VS_SPV, sizeof DECAL_VS_SPV, DECAL_PS_SPV, sizeof DECAL_PS_SPV, &vi,
                                VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ZERO);
    if (!F.decal_pipe) return logf_("render: decals: no pipeline\n"), false;
    if (!make_buf(&F.dvb, sizeof(DVert) * 6 * RD_MAX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, host, 0) ||
        !make_buf(&F.dcb, 256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host, 0))
        return logf_("render: decals: not started\n"), false;
    memset(F.dcb.ptr, 0, 256);
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = F.pool, .descriptorSetCount = 1, .pSetLayouts = &F.ddsl};
    if (vkAllocateDescriptorSets(V.dev, &dai, &F.dset) != VK_SUCCESS) return logf_("render: decals: no descriptor set\n"), false;
    VkDescriptorBufferInfo cb = {F.dcb.buf, 0, 80};
    VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = F.dset, .dstBinding = 0, .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &cb};
    vkUpdateDescriptorSets(V.dev, 1, &w, 0, NULL);
    F.dok = true;
    return true;
}

void render_decals_add(float x, float y, float z, int kind, float size, float yaw, const float rgb[3])
{
    gpu_idle();
    static const float COL[RD_KINDS][3] = {{0.40f, 0.04f, 0.03f}, {0.22f, 0.19f, 0.16f}, {0.75f, 0.88f, 1.0f}, {0.45f, 0.75f, 0.12f}};
    Decal *d = &F.d[F.dhead];
    F.dhead = (F.dhead + 1) % RD_MAX;
    if (F.n < RD_MAX) F.n++;
    d->pos[0] = x, d->pos[1] = y, d->pos[2] = z, d->size = size, d->yaw = yaw;
    memcpy(d->col, rgb ? rgb : COL[kind < 0 || kind >= RD_KINDS ? 0 : kind], 12);
    d->seed = (float)((F.dhead * 2654435761u) % 1000u) / 1000.0f;
    d->born = (double)rv_ticks() / 1000.0;
}

void render_decals_clear(void) { gpu_idle(); F.n = 0, F.dhead = 0; }
int render_decals_count(void) { return F.n; }

/* the frame's marks into their vertex buffer and constants (render_decals.c's rfx_decals_draw, its CPU half; before
   the commands that draw them) */
static void vfx_decals_frame(const float vp[16])
{
    F.nv = 0;
    if (!F.dok || !F.n) return;
    double now = (double)rv_ticks() / 1000.0;
    DVert *out = F.dvb.ptr;
    int nv = 0;
    for (int i = 0; i < F.n; i++) {
        const Decal *d = &F.d[i];
        float a = (float)((now - d->born) / 0.25);   /* it spreads in a quarter of a second */
        a = a < 0 ? 0 : a > 1 ? 1 : a;
        float s = d->size * (0.6f + 0.4f * a), c = cosf(d->yaw) * s, sn = sinf(d->yaw) * s;
        static const float CU[6] = {0, 1, 1, 0, 1, 0}, CV[6] = {0, 0, 1, 0, 1, 1};
        for (int k = 0; k < 6; k++) {
            DVert *v = &out[nv++];
            float u = CU[k] * 2 - 1, w = CV[k] * 2 - 1;
            v->pos[0] = d->pos[0] + u * c - w * sn, v->pos[1] = d->pos[1] + 0.006f, v->pos[2] = d->pos[2] + u * sn + w * c;
            v->uv[0] = CU[k], v->uv[1] = CV[k];
            memcpy(v->col, d->col, 12), v->col[3] = 0.85f * a;
            v->seed = d->seed;
        }
    }
    float *cb = F.dcb.ptr;
    memcpy(cb, vp, 64);
    cb[16] = (float)now;
    F.nv = nv;
}

/* the marks on the floor, then the particles, inside a rendering of the scene's colour and depth with the scene's
   viewport and scissor set (render.c's order: the decals under the particles) */
static void vfx_draw(bool decals, bool parts)
{
    VkDeviceSize zero = 0;
    if (decals && F.dok && F.nv) {
        vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, F.decal_pipe);
        vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, F.dpl, 0, 1, &F.dset, 0, NULL);
        vkCmdBindVertexBuffers(V.cmd, 0, 1, &F.dvb.buf, &zero);
        vkCmdDraw(V.cmd, (uint32_t)F.nv, 1, 0, 0);
    }
    if (parts && F.pok && F.drawable) {
        vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, F.draw_pipe);
        vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, F.ppl, 0, 1, &F.gset, 0, NULL);
        vkCmdDraw(V.cmd, RP_MAX * 6, 1, 0, 0);
        F.drawable = false;
    }
}

static void vfx_stop(void)
{
    if (F.spawn_pipe) vkDestroyPipeline(V.dev, F.spawn_pipe, NULL);
    if (F.update_pipe) vkDestroyPipeline(V.dev, F.update_pipe, NULL);
    if (F.draw_pipe) vkDestroyPipeline(V.dev, F.draw_pipe, NULL);
    if (F.decal_pipe) vkDestroyPipeline(V.dev, F.decal_pipe, NULL);
    if (F.ppl) vkDestroyPipelineLayout(V.dev, F.ppl, NULL);
    if (F.dpl) vkDestroyPipelineLayout(V.dev, F.dpl, NULL);
    if (F.pool) vkDestroyDescriptorPool(V.dev, F.pool, NULL);
    if (F.pdsl) vkDestroyDescriptorSetLayout(V.dev, F.pdsl, NULL);
    if (F.ddsl) vkDestroyDescriptorSetLayout(V.dev, F.ddsl, NULL);
    free_buf(&F.parts), free_buf(&F.spawn_up), free_buf(&F.pcb), free_buf(&F.dvb), free_buf(&F.dcb);
    memset(&F, 0, sizeof F);
}
