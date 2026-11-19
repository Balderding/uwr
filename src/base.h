/* Engine basics: fixed-size types, one memory arena, files, a log.
 *
 * Plain old data throughout: state is fixed-size structs and arrays. What comes from disk is
 * loaded into the arena, which is reset only between whole loads (a map, a check pass), never
 * while a frame is running. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static inline u32 rd16(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8; }
static inline i32 rd16s(const u8 *p) { return (i16)rd16(p); }
static inline u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline i32 rd32s(const u8 *p) { return (i32)rd32(p); }

/* --- the arena ---------------------------------------------------------------------------- */
void arena_init(size_t bytes);              /* once, at start (the one heap block the engine holds) */
void *arena_alloc(size_t n);                /* zeroed, 16-byte aligned; aborts when full */
size_t arena_mark(void);
void arena_reset(size_t mark);              /* frees everything allocated after the mark */
size_t arena_used(void);

/* --- files ---------------------------------------------------------------------------------- */
typedef struct {
    const u8 *data;
    size_t size;
} Bytes;
Bytes file_read(const char *path);          /* the whole file into the arena; size 0 when missing */
bool file_write(const char *path, const void *data, size_t n);
/* the checks: when set, file_write hands the bytes to this instead of writing */
extern bool (*g_write_hook)(const char *path, const void *data, size_t n);
bool file_exists(const char *path);
/* Files kept in memory (the user, 2026-11-16: the game's own files are not extracted onto the disk). Once a root is
 * named, file_write under it keeps the bytes in memory and writes nothing; file_read and file_exists look on the disk
 * first (a file there is someone's own and wins), then in memory; make_dirs_for makes no folder under it. */
void file_memory_root(const char *dir);
bool file_in_memory_root(const char *path);
int file_memory_count(size_t *bytes);

/* --- text ----------------------------------------------------------------------------------- */
void logf_(const char *fmt, ...);           /* to stdout, unbuffered */
/* prints "fatal: <message>" on stdout; where the build links rlog.c, also logs "fatal at <file>:<line>: <message>" (error); exits with code 1 */
void fatal_at(const char *file, int line, const char *fmt, ...);
#define fatal(...) fatal_at(__FILE__, __LINE__, __VA_ARGS__)
bool str_ieq(const char *a, const char *b); /* ASCII, case-insensitive */
#define str_copy uwb_str_copy   /* (apart from the game's own str_copy, MODEX.ASM's: our menu is built into the port) */
void str_copy(char *dst, size_t n, const char *src);

/* --- FNV-1a, for the checks against the Python reference ------------------------------------ */
typedef struct {
    u32 h;
} Fnv;
static inline void fnv_init(Fnv *f) { f->h = 2166136261u; }
static inline void fnv_bytes(Fnv *f, const void *p, size_t n) {
    const u8 *b = p;
    for (size_t i = 0; i < n; i++) f->h = (f->h ^ b[i]) * 16777619u;
}
static inline void fnv_i32(Fnv *f, i32 v) {
    u8 b[4] = {(u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24)};
    fnv_bytes(f, b, 4);
}

/* --- paths (png.c) -------------------------------------------------------------------------- */
/* The uo-new folder: the exe's folder, or its parent when the exe is in build\ (the one holding game\). */
const char *root_dir(void);
void path_join(char *out, size_t n, const char *a, const char *b);   /* a\b */
void make_dirs_for(const char *file_path);   /* creates every folder above the file */

/* --- PNG (png.c) ---------------------------------------------------------------------------- */
/* Written with stored (uncompressed) deflate blocks, byte-identical to tools\loose.py.
 * channels 3 RGB, 4 RGBA; png_write_indexed: 8-bit indices with a 256-colour palette (RGB). */
bool png_write(const char *path, const u8 *px, int w, int h, int channels);
bool png_write_indexed(const char *path, const u8 *idx, int w, int h, const u8 pal[768]);
/* Any 8-bit PNG (grey, grey+alpha, RGB, RGBA, indexed; not interlaced), compressed or not, into the
 * arena: rgba = w*h*4; idx = the palette indices when the file is indexed, else NULL. */
typedef struct {
    int w, h;
    u8 *rgba;
    u8 *idx;
} Image;
bool png_read(const char *path, Image *out);
bool png_exists(const char *path);   /* file_exists, the disk asked once for a picture png_read keeps (the relief maps) */
bool png_view(const char *path, Image *out, bool *copied);   /* (2026-11-18) png_read's answer, the pixels lent (png.c) */
/* (2026-11-17) the pictures a build is about to ask for: those png_read keeps and has not yet are read and unpacked
 * at once by several threads; png_exists and png_read answer the same as without it, sooner */
void png_prefetch(const char *const *paths, int n);
