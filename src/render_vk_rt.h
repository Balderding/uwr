/* render_vk_rt.h: the Vulkan renderer's ray tracing (milestone M4), render.c's start_rt, build_as, cards_as_make,
 * cards_as_build, scene_tlas and the probes' pass, through VK_KHR_acceleration_structure and VK_KHR_ray_query (DXR
 * 1.1's inline RayQuery is SPIR-V's OpRayQuery*: scene.hlsl -D RT and probes.hlsl as SPIR-V, buildview.cmd).
 * Included by render_vk.c after start_scene and scene_bind (it uses V, make_buf, shader, barrier, the scene's set
 * layout and pipeline maker).
 *
 * As render.c: the level's bottom level (all its triangles, opaque) and a top level of it once, built at
 * render_upload; the pictures' bottom level (not opaque: the shadow rays look at each picture's alpha) and a top level
 * of two instances (mask 1 the level, 2 the cards) rebuilt each frame; the frame's rays use the cards' top level when
 * it exists and the objects' shadows are on, else the level's. The probes: two buffers, written and read alternately
 * (this frame's out, last frame's in), zero at a level's upload. The descriptors that change per frame (the top level,
 * the probes) are written before the frame's commands: the frame before has finished (one frame at a time). */

#define PROBE_LAYERS 6   /* render.c's */
#define PROBES (64 * 64 * PROBE_LAYERS)
#define PROBE_BYTES ((size_t)PROBES * 6 * 16)

#include "probes_cs_spv.h"
#include "scene_ps_rt_spv.h"

typedef struct { VkAccelerationStructureKHR as; Buf buf; VkDeviceAddress addr; } Accel;

static struct {
    bool ok;             /* the pipelines and probes made: ray tracing can be switched on */
    bool off;            /* switched off (render_set_raytracing; the setting raytracing 0) */
    VkPipeline scene_rt, probe;
    VkDescriptorSet pset;          /* the probes' pass: the scene's layout, prev probes at t4, this frame's at u0 */
    Buf probes[2], zero;
    Accel blas, tlas, cblas, ctlas;
    Buf scratch, inst, cscratch, cinst, cpos;
    VkDeviceSize scratch_align;
    bool level_ok;       /* this level's structures are built */
} T;

static VkDeviceAddress buf_addr(const Buf *b)
{
    VkBufferDeviceAddressInfo i = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = b->buf};
    return b->buf ? vkGetBufferDeviceAddress(V.dev, &i) : 0;
}
static VkDeviceAddress aligned(VkDeviceAddress a) { return T.scratch_align > 1 ? (a + T.scratch_align - 1) & ~(T.scratch_align - 1) : a; }

static void accel_free(Accel *a)
{
    if (a->as) vkDestroyAccelerationStructureKHR(V.dev, a->as, NULL);
    free_buf(&a->buf);
    memset(a, 0, sizeof *a);
}

static bool accel_make(Accel *a, VkAccelerationStructureTypeKHR type, VkDeviceSize size)
{
    if (!make_buf(&a->buf, size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
        return false;
    VkAccelerationStructureCreateInfoKHR ci = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR, .buffer = a->buf.buf, .size = size, .type = type};
    if (vkCreateAccelerationStructureKHR(V.dev, &ci, NULL, &a->as) != VK_SUCCESS) return false;
    VkAccelerationStructureDeviceAddressInfoKHR ai = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = a->as};
    a->addr = vkGetAccelerationStructureDeviceAddressKHR(V.dev, &ai);
    return true;
}

/* the instances' layout is D3D12_RAYTRACING_INSTANCE_DESC's (64 bytes) */
static void instance_set(VkAccelerationStructureInstanceKHR *d, uint32_t id, uint32_t mask, VkDeviceAddress blas)
{
    memset(d, 0, sizeof *d);
    d->transform.matrix[0][0] = d->transform.matrix[1][1] = d->transform.matrix[2][2] = 1;
    d->instanceCustomIndex = id, d->mask = mask;
    d->accelerationStructureReference = blas;
}

/* a build's geometry: the level's triangles (indexed, opaque) or the cards' corners (cpos, 12 bytes a vertex) */
static VkAccelerationStructureGeometryKHR geo_tris(VkDeviceAddress verts, VkDeviceSize stride, uint32_t maxv, VkDeviceAddress index, bool opaque)
{
    VkAccelerationStructureGeometryKHR g = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                                            .flags = opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0};
    g.geometry.triangles = (VkAccelerationStructureGeometryTrianglesDataKHR){
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR, .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = verts}, .vertexStride = stride, .maxVertex = maxv,
        .indexType = index ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_NONE_KHR, .indexData = {.deviceAddress = index}};
    return g;
}
static VkAccelerationStructureGeometryKHR geo_inst(VkDeviceAddress inst)
{
    VkAccelerationStructureGeometryKHR g = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR};
    g.geometry.instances = (VkAccelerationStructureGeometryInstancesDataKHR){VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                                                                            .data = {.deviceAddress = inst}};
    return g;
}
static VkAccelerationStructureBuildSizesInfoKHR sizes_of(VkAccelerationStructureTypeKHR type, bool fast_build,
                                                         const VkAccelerationStructureGeometryKHR *g, uint32_t prims)
{
    VkAccelerationStructureBuildGeometryInfoKHR bi = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, .type = type,
                                                      .flags = fast_build ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                                                                          : VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                                                      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, .geometryCount = 1, .pGeometries = g};
    VkAccelerationStructureBuildSizesInfoKHR s = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(V.dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, &prims, &s);
    return s;
}
static void build(const Accel *dst, VkAccelerationStructureTypeKHR type, bool fast_build, const VkAccelerationStructureGeometryKHR *g,
                  uint32_t prims, VkDeviceAddress scratch)
{
    VkAccelerationStructureBuildGeometryInfoKHR bi = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR, .type = type,
                                                      .flags = fast_build ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR
                                                                          : VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                                                      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR, .dstAccelerationStructure = dst->as,
                                                      .geometryCount = 1, .pGeometries = g, .scratchData = {.deviceAddress = scratch}};
    VkAccelerationStructureBuildRangeInfoKHR r = {.primitiveCount = prims};
    const VkAccelerationStructureBuildRangeInfoKHR *rp = &r;
    vkCmdBuildAccelerationStructuresKHR(V.cmd, 1, &bi, &rp);
    vfx_mem_barrier();   /* (the build's writes before the next build or a ray's reads; the scratch reused) */
}

/* the pictures' structures (render.c's cards_as_make): sized for the most cards, a top level of two instances */
static bool cards_as_make(void)
{
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const VkBufferUsageFlags input = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (!T.cpos.buf && !make_buf(&T.cpos, 12 * (size_t)RENDER_MAX_SPRITE_VERTS, input, host, 0)) goto fail;
    VkAccelerationStructureGeometryKHR g = geo_tris(buf_addr(&T.cpos), 12, RENDER_MAX_SPRITE_VERTS - 1, 0, false);
    VkAccelerationStructureBuildSizesInfoKHR bs = sizes_of(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, true, &g, RENDER_MAX_SPRITE_VERTS / 3);
    if (!T.cinst.buf && !make_buf(&T.cinst, sizeof(VkAccelerationStructureInstanceKHR) * 2, input, host, 0)) goto fail;
    VkAccelerationStructureGeometryKHR gi = geo_inst(buf_addr(&T.cinst));
    VkAccelerationStructureBuildSizesInfoKHR ts = sizes_of(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, true, &gi, 2);
    VkDeviceSize sc = MAX(bs.buildScratchSize, ts.buildScratchSize) + T.scratch_align;
    if (!T.cblas.as && !accel_make(&T.cblas, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, bs.accelerationStructureSize)) goto fail;
    if (!T.ctlas.as && !accel_make(&T.ctlas, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, ts.accelerationStructureSize)) goto fail;
    if (!T.cscratch.buf && !make_buf(&T.cscratch, sc, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
        goto fail;
    VkAccelerationStructureInstanceKHR *d = T.cinst.ptr;
    instance_set(&d[0], 0, 0x01, T.blas.addr);
    instance_set(&d[1], 1, 0x02, T.cblas.addr);
    return true;
fail:
    logf_("render: the pictures' shadows: out of GPU memory (the level's alone)\n");
    accel_free(&T.cblas), accel_free(&T.ctlas), free_buf(&T.cscratch), free_buf(&T.cinst);
    return false;
}

/* each frame (ray tracing): the cards' bottom level from this frame's cards, the top level with the level and them
   (render.c's cards_as_build; the corners as its loop has them) */
static void cards_as_build(const float eye[3])
{
    if (!T.ctlas.as || !T.cinst.ptr || !T.cpos.ptr || !sprite_cpu) return;
    int n = V.nsprites / 3 * 3;
    const Vertex *sp = sprite_cpu;   /* (2026-11-18: not V.sprite_vb.ptr, the GPU's memory: render_vk.c's sprite_cpu) */
    /* the corners: a card (kind 2.5-4.5) stands upright at its bottom middle facing the eye across the floor, its
       corner offset in nrm (right, up) as scene.hlsl's vs turns it; anything else (a door, its frame) is where it is */
    for (int i = 0; i < n; i++) {
        const Vertex *v = &sp[i];
        float *p = (float *)T.cpos.ptr + (size_t)i * 3;
        if (v->color[3] > 2.5f && v->color[3] < 4.5f) {
            float dx = eye[0] - v->pos[0] + 1e-4f, dz = eye[2] - v->pos[2], l = sqrtf(dx * dx + dz * dz);
            if (l < 1e-6f) l = 1e-6f;
            dx /= l, dz /= l;
            p[0] = v->pos[0] - dz * v->nrm[0], p[1] = v->pos[1] + v->nrm[1], p[2] = v->pos[2] + dx * v->nrm[0];
        } else
            p[0] = v->pos[0], p[1] = v->pos[1], p[2] = v->pos[2];
    }
    VkDeviceAddress sc = aligned(buf_addr(&T.cscratch));
    if (n >= 3) {
        VkAccelerationStructureGeometryKHR g = geo_tris(buf_addr(&T.cpos), 12, (uint32_t)n - 1, 0, false);
        build(&T.cblas, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, true, &g, (uint32_t)n / 3, sc);
    }
    VkAccelerationStructureGeometryKHR gi = geo_inst(buf_addr(&T.cinst));
    build(&T.ctlas, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, true, &gi, n >= 3 ? 2 : 1, sc);
}

/* the top level the frame's rays use (render.c's scene_tlas) */
static VkAccelerationStructureKHR scene_tlas(void)
{
    static int off = -1;   /* UWVIEW_CARDSHADOW=0 (a check's): the level's alone */
    if (off < 0) { const char *e = getenv("UWVIEW_CARDSHADOW"); off = e && *e == '0'; }
    return T.ctlas.as && T.cinst.ptr && !off && g_card_shadows ? T.ctlas.as : T.tlas.as;
}

/* the level's structures (render.c's build_as), recorded into the upload's command buffer; scratch and instance
   buffers kept in T until the next level (the upload waits for them) */
static bool build_as(int vertices)
{
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkAccelerationStructureGeometryKHR g = geo_tris(buf_addr(&V.vbuf), sizeof(Vertex), (uint32_t)MAX(vertices, 1) - 1, buf_addr(&V.ibuf), true);
    uint32_t tris = (uint32_t)V.indices / 3;
    VkAccelerationStructureBuildSizesInfoKHR bs = sizes_of(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, false, &g, tris);
    if (!make_buf(&T.inst, sizeof(VkAccelerationStructureInstanceKHR),
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, host, 0))
        return logf_("render: out of GPU memory (ray tracing)\n"), false;
    VkAccelerationStructureGeometryKHR gi = geo_inst(buf_addr(&T.inst));
    VkAccelerationStructureBuildSizesInfoKHR ts = sizes_of(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, false, &gi, 1);
    if (!accel_make(&T.blas, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, bs.accelerationStructureSize) ||
        !accel_make(&T.tlas, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, ts.accelerationStructureSize) ||
        !make_buf(&T.scratch, MAX(bs.buildScratchSize, ts.buildScratchSize) + T.scratch_align,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
        return logf_("render: out of GPU memory (ray tracing)\n"), false;
    instance_set(T.inst.ptr, 0, 0xFF, T.blas.addr);
    VkDeviceAddress sc = aligned(buf_addr(&T.scratch));
    build(&T.blas, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, false, &g, tris, sc);
    build(&T.tlas, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, false, &gi, 1, sc);
    cards_as_make();
    /* the probes start dark */
    for (int i = 0; i < 2; i++) vkCmdFillBuffer(V.cmd, T.probes[i].buf, 0, VK_WHOLE_SIZE, 0);
    vfx_mem_barrier();
    return true;
}

static void level_as_free(void)
{
    accel_free(&T.blas), accel_free(&T.tlas), free_buf(&T.scratch), free_buf(&T.inst);
    /* the cards' structures point at the level's bottom level: made again with the next */
    accel_free(&T.cblas), accel_free(&T.ctlas), free_buf(&T.cscratch), free_buf(&T.cinst);
    T.level_ok = false;
}

/* the frame's descriptors that change: the top level (both sets), this frame's probes and last frame's */
static void rt_bind_frame(int wp)
{
    VkWriteDescriptorSetAccelerationStructureKHR wa = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                                                       .accelerationStructureCount = 1};
    VkAccelerationStructureKHR tl = scene_tlas();
    wa.pAccelerationStructures = &tl;
    VkDescriptorBufferInfo cur = {T.probes[wp].buf, 0, VK_WHOLE_SIZE}, prev = {T.probes[wp ^ 1].buf, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet w[5] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &wa, .dstSet = V.sset, .dstBinding = 19, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &wa, .dstSet = T.pset, .dstBinding = 19, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = V.sset, .dstBinding = 20, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &cur},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = T.pset, .dstBinding = 20, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &prev},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = T.pset, .dstBinding = 48, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &cur}};
    vkUpdateDescriptorSets(V.dev, 5, w, 0, NULL);
}

/* the level's buffers in both sets (with scene_bind, at an upload): verts t5, the vertex numbers t8, the cards t10 */
static void rt_bind_level(void)
{
    if (!T.ok) return;
    VkDescriptorBufferInfo vb = {V.vbuf.buf, 0, VK_WHOLE_SIZE}, ib = {V.ibuf.buf, 0, VK_WHOLE_SIZE}, sb = {V.sprite_vb.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorSet sets[2] = {V.sset, T.pset};
    VkWriteDescriptorSet w[6];
    for (int s = 0; s < 2; s++) {
        w[s * 3 + 0] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = sets[s], .dstBinding = 21, .descriptorCount = 1,
                                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &vb};
        w[s * 3 + 1] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = sets[s], .dstBinding = 24, .descriptorCount = 1,
                                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &ib};
        w[s * 3 + 2] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = sets[s], .dstBinding = 26, .descriptorCount = 1,
                                              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &sb};
    }
    vkUpdateDescriptorSets(V.dev, 6, w, 0, NULL);
}

/* the probes' pass (render.c's, before the scene): last frame's probes in, this frame's out */
static void probes_pass(void)
{
    vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, T.probe);
    vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, V.spl, 0, 1, &T.pset, 0, NULL);
    vkCmdDispatch(V.cmd, (PROBES + 63) / 64, 1, 1);
    vfx_mem_barrier();
}

static void rt_stop(void)
{
    level_as_free();
    free_buf(&T.cpos), free_buf(&T.probes[0]), free_buf(&T.probes[1]);
    if (T.scene_rt) vkDestroyPipeline(V.dev, T.scene_rt, NULL);
    if (T.probe) vkDestroyPipeline(V.dev, T.probe, NULL);
    memset(&T, 0, sizeof T);
}
