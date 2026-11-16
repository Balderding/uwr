"""One-off: reading books and scrolls (UW.EXE file 0x37fa0): a reading panel (game.c), ui clipping, inventory
use for 0x130-0x13F."""
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


patch('src/ui.h', [
    ('int ui_font_height(int font);', 'int ui_font_height(int font);\n/* lines of ui_text_wrap drawn only between y0 and y1 (ui_clip(-1, -1): everywhere) */\nvoid ui_clip(int y0, int y1);'),
])
patch('src/ui.c', [
    ('int ui_text_wrap(int f, const char *s, int x, int y, int w, u8 r, u8 g, u8 b, bool draw) {',
     'static int g_clip0 = -(1 << 30), g_clip1 = 1 << 30;\n'
     'void ui_clip(int y0, int y1) {\n'
     '    if (y0 < 0 && y1 < 0) g_clip0 = -(1 << 30), g_clip1 = 1 << 30;\n'
     '    else g_clip0 = y0, g_clip1 = y1;\n'
     '}\n\n'
     'int ui_text_wrap(int f, const char *s, int x, int y, int w, u8 r, u8 g, u8 b, bool draw) {'),
    ('        if (draw) ui_text(f, line, x, cy, r, g, b);', '        if (draw && cy >= g_clip0 && cy + lh <= g_clip1) ui_text(f, line, x, cy, r, g, b);'),
])
patch('src/game.h', [
    ('void game_message(const char *fmt, ...);',
     'void game_message(const char *fmt, ...);\n/* a book, scroll or anything with words to read: shown in the reading panel until clicked away */\nvoid game_read(const char *title, const char *text);'),
])
patch('src/game.c', [
    ('    bool talking;\n', '    bool talking;\n    bool reading;\n    char read_title[96], read_text[8192];\n    int read_scroll;\n'),
    ('bool game_start(const char *data_dir, bool give_torch) {',
     'void game_read(const char *title, const char *text) {\n'
     '    str_copy(G.read_title, sizeof G.read_title, title);\n'
     '    str_copy(G.read_text, sizeof G.read_text, text);\n'
     '    G.reading = true, G.read_scroll = 0;\n'
     '}\n\n'
     '/* the reading panel: where it is and how much text fits */\n'
     'static void read_rect(int *x, int *y, int *w, int *h) {\n'
     '    Overlay *o = ui_overlay();\n'
     '    *w = MIN(o->w - 40, 320), *h = MIN(o->h - 80, 200), *x = (o->w - *w) / 2, *y = 30;\n'
     '}\n\n'
     'bool game_start(const char *data_dir, bool give_torch) {'),
    ('    if (G.talking) { /* a conversation: replies by number or click, Esc / E leaves when it is over */',
     '    if (G.reading) { /* reading: a click or E turns the page, past the last one closes; Esc closes */\n'
     '        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;\n'
     '        int x, y, w, h;\n'
     '        read_rect(&x, &y, &w, &h);\n'
     '        int total = ui_text_wrap(FONT_NORMAL, G.read_text, 0, 0, w - 16, 0, 0, 0, false), page = h - 34;\n'
     '        if (in->escape) G.reading = false;\n'
     '        else if (in->click_left || in->use || in->click_right) {\n'
     '            if (G.read_scroll + page < total) G.read_scroll += page - ui_font_height(FONT_NORMAL) - 2;\n'
     '            else G.reading = false;\n'
     '        }\n'
     '        return true;\n'
     '    }\n'
     '    if (G.talking) { /* a conversation: replies by number or click, Esc / E leaves when it is over */'),
    ('    if (G.talking) {\n        const Conv *cv = conv_state();\n        int pw = MIN(o->w - 20, 360)',
     '    if (G.reading) {\n'
     '        int x, y, w, h;\n'
     '        read_rect(&x, &y, &w, &h);\n'
     '        /* parchment */\n'
     '        ui_fill(x, y, w, h, 196, 176, 132, 245);\n'
     '        ui_fill(x, y, w, 2, 120, 96, 60, 255), ui_fill(x, y + h - 2, w, 2, 120, 96, 60, 255);\n'
     '        ui_text(FONT_NORMAL, G.read_title, x + 8, y + 5, 70, 40, 20);\n'
     '        int top = y + 18, page = h - 34;\n'
     '        ui_clip(top, top + page);\n'
     '        int total = ui_text_wrap(FONT_NORMAL, G.read_text, x + 8, top - G.read_scroll, w - 16, 40, 28, 16, true);\n'
     '        ui_clip(-1, -1);\n'
     '        const char *hint = G.read_scroll + page < total ? "CLICK FOR MORE" : "CLICK TO CLOSE";\n'
     '        ui_text(FONT_SMALL, hint, x + w - 8 - ui_text_width(FONT_SMALL, hint), y + h - 11, 90, 60, 30);\n'
     '        const Pic *cur = ui_pic("ui\\\\cursors", 10);\n'
     '        if (cur) ui_draw(cur, G.mouse_x - cur->w / 2, G.mouse_y - cur->h / 2, 1);\n'
     '        return;\n'
     '    }\n'
     '    if (G.talking) {\n        const Conv *cv = conv_state();\n        int pw = MIN(o->w - 20, 360)'),
])
patch('src/inventory.c', [
    ('    int eaten = eat(p);',
     '''    if (read_thing(p)) return;
    int eaten = eat(p);'''),
    ('static void use_thing(int p) {',
     '''/* Reading (UW.EXE file 0x37fa0): books and scrolls 0x130-0x13F; the map (0x13B) opens the map; an enchanted
 * one (flags bit 3) casts its spell and one with flags bit 1 has its own text (both: later); else "You read
 * the scroll..." and strings block 3 [link & 0x1ff] (when below 0x100) */
static bool read_thing(int p) {
    const Object *o = &I.pool[p];
    int item = o->item & 511;
    if (item < 0x130 || item > 0x13f) return false;
    if (item == 0x13b) return game_message("The map will come later."), true;
    if (o->flags & 0x08) return game_message("You cannot cast from scrolls yet."), true;
    int n = o->link & 0x1ff;
    char noun[64], title[96];
    obj_noun(o, noun, sizeof noun);
    snprintf(title, sizeof title, "You read the %s...", noun);
    if ((o->flags & 0x02) || n >= 0x100 || !str_get(3, n)[0]) {
        game_read(title, str_get(3, 0)); /* "The pages are blank." */
        return true;
    }
    game_message("%s", title);
    game_read(title, str_get(3, n));
    return true;
}

static void use_thing(int p) {'''),
])
print('ok')
