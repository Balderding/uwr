/*
 * laigter-cli: Laigter's map generator (normal, specular, occlusion, parallax) as a command-line tool, without Qt.
 *
 * A port of Laigter 1.14.0 by Pablo Ivan Fonovich (azagaya), https://github.com/azagaya/laigter
 * Laigter: an automatic map generator for lighting effects. Copyright (C) 2019 Pablo Ivan Fonovich.
 * This port: Copyright (C) 2026, same licence.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 * License for more details (LICENSE-GPL3.txt beside this file).
 *
 * What is ported, routine by routine, from 1.14.0's src/image_processor.cpp (the path its own --no-gui mode takes):
 * set_current_heightmap, calculate, fill_neighbours, calculate_distance, modify_distance, generate_normal_map,
 * calculate_normal, modify_parallax, calculate_parallax, modify_specular, calculate_specular, modify_occlusion,
 * calculate_occlusion, get_normal/... (use-alpha), and gui/presets_manager.cpp applyPresets (the .preset format).
 * The image maths is CImg's, the same header Laigter uses (thirdparty/CImg.h, CeCILL-C), called the same way.
 * Qt is replaced where Laigter leaned on it:
 *   - QImage RGBA8888_Premultiplied: Qt's qPremultiply rounding, per channel.
 *   - convertToFormat(Grayscale8) from premultiplied: qGray(unpremultiplied), with Qt's rounding. Proved against
 *     laigter.exe 1.14.0 on all 16,777,216 opaque colours and 65,536 partly transparent pixels (0 differences).
 *   - QImage -> CImg and back: same channel order; CImg<float> -> uchar truncates, as CImg's conversion does.
 *   - the empty overlays Laigter adds in every pass: x * (1 - 0) + 0, then cut(0, 255): kept as the cut.
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define cimg_display 0
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC system_header
#endif
#include "thirdparty/CImg.h"

extern "C" {
typedef unsigned char u8;
typedef struct {
    int w, h;
    u8 *rgba;
    u8 *idx;
} Image;
bool png_read(const char *path, Image *out);
void arena_init(size_t bytes);
size_t arena_mark(void);
void arena_reset(size_t mark);
}

using cimg_library::CImg;
namespace fs = std::filesystem;
typedef unsigned char uchar;

/* ---------------------------------------------------------------------------------------------------------------- */
/* Settings: ImageProcessor's constructor defaults (1.14.0)                                                          */
/* ---------------------------------------------------------------------------------------------------------------- */
enum class ParallaxType { Binary, HeightMap, Quantization, Intervals };

struct Settings {
    int normal_bisel_depth = 100;
    int normal_bisel_distance = 60;
    int normal_depth = 250;
    /* doubles here (ints in Laigter) so a preset can give them in tenths; a whole number gives Laigter's result
     * exactly (calculate_normal only ever uses them as radius / 3.0) */
    double normal_blur_radius = 6;
    double normal_bisel_blur_radius = 10;
    bool normal_bisel_soft = true;
    int normalInvertX = 1, normalInvertY = 1, normalInvertZ = 1;
    bool tileable = false;
    int parallax_max = 140;
    int parallax_min = 0;
    bool parallax_invert = false;
    int parallax_focus = 2;
    int parallax_soft = 3;
    int parallax_quantization = 1;
    ParallaxType parallax_type = ParallaxType::Binary;
    int parallax_brightness = 0;
    double parallax_contrast = 1;
    int parallax_erode_dilate = 1;
    int specular_blur = 3;
    int specular_bright = 0;
    double specular_contrast = 1;
    int specular_thresh = 127;
    bool specular_invert = false;
    int occlusion_blur = 3;
    int occlusion_bright = 16;
    double occlusion_contrast = 1;
    int occlusion_thresh = 1;
    bool occlusion_invert = false;
    bool occlusion_distance_mode = true;
    int occlusion_distance = 10;
    bool useNormalAlpha = false, useSpecularAlpha = false, useOcclusionAlpha = false, useParallaxAlpha = false;
    /* ours: the occlusion's threshold per picture, half the picture's median grey + 1 (off: the preset's own). The
     * joints of a texture are its pixels far darker than its middle grey, whatever its brightness */
    bool occlusion_thresh_median = false;
    bool flat_when_x_zero = false; /* newer Laigter (after 1.14.0): a pixel whose x slope is 0 is drawn flat */
};

/* QByteArray::toInt: the whole text (spaces round it allowed) must be a number, else 0 */
static int qt_to_int(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (a == b) return 0;
    std::string t = s.substr(a, b - a);
    char *end = nullptr;
    long v = strtol(t.c_str(), &end, 10);
    if (*end) return 0;
    return (int)v;
}

/* ours (not Laigter's): EnhanceSoft and BumpSoft may be decimals, e.g. 3.6; a whole number reads as qt_to_int would */
static double qt_to_double(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    if (a == b) return 0;
    std::string t = s.substr(a, b - a);
    char *end = nullptr;
    double v = strtod(t.c_str(), &end);
    if (*end) return 0;
    return v;
}

/* gui/presets_manager.cpp: one "Name<TAB>value" per line; the names (spaces included) are Laigter's own */
static bool apply_preset_file(const std::string &path, Settings &p) {
    static const char *codes[34] = {
        "EnhanceHeight ", "EnhanceSoft ", "BumpHeight ", "BumpDistance", "BumpSoft ", "BumpCut ", "Tile ",
        "InvertX ", "InvertY ", "ParallaxType", "BinaryThreshold ", "BinaryFocus ", "ParallaxSoft ",
        "BinaryMinHeight ", "BinaryErodeDilate ", "HeightMapBrightness ", "HeightMapContrast ", "InvertParallax ",
        "SpecularBlur ", "SpecularBright ", "SpecularContrast ", "SpecularThresh ", "SpecularInvert ",
        "OcclusionBlur ", "OcclusionBright ", "OcclusionInvert ", "OcclusionThresh ", "OcclusionContrast ",
        "OcclusionDistance ", "OcclusionDistanceMode ", "Normal Use Alpha ", "Specular Use Alpha ",
        "Occlussion Use Alpha ", "Parallax Use Alpha "};
    std::ifstream f(path, std::ios::binary);
    if (!f) return false; /* Laigter: a preset it cannot open is simply not applied */
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t at = 0;
    while (at <= all.size()) {
        size_t nl = all.find('\n', at);
        std::string line = all.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
        at = nl == std::string::npos ? all.size() + 1 : nl + 1;
        size_t tab = line.find('\t');
        std::string key = line.substr(0, tab);
        std::string val;
        if (tab != std::string::npos) {
            size_t tab2 = line.find('\t', tab + 1);
            val = line.substr(tab + 1, tab2 == std::string::npos ? std::string::npos : tab2 - tab - 1);
        }
        int v = qt_to_int(val), i = 0;
        for (; i < 34; i++)
            if (key == codes[i]) break;
        switch (i) {
        case 0: p.normal_depth = v; break;
        case 1: p.normal_blur_radius = qt_to_double(val); break;
        case 2: p.normal_bisel_depth = v; break;
        case 3: p.normal_bisel_distance = v; break;
        case 4: p.normal_bisel_blur_radius = qt_to_double(val); break;
        case 5: p.normal_bisel_soft = (bool)v; break;
        case 6: p.tileable = (bool)v; break;
        case 7: p.normalInvertX = -(bool)v * 2 + 1; break;
        case 8: p.normalInvertY = -(bool)v * 2 + 1; break;
        case 9: p.parallax_type = (ParallaxType)v; break;
        case 10: p.parallax_max = v; break;
        case 11: p.parallax_focus = v; break;
        case 12: p.parallax_soft = v; break;
        case 13: p.parallax_min = v; break;
        case 14: p.parallax_erode_dilate = v; break;
        case 15: p.parallax_brightness = v; break;
        case 16: p.parallax_contrast = v / 1000.0; break;
        case 17: p.parallax_invert = (bool)v; break;
        case 18: p.specular_blur = v; break;
        case 19: p.specular_bright = v; break;
        case 20: p.specular_contrast = v / 1000.0; break;
        case 21: p.specular_thresh = v; break;
        case 22: p.specular_invert = (bool)v; break;
        case 23: p.occlusion_blur = v; break;
        case 24: p.occlusion_bright = v; break;
        case 25: p.occlusion_invert = (bool)v; break;
        case 26: p.occlusion_thresh = v; break;
        case 27: p.occlusion_contrast = v / 1000.0; break;
        case 28: p.occlusion_distance = v; break;
        case 29: p.occlusion_distance_mode = (bool)v; break;
        case 30: p.useNormalAlpha = (bool)v; break;
        case 31: p.useSpecularAlpha = (bool)v; break;
        case 32: p.useOcclusionAlpha = (bool)v; break;
        case 33: p.useParallaxAlpha = (bool)v; break;
        default: break; /* LightSource lines and anything else: nothing to do without a preview */
        }
    }
    return true;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* Qt's pixel conversions (see the header for the proof)                                                             */
/* ---------------------------------------------------------------------------------------------------------------- */
static inline unsigned qt_premul(unsigned c, unsigned a) { /* qPremultiply, one channel */
    unsigned v = c * a;
    return (v + (v >> 8) + 0x80) >> 8;
}
static inline unsigned qt_unpremul(unsigned c, unsigned a) { /* qUnpremultiply, one channel */
    if (a == 255) return c;
    if (a == 0) return 0;
    unsigned v = (c * 255 + a / 2) / a;
    return v > 255 ? 255 : v;
}
static inline unsigned qt_gray(unsigned r, unsigned g, unsigned b) { return (r * 11 + g * 16 + b * 5) / 32; }

/* A picture as Laigter holds it: RGBA8888_Premultiplied, w x h */
struct Picture {
    int w = 0, h = 0;
    std::vector<uchar> px; /* premultiplied RGBA */
};

/* heightmap.convertToFormat(RGBA8888) -> QImage2CImg: only its alpha is ever read, and alpha passes unchanged */
static CImg<float> rgba_cimg(const Picture &p) {
    CImg<uchar> c(p.w, p.h, 1, 4);
    for (int y = 0; y < p.h; y++)
        for (int x = 0; x < p.w; x++) {
            const uchar *s = &p.px[((size_t)y * p.w + x) * 4];
            unsigned a = s[3];
            c(x, y, 0, 0) = (uchar)qt_unpremul(s[0], a);
            c(x, y, 0, 1) = (uchar)qt_unpremul(s[1], a);
            c(x, y, 0, 2) = (uchar)qt_unpremul(s[2], a);
            c(x, y, 0, 3) = (uchar)a;
        }
    return CImg<float>(c);
}

/* heightmap.convertToFormat(Grayscale8) -> QImage2CImg */
static CImg<uchar> gray_cimg(const Picture &p) {
    CImg<uchar> c(p.w, p.h, 1, 1);
    for (int y = 0; y < p.h; y++)
        for (int x = 0; x < p.w; x++) {
            const uchar *s = &p.px[((size_t)y * p.w + x) * 4];
            unsigned a = s[3];
            c(x, y) = (uchar)qt_gray(qt_unpremul(s[0], a), qt_unpremul(s[1], a), qt_unpremul(s[2], a));
        }
    return c;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* ImageProcessor, the parts the command line runs                                                                    */
/* ---------------------------------------------------------------------------------------------------------------- */
struct Processor {
    Settings st;
    bool has_normal = false, has_parallax = false, has_specular = false, has_occlusion = false;

    Picture texture;    /* Diffuse = Heightmap = SpecularBase = OcclussionBase */
    Picture neighbours; /* 3w x 3h, the picture in all nine places */
    Picture heightmap;  /* the one set_current_heightmap picked */

    CImg<float> current_heightmap, m_distance, new_distance, m_distance_normal, m_emboss_normal, m_normal, m_gray,
        m_height_ov;
    CImg<float> current_parallax, current_specular, current_occlusion;
    CImg<uchar> out_normal, out_parallax, out_specular, out_occlusion;

    int sw() const { return texture.w; } /* sprite.size(): one frame */
    int sh() const { return texture.h; }

    void set_current_heightmap() {
        heightmap = st.tileable ? neighbours : texture;
        current_heightmap = rgba_cimg(heightmap);
        m_gray = CImg<float>(gray_cimg(heightmap));
    }

    /* loadImage -> fill_neighbours -> calculate */
    void load(const Picture &pic) {
        texture = pic;
        neighbours.w = pic.w * 3;
        neighbours.h = pic.h * 3;
        neighbours.px.assign((size_t)neighbours.w * neighbours.h * 4, 0);
        for (int ty = 0; ty < 3; ty++)
            for (int tx = 0; tx < 3; tx++)
                for (int y = 0; y < pic.h; y++)
                    memcpy(&neighbours.px[(((size_t)(ty * pic.h + y)) * neighbours.w + tx * pic.w) * 4],
                           &pic.px[(size_t)y * pic.w * 4], (size_t)pic.w * 4);
        calculate();
    }

    void calculate() {
        set_current_heightmap();
        if (has_normal || has_parallax) calculate_distance();
        if (has_normal) generate_normal_map();
        if (has_parallax) calculate_parallax();
        if (has_specular) calculate_specular();
        if (has_occlusion) calculate_occlusion();
    }

    void calculate_distance() {
        m_distance = rgba_cimg(heightmap);
        m_distance.channel(3).threshold(0.1) * 255.0f;
        cimg_for_borderXY(m_distance, x, y, 1) m_distance(x, y) = 0.0;
        m_distance.distance(0.0f);
        m_distance *= 1;
    }

    CImg<float> modify_distance() {
        CImg<float> dist(m_distance);
        if (st.normal_bisel_distance != 0) {
            dist *= 255.0 / st.normal_bisel_distance;
        } else {
            dist.threshold(0.1f) * 255.0;
        }
        dist.cut(0, 255);
        if (st.normal_bisel_soft) {
            dist = (1.0 - (dist / 255.0 - 1).pow(2)).sqrt() * 255.0;
        }
        return dist;
    }

    CImg<float> calculate_normal(CImg<float> in, int depth, double blur_radius) {
        const int SW = sw(), SH = sh();
        CImg<float> img(in);
        if (img.width() == SW * 3) {
            img.blur(blur_radius / 3.0, true, true);
        } else {
            img.blur(blur_radius / 3.0);
        }
        int xs = 0, xe = img.width() - 1, ys = 0, ye = img.height() - 1;
        CImg<float> normals(img.width(), img.height(), 1, 3);
        CImg<float> out(SW, SH, 1, 3);
        img /= 255.0;
        int w = img.width();
        int h = img.height();
        float dx, dy;
        for (int x = xs; x <= xe; x++) {
            for (int y = ys; y <= ye; y++) {
                if (current_heightmap(x, y, 0, 3) == 0.0) {
                    normals(x, y, 0, 0) = 0;
                    normals(x, y, 0, 1) = 0;
                    normals(x, y, 0, 2) = 1;
                    continue;
                }
                if (x == 0) {
                    dx = -3 * img(x, y) + 4 * img(x + 1, y) - img(x + 2, y);
                } else if (x == w - 1) {
                    dx = 3 * img(x, y) - 4 * img(x - 1, y) + img(x - 2, y);
                } else {
                    dx = -img(x - 1, y) + img(x + 1, y);
                }
                if (y == 0) {
                    dy = -3 * img(x, y) + 4 * img(x, y + 1) - img(x, y + 2);
                } else if (y == h - 1) {
                    dy = 3 * img(x, y) - 4 * img(x, y - 1) + img(x, y - 2);
                } else {
                    dy = -img(x, y - 1) + img(x, y + 1);
                }
                normals(x, y, 0, 0) = -dx * (depth / 100.0) * st.normalInvertX;
                normals(x, y, 0, 1) = dy * (depth / 100.0) * st.normalInvertY;
                normals(x, y, 0, 2) = 1.0;
            }
        }
        if (st.tileable) {
            /* one frame (h_frames = v_frames = 1): the middle of the nine; a picture that is not 3x comes out zero,
             * as Laigter's get_crop past its edge does */
            CImg<float> frame_normal = normals.get_crop(SW, SH, 2 * SW - 1, 2 * SH - 1);
            out.draw_image(0, 0, frame_normal);
        } else {
            out = normals;
        }
        return out;
    }

    void generate_normal_map() {
        /* the height overlay: an empty picture of the sprite's size */
        CImg<float> heightOv(sw(), sh(), 1, 4, 0.0f);
        heightOv = heightOv.mul(heightOv.get_channel(3) / 255.0);
        m_height_ov = calculate_normal(heightOv.channel(0), 5000, 0);

        if (st.tileable) { /* update_tileable, set by the preset: the same heightmap and distance again */
            set_current_heightmap();
            calculate_distance();
        }

        m_emboss_normal = calculate_normal(m_gray * 10.0, st.normal_depth, st.normal_blur_radius);
        new_distance = modify_distance();
        m_distance_normal =
            calculate_normal(new_distance, st.normal_bisel_depth * st.normal_bisel_distance, st.normal_bisel_blur_radius);

        m_normal = m_emboss_normal;
        const int W = texture.w, H = texture.h;
        for (int x = 0; x < W; ++x) {
            for (int y = 0; y < H; ++y) {
                float nr, ng, nb, norm, r, g, b, a;
                r = 0.0f * 2 - 1; /* the normal overlay: transparent */
                g = 0.0f * 2 - 1;
                b = 0.0f * 2 - 1;
                a = 0.0f;
                nr = m_emboss_normal(x, y, 0, 0) * 3 / 2.0 + m_distance_normal(x, y, 0, 0) * 3 / 2.0 +
                     m_height_ov(x, y, 0, 0);
                ng = m_emboss_normal(x, y, 0, 1) * 3 / 2.0 + m_distance_normal(x, y, 0, 1) * 3 / 2.0 +
                     m_height_ov(x, y, 0, 1);
                nb = m_emboss_normal(x, y, 0, 2) * 3 / 2.0 + m_distance_normal(x, y, 0, 2) * 3 / 2.0 +
                     m_height_ov(x, y, 0, 2);
                nr = nr * (1 - a) + (r)*a;
                ng = ng * (1 - a) + (g)*a;
                nb = nb * (1 - a) + (b)*a;
                norm = sqrtf(nr * nr + ng * ng + nb * nb);
                if (!st.flat_when_x_zero || nr) {
                    m_normal(x, y, 0, 0) = 255.0 * (nr / norm * 0.5 + 0.5);
                    m_normal(x, y, 0, 1) = 255.0 * (ng / norm * 0.5 + 0.5);
                    m_normal(x, y, 0, 2) = 255.0 * (nb / norm * 0.5 + 0.5);
                } else {
                    m_normal(x, y, 0, 0) = 127.5;
                    m_normal(x, y, 0, 1) = 127.5;
                    m_normal(x, y, 0, 2) = 255.0;
                }
            }
        }
        out_normal = CImg<uchar>(m_normal);
    }

    CImg<float> modify_parallax() {
        set_current_heightmap();
        CImg<float> par(gray_cimg(heightmap));
        CImg<float> dist = modify_distance();
        switch (st.parallax_type) {
        case ParallaxType::Binary: {
            par.blur(st.parallax_focus);
            par.threshold(st.parallax_max).normalize(0, 255);
            par -= st.parallax_min;
            if (!st.parallax_invert) {
                par = 255.0 - par;
            }
            if (st.parallax_erode_dilate > 0) {
                par.dilate(st.parallax_erode_dilate, st.parallax_erode_dilate);
            } else {
                par.erode(-st.parallax_erode_dilate, -st.parallax_erode_dilate);
            }
            par.blur(st.parallax_soft);
            break;
        }
        case ParallaxType::HeightMap: {
            par = (par + dist - 1) / 2.0 + 0.5;
            par = st.parallax_contrast * par + st.parallax_max * (1 - st.parallax_contrast);
            par += st.parallax_brightness;
            par.blur(st.parallax_soft);
            if (st.parallax_invert) {
                par = 255.0 - par;
            }
            break;
        }
        case ParallaxType::Intervals: break;
        case ParallaxType::Quantization: break;
        }
        par.cut(0, 255);
        return par;
    }

    void crop_middle(CImg<float> &c) {
        if (st.tileable) c.crop(sw(), sh(), 2 * sw() - 1, 2 * sh() - 1);
    }

    void calculate_parallax() {
        current_parallax = modify_parallax();
        crop_middle(current_parallax);
        current_parallax.cut(0.0, 255.0); /* the empty overlay */
        out_parallax = CImg<uchar>(current_parallax);
    }

    CImg<float> modify_specular() {
        CImg<uchar> img = gray_cimg(st.tileable ? neighbours : texture);
        CImg<float> img_float(img);
        img_float = st.specular_contrast * img_float + st.specular_thresh * (1 - st.specular_contrast);
        img_float += st.specular_bright;
        img_float.cut(0, 255);
        img_float.blur(st.specular_blur);
        if (st.specular_invert) {
            img_float = 255.0 - img_float;
        }
        return img_float;
    }

    void calculate_specular() {
        current_specular = modify_specular();
        crop_middle(current_specular);
        current_specular.cut(0.0, 255.0);
        out_specular = CImg<uchar>(current_specular);
    }

    CImg<float> modify_occlusion() {
        set_current_heightmap();
        CImg<float> occ(gray_cimg(heightmap));
        if (st.occlusion_invert) {
            occ = 255.0f - occ;
        }
        if (st.occlusion_distance_mode) {
            int th = st.occlusion_thresh;
            if (st.occlusion_thresh_median) th = (int)gray_cimg(texture).median() / 2 + 1;
            occ.threshold(th) * 255.0;
            if (st.occlusion_distance != 0) {
                occ.distance(0.0);
                occ *= 255.0 / st.occlusion_distance;
            }
            occ.cut(0, 255);
            occ = (1.0 - (occ / 255.0 - 1).pow(2)).sqrt() * 255.0;
        }
        occ = st.occlusion_contrast * occ + st.occlusion_thresh * (1 - st.occlusion_contrast);
        occ += st.occlusion_bright;
        occ.cut(0, 255);
        occ = occ.blur(st.occlusion_blur);
        return occ;
    }

    void calculate_occlusion() {
        current_occlusion = modify_occlusion();
        crop_middle(current_occlusion);
        current_occlusion.cut(0.0, 255.0);
        out_occlusion = CImg<uchar>(current_occlusion);
    }
};

/* ---------------------------------------------------------------------------------------------------------------- */
/* PNG out (stored deflate, like the engine's own writer) and in (the engine's reader, src/png.c)                     */
/* ---------------------------------------------------------------------------------------------------------------- */
static unsigned crc_table[256];
static void crc_init() {
    for (unsigned n = 0; n < 256; n++) {
        unsigned c = n;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}
static void put32(std::vector<uchar> &o, unsigned v) {
    o.push_back(v >> 24), o.push_back(v >> 16), o.push_back(v >> 8), o.push_back(v);
}
static void chunk(std::vector<uchar> &o, const char *type, const uchar *d, size_t n) {
    put32(o, (unsigned)n);
    size_t at = o.size();
    o.insert(o.end(), type, type + 4);
    if (n) o.insert(o.end(), d, d + n);
    unsigned c = 0xffffffffu;
    for (size_t i = at; i < o.size(); i++) c = crc_table[(c ^ o[i]) & 255] ^ (c >> 8);
    put32(o, c ^ 0xffffffffu);
}
/* channels 1 grey, 3 RGB, 4 RGBA; px row by row */
static bool png_save(const fs::path &path, const uchar *px, int w, int h, int channels) {
    size_t row = (size_t)w * channels + 1, raw = row * h;
    std::vector<uchar> f(raw);
    for (int y = 0; y < h; y++) {
        f[y * row] = 0;
        memcpy(&f[y * row + 1], px + (size_t)y * w * channels, (size_t)w * channels);
    }
    std::vector<uchar> z = {0x78, 0x01};
    unsigned a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) a = (a + f[i]) % 65521, b = (b + a) % 65521;
    for (size_t at = 0; at < raw;) {
        size_t n = std::min<size_t>(raw - at, 65535);
        z.push_back(at + n == raw);
        z.push_back((uchar)n), z.push_back((uchar)(n >> 8)), z.push_back((uchar)~n), z.push_back((uchar)(~n >> 8));
        z.insert(z.end(), f.begin() + at, f.begin() + at + n);
        at += n;
    }
    put32(z, b << 16 | a);
    std::vector<uchar> o = {137, 80, 78, 71, 13, 10, 26, 10};
    uchar ihdr[13] = {(uchar)(w >> 24), (uchar)(w >> 16), (uchar)(w >> 8), (uchar)w, (uchar)(h >> 24), (uchar)(h >> 16),
                      (uchar)(h >> 8), (uchar)h, 8, (uchar)(channels == 1 ? 0 : channels == 3 ? 2 : 6), 0, 0, 0};
    chunk(o, "IHDR", ihdr, 13);
    chunk(o, "IDAT", z.data(), z.size());
    chunk(o, "IEND", nullptr, 0);
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    out.write((const char *)o.data(), (std::streamsize)o.size());
    return (bool)out;
}

/* CImg2QImage then save; get_normal & co. with use-alpha: SetAlphaChannel writes the diffuse's alpha into the first
 * width bytes of each line, whatever the format (Laigter's own behaviour, kept) */
static bool save_map(const fs::path &path, const CImg<uchar> &c, bool use_alpha, const Picture &tex) {
    int w = c.width(), h = c.height(), ch = c.spectrum();
    if (ch != 1 && ch != 3 && ch != 4) return false;
    std::vector<uchar> px((size_t)w * h * ch);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            for (int k = 0; k < ch; k++) px[((size_t)y * w + x) * ch + k] = c(x, y, 0, k);
    if (use_alpha)
        for (int y = 0; y < h && y < tex.h; y++)
            for (int x = 0; x < tex.w && x < w * ch; x++) px[(size_t)y * w * ch + x] = tex.px[((size_t)y * tex.w + x) * 4 + 3];
    return png_save(path, px.data(), w, h, ch);
}

static std::mutex g_png_lock; /* the engine's reader allocates from its one arena */

/* QImage(file).convertToFormat(RGBA8888_Premultiplied) */
static bool load_picture(const fs::path &path, Picture &out) {
    std::lock_guard<std::mutex> lk(g_png_lock);
    size_t mark = arena_mark();
    Image img = {};
    bool ok = png_read(path.string().c_str(), &img) && img.w > 0 && img.h > 0;
    if (ok) {
        out.w = img.w, out.h = img.h;
        out.px.resize((size_t)img.w * img.h * 4);
        for (size_t i = 0; i < (size_t)img.w * img.h; i++) {
            const uchar *s = img.rgba + i * 4;
            unsigned a = s[3];
            out.px[i * 4 + 0] = (uchar)qt_premul(s[0], a);
            out.px[i * 4 + 1] = (uchar)qt_premul(s[1], a);
            out.px[i * 4 + 2] = (uchar)qt_premul(s[2], a);
            out.px[i * 4 + 3] = (uchar)a;
        }
    }
    arena_reset(mark);
    return ok;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* main: Laigter's command line (-d -n -c -o -p -r -l, suffixes, --not-recursive, --flatten, --check-changes)         */
/* ---------------------------------------------------------------------------------------------------------------- */
static void usage() {
    puts("laigter-cli: Laigter 1.14.0's normal/specular/occlusion/parallax maps, from the command line, no Qt.\n"
         "  -d, --diffuse <file|folder|a,b,c>  pictures to make maps for (a folder: every picture under it)\n"
         "  -n, --normal       make normal maps      (name_n.png)\n"
         "  -c, --specular     make specular maps    (name_s.png)\n"
         "  -o, --occlusion    make occlusion maps   (name_o.png)\n"
         "  -p, --parallax     make parallax maps    (name_p.png)\n"
         "  -r, --preset <file>  a Laigter .preset (the same file the Laigter window saves)\n"
         "  -l, --location <folder>  where the maps go (default: beside each picture)\n"
         "  --normal-suffix/--specular-suffix/--occlusion-suffix/--paralax-suffix <s>  (default _n _s _o _p)\n"
         "  --not-recursive    a folder's own pictures only\n"
         "  --flatten          no sub-folders in the output\n"
         "  --check-changes    only remake maps older than their picture\n"
         "extras (not in Laigter):\n"
         "  --tile             tileable, the same as a preset with \"Tile <tab>1\" (after the preset)\n"
         "  --flat-zero-x      newer Laigter's rule: a pixel whose x slope is exactly 0 is flat\n"
         "  --occlusion-thresh-median  the occlusion threshold per picture: half its median grey + 1\n"
         "  -j, --jobs <n>     pictures made at once (default: all cores)\n"
         "  -q, --quiet        no line per picture");
}

static std::string lower(std::string s) {
    for (char &c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static void tool_init() { /* once: --uwr runs laigter_main several times */
    static bool done = false;
    if (done) return;
    done = true;
    arena_init((size_t)1 << 30);
    crc_init();
}

static const char *uwr_progress; /* --uwr: the stage being made, named in a running count (NULL: none) */
static int laigter_main(int argc, char **argv) {
    std::string input, preset, location, sfx_n = "_n.", sfx_s = "_s.", sfx_o = "_o.", sfx_p = "_p.";
    bool has_n = false, has_c = false, has_o = false, has_p = false, not_recursive = false, flatten = false,
         check_changes = false, tile = false, flat_zero_x = false, occ_median = false, quiet = false;
    int jobs = (int)std::max(1u, std::thread::hardware_concurrency());
    auto suffix = [](std::string s) { return s.size() && s.back() == '.' ? s : s + "."; };
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s needs a value\n", a.c_str());
                exit(2);
            }
            return argv[++i];
        };
        if (a == "-d" || a == "--diffuse") input = next();
        else if (a == "-n" || a == "--normal") has_n = true;
        else if (a == "-c" || a == "--specular") has_c = true;
        else if (a == "-o" || a == "--occlusion") has_o = true;
        else if (a == "-p" || a == "--parallax") has_p = true;
        else if (a == "-r" || a == "--preset") preset = next();
        else if (a == "-l" || a == "--location") location = next();
        else if (a == "--normal-suffix") sfx_n = suffix(next());
        else if (a == "--specular-suffix") sfx_s = suffix(next());
        else if (a == "--occlusion-suffix") sfx_o = suffix(next());
        else if (a == "--paralax-suffix" || a == "--parallax-suffix") sfx_p = suffix(next());
        else if (a == "--not-recursive") not_recursive = true;
        else if (a == "--flatten") flatten = true;
        else if (a == "--check-changes") check_changes = true;
        else if (a == "--tile") tile = true;
        else if (a == "--flat-zero-x") flat_zero_x = true;
        else if (a == "--occlusion-thresh-median") occ_median = true;
        else if (a == "-j" || a == "--jobs") jobs = std::max(1, atoi(next().c_str()));
        else if (a == "-q" || a == "--quiet") quiet = true;
        else if (a == "-g" || a == "--no-gui" || a == "-s" || a == "--software-opengl") {} /* Laigter's; no window here anyway */
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else { fprintf(stderr, "unknown option %s\n", a.c_str()); usage(); return 2; }
    }
    if (input.empty()) { usage(); return 2; }

    Settings base;
    if (!preset.empty() && !apply_preset_file(preset, base)) fprintf(stderr, "preset %s: cannot open, not used\n", preset.c_str());
    if (tile) base.tileable = true;
    base.flat_when_x_zero = flat_zero_x;
    base.occlusion_thresh_median = occ_median;

    /* the files, as Laigter's main.cpp collects them */
    std::vector<fs::path> files;
    fs::path in_dir, out_dir = location.empty() ? fs::path() : fs::absolute(location);
    std::error_code ec;
    if (fs::is_regular_file(input, ec)) {
        files.push_back(fs::absolute(input));
        in_dir = files[0].parent_path();
    } else if (fs::is_directory(input, ec)) {
        in_dir = fs::absolute(input);
        auto take = [&](const fs::directory_entry &e) {
            if (!e.is_regular_file()) return;
            fs::path p = fs::absolute(e.path());
            if (!out_dir.empty()) { /* nothing from inside the output folder */
                std::string rel = fs::relative(p, out_dir, ec).generic_string();
                if (!ec && rel.rfind("..", 0) != 0) return;
            }
            files.push_back(p);
        };
        if (not_recursive)
            for (auto &e : fs::directory_iterator(in_dir)) take(e);
        else
            for (auto &e : fs::recursive_directory_iterator(in_dir)) take(e);
        std::sort(files.begin(), files.end());
    } else {
        size_t at = 0;
        while (at <= input.size()) {
            size_t c = input.find(',', at);
            files.push_back(fs::absolute(input.substr(at, c == std::string::npos ? std::string::npos : c - at)));
            at = c == std::string::npos ? input.size() + 1 : c + 1;
        }
        in_dir = files[0].parent_path();
    }

    tool_init();
    std::atomic<size_t> next_file{0}, made{0}, skipped{0}, failed{0};
    std::mutex print_lock;
    auto t0 = std::chrono::steady_clock::now();

    auto work = [&]() {
        for (;;) {
            size_t i = next_file++;
            if (i >= files.size()) return;
            const fs::path &img = files[i];
            std::string ext = img.extension().string(); /* ".png" */
            if (lower(ext) != ".png") { skipped++; continue; } /* the engine's reader: PNG only */
            fs::path odir = out_dir.empty() ? img.parent_path() : out_dir;
            fs::path rel = flatten ? img.filename() : fs::relative(img, in_dir, ec);
            fs::path stem = odir / rel;
            stem.replace_extension();
            std::string sx = ext.substr(1);
            auto name = [&](const std::string &s) { return fs::path(stem.string() + s + sx); };
            if (check_changes) {
                auto older = [&](const fs::path &o) {
                    std::error_code e2;
                    if (!fs::exists(o, e2)) return true;
                    return fs::last_write_time(o, e2) < fs::last_write_time(img, e2);
                };
                bool changed = (has_n && older(name(sfx_n))) || (has_c && older(name(sfx_s))) ||
                               (has_o && older(name(sfx_o))) || (has_p && older(name(sfx_p)));
                if (!changed) { skipped++; continue; }
            }
            Picture pic;
            if (!load_picture(img, pic)) { skipped++; continue; }
            if (pic.w < 3 || pic.h < 3) { /* Laigter's slopes read 2 pixels on: it reads past the picture here */
                std::lock_guard<std::mutex> lk(print_lock);
                fprintf(stderr, "skipped (under 3 pixels across): %s\n", img.string().c_str());
                skipped++;
                continue;
            }
            Processor p;
            p.st = base;
            p.has_normal = has_n, p.has_specular = has_c, p.has_occlusion = has_o, p.has_parallax = has_p;
            p.load(pic);
            bool ok = true;
            if (has_n) ok &= save_map(name(sfx_n), p.out_normal, p.st.useNormalAlpha, p.texture);
            if (has_c) ok &= save_map(name(sfx_s), p.out_specular, p.st.useSpecularAlpha, p.texture);
            if (has_o) ok &= save_map(name(sfx_o), p.out_occlusion, p.st.useOcclusionAlpha, p.texture);
            if (has_p) ok &= save_map(name(sfx_p), p.out_parallax, p.st.useParallaxAlpha, p.texture);
            (ok ? made : failed)++;
            if (uwr_progress) { /* uwr's maps: a running count, so that a long stage is seen to move */
                size_t done = (size_t)made + (size_t)failed + (size_t)skipped, step = files.size() / 10 ? files.size() / 10 : 1;
                if (done % step == 0 && done < files.size()) {
                    std::lock_guard<std::mutex> lk(print_lock);
                    printf("uwr maps: %s: %zu of %zu pictures\n", uwr_progress, done, files.size());
                    fflush(stdout);
                }
            }
            if (!quiet || !ok) {
                std::lock_guard<std::mutex> lk(print_lock);
                printf("%s %s\n", ok ? "made" : "FAILED", img.string().c_str());
            }
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < jobs; t++) pool.emplace_back(work);
    for (auto &t : pool) t.join();
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    printf("%zu pictures made, %zu skipped, %zu failed, %.1f s\n", (size_t)made, (size_t)skipped, (size_t)failed, s);
    return failed ? 1 : 0;
}

#include "uwr_maps.inc" /* --uwr: every map uwr's renderer reads, in one run (ours, not Laigter's) */

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--uwr")) return uwr_main(argc, argv);
    if (argc > 1 && !strcmp(argv[1], "--uwr-game")) return uwr_game_main(argc, argv);
    return laigter_main(argc, argv);
}
