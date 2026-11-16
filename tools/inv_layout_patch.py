"""One-off: the inventory board with labelled holes (wider), and equipping by dropping a thing on the figure: it
goes to its slot (armour by the armour table, weapons the right hand, shields the left, lights a shoulder, rings a
ring), and what was there comes onto the pointer."""
import sys

ROOT = sys.argv[1]


def patch(p, reps):
    s = open(ROOT + '/' + p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in s else '\n'
    for a, b in reps:
        a, b = a.replace('\n', nl), b.replace('\n', nl)
        assert s.count(a) == 1, (p, a[:70], s.count(a))
        s = s.replace(a, b)
    open(ROOT + '/' + p, 'w', encoding='utf-8', newline='').write(s)


patch('src/inventory.c', [
    ('#define PW 140\n#define PH 186', '#define PW 176\n#define PH 196'),
    ('''    static const int BX[INV_BODY] = {28, 28, 28, 92, 92, 4, 116, 4, 116, 4, 116};
    static const int BY[INV_BODY] = {8, 32, 56, 8, 32, 8, 8, 32, 32, 56, 56};''',
     '''    /* outer columns: shoulders, hands, rings; inner: helm, chest, gloves | leggings, boots; the figure between */
    static const int BX[INV_BODY] = {36, 36, 36, 120, 120, 4, 152, 4, 152, 4, 152};
    static const int BY[INV_BODY] = {6, 34, 62, 6, 34, 6, 6, 34, 34, 62, 62};'''),
    ('''    else *x = px + 14 + ((h - INV_BODY) % 4) * 29, *y = py + 106 + ((h - INV_BODY) / 4) * 26;''',
     '''    else *x = px + 32 + ((h - INV_BODY) % 4) * 30, *y = py + 118 + ((h - INV_BODY) / 4) * 28;'''),
    ('''    int y = py + 88;''', '''    int y = py + 100;'''),
    ('''    ui_text(FONT_SMALL, title, px + 24, py + 92, 30, 30, 30);''', '''    ui_text(FONT_SMALL, title, px + 24, py + 104, 30, 30, 30);'''),
    ('''    if (body) ui_draw(body, px + (PW - body->w) / 2, py + 8, 1);''', '''    if (body) ui_draw(body, px + (PW - body->w) / 2, py + 6, 1);'''),
    # labels under the body holes
    ('''        int p = thing_at(h);
        if (p) draw_thing(p, x, y);''', '''        int p = thing_at(h);
        if (p) draw_thing(p, x, y);
        if (h < INV_BODY) ui_text(FONT_SMALL, SHORT[h], x + HOLE / 2 - ui_text_width(FONT_SMALL, SHORT[h]) / 2, y + HOLE + 1, 40, 40, 40);'''),
    ('''    static const char *const LABEL[INV_BODY] = {"helm", "chest", "gloves", "leggings", "boots", "right shoulder", "left shoulder",
                                                "right hand", "left hand", "right ring", "left ring"};''',
     '''    static const char *const LABEL[INV_BODY] = {"helm", "chest", "gloves", "leggings", "boots", "right shoulder", "left shoulder",
                                                "right hand", "left hand", "right ring", "left ring"};
    static const char *const SHORT[INV_BODY] = {"HELM", "CHEST", "GLOVE", "LEGS", "BOOTS", "SHLDR", "SHLDR", "HAND", "HAND", "RING", "RING"};'''),
    # the figure: status, or equip what is held
    ('''    int fx = px + (PW - 36) / 2, fy = py + 8;
    if ((in->click_left || in->click_right) && !I.held && in->mouse_x >= fx && in->mouse_x < fx + 36 && in->mouse_y >= fy && in->mouse_y < fy + 69) {
        char st[160];
        inv_status(st, sizeof st);
        game_message("%s", st);
        return true;
    }''', '''    int fx = px + (PW - 36) / 2, fy = py + 6;
    bool on_figure = in->mouse_x >= fx && in->mouse_x < fx + 36 && in->mouse_y >= fy && in->mouse_y < fy + 69;
    if (on_figure && in->click_left && I.held) { /* dropped on the figure: worn or held where it belongs */
        equip_held();
        return true;
    }
    if ((in->click_left || in->click_right) && !I.held && on_figure) {
        char st[160];
        inv_status(st, sizeof st);
        game_message("%s", st);
        return true;
    }'''),
    ('bool inv_update(Level *lv, const InvInput *in, const float eye[3], const float fwd[3]) {',
     '''/* where a thing goes on the body: armour by the armour table, rings a ring (the free one), weapons the right
 * hand (0x00-0x0F hand weapons, 0x18-0x1F bows and slings), shields the left, lights a free shoulder; -1 none */
static int body_slot_for(int p) {
    int item = I.pool[p].item & 511, a = obj_info(item)->armour;
    if (a == 9) return !I.slot[9] ? 9 : !I.slot[10] ? 10 : 9;
    if (a >= 0) return a;
    if (item < 0x10 || (item >= 0x18 && item < 0x20)) return 7;
    if (item >= 0x37 && item <= 0x3f && obj_info(item)->armour < 0) return 8; /* shields */
    if (is_light(item)) return !I.slot[5] ? 5 : !I.slot[6] ? 6 : 5;
    return -1;
}

/* the held thing onto the body; what was in that slot comes onto the pointer */
static void equip_held(void) {
    int s = body_slot_for(I.held);
    char name[96];
    obj_name(&I.pool[I.held], name, sizeof name);
    if (s < 0) return game_message("You cannot wear %s.", name);
    int old = I.slot[s];
    I.slot[s] = I.held, I.held = old;
    static const char *const WHERE[INV_BODY] = {"on thy head", "on thy body", "on thy hands", "on thy legs", "on thy feet",
                                                "on thy right shoulder", "on thy left shoulder", "in thy right hand",
                                                "in thy left hand", "on thy right hand", "on thy left hand"};
    game_message("You put %s %s.", name, WHERE[s]);
}

bool inv_update(Level *lv, const InvInput *in, const float eye[3], const float fwd[3]) {'''),
])
print('ok')
