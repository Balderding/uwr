"""One-off patch: talking to creatures (conversations) in game.c / game.h / main.c."""
import sys

ROOT = sys.argv[1]


def edit(p, reps):
    p = ROOT + '/' + p
    s = open(p, encoding='utf-8').read()
    for a, b in reps:
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)


edit('src/game.h', [
    ("    bool attack;           /* swing at what is under the crosshair */",
     "    bool attack;           /* swing at what is under the crosshair */\n"
     "    int number;            /* 1-9 pressed (a reply in a conversation), else 0 */\n"
     "    bool escape;           /* leave the conversation */"),
])

edit('src/game.c', [
    ('#include "render.h"', '#include "conv.h"\n#include "render.h"'),
    ("    int ndoors, hover_door;", "    int ndoors, hover_door, hover_creature;\n    bool talking;\n    int option_y[CONV_MAX_OPTIONS], option_h[CONV_MAX_OPTIONS], options_x;"),
])

TALK = r'''
/* the creature the view ray passes within reach (living) */
static int creature_at(const float eye[3], const float fwd[3], float reach) {
    int best = -1;
    float bt = reach;
    for (int k = 0; k < g_ncr; k++) {
        Creature *c = &g_cr[k];
        if (c->state >= CS_DYING) continue;
        float p[3] = {c->x - eye[0], c->y + 0.35f - eye[1], c->z - eye[2]};
        float t = p[0] * fwd[0] + p[1] * fwd[1] + p[2] * fwd[2];
        if (t < 0.1f || t > bt) continue;
        float q[3] = {p[0] - fwd[0] * t, p[1] - fwd[1] * t, p[2] - fwd[2] * t};
        if (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > 0.4f * 0.4f) continue;
        bt = t, best = k;
    }
    return best;
}

/* talk: an NPC with a number (its last mobile byte) has its own conversation, the others the one for their
 * kind (256 + kind) */
static void talk(int k) {
    Creature *c = &g_cr[k];
    const Object *o = &G.lv->obj[c->obj];
    int whoami = c->obj < 256 ? o->mobile[18] : 0, n = whoami ? whoami : 256 + c->kind;
    char name[96];
    creature_name(c, name, sizeof name);
    const char *noun = strchr(name, ' ');
    if (!conv_start(G.data, n, noun ? noun + 1 : name, "Avatar")) {
        game_message("The %s does not answer.", noun ? noun + 1 : name);
        return;
    }
    G.talking = true;
}
'''

edit('src/game.c', [
    ("static int creature_cards(Vertex *out, int cap) {", TALK + "\nstatic int creature_cards(Vertex *out, int cap) {"),
    # input while talking
    ("    if (in->inventory) G.open = !G.open;",
     "    if (G.talking) { /* a conversation: replies by number or click, Esc / E leaves when it is over */\n"
     "        const Conv *cv = conv_state();\n"
     "        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;\n"
     "        int pick = in->number - 1;\n"
     "        if (in->click_left)\n"
     "            for (int i = 0; i < cv->noptions; i++)\n"
     "                if (in->mouse_y >= G.option_y[i] && in->mouse_y < G.option_y[i] + G.option_h[i]) pick = i;\n"
     "        if (cv->waiting && pick >= 0 && pick < cv->noptions) conv_choose(pick);\n"
     "        else if (in->escape || (!cv->running && (in->use || in->click_left))) conv_end(), G.talking = false;\n"
     "        return true;\n"
     "    }\n"
     "    if (in->inventory) G.open = !G.open;"),
    # E on a friendly creature talks
    ("        pick_door(eye, fwd, &best);\n        if (G.hover_door >= 0) G.hover_obj = -1;",
     "        pick_door(eye, fwd, &best);\n        if (G.hover_door >= 0) G.hover_obj = -1;\n"
     "        G.hover_creature = creature_at(eye, fwd, MIN(best + 0.05f, REACH));\n"
     "        if (G.hover_creature >= 0) G.hover_obj = -1, G.hover_door = -1;\n"
     "        if (in->use && G.hover_creature >= 0) {\n"
     "            if (!g_cr[G.hover_creature].hostile) talk(G.hover_creature);\n"
     "            else game_message(\"It is in no mood to talk.\");\n"
     "            return G.talking;\n"
     "        }"),
    # the hover text for creatures, and the conversation panel
    ("    if (!G.open) { /* the crosshair */",
     "    if (G.talking) {\n"
     "        const Conv *cv = conv_state();\n"
     "        int pw = MIN(o->w - 20, 360), px = (o->w - pw) / 2, py = o->h / 2 - 10, ph = o->h - py - 50;\n"
     "        ui_fill(px - 6, py - 16, pw + 12, ph + 22, 18, 12, 8, 225);\n"
     "        ui_text(FONT_NORMAL, cv->who, px, py - 12, 240, 200, 120);\n"
     "        /* the newest words that fit above the replies */\n"
     "        int opts_h = 0;\n"
     "        for (int i = 0; i < cv->noptions; i++) opts_h += ui_text_wrap(FONT_NORMAL, cv->options[i], 0, 0, pw - 14, 0, 0, 0, false) + 2;\n"
     "        int room = ph - opts_h - 8, first = cv->nlines, used = 0;\n"
     "        while (first > 0) {\n"
     "            int h = ui_text_wrap(FONT_NORMAL, cv->lines[first - 1], 0, 0, pw, 0, 0, 0, false) + 3;\n"
     "            if (used + h > room) break;\n"
     "            used += h, first--;\n"
     "        }\n"
     "        int y = py;\n"
     "        for (int i = first; i < cv->nlines; i++) {\n"
     "            bool mine = cv->lines[i][0] == '>';\n"
     "            y += ui_text_wrap(FONT_NORMAL, cv->lines[i], px, y, pw, mine ? 170 : 235, mine ? 170 : 225, mine ? 150 : 200, true) + 3;\n"
     "        }\n"
     "        y = py + ph - opts_h;\n"
     "        for (int i = 0; i < cv->noptions; i++) {\n"
     "            char num[8];\n"
     "            snprintf(num, sizeof num, \"%d.\", i + 1);\n"
     "            bool hov = G.mouse_y >= y && G.mouse_y < y + ui_font_height(FONT_NORMAL) + 2;\n"
     "            ui_text(FONT_NORMAL, num, px, y, 255, 200, 90);\n"
     "            int h = ui_text_wrap(FONT_NORMAL, cv->options[i], px + 14, y, pw - 14, hov ? 255 : 230, hov ? 240 : 190, hov ? 160 : 90, true);\n"
     "            G.option_y[i] = y, G.option_h[i] = h + 2;\n"
     "            y += h + 2;\n"
     "        }\n"
     "        if (!cv->running) ui_text(FONT_SMALL, \"E OR ESC TO LEAVE\", px + pw - ui_text_width(FONT_SMALL, \"E OR ESC TO LEAVE\"), py + ph, 200, 200, 200);\n"
     "        const Pic *cur = ui_pic(\"ui\\\\cursors\", 10);\n"
     "        if (cur) ui_draw(cur, G.mouse_x - cur->w / 2, G.mouse_y - cur->h / 2, 1);\n"
     "        return;\n"
     "    }\n"
     "    if (!G.open) { /* the crosshair */"),
    ("        if (G.hover_door >= 0) {\n            const Door *d = &G.doors[G.hover_door];",
     "        if (G.hover_creature >= 0) {\n"
     "            char name[96];\n"
     "            creature_name(&g_cr[G.hover_creature], name, sizeof name);\n"
     "            ui_text(FONT_NORMAL, name, o->w / 2 - ui_text_width(FONT_NORMAL, name) / 2, o->h / 2 + 10, 255, 230, 160);\n"
     "            if (!g_cr[G.hover_creature].hostile)\n"
     "                ui_text(FONT_SMALL, \"E - TALK\", o->w / 2 - ui_text_width(FONT_SMALL, \"E - TALK\") / 2, o->h / 2 + 20, 200, 200, 200);\n"
     "        }\n"
     "        if (G.hover_door >= 0) {\n            const Door *d = &G.doors[G.hover_door];"),
])

edit('src/game.c', [
    ("static int g_ncr; /* (the creatures: below) */",
     "static int g_ncr; /* (the creatures: below) */\nstatic void creature_name_fwd(int k, char *out, size_t n);"),
])

edit('src/main.c', [
    ("            gi.click_left = g_click_l, gi.click_right = g_click_r, gi.attack = g_attack;",
     "            gi.click_left = g_click_l, gi.click_right = g_click_r, gi.attack = g_attack;\n"
     "            gi.escape = g_inventory && (GetAsyncKeyState(VK_ESCAPE) & 1);\n"
     "            for (int d = 1; d <= 9 && g_focus; d++)\n"
     "                if (GetAsyncKeyState('0' + d) & 1) gi.number = d;"),
])
print('ok')
