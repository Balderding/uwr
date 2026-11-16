"""One-off: the options menu (tools/options_menu.c.txt) in main.c; Esc backs out one step, else opens the menu;
the game is paused while it is open; the view ray through the mouse for the world while the inventory is open."""
import sys

ROOT = sys.argv[1]
p = ROOT + '/src/main.c'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'


def rep(a, b):
    global s
    a, b = a.replace('\n', nl), b.replace('\n', nl)
    assert s.count(a) == 1, (a[:70], s.count(a))
    s = s.replace(a, b)


menu = open(ROOT + '/tools/options_menu.c.txt', encoding='utf-8').read().replace('\r\n', '\n').replace('\n', nl)
a = s.index('int main(int argc, char **argv) {')
s = s[:a] + 'static int g_start_style; /* the DLSS 5 style it started with (a change needs a restart) */' + nl + menu + s[a:]

rep('#include "audio.h"', '#include "audio.h"') if '#include "audio.h"' in s else None
rep('''    read_settings(path, &s);''', '''    read_settings(path, &s);
    str_copy(g_settings_path, sizeof g_settings_path, path);
    g_start_style = s.dlss5_style;''')
rep('''        if (pressed(A_RELEASE_MOUSE)) capture(false);
''', '''        /* Esc: one step out of what is open; with nothing open, the options menu (Esc again closes it) */
        if (pressed(A_RELEASE_MOUSE)) {
            if (g_menu) g_menu = false;
            else if (!game_escape()) g_menu = true, g_menu_sel = 0;
        }
''')
rep('''        player_step(&pl, &g_level, dt, mdx, mdy, &s);''', '''        if (!g_menu) player_step(&pl, &g_level, dt, mdx, mdy, &s); /* the options pause the game */''')
rep('''            gi.escape = g_inventory && (GetAsyncKeyState(VK_ESCAPE) & 1);
''', '')
rep('''            if (GetCursorPos(&mp) && ScreenToClient(g_hwnd, &mp)) gi.mouse_x = mp.x / us_, gi.mouse_y = mp.y / us_;
            g_inventory = game_update(dt, cam.pos, pl.yaw, fwd, &gi);''', '''            if (GetCursorPos(&mp) && ScreenToClient(g_hwnd, &mp)) {
                gi.mouse_x = mp.x / us_, gi.mouse_y = mp.y / us_;
                /* the view ray through the mouse pointer (when it is free): for the world while the inventory is open */
                RECT cr;
                GetClientRect(g_hwnd, &cr);
                if (!g_captured && cr.right > 0 && cr.bottom > 0) {
                    float nx = 2.0f * (float)mp.x / (float)cr.right - 1.0f, ny = 1.0f - 2.0f * (float)mp.y / (float)cr.bottom;
                    float th = tanf(s.fov * PI / 360.0f), asp = (float)cr.right / (float)cr.bottom;
                    float right[3] = {cosf(pl.yaw), 0, -sinf(pl.yaw)};
                    float up[3] = {-sinf(pl.pitch) * sinf(pl.yaw), cosf(pl.pitch), -sinf(pl.pitch) * cosf(pl.yaw)};
                    float d[3], len = 0;
                    for (int k = 0; k < 3; k++) d[k] = fwd[k] + right[k] * nx * th * asp + up[k] * ny * th, len += d[k] * d[k];
                    len = sqrtf(len);
                    for (int k = 0; k < 3; k++) gi.mouse_dir[k] = d[k] / len;
                    gi.mouse_dir_ok = true;
                }
            }
            bool menu_click_l = gi.click_left, menu_click_r = gi.click_right;
            if (g_menu) { /* paused: the game sees no input and no time */
                GameInput none = {0};
                none.mouse_x = gi.mouse_x, none.mouse_y = gi.mouse_y;
                gi = none;
            }
            g_inventory = game_update(g_menu ? 0.0f : dt, cam.pos, pl.yaw, fwd, &gi);
            if (g_menu) menu_update(&s, &look, data_dir, gi.mouse_x, gi.mouse_y, menu_click_l, menu_click_r);
            g_inventory = g_inventory || g_menu;''')
rep('''            game_draw_ui(pl.yaw);
            render_set_ui(ui_overlay()->px);
        }
        /* a move trigger''', '''            game_draw_ui(pl.yaw);
            if (g_menu) {
                menu_draw(&s, &look);
                const Pic *cur = ui_pic("ui\\\\cursors", 10);
                if (cur) ui_draw(cur, gi.mouse_x - cur->w / 2, gi.mouse_y - cur->h / 2, 1);
            }
            render_set_ui(ui_overlay()->px);
        }
        /* a move trigger''')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
