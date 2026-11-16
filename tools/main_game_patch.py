"""One-off patch: the game module (inventory, light from items, interface) into main.c."""
import sys

p = sys.argv[1] + '/src/main.c'
s = open(p, encoding='utf-8').read()


def rep(a, b, count=1):
    global s
    assert s.count(a) == count, a[:80]
    s = s.replace(a, b)


rep('A_DLSS, A_NORMALS, A_TORCH, A_SCREENSHOT, ACTIONS };', 'A_DLSS, A_NORMALS, A_TORCH, A_SCREENSHOT, A_USE, A_INVENTORY, ACTIONS };')
rep('"normal_maps", "torch", "screenshot"};', '"normal_maps", "torch", "screenshot", "use", "inventory"};')
rep('"screenshot P           # the picture on screen, copied to the clipboard"};',
    '"screenshot P           # the picture on screen, copied to the clipboard",\n'
    '                                              "use E                  # take / use what the crosshair is on",\n'
    '                                              "inventory I Tab        # thy pack (the mouse is free while it is open)"};')
rep('#include "world.h"', '#include "world.h"\n#include "game.h"\n#include "ui.h"')
# the level's cards now come from the game (it rebuilds them when things are taken or dropped)
rep('''    static Vertex cards[RENDER_MAX_SPRITE_VERTS];
    int ncards = world_sprites(&g_level, &m, lights, nl, cards, RENDER_MAX_SPRITE_VERTS);
    render_set_sprites(cards, ncards);
    logf_("level %d: %d objects and wall torches drawn\\n", n, ncards / 6);''',
    '''    game_level(&g_level, &m, lights, nl);''')
# the carried light: from the lit item in the pack
rep('''static void place_torch(const Player *p, const Settings *s, bool on, float t) {''',
    '''static void place_torch(const Player *p, const Settings *s, bool unused, float t) {
    (void)unused;
    float in = 0, range = 6, col[3] = {1, 0.72f, 0.42f};
    bool on = game_light(&in, &range, col);''')
rep('''    render_set_torch(on, pos, warm, s->torch_intensity * flicker, s->torch_range);''',
    '''    (void)warm;
    render_set_torch(on, pos, col, in * s->torch_intensity / 1.3f * flicker, range * s->torch_range / 6.0f);''')
# start the interface and the game before the level (their memory stays below the level's)
rep('''        g_level_mark = arena_mark();
        if (!load_level(level_n, data_dir, &pl, true)) return 1;''',
    '''        ui_start(data_dir, shot_w, shot_h);
        game_start(data_dir, torch);
        g_level_mark = arena_mark();
        if (!load_level(level_n, data_dir, &pl, true)) return 1;''')
rep('''    g_level_mark = arena_mark();
    if (!load_level(level_n, data_dir, &pl, true)) {''',
    '''    ui_start(data_dir, w, h);
    game_start(data_dir, torch);
    g_level_mark = arena_mark();
    if (!load_level(level_n, data_dir, &pl, true)) {''')
rep('''            render_frame(&cam, &look, f == 0);''',
    '''            if (shot_ui) {
                float fwd0[3] = {sinf(pl.yaw) * cosf(pl.pitch), sinf(pl.pitch), cosf(pl.yaw) * cosf(pl.pitch)};
                GameInput gi = {0};
                game_update(0.016f, cam.pos, pl.yaw, fwd0, &gi);
                game_draw_ui(pl.yaw);
                render_set_ui(ui_overlay()->px);
            }
            render_frame(&cam, &look, f == 0);''')
rep('''        if (pressed(A_TORCH)) {
            torch = !torch;
            logf_("torch %s\\n", torch ? "lit" : "out");
        }''', '')
rep('''        place_torch(&pl, &s, torch, (float)now.QuadPart / (float)freq.QuadPart);''',
    '''        /* the game: what the crosshair is on, the pack, the carried light, the interface */
        {
            float fwd[3] = {sinf(pl.yaw) * cosf(pl.pitch), sinf(pl.pitch), cosf(pl.yaw) * cosf(pl.pitch)};
            GameInput gi = {0};
            gi.use = pressed(A_USE), gi.inventory = pressed(A_INVENTORY), gi.torch = pressed(A_TORCH);
            gi.click_left = g_click_l, gi.click_right = g_click_r;
            g_click_l = g_click_r = false;
            POINT mp;
            int uw_, uh_, us_;
            render_ui_size(&uw_, &uh_, &us_);
            if (GetCursorPos(&mp) && ScreenToClient(g_hwnd, &mp)) gi.mouse_x = mp.x / us_, gi.mouse_y = mp.y / us_;
            g_inventory = game_update(dt, cam.pos, pl.yaw, fwd, &gi);
            if (g_inventory && g_captured) capture(false), ShowCursor(FALSE);
            if (!g_inventory && was_inventory && g_focus) ShowCursor(TRUE), capture(true);
            was_inventory = g_inventory;
            game_draw_ui(pl.yaw);
            render_set_ui(ui_overlay()->px);
        }
        place_torch(&pl, &s, true, (float)now.QuadPart / (float)freq.QuadPart);''')
rep('''    ULONGLONG title_until = 0;''', '''    ULONGLONG title_until = 0;
    bool was_inventory = false;''')
# clicks: in the inventory they are the game's; otherwise a click takes the mouse back
rep('''    case WM_LBUTTONDOWN:''', '''    case WM_RBUTTONDOWN:
        if (g_inventory) g_click_r = true;
        return 0;
    case WM_LBUTTONDOWN:
        if (g_inventory) {
            g_click_l = true;
            return 0;
        }''')
rep('''static bool g_focus, g_captured;''', '''static bool g_focus, g_captured, g_inventory, g_click_l, g_click_r;''')
# shots: --ui=1 draws the interface too
rep('''        else if (starts(a, "--rt=")) rt_arg = atoi(a + 5);''',
    '''        else if (starts(a, "--rt=")) rt_arg = atoi(a + 5);
        else if (starts(a, "--ui=")) shot_ui = atoi(a + 5) != 0;''')
rep('''torch_arg = -1, rt_arg = -1;''', '''torch_arg = -1, rt_arg = -1;
    bool shot_ui = false;''')
open(p, 'w', encoding='utf-8').write(s)
print('ok')
