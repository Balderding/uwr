/* xmidi.h: UW's Roland MT-32 sound driver (SOUND\MT32MPU.ADV: Miles AIL 2 XMIDI player + MT-32 interpreter + MPU-401
 * interface), ported routine by routine. It drives a Roland through bytes written to the MPU-401's ports: out(0x330,
 * b) MIDI data, out(0x331, b) an interface command; retrace() is one of the driver's VGA-retrace delays (sysex_wait).
 * tools/adv_music.py records the original driver doing the same; tests/xmidi_test.c must give identical records. */
#pragma once
#include "base.h"

typedef struct {
    void (*out)(void *user, int port, int value); /* a byte to the MPU-401 (0x330 data, 0x331 command) */
    void (*retrace)(void *user);                  /* one vertical-retrace wait */
    void *user;
} XmidiIO;

#define XMIDI_RATE 120      /* the driver is served 120 times a second (QUANT_RATE) */
#define XMIDI_SEQS 8        /* sequence handles (NSEQS) */
#define XMIDI_STATE_SIZE 1  /* (the state tables are the driver's own here: register_seq needs no buffer) */

/* the AIL functions (the driver's numbers in comments); a sequence handle is 0, 4, 8 ... as the original's */
void xmidi_init(const XmidiIO *io);                                    /* 102 init_driver (IO 0x330) */
void xmidi_serve(void);                                                /* 103 serve_driver, 120 times a second */
void xmidi_shutdown(const char *signoff);                              /* 104 */
int xmidi_register(const u8 *xmi, size_t size, int seqnum, const u8 *ctrl); /* 151: handle, or -1 */
void xmidi_release(int seq);                                           /* 152 */
int xmidi_timbre_request(int seq);                                     /* 155: bank << 8 | patch, or -1 (0xffff) */
void xmidi_install_timbre(int bank, int patch, const u8 *timbre);      /* 156 (the timbre with its length word) */
void xmidi_protect_timbre(int bank, int patch);                        /* 157 */
void xmidi_unprotect_timbre(int bank, int patch);                      /* 158 */
int xmidi_timbre_status(int bank, int patch);                          /* 159 */
void xmidi_start(int seq);                                             /* 170 */
void xmidi_stop(int seq);                                              /* 171 */
void xmidi_resume(int seq);                                            /* 173 */
int xmidi_status(int seq);                                             /* 174: 0 stopped, 1 playing, 2 done */
int xmidi_rel_volume(int seq);                                         /* 175 */
int xmidi_rel_tempo(int seq);                                          /* 176 */
void xmidi_set_rel_volume(int seq, int percent, int ms);               /* 177 */
void xmidi_set_rel_tempo(int seq, int percent, int ms);                /* 178 */
int xmidi_beat_count(int seq);                                         /* 179 */
int xmidi_bar_count(int seq);                                          /* 180 */
void xmidi_branch_index(int seq, int marker);                          /* 181 */
int xmidi_control_value(int seq, int chan, int control);               /* 182 */
void xmidi_set_control_value(int seq, int chan, int control, int val); /* 183 */
int xmidi_channel_notes(int seq, int chan);                            /* 185 */
void xmidi_send_cv(int status, int d1, int d2);                        /* 186 send_cv_msg */
void xmidi_send_sysex(int a, int b, int c, const u8 *data, int size, int wait); /* 187 */
void xmidi_write_display(const char *text);                            /* 188 */
int xmidi_lock_channel(void);                                          /* 191: 1-based channel, 0 none */
void xmidi_map_channel(int seq, int seq_chan, int phys_chan);          /* 192 */
int xmidi_true_channel(int seq, int seq_chan);                         /* 194 */
void xmidi_release_channel(int chan);                                  /* 193 */

/* (ours, not the original's: whether a channel 0-15 is locked for an effect: sound.c routes its notes) */
bool xmidi_channel_locked(int chan);
