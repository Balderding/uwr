/* check.exe: the engine's loaders and extractor against the Python reference.
 *
 *   check.exe     (run from anywhere; finds uo-new from its own folder)
 *
 * Needs extracted\data-ref (python -I tools\loose.py write). Writes nothing. */
#include <stdio.h>
#include <string.h>

#include "base.h"
#include "extract.h"
#include "level.h"

static char g_root[600], g_ref[700], g_virtual[700];
static int g_compared, g_differ;

/* the extractor's writes, compared with the reference instead of written */
static bool compare_hook(const char *path, const void *data, size_t n) {
    size_t vl = strlen(g_virtual);
    if (strncmp(path, g_virtual, vl)) return printf("  unexpected write %s\n", path), false;
    char ref[900];
    snprintf(ref, sizeof ref, "%s%s", g_ref, path + vl);
    size_t mark = arena_mark();
    Bytes b = file_read(ref);
    bool same = b.size == n && !memcmp(b.data, data, n);
    arena_reset(mark);
    g_compared++;
    if (!same) {
        if (g_differ < 5) printf("  DIFFERENT: %s (%zu bytes, reference %zu)\n", path + vl + 1, n, b.size);
        g_differ++;
    }
    return true;
}

static int failures;
static void ok(bool c, const char *what) {
    printf("%s %s\n", c ? "ok  " : "FAIL", what);
    if (!c) failures++;
}

int main(void) {
    arena_init((size_t)1536 << 20);
    str_copy(g_root, sizeof g_root, root_dir());
    char game[700], msg[300];
    snprintf(game, sizeof game, "%s\\game", g_root);
    snprintf(g_ref, sizeof g_ref, "%s\\extracted\\data-ref", g_root);
    snprintf(g_virtual, sizeof g_virtual, "%s\\build\\check-nowhere", g_root);

    /* 1. the C extractor = the Python reference, every file */
    char probe[800];
    snprintf(probe, sizeof probe, "%s\\levels\\level1.txt", g_ref);
    if (!file_exists(probe)) {
        printf("FAIL the reference is missing: run  python -I tools\\loose.py write\n");
        return 1;
    }
    g_write_hook = compare_hook;
    int made = extract_core(g_virtual, game);
    g_write_hook = NULL;
    static char names[400][48];
    int n = extract_list(names, 400);
    snprintf(msg, sizeof msg, "extractor: %d of %d files written, %d compared with tools\\loose.py, %d different", made, n, g_compared, g_differ);
    ok(made == n && g_compared == n && !g_differ, msg);

    /* 2. the level text reads back to the original blocks */
    char p[900];
    snprintf(p, sizeof p, "%s\\DATA\\LEV.ARK", game);
    Bytes ark = file_read(p);
    int bad_levels = 0;
    static Level lv;
    for (int L = 1; L <= 9; L++) {
        size_t mark = arena_mark();
        if (!level_load(&lv, g_ref, L)) {
            bad_levels++;
            continue;
        }
        const u8 *blk = ark.data + rd32(ark.data + 2 + 4 * (L - 1));
        const u8 *tm = ark.data + rd32(ark.data + 2 + 4 * (L + 17));
        bool same = true;
        for (int i = 0; i < 4096 && same; i++) {
            const Tile *t = &lv.tile[i];
            u32 w0 = t->type | t->height << 4 | t->unknown8 << 8 | t->floor << 10 | t->nomagic << 14 | t->door << 15;
            u32 w1 = t->wall | (u32)t->first_object << 6;
            same = w0 == rd16(blk + 4 * i) && w1 == rd16(blk + 4 * i + 2);
        }
        for (int i = 0; i < 1024 && same; i++) {
            const Object *o = &lv.obj[i];
            size_t off = 0x4000 + (i < 256 ? (size_t)i * 27 : 256 * 27 + (size_t)(i - 256) * 8);
            same = (u32)(o->item | o->flags << 9) == rd16(blk + off) && (u32)(o->z | o->heading << 7 | o->y << 10 | o->x << 13) == rd16(blk + off + 2) &&
                   (u32)(o->quality | o->next << 6) == rd16(blk + off + 4) && (u32)(o->owner | o->link << 6) == rd16(blk + off + 6) &&
                   (i >= 256 || !memcmp(o->mobile, blk + off + 8, 19));
        }
        for (int i = 0; i < 48 && same; i++) same = lv.walls[i] == rd16(tm + 2 * i);
        for (int i = 0; i < 10 && same; i++) same = lv.floors[i] == rd16(tm + 96 + 2 * i);
        for (int i = 0; i < 6 && same; i++) same = lv.doors[i] == tm[116 + i];
        if (!same) bad_levels++;
        arena_reset(mark);
    }
    snprintf(msg, sizeof msg, "levels: 9 level texts read back to LEV.ARK's tiles, objects and texture maps (%d wrong)", bad_levels);
    ok(!bad_levels, msg);

    /* 3. textures: the PNGs read back to the original palette indices */
    Bytes tr[2];
    snprintf(p, sizeof p, "%s\\DATA\\W64.TR", game);
    tr[0] = file_read(p);
    snprintf(p, sizeof p, "%s\\DATA\\F32.TR", game);
    tr[1] = file_read(p);
    int bad_tex = 0, tex = 0;
    for (int k = 0; k < 2; k++) {
        int count = (int)rd16(tr[k].data + 2), size = tr[k].data[1];
        for (int i = 0; i < count; i++) {
            snprintf(p, sizeof p, "%s\\textures\\%s\\%03d.png", g_ref, k ? "floors" : "walls", i);
            size_t mark = arena_mark();
            Image img;
            bool good = png_read(p, &img) && img.w == size && img.h == size && img.idx &&
                        !memcmp(img.idx, tr[k].data + rd32(tr[k].data + 4 + 4 * i), (size_t)size * size);
            bad_tex += !good;
            tex++;
            arena_reset(mark);
        }
    }
    snprintf(msg, sizeof msg, "textures: %d PNGs read back to W64.TR / F32.TR (%d wrong)", tex, bad_tex);
    ok(tex == 262 && !bad_tex, msg);

    /* 4. the PNG reader on a compressed file (zlib level 9, from tools\assets.py export) */
    snprintf(p, sizeof p, "%s\\extracted\\levels\\level1.png", g_root);
    if (file_exists(p)) {
        size_t mark = arena_mark();
        Image img;
        bool good = png_read(p, &img) && img.w == 1024 && img.h == 1024 && !img.idx;
        arena_reset(mark);
        ok(good, "png: reads a compressed (dynamic Huffman) RGB PNG");
    } else printf("skip png: extracted\\levels\\level1.png not there (python -I tools\\assets.py export)\n");

    /* 5. every level builds its triangles */
    int tris[10] = {0}, built = 0;
    for (int L = 1; L <= 9; L++) {
        size_t mark = arena_mark();
        Mesh m;
        if (level_load(&lv, g_ref, L) && level_mesh(&lv, g_ref, NULL, 0, &m) && m.vertices > 3000) tris[L] = m.vertices / 3, built++;
        arena_reset(mark);
    }
    snprintf(msg, sizeof msg, "meshes: %d of 9 levels built (triangles %d %d %d %d %d %d %d %d %d)", built, tris[1], tris[2], tris[3], tris[4],
             tris[5], tris[6], tris[7], tris[8], tris[9]);
    ok(built == 9, msg);

    printf(failures ? "%d CHECKS FAILED\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
