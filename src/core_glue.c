#include <stdlib.h>
#include <stdio.h>
/* core_glue.c: what the rules core (underworld-exhumed's C, tools/core.py) needs from outside that has to be exact:
 * Borland's C library pieces the rules use (rand above all: the same LCG and state as UW.EXE's, 0x12c37), and the
 * portability layer's few names (Exhume's compat.h and portable.h). Everything else the core calls outside itself
 * is a generated stub (build/gen/core_stubs.c) until the front end provides it.
 * Compiled with compat.h force-included, like the core. */
#include <stdarg.h>
#include "uw2.h"

/* --- Borland's rand (UW.EXE 0x12c37 / srand 0x12c26): seed = seed * 0x015A4E35 + 1, (seed >> 16) & 0x7FFF */
uint32_t core_seed = 1;
int bc_rand(void) { core_seed = core_seed * 0x015A4E35u + 1; return (int)(core_seed >> 16 & 0x7FFF); }
void bc_srand(unsigned seed) { core_seed = (uint16_t)seed; }
void rp_srand(unsigned seed) { bc_srand(seed); }

/* --- the game clock (GAME_TIME(): seg021's 1/256 s count, *Time) and the time of day */
uint32_t core_time;
uint32_t (*core_clock_fn)(void); /* the game's: real time in 1/256 s, as TICKS.ASM's timer kept it */
uint32 rp_time(void) { return core_clock_fn ? core_clock_fn() : core_time; }
int32 rp_walltime(void) { return 0; }
void rp_checkpoint(int n) { (void)n; }

/* --- the outside calls a run made (the stubs' and the glue's), for the proofs: names, NUL separated */
char core_calls[0x4000];
int core_ncalls, core_callpos;
void core_stub_log(const char *name)
{
    int n = 0;
    while (name[n]) n++;
    if (core_callpos + n + 1 < (int)sizeof core_calls) {
        for (int i = 0; i <= n; i++) core_calls[core_callpos + i] = name[i];
        core_callpos += n + 1;
    }
    core_ncalls++;
    static int trace = -1; /* UW_STUB_TRACE: each outside call on stderr (the checks) */
    if (trace < 0) { const char *e = getenv("UW_STUB_TRACE"); trace = e && *e; }
    if (trace) fprintf(stderr, "stub %s\n", name);
}
/* an outside call with its arguments (tools/core.py's SINKS): name(arg;...), F and N a string, w a word, b a byte,
 * X a far pointer (shown as *); a string's unprintable characters, quote, comma and semicolon as \hh */
void core_stub_logv(const char *name, const char *kinds, long long a0, long long a1, long long a2, long long a3)
{
    char buf[400];
    long long a[4] = { a0, a1, a2, a3 };
    int n = snprintf(buf, sizeof buf, "%s(", name);
    for (int i = 0; kinds[i] && i < 4 && n < (int)sizeof buf - 80; i++) {
        if (i) buf[n++] = ';';
        if (kinds[i] == 'F' || kinds[i] == 'N') {
            const unsigned char *s = (const unsigned char *)(intptr_t)a[i];
            buf[n++] = '"';
            for (int k = 0; s && s[k] && k < 200 && n < (int)sizeof buf - 8; k++)
                n += snprintf(buf + n, sizeof buf - n, s[k] < 32 || s[k] > 126 || s[k] == '"' || s[k] == ',' || s[k] == ';' || s[k] == 92 ? "\\%02x" : "%c", s[k]);
            buf[n++] = '"';
        } else if (kinds[i] == 'b') n += snprintf(buf + n, sizeof buf - n, "%d", (int)(unsigned char)a[i]);
        else if (kinds[i] == 'w') n += snprintf(buf + n, sizeof buf - n, "%d", (int)(int16_t)a[i]);
        else buf[n++] = '*';
    }
    buf[n++] = ')';
    buf[n] = 0;
    core_stub_log(buf);
}

/* --- the portability layer: a null pointer the original would read through reads zeroes (DS:0 and the vector
 * table hold nothing the rules depend on in what we have measured); the enhancements are all off */
static unsigned char null_block[0x10000];
static unsigned char null_near[0x10000]; /* DS:0 onward, as UW.EXE's DGROUP starts (core_null_init) */
void *port_null_near(const char *file, int line) { (void)file; (void)line; return null_near; }
void *port_null_far(const char *file, int line) { (void)file; (void)line; return null_block; }
void *port_null_far_record(const char *layout, unsigned host_size, const char *file, int line)
{
    (void)layout; (void)host_size; (void)file; (void)line;
    return null_block;
}
uint32_t enhance_on;
void port_cheat_point(void) {}
/* the debug printer is DEBUG.C's own (empty in the shipped game) */
void port_far_copy(void *dst, const void *src, unsigned n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) *d++ = *s++; /* forwards a byte at a time, as movedata */
}

/* --- Borland's string conversions */
static char *conv(unsigned long v, int neg, char *buf, int radix)
{
    char tmp[40];
    int n = 0;
    do { int d = (int)(v % (unsigned)radix); tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= (unsigned)radix; } while (v);
    char *p = buf;
    if (neg) *p++ = '-';
    while (n) *p++ = tmp[--n];
    *p = 0;
    return buf;
}
char *itoa(int value, char *buf, int radix)
{
    int16_t v = (int16_t)value; /* Borland's int */
    if (radix == 10 && v < 0) return conv((unsigned long)-(long)v, 1, buf, radix);
    return conv(radix == 10 ? (unsigned long)v : (uint16_t)v, 0, buf, radix);
}
char *ltoa(long value, char *buf, int radix)
{
    int32_t v = (int32_t)value;
    if (radix == 10 && v < 0) return conv((unsigned long)-(int64_t)v, 1, buf, radix);
    return conv(radix == 10 ? (unsigned long)v : (uint32_t)v, 0, buf, radix);
}
char *ultoa(unsigned long value, char *buf, int radix) { return conv((uint32_t)value, 0, buf, radix); }
char *strupr(char *s) { for (char *p = s; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32; return s; }
char *strlwr(char *s) { for (char *p = s; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32; return s; }

/* --- Borland's file calls (compat.h renames the game's to bc_): DOS paths (backslashes, any case) under the game's
 * folder, core_game_dir, which is only ever read; anything opened for writing, and anything already written, is in
 * core_write_dir instead, so the original data is never changed. Text mode is not used by the rules (they open
 * everything "rb"/O_BINARY); the files are opened binary. */
#undef fopen
#undef fread
#undef fwrite
#undef fseek
#undef ftell
#undef fgetc
#undef fgets
#undef open
#undef read
#undef write
#undef close
#undef lseek
#undef access
#undef unlink
char core_game_dir[260] = ".", core_write_dir[260] = "save";
static void host_path(char *out, const char *dir, const char *path)
{
    int n = 0;
    for (const char *s = dir; *s && n < 250; s++) out[n++] = *s;
    out[n++] = '/';
    for (const char *s = path; *s && n < 500; s++) out[n++] = *s == 92 ? '/' : *s; /* a backslash */
    out[n] = 0;
}
static FILE *open_either(const char *path, const char *mode)
{
    char p[520];
    int writing = mode[0] != 'r' || mode[1] == '+' || (mode[1] && mode[2] == '+');
    host_path(p, core_write_dir, path);
    FILE *f = fopen(p, mode);
    if (f || writing) return f;
    host_path(p, core_game_dir, path);
    return fopen(p, mode);
}
FILE *bc_fopen(const char *path, const char *mode)
{
    char m[8];
    int k = 0;
    for (const char *s = mode; *s && k < 5; s++) if (*s != 't') m[k++] = *s;
    if (!strchr(m, 'b')) m[k++] = 'b';
    m[k] = 0;
    return open_either(path, m);
}
size_t bc_fread(void *buf, size_t size, size_t n, FILE *fp) { return fp ? fread(buf, size, n, fp) : 0; }
size_t bc_fwrite(const void *buf, size_t size, size_t n, FILE *fp) { return fp ? fwrite(buf, size, n, fp) : 0; }
int bc_fseek(FILE *fp, long off, int whence) { return fp ? fseek(fp, off, whence) : -1; }
long bc_ftell(FILE *fp) { return fp ? ftell(fp) : -1; }
int bc_fgetc(FILE *fp) { return fp ? fgetc(fp) : -1; }
char *bc_fgets(char *s, int n, FILE *fp) { return fp ? fgets(s, n, fp) : 0; }
/* handles: Borland's are small numbers from 5 (0-4 the standard devices) */
static FILE *handles[40];
int bc_open(const char *path, int acc, ...)
{
    const char *mode = (acc & 0x200) ? "w+b" : (acc & 0x100) ? "a+b" : (acc & 6) ? "r+b" : "rb";
    FILE *f = open_either(path, mode);
    if (!f && (acc & 0x100)) f = open_either(path, "w+b");
    if (!f) return -1;
    if (!strcmp(mode, "a+b")) fseek(f, 0, SEEK_SET);
    for (int i = 5; i < 40; i++) if (!handles[i]) { handles[i] = f; return i; }
    fclose(f);
    return -1;
}
int bc_creat(const char *path, int mode) { (void)mode; return bc_open(path, 0x8000 | 0x200 | 0x100 | 4); }
int bc_read(int fd, void *buf, unsigned n) { return fd > 0 && fd < 40 && handles[fd] ? (int)fread(buf, 1, n, handles[fd]) : -1; }
int bc_write(int fd, const void *buf, unsigned n) { return fd > 0 && fd < 40 && handles[fd] ? (int)fwrite(buf, 1, n, handles[fd]) : -1; }
int bc_close(int fd)
{
    if (fd <= 0 || fd >= 40 || !handles[fd]) return -1;
    fclose(handles[fd]);
    handles[fd] = 0;
    return 0;
}
long bc_lseek(int fd, long off, int whence)
{
    if (fd <= 0 || fd >= 40 || !handles[fd] || fseek(handles[fd], off, whence)) return -1;
    return ftell(handles[fd]);
}
int bc_access(const char *path, int mode)
{
    (void)mode;
    FILE *f = open_either(path, "rb");
    if (!f) return -1;
    fclose(f);
    return 0;
}

/* --- far pointers as segment and offset (compat.h's MK_FP, FP_SEG, FP_OFF): the rules core only splits a far
 * buffer's pointer to hand it to DOS (MISCUTIL.C's file reads and writes through intdosx), so a pointer becomes a
 * "segment" of its own here, numbered from 0x100, with offset 0, and MK_FP gives it back */
static void *segs[4096];
static int nsegs;
unsigned port_fp_seg(const volatile void *p)
{
    for (int i = nsegs - 1; i >= 0 && i >= nsegs - 64; i--) if (segs[i] == (void *)p) return 0x100 + i;
    if (nsegs == 4096) nsegs = 0;
    segs[nsegs] = (void *)p;
    return 0x100 + nsegs++;
}
unsigned port_fp_off(const volatile void *p) { (void)p; return 0; }
void *port_mk_fp(unsigned seg, unsigned off)
{
    if (seg >= 0x100 && seg < 0x100 + 4096u && segs[seg - 0x100]) return (unsigned char *)segs[seg - 0x100] + off;
    return null_block + (off & 0xFFFF);
}

/* --- DOS through intdosx: the file read (3Fh) and write (40h) the far-buffer loaders use */
#include <dos.h>
int intdosx(union REGS *in, union REGS *out, struct SREGS *s)
{
    unsigned char *buf = port_mk_fp(s->ds, in->x.dx);
    int r = -1;
    if (in->h.ah == 0x3F) r = bc_read(in->x.bx, buf, in->x.cx);
    else if (in->h.ah == 0x40) r = bc_write(in->x.bx, buf, in->x.cx);
    out->x.cflag = r < 0;
    out->x.ax = r < 0 ? 6 : (unsigned short)r; /* 6: an invalid handle */
    return out->x.ax;
}
int intdos(union REGS *in, union REGS *out) { struct SREGS s = {0}; return intdosx(in, out, &s); }
int int86(int intno, union REGS *in, union REGS *out) { (void)intno; *out = *in; return out->x.ax; }

/* --- Borland's far heap (alloc.h): the host's heap (the pointers are the core's own; nothing compares them) */
void *farmalloc(unsigned long n) { return malloc(n ? n : 1); }
void *farcalloc(unsigned long n, unsigned long size) { return calloc(n ? n : 1, size ? size : 1); }
void *farrealloc(void *p, unsigned long n) { return realloc(p, n ? n : 1); }
void farfree(void *p) { free(p); }
unsigned long farcoreleft(void) { return 0x60000; }
unsigned long coreleft(void) { return 0x8000; }

/* --- MODEX.ASM's far string and memory helpers (underworld-exhumed gfx/MODEX.ASM), as the instructions do them:
 * lengths and counts are 16-bit (repne scasb from CX = FFFFh) */
static uint16_t asm_len(const unsigned char *s) { uint16_t n = 0; while (s[n]) n++; return n; }
int str_len(char *s) { return (int16_t)asm_len((unsigned char *)s); }
/* the copy loop: CX bytes as words, then the odd byte */
static void asm_copy(unsigned char *d, const unsigned char *s, uint16_t cx) { for (uint16_t i = 0; i < cx; i++) d[i] = s[i]; }
char *str_copy(char *dst, char *src) { asm_copy((unsigned char *)dst, (unsigned char *)src, (uint16_t)(asm_len((unsigned char *)src) + 1)); return dst; }
void str_ncopy(char *dst, char *src, int n)
{
    int16_t cx = (int16_t)(asm_len((unsigned char *)src) + 1), dx = (int16_t)n; /* jle: a signed compare */
    if (cx > dx) cx = dx;
    asm_copy((unsigned char *)dst, (unsigned char *)src, (uint16_t)cx);
}
/* strchr over the string and its terminator (so c = 0 finds the end) */
char *FindStringDelimiter(char *s, int c)
{
    uint16_t cx = (uint16_t)(asm_len((unsigned char *)s) + 1);
    for (uint16_t i = 0; i < cx; i++) if ((unsigned char)s[i] == (unsigned char)c) return s + i;
    return 0;
}
/* strstr, except that an empty find is never found */
char *str_str(char *s, char *find)
{
    uint16_t n = asm_len((unsigned char *)s), d = asm_len((unsigned char *)find);
    if (!d) return 0;
    for (uint16_t i = 0; i + d <= n; i++) {
        uint16_t k = 0;
        while (k < d && s[i + k] == find[k]) k++;
        if (k == d) return s + i;
    }
    return 0;
}
/* compares up to the first string's terminator; the difference of the last bytes compared */
int str_cmp(char *a, char *b)
{
    const unsigned char *x = (unsigned char *)a, *y = (unsigned char *)b;
    uint16_t cx = (uint16_t)(asm_len(x) + 1), i = 0;
    while (i < cx) { i++; if (x[i - 1] != y[i - 1]) break; }
    return (int16_t)((int)x[i - 1] - (int)y[i - 1]);
}
/* UW1's: count / 2 words of the value, an odd count's last byte left as it was */
void mem_set(void *p, int value, int count)
{
    unsigned char *d = p;
    uint16_t words = (uint16_t)count >> 1;
    for (uint16_t i = 0; i < words; i++) d[2 * i] = d[2 * i + 1] = (unsigned char)value;
}
/* Borland's movedata (segment and offset each side), through the far-pointer map */
void movedata(unsigned srcseg, unsigned srcoff, unsigned dstseg, unsigned dstoff, unsigned n)
{
    port_far_copy(port_mk_fp(dstseg, dstoff), port_mk_fp(srcseg, srcoff), (uint16_t)n);
}

/* --- the mouse pointer as the rules read it (MOUSE.ASM's mouse_getxy: screen pixels, 320 x 200): the front end
 * sets core_mouse_x and core_mouse_y */
int16_t core_mouse_x, core_mouse_y;
void mouse_getxy(int16 *x, int16 *y) { *x = core_mouse_x; *y = core_mouse_y; }

/* --- the shared workspace (TMPALLOC.C's 64 KB scratch segment, which the loaders borrow): a host block, as a
 * "segment" of the far-pointer map */
static unsigned char workspace[0x10000] __attribute__((aligned(16)));
unsigned get_workspace(void) { return port_fp_seg(workspace); }
void release_workspace(void) {}
void set_workspace(void) {}

/* --- what a null far pointer reads: DOS has the interrupt vector table at 0000:0000, the emulator the proofs run
 * the original in (tools/emu.py) has UW.EXE's load image there; the core reads the same bytes as the emulator, so a
 * path through a null pointer (a critter whose target is 0) is the same on both sides */
void core_null_init(const unsigned char *exe, long size)
{
    long img = (long)(exe[8] | exe[9] << 8) * 16;
    for (long i = 0; i < (long)sizeof null_block; i++) null_block[i] = img + i < size ? exe[img + i] : 0;
    for (long i = 0; i < (long)sizeof null_near; i++) null_near[i] = img + 0x5AAC0L + i < size ? exe[img + 0x5AAC0L + i] : 0;
}

/* the mouse's buttons and input queue as the rules read them (MOUSE.ASM): the front end sets core_mouse_buttons
 * (bit 0 left, bit 1 right, as the driver gives them); there is no queued input (-1: the original's waits for a click
 * end at once, the front end having its own) */
int16_t core_mouse_buttons;
int mouse_getbut(int16 *b) { if (b) *b = core_mouse_buttons; return core_mouse_buttons; }
int mouse_get_input(void) { return -1; }

/* the end of a game (UWEDIT.C's real_death: the death screen and back to the title menu) is the front end's: the
 * rules before it (SKILLS.C's player_is_dead: the talismans, the experience lost, the bones, the silver tree) are the
 * original's. tools/core.py's PATCHES send real_death here when the front end has set it. */
void (*core_death_fn)(int how);

/* a drag (the button held while the pointer moves): the front end says whether this click is one (it picks up) */
unsigned char core_mouse_dragged;
unsigned char mouse_dragged(char how) { (void)how; return core_mouse_dragged; }
