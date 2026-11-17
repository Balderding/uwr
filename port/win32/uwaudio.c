/* uwaudio.c: our positional sound effects over OpenAL Soft (uwaudio.h; docs\superpowers\plans\2026-11-12-fx-audio-
   and-visual.md, Task 1). OpenAL Soft (LGPL 2.1) is never linked: soft_oal.dll is loaded from the folder given (the
   game's bin\) when fx_audio is 1, its functions taken by GetProcAddress; without it, or without a sound device, the
   module stays off with one log line and the game sounds as it always has.

   What plays is the game's choice: an effect event (fxevent.c, from SOUND.C's play_effect / play_effect_here) plays
   the effect at the place the game gave, with the original's own falloff (play_effect: the effect's full volume
   within a tile, nothing beyond six tiles, linear between, which is OpenAL's AL_LINEAR_DISTANCE_CLAMPED with
   reference 1 and maximum 6); nothing beyond six tiles, as the original plays nothing there; nothing when the game's
   effects are off (FX_MUTED). 32 sources, the listener at the camera.

   The sound is the Roland's own (2026-11-13): each SOUNDS.DAT effect (timbre of bank 1 from SOUND\UW.MT, note,
   velocity, length) is rendered once by a Munt of our own (mt32emu.dll beside the game, the user's CM-32L ROMs in
   roms\; set up as UW's MT32MPU.ADV driver sets the unit up) into a mono OpenAL buffer, on a worker thread at start;
   OpenAL then places it. While an effect's buffer exists the game's own copy on the music driver is not played at all
   (SOUND.C fx_play through port_fx_control: uwaudio_taken; no note is sent, no channel taken from the music), so
   nothing plays twice; before it is rendered, or when
   Munt is not there, the effect plays as it always has. A data\sfx\NNN.wav (NNN the entry, three digits) still
   takes precedence when present (a pack of the user's own).

   Coordinates: ours are the renderer's (tiles; x east, y up, z north, a left-handed frame); OpenAL's are right-handed,
   so z is negated on the way in (the listener's right, at x up, then comes out east).

   The checks (hidden runs, nothing heard on the machine): UWPORT_FX_LOOPBACK=FILE renders through OpenAL Soft's
   loopback device into that WAV (48 kHz stereo) as the run goes (uwaudio_pump), so a run can prove where a sound
   came out. */
#define WIN32_LEAN_AND_MEAN
#include "uwos.h"   /* windows.h on Windows */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "uwaudio.h"
#include "uwsteam.h"

#ifndef FX_MUTED
#define FX_MUTED 0x100
#endif

/* OpenAL's names (al.h, alc.h, alext.h of OpenAL Soft 1.25.2: the values only, so nothing of it is needed to build) */
#define AL_SOURCE_RELATIVE 0x202
#define AL_POSITION 0x1004
#define AL_LOOPING 0x1007
#define AL_BUFFER 0x1009
#define AL_GAIN 0x100A
#define AL_ORIENTATION 0x100F
#define AL_SOURCE_STATE 0x1010
#define AL_PLAYING 0x1012
#define AL_REFERENCE_DISTANCE 0x1020
#define AL_ROLLOFF_FACTOR 0x1021
#define AL_MAX_DISTANCE 0x1023
#define AL_FORMAT_MONO8 0x1100
#define AL_FORMAT_MONO16 0x1101
#define AL_FORMAT_STEREO8 0x1102
#define AL_FORMAT_STEREO16 0x1103
#define AL_LINEAR_DISTANCE_CLAMPED 0xD004
#define ALC_FREQUENCY 0x1007
#define ALC_FORMAT_CHANNELS_SOFT 0x1990
#define ALC_FORMAT_TYPE_SOFT 0x1991
#define ALC_SHORT_SOFT 0x1402
#define ALC_STEREO_SOFT 0x1501
/* ALC_SOFT_HRTF: the head-related transfer function (front / back / above told apart on headphones, not left-right
   alone). Asked for outright (data\settings.txt fx_hrtf 1, the default): left to itself OpenAL Soft turns it on only
   when Windows reports headphones, so on most machines the effects would stay plain pan. fx_hrtf 0: off; 2: OpenAL's
   own choice. UWPORT_FX_HRTF for the checks. */
#define ALC_HRTF_SOFT 0x1992
#define ALC_HRTF_STATUS_SOFT 0x1993
#define ALC_HRTF_SPECIFIER_SOFT 0x1995
#define ALC_DONT_CARE_SOFT 0x0002
#define ALC_HRTF_DISABLED_SOFT 0x0000
#define ALC_HRTF_ENABLED_SOFT 0x0001

typedef void *(__cdecl *PFopen)(const char *);
typedef void *(__cdecl *PFctx)(void *, const int *);
typedef char (__cdecl *PFcur)(void *);
typedef void (__cdecl *PFvctx)(void *);
typedef char (__cdecl *PFclose)(void *);
typedef int (__cdecl *PFerr)(void);
typedef void (__cdecl *PFgen)(int, unsigned *);
typedef void (__cdecl *PFbufdata)(unsigned, int, const void *, int, int);
typedef void (__cdecl *PFsrci)(unsigned, int, int);
typedef void (__cdecl *PFsrcf)(unsigned, int, float);
typedef void (__cdecl *PFsrc3f)(unsigned, int, float, float, float);
typedef void (__cdecl *PFsrc)(unsigned);
typedef void (__cdecl *PFgetsrci)(unsigned, int, int *);
typedef void (__cdecl *PFl3f)(int, float, float, float);
typedef void (__cdecl *PFlfv)(int, const float *);
typedef void (__cdecl *PFlf)(int, float);
typedef void (__cdecl *PFdm)(int);
typedef void *(__cdecl *PFgetproc)(void *, const char *);
typedef char (__cdecl *PFisext)(void *, const char *);
typedef void (__cdecl *PFrender)(void *, void *, int);
typedef void (__cdecl *PFgetiv)(void *, int, int, int *);
typedef const char *(__cdecl *PFgetstr)(void *, int);

static struct {
    HMODULE dll;
    PFopen alcOpenDevice;
    PFctx alcCreateContext;
    PFcur alcMakeContextCurrent;
    PFvctx alcDestroyContext;
    PFclose alcCloseDevice;
    PFgetproc alcGetProcAddress;
    PFisext alcIsExtensionPresent;
    PFgetiv alcGetIntegerv;
    PFgetstr alcGetString;
    PFerr alGetError;
    PFgen alGenSources, alGenBuffers, alDeleteSources, alDeleteBuffers;
    PFbufdata alBufferData;
    PFsrci alSourcei;
    PFsrcf alSourcef;
    PFsrc3f alSource3f;
    PFsrc alSourcePlay, alSourceStop;
    PFgetsrci alGetSourcei;
    PFl3f alListener3f;
    PFlfv alListenerfv;
    PFlf alListenerf;
    PFdm alDistanceModel;
    PFrender render;              /* alcRenderSamplesSOFT (the loopback device: the checks) */
    void *(__cdecl *alGetProcAddress)(const char *);   /* (optional: the EFX functions' way in) */
    void (__cdecl *alSource3i)(unsigned, int, int, int, int);
    void (__cdecl *alSourceQueueBuffers)(unsigned, int, const unsigned *);     /* (optional: Acoustics High's stream) */
    void (__cdecl *alSourceUnqueueBuffers)(unsigned, int, unsigned *);
} A;

#define AL_MAX_GAIN 0x100E
#define FX_STOP 8                          /* portgame.h's: the game silenced the effect (kill_effect) */
#define NSRC 32
static void *dev, *ctx;
static int on;
static int hrtf_status = -1;       /* ALC_HRTF_STATUS_SOFT of the context made (-1: no context) */
static char hrtf_name[128];        /* ALC_HRTF_SPECIFIER_SOFT: the data set in use ("" none) */
static unsigned src[NSRC], src_used[NSRC], src_clock;
static int src_fx[NSRC];           /* the effect each source plays (-1 none), for FX_STOP */
static unsigned buf[256];          /* the samples, by SOUNDS.DAT entry: 0 not read yet */
static signed char buf_state[256]; /* 0 not tried, 1 a wav, 2 Munt's rendering, -1 no file and nothing rendered yet */
static char sfx_dir[MAX_PATH];
/* Task 6, footsteps by the floor (fx_footsteps, on unless written 0; the Esc menu's "Footsteps by floor"): the game's
   two footstep effects (1 and 2: the two feet, alternating, of the player and of a creature alike) sound by what the floor there is made of
   (uwacmat.c's presets through the level the backend hands over): a data\sfx\NNN-<preset>.wav of the user's when
   there is one, else the Roland's own note moved by the preset's `step` semitones (a rendering of its own, asked of
   the worker as the velocities are; a floor that takes more than half the high band, a carpet, at half the
   velocity). Stone has step 0: the original's sound, untouched. When and whether a step sounds stays the game's. */
#define STEP_PRESETS 16                      /* the first sixteen presets can colour a step */
#define STEP_SLOT(fx, mat) (32 + ((fx) - 1) * STEP_PRESETS + (mat))   /* its renderings' place in rend[]: 32-63 */
static int footsteps = 1;
/* a first-play guess of a velocity's level from the base rendering's, until its own is made: the Roland's level goes
   about as the cube of the velocity (uwaudio_test's effect 4: 72 -> 609, 0x7F -> 3603) */
#define VEL_GAIN 3.0f
/* the pan of an effect played at the player (the two feet) as a share of the way to a side: 1 is the game's own
   (0x48 / 0x38: an eighth to a side, barely heard over headphones); data\settings.txt fx_feet_spread in percent
   (400: half way to a side; 0: the middle), UWPORT_FX_FEET for the checks */
static float feet_spread = 1.0f;
/* the hearing range, tiles (the user, 2026-11-16; settings.txt fx_hear_range, the Esc menu's "Hearing range"): 6 is
   play_effect's own (nothing beyond dist 0x30); a longer one stretches its line: full within a tile, falling to
   nothing at the range. The walls' muffling and the way round corners go on as they are. */
static int hear_range = 6;
/* the footsteps' own level and their echoes' (the user, 2026-11-16: "a bit too loud, reverby"; settings.txt
   fx_step_volume and fx_step_echo, percentages, 80 and 50 unless written): effects 1 and 2 of anyone who steps. The
   echo's share is Acoustics High's (the voice's gain in uwsteam.c); the level scales both. */
static float step_vol = 0.8f, step_echo = 0.5f;
static int step_plain;             /* fx_step_plain: the player's own steps as the plain rendering */
#define AL_DIRECT_CHANNELS_SOFT_ 0x1033
static unsigned step_buf[2 * STEP_PRESETS];  /* the user's NNN-<preset>.wav, by (effect - 1) * 16 + preset */
static signed char step_state[2 * STEP_PRESETS];   /* 0 not tried, 1 there, -1 none */
static unsigned long long wav_mask;   /* bit n: effect n has a data\sfx\NNN.wav of the user's */
static volatile LONG stand_down;   /* uwaudio_stand_down: the game's device is not the Roland */
static int wav_load(const char *path, unsigned *out);
static float lis[3];               /* the listener (ours) */
static float lis_yaw;              /* its facing: 0 north, east positive (radians) */
static float gain = 1.0f;          /* the menu's Sound volume */
/* our copy brought up to the game's own Roland mix (2026-11-15, the user: "so quiet, like the volume at 10%", once
   90da6b4 stopped the Roland copy that had covered it). A sound at the listener measured against the game's own
   mix (work\acoustics\levelcmp.py + levelsum.py, the same walk's footsteps): 2.15 x under it with plain stereo (the
   pan's spread over two ears), 5.2 x with Headphone 3D (the HRTF's own loss on top). On the listener's gain, so the
   reverb and Acoustics High's echoes keep their balance with the sound. */
#define MAKEUP_STEREO 2.15f
#define MAKEUP_HRTF 5.2f
/* The sound's dials (2026-11-16, the user: "make every option for the sound system an option in the Esc menu ... so
   we can fine tune the reverb, time to hear the sound etc"): every number that was fixed in the code is a key of
   settings.txt with a row of the Esc menu (uwfront.c's SOUND_DIALS: the same keys and values as they come), read
   by tune_read at the start and whenever a row changes, heard at once. As they come, each is the number it was.
     fx_makeup_stereo 215, fx_makeup_hrtf 520   the makeup gains, percent
     acoustics_echo_level 5      Acoustics High: Steam Audio's echoes against the sound itself, percent (HI_LEVEL).
                                 12 until 2026-11-16: set when the stream still went through OpenAL's HRTF a second
                                 time; straight to the ears the same 12 came out 2.6 times as strong (a footstep's
                                 echoes at RMS 327 to 380 against the step's own 342 to 403, nearly all under 300 Hz:
                                 work\acoustics\steps16). 5 puts them back where 12 was measured.
     fx_step_plain 0             1: the player's own footsteps as the plain Roland rendering: no floor's note, no
                                 echoes, no placing by HRTF, no muffling (the row to compare by ear)
     acoustics_echo_first 1      High: a new sound rings at once, with the listener's place's response (uwsteam.h uwst_ear)
     acoustics_echo_ms 2000      High: the longest ring (Steam Audio started again when it changes)
     acoustics_scatter 25        High: the share of a bounce that goes every way, percent
     acoustics_bounces 0         High: the bounces a ray is followed for; 0: as Echo detail says (8 / 16 / 32 / 64)
     acoustics_decay 100, acoustics_decay_hf 100, acoustics_decay_lf 100     On: the reverb's ring time and its high and
                                 low notes' share of it, percent of what the rays measure
     acoustics_refl_delay 100, acoustics_late_delay 100     On: the first echoes' and the late ring's delay, percent
     acoustics_reverb_gain 32    On: the reverb's own gain, percent
     acoustics_ease_ms 170       On: the reverb's time constant on the way to a new room (about a third of the change)
     acoustics_filter_ms 300     the muffling's time constant on the way to a new path
     acoustics_room_rays 64, acoustics_room_legs 12, acoustics_room_reach 16     the room probe (uwacoustic.h uwac_tune)
     acoustics_way_reach 12      the way round a corner, tiles
     acoustics_echo_ahead 4      High: blocks of echoes (10.7 ms each) kept queued ahead; 2 until 2026-11-16
     acoustics_echo_direct 1     High: the echoes' stream straight to the two ears (0: through OpenAL's own
                                 placing again, as until 2026-11-16)
     acoustics_steam_direct 1    High, on headphones: the sound itself by Steam Audio (its binaural rendering), our
                                 OpenAL copy of it silent
     acoustics_steam_occlusion 1 ... and what the level lets through by Steam Audio's occlusion and transmission
                                 (0: by our rays)
     acoustics_direct_level 30   that sound's level, percent. Worked out, not heard: Steam Audio's HRTF measures at
                                 unity (work\acoustics\twinge\direct.c), the stream is played at the makeup's 5.2,
                                 and the game's own mix is 1.55 of a unity sound (750 against 482)
     acoustics_source_cm 50      the size of a sound's source for the occlusion's rays
     acoustics_steam_pathing 1   High, with the two above: a sound the level hides is heard along the ways Steam
                                 Audio's pathing finds through the level (uwsteam.h uwst_pathing), in place of our
                                 own way round a corner (acoustics_detour) for the sounds Steam Audio plays
     acoustics_path_level 100    those ways' level, percent of what the straight line lost */
static struct {
    float makeup_stereo, makeup_hrtf, hi_level, decay, decay_hf, decay_lf, refl_delay, late_delay, reverb_gain, reverb_tc, filter_tc;
    int bounces, ahead, direct;
    int sdirect, socc;            /* the sound itself by Steam Audio; its occlusion and transmission (else ours) */
    float dlevel, src_m;          /* that sound's level against the stream's; the source's size, metres */
    int spath;                    /* Steam Audio's pathing ... */
    float plevel;                 /* ... and its level */
} TU = {MAKEUP_STEREO, MAKEUP_HRTF, 0.05f, 1, 1, 1, 1, 1, 0.32f, 0.17f, 0.3f, 0, 4, 1, 1, 1, 0.30f, 0.5f, 1, 1.0f};
/* UWPORT_FX_MAKEUP: the checks' own factor (tests\uwaudio_test.c's levels and reference were taken at 1) */
static float makeup(void)
{
    const char *e = getenv("UWPORT_FX_MAKEUP");
    if (e && *e) return (float)atof(e);
    return hrtf_status == 1 || hrtf_status == 4 ? TU.makeup_hrtf : TU.makeup_stereo;
}
static float listener_gain(void) { return gain * makeup(); }
static FILE *lbfile;               /* UWPORT_FX_LOOPBACK: the WAV the loopback device is rendered into */
static unsigned lbframes, lbrate;
static LARGE_INTEGER lbtime, lbfreq;
static char bin_dir[MAX_PATH];     /* where soft_oal.dll (and phonon.dll) are */
/* the samples behind our OpenAL buffers (mono, 16 bits), by the buffer's name: Acoustics High (Task 7) feeds a sound's
   own samples to Steam Audio's reflections. own: the copy is ours to free (a wav's; a rendering's is the Rend's). */
/* (2026-11-16, the review: the table was 512 long and a rendering past it silently got no echoes and no placing by
   Steam Audio; every new velocity is a rendering, so a long session reached it. It grows now.) */
static struct PcmOf { unsigned al; const short *pcm; int frames, rate, own; } *pcm_of;
static int npcm, pcm_cap;
static void pcm_keep(unsigned al, const short *pcm, int frames, int rate, int copy)
{
    if (frames <= 0) return;
    if (npcm >= pcm_cap) {
        struct PcmOf *t = realloc(pcm_of, sizeof *t * (size_t)(pcm_cap ? pcm_cap * 2 : 256));
        if (!t) return;
        pcm_of = t, pcm_cap = pcm_cap ? pcm_cap * 2 : 256;
    }
    if (copy) {
        short *c = malloc(sizeof *c * (size_t)frames);
        if (!c) return;
        memcpy(c, pcm, sizeof *c * (size_t)frames), pcm = c;
    }
    pcm_of[npcm].al = al, pcm_of[npcm].pcm = pcm, pcm_of[npcm].frames = frames, pcm_of[npcm].rate = rate, pcm_of[npcm++].own = copy;
}
static void pcm_forget(void)
{
    for (int i = 0; i < npcm; i++) if (pcm_of[i].own) free((void *)pcm_of[i].pcm);
    npcm = 0;
}

/* a plain step's buffer (fx_step_plain): the rendering's samples on both channels of a stereo buffer, which OpenAL
   plays straight to the output with AL_DIRECT_CHANNELS_SOFT (a mono source is always placed, through the HRTF when
   that is on: measured, work\acoustics\steps16, the step's share under 300 Hz 88 % plain and 44 % placed) */
static struct { unsigned mono, stereo; } plain_of[64];
static int nplain;
static unsigned plain_stereo(unsigned al);

/* --- the Roland effects rendered by Munt (mt32emu.dll beside the game) --------------------------------------------- */
typedef void *mt_ctx;
static struct {
    HMODULE dll;
    mt_ctx (__cdecl *mt32emu_create_context)(void *, void *);
    int (__cdecl *mt32emu_add_rom_file)(mt_ctx, const char *);
    int (__cdecl *mt32emu_open_synth)(mt_ctx);
    void (__cdecl *mt32emu_close_synth)(mt_ctx);
    void (__cdecl *mt32emu_free_context)(mt_ctx);
    int (__cdecl *mt32emu_play_msg_now)(mt_ctx, unsigned);
    int (__cdecl *mt32emu_play_sysex_now)(mt_ctx, const unsigned char *, unsigned);
    void (__cdecl *mt32emu_render_bit16s)(mt_ctx, short *, unsigned);
    unsigned (__cdecl *mt32emu_get_actual_stereo_output_samplerate)(mt_ctx);
    void (__cdecl *mt32emu_set_stereo_output_samplerate)(mt_ctx, double);   /* (optional) */
    mt_ctx ctx;
    unsigned rate;
    int dry;                            /* the renderings without the Roland's own reverb (fx_roland_reverb) */
    unsigned char sounds[1 + 64 * 5];   /* SOUND\SOUNDS.DAT: the count, then 5 bytes an effect */
    int nsounds;
    unsigned char *mt;                  /* SOUND\UW.MT: the global timbre library (bank 1: the effects' timbres) */
    long mtlen;
    volatile LONG64 ready;              /* bit n: effect n rendered at its own velocity (the worker's, read by the window's thread) */
    HANDLE thread, asked;               /* the worker; a semaphore it waits on for renderings asked for */
    volatile LONG quit;
} M;

/* the renderings, by effect and the note's velocity (2026-11-13: the game's velocity is the SOUNDS.DAT one plus the
   caller's, a blow's rising with its damage, so one rendering an effect was too quiet for a hard hit): the effect's
   own velocity at start, then those of the fight sounds 3 and 4 that a blow can give (base + 4k, and 0x7F), the rest
   as the game asks for them (the first play of a new velocity uses the base rendering louder or softer by a guess,
   the exact one from then on) */
typedef struct {
    short *pcm;                         /* mono, M.rate a second (the worker's) */
    int frames;
    volatile LONG state;                /* 0 none, 1 asked, 2 rendered */
    unsigned albuf;                     /* its OpenAL buffer once made (the window's thread), 0 none */
} Rend;
static Rend rend[64][128];
#define ASK_RING 64
static volatile LONG ask_fx[ASK_RING], ask_vel[ASK_RING];   /* the window's thread writes, the worker reads */
static volatile LONG ask_head, ask_tail;

/* a Roland DT1 message to address a, b, c with its checksum (as the runtime's mt32.c send_sysex_msg) */
static void mt_sysex(unsigned a, unsigned b, unsigned c, const unsigned char *data, unsigned size)
{
    unsigned char m[300];
    unsigned sum = (a & 0xFF) + (b & 0xFF) + (c & 0xFF), n = 0;
    if (size > 256) return;
    m[n++] = 0xF0, m[n++] = 0x41, m[n++] = 0x10, m[n++] = 0x16, m[n++] = 0x12;
    m[n++] = (unsigned char)a, m[n++] = (unsigned char)b, m[n++] = (unsigned char)c;
    for (unsigned i = 0; i < size; i++) sum += data[i], m[n++] = data[i];
    m[n++] = (unsigned char)((0x80 - (sum & 0x7F)) & 0x7F);
    m[n++] = 0xF7;
    M.mt32emu_play_sysex_now(M.ctx, m, n);
}
static void mt_msg(unsigned status, unsigned d1, unsigned d2) { M.mt32emu_play_msg_now(M.ctx, status | d1 << 8 | d2 << 16); }

/* the timbre of bank 1, patch p from UW.MT (SOUND.C load_global_timbre: 6-byte entries bank, patch, offset to the
   end mark bank 0xFF; the timbre starts with its own length word), installed in memory timbre slot (the runtime's
   mt32.c install_timbre: 14 common bytes, then four partials of 58) and made patch `patch` (timbre group 2) */
static int mt_install(int slot, int p, int patch)
{
    long i = 0, off;
    unsigned len, dest = (unsigned)slot << 1;
    const unsigned char *t;
    unsigned char v[2] = {2, (unsigned char)slot};
    for (;; i += 6) {   /* struct GtlHdr: patch, bank (0xFF ends the directory), offset */
        if (i + 6 > M.mtlen || M.mt[i + 1] == 0xFF) return 0;
        if (M.mt[i] == p && M.mt[i + 1] == 1) break;
    }
    off = M.mt[i + 2] | M.mt[i + 3] << 8 | M.mt[i + 4] << 16 | (long)M.mt[i + 5] << 24;
    if (off < 0 || off + 2 > M.mtlen) return 0;
    len = M.mt[off] | M.mt[off + 1] << 8;
    if (len < 2 + 0x0E + 4 * 0x3A || off + (long)len > M.mtlen) return 0;
    t = M.mt + off + 2;
    mt_sysex(8, dest, 0x00, t, 0x0E), t += 0x0E;
    mt_sysex(8, dest, 0x0E, t, 0x3A), t += 0x3A;
    mt_sysex(8, dest, 0x48, t, 0x3A), t += 0x3A;
    mt_sysex(8, dest + 1, 0x02, t, 0x3A), t += 0x3A;
    mt_sysex(8, dest + 1, 0x3C, t, 0x3A);
    mt_sysex(5, (unsigned)(patch << 3) >> 7, (patch << 3) & 0x7F, v, 2);   /* patch memory: timbre group 2, number slot */
    return 1;
}

/* effect n (SOUNDS.DAT: timbre, note, velocity, length in 1/256 s) rendered to mono: the note held for its length
   (fx_play's timer at 16 Hz: length / 16 ticks), then let go and the release rendered until the output is quiet
   (4 s at most). Channel 1 (part 1 after the driver's channel map), pan centred, volume and expression full: the
   place and the distance are OpenAL's */
static int mt_render_as(int slot, int n, int vel, int shift)
{
    const unsigned char *e = M.sounds + 1 + n * 5;
    unsigned len = e[3] << 8 | e[4];
    unsigned hold = (unsigned)((double)(len == 0xFFFF ? 30 * 256 : len) / 256.0 * M.rate), cap = M.rate * 34, done = 0;
    short *mono = NULL, blk[1024 * 2];
    int quiet = 0;
    Rend *r = &rend[slot][vel & 0x7F];
    int note = e[1] + shift;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (r->state == 2) return 1;
    if (!mt_install(n, e[0], n)) return 0;
    if (hold > M.rate * 30) hold = M.rate * 30;
    if (!(mono = malloc(cap * sizeof *mono))) return 0;
    mt_msg(0xC1, (unsigned)n, 0);
    mt_msg(0xB1, 0x79, 0), mt_msg(0xB1, 7, 0x7F), mt_msg(0xB1, 0x0B, 0x7F), mt_msg(0xB1, 0x0A, 0x40);
    mt_msg(0x91, (unsigned)note, (unsigned)(vel & 0x7F));
    while (done < cap && !M.quit) {
        unsigned k = 1024, peak = 0;
        if (done < hold && hold - done < k) k = hold - done;
        if (done == hold) mt_msg(0x81, (unsigned)note, 0);
        M.mt32emu_render_bit16s(M.ctx, blk, k);
        for (unsigned i = 0; i < k; i++) {
            int v = ((int)blk[2 * i] + blk[2 * i + 1]) / 2, a = v < 0 ? -v : v;
            mono[done + i] = (short)v;
            if ((unsigned)a > peak) peak = (unsigned)a;
        }
        done += k;
        if (done > hold) {
            quiet = peak < 8 ? quiet + 1 : 0;
            if (quiet >= 3 || done - hold > M.rate * 4) break;
        }
    }
    mt_msg(0xB1, 0x7B, 0);   /* all notes off, whatever was left */
    if (M.quit) { free(mono); return 0; }
    {   /* (2026-11-16, the review: each rendering kept the whole 34 s it was made in, some 2 to 3 MB whatever its length) */
        short *t = realloc(mono, sizeof *mono * (size_t)(done ? done : 1));
        if (t) mono = t;
    }
    r->pcm = mono, r->frames = (int)done;
    InterlockedExchange(&r->state, 2);
    return 1;
}
static int mt_render(int n, int vel) { return mt_render_as(n, n, vel, 0); }
/* a footstep by its floor (Task 6): slot 32 + (effect - 1) * 16 + preset, the effect's note moved by the preset's step */
static int mt_render_step(int slot, int vel)
{
    int n = 1 + (slot - 32) / STEP_PRESETS;
    const UwaMaterial *m = uwacmat_get((slot - 32) % STEP_PRESETS);
    return m && n < M.nsounds ? mt_render_as(slot, n, vel, m->step) : 0;
}

static DWORD WINAPI mt_worker(LPVOID p)
{
    (void)p;
    for (int n = 0; n < M.nsounds && !M.quit; n++)   /* each effect at its own velocity: the usual case */
        if (mt_render(n, M.sounds[1 + n * 5 + 2])) InterlockedOr64(&M.ready, (LONG64)1 << n);
        else if (!M.quit) fprintf(stderr, "fx: Roland effect %d not rendered\n", n);
    for (int n = 3; n <= 4 && n < M.nsounds && !M.quit; n++) {   /* the blows (COMBAT.C: sounds 3 and 4 at damage * 4 louder) */
        int base = M.sounds[1 + n * 5 + 2];
        for (int v = base + 4; v < 0x7F && !M.quit; v += 4) mt_render(n, v);
        if (!M.quit) mt_render(n, 0x7F);
    }
    while (!M.quit) {   /* then whatever the game asks for */
        WaitForSingleObject(M.asked, 200);
        while (!M.quit && ask_tail != ask_head) {
            LONG i = ask_tail % ASK_RING, fx = ask_fx[i], vel = ask_vel[i];
            InterlockedIncrement(&ask_tail);
            if (fx >= 32 && fx < 64) mt_render_step((int)fx, (int)vel);
            else if (fx >= 0 && fx < M.nsounds) mt_render((int)fx, (int)vel);
        }
    }
    return 0;
}

static int read_file(const char *path, unsigned char **out, long *len, long most)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return 0;
    fseek(f, 0, SEEK_END), n = ftell(f), fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > most || !(*out = malloc((size_t)n))) { fclose(f); return 0; }
    *len = (long)fread(*out, 1, (size_t)n, f);
    fclose(f);
    return *len == n;
}

/* The game's own folder (uwaudio_game_dir, the front end's before uwaudio_init). The game's files are no longer
   extracted onto the disk (the user, 2026-11-16), so data\sound\sounds.dat and uw.mt (plain copies of the game's
   SOUND\SOUNDS.DAT and UW.MT) may not be there: the game's own are read then, the folder and the name in either case. */
static char game_dir_set[1024];
void uwaudio_game_dir(const char *dir) { snprintf(game_dir_set, sizeof game_dir_set, "%s", dir ? dir : ""); }
static int read_game_sound(const char *upper, const char *lower, unsigned char **out, long *len, long most)
{
    static const char *const dirs[2] = { "SOUND", "sound" };
    char p[1100];
    int d, k;
    if (!game_dir_set[0]) return 0;
    for (d = 0; d < 2; d++)
        for (k = 0; k < 2; k++) {
            snprintf(p, sizeof p, "%s" UWOS_SEP "%s" UWOS_SEP "%s", game_dir_set, dirs[d], k ? lower : upper);
            if (read_file(p, out, len, most)) return 1;
        }
    return 0;
}

/* Munt opened as UW's MT32MPU.ADV opens the unit (the runtime's mt32.c reset_synth / init_synth: the channel map,
   the partial reserve, the reverb) and the rendering started; 0 (with the reason logged) leaves the effects to the
   music driver as before */
static int setting_get(const char *datadir, const char *key, int dflt);
static int mt_start(const char *datadir)
{
    static const unsigned char part_chans[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9}, part_rsv[9] = {3, 4, 3, 4, 3, 4, 3, 4, 4},
                               init_reverb[3] = {0, 3, 2}, dry_reverb[3] = {0, 3, 0}, master_vol = 90;
    char base[MAX_PATH], p[MAX_PATH];
    const char *why = NULL, *e = getenv("UWPORT_FX_ROLAND");
    unsigned char *sd = NULL;
    long sdlen = 0, bl;
    if (e && *e && !atoi(e)) return 0;   /* (the checks: the wav path alone) */
    snprintf(base, sizeof base, "%s", datadir ? datadir : ".");
    bl = (long)strlen(base);
    while (bl > 0 && base[bl - 1] != '\\' && base[bl - 1] != '/') bl--;
    base[bl] = 0;   /* the game's folder: data\'s parent */
    snprintf(p, sizeof p, "%s" UWOS_LIB_MT32EMU, base);
    if (!(M.dll = LoadLibraryExA(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH))) why = "no mt32emu.dll";
#define GET(n) (why || (*(FARPROC *)&M.n = GetProcAddress(M.dll, #n)) != NULL || (why = "mt32emu.dll lacks " #n))
    GET(mt32emu_create_context), GET(mt32emu_add_rom_file), GET(mt32emu_open_synth), GET(mt32emu_close_synth);
    GET(mt32emu_free_context), GET(mt32emu_play_msg_now), GET(mt32emu_play_sysex_now), GET(mt32emu_render_bit16s);
    GET(mt32emu_get_actual_stereo_output_samplerate);
#undef GET
    if (!why) *(FARPROC *)&M.mt32emu_set_stereo_output_samplerate = GetProcAddress(M.dll, "mt32emu_set_stereo_output_samplerate");
    if (!why) {
        snprintf(p, sizeof p, "%s" UWOS_SEP "sound" UWOS_SEP "sounds.dat", datadir ? datadir : ".");
        if (!read_file(p, &sd, &sdlen, sizeof M.sounds) && !read_game_sound("SOUNDS.DAT", "sounds.dat", &sd, &sdlen, sizeof M.sounds)) why = "no SOUND\\SOUNDS.DAT";
        else if (sdlen < 1 || 1 + sd[0] * 5 > sdlen) why = "no SOUND\\SOUNDS.DAT";
        else memcpy(M.sounds, sd, (size_t)sdlen), M.nsounds = sd[0] > 64 ? 64 : sd[0];
        free(sd);
    }
    if (!why) {
        snprintf(p, sizeof p, "%s" UWOS_SEP "sound" UWOS_SEP "uw.mt", datadir ? datadir : ".");
        if (!read_file(p, &M.mt, &M.mtlen, 1 << 20) && !read_game_sound("UW.MT", "uw.mt", &M.mt, &M.mtlen, 1 << 20)) why = "no SOUND\\UW.MT";
    }
    if (!why) {
        M.ctx = M.mt32emu_create_context(NULL, NULL);
        snprintf(p, sizeof p, "%sroms" UWOS_SEP "cm32l_ctrl_1_02.rom", base);
        int r1 = M.mt32emu_add_rom_file(M.ctx, p);
        snprintf(p, sizeof p, "%sroms" UWOS_SEP "cm32l_pcm.rom", base);
        int r2 = M.mt32emu_add_rom_file(M.ctx, p);
        if (r1 < 0 || r2 < 0) why = "no roms\\cm32l_ctrl_1_02.rom + cm32l_pcm.rom";
        else {
            /* (2026-11-16, read in Exhume's audio.c mt_open: the emulator is asked for the output's own rate, its own
               resampler doing it. Ours rendered at the unit's 32 kHz and left the rest to a straight-line resampling
               in uwsteam.c and to OpenAL's.) */
            if (M.mt32emu_set_stereo_output_samplerate) M.mt32emu_set_stereo_output_samplerate(M.ctx, 48000.0);
            if (M.mt32emu_open_synth(M.ctx) != 0) why = "Munt did not open";
        }
    }
    if (why) {
        fprintf(stderr, "fx: Roland effects off (%s): the music driver plays them\n", why);
        if (M.ctx) M.mt32emu_free_context(M.ctx), M.ctx = NULL;
        if (M.dll) FreeLibrary(M.dll), M.dll = NULL;
        free(M.mt), M.mt = NULL, M.mtlen = 0, M.nsounds = 0;
        return 0;
    }
    M.rate = M.mt32emu_get_actual_stereo_output_samplerate(M.ctx);
    mt_sysex(0x7F, 0, 0, part_chans, 1);          /* reset */
    mt_sysex(0x10, 0, 0x0D, part_chans, 9);       /* the channel map */
    mt_sysex(0x10, 0, 0x04, part_rsv, 9);         /* the partial reserve */
    {   /* The Roland's own reverb (2026-11-16, the review): the driver sets a small room on every note (mode 0, time 3,
           level 2), and with the acoustics on the real room's echo was then made of a sound that already carried
           one, its tail muffled and placed as if it were the sound. fx_roland_reverb (the Esc menu's "Roland's own
           reverb"): 0 never, 1 always, 2 as it comes: none while the acoustics are on, the original's without them. */
        const char *ae = getenv("UWPORT_FX_ACOUSTICS"), *re = getenv("UWPORT_FX_ROLAND_REVERB");
        int rv = re && *re ? atoi(re) : setting_get(datadir, "fx_roland_reverb", 2);
        int ac = ae && *ae ? atoi(ae) : setting_get(datadir, "fx_acoustics", 0);
        M.dry = rv == 0 || (rv != 1 && ac != 0);
    }
    mt_sysex(0x10, 0, 0x01, M.dry ? dry_reverb : init_reverb, 3);      /* the reverb */
    mt_sysex(0x10, 0, 0x16, &master_vol, 1);      /* the master volume (the driver's "avoid MT-32 distortion") */
    M.ready = 0, M.quit = 0;
    ask_head = ask_tail = 0;
    if (!M.asked) M.asked = CreateSemaphoreA(NULL, 0, ASK_RING, NULL);
    M.thread = CreateThread(NULL, 1 << 20, mt_worker, NULL, 0, NULL);
    if (!M.thread) { fprintf(stderr, "fx: Roland effects off (no thread)\n"); return 0; }
    fprintf(stderr, "fx: Roland effects rendered by Munt (%d effects, %u Hz, %s)\n", M.nsounds, M.rate,
            M.dry ? "without the Roland's own reverb" : "with the Roland's own reverb");
    return 1;
}

static void mt_stop(void)
{
    if (M.thread) {
        InterlockedExchange(&M.quit, 1);
        if (M.asked) ReleaseSemaphore(M.asked, 1, NULL);
        WaitForSingleObject(M.thread, 30000);
        CloseHandle(M.thread), M.thread = NULL;
    }
    for (int i = 0; i < 64; i++)
        for (int v = 0; v < 128; v++) free(rend[i][v].pcm), rend[i][v].pcm = NULL, rend[i][v].frames = 0, rend[i][v].state = 0;
    M.ready = 0;
    if (M.ctx) M.mt32emu_close_synth(M.ctx), M.mt32emu_free_context(M.ctx), M.ctx = NULL;
    if (M.dll) FreeLibrary(M.dll), M.dll = NULL;
    free(M.mt), M.mt = NULL, M.mtlen = 0, M.nsounds = 0;
}

static int setting_get(const char *datadir, const char *key, int dflt)
{
    char p[MAX_PATH], line[256], k[64], v[64];
    int val = dflt;
    const char *e;
    FILE *f;
    snprintf(p, sizeof p, "%s" UWOS_SEP "settings.txt", datadir ? datadir : ".");
    if ((f = fopen(p, "r")) != NULL) {
        while (fgets(line, sizeof line, f))
            if (sscanf(line, "%63s %63s", k, v) == 2 && !strcmp(k, key)) val = atoi(v);
        fclose(f);
    }
    /* UWPORT_FX_AUDIO=0 / 1, UWPORT_FX_PANFIX=0 / 1 (the checks): whatever the settings say */
    if (!strcmp(key, "fx_audio") && (e = getenv("UWPORT_FX_AUDIO")) != NULL && *e) val = atoi(e);
    if (!strcmp(key, "fx_pan_fix") && (e = getenv("UWPORT_FX_PANFIX")) != NULL && *e) val = atoi(e);
    if (!strcmp(key, "fx_hrtf") && (e = getenv("UWPORT_FX_HRTF")) != NULL && *e) val = atoi(e);
    return val;
}

/* the context's attributes: HRTF as fx_hrtf says (1 on, the default; 0 off; else OpenAL's choice), then the caller's
   (the loopback's format; NULL none); into attrs (room for 16), returned for alcCreateContext */
static int *context_attrs(const char *datadir, const int *more, int *attrs)
{
    int h = uwaudio_setting(datadir, "fx_hrtf", 1), n = 0;
    attrs[n++] = ALC_HRTF_SOFT, attrs[n++] = h == 1 ? 1 : h == 0 ? 0 : ALC_DONT_CARE_SOFT;
    for (; more && *more && n < 14; more += 2) attrs[n++] = more[0], attrs[n++] = more[1];
    attrs[n] = 0;
    return attrs;
}

/* what the device made of it: hrtf_status / hrtf_name, and one log line */
static void hrtf_read(void)
{
    static const char *const st[] = {"off", "on", "denied by the device's configuration", "required by the device's configuration",
                                     "on (headphones detected)", "unsupported by the output format"};
    const char *s;
    int v = 0;
    hrtf_status = -1, hrtf_name[0] = 0;
    if (!dev || !A.alcGetIntegerv || !A.alcIsExtensionPresent(dev, "ALC_SOFT_HRTF")) {
        fprintf(stderr, "fx: HRTF unavailable (no ALC_SOFT_HRTF)\n");
        return;
    }
    A.alcGetIntegerv(dev, ALC_HRTF_STATUS_SOFT, 1, &v);
    hrtf_status = v;
    if (A.alcGetString && (s = A.alcGetString(dev, ALC_HRTF_SPECIFIER_SOFT)) != NULL) snprintf(hrtf_name, sizeof hrtf_name, "%s", s);
    fprintf(stderr, "fx: HRTF %s%s%s%s\n", v >= 0 && v < 6 ? st[v] : "?", hrtf_name[0] ? " (" : "", hrtf_name, hrtf_name[0] ? ")" : "");
    if (on) {   /* the makeup follows what the device made of HRTF */
        A.alListenerf(AL_GAIN, listener_gain());
        fprintf(stderr, "fx: makeup gain %.2f (to the game's own mix)\n", makeup());
    }
}

int uwaudio_hrtf(void) { return hrtf_status; }
const char *uwaudio_hrtf_name(void) { return hrtf_name; }
static int setting_on(const char *datadir, const char *key) { return setting_get(datadir, key, 0); }
int uwaudio_setting(const char *datadir, const char *key, int dflt) { return setting_get(datadir, key, dflt); }

static int load_dll(const char *bindir)
{
    char p[MAX_PATH];
    const char *e = getenv("UWPORT_FX_BIN");   /* (the checks: another folder for soft_oal.dll) */
    snprintf(p, sizeof p, "%s" UWOS_SEP UWOS_LIB_OPENAL, e && *e ? e : bindir ? bindir : ".");
    snprintf(bin_dir, sizeof bin_dir, "%s", bindir ? bindir : ".");   /* (phonon.dll's place too: the game's own bin\) */
    if (!uwos_lib_findable(p)) return 0;   /* (elsewhere: beside the path or by its bare name) */
    A.dll = LoadLibraryExA(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!A.dll) return 0;
#define GET(n) (*(FARPROC *)&A.n = GetProcAddress(A.dll, #n)) != NULL
    if (!(GET(alcOpenDevice) && GET(alcCreateContext) && GET(alcMakeContextCurrent) && GET(alcDestroyContext) &&
          GET(alcCloseDevice) && GET(alcGetProcAddress) && GET(alcIsExtensionPresent) && GET(alcGetIntegerv) &&
          GET(alcGetString) && GET(alGetError) &&
          GET(alGenSources) && GET(alGenBuffers) && GET(alDeleteSources) && GET(alDeleteBuffers) && GET(alBufferData) &&
          GET(alSourcei) && GET(alSourcef) && GET(alSource3f) && GET(alSourcePlay) && GET(alSourceStop) &&
          GET(alGetSourcei) && GET(alListener3f) && GET(alListenerfv) && GET(alListenerf) && GET(alDistanceModel))) {
        FreeLibrary(A.dll), A.dll = NULL;
        return 0;
    }
#undef GET
    *(FARPROC *)&A.alGetProcAddress = GetProcAddress(A.dll, "alGetProcAddress");   /* (the acoustics': without them, none) */
    *(FARPROC *)&A.alSource3i = GetProcAddress(A.dll, "alSource3i");
    *(FARPROC *)&A.alSourceQueueBuffers = GetProcAddress(A.dll, "alSourceQueueBuffers");
    *(FARPROC *)&A.alSourceUnqueueBuffers = GetProcAddress(A.dll, "alSourceUnqueueBuffers");
    return 1;
}

/* --- acoustics (docs\superpowers\plans\2026-11-14-acoustics-ours.md, Task 4): OpenAL's EFX driven by our own rays
   (uwacoustic.c). One auxiliary slot with an EAX reverb for the room round the listener, every source sending to it;
   one band-pass filter a source on its direct sound for what stands between it and the listener. Off (and nothing
   of it touched: the sound as before) unless fx_acoustics is 1 and the device has ALC_EXT_EFX. --- */
#define AL_EFFECTSLOT_EFFECT 0x0001
#define AL_EFFECTSLOT_GAIN 0x0002
#define AL_EFFECT_TYPE 0x8001
#define AL_EFFECT_EAXREVERB 0x8000
#define AL_EAXREVERB_GAIN 0x0003
#define AL_EAXREVERB_DECAY_TIME 0x0006
#define AL_EAXREVERB_DECAY_HFRATIO 0x0007
#define AL_EAXREVERB_DECAY_LFRATIO 0x0008
#define AL_EAXREVERB_REFLECTIONS_DELAY 0x000A
#define AL_EAXREVERB_LATE_REVERB_DELAY 0x000D
#define AL_FILTER_TYPE 0x8001
#define AL_FILTER_NULL 0x0000
#define AL_FILTER_BANDPASS 0x0003
#define AL_BANDPASS_GAIN 0x0001
#define AL_BANDPASS_GAINLF 0x0002
#define AL_BANDPASS_GAINHF 0x0003
#define AL_DIRECT_FILTER 0x20005
#define AL_AUXILIARY_SEND_FILTER 0x20006
#define AL_EFFECTSLOT_NULL 0x0000
#define AC_PROBE_S 0.1f          /* the room and the paths asked again this often */
/* (the reverb's values ease to a new room over about half a second, three of TU.reverb_tc, 0.17 s; a source's filter
   eases to a new path with TU.filter_tc, 0.3 s) */
#define AC_SLICE 960             /* frames rendered between two steps of the above (20 ms at 48 kHz) */
static struct {
    PFgen alGenEffects, alDeleteEffects, alGenAuxiliaryEffectSlots, alDeleteAuxiliaryEffectSlots, alGenFilters, alDeleteFilters;
    PFsrci alEffecti, alAuxiliaryEffectSloti, alFilteri;
    PFsrcf alEffectf, alAuxiliaryEffectSlotf, alFilterf;
    int on, have_room;
    unsigned slot, effect, filter[NSRC];
    float wet, occl, tile_m;         /* acoustics_wet / 100 (0..2), acoustics_occlusion / 100 (0..1), acoustics_tile_m */
    float cur[5], tgt[5], set[5];    /* decay time, its high and low ratios, the reflections' and the late reverb's delay: now, wanted, last given to OpenAL */
    float acc;                       /* seconds since the last probe */
    float pos[NSRC][3], g[NSRC][3], gt[NSRC][3];   /* each source's place, its gains by band now and wanted */
    unsigned char here[NSRC];        /* 1: the source is at the listener (never muffled) */
    int detour;                      /* acoustics_detour: a sound round a corner comes along the way a person would walk */
    unsigned char moved[NSRC];       /* 1: the source is placed at the opening its sound comes through, not at its own place */
    float heard[NSRC][3];            /* ... that place (the sound's own when not moved) */
    unsigned char raised[NSRC];      /* 1: its gains were raised by a way round a corner */
    int rate;                        /* the loopback device's frames a second (0: a sound card) */
    LARGE_INTEGER clock;             /* a visible run's last step */
} AC;

static float setting_float(const char *datadir, const char *key, float dflt)
{
    char p[MAX_PATH], line[256], k[64], v[64];
    float val = dflt;
    FILE *f;
    snprintf(p, sizeof p, "%s" UWOS_SEP "settings.txt", datadir ? datadir : ".");
    if ((f = fopen(p, "r")) != NULL) {
        while (fgets(line, sizeof line, f))
            if (sscanf(line, "%63s %63s", k, v) == 2 && !strcmp(k, key)) val = (float)atof(v);
        fclose(f);
    }
    return val;
}
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* --- Acoustics High (the acoustics plan's Task 7; fx_acoustics 2): Steam Audio's reflections on top of ours ---
   The muffling by walls and the way round corners stay ours (the filters, above). What changes: the one EFX reverb
   is off, and each sound's own echoes come from uwsteam.c (the level's real shape, the sound's own place: a sound in
   the next room rings with that room), mixed there and played here by one stereo source that we keep fed, block by
   block. A hidden run (the loopback device) simulates and feeds in step with the frames rendered, so the checks
   repeat; a sound card has two threads of its own: the rays every 50 ms, the feeding every 10. Without phonon.dll,
   or when Steam Audio does not start: one log line, and the acoustics go on as "On" (the EFX reverb). */
#define AL_BUFFERS_QUEUED 0x1015
#define AL_BUFFERS_PROCESSED 0x1016
#define HI_RATE 48000
#define HI_BUFS 8
/* blocks kept queued ahead of the playing one (TU.ahead; settings.txt acoustics_echo_ahead, 2 to 6). 2 until
   2026-11-16 (about 21 ms; the feed runs every 10): the uwr session's review measured the stream running dry
   0.9 to 21 times a second with 2 on the user's device, never with 3 or more. 4 now: about 43 ms. */
#define HI_AHEAD TU.ahead
#define AL_FORMAT_STEREO_FLOAT32 0x10011   /* AL_EXT_FLOAT32 */
#define AL_DIRECT_CHANNELS_SOFT 0x1033
/* Steam Audio's echoes against the sound itself (the review, 2026-11-14): at 1 a footstep's echoes measured 15 dB
   OVER the step (its samples at full scale, as from a metre away, while OpenAL's own output of a sound at the listener
   is some 10 dB under that: the pan's 3 dB, the HRTF's 7). 0.12 puts them a few dB under it, about where the EFX
   reverb of "On" sits; the menu's Reverb level scales it from there. To be judged by ear. */
#define HI_LEVEL TU.hi_level     /* (0.12 unless settings.txt acoustics_echo_level says another) */
static struct {
    int on;
    unsigned src, buf[HI_BUFS], free_buf[HI_BUFS];
    int nfree, idle;             /* idle: no voice sounds or rings (nothing is queued until one does) */
    int is_float;                /* the stream's buffers are float (AL_EXT_FLOAT32); else 16 bits, rounded */
    int live, dry;               /* more blocks are to come; the times the source stopped though they were */
    volatile LONG kick;          /* counts the sounds started (hi_voice): the feed's idle against a sound just begun */
    int voice[NSRC];             /* each source's voice in uwsteam.c (-1 none) */
    float sim_acc;               /* a hidden run: seconds since the last simulation */
    HANDLE sim_thread, feed_thread, sim_wake;
    volatile LONG quit;
} HI;

/* the stream kept fed: done buffers taken back, blocks mixed and queued up to HI_AHEAD, the source started again
   when it ran dry */
static void hi_feed(void)
{
    static float blk[UWST_FRAME * 2];
    static short blk16[UWST_FRAME * 2];
    int done = 0, queued = 0, st = 0;
    if (!HI.on) return;
    A.alGetSourcei(HI.src, AL_BUFFERS_PROCESSED, &done);
    while (done-- > 0 && HI.nfree < HI_BUFS) A.alSourceUnqueueBuffers(HI.src, 1, &HI.free_buf[HI.nfree]), HI.nfree++;
    A.alGetSourcei(HI.src, AL_BUFFERS_QUEUED, &queued);
    while (queued < HI_AHEAD && HI.nfree > 0 && !(HI.idle && queued == 0)) {
        LONG kick = HI.kick;
        int busy = uwst_mix_float(blk, UWST_FRAME, AC.wet * HI_LEVEL, hrtf_status == 1 || hrtf_status == 4);
        unsigned b = HI.free_buf[--HI.nfree];
        if (HI.is_float) A.alBufferData(b, AL_FORMAT_STEREO_FLOAT32, blk, (int)sizeof blk, HI_RATE);
        else {
            for (int i = 0; i < UWST_FRAME * 2; i++) {
                float v = blk[i] * 32767.0f;
                blk16[i] = (short)lrintf(v > 32767.0f ? 32767.0f : v < -32768.0f ? -32768.0f : v);
            }
            A.alBufferData(b, AL_FORMAT_STEREO16, blk16, (int)sizeof blk16, HI_RATE);
        }
        A.alSourceQueueBuffers(HI.src, 1, &b);
        queued++;
        if (!busy) {                                 /* (this last block carries the end of the ringing) */
            HI.idle = 1;
            if (HI.kick != kick) HI.idle = 0;        /* (a sound begun while this block was mixed: not idle after all) */
            if (HI.idle) break;
        }
    }
    if (queued > 0) {
        A.alGetSourcei(HI.src, AL_SOURCE_STATE, &st);
        if (st != AL_PLAYING) {
            if (HI.live) HI.dry++;                   /* it stopped though more was to come: the feed was late */
            A.alSourcePlay(HI.src);
        }
        HI.live = !HI.idle;
    }
}
/* every 50 ms, and at once when a new sound starts (2026-11-15: a voice's echoes are silent until its first run, so
   waiting out the 50 ms lost the start of each sound's echoes) */
static DWORD WINAPI hi_sim_worker(LPVOID p)
{
    (void)p;
    while (!HI.quit) uwst_simulate(), WaitForSingleObject(HI.sim_wake, 50);
    return 0;
}
static DWORD WINAPI hi_feed_worker(LPVOID p) { (void)p; while (!HI.quit) hi_feed(), Sleep(10); return 0; }
static void hi_stop(void)
{
    if (!HI.on) return;
    InterlockedExchange(&HI.quit, 1);
    if (HI.sim_wake) SetEvent(HI.sim_wake);
    if (HI.sim_thread) WaitForSingleObject(HI.sim_thread, 5000), CloseHandle(HI.sim_thread), HI.sim_thread = NULL;
    if (HI.feed_thread) WaitForSingleObject(HI.feed_thread, 5000), CloseHandle(HI.feed_thread), HI.feed_thread = NULL;
    if (HI.sim_wake) CloseHandle(HI.sim_wake), HI.sim_wake = NULL;
    HI.on = 0;
    if (HI.dry) fprintf(stderr, "fx: acoustics high: the echo stream ran dry %d times\n", HI.dry);
    A.alSourceStop(HI.src);
    A.alSourcei(HI.src, AL_BUFFER, 0);
    A.alDeleteSources(1, &HI.src);
    A.alDeleteBuffers(HI_BUFS, HI.buf);
    uwst_shutdown();
}
/* after the acoustics are on: 1 when Steam Audio's reflections run (the EFX reverb then stays silent) */
static int hi_start(void)
{
    const char *e = getenv("UWPORT_FX_PHONON");   /* (the checks: another folder for phonon.dll) */
    memset(&HI, 0, sizeof HI);
    for (int k = 0; k < NSRC; k++) HI.voice[k] = -1;
    if (!A.alSourceQueueBuffers || !A.alSourceUnqueueBuffers) { fprintf(stderr, "fx: acoustics high off (no buffer queues in OpenAL)\n"); return 0; }
    if (!uwst_init(e && *e ? e : bin_dir, HI_RATE)) return 0;
    A.alGetError();
    A.alGenSources(1, &HI.src);
    A.alGenBuffers(HI_BUFS, HI.buf);
    if (A.alGetError()) { fprintf(stderr, "fx: acoustics high off (no source for the reflections)\n"); uwst_shutdown(); return 0; }
    for (int i = 0; i < HI_BUFS; i++) HI.free_buf[i] = HI.buf[i];
    HI.nfree = HI_BUFS, HI.idle = 1;
    {   /* float buffers when OpenAL takes them (OpenAL Soft does: AL_EXT_FLOAT32) */
        static const float zero[8];
        A.alBufferData(HI.buf[0], AL_FORMAT_STEREO_FLOAT32, zero, (int)sizeof zero, HI_RATE);
        HI.is_float = A.alGetError() == 0;
    }
    A.alSourcei(HI.src, AL_SOURCE_RELATIVE, 1);
    A.alSource3f(HI.src, AL_POSITION, 0, 0, 0);
    A.alSourcef(HI.src, AL_ROLLOFF_FACTOR, 0);
    /* (2026-11-16, the same review: the stream is the two ears' sound already, Steam Audio's own HRTF in it, and as
       a plain stereo source it went through OpenAL's HRTF again: a click on the left smeared over 63 samples and
       leaked to the right. Its two channels straight to the output's two.) */
    A.alSourcei(HI.src, AL_DIRECT_CHANNELS_SOFT, TU.direct);
    A.alGetError();
    uwst_tile_m(AC.tile_m);
    HI.on = 1;
    if (AC.rate <= 0) {                              /* a sound card: its own two threads */
        HI.sim_wake = CreateEventA(NULL, FALSE, FALSE, NULL);
        HI.sim_thread = CreateThread(NULL, 0, hi_sim_worker, NULL, 0, NULL);
        HI.feed_thread = CreateThread(NULL, 0, hi_feed_worker, NULL, 0, NULL);
    }
    return 1;
}
static void hi_push(int k)
{
    if (!HI.on || HI.voice[k] < 0) return;
    if (AC.here[k]) { static const float one[3] = {1, 1, 1}; uwst_direct(HI.voice[k], NULL, one, 0); }
    else uwst_direct(HI.voice[k], AC.heard[k], AC.gt[k], AC.raised[k]);
}
/* source k starts a sound (buffer `al`, gain g; its echoes at `echo` of that) at a place: its voice in uwsteam.c.
   1: the sound itself is Steam Audio's too (the caller keeps its own copy silent) */
static int hi_voice(int k, unsigned al, float x, float y, float z, float g, float echo)
{
    if (!HI.on) return 0;
    if (HI.voice[k] >= 0) uwst_end(HI.voice[k]), HI.voice[k] = -1;   /* (the sound this source played before) */
    for (int i = 0; i < npcm; i++)
        if (pcm_of[i].al == al) {
            HI.voice[k] = uwst_begin(x, y, z, pcm_of[i].pcm, pcm_of[i].frames, pcm_of[i].rate, g > 4.0f ? 4.0f : g);
            if (HI.voice[k] >= 0) {
                uwst_echo(HI.voice[k], echo);
                hi_push(k);
                /* (the review, 2026-11-14: a voice is given again once its echoes have rung out; a source that had it
                   before must not end the new sound's echoes when it starts its own next one) */
                for (int j = 0; j < NSRC; j++) if (j != k && HI.voice[j] == HI.voice[k]) HI.voice[j] = -1;
                InterlockedIncrement(&HI.kick);
                HI.idle = 0;
                if (AC.rate > 0) uwst_simulate();    /* a hidden run: its response now, so the checks repeat */
                else if (HI.sim_wake) SetEvent(HI.sim_wake);
                return uwst_direct_on() && (hrtf_status == 1 || hrtf_status == 4);
            }
            return 0;
        }
    return 0;
}

/* the reverb's values and the slot's level given to OpenAL (the slot silent until the first room is known, and at
   acoustics_wet 0; above 100 % the reverb's own gain is raised, the slot's being 1 at most) */
static void ac_reverb_apply(int force)
{
    static const int id[5] = {AL_EAXREVERB_DECAY_TIME, AL_EAXREVERB_DECAY_HFRATIO, AL_EAXREVERB_DECAY_LFRATIO, AL_EAXREVERB_REFLECTIONS_DELAY, AL_EAXREVERB_LATE_REVERB_DELAY};
    int changed = force;
    for (int i = 0; i < 5; i++)
        if (force || fabsf(AC.cur[i] - AC.set[i]) > 0.01f * AC.set[i] + 1e-4f) AC.alEffectf(AC.effect, id[i], AC.cur[i]), AC.set[i] = AC.cur[i], changed = 1;
    if (force) AC.alEffectf(AC.effect, AL_EAXREVERB_GAIN, clampf(TU.reverb_gain * (AC.wet > 1 ? AC.wet : 1), 0, 1));
    /* (at acoustics_wet 0 the reverb is taken off the slot: its level alone leaves a tail already ringing to ring on) */
    if (changed) AC.alAuxiliaryEffectSloti(AC.slot, AL_EFFECTSLOT_EFFECT, AC.wet > 0 ? (int)AC.effect : 0);
    if (force) AC.alAuxiliaryEffectSlotf(AC.slot, AL_EFFECTSLOT_GAIN, AC.have_room ? clampf(AC.wet, 0, 1) : 0);
}
/* the room round the listener, as the reverb's wanted values */
static void ac_room(void)
{
    UwaRoom r;
    if (HI.on) return;                               /* (Acoustics High: the echoes are Steam Audio's, no EFX reverb) */
    if (!uwac_room(lis, &r)) return;                 /* the listener in rock or outside the map: the last values hold */
    AC.tgt[0] = clampf(r.rt60[1] * TU.decay, 0.1f, 20.0f);
    AC.tgt[1] = clampf(r.rt60[2] / r.rt60[1] * TU.decay_hf, 0.1f, 2.0f);
    AC.tgt[2] = clampf(r.rt60[0] / r.rt60[1] * TU.decay_lf, 0.1f, 2.0f);
    AC.tgt[3] = clampf(r.first_m / 343.0f * TU.refl_delay, 0, 0.3f);   /* (the first echo: the way to the nearest surfaces) */
    AC.tgt[4] = clampf(2.0f * r.mfp_m / 343.0f * TU.late_delay, 0, 0.1f);
    if (lbfile) {   /* (the checks' window: the room, whenever its decay is another by a twentieth) */
        static float said;
        if (fabsf(AC.tgt[0] - said) > 0.05f * said) {
            said = AC.tgt[0];
            fprintf(stderr, "fx: acoustics: room at (%.2f, %.2f): rt60 %.2f %.2f %.2f s, mean free path %.1f m\n", lis[0], lis[2], r.rt60[0], r.rt60[1], r.rt60[2], r.mfp_m);
        }
    }
    if (!AC.have_room) {
        memcpy(AC.cur, AC.tgt, sizeof AC.cur), AC.have_room = 1;
        ac_reverb_apply(1);
    }
}
/* source k heard from toward (the way round's opening), at its own distance from the listener; NULL: from its own place */
static void ac_place(int k, const float *toward)
{
    if (!toward) {
        if (AC.moved[k]) A.alSource3f(src[k], AL_POSITION, AC.pos[k][0], AC.pos[k][1], -AC.pos[k][2]), AC.moved[k] = 0;
        memcpy(AC.heard[k], AC.pos[k], sizeof AC.heard[k]);
        return;
    }
    float d[3] = {toward[0] - lis[0], 0, toward[2] - lis[2]}, s[3] = {AC.pos[k][0] - lis[0], AC.pos[k][1] - lis[1], AC.pos[k][2] - lis[2]};
    float dl = sqrtf(d[0] * d[0] + d[2] * d[2]), sl = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    if (dl < 1e-3f) return;
    A.alSource3f(src[k], AL_POSITION, lis[0] + d[0] / dl * sl, lis[1], -(lis[2] + d[2] / dl * sl));
    AC.heard[k][0] = lis[0] + d[0] / dl * sl, AC.heard[k][1] = lis[1], AC.heard[k][2] = lis[2] + d[2] / dl * sl;
    AC.moved[k] = 1;
}
/* source k's voice in uwsteam.c told what ac_path found (the sound itself by Steam Audio): where it is heard from,
   our gains, and whether they are a floor under Steam Audio's own (a way round a corner raised them) */
static void hi_push(int k);
/* what reaches the listener of source k's sound, by band, as acoustics_occlusion scales it */
static void ac_path(int k, float g[3])
{
    g[0] = g[1] = g[2] = 1.0f;
    AC.raised[k] = 0;
    if (AC.here[k] || AC.occl <= 0) { ac_place(k, NULL); hi_push(k); return; }
    /* Task 5, sound round corners: with rock on the straight line and a way round, the sound is louder by that way
       (uwac_path_round) and is heard from the opening it comes through (uwac_waypoint), at its own distance (the
       falloff stays the original's, by the straight line) */
    float wp[3];
    if (AC.detour && uwac_path_round(AC.pos[k], lis, g) && uwac_waypoint(AC.pos[k], lis, wp)) ac_place(k, wp), AC.raised[k] = 1;
    else {
        if (!AC.detour) uwac_path(AC.pos[k], lis, g);
        ac_place(k, NULL);
    }
    for (int b = 0; b < 3; b++) g[b] = 1.0f - (1.0f - g[b]) * AC.occl;
    hi_push(k);
}
static void ac_filter_apply(int k)
{
    /* (the review, 2026-11-14: the same filter on the sound's way into the room's reverb. Unfiltered, a sound that
       rock hid still rang in the listener's room at its full strength: its echo louder than itself, from everywhere) */
    if (AC.occl <= 0) {   /* 0: no filter at all */
        A.alSourcei(src[k], AL_DIRECT_FILTER, AL_FILTER_NULL);
        A.alSource3i(src[k], AL_AUXILIARY_SEND_FILTER, (int)AC.slot, 0, AL_FILTER_NULL);
        return;
    }
    float mid = clampf(AC.g[k][1], 0.001f, 1.0f);
    AC.alFilterf(AC.filter[k], AL_BANDPASS_GAIN, mid);
    AC.alFilterf(AC.filter[k], AL_BANDPASS_GAINLF, clampf(AC.g[k][0] / mid, 0.001f, 1.0f));
    AC.alFilterf(AC.filter[k], AL_BANDPASS_GAINHF, clampf(AC.g[k][2] / mid, 0.001f, 1.0f));
    A.alSourcei(src[k], AL_DIRECT_FILTER, (int)AC.filter[k]);
    A.alSource3i(src[k], AL_AUXILIARY_SEND_FILTER, (int)AC.slot, 0, (int)AC.filter[k]);
}
/* a sound starts on source k: its place kept, its filter from its own first path (never an older sound's) */
static void ac_source_start(int k, float x, float y, float z, int here)
{
    if (!AC.on) return;
    AC.pos[k][0] = x, AC.pos[k][1] = y, AC.pos[k][2] = z, AC.here[k] = (unsigned char)here, AC.moved[k] = 0;
    ac_path(k, AC.gt[k]);
    memcpy(AC.g[k], AC.gt[k], sizeof AC.g[k]);
    ac_filter_apply(k);
    if (lbfile && !here)   /* (the checks' window) */
        fprintf(stderr, "fx: acoustics: path of %d from (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f): %.3f %.3f %.3f\n", src_fx[k], x, y, z, lis[0], lis[1], lis[2],
                AC.g[k][0], AC.g[k][1], AC.g[k][2]);
}
/* dt seconds passed: the room and the paths asked again every 100 ms, the values eased */
static void ac_step(float dt)
{
    int st;
    if (!AC.on || dt <= 0) return;
    AC.acc += dt;
    if (HI.on && AC.rate > 0) {                      /* Acoustics High in a hidden run: the rays every 100 ms rendered, the stream fed */
        HI.sim_acc += dt;
        if (HI.sim_acc >= AC_PROBE_S) HI.sim_acc = 0, uwst_simulate();
        hi_feed();
    }
    if (AC.acc >= AC_PROBE_S || !AC.have_room) {
        AC.acc = 0;
        ac_room();
        for (int k = 0; k < NSRC; k++) {
            A.alGetSourcei(src[k], AL_SOURCE_STATE, &st);
            if (st == AL_PLAYING) ac_path(k, AC.gt[k]);
        }
    }
    float a = TU.reverb_tc > 1e-3f ? 1.0f - expf(-dt / TU.reverb_tc) : 1.0f, b = TU.filter_tc > 1e-3f ? 1.0f - expf(-dt / TU.filter_tc) : 1.0f;
    for (int i = 0; i < 5; i++) AC.cur[i] += (AC.tgt[i] - AC.cur[i]) * a;
    if (AC.have_room) ac_reverb_apply(0);
    for (int k = 0; k < NSRC; k++) {
        float d = 0;
        for (int i = 0; i < 3; i++) d += fabsf(AC.gt[k][i] - AC.g[k][i]);
        if (d < 1e-4f) continue;
        A.alGetSourcei(src[k], AL_SOURCE_STATE, &st);
        if (st != AL_PLAYING) continue;
        for (int i = 0; i < 3; i++) AC.g[k][i] += (AC.gt[k][i] - AC.g[k][i]) * b;
        ac_filter_apply(k);
    }
}
/* the dials (TU, above) read from settings.txt; the makeup alone when the acoustics are not on */
static void tune_read(const char *datadir)
{
    TU.makeup_stereo = clampf(setting_float(datadir, "fx_makeup_stereo", 215.0f) / 100.0f, 0.1f, 20.0f);
    TU.makeup_hrtf = clampf(setting_float(datadir, "fx_makeup_hrtf", 520.0f) / 100.0f, 0.1f, 20.0f);
    TU.hi_level = clampf(setting_float(datadir, "acoustics_echo_level", 5.0f) / 100.0f, 0, 1.0f);
    TU.decay = clampf(setting_float(datadir, "acoustics_decay", 100.0f) / 100.0f, 0.05f, 10.0f);
    TU.decay_hf = clampf(setting_float(datadir, "acoustics_decay_hf", 100.0f) / 100.0f, 0.05f, 10.0f);
    TU.decay_lf = clampf(setting_float(datadir, "acoustics_decay_lf", 100.0f) / 100.0f, 0.05f, 10.0f);
    TU.refl_delay = clampf(setting_float(datadir, "acoustics_refl_delay", 100.0f) / 100.0f, 0, 10.0f);
    TU.late_delay = clampf(setting_float(datadir, "acoustics_late_delay", 100.0f) / 100.0f, 0, 10.0f);
    TU.reverb_gain = clampf(setting_float(datadir, "acoustics_reverb_gain", 32.0f) / 100.0f, 0, 1.0f);
    TU.reverb_tc = clampf(setting_float(datadir, "acoustics_ease_ms", 170.0f) / 1000.0f, 0, 5.0f);
    TU.filter_tc = clampf(setting_float(datadir, "acoustics_filter_ms", 300.0f) / 1000.0f, 0, 5.0f);
    TU.bounces = (int)clampf(setting_float(datadir, "acoustics_bounces", 0), 0, 64.0f);
    TU.ahead = (int)clampf(setting_float(datadir, "acoustics_echo_ahead", 4.0f), 2.0f, 6.0f);
    TU.direct = setting_float(datadir, "acoustics_echo_direct", 1.0f) != 0;
    TU.sdirect = setting_float(datadir, "acoustics_steam_direct", 1.0f) != 0;
    TU.socc = setting_float(datadir, "acoustics_steam_occlusion", 1.0f) != 0;
    TU.dlevel = clampf(setting_float(datadir, "acoustics_direct_level", 30.0f) / 100.0f, 0, 2.0f);
    TU.src_m = clampf(setting_float(datadir, "acoustics_source_cm", 50.0f) / 100.0f, 0.05f, 3.0f);
    uwst_direct_level(TU.dlevel);
    TU.spath = setting_float(datadir, "acoustics_steam_pathing", 1.0f) != 0;
    TU.plevel = clampf(setting_float(datadir, "acoustics_path_level", 100.0f) / 100.0f, 0, 4.0f);
    uwst_pathing(TU.spath, TU.plevel);
    {   /* acoustics_door_frames 1: a door's frame stands in both ray codes (its opening half a tile wide, 13/16 high) */
        int fr = setting_float(datadir, "acoustics_door_frames", 1.0f) != 0;
        uwac_frames(fr), uwst_frames(fr);
        fr = setting_float(datadir, "acoustics_things", 1.0f) != 0;   /* the furniture and pillars stand for the sound */
        uwac_things_on(fr), uwst_things_on(fr);
    }
    uwac_tune((int)setting_float(datadir, "acoustics_room_rays", 64.0f), (int)setting_float(datadir, "acoustics_room_legs", 12.0f),
              setting_float(datadir, "acoustics_room_reach", 16.0f), setting_float(datadir, "acoustics_way_reach", 12.0f));
    uwst_ear(setting_float(datadir, "acoustics_echo_first", 1.0f) != 0);
    uwst_scatter(clampf(setting_float(datadir, "acoustics_scatter", 25.0f) / 100.0f, 0, 1.0f));
    uwst_length(clampf(setting_float(datadir, "acoustics_echo_ms", 2000.0f) / 1000.0f, 0.25f, 6.0f));
}
static void ac_settings(const char *datadir)
{
    tune_read(datadir);
    AC.wet = clampf(setting_float(datadir, "acoustics_wet", 100.0f) / 100.0f, 0, 2.0f);
    AC.occl = clampf(setting_float(datadir, "acoustics_occlusion", 100.0f) / 100.0f, 0, 1.0f);
    AC.tile_m = clampf(setting_float(datadir, "acoustics_tile_m", 2.4f), 0.5f, 10.0f);
    AC.detour = setting_float(datadir, "acoustics_detour", 1.0f) != 0;
    uwac_tile_m(AC.tile_m);
    uwst_direct_mode(TU.sdirect, TU.socc && AC.occl > 0, TU.src_m, 16, 8);   /* (Muffling by walls Off: none of Steam Audio's either) */
    {   /* Acoustics High's rays and bounces a run (acoustics_quality 0-3; 1 as it was) */
        static const int RAYS[4] = {1024, 2048, 4096, 8192}, BOUNCES[4] = {8, 16, 32, 64};
        int q = (int)clampf(setting_float(datadir, "acoustics_quality", 1.0f), 0, 3.0f);
        uwst_quality(RAYS[q], TU.bounces > 0 ? TU.bounces : BOUNCES[q]);
    }
}
/* after the sources are made: the slot, the reverb, the filters, the sends; 0 and one log line when it is not to be */
static int ac_start(const char *datadir)
{
    const char *e = getenv("UWPORT_FX_ACOUSTICS");   /* 0 / 1 (the checks): whatever the settings say */
    int want = e && *e ? atoi(e) : (int)setting_float(datadir, "fx_acoustics", 0);   /* 1 On, 2 High (Steam Audio's reflections) */
    AC.on = AC.have_room = 0, AC.acc = 0;
    memset(AC.cur, 0, sizeof AC.cur), memset(AC.tgt, 0, sizeof AC.tgt), memset(AC.set, 0, sizeof AC.set), memset(AC.here, 0, sizeof AC.here), memset(AC.moved, 0, sizeof AC.moved);
    if (!want) return 0;
    if (!A.alGetProcAddress || !A.alSource3i || !A.alcIsExtensionPresent(dev, "ALC_EXT_EFX")) {
        fprintf(stderr, "fx: acoustics off (no EFX)\n");
        return 0;
    }
#define GET(n) (*(void **)&AC.n = A.alGetProcAddress(#n)) != NULL
    if (!(GET(alGenEffects) && GET(alDeleteEffects) && GET(alGenAuxiliaryEffectSlots) && GET(alDeleteAuxiliaryEffectSlots) &&
          GET(alGenFilters) && GET(alDeleteFilters) && GET(alEffecti) && GET(alAuxiliaryEffectSloti) && GET(alFilteri) &&
          GET(alEffectf) && GET(alAuxiliaryEffectSlotf) && GET(alFilterf))) {
        fprintf(stderr, "fx: acoustics off (no EFX)\n");
        return 0;
    }
#undef GET
    uwacmat_write_default(datadir);                  /* data\acoustics.txt: the table we ship, when there is none yet */
    if (!uwacmat_load(datadir) && !uwacmat_load_default()) {
        fprintf(stderr, "fx: acoustics off (no materials: data\\acoustics.txt)\n");
        return 0;
    }
    A.alGetError();
    AC.alGenAuxiliaryEffectSlots(1, &AC.slot);
    AC.alGenEffects(1, &AC.effect);
    AC.alGenFilters(NSRC, AC.filter);
    AC.alEffecti(AC.effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
    if (A.alGetError()) {
        fprintf(stderr, "fx: acoustics off (no EFX reverb)\n");
        return 0;
    }
    ac_settings(datadir);
    AC.alAuxiliaryEffectSlotf(AC.slot, AL_EFFECTSLOT_GAIN, 0);
    for (int k = 0; k < NSRC; k++) {
        AC.alFilteri(AC.filter[k], AL_FILTER_TYPE, AL_FILTER_BANDPASS);
        A.alSource3i(src[k], AL_AUXILIARY_SEND_FILTER, (int)AC.slot, 0, AL_FILTER_NULL);
        AC.g[k][0] = AC.g[k][1] = AC.g[k][2] = AC.gt[k][0] = AC.gt[k][1] = AC.gt[k][2] = 1.0f;
    }
    QueryPerformanceCounter(&AC.clock);
    AC.on = 1;
    fprintf(stderr, "fx: acoustics on (EFX)\n");
    if (want >= 2) hi_start();
    return 1;
}
static void ac_stop(void)   /* (the context current, before the sources go) */
{
    if (!AC.on) return;
    hi_stop();
    for (int k = 0; k < NSRC; k++) {
        A.alSourcei(src[k], AL_DIRECT_FILTER, AL_FILTER_NULL);
        A.alSource3i(src[k], AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
    }
    AC.alAuxiliaryEffectSloti(AC.slot, AL_EFFECTSLOT_EFFECT, 0);
    AC.alDeleteAuxiliaryEffectSlots(1, &AC.slot);
    AC.alDeleteEffects(1, &AC.effect);
    AC.alDeleteFilters(NSRC, AC.filter);
    AC.on = 0;
}

void uwaudio_acoustics(const UwaTile *t, int ceil_mat, const UwaDoor *d, int nd)
{
    if (t) uwac_grid(t, ceil_mat, AC.tile_m > 0 ? AC.tile_m : 2.4f);
    uwac_doors(d, nd);
    if (HI.on) uwst_level(t, ceil_mat, d, nd, AC.tile_m);
}
void uwaudio_things(const UwaBox *b, int n)
{
    uwac_things(b, n);
    uwst_things(b, n);
}
void uwaudio_tune(const char *datadir)
{
    if (on) {   /* (the positional sound's own rows: no acoustics needed) */
        const char *fe = getenv("UWPORT_FX_FEET");
        footsteps = setting_get(datadir, "fx_footsteps", 1) != 0;
        if (!fe || !*fe) feet_spread = clampf(setting_float(datadir, "fx_feet_spread", 100.0f) / 100.0f, 0, 8.0f);
        hear_range = (int)clampf(setting_float(datadir, "fx_hear_range", 6.0f), 6.0f, 32.0f);
        step_vol = clampf(setting_float(datadir, "fx_step_volume", 80.0f) / 100.0f, 0, 2.0f);
        step_echo = clampf(setting_float(datadir, "fx_step_echo", 50.0f) / 100.0f, 0, 2.0f);
        step_plain = setting_get(datadir, "fx_step_plain", 0) != 0;
        for (int i = 0; i < NSRC; i++) A.alSourcef(src[i], AL_MAX_DISTANCE, (float)hear_range);
        if (!AC.on) tune_read(datadir);
    }
    if (!AC.on) { if (on) A.alListenerf(AL_GAIN, listener_gain()); return; }
    ac_settings(datadir);
    A.alListenerf(AL_GAIN, listener_gain());         /* (the makeup's rows) */
    if (HI.on) {
        /* the longest ring is the response's size, taken when Steam Audio starts: started again when the row changed
           (the level's tiles are kept there; the sounds ringing lose their echoes) */
        float want = clampf(setting_float(datadir, "acoustics_echo_ms", 2000.0f) / 1000.0f, 0.25f, 6.0f);
        if (uwst_length(want)) hi_stop(), hi_start();
    }
    if (HI.on) uwst_tile_m(AC.tile_m), A.alSourcei(HI.src, AL_DIRECT_CHANNELS_SOFT, TU.direct), A.alGetError();
    ac_room();
    if (AC.have_room) ac_reverb_apply(1);
    for (int k = 0; k < NSRC; k++) {   /* the muffling's new strength at once */
        ac_path(k, AC.gt[k]);
        memcpy(AC.g[k], AC.gt[k], sizeof AC.g[k]);
        ac_filter_apply(k);
    }
}
int uwaudio_acoustics_on(void) { return AC.on ? (HI.on ? 2 : 1) : 0; }
/* what our rays measure where the listener stands now (the menu's read-only row; in Acoustics High too, where the
   EFX reverb these numbers drive is silent): the ring time by band, the room's mean free path and the mean distance
   to the first surface, metres. 0: no level yet, or the listener in rock. */
int uwaudio_room(float rt60[3], float *mfp_m, float *first_m)
{
    UwaRoom r;
    if (!on || !uwac_room(lis, &r)) return 0;
    if (rt60) memcpy(rt60, r.rt60, sizeof r.rt60);
    if (mfp_m) *mfp_m = r.mfp_m;
    if (first_m) *first_m = r.first_m;
    return 1;
}
float uwaudio_acoustic_decay(void) { return AC.on && AC.have_room ? AC.cur[0] : 0; }

/* the context made: the sources, the original's falloff */
static int start(const char *datadir)
{
    if (!ctx || !A.alcMakeContextCurrent(ctx)) return 0;
    A.alGetError();
    A.alGenSources(NSRC, src);
    if (A.alGetError()) return 0;
    A.alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    for (int i = 0; i < NSRC; i++) {
        A.alSourcef(src[i], AL_REFERENCE_DISTANCE, 1.0f);   /* full within a tile (play_effect's dist < 8) */
        A.alSourcef(src[i], AL_MAX_DISTANCE, (float)hear_range);         /* nothing beyond six (dist > 0x30) */
        A.alSourcef(src[i], AL_ROLLOFF_FACTOR, 1.0f);       /* linear between: v * (0x30 - dist) / 0x28 */
        A.alSourcef(src[i], AL_MAX_GAIN, 4.0f);             /* (a louder velocity's guess, before its rendering) */
        src_fx[i] = -1;
    }
    snprintf(sfx_dir, sizeof sfx_dir, "%s" UWOS_SEP "sfx", datadir ? datadir : ".");
    memset(buf, 0, sizeof buf), memset(buf_state, 0, sizeof buf_state);
    memset(step_buf, 0, sizeof step_buf), memset(step_state, 0, sizeof step_state);
    /* (2026-11-16, the review: a data\sfx\NNN.wav was found at the effect's first play and never told to the game,
       so without the Roland's renderings, no ROMs or another device, the wav and the driver's own copy both played.
       Looked for now, and the effects that have one are ours whatever the device: uwaudio_taken) */
    wav_mask = 0;
    for (int fx = 0; fx < 64; fx++) {
        char wp[MAX_PATH];
        snprintf(wp, sizeof wp, "%s" UWOS_SEP "%03d.wav", sfx_dir, fx);
        buf_state[fx] = wav_load(wp, &buf[fx]) ? 1 : -1;
        if (buf_state[fx] == 1) wav_mask |= 1ull << fx;
    }
    {   /* the footsteps' materials: data\acoustics.txt when it is there, else the table in the code (nothing written
           here: the acoustics write the file when they start) */
        char ap[MAX_PATH];
        const char *fe = getenv("UWPORT_FX_FOOTSTEPS");   /* 0 / 1 (the checks) */
        snprintf(ap, sizeof ap, "%s" UWOS_SEP "acoustics.txt", datadir ? datadir : ".");
        if (GetFileAttributesA(ap) == INVALID_FILE_ATTRIBUTES || !uwacmat_load(datadir)) uwacmat_load_default();
        footsteps = fe && *fe ? atoi(fe) != 0 : setting_get(datadir, "fx_footsteps", 1) != 0;
        fe = getenv("UWPORT_FX_FEET");   /* percent of the game's own pan */
        feet_spread = clampf((fe && *fe ? (float)atof(fe) : setting_float(datadir, "fx_feet_spread", 100.0f)) / 100.0f, 0, 8.0f);
        hear_range = (int)clampf(setting_float(datadir, "fx_hear_range", 6.0f), 6.0f, 32.0f);
        step_vol = clampf(setting_float(datadir, "fx_step_volume", 80.0f) / 100.0f, 0, 2.0f);
        step_echo = clampf(setting_float(datadir, "fx_step_echo", 50.0f) / 100.0f, 0, 2.0f);
        step_plain = setting_get(datadir, "fx_step_plain", 0) != 0;
    }
    tune_read(datadir);
    A.alListenerf(AL_GAIN, listener_gain());   /* (again by hrtf_read, once the device says whether HRTF is on) */
    on = 1;
    mt_start(datadir);
    ac_start(datadir);   /* (the acoustics, when asked for and possible: the sound stands without them) */
    return 1;
}

static void lb_close(void)
{
    if (lbfile) {   /* the header over the 44 zero bytes written at the start */
        unsigned n = lbframes * 4, h[11] = {0x46464952, 36 + n, 0x45564157, 0x20746d66, 16, 0x00020001, lbrate, lbrate * 4, 0x00100004, 0x61746164, n};
        fseek(lbfile, 0, SEEK_SET), fwrite(h, 4, 11, lbfile), fclose(lbfile), lbfile = NULL;
        fprintf(stderr, "fx: loopback: %u frames written\n", lbframes);
    }
}

static void stop_all(void)
{
    mt_stop();
    lb_close();
    if (ctx) {
        A.alcMakeContextCurrent(ctx);
        if (on) {
            for (int i = 0; i < NSRC; i++) A.alSourceStop(src[i]);
            ac_stop();
            pcm_forget();
            for (int i = 0; i < nplain; i++) A.alDeleteBuffers(1, &plain_of[i].stereo);
            nplain = 0;
            A.alDeleteSources(NSRC, src);
            for (int i = 0; i < 256; i++)
                if (buf_state[i] == 1) A.alDeleteBuffers(1, &buf[i]);   /* (a rendering's buffer is the Rend's) */
            for (int i = 0; i < 2 * STEP_PRESETS; i++)
                if (step_state[i] == 1) A.alDeleteBuffers(1, &step_buf[i]);
            for (int i = 0; i < 64; i++)
                for (int v = 0; v < 128; v++)
                    if (rend[i][v].albuf) A.alDeleteBuffers(1, &rend[i][v].albuf), rend[i][v].albuf = 0;
        }
        A.alcMakeContextCurrent(NULL);
        A.alcDestroyContext(ctx);
    }
    if (dev) A.alcCloseDevice(dev);
    ctx = dev = NULL, on = 0;
}

int uwaudio_init(const char *bindir, const char *datadir)
{
    if (on) return 1;
    if (!setting_on(datadir, "fx_audio")) return 0;
    if (!A.dll && !load_dll(bindir)) {
        fprintf(stderr, "fx: audio off (no soft_oal.dll)\n");
        return 0;
    }
    /* never in a hidden run (the checks: nothing heard on the machine; tests\uwaudio_test.c has the loopback), unless
       the check asks for the loopback device rendered into a WAV file (UWPORT_FX_LOOPBACK=FILE, uwaudio_pump) */
    if (getenv("UWPORT_OFFSCREEN")) {
        const char *lb = getenv("UWPORT_FX_LOOPBACK");
        if (lb && *lb && uwaudio_init_loopback(bindir, datadir, 48000) && (lbfile = fopen(lb, "wb")) != NULL) {
            static const unsigned char zero[44];
            fwrite(zero, 1, 44, lbfile), lbframes = 0, lbrate = 48000;
            QueryPerformanceFrequency(&lbfreq), QueryPerformanceCounter(&lbtime);
            fprintf(stderr, "fx: audio on (OpenAL Soft loopback to %s)\n", lb);
            return 1;
        }
        if (on) stop_all();
        fprintf(stderr, "fx: audio off (a hidden run)\n");
        return 0;
    }
    {
        int attrs[16];
        dev = A.alcOpenDevice(NULL);
        AC.rate = 0;
        ctx = dev ? A.alcCreateContext(dev, context_attrs(datadir, NULL, attrs)) : NULL;
    }
    if (!ctx || !start(datadir)) {
        stop_all();
        fprintf(stderr, "fx: audio off (no sound device)\n");
        return 0;
    }
    fprintf(stderr, "fx: audio on (OpenAL Soft, %s)\n", sfx_dir);
    hrtf_read();
    return 1;
}

int uwaudio_init_loopback(const char *bindir, const char *datadir, int rate)
{
    PFopen lb;
    int fmt[] = {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT, ALC_SHORT_SOFT, ALC_FREQUENCY, rate, 0}, attrs[16];
    if (on) return 1;
    if (!A.dll && !load_dll(bindir)) {
        fprintf(stderr, "fx: audio off (no soft_oal.dll)\n");
        return 0;
    }
    if (!A.alcIsExtensionPresent(NULL, "ALC_SOFT_loopback")) return 0;
    lb = (PFopen)A.alcGetProcAddress(NULL, "alcLoopbackOpenDeviceSOFT");
    A.render = (PFrender)A.alcGetProcAddress(NULL, "alcRenderSamplesSOFT");
    if (!lb || !A.render) return 0;
    dev = lb(NULL);
    AC.rate = rate;
    ctx = dev ? A.alcCreateContext(dev, context_attrs(datadir, fmt, attrs)) : NULL;
    if (!ctx || !start(datadir)) { stop_all(); return 0; }
    hrtf_read();
    return 1;
}

/* frames through the loopback device; with the acoustics on in slices of 20 ms, their values stepped between (so a
   filter eases inside one call's rendering as it does from frame to frame in the game) */
static void render_frames(short *stereo, int frames)
{
    if (!AC.on || AC.rate <= 0) { A.render(dev, stereo, frames); return; }
    while (frames > 0) {
        int k = frames > AC_SLICE ? AC_SLICE : frames;
        ac_step((float)k / (float)AC.rate);
        A.render(dev, stereo, k);
        stereo += 2 * k, frames -= k;
    }
}

int uwaudio_render(short *stereo, int frames)
{
    if (!on || !A.render) return 0;
    render_frames(stereo, frames);
    return 1;
}

int uwaudio_error(void) { return on ? A.alGetError() : 0; }
int uwaudio_on(void) { return on; }

int uwaudio_playing(void)
{
    int n = 0, st;
    for (int i = 0; on && i < NSRC; i++) {
        A.alGetSourcei(src[i], AL_SOURCE_STATE, &st);
        n += st == AL_PLAYING;
    }
    return n;
}

void uwaudio_listener(float x, float y, float z, float yaw)
{
    float o[6] = {sinf(yaw), 0, -cosf(yaw), 0, 1, 0};   /* at (north is -z in OpenAL's frame), up */
    if (!on) return;
    /* (the acoustics: a jump of more than a tile, a teleport or a level's stairs, asks for the new room at once) */
    if (AC.on && (x - lis[0]) * (x - lis[0]) + (z - lis[2]) * (z - lis[2]) > 1.0f) AC.acc = AC_PROBE_S;
    lis[0] = x, lis[1] = y, lis[2] = z, lis_yaw = yaw;
    if (HI.on) uwst_listener(x, y, z, yaw);
    A.alListener3f(AL_POSITION, x, y, -z);
    A.alListenerfv(AL_ORIENTATION, o);
}

/* a little-endian RIFF WAVE file, PCM 8 or 16 bits, mono or stereo, into an OpenAL buffer; 0 when it is not one */
static unsigned rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
static int wav_load(const char *path, unsigned *out)
{
    FILE *f = fopen(path, "rb");
    unsigned char *d;
    long n;
    int ok = 0, ch = 0, bits = 0, rate = 0;
    if (!f) return 0;
    fseek(f, 0, SEEK_END), n = ftell(f), fseek(f, 0, SEEK_SET);
    if (n < 44 || n > (64 << 20) || !(d = malloc((size_t)n))) { fclose(f); return 0; }
    if (fread(d, 1, (size_t)n, f) == (size_t)n && !memcmp(d, "RIFF", 4) && !memcmp(d + 8, "WAVE", 4)) {
        long p = 12;
        while (p + 8 <= n) {
            unsigned len = rd32(d + p + 4);
            if (len > (unsigned)(n - p - 8)) len = (unsigned)(n - p - 8);
            if (!memcmp(d + p, "fmt ", 4) && len >= 16) {
                if ((d[p + 8] | d[p + 9] << 8) != 1) break;   /* PCM only */
                ch = d[p + 10] | d[p + 11] << 8, rate = (int)rd32(d + p + 12), bits = d[p + 22] | d[p + 23] << 8;
            } else if (!memcmp(d + p, "data", 4) && ch && rate > 0 && (bits == 8 || bits == 16) && (ch == 1 || ch == 2)) {
                int fmt = ch == 1 ? (bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16) : (bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16);
                A.alGetError();
                A.alGenBuffers(1, out);
                A.alBufferData(*out, fmt, d + p + 8, (int)len, rate);
                ok = A.alGetError() == 0;
                if (ok && ch == 1 && bits == 16) pcm_keep(*out, (const short *)(d + p + 8), (int)len / 2, rate, 1);
                if (!ok) A.alDeleteBuffers(1, out);
                if (ok && ch == 2) fprintf(stderr, "fx: %s is stereo: OpenAL plays it unplaced\n", path);
                break;
            }
            p += 8 + len + (len & 1);
        }
    }
    free(d), fclose(f);
    return ok;
}

/* the OpenAL buffer of a rendering (made on first use); 0 when it is not rendered yet or cannot be made */
static unsigned rend_buffer(Rend *r)
{
    if (r->state != 2) return 0;
    if (!r->albuf) {
        A.alGetError();
        A.alGenBuffers(1, &r->albuf);
        A.alBufferData(r->albuf, AL_FORMAT_MONO16, r->pcm, r->frames * 2, (int)M.rate);
        if (A.alGetError() != 0) A.alDeleteBuffers(1, &r->albuf), r->albuf = 0;
        else pcm_keep(r->albuf, r->pcm, r->frames, (int)M.rate, 0);
    }
    return r->albuf;
}

static unsigned plain_stereo(unsigned al)
{
    for (int i = 0; i < nplain; i++) if (plain_of[i].mono == al) return plain_of[i].stereo;
    if (nplain >= 64) return 0;
    for (int i = 0; i < npcm; i++)
        if (pcm_of[i].al == al) {
            unsigned b = 0;
            short *st = malloc(sizeof *st * 2 * (size_t)pcm_of[i].frames);
            if (!st) return 0;
            for (int k = 0; k < pcm_of[i].frames; k++) st[2 * k] = st[2 * k + 1] = pcm_of[i].pcm[k];
            A.alGetError();
            A.alGenBuffers(1, &b);
            A.alBufferData(b, AL_FORMAT_STEREO16, st, pcm_of[i].frames * 4, pcm_of[i].rate);
            free(st);
            if (A.alGetError() != 0) { A.alDeleteBuffers(1, &b); return 0; }
            plain_of[nplain].mono = al, plain_of[nplain++].stereo = b;
            return b;
        }
    return 0;
}
/* the window's thread asks the worker for effect fx at velocity vel (once) */
static void rend_ask(int fx, int vel)
{
    Rend *r = &rend[fx][vel];
    if (InterlockedCompareExchange(&r->state, 1, 0) != 0) return;
    if (ask_head - ask_tail >= ASK_RING) { InterlockedExchange(&r->state, 0); return; }
    ask_fx[ask_head % ASK_RING] = fx, ask_vel[ask_head % ASK_RING] = vel;
    InterlockedIncrement(&ask_head);
    if (M.asked) ReleaseSemaphore(M.asked, 1, NULL);
}

int uwaudio_rendered(int fx, int vel)
{
    return on && fx >= 0 && fx < 64 && vel >= 0 && vel < 128 && rend[fx][vel].state == 2;
}

void uwaudio_event(int kind, float x, float y, float z, int fx, int vol)
{
    char p[MAX_PATH];
    int k = -1, st, base = -1, by_vel = 0, pan = (kind >> 16 & 0xFF) - 1;   /* (pan: play_effect_here's, -1 none) */
    unsigned use = 0;
    float g = 1.0f;
    if (!on || fx < 0 || fx > 255) return;
    if ((kind & 0xFF) == FX_STOP) {   /* the game silenced it (kill_effect): our copy stops too (the water, 25 s long) */
        if (lbfile) fprintf(stderr, "fx: al stop %d at frame %u\n", fx, lbframes);   /* (the checks' window) */
        /* (2026-11-16, the review: the game ends ONE of its four slots, and every copy of that effect we played was
           stopped. The one started longest ago and still sounding goes, as the game's slots are given in turn; one
           that has rung out by itself is only forgotten.) */
        {
            int old = -1;
            for (int i = 0; i < NSRC; i++)
                if (src_fx[i] == fx) {
                    A.alGetSourcei(src[i], AL_SOURCE_STATE, &st);
                    if (st != AL_PLAYING) src_fx[i] = -1;
                    else if (old < 0 || src_used[i] < src_used[old]) old = i;
                }
            if (old >= 0) {
                A.alSourceStop(src[old]);
                src_fx[old] = -1;
                if (HI.on && HI.voice[old] >= 0) uwst_end(HI.voice[old]), HI.voice[old] = -1;
            }
        }
        return;
    }
    if (kind & FX_MUTED) return;
    if (fx < M.nsounds) base = M.sounds[1 + fx * 5 + 2];
    if (vol > 0x7F) vol = 0x7F;
    if (x >= 0) {
        /* play_effect's distance (2026-11-15, "I could hear the rat behind the door well before I got near it"):
           nothing beyond six tiles (dist > 0x30, in eighths of a tile), and from one tile to six it is the NOTE'S
           VELOCITY that falls (v * (0x30 - dist) / 0x28), fixed when the note starts. We had OpenAL fade the level
           by that line instead, and the Roland's notes fall far faster than their velocity (effect 4: 72 -> 609,
           0x7F -> 3603, about the cube): a rat's steps five tiles off came 10 dB under the player's own where the
           original plays them at velocity 12 of 64. So: the rendering at the original's velocity (by_vel), and no
           fall of OpenAL's on top. Without a velocity (no SOUNDS.DAT, vol -1 and no base) OpenAL's line as before. */
        float dx = x - lis[0], dz = z - lis[2];
        int dist = (int)(sqrtf(dx * dx + dz * dz) * 8.0f);
        int far8 = hear_range * 8;   /* (6 tiles: the original's 0x30) */
        if (dist > far8) return;
        if (vol < 0 && base >= 0) vol = base;
        if (vol >= 0) {
            by_vel = 1;
            if (dist >= 8) vol = vol * (far8 - dist) / (far8 - 8);
            if (vol <= 0) return;   /* (a note at velocity 0 is no note) */
        }
    }
    /* a footstep, by the floor under the one who steps (Task 6) */
    const UwaMaterial *floor = NULL;
    int fmat = -1;
    int plain = step_plain && (fx == 1 || fx == 2) && x < 0;   /* the player's own step, as the rendering is */
    if ((fx == 1 || fx == 2) && footsteps && !plain) {
        float at[3] = {x < 0 ? lis[0] : x, x < 0 ? lis[1] : y, x < 0 ? lis[2] : z};
        fmat = uwac_floor_mat(at);
        if (fmat >= 0 && fmat < STEP_PRESETS) floor = uwacmat_get(fmat);
    }
    if (floor) {   /* the user's own sound for that floor */
        int i = (fx - 1) * STEP_PRESETS + fmat;
        if (!step_state[i]) {
            snprintf(p, sizeof p, "%s" UWOS_SEP "%03d-%s.wav", sfx_dir, fx, floor->name);
            step_state[i] = wav_load(p, &step_buf[i]) ? 1 : -1;
        }
        if (step_state[i] == 1) use = step_buf[i];
    }
    if (!buf_state[fx]) {
        snprintf(p, sizeof p, "%s" UWOS_SEP "%03d.wav", sfx_dir, fx);
        buf_state[fx] = wav_load(p, &buf[fx]) ? 1 : -1;
    }
    if (use) {   /* (the floor's own wav: the velocity as a gain by the same guess) */
        if (vol >= 0 && base > 0 && vol != base) g = powf((float)vol / (float)base, VEL_GAIN);
    } else if (buf_state[fx] == 1) {   /* a wav of the user's: the velocity as a gain by the guess below */
        use = buf[fx];
        if (vol >= 0 && base > 0 && vol != base) g = powf((float)vol / (float)base, VEL_GAIN);
    } else if (fx < 64 && base >= 0 && !stand_down && (InterlockedOr64(&M.ready, 0) >> fx & 1)) {   /* Munt's renderings */
        if (floor && floor->step) {   /* the note moved for this floor: its own rendering (until it is made, the usual one) */
            int v = vol >= 0 ? vol : base;
            if (floor->absorb[2] > 0.5f) v /= 2;
            use = rend_buffer(&rend[STEP_SLOT(fx, fmat)][v]);
            if (!use) rend_ask(STEP_SLOT(fx, fmat), v);
        }
        if (use) buf_state[fx] = 2;
        else
        if (vol >= 0 && vol != base) {   /* the game's velocity: its own rendering, or until that is made the base one scaled by a guess */
            use = rend_buffer(&rend[fx][vol]);
            if (!use) rend_ask(fx, vol), g = powf((float)vol / (float)(base > 0 ? base : 1), VEL_GAIN);
        }
        if (!use) use = rend_buffer(&rend[fx][base]);
        if (use) buf_state[fx] = 2;
    }
    if (!use) return;
    /* a free source, else the one started longest ago */
    for (int i = 0; i < NSRC && k < 0; i++) {
        A.alGetSourcei(src[i], AL_SOURCE_STATE, &st);
        if (st != AL_PLAYING) k = i;
    }
    if (k < 0) {
        k = 0;
        for (int i = 1; i < NSRC; i++) if (src_used[i] < src_used[k]) k = i;
        A.alSourceStop(src[k]);
    }
    src_used[k] = ++src_clock, src_fx[k] = fx;
    if (lbfile) fprintf(stderr, "fx: al %d at frame %u (%.2f, %.2f) from (%.2f, %.2f) vol %d gain %.2f\n", fx, lbframes, x, z, lis[0], lis[2], vol, g);   /* (the checks' window) */
    A.alSourcei(src[k], AL_BUFFER, (int)use);
    A.alSourcei(src[k], AL_LOOPING, 0);
    if (fx == 1 || fx == 2) g *= step_vol;   /* (a footstep's own level) */
    A.alSourcef(src[k], AL_GAIN, g > 4.0f ? 4.0f : g);
    A.alSourcef(src[k], AL_ROLLOFF_FACTOR, by_vel ? 0.0f : 1.0f);   /* (the distance is in the velocity already) */
    if (x < 0) {   /* play_effect_here: at the listener */
        /* its pan (2026-11-15, "not playing both feet sounds"): the player's two feet are play_effect_here(1, 0x48)
           and (2, 0x38), a little right and a little left of the middle (0x40); ours had both dead centre, one
           sound. The side is the pan's share of the way to a side, a little ahead (a direction is all OpenAL
           needs; a relative source ignores the listener's turn). 0x40 and no pan: at the listener as before. */
        float s = pan >= 0 ? ((float)pan - 64.0f) / 64.0f * feet_spread : 0;
        s = s < -1 ? -1 : s > 1 ? 1 : s;
        A.alSourcei(src[k], AL_SOURCE_RELATIVE, 1);
        if (s != 0) A.alSource3f(src[k], AL_POSITION, s, 0, -sqrtf(1.0f - s * s));
        else A.alSource3f(src[k], AL_POSITION, 0, 0, 0);
    } else {
        A.alSourcei(src[k], AL_SOURCE_RELATIVE, 0);
        A.alSource3f(src[k], AL_POSITION, x, y, -z);
    }
    ac_source_start(k, x, y, z, x < 0);   /* (the acoustics: its own first filter, before it sounds) */
    /* (a plain step: the source's samples straight to the two channels, no HRTF's placing, no filter, no reverb) */
    {
        unsigned st = plain ? plain_stereo(use) : 0;
        if (!st) plain = 0;
        A.alSourcei(src[k], AL_DIRECT_CHANNELS_SOFT_, plain ? 1 : 0), A.alGetError();
        if (plain) {
            /* both channels whole: at the level a sound in the middle has without the HRTF (0.707 a side at the
               stereo makeup), whatever the makeup in force */
            float pg = g * 0.7071f * TU.makeup_stereo / makeup();
            A.alSourcei(src[k], AL_BUFFER, (int)st);
            A.alSourcef(src[k], AL_GAIN, pg > 4.0f ? 4.0f : pg);
        }
    }
    if (plain) {
        A.alSource3f(src[k], AL_POSITION, 0, 0, 0);
        if (AC.on) A.alSourcei(src[k], AL_DIRECT_FILTER, AL_FILTER_NULL), A.alSource3i(src[k], AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
        if (HI.on && HI.voice[k] >= 0) uwst_end(HI.voice[k]), HI.voice[k] = -1;
    } else {   /* Acoustics High: its echoes, and on headphones the sound itself (then our OpenAL copy plays silent) */
        float at[3] = {x, y, z}, sd = 0, fw = 0;
        if (x < 0) {   /* at the listener: its pan's side (the two feet) a third of a tile out, fixed to the head */
            sd = pan >= 0 ? clampf(((float)pan - 64.0f) / 64.0f * feet_spread, -1, 1) : 0, fw = sd != 0 ? sqrtf(1.0f - sd * sd) : 0;
            at[0] = lis[0] + (cosf(lis_yaw) * sd + sinf(lis_yaw) * fw) * 0.33f, at[1] = lis[1];
            at[2] = lis[2] + (-sinf(lis_yaw) * sd + cosf(lis_yaw) * fw) * 0.33f;
        } else uwac_lift(at);
        int took = hi_voice(k, use, at[0], at[1], at[2], g, fx == 1 || fx == 2 ? step_echo : 1.0f);
        if (x < 0 && HI.on && HI.voice[k] >= 0) uwst_relative(HI.voice[k], sd * 0.33f, fw * 0.33f);
        if (took) A.alSourcef(src[k], AL_GAIN, 0);
    }
    A.alSourcePlay(src[k]);
}

void uwaudio_shutdown(void)
{
    stop_all();
    if (A.dll) FreeLibrary(A.dll);
    memset(&A, 0, sizeof A);
}

/* the effects our audio plays itself now (bit n: SOUNDS.DAT entry n rendered and the module on): the backend hands it
   to the game (port_fx_control) so that the music driver's copy is silent */
/* uwaudio_stand_down(1): the game's own sound device is not the Roland (the Esc menu's "Music and effects device":
   Ad Lib, a Sound Blaster, the PC speaker), so the effects are that device's and our Roland renderings stay out of
   it: nothing is taken, the game's driver plays every effect itself (unplaced, as the original) */
void uwaudio_stand_down(int yes) { InterlockedExchange(&stand_down, yes != 0); }
unsigned long long uwaudio_taken(void)
{
    if (!on) return 0;
    return wav_mask | (stand_down ? 0 : (unsigned long long)InterlockedOr64(&M.ready, 0));
}

void uwaudio_volume(float g)
{
    gain = g < 0 ? 0 : g > 1 ? 1 : g;
    if (on) A.alListenerf(AL_GAIN, listener_gain());
}

/* the loopback WAV of a hidden check (UWPORT_FX_LOOPBACK): the frames of the time passed since the last call rendered
   through OpenAL into the file (a second at most a call) */
void uwaudio_pump(void)
{
    static short blk[4096 * 2];
    LARGE_INTEGER now;
    unsigned frames;
    if (on && AC.on && !lbfile) {   /* a sound card: the acoustics stepped by the clock (a tenth of a second at most a call) */
        QueryPerformanceFrequency(&lbfreq), QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - AC.clock.QuadPart) / (double)lbfreq.QuadPart;
        AC.clock = now;
        ac_step(dt > 0.1 ? 0.1f : (float)dt);
        return;
    }
    if (!on || !lbfile || !A.render) return;
    QueryPerformanceCounter(&now);
    frames = (unsigned)((double)(now.QuadPart - lbtime.QuadPart) * lbrate / (double)lbfreq.QuadPart);
    if (frames > lbrate) frames = lbrate;
    lbtime = now;
    while (frames) {
        unsigned k = frames > 4096 ? 4096 : frames;
        render_frames(blk, (int)k);
        fwrite(blk, 4, k, lbfile);
        lbframes += k, frames -= k;
    }
}
