/* uwrec.c: the window's pictures to a video (Shift+P: 15 seconds at the window's size, 1920x1080), H.264 in an
 * MP4 through Windows' own Media Foundation (nothing to install). Each frame the renderer is asked for its
 * picture (render_want_screenshot), read back after the frame (render_readback: the GPU has finished, render_frame
 * waits for it) and handed to a worker thread that encodes it, stamped with the real time it was shown, so the
 * video plays at the speed the game ran. When the worker falls behind a frame is dropped, never the game slowed.
 * The videos go to videos\ beside the DLL's folder (dist\UW1\videos). No sound. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uwrec.h"

#define SLOTS 6
static struct {
    volatile LONG active;            /* recording: frames wanted */
    int w, h;
    LONGLONG start, freq, end;        /* QueryPerformanceCounter ticks */
    IMFSinkWriter *wr;
    DWORD stream;
    unsigned char *buf[SLOTS];
    LONGLONG when[SLOTS];
    volatile LONG full[SLOTS];
    int put, get;
    HANDLE thread, wake;
    volatile LONG stop;
    LONGLONG last_ts;
    int frames, dropped;
    char path[MAX_PATH];
} Rc;

static DWORD WINAPI rec_worker(void *arg)
{
    (void)arg;
    for (;;) {
        WaitForSingleObject(Rc.wake, 100);
        while (Rc.full[Rc.get]) {
            int k = Rc.get;
            IMFMediaBuffer *mb = NULL;
            DWORD n = (DWORD)Rc.w * (DWORD)Rc.h * 4;
            if (SUCCEEDED(MFCreateMemoryBuffer(n, &mb))) {
                BYTE *d = NULL;
                IMFMediaBuffer_Lock(mb, &d, NULL, NULL);
                const unsigned char *s = Rc.buf[k];
                for (DWORD i = 0; i < n; i += 4) d[i] = s[i + 2], d[i + 1] = s[i + 1], d[i + 2] = s[i], d[i + 3] = 255; /* RGBA -> BGRA */
                IMFMediaBuffer_Unlock(mb);
                IMFMediaBuffer_SetCurrentLength(mb, n);
                IMFSample *sm = NULL;
                if (SUCCEEDED(MFCreateSample(&sm))) {
                    LONGLONG ts = (Rc.when[k] - Rc.start) * 10000000LL / Rc.freq;   /* 100 ns units */
                    if (ts <= Rc.last_ts && Rc.frames) ts = Rc.last_ts + 1;
                    IMFSample_AddBuffer(sm, mb);
                    IMFSample_SetSampleTime(sm, ts);
                    IMFSample_SetSampleDuration(sm, 166667);
                    if (SUCCEEDED(IMFSinkWriter_WriteSample(Rc.wr, Rc.stream, sm))) Rc.frames++, Rc.last_ts = ts;
                    IMFSample_Release(sm);
                }
                IMFMediaBuffer_Release(mb);
            }
            InterlockedExchange(&Rc.full[k], 0);
            Rc.get = (Rc.get + 1) % SLOTS;
        }
        if (Rc.stop) break;
    }
    IMFSinkWriter_Finalize(Rc.wr);
    IMFSinkWriter_Release(Rc.wr);
    Rc.wr = NULL;
    fprintf(stderr, "uwview: video %s: %d frames written, %d dropped\n", Rc.path, Rc.frames, Rc.dropped);
    MFShutdown();
    CoUninitialize();
    /* Explorer opened on the new video, chosen (the user's Shift+P; never in the off-screen checks) */
    const char *off = getenv("UWPORT_OFFSCREEN");
    if (Rc.frames && !(off && *off && *off != '0')) {
        char cmd[MAX_PATH + 64];
        snprintf(cmd, sizeof cmd, "explorer.exe /select,\"%s\"", Rc.path);
        STARTUPINFOA si = {sizeof si};
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
    }
    return 0;
}

static bool rec_open(const char *dir, int w, int h)
{
    WCHAR wp[MAX_PATH];
    SYSTEMTIME t;
    GetLocalTime(&t);
    CreateDirectoryA(dir, NULL);
    snprintf(Rc.path, sizeof Rc.path, "%s\\uw-%04d%02d%02d-%02d%02d%02d.mp4", dir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    MultiByteToWideChar(CP_UTF8, 0, Rc.path, -1, wp, MAX_PATH);
    IMFAttributes *at = NULL;
    MFCreateAttributes(&at, 2);
    IMFAttributes_SetUINT32(at, &MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);   /* the card's encoder when it has one */
    HRESULT hr = MFCreateSinkWriterFromURL(wp, NULL, at, &Rc.wr);
    IMFAttributes_Release(at);
    if (FAILED(hr)) return false;
    IMFMediaType *out = NULL, *in = NULL;
    MFCreateMediaType(&out);
    IMFMediaType_SetGUID(out, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(out, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
    IMFMediaType_SetUINT32(out, &MF_MT_AVG_BITRATE, 24000000);
    IMFMediaType_SetUINT32(out, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    IMFMediaType_SetUINT64(out, &MF_MT_FRAME_SIZE, (UINT64)w << 32 | (UINT32)h);
    IMFMediaType_SetUINT64(out, &MF_MT_FRAME_RATE, (UINT64)60 << 32 | 1);
    IMFMediaType_SetUINT64(out, &MF_MT_PIXEL_ASPECT_RATIO, (UINT64)1 << 32 | 1);
    hr = IMFSinkWriter_AddStream(Rc.wr, out, &Rc.stream);
    IMFMediaType_Release(out);
    if (FAILED(hr)) goto fail;
    MFCreateMediaType(&in);
    IMFMediaType_SetGUID(in, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(in, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    IMFMediaType_SetUINT32(in, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    IMFMediaType_SetUINT32(in, &MF_MT_DEFAULT_STRIDE, (UINT32)(w * 4));   /* top row first */
    IMFMediaType_SetUINT64(in, &MF_MT_FRAME_SIZE, (UINT64)w << 32 | (UINT32)h);
    IMFMediaType_SetUINT64(in, &MF_MT_FRAME_RATE, (UINT64)60 << 32 | 1);
    IMFMediaType_SetUINT64(in, &MF_MT_PIXEL_ASPECT_RATIO, (UINT64)1 << 32 | 1);
    hr = IMFSinkWriter_SetInputMediaType(Rc.wr, Rc.stream, in, NULL);
    IMFMediaType_Release(in);
    if (FAILED(hr)) goto fail;
    if (FAILED(IMFSinkWriter_BeginWriting(Rc.wr))) goto fail;
    return true;
fail:
    fprintf(stderr, "uwview: video: the encoder would not start (0x%08lX)\n", (unsigned long)hr);
    IMFSinkWriter_Release(Rc.wr);
    Rc.wr = NULL;
    return false;
}

bool uwrec_start(const char *dir, int w, int h, int seconds)
{
    if (Rc.active || Rc.thread) return false;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) return false;
    if (!rec_open(dir, w, h)) { MFShutdown(); return false; }
    Rc.w = w, Rc.h = h, Rc.put = Rc.get = 0, Rc.frames = Rc.dropped = 0, Rc.last_ts = 0, Rc.stop = 0;
    for (int k = 0; k < SLOTS; k++) {
        if (!Rc.buf[k]) Rc.buf[k] = malloc((size_t)w * h * 4);
        Rc.full[k] = 0;
    }
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f), QueryPerformanceCounter(&c);
    Rc.freq = f.QuadPart, Rc.start = c.QuadPart, Rc.end = c.QuadPart + f.QuadPart * seconds;
    Rc.wake = CreateEventA(NULL, FALSE, FALSE, NULL);
    Rc.thread = CreateThread(NULL, 0, rec_worker, NULL, 0, NULL);
    InterlockedExchange(&Rc.active, 1);
    fprintf(stderr, "uwview: recording %d seconds to %s\n", seconds, Rc.path);
    return true;
}

static void rec_finish(void)
{
    InterlockedExchange(&Rc.active, 0);
    Rc.stop = 1;
    SetEvent(Rc.wake);
}

bool uwrec_active(void) { return Rc.active != 0; }
const char *uwrec_path(void) { return Rc.path; }
int uwrec_seconds_left(void)
{
    if (!Rc.active) return 0;
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (int)((Rc.end - c.QuadPart + Rc.freq - 1) / Rc.freq);
}

/* after a frame drawn with render_want_screenshot: its picture (read by readback into the slot) */
void uwrec_frame(bool (*readback)(unsigned char *rgba))
{
    if (!Rc.active) {
        if (Rc.thread && WaitForSingleObject(Rc.thread, 0) == WAIT_OBJECT_0) {   /* the last video closed */
            CloseHandle(Rc.thread), CloseHandle(Rc.wake), Rc.thread = NULL;
        }
        return;
    }
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    if (c.QuadPart >= Rc.end) { rec_finish(); return; }
    int k = Rc.put;
    if (Rc.full[k]) { Rc.dropped++; return; }          /* the encoder behind: this one is left out */
    if (!readback(Rc.buf[k])) return;
    Rc.when[k] = c.QuadPart;
    InterlockedExchange(&Rc.full[k], 1);
    Rc.put = (k + 1) % SLOTS;
    SetEvent(Rc.wake);
}

/* P: one frame (RGBA, top row first) to the clipboard as a picture (CF_DIB: 32-bit, bottom row first) */
bool uwrec_clipboard(void *hwnd, int w, int h, bool (*readback)(unsigned char *rgba))
{
    size_t n = (size_t)w * h * 4;
    unsigned char *px = malloc(n);
    if (!px || !readback(px)) { free(px); return false; }
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + n);
    bool ok = false;
    if (g) {
        BITMAPINFOHEADER *bi = GlobalLock(g);
        memset(bi, 0, sizeof *bi);
        bi->biSize = sizeof *bi, bi->biWidth = w, bi->biHeight = h, bi->biPlanes = 1, bi->biBitCount = 32, bi->biCompression = BI_RGB;
        unsigned char *d = (unsigned char *)(bi + 1);
        for (int y = 0; y < h; y++) {
            const unsigned char *s = px + (size_t)(h - 1 - y) * w * 4;
            unsigned char *o = d + (size_t)y * w * 4;
            for (int x = 0; x < w * 4; x += 4) o[x] = s[x + 2], o[x + 1] = s[x + 1], o[x + 2] = s[x], o[x + 3] = 255;
        }
        GlobalUnlock(g);
        if (OpenClipboard((HWND)hwnd)) {
            EmptyClipboard();
            ok = SetClipboardData(CF_DIB, g) != NULL;
            CloseClipboard();
        }
        if (!ok) GlobalFree(g);
    }
    free(px);
    fprintf(stderr, "uwview: screenshot %s the clipboard\n", ok ? "on" : "could not be put on");
    return ok;
}

void uwrec_shutdown(void)
{
    if (Rc.active) rec_finish();
    if (Rc.thread) {
        WaitForSingleObject(Rc.thread, 10000);
        CloseHandle(Rc.thread), CloseHandle(Rc.wake), Rc.thread = NULL;
    }
}
