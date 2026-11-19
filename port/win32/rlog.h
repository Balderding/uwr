/* rlog.h: the one logger of the uwr front end (exe and renderer DLLs). Every line goes to stderr (uwr.log) as
 *   [HH:MM:SS.mmm] warn  tag: text
 * rlog          writes one line (printf format) at a level, under a short tag naming the part of the game.
 * rlog_set_level  lines above this level are dropped; the start value comes from env UWR_LOGLEVEL
 *                 (error|warn|info|debug or 0-3), else info.
 * rlog_level    the level now in force.
 * rlog_errno    the text of errno (strerror), for "cannot open %s: %s".
 * RLOG_E/W/I/D  shorthands for the four levels. */
#ifndef RLOG_H
#define RLOG_H

enum { RLOG_ERROR = 0, RLOG_WARN = 1, RLOG_INFO = 2, RLOG_DEBUG = 3 };

void rlog(int level, const char *tag, const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;
void rlog_set_level(int level);
int  rlog_level(void);
const char *rlog_errno(void);

#define RLOG_E(tag, ...) rlog(RLOG_ERROR, tag, __VA_ARGS__)
#define RLOG_W(tag, ...) rlog(RLOG_WARN,  tag, __VA_ARGS__)
#define RLOG_I(tag, ...) rlog(RLOG_INFO,  tag, __VA_ARGS__)
#define RLOG_D(tag, ...) rlog(RLOG_DEBUG, tag, __VA_ARGS__)

#endif
