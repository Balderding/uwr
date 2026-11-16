/* uwsteam.c: Steam Audio's reflections for our acoustics (uwsteam.h; the acoustics plan's Task 7, "Acoustics: High").
   phonon.dll (Steam Audio 4.8.1, Valve, Apache 2.0) is never linked: loaded from the folder given, its functions
   taken by GetProcAddress; phonon.h (port\win32, the SDK's own) gives the types only.

   What runs where:
   - uwst_simulate (a worker, or the checks): the level's triangles made again when the level or a door changed, the
     voices' places given to the simulator, one run of the reflection rays. Everything that changes Steam Audio's
     scene or sources happens here, so nothing changes under a running simulation.
   - uwst_mix (the audio's side): a block of each voice's samples through its convolution with the response last
     simulated for it, all into one mixer, the mixer's first-order ambisonics decoded for the listener's facing.
   - the rest (uwst_level, uwst_listener, uwst_begin, uwst_end) leave word under one lock.

   Frames: ours is the renderer's (tiles; x east, y up, z north); Steam Audio's is metres, right-handed, y up, -z
   ahead: (x, y, -z) * tile_m. */
#define WIN32_LEAN_AND_MEAN
#include "uwos.h"   /* windows.h on Windows */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "phonon.h"
#include "uwsteam.h"

#define N 64
#define STEP 0.25f
#define CEIL 4.0f
#define IR_SECONDS 2.0f
#define SCATTER 0.25f        /* our surfaces are rough-hewn: a quarter of a bounce goes every way (a guess, to be heard) */

#define FUNCS(X) \
    X(iplContextCreate) X(iplContextRelease) X(iplSceneCreate) X(iplSceneRelease) X(iplSceneCommit) \
    X(iplStaticMeshCreate) X(iplStaticMeshRelease) X(iplStaticMeshAdd) X(iplStaticMeshRemove) \
    X(iplAudioBufferAllocate) X(iplAudioBufferFree) X(iplHRTFCreate) X(iplHRTFRelease) \
    X(iplAmbisonicsDecodeEffectCreate) X(iplAmbisonicsDecodeEffectRelease) X(iplAmbisonicsDecodeEffectApply) \
    X(iplReflectionEffectCreate) X(iplReflectionEffectRelease) X(iplReflectionEffectReset) X(iplReflectionEffectApply) \
    X(iplReflectionEffectGetTail) X(iplReflectionMixerCreate) X(iplReflectionMixerRelease) X(iplReflectionMixerApply) \
    X(iplSimulatorCreate) X(iplSimulatorRelease) X(iplSimulatorSetScene) X(iplSimulatorSetSharedInputs) \
    X(iplSimulatorCommit) X(iplSimulatorRunReflections) X(iplSourceCreate) X(iplSourceRelease) X(iplSourceAdd) \
    X(iplSourceRemove) X(iplSourceSetInputs) X(iplSourceGetOutputs)
#define X(n) static __typeof__(n) *p_##n;
FUNCS(X)
#undef X

typedef struct {
    IPLSource src;
    IPLReflectionEffect fx;
    float *pcm;              /* the sound, mono, at the module's rate, its gain in it */
    int frames, pos;
    float p[3];
    int state;               /* 0 free, 1 sounding, 2 its echoes ringing out */
    int fresh;               /* 1: a new sound since the last block (the effect's memory to be cleared) */
    int ring;                /* blocks its echoes have rung since the sound ended */
    int added, simulated;    /* in the simulator; a response for this sound's place exists */
    unsigned gen;            /* counts the sounds this voice has had */
} Voice;

static struct {
    HMODULE dll;
    IPLContext ctx;
    IPLScene scene;
    IPLStaticMesh mesh;
    IPLSimulator sim;
    IPLHRTF hrtf;
    IPLReflectionMixer mixer;
    IPLAmbisonicsDecodeEffect decode;
    IPLAudioBuffer in, amb, out;
    int on, rate, tris, irsize;
    CRITICAL_SECTION lock;
    int lock_made;
    /* word left for uwst_simulate */
    UwaTile tiles[N * N];
    UwaDoor doors[128];
    int ceil_mat, ndoors, have_tiles, level_dirty;
    float tile_m, lis[3], yaw;
    Voice v[UWST_VOICES];
    short fifo[UWST_FRAME * 2];
    int fifo_at, fifo_n;
} S;

int uwst_on(void) { return S.on; }
int uwst_triangles(void) { return S.tris; }

static IPLVector3 vec(float x, float y, float z) { IPLVector3 v = {x * S.tile_m, y * S.tile_m, -z * S.tile_m}; return v; }
static IPLCoordinateSpace3 space_at(const float p[3], float yaw)
{
    IPLCoordinateSpace3 c;
    c.ahead.x = sinf(yaw), c.ahead.y = 0, c.ahead.z = -cosf(yaw);    /* north (+z of ours) is -z here */
    c.up.x = 0, c.up.y = 1, c.up.z = 0;
    c.right.x = cosf(yaw), c.right.y = 0, c.right.z = sinf(yaw);
    c.origin = vec(p[0], p[1], p[2]);
    return c;
}

/* --- the level's triangles (src\level.c's shapes: a tile's corners 0 SW, 1 SE, 2 NE, 3 NW) --- */
typedef struct { IPLVector3 *v; IPLTriangle *t; IPLint32 *m; int nv, nt, cap; } Mesh;
static const float CX[4] = {0, 1, 1, 0}, CZ[4] = {0, 0, 1, 1};
static float corner_h(const UwaTile *t, int c)
{
    float h = t->height;
    switch (t->type) {
    case UWAT_SLOPE_N: return h + (c == 2 || c == 3);
    case UWAT_SLOPE_S: return h + (c == 0 || c == 1);
    case UWAT_SLOPE_E: return h + (c == 1 || c == 2);
    case UWAT_SLOPE_W: return h + (c == 0 || c == 3);
    default: return h;
    }
}
static int open_on_edge(const UwaTile *t, int e)   /* edges 0 S, 1 E, 2 N, 3 W */
{
    if (!t || t->type == UWAT_SOLID) return 0;
    switch (t->type) {
    case UWAT_DIAG_SE: return e == 0 || e == 1;
    case UWAT_DIAG_SW: return e == 0 || e == 3;
    case UWAT_DIAG_NE: return e == 2 || e == 1;
    case UWAT_DIAG_NW: return e == 2 || e == 3;
    default: return 1;
    }
}
static void tri(Mesh *m, IPLVector3 a, IPLVector3 b, IPLVector3 c, int mat)
{
    if (m->nt >= m->cap) return;
    m->v[m->nv] = a, m->v[m->nv + 1] = b, m->v[m->nv + 2] = c;
    m->t[m->nt].indices[0] = m->nv, m->t[m->nt].indices[1] = m->nv + 1, m->t[m->nt].indices[2] = m->nv + 2;
    m->m[m->nt++] = mat, m->nv += 3;
}
/* an upright quad over the ground line (ax, az)-(bx, bz), from ya / yb up to ta / tb */
static void wall(Mesh *m, float ax, float az, float bx, float bz, float ya, float yb, float ta, float tb, int mat)
{
    if (ta - ya < 1e-4f && tb - yb < 1e-4f) return;
    tri(m, vec(ax, ya, az), vec(bx, yb, bz), vec(bx, tb, bz), mat);
    tri(m, vec(ax, ya, az), vec(bx, tb, bz), vec(ax, ta, az), mat);
}
static void level_mesh(Mesh *m, const UwaTile *tiles, int ceil_mat, const UwaDoor *doors, int nd)
{
    static const int POLY[6][4] = {{0, 1, 2, 3}, {0, 1, 2, 3}, {0, 1, 2, -1}, {0, 1, 3, -1}, {1, 2, 3, -1}, {0, 2, 3, -1}};
    static const int EC[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}}, DX[4] = {0, 1, 0, -1}, DZ[4] = {-1, 0, 1, 0};
    for (int z = 0; z < N; z++)
        for (int x = 0; x < N; x++) {
            const UwaTile *t = &tiles[z * N + x];
            if (t->type == UWAT_SOLID) continue;
            int pk = t->type >= UWAT_DIAG_SE && t->type <= UWAT_DIAG_NW ? t->type : 0, n = POLY[pk][3] < 0 ? 3 : 4;
            IPLVector3 f[4], c[4];
            for (int k = 0; k < n; k++) {
                int cn = POLY[pk][k];
                f[k] = vec(x + CX[cn], corner_h(t, cn) * STEP, z + CZ[cn]), c[k] = vec(x + CX[cn], CEIL, z + CZ[cn]);
            }
            tri(m, f[0], f[1], f[2], t->floor_mat), tri(m, c[0], c[1], c[2], ceil_mat);
            if (n == 4) tri(m, f[0], f[2], f[3], t->floor_mat), tri(m, c[0], c[2], c[3], ceil_mat);
            if (pk) {   /* the diagonal wall: SW-NE for SE and NW, SE-NW for SW and NE */
                float h = t->height * STEP;
                if (t->type == UWAT_DIAG_SE || t->type == UWAT_DIAG_NW) wall(m, (float)x, (float)z, x + 1.0f, z + 1.0f, h, h, CEIL, CEIL, t->wall_mat);
                else wall(m, x + 1.0f, (float)z, (float)x, z + 1.0f, h, h, CEIL, CEIL, t->wall_mat);
            }
            for (int e = 0; e < 4; e++) {
                if (!open_on_edge(t, e)) continue;
                int nx = x + DX[e], nz = z + DZ[e], ca = EC[e][0], cb = EC[e][1];
                const UwaTile *nb = nx < 0 || nx >= N || nz < 0 || nz >= N ? NULL : &tiles[nz * N + nx];
                float ax = x + CX[ca], az = z + CZ[ca], bx = x + CX[cb], bz = z + CZ[cb];
                float ha = corner_h(t, ca) * STEP, hb = corner_h(t, cb) * STEP;
                if (!open_on_edge(nb, (e + 2) % 4)) wall(m, ax, az, bx, bz, ha, hb, CEIL, CEIL, nb ? nb->wall_mat : t->wall_mat);
                else {   /* the neighbour's floor above ours along this edge: the riser (its own side of it is the neighbour's) */
                    static const int OPP[4][2] = {{3, 2}, {0, 3}, {1, 0}, {2, 1}};   /* the neighbour's corners at our ca, cb */
                    float na = corner_h(nb, OPP[e][0]) * STEP, nbh = corner_h(nb, OPP[e][1]) * STEP;
                    if (na > ha + 1e-4f || nbh > hb + 1e-4f) wall(m, ax, az, bx, bz, ha, hb, na > ha ? na : ha, nbh > hb ? nbh : hb, nb->wall_mat);
                }
            }
        }
    for (int i = 0; i < nd; i++) {   /* a shut door: across the middle of its tile, floor to ceiling */
        const UwaDoor *d = &doors[i];
        if (!d->shut || d->tx < 0 || d->tx >= N || d->tz < 0 || d->tz >= N) continue;
        float h = tiles[d->tz * N + d->tx].height * STEP, x = (float)d->tx, z = (float)d->tz;
        if ((d->heading & 2) == 0) wall(m, x, z + 0.5f, x + 1.0f, z + 0.5f, h, h, CEIL, CEIL, d->mat);
        else wall(m, x + 0.5f, z, x + 0.5f, z + 1.0f, h, h, CEIL, CEIL, d->mat);
    }
}
/* the scene's mesh made again from the tiles and doors given (the simulation's thread) */
static void scene_build(const UwaTile *tiles, int ceil_mat, const UwaDoor *doors, int nd)
{
    Mesh m = {0};
    IPLMaterial mats[32];
    IPLStaticMeshSettings ms;
    IPLStaticMesh fresh = NULL;
    int nm = uwacmat_count();
    if (nm < 1) return;
    if (nm > 32) nm = 32;
    m.cap = N * N * 16;
    m.v = malloc(sizeof *m.v * (size_t)m.cap * 3), m.t = malloc(sizeof *m.t * (size_t)m.cap), m.m = malloc(sizeof *m.m * (size_t)m.cap);
    if (!m.v || !m.t || !m.m) { free(m.v), free(m.t), free(m.m); return; }
    level_mesh(&m, tiles, ceil_mat, doors, nd);
    for (int i = 0; i < nm; i++) {
        const UwaMaterial *u = uwacmat_get(i);
        for (int b = 0; b < 3; b++) mats[i].absorption[b] = u->absorb[b], mats[i].transmission[b] = u->transmit[b];
        mats[i].scattering = SCATTER;
    }
    for (int i = 0; i < m.nt; i++) if (m.m[i] < 0 || m.m[i] >= nm) m.m[i] = 0;
    memset(&ms, 0, sizeof ms);
    ms.numVertices = m.nv, ms.numTriangles = m.nt, ms.numMaterials = nm;
    ms.vertices = m.v, ms.triangles = m.t, ms.materialIndices = m.m, ms.materials = mats;
    if (m.nt > 0 && p_iplStaticMeshCreate(S.scene, &ms, &fresh) == IPL_STATUS_SUCCESS) {
        if (S.mesh) p_iplStaticMeshRemove(S.mesh, S.scene), p_iplStaticMeshRelease(&S.mesh);
        S.mesh = fresh;
        p_iplStaticMeshAdd(S.mesh, S.scene);
        p_iplSceneCommit(S.scene);
        S.tris = m.nt;
    }
    free(m.v), free(m.t), free(m.m);
}

/* --- start and stop --- */
static void release_all(void)
{
    for (int i = 0; i < UWST_VOICES; i++) {
        if (S.v[i].fx) p_iplReflectionEffectRelease(&S.v[i].fx);
        if (S.v[i].src) p_iplSourceRelease(&S.v[i].src);
        free(S.v[i].pcm);
    }
    memset(S.v, 0, sizeof S.v);
    if (S.ctx) {
        if (S.in.data) p_iplAudioBufferFree(S.ctx, &S.in);
        if (S.amb.data) p_iplAudioBufferFree(S.ctx, &S.amb);
        if (S.out.data) p_iplAudioBufferFree(S.ctx, &S.out);
    }
    memset(&S.in, 0, sizeof S.in), memset(&S.amb, 0, sizeof S.amb), memset(&S.out, 0, sizeof S.out);
    if (S.decode) p_iplAmbisonicsDecodeEffectRelease(&S.decode);
    if (S.mixer) p_iplReflectionMixerRelease(&S.mixer);
    if (S.hrtf) p_iplHRTFRelease(&S.hrtf);
    if (S.sim) p_iplSimulatorRelease(&S.sim);
    if (S.mesh) p_iplStaticMeshRelease(&S.mesh);
    if (S.scene) p_iplSceneRelease(&S.scene);
    if (S.ctx) p_iplContextRelease(&S.ctx);
    S.on = 0, S.tris = 0, S.fifo_n = S.fifo_at = 0, S.have_tiles = S.level_dirty = 0;
}

int uwst_init(const char *bindir, int rate)
{
    char p[MAX_PATH];
    const char *why = NULL;
    IPLContextSettings cs;
    IPLSceneSettings ss;
    IPLSimulationSettings sim;
    IPLAudioSettings au = {rate, UWST_FRAME};
    IPLHRTFSettings hs;
    IPLReflectionEffectSettings rs;
    IPLAmbisonicsDecodeEffectSettings ds;
    if (S.on) return 1;
    if (!S.dll) {
        snprintf(p, sizeof p, "%s" UWOS_SEP UWOS_LIB_PHONON, bindir ? bindir : ".");
        if (!uwos_lib_findable(p) || !(S.dll = LoadLibraryExA(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH))) {
            fprintf(stderr, "fx: acoustics high off (no phonon.dll)\n");
            return 0;
        }
#define X(n) if (!why && !(*(FARPROC *)&p_##n = GetProcAddress(S.dll, #n))) why = "phonon.dll lacks " #n;
        FUNCS(X)
#undef X
        if (why) {
            fprintf(stderr, "fx: acoustics high off (%s)\n", why);
            FreeLibrary(S.dll), S.dll = NULL;
            return 0;
        }
    }
    if (!S.lock_made) InitializeCriticalSection(&S.lock), S.lock_made = 1;
    S.rate = rate, S.irsize = (int)(IR_SECONDS * rate), S.tile_m = 2.4f;
    memset(&cs, 0, sizeof cs);
    cs.version = STEAMAUDIO_VERSION, cs.simdLevel = IPL_SIMDLEVEL_AVX2;
    memset(&ss, 0, sizeof ss);
    ss.type = IPL_SCENETYPE_DEFAULT;
    memset(&sim, 0, sizeof sim);
    sim.flags = IPL_SIMULATIONFLAGS_REFLECTIONS, sim.sceneType = IPL_SCENETYPE_DEFAULT, sim.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
    sim.maxNumOcclusionSamples = 16, sim.maxNumRays = 8192, sim.numDiffuseSamples = 32, sim.maxDuration = IR_SECONDS, sim.maxOrder = 1;
    sim.maxNumSources = UWST_VOICES, sim.numThreads = 2, sim.rayBatchSize = 1, sim.numVisSamples = 1;
    sim.samplingRate = rate, sim.frameSize = UWST_FRAME;
    memset(&hs, 0, sizeof hs);
    hs.type = IPL_HRTFTYPE_DEFAULT, hs.volume = 1.0f, hs.normType = IPL_HRTFNORMTYPE_NONE;
    rs.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, rs.irSize = S.irsize, rs.numChannels = 4;
    if (p_iplContextCreate(&cs, &S.ctx) != IPL_STATUS_SUCCESS) why = "iplContextCreate failed";
    else if (p_iplSceneCreate(S.ctx, &ss, &S.scene) != IPL_STATUS_SUCCESS) why = "no scene";
    else if (p_iplSimulatorCreate(S.ctx, &sim, &S.sim) != IPL_STATUS_SUCCESS) why = "no simulator";
    else if (p_iplHRTFCreate(S.ctx, &au, &hs, &S.hrtf) != IPL_STATUS_SUCCESS) why = "no HRTF";
    else if (p_iplReflectionMixerCreate(S.ctx, &au, &rs, &S.mixer) != IPL_STATUS_SUCCESS) why = "no reflection mixer";
    if (!why) {
        memset(&ds, 0, sizeof ds);
        ds.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO, ds.hrtf = S.hrtf, ds.maxOrder = 1;
        if (p_iplAmbisonicsDecodeEffectCreate(S.ctx, &au, &ds, &S.decode) != IPL_STATUS_SUCCESS) why = "no ambisonics decoder";
        else if (p_iplAudioBufferAllocate(S.ctx, 1, UWST_FRAME, &S.in) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, 4, UWST_FRAME, &S.amb) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, 2, UWST_FRAME, &S.out) != IPL_STATUS_SUCCESS) why = "no audio buffers";
    }
    for (int i = 0; i < UWST_VOICES && !why; i++) {
        IPLSourceSettings so = {IPL_SIMULATIONFLAGS_REFLECTIONS};
        if (p_iplSourceCreate(S.sim, &so, &S.v[i].src) != IPL_STATUS_SUCCESS) why = "no source";
        else if (p_iplReflectionEffectCreate(S.ctx, &au, &rs, &S.v[i].fx) != IPL_STATUS_SUCCESS) why = "no reflection effect";
    }
    if (why) {
        fprintf(stderr, "fx: acoustics high off (%s)\n", why);
        release_all();
        return 0;
    }
    p_iplSimulatorSetScene(S.sim, S.scene);
    p_iplSimulatorCommit(S.sim);
    S.on = 1;
    fprintf(stderr, "fx: acoustics high on (Steam Audio %d.%d.%d)\n", STEAMAUDIO_VERSION_MAJOR, STEAMAUDIO_VERSION_MINOR, STEAMAUDIO_VERSION_PATCH);
    return 1;
}

void uwst_shutdown(void)
{
    if (S.dll && S.lock_made) EnterCriticalSection(&S.lock);
    if (S.dll) release_all();
    if (S.dll && S.lock_made) LeaveCriticalSection(&S.lock);
}

/* --- word left by the game's side --- */
void uwst_level(const UwaTile t[N * N], int ceil_mat, const UwaDoor *d, int nd, float tile_m)
{
    if (!S.on) return;
    EnterCriticalSection(&S.lock);
    if (t) memcpy(S.tiles, t, sizeof S.tiles), S.ceil_mat = ceil_mat, S.have_tiles = 1;
    S.ndoors = 0;
    for (int i = 0; d && i < nd && S.ndoors < 128; i++) S.doors[S.ndoors++] = d[i];
    if (tile_m > 0.1f) S.tile_m = tile_m;
    S.level_dirty = S.have_tiles;
    LeaveCriticalSection(&S.lock);
}
static volatile LONG q_rays = 2048, q_bounces = 16;   /* (settings.txt acoustics_quality through uwaudio.c) */
void uwst_quality(int rays, int bounces)
{
    q_rays = rays < 256 ? 256 : rays > 8192 ? 8192 : rays, q_bounces = bounces < 1 ? 1 : bounces > 64 ? 64 : bounces;
}
void uwst_tile_m(float tile_m)
{
    if (!S.on || tile_m <= 0.1f || tile_m == S.tile_m) return;
    EnterCriticalSection(&S.lock);
    S.tile_m = tile_m, S.level_dirty = S.have_tiles;
    LeaveCriticalSection(&S.lock);
}
void uwst_listener(float x, float y, float z, float yaw)
{
    if (!S.on) return;
    EnterCriticalSection(&S.lock);
    S.lis[0] = x, S.lis[1] = y, S.lis[2] = z, S.yaw = yaw;
    LeaveCriticalSection(&S.lock);
}
int uwst_begin(float x, float y, float z, const short *mono, int frames, int rate, float gain)
{
    int k = -1, n;
    float *pcm;
    if (!S.on || !mono || frames <= 0 || rate <= 0) return -1;
    n = (int)((double)frames * S.rate / rate);
    if (n < 1 || !(pcm = malloc(sizeof *pcm * (size_t)n))) return -1;
    for (int i = 0; i < n; i++) {   /* to the module's rate, linear; the sound's gain in it */
        double at = (double)i * rate / S.rate;
        int a = (int)at, b = a + 1 < frames ? a + 1 : a;
        float f = (float)(at - a);
        pcm[i] = (mono[a] * (1.0f - f) + mono[b] * f) / 32768.0f * gain;
    }
    EnterCriticalSection(&S.lock);
    for (int i = 0; i < UWST_VOICES && k < 0; i++) if (!S.v[i].state) k = i;
    /* none free (the review, 2026-11-14: a walk's footsteps, each ringing some 3 s, held six of the eight, and the
       sounds after them had no echoes): the one whose echoes have rung out longest gives way */
    for (int i = 0; i < UWST_VOICES && k < 0; i++) if (S.v[i].state == 2) k = i;
    for (int i = 0; i < UWST_VOICES && k >= 0 && S.v[k].state == 2; i++) if (S.v[i].state == 2 && S.v[i].ring > S.v[k].ring) k = i;
    if (k >= 0) {
        Voice *v = &S.v[k];
        v->ring = 0;
        free(v->pcm);
        v->pcm = pcm, v->frames = n, v->pos = 0, v->p[0] = x, v->p[1] = y, v->p[2] = z;
        v->state = 1, v->fresh = 1, v->simulated = 0, v->gen++;
    }
    LeaveCriticalSection(&S.lock);
    if (k < 0) free(pcm);
    return k;
}
void uwst_end(int voice)
{
    if (!S.on || voice < 0 || voice >= UWST_VOICES) return;
    EnterCriticalSection(&S.lock);
    if (S.v[voice].state == 1) S.v[voice].state = 2, S.v[voice].ring = 0;
    LeaveCriticalSection(&S.lock);
}

/* --- the rays --- */
void uwst_simulate(void)
{
    static UwaTile tiles[N * N];
    static UwaDoor doors[128];
    IPLSimulationSharedInputs sh;
    float lis[3], yaw, place[UWST_VOICES][3];
    unsigned gen[UWST_VOICES];
    int live[UWST_VOICES], dirty, nd, ceil_mat;
    if (!S.on) return;
    EnterCriticalSection(&S.lock);
    dirty = S.level_dirty, S.level_dirty = 0, nd = S.ndoors, ceil_mat = S.ceil_mat;
    if (dirty) memcpy(tiles, S.tiles, sizeof tiles), memcpy(doors, S.doors, sizeof doors);
    memcpy(lis, S.lis, sizeof lis), yaw = S.yaw;
    for (int i = 0; i < UWST_VOICES; i++) live[i] = S.v[i].state != 0, gen[i] = S.v[i].gen, memcpy(place[i], S.v[i].p, sizeof place[i]);
    LeaveCriticalSection(&S.lock);
    if (dirty) scene_build(tiles, ceil_mat, doors, nd);
    if (!S.tris) return;                             /* no level yet: nothing to ring in */
    for (int i = 0; i < UWST_VOICES; i++) {
        Voice *v = &S.v[i];
        if (live[i] && !v->added) p_iplSourceAdd(v->src, S.sim), v->added = 1;
        if (!live[i] && v->added) p_iplSourceRemove(v->src, S.sim), v->added = 0;
        if (live[i]) {
            IPLSimulationInputs in;
            memset(&in, 0, sizeof in);
            in.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
            in.source = space_at(place[i], 0);
            in.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
            in.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
            in.directivity.dipoleWeight = 0, in.directivity.dipolePower = 1;
            for (int b = 0; b < 3; b++) in.reverbScale[b] = 1.0f;
            p_iplSourceSetInputs(v->src, IPL_SIMULATIONFLAGS_REFLECTIONS, &in);
        }
    }
    memset(&sh, 0, sizeof sh);
    sh.listener = space_at(lis, yaw);
    sh.numRays = (int)q_rays, sh.numBounces = (int)q_bounces, sh.duration = IR_SECONDS, sh.order = 1, sh.irradianceMinDistance = 1.0f;
    p_iplSimulatorSetSharedInputs(S.sim, IPL_SIMULATIONFLAGS_REFLECTIONS, &sh);
    p_iplSimulatorCommit(S.sim);
    p_iplSimulatorRunReflections(S.sim);
    EnterCriticalSection(&S.lock);
    for (int i = 0; i < UWST_VOICES; i++)
        if (live[i] && S.v[i].gen == gen[i]) S.v[i].simulated = 1;   /* (a newer sound on the voice waits for its own run) */
    LeaveCriticalSection(&S.lock);
}

/* --- the sound --- */
/* one block: every voice's next samples through its response into the mixer, the mixer decoded into S.out */
static int block(int hrtf)
{
    IPLReflectionEffectParams mp;
    IPLAmbisonicsDecodeEffectParams dp;
    float lis0[3] = {0, 0, 0}, yaw;
    int busy = 0;
    memset(&mp, 0, sizeof mp);
    mp.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, mp.numChannels = 4, mp.irSize = S.irsize;
    for (int i = 0; i < UWST_VOICES; i++) {
        Voice *v = &S.v[i];
        IPLSimulationOutputs out;
        int state, simulated, fresh, pos, frames;
        float *pcm;
        EnterCriticalSection(&S.lock);
        state = v->state, simulated = v->simulated, fresh = v->fresh, pos = v->pos, frames = v->frames, pcm = v->pcm;
        v->fresh = 0;
        if (state == 1) {
            int n = frames - pos < UWST_FRAME ? frames - pos : UWST_FRAME;
            memset(S.in.data[0], 0, sizeof(float) * UWST_FRAME);
            if (n > 0 && simulated) memcpy(S.in.data[0], pcm + pos, sizeof(float) * (size_t)n);   /* (before its first run: silence, in step) */
            v->pos = pos + UWST_FRAME;
            if (v->pos >= frames) v->state = 2, v->ring = 0;
        } else if (state == 2) v->ring++;
        LeaveCriticalSection(&S.lock);
        if (!state) continue;
        busy++;
        if (fresh) p_iplReflectionEffectReset(v->fx);
        if (!simulated) {                            /* nothing to ring with yet; a cut sound that never got its run is done */
            if (state == 2) { EnterCriticalSection(&S.lock); if (v->state == 2 && !v->simulated) v->state = 0; LeaveCriticalSection(&S.lock); }
            continue;
        }
        memset(&out, 0, sizeof out);
        p_iplSourceGetOutputs(v->src, IPL_SIMULATIONFLAGS_REFLECTIONS, &out);
        if (!out.reflections.ir) continue;
        out.reflections.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, out.reflections.numChannels = 4, out.reflections.irSize = S.irsize;
        if (state == 1) p_iplReflectionEffectApply(v->fx, &out.reflections, &S.in, &S.amb, S.mixer);
        else if (p_iplReflectionEffectGetTail(v->fx, &S.amb, S.mixer) == IPL_AUDIOEFFECTSTATE_TAILCOMPLETE) {
            EnterCriticalSection(&S.lock);
            if (v->state == 2) v->state = 0;         /* rung out: the voice is free */
            LeaveCriticalSection(&S.lock);
        }
    }
    p_iplReflectionMixerApply(S.mixer, &mp, &S.amb);
    EnterCriticalSection(&S.lock);
    yaw = S.yaw;
    LeaveCriticalSection(&S.lock);
    memset(&dp, 0, sizeof dp);
    dp.order = 1, dp.hrtf = S.hrtf, dp.orientation = space_at(lis0, yaw), dp.binaural = hrtf ? IPL_TRUE : IPL_FALSE;
    p_iplAmbisonicsDecodeEffectApply(S.decode, &dp, &S.amb, &S.out);
    return busy;
}

int uwst_mix(short *stereo, int frames, float wet, int hrtf)
{
    int busy = 0, done = 0;
    if (!S.on) { memset(stereo, 0, sizeof(short) * 2 * (size_t)frames); return 0; }
    while (done < frames) {
        if (S.fifo_at >= S.fifo_n) {
            busy = block(hrtf);
            for (int i = 0; i < UWST_FRAME; i++)
                for (int c = 0; c < 2; c++) {
                    float f = S.out.data[c][i] * wet * 32767.0f;
                    S.fifo[2 * i + c] = (short)(f > 32767.0f ? 32767.0f : f < -32768.0f ? -32768.0f : f);
                }
            S.fifo_at = 0, S.fifo_n = UWST_FRAME;
        }
        int n = S.fifo_n - S.fifo_at < frames - done ? S.fifo_n - S.fifo_at : frames - done;
        memcpy(stereo + 2 * done, S.fifo + 2 * S.fifo_at, sizeof(short) * 2 * (size_t)n);
        S.fifo_at += n, done += n;
    }
    for (int i = 0; i < UWST_VOICES; i++) busy += S.v[i].state != 0;
    return busy;
}
