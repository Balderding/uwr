/* Paths, and PNG writing (stored deflate, identical to tools\loose.py) and reading (full inflate). */
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* --- paths ------------------------------------------------------------------------------------ */
#ifdef _WIN32
const char *root_dir(void) {
    static char root[MAX_PATH];
    if (root[0]) return root;
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);
    if (!n || n >= sizeof exe) fatal("cannot find the exe's folder");
    char *slash = strrchr(exe, '\\');
    if (slash) *slash = 0;
    /* the exe's folder holds game\, or it is build\ inside uo-new */
    char probe[MAX_PATH];
    snprintf(probe, sizeof probe, "%s\\game\\DATA", exe);
    if (GetFileAttributesA(probe) == INVALID_FILE_ATTRIBUTES) {
        char parent[MAX_PATH];
        str_copy(parent, sizeof parent, exe);
        char *up = strrchr(parent, '\\');
        if (up) *up = 0;
        snprintf(probe, sizeof probe, "%s\\game\\DATA", parent);
        if (GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES) str_copy(exe, sizeof exe, parent);
    }
    str_copy(root, sizeof root, exe);
    return root;
}

void path_join(char *out, size_t n, const char *a, const char *b) { snprintf(out, n, "%s\\%s", a, b); }

void make_dirs_for(const char *file_path) {
    char p[MAX_PATH];
    if (file_in_memory_root(file_path)) return;
    str_copy(p, sizeof p, file_path);
    for (char *s = p; *s; s++) {
        if ((*s == '\\' || *s == '/') && s > p && s[-1] != ':') {
            char c = *s;
            *s = 0;
            CreateDirectoryA(p, NULL); /* fails harmlessly when it exists */
            *s = c;
        }
    }
}
#else
/* (the Linux build: the program's own folder, by /proc/self/exe; its folders made with '/') */
const char *root_dir(void) {
    static char root[1024];
    if (root[0]) return root;
    ssize_t n = readlink("/proc/self/exe", root, sizeof root - 1);
    if (n <= 0) fatal("cannot find the program's folder");
    root[n] = 0;
    char *slash = strrchr(root, '/');
    if (slash) *slash = 0;
    return root;
}

void path_join(char *out, size_t n, const char *a, const char *b) { snprintf(out, n, "%s/%s", a, b); }

void make_dirs_for(const char *file_path) {
    char p[1024];
    if (file_in_memory_root(file_path)) return;
    str_copy(p, sizeof p, file_path);
    for (char *s = p; *s; s++) if (*s == '\\') *s = '/';
    for (char *s = p; *s; s++) {
        if (*s == '/' && s > p) {
            *s = 0;
            mkdir(p, 0755); /* fails harmlessly when it exists */
            *s = '/';
        }
    }
}
#endif

/* --- PNG writing -------------------------------------------------------------------------------- */
static u32 crc_table[256];
static void crc_init(void) {
    for (u32 n = 0; n < 256; n++) {
        u32 c = n;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}
static u32 crc(u32 c, const u8 *p, size_t n) {
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}
static void put32be(u8 *p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}
static u32 get32be(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static size_t chunk(u8 *out, const char *type, const u8 *data, size_t n) {
    put32be(out, (u32)n);
    memcpy(out + 4, type, 4);
    if (n) memcpy(out + 8, data, n);
    put32be(out + 8 + n, crc(0xFFFFFFFFu, out + 4, n + 4) ^ 0xFFFFFFFFu);
    return n + 12;
}

/* bpp bytes per pixel; color_type 2, 3 or 6; plte 768 bytes for type 3 */
static bool png_encode(const char *path, const u8 *px, int w, int h, int bpp, int color_type, const u8 *plte) {
    if (!crc_table[1]) crc_init();
    size_t row = (size_t)w * bpp + 1, raw = row * h;
    size_t blocks = (raw + 65534) / 65535;
    size_t zsize = 2 + raw + blocks * 5 + 4;
    size_t total = 8 + 25 + (plte ? 12 + 768 : 0) + (12 + zsize) + 12;
    size_t mark = arena_mark();
    u8 *out = arena_alloc(total), *z = arena_alloc(zsize), *filtered = arena_alloc(raw);
    for (int y = 0; y < h; y++) {
        filtered[y * row] = 0;
        memcpy(filtered + y * row + 1, px + (size_t)y * w * bpp, (size_t)w * bpp);
    }
    size_t zp = 0;
    z[zp++] = 0x78;
    z[zp++] = 0x01;
    u32 a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) {
        a = (a + filtered[i]) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t at = 0; at < raw;) {
        size_t n = MIN(raw - at, 65535);
        z[zp++] = at + n == raw;
        z[zp++] = (u8)n;
        z[zp++] = (u8)(n >> 8);
        z[zp++] = (u8)~n;
        z[zp++] = (u8)(~n >> 8);
        memcpy(z + zp, filtered + at, n);
        zp += n;
        at += n;
    }
    put32be(z + zp, b << 16 | a);
    zp += 4;
    static const u8 sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    size_t op = 0;
    memcpy(out, sig, 8);
    op += 8;
    u8 ihdr[13];
    put32be(ihdr, (u32)w);
    put32be(ihdr + 4, (u32)h);
    ihdr[8] = 8;
    ihdr[9] = (u8)color_type;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    op += chunk(out + op, "IHDR", ihdr, 13);
    if (plte) op += chunk(out + op, "PLTE", plte, 768);
    op += chunk(out + op, "IDAT", z, zp);
    op += chunk(out + op, "IEND", NULL, 0);
    make_dirs_for(path);
    bool ok = file_write(path, out, op);
    arena_reset(mark);
    return ok;
}

bool png_write(const char *path, const u8 *px, int w, int h, int channels) {
    return png_encode(path, px, w, h, channels, channels == 4 ? 6 : 2, NULL);
}
bool png_write_indexed(const char *path, const u8 *idx, int w, int h, const u8 pal[768]) {
    return png_encode(path, idx, w, h, 1, 3, pal);
}

/* --- inflate (RFC 1951) ------------------------------------------------------------------------- */
typedef struct {
    const u8 *in;
    size_t in_n, pos;
    u32 bitbuf;
    int bitcnt;
    u8 *out;
    size_t out_n, out_cap;
    bool bad;
} Inf;

static int bits(Inf *s, int need) {
    u32 v = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->pos >= s->in_n) {
            s->bad = true;
            return 0;
        }
        v |= (u32)s->in[s->pos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> need;
    s->bitcnt -= need;
    return (int)(v & ((1u << need) - 1));
}

typedef struct {
    short count[16];
    short symbol[320];
} Huff;

static int decode(Inf *s, const Huff *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= bits(s, 1);
        if (s->bad) return -1;
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s->bad = true;
    return -1;
}

static void build(Huff *h, const short *length, int n) {
    short offs[16];
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++) h->count[length[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = (short)(offs[len] + h->count[len]);
    for (int i = 0; i < n; i++)
        if (length[i]) h->symbol[offs[length[i]]++] = (short)i;
}

static void put(Inf *s, u8 c) {
    if (s->out_n >= s->out_cap) {
        s->bad = true;
        return;
    }
    s->out[s->out_n++] = c;
}

static void codes(Inf *s, const Huff *lc, const Huff *dc) {
    static const short lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
                                    4097, 6145, 8193, 12289, 16385, 24577};
    static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = decode(s, lc);
        if (s->bad || sym < 0) return;
        if (sym < 256) put(s, (u8)sym);
        else if (sym == 256) return;
        else {
            sym -= 257;
            if (sym >= 29) {
                s->bad = true;
                return;
            }
            int len = lbase[sym] + bits(s, lext[sym]);
            int ds = decode(s, dc);
            if (ds < 0 || ds >= 30) {
                s->bad = true;
                return;
            }
            size_t dist = (size_t)(dbase[ds] + bits(s, dext[ds]));
            if (dist > s->out_n) {
                s->bad = true;
                return;
            }
            for (int i = 0; i < len; i++) put(s, s->out[s->out_n - dist]);
        }
        if (s->bad) return;
    }
}

static bool inflate_raw(Inf *s) {
    int last;
    do {
        last = bits(s, 1);
        int type = bits(s, 2);
        if (s->bad) return false;
        if (type == 0) {
            s->bitbuf = 0, s->bitcnt = 0;
            if (s->pos + 4 > s->in_n) return false;
            size_t len = s->in[s->pos] | (size_t)s->in[s->pos + 1] << 8;
            s->pos += 4;
            if (s->pos + len > s->in_n || s->out_n + len > s->out_cap) return false;
            memcpy(s->out + s->out_n, s->in + s->pos, len);
            s->out_n += len, s->pos += len;
        } else if (type == 1) {
            static Huff lc, dc;
            static bool made;
            if (!made) {
                short l[288];
                int i = 0;
                for (; i < 144; i++) l[i] = 8;
                for (; i < 256; i++) l[i] = 9;
                for (; i < 280; i++) l[i] = 7;
                for (; i < 288; i++) l[i] = 8;
                build(&lc, l, 288);
                for (i = 0; i < 30; i++) l[i] = 5;
                build(&dc, l, 30);
                made = true;
            }
            codes(s, &lc, &dc);
        } else if (type == 2) {
            static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
            if (s->bad || nlen > 286 || ndist > 30) return false;
            short len[320];
            memset(len, 0, sizeof len);
            for (int i = 0; i < ncode; i++) len[order[i]] = (short)bits(s, 3);
            Huff lc, dc;
            build(&lc, len, 19);
            memset(len, 0, sizeof len);
            for (int i = 0; i < nlen + ndist;) {
                int sym = decode(s, &lc);
                if (s->bad || sym < 0) return false;
                if (sym < 16) len[i++] = (short)sym;
                else {
                    short v = 0;
                    int rep;
                    if (sym == 16) {
                        if (!i) return false;
                        v = len[i - 1];
                        rep = 3 + bits(s, 2);
                    } else if (sym == 17) rep = 3 + bits(s, 3);
                    else rep = 11 + bits(s, 7);
                    if (i + rep > nlen + ndist) return false;
                    while (rep--) len[i++] = v;
                }
            }
            build(&lc, len, nlen);
            build(&dc, len + nlen, ndist);
            codes(s, &lc, &dc);
        } else return false;
        if (s->bad) return false;
    } while (!last);
    return true;
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

bool png_read(const char *path, Image *img) {
    memset(img, 0, sizeof *img);
    Bytes f = file_read(path);
    static const u8 sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (f.size < 33 || memcmp(f.data, sig, 8)) return false;
    int w = 0, h = 0, depth = 0, type = -1, interlace = 0;
    u8 pal[256][4];
    memset(pal, 255, sizeof pal);
    u8 *z = arena_alloc(f.size); /* all IDAT data, joined */
    size_t zn = 0;
    for (size_t p = 8; p + 12 <= f.size;) {
        u32 n = get32be(f.data + p);
        const u8 *t = f.data + p + 4, *d = t + 4;
        if (p + 12 + n > f.size) return false;
        if (!memcmp(t, "IHDR", 4) && n >= 13) {
            w = (int)get32be(d), h = (int)get32be(d + 4), depth = d[8], type = d[9], interlace = d[12];
        } else if (!memcmp(t, "PLTE", 4)) {
            for (u32 i = 0; i < n / 3 && i < 256; i++) pal[i][0] = d[3 * i], pal[i][1] = d[3 * i + 1], pal[i][2] = d[3 * i + 2];
        } else if (!memcmp(t, "tRNS", 4) && type == 3) {
            for (u32 i = 0; i < n && i < 256; i++) pal[i][3] = d[i];
        } else if (!memcmp(t, "IDAT", 4)) {
            memcpy(z + zn, d, n);
            zn += n;
        } else if (!memcmp(t, "IEND", 4)) break;
        p += 12 + n;
    }
    int ch = type == 0 ? 1 : type == 2 ? 3 : type == 3 ? 1 : type == 4 ? 2 : type == 6 ? 4 : 0;
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || depth != 8 || !ch || interlace || zn < 6) return false;
    size_t row = (size_t)w * ch + 1;
    Inf s = {z + 2, zn - 2, 0, 0, 0, arena_alloc(row * h), 0, row * h, false};
    if (!inflate_raw(&s) || s.out_n != row * h) return false;
    u8 *px = arena_alloc((size_t)w * h * ch);
    for (int y = 0; y < h; y++) {
        const u8 *in = s.out + y * row;
        u8 *o = px + (size_t)y * w * ch, *up = y ? o - (size_t)w * ch : NULL;
        int ft = in[0];
        for (int x = 0; x < w * ch; x++) {
            int a = x >= ch ? o[x - ch] : 0, b = up ? up[x] : 0, c = (up && x >= ch) ? up[x - ch] : 0, v = in[1 + x];
            switch (ft) {
            case 0: break;
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) >> 1; break;
            case 4: v += paeth(a, b, c); break;
            default: return false;
            }
            o[x] = (u8)v;
        }
    }
    img->w = w, img->h = h;
    img->rgba = arena_alloc((size_t)w * h * 4);
    if (type == 3) img->idx = px;
    for (size_t i = 0; i < (size_t)w * h; i++) {
        u8 *o = img->rgba + i * 4;
        const u8 *q = px + i * ch;
        switch (type) {
        case 0: o[0] = o[1] = o[2] = q[0], o[3] = 255; break;
        case 2: o[0] = q[0], o[1] = q[1], o[2] = q[2], o[3] = 255; break;
        case 3: memcpy(o, pal[q[0]], 4); break;
        case 4: o[0] = o[1] = o[2] = q[0], o[3] = q[1]; break;
        case 6: memcpy(o, q, 4); break;
        }
    }
    return true;
}
