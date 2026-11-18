/* render_vk_vol.h: the fog and smoke of the Vulkan renderer (the user, 2026-11-17: "physical simulation of smoke and
 * water haze fog ... displaced by you when you move through it"; render.h RenderVolume, shaders\volume.hlsl has the
 * method). A box of air cells round the eye as 3D pictures on the GPU: the air's velocity and heat (U), the fog and
 * the smoke in it (D), the pressure (P), each cell's light (LA), what it sends the eye and blocks (RW) and the way to
 * its light (GW). Each frame: the step (cs_advect, cs_div, the pressure's passes, cs_project) when the simulation's
 * rate says so, the light (cs_light: with ray queries while ray tracing runs, through the tile grid otherwise), and
 * after the scene is drawn one triangle over it that steps each pixel's view through the box (vs / ps) to the scene's
 * depth. Included by render_vk.c after the ray tracing (it uses V, T, shader, make_buf, vfx_mem_barrier, the scene's
 * set layout: set 0 of these passes is the scene's own, set 1 these pictures).
 *
 * The cells stay at their place in the world modulo the box's size, so nothing is copied as the box follows the eye;
 * the pictures are made again when the Fineness or the Reach rows change (the air then starts still and clear). */
#include "volume_cs_advect_spv.h"
#include "volume_cs_curl_spv.h"
#include "volume_cs_mac_spv.h"
#include "volume_cs_div_spv.h"
#include "volume_cs_jac_ab_spv.h"
#include "volume_cs_jac_ba_spv.h"
#include "volume_cs_light_grid_spv.h"
#include "volume_cs_light_rt_spv.h"
#include "volume_cs_project_spv.h"
#include "volume_cs_march_spv.h"
#include "volume_ps_spv.h"
#include "volume_vs_spv.h"

typedef struct {
    float org[4], dims[4], prev[4], tm[4], cam[4], camr[4], camu[4], camf[4], misc[4];
    float par[RVP_N][4];
} VolConst;

enum { VI_UA, VI_UB, VI_UC, VI_DA, VI_DB, VI_DC, VI_PA, VI_PB, VI_LA, VI_RW, VI_GW, VI_WV, VI_N };
static struct {
    bool ok;
    VkDescriptorPool pool;
    VkDescriptorSetLayout dsl;
    VkPipelineLayout pl;
    VkDescriptorSet set;
    VkPipeline curl, advect, mac, div, jab, jba, project, light_rt, light_grid, draw, march;
    Img low;                      /* the picture at half size (cs_march), and its size */
    int low_w, low_h;
    VkSampler samp;
    Img img[VI_N];
    Buf cb, emit;
    int cells, box, nx, ny, nz;   /* the pictures as made */
    bool fresh;                   /* just made: their layouts set at the next frame */
    int fail_cells, fail_box;     /* the size that could not be made (out of GPU memory): not tried again */
    bool have_prev;               /* the air of the frame before is there */
    int org[3];
    unsigned long long last_ms;
    float acc;                    /* time waiting for the next step (s) */
    unsigned steps;
} VL;
static RenderVolume g_vol;

void render_volume(const RenderVolume *v)
{
    if (v) g_vol = *v;
    else g_vol.on = 0;
}
bool render_volume_available(void) { return VL.ok && !VL.fail_cells; }

static bool vvol_img(Img *m, int w, int h, int d, VkFormat fmt)
{
    m->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_3D, .format = fmt,
                            .extent = {(uint32_t)w, (uint32_t)h, (uint32_t)d}, .mipLevels = 1, .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
                            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    if (vkCreateImage(V.dev, &ii, NULL, &m->img) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(V.dev, m->img, &mr);
    uint32_t t = mem_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (t == UINT32_MAX) return false;
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = t};
    if (vkAllocateMemory(V.dev, &ai, NULL, &m->mem) != VK_SUCCESS) return false;
    vkBindImageMemory(V.dev, m->img, m->mem, 0);
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = m->img, .viewType = VK_IMAGE_VIEW_TYPE_3D, .format = fmt,
                                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    return vkCreateImageView(V.dev, &vi, NULL, &m->view) == VK_SUCCESS;
}

static VkPipeline vvol_compute(const unsigned char *code, size_t n, const char *entry)
{
    VkShaderModule m = shader(code, n);
    if (!m) return VK_NULL_HANDLE;
    VkComputePipelineCreateInfo ci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                .module = m, .pName = entry},
                                      .layout = VL.pl};
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(V.dev, VK_NULL_HANDLE, 1, &ci, NULL, &p) != VK_SUCCESS) p = VK_NULL_HANDLE;
    vkDestroyShaderModule(V.dev, m, NULL);
    return p;
}

/* the triangle over the scene's colour: src + dst x (1 - src alpha), no depth (the shader reads the scene's) */
static VkPipeline vvol_draw_pipe(void)
{
    VkShaderModule vs = shader(VOLUME_VS_SPV, sizeof VOLUME_VS_SPV), fs = shader(VOLUME_PS_SPV, sizeof VOLUME_PS_SPV);
    if (!vs || !fs) return VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "vs"},
                                             {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "ps"}};
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL,
                                                 .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo dss = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState ba = {.blendEnable = VK_TRUE, .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                              .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, .colorBlendOp = VK_BLEND_OP_ADD,
                                              .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                              .alphaBlendOp = VK_BLEND_OP_ADD, .colorWriteMask = 0xF};
    VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &ba};
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    VkFormat fmt = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkPipelineRenderingCreateInfo ri = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1, .pColorAttachmentFormats = &fmt};
    VkGraphicsPipelineCreateInfo gi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2, .pStages = st,
                                       .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vp, .pRasterizationState = &rs,
                                       .pMultisampleState = &ms, .pDepthStencilState = &dss, .pColorBlendState = &cbs, .pDynamicState = &ds,
                                       .layout = VL.pl};
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(V.dev, VK_NULL_HANDLE, 1, &gi, NULL, &p) != VK_SUCCESS) p = VK_NULL_HANDLE;
    vkDestroyShaderModule(V.dev, vs, NULL);
    vkDestroyShaderModule(V.dev, fs, NULL);
    return p;
}

/* set 1's bindings: 0 the constants, 1 the emitters, 2 the sampler, 3-9 pictures read, 10-21 pictures written */
static bool vvol_start(void)
{
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const VkShaderStageFlags all = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding b[24];
    int nb = 0;
    b[nb++] = (VkDescriptorSetLayoutBinding){0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, all, NULL};
    b[nb++] = (VkDescriptorSetLayoutBinding){1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, all, NULL};
    b[nb++] = (VkDescriptorSetLayoutBinding){2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, all, NULL};
    for (uint32_t i = 3; i <= 23; i++)   /* (22: the half-size picture written, 23 read) */
        b[nb++] = (VkDescriptorSetLayoutBinding){i, i >= 10 && i <= 22 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, all, NULL};
    VkDescriptorSetLayoutCreateInfo li = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = (uint32_t)nb, .pBindings = b};
    if (vkCreateDescriptorSetLayout(V.dev, &li, NULL, &VL.dsl) != VK_SUCCESS) return logf_("render: fog and smoke: no set layout\n"), false;
    VkDescriptorSetLayout two[2] = {V.sdsl, VL.dsl};
    VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 2, .pSetLayouts = two};
    if (vkCreatePipelineLayout(V.dev, &pli, NULL, &VL.pl) != VK_SUCCESS) return logf_("render: fog and smoke: no pipeline layout\n"), false;
    VkDescriptorPoolSize ps[5] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
                                  {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 13}};
    VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 5, .pPoolSizes = ps};
    if (vkCreateDescriptorPool(V.dev, &dpi, NULL, &VL.pool) != VK_SUCCESS) return logf_("render: fog and smoke: no descriptor pool\n"), false;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = VL.pool, .descriptorSetCount = 1, .pSetLayouts = &VL.dsl};
    if (vkAllocateDescriptorSets(V.dev, &dai, &VL.set) != VK_SUCCESS) return logf_("render: fog and smoke: no descriptor set\n"), false;
    VkSamplerCreateInfo si = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST, .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT, .maxLod = 0};
    if (vkCreateSampler(V.dev, &si, NULL, &VL.samp) != VK_SUCCESS) return logf_("render: fog and smoke: no sampler\n"), false;
    VL.advect = vvol_compute(VOLUME_CS_ADVECT_SPV, sizeof VOLUME_CS_ADVECT_SPV, "cs_advect");
    VL.curl = vvol_compute(VOLUME_CS_CURL_SPV, sizeof VOLUME_CS_CURL_SPV, "cs_curl");
    VL.mac = vvol_compute(VOLUME_CS_MAC_SPV, sizeof VOLUME_CS_MAC_SPV, "cs_mac");
    VL.div = vvol_compute(VOLUME_CS_DIV_SPV, sizeof VOLUME_CS_DIV_SPV, "cs_div");
    VL.jab = vvol_compute(VOLUME_CS_JAC_AB_SPV, sizeof VOLUME_CS_JAC_AB_SPV, "cs_jac_ab");
    VL.jba = vvol_compute(VOLUME_CS_JAC_BA_SPV, sizeof VOLUME_CS_JAC_BA_SPV, "cs_jac_ba");
    VL.project = vvol_compute(VOLUME_CS_PROJECT_SPV, sizeof VOLUME_CS_PROJECT_SPV, "cs_project");
    VL.light_grid = vvol_compute(VOLUME_CS_LIGHT_GRID_SPV, sizeof VOLUME_CS_LIGHT_GRID_SPV, "cs_light");
    if (g_rt_dev && T.ok) VL.light_rt = vvol_compute(VOLUME_CS_LIGHT_RT_SPV, sizeof VOLUME_CS_LIGHT_RT_SPV, "cs_light");
    VL.draw = vvol_draw_pipe();
    VL.march = vvol_compute(VOLUME_CS_MARCH_SPV, sizeof VOLUME_CS_MARCH_SPV, "cs_march");
    if (!VL.march || !VL.curl || !VL.mac || !VL.advect || !VL.div || !VL.jab || !VL.jba || !VL.project || !VL.light_grid || !VL.draw)
        return logf_("render: fog and smoke: no pipeline\n"), false;
    if (!make_buf(&VL.cb, 1024, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host, 0) ||
        !make_buf(&VL.emit, sizeof(RvEmit) * RV_EMIT_MAX, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, host, 0))
        return logf_("render: fog and smoke: out of GPU memory\n"), false;
    memset(VL.cb.ptr, 0, 1024), memset(VL.emit.ptr, 0, sizeof(RvEmit) * RV_EMIT_MAX);
    VkDescriptorBufferInfo cb = {VL.cb.buf, 0, sizeof(VolConst)}, eb = {VL.emit.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorImageInfo sm = {VL.samp, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
    VkDescriptorImageInfo zb = {VK_NULL_HANDLE, V.depth.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w[4] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = VL.set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &cb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = VL.set, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &eb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = VL.set, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .pImageInfo = &sm},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = VL.set, .dstBinding = 9, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &zb}};
    vkUpdateDescriptorSets(V.dev, 4, w, 0, NULL);
    VL.ok = true;
    return true;
}

/* the pictures for this fineness and reach (the GPU idle: between frames) */
static bool vvol_size(int cells, int box)
{
    /* binding -> picture: 3 UA, 4 DA, 5 UB, 6 DB, 7 RW, 8 GW read (9: the scene's depth); 10 UA, 11 DA, 12 UB, 13 DB, 14 UC,
       15 DC, 16 PA, 17 PB, 18 LA, 19 RW, 20 GW, 21 WV written */
    static const int BIND[18] = {3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
    static const int AT[18] = {VI_UA, VI_DA, VI_UB, VI_DB, VI_RW, VI_GW, VI_UA, VI_DA, VI_UB, VI_DB, VI_UC, VI_DC, VI_PA, VI_PB,
                               VI_LA, VI_RW, VI_GW, VI_WV};
    cells = cells < 2 ? 2 : cells > 8 ? 8 : cells, box = box < 8 ? 8 : box > 32 ? 32 : box;
    if (VL.img[0].img && cells == VL.cells && box == VL.box) return true;
    if (cells == VL.fail_cells && box == VL.fail_box) return false;
    VL.fail_cells = VL.fail_box = 0;
    vkDeviceWaitIdle(V.dev);
    for (int i = 0; i < VI_N; i++) free_img(&VL.img[i]);
    VL.cells = cells, VL.box = box, VL.nx = VL.nz = cells * box, VL.ny = cells * 4, VL.have_prev = false, VL.fresh = true;
    for (int i = 0; i < VI_N; i++)
        if (!vvol_img(&VL.img[i], VL.nx, VL.ny, VL.nz, i == VI_PA || i == VI_PB ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R16G16B16A16_SFLOAT)) {
            logf_("render: fog and smoke: out of GPU memory (%d x %d x %d cells)\n", VL.nx, VL.ny, VL.nz);
            for (int k = 0; k < VI_N; k++) free_img(&VL.img[k]);
            VL.fail_cells = cells, VL.fail_box = box;
            return false;
        }
    VkDescriptorImageInfo im[18];
    VkWriteDescriptorSet w[18];
    for (int i = 0; i < 18; i++) {
        uint32_t bind = (uint32_t)BIND[i];
        im[i] = (VkDescriptorImageInfo){VK_NULL_HANDLE, VL.img[AT[i]].view, VK_IMAGE_LAYOUT_GENERAL};
        w[i] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = VL.set, .dstBinding = bind, .descriptorCount = 1,
                                      .descriptorType = bind >= 10 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                      .pImageInfo = &im[i]};
    }
    vkUpdateDescriptorSets(V.dev, 18, w, 0, NULL);
    logf_("render: fog and smoke: %d x %d x %d cells of air (%d to a tile, %d tiles wide), %d MB\n", VL.nx, VL.ny, VL.nz, cells, box,
          (int)((size_t)VL.nx * VL.ny * VL.nz * 112 >> 20));
    return true;
}

/* the half-size picture for a scene of w x h pixels (made again when the size changes; the GPU idle: between
   frames' work, before this frame's sets are bound) */
static bool vvol_low(int w, int h)
{
    int lw = (w + 1) / 2, lh = (h + 1) / 2;
    if (VL.low.img && lw == VL.low_w && lh == VL.low_h) return true;
    vkDeviceWaitIdle(V.dev);
    free_img(&VL.low);
    VL.low.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                            .extent = {(uint32_t)lw, (uint32_t)lh, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    if (vkCreateImage(V.dev, &ii, NULL, &VL.low.img) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(V.dev, VL.low.img, &mr);
    uint32_t t = mem_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = t};
    if (t == UINT32_MAX || vkAllocateMemory(V.dev, &ai, NULL, &VL.low.mem) != VK_SUCCESS) return free_img(&VL.low), false;
    vkBindImageMemory(V.dev, VL.low.img, VL.low.mem, 0);
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = VL.low.img, .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                .format = VK_FORMAT_R16G16B16A16_SFLOAT, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    if (vkCreateImageView(V.dev, &vi, NULL, &VL.low.view) != VK_SUCCESS) return free_img(&VL.low), false;
    VkDescriptorImageInfo im = {VK_NULL_HANDLE, VL.low.view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet wr[2] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = VL.set, .dstBinding = 22, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &im},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = VL.set, .dstBinding = 23, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &im}};
    vkUpdateDescriptorSets(V.dev, 2, wr, 0, NULL);
    barrier(VL.low.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    VL.low_w = lw, VL.low_h = lh;
    return true;
}

static void vvol_pass(VkPipeline p, uint32_t groups)
{
    vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p);
    vkCmdDispatch(V.cmd, groups, 1, 1);
    vfx_mem_barrier();
}

/* the frame's step and light, in the frame's commands before the scene is drawn (outside a rendering; after the
   pictures' structures are built: the light's rays see them). view, proj: view_proj's, unjittered; jx, jy: the
   projection's jitter as given to it. false: nothing to draw */
static bool vvol_frame(const Camera *cam, const float view[16], const float proj[16], float jx, float jy, bool rt_live, bool hold, int rw, int rh)
{
    if (!VL.ok || !g_vol.on || !V.level) { VL.have_prev = false, VL.last_ms = 0; return false; }
    if (!vvol_size(g_vol.cells, g_vol.box)) return false;
    if (!vvol_low(rw, rh)) return false;   /* (made even while the half size is off: the picture's pass reads its binding) */
    unsigned long long now = rv_ticks();
    float el = VL.last_ms ? (float)(now - VL.last_ms) / 1000.0f : 0;
    VL.last_ms = now;
    if (el > 0.25f) el = 0.25f;
    if (hold) el = 0;   /* (a check's comparison: the air held as the flames' time is) */
    VL.acc += el;
    float want = g_vol.rate > 0 ? 1.0f / g_vol.rate : 0;
    bool step = !VL.have_prev || (VL.acc > 0 && VL.acc >= want - 0.0005f);
    float dt = VL.acc > 0.05f ? 0.05f : VL.acc;
    VolConst *c = VL.cb.ptr;
    const float h = 1.0f / (float)VL.cells;
    int org[3] = {(int)floorf(cam->pos[0] * (float)VL.cells) - VL.nx / 2, 0, (int)floorf(cam->pos[2] * (float)VL.cells) - VL.nz / 2};
    if (!step) memcpy(org, VL.org, sizeof org);   /* (the box moves with the step) */
    c->org[0] = (float)org[0], c->org[1] = 0, c->org[2] = (float)org[2], c->org[3] = (float)VL.cells;
    c->dims[0] = (float)VL.nx, c->dims[1] = (float)VL.ny, c->dims[2] = (float)VL.nz, c->dims[3] = h;
    c->prev[0] = (float)VL.org[0], c->prev[1] = 0, c->prev[2] = (float)VL.org[2], c->prev[3] = VL.have_prev ? 1.0f : 0.0f;
    c->tm[0] = step ? dt : 0, c->tm[1] = (float)(now % 100000000ull) / 1000.0f, c->tm[2] = (float)(V.frame & 0xFFFF);
    c->tm[3] = (float)(g_vol.light_share > 0 ? g_vol.light_share : 1);
    memcpy(c->cam, cam->pos, 12), c->cam[3] = 1.0f / proj[0];
    memcpy(c->camr, &view[0], 12), c->camr[3] = 1.0f / proj[5];
    memcpy(c->camu, &view[4], 12), c->camu[3] = V.cfg.exposure;
    memcpy(c->camf, &view[8], 12), c->camf[3] = (float)g_vol.view_steps;
    int ne = g_vol.nemit < 0 ? 0 : g_vol.nemit > RV_EMIT_MAX ? RV_EMIT_MAX : g_vol.nemit;
    c->misc[0] = step ? (float)ne : 0, c->misc[1] = proj[2] + jx, c->misc[2] = proj[6] + jy, c->misc[3] = g_vol.half ? 1.0f : 0.0f;
    memcpy(c->par, g_vol.par, sizeof c->par);
    c->par[RVP_SCREEN][0] = (float)rw, c->par[RVP_SCREEN][1] = (float)rh, c->par[RVP_SCREEN][2] = (float)VL.low_w, c->par[RVP_SCREEN][3] = (float)VL.low_h;
    memcpy(VL.emit.ptr, g_vol.emit, sizeof(RvEmit) * (size_t)ne);
    if (VL.fresh) {
        for (int i = 0; i < VI_N; i++) barrier(VL.img[i].img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        VL.fresh = false;
    }
    VkDescriptorSet sets[2] = {V.sset, VL.set};
    vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, VL.pl, 0, 2, sets, 0, NULL);
    const uint32_t groups = (uint32_t)(((size_t)VL.nx * VL.ny * VL.nz + 63) / 64);
    if (step) {
        vvol_pass(VL.curl, groups);
        vvol_pass(VL.advect, groups);
        vvol_pass(VL.mac, groups);
        vvol_pass(VL.div, groups);
        int np = g_vol.pressure < 2 ? 2 : g_vol.pressure > 80 ? 80 : g_vol.pressure;
        for (int i = 0; i < np / 2; i++) vvol_pass(VL.jab, groups), vvol_pass(VL.jba, groups);
        vvol_pass(VL.project, groups);
        VL.acc = 0, VL.steps++;
        memcpy(VL.org, org, sizeof org), VL.have_prev = true;
    }
    vvol_pass(rt_live && VL.light_rt ? VL.light_rt : VL.light_grid, groups);
    return true;
}

/* the fog and smoke over the scene's colour (the scene's pictures readable again: after its rendering's last barriers) */
static void vvol_draw(int rw, int rh)
{
    if (g_vol.half) {   /* the half-size picture first (the scene's depth readable: after its rendering) */
        VkDescriptorSet sets[2] = {V.sset, VL.set};
        vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, VL.march);
        vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, VL.pl, 0, 2, sets, 0, NULL);
        vkCmdDispatch(V.cmd, (uint32_t)(VL.low_w + 7) / 8, (uint32_t)(VL.low_h + 7) / 8, 1);
        vfx_mem_barrier();
    }
    barrier(V.scene.img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo fa = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = V.scene.view,
                                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
    VkRenderingInfo fri = {VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, {(uint32_t)V.cfg.w, (uint32_t)V.cfg.h}}, .layerCount = 1,
                           .colorAttachmentCount = 1, .pColorAttachments = &fa};
    VkViewport fvp = {0, (float)rh, (float)rw, -(float)rh, 0, 1};
    VkRect2D fsc = {{0, 0}, {(uint32_t)rw, (uint32_t)rh}};
    vkCmdBeginRendering(V.cmd, &fri);
    vkCmdSetViewport(V.cmd, 0, 1, &fvp);
    vkCmdSetScissor(V.cmd, 0, 1, &fsc);
    vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, VL.draw);
    vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, VL.pl, 1, 1, &VL.set, 0, NULL);
    vkCmdDraw(V.cmd, 3, 1, 0, 0);
    vkCmdEndRendering(V.cmd);
    barrier(V.scene.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

static void vvol_stop(void)
{
    VkPipeline *p[11] = {&VL.curl, &VL.mac, &VL.advect, &VL.div, &VL.jab, &VL.jba, &VL.project, &VL.light_rt, &VL.light_grid, &VL.draw, &VL.march};
    for (int i = 0; i < 11; i++) if (*p[i]) vkDestroyPipeline(V.dev, *p[i], NULL);
    for (int i = 0; i < VI_N; i++) free_img(&VL.img[i]);
    free_img(&VL.low);
    if (VL.samp) vkDestroySampler(V.dev, VL.samp, NULL);
    if (VL.pl) vkDestroyPipelineLayout(V.dev, VL.pl, NULL);
    if (VL.pool) vkDestroyDescriptorPool(V.dev, VL.pool, NULL);
    if (VL.dsl) vkDestroyDescriptorSetLayout(V.dev, VL.dsl, NULL);
    free_buf(&VL.cb), free_buf(&VL.emit);
    memset(&VL, 0, sizeof VL);
}
