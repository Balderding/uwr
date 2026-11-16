/* uo-new: Ultima Underworld in a new engine.
 *
 *   uw.exe                      play (settings in data\settings.txt, keys in data\keys.txt)
 *   uw.exe --level=N            start on level N (1-9)
 *   uw.exe --shot=FILE.png [--level=N] [--pos=X,Z] [--yaw=DEG] [--pitch=DEG] [--size=WxH]
 *                               no window: draw one picture and save it (for the checks)
 *
 * Everything is read from loose files in data\; any that are missing are first extracted from the
 * original files in game\. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "base.h"
#include "extract.h"
#include "level.h"
#include "normals.h"
#include "render.h"
#include "world.h"
#include "game.h"
#include "core_world.h"
#include "inventory.h"
#include "ui.h"
#include "audio.h"
#include "sound.h"
#include "conv.h"
const Conv *game_core_conv(void); /* game.c: the checks */

static const float PI = 3.14159265f;

/* --- settings ------------------------------------------------------------------------------------ */
typedef struct {
    int width, height, fullscreen, vsync;
    float fov, mouse, gamma, normal_strength, bounce, torch_intensity, torch_range, music_volume, sound_volume;
    int music;
    int invert_mouse, start_level, normal_maps, raytracing, rt_high, torch, character_with_pack;
    int dlss, dlss_quality, dlss5, dlss5_style;
    int original_movement;
    float dlss5_intensity, dlss5_structure, dlss5_tone, dlss5_saturation, exposure;
} Settings;

/* every setting: its name and the line written for it when the file lacks it */
static const struct {
    const char *key, *line;
} SETTING[] = {
    {"width", "width 1600"},
    {"height", "height 900"},
    {"fullscreen", "fullscreen 0          # 1 = borderless at the desktop's size (width and height are then ignored)"},
    {"vsync", "vsync 1"},
    {"fov", "fov 75                # vertical field of view, degrees"},
    {"mouse_sensitivity", "mouse_sensitivity 1.0"},
    {"invert_mouse", "invert_mouse 0"},
    {"gamma", "gamma 1.0"},
    {"normal_maps", "normal_maps 1         # stones standing out of the walls (F6 switches while playing)"},
    {"normal_strength", "normal_strength 1.0   # how deep the relief looks"},
    {"raytracing", "raytracing 1          # hardware ray tracing: shadows of everything + light bouncing off walls"},
    {"character_with_pack", "character_with_pack 1 # Tab opens the character panel (left) with the pack and bags (right); the pack's checkbox"},
    {"rt_quality", "rt_quality normal     # high: a bounce ray from every pixel, cleaned by DLSS Ray Reconstruction (needs DLSS on)"},
    {"bounce", "bounce 1.0            # how much light bounces off surfaces (ray tracing), 0 = none"},
    {"torch", "torch 1               # start with the torch lit (T lights it / puts it out)"},
    {"torch_intensity", "torch_intensity 1.3"},
    {"torch_range", "torch_range 6.0       # tiles the torch reaches"},
    {"music", "music 1               # the game's music (data\\music\\playlist.txt)"},
    {"music_volume", "music_volume 0.8"},
    {"sound_volume", "sound_volume 0.8      # the sound effects (doors, blows, water, spells)"},
    {"start_level", "start_level 1"},
    {"original_movement", "original_movement 1   # the player moves by UW.EXE's own physics (the rules core); 0 = ours"},
    {"dlss", "dlss 1                # NVIDIA DLSS on at start (always loaded when the card has it; the options switch it)"},
    {"dlss_quality", "dlss_quality quality  # dlaa (full resolution), quality, balanced or performance"},
    {"dlss5", "dlss5 1               # DLSS 5 on at start (always loaded when the card has it; the ' key switches it)"},
    {"dlss5_style", "dlss5_style 1         # 0 A - Default, 1 B - Natural, 2 C - Cinematic"},
    {"dlss5_intensity", "dlss5_intensity 1.0"},
    {"dlss5_structure", "dlss5_structure 1.0   # local structure strength"},
    {"dlss5_tone", "dlss5_tone 1.0        # local tone strength"},
    {"dlss5_saturation", "dlss5_saturation 1.15 # colour given back after DLSS 5 (1 = none)"},
    {"exposure", "exposure 2.0          # brightness scale DLSS and DLSS 5 work at (above 2, a torch-lit wall up close looks hazy in DLSS 5)"},
};

/* one "key value" setting into s (unknown keys are left alone) */
static void apply_setting(Settings *s, const char *key, const char *val) {
    float v = (float)atof(val);
    if (!strcmp(key, "width")) s->width = (int)v;
    else if (!strcmp(key, "height")) s->height = (int)v;
    else if (!strcmp(key, "fullscreen")) s->fullscreen = (int)v;
    else if (!strcmp(key, "vsync")) s->vsync = (int)v;
    else if (!strcmp(key, "fov")) s->fov = v;
    else if (!strcmp(key, "mouse_sensitivity")) s->mouse = v;
    else if (!strcmp(key, "invert_mouse")) s->invert_mouse = (int)v;
    else if (!strcmp(key, "normal_maps")) s->normal_maps = (int)v;
    else if (!strcmp(key, "normal_strength")) s->normal_strength = v;
    else if (!strcmp(key, "raytracing")) s->raytracing = (int)v;
    else if (!strcmp(key, "rt_quality")) s->rt_high = str_ieq(val, "high");
    else if (!strcmp(key, "character_with_pack")) s->character_with_pack = (int)v;
    else if (!strcmp(key, "bounce")) s->bounce = v;
    else if (!strcmp(key, "torch")) s->torch = (int)v;
    else if (!strcmp(key, "torch_intensity")) s->torch_intensity = v;
    else if (!strcmp(key, "torch_range")) s->torch_range = v;
    else if (!strcmp(key, "music")) s->music = (int)v;
    else if (!strcmp(key, "music_volume")) s->music_volume = v;
    else if (!strcmp(key, "sound_volume")) s->sound_volume = v;
    else if (!strcmp(key, "gamma")) s->gamma = v;
    else if (!strcmp(key, "start_level")) s->start_level = (int)v;
    else if (!strcmp(key, "original_movement")) s->original_movement = (int)v;
    else if (!strcmp(key, "dlss")) s->dlss = (int)v;
    else if (!strcmp(key, "dlss_quality"))
        s->dlss_quality = str_ieq(val, "dlaa") ? DLSS_Q_DLAA : str_ieq(val, "quality") ? DLSS_Q_QUALITY
                        : str_ieq(val, "balanced") ? DLSS_Q_BALANCED : str_ieq(val, "performance") ? DLSS_Q_PERF : DLSS_Q_QUALITY;
    else if (!strcmp(key, "dlss5")) s->dlss5 = (int)v;
    else if (!strcmp(key, "dlss5_style")) s->dlss5_style = (int)v;
    else if (!strcmp(key, "dlss5_intensity")) s->dlss5_intensity = v;
    else if (!strcmp(key, "dlss5_structure")) s->dlss5_structure = v;
    else if (!strcmp(key, "dlss5_tone")) s->dlss5_tone = v;
    else if (!strcmp(key, "dlss5_saturation")) s->dlss5_saturation = v;
    else if (!strcmp(key, "exposure")) s->exposure = v;
}

static void read_settings(const char *path, Settings *s) {
    *s = (Settings){.width = 1600, .height = 900, .vsync = 1, .fov = 75, .mouse = 1, .gamma = 1, .normal_strength = 1,
                    .start_level = 1, .original_movement = 1, .normal_maps = 1, .raytracing = 1, .bounce = 1, .torch = 1, .torch_intensity = 1.3f,
                    .torch_range = 6, .music = 1, .music_volume = 0.8f, .sound_volume = 0.8f, .character_with_pack = 1, .dlss = 1, .dlss_quality = DLSS_Q_QUALITY, .dlss5 = 1, .dlss5_style = 1,
                    .dlss5_intensity = 1, .dlss5_structure = 1, .dlss5_tone = 1, .dlss5_saturation = 1.15f, .exposure = 2};
    if (!file_exists(path)) {
        static const char head[] = "# uo-new settings. Delete this file to get the defaults back.\r\n";
        make_dirs_for(path);
        file_write(path, head, sizeof head - 1);
    }
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    bool have[COUNT(SETTING)] = {0};
    char key[64], val[64];
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        if (sscanf(p, "%63s %63s", key, val) == 2 && key[0] != '#') {
            for (int i = 0; i < COUNT(SETTING); i++)
                if (!strcmp(key, SETTING[i].key)) have[i] = true;
            apply_setting(s, key, val);
        }
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    /* settings the file lacks (new ones): added at its end with their defaults; nothing else changes */
    char add[4096];
    size_t an = 0;
    int missing = 0;
    for (int i = 0; i < COUNT(SETTING); i++) missing += !have[i];
    if (missing && f.size && f.data[f.size - 1] != '\n') an += (size_t)snprintf(add + an, sizeof add - an, "\r\n");
    for (int i = 0; i < COUNT(SETTING); i++)
        if (!have[i]) an += (size_t)snprintf(add + an, sizeof add - an, "%s\r\n", SETTING[i].line);
    if (missing) {
        FILE *o = fopen(path, "ab");
        if (o) fwrite(add, 1, an, o), fclose(o);
    }
    arena_reset(mark);
    s->width = MAX(320, MIN(s->width, 7680)), s->height = MAX(200, MIN(s->height, 4320));
    s->fov = MAX(40, MIN(s->fov, 120));
    s->start_level = MAX(1, MIN(s->start_level, 9));
    s->dlss5_style = MAX(0, MIN(s->dlss5_style, 2));
    if (s->exposure <= 0) s->exposure = 2;
}

/* --- keys ---------------------------------------------------------------------------------------- */
enum { A_FORWARD, A_BACK, A_LEFT, A_RIGHT, A_TURN_LEFT, A_TURN_RIGHT, A_LOOK_UP, A_LOOK_DOWN, A_RUN, A_JUMP,
       A_RELEASE_MOUSE, A_NEXT_LEVEL, A_PREV_LEVEL, A_FLY, A_DLSS, A_NORMALS, A_TORCH, A_SCREENSHOT, A_USE, A_INVENTORY, A_LOOK, A_RAYTRACING, A_STYLE, A_FIGHT, A_CHARACTER, A_QUICKSAVE, A_QUICKLOAD, A_SLEEP, A_TRACK, A_RUNES, A_CAST, ACTIONS };
static const char *ACTION_NAME[ACTIONS] = {"forward", "back", "left", "right", "turn_left", "turn_right", "look_up", "look_down",
                                           "run", "jump", "release_mouse", "next_level", "prev_level", "fly", "dlss", "normal_maps", "torch", "screenshot", "use", "inventory", "look", "raytracing", "dlss5_style", "fight", "character", "quicksave", "quickload", "sleep", "track", "runes", "cast"};
/* the line added to keys.txt for an action it lacks */
static const char *ACTION_DEFAULT[ACTIONS] = {"forward W Up", "back S Down", "left A", "right D", "turn_left Left", "turn_right Right",
                                              "look_up PageUp", "look_down PageDown", "run Shift", "jump Space", "release_mouse Escape",
                                              "next_level F3", "prev_level F2", "fly F4", "dlss '                 # DLSS 5 on / off (DLSS itself when there is no DLSS 5)",
                                              "normal_maps F6         # the stones' relief on / off",
                                              "torch T                # light the torch / put it out",
                                              "screenshot P           # the picture on screen, copied to the clipboard",
                                              "use E                  # take / use what the crosshair is on",
                                              "inventory I Tab        # thy pack (the mouse is free while it is open)",
                                              "look L                 # look at what the crosshair is on (left click too)",
                                              "raytracing ;           # ray tracing High / Normal / Off",
                                              "dlss5_style ]          # DLSS 5 style: Default / Natural / Cinematic",
                                              "fight C                # fight mode on / off (the original's Fight icon: only then a click attacks)",
                                              "character ,            # the character panel: attributes, skills, armour",
                                              "quicksave F5           # save the game (the quick slot)",
                                              "quickload F9           # load the quick save",
                                              "sleep Z                # make camp and sleep (as a bedroll does)",
                                              "track K                # the Track skill: what creatures are about, and which way",
                                              "runes R                # the rune panel: runes onto the shelf",
                                              "cast Q                 # cast the spell on the shelf"};
static int g_bind[ACTIONS][4]; /* virtual-key codes, 0 = none */

static const char DEFAULT_KEYS[] =
    "# uo-new keys: an action, then up to four keys. Delete this file to get the defaults back.\r\n"
    "# Key names: A-Z 0-9 Space Shift Ctrl Alt Tab Enter Escape Backspace Up Down Left Right PageUp PageDown\r\n"
    "#            Home End Insert Delete F1-F12 Mouse1 Mouse2 Mouse3 Mouse4 Mouse5 ' (or Quote), or a number (a Windows key code)\r\n"
    "dlss '                 # DLSS 5 on / off (DLSS itself when there is no DLSS 5)\r\n"
    "raytracing ;           # ray tracing High / Normal / Off\r\n"
    "dlss5_style ]          # DLSS 5 style: Default / Natural / Cinematic\r\n"
    "forward W Up\r\n"
    "back S Down\r\n"
    "left A\r\n"
    "right D\r\n"
    "turn_left Left\r\n"
    "turn_right Right\r\n"
    "look_up PageUp\r\n"
    "look_down PageDown\r\n"
    "run Shift\r\n"
    "jump Space\r\n"
    "release_mouse Escape\r\n"
    "# for exploring while the game is being built:\r\n"
    "next_level F3\r\n"
    "prev_level F2\r\n"
    "fly F4\r\n";

static int key_code(const char *n) {
    static const struct {
        const char *name;
        int vk;
    } K[] = {{"Space", VK_SPACE}, {"Shift", VK_SHIFT}, {"Ctrl", VK_CONTROL}, {"Alt", VK_MENU}, {"Tab", VK_TAB}, {"Enter", VK_RETURN},
             {"Escape", VK_ESCAPE}, {"Backspace", VK_BACK}, {"Up", VK_UP}, {"Down", VK_DOWN}, {"Left", VK_LEFT}, {"Right", VK_RIGHT},
             {"PageUp", VK_PRIOR}, {"PageDown", VK_NEXT}, {"Home", VK_HOME}, {"End", VK_END}, {"Insert", VK_INSERT}, {"Delete", VK_DELETE},
             {"Mouse1", VK_LBUTTON}, {"Mouse2", VK_RBUTTON}, {"Mouse3", VK_MBUTTON}, {"Mouse4", VK_XBUTTON1}, {"Mouse5", VK_XBUTTON2},
             {"'", VK_OEM_7}, {"Quote", VK_OEM_7}, {";", VK_OEM_1}, {",", VK_OEM_COMMA}, {".", VK_OEM_PERIOD}, {"/", VK_OEM_2},
             {"[", VK_OEM_4}, {"]", VK_OEM_6}, {"\\", VK_OEM_5}, {"-", VK_OEM_MINUS}, {"=", VK_OEM_PLUS}, {"`", VK_OEM_3}};
    for (int i = 0; i < COUNT(K); i++)
        if (str_ieq(n, K[i].name)) return K[i].vk;
    if ((n[0] == 'F' || n[0] == 'f') && n[1] >= '1' && n[1] <= '9') {
        int f = atoi(n + 1);
        if (f >= 1 && f <= 12) return VK_F1 + f - 1;
    }
    if (n[0] && !n[1]) {
        char c = n[0];
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c;
    }
    if (n[0] >= '0' && n[0] <= '9') return atoi(n);
    return 0;
}

static void read_keys(const char *path) {
    if (!file_exists(path)) {
        make_dirs_for(path);
        file_write(path, DEFAULT_KEYS, sizeof DEFAULT_KEYS - 1);
    }
    memset(g_bind, 0, sizeof g_bind);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    bool seen[ACTIONS] = {0};
    char line[256];
    const char *p = (const char *)f.data, *end = p + f.size;
    while (p && p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        size_t n = (size_t)((nl ? nl : end) - p);
        n = MIN(n, sizeof line - 1);
        memcpy(line, p, n);
        line[n] = 0;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *tok = strtok(line, " \t\r");
        if (tok) {
            for (int a = 0; a < ACTIONS; a++)
                if (str_ieq(tok, ACTION_NAME[a])) {
                    seen[a] = true;
                    int k = 0;
                    for (char *t = strtok(NULL, " \t\r"); t && k < 4; t = strtok(NULL, " \t\r")) {
                        int vk = key_code(t);
                        if (vk) g_bind[a][k++] = vk;
                        else logf_("keys.txt: unknown key \"%s\" for %s\n", t, ACTION_NAME[a]);
                    }
                }
        }
        p = nl ? nl + 1 : NULL;
    }
    /* actions the file lacks (new ones): their default line added at its end, and used now */
    FILE *o = NULL;
    for (int a = 0; a < ACTIONS; a++) {
        if (seen[a]) continue;
        if (!o && (o = fopen(path, "ab")) && f.size && f.data[f.size - 1] != '\n') fputs("\r\n", o);
        if (o) fprintf(o, "%s\r\n", ACTION_DEFAULT[a]);
        char def[128];
        str_copy(def, sizeof def, ACTION_DEFAULT[a]);
        char *hash = strchr(def, '#');
        if (hash) *hash = 0;
        strtok(def, " \t");
        int k = 0;
        for (char *t = strtok(NULL, " \t"); t && k < 4; t = strtok(NULL, " \t"))
            if (key_code(t)) g_bind[a][k++] = key_code(t);
    }
    if (o) fclose(o);
    arena_reset(mark);
}

static bool g_focus, g_captured, g_inventory, g_click_l, g_click_r, g_attack, g_use_click, g_lmb_held;
static int g_text; /* a character typed (WM_CHAR), for the name in character creation */
static bool down(int a) {
    if (!g_focus) return false;
    for (int k = 0; k < 4; k++)
        if (g_bind[a][k] && (GetAsyncKeyState(g_bind[a][k]) & 0x8000)) return true;
    return false;
}
/* pressed since last asked */
static bool pressed(int a) {
    static bool was[ACTIONS];
    bool d = down(a), p = d && !was[a];
    was[a] = d;
    return p;
}

/* --- the window ---------------------------------------------------------------------------------- */
static HWND g_hwnd;
static bool g_quit;
static long g_mdx, g_mdy;

static void capture(bool on) {
    if (on == g_captured) return;
    g_captured = on;
    ShowCursor(on ? FALSE : TRUE);
    if (on) {
        RECT r;
        GetClientRect(g_hwnd, &r);
        MapWindowPoints(g_hwnd, NULL, (POINT *)&r, 2);
        ClipCursor(&r);
    } else ClipCursor(NULL);
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CLOSE: g_quit = true; return 0;
    case WM_ACTIVATE:
        g_focus = LOWORD(w) != WA_INACTIVE;
        if (!g_focus) capture(false), g_lmb_held = false;
        return 0;
    case WM_RBUTTONDOWN:
        if (g_inventory) g_click_r = true;
        else if (g_captured) g_use_click = true; /* right click = use, like E */
        return 0;
    case WM_LBUTTONDOWN:
        if (g_inventory) {
            g_click_l = true;
            return 0;
        }
        if (g_captured) {
            g_attack = true, g_lmb_held = true; /* pressed: a look (on a tagged thing) or the attack starts charging */
            return 0;
        }
        if (!g_captured) capture(true);
        return 0;
    case WM_CHAR:
        if (w == 8 || w == 13 || (w >= 32 && w < 127)) g_text = (int)w;
        return 0;
    case WM_LBUTTONUP:
        g_lmb_held = false; /* released: a charged attack strikes */
        return 0;
    case WM_INPUT: {
        RAWINPUT ri;
        UINT sz = sizeof ri;
        if (GetRawInputData((HRAWINPUT)l, RID_INPUT, &ri, &sz, sizeof(RAWINPUTHEADER)) != (UINT)-1 && ri.header.dwType == RIM_TYPEMOUSE &&
            !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
            g_mdx += ri.data.mouse.lLastX;
            g_mdy += ri.data.mouse.lLastY;
        }
        break;
    }
    case WM_SYSKEYDOWN:
        if (w == VK_F4 && (l & (1 << 29))) g_quit = true; /* Alt+F4 */
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* the rules core's clock: real time in 1/256 s (the DOS timer the original read) */
static unsigned core_clock_now(void) {
    static LARGE_INTEGER f, t0;
    LARGE_INTEGER t;
    if (!f.QuadPart) QueryPerformanceFrequency(&f), QueryPerformanceCounter(&t0);
    QueryPerformanceCounter(&t);
    return (unsigned)((t.QuadPart - t0.QuadPart) * 256 / f.QuadPart);
}

/* --- the player ---------------------------------------------------------------------------------- */
typedef struct {
    float x, z, feet, vy; /* feet: world y of the soles */
    float yaw, pitch;
    bool fly, leap; /* leap: this jump under the Leap spell (half gravity) */
} Player;

#define EYE 0.55f
#define RADIUS 0.18f
#define MAX_STEP 0.3f  /* highest step walked up without jumping (a little over one height step) */

/* can the player stand at (x, z) with soles at feet */
static bool fits(const Level *lv, float x, float z, float feet) {
    static const float OX[8] = {1, -1, 0, 0, 0.7f, -0.7f, 0.7f, -0.7f}, OZ[8] = {0, 0, 1, -1, 0.7f, 0.7f, -0.7f, -0.7f};
    for (int k = 0; k < 8; k++) {
        float px = x + OX[k] * RADIUS, pz = z + OZ[k] * RADIUS;
        if (level_solid_at(lv, px, pz)) return false;
        if (game_blocked(px, pz, 0.02f)) return false;
        if (level_floor_y(lv, px, pz) > feet + MAX_STEP) return false;
    }
    return true;
}

static float floor_under(const Level *lv, float x, float z) {
    float y = -100;
    static const float OX[5] = {0, 1, -1, 0, 0}, OZ[5] = {0, 0, 0, 1, -1};
    for (int k = 0; k < 5; k++) {
        float px = x + OX[k] * RADIUS * 0.7f, pz = z + OZ[k] * RADIUS * 0.7f;
        if (!level_solid_at(lv, px, pz)) y = MAX(y, level_floor_y(lv, px, pz));
    }
    return y;
}

static void player_step(Player *p, const Level *lv, float dt, float mdx, float mdy, const Settings *s) {
    float sens = 0.0022f * s->mouse;
    p->yaw += mdx * sens;
    p->pitch -= mdy * sens * (s->invert_mouse ? -1.0f : 1.0f);
    float turn = 2.2f * dt;
    if (down(A_TURN_LEFT)) p->yaw -= turn;
    if (down(A_TURN_RIGHT)) p->yaw += turn;
    if (down(A_LOOK_UP)) p->pitch += turn;
    if (down(A_LOOK_DOWN)) p->pitch -= turn;
    p->pitch = MAX(-1.45f, MIN(p->pitch, 1.45f));
    if (s->original_movement && cw_running()) {
        /* UW.EXE's physics (the rules core): one movement at a time, as the original's keyboard handler gives them
         * (forward first, then back, the sides, a jump); the facing is our mouse look */
        int in = down(A_JUMP) && pressed(A_JUMP) ? CW_JUMP : down(A_FORWARD) ? CW_FORWARD : down(A_BACK) ? CW_BACK
                 : down(A_LEFT) ? CW_LEFT : down(A_RIGHT) ? CW_RIGHT : CW_NONE;
        int level;
        if (game_talking()) in = CW_NONE; /* the original takes no movement while the conversation screen is up */
        cw_step(dt, in, down(A_RUN), p->yaw);
        cw_player(&p->x, &p->z, &p->feet, &level);
        p->vy = 0;
        return;
    }
    if (pressed(A_FLY)) p->fly = !p->fly;

    float fx = 0, fz = 0;
    if (down(A_FORWARD)) fz += 1;
    if (down(A_BACK)) fz -= 1;
    if (down(A_RIGHT)) fx += 1;
    if (down(A_LEFT)) fx -= 1;
    float len = sqrtf(fx * fx + fz * fz);
    bool water = game_terrain(p->x, p->z) == 1 && p->feet <= level_floor_y(lv, p->x, p->z) + 0.02f;
    int magic = game_motion();
    if (magic & 8) water = false; /* water walk: the water is floor */
    float speed = (down(A_RUN) ? 4.5f : 2.2f) * dt * (water ? 0.55f : 1.0f); /* swimming is slower (ours) */
    if (len > 0) fx /= len, fz /= len;
    float sy = sinf(p->yaw), cy = cosf(p->yaw);
    float mx = (fz * sy + fx * cy) * speed, mz = (fz * cy - fx * sy) * speed;
    if (p->fly) {
        p->x += mx, p->z += mz;
        if (down(A_JUMP)) p->feet += speed;
        p->vy = 0;
        return;
    }
    /* move in small pieces, each axis on its own (slides along walls) */
    int pieces = 1 + (int)(sqrtf(mx * mx + mz * mz) / 0.05f);
    for (int i = 0; i < pieces; i++) {
        float sx = mx / (float)pieces, sz = mz / (float)pieces;
        if (fits(lv, p->x + sx, p->z, p->feet)) p->x += sx;
        if (fits(lv, p->x, p->z + sz, p->feet)) p->z += sz;
    }
    float ground = floor_under(lv, p->x, p->z);
    bool on_ground = p->feet <= ground + 0.01f;
    /* The spells' motion (UW.EXE 0x1c62f, 0x1d2c2; the original: jump 141, gravity 4 a tick): leap halves gravity
     * through the jump; slow fall holds the fall to 94 / 141 of a jump's speed and halves its harm; levitate and
     * fly: no gravity, the rise or fall fading by a fifth a tick (Space rises, looking down while moving sinks: our
     * keys) */
    if (on_ground && !water && pressed(A_JUMP)) p->vy = 2.6f, p->leap = (magic & 1) != 0;
    if (on_ground && p->vy <= 0) p->leap = false;
    float gravity = 9.0f;
    if (magic & 20) {
        gravity = 0;
        if (down(A_JUMP)) p->vy = 1.6f;
        else if (len > 0 && fz > 0 && p->pitch < -0.35f) p->vy = -1.6f;
        else p->vy = fabsf(p->vy) > 0.18f ? p->vy * powf(0.8f, dt * 18.0f) : 0;
    } else if (p->leap) gravity = 4.5f;
    p->vy -= gravity * dt;
    if (magic & 2 && p->vy < -1.73f) p->vy = -1.73f;
    p->feet += p->vy * dt;
    if (p->feet < ground) {
        if (p->vy < -3.0f) game_landed(magic & 2 ? -p->vy * 0.7071f : -p->vy); /* a fall's harm (slow fall: half) */
        /* walking up a step or slope: ease up; landing: stop */
        p->feet = ground - p->feet > 0.02f && on_ground ? p->feet + MIN(ground - p->feet, 3.0f * dt + 0.02f) : ground;
        if (p->feet < ground) p->feet = ground;
        p->vy = 0;
    }
    float ceil = level_ceil_y() - 0.05f;
    if (p->feet + EYE > ceil) p->feet = ceil - EYE, p->vy = MIN(p->vy, 0);
}

/* --- loading a level ----------------------------------------------------------------------------- */
static Level g_level;
static size_t g_level_mark;

/* a trap changed the tiles (change terrain): the geometry and lights made again from the level as it is now */
static bool remesh_level(const char *data_dir) {
    arena_reset(g_level_mark);
    static Light lights[MAX_LIGHTS];
    static u32 tiles[64 * 64 * TILE_LIGHTS];
    int nl = lights_load(data_dir, &g_level, lights, MAX_LIGHTS);
    if (nl < 0) nl = 0;
    lights_tiles(&g_level, lights, nl, tiles);
    static Mesh m;
    if (!level_mesh(&g_level, data_dir, NULL, 0, &m) || !render_upload(&m)) return false;
    render_set_lights(lights, nl, tiles);
    game_remesh(&m, lights, nl);
    logf_("level %d made again (a trap changed it)\n", g_level.number);
    return true;
}

static bool load_level(int n, const char *data_dir, Player *p, bool place) {
    if (g_level.number > 0 && level_dir()[0]) { /* the level left behind, as it is now (the original keeps it so) */
        game_store_level();
        if (!level_save(&g_level, level_dir())) logf_("save: cannot write level %d into %s\n", g_level.number, level_dir());
    }
    arena_reset(g_level_mark);
    if (!level_load(&g_level, data_dir, n)) return false;
    static Light lights[MAX_LIGHTS];
    static u32 tiles[64 * 64 * TILE_LIGHTS];
    int nl = lights_load(data_dir, &g_level, lights, MAX_LIGHTS);
    if (nl < 0) logf_("lights: cannot read or write data\\lights.txt (level %d)\n", n), nl = 0;
    lights_tiles(&g_level, lights, nl, tiles);
    /* the orbs are drawn by the renderer over the finished picture, not as geometry */
    static Mesh m;
    if (!level_mesh(&g_level, data_dir, NULL, 0, &m)) return false;
    if (!render_upload(&m)) return false;
    render_set_lights(lights, nl, tiles);
    /* the objects lying about and the wall torches, as picture cards */
    game_level(&g_level, &m, lights, nl);
    logf_("level %d: %d triangles (%d of %d faces with relief), %d normal maps, %d light%s\n", n, m.indices / 3, m.relief_faces, m.faces,
          m.normal_maps, nl, nl == 1 ? "" : "s");
    if (place) {
        /* where the game starts you: the entrance room at the south edge of level 1 (other levels: the
         * first open tile nearest the middle, until the stairs are linked) */
        float sx = 32.0f, sz = 1.5f;
        if (n != 1 || level_solid_at(&g_level, sx, sz)) {
            float best = 1e9f;
            for (int z = 0; z < 64; z++)
                for (int x = 0; x < 64; x++)
                    if (g_level.tile[z * 64 + x].type == T_OPEN) {
                        float d = (float)((x - 32) * (x - 32) + (z - 32) * (z - 32));
                        if (d < best) best = d, sx = (float)x + 0.5f, sz = (float)z + 0.5f;
                    }
        }
        p->x = sx, p->z = sz, p->feet = level_floor_y(&g_level, sx, sz), p->vy = 0;
    }
    return true;
}

static bool starts(const char *a, const char *pre) { return !strncmp(a, pre, strlen(pre)); }

/* --- saves ---------------------------------------------------------------------------------------------------
 * data\saves\run-N: the game being played (the levels as the player left them); data\saves\current.txt names
 * it. A save slot (data\saves\slot1-4, quick) holds those levels, the character, the game's state, the
 * inventory, what the conversations remember and where the player stands. Nothing is ever deleted: a new game or
 * a loaded save starts a new run folder. */
static char g_saves[600], g_run[700];
static int g_save_req = -1, g_load_req = -1; /* a slot asked for by the menu or a key (0 quick, 1-4) */
static bool g_new_req;
static int g_slot = 1;                        /* the menu's slot */

enum { SLOT_AUTO = 9 }; /* written when the game closes, read when it opens */
static void slot_dir(int k, char *out, size_t n) {
    if (k == SLOT_AUTO) snprintf(out, n, "%s\\autosave", g_saves);
    else if (k == 0) snprintf(out, n, "%s\\quick", g_saves);
    else snprintf(out, n, "%s\\slot%d", g_saves, k);
}
/* a new run folder (never one in use), named in current.txt, the levels read from it */
static void run_new(void) {
    for (int k = 1; k < 100000; k++) {
        snprintf(g_run, sizeof g_run, "%s\\run-%d", g_saves, k);
        if (GetFileAttributesA(g_run) == INVALID_FILE_ATTRIBUTES) break;
    }
    CreateDirectoryA(g_saves, NULL);
    CreateDirectoryA(g_run, NULL);
    char path[800];
    snprintf(path, sizeof path, "%s\\current.txt", g_saves);
    const char *name = strrchr(g_run, '\\') + 1;
    file_write(path, name, strlen(name));
    level_set_dir(g_run);
}
/* the level files of one folder into another */
static void copy_levels(const char *from, const char *to) {
    char pat[800];
    snprintf(pat, sizeof pat, "%s\\level*.txt", from);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char a[800], b[800];
        snprintf(a, sizeof a, "%s\\%s", from, fd.cFileName), snprintf(b, sizeof b, "%s\\%s", to, fd.cFileName);
        size_t mark = arena_mark();
        Bytes f = file_read(a);
        if (f.size) file_write(b, f.data, f.size);
        arena_reset(mark);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
static bool save_game(int k, int level_n, const Player *pl) {
    char dir[700], path[800];
    slot_dir(k, dir, sizeof dir);
    CreateDirectoryA(g_saves, NULL), CreateDirectoryA(dir, NULL);
    game_store_level();
    if (!level_save(&g_level, g_run)) return false;
    copy_levels(g_run, dir);
    if (!game_save(dir)) return false;
    snprintf(path, sizeof path, "%s\\where.txt", dir);
    char text[256];
    int n = snprintf(text, sizeof text, "level %d\r\nx %.4f\r\nz %.4f\r\nyaw %.4f\r\npitch %.4f\r\n", level_n, (double)pl->x, (double)pl->z,
                     (double)pl->yaw, (double)pl->pitch);
    return file_write(path, text, (size_t)n);
}
/* a save into a new run: its levels, then the level the player stood on, then the game's state */
static bool load_game(int k, const char *data_dir, int *level_n, Player *pl) {
    char dir[700], path[800];
    slot_dir(k, dir, sizeof dir);
    snprintf(path, sizeof path, "%s\\where.txt", dir);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    if (!f.size) return arena_reset(mark), false;
    int lv = 1;
    double x = 32, z = 1.5, yaw = 0, pitch = 0;
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        sscanf(p, "level %d", &lv), sscanf(p, "x %lf", &x), sscanf(p, "z %lf", &z), sscanf(p, "yaw %lf", &yaw), sscanf(p, "pitch %lf", &pitch);
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
    g_level.number = 0; /* the level being left is not written into the new run */
    run_new();
    copy_levels(dir, g_run);
    if (!load_level(lv, data_dir, pl, false)) return false;
    *level_n = lv;
    pl->x = (float)x, pl->z = (float)z, pl->yaw = (float)yaw, pl->pitch = (float)pitch;
    pl->feet = level_floor_y(&g_level, pl->x, pl->z), pl->vy = 0;
    game_load(dir);
    return true;
}

static wchar_t g_dlss_dir[MAX_PATH], g_log_dir[MAX_PATH];
static RenderConfig render_config(void *hwnd, int w, int h, const Settings *s) {
    RenderConfig rc = {hwnd, w, h, s->vsync != 0};
    rc.dlss = s->dlss != 0, rc.dlss_quality = s->dlss_quality, rc.dlss5 = s->dlss5 != 0, rc.nr_style = s->dlss5_style;
    rc.nr_intensity = s->dlss5_intensity, rc.nr_structure = s->dlss5_structure, rc.nr_tone = s->dlss5_tone;
    rc.nr_saturation = s->dlss5_saturation, rc.exposure = s->exposure, rc.raytracing = s->raytracing != 0;
    rc.rt_high = s->rt_high != 0;
    /* uo-new\dlss holds nvngx_dlss.dll and nvngx_dlssnr.dll; NGX's logs (only when UW_NGX_LOG is set) go to uo-new\logs */
    MultiByteToWideChar(CP_UTF8, 0, root_dir(), -1, g_dlss_dir, MAX_PATH);
    wcscpy(g_log_dir, g_dlss_dir);
    wcscat(g_dlss_dir, L"\\dlss");
    wcscat(g_log_dir, L"\\logs");
    rc.dlss_dir = g_dlss_dir, rc.log_dir = g_log_dir;
    return rc;
}

static void show_title(void) {
    char t[200];
    snprintf(t, sizeof t, "Ultima Underworld - uo-new   [%s - ' to switch]   [%s]", render_dlss_text(),
             !render_raytracing() ? "no ray tracing" : render_rt_high_active() ? "ray tracing High" : "ray tracing"); /* ; switches */
    SetWindowTextA(g_hwnd, t);
}

/* the picture (w x h RGBA8, top row first) onto the clipboard as a 32-bit bitmap (CF_DIB: rows bottom first,
 * BGRA), so it pastes into chat windows and image editors */
static bool copy_to_clipboard(const u8 *rgba, int w, int h) {
    size_t px = (size_t)w * h * 4;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + px);
    if (!g) return false;
    u8 *p = GlobalLock(g);
    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof bi);
    bi.biSize = sizeof bi, bi.biWidth = w, bi.biHeight = h, bi.biPlanes = 1, bi.biBitCount = 32, bi.biCompression = BI_RGB;
    bi.biSizeImage = (DWORD)px;
    memcpy(p, &bi, sizeof bi);
    u8 *d = p + sizeof bi;
    for (int y = 0; y < h; y++) {
        const u8 *s = rgba + (size_t)(h - 1 - y) * w * 4;
        for (int x = 0; x < w; x++, d += 4, s += 4) d[0] = s[2], d[1] = s[1], d[2] = s[0], d[3] = 255;
    }
    GlobalUnlock(g);
    if (!OpenClipboard(g_hwnd)) return GlobalFree(g), false;
    EmptyClipboard();
    bool ok = SetClipboardData(CF_DIB, g) != NULL;
    CloseClipboard();
    if (!ok) GlobalFree(g); /* on success the clipboard owns it */
    return ok;
}

/* the torch in the right hand: a little right of, below and before the eyes; it flickers */
static void place_torch(const Player *p, const Settings *s, bool unused, float t) {
    (void)unused;
    float in = 0, range = 6, col[3] = {1, 0.72f, 0.42f};
    bool on = game_light(&in, &range, col);
    float sy = sinf(p->yaw), cy = cosf(p->yaw);
    /* the hand: right of and before the eyes, but always in the open, a fifth of a tile from any wall (else
     * the light is inside the stone: the room goes dark, or a wall right in front blows out). Close to a wall
     * the hand comes back toward the eyes (the player is 0.18 from walls, so at worst the light is there). */
    float ox = cy * 0.22f + sy * 0.25f, oz = -sy * 0.22f + cy * 0.25f;
    float pos[3] = {p->x, p->feet + EYE - 0.12f, p->z};
    for (float f = 1.0f; f > 0.01f; f -= 0.125f) {
        float x = p->x + ox * f, z = p->z + oz * f;
        bool open = true;
        for (int k = 0; k < 8 && open; k++) {
            float a = (float)k * 0.785398f;
            open = !level_solid_at(&g_level, x + cosf(a) * 0.2f, z + sinf(a) * 0.2f);
        }
        if (open) {
            pos[0] = x, pos[2] = z;
            break;
        }
    }
    float flicker = 1.0f + 0.05f * sinf(t * 13.1f) + 0.035f * sinf(t * 29.7f + 1.3f) + 0.02f * sinf(t * 53.3f + 0.4f);
    static const float warm[3] = {1.0f, 0.72f, 0.42f};
    (void)warm;
    render_set_torch(on, pos, col, in * s->torch_intensity / 1.3f * flicker, range * s->torch_range / 6.0f);
}

/* --- the options menu: Esc when nothing else is open; Esc again (or Resume) closes it --------------------------
 * Each change takes effect at once where it can and is written back to data\settings.txt (only that value; the
 * comments stay). DLSS 5 is only offered when it started on this card (an RTX 20-series card does not try it). */
static bool g_menu;
static int g_menu_sel, g_menu_hover = -1;
static char g_settings_path[700];
enum { OPT_RESUME, OPT_SLOT, OPT_SAVE, OPT_LOAD, OPT_NEW, OPT_DLSS, OPT_DLSS5, OPT_STYLE, OPT_RT, OPT_BOUNCE, OPT_RELIEF, OPT_MUSIC,
       OPT_VOLUME, OPT_SOUND, OPT_MOUSE, OPT_FOV, OPT_QUIT, OPTS };

/* the value of one setting in settings.txt (the rest of the line, a comment, stays) */
static void settings_save(const char *key, const char *value) {
    size_t mark = arena_mark();
    Bytes f = file_read(g_settings_path);
    size_t cap = f.size + 256, n = 0;
    char *out = arena_alloc(cap);
    size_t klen = strlen(key);
    bool done = false;
    for (size_t i = 0; i < f.size;) {
        size_t e = i;
        while (e < f.size && f.data[e] != '\n') e++;
        const char *line = (const char *)f.data + i;
        if (!done && e - i > klen && !memcmp(line, key, klen) && line[klen] == ' ') {
            /* key, the new value, then the comment (from '#') padded to where it was */
            const char *hash = memchr(line, '#', e - i);
            int col = hash ? (int)(hash - line) : 0;
            int w = snprintf(out + n, cap - n, "%s %s", key, value);
            n += (size_t)w;
            if (hash) {
                for (int k = w; k < col; k++) out[n++] = ' ';
                if (w >= col) out[n++] = ' ';
                size_t hl = (size_t)((const char *)f.data + e - hash);
                memcpy(out + n, hash, hl), n += hl;
            } else if (e > i && line[e - i - 1] == '\r') out[n++] = '\r';
            done = true;
        } else memcpy(out + n, line, e - i), n += e - i;
        if (e < f.size) out[n++] = '\n';
        i = e + 1;
    }
    if (!done) n += (size_t)snprintf(out + n, cap - n, "%s%s %s\r\n", n && out[n - 1] != '\n' ? "\r\n" : "", key, value);
    file_write(g_settings_path, out, n);
    arena_reset(mark);
}
static void save_int(const char *key, int v) {
    char t[32];
    snprintf(t, sizeof t, "%d", v);
    settings_save(key, t);
}
static void save_float(const char *key, float v) {
    char t[32];
    snprintf(t, sizeof t, "%.2f", v);
    settings_save(key, t);
}

static bool opt_shown(int id, const Settings *s) {
    (void)s;
    if (id == OPT_DLSS) return true; /* not started: switched on for the next start */
    if (id == OPT_DLSS5 || id == OPT_STYLE) return render_dlss5_available();
    return true;
}
static void opt_text(int id, const Settings *s, const Look *look, char *label, char *value) {
    static const char *const STYLE[3] = {"A - Default", "B - Natural", "C - Cinematic"};
    value[0] = 0;
    switch (id) {
    case OPT_RESUME: strcpy(label, "Resume"); break;
    case OPT_SLOT: strcpy(label, "Save slot"), sprintf(value, "%d", g_slot); break;
    case OPT_SAVE: strcpy(label, "Save game (F5: quick)"); break;
    case OPT_LOAD: strcpy(label, "Load game (F9: quick)"); break;
    case OPT_NEW: strcpy(label, "New game"); break;
    case OPT_DLSS: strcpy(label, "DLSS"), strcpy(value, render_dlss_available() ? (render_dlss_on() ? "On" : "Off") : "Not on this card"); break;
    case OPT_DLSS5: strcpy(label, "DLSS 5"), strcpy(value, render_dlss5_on() ? "On" : "Off"); break;
    case OPT_STYLE: strcpy(label, "DLSS 5 style"), strcpy(value, STYLE[MAX(0, MIN(2, s->dlss5_style))]); break;
    case OPT_RT: /* Off, Normal (probes), High (a ray per pixel + Ray Reconstruction) */
        strcpy(label, "Ray tracing");
        sprintf(value, "%s%s", !s->raytracing ? "Off" : s->rt_high ? "High" : "Normal",
                s->raytracing && !render_raytracing_available() ? " (no DXR)"
                : s->raytracing && s->rt_high && !render_rt_high_active() ? (render_dlss_on() ? " (unavailable)" : " (needs DLSS)")
                                                                           : "");
        break;
    case OPT_BOUNCE: strcpy(label, "Bounced light"), strcpy(value, look->bounce > 0 ? "On" : "Off"); break;
    case OPT_RELIEF: strcpy(label, "Stone relief"), strcpy(value, look->normal_maps ? "On" : "Off"); break;
    case OPT_MUSIC: strcpy(label, "Music"), strcpy(value, s->music ? "On" : "Off"); break;
    case OPT_VOLUME: strcpy(label, "Music volume"), sprintf(value, "%d%%", (int)(s->music_volume * 100 + 0.5f)); break;
    case OPT_SOUND: strcpy(label, "Sound volume"), sprintf(value, "%d%%", (int)(s->sound_volume * 100 + 0.5f)); break;
    case OPT_MOUSE: strcpy(label, "Mouse sensitivity"), sprintf(value, "%.1f", s->mouse); break;
    case OPT_FOV: strcpy(label, "Field of view"), sprintf(value, "%d", (int)(s->fov + 0.5f)); break;
    case OPT_QUIT: strcpy(label, "Quit the game"); break;
    }
}
/* a click / Enter / Right (dir +1), a right click / Left (dir -1) */
static void opt_change(int id, int dir, Settings *s, Look *look, const char *data_dir) {
    switch (id) {
    case OPT_RESUME: g_menu = false; break;
    case OPT_SLOT: g_slot = (g_slot - 1 + dir + 4) % 4 + 1; break;
    case OPT_SAVE: g_save_req = g_slot, g_menu = false; break;
    case OPT_LOAD: g_load_req = g_slot, g_menu = false; break;
    case OPT_NEW: g_new_req = true, g_menu = false; break;
    case OPT_DLSS:
        if (render_dlss_available()) render_set_dlss(!render_dlss_on()), save_int("dlss", render_dlss_on()), show_title();
        break;
    case OPT_DLSS5: render_set_dlss5(!render_dlss5_on()), save_int("dlss5", render_dlss5_on()), show_title(); break;
    case OPT_STYLE: s->dlss5_style = (s->dlss5_style + dir + 3) % 3, render_set_dlss5_style(s->dlss5_style), save_int("dlss5_style", s->dlss5_style); break;
    case OPT_RT: {
        int q = !s->raytracing ? 0 : s->rt_high ? 2 : 1;
        q = (q + dir + 3) % 3;
        if (!render_raytracing_available()) q = 0;
        s->raytracing = q > 0, s->rt_high = q == 2;
        render_set_raytracing(s->raytracing), render_set_rt_high(s->rt_high);
        save_int("raytracing", s->raytracing), settings_save("rt_quality", s->rt_high ? "high" : "normal");
        break;
    }
    case OPT_BOUNCE: look->bounce = look->bounce > 0 ? 0.0f : 1.0f, s->bounce = look->bounce, save_float("bounce", look->bounce); break;
    case OPT_RELIEF: look->normal_maps = !look->normal_maps, s->normal_maps = look->normal_maps, save_int("normal_maps", s->normal_maps); break;
    case OPT_MUSIC:
        s->music = !s->music;
        if (sound_active()) sound_music(s->music); /* the Roland: the music on / off, the effects go on */
        else if (s->music) music_start(data_dir, s->music_volume);
        else music_stop();
        save_int("music", s->music);
        break;
    case OPT_VOLUME:
        s->music_volume = MAX(0.0f, MIN(1.0f, s->music_volume + dir * 0.1f));
        music_volume(s->music_volume), sound_music_volume(s->music_volume), save_float("music_volume", s->music_volume);
        break;
    case OPT_SOUND:
        s->sound_volume = MAX(0.0f, MIN(1.0f, s->sound_volume + dir * 0.1f));
        sound_effect_volume(s->sound_volume), save_float("sound_volume", s->sound_volume);
        break;
    case OPT_MOUSE: s->mouse = MAX(0.2f, MIN(4.0f, s->mouse + dir * 0.1f)), save_float("mouse_sensitivity", s->mouse); break;
    case OPT_FOV: s->fov = MAX(50.0f, MIN(110.0f, s->fov + dir * 5.0f)), save_int("fov", (int)s->fov); break;
    case OPT_QUIT: g_quit = true; break;
    }
}

/* the rows shown, in order */
static int opt_rows(const Settings *s, int *ids) {
    int n = 0;
    for (int id = 0; id < OPTS; id++)
        if (opt_shown(id, s)) ids[n++] = id;
    return n;
}
#define MENU_W 250
#define MENU_ROW 13
static void menu_rect(int rows, int *x, int *y, int *w, int *h) {
    Overlay *o = ui_overlay();
    *w = MENU_W, *h = 36 + rows * MENU_ROW, *x = (o->w - *w) / 2, *y = MAX(4, (o->h - *h) / 2);
}
static void menu_update(Settings *s, Look *look, const char *data_dir, int mx, int my, bool click_l, bool click_r) {
    int ids[OPTS], n = opt_rows(s, ids), x, y, w, h;
    menu_rect(n, &x, &y, &w, &h);
    g_menu_sel = MAX(0, MIN(n - 1, g_menu_sel));
    g_menu_hover = -1;
    if (mx >= x && mx < x + w && my >= y + 22 && my < y + 22 + n * MENU_ROW) g_menu_hover = (my - y - 22) / MENU_ROW, g_menu_sel = g_menu_hover;
    if (g_focus && (GetAsyncKeyState(VK_UP) & 1)) g_menu_sel = (g_menu_sel + n - 1) % n;
    if (g_focus && (GetAsyncKeyState(VK_DOWN) & 1)) g_menu_sel = (g_menu_sel + 1) % n;
    int dir = 0;
    if (g_focus && ((GetAsyncKeyState(VK_RETURN) & 1) || (GetAsyncKeyState(VK_RIGHT) & 1))) dir = 1;
    if (g_focus && (GetAsyncKeyState(VK_LEFT) & 1)) dir = -1;
    if (click_l && g_menu_hover >= 0) dir = 1;
    if (click_r && g_menu_hover >= 0) dir = -1;
    if (dir) opt_change(ids[g_menu_sel], dir, s, look, data_dir);
}
static void menu_draw(const Settings *s, const Look *look) {
    int ids[OPTS], n = opt_rows(s, ids), x, y, w, h;
    menu_rect(n, &x, &y, &w, &h);
    ui_fill(0, 0, ui_overlay()->w, ui_overlay()->h, 0, 0, 0, 110); /* the game behind, dimmed (paused) */
    ui_parchment(x, y, w, h);
    ui_text(FONT_NORMAL, "Options", x + w / 2 - ui_text_width(FONT_NORMAL, "Options") / 2, y + 6, UI_INK_HEAD);
    for (int r = 0; r < n; r++) {
        char label[48], value[48];
        opt_text(ids[r], s, look, label, value);
        int ry = y + 22 + r * MENU_ROW;
        bool sel = r == g_menu_sel;
        if (sel) ui_fill(x + 3, ry - 2, w - 6, MENU_ROW, UI_PAPER_MID, 255);
        ui_text(FONT_NORMAL, label, x + 10, ry + 1, sel ? 112 : 60, sel ? 36 : 40, sel ? 12 : 32);
        if (value[0]) ui_text(FONT_NORMAL, value, x + w - 10 - ui_text_width(FONT_NORMAL, value), ry + 1, UI_INK_GOLD);
    }
    const char *hint = "Click: change   Right click: back   Esc: close";
    ui_text(FONT_NORMAL, hint, x + w / 2 - ui_text_width(FONT_NORMAL, hint) / 2, y + h - 11, UI_INK_FAINT);
}

int main(int argc, char **argv) {
    arena_init((size_t)1536 << 20);
    const char *root = root_dir();
    char data_dir[MAX_PATH], game_dir[MAX_PATH], path[MAX_PATH];
    path_join(data_dir, sizeof data_dir, root, "data");
    path_join(game_dir, sizeof game_dir, root, "game");

    const char *shot = NULL;
    const char *picks_arg = NULL, *saves_arg = NULL;
    int save_at = -1, load_at = -1;
    int cast_at = -1, runes_frame = -1, char_level = 0;
    char cast_runes[32] = "";
    int level_arg = 0, shot_w = 1280, shot_h = 720, dlss_arg = -1, frames = 1, toggle_every = 0, rt_every = 0, style_at[2] = {-1, 0}, wield_arg = -1, fight_frame = -1, sheet_frame = -1, sleep_at = -1, plant_at = -1, fish_at = -1, give_arg = -1, hurt_at[2] = {-1, 0}, chargen_arg = 0, converse_arg = -1, normals_arg = -1, torch_arg = -1, rt_arg = -1;
    int talk_frame = -1; /* --talk, --picks: the checks of the core's conversations */
    char talk_picks[128] = "";
    const char *talk_picks_at = talk_picks;
    bool shot_ui = false;
    int click2_frame = -1;
    int use_frame = -1, talk_arg = -1, look_frame = -1, click_frame = -1, rclick_frame = -1, hold_frames = 1, rclick2_frame = -1, mouse2_frame = 1 << 30, shot_mouse2[2] = {0, 0}, attack_frame = -1, shot_mouse[2] = {0, 0};
    int inv_frame = -1;
    int shift_frame = -1;
    float view2[3] = {0, 0, 0};
    bool shot_menu = false;
    const char *choose_arg = NULL;
    const char *set_args[16];
    int nset = 0;
    float pos[2] = {-1, -1}, yaw_deg = 0, pitch_deg = 0;
    int walk_arg = 0; /* --walk=N: N frames walking forward on the rules core before the shot */
    float walk_face_x = -1, walk_face_z = -1;
    int walk_face_i = -1; /* the creature the walk's shot faces (and keeps facing) */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (starts(a, "--shot=")) shot = a + 7;
        else if (starts(a, "--level=")) level_arg = atoi(a + 8);
        else if (starts(a, "--pos=")) sscanf(a + 6, "%f,%f", &pos[0], &pos[1]);
        else if (starts(a, "--yaw=")) yaw_deg = (float)atof(a + 6);
        else if (starts(a, "--pitch=")) pitch_deg = (float)atof(a + 8);
        else if (starts(a, "--size=")) sscanf(a + 7, "%dx%d", &shot_w, &shot_h);
        else if (starts(a, "--dlss=")) dlss_arg = atoi(a + 7); /* 0 off, 1 DLSS, 2 DLSS + DLSS 5 */
        else if (starts(a, "--frames=")) frames = MAX(1, atoi(a + 9));
        else if (starts(a, "--walk=")) walk_arg = atoi(a + 7);
        else if (starts(a, "--toggle-every=")) toggle_every = atoi(a + 15); /* the ' key's switch, every N frames */
        else if (starts(a, "--rt-every=")) rt_every = atoi(a + 11); /* the ; key's switch, every N frames */
        else if (starts(a, "--style-at=")) sscanf(a + 11, "%d,%d", &style_at[0], &style_at[1]); /* frame,style: the options' DLSS 5 style switch */
        else if (starts(a, "--fight=")) fight_frame = atoi(a + 8); /* C pressed on frame N */
        else if (starts(a, "--talk=")) talk_frame = atoi(a + 7); /* the checks: talk to the creature faced on frame N */
        else if (starts(a, "--picks=")) snprintf(talk_picks, sizeof talk_picks, "%s", a + 8); /* its answers: 1,2,1 */
        else if (starts(a, "--sheet=")) sheet_frame = atoi(a + 8); /* , pressed on frame N */
        else if (starts(a, "--sleep-at=")) sleep_at = atoi(a + 11); /* Z pressed on frame N */
        else if (starts(a, "--plant-at=")) plant_at = atoi(a + 11); /* a silver seed planted on frame N */
        else if (starts(a, "--fish-at=")) fish_at = atoi(a + 10); /* a fishing pole used on frame N */
        else if (starts(a, "--char-level=")) char_level = atoi(a + 13); /* the character at this level (the checks) */
        else if (starts(a, "--runes=")) runes_frame = atoi(a + 8); /* R pressed on frame N */
        else if (starts(a, "--cast=")) { /* --cast=N:In,Lor: those runes on the shelf, Q pressed on frame N */
            cast_at = atoi(a + 7);
            const char *c = strchr(a, ':');
            if (c) str_copy(cast_runes, sizeof cast_runes, c + 1);
        } else if (starts(a, "--give=")) give_arg = (int)strtol(a + 7, NULL, 0); /* a thing of this kind into the pack (5 of it) */
        else if (starts(a, "--hurt-at=")) sscanf(a + 10, "%d,%d", &hurt_at[0], &hurt_at[1]); /* frame,damage */
        else if (starts(a, "--chargen=")) chargen_arg = atoi(a + 10); /* character creation shown (the checks) */
        else if (starts(a, "--frame-picks=")) picks_arg = a + 14;   /* (was a second "--picks=", never reached: 2026-11-16) */
        else if (starts(a, "--saves=")) saves_arg = a + 8; /* the checks' own saves folder (never data\\saves) */
        else if (starts(a, "--save-at=")) save_at = atoi(a + 10); /* save slot 1 on frame N */
        else if (starts(a, "--load-at=")) load_at = atoi(a + 10); /* load slot 1 on frame N */
        else if (starts(a, "--converse=")) converse_arg = atoi(a + 11); /* conversation N opens on frame 1 */
        else if (starts(a, "--wield=")) wield_arg = (int)strtol(a + 8, NULL, 0); /* a thing of this kind into the right hand */
        else if (starts(a, "--normals=")) normals_arg = atoi(a + 10);
        else if (starts(a, "--torch=")) torch_arg = atoi(a + 8);
        else if (starts(a, "--rt=")) rt_arg = atoi(a + 5);
        else if (starts(a, "--ui=")) shot_ui = atoi(a + 5) != 0;
        else if (starts(a, "--use=")) use_frame = atoi(a + 6);
        else if (starts(a, "--look=")) look_frame = atoi(a + 7);
        else if (starts(a, "--inv=")) inv_frame = atoi(a + 6); /* the panel opened on frame N */
        else if (starts(a, "--shift=")) shift_frame = atoi(a + 8); /* shift held from frame N */
        else if (!strcmp(a, "--menu")) shot_menu = true; /* the options menu drawn */
        else if (starts(a, "--view2=")) sscanf(a + 8, "%f,%f,%f", &view2[0], &view2[1], &view2[2]);
        else if (starts(a, "--mouse=")) sscanf(a + 8, "%d,%d", &shot_mouse[0], &shot_mouse[1]);
        else if (starts(a, "--click=")) click_frame = atoi(a + 8);
        else if (starts(a, "--click2=")) click2_frame = atoi(a + 9); /* a second left click */
        else if (starts(a, "--rclick=")) rclick_frame = atoi(a + 9);
        else if (starts(a, "--rclick2=")) rclick2_frame = atoi(a + 10);
        else if (starts(a, "--mouse2=")) sscanf(a + 9, "%d,%d,%d", &shot_mouse2[0], &shot_mouse2[1], &mouse2_frame); /* x,y,from frame */
        else if (starts(a, "--attack=")) attack_frame = atoi(a + 9);
        else if (starts(a, "--hold=")) hold_frames = MAX(1, atoi(a + 7)); /* the attack button held this many frames */
        else if (starts(a, "--conv=")) talk_arg = atoi(a + 7);   /* print conversation N to the console (the checks; was a second "--talk=", never reached: 2026-11-16) */
        else if (starts(a, "--choose=")) choose_arg = a + 9;     /* its answers: 1,2,... */
        else if (starts(a, "--set=") && nset < 16) set_args[nset++] = a + 6; /* a setting just for this run: key=value */
    }

    int made = extract_missing(data_dir, game_dir);
    if (made < 0) {
        MessageBoxA(NULL, "The original game files were not found in the game folder next to uw.exe.", "uo-new", MB_OK);
        return 1;
    }
    if (made) logf_("extracted %d files from %s into %s\n", made, game_dir, data_dir);
    for (int i = 1; i < argc; i++) /* --sound-wav=FILE,SECONDS: the Roland chain rendered to a WAV file (the checks) */
        if (starts(argv[i], "--sound-wav=")) {
            char wav[MAX_PATH];
            float secs = 20;
            int fx = -1;
            str_copy(wav, sizeof wav, argv[i] + 12);
            char *comma = strchr(wav, ',');
            if (comma) {
                *comma = 0, secs = (float)atof(comma + 1);
                char *c2 = strchr(comma + 1, ',');
                if (c2) fx = atoi(c2 + 1);
            }
            return sound_render_wav(data_dir, wav, secs, 0.8f, 0.8f, fx) ? 0 : 1; /* FILE,SECONDS[,EFFECT] */
        }
    int nmade = normals_missing(data_dir);
    if (nmade > 0) logf_("made %d normal maps in %s\\textures\n", nmade, data_dir);

    Settings s;
    path_join(path, sizeof path, data_dir, "settings.txt");
    read_settings(path, &s);
    str_copy(g_settings_path, sizeof g_settings_path, path);
    path_join(path, sizeof path, data_dir, "keys.txt");
    read_keys(path);
    int level_n = level_arg >= 1 && level_arg <= 9 ? level_arg : s.start_level;
    if (normals_arg >= 0) s.normal_maps = normals_arg;
    if (torch_arg >= 0) s.torch = torch_arg;
    for (int i = 0; i < nset; i++) {
        char key[64];
        const char *eq = strchr(set_args[i], '=');
        if (!eq || eq - set_args[i] >= 63) continue;
        memcpy(key, set_args[i], (size_t)(eq - set_args[i]));
        key[eq - set_args[i]] = 0;
        apply_setting(&s, key, eq + 1);
    }
    if (rt_arg >= 0) s.raytracing = rt_arg;
    Look look = {s.gamma, s.normal_maps != 0, s.normal_strength, s.bounce};
    Player pl = {0};
    bool torch = s.torch != 0;

    if (talk_arg >= 0) { /* a conversation to the console */
        if (!conv_start(data_dir, talk_arg, "the stranger", "Avatar")) return logf_("no conversation %d\n", talk_arg), 1;
        const char *ch = choose_arg ? choose_arg : "";
        int shown = 0;
        for (;;) {
            const Conv *c = conv_state();
            for (; shown < c->nlines; shown++) logf_("%s\n", c->lines[shown]);
            if (!c->waiting) break;
            for (int i = 0; i < c->noptions; i++) logf_("   %d) %s\n", i + 1, c->options[i]);
            if (!*ch) break;
            int pick = atoi(ch);
            while (*ch && *ch != ',') ch++;
            if (*ch == ',') ch++;
            conv_choose(pick - 1);
        }
        logf_("[%s]\n", conv_state()->running ? "waiting" : "the conversation ended");
        return 0;
    }

    if (shot) { /* headless: one picture (after --frames= frames of the same view, for DLSS's history) */
        if (dlss_arg >= 0) s.dlss = dlss_arg >= 1, s.dlss5 = dlss_arg >= 2;
        else s.dlss = 0; /* the checks' pictures are plain unless asked */
        RenderConfig rc = render_config(NULL, shot_w, shot_h, &s);
        if (!render_init(&rc)) return 1;
        if (s.dlss && !render_dlss_on()) logf_("%s\n", render_dlss_text());
        ui_start(data_dir, shot_w, shot_h);
        game_start(data_dir, torch);
        inv_set_with_sheet(s.character_with_pack != 0);
        if (chargen_arg) game_chargen(true);
        if (saves_arg) str_copy(g_saves, sizeof g_saves, saves_arg), run_new();
        g_level_mark = arena_mark();
        if (!load_level(level_n, data_dir, &pl, true)) return 1;
        if (pos[0] >= 0) pl.x = pos[0], pl.z = pos[1], pl.feet = floor_under(&g_level, pl.x, pl.z);
        pl.yaw = yaw_deg * PI / 180, pl.pitch = pitch_deg * PI / 180;
        if (walk_arg > 0) { /* the original's physics from the new game's start, then the shot from where it ends */
            char core_dir[MAX_PATH], save0[MAX_PATH];
            path_join(core_dir, sizeof core_dir, root, "build\\core_run");
            CreateDirectoryA(core_dir, NULL);
            path_join(save0, sizeof save0, core_dir, "SAVE0");
            CreateDirectoryA(save0, NULL);
            if (!cw_start(game_dir, core_dir)) { logf_("the rules core did not start\n"); return 1; }
            if (torch) cw_give_torch();
            if (pos[0] >= 0 || level_n != 1) cw_teleport(level_n, (int)pos[0], (int)pos[1]); /* --pos, --level: there */
            int lv;
            for (int f = 0; f < walk_arg; f++) {
                cw_step(1.0f / 60, CW_FORWARD, 0, pl.yaw);
                if (f % 30 == 0 || f == walk_arg - 1) {
                    cw_player(&pl.x, &pl.z, &pl.feet, &lv);
                    logf_("walk %d: level %d at %.3f, %.3f, feet %.3f\n", f, lv, pl.x, pl.z, pl.feet);
                }
            }
            cw_player(&pl.x, &pl.z, &pl.feet, &lv);
            int vh, vhm, vm, vmm;
            cw_vitals(&vh, &vhm, &vm, &vmm);
            logf_("the player: vitality %d of %d, mana %d of %d\n", vh, vhm, vm, vmm);
            /* the core's creatures near the player; the shot faces the nearest */
            const u8 *b = cw_block();
            float best = 1e9f, bx = 0, bz = 0;
            for (int i = 2; b && i < 256; i++) {
                const u8 *o = b + 0x4000 + i * 27;
                int item = rd16(o) & 511, home = rd16(o + 0x16), tx = home >> 10, tz = (home >> 4) & 63;
                if (item < 0x40 || item >= 0x80) continue;
                float cx = (float)tx + ((rd16(o + 2) >> 13) + 0.5f) / 8, cz = (float)tz + (((rd16(o + 2) >> 10) & 7) + 0.5f) / 8;
                float d = (cx - pl.x) * (cx - pl.x) + (cz - pl.z) * (cz - pl.z);
                if (d < 64) logf_("creature %d: item 0x%x at %.2f, %.2f, sequence 0x%x\n", i, item, cx, cz, o[0x15] & 0x3f);
                bool seen = true; /* nothing solid between */
                for (int k = 1; k < 32 && seen; k++)
                    if (level_solid_at(&g_level, pl.x + (cx - pl.x) * k / 32, pl.z + (cz - pl.z) * k / 32)) seen = false;
                if (seen && d < best) best = d, bx = cx, bz = cz, walk_face_i = i;
            }
            if (best < 64 && fight_frame >= 0) pl.yaw = atan2f(bx - pl.x, bz - pl.z), logf_("facing the nearest in sight (%.1f tiles)\n", sqrtf(best));
            walk_face_x = bx, walk_face_z = bz;
        }
        Camera cam = {{pl.x, pl.feet + EYE, pl.z}, pl.yaw, pl.pitch, s.fov * PI / 180};
        for (int f = 0; f < frames; f++) {
            if (walk_arg > 0 && f > 0) { /* the core goes on through the shot's frames (a swing, the creatures) */
                if (walk_face_i > 0 && fight_frame >= 0 && cw_block()) { /* still turned to the creature it faces */
                    const u8 *o = cw_block() + 0x4000 + walk_face_i * 27;
                    int home = rd16(o + 0x16);
                    float cx = (float)(home >> 10) + ((rd16(o + 2) >> 13) + 0.5f) / 8, cz = (float)((home >> 4) & 63) + (((rd16(o + 2) >> 10) & 7) + 0.5f) / 8;
                    pl.yaw = atan2f(cx - pl.x, cz - pl.z), cam.yaw = pl.yaw;
                    walk_face_x = cx, walk_face_z = cz;
                }
                { /* to the creature it faces, to within half a tile (a blow's reach), then still */
                    float dd = (walk_face_x - pl.x) * (walk_face_x - pl.x) + (walk_face_z - pl.z) * (walk_face_z - pl.z);
                    int lvx;
                    cw_step(1.0f / 60, walk_face_i > 0 && dd > 0.45f * 0.45f ? CW_FORWARD : CW_NONE, 0, pl.yaw);
                    cw_player(&pl.x, &pl.z, &pl.feet, &lvx);
                    cam.pos[0] = pl.x, cam.pos[1] = pl.feet + EYE, cam.pos[2] = pl.z;
                }
                if (talk_frame >= 0 && walk_face_i > 0 && f == talk_frame && cw_block()) { /* the checks: talk */
                    const u8 *o = cw_block() + 0x4000 + walk_face_i * 27;
                    int home = rd16(o + 0x16);
                    logf_("frame %d: talking to creature %d\n", f, walk_face_i);
                    cw_talk_world(walk_face_i, ((home >> 4) & 63) * 64 + (home >> 10));
                    logf_("the conversation screen %s\n", cw_conv_active() ? "is up" : "did not open");
                }
                if (talk_frame >= 0 && f > talk_frame) { /* its lines as they come, the menu, the answers given */
                    static int shown;
                    const Conv *cv = game_core_conv();
                    for (; shown < cv->nlines; shown++) logf_("  | %s\n", cv->lines[shown]);
                    if (cw_conv_waiting() && f % 30 == 0) {
                        char opts[CONV_MAX_OPTIONS][160];
                        int n = cw_conv_options(opts, CONV_MAX_OPTIONS);
                        for (int i = 0; i < n; i++) logf_("  %d. %s\n", i + 1, opts[i]);
                        int p = atoi(talk_picks_at);
                        const char *c = strchr(talk_picks_at, ',');
                        talk_picks_at = c ? c + 1 : talk_picks_at + strlen(talk_picks_at);
                        if (p < 1 || p > n) p = 1;
                        logf_("  -> %d\n", p);
                        cw_conv_choose(p - 1);
                    }
                    static bool ended;
                    if (!ended && !cw_conv_active()) ended = true, logf_("frame %d: the conversation is over\n", f);
                }
                if (f % 60 == 59) { /* the creatures by the player, every second: their hit points */
                    int vh, vhm, vm, vmm;
                    cw_vitals(&vh, &vhm, &vm, &vmm);
                    logf_("frame %d: the player's vitality %d of %d, mana %d of %d\n", f, vh, vhm, vm, vmm);
                    const u8 *b = cw_block();
                    for (int i = 2; b && i < 256; i++) {
                        const u8 *o = b + 0x4000 + i * 27;
                        int item = rd16(o) & 511, home = rd16(o + 0x16);
                        float cx = (float)(home >> 10) + 0.5f, cz = (float)((home >> 4) & 63) + 0.5f;
                        if (item >= 0x40 && item < 0x80 && (cx - pl.x) * (cx - pl.x) + (cz - pl.z) * (cz - pl.z) < 9)
                            logf_("frame %d: creature %d item 0x%x hit points %d, sequence 0x%x\n", f, i, item, o[8], o[0x15] & 0x3f);
                    }
                }
            }
            if (view2[2] > 0 && f == (int)view2[2]) { /* --view2=yaw,pitch,frame: look elsewhere from that frame */
                pl.yaw = view2[0] * PI / 180, pl.pitch = view2[1] * PI / 180;
                cam.yaw = pl.yaw, cam.pitch = pl.pitch;
            }
            place_torch(&pl, &s, torch, 0); /* steady (no flicker) for the checks */
            if (toggle_every > 0 && f > 0 && f % toggle_every == 0) logf_("frame %d: %s\n", f, (render_set_dlss(!render_dlss_on()), render_dlss_text()));
            if (f == style_at[0]) render_set_dlss5_style(style_at[1]), logf_("frame %d: DLSS 5 style %d\n", f, style_at[1]);
            if (f == 0 && wield_arg >= 0) inv_wield(wield_arg);
            if (f == plant_at) game_plant_seed();
            if (f == 0 && give_arg >= 0) for (int g = 0; g < 5; g++) inv_give(give_arg, 40);
            if (f == 0 && char_level) game_set_char_level(char_level);
            if (f == 0 && cast_runes[0]) logf_("shelf: %d runes\n", game_set_shelf(cast_runes));
            if (f == fish_at) game_fish();
            if (f == hurt_at[0]) game_hurt((float)hurt_at[1]);
            if (saves_arg && f == save_at) logf_("save slot 1: %s\n", save_game(1, level_n, &pl) ? "written" : "FAILED");
            if (saves_arg && f == load_at) logf_("load slot 1: %s\n", load_game(1, data_dir, &level_n, &pl) ? "restored" : "FAILED");
            if (f == 1 && converse_arg >= 0) game_converse(converse_arg);
            if (rt_every > 0 && f > 0 && f % rt_every == 0) { /* the ; key's cycle: High -> Normal -> Off -> High */
                int q = !s.raytracing ? 0 : s.rt_high ? 2 : 1;
                q = (q + 2) % 3;
                s.raytracing = q > 0, s.rt_high = q == 2;
                render_set_raytracing(s.raytracing), render_set_rt_high(s.rt_high);
                logf_("frame %d: ray tracing %s\n", f, q == 2 ? "High" : q ? "Normal" : "Off");
            }
            if (shot_ui) {
                float fwd0[3] = {sinf(pl.yaw) * cosf(pl.pitch), sinf(pl.pitch), cosf(pl.yaw) * cosf(pl.pitch)};
                GameInput gi = {0};
                gi.use = use_frame == f; /* --use=N: E pressed on frame N (the checks) */
                gi.fight = fight_frame == f, gi.character = sheet_frame == f, gi.sleep = sleep_at == f, gi.cast = cast_at == f, gi.runes = runes_frame == f;
                if (picks_arg && f >= 2) { /* --frame-picks=a,b,...: one answer a frame from frame 2 (0: Enter) */
                    int k = f - 2, v = -1;
                    for (const char *q = picks_arg; *q && k >= 0; k--) {
                        v = atoi(q);
                        q = strchr(q, ',');
                        if (!q) { if (k) v = -1; break; }
                        q++;
                    }
                    if (v == 0) gi.text = 13;
                    else if (v > 0) gi.number = v;
                }
                gi.look = look_frame == f, gi.inventory = inv_frame == f, gi.shift = shift_frame >= 0 && f >= shift_frame;
                gi.mouse_x = f >= mouse2_frame ? shot_mouse2[0] : shot_mouse[0], gi.mouse_y = f >= mouse2_frame ? shot_mouse2[1] : shot_mouse[1];
                gi.click_left = click_frame == f || click2_frame == f, gi.click_right = rclick_frame == f || rclick2_frame == f, gi.attack = attack_frame == f, gi.attack_held = attack_frame >= 0 && f >= attack_frame && f < attack_frame + hold_frames;
                if (walk_arg > 0 && attack_frame >= 0 && f > attack_frame) { /* on the core: a swing every 60 frames */
                    int k = (f - attack_frame) % 60;
                    gi.attack = k == 0, gi.attack_held = k < hold_frames;
                }
                game_update(0.016f, cam.pos, pl.yaw, fwd0, &gi);
                if (game_rebuild_wanted()) remesh_level(data_dir);
                game_draw_ui(pl.yaw);
                if (shot_menu) menu_draw(&s, &look);
                render_set_ui(ui_overlay()->px);
            }
            render_frame(&cam, &look, f == 0);
        }
        u8 *px = arena_alloc((size_t)shot_w * shot_h * 4);
        if (!render_readback(px) || !png_write(shot, px, shot_w, shot_h, 4)) return 1;
        logf_("%s\nwrote %s\n", render_status(), shot);
        render_shutdown();
        return 0;
    }

    /* the window */
    HINSTANCE hi = GetModuleHandleW(NULL);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wndproc, wc.hInstance = hi, wc.lpszClassName = L"uo-new", wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    int w = s.width, h = s.height;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    int wx = CW_USEDEFAULT, wy = CW_USEDEFAULT, ww, wh;
    if (s.fullscreen) {
        w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
        style = WS_POPUP, wx = 0, wy = 0, ww = w, wh = h;
    } else {
        RECT r = {0, 0, w, h};
        AdjustWindowRect(&r, style, FALSE);
        ww = r.right - r.left, wh = r.bottom - r.top;
    }
    g_hwnd = CreateWindowW(L"uo-new", L"Ultima Underworld - uo-new", style | WS_VISIBLE, wx, wy, ww, wh, NULL, NULL, hi, NULL);
    RAWINPUTDEVICE rid = {0x01, 0x02, 0, g_hwnd};
    RegisterRawInputDevices(&rid, 1, sizeof rid);
    if (dlss_arg >= 0) s.dlss = dlss_arg >= 1, s.dlss5 = dlss_arg >= 2;
    RenderConfig rc = render_config(g_hwnd, w, h, &s);
    if (!render_init(&rc)) {
        MessageBoxA(g_hwnd, "Could not start Direct3D 12.", "uo-new", MB_OK);
        return 1;
    }
    ui_start(data_dir, w, h);
    game_start(data_dir, torch);
    inv_set_with_sheet(s.character_with_pack != 0);
    snprintf(g_saves, sizeof g_saves, "%s\\saves", data_dir);
    run_new(); /* a fresh run: the levels as the original has them */
    game_new(), game_init_char(false); /* every start a new game from the beginning, a random character (the user: no
                                        * resuming while the game is being built; saves load from the menu) */
    if (s.original_movement) { /* the rules core: the original's world, its own working copy beside the run */
        char core_dir[MAX_PATH];
        path_join(core_dir, sizeof core_dir, root, "save\\core");
        CreateDirectoryA(core_dir, NULL);
        char save0[MAX_PATH];
        path_join(save0, sizeof save0, core_dir, "SAVE0");
        CreateDirectoryA(save0, NULL);
        if (!cw_start(game_dir, core_dir)) fprintf(stderr, "the rules core did not start: our movement instead\n");
        else {
            cw_clock(core_clock_now);
            if (s.torch) cw_give_torch(); /* the setting: start with the torch lit */
        }
    }
    g_level_mark = arena_mark();
    if (!load_level(level_n, data_dir, &pl, true)) {
        MessageBoxA(g_hwnd, "Could not load the level (see the console).", "uo-new", MB_OK);
        return 1;
    }
    logf_("%s\n", render_status());
    /* (no resuming the autosave: every start is a new game; saves load from the menu) */
    /* the sound: the Roland (UW's own driver into Munt with the user's ROMs); without ROMs the music on Windows' synth
     * (the AdLib driver comes as the no-ROM fallback) */
    if (sound_start(data_dir, s.music_volume, s.sound_volume)) {
        sound_song_play(1); /* the title song, as the original's start (UW.EXE 0x74d53); then the game's own choice */
        sound_music(s.music != 0);
    }
    else if (s.music && !music_start(data_dir, s.music_volume)) logf_("no music (no MIDI device, or no songs in data\\music\\playlist.txt)\n");
    show_title();
    g_focus = GetForegroundWindow() == g_hwnd;
    capture(g_focus);

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    bool reset = true;
    ULONGLONG title_until = 0;
    bool was_inventory = false; /* a message in the title until then, then the usual title again */
    while (!g_quit) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        last = now;
        dt = MIN(dt, 0.05f);
        /* Esc: one step out of what is open; with nothing open, the options menu (Esc again closes it) */
        if (pressed(A_RELEASE_MOUSE)) {
            if (g_menu) g_menu = false;
            else if (!game_escape()) g_menu = true, g_menu_sel = 0;
        }

        if (pressed(A_NORMALS)) {
            look.normal_maps = !look.normal_maps;
            logf_("normal maps %s\n", look.normal_maps ? "on" : "off");
        }
        if (pressed(A_DLSS)) { /* ': DLSS 5 on / off (DLSS stays on); without DLSS 5 on this card, DLSS itself */
            if (render_dlss5_available()) {
                bool on = !(render_dlss_on() && render_dlss5_on());
                if (on) render_set_dlss(true);
                render_set_dlss5(on);
                save_int("dlss5", on);
            } else render_set_dlss(!render_dlss_on()), save_int("dlss", render_dlss_on());
            logf_("%s\n", render_dlss_text());
            game_message("%s", render_dlss_text());
            show_title();
        }
        if (pressed(A_STYLE) && render_dlss5_available()) { /* ]: the DLSS 5 style, A -> B -> C -> A (live) */
            s.dlss5_style = (s.dlss5_style + 1) % 3;
            render_set_dlss5_style(s.dlss5_style), save_int("dlss5_style", s.dlss5_style);
            static const char *const SN[3] = {"A - Default", "B - Natural", "C - Cinematic"};
            logf_("DLSS 5 style %s\n", SN[s.dlss5_style]);
            game_message("DLSS 5 style: %s", SN[s.dlss5_style]);
        }
        if (pressed(A_RAYTRACING)) { /* ;: ray tracing High -> Normal -> Off -> High (Off / on: only with DXR) */
            int q = !s.raytracing ? 0 : s.rt_high ? 2 : 1;
            q = (q + 2) % 3;
            if (!render_raytracing_available()) q = 0;
            s.raytracing = q > 0, s.rt_high = q == 2;
            render_set_raytracing(s.raytracing), render_set_rt_high(s.rt_high);
            save_int("raytracing", s.raytracing), settings_save("rt_quality", s.rt_high ? "high" : "normal");
            const char *rtn = q == 2 ? (render_dlss_on() ? "High" : "High (needs DLSS on)") : q ? "Normal" : "Off";
            logf_("ray tracing %s\n", rtn);
            game_message("Ray tracing: %s", rtn);
            show_title();
        }
        if (pressed(A_NEXT_LEVEL) || pressed(A_PREV_LEVEL)) {
            int n = level_n + (down(A_NEXT_LEVEL) ? 1 : -1);
            if (n >= 1 && n <= 9 && load_level(n, data_dir, &pl, true)) level_n = n, reset = true;
        }
        float mdx = g_captured ? (float)g_mdx : 0, mdy = g_captured ? (float)g_mdy : 0;
        g_mdx = g_mdy = 0;
        if (!g_menu && !game_chargen_on()) player_step(&pl, &g_level, dt, mdx, mdy, &s); /* the options pause the game */
        if (s.original_movement && cw_running()) {
            /* the original moved the player to another level (stairs, a teleport): ours follows */
            float cx, cz, cf;
            int cl;
            cw_player(&cx, &cz, &cf, &cl);
            if (cl != level_n && cl >= 1 && cl <= 9 && load_level(cl, data_dir, &pl, true)) {
                level_n = cl, reset = true;
                pl.x = cx, pl.z = cz, pl.feet = cf;
            }
        }
        Camera cam = {{pl.x, pl.feet + EYE, pl.z}, pl.yaw, pl.pitch, s.fov * PI / 180};
        /* the game: what the crosshair is on, the pack, the carried light, the interface */
        {
            float fwd[3] = {sinf(pl.yaw) * cosf(pl.pitch), sinf(pl.pitch), cosf(pl.yaw) * cosf(pl.pitch)};
            GameInput gi = {0};
            gi.use = pressed(A_USE) || g_use_click, gi.inventory = pressed(A_INVENTORY), gi.torch = pressed(A_TORCH);
            gi.look = pressed(A_LOOK), gi.fight = pressed(A_FIGHT), gi.character = pressed(A_CHARACTER);
            gi.shift = g_focus && (GetKeyState(VK_SHIFT) & 0x8000);
            g_use_click = false;
            gi.click_left = g_click_l, gi.click_right = g_click_r, gi.attack = g_attack, gi.attack_held = g_lmb_held;
            gi.text = g_text, g_text = 0;
            gi.sleep = pressed(A_SLEEP), gi.track = pressed(A_TRACK), gi.runes = pressed(A_RUNES), gi.cast = pressed(A_CAST);
            for (int d = 1; d <= 9 && g_focus; d++)
                if (GetAsyncKeyState('0' + d) & 1) gi.number = d;
            g_click_l = g_click_r = g_attack = false;
            POINT mp;
            int uw_, uh_, us_;
            render_ui_size(&uw_, &uh_, &us_);
            if (GetCursorPos(&mp) && ScreenToClient(g_hwnd, &mp)) {
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
            if (inv_with_sheet() != (s.character_with_pack != 0)) /* the pack's checkbox changed: kept for next time */
                s.character_with_pack = inv_with_sheet(), save_int("character_with_pack", s.character_with_pack);
            if (g_menu) menu_update(&s, &look, data_dir, gi.mouse_x, gi.mouse_y, menu_click_l, menu_click_r);
            g_inventory = g_inventory || g_menu;
            if (g_inventory && g_captured) capture(false), ShowCursor(FALSE);
            if (!g_inventory && was_inventory && g_focus) ShowCursor(TRUE), capture(true);
            was_inventory = g_inventory;
            game_draw_ui(pl.yaw);
            if (g_menu) {
                menu_draw(&s, &look);
                ui_cursor(10, gi.mouse_x, gi.mouse_y);
            }
            render_set_ui(ui_overlay()->px);
        }
        if (game_rebuild_wanted()) remesh_level(data_dir);
        if (pressed(A_QUICKSAVE)) g_save_req = 0;
        if (pressed(A_QUICKLOAD)) g_load_req = 0;
        if (g_save_req >= 0) {
            bool ok = save_game(g_save_req, level_n, &pl);
            game_message(ok ? (g_save_req ? "Game saved in slot %d." : "Game saved (quick).") : "The game could not be saved.", g_save_req);
            g_save_req = -1;
        }
        if (g_load_req >= 0) {
            int k = g_load_req;
            g_load_req = -1;
            if (load_game(k, data_dir, &level_n, &pl)) game_message(k ? "Game %d restored." : "Quick save restored.", k), reset = true;
            else game_message("There is no saved game there.");
        }
        switch (game_death_choice()) { /* dead with no silver tree: the newest save, or a new game */
        case 1: {
            int best = -1;
            FILETIME bt = {0, 0};
            for (int k = 0; k <= SLOT_AUTO; k++) {
                if (k > 4 && k != SLOT_AUTO) continue;
                char dir[700], path[800];
                slot_dir(k, dir, sizeof dir);
                snprintf(path, sizeof path, "%s\\where.txt", dir);
                WIN32_FILE_ATTRIBUTE_DATA a;
                if (GetFileAttributesExA(path, GetFileExInfoStandard, &a) && CompareFileTime(&a.ftLastWriteTime, &bt) > 0) bt = a.ftLastWriteTime, best = k;
            }
            if (best >= 0) g_load_req = best;
            else g_new_req = true;
            break;
        }
        case 2: g_new_req = true; break;
        }
        if (g_new_req) { /* a new game: back to level 1, a new character */
            g_new_req = false;
            g_level.number = 0; /* the old game's level is not written into the new run */
            game_new(), run_new();
            game_start(data_dir, s.torch != 0);
            if (s.original_movement && cw_running()) { /* the original's world anew */
                char core_dir[MAX_PATH];
                path_join(core_dir, sizeof core_dir, root, "save\\core");
                if (!cw_start(game_dir, core_dir)) fprintf(stderr, "the rules core did not start again\n");
            }
            inv_set_with_sheet(s.character_with_pack != 0);
            if (load_level(1, data_dir, &pl, true)) level_n = 1, reset = true;
            game_init_char(true); /* everything back to the original's start, then the creation screens */
            game_chargen(true);
        }
        /* a move trigger (stairs, a teleport) sends the player elsewhere */
        {
            int tl;
            float gx, gz;
            if (game_travel(&tl, &gx, &gz)) {
                if (tl != level_n && tl >= 1 && tl <= 9 && load_level(tl, data_dir, &pl, false)) level_n = tl;
                pl.x = gx, pl.z = gz, pl.feet = level_floor_y(&g_level, gx, gz), pl.vy = 0;
                reset = true;
            }
        }
        place_torch(&pl, &s, true, (float)now.QuadPart / (float)freq.QuadPart);
        bool shot_now = pressed(A_SCREENSHOT);
        if (shot_now) render_want_screenshot();
        render_frame(&cam, &look, reset);
        if (shot_now) {
            size_t mark = arena_mark();
            u8 *px = arena_alloc((size_t)w * h * 4);
            bool ok = render_readback(px) && copy_to_clipboard(px, w, h);
            arena_reset(mark);
            logf_("screenshot %s\n", ok ? "copied to the clipboard" : "FAILED");
            char t[200];
            snprintf(t, sizeof t, "Ultima Underworld - uo-new   [%s]", ok ? "screenshot copied to the clipboard" : "screenshot failed");
            SetWindowTextA(g_hwnd, t);
            title_until = GetTickCount64() + 2000;
        }
        if (title_until && GetTickCount64() > title_until) title_until = 0, show_title();
        reset = false;
        if (IsIconic(g_hwnd)) Sleep(30);
    }
    capture(false);
    sound_stop();
    music_stop();
    if (!game_chargen_on() && level_dir()[0]) save_game(SLOT_AUTO, level_n, &pl); /* where the player left off */
    render_shutdown();
    return 0;
}
