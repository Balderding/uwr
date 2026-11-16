/* models_test.c: the 32 models decoded from game\UW.EXE (src\models.c) with plain variables: every model runs to its
 * end with no unknown opcode, gives triangles, and stays inside a sane box; the counts are printed beside
 * tools\models.py's face counts for a look.  models_test.exe GAME_DIR */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base.h"
#include "models.h"

static Vertex verts[60000];

int main(int argc, char **argv) {
    const char *game = argc > 1 ? argv[1] : "O:\\uw\\uo-new\\dist\\UW1\\game";
    int failed = 0, total = 0;
    arena_init(64u << 20);
    if (!models_load(game)) { printf("models_test: no models\n"); return 1; }
    for (int n = 0; n < MODELS_N; n++) {
        ModelInst mi;
        memset(&mi, 0, sizeof mi);
        mi.model = n;
        mi.var0_rgb[0] = -1;
        mi.kind_tex = 5, mi.kind_flat = 1;
        mi.rect[0] = 0, mi.rect[1] = 0, mi.rect[2] = 64, mi.rect[3] = 64;
        const unsigned char *cols = models_cols(n);
        for (int i = 0; i < MCOL_NCOLS(cols[0]) && i < 3; i++) mi.vars[i] = cols[1 + i];
        mi.vars[3] = 1;                      /* a door: open */
        mi.vars[5] = 4 << 12;                /* a leaf's swing: 4 sixteenths */
        mi.delta_y = 3.0f;
        int nv = models_place(&mi, verts, (int)(sizeof verts / sizeof verts[0]));
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        int bad = 0;
        for (int i = 0; i < nv; i++)
            for (int k = 0; k < 3; k++) {
                float v = verts[i].pos[k];
                if (!(v > -8 && v < 8)) bad++;
                if (v < lo[k]) lo[k] = v;
                if (v > hi[k]) hi[k] = v;
            }
        bool empty_ok = n == 0 || n == 4 || n == 21 || n == 29 || n == 30 || n == 31;   /* the models with no program */
        bool ok = (nv > 0 || empty_ok) && !bad && nv % 3 == 0;
        printf("model %2d: %5d vertices (%4d triangles) box x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f %s\n", n, nv, nv / 3,
               nv ? lo[0] : 0, nv ? hi[0] : 0, nv ? lo[1] : 0, nv ? hi[1] : 0, nv ? lo[2] : 0, nv ? hi[2] : 0, ok ? "PASS" : "FAIL");
        total++;
        if (!ok) failed++;
    }
    printf("models_test: %d passed, %d failed\n", total - failed, failed);
    return failed;
}
