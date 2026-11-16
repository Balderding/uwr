/* sound.c: see sound.h. */
#include "sound.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "orig.h"
#include "sfx.h"
#include "songs.h"
#include "xmidi.h"

/* --- Munt (mt32emu.dll, its C API, loaded at run time) --------------------------------------------------------- */
typedef void *MuntCtx;
static struct {
    HMODULE dll;
    MuntCtx (*create)(void *report, void *instance);
    int (*add_rom)(MuntCtx, const char *);
    int (*open)(MuntCtx);
    void (*close)(MuntCtx);
    void (*free_ctx)(MuntCtx);
    void (*parse)(MuntCtx, const u8 *, u32);
    void (*render)(MuntCtx, i16 *, u32);
    u32 (*rate)(MuntCtx);
} MU;

static bool munt_load(void) {
    if (MU.dll) return true;
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, sizeof path);
    char *slash = strrchr(path, '\\');
    if (slash) strcpy(slash + 1, "mt32emu.dll");
    MU.dll = LoadLibraryA(path);
    if (!MU.dll) return false;
    MU.create = (MuntCtx(*)(void *, void *))(void *)GetProcAddress(MU.dll, "mt32emu_create_context");
    MU.add_rom = (int (*)(MuntCtx, const char *))(void *)GetProcAddress(MU.dll, "mt32emu_add_rom_file");
    MU.open = (int (*)(MuntCtx))(void *)GetProcAddress(MU.dll, "mt32emu_open_synth");
    MU.close = (void (*)(MuntCtx))(void *)GetProcAddress(MU.dll, "mt32emu_close_synth");
    MU.free_ctx = (void (*)(MuntCtx))(void *)GetProcAddress(MU.dll, "mt32emu_free_context");
    MU.parse = (void (*)(MuntCtx, const u8 *, u32))(void *)GetProcAddress(MU.dll, "mt32emu_parse_stream");
    MU.render = (void (*)(MuntCtx, i16 *, u32))(void *)GetProcAddress(MU.dll, "mt32emu_render_bit16s");
    MU.rate = (u32(*)(MuntCtx))(void *)GetProcAddress(MU.dll, "mt32emu_get_actual_stereo_output_samplerate");
    if (!MU.create || !MU.add_rom || !MU.open || !MU.close || !MU.free_ctx || !MU.parse || !MU.render || !MU.rate) {
        FreeLibrary(MU.dll), MU.dll = NULL;
        return false;
    }
    return true;
}

/* the ROM pairs looked for in uo-new\roms, the CM-32L first */
static const char *const ROMS[][3] = {
    {"cm32l_ctrl_1_02.rom", "cm32l_pcm.rom", "Roland CM-32L"},
    {"CM32L_CONTROL.ROM", "CM32L_PCM.ROM", "Roland CM-32L"},
    {"mt32_ctrl_1_07.rom", "mt32_pcm.rom", "Roland MT-32"},
    {"MT32_CONTROL.ROM", "MT32_PCM.ROM", "Roland MT-32"},
};

static MuntCtx munt_open(const char *rom_dir, int *which) {
    for (int k = 0; k < COUNT(ROMS); k++) {
        char a[700], b[700];
        snprintf(a, sizeof a, "%s\\%s", rom_dir, ROMS[k][0]);
        snprintf(b, sizeof b, "%s\\%s", rom_dir, ROMS[k][1]);
        if (!file_exists(a) || !file_exists(b)) continue;
        MuntCtx c = MU.create(NULL, NULL);
        if (!c) return NULL;
        if (MU.add_rom(c, a) > 0 && MU.add_rom(c, b) > 0 && MU.open(c) == 0) {
            *which = k;
            return c;
        }
        MU.free_ctx(c);
    }
    return NULL;
}

/* --- the state ---------------------------------------------------------------------------------------------- */
enum { RATE = 32000, BUFS = 6, FRAMES = 512 };
static struct {
    bool on;
    CRITICAL_SECTION cs;
    MuntCtx music, effects;
    int roms;
    HWAVEOUT wo;
    HANDLE event, thread;
    volatile LONG stop;
    WAVEHDR hdr[BUFS];
    i16 buf[BUFS][FRAMES * 2];
    volatile float music_gain, effect_gain;
    double tick_left; /* frames until the driver's next service */
    double sfx_left;  /* frames until the effects' timer (16 a second, UW.EXE 0x1fe99) */
    /* the message being assembled from the driver's bytes */
    u8 msg[512];
    int msg_len, msg_need;
    /* the songs (data\music\NAME.xmi): the game's own choice (songs.c, UW.EXE seg 1bd8) */
    char data[600];
    u8 *xmi, *pending; /* the song the driver plays from; the one just read */
    size_t pending_size;
    Bytes uwmt;
} S;

/* a complete MIDI message from the driver: everything to both Munts, notes by their channel */
static void route(const u8 *m, int n) {
    int st = m[0];
    if ((st & 0xe0) == 0x80) { /* note off / on */
        MU.parse(xmidi_channel_locked(st & 0x0f) ? S.effects : S.music, m, (u32)n);
        return;
    }
    MU.parse(S.music, m, (u32)n);
    MU.parse(S.effects, m, (u32)n);
}
static void mpu_out(void *user, int port, int v) {
    (void)user;
    if (port != 0x330) return; /* an interface command (reset, UART mode): not MIDI */
    u8 b = (u8)v;
    if (b >= 0x80 && b != 0xf7) { /* a status byte starts a message */
        S.msg_len = 0;
        int t = b & 0xf0;
        S.msg_need = b == 0xf0 ? -1 : (t == 0xc0 || t == 0xd0) ? 2 : 3;
    }
    if (S.msg_len < (int)sizeof S.msg) S.msg[S.msg_len++] = b;
    if ((S.msg_need > 0 && S.msg_len == S.msg_need) || (S.msg_need == -1 && b == 0xf7)) {
        route(S.msg, S.msg_len);
        S.msg_len = 0, S.msg_need = 0;
    }
}
static void mpu_wait(void *user) { (void)user; } /* the retrace waits a real Roland needed: Munt takes bytes at once */

static Bytes read_heap(const char *path) { /* a whole file into the heap (kept: the driver points into it) */
    Bytes b = {0};
    FILE *f = fopen(path, "rb");
    if (!f) return b;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *d = malloc((size_t)n + 16);
    if (d && fread(d, 1, (size_t)n, f) == (size_t)n) b.data = d, b.size = (size_t)n;
    else free(d);
    fclose(f);
    return b;
}

/* --- the songs ------------------------------------------------------------------------------------------------- */
static const u8 *gtl_find(const Bytes *g, int bank, int patch) {
    for (size_t i = 0; i + 6 <= g->size; i += 6) {
        if (g->data[i + 1] == 0xff) break;
        if (g->data[i] == patch && g->data[i + 1] == bank) {
            u32 off = g->data[i + 2] | g->data[i + 3] << 8 | (u32)g->data[i + 4] << 16 | (u32)g->data[i + 5] << 24;
            return off < g->size ? g->data + off : NULL;
        }
    }
    return NULL;
}
/* the songs' driver calls (songs.c), under the lock */
static bool ail_file(const char *name) { /* 0x1f6db: the file, kept apart until it is registered */
    char path[700];
    snprintf(path, sizeof path, "%s\\music\\%s", S.data, name);
    Bytes b = read_heap(path);
    if (!b.data) return false;
    free(S.pending);
    S.pending = (u8 *)b.data, S.pending_size = b.size;
    return true;
}
static int ail_register(void) { /* 151; the old song (stopped and released by now) gives its memory back */
    int seq = S.pending ? xmidi_register(S.pending, S.pending_size, 0, NULL) : -1;
    if (seq >= 0) free(S.xmi), S.xmi = S.pending, S.pending = NULL;
    return seq;
}
static bool ail_install(int bank, int patch);
static int ail_rand(void) { return uw_rand(); }
static const SongsAil SONGS_AIL = {ail_file,   ail_register, xmidi_release,        xmidi_timbre_request, ail_install, xmidi_start,
                                   xmidi_stop, xmidi_status, xmidi_set_rel_volume, xmidi_set_rel_tempo,  ail_rand};

/* --- the effects (sfx.c: UW.EXE's own) through the driver ---------------------------------------------------- */
static int ail_tstat(int bank, int patch) { return xmidi_timbre_status(bank, patch); }
static bool ail_install(int bank, int patch) { /* UW.EXE 0x1f341: the timbre from the library into the driver */
    const u8 *t = gtl_find(&S.uwmt, bank, patch);
    if (!t) return false;
    xmidi_install_timbre(bank, patch, t);
    return true;
}
static int ail_lock(void) { return xmidi_lock_channel(); }
static void ail_release(int ch) { xmidi_release_channel(ch); }
static void ail_cv(int st, int d1, int d2) { xmidi_send_cv(st, d1, d2); }

int sound_effect(int id, int pan, int vol_delta) {
    if (!S.music) return 0xff;
    EnterCriticalSection(&S.cs);
    int r = sfx_play(id, pan, vol_delta);
    LeaveCriticalSection(&S.cs);
    return r;
}
int sound_effect_at(int id, int x, int y, int vol_delta, int px, int py, int heading8) {
    if (!S.music) return 0xff;
    EnterCriticalSection(&S.cs);
    int r = sfx_play_at(id, x, y, vol_delta, px, py, heading8);
    LeaveCriticalSection(&S.cs);
    return r;
}
void sound_effect_stop(int slot) {
    if (!S.music) return;
    EnterCriticalSection(&S.cs);
    sfx_stop(slot);
    LeaveCriticalSection(&S.cs);
}

/* --- the audio thread --------------------------------------------------------------------------------------- */
static void serve(void) { xmidi_serve(); } /* one driver service, 1/120 s (under the lock); the songs: the game's turn */
static void fill(i16 *out) {
    static i16 a[FRAMES * 2], b[FRAMES * 2];
    int done = 0;
    while (done < FRAMES) {
        if (S.tick_left <= 0) {
            EnterCriticalSection(&S.cs);
            serve();
            LeaveCriticalSection(&S.cs);
            S.tick_left += (double)RATE / XMIDI_RATE;
        }
        if (S.sfx_left <= 0) { /* the effects' own timer: their durations (1/16 s) */
            EnterCriticalSection(&S.cs);
            sfx_tick();
            LeaveCriticalSection(&S.cs);
            S.sfx_left += (double)RATE / 16;
        }
        int n = (int)S.tick_left;
        if (S.sfx_left < n) n = (int)S.sfx_left;
        if (n < 1) n = 1;
        if (n > FRAMES - done) n = FRAMES - done;
        MU.render(S.music, a, (u32)n);
        MU.render(S.effects, b, (u32)n);
        float gm = S.music_gain, ge = S.effect_gain;
        for (int i = 0; i < n * 2; i++) {
            float v = a[i] * gm + b[i] * ge;
            out[(done * 2) + i] = (i16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        done += n;
        S.tick_left -= n, S.sfx_left -= n;
    }
}
static DWORD WINAPI audio_thread(void *arg) {
    (void)arg;
    for (int k = 0; k < BUFS; k++) { /* all the buffers out first */
        fill(S.buf[k]);
        waveOutWrite(S.wo, &S.hdr[k], sizeof S.hdr[k]);
    }
    while (!S.stop) {
        WaitForSingleObject(S.event, 50);
        for (int k = 0; k < BUFS && !S.stop; k++) {
            if (!(S.hdr[k].dwFlags & WHDR_DONE)) continue;
            fill(S.buf[k]);
            waveOutWrite(S.wo, &S.hdr[k], sizeof S.hdr[k]);
        }
    }
    return 0;
}

/* --- the interface ---------------------------------------------------------------------------------------------- */
static bool start(const char *data_dir, float music_volume, float effect_volume, bool live);
bool sound_start(const char *data_dir, float music_volume, float effect_volume) { return start(data_dir, music_volume, effect_volume, true); }

/* the checks: the same chain (the driver, both Munts, the mix) rendered for some seconds into a WAV file, no output
 * device and no thread */
bool sound_render_wav(const char *data_dir, const char *wav, float seconds, float music_volume, float effect_volume, int effect) {
    if (!start(data_dir, music_volume, effect_volume, false)) return false;
    int frames = (int)(seconds * RATE) / FRAMES * FRAMES;
    i16 *pcm = malloc((size_t)frames * 4);
    if (!pcm) return false;
    if (effect < 0) songs_play(1, true); /* the title song, as the game begins */
    for (int f = 0; f < frames; f += FRAMES) {
        if (effect >= 0 && f / FRAMES == RATE / FRAMES) logf_("sound: effect %d -> slot %d\n", effect, sfx_play(effect, 0x40, 0));
        fill(pcm + f * 2);
    }
    FILE *o = fopen(wav, "wb");
    bool ok = o != NULL;
    if (o) {
        u32 n = (u32)frames * 4, sr = RATE;
        u32 hdr[11] = {0x46464952, 36 + n, 0x45564157, 0x20746d66, 16, 0x00020001, sr, sr * 4, 0x00100004, 0x61746164, n};
        ok = fwrite(hdr, 4, 11, o) == 11 && fwrite(pcm, 1, n, o) == n;
        fclose(o);
    }
    int peak = 0;
    for (int i = 0; i < frames * 2; i++) peak = MAX(peak, pcm[i] < 0 ? -pcm[i] : pcm[i]);
    logf_("sound: %.1f s of %s rendered to %s, peak %d\n", seconds, ROMS[S.roms][2], wav, peak);
    free(pcm);
    return ok && peak > 0;
}

static bool start(const char *data_dir, float music_volume, float effect_volume, bool live) {
    if (S.on) return true;
    if (!munt_load()) return false;
    char rom_dir[700];
    snprintf(rom_dir, sizeof rom_dir, "%s\\..\\roms", data_dir);
    int which = -1, which2 = -1;
    S.music = munt_open(rom_dir, &which);
    if (!S.music) return false;
    S.effects = munt_open(rom_dir, &which2);
    if (!S.effects || MU.rate(S.music) != RATE) {
        if (S.effects) MU.close(S.effects), MU.free_ctx(S.effects);
        MU.close(S.music), MU.free_ctx(S.music);
        S.music = S.effects = NULL;
        return false;
    }
    S.roms = which;
    WAVEFORMATEX wf = {WAVE_FORMAT_PCM, 2, RATE, RATE * 4, 4, 16, 0};
    S.event = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (live && waveOutOpen(&S.wo, WAVE_MAPPER, &wf, (DWORD_PTR)S.event, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        MU.close(S.effects), MU.free_ctx(S.effects), MU.close(S.music), MU.free_ctx(S.music);
        S.music = S.effects = NULL;
        CloseHandle(S.event);
        return false;
    }
    for (int k = 0; k < BUFS && live; k++) {
        memset(&S.hdr[k], 0, sizeof S.hdr[k]);
        S.hdr[k].lpData = (LPSTR)S.buf[k];
        S.hdr[k].dwBufferLength = sizeof S.buf[k];
        waveOutPrepareHeader(S.wo, &S.hdr[k], sizeof S.hdr[k]);
    }
    InitializeCriticalSection(&S.cs);
    str_copy(S.data, sizeof S.data, data_dir);
    S.music_gain = music_volume, S.effect_gain = effect_volume;
    S.stop = 0, S.tick_left = 0;
    char path[700];
    snprintf(path, sizeof path, "%s\\sound\\uw.mt", data_dir);
    S.uwmt = read_heap(path);
    XmidiIO io = {mpu_out, mpu_wait, NULL};
    xmidi_init(&io); /* the driver's start: the Roland reset and set up, as the original's AIL_init_driver */
    songs_init(&SONGS_AIL, true); /* the music: the game's own choice (UW.EXE seg 1bd8), the Roland's files */
    { /* the effects (UW.EXE 0x1fd8e: SOUNDS.DAT; the device: the Roland, 6) */
        static const SfxAil ail = {ail_tstat, ail_install, ail_lock, ail_release, ail_cv};
        snprintf(path, sizeof path, "%s\\sound\\sounds.dat", data_dir);
        Bytes sd = read_heap(path);
        sfx_init(&ail, sd.data, sd.size, 6);
        free((void *)sd.data);
        S.sfx_left = 0;
    }
    S.on = live;
    if (live) S.thread = CreateThread(NULL, 0, audio_thread, NULL, 0, NULL);
    logf_("sound: %s (Munt) through UW's MT32MPU.ADV driver, the game's own music choice\n", ROMS[S.roms][2]);
    return true;
}

bool sound_active(void) { return S.on; }
const char *sound_device(void) { return S.on ? ROMS[S.roms][2] : ""; }

void sound_stop(void) {
    if (!S.on) return;
    S.stop = 1;
    SetEvent(S.event);
    WaitForSingleObject(S.thread, 2000);
    EnterCriticalSection(&S.cs);
    if (g_songs.handle != -1) xmidi_stop(g_songs.handle), xmidi_release(g_songs.handle), g_songs.handle = -1;
    xmidi_shutdown(NULL);
    LeaveCriticalSection(&S.cs);
    waveOutReset(S.wo);
    for (int k = 0; k < BUFS; k++) waveOutUnprepareHeader(S.wo, &S.hdr[k], sizeof S.hdr[k]);
    waveOutClose(S.wo);
    MU.close(S.music), MU.free_ctx(S.music), MU.close(S.effects), MU.free_ctx(S.effects);
    S.music = S.effects = NULL;
    CloseHandle(S.event), CloseHandle(S.thread);
    free(S.xmi), S.xmi = NULL, free(S.pending), S.pending = NULL;
    S.on = false;
}

void sound_music_volume(float v) { S.music_gain = v < 0 ? 0 : v > 1 ? 1 : v; }
void sound_effect_volume(float v) { S.effect_gain = v < 0 ? 0 : v > 1 ? 1 : v; }
/* the songs (songs.c) from the game, under the lock */
#define SONGS(call)                                                                                                    \
    do {                                                                                                               \
        if (!S.on) return;                                                                                             \
        EnterCriticalSection(&S.cs);                                                                                   \
        call;                                                                                                          \
        LeaveCriticalSection(&S.cs);                                                                                   \
    } while (0)
void sound_music(bool on) { SONGS(songs_enable(on)); }
void sound_next_song(void) { SONGS(songs_next_random()); }
void sound_song_play(int n) { SONGS(songs_play(n, true)); }
void sound_song_set_next(int n) { SONGS(songs_set_next(n)); }
void sound_song_random(void) { SONGS(songs_next_random()); }
void sound_song_restart(void) { SONGS(songs_restart_if_over()); }
void sound_songs_update(u32 clock, bool fight_mode) { SONGS(songs_update(clock, fight_mode, true)); }
void sound_song_engaged(u32 clock) { SONGS(songs_engaged(clock)); }
void sound_song_blow(u32 clock, bool by_player, int hp, int max_hp) { SONGS(songs_blow(clock, by_player, hp, max_hp)); }
void sound_song_fight_mode(bool on) { SONGS(songs_fight_mode(on)); }
void sound_effect_stop_all(void) { SONGS(sfx_stop_all()); }
void sound_lock(void) {
    if (S.on) EnterCriticalSection(&S.cs);
}
void sound_unlock(void) {
    if (S.on) LeaveCriticalSection(&S.cs);
}
