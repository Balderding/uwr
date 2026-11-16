/* uwos_test.c: port\win32\uwos.h's Win32 subset made of POSIX (the Linux build's), checked where it can run: on Linux,
   and on Windows with -DUWOS_FORCE_POSIX (MinGW's winpthreads under the same code). Built on Windows without it, the
   same checks run against Windows itself.
     clang ... -DUWOS_FORCE_POSIX -Iport\win32 -o uwos_test.exe tests\uwos_test.c       (the POSIX side, here)
   Exit code: the failures. */
#include <stdio.h>
#include "../port/win32/uwos.h"

static int fails;
static void check(const char *what, int ok)
{
    printf("%s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) fails++;
}

static HANDLE ev;
static volatile LONG flag;
static DWORD WINAPI set_later(LPVOID p)
{
    Sleep((DWORD)(size_t)p);
    InterlockedExchange(&flag, 1);
    SetEvent(ev);
    return 0;
}
static DWORD WINAPI just_sleep(LPVOID p)
{
    Sleep((DWORD)(size_t)p);
    InterlockedIncrement(&flag);
    return 0;
}

int main(void)
{
    unsigned long long t0, dt;
    HANDLE th, sem;
    CRITICAL_SECTION cs;
    LONG v = 5, old;
#ifdef UWOS_FORCE_POSIX
    printf("uwos_test: the POSIX side (UWOS_FORCE_POSIX)\n");
#elif defined(_WIN32)
    printf("uwos_test: Windows itself\n");
#else
    printf("uwos_test: the POSIX side\n");
#endif
    /* an event signalled from a second thread */
    ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    t0 = GetTickCount64();
    th = CreateThread(NULL, 0, set_later, (LPVOID)(size_t)50, 0, NULL);
    check("event: set from a second thread, waited for", WaitForSingleObject(ev, 2000) == WAIT_OBJECT_0 && flag == 1);
    dt = GetTickCount64() - t0;
    check("event: the wait ended when it was set (40..1000 ms)", dt >= 40 && dt < 1000);
    /* the thread joined, and waited for again */
    check("thread: waited for (ended)", WaitForSingleObject(th, 2000) == WAIT_OBJECT_0);
    check("thread: a second wait signalled at once", WaitForSingleObject(th, 0) == WAIT_OBJECT_0);
    CloseHandle(th);
    /* a timed-out wait; auto-reset */
    t0 = GetTickCount64();
    check("event: unset, the wait times out", WaitForSingleObject(ev, 100) == WAIT_TIMEOUT);
    dt = GetTickCount64() - t0;
    check("event: the time-out took about 100 ms (90..600)", dt >= 90 && dt < 600);
    SetEvent(ev), SetEvent(ev);
    check("event: set twice, one wait taken", WaitForSingleObject(ev, 0) == WAIT_OBJECT_0);
    check("event: auto-reset (the second wait times out)", WaitForSingleObject(ev, 10) == WAIT_TIMEOUT);
    CloseHandle(ev);
    /* a semaphore counted up and down */
    sem = CreateSemaphoreA(NULL, 0, 3, NULL);
    check("semaphore: released by 2", ReleaseSemaphore(sem, 2, NULL) != 0);
    check("semaphore: past its maximum (2 + 2 > 3) refused", ReleaseSemaphore(sem, 2, NULL) == 0);
    check("semaphore: taken twice", WaitForSingleObject(sem, 0) == WAIT_OBJECT_0 && WaitForSingleObject(sem, 0) == WAIT_OBJECT_0);
    check("semaphore: empty, the third times out", WaitForSingleObject(sem, 20) == WAIT_TIMEOUT);
    CloseHandle(sem);
    /* a thread closed while it runs (it ends by itself); one that ended, never waited for */
    flag = 0;
    th = CreateThread(NULL, 1 << 20, just_sleep, (LPVOID)(size_t)100, 0, NULL);
    check("thread: closed while it runs", th && CloseHandle(th));
    th = CreateThread(NULL, 0, just_sleep, (LPVOID)(size_t)0, 0, NULL);
    Sleep(250);
    check("thread: both ran to their end", flag == 2);
    check("thread: ended, never waited for, closed", CloseHandle(th) != 0);
    /* the atomics and the critical section */
    old = InterlockedCompareExchange(&v, 7, 5);
    check("InterlockedCompareExchange: swapped, the old value", old == 5 && v == 7);
    old = InterlockedCompareExchange(&v, 9, 5);
    check("InterlockedCompareExchange: not swapped, the value it held", old == 7 && v == 7);
    check("InterlockedExchange / Increment", InterlockedExchange(&v, 1) == 7 && InterlockedIncrement(&v) == 2);
    InitializeCriticalSection(&cs);
    EnterCriticalSection(&cs), EnterCriticalSection(&cs);   /* (recursive, as Windows') */
    LeaveCriticalSection(&cs), LeaveCriticalSection(&cs);
    check("critical section: entered twice by one thread", 1);
    /* the clocks */
    {
        LARGE_INTEGER f, a, b;
        QueryPerformanceFrequency(&f), QueryPerformanceCounter(&a);
        Sleep(30);
        QueryPerformanceCounter(&b);
        dt = (unsigned long long)((b.QuadPart - a.QuadPart) * 1000 / f.QuadPart);
        check("QueryPerformanceCounter over Sleep(30): 25..500 ms", dt >= 25 && dt < 500);
    }
    printf("uwos_test: %d failure(s)\n", fails);
    return fails;
}
