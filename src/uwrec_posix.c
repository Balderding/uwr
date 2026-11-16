/* uwrec_posix.c: uwrec.h on Linux (libuwview.so, tools\buildview_linux.py; uwrec.c is Windows' Media Foundation).
 * Shift+P's video: not on Linux yet (nothing on a stock Linux Mint encodes H.264 without a library we would ship;
 * said in the log, the game goes on). P's screenshot: the frame as a PNG on the clipboard through SDL3 (image/png,
 * which X11's and Wayland's clipboards both carry). */
#ifndef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL3/SDL.h>
#include "base.h"
#include "uwrec.h"

bool uwrec_start(const char *dir, int w, int h, int seconds)
{
    (void)dir, (void)w, (void)h, (void)seconds;
    fprintf(stderr, "uwview: video recording is not in the Linux build yet\n");
    return false;
}
bool uwrec_active(void) { return false; }
const char *uwrec_path(void) { return ""; }
int uwrec_seconds_left(void) { return 0; }
void uwrec_frame(bool (*readback)(unsigned char *rgba)) { (void)readback; }
void uwrec_shutdown(void) {}

static unsigned char *clip_png;
static size_t clip_n;
static const void *clip_data(void *user, const char *mime, size_t *size)
{
    (void)user;
    if (!mime || strcmp(mime, "image/png") || !clip_png) return *size = 0, NULL;
    *size = clip_n;
    return clip_png;
}

/* P: one frame (RGBA, top row first) to the clipboard as a PNG */
bool uwrec_clipboard(void *hwnd, int w, int h, bool (*readback)(unsigned char *rgba))
{
    (void)hwnd;
    size_t n = (size_t)w * h * 4;
    unsigned char *px = malloc(n);
    bool ok = false;
    if (!px || !readback(px)) { free(px); return false; }
    for (size_t i = 3; i < n; i += 4) px[i] = 255;
    char tmp[256];
    const char *dir = getenv("XDG_RUNTIME_DIR");
    snprintf(tmp, sizeof tmp, "%s/uw-new-shot-%d.png", dir && *dir ? dir : "/tmp", (int)getpid());
    if (png_write(tmp, px, w, h, 4)) {
        FILE *f = fopen(tmp, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            unsigned char *b = len > 0 ? malloc((size_t)len) : NULL;
            if (b && fread(b, 1, (size_t)len, f) == (size_t)len) {
                free(clip_png), clip_png = b, clip_n = (size_t)len, b = NULL;
                const char *mimes[1] = {"image/png"};
                ok = SDL_SetClipboardData(clip_data, NULL, NULL, mimes, 1);
            }
            free(b);
            fclose(f);
        }
        unlink(tmp);
    }
    free(px);
    fprintf(stderr, "uwview: screenshot %s the clipboard\n", ok ? "on" : "could not be put on");
    return ok;
}
#endif
