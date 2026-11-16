/* os_posix.c: os_posix.h's path finding, for the Linux build of the renderer (libuwview.so) only */
#ifndef _WIN32
#include "os_posix.h"
#undef fopen
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <dlfcn.h>

/* dladdr and its Dl_info as glibc has them (dlfcn.h gives them only with _GNU_SOURCE, which os_posix.h leaves off) */
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } UwosDlInfo;
extern int dladdr(const void *addr, UwosDlInfo *info);

int uwos_self_dir(const void *addr, char *out, size_t n)
{
    UwosDlInfo di;
    if (!dladdr(addr, &di) || !di.dli_fname || !di.dli_fname[0]) return 0;
    snprintf(out, n, "%s", di.dli_fname);
    char *cut = strrchr(out, '/');
    if (cut) *cut = 0;
    else snprintf(out, n, ".");
    return 1;
}

/* name in folder dir whatever its case: written into out (the name as given when none matches) */
static void find_name(const char *dir, const char *name, char *out, size_t n)
{
    snprintf(out, n, "%s", name);
    char p[MAX_PATH];
    struct stat st;
    snprintf(p, sizeof p, "%s/%s", *dir ? dir : ".", name);
    if (!stat(p, &st)) return;   /* there as given */
    DIR *d = opendir(*dir ? dir : ".");
    if (!d) return;
    for (struct dirent *e; (e = readdir(d));)
        if (!strcasecmp(e->d_name, name)) {
            snprintf(out, n, "%s", e->d_name);
            break;
        }
    closedir(d);
}

void uwos_path(const char *in, char *out, size_t n)
{
    char src[MAX_PATH], res[MAX_PATH] = "", part[256];
    snprintf(src, sizeof src, "%s", in ? in : "");
    for (char *c = src; *c; c++)
        if (*c == '\\') *c = '/';
    const char *s = src;
    size_t len = 0;
    if (*s == '/') res[0] = '/', res[1] = 0, len = 1;
    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;
        size_t k = 0;
        while (s[k] && s[k] != '/') k++;
        char name[256];
        snprintf(name, sizeof name, "%.*s", (int)k, s);
        s += k;
        if (!strcmp(name, ".") || !strcmp(name, "..")) snprintf(part, sizeof part, "%s", name);
        else {
            char dir[MAX_PATH];
            snprintf(dir, sizeof dir, "%.*s", (int)(len > 1 && res[len - 1] == '/' ? len - 1 : len), res);
            find_name(dir, name, part, sizeof part);
        }
        len += (size_t)snprintf(res + len, sizeof res - len, "%s%s", len && res[len - 1] != '/' ? "/" : "", part);
        if (len >= sizeof res) break;
    }
    snprintf(out, n, "%s", res);
}

FILE *uwos_fopen(const char *path, const char *mode)
{
    char p[MAX_PATH];
    uwos_path(path, p, sizeof p);
    return fopen(p, mode);
}

int uwos_mkdir(const char *path)
{
    char p[MAX_PATH];
    uwos_path(path, p, sizeof p);
    return mkdir(p, 0755) == 0 || errno == EEXIST;
}

typedef struct { DIR *d; char dir[MAX_PATH], ext[32]; } Find;
static int find_step(Find *f, WIN32_FIND_DATAA *fd)
{
    for (struct dirent *e; (e = readdir(f->d));) {
        if (e->d_name[0] == '.') continue;
        if (f->ext[0]) {
            size_t l = strlen(e->d_name), k = strlen(f->ext);
            if (l < k || strcasecmp(e->d_name + l - k, f->ext)) continue;
        }
        char p[MAX_PATH + 300];
        struct stat st;
        snprintf(p, sizeof p, "%s/%s", f->dir, e->d_name);
        if (stat(p, &st) || !S_ISREG(st.st_mode)) continue;
        snprintf(fd->cFileName, sizeof fd->cFileName, "%s", e->d_name);
        return 1;
    }
    return 0;
}
HANDLE uwos_find_first(const char *pattern, WIN32_FIND_DATAA *fd)
{
    char p[MAX_PATH];
    snprintf(p, sizeof p, "%s", pattern);
    for (char *c = p; *c; c++) if (*c == '\\') *c = '/';
    char *slash = strrchr(p, '/');
    const char *mask = slash ? slash + 1 : p;
    if (mask[0] != '*') return INVALID_HANDLE_VALUE;   /* (only the two listings above) */
    Find *f = calloc(1, sizeof *f);
    if (!f) return INVALID_HANDLE_VALUE;
    snprintf(f->ext, sizeof f->ext, "%s", mask[1] == '.' ? mask + 1 : "");
    if (slash) *slash = 0;
    uwos_path(slash ? p : ".", f->dir, sizeof f->dir);
    if (!(f->d = opendir(f->dir))) return free(f), INVALID_HANDLE_VALUE;
    if (!find_step(f, fd)) return closedir(f->d), free(f), INVALID_HANDLE_VALUE;
    return f;
}
int uwos_find_next(HANDLE h, WIN32_FIND_DATAA *fd) { return h && find_step((Find *)h, fd); }
void uwos_find_close(HANDLE h)
{
    if (!h) return;
    closedir(((Find *)h)->d);
    free(h);
}
#endif
