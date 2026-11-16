/* xmidi.c: UW's MT32MPU.ADV (Miles AIL 2 XMIDI driver for the Roland MT-32 through an MPU-401), ported routine by
 * routine. The map: AIL 2.14's XMIDI.ASM (v1.10), MT32.INC and MPU401.INC; the truth: UW's own driver (1991, an
 * earlier XMIDI.ASM: see "version" notes), run in the emulator by tools/adv.py and compared byte for byte by
 * tests/xmidi_test.c. Each routine keeps its original name; 16-bit arithmetic is kept where it shows. */
#include "xmidi.h"

#include <string.h>

enum { NUM_CHANS = 16, MAX_NOTES = 32, FOR_NEST = 4, NUM_TIMBS = 64, NUM_CONTROLS = 9 };
enum { SEQ_STOPPED = 0, SEQ_PLAYING = 1, SEQ_DONE = 2 };
enum { MAX_REC_CHAN = 10, MAX_TRUE_CHAN = 9, MIN_TRUE_CHAN = 2, SYSEX_SIZE = 32, SYSEX_Q_CNT = 3, DEF_SYNTH_VOL = 90 };
enum { QUANT_TIME = 8333, QUANT_TIME_16 = 0x208d5, DEF_PITCH_L = 0x00, DEF_PITCH_H = 0x40 };
/* controllers (AIL.INC) */
enum {
    MODULATION = 1, PART_VOLUME = 7, PANPOT = 10, EXPRESSION = 11, SUSTAIN = 64, RESET_ALL_CTRLS = 121, ALL_NOTES_OFF = 123,
    START_MSB_1 = 32, FINAL_BYTE_1 = 36, RHYTHM_KEY_TIMB = 58, PATCH_REVERB = 59, PATCH_BENDER = 60, REVERB_MODE = 61,
    REVERB_TIME = 62, REVERB_LEVEL = 63, CHAN_LOCK = 110, CHAN_PROTECT = 111, VOICE_PROTECT = 112, TIMBRE_PROTECT = 113,
    PATCH_BANK_SEL = 114, INDIRECT_C_PFX = 115, FOR_LOOP = 116, NEXT_LOOP = 117, CLEAR_BEAT_BAR = 118, CALLBACK_TRIG = 119,
    SEQ_INDEX = 120,
};
enum { SYSEX_BLK_SIZE = FINAL_BYTE_1 - START_MSB_1 + 1, SYSEX_RANGE_BEG = START_MSB_1,
       SYSEX_RANGE_END = SYSEX_RANGE_BEG + SYSEX_BLK_SIZE * SYSEX_Q_CNT - 1 };
/* the controller log's rows (ctrl_log: PV, MODUL, PAN, EXP, SUS, PBS, C_LOCK, C_PROT, V_PROT) */
enum { PV = 0, MODUL = 1, PAN = 2, EXP = 3, SUS = 4, PBS = 5, C_LOCK = 6, C_PROT = 7, V_PROT = 8 };

static const u8 logged_ctrls[NUM_CONTROLS] = {PART_VOLUME, MODULATION, PANPOT, EXPRESSION, SUSTAIN, PATCH_BANK_SEL,
                                              CHAN_LOCK, CHAN_PROTECT, VOICE_PROTECT};
static const u8 ctrl_default[NUM_CONTROLS] = {127, 0, 64, 127, 0, 0, 0, 0, 0};
static const u8 prg_default[9] = {68, 48, 95, 78, 41, 3, 110, 122, 0xff}; /* (Roland defaults) */
static const u8 init_reverb[3] = {0, 3, 2};
static const u8 part_chans[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
static const u8 part_rsv[9] = {3, 4, 3, 4, 3, 4, 3, 4, 4};

typedef struct { /* state_table */
    const u8 *x;            /* the XMIDI file (the original's far pointers: offsets into it here; -1 = none) */
    i32 TIMB, RBRN, EVNT, EVNT_ptr;
    int cur_callback;
    const u8 *ctrl_ptr;
    int seq_handle, seq_started, status, post_release;
    i16 interval_cnt;
    u16 note_count;
    u16 vol_percent, vol_target, tempo_percent, tempo_target;
    i16 tempo_error;
    u32 vol_accum, vol_period, tempo_accum, tempo_period;
    u16 beat_count, time_numerator;
    i16 measure_count;
    u32 time_fraction, beat_fraction, time_per_beat;
    i32 FOR_ptrs[FOR_NEST];
    i16 FOR_loop_cnt[FOR_NEST];
    u8 chan_map[NUM_CHANS], chan_program[NUM_CHANS], chan_pitch_l[NUM_CHANS], chan_pitch_h[NUM_CHANS], chan_indirect[NUM_CHANS];
    u8 chan_controls[NUM_CONTROLS * NUM_CHANS];
    u8 note_chan[MAX_NOTES], note_num[MAX_NOTES];
    i32 note_time[MAX_NOTES];
} State;

static struct {
    XmidiIO io;
    State states[XMIDI_SEQS];
    State *sequence_state[XMIDI_SEQS];
    int sequence_count, current_handle, service_active, init_OK;
    void (*trigger_fn)(int seq, int value);
    u8 global_controls[NUM_CONTROLS * NUM_CHANS], global_program[NUM_CHANS], global_pitch_l[NUM_CHANS], global_pitch_h[NUM_CHANS];
    u8 active_notes[NUM_CHANS], lock_status[NUM_CHANS];
    u8 ctrl_hash[256];
    /* MT32.INC */
    u8 patch_bank[128];
    u16 chksum;
    u8 string_buf[32];
    u8 sysex_queues[SYSEX_Q_CNT][SYSEX_SIZE];
    u8 M_ad[SYSEX_Q_CNT], K_ad[SYSEX_Q_CNT], L_ad[SYSEX_Q_CNT], queue_ptrs[SYSEX_Q_CNT];
    u32 note_event, timb_hist[NUM_TIMBS];
    u8 timb_bank[NUM_TIMBS], timb_num[NUM_TIMBS], timb_attribs[NUM_TIMBS];
    u8 chan_timbs[NUM_CHANS], MIDI_bank[NUM_CHANS], MIDI_program[NUM_CHANS];
} X;

static State *seq_state(int handle) { return handle >= 0 && handle < XMIDI_SEQS * 4 && !(handle & 3) ? X.sequence_state[handle / 4] : NULL; }

static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

/* --- MPU401.INC ------------------------------------------------------------------------------------------------- */
static void send_byte(int b) { X.io.out(X.io.user, 0x330, b & 0xff); }
static void send_cmd(int b) { X.io.out(X.io.user, 0x331, b & 0xff); } /* (the acknowledgement 0xFE always comes) */
static void reset_interface(void) { send_cmd(0xff); }
static void init_interface(void) { send_cmd(0x3f); }

/* --- MT32.INC ----------------------------------------------------------------------------------------------------- */
static void sysex_wait(int delay) {
    for (int i = 0; i < delay; i++) X.io.retrace(X.io.user);
}
static void send_sysex_msg(int a, int b, int c, const u8 *data, int size, int wait) {
    send_byte(0xf0), send_byte(0x41), send_byte(0x10), send_byte(0x16), send_byte(0x12);
    send_byte(a), send_byte(b), send_byte(c);
    X.chksum = (u16)((a & 0xff) + (b & 0xff) + (c & 0xff));
    for (int i = 0; i < size; i++) X.chksum = (u16)(X.chksum + data[i]), send_byte(data[i]);
    u8 al = (u8)(X.chksum & 0x7f);
    al = (u8)(al - 0x80), al = (u8)-al;
    send_byte(al & 0x7f);
    send_byte(0xf7);
    sysex_wait(wait);
}
/* add a value to a 21-bit (3 x 7) sysex address */
static void add_sysex_addr(int addend, int msb, int ksb, int lsb, int *m, int *k, int *l) {
    int ax = (lsb & 0xff) + addend, bx = ksb & 0xff, cx = msb & 0xff;
    while ((u16)ax >= 0x80) ax -= 0x80, bx++;
    while ((u16)bx >= 0x80) bx -= 0x80, cx++;
    *l = ax & 0xff, *k = bx & 0xff, *m = cx & 0xff;
}
static void write_system(int index, int value) {
    u8 v = (u8)value;
    send_sysex_msg(0x10, 0, index, &v, 1, 0);
}
static void write_rhythm_setup(int keynum, int offset, int value) {
    int m, k, l;
    u8 v = (u8)value;
    add_sysex_addr((keynum - 24) * 4 + offset, 3, 1, 16, &m, &k, &l);
    send_sysex_msg(m, k, l, &v, 1, 4);
}
static void write_patch(int patch, int index, int value, int size) {
    int m, k, l;
    u8 v[2] = {(u8)value, (u8)(value >> 8)};
    add_sysex_addr((patch & 0xffff) * 8 + index, 5, 0, 0, &m, &k, &l);
    send_sysex_msg(m, k, l, v, size, 2);
}
static int index_timbre(int gnum) {
    for (int si = 0; si < NUM_TIMBS; si++)
        if ((X.timb_attribs[si] & 0x80) && X.timb_bank[si] == ((gnum >> 8) & 0xff) && X.timb_num[si] == (gnum & 0xff)) return si;
    return -1;
}
static void setup_patch(int patch, int bank) {
    int ax;
    X.patch_bank[patch & 0x7f] = (u8)bank;
    if ((bank & 0xff) != 0) {
        int t = index_timbre((bank & 0xff) << 8 | (patch & 0xff));
        if (t != -1) {
            ax = (t & 0xff) << 8 | 2; /* timbre group 2 (memory), its number */
            write_patch(patch, 0, ax, 2);
            return;
        }
    }
    ax = (patch & 63) << 8 | ((patch & 0xff) < 64 ? 0 : 1); /* group 0 (A) or 1 (B) */
    write_patch(patch, 0, ax, 2);
}
static void reset_synth(void) {
    send_sysex_msg(0x7f, 0x00, 0x00, part_chans, 1, 12); /* reset all internal parameters / patches */
    send_sysex_msg(0x10, 0x00, 0x0d, part_chans, 9, 4);   /* part / channel assignments */
}
static void init_synth(void) {
    send_sysex_msg(0x10, 0x00, 0x04, part_rsv, 9, 4); /* partial reserve */
    send_sysex_msg(0x10, 0x00, 0x01, init_reverb, 3, 4);
    for (int b = 0; b < SYSEX_Q_CNT; b++) X.queue_ptrs[b] = 0;
    X.note_event = 0;
    for (int i = 0; i < NUM_TIMBS; i++) X.timb_attribs[i] = 0;
    for (int i = 0; i < NUM_CHANS; i++) X.chan_timbs[i] = 0xff, X.MIDI_program[i] = 0xff, X.MIDI_bank[i] = 0;
    for (int i = 0; i < 128; i++) X.patch_bank[i] = 0;
}
static void send_MIDI_sysex(const u8 *src, int type, int len) {
    if (type == 0xf0) send_byte(0xf0);
    for (int i = 0; i < len; i++) send_byte(src[i]);
}
static void send_MIDI_message(int stat, int d1, int d2) {
    int si = d1 & 0xff, di = stat & 0x0f, ax = stat & 0xf0;
    if (ax == 0xb0) {
        if (si < SYSEX_RANGE_BEG) goto send;
        if (si <= SYSEX_RANGE_END) {
            int idx = si - SYSEX_RANGE_BEG, q = idx / SYSEX_BLK_SIZE, op = idx % SYSEX_BLK_SIZE, cnt;
            u8 val = (u8)d2;
            if (op == 0) X.M_ad[q] = val;
            else if (op == 1) X.K_ad[q] = val;
            else if (op == 2) X.L_ad[q] = val;
            else {
                X.sysex_queues[q][X.queue_ptrs[q]] = val;
                if (op == 3 && X.queue_ptrs[q] < SYSEX_SIZE - 1) {
                    X.queue_ptrs[q]++;
                    return;
                }
                cnt = X.queue_ptrs[q] + 1;
                send_sysex_msg(X.M_ad[q], X.K_ad[q], X.L_ad[q], X.sysex_queues[q], cnt, 0);
                if (op == 3) sysex_wait(4);
                else cnt--;
                int m, k, l;
                add_sysex_addr(cnt, X.M_ad[q], X.K_ad[q], X.L_ad[q], &m, &k, &l);
                X.L_ad[q] = (u8)l, X.K_ad[q] = (u8)k, X.M_ad[q] = (u8)m;
                X.queue_ptrs[q] = 0;
            }
            return;
        }
        if (si == PATCH_REVERB || si == PATCH_BENDER) {
            int prog = X.MIDI_program[di];
            if (prog == 0xff) return;
            write_patch(prog, si == PATCH_REVERB ? 6 : 4, d2 & 0xff, 1);
            send_byte(0xc0 | di), send_byte(prog);
            return;
        }
        if (si == REVERB_MODE) return write_system(1, d2);
        if (si == REVERB_TIME) return write_system(2, d2);
        if (si == REVERB_LEVEL) return write_system(3, d2);
        if (si == PATCH_BANK_SEL) {
            X.MIDI_bank[di] = (u8)d2;
            return;
        }
        if (si == RHYTHM_KEY_TIMB) {
            if (X.chan_timbs[di] == 0xff) return;
            return write_rhythm_setup(d2 & 0xff, 0, X.chan_timbs[di]);
        }
        if (si == TIMBRE_PROTECT) {
            int t = X.chan_timbs[di];
            if (t == 0xff) return;
            u8 a = X.timb_attribs[t] & 0xbf;
            if ((i8)(u8)d2 >= 64) a |= 0x40;
            X.timb_attribs[t] = a;
            return;
        }
        if (si < CHAN_LOCK || si > SEQ_INDEX) goto send;
        return;
    }
    if (ax == 0xc0) { /* program change */
        X.MIDI_program[di] = (u8)si;
        int bank = X.MIDI_bank[di];
        if (bank != X.patch_bank[si & 0x7f]) setup_patch(si, bank);
        X.chan_timbs[di] = (u8)index_timbre(bank << 8 | si);
        goto send;
    }
    if (ax == 0x90) { /* note on (or off): the timbre cache's LRU count */
        X.note_event++;
        if (X.chan_timbs[di] != 0xff) X.timb_hist[X.chan_timbs[di]] = X.note_event;
    }
send:
    send_byte(stat);
    send_byte(si);
    if ((stat & 0xf0) != 0xc0 && (stat & 0xf0) != 0xd0) send_byte(d2);
}

/* --- XMIDI.ASM ---------------------------------------------------------------------------------------------------- */
static int find_seq(const u8 *x, size_t size, int seqnum) { /* the offset of FORM XMID seqnum, or -1 */
    int cx = seqnum + 1;
    size_t si = 0;
    i32 end;
    for (;;) {
        if (si + 12 > size) return -1;
        if (memcmp(x + si, "CAT ", 4) && memcmp(x + si, "FORM", 4)) return -1;
        if (!memcmp(x + si + 8, "XMID", 4)) break;
        si += be32(x + si + 4) + 8;
    }
    end = (i32)(be32(x + si + 4) - 5);
    if (!memcmp(x + si, "FORM", 4)) return cx == 1 ? (int)si : -1;
    si += 12;
    for (;;) {
        if (si + 12 > size) return -1;
        if (!memcmp(x + si + 8, "XMID", 4) && --cx == 0) return (int)si;
        u32 len = be32(x + si + 4) + 8;
        end -= (i32)len;
        if (end < 0) return -1;
        si += len;
    }
}
static void rewind_seq(int handle) {
    State *s = seq_state(handle);
    for (int i = 0; i < FOR_NEST; i++) s->FOR_loop_cnt[i] = -1;
    for (int b = NUM_CHANS - 1; b >= 0; b--)
        s->chan_map[b] = (u8)b, s->chan_program[b] = 0xff, s->chan_pitch_l[b] = 0xff, s->chan_pitch_h[b] = 0xff, s->chan_indirect[b] = 0xff;
    memset(s->chan_controls, 0xff, sizeof s->chan_controls);
    memset(s->note_chan, 0xff, sizeof s->note_chan);
    s->cur_callback = -1, s->interval_cnt = 0, s->note_count = 0;
    s->vol_percent = s->vol_target = DEF_SYNTH_VOL;
    s->tempo_percent = s->tempo_target = 100, s->tempo_error = 0;
    s->beat_count = 0, s->measure_count = -1, s->beat_fraction = 0, s->time_fraction = 0;
    s->time_numerator = 4, s->time_per_beat = 0x7a1200; /* 500000 us a beat x 16 (120 beats a minute) */
}
static void flush_channel_notes(int chan) {
    int handle = 0;
    for (int seqcnt = X.sequence_count; seqcnt > 0; seqcnt--) {
        State *s;
        while (!(s = seq_state(handle))) handle += 4;
        handle += 4;
        if (!s->note_count) continue;
        for (int b = 0; b < MAX_NOTES; b++) {
            if (s->note_chan[b] != (u8)chan) continue;
            s->note_chan[b] = 0xff;
            int phys = s->chan_map[chan & 0x0f];
            X.active_notes[phys]--;
            send_MIDI_message(0x80 | phys, s->note_num[b], 0);
            s->note_count--;
        }
    }
}
static void flush_note_queue(State *s) {
    for (int b = 0; b < MAX_NOTES; b++) {
        if (s->note_chan[b] == 0xff) continue;
        int chan = s->note_chan[b];
        s->note_chan[b] = 0xff;
        int phys = s->chan_map[chan];
        X.active_notes[phys]--;
        send_MIDI_message(0x80 | phys, s->note_num[b], 0);
    }
    s->note_count = 0;
}
static void reset_sequence(State *s) {
    for (int di = 0; di < NUM_CHANS; di++) {
        if ((i8)s->chan_controls[SUS * NUM_CHANS + di] >= 64) {
            X.global_controls[SUS * NUM_CHANS + di] = 0; /* (version 1.04: UW's driver may not do this) */
            send_MIDI_message(0xb0 | di, SUSTAIN, 0);
        }
        if ((i8)s->chan_controls[C_LOCK * NUM_CHANS + di] >= 64) {
            flush_channel_notes(di);
            xmidi_release_channel(s->chan_map[di] + 1);
            s->chan_map[di] = (u8)di;
        }
        if ((i8)s->chan_controls[C_PROT * NUM_CHANS + di] >= 64) X.lock_status[di] &= 0xbf;
        if ((i8)s->chan_controls[V_PROT * NUM_CHANS + di] >= 64) send_MIDI_message(0xb0 | di, VOICE_PROTECT, 0);
    }
}
static int XMIDI_control(State *s, int chan, int con, int val);
static void restore_sequence(State *s) {
    for (int di = 0; di < NUM_CHANS; di++) { /* re-lock channels formerly locked by CHAN_LOCK */
        u8 al = s->chan_controls[C_LOCK * NUM_CHANS + di];
        if (al == 0xff || (i8)al < 64) continue;
        int ax = xmidi_lock_channel() - 1;
        if (ax == -1) ax = di;
        s->chan_map[di] = (u8)ax;
    }
    for (int con = 0; con < NUM_CONTROLS; con++) {
        int ctrl = logged_ctrls[con];
        if (ctrl == CHAN_LOCK) continue;
        int index = X.ctrl_hash[ctrl];
        for (int di = 0; di < NUM_CHANS; di++) {
            u8 al = s->chan_controls[index + di];
            if (al == 0xff) continue;
            XMIDI_control(s, di, ctrl, al);
        }
    }
    for (int di = 0; di < NUM_CHANS; di++) {
        if (s->chan_pitch_l[di] != 0xff && s->chan_pitch_h[di] != 0xff)
            send_MIDI_message(0xe0 | s->chan_map[di], s->chan_pitch_l[di], s->chan_pitch_h[di]);
        if (s->chan_program[di] != 0xff) send_MIDI_message(0xc0 | s->chan_map[di], s->chan_program[di], 0);
    }
}
static void XMIDI_volume(State *s) {
    for (int b = 0; b < NUM_CHANS; b++) {
        u8 al = s->chan_controls[PV * NUM_CHANS + b];
        if (al == 0xff) continue;
        u16 ax = (u16)((u32)al * s->vol_percent / 100);
        if (ax >= 127) ax = 127;
        X.global_controls[PV * NUM_CHANS + b] = (u8)ax;
        if (X.lock_status[b] & 0x80) continue;
        send_MIDI_message(0xb0 | s->chan_map[b], PART_VOLUME, ax);
    }
}
static int XMIDI_control(State *s, int chan, int con, int val) {
    int bx = chan & 0xff, dx = val & 0xff;
    if (s->chan_indirect[bx] != 0xff) { /* a value from the caller's controller table */
        int at = s->chan_indirect[bx];
        s->chan_indirect[bx] = 0xff;
        dx = s->ctrl_ptr ? s->ctrl_ptr[at] : 0;
    }
    if (X.ctrl_hash[con & 0xff] != 0xff) {
        int at = X.ctrl_hash[con & 0xff] + chan;
        X.global_controls[at] = (u8)dx;
        s->chan_controls[at] = (u8)dx;
    }
    int ax = con & 0xff;
    switch (ax) {
    case PART_VOLUME:
        if (s->vol_percent != 100) {
            u16 v = (u16)((u32)dx * s->vol_percent / 100);
            dx = v < 127 ? v : 127;
            X.global_controls[PV * NUM_CHANS + bx] = (u8)dx;
        }
        break;
    case CLEAR_BEAT_BAR:
        s->beat_count = 0, s->measure_count = 0;
        s->beat_fraction = 0 - s->time_fraction;
        return 3;
    case CALLBACK_TRIG:
        s->cur_callback = dx;
        if (X.trigger_fn) X.trigger_fn(s->seq_handle, dx);
        return 3;
    case FOR_LOOP:
        for (int b = 0; b < FOR_NEST; b++)
            if (s->FOR_loop_cnt[b] == -1) {
                s->FOR_loop_cnt[b] = (i16)dx;
                s->FOR_ptrs[b] = s->EVNT_ptr;
                break;
            }
        return 3;
    case NEXT_LOOP:
        if ((i8)(u8)dx < 64) return 3;
        for (int b = FOR_NEST - 1; b >= 0; b--) {
            if (s->FOR_loop_cnt[b] == -1) continue;
            if (s->FOR_loop_cnt[b] != 0) {
                if (--s->FOR_loop_cnt[b] == 0) {
                    s->FOR_loop_cnt[b] = -1;
                    return 3;
                }
            }
            s->EVNT_ptr = s->FOR_ptrs[b];
            return 3;
        }
        return 3;
    case CHAN_PROTECT:
        X.lock_status[bx] |= 0x40;
        if ((i8)(u8)dx < 64) X.lock_status[bx] &= 0xbf;
        return 3;
    case CHAN_LOCK:
        if ((i8)(u8)dx >= 64) {
            int a = xmidi_lock_channel() - 1;
            if (a == -1) a = bx;
            s->chan_map[bx] = (u8)a;
        } else {
            flush_channel_notes(bx);
            xmidi_release_channel(s->chan_map[bx] + 1);
            s->chan_map[bx] = (u8)bx;
        }
        return 3;
    case INDIRECT_C_PFX: s->chan_indirect[bx] = (u8)dx; return 3;
    }
    if (X.lock_status[bx] & 0x80) return 3;
    send_MIDI_message(0xb0 | s->chan_map[bx], ax, dx);
    return 3;
}
static u32 vlq(const u8 *p, int *n) {
    u32 v = 0;
    int i = 0;
    u8 b;
    do b = p[i++], v = v << 7 | (b & 0x7f);
    while (b & 0x80);
    *n = i;
    return v;
}
static int XMIDI_note_on(State *s) {
    const u8 *e = s->x + s->EVNT_ptr;
    int chan = e[0] & 0x0f, note = e[1], vel = e[2], n;
    u32 dur = vlq(e + 3, &n);
    int len = 3 + n;
    if (X.lock_status[chan] & 0x80) return len;
    int slot = 0;
    while (slot < MAX_NOTES && s->note_chan[slot] != 0xff) slot++;
    if (slot < MAX_NOTES) s->note_count++;
    else slot = 0; /* the queue is full: the first entry is overwritten (the original's repne scasb) */
    s->note_chan[slot] = (u8)chan, s->note_num[slot] = (u8)note;
    s->note_time[slot] = (i32)(dur - 1);
    int phys = s->chan_map[chan];
    X.active_notes[phys]++;
    send_MIDI_message(0x90 | phys, note, vel);
    return len;
}
static int XMIDI_meta(State *s) {
    const u8 *e = s->x + s->EVNT_ptr;
    int type = e[1], n;
    u32 len = vlq(e + 2, &n);
    int event_len = (int)len + 2 + n;
    const u8 *d = e + 2 + n;
    if (type == 0x2f) { /* end of the sequence */
        reset_sequence(s);
        s->status = SEQ_DONE;
        if (s->post_release) xmidi_release(X.current_handle);
    } else if (type == 0x58) { /* time signature: beat counting only */
        s->time_numerator = d[0];
        int cx = d[1] - 2;
        u32 frac;
        if (cx >= 0) frac = (u32)QUANT_TIME_16 * (u32)(1u << cx);
        else frac = (u32)QUANT_TIME_16 >> -cx;
        s->time_fraction = frac;
        s->beat_fraction = 0 - frac;
        s->beat_count = 0;
        s->measure_count++;
    } else if (type == 0x51) { /* tempo: beat counting only (the playback is at 120 intervals a second) */
        s->time_per_beat = ((u32)d[0] << 16 | (u32)d[1] << 8 | d[2]) << 4;
    }
    return event_len;
}
static int XMIDI_sysex(State *s) {
    const u8 *e = s->x + s->EVNT_ptr;
    int n;
    u32 len = vlq(e + 1, &n);
    send_MIDI_sysex(e + 1 + n, e[0], (int)len);
    return 1 + n + (int)len;
}

void xmidi_serve(void) {
    if (X.service_active) return;
    X.service_active++;
    X.current_handle = -4;
    for (int seqcnt = X.sequence_count; seqcnt > 0; seqcnt--) {
        State *s;
        do X.current_handle += 4;
        while (!(s = seq_state(X.current_handle)));
        if (s->status != SEQ_PLAYING) continue;
        s->tempo_error = (i16)(s->tempo_error + s->tempo_percent);
        i16 ax = (i16)(s->tempo_error - 100);
        while (ax >= 0) { /* an interval, as many as the tempo gives */
            s->tempo_error = ax;
            if (s->note_count) { /* the note queue: each note's time; at its end, its Note Off */
                for (int b = 0; b < MAX_NOTES && s->note_count; b++) {
                    if (s->note_chan[b] == 0xff) continue;
                    if (--s->note_time[b] >= 0) continue;
                    int chan = s->note_chan[b];
                    s->note_chan[b] = 0xff;
                    int phys = s->chan_map[chan];
                    X.active_notes[phys]--;
                    send_MIDI_message(0x80 | phys, s->note_num[b], 0);
                    s->note_count--;
                }
            }
            if (--s->interval_cnt <= 0) { /* the events due */
                for (;;) {
                    const u8 *e = s->x + s->EVNT_ptr;
                    int b = e[0], di;
                    if (b < 128) { /* an interval count */
                        s->EVNT_ptr++;
                        s->interval_cnt = (i16)b;
                        break;
                    }
                    int st = b & 0xf0, chan = b & 0x0f, cl = e[1], dl = e[2];
                    if (st == 0xf0) di = chan == 0x0f ? XMIDI_meta(s) : XMIDI_sysex(s);
                    else if (st == 0xb0) XMIDI_control(s, chan, cl, dl), di = 3;
                    else if (st < 0xa0) di = XMIDI_note_on(s);
                    else {
                        if (st == 0xe0) {
                            s->chan_pitch_l[chan] = (u8)cl, s->chan_pitch_h[chan] = (u8)dl;
                            X.global_pitch_l[chan] = (u8)cl, X.global_pitch_h[chan] = (u8)dl;
                            di = 3;
                        } else if (st == 0xd0) di = 2;
                        else if (st == 0xc0) {
                            s->chan_program[chan] = (u8)cl, X.global_program[chan] = (u8)cl;
                            di = 2;
                        } else di = 3; /* 0xa0 */
                        if (!(X.lock_status[chan] & 0x80)) send_MIDI_message(st | s->chan_map[chan], cl, dl);
                    }
                    s->EVNT_ptr += di;
                    if (s->status != SEQ_PLAYING) goto next_seq;
                }
            }
            { /* the beat count */
                u32 f = s->beat_fraction + s->time_fraction;
                i16 fh = (i16)(f >> 16), th = (i16)(s->time_per_beat >> 16);
                bool beat = fh > th || (fh == th && (i16)(f & 0xffff) >= (i16)(s->time_per_beat & 0xffff));
                if (beat) {
                    f -= s->time_per_beat;
                    if (++s->beat_count >= s->time_numerator) s->beat_count = 0, s->measure_count++;
                }
                s->beat_fraction = f;
            }
            ax = (i16)(s->tempo_error - 100);
        }
        if (s->tempo_percent != s->tempo_target) { /* a tempo gradient */
            bool less = (i16)s->tempo_percent < (i16)s->tempo_target;
            u32 acc = s->tempo_accum + QUANT_TIME / 100;
            int cx = -1;
            do cx++, s->tempo_accum = acc;
            while ((i32)(acc -= s->tempo_period) >= 0);
            if (cx) {
                int t = s->tempo_target, a = s->tempo_percent;
                if (less) a += cx, a = a > t ? t : a;
                else a -= cx, a = a < t ? t : a;
                s->tempo_percent = (u16)a;
            }
        }
        if (s->vol_percent != s->vol_target) { /* a volume gradient */
            bool less = (i16)s->vol_percent < (i16)s->vol_target;
            u32 acc = s->vol_accum + QUANT_TIME / 100;
            int cx = -1;
            do cx++, s->vol_accum = acc;
            while ((i32)(acc -= s->vol_period) >= 0);
            if (cx) {
                int t = s->vol_target, a = s->vol_percent;
                if (less) a += cx, a = a > t ? t : a;
                else a -= cx, a = a < t ? t : a;
                s->vol_percent = (u16)a;
                XMIDI_volume(s);
            }
        }
    next_seq:;
    }
    X.service_active--;
}

void xmidi_init(const XmidiIO *io) {
    X.io = *io;
    X.service_active = 0, X.sequence_count = 0;
    memset(X.global_controls, 0xff, sizeof X.global_controls);
    memset(X.global_program, 0xff, sizeof X.global_program);
    memset(X.global_pitch_l, 0xff, sizeof X.global_pitch_l);
    memset(X.global_pitch_h, 0xff, sizeof X.global_pitch_h);
    memset(X.ctrl_hash, 0xff, sizeof X.ctrl_hash);
    memset(X.sequence_state, 0, sizeof X.sequence_state);
    memset(X.lock_status, 0, sizeof X.lock_status);
    memset(X.active_notes, 0, sizeof X.active_notes);
    for (int i = 0; i < NUM_CONTROLS; i++) X.ctrl_hash[logged_ctrls[i]] = (u8)(i * NUM_CHANS);
    reset_interface();
    init_interface();
    reset_synth();
    init_synth();
    X.trigger_fn = NULL;
    for (int si = 0; si < NUM_CONTROLS; si++) /* the controllers' start values, channels 2-10 */
        for (int di = MIN_TRUE_CHAN - 1; di <= MAX_REC_CHAN - 1; di++) {
            X.global_controls[X.ctrl_hash[logged_ctrls[si]] + di] = ctrl_default[si];
            send_MIDI_message(0xb0 | di, logged_ctrls[si], ctrl_default[si]);
        }
    sysex_wait(10);
    for (int di = MIN_TRUE_CHAN - 1; di <= MAX_REC_CHAN - 1; di++) {
        X.global_pitch_l[di] = DEF_PITCH_L, X.global_pitch_h[di] = DEF_PITCH_H;
        send_MIDI_message(0xe0 | di, DEF_PITCH_L, DEF_PITCH_H);
        int p = prg_default[di - (MIN_TRUE_CHAN - 1)];
        if (p == 0xff) continue;
        X.global_program[di] = (u8)p;
        send_MIDI_message(0xc0 | di, p, 0);
    }
    sysex_wait(10);
    X.init_OK = 1;
}

void xmidi_shutdown(const char *signoff) {
    if (!X.init_OK) return;
    int handle = 0;
    for (int seqcnt = X.sequence_count; seqcnt > 0; seqcnt--) {
        while (!seq_state(handle)) handle += 4;
        xmidi_stop(handle);
        xmidi_release(handle);
        handle += 4;
    }
    reset_synth();
    xmidi_write_display(signoff);
    reset_interface();
    X.init_OK = 0;
}

int xmidi_register(const u8 *xmi, size_t size, int seqnum, const u8 *ctrl) {
    int handle = -1;
    for (int b = 0; b < XMIDI_SEQS && handle < 0; b++)
        if (!X.sequence_state[b]) handle = b * 4;
    if (handle < 0) return -1;
    int di = find_seq(xmi, size, seqnum);
    if (di < 0) return -1;
    State *s = &X.states[handle / 4];
    memset(s, 0, sizeof *s);
    s->x = xmi, s->TIMB = s->RBRN = s->EVNT = -1;
    X.sequence_state[handle / 4] = s;
    u32 chunk_len = 12;
    for (;;) {
        di += (int)chunk_len;
        if ((size_t)di + 8 > size) return -1; /* (the original reads on) */
        chunk_len = be32(xmi + di + 4) + 8;
        if (!memcmp(xmi + di, "TIMB", 4)) s->TIMB = di;
        else if (!memcmp(xmi + di, "RBRN", 4)) s->RBRN = di;
        else if (!memcmp(xmi + di, "EVNT", 4)) break;
    }
    s->seq_handle = handle, s->EVNT = di, s->ctrl_ptr = ctrl;
    s->post_release = 0, s->seq_started = 0, s->status = SEQ_STOPPED;
    X.sequence_count++;
    rewind_seq(handle);
    return handle;
}

void xmidi_release(int seq) {
    State *s = seq_state(seq);
    if (!s) return;
    if (s->status == SEQ_PLAYING) {
        s->post_release = 1;
        return;
    }
    X.sequence_state[seq / 4] = NULL;
    X.sequence_count--;
}

void xmidi_start(int seq) {
    State *s = seq_state(seq);
    if (!s) return;
    if (s->status == SEQ_PLAYING) xmidi_stop(seq);
    rewind_seq(seq);
    s->EVNT_ptr = s->EVNT + 8;
    s->status = SEQ_PLAYING;
    s->seq_started = 1;
}

void xmidi_stop(int seq) {
    State *s = seq_state(seq);
    if (!s || s->status != SEQ_PLAYING) return;
    flush_note_queue(s);
    reset_sequence(s);
    s->status = SEQ_STOPPED;
}

void xmidi_resume(int seq) {
    State *s = seq_state(seq);
    if (!s || s->status != SEQ_STOPPED || !s->seq_started) return;
    restore_sequence(s);
    s->status = SEQ_PLAYING;
}

int xmidi_status(int seq) { /* (a released handle: the original reads its stale pointer, segment 0: 0 in practice) */
    State *s = seq_state(seq);
    if (s) return s->status;
    return seq >= 0 && seq < XMIDI_SEQS * 4 && !(seq & 3) ? 0 : -1;
}
int xmidi_rel_volume(int seq) { State *s = seq_state(seq); return s ? s->vol_percent : -1; }
int xmidi_rel_tempo(int seq) { State *s = seq_state(seq); return s ? s->tempo_percent : -1; }

static u32 ul_divide(u32 num, u32 den) { return den ? num / den : 0; }
void xmidi_set_rel_tempo(int seq, int percent, int ms) {
    State *s = seq_state(seq);
    if (!s) return;
    s->tempo_target = (u16)percent;
    if (!ms) {
        s->tempo_percent = (u16)percent;
        return;
    }
    int d = (i16)(s->tempo_target - s->tempo_percent);
    if (!d) return;
    u32 p = ul_divide((u32)(u16)ms * 10u, (u32)(d < 0 ? -d : d));
    s->tempo_period = p ? p : 1, s->tempo_accum = 0;
}
void xmidi_set_rel_volume(int seq, int percent, int ms) {
    State *s = seq_state(seq);
    if (!s) return;
    s->vol_target = (u16)percent;
    if (!ms) {
        s->vol_percent = (u16)percent;
        XMIDI_volume(s);
        return;
    }
    int d = (i16)(s->vol_target - s->vol_percent);
    if (!d) return;
    u32 p = ul_divide((u32)(u16)ms * 10u, (u32)(d < 0 ? -d : d));
    s->vol_period = p ? p : 1, s->vol_accum = 0;
}

static void advance_count(State *s, int *beat, int *bar) {
    int ax = s->beat_count, dx = s->measure_count;
    u32 f = s->beat_fraction + s->time_fraction; /* QUANT_ADVANCE 1 */
    i16 fh = (i16)(f >> 16), th = (i16)(s->time_per_beat >> 16);
    if (fh > th || (fh == th && (i16)(f & 0xffff) >= (i16)(s->time_per_beat & 0xffff))) {
        if (++ax >= s->time_numerator) ax = 0, dx++;
    }
    *beat = ax, *bar = dx < 0 ? 0 : dx;
}
int xmidi_beat_count(int seq) {
    State *s = seq_state(seq);
    int b, m;
    if (!s) return -1;
    advance_count(s, &b, &m);
    return b;
}
int xmidi_bar_count(int seq) {
    State *s = seq_state(seq);
    int b, m;
    if (!s) return -1;
    advance_count(s, &b, &m);
    return m;
}

void xmidi_branch_index(int seq, int marker) {
    State *s = seq_state(seq);
    if (!s || s->RBRN < 0) return;
    const u8 *r = s->x + s->RBRN;
    if (memcmp(r, "RBRN", 4)) return;
    int cx = r[8] | r[9] << 8;
    for (int i = 0; i < cx; i++) {
        const u8 *e = r + 10 + i * 6;
        if (e[0] != (u8)marker) continue;
        u32 off = (u32)(e[2] | e[3] << 8 | e[4] << 16 | (u32)e[5] << 24) + 8;
        s->EVNT_ptr = s->EVNT + (i32)off;
        s->interval_cnt = 0;
        flush_note_queue(s);
        for (int b = 0; b < FOR_NEST; b++) s->FOR_loop_cnt[b] = -1; /* BRANCH_EXIT (version 1.08) */
        return;
    }
}

int xmidi_control_value(int seq, int chan, int control) {
    State *s = seq_state(seq);
    if (!s) return -1;
    if (control == CALLBACK_TRIG) return s->cur_callback;
    if (X.ctrl_hash[control & 0xff] == 0xff) return -1;
    return (i8)s->chan_controls[X.ctrl_hash[control & 0xff] + chan - 1];
}
void xmidi_set_control_value(int seq, int chan, int control, int val) {
    State *s = seq_state(seq);
    if (s) XMIDI_control(s, chan - 1, control, val);
}
int xmidi_channel_notes(int seq, int chan) {
    State *s = seq_state(seq);
    if (!s) return -1;
    int n = 0;
    for (int b = 0; b < MAX_NOTES; b++) n += s->note_chan[b] == (u8)(chan - 1);
    return n;
}
void xmidi_map_channel(int seq, int seq_chan, int phys_chan) {
    State *s = seq_state(seq);
    if (s) s->chan_map[seq_chan - 1] = (u8)(phys_chan - 1);
}
int xmidi_true_channel(int seq, int seq_chan) {
    State *s = seq_state(seq);
    return s ? s->chan_map[seq_chan - 1] + 1 : -1;
}

int xmidi_lock_channel(void) {
    int si = -1;
    u8 cl = 0xff;
    for (int pass = 0; pass < 2 && si == -1; pass++) {
        u8 mask = pass ? 0x80 : 0xc0;
        for (int di = MAX_TRUE_CHAN - 1; di >= MIN_TRUE_CHAN - 1; di--) {
            if (X.lock_status[di] & mask) continue;
            if (X.active_notes[di] >= cl) continue;
            cl = X.active_notes[di], si = di;
        }
    }
    if (si == -1) return 0;
    send_MIDI_message(0xb0 | si, SUSTAIN, 0);
    flush_channel_notes(si);
    X.active_notes[si] = 0;
    X.lock_status[si] |= 0x80;
    return si + 1;
}

void xmidi_release_channel(int chan) {
    int si = chan - 1;
    if (si < 0 || si >= NUM_CHANS || !(X.lock_status[si] & 0x80)) return;
    X.lock_status[si] &= 0x7f;
    X.active_notes[si] = 0;
    send_MIDI_message(0xb0 | si, SUSTAIN, 0);
    send_MIDI_message(0xb0 | si, ALL_NOTES_OFF, 0);
    for (int di = 0; di < NUM_CONTROLS; di++) {
        u8 dl = X.global_controls[di * NUM_CHANS + si];
        if (dl == 0xff) continue;
        send_MIDI_message(0xb0 | si, logged_ctrls[di], dl);
    }
    if (X.global_program[si] != 0xff) send_MIDI_message(0xc0 | si, X.global_program[si], 0);
    if (X.global_pitch_l[si] != 0xff && X.global_pitch_h[si] != 0xff) send_MIDI_message(0xe0 | si, X.global_pitch_l[si], X.global_pitch_h[si]);
}

void xmidi_send_cv(int status, int d1, int d2) { send_MIDI_message(status, d1, d2); }
void xmidi_send_sysex(int a, int b, int c, const u8 *data, int size, int wait) { send_sysex_msg(a, b, c, data, size, wait); }
void xmidi_write_display(const char *text) {
    if (!text) return;
    memset(X.string_buf, ' ', 20);
    for (int i = 0; i < 20 && text[i]; i++) X.string_buf[i] = (u8)text[i];
    send_sysex_msg(0x20, 0x00, 0x00, X.string_buf, 20, 4);
}

/* --- the timbres ------------------------------------------------------------------------------------------------- */
int xmidi_timbre_request(int seq) {
    State *s = seq_state(seq);
    if (!s || s->TIMB < 0) return -1;
    const u8 *t = s->x + s->TIMB;
    if (memcmp(t, "TIMB", 4)) return -1;
    int di = t[8] | t[9] << 8;
    for (int i = 0; i < di; i++) {
        int ax = t[10 + i * 2] | t[11 + i * 2] << 8; /* patch, bank */
        int bank = ax >> 8;
        if (bank == 0 || bank == 127) continue;
        if (index_timbre(ax) == -1) return ax;
    }
    return -1;
}
void xmidi_install_timbre(int bank, int patch, const u8 *timbre) {
    bank &= 0xff, patch &= 0xff;
    if (bank == 0) return setup_patch(patch, bank);
    if (bank == 127) return;
    if (index_timbre(bank << 8 | patch) != -1) return setup_patch(patch, bank);
    if (!timbre) return;
    int si = 0;
    while (si < NUM_TIMBS && (X.timb_attribs[si] & 0x80)) si++;
    if (si == NUM_TIMBS) { /* the least recently used unprotected timbre */
        int cx = -1;
        u32 best = 0xffffffffu;
        for (int i = 0; i < NUM_TIMBS; i++) {
            if (X.timb_attribs[i] & 0x40) continue;
            if (X.timb_hist[i] <= best) best = X.timb_hist[i], cx = i;
        }
        if (cx == -1) return;
        si = cx;
    }
    X.timb_hist[si] = X.note_event++;
    X.timb_num[si] = (u8)patch, X.timb_bank[si] = (u8)bank, X.timb_attribs[si] = 0x80;
    /* the timbre into the Roland's timbre memory (08 dest 00): the common part (14 bytes), four partials (58 each) */
    const u8 *di = timbre + 2;
    int dest = si * 2;
    send_sysex_msg(8, dest, 0x00, di, 0x0e, 3), di += 0x0e;
    send_sysex_msg(8, dest, 0x0e, di, 0x3a, 3), di += 0x3a;
    send_sysex_msg(8, dest, 0x48, di, 0x3a, 3), di += 0x3a;
    dest++;
    send_sysex_msg(8, dest, 0x02, di, 0x3a, 3), di += 0x3a;
    send_sysex_msg(8, dest, 0x3c, di, 0x3a, 3);
    setup_patch(patch, bank);
}
void xmidi_protect_timbre(int bank, int patch) {
    if ((bank & 0xff) == 0xff && (patch & 0xff) == 0xff) {
        for (int i = 0; i < NUM_TIMBS; i++) X.timb_attribs[i] |= 0x40;
        return;
    }
    int t = index_timbre((bank & 0xff) << 8 | (patch & 0xff));
    if (t != -1) X.timb_attribs[t] |= 0x40;
}
void xmidi_unprotect_timbre(int bank, int patch) {
    if ((bank & 0xff) == 0xff && (patch & 0xff) == 0xff) {
        for (int i = 0; i < NUM_TIMBS; i++) X.timb_attribs[i] &= 0xbf;
        return;
    }
    int t = index_timbre((bank & 0xff) << 8 | (patch & 0xff));
    if (t != -1) X.timb_attribs[t] &= 0xbf;
}
int xmidi_timbre_status(int bank, int patch) {
    bank &= 0xff, patch &= 0xff;
    if (bank == 0 || bank == 127) return (bank << 8 | patch) + 1;
    return index_timbre(bank << 8 | patch) + 1;
}

bool xmidi_channel_locked(int chan) { return chan >= 0 && chan < NUM_CHANS && (X.lock_status[chan] & 0x80); }
