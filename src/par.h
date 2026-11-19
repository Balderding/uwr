/* par.h (2026-11-18): par_for(n, fn, ctx) calls fn(ctx, i) for every i from 0 to n-1 on several threads at once (this
 * one works too), and returns when all are done. Two processors are left to the rest of the game and its sound, as
 * png.c's prefetch leaves them; at most PAR_MAX threads. Each i must touch only memory of its own: never the arena,
 * png_read or any table that grows. Windows threads or pthreads. Header only (static), for level.c and render_vk.c. */
#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#define PAR_MAX 16
typedef void (*ParFn)(void *ctx, int i);
typedef struct {
    ParFn fn;
    void *ctx;
    int n;
#ifdef _WIN32
    volatile LONG next;
#else
    int next;
#endif
} ParJob;

static inline void par_work1(ParJob *j) {
    for (;;) {
#ifdef _WIN32
        int i = (int)InterlockedIncrement(&j->next) - 1;
#else
        int i = __atomic_fetch_add(&j->next, 1, __ATOMIC_SEQ_CST);
#endif
        if (i >= j->n) return;
        j->fn(j->ctx, i);
    }
}
#ifdef _WIN32
static DWORD WINAPI par_thread(LPVOID a) { par_work1((ParJob *)a); return 0; }
#else
static void *par_thread(void *a) { par_work1((ParJob *)a); return NULL; }
#endif

static inline int par_cpus(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long c = sysconf(_SC_NPROCESSORS_ONLN);
    return c > 0 ? (int)c : 1;
#endif
}

static inline void par_for(int n, ParFn fn, void *ctx) {
    ParJob j;
    j.fn = fn, j.ctx = ctx, j.n = n, j.next = 0;
    int threads = par_cpus() - 2;
    if (threads > PAR_MAX) threads = PAR_MAX;
    if (threads > n) threads = n;
    threads--;   /* (this thread is one of them) */
    int made = 0;
#ifdef _WIN32
    HANDLE th[PAR_MAX];
    for (int i = 0; i < threads; i++)
        if ((th[made] = CreateThread(NULL, 0, par_thread, &j, 0, NULL)) != NULL) made++;
    par_work1(&j);
    if (made) WaitForMultipleObjects((DWORD)made, th, TRUE, INFINITE);
    for (int i = 0; i < made; i++) CloseHandle(th[i]);
#else
    pthread_t th[PAR_MAX];
    for (int i = 0; i < threads; i++)
        if (!pthread_create(&th[made], NULL, par_thread, &j)) made++;
    par_work1(&j);
    for (int i = 0; i < made; i++) pthread_join(th[i], NULL);
#endif
}
