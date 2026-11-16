/* uwinv_draw.c: our inventory panel (Tab in our full-screen display), the original's right-hand panel made tall (the
   user's picture, work\tabpanel_ref.jpg): its top is the original's own panel as the game draws it now (the figure, the
   worn armour, the shoulder, hand and ring holes with what is in them, their numbers, the weight stone), copied from the
   original's screen; below it, instead of the original's eight holes, our inventory in the original's art, Bagnon-like:
   the pack's eight holes, then every bag the player has opened in our grid, its contents in rows of four of the
   original's round holes, then the bag bar (every bag carried; a right click opens or closes one in the grid), the
   name of what the pointer is on in gold, and the character sheet's check box. The hole art and the leather are the
   original's main screen's (data\ui\screens\MAIN.png): one row of its pack holes is repeated for every row.

   What it shows is the original's own data (port\win32\uwinv.c's snapshot). The clicks are the backend's: this returns
   what the pointer is on: a display position of the original's panel (1..11 the body's holes, from the top), a thing
   in the grid (UWI_HIT_NODE + its node), a bag in the bag bar or an empty hole of a bag's rows (UWI_HIT_BAGBAR + the
   bag's node), an empty hole of the pack's rows (12..19, the pack's display positions), 0 the panel elsewhere, -2 the
   check box, -1 off the panel (the world). */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "ui.h"
#include "../port/win32/uwinv.h"

const unsigned *uwv_screen(void);        /* uwview.c: the original's 320 x 200 screen, the last frame's */
extern int uwv_sheet_full;               /* uwview.c, settings.txt char_sheet: 1 the full sheet (default), 0 the game's page */

/* the original's right-hand panel on its screen (the main screen's art): its left edge, top, width; its bottom */
#define OX 237
#define OY 7
#define OW 82
#define OBOT 121
#define CELLS 4
#define SW 83                                                   /* the original's statistics page */
#define SH (66 + 20 * (ui_font_height(FONT_SMALL) + 2) + 3 + 18)

static void capital(char *d, const char *s, size_t n)
{
    snprintf(d, n, "%s", s);
    if (d[0] >= 'a' && d[0] <= 'z') d[0] = (char)(d[0] - 32);
}

/* a thing in a hole of w x h: its picture, centred, and its count */
static void thing_in(const UwiThing *t, int x, int y, int w, int h)
{
    const Pic *pic;
    if (t->item < 0) return;
    pic = ui_pic("sprites\\objects", t->item);
    if (pic) ui_draw(pic, x + (w - pic->w) / 2, y + (h - pic->h) / 2, 1);
    if (t->qty > 1) {
        char q[12];
        snprintf(q, sizeof q, "%d", t->qty);
        ui_text(FONT_NORMAL, q, x + w - 1 - ui_text_width(FONT_NORMAL, q), y + h - 7, 255, 230, 120);
    }
}

/* the old version's tooltip (inventory.c's tip_build / tip_draw): lines on the parchment, each its own colour, beside
   the pointer (to its left: the panel is at the right); with what it would replace in its body slot (worn: the thing
   worn there now), "If you equip this:" and the differences, green up and red down */
typedef struct { char text[64]; u8 r, g, b; } TipLine;
static int tip_n;
static TipLine tip_l[16];
static void tip_add(u8 r, u8 g, u8 b, const char *fmt, ...)
{
    va_list ap;
    if (tip_n >= 16) return;
    va_start(ap, fmt);
    vsnprintf(tip_l[tip_n].text, sizeof tip_l[tip_n].text, fmt, ap);
    va_end(ap);
    tip_l[tip_n].r = r, tip_l[tip_n].g = g, tip_l[tip_n].b = b, tip_n++;
}
static void tip_of(const UwiThing *t, const UwiThing *worn, int body, int mx, int my)   /* body: it is worn now */
{
    static const char *const KIND[3] = {"slash", "bash", "stab"};
    char l[64];
    int k, w = 0, h, lh = ui_font_height(FONT_NORMAL) + 1, x, y;
    tip_n = 0;
    if (t->qty > 1) snprintf(l, sizeof l, "%d %s", t->qty, t->name);
    else capital(l, t->name, sizeof l);
    tip_add(UI_INK_HEAD, "%s", l);
    if (t->cond[0]) capital(l, t->cond, sizeof l), tip_add(UI_INK, "%s condition", l);
    if (t->magic) tip_add(40, 60, 150, "Enchanted");
    if (t->kind == 1) tip_add(UI_INK, "Protection %d", t->prot);
    if (t->kind == 2) tip_add(UI_INK, "Damage: slash %d  bash %d  stab %d", t->dmg[0], t->dmg[1], t->dmg[2]);
    tip_add(UI_INK_FAINT, "Weight %d.%d stones", t->weight / 10, t->weight % 10);
    if (t->worth) tip_add(UI_INK_FAINT, "Worth %d gold", t->worth);
    if (worn && worn->item >= 0 && worn->kind == t->kind && t->kind) {
        int any = 0;
        for (k = 0; k < (t->kind == 2 ? 3 : 1); k++) {
            int d = t->kind == 2 ? t->dmg[k] - worn->dmg[k] : t->prot - worn->prot;
            if (!d) continue;
            if (!any) tip_add(UI_INK, "If you equip this:"), any = 1;
            tip_add(d > 0 ? 20 : 150, d > 0 ? 110 : 20, d > 0 ? 20 : 20, "%+d %s", d, t->kind == 2 ? KIND[k] : "protection");
        }
    } else if (!body && t->kind && t->wear > 0 && (!worn || worn->item < 0)) tip_add(20, 110, 20, "Can be worn: the slot is free");
    for (k = 0; k < tip_n; k++)
        if (ui_text_width(FONT_NORMAL, tip_l[k].text) > w) w = ui_text_width(FONT_NORMAL, tip_l[k].text);
    w += 4, h = tip_n * lh + 4;   /* (margins of one overlay pixel inside the frame: the user, 2026-11-14) */
    x = mx - w - 8, y = my - h / 2;
    if (x < 2) x = mx + 12;
    if (y < 2) y = 2;
    if (y + h > ui_overlay()->h - 2) y = ui_overlay()->h - 2 - h;
    ui_parchment(x, y, w, h);
    for (k = 0; k < tip_n; k++) ui_text(FONT_NORMAL, tip_l[k].text, x + 2, y + 2 + k * lh, tip_l[k].r, tip_l[k].g, tip_l[k].b);
}
/* an empty body hole's name, on the hover (the user: only then, nothing written in the holes) */
static const char *slot_name(int d)
{
    static const char *const N[12] = {"", "Legs", "Head", "Chest", "Hands", "Feet", "Right shoulder", "Left shoulder",
                                      "Right hand", "Left hand", "Right ring", "Left ring"};
    return d >= 1 && d <= 11 ? N[d] : "";
}

/* the character sheet on the original's own statistics page (PANELS.GR picture 2, data\ui\panels\002.png, 83 x 114):
   its top as it is (the leather and its STR DEX INT VIT MANA EXP, labels at rows 21, 28 ... 56) with the name and class
   above and the values beside the labels; its dark skills box (rows 63..104) made tall enough for all twenty skills,
   its middle rows (66..90) repeated; its bottom edge (rows 110..113) under it */
static void sheet_draw(const UwiSnap *s, int x, int y)
{
    const Pic *p = ui_pic_file("ui\\panels\\002.png");
    int lh = ui_font_height(FONT_SMALL) + 2, k, by, need, yy;
    char a[64];
    if (!p) { ui_parchment(x, y, SW, SH); return; }
    need = 20 * lh + 6;                 /* the box's inside */
    ui_blit(p, 0, 0, 83, 66, x, y);
    for (yy = 0; yy < need - 3; yy += 24) ui_blit(p, 0, 66, 83, need - 3 - yy < 24 ? need - 3 - yy : 24, x, y + 66 + yy);
    by = y + 66 + need - 3;
    ui_blit(p, 0, 91, 83, 14, x, by);
    ui_blit(ui_pic_file("ui\\panels\\000.png"), 0, 110, 83, 4, x, by + 14);   /* the plain page's bottom edge */
    /* the name and the class, gold over the leather */
    snprintf(a, sizeof a, "%s", s->name);
    ui_text(FONT_NORMAL, a, x + 42 - ui_text_width(FONT_NORMAL, a) / 2, y + 5, 232, 190, 40);
    {
        char c[32];
        capital(c, s->cls, sizeof c);
        snprintf(a, sizeof a, "LEVEL %d %s", s->level, c);
        for (k = 0; a[k]; k++) if (a[k] >= 'a' && a[k] <= 'z') a[k] = (char)(a[k] - 32);
        ui_text(FONT_SMALL, a, x + 42 - ui_text_width(FONT_SMALL, a) / 2, y + 13, 207, 207, 223);
    }
    {   /* the values beside the page's own labels */
        int v[6][2] = {{s->attr[0], -1}, {s->attr[1], -1}, {s->attr[2], -1}, {s->health, s->maxhealth}, {s->mana, s->maxmana}, {s->exp, -1}};
        for (k = 0; k < 6; k++) {
            if (v[k][1] >= 0) snprintf(a, sizeof a, "%d/%d", v[k][0], v[k][1]);
            else snprintf(a, sizeof a, "%d", v[k][0]);
            ui_text(FONT_SMALL, a, x + 77 - ui_text_width(FONT_SMALL, a), y + 21 + k * 7, 232, 190, 40);
        }
    }
    for (k = 0; k < 20; k++) {   /* the skills in the box: the name white, the value gold, 0 dim */
        int ky = y + 67 + k * lh;
        char nm[24];
        capital(nm, s->skn[k], sizeof nm);
        for (int i = 0; nm[i]; i++) if (nm[i] >= 'a' && nm[i] <= 'z') nm[i] = (char)(nm[i] - 32);
        ui_text(FONT_SMALL, nm, x + 6, ky, s->skill[k] ? 207 : 140, s->skill[k] ? 207 : 130, s->skill[k] ? 223 : 140);
        snprintf(a, sizeof a, "%d", s->skill[k]);
        ui_text(FONT_SMALL, a, x + 77 - ui_text_width(FONT_SMALL, a), ky, s->skill[k] ? 255 : 150, s->skill[k] ? 220 : 120, s->skill[k] ? 60 : 60);
    }
}

/* the character sheet's tooltips, as our previous version had them (game.c's sheet_tip): what the stat or skill under
   the pointer does by the original's rules, with this character's numbers, and how it grows */
static void sheet_tip_k(const UwiSnap *s, int k, int mx, int my)
{
    static const char *const WHAT[20] = {
        "Fighting with any weapon: half of it is added to every attack score.",
        "A creature's blow is a skill check of its attack score against this: the higher, the more it misses.",
        "Fists: attack score + Unarmed. Damage Unarmed x 2/5 + Strength / 6 + 4.",
        "Attack score with a sword: + Sword.",
        "Attack score with an axe: + Axe.",
        "Attack score with a mace or club: + Mace.",
        "Bows, crossbows and slings: attack score + Missile. A missile flies at Missile x 8 + 192; a Missile check against "
        "10 makes it faster (critical) or slower (fumble), and its damage grows with its speed.",
        "Mana: thy most is (Mana + 1) x Intelligence / 8. Every 3 ticks (about a minute) a Mana check against 10 gives "
        "back 1 or 2 points.",
        "Lore: looking at a thing carried rolls Lore to identify it (the best roll is kept).",
        "Each spell is a check of Casting + 5 against twice its circle: a failure fizzles, a fumble backfires and harms thee.",
        "Traps: finding and disarming traps.",
        "A hidden thing set to be found by looking: a Search check against its difficulty finds it.",
        "Track: every creature within 8 tiles gets a Track check; thou art told the direction where the most are.",
        "Creatures hear thee by 13 - Sneak / 3 and see thee by 15 - Sneak / 5: the higher, the less they notice.",
        "Use an anvil on a weapon or armour: a Repair check against its durability; success mends Repair / 5 + 3, a "
        "critical mends it fully.",
        "Charm: bartering.",
        "Use a lockpick on a lock: a Picklock check against 3 x the lock's difficulty. Some locks cannot be picked.",
        "A fall: an Acrobat check against twice its harm; on success only (30 - Acrobat) / 30 of it is taken.",
        "Appraise: bartering, the worth of things.",
        "In water, every tick a Swimming check against thy load (carried / most x 32): failures tire thee; too tired and "
        "thou dost drown.",
    };
    static const unsigned char NEXT[16] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192};
    static const char *const SKN[7] = {"", "", "Unarmed", "Sword", "Axe", "Mace", "Missile"};
    int n = 0;
    int str = s->attr[0], dex = s->attr[1], intl = s->attr[2];
    char t[64] = "", b[1200];
#define ADD(...) (n += snprintf(b + n, sizeof b - (size_t)(n < (int)sizeof b ? n : (int)sizeof b - 1), __VA_ARGS__), n = n < (int)sizeof b ? n : (int)sizeof b - 1)
    b[0] = 0;
    (void)SKN;
    if (k >= 0 && k < 20) {
        static const char *const AT[3] = {"Strength", "Intelligence", "Dexterity"};
        int ai = k < 7 ? 0 : k < 10 ? 1 : 2, av = ai == 0 ? str : ai == 1 ? intl : dex;
        char nm[24];
        capital(nm, s->skn[k], sizeof nm);
        snprintf(t, sizeof t, "%s %d", nm, s->skill[k]);
        ADD("%s\n", WHAT[k]);
        if (k == 0) ADD("Thy attack score now: %d (with fists).\n", s->skill[0] / 2 + s->skill[2] + dex / 7);
        if (k >= 2 && k <= 5) ADD("Thy attack score with it: %d.\n", s->skill[0] / 2 + s->skill[k] + dex / 7);
        if (k == 7) ADD("Thy most mana now: %d.\n", (s->skill[7] + 1) * intl / 8);
        ADD("Raised at a shrine by chanting its mantra (a skill point): two tries, each +1, +1 more ");
        if (ai) ADD("while below half thy %s, +1 more ", AT[ai]);
        ADD("by chance (likelier the further below %s it is). It stops at 30, or once above twice thy %s (%d).", AT[ai], AT[ai], 2 * av);
    } else switch (k) {
    case -1:
        snprintf(t, sizeof t, "%s", s->name);
        ADD("Thy class gave thy starting attributes and the skills chosen at the start.");
        break;
    case -2:
        snprintf(t, sizeof t, "Level %d", s->level);
        ADD("Each level adds a skill point and raises thy most vitality (30 + level x Strength / 5).\n");
        ADD("Spells up to circle (level + 1) / 2 can be cast: circle %d now.\n", (s->level + 1) / 2);
        if (s->level >= 0 && s->level < 16) ADD("Next level at %d experience.", NEXT[s->level] * 50);
        else ADD("The highest level.");
        break;
    case -10:
        snprintf(t, sizeof t, "Strength %d", str);
        ADD("Most vitality: 30 + level x Strength / 5 (%d).\n", 30 + s->level * str / 5);
        ADD("Damage: + Strength / 9 to weapon blows (+%d); fists + Strength / 6.\n", str / 9);
        ADD("Carry limit: Strength x 2 stones (%d).\n", str * 2);
        ADD("Every 24 ticks (about 8 minutes) a Strength check against 15 heals a point.\n");
        ADD("Governs Attack, Defense, Unarmed, Sword, Axe, Mace and Missile.");
        break;
    case -11:
        snprintf(t, sizeof t, "Dexterity %d", dex);
        ADD("Attack score: + Dexterity / 7 (+%d).\n", dex / 7);
        ADD("Governs Traps, Search, Track, Sneak, Repair, Charm, Picklock, Acrobat, Appraise and Swimming.");
        break;
    case -12:
        snprintf(t, sizeof t, "Intelligence %d", intl);
        ADD("Most mana: (Mana skill + 1) x Intelligence / 8 (%d).\n", (s->skill[7] + 1) * intl / 8);
        ADD("Governs Mana, Lore and Casting.");
        break;
    case -13:
        snprintf(t, sizeof t, "Vitality %d of %d", s->health, s->maxhealth);
        ADD("Thy life: at nothing thou art dead. Most: 30 + level x Strength / 5.\n");
        ADD("Heals a point on a Strength check against 15 every 24 ticks (about 8 minutes), and by sixteenths of the most "
            "while sleeping. Poison takes its strength every 3 ticks.");
        break;
    case -14:
        snprintf(t, sizeof t, "Mana %d of %d", s->mana, s->maxmana);
        ADD("Spent casting: 3 x the spell's circle.\nMost: (Mana skill + 1) x Intelligence / 8.\n");
        ADD("Every 3 ticks (about a minute) a Mana skill check against 10 gives back 1 or 2; sleeping gives back sixteenths "
            "of the most.");
        break;
    case -15:
        snprintf(t, sizeof t, "Experience %d", s->exp);
        ADD("Gained by slaying creatures: 0.4 x the creature's worth + 2 dice of it, tenths; a powerful one 1.5 to 3 times "
            "that. Death takes an eighth.\nHalved when thy level is above twice the dungeon level + 2.\n");
        ADD("Every 300 gives a skill point.");
        if (s->level >= 0 && s->level < 16) ADD(" Next level at %d.", NEXT[s->level] * 50);
        break;
    /* the full sheet's own rows */
    case -20:
        snprintf(t, sizeof t, "Skill points %d", s->skill_points);
        ADD("Spent at a shrine: use the shrine and chant a skill's mantra to raise that skill.\n");
        ADD("One for every level and for every 300 experience.");
        break;
    case -21:
        snprintf(t, sizeof t, "Hunger %d, fatigue %d", s->hunger, s->fatigue);
        ADD("The game's own words for them (a click on the compass prints the same).\n");
        ADD("Eat to keep hunger up; sleep to rest. Starving or exhausted, thou dost weaken.");
        if (s->poison) ADD("\nPoisoned: strength %d. Poison takes vitality every 3 ticks until it wears off or is cured.", s->poison);
        break;
    case -22:
        snprintf(t, sizeof t, "Carrying %d.%d of %d.%d stones", s->weight / 10, s->weight % 10, s->max_weight / 10, s->max_weight % 10);
        ADD("The most thou canst carry: Strength x 2 stones.\nThe load also makes swimming harder.");
        break;
    case -23:
        snprintf(t, sizeof t, "Armour");
        ADD("By where a blow lands: body %d, arms %d, legs %d, head %d.\n", s->armour[0], s->armour[1], s->armour[2], s->armour[3]);
        ADD("Each piece worn gives its protection x its condition / 64 + 1; a shield counts on body and arms. "
            "Protection spells add to all four.");
        break;
    case -24:
        if (s->attack < 0) {
            snprintf(t, sizeof t, "Missile %d", s->skill[6]);
            ADD("A missile weapon: its shots fly at Missile x 8 + 192; a Missile check makes a shot faster or slower, and "
                "its damage grows with its speed.\nThe launcher's damage: %d.", s->dmg[0]);
        } else {
            snprintf(t, sizeof t, "Attack score %d", s->attack);
            ADD("Each blow is a check of this against the creature's defence.\n");
            ADD("Attack / 2 (%d) + %s (%d) + Dexterity / 7 (%d)%s.", s->skill[0] / 2, SKN[s->atk_skill], s->skill[s->atk_skill],
                dex / 7, s->easy ? " + 7 (easy)" : "");
        }
        break;
    case -25:
        snprintf(t, sizeof t, "Defense %d", s->defence);
        ADD("A creature's blow is a check of its attack score against this: the higher, the more it misses.\n");
        ADD("The Defense skill (%d), with half the skill of the weapon in hand added.", s->skill[1]);
        break;
    case -26:
        snprintf(t, sizeof t, "Damage");
        if (s->attack < 0) ADD("The launcher's damage: %d.", s->dmg[0]);
        else if (s->atk_skill == 2) ADD("Fists: Unarmed x 2/5 + Strength / 6 + 4 = %d, by the blow's charge.", s->dmg[0]);
        else ADD("A slash %d, a bash %d, a stab %d: the weapon's own, + Strength / 9 (%d). The charge of the blow scales it.",
                 s->dmg[0], s->dmg[1], s->dmg[2], str / 9);
        break;
    case -27:
        snprintf(t, sizeof t, "%s", s->drawn ? "Fight mode" : "Weapon put away");
        ADD("C (or a click on the weapon hand) draws the weapon or puts it away.");
        break;
    case -28:
        snprintf(t, sizeof t, "Where and when");
        ADD("The level of the Abyss thou art on, the day of thy imprisonment and the time of day, as the game says them "
            "when the compass is clicked.");
        break;
    default: return;
    }
#undef ADD
    ui_tip(mx, my, t, b, 230);
}

/* the game's page: what its rows are at the pointer */
static void sheet_tip(const UwiSnap *s, int x, int y, int mx, int my)
{
    int lh = ui_font_height(FONT_SMALL) + 2, k = -100;
    if (mx < x || mx >= x + SW) return;
    if (my >= y + 3 && my < y + 13) k = -1;                          /* the name */
    else if (my >= y + 13 && my < y + 21) k = -2;                    /* the level and class */
    else if (my >= y + 21 && my < y + 63) k = -10 - (my - y - 21) / 7;   /* STR DEX INT VIT MANA EXP */
    else if (my >= y + 67 && my < y + 67 + 20 * lh) k = (my - y - 67) / lh;
    if (k != -100) sheet_tip_k(s, k, mx, my);
}

/* the full sheet (the user, 2026-11-14: our first engine's character panel, src\game.c sheet_draw, "it had way more
   information"; the game's page stays the Esc menu's other choice): parchment, the numbers the rules use in the
   game's words, each piece with its tooltip. FW wide; its height from the rows */
#define FW 120   /* (half the width it was: the user, 2026-11-14, "the character panel could be half the size it is now") */
static int full_hot;              /* what the pointer is on (sheet_tip_k's key), -100 nothing */
static int fmx, fmy;              /* the pointer, overlay pixels */
static int full_h(void) { int lh = ui_font_height(FONT_NORMAL) + 1; return 11 * lh + 10 * lh + 7; }   /* (tight: less wasted room) */
/* a piece of text; under the pointer a darker paper behind it and its key kept for the tooltip */
static void seg(int *x, int y, const char *txt, u8 r, u8 g, u8 b, int key)
{
    int w = ui_text_width(FONT_NORMAL, txt), lh = ui_font_height(FONT_NORMAL) + 1;
    if (fmx >= *x - 1 && fmx < *x + w + 1 && fmy >= y && fmy < y + lh) full_hot = key, ui_fill(*x - 1, y, w + 2, lh, UI_PAPER_MID, 255);
    ui_text(FONT_NORMAL, txt, *x, y, r, g, b);
    *x += w + 4;
}
static void sheet_full_draw(const UwiSnap *s, int x, int y, int mx, int my)
{
    static const char *const SKN[7] = {"", "", "fists", "Sword", "Axe", "Mace", "Missile"};
    int lh = ui_font_height(FONT_NORMAL) + 1, cx = x + 2, cy = y + 2, sx, k;   /* (margins of one overlay pixel inside the frame) */
    char a[96], b[64], c[64];
    fmx = mx, fmy = my, full_hot = -100;
    ui_parchment(x, y, FW, full_h());
    {   /* the face (HEADS.GR: 0-4 male, 5-9 female) */
        const Pic *face = ui_pic("ui\\heads", (s->female ? 5 : 0) + (s->body % 5));
        if (face) ui_draw(face, x + FW - face->w - 2, y + 2, 1);
    }
    capital(c, s->cls, sizeof c);
    snprintf(a, sizeof a, "%s the %s", s->name, c);
    snprintf(b, sizeof b, "level %d", s->level);
    sx = cx, seg(&sx, cy, a, UI_INK_HEAD, -1), seg(&sx, cy, b, UI_INK_HEAD, -2), cy += lh;
    snprintf(a, sizeof a, "Experience %d", s->exp);
    snprintf(b, sizeof b, "Skill points %d", s->skill_points);
    sx = cx, seg(&sx, cy, a, UI_INK, -15), seg(&sx, cy, b, UI_INK, -20), cy += lh;
    snprintf(a, sizeof a, "Strength %d", s->attr[0]);
    snprintf(b, sizeof b, "Dexterity %d", s->attr[1]);
    snprintf(c, sizeof c, "Intelligence %d", s->attr[2]);
    sx = cx, seg(&sx, cy, a, UI_INK, -10), seg(&sx, cy, b, UI_INK, -11), cy += lh;
    sx = cx, seg(&sx, cy, c, UI_INK, -12), cy += lh;
    snprintf(a, sizeof a, "Vitality %d/%d", s->health, s->maxhealth);
    snprintf(b, sizeof b, "Mana %d/%d", s->mana, s->maxmana);
    sx = cx, seg(&sx, cy, a, UI_INK_RED, -13), seg(&sx, cy, b, UI_INK_BLUE, -14), cy += lh;
    snprintf(a, sizeof a, "You are %s and %s%s.", s->w_hunger, s->w_fatigue, s->poison ? ", poisoned" : "");
    sx = cx, seg(&sx, cy, a, s->poison ? 22 : 88, s->poison ? 84 : 62, s->poison ? 18 : 48, -21)   /* (poisoned: green, never red) */, cy += lh;
    snprintf(a, sizeof a, "Carrying %d.%d of %d.%d stones", s->weight / 10, s->weight % 10, s->max_weight / 10, s->max_weight % 10);
    sx = cx, seg(&sx, cy, a, UI_INK_FAINT, -22), cy += lh;
    snprintf(a, sizeof a, "Armour: body %d  arms %d  legs %d  head %d", s->armour[0], s->armour[1], s->armour[2], s->armour[3]);
    sx = cx, seg(&sx, cy, a, UI_INK_BLUE, -23), cy += lh;
    if (s->attack < 0) snprintf(a, sizeof a, "Missile %d", s->skill[6]);
    else snprintf(a, sizeof a, "Attack %d (%s)", s->attack, SKN[s->atk_skill]);
    snprintf(b, sizeof b, "Defense %d", s->defence);
    sx = cx, seg(&sx, cy, a, UI_INK_BLUE, -24), seg(&sx, cy, b, UI_INK_BLUE, -25), cy += lh;
    if (s->attack < 0) snprintf(a, sizeof a, "Damage %d", s->dmg[0]);
    else if (s->atk_skill == 2) snprintf(a, sizeof a, "Damage %d", s->dmg[0]);
    else snprintf(a, sizeof a, "Damage %d / %d / %d", s->dmg[0], s->dmg[1], s->dmg[2]);
    sx = cx, seg(&sx, cy, a, UI_INK_BLUE, -26);
    seg(&sx, cy, s->drawn ? "Fight mode (C)" : "Weapon away (C)", UI_INK_GOLD, -27), cy += lh;
    if (s->day >= 0) snprintf(a, sizeof a, "The %s level, the %s day, %s", s->w_level, s->w_day, s->w_period);
    else snprintf(a, sizeof a, "The %s level, %s", s->w_level, s->w_period);
    sx = cx, seg(&sx, cy, a, UI_INK_FAINT, -28), cy += lh + 1;
    ui_fill(cx, cy - 1, FW - 4, 1, UI_PAPER_DARK, 255);
    cy += 1;
    for (k = 0; k < 20; k++) {   /* two columns of ten; a whole row is the skill's */
        int col = k / 10, row = k % 10, kx = cx + col * ((FW - 4) / 2 + 2), ky = cy + row * lh, kw = (FW - 4) / 2 - 3;
        char nm[24];
        if (fmx >= kx - 1 && fmx < kx + kw + 1 && fmy >= ky && fmy < ky + lh) full_hot = k, ui_fill(kx - 1, ky, kw + 2, lh, UI_PAPER_MID, 255);
        capital(nm, s->skn[k], sizeof nm);
        ui_text(FONT_NORMAL, nm, kx, ky, UI_INK);
        snprintf(a, sizeof a, "%d", s->skill[k]);
        ui_text(FONT_NORMAL, a, kx + kw - ui_text_width(FONT_NORMAL, a), ky, s->skill[k] ? 16 : 88, s->skill[k] ? 40 : 62, s->skill[k] ? 116 : 48)   /* (navy, not red) */;
    }
}

/* ---- the art: rows of the main screen's picture (MAIN.png) or of the original's live screen, onto the overlay ---- */
static void blit_pic(const Pic *p, int sx, int sy, int w, int h, int dx, int dy)
{
    Overlay *o = ui_overlay();
    for (int y = 0; y < h; y++) {
        int ty = dy + y, py = sy + y;
        if (ty < 0 || ty >= o->h || py < 0 || py >= p->h) continue;
        for (int x = 0; x < w; x++) {
            int tx = dx + x, px = sx + x;
            if (tx < 0 || tx >= o->w || px < 0 || px >= p->w) continue;
            const u8 *c = p->rgba + ((size_t)py * p->w + px) * 4;
            u8 *d = o->px + ((size_t)ty * o->w + tx) * 4;
            d[0] = c[0], d[1] = c[1], d[2] = c[2], d[3] = 255;
        }
    }
}
static void blit_screen(const unsigned *scr, int sx, int sy, int w, int h, int dx, int dy)
{
    Overlay *o = ui_overlay();
    for (int y = 0; y < h; y++) {
        int ty = dy + y, py = sy + y;
        if (ty < 0 || ty >= o->h || py < 0 || py >= 200) continue;
        for (int x = 0; x < w; x++) {
            int tx = dx + x, px = sx + x;
            if (tx < 0 || tx >= o->w || px < 0 || px >= 320) continue;
            unsigned c = scr[py * 320 + px];
            u8 *d = o->px + ((size_t)ty * o->w + tx) * 4;
            d[0] = (u8)(c >> 16), d[1] = (u8)(c >> 8), d[2] = (u8)c, d[3] = 255;
        }
    }
}

/* the top as the original last drew it on the inventory page: kept, so that the character stays shown while the
   original's panel is on the rune page (which our own panel beside it shows instead, the user's) */
static unsigned topc[OW * 200];
static int topc_ok, topc_h;
#define TOPN 8
static unsigned topr[TOPN][OW * 200];   /* the last TOPN tops as drawn; topc the one 6 frames old */
static int topr_n;
static void top_keep(int h, int dx, int dy)
{
    Overlay *o = ui_overlay();
    unsigned *r = topr[topr_n % TOPN];
    for (int y = 0; y < h && y < 200; y++)
        for (int x = 0; x < OW; x++) {
            int tx = dx + x, ty = dy + y;
            const u8 *p = tx >= 0 && tx < o->w && ty >= 0 && ty < o->h ? o->px + ((size_t)ty * o->w + tx) * 4 : NULL;
            r[y * OW + x] = p ? (unsigned)p[0] << 16 | (unsigned)p[1] << 8 | p[2] : 0;
        }
    topr_n++;
    if (topr_n >= 6) memcpy(topc, topr[(topr_n - 6) % TOPN], sizeof topc), topc_ok = 1, topc_h = h;
}
#define RUNE_H 114          /* our rune page: the original's page art, its rune grid and the shelf strip */
#define RUNE_X0 6           /* the grid's first cell in the page (ShowRune: x 0xF4 on the screen, the page at 0xED) */
#define RUNE_Y0 4
static void blit_cache(int h, int dx, int dy)
{
    Overlay *o = ui_overlay();
    for (int y = 0; y < h && y < topc_h; y++)
        for (int x = 0; x < OW; x++) {
            int tx = dx + x, ty = dy + y;
            if (tx < 0 || tx >= o->w || ty < 0 || ty >= o->h) continue;
            unsigned c = topc[y * OW + x];
            u8 *d = o->px + ((size_t)ty * o->w + tx) * 4;
            d[0] = (u8)(c >> 16), d[1] = (u8)(c >> 8), d[2] = (u8)c, d[3] = 255;
        }
}

/* ---- the layout ---- */
typedef struct {
    int x, y, w, h;      /* overlay pixels */
    int hit;             /* what it returns */
    int node;            /* the thing in it (a node), -1 none */
    int dim;             /* a closed bag in the bag bar */
} Hole;
enum { R_HOLES, R_PLAIN };
typedef struct {
    int kind, y;         /* a row of holes (MAIN.png's pack row) or of plain leather */
} Row;
static struct {
    int px, py, ph;      /* the panel */
    int top_h;           /* the original's top: OY .. OY + top_h */
    int band_y, band_h;  /* MAIN.png's rows for a row of holes */
    int plain_y, plain_h;/* ... for plain leather */
    int bot_y, bot_h;    /* ... the panel's bottom edge */
    int hole_x[CELLS], hole_w, hole_h;  /* a hole within a row */
    Hole hole[160];
    int nholes;
    Row row[40];
    int nrows;
    int name_y, check_x, check_y;
    /* the open bags' rows scrolled when they do not all fit (the user, 2026-11-15: "a slider on the right side like
       the esc menu", the wheel over the panel): bar on, the track's place, the thumb's */
    int bar, tx, ty, tw, th, thumb_y, thumb_h;
} L;
/* the bags' rows: the first shown (bag_top), how many there are and how many fit; the thumb dragged */
static int bag_top, bag_rows, bag_vis, bag_drag, bag_drag_dy;

static int bag_open(const UwiSnap *s, int obj)
{
    for (int i = 0; i < s->nopen; i++)
        if (s->open_bags[i] == obj) return 1;
    return 0;
}

static void layout(const UwiSnap *s)
{
    const Overlay *o = ui_overlay();
    /* the original's pack holes (display positions 12..15 the first row, 16 below 12): their places on its screen */
    const int *a = s->rect[12], *b = s->rect[16];
    int pitch = b[1] - a[1], hole_h = a[3] - a[1] + 1, m, y, k, i, n;
    if (pitch < 12 || pitch > 30) pitch = 18;
    if (hole_h < 8 || hole_h > pitch) hole_h = 15;
    m = (pitch - hole_h) / 2;
    L.band_y = a[1] - m, L.band_h = pitch;
    L.top_h = L.band_y - OY;
    L.plain_y = L.band_y - 14, L.plain_h = 12;            /* the leather just above the pack's rows: no holes */
    L.bot_y = b[1] - m + pitch, L.bot_h = OBOT - L.bot_y; /* below the pack's second row to the panel's edge */
    if (L.bot_h < 2) L.bot_h = 2;
    for (k = 0; k < CELLS; k++) L.hole_x[k] = s->rect[12 + k][0] - OX;
    L.hole_w = a[2] - a[0] + 1, L.hole_h = hole_h;
    L.py = 2;
    L.nholes = 0, L.nrows = 0;
    y = L.py + L.top_h;
    {   /* the open bags' rows: all of them, and how many fit above the name line (else the slider, the panel moved
           left for its track) */
        int total = 0, room = o->h - 60 - (y + 2 * pitch);
        for (i = 0; i < s->nnodes; i++) {
            if (!s->node[i].bag || !bag_open(s, s->node[i].obj)) continue;
            for (n = 0, k = 0; k < s->nnodes; k++) n += s->node[k].parent == i;
            total += n ? (n + CELLS - 1) / CELLS : 1;
        }
        bag_vis = room / pitch;
        if (bag_vis < 1) bag_vis = 1;
        if (bag_vis > 30) bag_vis = 30;                    /* (L.row's and L.hole's room) */
        bag_rows = total;
        L.bar = total > bag_vis;
        if (!L.bar) bag_top = 0;
        if (bag_top > total - bag_vis) bag_top = total - bag_vis;
        if (bag_top < 0) bag_top = 0;
    }
    L.px = o->w - OW - 2 - (L.bar ? 8 : 0);
    /* the pack's eight: the original's slots 11..18 */
    for (k = 0; k < 8; k++) {
        Hole *h = &L.hole[L.nholes++];
        if (k % CELLS == 0) L.row[L.nrows++] = (Row){R_HOLES, y + (k / CELLS) * pitch};
        h->x = L.px + L.hole_x[k % CELLS], h->y = y + (k / CELLS) * pitch + m, h->w = L.hole_w, h->h = hole_h;
        h->node = -1, h->dim = 0, h->hit = UWI_HIT_PACK + k;   /* an empty hole: into the pack's slot 11 + k */
        for (i = 0; i < s->nnodes; i++)
            if (s->node[i].parent < 0 && s->node[i].slot == 11 + k) h->node = i, h->hit = UWI_HIT_NODE + i;
    }
    y += 2 * pitch;
    /* each open bag's contents, in the tree's order (a bag's after the bag): only the rows bag_top .. + bag_vis */
    {
        int r0 = 0, shown = 0, y0 = y;
        for (i = 0; i < s->nnodes; i++) {
            if (!s->node[i].bag || !bag_open(s, s->node[i].obj)) continue;
            for (n = 0, k = 0; k < s->nnodes; k++) n += s->node[k].parent == i;
            n = n ? (n + CELLS - 1) / CELLS * CELLS : CELLS;     /* whole rows; an empty bag one row */
            for (int c = 0, kk = 0; c < n; c++) {
                int row = r0 + c / CELLS, cell = -1;
                for (; kk < s->nnodes; kk++)
                    if (s->node[kk].parent == i) { cell = kk++; break; }
                if (row < bag_top || row >= bag_top + bag_vis || L.nrows >= 36 || L.nholes >= 156) continue;
                Hole *h = &L.hole[L.nholes++];
                int ry = y0 + (row - bag_top) * pitch;
                if (c % CELLS == 0) L.row[L.nrows++] = (Row){R_HOLES, ry}, shown++;
                h->x = L.px + L.hole_x[c % CELLS], h->y = ry + m, h->w = L.hole_w, h->h = hole_h;
                h->node = cell, h->dim = 0;
                h->hit = cell >= 0 ? UWI_HIT_NODE + cell : UWI_HIT_INTO + i;   /* an empty hole: into the bag */
            }
            r0 += n / CELLS;
        }
        y += shown * pitch;
        /* the slider: its track right of the bags' rows, the thumb its share of them */
        L.tx = L.px + OW + 2, L.ty = y0, L.tw = 5, L.th = shown * pitch;
        if (L.bar && L.th > 0) {
            L.thumb_h = L.th * bag_vis / bag_rows;
            if (L.thumb_h < 8) L.thumb_h = 8;
            L.thumb_y = L.ty + (bag_rows > bag_vis ? (L.th - L.thumb_h) * bag_top / (bag_rows - bag_vis) : 0);
        } else L.bar = 0;
    }
    /* (no bag bar: a right click on a bag in the grid opens or closes its rows, the user's) */
    /* the name line and the check box */
    L.row[L.nrows++] = (Row){R_PLAIN, y};
    L.name_y = y + 3;
    L.check_x = L.px + OW - 34, L.check_y = y + 3;
    y += L.plain_h;
    L.ph = y + L.bot_h - L.py;
}

/* the wheel over the panel: notches up (positive) show earlier rows of the bags, a row a notch */
void uwi_inv_wheel(int notches)
{
    bag_top -= notches;
    if (bag_top > bag_rows - bag_vis) bag_top = bag_rows - bag_vis;
    if (bag_top < 0) bag_top = 0;
}
/* a left press (down 1) or release (0) at window pixel mx, my: on the thumb it starts a drag, on the track above or
   below it pages; 1 when the slider took it */
int uwi_inv_click(int mx, int my, int down)
{
    const Overlay *o = ui_overlay();
    int x, y;
    if (!down) { int was = bag_drag; bag_drag = 0; return was; }
    if (!o || !L.bar) return 0;
    x = mx / o->scale, y = my / o->scale;
    if (x < L.tx - 2 || x >= L.tx + L.tw + 2 || y < L.ty || y >= L.ty + L.th) return 0;
    if (y >= L.thumb_y && y < L.thumb_y + L.thumb_h) bag_drag = 1, bag_drag_dy = y - L.thumb_y;
    else uwi_inv_wheel(y < L.thumb_y ? bag_vis : -bag_vis);   /* a page */
    return 1;
}

/* Draws the panel (draw) and finds what the window point (mx, my) is on (see the top). */
int uwi_panel(const UwiSnap *s, int mx, int my, int sheet, int draw, float *fx, float *fy)
{
    const Overlay *o = ui_overlay();
    const Pic *main = ui_pic_file("ui\\screens\\MAIN.png");
    const unsigned *scr = uwv_screen();
    int x, y, k, hit = -1, hover = -1, sx, sy;
    if (!o || !s) return -1;
    x = mx / o->scale, y = my / o->scale;
    layout(s);
    if (L.bar && bag_drag) {   /* the thumb dragged: the rows follow the pointer */
        int span = L.th - L.thumb_h;
        int t = span > 0 ? (int)((float)(y - bag_drag_dy - L.ty) / (float)span * (float)(bag_rows - bag_vis) + 0.5f) : 0;
        if (t != bag_top) bag_top = t, layout(s);
    }
    if (L.bar && x >= L.tx - 2 && x < L.tx + L.tw + 2 && y >= L.ty && y < L.ty + L.th) {
        if (!draw) return UWI_HIT_SCROLL;
        hit = UWI_HIT_SCROLL;
    }
    sx = 2, sy = L.py;   /* the character sheet at the screen's left edge (the user) */
    if (hit == UWI_HIT_SCROLL) ;
    else if (x >= L.px && x < L.px + OW && y >= L.py && y < L.py + L.ph) hit = 0;
    else if (sheet && x >= sx && x < sx + (uwv_sheet_full ? FW : SW) && y >= sy && y < sy + (uwv_sheet_full ? full_h() : SH)) hit = 0;
    if (hit == 0 && x >= L.check_x && x < L.check_x + 32 && y >= L.check_y && y < L.check_y + 8) hit = -2;
    if (hit == -1 && s->runes_open) {   /* our rune page beside the panel: a rune's cell (the original's places, four to a
                                            row, An at the top left), the cast mark, the shelf strip */
        int rx = L.px - OW - 3, ry = L.py;
        if (x >= rx && x < rx + OW && y >= ry && y < ry + RUNE_H) {
            int lx = x - rx, ly = y - ry;
            hit = 0;
            if (ly >= RUNE_Y0 && ly < RUNE_Y0 + 6 * 15 && lx >= RUNE_X0 && lx < RUNE_X0 + 4 * 18)
                hit = UWI_HIT_RUNE + ((ly - RUNE_Y0) / 15) * 4 + (lx - RUNE_X0) / 18;
            else if (ly >= RUNE_H - 18) hit = lx >= OW - 30 ? UWI_HIT_CAST : UWI_HIT_SHELF;
        }
    }
    if (hit == 0 && y < L.py + L.top_h) {   /* the original's top: its own display positions there */
        int gx = OX + (x - L.px), gy = OY + (y - L.py);
        for (k = 1; k <= 11; k++)
            if (gx >= s->rect[k][0] && gx <= s->rect[k][2] && gy >= s->rect[k][1] && gy <= s->rect[k][3]) {
                hit = k;
                if (fx) *fx = (float)(gx - s->rect[k][0]) / (float)(s->rect[k][2] - s->rect[k][0] + 1), *fy = (float)(gy - s->rect[k][1]) / (float)(s->rect[k][3] - s->rect[k][1] + 1);
                break;
            }
    }
    if (hit == 0)
        for (k = 0; k < L.nholes; k++) {
            const Hole *h = &L.hole[k];
            if (x >= h->x && x < h->x + h->w && y >= h->y && y < h->y + h->h) {
                hit = h->hit, hover = k;
                if (fx) *fx = (float)(x - h->x) / (float)h->w, *fy = (float)(y - h->y) / (float)h->h;
                break;
            }
        }
    if (!draw) return hit;
    if (sheet && uwv_sheet_full) sheet_full_draw(s, sx, sy, x, y);
    else if (sheet) sheet_draw(s, sx, sy);
    /* the top: the original's panel as the game draws it, when the inventory is on its panel; its art else */
    if (s->runes_open) {   /* our rune page: the original's page art (PANELS.GR 1), the runes the player has in their
                              cells (RUNES.C ShowRune: 18 apart, 15 a row), the shelf's runes in the bottom strip */
        int rx = L.px - OW - 3, ry = L.py, r;
        const Pic *page = ui_pic_file("ui\\panels\\001.png");
        ui_fill(rx - 1, ry - 1, OW + 2, RUNE_H + 2, 30, 20, 14, 255);
        if (page) ui_blit(page, 0, 0, OW, RUNE_H, rx, ry);
        for (r = 0; r < 24; r++) {
            UwiThing t;
            if (!(s->runebag[r >> 3] >> (7 - (r & 7)) & 1)) continue;
            memset(&t, 0, sizeof t), t.item = 0xE8 + r, t.qty = 1;
            thing_in(&t, rx + RUNE_X0 + (r & 3) * 18, ry + RUNE_Y0 + (r >> 2) * 15, 18, 15);
        }
        for (r = 0; r < s->nrunes && r < 3; r++) {
            UwiThing t;
            memset(&t, 0, sizeof t), t.item = 0xE8 + s->shelf[r], t.qty = 1;
            thing_in(&t, rx + 4 + r * 14, ry + RUNE_H - 17, 14, 15);
        }
        if (hit >= UWI_HIT_RUNE && hit < UWI_HIT_RUNE + 24) ui_fill(rx + RUNE_X0 + ((hit - UWI_HIT_RUNE) & 3) * 18, ry + RUNE_Y0 + ((hit - UWI_HIT_RUNE) >> 2) * 15, 18, 15, 255, 230, 120, 50);
    }
    /* the top: live only once the original's panel has shown the inventory for a few frames (the screen can be a frame
       ahead of the snapshot: a page starting to turn, the original's art); else the top as it was 6 frames before
       (TOPN kept, as drawn here: the open bag's positions covered) */
    static int stable;
    stable = s->panel == 0 ? stable + 1 : 0;
    if (scr && (s->panel != 0 || stable < 6) && topc_ok) blit_cache(L.top_h, L.px, L.py);
    else if (scr && s->panel == 0) {
        blit_screen(scr, OX, OY, OW, L.top_h, L.px, L.py);
        /* the original's open bag and its scroll arrows (display positions 20..22) are ours to show (the bag bar):
           where they fall in the top, its plain leather again (MAIN.png has them empty) */
        for (k = 20; k <= 22 && main; k++) {
            int x0 = s->rect[k][0] - 1, y0 = s->rect[k][1] - 1, x1 = s->rect[k][2] + 1, y1 = s->rect[k][3] + 1;
            if (y0 < OY) y0 = OY;
            if (y1 >= OY + L.top_h) y1 = OY + L.top_h - 1;
            if (x0 < OX) x0 = OX;
            if (x1 >= OX + OW) x1 = OX + OW - 1;
            if (x1 >= x0 && y1 >= y0) blit_pic(main, x0, y0, x1 - x0 + 1, y1 - y0 + 1, L.px + x0 - OX, L.py + y0 - OY);
        }
        top_keep(L.top_h, L.px, L.py);
    } else if (main) blit_pic(main, OX, OY, OW, L.top_h, L.px, L.py);
    for (k = 0; k < L.nrows && main; k++) {
        if (L.row[k].kind == R_HOLES) blit_pic(main, OX, L.band_y, OW, L.band_h, L.px, L.row[k].y);
        else blit_pic(main, OX, L.plain_y, OW, L.plain_h, L.px, L.row[k].y);
    }
    if (main) blit_pic(main, OX, L.bot_y, OW, L.bot_h, L.px, L.py + L.ph - L.bot_h);
    if (L.bar) {   /* the slider, as the Esc menu's: a dark track, the thumb gold (brighter under the pointer) */
        int on = bag_drag || hit == UWI_HIT_SCROLL;
        ui_fill(L.tx - 1, L.ty - 1, L.tw + 2, L.th + 2, 30, 20, 14, 255);
        ui_fill(L.tx, L.ty, L.tw, L.th, 40, 28, 16, 200);
        ui_fill(L.tx, L.thumb_y, L.tw, L.thumb_h, on ? 255 : 210, on ? 220 : 170, on ? 60 : 40, 255);
    }
    for (k = 0; k < L.nholes; k++) {
        const Hole *h = &L.hole[k];
        if (h->node < 0) continue;
        {   /* a bag whose rows are shown: its open picture (the original's: item + 1 for the sack to the gold coffer) */
            UwiThing t = s->node[h->node].t;
            if (s->node[h->node].bag && bag_open(s, s->node[h->node].obj) && t.item >= 128 && t.item <= 138 && !(t.item & 1)) t.item++;
            thing_in(&t, h->x, h->y, h->w, h->h);
        }
        if (h->dim) ui_fill(h->x + 1, h->y + 1, h->w - 2, h->h - 2, 40, 24, 16, 150);   /* a closed bag: dimmed */
    }
    {   /* the name of what the pointer is on, in gold (the original's), and the check box */
        const UwiThing *t = NULL;
        if (hover >= 0 && L.hole[hover].node >= 0) t = &s->node[L.hole[hover].node].t;
        else if (hit >= 1 && hit <= 11 && s->t[hit].item >= 0) t = &s->t[hit];
        if (!t && !s->held && hit >= 1 && hit <= 11) {   /* an empty body hole: its name */
            const char *nm = slot_name(hit);
            int w = ui_text_width(FONT_NORMAL, nm) + 4, hh = ui_font_height(FONT_NORMAL) + 4;
            ui_parchment(x - w - 8, y - hh / 2, w, hh);
            ui_text(FONT_NORMAL, nm, x - w - 6, y - hh / 2 + 2, UI_INK);
        }
        if (t && !s->held) ui_text(FONT_NORMAL, t->name, L.px + 4, L.name_y, 232, 190, 40);
        ui_fill(L.check_x, L.check_y, 7, 7, 40, 24, 16, 255), ui_fill(L.check_x + 1, L.check_y + 1, 5, 5, 150, 100, 60, 255);
        if (sheet) ui_fill(L.check_x + 2, L.check_y + 2, 3, 3, 232, 190, 40, 255);
        ui_text(FONT_SMALL, "SHEET", L.check_x + 9, L.check_y + 1, 232, 190, 40);
        if (t && !s->held) {   /* compared with what is worn in its slot (not when it is that) */
            const UwiThing *worn = t->wear > 0 && t->wear < UWI_DISP && &s->t[t->wear] != t ? &s->t[t->wear] : NULL;
            if (worn && worn->item == t->item && worn->weight == t->weight && hit == t->wear) worn = NULL;
            tip_of(t, hit >= 1 && hit <= 11 ? NULL : worn, hit >= 1 && hit <= 11, x, y);
        }
    }
    if (sheet && !s->held) {   /* what a stat or skill does, on hover */
        if (!uwv_sheet_full) sheet_tip(s, sx, sy, x, y);
        else if (full_hot != -100) sheet_tip_k(s, full_hot, x, y);
    }
    if (s->held && s->h.item >= 0) thing_in(&s->h, x - 8, y - 8, 16, 16);
    else ui_cursor(10, x, y);   /* the game's cursor (the host's never shows over our display) */
    return hit;
}
