/* uwsteam.c: Steam Audio for our acoustics, "Acoustics: High" (uwsteam.h): each sound's echoes from the level's real
   shape, and on headphones the sound itself (binaural, occlusion and transmission, pathing round corners).
   phonon.dll (Steam Audio 4.8.1, Valve, Apache 2.0) is never linked: loaded from the folder given, its functions
   taken by GetProcAddress; phonon.h (port\win32, the SDK's own) gives the types only.

   What runs where:
   - uwst_simulate (a worker, or the checks): the level's triangles made again when the level or a door changed, the
     voices' places given to the simulator, one run of the reflection rays. Everything that changes Steam Audio's
     scene or sources happens here, so nothing changes under a running simulation.
   - uwst_mix (the audio's side): a block of each voice's samples through its convolution with the response last
     simulated for it, all into one mixer, the mixer's first-order ambisonics decoded for the listener's facing.
   - the rest (uwst_level, uwst_listener, uwst_begin, uwst_end) leave word under one lock.
   - the bake of the level's sound paths (bake_worker): a thread of its own, started and reaped by uwst_simulate.

   The locks: S.lock guards the word left for the simulation and every voice's fields the audio's side shares;
   S.scene_lock keeps a change of the scene or of the probes away from a direct run (the audio's side only tries it,
   and skips the run when it is taken). The log takes neither (UWPORT_DIRECT_LOG's lines are written under S.lock).

   The log: lines through rlog with the tag "steam"; the "fx: acoustics high ..." lines tools\fxcheck.py and
   tests\uwsteam_test.c look for stay plain stderr lines, word for word.

   Frames: ours is the renderer's (tiles; x east, y up, z north); Steam Audio's is metres, right-handed, y up, -z
   ahead: (x, y, -z) * tile_m. */
#define WIN32_LEAN_AND_MEAN
#include "uwos.h"   /* windows.h on Windows */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef _WIN32
#include <unistd.h>   /* (sysconf: st_threads) */
#endif
#include "phonon.h"
#include "uwsteam.h"
#include "rlog.h"

#define N 64
#define STEP 0.25f
#define CEIL 4.0f
/* the dials (every number of the sound a setting; uwsteam.h's uwst_length / _scatter / _ear, from settings.txt
   through uwaudio.c). length: the longest ring, seconds (the response's size: taken when
   Steam Audio starts). scatter: our surfaces are rough-hewn, a quarter of a bounce goes every way (a guess, to be
   heard). ear: a response for the listener's own place kept ready, for a new sound's first blocks (below). */
static struct { float length, scatter; volatile LONG ear; } T = {2.0f, 0.25f, 1};
/* The sound itself through Steam Audio (uwst_direct_mode): each voice's own samples go through
   Steam Audio's direct effect (what the level's surfaces between the sound and the listener let through, by band:
   its occlusion rays from a source of some size, its transmission by the materials) and its binaural effect (its
   HRTF, from the direction of the sound), into the same stream as the echoes. on: 0 the sound stays OpenAL's;
   occl: 1 Steam Audio's occlusion and transmission, 0 the gains uwaudio.c gives (ours); radius: the source's size
   in metres for the occlusion's rays; samples / rays: the occlusion's and the transmission's counts. */
static struct { volatile LONG on, occl, samples, rays; float radius; } D = {0, 1, 16, 8, 0.5f};
/* Steam Audio's pathing (uwst_pathing). A probe stands
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
/* (2026-11-19, the sound review: work\soundreview\REPORT-2026-11-19.md) more of Steam Audio, each a row:
   limit: the sounds that have echoes at once (the echoes have slots of their own, UWST_ECHOES, apart from the
   sounds: every sound goes through Steam Audio's sound itself, the newest `limit` of them also through the echoes);
   thread: the muffling's and the paths' sums made by uwst_direct_tick on the caller's thread of its own, not in the
   mixing (block() makes them itself when no tick has come for 100 ms: the hidden runs, the row off); speakers: the
   sound itself on speakers too (Steam Audio's panning, the paths panned); by_mat: a material's own scattering;
   spk: the sound itself's level on speakers against dry_level's (the makeups' ratio, uwaudio.c). order_want /
   head_want: the echoes' order and the head for the next start. */
static struct { volatile LONG limit, thread, speakers, by_mat; volatile unsigned long long last_tick; float spk; } XT = {16, 1, 1, 1, 0, 1.0f};
static int order_want = 1;
static char head_want[512];
static int ch_want = 2;   /* the output's channels for the next start (uwst_channels) */
/* Steam Audio's speaker layout for a channel count (2, 4, 6 or 8) */
static IPLSpeakerLayoutType layout_for(int ch)
{
    return ch == 8 ? IPL_SPEAKERLAYOUTTYPE_SURROUND_7_1 : ch == 6 ? IPL_SPEAKERLAYOUTTYPE_SURROUND_5_1
         : ch == 4 ? IPL_SPEAKERLAYOUTTYPE_QUADRAPHONIC : IPL_SPEAKERLAYOUTTYPE_STEREO;
}
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
    X(iplSimulatorRunDirect)     X(iplProbeBatchCreate) X(iplProbeBatchRelease) X(iplProbeBatchAddProbe) X(iplProbeBatchCommit)     X(iplProbeBatchGetNumProbes) X(iplPathBakerBake) X(iplPathBakerCancelBake)         X(iplSimulatorAddProbeBatch) X(iplSimulatorRemoveProbeBatch) X(iplSimulatorRunPathing)     X(iplPathEffectCreate) X(iplPathEffectRelease) X(iplPathEffectReset) X(iplPathEffectApply) \
    X(iplPanningEffectCreate) X(iplPanningEffectRelease) X(iplPanningEffectReset) X(iplPanningEffectApply)
#define X(n) static __typeof__(n) *p_##n;
FUNCS(X)
#undef X
/* the echoes' rays by Intel's Embree, which phonon carries (iplEmbreeDeviceCreate): Steam Audio's own ray tracer
   takes 70-115 ms of a core for each of the 10 runs a second. Asked for by name apart from
   FUNCS: a phonon without them keeps its own ray tracer, as before. */
static __typeof__(iplEmbreeDeviceCreate) *p_iplEmbreeDeviceCreate;
static __typeof__(iplEmbreeDeviceRelease) *p_iplEmbreeDeviceRelease;
static IPLEmbreeDevice st_embree;
static IPLSceneType st_scene_type = IPL_SCENETYPE_DEFAULT;
static int st_env_int(const char *name, int dflt) { const char *e = getenv(name); return e && *e ? atoi(e) : dflt; }
static volatile LONG st_fast = 1;   /* settings.txt acoustics_embree (uwaudio.c, before uwst_init): the Fast echo rays row */
void uwst_fast_rays(int on) { InterlockedExchange(&st_fast, on != 0); }
/* the echoes' threads: a quarter of the processors' (2 to 8: 8 on 36), or 2 with the row Off;
   UWPORT_FX_SIMTHREADS=N (the checks) any from 1 to 16 */
static int st_threads(void)
{
    int n = 2;
    if (st_fast) {
#ifdef _WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        n = (int)si.dwNumberOfProcessors / 4;
#else
        n = (int)sysconf(_SC_NPROCESSORS_ONLN) / 4;
#endif
        n = n < 2 ? 2 : n > 8 ? 8 : n;
    }
    n = st_env_int("UWPORT_FX_SIMTHREADS", n);
    return n < 1 ? 1 : n > 16 ? 16 : n;
}

/* a sound (2026-11-19: its echoes in an Echo slot of their own, below) */
typedef struct {
    float *pcm;              /* the sound, mono, at the module's rate, its gain in it */
    int frames, pos;
    float p[3];
    int state;               /* 0 free, 1 sounding */
    int slot;                /* its echoes' slot in S.e (-1: none) */
    IPLSource dsrc;          /* in the direct simulator (always added) */
    IPLDirectEffect dfx;
    IPLBinauralEffect bfx;
    IPLPanningEffect nfx;    /* (on speakers: the sound placed by panning, not by the HRTF) */
    IPLPathEffect pfx;       /* the sound as it comes along Steam Audio's paths */
    IPLPathEffectParams pp;  /* ... the last run's (its shCoeffs pointing at sh) */
    float sh[16];            /* ... their directional coefficients, copied from the run (first order: 4) */
    int path_ok;             /* a pathing run has been made for this sound */
    float pgain;             /* the paths' gain at the end of the last block (eased over the next) */
    float heard[3];          /* where the sound is heard from (its place, or the opening its way round comes through) */
    float gain[3];           /* uwaudio.c's gains by band (ours) ... */
    int floor;               /* ... 1: a floor under Steam Audio's; 0: the gains themselves */
    float occ, trans[3];     /* Steam Audio's last: the share not occluded, what the surfaces between let through */
    int rel;                 /* 1: heard from a place fixed to the listener's head (the player's own sounds) ... */
    float rel_right, rel_ahead, rel_below;   /* ... this far to the right, ahead and under the ears, tiles */
    int fresh;               /* 1: a new sound since the last block (the effects' memory to be cleared) */
    unsigned gen;            /* counts the sounds this voice has had */
} Voice;
/* a sound's echoes: fed by its sound while that sounds, then ringing out on their own (the sound's voice may have a
   new sound by then) */
typedef struct {
    IPLSource src;
    IPLReflectionEffect fx;
    float p[3];              /* where its sound was */
    int state;               /* 0 free, 1 fed by its sound, 2 ringing out */
    int voice;               /* the sound feeding it ... */
    unsigned vgen;           /* ... that sound's gen (a newer sound on that voice does not feed it) */
    float echo;              /* the echoes' share of the sound's level (1 unless told: a footstep's) */
    int applied;             /* the audio's own: it has gone through the effect (there is a tail to ring out) */
    int fresh;               /* 1: new since the last block (the effect's memory to be cleared) */
    int ring;                /* blocks it has rung since its sound ended */
    int added, simulated;    /* in the simulator; a response for its place exists */
    unsigned gen;            /* counts the sounds this slot has had */
} Echo;

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
    /* The listener's own place as a source: a sound's echoes would be silent until the rays had run once for it,
       and a run takes 50 to 500 ms by the detail and the voices live, so the echoes would come in late and without
       their head (measured: work\acoustics\twinge). The response of the listener's place
       is kept ready instead (made again when the listener has moved a quarter tile, the level changed, or a sound
       is live), and a new sound rings with it from its first block until its own is made. */
    IPLSimulator dsim;       /* the direct simulator (occlusion, transmission): run by the audio's side every block */
    IPLAudioBuffer din, dmono, bin, pbin;
    IPLProbeBatch probes;    /* the level's, in dsim (changed under scene_lock) */
    int nprobes, path_ran;   /* path_ran: the direct runs make the paths too */
    unsigned path_tick;
    unsigned path_key;       /* the level S.probes were made for (level_key) */
    CRITICAL_SECTION scene_lock;   /* the scene's change against a direct run */
    float dry[UWST_MAXCH][UWST_FRAME];   /* the sounds themselves this block, a row a channel */
    int och;                 /* the output's channels this start (2 stereo, 4, 6 or 8: uwst_channels) */
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
    Echo e[UWST_ECHOES];
    int order, nch;          /* the echoes' ambisonic order this start, its channels ((order + 1)^2) */
    char head[512];          /* the SOFA file this start's head is ("": Steam Audio's own) */
    int hrtf_now;            /* the output the last block was mixed for (1 headphones' HRTF, 0 speakers) */
    float fifo[UWST_FRAME * UWST_MAXCH];
    int fifo_at, fifo_n;
} S;

/* Steam Audio running; the scene's triangles (the checks) */
int uwst_on(void) { return S.on; }
int uwst_triangles(void) { return S.tris; }

/* a point of ours (tiles) in Steam Audio's frame (metres, -z north); a place and facing as its coordinate space */
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
/* a tile's floor height at corner c (a slope's raised side one step up) */
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
/* one triangle of material mat into the mesh (none past its room) */
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
/* the level as triangles: every open tile's floor and ceiling, its walls and risers, the diagonal walls, the doors
   (their frames, a shut one's leaf) and the things standing in it */
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
        /* a door: across the middle of its tile. With frames (uwacoustic.c has why): the slab round the
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
    IPLerror err = IPL_STATUS_SUCCESS;
    if (nm < 1) { RLOG_W("steam", "no materials: the level not made for Steam Audio"); return NULL; }
    if (nm > 32) nm = 32;
    m.cap = N * N * 16;
    m.v = malloc(sizeof *m.v * (size_t)m.cap * 3), m.t = malloc(sizeof *m.t * (size_t)m.cap), m.m = malloc(sizeof *m.m * (size_t)m.cap);
    if (!m.v || !m.t || !m.m) {
        RLOG_E("steam", "out of memory (%zu bytes) for the level's triangles", (sizeof *m.v * 3 + sizeof *m.t + sizeof *m.m) * (size_t)m.cap);
        free(m.v), free(m.t), free(m.m);
        return NULL;
    }
    level_mesh(&m, tiles, ceil_mat, doors, nd);
    for (int i = 0; i < nm; i++) {
        const UwaMaterial *u = uwacmat_get(i);
        for (int b = 0; b < 3; b++) mats[i].absorption[b] = u->absorb[b], mats[i].transmission[b] = u->transmit[b];
        mats[i].scattering = XT.by_mat && u->scatter >= 0 ? u->scatter : T.scatter;   /* (2026-11-19: by material) */
    }
    for (int i = 0; i < m.nt; i++) if (m.m[i] < 0 || m.m[i] >= nm) m.m[i] = 0;
    memset(&ms, 0, sizeof ms);
    ms.numVertices = m.nv, ms.numTriangles = m.nt, ms.numMaterials = nm;
    ms.vertices = m.v, ms.triangles = m.t, ms.materialIndices = m.m, ms.materials = mats;
    if (m.nt <= 0 || (err = p_iplStaticMeshCreate(scene, &ms, &fresh)) != IPL_STATUS_SUCCESS) fresh = NULL;
    if (m.nt > 0 && err != IPL_STATUS_SUCCESS) RLOG_E("steam", "iplStaticMeshCreate failed (%d): the level's sound not made", (int)err);
    if (tris) *tris = m.nt;
    free(m.v), free(m.t), free(m.m);
    return fresh;
}
/* the scene's mesh made again from the tiles and doors given (the simulation's thread) */
static double st_ms(void) { LARGE_INTEGER c, f; QueryPerformanceCounter(&c), QueryPerformanceFrequency(&f); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart; }
static void scene_build(const UwaTile *tiles, int ceil_mat, const UwaDoor *doors, int nd)
{
    int tris = 0;
    double t0 = st_ms(), t1;
    IPLStaticMesh fresh = mesh_make(S.scene, tiles, ceil_mat, doors, nd, &tris);
    if (!fresh) return;
    t1 = st_ms();
    EnterCriticalSection(&S.scene_lock);             /* (never under a direct run: block()) */
    if (S.mesh) p_iplStaticMeshRemove(S.mesh, S.scene), p_iplStaticMeshRelease(&S.mesh);
    S.mesh = fresh;
    p_iplStaticMeshAdd(S.mesh, S.scene);
    p_iplSceneCommit(S.scene);
    p_iplSimulatorCommit(S.dsim);
    S.tris = tris;
    LeaveCriticalSection(&S.scene_lock);
    /* (2026-11-19: what a door's opening costs, the whole level made again; UWR_LOGLEVEL=3) */
    RLOG_D("steam", "the level's sound shape made again: %d triangles, %.1f ms to make, %.1f ms to put in", tris, t1 - t0, st_ms() - t1);
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
/* the bake's thread: a scene of its own with every door open, a probe in every open tile, the ways between them
   baked into B.batch (NULL when it failed or was given up); B.busy 0 at the end */
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
        IPLerror err = p_iplSceneCreate(S.ctx, &ss, &scene);
        if (err != IPL_STATUS_SUCCESS) RLOG_E("steam", "iplSceneCreate failed (%d): no sound paths baked", (int)err);
        if (err == IPL_STATUS_SUCCESS && (mesh = mesh_make(scene, B.tiles, B.ceil_mat, B.doors, B.nd, NULL)) != NULL) {
            p_iplStaticMeshAdd(mesh, scene);
            p_iplSceneCommit(scene);
            if ((err = p_iplProbeBatchCreate(S.ctx, &B.batch)) != IPL_STATUS_SUCCESS) {
                RLOG_E("steam", "iplProbeBatchCreate failed (%d): no sound paths baked", (int)err);
                B.batch = NULL;
            }
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
    /* (the bake is waited out, not cancelled. iplPathBakerCancelBake is not safe in 4.8.1's Linux library:
       called while the bake's jobs run, its pool's threads spin for ever and iplPathBakerBake never comes back
       (work\soundfix\bakequit_linux.c with a simulation thread: the second round hung, the stacks in the library), and
       in the game a quit during the bake ended in a fault in one of those threads. B.cancel still keeps a bake that
       has not reached the library from starting. UWPORT_FX_BAKE_CANCEL=1: the cancel all the same, for a comparison.) */
    {
        const char *e = getenv("UWPORT_FX_BAKE_CANCEL");
        while (B.busy) { if (e && *e == '1') p_iplPathBakerCancelBake(S.ctx); Sleep(5); }
    }
    if (WaitForSingleObject(B.thread, 60000) != WAIT_OBJECT_0) RLOG_W("steam", "the sound paths' bake did not end in a minute");
    CloseHandle(B.thread), B.thread = NULL;
    if (B.batch) p_iplProbeBatchRelease(&B.batch);
    B.batch = NULL, B.key = 0, B.cancel = 0;
}
/* a level change under way (the front end, when it sees one): no pathing until the new level's tiles have come
   (path_service), 8 s at most. The paths run with the old level's probes and the listener already in the new one
   fault inside phonon.dll (a null read, seen going from level 1 back to level 2); Steam Audio's own release notes
   list faults in iplSimulatorRunPathing across a scene's and a probe batch's change */
static volatile LONG path_hold;
static unsigned long long path_hold_t;
void uwst_level_change(void)
{
    path_hold_t = GetTickCount64();
    InterlockedExchange(&path_hold, 1);
}
/* 1 while a level change holds the paths back (the hold given up after 8 s) */
static int path_held(void)
{
    if (path_hold && GetTickCount64() - path_hold_t > 8000) InterlockedExchange(&path_hold, 0);
    return path_hold != 0;
}
/* every source's paths off, where a batch of probes leaves the simulator: a source still naming a batch that has left
   faults iplSimulatorRunPathing (work\steamproof, the same calls without the game; direct_run has the
   other half). The sounding voices are given theirs again by the next direct_run. */
static void paths_forget(void)
{
    for (int i = 0; i < UWST_VOICES; i++) {
        IPLSimulationInputs in;
        memset(&in, 0, sizeof in);
        p_iplSourceSetInputs(S.v[i].dsrc, IPL_SIMULATIONFLAGS_PATHING, &in);
    }
}
/* the simulation's thread, each run: a level whose ways are not the ones in use is given to the bake; a bake that
   has ended has its probes put in the direct simulator in place of the old ones */
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
            RLOG_I("steam", "working out this level's sound paths (Steam Audio's pathing, in the background)");
            if (!(B.thread = CreateThread(NULL, 0, bake_worker, NULL, 0, NULL))) {
                B.busy = 0, B.key = 0;
                RLOG_E("steam", "no thread for the sound paths' bake: none this time");
            }
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
            RLOG_I("steam", "sound paths ready (Steam Audio's pathing: %d probes, baked in %.1f s)", B.n, B.seconds);
        } else RLOG_W("steam", "sound paths: none made for this level (the sounds go on without Steam Audio's pathing)");
        B.batch = NULL, B.key = 0;
    }
}

/* --- start and stop --- */
/* everything of Steam Audio's released (the bake stopped first), the module off; the level's tiles kept */
static void release_all(void)
{
    if (S.ctx) bake_stop();
    for (int i = 0; i < UWST_VOICES; i++) {
        if (S.v[i].dfx) p_iplDirectEffectRelease(&S.v[i].dfx);
        if (S.v[i].bfx) p_iplBinauralEffectRelease(&S.v[i].bfx);
        if (S.v[i].nfx) p_iplPanningEffectRelease(&S.v[i].nfx);
        if (S.v[i].pfx) p_iplPathEffectRelease(&S.v[i].pfx);
        if (S.v[i].dsrc) p_iplSourceRelease(&S.v[i].dsrc);
        free(S.v[i].pcm);
    }
    memset(S.v, 0, sizeof S.v);
    for (int i = 0; i < UWST_VOICES; i++) S.v[i].slot = -1;
    for (int i = 0; i < UWST_ECHOES; i++) {
        if (S.e[i].fx) p_iplReflectionEffectRelease(&S.e[i].fx);
        if (S.e[i].src) p_iplSourceRelease(&S.e[i].src);
    }
    memset(S.e, 0, sizeof S.e);
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
    if (st_embree) p_iplEmbreeDeviceRelease(&st_embree);
    st_embree = NULL;
    if (S.ctx) p_iplContextRelease(&S.ctx);
    /* (the tiles last given are kept: a start again for a new response length builds the same level) */
    S.on = 0, S.tris = 0, S.fifo_n = S.fifo_at = 0, S.level_dirty = 0;
}

/* phonon.dll loaded and Steam Audio started: the context, the scene (Embree's when it can), the reflections'
   simulator, the HRTF, the mixer and decoder, the voices, and the direct simulator with its effects. 0 and one
   "fx: acoustics high off (...)" line when any of it fails */
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
    S.order = order_want == 2 ? 2 : 1, S.nch = (S.order + 1) * (S.order + 1);
    snprintf(S.head, sizeof S.head, "%s", head_want);
    S.och = ch_want;
    if (S.tile_m <= 0.1f) S.tile_m = 2.4f;
    memset(&cs, 0, sizeof cs);
    cs.version = STEAMAUDIO_VERSION, cs.simdLevel = IPL_SIMDLEVEL_AVX2;
    memset(&ss, 0, sizeof ss);
    ss.type = IPL_SCENETYPE_DEFAULT;
    memset(&sim, 0, sizeof sim);
    sim.flags = IPL_SIMULATIONFLAGS_REFLECTIONS, sim.sceneType = IPL_SCENETYPE_DEFAULT, sim.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
    sim.maxNumOcclusionSamples = 16, sim.maxNumRays = 8192, sim.numDiffuseSamples = 32, sim.maxDuration = S.ir_s, sim.maxOrder = S.order;
    sim.maxNumSources = UWST_ECHOES + 1, sim.numThreads = 2, sim.rayBatchSize = 1, sim.numVisSamples = 1;
    sim.numThreads = st_threads();
    sim.samplingRate = rate, sim.frameSize = UWST_FRAME;
    memset(&hs, 0, sizeof hs);
    hs.type = IPL_HRTFTYPE_DEFAULT, hs.volume = 1.0f, hs.normType = IPL_HRTFNORMTYPE_NONE;
    rs.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, rs.irSize = S.irsize, rs.numChannels = S.nch;
    if (p_iplContextCreate(&cs, &S.ctx) != IPL_STATUS_SUCCESS) why = "iplContextCreate failed";
    if (!why && st_fast) {
        if (!p_iplEmbreeDeviceCreate) *(FARPROC *)&p_iplEmbreeDeviceCreate = GetProcAddress(S.dll, "iplEmbreeDeviceCreate");
        if (!p_iplEmbreeDeviceRelease) *(FARPROC *)&p_iplEmbreeDeviceRelease = GetProcAddress(S.dll, "iplEmbreeDeviceRelease");
        st_embree = NULL;
        if (p_iplEmbreeDeviceCreate && p_iplEmbreeDeviceRelease && p_iplEmbreeDeviceCreate(S.ctx, NULL, &st_embree) == IPL_STATUS_SUCCESS && st_embree)
            st_scene_type = IPL_SCENETYPE_EMBREE;
        else
            st_embree = NULL, st_scene_type = IPL_SCENETYPE_DEFAULT, RLOG_W("steam", "acoustics: no Embree (Steam Audio's own ray tracer)");
    } else
        st_scene_type = IPL_SCENETYPE_DEFAULT;
    ss.type = sim.sceneType = st_scene_type, ss.embreeDevice = st_embree;
    if (why) ;
    else if (p_iplSceneCreate(S.ctx, &ss, &S.scene) != IPL_STATUS_SUCCESS) why = "no scene";
    else if (p_iplSimulatorCreate(S.ctx, &sim, &S.sim) != IPL_STATUS_SUCCESS) why = "no simulator";
    else {
        /* (2026-11-19) the head: a SOFA file's when one is asked for (its loudness evened out by RMS, as measured heads
           differ), else, or when it cannot be read, Steam Audio's own */
        if (S.head[0]) {
            IPLHRTFSettings sofa = hs;
            sofa.type = IPL_HRTFTYPE_SOFA, sofa.sofaFileName = S.head, sofa.normType = IPL_HRTFNORMTYPE_RMS;
            IPLerror herr = p_iplHRTFCreate(S.ctx, &au, &sofa, &S.hrtf);
            if (herr != IPL_STATUS_SUCCESS) {
                RLOG_W("steam", "the head %s could not be read (%d): Steam Audio's own head instead", S.head, (int)herr);
                S.hrtf = NULL, S.head[0] = 0;
            }
        }
        if (!S.hrtf && p_iplHRTFCreate(S.ctx, &au, &hs, &S.hrtf) != IPL_STATUS_SUCCESS) why = "no HRTF";
    }
    if (why) ;
    else if (p_iplReflectionMixerCreate(S.ctx, &au, &rs, &S.mixer) != IPL_STATUS_SUCCESS) why = "no reflection mixer";
    if (!why) {
        memset(&ds, 0, sizeof ds);
        ds.speakerLayout.type = layout_for(S.och), ds.hrtf = S.hrtf, ds.maxOrder = S.order;
        if (p_iplAmbisonicsDecodeEffectCreate(S.ctx, &au, &ds, &S.decode) != IPL_STATUS_SUCCESS) why = "no ambisonics decoder";
        else if (p_iplAudioBufferAllocate(S.ctx, 1, UWST_FRAME, &S.in) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, S.nch, UWST_FRAME, &S.amb) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, S.och, UWST_FRAME, &S.out) != IPL_STATUS_SUCCESS) why = "no audio buffers";
    }
    for (int i = 0; i < UWST_ECHOES && !why; i++) {   /* (the echoes' slots) */
        IPLSourceSettings so = {IPL_SIMULATIONFLAGS_REFLECTIONS};
        if (p_iplSourceCreate(S.sim, &so, &S.e[i].src) != IPL_STATUS_SUCCESS) why = "no source";
        else if (p_iplReflectionEffectCreate(S.ctx, &au, &rs, &S.e[i].fx) != IPL_STATUS_SUCCESS) why = "no reflection effect";
    }
    for (int i = 0; i < UWST_VOICES; i++) S.v[i].slot = -1;
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
        IPLPanningEffectSettings pn;
        memset(&pn, 0, sizeof pn);
        pn.speakerLayout.type = layout_for(S.och);
        memset(&pe, 0, sizeof pe);
        pe.maxOrder = 1, pe.spatialize = IPL_TRUE, pe.speakerLayout.type = layout_for(S.och), pe.hrtf = S.hrtf;
        memset(&ds2, 0, sizeof ds2);
        ds2.flags = IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_PATHING, ds2.sceneType = st_scene_type, ds2.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
        ds2.maxNumOcclusionSamples = 64, ds2.maxNumRays = 256, ds2.numDiffuseSamples = 8, ds2.maxDuration = 0.5f, ds2.maxOrder = 1;
        ds2.maxNumSources = UWST_VOICES, ds2.numThreads = 1, ds2.rayBatchSize = 1, ds2.numVisSamples = 1;
        ds2.samplingRate = rate, ds2.frameSize = UWST_FRAME;
        if (p_iplSimulatorCreate(S.ctx, &ds2, &S.dsim) != IPL_STATUS_SUCCESS) why = "no direct simulator";
        else if (p_iplAudioBufferAllocate(S.ctx, 1, UWST_FRAME, &S.din) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, 1, UWST_FRAME, &S.dmono) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, S.och, UWST_FRAME, &S.bin) != IPL_STATUS_SUCCESS ||
                 p_iplAudioBufferAllocate(S.ctx, S.och, UWST_FRAME, &S.pbin) != IPL_STATUS_SUCCESS) why = "no audio buffers";
        for (int i = 0; i < UWST_VOICES && !why; i++) {
            if (p_iplSourceCreate(S.dsim, &so, &S.v[i].dsrc) != IPL_STATUS_SUCCESS) why = "no direct source";
            else if (p_iplDirectEffectCreate(S.ctx, &au, &de, &S.v[i].dfx) != IPL_STATUS_SUCCESS) why = "no direct effect";
            else if (p_iplBinauralEffectCreate(S.ctx, &au, &be, &S.v[i].bfx) != IPL_STATUS_SUCCESS) why = "no binaural effect";
            else if (p_iplPathEffectCreate(S.ctx, &au, &pe, &S.v[i].pfx) != IPL_STATUS_SUCCESS) why = "no path effect";
            else if (p_iplPanningEffectCreate(S.ctx, &au, &pn, &S.v[i].nfx) != IPL_STATUS_SUCCESS) why = "no panning effect";
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
    fprintf(stderr, "fx: acoustics high on (Steam Audio %d.%d.%d, %s's rays, %d threads)\n", STEAMAUDIO_VERSION_MAJOR, STEAMAUDIO_VERSION_MINOR, STEAMAUDIO_VERSION_PATCH,
            st_scene_type == IPL_SCENETYPE_EMBREE ? "Embree" : "Steam Audio", sim.numThreads);
    RLOG_I("steam", "%d sounds, echoes for %d of them at once (order %d, %d channels); the head: %s; out to %s", UWST_VOICES, (int)XT.limit,
           S.order, S.nch, uwst_head_now(), S.och == 8 ? "7.1 speakers" : S.och == 6 ? "5.1 speakers" : S.och == 4 ? "4 speakers" : "stereo");
    return 1;
}

/* Steam Audio stopped and released */
void uwst_shutdown(void)
{
    /* (the bake's thread takes S.lock for the things' boxes (level_mesh); waited for under that lock it could never
       end, and the game would hang on the way out. Stopped first, the lock not held.) */
    if (S.dll && S.ctx) bake_stop();
    if (S.dll && S.lock_made) EnterCriticalSection(&S.lock);
    if (S.dll) release_all();
    if (S.dll && S.lock_made) LeaveCriticalSection(&S.lock);
}

/* --- word left by the game's side (uwsteam.h has each; under S.lock where the simulation reads it) --- */
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
void uwst_echo_voices(int n) { InterlockedExchange(&XT.limit, n < 1 ? 1 : n > UWST_ECHOES ? UWST_ECHOES : n); }
void uwst_direct_thread(int on) { InterlockedExchange(&XT.thread, on != 0); }
void uwst_speakers(int on) { InterlockedExchange(&XT.speakers, on != 0); }
int uwst_direct_for(int hrtf) { return S.on && D.on && (hrtf || XT.speakers); }
void uwst_speaker_level(float ratio) { XT.spk = ratio < 0 ? 0 : ratio > 8.0f ? 8.0f : ratio; }
int uwst_channels(int n)
{
    ch_want = n == 4 || n == 6 || n == 8 ? n : 2;
    return S.on && ch_want != S.och;
}
int uwst_channels_now(void) { return S.on ? S.och : ch_want; }
int uwst_order(int order)
{
    order_want = order == 2 ? 2 : 1;
    return S.on && order_want != S.order;
}
int uwst_head(const char *sofa_path)
{
    snprintf(head_want, sizeof head_want, "%s", sofa_path ? sofa_path : "");
    return S.on && strcmp(head_want, S.head) != 0;
}
int uwst_restart_wanted(void) { return S.on && (order_want != S.order || strcmp(head_want, S.head) != 0); }
const char *uwst_head_now(void)
{
    const char *h = S.on ? S.head : head_want, *b = h;
    for (const char *c = h; *c; c++) if (*c == '\\' || *c == '/') b = c + 1;
    return *b ? b : "Steam Audio's own";
}
void uwst_scatter_by_mat(int on)
{
    if ((on != 0) == (XT.by_mat != 0)) return;
    InterlockedExchange(&XT.by_mat, on != 0);
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
    EnterCriticalSection(&S.lock);
    ok = S.v[voice].path_ok;
    for (int k = 0; k < 3; k++) eq[k] = ok ? S.v[voice].pp.eqCoeffs[k] : 0;
    for (int k = 0; k < 4; k++) sh[k] = ok ? S.v[voice].sh[k] : 0;
    LeaveCriticalSection(&S.lock);
    return ok;
}
void uwst_relative(int voice, float right, float ahead, float below)
{
    if (!S.on || voice < 0 || voice >= UWST_VOICES) return;
    EnterCriticalSection(&S.lock);
    S.v[voice].rel = 1, S.v[voice].rel_right = right, S.v[voice].rel_ahead = ahead, S.v[voice].rel_below = below;
    LeaveCriticalSection(&S.lock);
}
void uwst_echo(int voice, float share)
{
    if (!S.on || voice < 0 || voice >= UWST_VOICES) return;
    EnterCriticalSection(&S.lock);
    {
        int s = S.v[voice].slot;
        if (s >= 0 && S.e[s].voice == voice && S.e[s].vgen == S.v[voice].gen) S.e[s].echo = share < 0 ? 0 : share;
    }
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
/* a sound starts at (x, y, z): its samples to the module's rate with its gain in them, on a free voice; its echoes
   on a free slot of the first XT.limit (else the one rung out longest; none when all are fed); the voice, or -1 */
int uwst_begin(float x, float y, float z, const short *mono, int frames, int rate, float gain)
{
    int k = -1, n;
    float *pcm;
    if (!S.on || !mono || frames <= 0 || rate <= 0) return -1;
    n = (int)((double)frames * S.rate / rate);
    if (n < 1 || !(pcm = malloc(sizeof *pcm * (size_t)n))) {
        if (n >= 1) {
            static int said;
            if (!said++) RLOG_E("steam", "out of memory (%zu bytes): a sound gets no echoes", sizeof *pcm * (size_t)n);
        }
        return -1;
    }
    for (int i = 0; i < n; i++) {   /* to the module's rate, linear; the sound's gain in it */
        double at = (double)i * rate / S.rate;
        int a = (int)at, b = a + 1 < frames ? a + 1 : a;
        float f = (float)(at - a);
        pcm[i] = (mono[a] * (1.0f - f) + mono[b] * f) / 32768.0f * gain;
    }
    EnterCriticalSection(&S.lock);
    for (int i = 0; i < UWST_VOICES && k < 0; i++) if (!S.v[i].state) k = i;
    if (k >= 0) {
        Voice *v = &S.v[k];
        int s = -1, lim = (int)XT.limit;
        free(v->pcm);
        v->pcm = pcm, v->frames = n, v->pos = 0, v->p[0] = x, v->p[1] = y, v->p[2] = z;
        v->state = 1, v->fresh = 1, v->gen++, v->slot = -1;
        v->heard[0] = x, v->heard[1] = y, v->heard[2] = z, v->gain[0] = v->gain[1] = v->gain[2] = 0, v->floor = 1;
        v->occ = 1, v->trans[0] = v->trans[1] = v->trans[2] = 1, v->rel = 0, v->path_ok = 0;
        /* its echoes: a free slot, else the one whose echoes have rung out longest (a walk's footsteps ring some
           3 s each); with every slot fed by a sound, none */
        for (int i = 0; i < lim && s < 0; i++) if (!S.e[i].state) s = i;
        for (int i = 0; i < lim && s < 0; i++) if (S.e[i].state == 2) s = i;
        for (int i = 0; i < lim && s >= 0 && S.e[s].state == 2; i++) if (S.e[i].state == 2 && S.e[i].ring > S.e[s].ring) s = i;
        if (s >= 0) {
            Echo *e = &S.e[s];
            e->state = 1, e->voice = k, e->vgen = v->gen, e->echo = 1, e->fresh = 1, e->simulated = 0, e->ring = 0, e->gen++;
            memcpy(e->p, v->p, sizeof e->p);
            v->slot = s;
        } else {
            static int said;
            if (!said++) RLOG_I("steam", "every echo slot fed by a sound: a new sound plays without echoes (%d slots in use)", lim);
        }
    }
    LeaveCriticalSection(&S.lock);
    if (k < 0) {
        static int said;
        free(pcm);
        if (!said++) RLOG_W("steam", "every voice sounding: a new sound is not Steam Audio's (%d voices)", UWST_VOICES);
    }
    return k;
}
/* a voice's sound over (cut, or at its end; under S.lock): the voice free, its echoes ringing out */
static void voice_over(int i)
{
    Voice *v = &S.v[i];
    if (v->slot >= 0) {
        Echo *e = &S.e[v->slot];
        if (e->voice == i && e->vgen == v->gen && e->state == 1) e->state = 2, e->ring = 0;
    }
    v->state = 0, v->slot = -1;
}
/* the voice's sound cut: its echoes ring out */
void uwst_end(int voice)
{
    if (!S.on || voice < 0 || voice >= UWST_VOICES) return;
    EnterCriticalSection(&S.lock);
    if (S.v[voice].state == 1) voice_over(voice);
    LeaveCriticalSection(&S.lock);
}

/* --- the rays --- */
/* one simulation run (the simulation's thread, or the checks): the word left taken under the lock, the scene made
   again when the level changed, the paths' bake looked after, the live voices' and the listener's places given,
   the reflection rays run, and the voices that got a response marked */
void uwst_simulate(void)
{
    static UwaTile tiles[N * N];
    static UwaDoor doors[128];
    IPLSimulationSharedInputs sh;
    float lis[3], yaw, place[UWST_ECHOES][3];
    unsigned gen[UWST_ECHOES];
    int live[UWST_ECHOES], dirty, nd, ceil_mat, any = 0, ear = (int)T.ear, stale;
    if (!S.on) return;
    EnterCriticalSection(&S.lock);
    dirty = S.level_dirty, S.level_dirty = 0, nd = S.ndoors, ceil_mat = S.ceil_mat;
    if (dirty) memcpy(tiles, S.tiles, sizeof tiles), memcpy(doors, S.doors, sizeof doors);
    memcpy(lis, S.lis, sizeof lis), yaw = S.yaw;
    /* (a voice ringing out leaves the simulation, its tail needs no new response: a walk keeps the two or three
       voices still sounding live, not the six that ring, each adding to every run's time) */
    for (int i = 0; i < UWST_ECHOES; i++) live[i] = S.e[i].state == 1, any |= live[i], gen[i] = S.e[i].gen, memcpy(place[i], S.e[i].p, sizeof place[i]);
    stale = !S.ear_ok || (lis[0] - S.ear_at[0]) * (lis[0] - S.ear_at[0]) + (lis[1] - S.ear_at[1]) * (lis[1] - S.ear_at[1]) +
            (lis[2] - S.ear_at[2]) * (lis[2] - S.ear_at[2]) > 0.0625f;
    LeaveCriticalSection(&S.lock);
    if (dirty) scene_build(tiles, ceil_mat, doors, nd);
    path_service(tiles, doors, nd, ceil_mat, dirty);
    if (dirty) InterlockedExchange(&path_hold, 0);   /* (the new level's tiles are in: its own probes decide from here) */
    if (!S.tris) return;                             /* no level yet: nothing to ring in */
    for (int i = 0; i < UWST_ECHOES; i++) {
        Echo *v = &S.e[i];
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
    sh.numRays = (int)q_rays, sh.numBounces = (int)q_bounces, sh.duration = S.ir_s, sh.order = S.order, sh.irradianceMinDistance = 1.0f;
    p_iplSimulatorSetSharedInputs(S.sim, IPL_SIMULATIONFLAGS_REFLECTIONS, &sh);
    p_iplSimulatorCommit(S.sim);
    if (!any && !(ear && (stale || dirty))) return;  /* nothing sounds and the listener's response stands: no rays */
    p_iplSimulatorRunReflections(S.sim);
    EnterCriticalSection(&S.lock);
    for (int i = 0; i < UWST_ECHOES; i++)
        if (live[i] && S.e[i].gen == gen[i]) S.e[i].simulated = 1;   /* (a newer sound on the slot waits for its own run) */
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
                /* the crash on a level's load: a voice not sounding keeps the inputs of its last sound,
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
                /* (a sound begun on this voice while the run was under way would take the numbers of the sound
                   before it, or of no run at all, for a few blocks) */
                if (!asked[i]) continue;
                memset(&out, 0, sizeof out);
                p_iplSourceGetOutputs(S.v[i].dsrc, fl, &out);
                EnterCriticalSection(&S.lock);
                if (S.v[i].state == 1 && S.v[i].gen == gen[i]) {
                    S.v[i].occ = out.direct.occlusion;
                    for (int k = 0; k < 3; k++) S.v[i].trans[k] = out.direct.transmission[k];
                    if (paths) {   /* (copied: the run's own memory is written again by the next run, on another thread) */
                        S.v[i].pp = out.pathing;
                        if (out.pathing.shCoeffs) memcpy(S.v[i].sh, out.pathing.shCoeffs, sizeof(float) * 4);
                        S.v[i].pp.shCoeffs = S.v[i].sh, S.v[i].path_ok = out.pathing.shCoeffs != NULL;
                    }
                }
                LeaveCriticalSection(&S.lock);
            }
        }
    }
    LeaveCriticalSection(&S.scene_lock);
}
/* the muffling's and the paths' sums, on the caller's thread of its own (2026-11-19: they took 2.3 to 2.5 ms of
   every 10.7 ms block in the mixing, on the thread that must not be late) */
void uwst_direct_tick(void)
{
    float lis[3], yaw;
    int hrtf;
    if (!S.on || !XT.thread) return;
    XT.last_tick = GetTickCount64();
    if (!D.on || !D.occl) return;
    EnterCriticalSection(&S.lock);
    memcpy(lis, S.lis, sizeof lis), yaw = S.yaw, hrtf = S.hrtf_now;
    LeaveCriticalSection(&S.lock);
    if (hrtf || XT.speakers) direct_run(lis, yaw);
}
/* one block: every voice's next samples through its response into the mixer, the mixer decoded into S.out; with the
   sound itself Steam Audio's (D.on, and a listener on headphones: hrtf), every sounding voice's samples through its
   direct and binaural effects into S.dry */
static int block(int hrtf)
{
    IPLReflectionEffectParams mp;
    IPLAmbisonicsDecodeEffectParams dp;
    float lis0[3] = {0, 0, 0}, lis[3], yaw;
    if (S.och != 2) hrtf = 0;                    /* (binaural is for two ears: surround speakers are panned) */
    int busy = 0, direct = D.on && (hrtf || XT.speakers);   /* (2026-11-19: on speakers too, panned) */
    memset(&mp, 0, sizeof mp);
    mp.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, mp.numChannels = S.nch, mp.irSize = S.irsize;
    EnterCriticalSection(&S.lock);
    yaw = S.yaw, memcpy(lis, S.lis, sizeof lis), S.hrtf_now = hrtf;
    LeaveCriticalSection(&S.lock);
    memset(S.dry, 0, sizeof S.dry);
    /* the muffling's and the paths' sums: uwst_direct_tick's, unless no tick has come for 100 ms */
    if (direct && D.occl && (!XT.thread || GetTickCount64() - XT.last_tick > 100)) direct_run(lis, yaw);
    {   /* UWPORT_DIRECT_LOG=1 (the checks, for a sound that cuts out): every
           24 blocks (about a quarter second) the listener and each sounding voice's direct numbers */
        static int dl = -1, tick;
        if (dl < 0) { const char *e = getenv("UWPORT_DIRECT_LOG"); dl = e && *e == '1'; }
        if (dl && tick++ % 24 == 0) {
            EnterCriticalSection(&S.lock);
            for (int i = 0; i < UWST_VOICES; i++)
                if (S.v[i].state == 1)
                    RLOG_I("steam", "listener (%.2f, %.2f, %.2f) voice %d at (%.2f, %.2f, %.2f) occlusion %.3f transmission %.3f %.3f %.3f gain %.3f %.3f %.3f",
                            lis[0], lis[1], lis[2], i, S.v[i].p[0], S.v[i].p[1], S.v[i].p[2], S.v[i].occ, S.v[i].trans[0], S.v[i].trans[1], S.v[i].trans[2],
                            S.v[i].gain[0], S.v[i].gain[1], S.v[i].gain[2]);
            LeaveCriticalSection(&S.lock);
        }
    }
    /* the echoes: each slot fed by its sound's next samples while that sounds (before the sounds below move on),
       then its tail */
    for (int i = 0; i < UWST_ECHOES; i++) {
        Echo *e = &S.e[i];
        IPLSimulationOutputs out;
        int state, simulated, fresh, by_ear;
        EnterCriticalSection(&S.lock);
        state = e->state, simulated = e->simulated, fresh = e->fresh, e->fresh = 0;
        by_ear = !simulated && S.ear_ok && T.ear;   /* its own response not made yet: the listener's place's */
        if (state == 1) {
            const Voice *v = &S.v[e->voice];
            memset(S.in.data[0], 0, sizeof(float) * UWST_FRAME);
            if (v->state == 1 && v->gen == e->vgen) {
                int n = v->frames - v->pos < UWST_FRAME ? v->frames - v->pos : UWST_FRAME;
                if (n > 0 && (simulated || by_ear)) {    /* (with no response at all: silence, in step) */
                    memcpy(S.in.data[0], v->pcm + v->pos, sizeof(float) * (size_t)n);
                    if (e->echo != 1.0f) for (int k = 0; k < n; k++) S.in.data[0][k] *= e->echo;
                }
                if (v->pos + UWST_FRAME >= v->frames) e->state = 2, e->ring = 0;   /* (this block has its last) */
            } else e->state = 2, e->ring = 0;           /* (its sound was cut) */
        } else if (state == 2) e->ring++;
        LeaveCriticalSection(&S.lock);
        if (!state) continue;
        busy++;
        if (fresh) p_iplReflectionEffectReset(e->fx), e->applied = 0;
        if (state == 1) {
            if (!simulated && !by_ear) continue;     /* nothing to ring with yet */
            memset(&out, 0, sizeof out);
            p_iplSourceGetOutputs(by_ear ? S.ear : e->src, IPL_SIMULATIONFLAGS_REFLECTIONS, &out);
            if (!out.reflections.ir) continue;
            out.reflections.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, out.reflections.numChannels = S.nch, out.reflections.irSize = S.irsize;
            p_iplReflectionEffectApply(e->fx, &out.reflections, &S.in, &S.amb, S.mixer);
            e->applied = 1;
        } else if (!e->applied || p_iplReflectionEffectGetTail(e->fx, &S.amb, S.mixer) == IPL_AUDIOEFFECTSTATE_TAILCOMPLETE) {
            /* (a cut sound that never went through the effect has no tail) */
            EnterCriticalSection(&S.lock);
            if (e->state == 2) e->state = 0;         /* rung out: the slot is free */
            LeaveCriticalSection(&S.lock);
        }
    }
    /* the sounds themselves */
    for (int i = 0; i < UWST_VOICES; i++) {
        Voice *v = &S.v[i];
        int state, fresh, floor, rel, path_ok, n = 0;
        float heard[3], gain[3], p[3], occ, trans[3], sh[16];
        IPLPathEffectParams pp;
        EnterCriticalSection(&S.lock);
        state = v->state, fresh = v->fresh, v->fresh = 0;
        if (state == 1) {
            n = v->frames - v->pos < UWST_FRAME ? v->frames - v->pos : UWST_FRAME;
            memset(S.din.data[0], 0, sizeof(float) * UWST_FRAME);
            if (n > 0 && direct) memcpy(S.din.data[0], v->pcm + v->pos, sizeof(float) * (size_t)n);
            memcpy(heard, v->heard, sizeof heard), memcpy(gain, v->gain, sizeof gain), memcpy(p, v->p, sizeof p), floor = v->floor;
            occ = v->occ, memcpy(trans, v->trans, sizeof trans), rel = v->rel, path_ok = v->path_ok;
            pp = v->pp, memcpy(sh, v->sh, sizeof sh), pp.shCoeffs = sh;
            if (rel) {   /* fixed to the head: it walks and turns with the listener (ours: yaw 0 looks north, +z) */
                heard[0] = lis[0] + cosf(yaw) * v->rel_right + sinf(yaw) * v->rel_ahead, heard[1] = lis[1] - v->rel_below;
                heard[2] = lis[2] - sinf(yaw) * v->rel_right + cosf(yaw) * v->rel_ahead;
            }
            v->pos += UWST_FRAME;
            if (v->pos >= v->frames) voice_over(i);
        }
        LeaveCriticalSection(&S.lock);
        if (state != 1) continue;
        busy++;
        if (fresh) p_iplDirectEffectReset(v->dfx), p_iplBinauralEffectReset(v->bfx), p_iplPanningEffectReset(v->nfx), p_iplPathEffectReset(v->pfx);
        if (direct && n > 0) {                       /* the sound itself */
            IPLDirectEffectParams de;
            /* Steam Audio's paths for this sound (the last direct run made them): our own way round a corner stands
               back, the sound's straight share comes from its own place and the rest along the paths */
            int paths = D.occl && S.path_ran && path_ok && !rel;
            if (paths) memcpy(heard, p, sizeof heard), floor = 0;
            float d[3] = {heard[0] - lis[0], heard[1] - lis[1], heard[2] - lis[2]}, l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            float lvl = hrtf ? 1.0f : XT.spk;        /* (on speakers: the makeups' ratio, uwaudio.c) */
            memset(&de, 0, sizeof de);
            de.flags = IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION;
            de.transmissionType = IPL_TRANSMISSIONTYPE_FREQDEPENDENT;
            de.occlusion = 0;                        /* (all of it by the three bands below) */
            for (int k = 0; k < 3; k++) {
                float g = D.occl ? occ + (1.0f - occ) * trans[k] : gain[k];
                if (D.occl && floor && gain[k] > g) g = gain[k];   /* (the way round a corner, ours: never quieter than that) */
                /* (a sound fixed to the head is the player's own, placed a third of a tile ahead; facing a wall
                   nearer than that the place is in the rock and the step would come 39 dB down
                   (work\steptwitch\steptest.c). Nothing stands between the player and their own sound:
                   uwaudio.c's own filters never muffle it either, AC.here.) */
                if (rel) g = 1.0f;
                de.transmission[k] = g < 0 ? 0 : g > 1 ? 1 : g;
            }
            p_iplDirectEffectApply(v->dfx, &de, &S.din, &S.dmono);
            /* the direction in the listener's own frame (x right, y up, -z ahead; ours: yaw 0 looks north, +z) */
            {
                IPLVector3 dir = {0, 0, -1.0f};
                float blend = 0;
                if (l > 0.05f) {
                    float ax = sinf(yaw), az = cosf(yaw);
                    dir.x = (d[0] * az - d[2] * ax) / l, dir.y = d[1] / l, dir.z = -(d[0] * ax + d[2] * az) / l;
                    blend = 1.0f;
                }
                if (hrtf) {
                    IPLBinauralEffectParams be;
                    memset(&be, 0, sizeof be);
                    be.direction = dir, be.spatialBlend = blend;   /* (at the listener, blend 0: in the head) */
                    be.interpolation = IPL_HRTFINTERPOLATION_BILINEAR, be.hrtf = S.hrtf;
                    p_iplBinauralEffectApply(v->bfx, &be, &S.dmono, &S.bin);
                } else {   /* speakers: panned (at the listener: the middle) */
                    IPLPanningEffectParams pe;
                    if (blend == 0) dir.x = 0, dir.y = 0, dir.z = -1.0f;
                    pe.direction = dir;
                    p_iplPanningEffectApply(v->nfx, &pe, &S.dmono, &S.bin);
                }
            }
            for (int c = 0; c < S.och; c++)
                for (int k = 0; k < UWST_FRAME; k++) S.dry[c][k] += S.bin.data[c][k] * lvl;
            if (paths) {
                /* The paths' level. Steam Audio's coefficients carry 1 / the way's length in metres (measured:
                   the first is 0.2821 / metres), and the sound's samples already carry the game's own falloff by
                   the straight line (play_effect's, in the gain uwaudio.c starts it with). So a way as long as the
                   straight line plays at that level, and a longer one at straight / way of it: `way` undoes the
                   metres down to that. `share` is what the straight line lost (in plain sight none). The bands'
                   gains never 0 into the effect (4.8.1 gave NaN from then on). */
                float a = fabsf(sh[0]) / 0.2820948f, way = l * S.tile_m, share = (1.0f - occ) * P.level;
                if (way < 1.0f) way = 1.0f;
                if (a > 1e-6f && way > 1.0f / a) way = 1.0f / a;
                share = a > 1e-6f && share > 0 ? share * way * lvl : 0;
                pp.order = 1, pp.binaural = hrtf ? IPL_TRUE : IPL_FALSE, pp.hrtf = S.hrtf, pp.listener = space_at(lis, yaw), pp.normalizeEQ = IPL_FALSE;
                for (int k = 0; k < 3; k++) pp.eqCoeffs[k] = pp.eqCoeffs[k] < 0.001f ? 0.001f : pp.eqCoeffs[k] > 1.0f ? 1.0f : pp.eqCoeffs[k];
                p_iplPathEffectApply(v->pfx, &pp, &S.din, &S.pbin);
                {
                    float g0 = fresh ? share : v->pgain, dg = (share - g0) / UWST_FRAME;
                    for (int c = 0; c < S.och; c++)
                        for (int k = 0; k < UWST_FRAME; k++) S.dry[c][k] += S.pbin.data[c][k] * (g0 + dg * (float)k);
                    v->pgain = share;
                }
            } else v->pgain = 0;
        }
    }
    p_iplReflectionMixerApply(S.mixer, &mp, &S.amb);
    memset(&dp, 0, sizeof dp);
    dp.order = S.order, dp.hrtf = S.hrtf, dp.orientation = space_at(lis0, yaw), dp.binaural = hrtf ? IPL_TRUE : IPL_FALSE;
    p_iplAmbisonicsDecodeEffectApply(S.decode, &dp, &S.amb, &S.out);
    return busy;
}

/* (the mix is float, whole: as 16-bit samples after the echo level a footstep's echoes peak at 12 to 93 of 32767,
   a tail of a few steps' height, fizz and ticks as it dies, and an end at exact zero well before its time:
   work\review\agents\tail) */
static float dry_level = 1.0f;   /* the sound itself against the stream (uwst_direct_level) */
void uwst_direct_level(float g) { dry_level = g < 0 ? 0 : g; }
/* frames of the stream (float, interleaved, uwst_channels_now() channels): blocks made as needed, the echoes at
   `wet` and the sound itself at dry_level; the voices still sounding or ringing */
int uwst_mix_float(float *out, int frames, float wet, int hrtf)
{
    int busy = 0, done = 0, ch = uwst_channels_now();
    if (!S.on) { memset(out, 0, sizeof(float) * (size_t)ch * (size_t)frames); return 0; }
    while (done < frames) {
        if (S.fifo_at >= S.fifo_n) {
            busy = block(hrtf);
            for (int i = 0; i < UWST_FRAME; i++)
                for (int c = 0; c < ch; c++) S.fifo[ch * i + c] = S.out.data[c][i] * wet + S.dry[c][i] * dry_level;
            S.fifo_at = 0, S.fifo_n = UWST_FRAME;
        }
        int n = S.fifo_n - S.fifo_at < frames - done ? S.fifo_n - S.fifo_at : frames - done;
        memcpy(out + ch * done, S.fifo + ch * S.fifo_at, sizeof(float) * (size_t)ch * (size_t)n);
        S.fifo_at += n, done += n;
    }
    for (int i = 0; i < UWST_VOICES; i++) busy += S.v[i].state != 0;
    for (int i = 0; i < UWST_ECHOES; i++) busy += S.e[i].state != 0;
    return busy;
}
/* the same as 16-bit samples, rounded (the checks) */
int uwst_mix(short *out, int frames, float wet, int hrtf)
{
    static float f[UWST_FRAME * UWST_MAXCH];
    int busy = 0, done = 0, ch = uwst_channels_now();
    while (done < frames) {
        int n = frames - done < UWST_FRAME ? frames - done : UWST_FRAME;
        busy = uwst_mix_float(f, n, wet, hrtf);
        for (int i = 0; i < ch * n; i++) {
            float v = f[i] * 32767.0f;
            out[ch * done + i] = (short)lrintf(v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v);
        }
        done += n;
    }
    return busy;
}
