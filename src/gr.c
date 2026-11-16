#include "gr.h"

#include <string.h>

int gr_count(Bytes gr) { return gr.size >= 3 ? (int)rd16(gr.data + 1) : 0; }

typedef struct {
    const u8 *d;
    size_t n, i; /* nibble position */
} Nib;
static int nib(Nib *s) {
    size_t b = s->i >> 1;
    int v = b < s->n ? ((s->i & 1) ? s->d[b] & 15 : s->d[b] >> 4) : 0;
    s->i++;
    return v;
}
static int nib_count(Nib *s) {
    int c = nib(s);
    if (c) return c;
    c = nib(s) << 4, c |= nib(s);
    if (c) return c;
    c = nib(s) << 8, c |= nib(s) << 4, c |= nib(s);
    return c;
}

bool gr_picture(Bytes gr, int k, const u8 *allpals, u8 *out, int *w, int *h) {
    int count = gr_count(gr);
    if (k < 0 || k >= count || 3 + 4 * (size_t)count > gr.size) return false;
    u32 o = rd32(gr.data + 3 + 4 * k);
    if (o + 3 > gr.size) return false;
    int type = gr.data[o], W = gr.data[o + 1], H = gr.data[o + 2], n = W * H;
    *w = W, *h = H;
    memset(out, 0, (size_t)n);
    if (type == 4) {
        for (int i = 0; i < n && o + 5 + (size_t)i < gr.size; i++) out[i] = gr.data[o + 5 + i];
        return true;
    }
    if (type != 8 && type != 10) return false;
    const u8 *pal = allpals + (gr.data[o + 3] & 31) * 16;
    Nib s = {gr.data, gr.size, (size_t)(o + 6) * 2};
    if (type == 10) {
        for (int i = 0; i < n; i++) out[i] = pal[nib(&s)];
        return true;
    }
    /* run-length: repeat records and run records by turns (tools\gr.py) */
    int at = 0, stage = 0, repeats = 0, guard = 0;
    while (at < n && guard++ < n * 4) {
        if (stage == 0) {
            int c = nib_count(&s);
            if (c == 1) {
                stage = 1;
                continue;
            }
            if (c == 2) {
                repeats = nib_count(&s) - 1;
                continue;
            }
            int v = pal[nib(&s)];
            for (int i = 0; i < c && at < n; i++) out[at++] = (u8)v;
            if (repeats == 0) stage = 1;
            else repeats--;
        } else {
            int c = nib_count(&s);
            for (int i = 0; i < c && at < n; i++) out[at++] = pal[nib(&s)];
            stage = 0;
        }
    }
    return true;
}

/* --- creature frames: the same run-length records in 4- or 5-bit units; counts of more than one unit combine
 * 4 bits a unit even in 5-bit data ------------------------------------------------------------------------ */
typedef struct {
    const u8 *d;
    size_t n, i; /* bit position */
    int size;
} Bit;
static int bit_get(Bit *s) {
    int v = 0;
    for (int k = 0; k < s->size; k++, s->i++) {
        size_t b = s->i >> 3;
        v = v << 1 | (b < s->n ? (s->d[b] >> (7 - (s->i & 7))) & 1 : 0);
    }
    return v;
}
static int bit_count(Bit *s) {
    int c = bit_get(s);
    if (c) return c;
    c = bit_get(s) << 4, c |= bit_get(s);
    if (c) return c;
    c = bit_get(s) << 8, c |= bit_get(s) << 4, c |= bit_get(s);
    return c;
}

bool crit_page(Bytes d, CritPage *p) {
    memset(p, 0, sizeof *p);
    if (d.size < 4) return false;
    size_t at = 1;
    p->nslots = d.data[at++];
    for (int i = 0; i < p->nslots && at < d.size; i++) p->slots[i] = d.data[at++];
    if (at >= d.size) return false;
    p->nsegs = d.data[at++];
    for (int i = 0; i < p->nsegs && at + 8 <= d.size; i++, at += 8) memcpy(p->segs[i], d.data + at, 8);
    if (at >= d.size) return false;
    p->npals = d.data[at++];
    for (int i = 0; i < p->npals && at + 32 <= d.size; i++, at += 32)
        if (i < 16) memcpy(p->pals[i], d.data + at, 32);
    if (at + 2 > d.size) return false;
    p->nframes = d.data[at];
    at += 2;
    for (int i = 0; i < p->nframes && at + 2 <= d.size; i++, at += 2) p->off[i] = (u16)rd16(d.data + at);
    if (p->npals > 16) p->npals = 16;
    return true;
}

bool crit_frame(Bytes d, const CritPage *p, int k, u8 *out, int *w, int *h, int *hx, int *hy) {
    if (k < 0 || k >= p->nframes) return false;
    size_t o = p->off[k];
    if (o + 7 > d.size) return false;
    int W = d.data[o], H = d.data[o + 1], n = W * H;
    *w = W, *h = H, *hx = d.data[o + 2], *hy = d.data[o + 3];
    memset(out, 0, (size_t)n);
    Bit s = {d.data, d.size, (o + 7) * 8, d.data[o + 4] == 6 ? 5 : 4};
    int at = 0, stage = 0, repeats = 0, guard = 0;
    while (at < n && guard++ < n * 4) {
        if (stage == 0) {
            int c = bit_count(&s);
            if (c == 1) {
                stage = 1;
                continue;
            }
            if (c == 2) {
                repeats = bit_count(&s) - 1;
                continue;
            }
            int v = bit_get(&s);
            for (int i = 0; i < c && at < n; i++) out[at++] = (u8)v;
            if (repeats <= 0) stage = 1, repeats = 0;
            else repeats--;
        } else {
            int c = bit_count(&s);
            for (int i = 0; i < c && at < n; i++) out[at++] = (u8)bit_get(&s);
            stage = 0;
        }
    }
    return true;
}

bool strings_each(Bytes d, void (*each)(void *ctx, int block, int index, const char *s, int n), void *ctx) {
    if (d.size < 4) return false;
    int nn = (int)rd16(d.data);
    const u8 *nodes = d.data + 2;
    size_t at = 2 + 4 * (size_t)nn;
    if (at + 2 > d.size) return false;
    int nb = (int)rd16(d.data + at);
    at += 2;
    static char buf[8192];
    for (int b = 0; b < nb; b++) {
        if (at + 6 * (size_t)(b + 1) > d.size) return false;
        int num = (int)rd16(d.data + at + 6 * b);
        u32 off = rd32(d.data + at + 6 * b + 2);
        if (off + 2 > d.size) return false;
        int ns = (int)rd16(d.data + off);
        size_t base = off + 2 + 2 * (size_t)ns;
        for (int i = 0; i < ns; i++) {
            size_t p = base + rd16(d.data + off + 2 + 2 * i);
            int bit = 7, len = 0;
            while (p < d.size && len < (int)sizeof buf - 1) {
                int node = nn - 1, guard = 0;
                while (nodes[node * 4 + 2] != 0xFF && guard++ < 64) {
                    if (p >= d.size) break;
                    int v = (d.data[p] >> bit) & 1;
                    if (--bit < 0) bit = 7, p++;
                    node = v ? nodes[node * 4 + 3] : nodes[node * 4 + 2];
                    if (node >= nn) node = nn - 1;
                }
                char ch = (char)nodes[node * 4];
                if (ch == '|') break;
                buf[len++] = ch;
            }
            buf[len] = 0;
            each(ctx, num, i, buf, len);
        }
    }
    return true;
}
