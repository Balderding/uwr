/* uwrec.h: the window's pictures to an MP4 (uwrec.c) */
#pragma once
#include <stdbool.h>
bool uwrec_start(const char *dir, int w, int h, int seconds);
bool uwrec_active(void);
const char *uwrec_path(void);
int uwrec_seconds_left(void);
void uwrec_frame(bool (*readback)(unsigned char *rgba));
void uwrec_shutdown(void);
bool uwrec_clipboard(void *hwnd, int w, int h, bool (*readback)(unsigned char *rgba));
