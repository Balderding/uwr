/* uwtour.c: the port's renderer (uwview.c over render.c) with no game and no window: every level of the original
 * (DATA\LEV.ARK's blocks, as the game holds them in mapdata) built in turn, and pictures of the items lying about
 * in each, the camera standing in an open square beside the item, facing it at eye height. Checks that every level
 * loads, and makes pictures to look at.
 *
 *   uwtour.exe GAME_DIR OUT_DIR [PER_LEVEL]
 *   (UWVIEW_DATA names our extracted data; the renderer's settings as the port has them)
 * Writes OUT_DIR\levelN_K.png (640x480) and OUT_DIR\levelN_sheet.png (the level's pictures, a quarter size). */
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base.h"

int uwv_init(void *hwnd, int w, int h);
void uwv_ui_size(int *w, int *h);
void uwv_game_dir(const char *dir);
int uwv_level(const unsigned char *block, int n);
void uwv_camera(float x, float y, float z, float yaw, float pitch);
void uwv_light(int level);
void uwv_lens(float tan_x, float tan_y);
void uwv_view_rect(int x0, int y0, int x1, int y1, int w, int h);
int uwv_frame(const unsigned char *ui_rgba);
int uwv_readback(unsigned char *rgba);

#define W 640
#define H 480
#define BLOCK 0x7C08

static unsigned rd(const unsigned char *p) { return p[0] | p[1] << 8; }

/* a square's type and floor (the block's tile words: type bits 0-3, height bits 4-7) */
static int tile_type(const unsigned char *b, int x, int z) { return x < 0 || x > 63 || z < 0 || z > 63 ? 0 : (int)(rd(b + (z * 64 + x) * 4) & 15); }
static float tile_floor(const unsigned char *b, int x, int z) { return (float)((rd(b + (z * 64 + x) * 4) >> 4) & 15) * 0.25f; }
static bool open_sq(const unsigned char *b, float x, float z) { return tile_type(b, (int)floorf(x), (int)floorf(z)) == 1; }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uwtour GAME_DIR OUT_DIR [PER_LEVEL]\n"); return 2; }
    const char *game = argv[1], *out = argv[2];
    int per = argc > 3 ? atoi(argv[3]) : 8;
    if (per < 1) per = 1;
    if (per > 8) per = 8;
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s\\DATA\\LEV.ARK", game);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "uwtour: cannot read %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *ark = malloc((size_t)size);
    if (!ark || fread(ark, 1, (size_t)size, f) != (size_t)size) { fprintf(stderr, "uwtour: cannot read %s\n", path); return 1; }
    fclose(f);
    CreateDirectoryA(out, NULL);

    uwv_game_dir(game);
    if (!uwv_init(NULL, W, H)) { fprintf(stderr, "uwtour: the renderer did not start\n"); return 1; }
    int uw, uh;
    uwv_ui_size(&uw, &uh);
    unsigned char *ui = calloc((size_t)uw * uh, 4);       /* the interface layer: clear everywhere */
    unsigned char *px = malloc((size_t)W * H * 4), *sheet = malloc((size_t)(W / 2) * 4 * (H / 2) * 2 * 3);
    uwv_view_rect(0, 0, W, H, W, H);
    uwv_lens(0.99f, 0.99f * 0.75f);                        /* the original's width of view, square pixels at 4:3 */
    uwv_light(4);                                         /* a lantern's light */
    int failed = 0, shots = 0;
    for (int n = 1; n <= 9; n++) {
        long off = (long)(ark[2 + (n - 1) * 4] | ark[3 + (n - 1) * 4] << 8 | ark[4 + (n - 1) * 4] << 16 | (long)ark[5 + (n - 1) * 4] << 24);
        if (off <= 0 || off + BLOCK > size) { fprintf(stderr, "uwtour: level %d: no block in LEV.ARK\n", n), failed++; continue; }
        static unsigned char block[BLOCK];
        memcpy(block, ark + off, BLOCK);
        if (!uwv_level(block, n)) { fprintf(stderr, "uwtour: level %d did NOT load\n", n), failed++; continue; }
        fprintf(stderr, "uwtour: level %d loaded\n", n);
        /* the items lying about (not creatures 0x40-0x7F, doors and the rest from 0x140, nor invisible ones), at
         * least 6 squares apart, each seen from an open square beside it */
        float chosen[8][2];
        int nch = 0;
        memset(sheet, 0, (size_t)(W / 2) * 4 * (H / 2) * 2 * 3);
        for (int t = 0; t < 4096 && nch < per; t++)
            for (int i = rd(block + t * 4 + 2) >> 6, guard = 0; i > 1 && i < 1024 && guard < 1024 && nch < per; guard++) {
                size_t o = 0x4000 + (i < 256 ? (size_t)i * 27 : 256 * 27 + (size_t)(i - 256) * 8);
                unsigned w0 = rd(block + o), w1 = rd(block + o + 2);
                int item = (int)(w0 & 511), next = (int)(rd(block + o + 4) >> 6);
                /* UWTOUR_ITEMS=animo: the animated objects only (0x1C0-0x1CE: fountains, mist, the silver tree...) */
                static int animo = -1;
                if (animo < 0) { const char *e = getenv("UWTOUR_ITEMS"); animo = e && !strcmp(e, "animo"); }
                bool shown = animo ? item >= 0x1C0 && item <= 0x1CE && !((w0 >> 14) & 1)
                                   : item > 0 && item < 0x140 && !(item >= 0x40 && item < 0x80) && !((w0 >> 14) & 1);
                float ix = (float)(t % 64) + ((float)(w1 >> 13) + 0.5f) / 8.0f, iz = (float)(t / 64) + ((float)((w1 >> 10) & 7) + 0.5f) / 8.0f;
                float iy = (float)(w1 & 127) / 32.0f;
                bool far_enough = true;
                for (int k = 0; k < nch; k++)
                    if ((chosen[k][0] - ix) * (chosen[k][0] - ix) + (chosen[k][1] - iz) * (chosen[k][1] - iz) < 36) far_enough = false;
                int view = -1;
                static const float DX[8] = {0, 1, 0, -1, 0.7f, 0.7f, -0.7f, -0.7f}, DZ[8] = {-1, 0, 1, 0, 0.7f, -0.7f, 0.7f, -0.7f};
                for (int d = 0; shown && far_enough && d < 8 && view < 0; d++) {
                    float cx = ix + DX[d] * 1.3f, cz = iz + DZ[d] * 1.3f;
                    if (open_sq(block, cx, cz) && open_sq(block, (cx + ix) * 0.5f, (cz + iz) * 0.5f) && open_sq(block, ix, iz)) view = d;
                }
                if (view >= 0) {
                    float cx = ix + DX[view] * 1.3f, cz = iz + DZ[view] * 1.3f;
                    float eye = tile_floor(block, (int)cx, (int)cz) + 0.64f;
                    float yaw = atan2f(ix - cx, iz - cz), dist = sqrtf((ix - cx) * (ix - cx) + (iz - cz) * (iz - cz));
                    float pitch = atan2f(iy + 0.1f - eye, dist);
                    uwv_camera(cx, eye, cz, yaw, pitch);
                    for (int k = 0; k < 40; k++) uwv_frame(ui);  /* DLSS and Ray Reconstruction settle */
                    if (uwv_readback(px)) {
                        snprintf(path, sizeof path, "%s\\level%d_%d.png", out, n, nch + 1);
                        png_write(path, px, W, H, 4);
                        int sx = (nch % 4) * (W / 2), sy = (nch / 4) * (H / 2);
                        for (int y = 0; y < H / 2; y++)
                            for (int x = 0; x < W / 2; x++) {
                                const unsigned char *s = px + ((size_t)(y * 2) * W + (size_t)(x * 2)) * 4;
                                unsigned char *d = sheet + ((size_t)(sy + y) * (W / 2 * 4) + (size_t)(sx + x)) * 3;
                                d[0] = s[0], d[1] = s[1], d[2] = s[2];
                            }
                        fprintf(stderr, "uwtour: level %d: item 0x%03x at %.1f,%.1f: %s\n", n, item, ix, iz, path);
                        chosen[nch][0] = ix, chosen[nch][1] = iz, nch++, shots++;
                    }
                }
                i = next;
            }
        if (nch) {
            snprintf(path, sizeof path, "%s\\level%d_sheet.png", out, n);
            png_write(path, sheet, W / 2 * 4, H / 2 * 2, 3);
        }
        fprintf(stderr, "uwtour: level %d: %d pictures\n", n, nch);
    }
    fprintf(stderr, "uwtour: %d of 9 levels loaded, %d pictures\n", 9 - failed, shots);
    return failed ? 1 : 0;
}
