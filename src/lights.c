#include "lights.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char DEFAULT_LIGHTS[] =
    "# uo-new lights placed by hand. There are none: as in the original, the only light is the one the player\r\n"
    "# carries. Lines added here put lights in the world:\r\n"
    "#   light LEVEL X Z HEIGHT  R G B  INTENSITY RANGE\r\n"
    "# X, Z in tiles (x east, z north, as in the level files); HEIGHT in floor-height steps (the ceiling is\r\n"
    "# 16); R G B from 0 to 1; INTENSITY = brightness one tile away; RANGE = how many tiles it reaches.\r\n"
    "#   walltorch LEVEL X Z HEIGHT  R G B  INTENSITY RANGE  FX FZ   (a burning torch on a wall, facing FX FZ)\r\n"
    "# Delete this file to get this one back.\r\n";

/* reads every "light" line of a file for this level into out (from *n, up to cap) */
static bool read_file(const char *path, int level, Light *out, int *n, int cap) {
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    if (!f.size) return arena_reset(mark), false;
    const char *p = (const char *)f.data, *end = p + f.size;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        char line[256];
        size_t len = MIN((size_t)((nl ? nl : end) - p), sizeof line - 1);
        memcpy(line, p, len);
        line[len] = 0;
        int lv;
        float x, z, h, r, g, b, in, range;
        float fx = 0, fz = 0;
        bool orb = sscanf(line, " light %d %f %f %f %f %f %f %f %f", &lv, &x, &z, &h, &r, &g, &b, &in, &range) == 9;
        bool torch = !orb && sscanf(line, " walltorch %d %f %f %f %f %f %f %f %f %f %f", &lv, &x, &z, &h, &r, &g, &b, &in, &range, &fx, &fz) == 11;
        if ((orb || torch) && lv == level && *n < cap) {
            Light *L = &out[(*n)++];
            L->pos[0] = x, L->pos[1] = h * STEP, L->pos[2] = z;
            L->color[0] = r, L->color[1] = g, L->color[2] = b;
            L->intensity = in, L->range = range > 0.5f ? range : 0.5f;
            L->kind = torch ? LIGHT_WALL_TORCH : LIGHT_ORB, L->facing[0] = fx, L->facing[1] = fz;
        }
        p = nl ? nl + 1 : end;
    }
    arena_reset(mark);
    return true;
}

/* can a straight line from a to b pass through the open part of the level (sampled every 0.1 tiles) */
static bool clear_line(const Level *lv, const float a[3], const float b[3]) {
    float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    int n = (int)(len / 0.1f) + 1;
    for (int k = 1; k < n; k++) {
        float t = (float)k / (float)n, x = a[0] + d[0] * t, y = a[1] + d[1] * t, z = a[2] + d[2] * t;
        if (level_solid_at(lv, x, z) || y < level_floor_y(lv, x, z) - 0.01f || y > level_ceil_y()) return false;
    }
    return true;
}

int lights_load(const char *data_dir, const Level *lv, Light *out, int cap) {
    char path[700];
    snprintf(path, sizeof path, "%s\\lights.txt", data_dir);
    if (!file_exists(path)) {
        make_dirs_for(path);
        if (!file_write(path, DEFAULT_LIGHTS, sizeof DEFAULT_LIGHTS - 1)) return -1;
    }
    int n = 0;
    read_file(path, lv->number, out, &n, cap);
    return n;
}

void lights_tiles(const Level *lv, const Light *l, int n, u32 *out) {
    for (int i = 0; i < 64 * 64 * TILE_LIGHTS; i++) out[i] = NO_LIGHT;
    for (int z = 0; z < 64; z++)
        for (int x = 0; x < 64; x++) {
            const Tile *t = &lv->tile[z * 64 + x];
            if (t->type == T_SOLID) continue;
            float best[TILE_LIGHTS];
            u32 *o = out + (z * 64 + x) * TILE_LIGHTS;
            int k = 0;
            /* points of the tile a light may reach: its middle near the floor and higher up */
            float fy = (float)t->height * STEP;
            float pts[3][3] = {{x + 0.5f, fy + 0.3f, z + 0.5f}, {x + 0.5f, MIN(fy + 0.9f, level_ceil_y() - 0.05f), z + 0.5f},
                               {x + 0.5f, level_ceil_y() - 0.05f, z + 0.5f}};
            if (t->type >= T_DIAG_SE && t->type <= T_DIAG_NW) {
                static const float LX[4] = {0.67f, 0.33f, 0.67f, 0.33f}, LZ[4] = {0.33f, 0.33f, 0.67f, 0.67f};
                for (int p = 0; p < 3; p++) pts[p][0] = x + LX[t->type - 2], pts[p][2] = z + LZ[t->type - 2];
            }
            for (int i = 0; i < n; i++) {
                float dx = l[i].pos[0] - (x + 0.5f), dz = l[i].pos[2] - (z + 0.5f);
                float d2 = dx * dx + dz * dz, reach = l[i].range + 0.75f;
                if (d2 > reach * reach) continue;
                bool seen = false;
                for (int p = 0; p < 3 && !seen; p++) seen = clear_line(lv, l[i].pos, pts[p]);
                /* lights it can see count a hundred times more */
                float score = l[i].intensity / (d2 + 0.5f) * (seen ? 100.0f : 1.0f);
                int at = k < TILE_LIGHTS ? k++ : TILE_LIGHTS - 1;
                if (at == TILE_LIGHTS - 1 && k == TILE_LIGHTS && score <= best[at] && o[at] != NO_LIGHT) continue;
                while (at > 0 && best[at - 1] < score) best[at] = best[at - 1], o[at] = o[at - 1], at--;
                best[at] = score, o[at] = (u32)i;
            }
        }
}
