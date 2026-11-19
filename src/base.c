#include "base.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the shared logger, for fatal_at's line in uwr.log: port\win32\rlog.h, found by -I port\win32 (the renderer DLLs,
   build.cmd's uw.exe, the Linux renderer) or one folder up (the SDL3 menu build's menu\ copy of this file). A build
   without it (build.cmd's check.exe and models_test.exe, the Laigter tool) links no rlog.c: fatal then only prints, as before */
#if __has_include("rlog.h")
#include "rlog.h"
#define BASE_HAS_RLOG 1
#elif __has_include("../rlog.h")
#include "../rlog.h"
#define BASE_HAS_RLOG 1
#else
#define BASE_HAS_RLOG 0
#endif

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

/* --- files kept in memory (base.h) --- */
typedef struct {
    char *key;   /* the path, lower case, '\\' for every separator */
    u8 *data;
    size_t n;
} MemFile;
static MemFile *g_mem;          /* open addressing, g_mem_cap a power of two */
static size_t g_mem_cap, g_mem_n, g_mem_bytes;
static char g_mem_root[1024];   /* normalised as the keys, ending in '\\'; empty: nothing kept in memory */

static void mem_key(const char *path, char *out, size_t n) {
    size_t i = 0;
    for (; path[i] && i + 1 < n; i++) {
        char c = path[i];
        out[i] = c == '/' ? '\\' : (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    out[i] = 0;
}
static size_t mem_hash(const char *k) {
    size_t h = 1469598103934665603ull;
    for (; *k; k++) h = (h ^ (u8)*k) * 1099511628211ull;
    return h;
}
static MemFile *mem_slot(const char *key) {   /* the file's slot, or the empty one it would take */
    size_t i = mem_hash(key) & (g_mem_cap - 1);
    while (g_mem[i].key && strcmp(g_mem[i].key, key)) i = (i + 1) & (g_mem_cap - 1);
    return &g_mem[i];
}
static const MemFile *mem_find(const char *path) {
    char key[1024];
    if (!g_mem_n) return NULL;
    mem_key(path, key, sizeof key);
    const MemFile *m = mem_slot(key);
    return m->key ? m : NULL;
}
static bool mem_put(const char *path, const void *data, size_t n) {
    char key[1024];
    mem_key(path, key, sizeof key);
    if ((g_mem_n + 1) * 2 > g_mem_cap) {   /* grow: at most half full */
        MemFile *old = g_mem;
        size_t oc = g_mem_cap;
        g_mem_cap = oc ? oc * 2 : 16384;
        g_mem = calloc(g_mem_cap, sizeof *g_mem);
        if (!g_mem) fatal("no memory for the files kept in memory");
        for (size_t i = 0; i < oc; i++)
            if (old[i].key) *mem_slot(old[i].key) = old[i];
        free(old);
    }
    MemFile *m = mem_slot(key);
    u8 *copy = malloc(n ? n : 1);
    if (!copy) return false;
    memcpy(copy, data, n);
    if (m->key) g_mem_bytes -= m->n, free(m->data);
    else {
        size_t kl = strlen(key) + 1;
        if (!(m->key = malloc(kl))) return free(copy), false;
        memcpy(m->key, key, kl);
        g_mem_n++;
    }
    m->data = copy, m->n = n, g_mem_bytes += n;
    return true;
}
void file_memory_root(const char *dir) {
    mem_key(dir ? dir : "", g_mem_root, sizeof g_mem_root - 1);
    size_t n = strlen(g_mem_root);
    while (n && g_mem_root[n - 1] == '\\') g_mem_root[--n] = 0;
    if (n) g_mem_root[n] = '\\', g_mem_root[n + 1] = 0;
}
bool file_in_memory_root(const char *path) {
    char key[1024];
    if (!g_mem_root[0]) return false;
    mem_key(path, key, sizeof key);
    return !strncmp(key, g_mem_root, strlen(g_mem_root));
}
int file_memory_count(size_t *bytes) {
    if (bytes) *bytes = g_mem_bytes;
    return (int)g_mem_n;
}

Bytes file_read(const char *path) {
    Bytes b = {0};
    char pb[1024];
    FILE *f = fopen(OS_PATH(path, pb), "rb");
    if (!f) {
        const MemFile *m = mem_find(path);   /* not on the disk: ours in memory */
        if (m && m->n) {
            u8 *p = arena_alloc(m->n + 1);
            memcpy(p, m->data, m->n);
            b.data = p, b.size = m->n;
        }
        return b;
    }
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
    if (file_in_memory_root(path)) return mem_put(path, data, n);
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
    return f != NULL || mem_find(path) != NULL;
}

void logf_(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
}

/* fatal(...) in base.h: the message on stdout as always ("fatal: <message>", which scripts may read), and, where
   the build has the logger, one error line in uwr.log naming the caller's file and line; then exit code 1 */
void fatal_at(const char *file, int line, const char *fmt, ...) {
    va_list ap;
#if BASE_HAS_RLOG
    char msg[768];
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    RLOG_E("base", "fatal at %s:%d: %s", file, line, msg);
#else
    (void)file, (void)line;
#endif
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

