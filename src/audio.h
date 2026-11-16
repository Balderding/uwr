/* Sound: the game's music (data\music\*.mid, converted from the .XMI files: tools\xmi.py is the reference)
 * through Windows' MIDI synthesiser on its own thread, the playlist in data\music\playlist.txt; the speech
 * (data\sound\speech\NN.wav, from the .VOC files). */
#pragma once
#include "base.h"

/* XMI -> a standard MIDI file (format 0, 60 ticks a quarter at 120 a second); false when it is not XMI */
bool xmi_to_mid(Bytes xmi, u8 *out, size_t cap, size_t *n);
/* Creative Voice -> WAV (8-bit mono); false when it is not a .VOC */
bool voc_to_wav(Bytes voc, u8 *out, size_t cap, size_t *n);

bool music_start(const char *data_dir, float volume); /* plays the playlist round and round; false = no music */
void music_volume(float v);                          /* 0..1 */
void music_next(void);                               /* the next song */
void music_stop(void);
