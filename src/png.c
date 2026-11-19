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
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
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

/* The quick tables (2026-11-17, with inflate_quick below): beside a code's counts and symbols, the symbol and the
 * length of every code of up to QUICK_BITS bits by the stream's next QUICK_BITS bits (a code's bits come first bit
 * first: the index is the code reversed). */
#define QUICK_BITS 10
typedef struct {
    Huff h;
    u16 quick[1 << QUICK_BITS];   /* the symbol | the code's length << 9; 0: a longer code, or none */
} HuffQ;
/* false: more codes than their lengths have room for (no such table is made: the careful decoder reads the stream) */
static bool build_quick(HuffQ *q, const short *length, int n) {
    build(&q->h, length, n);
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1;
        left -= q->h.count[len];
        if (left < 0) return false;
    }
    memset(q->quick, 0, sizeof q->quick);
    int code = 0, index = 0;
    for (int len = 1; len <= QUICK_BITS; len++) {
        for (int k = 0; k < q->h.count[len]; k++, code++, index++) {
            int rev = 0;
            for (int b = 0; b < len; b++) rev |= ((code >> b) & 1) << (len - 1 - b);
            for (int at = rev; at < 1 << QUICK_BITS; at += 1 << len) q->quick[at] = (u16)(q->h.symbol[index] | len << 9);
        }
        code <<= 1;
    }
    return true;
}

/* the fixed codes' tables, made once (before any thread unpacks: png_prefetch) */
static Huff fixed_lc, fixed_dc;
static HuffQ fixed_qlc, fixed_qdc;
static bool fixed_made;
static void fixed_tables(void) {
    if (fixed_made) return;
    short l[288];
    int i = 0;
    for (; i < 144; i++) l[i] = 8;
    for (; i < 256; i++) l[i] = 9;
    for (; i < 280; i++) l[i] = 7;
    for (; i < 288; i++) l[i] = 8;
    build(&fixed_lc, l, 288);
    build_quick(&fixed_qlc, l, 288);
    for (i = 0; i < 30; i++) l[i] = 5;
    build(&fixed_dc, l, 30);
    build_quick(&fixed_qdc, l, 30);
    fixed_made = true;
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
            fixed_tables();
            codes(s, &fixed_lc, &fixed_dc);
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

/* The same stream unpacked quicker (2026-11-17: the first level's build unpacks some 8000 pictures, 70 MB of
 * pixels, and inflate_raw's bit at a time was most of it): 64 bits of the stream at hand, a code looked up in
 * build_quick's table by its next bits, the longer codes walked as decode() walks them. It reads what inflate_raw
 * reads, the same checks; where it gives up for any reason (false) the caller unpacks the stream again with
 * inflate_raw, whose verdict stands: a stream it cannot read is treated exactly as before. */
typedef struct {
    const u8 *in;
    size_t in_n, pos;
    u64 buf;   /* the stream's next bits, the lowest first; cnt of them are counted (those above are the stream's too, or 0 past its end) */
    int cnt;
    u8 *out;
    size_t out_n, out_cap;
} InfQ;

static inline void q_fill(InfQ *s) {
    if (s->pos + 8 <= s->in_n) {
        u64 v;
        memcpy(&v, s->in + s->pos, 8);   /* (little-endian: x86-64, both systems) */
        s->buf |= v << s->cnt;
        s->pos += (size_t)((63 - s->cnt) >> 3);
        s->cnt |= 56;
    } else
        while (s->cnt <= 56 && s->pos < s->in_n) {
            s->buf |= (u64)s->in[s->pos++] << s->cnt;
            s->cnt += 8;
        }
}
/* need bits (up to 16), -1 when the stream ends before them */
static inline int q_bits(InfQ *s, int need) {
    if (s->cnt < need) {
        q_fill(s);
        if (s->cnt < need) return -1;
    }
    int v = (int)(s->buf & ((1u << need) - 1));
    s->buf >>= need, s->cnt -= need;
    return v;
}
static inline int q_decode(InfQ *s, const HuffQ *q) {
    if (s->cnt < 15) q_fill(s);
    u32 e = q->quick[s->buf & ((1u << QUICK_BITS) - 1)];
    if (e) {
        int len = (int)(e >> 9);
        if (len > s->cnt) return -1;
        s->buf >>= len, s->cnt -= len;
        return (int)(e & 511);
    }
    int code = 0, first = 0, index = 0;
    u64 b = s->buf;
    for (int len = 1; len < 16; len++) {
        if (len > s->cnt) return -1;
        code |= (int)(b & 1);
        b >>= 1;
        int count = q->h.count[len];
        if (code - count < first) {
            s->buf >>= len, s->cnt -= len;
            return q->h.symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}
static bool q_codes(InfQ *s, const HuffQ *lc, const HuffQ *dc) {
    static const short lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
                                    4097, 6145, 8193, 12289, 16385, 24577};
    static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = q_decode(s, lc);
        if (sym < 0) return false;
        if (sym < 256) {
            if (s->out_n >= s->out_cap) return false;
            s->out[s->out_n++] = (u8)sym;
        } else if (sym == 256) return true;
        else {
            sym -= 257;
            if (sym >= 29) return false;
            int more = q_bits(s, lext[sym]);
            if (more < 0) return false;
            int len = lbase[sym] + more;
            int ds = q_decode(s, dc);
            if (ds < 0 || ds >= 30) return false;
            more = q_bits(s, dext[ds]);
            if (more < 0) return false;
            size_t dist = (size_t)(dbase[ds] + more);
            if (dist > s->out_n || s->out_n + (size_t)len > s->out_cap) return false;
            u8 *o = s->out + s->out_n;
            const u8 *from = o - dist;
            for (int i = 0; i < len; i++) o[i] = from[i];   /* (a byte at a time: the copy may run into itself) */
            s->out_n += (size_t)len;
        }
    }
}
static bool inflate_quick(InfQ *s) {
    int last;
    fixed_tables();
    do {
        last = q_bits(s, 1);
        int type = q_bits(s, 2);
        if (last < 0 || type < 0) return false;
        if (type == 0) {
            /* (inflate_raw drops the bits of the byte begun; here the whole bytes at hand go back to the stream too) */
            s->pos -= (size_t)(s->cnt >> 3);
            s->buf = 0, s->cnt = 0;
            if (s->pos + 4 > s->in_n) return false;
            size_t len = s->in[s->pos] | (size_t)s->in[s->pos + 1] << 8;
            s->pos += 4;
            if (s->pos + len > s->in_n || s->out_n + len > s->out_cap) return false;
            memcpy(s->out + s->out_n, s->in + s->pos, len);
            s->out_n += len, s->pos += len;
        } else if (type == 1) {
            if (!q_codes(s, &fixed_qlc, &fixed_qdc)) return false;
        } else if (type == 2) {
            static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            int nlen = q_bits(s, 5), ndist = q_bits(s, 5), ncode = q_bits(s, 4);
            if (nlen < 0 || ndist < 0 || ncode < 0) return false;
            nlen += 257, ndist += 1, ncode += 4;
            if (nlen > 286 || ndist > 30) return false;
            short len[320];
            memset(len, 0, sizeof len);
            for (int i = 0; i < ncode; i++) {
                int v = q_bits(s, 3);
                if (v < 0) return false;
                len[order[i]] = (short)v;
            }
            HuffQ lc, dc;
            if (!build_quick(&lc, len, 19)) return false;
            memset(len, 0, sizeof len);
            for (int i = 0; i < nlen + ndist;) {
                int sym = q_decode(s, &lc);
                if (sym < 0) return false;
                if (sym < 16) len[i++] = (short)sym;
                else {
                    short v = 0;
                    int rep;
                    if (sym == 16) {
                        if (!i) return false;
                        v = len[i - 1];
                        rep = q_bits(s, 2);
                        if (rep < 0) return false;
                        rep += 3;
                    } else if (sym == 17) {
                        rep = q_bits(s, 3);
                        if (rep < 0) return false;
                        rep += 3;
                    } else {
                        rep = q_bits(s, 7);
                        if (rep < 0) return false;
                        rep += 11;
                    }
                    if (i + rep > nlen + ndist) return false;
                    while (rep--) len[i++] = v;
                }
            }
            if (!build_quick(&lc, len, nlen) || !build_quick(&dc, len + nlen, ndist)) return false;
            if (!q_codes(s, &lc, &dc)) return false;
        } else return false;
    } while (!last);
    return true;
}

static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

#include <time.h>
long png_stat_n, png_stat_ticks;   /* pictures unpacked and the clock's ticks it took (a level's build prints and zeroes them) */
static bool png_read_file1(const char *path, Image *img);
static bool png_read_file(const char *path, Image *img) {
    clock_t t = clock();
    bool ok = png_read_file1(path, img);
    png_stat_n++, png_stat_ticks += (long)(clock() - t);
    return ok;
}
static bool png_decode(const u8 *data, size_t size, Image *img, bool heap);
static bool png_read_file1(const char *path, Image *img) {
    memset(img, 0, sizeof *img);
    Bytes f = file_read(path);
    return png_decode(f.data, f.size, img, false);
}
/* A PNG file's bytes into a picture. 2026-11-17: apart from the file's reading, so that a thread can unpack one
 * (png_prefetch): heap false, everything in the arena as before; heap true, nothing touches the arena: the working
 * memory is malloc's and freed here, the picture's rgba and idx are malloc's and the caller's to free. And quicker:
 * inflate_quick first; a row's filter picked once a row, not once a byte; the colour type once a picture. */
static bool png_decode(const u8 *data, size_t size, Image *img, bool heap) {
    memset(img, 0, sizeof *img);
    static const u8 sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 33 || memcmp(data, sig, 8)) return false;
    int w = 0, h = 0, depth = 0, type = -1, interlace = 0;
    u8 pal[256][4];
    memset(pal, 255, sizeof pal);
    u8 *z = heap ? malloc(size) : arena_alloc(size); /* all IDAT data, joined */
    u8 *raw = NULL, *px = NULL, *rgba = NULL;
    bool ok = false;
    if (!z) return false;
    size_t zn = 0;
    for (size_t p = 8; p + 12 <= size;) {
        u32 n = get32be(data + p);
        const u8 *t = data + p + 4, *d = t + 4;
        if (p + 12 + n > size) goto out;
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
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || depth != 8 || !ch || interlace || zn < 6) goto out;
    size_t row = (size_t)w * ch + 1, line = (size_t)w * ch;
    raw = heap ? malloc(row * h) : arena_alloc(row * h);
    if (!raw) goto out;
    InfQ sq = {z + 2, zn - 2, 0, 0, 0, raw, 0, row * h};
    if (!inflate_quick(&sq) || sq.out_n != row * h) {   /* (not read the quick way: the careful way, as before) */
        Inf s = {z + 2, zn - 2, 0, 0, 0, raw, 0, row * h, false};
        if (!inflate_raw(&s) || s.out_n != row * h) goto out;
    }
    px = heap ? malloc(line * h) : arena_alloc(line * h);
    if (!px) goto out;
    for (int y = 0; y < h; y++) {
        const u8 *in = raw + y * row + 1;
        u8 *o = px + (size_t)y * line, *up = y ? o - line : NULL;
        size_t x = 0;
        switch (in[-1]) {
        case 0: memcpy(o, in, line); break;
        case 1:
            for (; x < (size_t)ch; x++) o[x] = in[x];
            for (; x < line; x++) o[x] = (u8)(in[x] + o[x - ch]);
            break;
        case 2:
            if (up) for (; x < line; x++) o[x] = (u8)(in[x] + up[x]);
            else memcpy(o, in, line);
            break;
        case 3:
            if (up) {
                for (; x < (size_t)ch; x++) o[x] = (u8)(in[x] + (up[x] >> 1));
                for (; x < line; x++) o[x] = (u8)(in[x] + ((o[x - ch] + up[x]) >> 1));
            } else {
                for (; x < (size_t)ch; x++) o[x] = in[x];
                for (; x < line; x++) o[x] = (u8)(in[x] + (o[x - ch] >> 1));
            }
            break;
        case 4:
            if (up) {
                for (; x < (size_t)ch; x++) o[x] = (u8)(in[x] + paeth(0, up[x], 0));
                for (; x < line; x++) o[x] = (u8)(in[x] + paeth(o[x - ch], up[x], up[x - ch]));
            } else {
                for (; x < (size_t)ch; x++) o[x] = (u8)(in[x] + paeth(0, 0, 0));
                for (; x < line; x++) o[x] = (u8)(in[x] + paeth(o[x - ch], 0, 0));
            }
            break;
        default: goto out;
        }
    }
    size_t count = (size_t)w * h;
    rgba = heap ? malloc(count * 4) : arena_alloc(count * 4);
    if (!rgba) goto out;
    {
        u8 *o = rgba;
        const u8 *q = px;
        switch (type) {
        case 0: for (size_t i = 0; i < count; i++, o += 4, q += 1) o[0] = o[1] = o[2] = q[0], o[3] = 255; break;
        case 2: for (size_t i = 0; i < count; i++, o += 4, q += 3) o[0] = q[0], o[1] = q[1], o[2] = q[2], o[3] = 255; break;
        case 3: for (size_t i = 0; i < count; i++, o += 4, q += 1) memcpy(o, pal[q[0]], 4); break;
        case 4: for (size_t i = 0; i < count; i++, o += 4, q += 2) o[0] = o[1] = o[2] = q[0], o[3] = q[1]; break;
        case 6: memcpy(o, q, count * 4); break;
        }
    }
    img->w = w, img->h = h;
    img->rgba = rgba;
    if (type == 3) img->idx = px;
    ok = true;
out:
    if (heap) {
        free(z), free(raw);
        if (!ok || type != 3) free(px);
        if (!ok) free(rgba);
    }
    return ok;
}

/* The relief maps' pictures kept for the run (2026-11-17). A level's build asked the disk for every one of them again
 * (is it there: file_exists) and read and unpacked it, some thousands of small files (data\laigter\walls, floors,
 * sprites, ui: three maps a texture and a picture): 2.8 s of the 2.9 s a level's change took, the screen black and the
 * game held. They do not change while the game runs (the first run's making of them is over before the first level is
 * built), so each is asked for once and read once, its pixels kept outside the arena; a caller gets its own copy in
 * the arena, as before. The larger textures (laigter\upscaled) are read once anyway and are not kept here. Up to
 * PNG_KEEP_MAX bytes of pixels; past that, read as before. */
#define PNG_KEEP_MAX ((size_t)384 << 20)
typedef struct {
    char *path;
    int there;      /* 1 the file is there, 0 it is not */
    int w, h;
    u8 *rgba, *idx; /* (NULL: not read yet, or it could not be read as a picture: tried again) */
} PngKept;
static PngKept *png_kept;
static u32 *png_slot;                /* a hash table of indices + 1 into png_kept */
static int png_nkept, png_kept_cap, png_slots;
static size_t png_kept_bytes;
static u32 png_path_hash(const char *s) {
    u32 h = 2166136261u;
    for (; *s; s++) h = (h ^ (u8)*s) * 16777619u;
    return h;
}
static bool png_keeps(const char *path) {
    return (strstr(path, "\\laigter\\") || strstr(path, "/laigter/")) && !strstr(path, "upscaled") && !strstr(path, "unpack-tmp");
}
static void png_slots_grow(void) {
    int n = png_slots ? png_slots * 2 : 16384;
    u32 *t = calloc((size_t)n, sizeof *t);
    if (!t) return;
    for (int i = 0; i < png_nkept; i++) {
        u32 at = png_path_hash(png_kept[i].path) & (u32)(n - 1);
        while (t[at]) at = (at + 1) & (u32)(n - 1);
        t[at] = (u32)i + 1;
    }
    free(png_slot);
    png_slot = t, png_slots = n;
}
/* the path's entry when it has one */
static PngKept *png_find(const char *path) {
    if (png_slots)
        for (u32 at = png_path_hash(path) & (u32)(png_slots - 1); png_slot[at]; at = (at + 1) & (u32)(png_slots - 1))
            if (!strcmp(png_kept[png_slot[at] - 1].path, path)) return &png_kept[png_slot[at] - 1];
    return NULL;
}
/* the path's entry, made (with what the disk says: there or not; 2026-11-17: or with what png_prefetch's threads
 * found, there 1 or 0, the disk not asked again) when it has none; NULL when there is no room */
static PngKept *png_entry_as(const char *path, int there) {
    PngKept *found = png_find(path);
    if (found) return found;
    if (png_nkept == png_kept_cap) {
        int cap = png_kept_cap ? png_kept_cap * 2 : 8192;
        PngKept *t = realloc(png_kept, (size_t)cap * sizeof *t);
        if (!t) return NULL;
        png_kept = t, png_kept_cap = cap;
    }
    if ((png_nkept + 1) * 2 > png_slots) png_slots_grow();
    if ((png_nkept + 1) * 2 > png_slots) return NULL;
    PngKept k = {0};
    size_t len = strlen(path) + 1;
    if (!(k.path = malloc(len))) return NULL;
    memcpy(k.path, path, len);
    k.there = there < 0 ? file_exists(path) : there;
    png_kept[png_nkept] = k;
    u32 at = png_path_hash(path) & (u32)(png_slots - 1);
    while (png_slot[at]) at = (at + 1) & (u32)(png_slots - 1);
    png_slot[at] = (u32)++png_nkept;
    return &png_kept[png_nkept - 1];
}
static PngKept *png_entry(const char *path) { return png_entry_as(path, -1); }
/* file_exists for a picture png_read keeps: the disk asked once */
bool png_exists(const char *path) {
    PngKept *k = png_keeps(path) ? png_entry(path) : NULL;
    return k ? k->there != 0 : file_exists(path);
}
/* The pictures png_prefetch read ahead that are NOT kept for the run (2026-11-17: the textures, the objects' and
 * creatures' own pictures: read again at every build, as before): handed over once, to the png_read that asks for
 * the path next, and dropped when the build ends (png_prefetch(NULL, 0)) or the next list is named. */
typedef struct {
    char *path;
    int w, h;
    u8 *rgba, *idx;   /* (rgba NULL: handed over already) */
    int viewed;       /* (2026-11-18) handed over by png_view: its pixels lent, freed with the rest at png_hand_drop */
} PngHanded;
static PngHanded *png_hand;
static u32 *png_hand_slot;   /* a hash table of indices + 1 into png_hand */
static int png_nhand, png_hand_slots;
static void png_hand_drop(void) {
    for (int i = 0; i < png_nhand; i++) free(png_hand[i].path), free(png_hand[i].rgba), free(png_hand[i].idx);
    free(png_hand), free(png_hand_slot);
    png_hand = NULL, png_hand_slot = NULL, png_nhand = png_hand_slots = 0;
}
/* the picture read ahead for this path, into the arena as png_read_file leaves one; false: none (read it) */
static bool png_hand_take(const char *path, Image *img) {
    for (u32 at = png_path_hash(path) & (u32)(png_hand_slots - 1); png_hand_slot[at]; at = (at + 1) & (u32)(png_hand_slots - 1)) {
        PngHanded *p = &png_hand[png_hand_slot[at] - 1];
        if (strcmp(p->path, path)) continue;
        if (!p->rgba || p->viewed) return false;
        size_t n = (size_t)p->w * (size_t)p->h;
        memset(img, 0, sizeof *img);
        img->w = p->w, img->h = p->h;
        img->rgba = arena_alloc(n * 4);
        memcpy(img->rgba, p->rgba, n * 4);
        if (p->idx) img->idx = arena_alloc(n), memcpy(img->idx, p->idx, n);
        free(p->rgba), free(p->idx);
        p->rgba = p->idx = NULL;
        return true;
    }
    return false;
}
/* (2026-11-18) png_read's answer with the pixels lent rather than copied into the arena, for work on other threads
 * (level.c's cells): a kept picture's own (for the run) or a read-ahead one's (handed over as png_read would hand it,
 * freed at the build's next png_prefetch). *copied: the answer is png_read's own, its pixels in the arena (a picture
 * not read ahead). Never to be written to. */
bool png_view(const char *path, Image *img, bool *copied) {
    *copied = false;
    PngKept *k = png_keeps(path) ? png_entry(path) : NULL;
    if (k) {
        memset(img, 0, sizeof *img);
        if (!k->there) return false;
        if (k->rgba) {
            img->w = k->w, img->h = k->h, img->rgba = k->rgba, img->idx = k->idx;
            return true;
        }
    } else if (png_nhand)
        for (u32 at = png_path_hash(path) & (u32)(png_hand_slots - 1); png_hand_slot[at]; at = (at + 1) & (u32)(png_hand_slots - 1)) {
            PngHanded *p = &png_hand[png_hand_slot[at] - 1];
            if (strcmp(p->path, path)) continue;
            if (!p->rgba || p->viewed) break;   /* (handed over already: png_read reads it from the disk) */
            memset(img, 0, sizeof *img);
            p->viewed = 1;
            img->w = p->w, img->h = p->h, img->rgba = p->rgba, img->idx = p->idx;
            return true;
        }
    *copied = true;
    return png_read(path, img);
}
bool png_read(const char *path, Image *img) {
    PngKept *k = png_keeps(path) ? png_entry(path) : NULL;
    if (!k) return (png_nhand && png_hand_take(path, img)) || png_read_file(path, img);
    memset(img, 0, sizeof *img);
    if (!k->there) return false;
    if (k->rgba) {
        size_t n = (size_t)k->w * (size_t)k->h;
        img->w = k->w, img->h = k->h;
        img->rgba = arena_alloc(n * 4);
        memcpy(img->rgba, k->rgba, n * 4);
        if (k->idx) img->idx = arena_alloc(n), memcpy(img->idx, k->idx, n);
        return true;
    }
    if (!png_read_file(path, img)) return false;
    size_t n = (size_t)img->w * (size_t)img->h;
    if (png_kept_bytes + n * 5 > PNG_KEEP_MAX) return true;
    u8 *px = malloc(n * 4 ? n * 4 : 1);
    if (!px) return true;
    memcpy(px, img->rgba, n * 4);
    if (img->idx && (k->idx = malloc(n ? n : 1))) memcpy(k->idx, img->idx, n);
    k->w = img->w, k->h = img->h, k->rgba = px;
    png_kept_bytes += n * 5;
    return true;
}

/* Many pictures read and unpacked at once (2026-11-17). The first level's build read some 8000 pictures one after
 * another, 1.7 s of its 3 s: opening each file (the virus scanner looks at every one) and unpacking it. A build now
 * names the pictures it is about to ask for; those not kept yet are read and unpacked by several threads, each
 * file's bytes and pixels in malloc's memory (png_decode's heap way: the arena is one thread's, and no thread here
 * touches it, the tables below or the files kept in memory), and when all are done this thread puts them in the
 * kept table (the relief maps: png_keeps) or hands them over once (the others: png_hand). png_exists and png_read
 * then answer exactly as if each had been read when asked for; a picture a thread could not open or unpack is left
 * for them to read the old way (a file kept in memory, a damaged one: the same answer as before), and so is every
 * picture where there is no room. (2026-11-18: Linux has the threads too, pthreads; before that it read every
 * picture one after another.) The pixels are png_decode's either way: the same
 * bytes. A build ends with png_prefetch(NULL, 0): what was read ahead and not asked for is dropped, so that
 * nothing read during one build is handed over after it. */
typedef struct {
    const char *path;
    int state;   /* 0 left to png_read, 1 unpacked, 2 a file that is there but did not unpack, 3 no such file */
    int w, h;
    u8 *rgba, *idx;
} PngJob;
static PngJob *pf_job;
#ifdef _WIN32
static LONG pf_n;
static volatile LONG pf_next;
static DWORD WINAPI pf_work(LPVOID unused) {
    (void)unused;
    for (;;) {
        LONG i = InterlockedIncrement(&pf_next) - 1;
        if (i >= pf_n) return 0;
        PngJob *j = &pf_job[i];
        HANDLE f = CreateFileA(j->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (f == INVALID_HANDLE_VALUE) {
            DWORD e = GetLastError();
            if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) j->state = 3;
            continue;
        }
        LARGE_INTEGER size;
        u8 *d = NULL;
        DWORD got = 0;
        bool read = GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < ((LONGLONG)1 << 30) && (d = malloc((size_t)size.QuadPart)) != NULL &&
                    ReadFile(f, d, (DWORD)size.QuadPart, &got, NULL) && got == (DWORD)size.QuadPart;
        CloseHandle(f);
        if (read) {
            Image img;
            j->state = 2;
            if (png_decode(d, (size_t)got, &img, true)) j->state = 1, j->w = img.w, j->h = img.h, j->rgba = img.rgba, j->idx = img.idx;
        }
        free(d);
    }
}
/* the jobs done: by this thread and as many more as the processors and the jobs allow */
static void pf_run(PngJob *job, int jobs) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    /* (this thread works too; two processors left to the rest of the game, its sound; measured on 36: the walls'
     * and objects' pictures 885 ms on one thread, 119 on 8, 75 on 16, 61 on 24, 54 on 36) */
    int threads = MIN(MIN((int)si.dwNumberOfProcessors - 2, 24), jobs / 4) - 1, made = 0;
    HANDLE th[24];
    pf_job = job, pf_n = jobs, pf_next = 0;
    for (int i = 0; i < threads; i++)
        if ((th[made] = CreateThread(NULL, 0, pf_work, NULL, 0, NULL)) != NULL) made++;
    pf_work(NULL);
    if (made) WaitForMultipleObjects((DWORD)made, th, TRUE, INFINITE);
    for (int i = 0; i < made; i++) CloseHandle(th[i]);
    pf_job = NULL, pf_n = 0;
}
#else
static int pf_n, pf_next;
static void *pf_work(void *unused) {
    (void)unused;
    for (;;) {
        int i = __atomic_fetch_add(&pf_next, 1, __ATOMIC_SEQ_CST);
        if (i >= pf_n) return NULL;
        PngJob *j = &pf_job[i];
        char pb[1024];
        size_t c = 0;
        for (; j->path[c] && c + 1 < sizeof pb; c++) pb[c] = j->path[c] == '\\' ? '/' : j->path[c];
        pb[c] = 0;
        if (j->path[c]) continue;   /* (longer than the buffer: left to png_read) */
        int f = open(pb, O_RDONLY | O_CLOEXEC);
        if (f < 0) {
            if (errno == ENOENT || errno == ENOTDIR) j->state = 3;
            continue;
        }
        struct stat st;
        u8 *d = NULL;
        size_t size = 0, got = 0;
        if (!fstat(f, &st) && S_ISREG(st.st_mode) && st.st_size > 0 && st.st_size < ((off_t)1 << 30) && (d = malloc((size_t)st.st_size)) != NULL) {
            size = (size_t)st.st_size;
            while (got < size) {
                ssize_t r = read(f, d + got, size - got);
                if (r < 0 && errno == EINTR) continue;
                if (r <= 0) break;
                got += (size_t)r;
            }
        }
        close(f);
        if (size && got == size) {
            Image img;
            j->state = 2;
            if (png_decode(d, got, &img, true)) j->state = 1, j->w = img.w, j->h = img.h, j->rgba = img.rgba, j->idx = img.idx;
        }
        free(d);
    }
}
static void pf_run(PngJob *job, int jobs) {
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    int threads = MIN(MIN((int)cpus - 2, 24), jobs / 4) - 1, made = 0;
    pthread_t th[24];
    pf_job = job, pf_n = jobs, pf_next = 0;
    for (int i = 0; i < threads; i++)
        if (!pthread_create(&th[made], NULL, pf_work, NULL)) made++;
    pf_work(NULL);
    for (int i = 0; i < made; i++) pthread_join(th[i], NULL);
    pf_job = NULL, pf_n = 0;
}
#endif
void png_prefetch(const char *const *paths, int n) {
    png_hand_drop();
    PngJob *job = n > 0 ? malloc((size_t)n * sizeof *job) : NULL;
    if (!job) return;
    int jobs = 0;
    for (int i = 0; i < n; i++)
        if (!png_keeps(paths[i]) || !png_find(paths[i])) {
            PngJob j = {paths[i], 0, 0, 0, NULL, NULL};
            job[jobs++] = j;
        }
    if (jobs >= 8) {
        clock_t t = clock();
        fixed_tables();
        pf_run(job, jobs);
        int slots = 16;
        while (slots < jobs * 2) slots *= 2;
        png_hand = calloc((size_t)jobs, sizeof *png_hand), png_hand_slot = calloc((size_t)slots, sizeof *png_hand_slot);
        if (png_hand && png_hand_slot) png_hand_slots = slots;
        for (int i = 0; i < jobs; i++) {
            PngJob *j = &job[i];
            size_t count = (size_t)j->w * (size_t)j->h;
            bool taken = false;
            if (png_keeps(j->path)) {
                /* (no such file on the disk: it may still be one of the files kept in memory: file_exists knows) */
                bool settled = j->state == 1 || j->state == 2 || (j->state == 3 && !file_in_memory_root(j->path));
                PngKept *k = settled && !png_find(j->path) ? png_entry_as(j->path, j->state != 3) : NULL;
                if (k && j->state == 1 && png_kept_bytes + count * 5 <= PNG_KEEP_MAX) {
                    k->w = j->w, k->h = j->h, k->rgba = j->rgba, k->idx = j->idx;
                    png_kept_bytes += count * 5;
                    taken = true;
                }
            } else if (j->state == 1 && png_hand_slots) {
                size_t len = strlen(j->path) + 1;
                char *copy = malloc(len);
                if (copy) {
                    memcpy(copy, j->path, len);
                    PngHanded p = {copy, j->w, j->h, j->rgba, j->idx};
                    png_hand[png_nhand] = p;
                    u32 at = png_path_hash(copy) & (u32)(slots - 1);
                    while (png_hand_slot[at]) at = (at + 1) & (u32)(slots - 1);
                    png_hand_slot[at] = (u32)++png_nhand;   /* (a path named twice: found first, handed over first) */
                    taken = true;
                }
            }
            if (taken) png_stat_n++;
            else free(j->rgba), free(j->idx);   /* (named twice, or no room: read the old way when asked for) */
        }
        if (!png_nhand) png_hand_drop();
        png_stat_ticks += (long)(clock() - t);
    }
    free(job);
}
