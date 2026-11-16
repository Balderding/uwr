/* uwlog.h: the game's message scroll as text (uwlog.c) */
#ifndef UWLOG_H
#define UWLOG_H
#define UWLOG_W 96
#define UWLOG_ROWS 9   /* the lines our message box shows (2026-11-14, the user: a few more; it was 5) */
void port_scroll_note(const char *s);   /* the game's thread: a scroll_print's text */
/* up to max lines ending back lines before the newest, oldest first; the newest text's time (GetTickCount64) and the
   lines held. Returns the lines given. */
int uwlog_get(char (*out)[UWLOG_W], int max, int back, unsigned long long *newest_ms, int *all);
/* an instrument is being played (SOUND.C play_instrument's loop, which only Esc ends): from its messages */
int uwlog_instrument(void);
void uwlog_instrument_clear(void);
#endif
