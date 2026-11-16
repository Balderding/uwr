#include "base.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the path as the system takes it: Windows as given; elsewhere (the Linux build's menu) its backslashes as '/' */
#ifdef _WIN32
#define OS_PATH(p, buf) ((void)(buf), (p))
#else
static const char *os_path(const char *p, char *buf, size_t n) {
    size_t i = 0;
    for (; p[i] && i + 1 < n; i++) buf[i] = p[i] == '\\' ? '/' : p[i];
    buf[i] = 0;
    return buf;
}
#define OS_PATH(p, buf) os_path((p), (buf), sizeof (buf))
#endif

static u8 *g_arena;
static size_t g_arena_cap, g_arena_used;

void arena_init(size_t bytes) {
    g_arena = malloc(bytes);
    if (!g_arena) fatal("cannot reserve the %zu MB arena", bytes >> 20);
    g_arena_cap = bytes;
    g_arena_used = 0;
}

void *arena_alloc(size_t n) {
    size_t at = (g_arena_used + 15) & ~(size_t)15;
    if (at + n > g_arena_cap) fatal("arena full (%zu MB used, %zu more wanted)", g_arena_used >> 20, n);
    g_arena_used = at + n;
    memset(g_arena + at, 0, n);
    return g_arena + at;
}

size_t arena_mark(void) { return g_arena_used; }
void arena_reset(size_t mark) { g_arena_used = mark; }
size_t arena_used(void) { return g_arena_used; }

Bytes file_read(const char *path) {
    Bytes b = {0};
    char pb[1024];
    FILE *f = fopen(OS_PATH(path, pb), "rb");
    if (!f) return b;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0) {
        u8 *p = arena_alloc((size_t)n + 1);
        if (fread(p, 1, (size_t)n, f) == (size_t)n) {
            b.data = p;
            b.size = (size_t)n;
        }
    }
    fclose(f);
    return b;
}

bool (*g_write_hook)(const char *path, const void *data, size_t n);

bool file_write(const char *path, const void *data, size_t n) {
    if (g_write_hook) return g_write_hook(path, data, n);
    char pb[1024];
    FILE *f = fopen(OS_PATH(path, pb), "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

bool file_exists(const char *path) {
    char pb[1024];
    FILE *f = fopen(OS_PATH(path, pb), "rb");
    if (f) fclose(f);
    return f != NULL;
}

void logf_(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
}

void fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("fatal: ", stdout);
    vprintf(fmt, ap);
    fputs("\n", stdout);
    va_end(ap);
    fflush(stdout);
    exit(1);
}

bool str_ieq(const char *a, const char *b) {
    for (;; a++, b++) {
        int x = (u8)*a, y = (u8)*b;
        if (x >= 'a' && x <= 'z') x -= 32;
        if (y >= 'a' && y <= 'z') y -= 32;
        if (x != y) return false;
        if (!x) return true;
    }
}

void str_copy(char *dst, size_t n, const char *src) {
    if (!n) return;
    size_t i = 0;
    for (; i + 1 < n && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

