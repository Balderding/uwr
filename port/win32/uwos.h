/* uwos.h: the operating system calls of the shared files that run threads of their own or load libraries (uwaudio.c,
   uwsteam.c, uwgame.c, uwlog.c, uwconv.c), for the Linux build (the Linux / SDL plan, step 5). Header only, so that
   the standalone tests (tests\uwaudio_test.c, uwsteam_test.c ...) build with the same file lists as before.

   On Windows it is windows.h itself: the files make exactly the calls they made before, in the same order. Elsewhere
   it gives the same small Win32 subset they use, made of POSIX: pthreads (threads, CRITICAL_SECTION as a recursive
   mutex, SRWLOCK as a rwlock, an auto-reset event and a semaphore as a mutex + condition), the __atomic builtins
   (Interlocked*), clock_gettime (GetTickCount64, QueryPerformanceCounter), nanosleep (Sleep), dlopen / dlsym
   (LoadLibraryExA, GetProcAddress) and stat (GetFileAttributesA). Not SDL: the tests build without it.

   The libraries' file names and the path separator (UWOS_LIB_*, UWOS_SEP) are the same string literals on Windows. */
#ifndef UWOS_H
#define UWOS_H

/* (UWOS_FORCE_POSIX: the POSIX side on Windows too, through MinGW's winpthreads: tests\uwos_test.c's way to run it
   here. Its libraries' part is left out then: MinGW has no dlfcn.h) */
#if defined(_WIN32) && !defined(UWOS_FORCE_POSIX)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define UWOS_SEP "\\"
#define UWOS_LIB_OPENAL "soft_oal.dll"     /* OpenAL Soft */
#define UWOS_LIB_PHONON "phonon.dll"       /* Steam Audio */
#define UWOS_LIB_MT32EMU "mt32emu.dll"     /* Munt */
#define uwos_lib_findable(path) (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)   /* (the file there) */
#else
#include <pthread.h>
#ifndef UWOS_FORCE_POSIX
#include <dlfcn.h>
#endif
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#define UWOS_SEP "/"
#define UWOS_LIB_OPENAL "libopenal.so.1"   /* Debian's libopenal1 (OpenAL Soft) */
#define UWOS_LIB_PHONON "libphonon.so"     /* the Steam Audio SDK's lib/linux-x64 */
#define UWOS_LIB_MT32EMU "libmt32emu.so.2" /* Munt's libmt32emu (not in Debian 13: built or shipped) */

#ifndef WINAPI   /* (MinGW's compiler has its own __cdecl and __stdcall: UWOS_FORCE_POSIX there) */
#define WINAPI
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFFu
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define LOAD_WITH_ALTERED_SEARCH_PATH 0x8
#define TRUE 1
#define FALSE 0
typedef int BOOL;
typedef unsigned long DWORD;
typedef long LONG;                         /* (64 bits here: only ever used through the Interlocked calls and as numbers) */
typedef long long LONG64;
typedef void *LPVOID;
typedef void *HMODULE;
typedef int (*FARPROC)(void);
typedef union { struct { uint32_t LowPart; int32_t HighPart; } u; long long QuadPart; } LARGE_INTEGER;

/* ---- time ---- */
static inline unsigned long long GetTickCount64(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000u + (unsigned long long)t.tv_nsec / 1000000u;
}
static inline DWORD GetTickCount(void) { return (DWORD)GetTickCount64(); }
static inline int QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000000LL; return 1; }
static inline int QueryPerformanceCounter(LARGE_INTEGER *c)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    c->QuadPart = (long long)t.tv_sec * 1000000000LL + t.tv_nsec;
    return 1;
}
static inline void Sleep(DWORD ms)
{
    struct timespec t = { (time_t)(ms / 1000u), (long)(ms % 1000u) * 1000000L };
    while (nanosleep(&t, &t) && errno == EINTR) {}
}

/* ---- atomics (a full barrier each, as the Interlocked calls) ---- */
static inline LONG InterlockedExchange(volatile LONG *p, LONG v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
static inline LONG InterlockedIncrement(volatile LONG *p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
static inline LONG InterlockedCompareExchange(volatile LONG *p, LONG v, LONG cmp)
{
    __atomic_compare_exchange_n(p, &cmp, v, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return cmp;   /* (the value it held: cmp when it was swapped) */
}
static inline LONG64 InterlockedOr64(volatile LONG64 *p, LONG64 v) { return __atomic_fetch_or(p, v, __ATOMIC_SEQ_CST); }

#ifndef UWOS_FORCE_POSIX
/* ---- libraries and files ---- */
/* the path given (the game's bin\ and the like), else the bare file name: the program's own folder first (its
   RUNPATH $ORIGIN), then the system's (Mint's libopenal1) */
static inline HMODULE LoadLibraryExA(const char *path, void *unused, DWORD flags)
{
    const char *base = path ? strrchr(path, '/') : NULL;
    HMODULE h = path ? dlopen(path, RTLD_NOW | RTLD_LOCAL) : NULL;
    (void)unused, (void)flags;
    if (!h && base) h = dlopen(base + 1, RTLD_NOW | RTLD_LOCAL);
    return h;
}
/* 1 when the library is beside the path given or found by its bare name (uwaudio.c's and uwsteam.c's look first) */
static inline int uwos_lib_findable(const char *path)
{
    struct stat st;
    const char *base = strrchr(path, '/');
    HMODULE h;
    if (stat(path, &st) == 0) return 1;
    if (!base || !(h = dlopen(base + 1, RTLD_LAZY | RTLD_LOCAL))) return 0;
    dlclose(h);
    return 1;
}
static inline HMODULE LoadLibraryA(const char *path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
static inline void *uwos_sym(HMODULE m, const char *name) { return m ? dlsym(m, name) : NULL; }
#define GetProcAddress(m, name) ((FARPROC)uwos_sym((HMODULE)(m), (name)))
static inline int FreeLibrary(HMODULE m) { return m && dlclose(m) == 0; }
static inline DWORD GetFileAttributesA(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (DWORD)(S_ISDIR(st.st_mode) ? 0x10 : 0x80) : INVALID_FILE_ATTRIBUTES;
}

#endif

/* ---- locks ---- */
typedef pthread_mutex_t CRITICAL_SECTION;   /* (recursive, as a critical section is) */
static inline void InitializeCriticalSection(CRITICAL_SECTION *c)
{
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(c, &a);
    pthread_mutexattr_destroy(&a);
}
static inline void EnterCriticalSection(CRITICAL_SECTION *c) { pthread_mutex_lock(c); }
static inline int TryEnterCriticalSection(CRITICAL_SECTION *c) { return pthread_mutex_trylock(c) == 0; }
static inline void LeaveCriticalSection(CRITICAL_SECTION *c) { pthread_mutex_unlock(c); }
static inline void DeleteCriticalSection(CRITICAL_SECTION *c) { pthread_mutex_destroy(c); }
typedef pthread_rwlock_t SRWLOCK;
#define SRWLOCK_INIT PTHREAD_RWLOCK_INITIALIZER
static inline void AcquireSRWLockExclusive(SRWLOCK *l) { pthread_rwlock_wrlock(l); }
static inline void ReleaseSRWLockExclusive(SRWLOCK *l) { pthread_rwlock_unlock(l); }
static inline void AcquireSRWLockShared(SRWLOCK *l) { pthread_rwlock_rdlock(l); }
static inline void ReleaseSRWLockShared(SRWLOCK *l) { pthread_rwlock_unlock(l); }

/* ---- handles: a thread, an auto-reset event or a semaphore, each waited for by WaitForSingleObject ---- */
enum { UWOS_THREAD = 1, UWOS_EVENT, UWOS_SEMA };
typedef struct uwos_handle {
    int kind, joined;
    pthread_t th;
    pthread_mutex_t m;
    pthread_cond_t c;
    long count, max;                       /* an event's signal (0 / 1), a semaphore's count; a thread's end (1) */
    clockid_t clock;                       /* the condition's clock: CLOCK_MONOTONIC where it can be (glibc), else the wall's */
    DWORD (*fn)(LPVOID);
    LPVOID arg;
} *HANDLE;
/* (the review, 2026-11-15: a wait's deadline on the condition's own clock, CLOCK_MONOTONIC where pthreads allows it, so that a change of the
   wall clock neither stretches nor cuts it; ReleaseSemaphore past the maximum fails and leaves the count, as Windows';
   CloseHandle of a thread that has ended joins and frees it, of one still running detaches it and leaves the handle to
   its thread to free at its end; the predicate is read, and an event or a semaphore taken, under the handle's mutex) */
static inline HANDLE uwos_handle_new(int kind, long count, long max)
{
    pthread_condattr_t ca;
    HANDLE h = (HANDLE)calloc(1, sizeof *h);
    if (!h) return NULL;
    h->kind = kind, h->count = count, h->max = max;
    pthread_mutex_init(&h->m, NULL);
    pthread_condattr_init(&ca);
    h->clock = pthread_condattr_setclock(&ca, CLOCK_MONOTONIC) == 0 ? CLOCK_MONOTONIC : CLOCK_REALTIME;   /* (winpthreads: EINVAL) */
    pthread_cond_init(&h->c, &ca);
    pthread_condattr_destroy(&ca);
    return h;
}
static inline void uwos_handle_free(HANDLE h)
{
    pthread_mutex_destroy(&h->m), pthread_cond_destroy(&h->c);
    free(h);
}
/* count + add (an event's at most 1); 0 when a semaphore's maximum would be passed (nothing changed then) */
static inline int uwos_signal(HANDLE h, long add, long *prev)
{
    int ok = 1;
    pthread_mutex_lock(&h->m);
    if (prev) *prev = h->count;
    if (h->kind == UWOS_SEMA && (add <= 0 || h->count + add > h->max)) ok = 0;
    else h->count = h->count + add > h->max ? h->max : h->count + add;
    if (ok) pthread_cond_broadcast(&h->c);
    pthread_mutex_unlock(&h->m);
    return ok;
}
static inline HANDLE CreateEventA(void *sa, int manual, int initial, const char *name)
{
    (void)sa, (void)manual, (void)name;   /* (auto-reset only: the files' every event is) */
    return uwos_handle_new(UWOS_EVENT, initial ? 1 : 0, 1);
}
static inline int SetEvent(HANDLE h) { return h ? uwos_signal(h, 1, NULL) : 0; }
static inline HANDLE CreateSemaphoreA(void *sa, long initial, long max, const char *name)
{
    (void)sa, (void)name;
    return uwos_handle_new(UWOS_SEMA, initial, max);
}
static inline int ReleaseSemaphore(HANDLE h, long n, long *prev) { return h ? uwos_signal(h, n, prev) : 0; }
static inline void *uwos_thread_main(void *p)
{
    HANDLE h = (HANDLE)p;
    int closed;
    h->fn(h->arg);
    pthread_mutex_lock(&h->m);
    h->count = 1;                          /* (the end: what WaitForSingleObject waits for) */
    closed = h->joined == 2;               /* (CloseHandle let it go while it ran: the handle is this thread's to free) */
    pthread_cond_broadcast(&h->c);
    pthread_mutex_unlock(&h->m);
    if (closed) uwos_handle_free(h);
    return NULL;
}
static inline HANDLE CreateThread(void *sa, size_t stack, DWORD (*fn)(LPVOID), LPVOID arg, DWORD flags, DWORD *id)
{
    pthread_attr_t a;
    HANDLE h = uwos_handle_new(UWOS_THREAD, 0, 1);
    (void)sa, (void)flags, (void)id;
    if (!h) return NULL;
    h->fn = fn, h->arg = arg;
    pthread_attr_init(&a);
    if (stack) pthread_attr_setstacksize(&a, stack < 65536 ? 65536 : stack);
    if (pthread_create(&h->th, &a, uwos_thread_main, h)) { pthread_attr_destroy(&a); uwos_handle_free(h); return NULL; }
    pthread_attr_destroy(&a);
    return h;
}
/* WAIT_OBJECT_0 once signalled (an event's signal or a semaphore's count is taken; a thread has ended: joined once,
   and a second wait on it is signalled at once), WAIT_TIMEOUT after ms (INFINITE: no limit) */
static inline DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
    struct timespec end;
    int timed_out = 0, join = 0;
    if (!h) return WAIT_TIMEOUT;
    clock_gettime(h->clock, &end);   /* (on the condition's own clock) */
    end.tv_sec += (time_t)(ms / 1000u), end.tv_nsec += (long)(ms % 1000u) * 1000000L;
    if (end.tv_nsec >= 1000000000L) end.tv_sec++, end.tv_nsec -= 1000000000L;
    pthread_mutex_lock(&h->m);
    while (h->count == 0 && !timed_out) {
        if (ms == INFINITE) pthread_cond_wait(&h->c, &h->m);
        else timed_out = pthread_cond_timedwait(&h->c, &h->m, &end) == ETIMEDOUT;
    }
    if (h->count == 0) { pthread_mutex_unlock(&h->m); return WAIT_TIMEOUT; }
    if (h->kind != UWOS_THREAD) h->count--;   /* (a thread's end stays signalled) */
    else if (!h->joined) h->joined = 1, join = 1;
    pthread_mutex_unlock(&h->m);
    if (join) pthread_join(h->th, NULL);
    return WAIT_OBJECT_0;
}
static inline int CloseHandle(HANDLE h)
{
    if (!h) return 0;
    if (h->kind == UWOS_THREAD) {
        pthread_t th = h->th;
        int joined, detach;
        pthread_mutex_lock(&h->m);
        joined = h->joined;
        detach = !joined && h->count == 0;   /* (still running: its thread frees the handle at its end) */
        if (detach) h->joined = 2;
        pthread_mutex_unlock(&h->m);
        if (detach) { pthread_detach(th); return 1; }   /* (h may be gone already: only th is used) */
        if (!joined) pthread_join(th, NULL);   /* (ended, never waited for: its last moments, then freed) */
    }
    uwos_handle_free(h);
    return 1;
}
#endif
#endif
