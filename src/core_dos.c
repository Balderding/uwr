/* core_dos.c: what DOS's memory held where the original reads past the end of an array (the rules core's PATCHES in
 * tools/core.py send such reads here). Compiled like the core (compat.h force-included).
 *
 * res_to_terr (MOTION.C, DS:03FA, 18 bytes) is indexed by a footing bit, 1 << terrain code: OBJPHYS.C reads it with
 * 32 (the player's shore bit; a creature of terrain code 5), 64 and 128 (terrain codes 6 and 7, which some shipped
 * creatures have). DOS reads on into MP (DS:040C, struct MotionParams as DOS lays it out, its two near pointers
 * included) and then the 3D view's constant tables (chgtable DS:0444, headmod 045C, trans_grid 0464), whose bytes are
 * UW.EXE's own. */
#include "uw2.h"
#include "map.h"
#include "motion.h"

static unsigned char dgroup[0x500]; /* UW.EXE's initialised DGROUP from DS:0000 (the constant tables) */

void core_dos_init(const unsigned char *exe, long size)
{
    long img = (long)(exe[8] | exe[9] << 8) * 16, at = img + 0x5AACL * 16;
    for (int i = 0; i < (int)sizeof dgroup; i++) dgroup[i] = at + i < size ? exe[at + i] : 0;
}

/* a physics record's DOS address (PN at DS:2780, then CN1 to CN4, 0x28 bytes each) */
static unsigned phys_ds(void *p)
{
    struct Phys *r[5] = { &PN, &CN1, &CN2, &CN3, &CN4 };
    for (int k = 0; k < 5; k++)
        if ((unsigned char *)p >= (unsigned char *)r[k] && (unsigned char *)p < (unsigned char *)(r[k] + 1))
            return 0x2780 + k * 0x28 + (unsigned)((unsigned char *)p - (unsigned char *)r[k]);
    return 0;
}

/* MP's byte at DOS offset off (its fields as DOS lays them: vel and pos near pointers, then the words) */
static unsigned char mp_byte(unsigned off)
{
    unsigned char b[0x38];
    unsigned vel = phys_ds(MP.vel), pos = (unsigned char *)MP.pos == (unsigned char *)&Ppd.x ? 0x2768 : 0;
    int n = 0;
#define W(v) (b[n++] = (unsigned char)((uint16)(v)), b[n++] = (unsigned char)((uint16)(v) >> 8))
    W(vel); W(pos);
    for (int i = 0; i < 3; i++) W(MP.frac[i]);
    for (int i = 0; i < 3; i++) W(MP.step[i]);
    W(MP.major); W(MP.minor); W(MP.steps); W(MP.rem); W(MP.dt); W(MP.done);
    b[n++] = (unsigned char)MP.hit;
    W(MP.item); W(MP.targz); W(MP.f21); W(MP.f23); W(MP.zspeed);
    for (int i = 0; i < 8; i++) W(MP.headings[i]);
#undef W
    return off < (unsigned)n ? b[off] : 0;
}

unsigned char core_res_to_terr(unsigned i)
{
    unsigned ds = 0x3FA + i;
    if (i < 18) return res_to_terr[i];
    if (ds >= 0x40C && ds < 0x444) return mp_byte(ds - 0x40C);
    return ds < sizeof dgroup ? dgroup[ds] : 0;
}
