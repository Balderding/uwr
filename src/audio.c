#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* --- XMI -> MIDI (tools\xmi.py) -------------------------------------------------------------------- */
typedef struct {
    u32 t;
    u8 b[8];
    int n;      /* bytes in b (channel messages) */
    const u8 *ext; /* meta / sysex: the bytes after the status (type, length, data), extn of them */
    int extn;
    u32 seq;       /* the order in the file: events at one time keep it */
} Ev;

static u32 vlq(const u8 *d, size_t n, size_t *i) {
    u32 v = 0;
    while (*i < n) {
        u8 b = d[(*i)++];
        v = v << 7 | (b & 0x7F);
        if (!(b & 0x80)) break;
    }
    return v;
}
static size_t put_vlq(u8 *o, u32 v) {
    u8 t[5];
    int k = 0;
    t[k++] = v & 0x7F;
    while (v >>= 7) t[k++] = (u8)((v & 0x7F) | 0x80);
    for (int i = 0; i < k; i++) o[i] = t[k - 1 - i];
    return (size_t)k;
}
static int ev_cmp(const void *a, const void *b) {
    const Ev *x = a, *y = b;
    return x->t < y->t ? -1 : x->t > y->t ? 1 : (x->seq < y->seq ? -1 : x->seq > y->seq ? 1 : 0);
}

bool xmi_to_mid(Bytes x, u8 *out, size_t cap, size_t *outn) {
    const u8 *d = x.data;
    size_t at = 0;
    for (; at + 8 <= x.size; at++)
        if (!memcmp(d + at, "EVNT", 4)) break;
    if (at + 8 > x.size) return false;
    u32 len = (u32)d[at + 4] << 24 | (u32)d[at + 5] << 16 | (u32)d[at + 6] << 8 | d[at + 7];
    const u8 *ev = d + at + 8;
    size_t n = MIN((size_t)len, x.size - at - 8), i = 0;
    size_t mark = arena_mark();
    int cap_ev = (int)n + 16, ne = 0;
    Ev *e = arena_alloc(sizeof(Ev) * (size_t)cap_ev);
    u32 t = 0;
    while (i < n && ne + 2 < cap_ev) {
        u8 b = ev[i];
        if (b < 0x80) {
            t += b, i++;
            continue;
        }
        u8 st = b;
        i++;
        if (st == 0xFF) {
            if (i >= n) break;
            u8 type = ev[i];
            size_t j = i + 1;
            u32 l = vlq(ev, n, &j);
            if (type == 0x2F) break;
            if (type != 0x51) { /* tempo: XMI's timing is fixed */
                Ev *o = &e[ne++];
                memset(o, 0, sizeof *o);
                o->t = t, o->b[0] = 0xFF, o->n = 1, o->ext = ev + i, o->extn = (int)(j + l - i);
            }
            i = j + l;
            continue;
        }
        if (st == 0xF0 || st == 0xF7) {
            size_t j = i;
            u32 l = vlq(ev, n, &j);
            Ev *o = &e[ne++];
            memset(o, 0, sizeof *o);
            o->t = t, o->b[0] = st, o->n = 1, o->ext = ev + i, o->extn = (int)(j + l - i);
            i = j + l;
            continue;
        }
        int kind = st & 0xF0, nd = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;
        if (i + (size_t)nd > n) break;
        Ev *o = &e[ne++];
        memset(o, 0, sizeof *o);
        o->t = t, o->b[0] = st, o->n = 1 + nd;
        for (int k = 0; k < nd; k++) o->b[1 + k] = ev[i + k];
        i += (size_t)nd;
        if (kind == 0x90) { /* a note-on carries its length: its note-off */
            u32 dur = vlq(ev, n, &i);
            Ev *off = &e[ne++];
            memset(off, 0, sizeof *off);
            off->t = t + dur, off->b[0] = (u8)(0x80 | (st & 15)), off->b[1] = o->b[1], off->b[2] = 0, off->n = 3;
        }
    }
    for (int k = 0; k < ne; k++) e[k].seq = (u32)k;
    qsort(e, (size_t)ne, sizeof(Ev), ev_cmp);
    /* MThd, then one track: the tempo (500,000 us a quarter: 120 ticks a second at 60 a quarter) and the events */
    size_t o = 0;
    if (cap < 64) return arena_reset(mark), false;
    memcpy(out, "MThd\0\0\0\6\0\0\0\1\0\x3c", 14), o = 14;
    memcpy(out + o, "MTrk\0\0\0\0", 8), o += 8;
    size_t trk = o;
    static const u8 tempo[7] = {0, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20};
    memcpy(out + o, tempo, 7), o += 7;
    u32 last = 0;
    for (int k = 0; k < ne; k++) {
        if (o + 16 + (size_t)e[k].extn > cap) return arena_reset(mark), false;
        o += put_vlq(out + o, e[k].t - last), last = e[k].t;
        memcpy(out + o, e[k].b, (size_t)e[k].n), o += (size_t)e[k].n;
        if (e[k].ext) memcpy(out + o, e[k].ext, (size_t)e[k].extn), o += (size_t)e[k].extn;
    }
    static const u8 end[4] = {0, 0xFF, 0x2F, 0};
    memcpy(out + o, end, 4), o += 4;
    u32 tl = (u32)(o - trk);
    out[trk - 4] = (u8)(tl >> 24), out[trk - 3] = (u8)(tl >> 16), out[trk - 2] = (u8)(tl >> 8), out[trk - 1] = (u8)tl;
    *outn = o;
    arena_reset(mark);
    return true;
}

/* --- VOC -> WAV ------------------------------------------------------------------------------------- */
bool voc_to_wav(Bytes v, u8 *out, size_t cap, size_t *outn) {
    if (v.size < 0x1A || memcmp(v.data, "Creative Voice File", 19)) return false;
    size_t i = rd16(v.data + 0x14), n = 44;
    u32 rate = 11025;
    while (i + 4 <= v.size && v.data[i] != 0) {
        u8 type = v.data[i];
        u32 len = v.data[i + 1] | (u32)v.data[i + 2] << 8 | (u32)v.data[i + 3] << 16;
        if (type == 1 && len >= 2 && i + 4 + len <= v.size) {
            rate = 1000000u / (256u - v.data[i + 4]);
            size_t samples = len - 2;
            if (n + samples > cap) return false;
            memcpy(out + n, v.data + i + 6, samples), n += samples;
        }
        i += 4 + len;
    }
    u32 data = (u32)(n - 44);
    memcpy(out, "RIFF", 4);
    u32 riff = data + 36;
    memcpy(out + 4, &riff, 4), memcpy(out + 8, "WAVEfmt ", 8);
    u32 fmt_len = 16, byte_rate = rate;
    u16 pcm = 1, ch = 1, align = 1, bits = 8;
    memcpy(out + 16, &fmt_len, 4), memcpy(out + 20, &pcm, 2), memcpy(out + 22, &ch, 2), memcpy(out + 24, &rate, 4);
    memcpy(out + 28, &byte_rate, 4), memcpy(out + 32, &align, 2), memcpy(out + 34, &bits, 2), memcpy(out + 36, "data", 4), memcpy(out + 40, &data, 4);
    *outn = n;
    return true;
}

/* --- the music: a .mid (format 0, ours) played on its own thread ----------------------------------------- */
#ifndef _WIN32
/* (Linux: the renderer's library takes only the converters above; the standalone engine's music player is Windows') */
bool music_start(const char *data_dir, float volume) { (void)data_dir, (void)volume; return false; }
void music_volume(float v) { (void)v; }
void music_next(void) {}
void music_stop(void) {}
#else
static struct {
    HMIDIOUT out;
    HANDLE thread;
    volatile LONG stop, next;
    volatile float volume;
    char data[600];
    char list[32][32];
    int nlist;
} M;

typedef struct {
    u32 t;      /* ticks (120 a second) */
    u32 msg;    /* a short message for midiOutShortMsg */
} Note;

static int load_song(const char *path, Note *ev, int cap) {
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    int ne = 0;
    if (f.size > 22 && !memcmp(f.data, "MThd", 4) && !memcmp(f.data + 14, "MTrk", 4)) {
        size_t i = 22, n = f.size;
        u32 t = 0;
        u8 run = 0;
        while (i < n && ne < cap) {
            t += vlq(f.data, n, &i);
            if (i >= n) break;
            u8 st = f.data[i];
            if (st >= 0x80) i++, run = st;
            else st = run;
            if (st == 0xFF) {
                u8 type = f.data[i++];
                u32 l = vlq(f.data, n, &i);
                i += l;
                if (type == 0x2F) break;
                continue;
            }
            if (st == 0xF0 || st == 0xF7) {
                u32 l = vlq(f.data, n, &i);
                i += l;
                continue;
            }
            int nd = ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) ? 1 : 2;
            u32 msg = st;
            for (int k = 0; k < nd && i < n; k++) msg |= (u32)f.data[i++] << (8 * (k + 1));
            ev[ne].t = t, ev[ne].msg = msg, ne++;
        }
    }
    arena_reset(mark);
    return ne;
}

static void all_off(void) {
    for (int c = 0; c < 16; c++) midiOutShortMsg(M.out, (DWORD)(0xB0 | c) | 123u << 8); /* all notes off */
}

static DWORD WINAPI music_thread(void *arg) {
    (void)arg;
    static Note ev[65536];
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    for (int song = 0; !M.stop; song = (song + 1) % MAX(1, M.nlist)) {
        if (!M.nlist) break;
        char path[700];
        snprintf(path, sizeof path, "%s\\music\\%s.mid", M.data, M.list[song]);
        int ne = load_song(path, ev, COUNT(ev));
        QueryPerformanceCounter(&start);
        M.next = 0;
        for (int k = 0; k < ne && !M.stop && !M.next; k++) {
            for (;;) {
                QueryPerformanceCounter(&now);
                double secs = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
                double due = ev[k].t / 120.0 - secs;
                if (due <= 0 || M.stop || M.next) break;
                Sleep(due > 0.004 ? (DWORD)(due * 1000 - 2) : 0);
            }
            u32 m = ev[k].msg;
            if ((m & 0xF0) == 0x90 && ((m >> 16) & 0x7F)) { /* the volume on the note's velocity */
                u32 vel = (u32)(((m >> 16) & 0x7F) * M.volume);
                m = (m & 0xFFFF) | (MAX(1u, vel) << 16);
            }
            midiOutShortMsg(M.out, m);
        }
        all_off();
        for (int w = 0; w < 30 && !M.stop; w++) Sleep(100); /* a little silence between songs */
    }
    return 0;
}

bool music_start(const char *data_dir, float volume) {
    memset(&M, 0, sizeof M);
    str_copy(M.data, sizeof M.data, data_dir);
    M.volume = volume;
    char path[700];
    snprintf(path, sizeof path, "%s\\music\\playlist.txt", data_dir);
    if (!file_exists(path)) {
        static const char def[] = "# uo-new music: the songs (data\\music\\NAME.mid) played in the dungeon, in turn. Edit freely.\r\n"
                                  "UW02\r\nUW03\r\nUW04\r\nUW05\r\nUW06\r\nUW07\r\nUW10\r\nUW11\r\nUW12\r\nUW13\r\n";
        make_dirs_for(path);
        file_write(path, def, sizeof def - 1);
    }
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size && M.nlist < 32;) {
        char name[32];
        if (sscanf(p, "%31s", name) == 1 && name[0] != '#') str_copy(M.list[M.nlist++], 32, name);
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
    if (!M.nlist || midiOutOpen(&M.out, MIDI_MAPPER, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return false;
    M.thread = CreateThread(NULL, 0, music_thread, NULL, 0, NULL);
    return M.thread != NULL;
}

void music_volume(float v) { M.volume = v < 0 ? 0 : v > 1 ? 1 : v; }
void music_next(void) { M.next = 1; }
void music_stop(void) {
    if (!M.thread) return;
    M.stop = 1;
    WaitForSingleObject(M.thread, 2000);
    all_off();
    midiOutReset(M.out);
    midiOutClose(M.out);
    M.thread = NULL;
}
#endif
