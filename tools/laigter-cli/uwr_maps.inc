/*
 * uwr_maps.inc: part of laigter_cli.cpp (GPL v3, as that file). Ours, not Laigter's.
 *
 * laigter-cli --uwr DATA [PRESETS] [-j N]            from loose pictures (the checks)
 * laigter-cli --uwr-game GAME OUT [PRESETS] [-j N]   from the player's own game files (the game's first start)
 * makes every map uwr's renderer reads, in one run, from the game data the player's own copy was unpacked to.
 * The steps are make-normals.cmd's, then for the walls and floors tools\upsize_normals.py and
 * tools\make_soft_maps.py (variant A), ported here so that no Python is needed:
 *   sprites, doors, creatures, items, effects: Laigter's normal, occlusion and height maps by their presets
 *   walls, floors: Laigter's normal map (texture.preset, tiled) enlarged 4x by a bicubic curve that wraps round the
 *                  tile (Pillow's BICUBIC on float pictures, its arithmetic kept), the normals brought back to
 *                  length 1; the soft height and occlusion maps at 4x from the texture's grey
 */
static const int UWR_UP = 4, UWR_PAD = 8;

template <class F> static void parallel_for(size_t n, int jobs, F f) {
    std::atomic<size_t> next{0};
    auto work = [&]() {
        for (;;) {
            size_t i = next++;
            if (i >= n) return;
            f(i);
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < jobs; t++) pool.emplace_back(work);
    for (auto &t : pool) t.join();
}

/* Pillow's bicubic_filter (src/libImaging/Resample.c) */
static double pil_bicubic(double x) {
    const double a = -0.5;
    if (x < 0.0) x = -x;
    if (x < 1.0) return ((a + 2.0) * x - (a + 3.0)) * x * x + 1;
    if (x < 2.0) return (((x - 5) * x + 8) * x - 4) * a;
    return 0.0;
}

struct Taps {
    int first[5];
    double k[5];
    int n;
};
/* Pillow's precompute_coeffs for a picture of n texels with UWR_PAD wrapped texels each side, enlarged UWR_UP times,
 * for the output texels inside the picture itself; first[] are the source texels, wrapped round */
static std::vector<Taps> uwr_taps(int n) {
    std::vector<Taps> out((size_t)n * UWR_UP);
    const double scale = (double)(n + 2 * UWR_PAD) / (double)((n + 2 * UWR_PAD) * UWR_UP), support = 2.0;
    for (int X = 0; X < n * UWR_UP; X++) {
        int xx = X + UWR_PAD * UWR_UP;
        double center = (xx + 0.5) * scale, ww = 0.0;
        int xmin = (int)(center - support + 0.5), xmax = (int)(center + support + 0.5);
        if (xmin < 0) xmin = 0;
        if (xmax > n + 2 * UWR_PAD) xmax = n + 2 * UWR_PAD;
        Taps &t = out[X];
        t.n = std::min(5, xmax - xmin);
        for (int x = 0; x < t.n; x++) {
            double w = pil_bicubic((x + xmin - center + 0.5) * 1.0);
            t.k[x] = w, ww += w;
            t.first[x] = (((x + xmin - UWR_PAD) % n) + n) % n;
        }
        for (int x = 0; x < t.n; x++)
            if (ww != 0.0) t.k[x] /= ww;
    }
    return out;
}

/* rgb: w*h*3 bytes (a normal map); out: (w*4)*(h*4)*3 bytes */
static std::vector<uchar> uwr_upsize_normal(const std::vector<uchar> &rgb, int w, int h) {
    int W = w * UWR_UP, H = h * UWR_UP;
    std::vector<Taps> tx = uwr_taps(w), ty = uwr_taps(h);
    std::vector<float> mid((size_t)W * h * 3), big((size_t)W * H * 3);
    for (int c = 0; c < 3; c++) {
        for (int y = 0; y < h; y++) /* Pillow: the horizontal pass first, sums in double, kept as float */
            for (int X = 0; X < W; X++) {
                const Taps &t = tx[X];
                double ss = 0.0;
                for (int x = 0; x < t.n; x++) ss += (float)rgb[((size_t)y * w + t.first[x]) * 3 + c] * t.k[x];
                mid[((size_t)y * W + X) * 3 + c] = (float)ss;
            }
        for (int Y = 0; Y < H; Y++) {
            const Taps &t = ty[Y];
            for (int X = 0; X < W; X++) {
                double ss = 0.0;
                for (int y = 0; y < t.n; y++) ss += mid[((size_t)t.first[y] * W + X) * 3 + c] * t.k[y];
                big[((size_t)Y * W + X) * 3 + c] = (float)ss;
            }
        }
    }
    std::vector<uchar> out((size_t)W * H * 3);
    for (size_t i = 0; i < (size_t)W * H; i++) { /* numpy's float32 arithmetic, in its order */
        float n[3];
        for (int c = 0; c < 3; c++) n[c] = big[i * 3 + c] / 127.5f - 1.0f;
        float q0 = n[0] * n[0], q1 = n[1] * n[1], q2 = n[2] * n[2];
        float s = q0 + q1;
        s = s + q2;
        float len = std::sqrt(s);
        if (len < 1e-6f) len = 1e-6f;
        for (int c = 0; c < 3; c++) {
            float v = n[c] / len;
            v = (v + 1.0f) * 127.5f;
            v = v + 0.5f;
            out[i * 3 + c] = (uchar)(v < 0.0f ? 0.0f : v > 255.0f ? 255.0f : v);
        }
    }
    return out;
}

/* make_soft_maps.py's blur: a Gaussian that wraps round the picture, the same one its FFT applies (the kernel is the
 * inverse transform of exp(-2 (pi sigma f)^2), separable), laid on directly; taps too small to matter are left out */
static std::vector<double> uwr_kernel(int n, double sigma) {
    const double pi = 3.14159265358979323846;
    std::vector<double> g(n), k(n);
    for (int j = 0; j < n; j++) {
        double f = (double)(j < (n + 1) / 2 ? j : j - n) / n;
        g[j] = std::exp(-2.0 * (pi * sigma) * (pi * sigma) * f * f);
    }
    for (int i = 0; i < n; i++) {
        double s = 0.0;
        for (int j = 0; j < n; j++) s += g[j] * std::cos(2.0 * pi * (double)(((long long)j * i) % n) / n);
        k[i] = s / n;
    }
    return k;
}
static void uwr_blur(std::vector<double> &a, int w, int h, double sigma) {
    if (sigma <= 0) return;
    std::vector<double> kx = uwr_kernel(w, sigma), ky = uwr_kernel(h, sigma), t((size_t)w * h);
    auto taps = [](const std::vector<double> &k) {
        std::vector<int> o;
        for (int i = 0; i < (int)k.size(); i++)
            if (std::fabs(k[i]) > 1e-15 * std::fabs(k[0])) o.push_back(i);
        return o;
    };
    std::vector<int> ox = taps(kx), oy = taps(ky);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            double s = 0.0;
            for (int d : ox) s += kx[d] * a[(size_t)y * w + (x - d + w) % w];
            t[(size_t)y * w + x] = s;
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            double s = 0.0;
            for (int d : oy) s += ky[d] * t[(size_t)((y - d + h) % h) * w + x];
            a[(size_t)y * w + x] = s;
        }
}

/* make_soft_maps.py make(g, "A"): rgba the texture as read (not premultiplied); ao and hgt at 4x, 0..255 */
static void uwr_soft_maps(const uchar *rgba, int w, int h, std::vector<uchar> &ao8, std::vector<uchar> &hgt8) {
    const double hb = 0.70, an = 1.2, aw = 1.6, kn = 3.6, kw = 2.6, floor_ = 0.45; /* variant A (the user, 2026-11-16) */
    int W = w * UWR_UP, H = h * UWR_UP;
    std::vector<double> g((size_t)w * h);
    for (size_t i = 0; i < g.size(); i++)
        g[i] = ((double)rgba[i * 4] * 11 + (double)rgba[i * 4 + 1] * 16 + (double)rgba[i * 4 + 2] * 5) / 32.0 / 255.0;
    std::vector<double> sorted = g;
    std::sort(sorted.begin(), sorted.end());
    size_t m = sorted.size() / 2;
    double med = sorted.size() % 2 ? sorted[m] : (sorted[m - 1] + sorted[m]) / 2.0;
    std::vector<double> b((size_t)W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) b[(size_t)y * W + x] = g[(size_t)(y / UWR_UP) * w + x / UWR_UP];
    uwr_blur(b, W, H, hb * UWR_UP);
    double wd = std::max(0.14, 0.6 * med);
    std::vector<double> hgt((size_t)W * H);
    for (size_t i = 0; i < hgt.size(); i++) {
        double d = (b[i] - med) / wd;
        double t = d < 0 ? std::tanh(d) : 0.35 * std::tanh(d);
        hgt[i] = 0.62 + 0.36 * t;
    }
    std::vector<double> near_ = hgt;
    uwr_blur(near_, W, H, an * UWR_UP);
    for (size_t i = 0; i < hgt.size(); i++) near_[i] = std::max(near_[i] - hgt[i], 0.0);
    std::vector<double> wide = near_;
    uwr_blur(wide, W, H, aw * UWR_UP);
    std::vector<double> ao((size_t)W * H);
    for (size_t i = 0; i < ao.size(); i++) ao[i] = 1.0 - kn * near_[i] - kw * wide[i];
    uwr_blur(ao, W, H, 0.35 * UWR_UP);
    auto to8 = [](double v) { return (uchar)((v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v) * 255 + 0.5); };
    ao8.resize(ao.size()), hgt8.resize(hgt.size());
    for (size_t i = 0; i < ao.size(); i++) {
        ao8[i] = to8(ao[i] < floor_ ? floor_ : ao[i] > 1.0 ? 1.0 : ao[i]);
        hgt8[i] = to8(hgt[i]);
    }
}

/* the texture as png_read gives it (make_soft_maps.py reads the plain colours) */
static bool load_plain(const fs::path &path, std::vector<uchar> &rgba, int &w, int &h) {
    std::lock_guard<std::mutex> lk(g_png_lock);
    size_t mark = arena_mark();
    Image img = {};
    bool ok = png_read(path.string().c_str(), &img) && img.w > 0 && img.h > 0;
    if (ok) {
        w = img.w, h = img.h;
        rgba.assign(img.rgba, img.rgba + (size_t)img.w * img.h * 4);
    }
    arena_reset(mark);
    return ok;
}

/* walls or floors: DATA\textures\<kind>\NNN.png -> DATA\laigter\<kind>\NNN_n.png, _o, _p (all at 4x) */
static size_t uwr_tiles(const fs::path &data, const fs::path &outdir, const char *kind, const fs::path &preset, int jobs) {
    Settings base;
    if (!apply_preset_file(preset.string(), base)) {
        fprintf(stderr, "preset %s: cannot open\n", preset.string().c_str());
        return 1;
    }
    base.tileable = true;
    base.occlusion_thresh_median = true;
    std::vector<fs::path> files;
    std::error_code ec;
    for (auto &e : fs::directory_iterator(data / "textures" / kind, ec))
        if (e.is_regular_file() && lower(e.path().extension().string()) == ".png") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        fprintf(stderr, "no textures in %s\n", (data / "textures" / kind).string().c_str());
        return 1;
    }
    fs::path out = outdir / "laigter" / kind;
    std::atomic<size_t> failed{0}, done{0};
    std::mutex say;
    const size_t step = files.size() / 10 ? files.size() / 10 : 1;
    printf("uwr maps: %s: normal, occlusion and height maps for %zu textures\n", kind, files.size());
    fflush(stdout);
    parallel_for(files.size(), jobs, [&](size_t i) {
        struct Count { /* a running count as each texture is left, made or not */
            std::atomic<size_t> &done;
            std::mutex &say;
            size_t n, step;
            const char *kind;
            ~Count() {
                size_t d = ++done;
                if (d % step == 0 && d < n) {
                    std::lock_guard<std::mutex> lk(say);
                    printf("uwr maps: %s: %zu of %zu textures\n", kind, d, n);
                    fflush(stdout);
                }
            }
        } count{done, say, files.size(), step, kind};
        Picture pic;
        std::vector<uchar> plain;
        int w = 0, h = 0;
        if (!load_picture(files[i], pic) || !load_plain(files[i], plain, w, h) || w < 3 || h < 3) return;
        Processor p;
        p.st = base;
        p.has_normal = true, p.has_specular = false, p.has_occlusion = false, p.has_parallax = false;
        p.load(pic);
        const CImg<uchar> &c = p.out_normal;
        if (c.width() != w || c.height() != h || c.spectrum() < 3) {
            failed++;
            return;
        }
        std::vector<uchar> rgb((size_t)w * h * 3);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                for (int k = 0; k < 3; k++) rgb[((size_t)y * w + x) * 3 + k] = c(x, y, 0, k);
        std::vector<uchar> big = uwr_upsize_normal(rgb, w, h), ao, hgt;
        uwr_soft_maps(plain.data(), w, h, ao, hgt);
        std::string stem = files[i].stem().string();
        bool ok = png_save(out / (stem + "_n.png"), big.data(), w * UWR_UP, h * UWR_UP, 3) &&
                  png_save(out / (stem + "_o.png"), ao.data(), w * UWR_UP, h * UWR_UP, 1) &&
                  png_save(out / (stem + "_p.png"), hgt.data(), w * UWR_UP, h * UWR_UP, 1);
        if (!ok) failed++;
    });
    printf("uwr maps: %s: %zu textures' maps made, %zu failed\n", kind, files.size(), (size_t)failed);
    fflush(stdout);
    return failed;
}

/* data: a folder with the game's pictures as loose files (textures\, sprites\, ui\); out: where laigter\ is made
 * (out\laigter\..., and last of all out\laigter\made.txt, so a run cut short is made again) */
static int uwr_run(const char *self, const fs::path &data, const fs::path &out, const fs::path &presets, int jobs) {
    std::error_code ec;
    auto t0 = std::chrono::steady_clock::now();
    tool_init();
    fs::remove(out / "laigter" / "made.txt", ec);
    size_t bad = 0;
    bad += uwr_tiles(data, out, "walls", presets / "texture.preset", jobs);
    bad += uwr_tiles(data, out, "floors", presets / "texture.preset", jobs);
    struct Job {
        const char *src, *preset;
        bool median;
    };
    const Job todo[] = {{"sprites/doors", "sprite_op.preset", false},        {"sprites/flat_objects", "sprite_op.preset", false},
                        {"sprites/wall_objects", "sprite_op.preset", false}, {"sprites/creatures", "creature.preset", true},
                        {"sprites/objects", "item.preset", true},            {"ui/animo", "sprite_op.preset", false}};
    for (const Job &j : todo) {
        std::string src = (data / j.src).string(), dst = (out / "laigter" / j.src).string(),
                    pre = (presets / j.preset).string(), js = std::to_string(jobs);
        if (!fs::is_directory(data / j.src, ec) || !fs::is_regular_file(presets / j.preset, ec)) {
            fprintf(stderr, "missing: %s or %s\n", src.c_str(), pre.c_str());
            bad++;
            continue;
        }
        std::vector<const char *> a = {self, "-q", "-n", "-o", "-p", "-r", pre.c_str(), "-d", src.c_str(),
                                       "-l", dst.c_str(), "-j", js.c_str()};
        if (j.median) a.push_back("--occlusion-thresh-median");
        printf("uwr maps: %s: normal, occlusion and height maps\n", j.src);
        fflush(stdout);
        uwr_progress = j.src;
        if (laigter_main((int)a.size(), (char **)a.data())) bad++;
        uwr_progress = NULL;
        fflush(stdout);
    }
    double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (bad) {
        printf("uwr maps: %zu steps FAILED, %.1f s\n", bad, s);
        fflush(stdout);
        return 1;
    }
    std::ofstream(out / "laigter" / "made.txt") << "made by laigter-cli from the game's own textures and sprites\n";
    printf("uwr maps: all made in %s, %.1f s\n", (out / "laigter").string().c_str(), s);
    fflush(stdout);
    return 0;
}

/* the folder this program is in (the .preset files' default place) */
static fs::path uwr_self_dir(const char *argv0) {
    std::error_code ec;
#ifdef _WIN32
    char p[1024];
    unsigned long n = GetModuleFileNameA(nullptr, p, sizeof p);
    if (n && n < sizeof p) return fs::path(p).parent_path();
#else
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return exe.parent_path();
#endif
    return fs::absolute(argv0, ec).parent_path();
}

/* laigter-cli --uwr DATA [PRESETS] [-j N]: DATA the folder of loose pictures; the maps go to DATA\laigter */
static int uwr_main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "laigter-cli --uwr DATA [PRESETS] [-j N]\n");
        return 2;
    }
    fs::path data = fs::absolute(argv[2]), presets;
    int jobs = (int)std::max(1u, std::thread::hardware_concurrency());
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-j") && i + 1 < argc) jobs = std::max(1, atoi(argv[++i]));
        else presets = fs::absolute(argv[i]);
    }
    if (presets.empty()) presets = uwr_self_dir(argv[0]);
    return uwr_run(argv[0], data, data, presets, jobs);
}

/* laigter-cli --uwr-game GAME OUT [PRESETS] [-j N]: what the game runs on its first start. GAME is the player's own
 * copy (UW.EXE, DATA\ ...); its pictures are unpacked by the game's own extractor (src\extract.c) into
 * OUT\laigter\unpack-tmp, the maps are made from them into OUT\laigter, and the unpacked pictures are removed again:
 * only the maps stay on the disk */
extern "C" int extract_missing(const char *data_dir, const char *game_dir);
static int uwr_game_main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "laigter-cli --uwr-game GAME OUT [PRESETS] [-j N]\n");
        return 2;
    }
    std::error_code ec;
    fs::path game = fs::absolute(argv[2]), out = fs::absolute(argv[3]), presets;
    int jobs = (int)std::max(1u, std::thread::hardware_concurrency());
    for (int i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "-j") && i + 1 < argc) jobs = std::max(1, atoi(argv[++i]));
        else presets = fs::absolute(argv[i]);
    }
    if (presets.empty()) presets = uwr_self_dir(argv[0]);
    fs::path tmp = out / "laigter" / "unpack-tmp";
    fs::remove_all(tmp, ec); /* (ours, of a run cut short) */
    fs::create_directories(tmp, ec);
    tool_init();
    printf("uwr maps: reading the game's own pictures from %s\n", game.string().c_str());
    fflush(stdout);
    int made = extract_missing(tmp.string().c_str(), game.string().c_str());
    int r = 1;
    if (made < 0) fprintf(stderr, "laigter-cli: cannot read the game's files in %s\n", game.string().c_str());
    else r = uwr_run(argv[0], tmp, out, presets, jobs);
    fs::remove_all(tmp, ec);
    return r;
}
