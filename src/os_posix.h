/* os_posix.h: what the renderer's sources (libuwview.so, tools\buildview_linux.py) take from Windows, given on Linux.
 * Force-included (-include os_posix.h) by the Linux build only; the Windows builds never see it.
 *
 * The paths: the sources build them as Windows writes them ("%s\\data", "DATA\\PALS.DAT"), and the folder's names are
 * lower case on Linux (makedist.py: every name lower-cased) while the game's own files may be in either case. fopen
 * goes through uwos_fopen, which turns '\' into '/' and finds each name of the path whatever its case (the name as
 * given when nothing matches, so a new file is made by that name). */
#ifndef OS_POSIX_H
#define OS_POSIX_H
#ifndef _WIN32

/* POSIX and the BSD names (clock_gettime, strcasecmp, readlink), not _GNU_SOURCE: with it glibc 2.38's headers send
   sscanf and strtol to their C23 forms (__isoc23_*, GLIBC_2.38), which Linux Mint 21 (glibc 2.35) lacks */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#define _stricmp strcasecmp
#define _strnicmp strncasecmp

/* the path as Linux finds it (above); out may be in */
void uwos_path(const char *in, char *out, size_t n);
FILE *uwos_fopen(const char *path, const char *mode);
#define fopen(p, m) uwos_fopen(p, m)
/* CreateDirectoryA's (the folder made when it is not there; its parents are not) */
int uwos_mkdir(const char *path);
#define CreateDirectoryA(p, sa) uwos_mkdir(p)

static inline unsigned long long uwos_ticks(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
}
#define GetTickCount64() uwos_ticks()

/* fmod and fmodf by their first version: glibc 2.38 gave them a new one, which a program built here would ask for */
__asm__(".symver fmod,fmod@GLIBC_2.2.5");
__asm__(".symver fmodf,fmodf@GLIBC_2.2.5");

/* the folder of the library (or program) holding addr: GetModuleFileName's, by dladdr; 0 if not found */
int uwos_self_dir(const void *addr, char *out, size_t n);

/* FindFirstFileA / FindNextFileA / FindClose for extract.c's two listings: "DIR\\*" and "DIR\\*.EXT" (the extension's
   case not minded), files only; cFileName is the name only, as Windows gives it */
typedef void *HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)0)
typedef struct { char cFileName[260]; } WIN32_FIND_DATAA;
HANDLE uwos_find_first(const char *pattern, WIN32_FIND_DATAA *fd);
int uwos_find_next(HANDLE h, WIN32_FIND_DATAA *fd);
void uwos_find_close(HANDLE h);
#define FindFirstFileA(p, fd) uwos_find_first(p, fd)
#define FindNextFileA(h, fd) uwos_find_next(h, fd)
#define FindClose(h) uwos_find_close(h)

#endif
#endif
