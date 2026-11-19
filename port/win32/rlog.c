/* rlog.c: the shared logger (see rlog.h). Standard C only, so it builds for Windows and Linux alike.
 * rlog_init_level  reads env UWR_LOGLEVEL once, on the first use.
 * rlog_set_level / rlog_level  set and read the level.
 * rlog_errno       strerror(errno) as text.
 * rlog             formats the whole line into one buffer and writes it with ONE fputs + fflush, so lines from the
 *                  game thread and the render thread never mix mid-line. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L   /* localtime_r, strerror_r */
#endif
#include "rlog.h"

#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static _Atomic int g_level = -1;         /* -1: not read from the environment yet */

static void rlog_init_level(void)
{
    const char *e = getenv("UWR_LOGLEVEL");
    int lv = RLOG_INFO, none = -1;
    if (e && e[0] >= '0' && e[0] <= '3' && !e[1]) lv = e[0] - '0';
    else if (e && !strcmp(e, "error")) lv = RLOG_ERROR;
    else if (e && !strcmp(e, "warn")) lv = RLOG_WARN;
    else if (e && !strcmp(e, "info")) lv = RLOG_INFO;
    else if (e && !strcmp(e, "debug")) lv = RLOG_DEBUG;
    /* set once: if another thread (or rlog_set_level) got there first, keep its value */
    atomic_compare_exchange_strong(&g_level, &none, lv);
}

void rlog_set_level(int level)
{
    g_level = level < RLOG_ERROR ? RLOG_ERROR : level > RLOG_DEBUG ? RLOG_DEBUG : level;
}

int rlog_level(void)
{
    if (atomic_load(&g_level) < 0) rlog_init_level();
    return atomic_load(&g_level);
}

const char *rlog_errno(void)
{
    static _Thread_local char text[128];   /* one per thread, so the returned text stays valid */
    int e = errno;
#ifdef _WIN32
    if (strerror_s(text, sizeof text, e) != 0) snprintf(text, sizeof text, "error %d", e);
#elif defined(__GLIBC__) && defined(_GNU_SOURCE)
    {   /* glibc's GNU form returns the text (it may not write into the buffer) */
        const char *m = strerror_r(e, text, sizeof text);
        if (m != text) snprintf(text, sizeof text, "%s", m);
    }
#else
    if (strerror_r(e, text, sizeof text) != 0) snprintf(text, sizeof text, "error %d", e);   /* the POSIX form (_POSIX_C_SOURCE 200809L) */
#endif
    return text;
}

void rlog(int level, const char *tag, const char *fmt, ...)
{
    static const char *const names[] = { "error", "warn ", "info ", "debug" };
    char buf[1024];
    struct timespec ts;
    struct tm tmv;
    time_t sec;
    int n, cap = (int)sizeof buf - 2;    /* room for the final newline and the NUL */
    va_list ap;

    if (level > rlog_level()) return;
    if (level < RLOG_ERROR) level = RLOG_ERROR;
    if (level > RLOG_DEBUG) level = RLOG_DEBUG;
    if (!timespec_get(&ts, TIME_UTC)) { ts.tv_sec = time(NULL); ts.tv_nsec = 0; }
    sec = (time_t)ts.tv_sec;
#ifdef _WIN32
    if (localtime_s(&tmv, &sec) != 0) memset(&tmv, 0, sizeof tmv);
#else
    if (!localtime_r(&sec, &tmv)) memset(&tmv, 0, sizeof tmv);
#endif
    n = snprintf(buf, (size_t)cap, "[%02d:%02d:%02d.%03d] %s %s: ", tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
                 (int)(ts.tv_nsec / 1000000), names[level], tag ? tag : "?");
    if (n < 0) n = 0;
    if (n < cap) {
        int m;
        va_start(ap, fmt);
        m = vsnprintf(buf + n, (size_t)(cap - n), fmt, ap);
        va_end(ap);
        if (m < 0) m = 0;
        if (m >= cap - n) {              /* cut: end with "..." */
            n = cap - 1;
            memcpy(buf + n - 3, "...", 3);
        } else n += m;
    } else n = cap - 1;
    if (n > 0 && buf[n - 1] == '\n') n--;   /* a message that brings its own newline gets no second one */
    buf[n++] = '\n';
    buf[n] = 0;
    fputs(buf, stderr);
    fflush(stderr);
}
