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
/* the dials (2026-11-16, the user: every fixed number of the sound a setting; uwsteam.h's uwst_length / _scatter /
   _ear, from settings.txt through uwaudio.c). length: the longest ring, seconds (the response's size: taken when
   Steam Audio starts). scatter: our surfaces are rough-hewn, a quarter of a bounce goes every way (a guess, to be
   heard). ear: a response for the listener's own place kept ready, for a new sound's first blocks (below). */
static struct { float length, scatter; volatile LONG ear; } T = {2.0f, 0.25f, 1};
/* The sound itself through Steam Audio (2026-11-16, the user: "all of this needs to be in the game ... HRTF binaural
   rendering ... physics-based occlusion, transmission"; uwst_direct_mode): each voice's own samples go through
   Steam Audio's direct effect (what the level's surfaces between the sound and the listener let through, by band:
   its occlusion rays from a source of some size, its transmission by the materials) and its binaural effect (its
   HRTF, from the direction of the sound), into the same stream as the echoes. on: 0 the sound stays OpenAL's;
   occl: 1 Steam Audio's occlusion and transmission, 0 the gains uwaudio.c gives (ours); radius: the source's size
   in metres for the occlusion's rays; samples / rays: the occlusion's and the transmission's counts. */
static struct { volatile LONG on, occl, samples, rays; float radius; } D = {0, 1, 16, 8, 0.5f};
/* Steam Audio's pathing (2026-11-16, the user: "we want pathing from the steam audio"; uwst_pathing). A probe stands
   in every open tile of the level; iplPathBakerBake works out, once a level and in a thread of its own, which probes
   see each other and the shortest way between every two of them (the doors open: only their frames stand in the
   baked scene). While a sound plays, iplSimulatorRunPathing finds the ways from it to the listener through those
   probes, tested against the scene as it is now (a shut door's leaf closes a way and another is looked for), and
   iplPathEffect plays the sound as it arrives along them: from the direction of the opening it comes through, duller
   the more the way bends. It takes the place of our own way round a corner (uwacoustic.c's detour and waypoint) for
   the voices Steam Audio plays; the straight line's share stays the direct effect's (occlusion, transmission), and
   the paths' share is what the occlusion took away (1 - occlusion): a sound in plain sight is not heard twice.
   on: the setting; level: the paths' gain; range: how far a way may be, tiles; see: how far two probes may see each
   other, tiles. */
static struct { volatile LONG on; float level, range, see; } P = {1, 1.0f, 24.0f, 6.0f};
#define PROBE_UP 0.6f        /* a probe above its tile's floor, tiles (the eye is at 0.64) */
#define PROBE_REACH 1.0f     /* a probe's radius of influence, tiles: every place in a tile is in its own probe's */

#define FUNCS(X) \
    X(iplContextCreate) X(iplContextRelease) X(iplSceneCreate) X(iplSceneRelease) X(iplSceneCommit) \
    X(iplStaticMeshCreate) X(iplStaticMeshRelease) X(iplStaticMeshAdd) X(iplStaticMeshRemove) \
    X(iplAudioBufferAllocate) X(iplAudioBufferFree) X(iplHRTFCreate) X(iplHRTFRelease) \
    X(iplAmbisonicsDecodeEffectCreate) X(iplAmbisonicsDecodeEffectRelease) X(iplAmbisonicsDecodeEffectApply) \
    X(iplReflectionEffectCreate) X(iplReflectionEffectRelease) X(iplReflectionEffectReset) X(iplReflectionEffectApply) \
    X(iplReflectionEffectGetTail) X(iplReflectionMixerCreate) X(iplReflectionMixerRelease) X(iplReflectionMixerApply) \
    X(iplSimulatorCreate) X(iplSimulatorRelease) X(iplSimulatorSetScene) X(iplSimulatorSetSharedInputs) \
    X(iplSimulatorCommit) X(iplSimulatorRunReflections) X(iplSourceCreate) X(iplSourceRelease) X(iplSourceAdd) \
    X(iplSourceRemove) X(iplSourceSetInputs) X(iplSourceGetOutputs) \
    X(iplBinauralEffectCreate) X(iplBinauralEffectRelease) X(iplBinauralEffectReset) X(iplBinauralEffectApply) \
    X(iplDirectEffectCreate) X(iplDirectEffectRelease) X(iplDirectEffectReset) X(iplDirectEffectApply) \
    X(iplSimulatorRunDirect)     X(iplProbeBatchCreate) X(iplProbeBatchRelease) X(iplProbeBatchAddProbe) X(iplProbeBatchCommit)     X(iplProbeBatchGetNumProbes) X(iplPathBakerBake) X(iplPathBakerCancelBake)         X(iplSimulatorAddProbeBatch) X(iplSimulatorRemoveProbeBatch) X(iplSimulatorRunPathing)     X(iplPathEffectCreate) X(iplPathEffectRelease) X(iplPathEffectReset) X(iplPathEffectApply)
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
    int applied;             /* the audio's own: this sound has gone through the effect (there is a tail to ring out) */
    IPLSource dsrc;          /* in the direct simulator (always added) */
    IPLDirectEffect dfx;
    IPLBinauralEffect bfx;
    IPLPathEffect pfx;       /* the sound as it comes along Steam Audio's paths */
    IPLPathEffectParams pp;  /* ... the last run's (its shCoeffs are the source's own) */
    int path_ok;             /* a pathing run has been made for this sound */
    float pgain;             /* the paths' gain at the end of the last block (eased over the next) */
    float heard[3];          /* where the sound is heard from (its place, or the opening its way round comes through) */
    float gain[3];           /* uwaudio.c's gains by band (ours) ... */
    int floor;               /* ... 1: a floor under Steam Audio's; 0: the gains themselves */
    float occ, trans[3];     /* Steam Audio's last: the share not occluded, what the surfaces between let through */
    float echo;              /* its echoes' share of its level (1 unless told: a footstep's) */
    int rel;                 /* 1: heard from a place fixed to the listener's head (the player's own sounds) ... */
    float rel_right, rel_ahead;   /* ... this far to the right and ahead, tiles */
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
    float ir_s;              /* the response's length this start was made with (T.length then) */
    /* The listener's own place as a source (2026-11-16, "this twinge at the end"): a sound's echoes were silent until
       the rays had run once for it, and a run takes 50 to 500 ms by the detail and the voices live, so the echoes
       came in late and without their head (measured: work\acoustics\twinge). The response of the listener's place
       is kept ready instead (made again when the listener has moved a quarter tile, the level changed, or a sound
       is live), and a new sound rings with it from its first block until its own is made. */
    IPLSimulator dsim;       /* the direct simulator (occlusion, transmission): run by the audio's side every block */
    IPLAudioBuffer din, dmono, bin, pbin;
    IPLProbeBatch probes;    /* the level's, in dsim (changed under scene_lock) */
    int nprobes, path_ran;   /* path_ran: the direct runs make the paths too */
    unsigned path_tick;
    unsigned path_key;       /* the level S.probes were made for (level_key) */
    CRITICAL_SECTION scene_lock;   /* the scene's change against a direct run */
    float dry[2][UWST_FRAME];
    IPLSource ear;
    int ear_added, ear_ok;   /* in the simulator; a response exists */
    float ear_at[3];         /* where it was made */
    CRITICAL_SECTION lock;
    int lock_made;
    /* word left for uwst_simulate */
    UwaTile tiles[N * N];
    UwaDoor doors[128];
    int ceil_mat, ndoors, have_tiles, level_dirty;
    float tile_m, lis[3], yaw;
    Voice v[UWST_VOICES];
    float fifo[UWST_FRAME * 2];
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
static int door_frames = 1;   /* (uwst_frames) */
/* the things standing in the level (uwst_things; uwacoustic.c has why): a box of twelve triangles each */
static UwaBox things[256];
static int nthings, things_on = 1;
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
    for (int i = 0; i < nd; i++) {
        /* a door: across the middle of its tile. With frames (2026-11-16; uwacoustic.c has why): the slab round the
           opening always, of the tile's wall, the opening half a tile wide and 13/16 high; the leaf in it when shut.
           Without: a shut door from wall to wall, floor to ceiling, an open one nothing. */
        const UwaDoor *d = &doors[i];
        if (d->tx < 0 || d->tx >= N || d->tz < 0 || d->tz >= N) continue;
        const UwaTile *t = &tiles[d->tz * N + d->tx];
        float h = t->height * STEP, x = (float)d->tx, z = (float)d->tz, top = h + 0.8125f < CEIL ? h + 0.8125f : CEIL;
        int span_x = (d->heading & 2) == 0;
#define SLAB(a, b, y0, y1, mat) (span_x ? wall(m, x + (a), z + 0.5f, x + (b), z + 0.5f, y0, y0, y1, y1, mat) \
                                        : wall(m, x + 0.5f, z + (a), x + 0.5f, z + (b), y0, y0, y1, y1, mat))
        if (door_frames) {
            SLAB(0.0f, 0.25f, h, CEIL, t->wall_mat), SLAB(0.75f, 1.0f, h, CEIL, t->wall_mat);
            SLAB(0.25f, 0.75f, top, CEIL, t->wall_mat);
            if (d->shut) SLAB(0.25f, 0.75f, h, top, d->mat);
        } else if (d->shut) SLAB(0.0f, 1.0f, h, CEIL, d->mat);
#undef SLAB
    }
    {   /* the things: their boxes, as they stand now (a copy under the lock: the game's side may give them anew) */
        static const int F[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
        UwaBox b[256];
        int nb = 0;
        EnterCriticalSection(&S.lock);
        if (things_on) nb = nthings, memcpy(b, things, sizeof(UwaBox) * (size_t)nb);
        LeaveCriticalSection(&S.lock);
        for (int i = 0; i < nb; i++) {
            IPLVector3 c[8];
            for (int k = 0; k < 8; k++) c[k] = vec(k & 1 ? b[i].hi[0] : b[i].lo[0], k & 2 ? b[i].hi[1] : b[i].lo[1], k & 4 ? b[i].hi[2] : b[i].lo[2]);
            for (int f = 0; f < 6; f++) tri(m, c[F[f][0]], c[F[f][1]], c[F[f][2]], b[i].mat), tri(m, c[F[f][0]], c[F[f][2]], c[F[f][3]], b[i].mat);
        }
    }
}
/* the tiles and doors given as a mesh of `scene` (not yet added to it); *tris its triangles. NULL: none */
static IPLStaticMesh mesh_make(IPLScene scene, const UwaTile *tiles, int ceil_mat, const UwaDoor *doors, int nd, int *tris)
{
    Mesh m = {0};
    IPLMaterial mats[32];
    IPLStaticMeshSettings ms;
    IPLStaticMesh fresh = NULL;
    int nm = uwacmat_count();
    if (nm < 1) return NULL;
    if (nm > 32) nm = 32;
    m.cap = N * N * 16;
    m.v = malloc(sizeof *m.v * (size_t)m.cap * 3), m.t = malloc(sizeof *m.t * (size_t)m.cap), m.m = malloc(sizeof *m.m * (size_t)m.cap);
    if (!m.v || !m.t || !m.m) { free(m.v), free(m.t), free(m.m); return NULL; }
    level_mesh(&m, tiles, ceil_mat, doors, nd);
    for (int i = 0; i < nm; i++) {
        const UwaMaterial *u = uwacmat_get(i);
        for (int b = 0; b < 3; b++) mats[i].absorption[b] = u->absorb[b], mats[i].transmission[b] = u->transmit[b];
        mats[i].scattering = T.scatter;
    }
    for (int i = 0; i < m.nt; i++) if (m.m[i] < 0 || m.m[i] >= nm) m.m[i] = 0;
    memset(&ms, 0, sizeof ms);
    ms.numVertices = m.nv, ms.numTriangles = m.nt, ms.numMaterials = nm;
    ms.vertices = m.v, ms.triangles = m.t, ms.materialIndices = m.m, ms.materials = mats;
    if (m.nt <= 0 || p_iplStaticMeshCreate(scene, &ms, &fresh) != IPL_STATUS_SUCCESS) fresh = NULL;
    if (tris) *tris = m.nt;
    free(m.v), free(m.t), free(m.m);
    return fresh;
}
/* the scene's mesh made again from the tiles and doors given (the simulation's thread) */
static void scene_build(const UwaTile *tiles, int ceil_mat, const UwaDoor *doors, int nd)
{
    int tris = 0;
    IPLStaticMesh fresh = mesh_make(S.scene, tiles, ceil_mat, doors, nd, &tris);
    if (!fresh) return;
    EnterCriticalSection(&S.scene_lock);             /* (never under a direct run: block()) */
    if (S.mesh) p_iplStaticMeshRemove(S.mesh, S.scene), p_iplStaticMeshRelease(&S.mesh);
    S.mesh = fresh;
    p_iplStaticMeshAdd(S.mesh, S.scene);
    p_iplSceneCommit(S.scene);
    p_iplSimulatorCommit(S.dsim);
    S.tris = tris;
    LeaveCriticalSection(&S.scene_lock);
}

/* --- Steam Audio's pathing: the level's probes and the ways between them, baked in a thread of its own --- */
static struct {
    HANDLE thread;
    volatile LONG busy, cancel;
    UwaTile tiles[N * N];
    UwaDoor doors[128];
    int nd, ceil_mat;
    unsigned key;                /* the level being baked (level_key) */
    IPLProbeBatch batch;         /* the result: NULL when it failed or was cancelled */
    int n;
    float seconds;
} B;
/* what the baked ways depend on: the tiles' shapes and heights, the doors' places (their frames), the metres a tile,
   the frames' setting and the bake's own numbers. Not the materials, not which doors are shut. */
static unsigned level_key(const UwaTile *t, const UwaDoor *d, int nd)
{
    unsigned h = 2166136261u;
#define MIX(v) (h = (h ^ (unsigned)(v)) * 16777619u)
    for (int i = 0; i < N * N; i++) MIX(t[i].type), MIX(t[i].height);
    for (int i = 0; i < nd; i++) MIX(d[i].tx), MIX(d[i].tz), MIX(d[i].heading & 2);
    EnterCriticalSection(&S.lock);   /* (the things standing in the level: a pillar in a passage changes the ways) */
    for (int i = 0; things_on && i < nthings; i++)
        for (int k = 0; k < 3; k++) MIX((int)(things[i].lo[k] * 16.0f)), MIX((int)(things[i].hi[k] * 16.0f));
    LeaveCriticalSection(&S.lock);
    MIX((int)(S.tile_m * 1000.0f)), MIX(door_frames), MIX((int)(P.range * 16.0f)), MIX((int)(P.see * 16.0f));
    MIX((int)(PROBE_UP * 100.0f)), MIX((int)(PROBE_REACH * 100.0f)), MIX(3);   /* (the last: this code's own version) */
#undef MIX
    return h ? h : 1;
}
/* a probe in every open tile: over the middle of its floor (a diagonal tile's open half), PROBE_UP above it */
static int probes_place(IPLProbeBatch batch, const UwaTile *tiles)
{
    static const int POLY[6][4] = {{0, 1, 2, 3}, {0, 1, 2, 3}, {0, 1, 2, -1}, {0, 1, 3, -1}, {1, 2, 3, -1}, {0, 2, 3, -1}};
    int n = 0;
    for (int z = 0; z < N; z++)
        for (int x = 0; x < N; x++) {
            const UwaTile *t = &tiles[z * N + x];
            if (t->type == UWAT_SOLID) continue;
            int pk = t->type >= UWAT_DIAG_SE && t->type <= UWAT_DIAG_NW ? t->type : 0, k = POLY[pk][3] < 0 ? 3 : 4;
            float cx = 0, cz = 0, h = 0;
            IPLSphere sp;
            for (int i = 0; i < k; i++) cx += CX[POLY[pk][i]], cz += CZ[POLY[pk][i]], h += corner_h(t, POLY[pk][i]) * STEP;
            cx /= k, cz /= k, h /= k;
            if (h + 0.3f > CEIL) continue;           /* (no air to speak of over this floor) */
            sp.center = vec(x + cx, h + PROBE_UP < CEIL - 0.1f ? h + PROBE_UP : CEIL - 0.1f, z + cz);
            sp.radius = PROBE_REACH * S.tile_m;
            p_iplProbeBatchAddProbe(batch, sp);
            n++;
        }
    return n;
}
/* (iplPathBakerBake calls its progress callback without asking whether there is one: 4.8.1 crashed on NULL) */
static void IPLCALL bake_progress(IPLfloat32 done, void *user) { (void)done, (void)user; }
static DWORD WINAPI bake_worker(LPVOID arg)
{
    IPLSceneSettings ss;
    IPLScene scene = NULL;
    IPLStaticMesh mesh = NULL;
    LARGE_INTEGER a, b, q;
    (void)arg;
    QueryPerformanceFrequency(&q), QueryPerformanceCounter(&a);
    B.batch = NULL, B.n = 0;
    {
        memset(&ss, 0, sizeof ss);
        ss.type = IPL_SCENETYPE_DEFAULT;
        for (int i = 0; i < B.nd; i++) B.doors[i].shut = 0;      /* the ways are baked with every door open */
        if (p_iplSceneCreate(S.ctx, &ss, &scene) == IPL_STATUS_SUCCESS && (mesh = mesh_make(scene, B.tiles, B.ceil_mat, B.doors, B.nd, NULL)) != NULL) {
            p_iplStaticMeshAdd(mesh, scene);
            p_iplSceneCommit(scene);
            if (p_iplProbeBatchCreate(S.ctx, &B.batch) != IPL_STATUS_SUCCESS) B.batch = NULL;
            if (B.batch) {
                IPLPathBakeParams bp;
                B.n = probes_place(B.batch, B.tiles);
                p_iplProbeBatchCommit(B.batch);
                memset(&bp, 0, sizeof bp);
                bp.scene = scene, bp.probeBatch = B.batch;
                bp.identifier.type = IPL_BAKEDDATATYPE_PATHING, bp.identifier.variation = IPL_BAKEDDATAVARIATION_DYNAMIC;
                bp.numSamples = 1, bp.radius = 0.1f * S.tile_m, bp.threshold = 0.1f;
                bp.visRange = P.see * S.tile_m, bp.pathRange = P.range * S.tile_m, bp.numThreads = 2;
                if (B.n > 1 && !B.cancel) p_iplPathBakerBake(S.ctx, &bp, bake_progress, NULL);
                if (B.n < 2 || B.cancel) p_iplProbeBatchRelease(&B.batch), B.batch = NULL;
            }
        }
        if (mesh) p_iplStaticMeshRelease(&mesh);
        if (scene) p_iplSceneRelease(&scene);
    }
    QueryPerformanceCounter(&b);
    B.seconds = (float)((double)(b.QuadPart - a.QuadPart) / (double)q.QuadPart);
    InterlockedExchange(&B.busy, 0);
    return 0;
}
static void bake_stop(void)   /* a bake under way given up and waited out; its result dropped */
{
    if (!B.thread) return;
    InterlockedExchange(&B.cancel, 1);
    /* (2026-11-17: the bake is waited out, not cancelled. iplPathBakerCancelBake is not safe in 4.8.1's Linux library:
       called while the bake's jobs run, its pool's threads spun for ever and iplPathBakerBake never came back
       (work\soundfix\bakequit_linux.c with a simulation thread: the second round hung, the stacks in the library), and
       in the game a quit during the bake ended in a fault in one of those threads. B.cancel still keeps a bake that
       has not reached the library from starting. UWPORT_FX_BAKE_CANCEL=1: the cancel as before, for a comparison.) */
    {
        const char *e = getenv("UWPORT_FX_BAKE_CANCEL");
        while (B.busy) { if (e && *e == '1') p_iplPathBakerCancelBake(S.ctx); Sleep(5); }
    }
    WaitForSingleObject(B.thread, 60000), CloseHandle(B.thread), B.thread = NULL;
    if (B.batch) p_iplProbeBatchRelease(&B.batch);
    B.batch = NULL, B.key = 0, B.cancel = 0;
}
/* the simulation's thread, each run: a level whose ways are not the ones in use is given to the bake; a bake that
   has ended has its probes put in the direct simulator in place of the old ones */
/* a level change under way (the front end, when it sees one): no pathing until the new level's tiles have come
   (path_service), 8 s at most. 2026-11-17: the game faulted inside phonon.dll (a null read) going from level 1
   back to level 2, the paths still run with the old level's probes and the listener already in the new one; Steam
   Audio's own release notes list faults in iplSimulatorRunPathing across a scene's and a probe batch's change */
static volatile LONG path_hold;
static unsigned long long path_hold_t;
void uwst_level_change(void)
{
    path_hold_t = GetTickCount64();
    InterlockedExchange(&path_hold, 1);
}
static int path_held(void)
{
    if (path_hold && GetTickCount64() - path_hold_t > 8000) InterlockedExchange(&path_hold, 0);
    return path_hold != 0;
}
/* every source's paths off, where a batch of probes leaves the simulator: a source still naming a batch that has left
   faults iplSimulatorRunPathing (2026-11-17: work\steamproof, the same calls without the game; direct_run has the
   other half). The sounding voices are given theirs again by the next direct_run. */
static void paths_forget(void)
{
    for (int i = 0; i < UWST_VOICES; i++) {
        IPLSimulationInputs in;
        memset(&in, 0, sizeof in);
        p_iplSourceSetInputs(S.v[i].dsrc, IPL_SIMULATIONFLAGS_PATHING, &in);
    }
}
static void path_service(const UwaTile *tiles, const UwaDoor *doors, int nd, int ceil_mat, int dirty)
{
    if (dirty && P.on) {
        unsigned key = level_key(tiles, doors, nd);
        if (key != S.path_key && S.probes) {
            /* another level's probes: out at once (no paths until this level's are baked), every voice's old
               paths forgotten */
            EnterCriticalSection(&S.scene_lock);
            paths_forget(), p_iplSimulatorRemoveProbeBatch(S.dsim, S.probes), p_iplProbeBatchRelease(&S.probes);
            S.probes = NULL, S.nprobes = 0, S.path_key = 0, S.path_ran = 0;
            p_iplSimulatorCommit(S.dsim);
            LeaveCriticalSection(&S.scene_lock);
            EnterCriticalSection(&S.lock);
            for (int i = 0; i < UWST_VOICES; i++) S.v[i].path_ok = 0;
            LeaveCriticalSection(&S.lock);
        }
        if (key != S.path_key && key != B.key) {
            bake_stop();
            memcpy(B.tiles, tiles, sizeof B.tiles), memcpy(B.doors, doors, sizeof(UwaDoor) * (size_t)nd);
            B.nd = nd, B.ceil_mat = ceil_mat, B.key = key;
            InterlockedExchange(&B.busy, 1);
            fprintf(stderr, "fx: working out this level's sound paths (Steam Audio's pathing, in the background)\n");
            if (!(B.thread = CreateThread(NULL, 0, bake_worker, NULL, 0, NULL))) B.busy = 0, B.key = 0;
        }
    }
    if (B.thread && !B.busy) {
        WaitForSingleObject(B.thread, 60000), CloseHandle(B.thread), B.thread = NULL;
        if (B.batch) {
            EnterCriticalSection(&S.scene_lock);
            if (S.probes) paths_forget(), p_iplSimulatorRemoveProbeBatch(S.dsim, S.probes), p_iplProbeBatchRelease(&S.probes);
            S.probes = B.batch, S.nprobes = B.n, S.path_key = B.key, S.path_ran = 0;
            for (int i = 0; i < UWST_VOICES; i++) S.v[i].path_ok = 0;
            p_iplSimulatorAddProbeBatch(S.dsim, S.probes);
            p_iplSimulatorCommit(S.dsim);
            LeaveCriticalSection(&S.scene_lock);
            fprintf(stderr, "fx: sound paths ready (Steam Audio's pathing: %d probes, baked in %.1f s)\n", B.n, B.seconds);
        } else fprintf(stderr, "fx: sound paths: none made for this level\n");
        B.batch = NULL, B.key = 0;
    }
}

/* --- start and stop --- */
static void release_all(void)
{
    if (S.ctx) bake_stop();
    for (int i = 0; i < UWST_VOICES; i++) {
        if (S.v[i].fx) p_iplReflectionEffectRelease(&S.v[i].fx);
        if (S.v[i].src) p_iplSourceRelease(&S.v[i].src);
        if (S.v[i].dfx) p_iplDirectEffectRelease(&S.v[i].dfx);
        if (S.v[i].bfx) p_iplBinauralEffectRelease(&S.v[i].bfx);
        if (S.v[i].pfx) p_iplPathEffectRelease(&S.v[i].pfx);
        if (S.v[i].dsrc) p_iplSourceRelease(&S.v[i].dsrc);
        free(S.v[i].pcm);
    }
    memset(S.v, 0, sizeof S.v);
    if (S.ctx) {
        if (S.in.data) p_iplAudioBufferFree(S.ctx, &S.in);
        if (S.amb.data) p_iplAudioBufferFree(S.ctx, &S.amb);
        if (S.out.data) p_iplAudioBufferFree(S.ctx, &S.out);
        if (S.din.data) p_iplAudioBufferFree(S.ctx, &S.din);
        if (S.dmono.data) p_iplAudioBufferFree(S.ctx, &S.dmono);
        if (S.bin.data) p_iplAudioBufferFree(S.ctx, &S.bin);
        if (S.pbin.data) p_iplAudioBufferFree(S.ctx, &S.pbin);
    }
    memset(&S.pbin, 0, sizeof S.pbin);
    if (S.probes) {
        if (S.dsim) p_iplSimulatorRemoveProbeBatch(S.dsim, S.probes);
        p_iplProbeBatchRelease(&S.probes);
    }
    S.nprobes = 0, S.path_key = 0, S.path_ran = 0;
    memset(&S.in, 0, sizeof S.in), memset(&S.amb, 0, sizeof S.amb), memset(&S.out, 0, sizeof S.out);
    memset(&S.din, 0, sizeof S.din), memset(&S.dmono, 0, sizeof S.dmono), memset(&S.bin, 0, sizeof S.bin);
    if (S.dsim) p_iplSimulatorRelease(&S.dsim);
    if (S.ear) p_iplSourceRelease(&S.ear);
    S.ear_added = S.ear_ok = 0;
    if (S.decode) p_iplAmbisonicsDecodeEffectRelease(&S.decode);
    if (S.mixer) p_iplReflectionMixerRelease(&S.mixer);
    if (S.hrtf) p_iplHRTFRelease(&S.hrtf);
    if (S.sim) p_iplSimulatorRelease(&S.sim);
    if (S.mesh) p_iplStaticMeshRelease(&S.mesh);
    if (S.scene) p_iplSceneRelease(&S.scene);
    if (S.ctx) p_iplContextRelease(&S.ctx);
    /* (the tiles last given are kept: a start again for a new response length builds the same level) */
    S.on = 0, S.tris = 0, S.fifo_n = S.fifo_at = 0, S.level_dirty = 0;
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
    if (!S.lock_made) InitializeCriticalSection(&S.lock), InitializeCriticalSection(&S.scene_lock), S.lock_made = 1;
    S.rate = rate, S.ir_s = T.length, S.irsize = (int)(S.ir_s * rate);
    if (S.tile_m <= 0.1f) S.tile_m = 2.4f;
    memset(&cs, 0, sizeof cs);
    cs.version = STEAMAUDIO_VERSION, cs.simdLevel = IPL_SIMDLEVEL_AVX2;
    memset(&ss, 0, sizeof ss);
    ss.type = IPL_SCENETYPE_DEFAULT;
    memset(&sim, 0, sizeof sim);
    sim.flags = IPL_SIMULATIONFLAGS_REFLECTIONS, sim.sceneType = IPL_SCENETYPE_DEFAULT, sim.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
    sim.maxNumOcclusionSamples = 16, sim.maxNumRays = 8192, sim.numDiffuseSamples = 32, sim.maxDuration = S.ir_s, sim.maxOrder = 1;
    sim.maxNumSources = UWST_VOICES + 1, sim.numThreads = 2, sim.rayBatchSize = 1, sim.numVisSamples = 1;
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
    if (!why) {
        IPLSourceSettings so = {IPL_SIMULATIONFLAGS_REFLECTIONS};
        if (p_iplSourceCreate(S.sim, &so, &S.ear) != IPL_STATUS_SUCCESS) why = "no source";
    }
    if (!why) {   /* the sound itself: a simulator for the direct path alone, a direct and a binaural effect a voice */
        IPLSimulationSettings ds2;
        IPLDirectEffectSettings de = {1};
        IPLBinauralEffectSettings be = {S.hrtf};
        IPLSourceSettings so = {IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_PATHING};
        IPLPathEffectSettings pe;
        memset(&pe, 0, sizeof pe);
        pe.maxOrder = 1, pe.spatialize = IPL_TRUE, pe.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO, pe.hrtf = S.hrtf;
        memset(&ds2, 0, sizeof ds2);
        ds2.flags = IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_PATHING, ds2.sceneType = IPL_SCENETYPE_DEFAULT, ds2.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        ds2.maxNumOcclusionSamples = 64, ds2.maxNumRays = 256, ds2.numDiffuseSamples = 8, ds2.maxDuration = 0.5f, ds2.maxOrder = 1;
        ds2.maxNumSources = UWST_VOICES, ds2.numThreads = 1, ds2.rayBatchSize = 1, ds2.numVisSamples = 1;
        ds2.samplingRate = rate, ds2.frameSize = UWST_FRAME;
        if (p_iplSimulatorCreate(S.ctx, &ds2, &S.dsim) != IPL_STATUS_SUCCESS) why = "no direct simulator";
        else if (p_iplAudioBufferAllocate(S.ctx, 1, UWST_FRAME, &S.din) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, 1, UWST_FRAME, &S.dmono) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, 2, UWST_FRAME, &S.bin) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, 2, UWST_FRAME, &S.pbin) != IPL_STATUS_SUCCESS) why = "no audio buffers";
        for (int i = 0; i < UWST_VOICES && !why; i++) {
            if (p_iplSourceCreate(S.dsim, &so, &S.v[i].dsrc) != IPL_STATUS_SUCCESS) why = "no direct source";
            else if (p_iplDirectEffectCreate(S.ctx, &au, &de, &S.v[i].dfx) != IPL_STATUS_SUCCESS) why = "no direct effect";
            else if (p_iplBinauralEffectCreate(S.ctx, &au, &be, &S.v[i].bfx) != IPL_STATUS_SUCCESS) why = "no binaural effect";
            else if (p_iplPathEffectCreate(S.ctx, &au, &pe, &S.v[i].pfx) != IPL_STATUS_SUCCESS) why = "no path effect";
            else p_iplSourceAdd(S.v[i].dsrc, S.dsim);
        }
    }
    if (why) {
        fprintf(stderr, "fx: acoustics high off (%s)\n", why);
        release_all();
        return 0;
    }
    p_iplSimulatorSetScene(S.sim, S.scene);
    p_iplSimulatorCommit(S.sim);
    p_iplSimulatorSetScene(S.dsim, S.scene);
    p_iplSimulatorCommit(S.dsim);
    S.level_dirty = S.have_tiles;
    S.on = 1;
    fprintf(stderr, "fx: acoustics high on (Steam Audio %d.%d.%d)\n", STEAMAUDIO_VERSION_MAJOR, STEAMAUDIO_VERSION_MINOR, STEAMAUDIO_VERSION_PATCH);
    return 1;
}

void uwst_shutdown(void)
{
    /* (2026-11-16, the review: the bake's thread takes S.lock for the things' boxes (level_mesh); waited for under
       that lock it could never end, and the game hung on the way out. Stopped first, the lock not held.) */
    if (S.dll && S.ctx) bake_stop();
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
int uwst_length(float seconds)
{
    T.length = seconds < 0.25f ? 0.25f : seconds > 6.0f ? 6.0f : seconds;
    return S.on && fabsf(T.length - S.ir_s) > 1e-3f;
}
float uwst_length_now(void) { return S.on ? S.ir_s : T.length; }
void uwst_scatter(float share)
{
    share = share < 0 ? 0 : share > 1 ? 1 : share;
    if (share == T.scatter) return;
    T.scatter = share;
    if (!S.on) return;
    EnterCriticalSection(&S.lock);
    S.level_dirty = S.have_tiles;
    LeaveCriticalSection(&S.lock);
}
void uwst_ear(int on) { InterlockedExchange(&T.ear, on != 0); }
void uwst_things(const UwaBox *b, int n)
{
    if (S.lock_made) EnterCriticalSection(&S.lock);
    nthings = 0;
    for (int i = 0; b && i < n && nthings < 256; i++) things[nthings++] = b[i];
    if (S.on) S.level_dirty = S.have_tiles;
    if (S.lock_made) LeaveCriticalSection(&S.lock);
}
void uwst_things_on(int on)
{
    if ((on != 0) == things_on) return;
    if (S.lock_made) EnterCriticalSection(&S.lock);
    things_on = on != 0;
    if (S.on) S.level_dirty = S.have_tiles;
    if (S.lock_made) LeaveCriticalSection(&S.lock);
}
void uwst_frames(int on)
{
    if ((on != 0) == door_frames) return;
    door_frames = on != 0;
    if (!S.on) return;
    EnterCriticalSection(&S.lock);
    S.level_dirty = S.have_tiles;
    LeaveCriticalSection(&S.lock);
}
void uwst_direct_mode(int on, int steam_occlusion, float radius_m, int samples, int rays)
{
    InterlockedExchange(&D.on, on != 0), InterlockedExchange(&D.occl, steam_occlusion != 0);
    D.radius = radius_m < 0.05f ? 0.05f : radius_m > 3.0f ? 3.0f : radius_m;
    InterlockedExchange(&D.samples, samples < 1 ? 1 : samples > 64 ? 64 : samples);
    InterlockedExchange(&D.rays, rays < 1 ? 1 : rays > 32 ? 32 : rays);
}
int uwst_direct_on(void) { return S.on && D.on; }
void uwst_pathing(int on, float level)
{
    P.level = level < 0 ? 0 : level > 4.0f ? 4.0f : level;
    if ((on != 0) == (P.on != 0)) return;
    InterlockedExchange(&P.on, on != 0);
    if (!S.on || !on) return;
    EnterCriticalSection(&S.lock);
    S.level_dirty = S.have_tiles;                    /* (the level looked at again: its ways are wanted now) */
    LeaveCriticalSection(&S.lock);
}
int uwst_pathing_probes(void) { return S.on && P.on ? S.nprobes : 0; }
int uwst_pathing_busy(void) { return B.thread != NULL; }
int uwst_pathing_heard(int voice, float eq[3], float sh[4])
{
    int ok;
    if (!S.on || voice < 0 || voice >= UWST_VOICES) return 0;
    ok = S.v[voice].path_ok && S.v[voice].pp.shCoeffs != NULL;
    for (int k = 0; k < 3; k++) eq[k] = ok ? S.v[voice].pp.eqCoeffs[k] : 0;
    for (int k = 0; k < 4; k++) sh[k] = ok ? S.v[voice].pp.shCoeffs[k] : 0;
    return ok;
}
void uwst_relative(int voice, float right, float ahead)
{
    if (!S.on || voice < 0 || voice >= UWST_VOICES) return;
    EnterCriticalSection(&S.lock);
    S.v[voice].rel = 1, S.v[voice].rel_right = right, S.v[voice].rel_ahead = ahead;
    LeaveCriticalSection(&S.lock);
}
void uwst_echo(int voice, float share)
{
    if (!S.on || voice < 0 || voice >= UWST_VOICES) return;
    EnterCriticalSection(&S.lock);
    S.v[voice].echo = share < 0 ? 0 : share;
    LeaveCriticalSection(&S.lock);
}
void uwst_direct(int voice, const float heard[3], const float gain[3], int floor)
{
    if (!S.on || voice < 0 || voice >= UWST_VOICES) return;
    EnterCriticalSection(&S.lock);
    if (heard) memcpy(S.v[voice].heard, heard, sizeof S.v[voice].heard);
    if (gain) memcpy(S.v[voice].gain, gain, sizeof S.v[voice].gain);
    S.v[voice].floor = floor;
    LeaveCriticalSection(&S.lock);
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
        v->heard[0] = x, v->heard[1] = y, v->heard[2] = z, v->gain[0] = v->gain[1] = v->gain[2] = 0, v->floor = 1;
        v->occ = 1, v->trans[0] = v->trans[1] = v->trans[2] = 1, v->echo = 1, v->rel = 0, v->path_ok = 0;
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
    int live[UWST_VOICES], dirty, nd, ceil_mat, any = 0, ear = (int)T.ear, stale;
    if (!S.on) return;
    EnterCriticalSection(&S.lock);
    dirty = S.level_dirty, S.level_dirty = 0, nd = S.ndoors, ceil_mat = S.ceil_mat;
    if (dirty) memcpy(tiles, S.tiles, sizeof tiles), memcpy(doors, S.doors, sizeof doors);
    memcpy(lis, S.lis, sizeof lis), yaw = S.yaw;
    /* (2026-11-16: a voice ringing out leaves the simulation, its tail needs no new response; a walk kept six
       voices live, each adding to every run's time, and now keeps the two or three still sounding) */
    for (int i = 0; i < UWST_VOICES; i++) live[i] = S.v[i].state == 1, any |= live[i], gen[i] = S.v[i].gen, memcpy(place[i], S.v[i].p, sizeof place[i]);
    stale = !S.ear_ok || (lis[0] - S.ear_at[0]) * (lis[0] - S.ear_at[0]) + (lis[1] - S.ear_at[1]) * (lis[1] - S.ear_at[1]) +
            (lis[2] - S.ear_at[2]) * (lis[2] - S.ear_at[2]) > 0.0625f;
    LeaveCriticalSection(&S.lock);
    if (dirty) scene_build(tiles, ceil_mat, doors, nd);
    path_service(tiles, doors, nd, ceil_mat, dirty);
    if (dirty) InterlockedExchange(&path_hold, 0);   /* (the new level's tiles are in: its own probes decide from here) */
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
    if (ear) {                                       /* the listener's own place */
        IPLSimulationInputs in;
        if (!S.ear_added) p_iplSourceAdd(S.ear, S.sim), S.ear_added = 1;
        memset(&in, 0, sizeof in);
        in.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
        in.source = space_at(lis, 0);
        in.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
        in.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
        in.directivity.dipoleWeight = 0, in.directivity.dipolePower = 1;
        for (int b = 0; b < 3; b++) in.reverbScale[b] = 1.0f;
        p_iplSourceSetInputs(S.ear, IPL_SIMULATIONFLAGS_REFLECTIONS, &in);
    } else if (S.ear_added) {
        p_iplSourceRemove(S.ear, S.sim), S.ear_added = 0;
        EnterCriticalSection(&S.lock);
        S.ear_ok = 0;
        LeaveCriticalSection(&S.lock);
    }
    memset(&sh, 0, sizeof sh);
    sh.listener = space_at(lis, yaw);
    sh.numRays = (int)q_rays, sh.numBounces = (int)q_bounces, sh.duration = S.ir_s, sh.order = 1, sh.irradianceMinDistance = 1.0f;
    p_iplSimulatorSetSharedInputs(S.sim, IPL_SIMULATIONFLAGS_REFLECTIONS, &sh);
    p_iplSimulatorCommit(S.sim);
    if (!any && !(ear && (stale || dirty))) return;  /* nothing sounds and the listener's response stands: no rays */
    p_iplSimulatorRunReflections(S.sim);
    EnterCriticalSection(&S.lock);
    for (int i = 0; i < UWST_VOICES; i++)
        if (live[i] && S.v[i].gen == gen[i]) S.v[i].simulated = 1;   /* (a newer sound on the voice waits for its own run) */
    if (ear) S.ear_ok = 1, memcpy(S.ear_at, lis, sizeof S.ear_at);
    LeaveCriticalSection(&S.lock);
}

/* --- the sound --- */
/* the direct path's numbers for the voices sounding: Steam Audio's occlusion (rays from a source of D.radius to the
   listener) and transmission (what the surfaces on the straight line let through, by band). Skipped, the last
   numbers kept, while the scene is being changed. */
static void direct_run(const float lis[3], float yaw)
{
    static IPLDeviationModel deviation;              /* (zeroed: Steam Audio's own, the UTD's) */
    IPLSimulationSharedInputs sh;
    int any = 0, asked[UWST_VOICES] = {0};
    unsigned gen[UWST_VOICES];
    if (!TryEnterCriticalSection(&S.scene_lock)) return;
    if (S.tris) {
        /* the paths with it: every third block (32 ms), and at once when a sound has none yet. Measured on level 1
           (work\acoustics\pathing\pathtest.c): a block of seven sounds 2.51 ms with the paths, 2.26 ms without */
        int paths = P.on && S.probes != NULL && !path_held(), due = S.path_tick++ % 3 == 0;
        IPLSimulationFlags fl;
        for (int i = 0; i < UWST_VOICES && paths && !due; i++) due = S.v[i].state == 1 && !S.v[i].path_ok;
        S.path_ran = paths;
        paths = paths && due;
        fl = paths ? IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_PATHING : IPL_SIMULATIONFLAGS_DIRECT;
        for (int i = 0; i < UWST_VOICES; i++) {
            IPLSimulationInputs in;
            float p[3];
            int state;
            EnterCriticalSection(&S.lock);
            state = S.v[i].state, memcpy(p, S.v[i].p, sizeof p), gen[i] = S.v[i].gen;
            LeaveCriticalSection(&S.lock);
            if (state != 1) {
                /* 2026-11-17, the crash on a level's load: a voice not sounding kept the inputs of its last sound,
                   the paths on and the probes of the level it sounded on. Those probes are taken out of the
                   simulator when the level changes, and iplSimulatorRunPathing goes through EVERY source: for a
                   batch no longer in the simulator it has no path simulator and reads through a null one
                   (phonon.dll + 0x27588b5, ProbeVisibilityTester::areProbesVisible; work\crashrepro). So before
                   every run of the paths each silent voice is told: no paths, and the probes in use now. */
                if (paths) {
                    memset(&in, 0, sizeof in);
                    in.flags = IPL_SIMULATIONFLAGS_DIRECT;
                    in.pathingProbes = S.probes, in.pathingOrder = 1, in.deviationModel = &deviation;
                    in.source = space_at(p, 0);
                    p_iplSourceSetInputs(S.v[i].dsrc, IPL_SIMULATIONFLAGS_PATHING, &in);
                }
                continue;
            }
            asked[i] = 1;
            memset(&in, 0, sizeof in);
            in.flags = fl;
            in.directFlags = IPL_DIRECTSIMULATIONFLAGS_OCCLUSION | IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION;
            if (paths) {
                in.pathingProbes = S.probes, in.pathingOrder = 1, in.deviationModel = &deviation;
                in.visRadius = 0.1f * S.tile_m, in.visThreshold = 0.1f, in.visRange = P.see * S.tile_m;
                in.enableValidation = IPL_TRUE, in.findAlternatePaths = IPL_TRUE;   /* (a shut door's leaf: another way) */
            }
            in.source = space_at(p, 0);
            in.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
            in.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
            in.directivity.dipoleWeight = 0, in.directivity.dipolePower = 1;
            in.occlusionType = IPL_OCCLUSIONTYPE_VOLUMETRIC, in.occlusionRadius = D.radius, in.numOcclusionSamples = (int)D.samples;
            in.numTransmissionRays = (int)D.rays;
            for (int k = 0; k < 3; k++) in.reverbScale[k] = 1.0f;
            p_iplSourceSetInputs(S.v[i].dsrc, fl, &in);
            any = 1;
        }
        if (any) {
            memset(&sh, 0, sizeof sh);
            sh.listener = space_at(lis, yaw);
            p_iplSimulatorSetSharedInputs(S.dsim, fl, &sh);
            p_iplSimulatorRunDirect(S.dsim);
            if (paths) p_iplSimulatorRunPathing(S.dsim);
            for (int i = 0; i < UWST_VOICES; i++) {
                IPLSimulationOutputs out;
                /* (2026-11-16, the review: a sound begun on this voice while the run was under way took the numbers
                   of the sound before it, or of no run at all, for a few blocks) */
                if (!asked[i]) continue;
                memset(&out, 0, sizeof out);
                p_iplSourceGetOutputs(S.v[i].dsrc, fl, &out);
                EnterCriticalSection(&S.lock);
                if (S.v[i].state == 1 && S.v[i].gen == gen[i]) {
                    S.v[i].occ = out.direct.occlusion;
                    for (int k = 0; k < 3; k++) S.v[i].trans[k] = out.direct.transmission[k];
                    if (paths) S.v[i].pp = out.pathing, S.v[i].path_ok = 1;
                }
                LeaveCriticalSection(&S.lock);
            }
        }
    }
    LeaveCriticalSection(&S.scene_lock);
}
/* one block: every voice's next samples through its response into the mixer, the mixer decoded into S.out; with the
   sound itself Steam Audio's (D.on, and a listener on headphones: hrtf), every sounding voice's samples through its
   direct and binaural effects into S.dry */
static int block(int hrtf)
{
    IPLReflectionEffectParams mp;
    IPLAmbisonicsDecodeEffectParams dp;
    float lis0[3] = {0, 0, 0}, lis[3], yaw;
    int busy = 0, direct = D.on && hrtf;
    memset(&mp, 0, sizeof mp);
    mp.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, mp.numChannels = 4, mp.irSize = S.irsize;
    EnterCriticalSection(&S.lock);
    yaw = S.yaw, memcpy(lis, S.lis, sizeof lis);
    LeaveCriticalSection(&S.lock);
    memset(S.dry, 0, sizeof S.dry);
    if (direct && D.occl) direct_run(lis, yaw);
    for (int i = 0; i < UWST_VOICES; i++) {
        Voice *v = &S.v[i];
        IPLSimulationOutputs out;
        int state, simulated, fresh, pos, frames, by_ear, floor, n = 0;
        float *pcm, heard[3], gain[3];
        EnterCriticalSection(&S.lock);
        state = v->state, simulated = v->simulated, fresh = v->fresh, pos = v->pos, frames = v->frames, pcm = v->pcm;
        by_ear = !simulated && S.ear_ok && T.ear;   /* its own response not made yet: the listener's place's */
        memcpy(heard, v->heard, sizeof heard), memcpy(gain, v->gain, sizeof gain), floor = v->floor;
        if (v->rel) {   /* fixed to the head: it walks and turns with the listener (ours: yaw 0 looks north, +z) */
            heard[0] = lis[0] + cosf(yaw) * v->rel_right + sinf(yaw) * v->rel_ahead, heard[1] = lis[1];
            heard[2] = lis[2] - sinf(yaw) * v->rel_right + cosf(yaw) * v->rel_ahead;
        }
        v->fresh = 0;
        if (state == 1) {
            n = frames - pos < UWST_FRAME ? frames - pos : UWST_FRAME;
            memset(S.in.data[0], 0, sizeof(float) * UWST_FRAME);
            memset(S.din.data[0], 0, sizeof(float) * UWST_FRAME);
            if (n > 0 && (simulated || by_ear)) {    /* (with no response at all: silence, in step) */
                memcpy(S.in.data[0], pcm + pos, sizeof(float) * (size_t)n);
                if (v->echo != 1.0f) for (int k = 0; k < n; k++) S.in.data[0][k] *= v->echo;
            }
            if (n > 0 && direct) memcpy(S.din.data[0], pcm + pos, sizeof(float) * (size_t)n);
            v->pos = pos + UWST_FRAME;
            if (v->pos >= frames) v->state = 2, v->ring = 0;
        } else if (state == 2) v->ring++;
        LeaveCriticalSection(&S.lock);
        if (!state) continue;
        busy++;
        if (fresh) p_iplReflectionEffectReset(v->fx), p_iplDirectEffectReset(v->dfx), p_iplBinauralEffectReset(v->bfx), p_iplPathEffectReset(v->pfx), v->applied = 0;
        if (state == 1 && direct && n > 0) {         /* the sound itself */
            IPLDirectEffectParams de;
            IPLBinauralEffectParams be;
            /* Steam Audio's paths for this sound (the last direct run made them): our own way round a corner stands
               back, the sound's straight share comes from its own place and the rest along the paths */
            int paths = D.occl && S.path_ran && v->path_ok && v->pp.shCoeffs && !v->rel;
            if (paths) memcpy(heard, v->p, sizeof heard), floor = 0;
            float d[3] = {heard[0] - lis[0], heard[1] - lis[1], heard[2] - lis[2]}, l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            memset(&de, 0, sizeof de), memset(&be, 0, sizeof be);
            de.flags = IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION;
            de.transmissionType = IPL_TRANSMISSIONTYPE_FREQDEPENDENT;
            de.occlusion = 0;                        /* (all of it by the three bands below) */
            for (int k = 0; k < 3; k++) {
                float g = D.occl ? v->occ + (1.0f - v->occ) * v->trans[k] : gain[k];
                if (D.occl && floor && gain[k] > g) g = gain[k];   /* (the way round a corner, ours: never quieter than that) */
                /* (2026-11-16, "the footsteps sound twitchy": a sound fixed to the head is the player's own, placed a
                   third of a tile ahead; facing a wall nearer than that the place is in the rock and the step came
                   39 dB down (work\steptwitch\steptest.c). Nothing stands between the player and their own sound:
                   uwaudio.c's own filters never muffle it either, AC.here.) */
                if (v->rel) g = 1.0f;
                de.transmission[k] = g < 0 ? 0 : g > 1 ? 1 : g;
            }
            p_iplDirectEffectApply(v->dfx, &de, &S.din, &S.dmono);
            /* the direction in the listener's own frame (x right, y up, -z ahead; ours: yaw 0 looks north, +z) */
            if (l > 0.05f) {
                float ax = sinf(yaw), az = cosf(yaw);
                be.direction.x = (d[0] * az - d[2] * ax) / l, be.direction.y = d[1] / l, be.direction.z = -(d[0] * ax + d[2] * az) / l;
                be.spatialBlend = 1.0f;
            } else be.direction.x = 0, be.direction.y = 0, be.direction.z = -1.0f, be.spatialBlend = 0;   /* at the listener: in the head */
            be.interpolation = IPL_HRTFINTERPOLATION_BILINEAR, be.hrtf = S.hrtf;
            p_iplBinauralEffectApply(v->bfx, &be, &S.dmono, &S.bin);
            for (int c = 0; c < 2; c++)
                for (int k = 0; k < UWST_FRAME; k++) S.dry[c][k] += S.bin.data[c][k];
            if (paths) {
                /* The paths' level. Steam Audio's coefficients carry 1 / the way's length in metres (measured:
                   the first is 0.2821 / metres), and the sound's samples already carry the game's own falloff by
                   the straight line (play_effect's, in the gain uwaudio.c starts it with). So a way as long as the
                   straight line plays at that level, and a longer one at straight / way of it: `way` undoes the
                   metres down to that. `share` is what the straight line lost (in plain sight none). The bands'
                   gains never 0 into the effect (4.8.1 gave NaN from then on). */
                IPLPathEffectParams pp = v->pp;
                float a = fabsf(pp.shCoeffs[0]) / 0.2820948f, way = l * S.tile_m, share = (1.0f - v->occ) * P.level;
                if (way < 1.0f) way = 1.0f;
                if (a > 1e-6f && way > 1.0f / a) way = 1.0f / a;
                share = a > 1e-6f && share > 0 ? share * way : 0;
                pp.order = 1, pp.binaural = IPL_TRUE, pp.hrtf = S.hrtf, pp.listener = space_at(lis, yaw), pp.normalizeEQ = IPL_FALSE;
                for (int k = 0; k < 3; k++) pp.eqCoeffs[k] = pp.eqCoeffs[k] < 0.001f ? 0.001f : pp.eqCoeffs[k] > 1.0f ? 1.0f : pp.eqCoeffs[k];
                p_iplPathEffectApply(v->pfx, &pp, &S.din, &S.pbin);
                {
                    float g0 = fresh ? share : v->pgain, dg = (share - g0) / UWST_FRAME;
                    for (int c = 0; c < 2; c++)
                        for (int k = 0; k < UWST_FRAME; k++) S.dry[c][k] += S.pbin.data[c][k] * (g0 + dg * (float)k);
                    v->pgain = share;
                }
            } else v->pgain = 0;
        }
        if (state == 1) {
            if (!simulated && !by_ear) continue;     /* nothing to ring with yet */
            memset(&out, 0, sizeof out);
            p_iplSourceGetOutputs(by_ear ? S.ear : v->src, IPL_SIMULATIONFLAGS_REFLECTIONS, &out);
            if (!out.reflections.ir) continue;
            out.reflections.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, out.reflections.numChannels = 4, out.reflections.irSize = S.irsize;
            p_iplReflectionEffectApply(v->fx, &out.reflections, &S.in, &S.amb, S.mixer);
            v->applied = 1;
        } else if (!v->applied || p_iplReflectionEffectGetTail(v->fx, &S.amb, S.mixer) == IPL_AUDIOEFFECTSTATE_TAILCOMPLETE) {
            /* (a cut sound that never went through the effect has no tail) */
            EnterCriticalSection(&S.lock);
            if (v->state == 2) v->state = 0;         /* rung out: the voice is free */
            LeaveCriticalSection(&S.lock);
        }
    }
    p_iplReflectionMixerApply(S.mixer, &mp, &S.amb);
    memset(&dp, 0, sizeof dp);
    dp.order = 1, dp.hrtf = S.hrtf, dp.orientation = space_at(lis0, yaw), dp.binaural = hrtf ? IPL_TRUE : IPL_FALSE;
    p_iplAmbisonicsDecodeEffectApply(S.decode, &dp, &S.amb, &S.out);
    return busy;
}

/* (2026-11-16, found by the uwr session's review, work\review\agents\tail: the echoes went out as 16-bit samples
   after the level's 0.12, where a footstep's echoes peak at 12 to 93 of 32767: a tail of a few steps' height, fizz
   and ticks as it died, and an end at exact zero well before its time. The mix is float now, whole.) */
static float dry_level = 1.0f;
void uwst_direct_level(float g) { dry_level = g < 0 ? 0 : g; }
int uwst_mix_float(float *stereo, int frames, float wet, int hrtf)
{
    int busy = 0, done = 0;
    if (!S.on) { memset(stereo, 0, sizeof(float) * 2 * (size_t)frames); return 0; }
    while (done < frames) {
        if (S.fifo_at >= S.fifo_n) {
            busy = block(hrtf);
            for (int i = 0; i < UWST_FRAME; i++)
                for (int c = 0; c < 2; c++) S.fifo[2 * i + c] = S.out.data[c][i] * wet + S.dry[c][i] * dry_level;
            S.fifo_at = 0, S.fifo_n = UWST_FRAME;
        }
        int n = S.fifo_n - S.fifo_at < frames - done ? S.fifo_n - S.fifo_at : frames - done;
        memcpy(stereo + 2 * done, S.fifo + 2 * S.fifo_at, sizeof(float) * 2 * (size_t)n);
        S.fifo_at += n, done += n;
    }
    for (int i = 0; i < UWST_VOICES; i++) busy += S.v[i].state != 0;
    return busy;
}
/* the same as 16-bit samples, rounded (the checks) */
int uwst_mix(short *stereo, int frames, float wet, int hrtf)
{
    static float f[UWST_FRAME * 2];
    int busy = 0, done = 0;
    while (done < frames) {
        int n = frames - done < UWST_FRAME ? frames - done : UWST_FRAME;
        busy = uwst_mix_float(f, n, wet, hrtf);
        for (int i = 0; i < 2 * n; i++) {
            float v = f[i] * 32767.0f;
            stereo[2 * done + i] = (short)lrintf(v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v);
        }
        done += n;
    }
    return busy;
}
