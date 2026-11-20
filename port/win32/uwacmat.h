/* uwacmat.h: the acoustic materials (docs\superpowers\plans\2026-11-14-acoustics-ours.md, Task 1): what each wall,
   floor and door of the level is made of for the sound (stone, wood, cloth, ...), read from data\acoustics.txt.
   Presentation only: nothing here is read by the game. Plain C, no OpenAL, no DLL: tests\uwacmat_test.c runs it
   alone (tools\fxcheck.py materials).

   data\acoustics.txt, one line a thing ('#' starts a comment):
       preset NAME absorb LOW MID HIGH transmit LOW MID HIGH [step N] [scatter S]     (transmit T: all three bands T)
       wall NNN PRESET        data\textures\walls\NNN.png (the texture's file number, decimal)
       floor NNN PRESET       data\textures\floors\NNN.png
       ceiling NNN PRESET     a floor texture used as the ceiling (one a level: the level's floor slot 9, the
                              original's GRIDDB.C:606); without a line of its own a ceiling is what its picture
                              is as a floor, else the ceilings' '*'
       door N PRESET          N 0-12: the door's picture, data\sprites\doors\NNN.png; N 0x140-0x14F: the door's item
       KIND * PRESET          every texture of the kind
   The last matching line wins (a '*' line after a numbered one takes that number back). absorb: the share of the
   sound a surface takes at a bounce (low, mid, high band, 0..1); transmit: the share that passes through it,
   by band (0..1: a door lets the low through and keeps the high, so what is heard behind it is dull as well as quiet);
   step: Task 6's footsteps: the semitones a footstep on such a floor is above the original's note (below when
   negative; -24 to 24), 0 when absent (stone: the original's sound). scatter (2026-11-19): the share of a bounce off
   it that goes every way rather than as off a mirror (0..1: polished marble about 0.05, rough-hewn rock about 0.5);
   absent: the Esc menu's "Wall roughness" for every surface, as before. Steam Audio's echoes only.
   Two tables (2026-11-19): acoustics.txt (made from the research, docs\SOUND_RESEARCH.md) and
   acoustics-2026-11-14.txt (the earlier one, every stone surface rough rock); uwacmat_earlier picks which is read. */
#ifndef UWACMAT_H
#define UWACMAT_H

typedef struct { char name[16]; float absorb[3], transmit[3]; int step; float scatter; } UwaMaterial;   /* low, mid, high; scatter -1: not given */
enum { UWA_FLOOR, UWA_CEILING, UWA_WALL, UWA_DOOR };

int uwacmat_load(const char *datadir);            /* presets read; 0 and one log line when the file is bad */
int uwacmat_count(void);                          /* the presets; 0 when nothing is loaded */
const UwaMaterial *uwacmat_get(int preset);       /* NULL out of range */
int uwacmat_for(int kind, int tex);               /* preset index; tex = the texture's file number, a door's item */

/* A door: its item's own line when there is one (the secret door looks like the wall whatever the level), else its
   picture's (the same item wears a different picture from level to level: level.c's lv.doors[item - 0x140]), else
   the doors' '*'. picture < 0: not known. */
int uwacmat_door(int item, int picture);

/* data\ is made on the first run and not kept in the repository, so the table we ship is in the code: the text
   (uwacmat_default) and a call that writes it as data\acoustics.txt when that file is not there yet (1 written,
   0 already there: the player's own edits are never overwritten; -1 it could not be written). */
const char *uwacmat_default(void);
int uwacmat_load_default(void);                   /* the table in the code read, no file touched: 1 */
int uwacmat_write_default(const char *datadir);
/* (2026-11-19) 1: the earlier table (acoustics-2026-11-14.txt, or the code's copy of it) is the one read; 0 (as it
   comes): acoustics.txt. uwacmat_write_default also writes the earlier one when it is not there, and gives a player's
   acoustics.txt that is still the earlier table word for word the new one (the old stays as the earlier file). */
void uwacmat_earlier(int on);

#endif
