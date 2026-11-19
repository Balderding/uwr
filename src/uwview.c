/* uwview.c: the renderer DLL's C face. It is compiled into uwview.dll (with render.c: Direct3D 12), uwview_vk.dll
 * (with render_vk.c: Vulkan, the default), the Linux libuwview.so (tools\buildview_linux.py; os_posix.h stands in for
 * Windows' names) and tests\uwtour.c's uwtour.exe. The program's backend (port\win32\uwfront.c) finds the uwv_*
 * entry points by name, so the port's build needs nothing of ours.
 *
 * What it does: reads our settings (data\settings.txt), starts the renderer on the program's window, builds each
 * level's scene from the game's own level block (walls, floors, textures, lights, objects, creatures, doors), moves
 * the camera and the player's torch with the game, and draws our interface over the scene: the Esc menu's rows
 * (uwv_option), the HUD pieces, the message box, the inventory panel, the effects (particles, decals, water, fog).
 * It only shows the game: no rule of the game is decided here.
 *
 * Log: lines go through rlog (port\win32\rlog.h) with the tag "view". A few stay raw "uwview: ..." lines on stderr,
 * because check scripts read them from the start of the line or as the whole line: "level N: cannot read our level
 * data" and "level N: T triangles" (tools\inputcheck.py, ^uwview: level; also deathcheck.py), "particles: ..." and
 * "decals: level N: cleared" (fxcheck.py: startswith and whole-line compares), "decal test at screen" and "fx event
 * sound" (fxcheck.py: startswith). The acoustic dump (UWVIEW_ACOUSTIC_DUMP) is data in its own file. */
#ifdef _WIN32
#include <windows.h>
#else
#include <wchar.h>
#include <sys/stat.h>
#endif
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "rlog.h"
#include "render.h"
#include "base.h"
#include "extract.h"
#include "mapsmake.h"
#include "lights.h"
#include "world.h"
#include "models.h"
#include "ui.h"
#include "menu_draw.h"
#include "uwrec.h"
#include "smoothtext.h"
#include "../port/win32/uwinv.h"
#include "../port/win32/uwlog.h"
#define UWV_LOG_ROWS UWLOG_ROWS

#ifdef _WIN32
#define UWV_API __declspec(dllexport)
#else
#define UWV_API __attribute__((visibility("default")))
#endif

/* the effects' passes this DLL has (render.c runs each only while its setting is on) */
static const RenderFxPasses fx_passes = {rfx_particles_init, rfx_particles_frame, rfx_particles_draw, rfx_decals_init, rfx_decals_draw};

static bool started;
static wchar_t dlss_dir[MAX_PATH], log_dir[MAX_PATH];
static char root[MAX_PATH];             /* the folder with dlss\: uo-new, or the game's own folder (dist\UW1) */
static int lv_number;                   /* the level the scene holds (below) */
static bool cam_reset;
static int out_w, out_h;               /* the output's size */

/* --- our engine's settings (data\settings.txt, as uw.exe reads it: main.c's read_settings, the same keys and
 * defaults): the renderer's options --- */
static struct {
    int vsync, dlss, dlss_quality, dlss5, dlss5_style, raytracing, rt_high, normal_maps, hand, moving_light, port_quality, bounces, invert_mouse;
    float dlss5_intensity, dlss5_structure, dlss5_tone, dlss5_saturation, exposure, gamma, normal_strength, bounce, sheen, ray_length, sprite_strength, mouse, fov, sprite_ao, sprite_height, texture_ao, texture_shadow;
} S = {1, 1, DLSS_Q_QUALITY, 0, 1, 1, 0, 1, 1, 0, DLSS_Q_DLAA, 1, 0, 1, 1, 1, 1.15f, 2, 1, 1, 1, 0, 8, 0.5f, 1, 85, 0.5f, 0, 1, 3}; /* normal strength 1, no sheen: no shiny
                                                                            * walls unless asked ("sheen" turns it on) */
/* The glowing things (each its own Esc row): glow_things, the glowing rocks, orbs and orb rocks
 * glow a foot round them (light_glowing_things); moonstone, the moonstone's deep blue light flooding its room, as a
 * share of a candle (light_moonstone, 0.10); elemental, a fire elemental's smouldering light, the same way
 * (light_fire_elemental, 0.30); camp_smoke, smoke rising from the campfires and thinning out (fx_campfire_smoke);
 * water_mist, fog lying over the water (fx_water_mist). The last two are particles: they need fx_particles. */
static struct { int glow_things; float moonstone, elemental; int camp_smoke, water_mist; } GL = {1, 0.10f, 0.30f, 1, 1};
/* Textures (the Esc menu): the walls' and floors' own (0) or upscaled, 256 or 512 (settings.txt
 * texture_res; data\upscaled, tools\make_upscaled_data.py); hires_apply makes the renderer's copy when it changes */
static int tex_res;
/* the relief maps, a row a kind (each kind of map its own choice, with the
 * textures' choices): map_res[0] the normal maps, [1] the occlusion (the joints' shadows), [2] the height (the
 * relief's shadows); -1 as the textures (the default), 0 the original's 64 texel maps, 256 or 512 the maps made from
 * the upscaled texture of that size (tools\make_upscaled_maps.py). settings.txt normal_map_res, ao_map_res,
 * height_map_res */
static int map_res[3] = {-1, -1, -1};
static int hires_built[4] = {0, 0, 0, 0};   /* what the renderer's copy was made for: tex_res, then each kind's */
static int map_res_of(float v) { return (int)v == 512 ? 512 : (int)v == 256 ? 256 : v < 0 ? -1 : 0; }
static void hires_apply(void);
/* the effects (docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md): each off (0) until the user turns it on;
 * off in every proof and check (UWVIEW_FX_*=N for the checks; fx_audio and fx_acoustics are the backend's, kept here
 * with the rest so that one file holds them) */
static struct { int audio, acoustics, fog, particles, decals, water; } FX;
static int smooth_on = 1;               /* Smooth motion (uwv_camera's smooth_pose): settings.txt smooth_motion */
int uwv_conv_font_original;             /* uwconv_draw.c's text: settings.txt conv_font (smooth 0, original 1) */
/* the inventory's character sheet (uwinv_draw.c): settings.txt char_sheet, full (1, the default:
 * our first engine's sheet, every number the rules use) or original (0: the game's statistics page) */
int uwv_sheet_full = 1;
/* our message box's text (uwv_log; smaller and easier to read): settings.txt log_font, smooth (0,
 * the default: smoothtext.c) or original (1: the game's letters, as it was) */
static int log_font_original;
/* the Esc menu's text (the game's small letters are hard to read): settings.txt menu_font,
 * smooth (0, the default: smoothtext.c) or original (1: FONT_SMALL in capitals, as it was) */
static int menu_font_original;
static int menu_leather;                /* Menu background (settings.txt menu_background leather): 0 the solid colour */
/* the backend's sound settings kept here with the rest, for their rows: fx_hrtf (1 on, 0 off, 2 OpenAL's choice) and
 * fx_pan_fix (1 the effects' sides corrected, 0 as the MT-32 reads the pan); uwaudio.c reads the file itself */
static int fx_hrtf_set = 1, fx_panfix_set = 1;
/* the acoustics' three dials (uwaudio.c reads the file itself; uwaudio_tune after a change): acoustics_wet (the
 * reverb's level, a percentage), acoustics_tile_m (the metres a tile: the rooms' size), acoustics_occlusion (the
 * muffling by walls, a percentage) */
static int vol_setting(const char *key, float v);   /* the fog and smoke's rows (volfog.inc) */
static int ac_wet_set = 100, ac_occl_set = 100, ac_detour_set = 1;   /* (acoustics_detour: sound round corners) */
static float ac_tile_set = 2.4f;
static int fx_range_set = 6, fx_feet_set = 100, ac_quality_set = 1;   /* fx_hear_range (tiles), fx_feet_spread (%), acoustics_quality (0-3): uwaudio.c */
static int fx_stepvol_set = 80, fx_stepecho_set = 50;   /* fx_step_volume, fx_step_echo (%): uwaudio.c */
static int fx_footsteps_set = 1;   /* fx_footsteps: the footsteps by their floor's material (uwaudio.c) */
static char ui_face_name[64];   /* settings.txt ui_font_name: the interface's face by name (st_face_find) */
/* the fires' swells as settings.txt has them (render.c's defaults: 0.35 and 0.15), for their Esc rows */
static float camp_swell = 0.35f, flame_swell = 0.15f;
/* settings.txt (or a check's UWVIEW_SETTINGS file): each "key value" line into the settings above; unknown keys skipped */
static void read_settings(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];
    if (!f && errno != ENOENT) RLOG_W("view", "cannot read %s: %s (its settings not taken)", path, rlog_errno());   /* (none yet: the defaults, no failure) */
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char key[64], val[64];
        if (sscanf(line, "%63s %63s", key, val) != 2 || key[0] == '#') continue;
        float v = (float)atof(val);
        if (!strcmp(key, "vsync")) S.vsync = (int)v;
        else if (!strcmp(key, "smooth_motion")) smooth_on = (int)v != 0;   /* the camera eased between the game's steps */
        else if (!strcmp(key, "conv_font")) uwv_conv_font_original = !_stricmp(val, "original");   /* uwconv_draw.c */
        else if (!strcmp(key, "char_sheet")) uwv_sheet_full = _stricmp(val, "original") != 0;      /* uwinv_draw.c */
        else if (!strcmp(key, "log_font")) log_font_original = !_stricmp(val, "original");          /* uwv_log */
        else if (!strcmp(key, "ui_font")) ui_set_font((int)v != 0);     /* the interface's font: 0 the game's, 1 smooth */
        else if (!strcmp(key, "ui_font_name")) {                       /* which face (spaces kept as _), applied after */
            snprintf(ui_face_name, sizeof ui_face_name, "%s", val);
            for (char *c = ui_face_name; *c; c++) if (*c == '_') *c = ' ';
        }
        else if (!strcmp(key, "ui_font_size")) st_set_size_pct((int)v); /* its size, % of the resolution's (st_game_em) */
        else if (!strcmp(key, "menu_font")) menu_font_original = !_stricmp(val, "original");
        else if (!strcmp(key, "menu_background")) menu_leather = !_stricmp(val, "leather"), md_set_solid(!menu_leather);
        else if (!strcmp(key, "fx_hrtf")) fx_hrtf_set = (int)v < 0 ? 0 : (int)v > 2 ? 2 : (int)v;
        else if (!strcmp(key, "fx_pan_fix")) fx_panfix_set = (int)v != 0;
        else if (!strcmp(key, "dlss")) S.dlss = (int)v;
        else if (!strcmp(key, "dlss_quality"))
            S.dlss_quality = !_stricmp(val, "dlaa") ? DLSS_Q_DLAA : !_stricmp(val, "balanced") ? DLSS_Q_BALANCED
                           : !_stricmp(val, "performance") ? DLSS_Q_PERF : DLSS_Q_QUALITY;
        else if (!strcmp(key, "port_dlss_quality")) /* the port: DLSS's quality (dlaa: no upscaling, the default) */
            S.port_quality = !_stricmp(val, "quality") ? DLSS_Q_QUALITY : !_stricmp(val, "balanced") ? DLSS_Q_BALANCED
                           : !_stricmp(val, "performance") ? DLSS_Q_PERF : !_stricmp(val, "ultra") ? DLSS_Q_ULTRA : DLSS_Q_DLAA;
        else if (!strcmp(key, "dlss5")) S.dlss5 = (int)v;
        else if (!strcmp(key, "dlss5_style")) S.dlss5_style = (int)v;
        else if (!strcmp(key, "dlss5_intensity")) S.dlss5_intensity = v;
        else if (!strcmp(key, "dlss5_structure")) S.dlss5_structure = v;
        else if (!strcmp(key, "dlss5_tone")) S.dlss5_tone = v;
        else if (!strcmp(key, "dlss5_saturation")) S.dlss5_saturation = v;
        else if (!strcmp(key, "exposure")) S.exposure = v;
        else if (!strcmp(key, "raytracing")) S.raytracing = (int)v;
        else if (!strcmp(key, "rt_quality")) S.rt_high = !_stricmp(val, "high");
        else if (!strcmp(key, "normal_maps")) S.normal_maps = (int)v;
        else if (!strcmp(key, "gamma")) S.gamma = v;
        else if (!strcmp(key, "bounce")) S.bounce = v;
        else if (!strcmp(key, "sprite_normal_strength")) S.sprite_strength = v; /* the pictures' maps (0.5) */
        else if (!strcmp(key, "sprite_ao_strength")) S.sprite_ao = v;   /* the pictures' occlusion maps (0.5; 0 none) */
        else if (!strcmp(key, "sprite_height")) S.sprite_height = v;    /* their height maps' parallax, texels (0 none) */
        else if (!strcmp(key, "texture_ao_strength")) S.texture_ao = v; /* the walls' and floors' occlusion maps (1; 0 none) */
        else if (!strcmp(key, "texture_shadow")) S.texture_shadow = v;  /* their relief's shadows: its depth, texels (3; 0 none) */
        else if (!strcmp(key, "texture_res")) tex_res = (int)v == 512 ? 512 : (int)v == 256 ? 256 : 0;
        else if (!strncmp(key, "sprite_new_", 11)) {   /* the Sprites tab: object NNN's picture ours (spr_option) */
            int k = atoi(key + 11);
            if (k >= 0 && k < 512) level_new_sprite[k] = ui_new_sprite[k] = (int)v != 0;
        }
        else if (!strcmp(key, "normal_map_res")) map_res[0] = map_res_of(v);   /* -1 as the textures, 0, 256, 512 */
        else if (!strcmp(key, "ao_map_res")) map_res[1] = map_res_of(v);
        else if (!strcmp(key, "height_map_res")) map_res[2] = map_res_of(v);
        else if (!strcmp(key, "rt_ray_length")) S.ray_length = v;
        else if (!strcmp(key, "mouse_sensitivity")) S.mouse = v;
        else if (!strcmp(key, "fov")) S.fov = v;
        else if (!strcmp(key, "light_adaptation")) render_set_adaptation(!_stricmp(val, "auto") ? -1.0f : v);   /* auto, or 0 raw firelight .. 1 neutral */
        else if (!strcmp(key, "light_colour")) render_set_light_cat(!_stricmp(val, "bradford"));   /* cat16 / bradford (render.c) */
        else if (!strcmp(key, "hdr")) render_set_hdr((int)v != 0);   /* HDR output on an HDR monitor (1, the default) */
        else if (!strcmp(key, "hdr_peak")) render_set_hdr_levels(v, render_hdr_paper());   /* nits; 0 the monitor's own */
        else if (!strcmp(key, "hdr_paper_white")) render_set_hdr_levels(render_hdr_peak_setting(), v);   /* nits (200) */
        else if (!strcmp(key, "object_shadows")) render_set_object_shadows(v != 0);
        else if (!strcmp(key, "light_fade")) render_set_light_fade(v != 0);   /* the lights' fade by distance (1) */
        else if (!strcmp(key, "flame_breath")) render_set_breath_depth(v);   /* the fires' slow breath, 0-0.4 (0.20) */
        else if (!strcmp(key, "campfire_swell")) render_set_campfire_depth(camp_swell = v);   /* the campfires' own swell, 0-0.6 (0.35) */
        else if (!strcmp(key, "flame_swell")) render_set_puff_depth(flame_swell = v);   /* the fires' coherent swell, 0-0.5 */   /* objects, creatures, doors */                 /* uw.exe's: the vertical field of view, degrees */   /* uw.exe's: how fast the mouse turns the view */
        else if (!strcmp(key, "invert_mouse")) S.invert_mouse = (int)v;   /* tiles a bounce ray goes (8) */
        else if (!strcmp(key, "rt_bounces")) S.bounces = (int)v;     /* 1 one bounce; 2+ the probes' feedback */
        else if (!strcmp(key, "sheen")) S.sheen = v;   /* the port: the stone's sheen, 0 none */
        else if (!strcmp(key, "port_normal_strength")) S.normal_strength = v;
        else if (!strcmp(key, "torch_in_hand")) S.hand = (int)v;   /* the port: the light held up over the head (1), or at the eye (0) */
        else if (!strcmp(key, "moving_light")) S.moving_light = (int)v; /* a light going up and down the starting hall */
        else if (!strcmp(key, "fx_audio")) FX.audio = (int)v != 0;        /* positional samples (the backend's uwaudio.c) */
        else if (!strcmp(key, "fx_acoustics")) FX.acoustics = (int)v < 0 ? 0 : (int)v > 2 ? 2 : (int)v; /* the walls' muffling and the rooms' reverb (uwaudio.c): 1 On, 2 High (Steam Audio's echoes) */
        else if (!strcmp(key, "acoustics_wet")) ac_wet_set = (int)v < 0 ? 0 : (int)v > 200 ? 200 : (int)v;
        else if (!strcmp(key, "acoustics_occlusion")) ac_occl_set = (int)v < 0 ? 0 : (int)v > 100 ? 100 : (int)v;
        else if (!strcmp(key, "acoustics_tile_m")) ac_tile_set = v < 1.5f ? 1.5f : v > 4.0f ? 4.0f : v;
        else if (!strcmp(key, "acoustics_detour")) ac_detour_set = (int)v != 0;
        else if (!strcmp(key, "fx_footsteps")) fx_footsteps_set = (int)v != 0;
        else if (!strcmp(key, "fx_step_volume")) fx_stepvol_set = (int)v < 0 ? 0 : (int)v > 200 ? 200 : (int)v;
        else if (!strcmp(key, "fx_step_echo")) fx_stepecho_set = (int)v < 0 ? 0 : (int)v > 200 ? 200 : (int)v;
        else if (!strcmp(key, "fx_hear_range")) fx_range_set = (int)v < 6 ? 6 : (int)v > 32 ? 32 : (int)v;
        else if (!strcmp(key, "fx_feet_spread")) fx_feet_set = (int)v < 0 ? 0 : (int)v > 800 ? 800 : (int)v;
        else if (!strcmp(key, "acoustics_quality")) ac_quality_set = (int)v < 0 ? 0 : (int)v > 3 ? 3 : (int)v;
        else if (vol_setting(key, v)) { /* a row of the Fog and smoke page (volfog.inc) */ }
        else if (!strcmp(key, "fx_fog")) FX.fog = (int)v < 0 ? 0 : (int)v > 3 ? 3 : (int)v;   /* fog and light shafts, quality 1-3 */
        else if (!strcmp(key, "fx_particles")) FX.particles = (int)v != 0;
        else if (!strcmp(key, "light_glowing_things")) GL.glow_things = (int)v != 0, world_glow_things = GL.glow_things;
        else if (!strcmp(key, "light_moonstone")) GL.moonstone = v < 0 ? 0 : v > 1 ? 1 : v;
        else if (!strcmp(key, "light_fire_elemental")) GL.elemental = v < 0 ? 0 : v > 1 ? 1 : v;
        else if (!strcmp(key, "fx_campfire_smoke")) GL.camp_smoke = (int)v != 0;
        else if (!strcmp(key, "fx_water_mist")) GL.water_mist = (int)v != 0;
        else if (!strcmp(key, "fx_decals")) FX.decals = (int)v != 0;
        else if (!strcmp(key, "fx_water")) FX.water = (int)v != 0;
    }
    fclose(f);
}
/* UWVIEW_NORMALS=0 / 1: the normal maps off / on whatever the settings say (the checks: pictures with and without) */
static void settings_env(void)
{
    const char *e = getenv("UWVIEW_NORMALS");
    if (e && *e) S.normal_maps = atoi(e) != 0;
    e = getenv("UWVIEW_NSTRENGTH");      /* the normal maps' strength (the checks) */
    if (e && *e) S.normal_strength = (float)atof(e);
    e = getenv("UWVIEW_MOVING_LIGHT");   /* 1: the light going up and down the starting hall (the pictures) */
    if (e && *e) S.moving_light = atoi(e) != 0;
    e = getenv("UWVIEW_SPRITE_AO");      /* the pictures' occlusion strength (the checks) */
    if (e && *e) S.sprite_ao = (float)atof(e);
    e = getenv("UWVIEW_SPRITE_HEIGHT");  /* the pictures' height parallax (the checks) */
    if (e && *e) S.sprite_height = (float)atof(e);
    e = getenv("UWVIEW_TEXTURE_AO");     /* the walls' and floors' occlusion strength (the checks) */
    if (e && *e) S.texture_ao = (float)atof(e);
    e = getenv("UWVIEW_TEXTURE_SHADOW"); /* their relief's shadow depth (the checks) */
    if (e && *e) S.texture_shadow = (float)atof(e);
    e = getenv("UWVIEW_TEXRES"); /* Textures: 0, 256 or 512 (the checks) */
    if (e && *e) tex_res = atoi(e) == 512 ? 512 : atoi(e) == 256 ? 256 : 0;
    e = getenv("UWVIEW_MAPRES"); /* the relief maps' "normal,occlusion,height": each -1, 0, 256 or 512 (the checks) */
    if (e && *e) {
        int a = -1, b = -1, c = -1;
        sscanf(e, "%d,%d,%d", &a, &b, &c);
        map_res[0] = map_res_of((float)a), map_res[1] = map_res_of((float)b), map_res[2] = map_res_of((float)c);
    }
    e = getenv("UWVIEW_SHEEN");          /* the sheen (the checks) */
    if (e && *e) S.sheen = (float)atof(e);
    e = getenv("UWVIEW_RTHIGH");         /* 0 / 1: ray tracing High (Ray Reconstruction) (the checks) */
    if (e && *e) S.rt_high = atoi(e) != 0;
    e = getenv("UWVIEW_RT");             /* 0 / 1: ray tracing (the checks) */
    if (e && *e) S.raytracing = atoi(e) != 0;
    e = getenv("UWVIEW_CONVFONT");       /* smooth / original: the conversation's text (the checks) */
    if (e && *e) uwv_conv_font_original = !_stricmp(e, "original");
    e = getenv("UWVIEW_CHARSHEET");      /* full / original: the inventory's character sheet (the checks) */
    if (e && *e) uwv_sheet_full = _stricmp(e, "original") != 0;
    e = getenv("UWVIEW_LOGFONT");        /* smooth / original: the message box's text (the checks) */
    if (e && *e) log_font_original = !_stricmp(e, "original");
    e = getenv("UWVIEW_MENUFONT");       /* smooth / original: the Esc menu's text (the checks) */
    if (e && *e) menu_font_original = !_stricmp(e, "original");
    e = getenv("UWVIEW_DLSS5");          /* 0 / 1: DLSS 5 (the checks) */
    if (e && *e) S.dlss5 = atoi(e) != 0;
    e = getenv("UWVIEW_PUFF");           /* the flame's coherent swell (the checks) */
    if (e && *e) render_set_puff_depth((float)atof(e));
    e = getenv("UWVIEW_BREATH");         /* the fires' slow breath (the checks) */
    if (e && *e) render_set_breath_depth((float)atof(e));
    e = getenv("UWVIEW_CAMPFIRE");       /* the campfires' swell (the checks) */
    if (e && *e) render_set_campfire_depth((float)atof(e));
    e = getenv("UWVIEW_ADAPT");          /* the eye's adaptation to firelight, 0..1 (the checks) */
    if (e && *e) render_set_adaptation((float)atof(e));
    e = getenv("UWVIEW_D5TONE");         /* DLSS 5's local tone strength (the checks) */
    if (e && *e) S.dlss5_tone = (float)atof(e);
    e = getenv("UWVIEW_D5INT");          /* DLSS 5's intensity (the checks) */
    if (e && *e) S.dlss5_intensity = (float)atof(e);
    if (getenv("UWVIEW_FULLBRIGHT")) render_set_fullbright(true);   /* (the checks: everything lit, as Shift+L) */
    e = getenv("UWVIEW_DEBUG");          /* normals: the shading normals as colours (the checks) */
    if (e && !strcmp(e, "normals")) render_set_debug(1);
    if (e && !strcmp(e, "water")) render_set_debug(2);   /* the water's mask (the effects' checks) */
    e = getenv("UWVIEW_HAND");           /* 0: the torch at the eye (the checks) */
    if (e && *e) S.hand = atoi(e) != 0;
    if ((e = getenv("UWVIEW_FX_FOG")) && *e) FX.fog = atoi(e) < 0 ? 0 : atoi(e) > 3 ? 3 : atoi(e);   /* the effects (the checks) */
    if ((e = getenv("UWVIEW_FX_PARTICLES")) && *e) FX.particles = atoi(e) != 0;
    if ((e = getenv("UWVIEW_FX_DECALS")) && *e) FX.decals = atoi(e) != 0;
    if ((e = getenv("UWVIEW_FX_WATER")) && *e) FX.water = atoi(e) != 0;
}
static void data_dirs(char *data, char *game);

/* the folder holding dlss\nvngx_dlss.dll: up from this DLL's own folder (build\, or beside the port) */
static void find_dirs(void)
{
#ifndef _WIN32
    /* Linux: up from this library's own folder (build\linux, or beside the program) to the one with dlss\ (NGX looks
       there for libnvidia-ngx-dlss.so.*; without them, no DLSS). NGX takes its paths as wide strings here too. */
    char p[MAX_PATH];
    dlss_dir[0] = log_dir[0] = 0, root[0] = 0;
    if (!uwos_self_dir((const void *)find_dirs, p, sizeof p)) return;
    for (int up = 0; up < 8; up++) {
        if (up) {
            char *cut = strrchr(p, '/');
            if (!cut) break;
            *cut = 0;
        }
        char probe[MAX_PATH + 16];
        snprintf(probe, sizeof probe, "%s/dlss", p);
        struct stat st;
        if (!stat(probe, &st) && S_ISDIR(st.st_mode)) {
            snprintf(root, sizeof root, "%s", p);
            /* (a path with letters the program's locale cannot widen: no DLSS, said once; the rest goes on) */
            if (swprintf(dlss_dir, MAX_PATH, L"%s/dlss", p) < 0 || swprintf(log_dir, MAX_PATH, L"%s/logs", p) < 0) {
                dlss_dir[0] = log_dir[0] = 0;
                RLOG_W("view", "DLSS off: NGX cannot be given this folder's path (%s): use a folder with plain letters", p);
                return;
            }
            snprintf(probe, sizeof probe, "%s/logs", p);
            uwos_mkdir(probe);   /* NGX's data folder, made as Windows' path makes it (render_vk_dlss.h) */
            return;
        }
    }
#else
    wchar_t p[MAX_PATH];
    HMODULE self = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(void *)find_dirs, &self);
    GetModuleFileNameW(self, p, MAX_PATH);
    for (int up = 0; up < 8; up++) {
        wchar_t *cut = wcsrchr(p, L'\\');
        if (!cut) break;
        *cut = 0;
        wchar_t probe[MAX_PATH];
        _snwprintf(probe, MAX_PATH, L"%ls\\dlss\\nvngx_dlss.dll", p);
        if (GetFileAttributesW(probe) != INVALID_FILE_ATTRIBUTES) {
            WideCharToMultiByte(CP_UTF8, 0, p, -1, root, MAX_PATH, NULL, NULL);
            _snwprintf(dlss_dir, MAX_PATH, L"%ls\\dlss", p);
            _snwprintf(log_dir, MAX_PATH, L"%ls\\logs", p);
            return;
        }
    }
    dlss_dir[0] = log_dir[0] = 0;
#endif
}

/* the renderer on window hwnd, w by h pixels; 1 if it started */
static void *g_hwnd;                    /* the window (the clipboard's owner for P) */
UWV_API int uwv_init(void *hwnd, int w, int h)
{
    g_hwnd = hwnd;
    if (started) render_shutdown(), started = false;
    /* UWVIEW_TEST, the checks of the fallbacks: "fail" the renderer does not start (the port draws with GDI);
     * "nodlss" as on a machine without NVIDIA's DLSS libraries (the renderer without DLSS) */
    const char *test = getenv("UWVIEW_TEST");
    if (test && !strcmp(test, "fail")) return 0;
    find_dirs();
    if (test && !strcmp(test, "nodlss")) dlss_dir[0] = 0;
    {
        char data[MAX_PATH], game[MAX_PATH], p[MAX_PATH];
        data_dirs(data, game);
        snprintf(p, sizeof p, "%s\\settings.txt", data);
        {   /* the user's own fonts: every .ttf / .otf in the fonts folder beside the game, loaded for this program only
               and offered in the Esc menu's Font row */
            char fd[MAX_PATH];
            snprintf(fd, sizeof fd, "%s\\fonts", root);
            CreateDirectoryA(fd, NULL);
            st_load_font_dir(fd);
        }
        ui_set_font(1);   /* the interface's font: smooth (Segoe UI) unless settings.txt ui_font says otherwise */
        read_settings(p);
        {   /* UWVIEW_SETTINGS=FILE (the checks): the options a check changed, read over the folder's */
            const char *e = getenv("UWVIEW_SETTINGS");
            if (e && *e) read_settings(e);
        }
        if (ui_font() > 0 && ui_face_name[0]) {   /* the face kept by name (settings.txt ui_font_name, spaces as _) */
            int k = st_face_find(ui_face_name);
            if (k >= 0) ui_set_font(k + 1);
        }
        settings_env();
        RLOG_I("view", "settings read from %s", p);
        /* the first run of a package without our relief maps (mapsmake.h): made here, before the title
           (level_build asks again, for a renderer that had no game folder yet) */
        if (game[0]) maps_make_if_missing(root, data, game);
    }
    RenderConfig rc = {hwnd, w, h, S.vsync != 0};
    RLOG_I("view", "starting the graphics card's renderer (%dx%d)", w, h);
    /* DLSS, DLSS 5, ray tracing and ray tracing High (DLSS Ray Reconstruction) as our settings have them; each
     * falls back by itself where the card or its driver lacks it (render.c: without DLSS the scene is drawn at the
     * window's size, without DXR the shadows go through the tile grid) */
    /* DLSS at the window's own resolution (DLAA: no upscaling), unless port_dlss_quality says otherwise;
     * Ray Reconstruction and DLSS 5 work on it as on an upscaled picture */
    rc.dlss = dlss_dir[0] != 0 && S.dlss, rc.dlss_quality = S.port_quality;
    rc.dlss5 = S.dlss5 != 0, rc.nr_style = S.dlss5_style;
    rc.exposure = S.exposure;
    rc.nr_intensity = S.dlss5_intensity, rc.nr_structure = S.dlss5_structure, rc.nr_tone = S.dlss5_tone;
    rc.nr_saturation = S.dlss5_saturation;
    rc.dlss_dir = dlss_dir[0] ? dlss_dir : NULL, rc.log_dir = log_dir[0] ? log_dir : NULL;
    rc.raytracing = S.raytracing != 0, rc.rt_high = S.rt_high != 0;
    rc.ui_scale = 1; /* the game's screen at the window's own pixels */
    render_fx_attach(&fx_passes);   /* the effects' passes (render_particles.c, render_decals.c), each used only while it is on */
    started = render_init(&rc);
    render_set_fx(FX.particles, FX.decals, FX.water, FX.fog);
    out_w = w, out_h = h;
    if (started) render_set_dlss(false); /* no scene until a level comes */
    lv_number = 0, cam_reset = true;
    return started;
}

/* the interface layer's size (the window divided by a whole factor) */
UWV_API void uwv_ui_size(int *w, int *h)
{
    int s;
    if (!started) { *w = *h = 0; return; }
    render_ui_size(w, h, &s);
}

/* --- the world: the original's level, as its block holds it (the tiles and objects), with our textures, lights
 * and normal maps; the camera where the player stands --- */
static Level lv;
static Mesh m;                        /* the level's mesh and atlases (textures, object and creature pictures) */
static const unsigned char *lv_block; /* the game's level block (mapdata) */
static Light lights[MAX_LIGHTS];      /* the level's placed lights */
static u32 tiles[64 * 64 * TILE_LIGHTS];
static int nlights;
static int nbase;                    /* the level's own placed lights, before the burning things and the moving light */
static Light mv_light;               /* the moving light (moving_light 1) */
static unsigned burn_sig;            /* what burns where, as the lights were last made */
static void lights_rebuild(void);
static int mv_index = -1;            /* the moving light's place among the placed ones */
static bool mv_tried;
static float mv_a[2], mv_b[2];       /* the ends of its run (x, z) */
static size_t lv_mark;
static bool arena_ready;
static Camera cam = {{32, 1, 32}, 0, 0, 1.15f};

/* our data (the textures and the rest, extracted from the game's files): UWVIEW_DATA when set (uo-new's data for
 * the checks), else data\ in the folder the renderer found dlss\ in (the game's own folder: made on the first
 * run); the game's files: the folder the program was given (uwv_game_dir) */
static char game_dir[MAX_PATH];
UWV_API void uwv_game_dir(const char *dir) { snprintf(game_dir, sizeof game_dir, "%s", dir ? dir : ""); }
static void data_dirs(char *data, char *game)
{
    const char *e = getenv("UWVIEW_DATA");
    /* a flat folder (the program beside the game's own UW.EXE, no game\ folder): our own files
       are in uwr\, so that nothing of ours lands in the game's DATA\ (uwfront.c's uwf_our_data makes the same choice) */
    static int is_flat = -1;
    if (is_flat < 0 && root[0]) {
        static const char *const old_way[2] = {"game\\UW.EXE", "game\\uw.exe"}, *const flat[2] = {"UW.EXE", "uw.exe"};
        char p[MAX_PATH + 16];
        is_flat = 0;
        snprintf(p, sizeof p, "%s\\%s", root, old_way[0]);
        bool old = file_exists(p);
        if (!old) snprintf(p, sizeof p, "%s\\%s", root, old_way[1]), old = file_exists(p);
        if (!old)
            for (int k = 0; k < 2 && !is_flat; k++) snprintf(p, sizeof p, "%s\\%s", root, flat[k]), is_flat = file_exists(p);
    }
    if (e && *e) snprintf(data, MAX_PATH, "%s", e);
    else snprintf(data, MAX_PATH, "%s\\%s", root, is_flat > 0 ? "uwr" : "data");
    snprintf(game, MAX_PATH, "%s", game_dir);
    ui_game_dir(game);   /* (ui_start's fonts, when data\ui\fonts has none: the test build's data folder) */
}

/* level n (1-9) from the game's block: built when the level changes; 1 if there is a scene */
static bool base_read; /* the palette's work (below) */
static unsigned lv_shape;
static unsigned shape_hash(const u8 *b);
static void doors_find(void);
static void acoustic_built(void);
static void fx_level_change(int n);
static void fx_frame(void);
static bool lv_wait;   /* (uwv_level_wait, below) */
/* the fog and smoke (volfog.inc, included before fx_frame) */
static void vol_blast(const float p[3], float size);
static bool vol_covers(float x, float z, int smoke);
static void vol_frame(float dt);
static void vol_shot(float dt);
static void fx_decal(float x, float z, int kind, float size, const float rgb[3], int who);
static void fx_decal_screen(float x, float y, float z);
/* level n's scene: our level data, its lights and mesh, to the graphics card (the game's files read first, once);
   false: none */
static bool level_build(int n)
{
    const unsigned char *block = lv_block;
    char data[MAX_PATH], game[MAX_PATH];
    data_dirs(data, game);
    if (!arena_ready) {
        arena_init((size_t)1024 << 20);
        /* the game's own files are not extracted onto the disk: what extract_missing makes is
           kept in memory, this run's; a file on the disk under data\ is someone's own and is read first.
           UWVIEW_EXTRACT_TO_DISK=1: written out as before (the tools that work on the loose files) */
        {
            const char *e = getenv("UWVIEW_EXTRACT_TO_DISK");
            int made;
            size_t bytes = 0;
            clock_t t0 = clock();
            {
                /* where this run's files are (a folder that stands alone shows its own paths here) */
                static const char *const ours[3] = {"settings.txt", "acoustics.txt", "laigter\\walls\\000_n.png"};
                char p[MAX_PATH + 64], has[160] = "";
                for (int k = 0; k < 3; k++) {
                    snprintf(p, sizeof p, "%s\\%s", data, ours[k]);
                    snprintf(has + strlen(has), sizeof has - strlen(has), " %s %s,", ours[k], file_exists(p) ? "there" : "MISSING");
                }
                RLOG_I("view", "the game's own files in %s; ours in %s:%s", game, data, has);
            }
            RLOG_I("view", "reading the game's own files (textures, sprites, levels, texts)");
            if (!(e && *e && *e != '0')) file_memory_root(data);
            /* (about 0.4 s at a start; a cache file would save only half of it) */
            made = extract_missing(data, game);
            RLOG_I("view", "the game's files read: %d made, %d kept in memory (%zu KB), %ld ms", made, file_memory_count(&bytes), bytes >> 10,
                    (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC));
        }
        maps_make_if_missing(root, data, game);   /* the first run of a package without our relief maps (mapsmake.h) */
        RLOG_I("view", "reading the game's 3D models");
        models_load(game);   /* the original's 3D models (game\UW.EXE), for world_model_quads */
        lv_mark = arena_mark();
        arena_ready = true;
    }
    arena_reset(lv_mark);
    lv_number = 0;
    RLOG_I("view", "building level %d (its walls, floors, textures and lights)", n);
    clock_t bt[5];   /* (the parts of the build timed: a level's change is slow) */
    extern long png_stat_n, png_stat_ticks;
    png_stat_n = png_stat_ticks = 0;
    bt[0] = clock();
    if (!level_load(&lv, data, n)) {
        fprintf(stderr, "uwview: level %d: cannot read our level data in %s\n", n, data);
        return false;
    }
    level_from_block(&lv, block); /* the level as the game has it now */
    int nl = lights_load(data, &lv, lights, MAX_LIGHTS);
    if (nl < 0) nl = 0;
    lights_tiles(&lv, lights, nl, tiles);
    nlights = nbase = nl, mv_index = -1, mv_tried = false, burn_sig = 0;
    level_relief = false; /* the original's flat walls and floors, our normal maps on them */
    bt[1] = clock();
    bool mesh_ok = level_mesh(&lv, data, NULL, 0, &m);
    bt[2] = clock();
    if (!mesh_ok || !render_upload(&m)) {
        RLOG_E("view", "level %d: no mesh (the level's mesh not made or not taken by the graphics card)", n);
        return false;
    }
    render_set_lights(lights, nl, tiles);
    render_set_dlss(S.dlss != 0);
    fx_level_change(n);   /* the old level's particles gone */
    lv_number = n, cam_reset = true;
    lv_shape = shape_hash(block);
    doors_find();
    acoustic_built();  /* the sound's copy of the level is out of date (uwv_acoustic_epoch) */
    base_read = false; /* the cycling pixels found again in the new atlas */
    bt[3] = clock();
    hires_apply();     /* the upscaled textures (once: the renderer keeps them across levels) */
    bt[4] = clock();
    fprintf(stderr, "uwview: level %d: %d triangles, %d lights\n", n, m.indices / 3, nl);
    RLOG_I("view", "level %d's build: the level read %ld ms, its mesh and textures %ld ms, to the graphics card %ld ms, the larger textures %ld ms; %ld pictures unpacked in %ld ms", n,
            (long)((bt[1] - bt[0]) * 1000 / CLOCKS_PER_SEC), (long)((bt[2] - bt[1]) * 1000 / CLOCKS_PER_SEC),
            (long)((bt[3] - bt[2]) * 1000 / CLOCKS_PER_SEC), (long)((bt[4] - bt[3]) * 1000 / CLOCKS_PER_SEC), png_stat_n, png_stat_ticks * 1000 / CLOCKS_PER_SEC);
    return true;
}

/* level n (1-9) from the game's block: built when the level changes; 1 if there is a scene */
UWV_API int uwv_level(const unsigned char *block, int n)
{
    if (!started || !block || n < 1 || n > 9) return 0;
    lv_block = block;
    /* (a level whose shape changed is built again here, where the front end holds the game, not later in
       things_update) */
    if (n == lv_number && shape_hash(block) == lv_shape) return 1;
    clock_t t0 = clock();
    bool ok = level_build(n);
    RLOG_I("view", "level %d's scene took %ld ms to build", n, (long)((clock() - t0) * 1000 / CLOCKS_PER_SEC));
    return ok;
}

/* 1: level n's scene is there (uwv_level would build nothing). The front end holds the game still while a scene
   is built (otherwise the player could be attacked on a level not yet shown) */
/* (0: the scene is level n's as the block has it; 1: another level, to be built; 2: the same level number but the
   block's shape changed: the game reading a new level in, its number not yet the new one, or a trap's change) */
UWV_API int uwv_level_state(const unsigned char *block, int n)
{
    if (!started || !block || n < 1 || n > 9 || n != lv_number) return 1;
    return shape_hash(block) == lv_shape ? 0 : 2;
}
/* 1: the game's block is not to be read (it is being filled: a level change under way); the scene and its things
   stay as they are until the front end says 0 */
static bool lv_wait;
UWV_API void uwv_level_wait(int on) { lv_wait = on != 0; }

/* UWVIEW_COMPARE=PREFIX (the checks): when the window's screenshot is asked for, the camera and the picture are
 * held still, the frame is drawn 60 times with the normal maps on and read back (PREFIX-on.png), 60 more with them
 * off (PREFIX-off.png), and how much the two differ is printed: the same view with only the normal maps changed */
static int cmp_phase;                  /* 0 off, 1 the first 60 frames (on), 2 the next 60 (off), 3 done */
static int fx_steady_reset;            /* UWVIEW_STEADY: a comparison begins: the particles cleared, their emitters and
                                          rand() from the start, so that its picture N frames on is every run's */
static int cmp_frames;
static unsigned char *cmp_ui, *cmp_a;
/* the frame of each picture: 60 and 120; an effect's comparison (UWVIEW_COMPARE_FX) 64 and 128, so that both are at
 * the same place of DLSS's 16-frame jitter and only the effect differs */
static int cmp_at(int k)
{
    const char *cf = getenv("UWVIEW_COMPARE_FX");
    return (cf && *cf ? 64 : 60) * k;
}


/* the camera: where the player's eye is (tiles; y up from the floor's zero), the facing (radians, 0 north, east
 * positive) and the pitch (radians, up positive) */
static int cam_frames_still;           /* (the checks: frames the eye has not moved) */
/* Smooth motion (the Esc menu's row, settings.txt smooth_motion, 1 the default): the game moves
 * the eye in steps, at its own pace of 8 ticks of its 256 Hz clock (31.25 ms: the original's, kept for its physics),
 * and the backend hands us the newest each frame we draw (about 100 a second). Each new pose is eased into from the
 * one on show over one game step, so the picture moves every frame we draw; at most one step (31 ms) behind the
 * game. Presentation only: the game's own eye is untouched. A jump of more than 2 tiles (a teleport, a load, a level)
 * is shown at once. */
#define SMOOTH_STEP_MS 31.25
/* UWVIEW_STEADY=1 (the checks: tools/vkcompare.py): everything of ours that goes by the clock or the frame count is
   held, so that two runs of one renderer give the same picture: the carried light (uwv_frame), the renderer's frame
   count and flames' time (render_hold_time from the first frame: the shadow rays' and the water's seed), the panel's
   liquids, the swing light's pulse and the ankhs' halos */
static int steady_check(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("UWVIEW_STEADY"); v = e && *e && *e != '0'; }
    return v;
}
/* a steady clock in milliseconds (the smooth motion's) */
static double qpc_ms(void)
{
#ifndef _WIN32
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
#else
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#endif
}
static float ang_diff(float a, float b)   /* b - a, the short way round (radians) */
{
    float d = fmodf(b - a, 6.2831853f);
    if (d > 3.14159265f) d -= 6.2831853f;
    if (d < -3.14159265f) d += 6.2831853f;
    return d;
}
/* the eye's pose eased from the game's last step to its newest (Smooth motion; at once when it is off) */
static void smooth_pose(float *x, float *y, float *z, float *yaw, float *pitch)
{
    static bool have;
    static float from[5], to[5];
    static double t0;
    float tgt[5] = {*x, *y, *z, *yaw, *pitch}, P[5];
    double now = qpc_ms();
    if (!have || memcmp(tgt, to, sizeof tgt)) {
        float dx = tgt[0] - to[0], dz = tgt[2] - to[2];
        if (!have || !smooth_on || dx * dx + dz * dz > 4.0f) memcpy(from, tgt, sizeof tgt);
        else {                          /* from where the eye is shown now */
            float a = (float)((now - t0) / SMOOTH_STEP_MS);
            a = a < 0 ? 0 : a > 1 ? 1 : a;
            for (int k = 0; k < 5; k++) from[k] += (k == 3 ? ang_diff(from[k], to[k]) : to[k] - from[k]) * a;
        }
        memcpy(to, tgt, sizeof tgt), t0 = now, have = true;
    }
    float a = smooth_on ? (float)((now - t0) / SMOOTH_STEP_MS) : 1.0f;
    a = a < 0 ? 0 : a > 1 ? 1 : a;
    for (int k = 0; k < 5; k++) P[k] = from[k] + (k == 3 ? ang_diff(from[k], to[k]) : to[k] - from[k]) * a;
    *x = P[0], *y = P[1], *z = P[2], *yaw = P[3], *pitch = P[4];
}
/* the eye from the game, each frame: eased, a jump of over two tiles counted as a teleport (no motion, the marks gone) */
UWV_API void uwv_camera(float x, float y, float z, float yaw, float pitch)
{
    if (cmp_phase) return;             /* held still for the comparison */
    smooth_pose(&x, &y, &z, &yaw, &pitch);
    float d = (x - cam.pos[0]) * (x - cam.pos[0]) + (z - cam.pos[2]) * (z - cam.pos[2]);
    cam_frames_still = d < 1e-6f && yaw == cam.yaw ? cam_frames_still + 1 : 0;
    if (d > 4.0f) cam_reset = true; /* a teleport: no motion from the last frame */
    if (d > 4.0f && render_decals_count()) {   /* (the effects: a jump of the eye, a load among them: the marks gone, as never saved) */
        render_decals_clear();
        if (getenv("UWVIEW_FX_LOG")) RLOG_I("view", "decals: the eye jumped: cleared");
    }
    cam.pos[0] = x, cam.pos[1] = y, cam.pos[2] = z, cam.yaw = yaw, cam.pitch = pitch;
}

/* the player's light: the original's light level (LIGHTING.C's cur_light_level, SHADES.DAT's row: 0 darkest to 7,
 * from the brightest light carried or a light spell, PLAYDATA.C's FixPlayerEquips) as our torch at the eye. Even
 * level 0 shows what is next to the player, as the original's darkest row does. */
static int light_level;
UWV_API void uwv_light(int level)
{
    int l = level < 0 || level > 7 ? 0 : level;
    if (l != light_level) RLOG_I("view", "light level %d", l);
    light_level = l;
}

/* --- the game's palette (the VGA DAC, 6 bits an entry), each frame. Two things the original does with it that
 * our scene, drawn in its own colours, would miss:
 * - colour cycling (COLCYCLE.C's cycle_colors: entries 10h..17h and 30h..3Fh rotate; water, lava): the texture
 *   pixels that were those entries in DATA\PALS.DAT's palette 0 (found when a level is built) take the entry's
 *   colour now, and the atlas's texture rows go up again;
 * - fades and flashes (the whole palette dimmed or tinted: a blow's red, a fade to black): the scene is multiplied
 *   by how far the other entries have moved from palette 0, channel by channel. --- */
static u8 base_pal[768];
static bool base_read, base_ok;
static int cyc_n;
static int *cyc_at;        /* atlas pixel (y * ATLAS_SIZE + x) */
static u8 *cyc_idx;        /* its palette entry */
static u8 last_cyc[3 * 32];
static bool cycling(int i) { return (i >= 0x10 && i <= 0x17) || (i >= 0x30 && i <= 0x3F); }
static u8 c8(u8 v) { return (u8)((v << 2) | (v >> 4)); }
/* the atlas's texture pixels in palette 0's cycling colours (PALS.DAT read once), for uwv_palette */
static void cycle_find(void)
{
    free(cyc_at), free(cyc_idx), cyc_at = NULL, cyc_idx = NULL, cyc_n = 0;
    memset(last_cyc, 0xFF, sizeof last_cyc);
    if (!base_read) {
        char p[MAX_PATH];
        base_read = true;
        snprintf(p, sizeof p, "%s\\DATA\\PALS.DAT", game_dir);
        FILE *f = fopen(p, "rb");
        if (!f) RLOG_W("view", "cannot open %s: %s (no colour cycling, fades or flashes in the scene)", p, rlog_errno());
        if (f) base_ok = fread(base_pal, 1, 768, f) == 768, fclose(f);
        if (f && !base_ok) RLOG_W("view", "%s: shorter than one palette (no colour cycling, fades or flashes in the scene)", p);
    }
    if (!base_ok || !m.atlas) return;
    int cap = 1 << 16;
    cyc_at = malloc(sizeof(int) * (size_t)cap), cyc_idx = malloc((size_t)cap);
    if (!cyc_at || !cyc_idx) {
        RLOG_E("view", "the cycling colours' lists: out of memory (water and lava not cycling)");
        return;
    }
    for (int y = 0; y < 960; y++)   /* the textures' rows (row 1023: the cycling colours' row, below) */
        for (int x = 0; x < ATLAS_SIZE && cyc_n < cap; x++) {
            const u8 *px = m.atlas + ((size_t)y * ATLAS_SIZE + x) * 4;
            for (int i = 0x10; i <= 0x3F; i++) {
                if (!cycling(i)) continue;
                const u8 *b = base_pal + i * 3;
                if (abs(px[0] - c8(b[0])) <= 2 && abs(px[1] - c8(b[1])) <= 2 && abs(px[2] - c8(b[2])) <= 2) {
                    cyc_at[cyc_n] = y * ATLAS_SIZE + x, cyc_idx[cyc_n++] = (u8)i;
                    break;
                }
            }
        }
    RLOG_I("view", "%d texture pixels in the cycling colours", cyc_n);
}
/* the game's palette this frame: its fades and flashes as the scene's tint, its cycling colours into the atlas */
UWV_API void uwv_palette(const unsigned char *pal)
{
    if (!started || !pal) return;
    if (!base_read) cycle_find();
    if (!base_ok) return;
    /* fades and flashes */
    double cur[3] = {0}, base[3] = {0};
    for (int i = 1; i < 256; i++) {
        if (cycling(i)) continue;
        for (int c = 0; c < 3; c++) cur[c] += pal[i * 3 + c], base[c] += base_pal[i * 3 + c];
    }
    float t[3];
    bool on = false;
    for (int c = 0; c < 3; c++) {
        t[c] = base[c] > 0 ? (float)(cur[c] / base[c]) : 1.0f;
        if (t[c] > 2) t[c] = 2;
        if (fabsf(t[c] - 1.0f) > 0.02f) on = true;
    }
    render_set_tint(t[0], t[1], t[2], on);
    /* the cycling colours */
    if (cyc_n && memcmp(last_cyc, pal + 0x10 * 3, 8 * 3) | memcmp(last_cyc + 24, pal + 0x30 * 3, 16 * 3)) {
        memcpy(last_cyc, pal + 0x10 * 3, 8 * 3), memcpy(last_cyc + 24, pal + 0x30 * 3, 16 * 3);
        for (int k = 0; k < cyc_n; k++) {
            u8 *px = m.atlas + (size_t)cyc_at[k] * 4;
            const u8 *e = pal + cyc_idx[k] * 3;
            px[0] = c8(e[0]), px[1] = c8(e[1]), px[2] = c8(e[2]);
        }
        /* the cycling entries' colours now, at x = the entry in row 1023 (no texture there): the upscaled
         * textures' cycling texels read them (common.hlsli atlas_at) */
        for (int i = 0x10; i <= 0x3F; i++) {
            if (!cycling(i)) continue;
            u8 *px = m.atlas + ((size_t)1023 * ATLAS_SIZE + (size_t)i) * 4;
            px[0] = c8(pal[i * 3]), px[1] = c8(pal[i * 3 + 1]), px[2] = c8(pal[i * 3 + 2]), px[3] = 255;
        }
        render_update_atlas(m.atlas, 1024);
    }
}

/* the walls' and floors' upscaled textures into the renderer: data\upscaled\walls|floors\256|512\NNN.png, each at
 * its atlas place times 4 (256) or 8 (512), nearest texel when a file's size differs; a missing one: the original
 * texture, enlarged. A texel in one of palette 0's cycling colours (as cycle_find matches them) keeps the entry in
 * its alpha (common.hlsli). Off (texture_res 0): none. */
/* The relief maps with them (map_res, the Esc menu's three "... map detail" rows): the
 * picture is made twice as wide and its right half holds, at each texture's place, r, g the normal's x, y; b the
 * occlusion; a the height (common.hlsli hmaps_at). Each of the three comes from where its row says (eff[]: 0 the
 * original's maps, enlarged; 256 or 512 data\laigter\upscaled\walls|floors\256|512\NNN_m.png, nearest texel when
 * the picture's scale differs; a missing file: the original's). The picture's scale f is the largest any of the
 * textures and the three maps needs; `colours` off (Textures: Original): its left half is left empty and the shader
 * keeps the atlas's colours. eff NULL, or no room for the wider picture: the colours alone. */
static bool hires_build(int f, const int *eff, bool colours)
{
    bool maps = eff != NULL;
    int size = 1024 * f, wide = maps ? size * 2 : size;
    u8 *px = calloc((size_t)wide * size, 4);
    u8 *lut = base_ok && colours ? calloc((size_t)1 << 24, 1) : NULL;   /* colour -> cycling entry (0: none) */
    if (base_ok && colours && !lut) RLOG_W("view", "textures %d: the cycling colours' table: out of memory (water and lava not cycling in them)", tex_res);
    if (!px) {
        free(lut);
        RLOG_E("view", "textures %d%s: out of memory", tex_res, maps ? " with relief maps" : "");
        return false;
    }
    if (maps)   /* flat, no occlusion, no height where no texture is */
        for (int y = 0; y < size; y++) {
            u8 *d = px + ((size_t)y * wide + (size_t)size) * 4;
            for (int x = 0; x < size; x++, d += 4) d[0] = d[1] = 128, d[2] = d[3] = 255;
        }
    if (lut)
        for (int i = 0x10; i <= 0x3F; i++) {
            if (!cycling(i)) continue;
            int c[3] = {c8(base_pal[i * 3]), c8(base_pal[i * 3 + 1]), c8(base_pal[i * 3 + 2])};
            for (int r = c[0] - 2; r <= c[0] + 2; r++)
                for (int g = c[1] - 2; g <= c[1] + 2; g++)
                    for (int b = c[2] - 2; b <= c[2] + 2; b++) {
                        if (r < 0 || g < 0 || b < 0 || r > 255 || g > 255 || b > 255) continue;
                        u8 *e = &lut[(r << 16) | (g << 8) | b];
                        if (!*e) *e = (u8)i;   /* (cycle_find's order: the lowest entry first) */
                    }
        }
    char data[MAX_PATH], game[MAX_PATH], path[MAX_PATH + 64];
    data_dirs(data, game);
    int found = 0, mfound = 0, mwant = 0;
    for (int i = 0; i < 210 + 52; i++) {
        float r[4];
        level_texture_rect(i, r);
        int x0 = (int)r[0] * f, y0 = (int)r[1] * f, cell = (int)r[2] * f;
        size_t mark = arena_mark();
        Image img;
        if (colours) {
            snprintf(path, sizeof path, "%s\\upscaled\\%s\\%d\\%03d.png", data, i < 210 ? "walls" : "floors", tex_res, i < 210 ? i : i - 210);
            bool ok = png_read(path, &img) && img.w > 0 && img.h > 0;
            found += ok;
            for (int y = 0; y < cell; y++)
                for (int x = 0; x < cell; x++) {
                    const u8 *s = ok ? img.rgba + ((size_t)(y * img.h / cell) * img.w + (size_t)(x * img.w / cell)) * 4
                                     : m.atlas + ((size_t)((int)r[1] + y / f) * ATLAS_SIZE + (size_t)((int)r[0] + x / f)) * 4;
                    u8 *d = px + ((size_t)(y0 + y) * wide + (size_t)(x0 + x)) * 4;
                    d[0] = s[0], d[1] = s[1], d[2] = s[2], d[3] = 255;
                    u8 e = lut ? lut[(s[0] << 16) | (s[1] << 8) | s[2]] : 0;
                    if (e) d[3] = e;
                }
            arena_reset(mark);
        }
        if (!maps) continue;
        Image mi[2];             /* this texture's packed maps: [0] the 256 ones, [1] the 512 ones, as far as wanted */
        bool mok[2] = {false, false};
        for (int k = 0; k < 2; k++) {
            int res = k ? 512 : 256;
            if (eff[0] != res && eff[1] != res && eff[2] != res) continue;
            snprintf(path, sizeof path, "%s\\laigter\\upscaled\\%s\\%d\\%03d_m.png", data, i < 210 ? "walls" : "floors", res, i < 210 ? i : i - 210);
            mok[k] = png_read(path, &mi[k]) && mi[k].w > 0 && mi[k].h > 0;
            mwant++, mfound += mok[k];
        }
        for (int y = 0; y < cell; y++)
            for (int x = 0; x < cell; x++) {
                u8 *d = px + ((size_t)(y0 + y) * wide + (size_t)(size + x0 + x)) * 4;
                /* the original's: the normal atlas (4x: normal, height) and the occlusion atlas (1x) */
                const u8 *o = m.natlas + ((size_t)((int)r[1] * 4 + y * 4 / f) * NATLAS_SIZE + (size_t)((int)r[0] * 4 + x * 4 / f)) * 4;
                const u8 *s[3];
                for (int c = 0; c < 3; c++) {
                    int k = eff[c] == 512 ? 1 : eff[c] == 256 ? 0 : -1;
                    s[c] = k >= 0 && mok[k] ? mi[k].rgba + ((size_t)(y * mi[k].h / cell) * mi[k].w + (size_t)(x * mi[k].w / cell)) * 4 : NULL;
                }
                d[0] = s[0] ? s[0][0] : o[0], d[1] = s[0] ? s[0][1] : o[1];
                d[2] = s[1] ? s[1][2] : o[2];   /* (the original's occlusion: the normal atlas's blue, 4x) */
                d[3] = s[2] ? s[2][3] : o[3];
            }
        arena_reset(mark);
    }
    bool done = render_set_hires(px, size, f, maps, colours);
    free(px), free(lut);
    if (colours) rlog(done ? RLOG_INFO : RLOG_WARN, "view", "textures %d: %d of 262 upscaled files read%s", tex_res, found, done ? "" : ", NOT made (GPU memory)");
    if (maps) rlog(done ? RLOG_INFO : RLOG_WARN, "view", "relief maps (normal %d, occlusion %d, height %d): %d of %d map files read%s", eff[0], eff[1], eff[2], mfound, mwant, done ? "" : ", NOT made (GPU memory)");
    return done;
}

/* the upscaled textures and relief maps as the Esc rows have them, made again only when a choice changed */
static void hires_apply(void)
{
    int tf = tex_res == 512 ? 8 : tex_res == 256 ? 4 : 0, f = tf, eff[3];
    bool maps = false;
    for (int c = 0; c < 3; c++) {
        eff[c] = map_res[c] < 0 ? tex_res : map_res[c];
        if (eff[c]) maps = true;
        if (eff[c] == 512 && f < 8) f = 8;
        if (eff[c] == 256 && f < 4) f = 4;
    }
    if (!started || !m.atlas) return;
    if (tex_res == hires_built[0] && eff[0] == hires_built[1] && eff[1] == hires_built[2] && eff[2] == hires_built[3]) return;
    hires_built[0] = tex_res, hires_built[1] = eff[0], hires_built[2] = eff[1], hires_built[3] = eff[2];   /* (not tried again every frame) */
    if (!f) {
        render_set_hires(NULL, 0, 0, false, false);
        return;
    }
    if (tf && !base_read) cycle_find();   /* palette 0 (base_pal) */
    if (maps && m.natlas && m.saoatlas && hires_build(f, eff, tf > 0)) return;
    /* the maps' rows all at Original, or there was no room for both: the colours alone, or nothing */
    if (!tf || !hires_build(tf, NULL, true)) render_set_hires(NULL, 0, 0, false, false);
}

/* the eye's roll (get_eye's bank), radians */
UWV_API void uwv_roll(float roll) { cam.roll = roll; }

/* the original's lens: the tangents of the view's half angles across and up (uw's scale_matrix) */
static float lens_tx, lens_ty;          /* (uwv_pick's ray) */
UWV_API void uwv_lens(float tan_x, float tan_y) { render_set_lens(tan_x, tan_y), lens_tx = tan_x, lens_ty = tan_y; }
/* the original's zoom / 8000h (its view depth's scale, which its shading measures with) */
UWV_API void uwv_shade_scale(float k)
{
    const char *e = getenv("UWVIEW_SHADEK");   /* the checks: the depth scale tried by hand */
    render_set_shade_scale(e && *e ? (float)atof(e) : k);
}

/* the window's part the view fills (pixels of the output, from the top left) */
UWV_API void uwv_view_rect(int x0, int y0, int x1, int y1, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    render_set_view_rect((float)x0 / (float)w, (float)y0 / (float)h, (float)x1 / (float)w, (float)y1 / (float)h);
}

/* The moving light (moving_light 1, for the pictures): a glowing orb going along the hall the player stands in when
 * it is placed (at a new game, the starting hall: CHARGEN.C puts the player on square (0x20, 2)) and back: the
 * longer run of open tiles through the player's square, north-south or east-west. Its tile lists are made from the
 * run's middle with its reach widened to the whole run, so that it lights the hall wherever along it it is. */
static void moving_light_place(void)
{
    mv_tried = true;
    /* the starting hall: level 1's hall north from the start, x 31 (the game's tile x, y: our x, z), its slopes
     * at the south end included */
    int px = 31, pz = 5;
    if (px < 1 || px > 62 || pz < 1 || pz > 62 || nlights >= MAX_LIGHTS) return;
    int solid = T_SOLID, a[2], b[2];
    int z0 = pz, z1 = pz, x0 = px, x1 = px;
    while (z0 > 1 && lv.tile[(z0 - 1) * 64 + px].type != solid) z0--;
    while (z1 < 62 && lv.tile[(z1 + 1) * 64 + px].type != solid) z1++;
    while (x0 > 1 && lv.tile[pz * 64 + x0 - 1].type != solid) x0--;
    while (x1 < 62 && lv.tile[pz * 64 + x1 + 1].type != solid) x1++;
    if (z1 - z0 >= x1 - x0) a[0] = b[0] = px, a[1] = z0, b[1] = z1;
    else a[1] = b[1] = pz, a[0] = x0, b[0] = x1;
    if (a[0] == b[0] && a[1] == b[1]) return;
    mv_a[0] = a[0] + 0.5f, mv_a[1] = a[1] + 0.5f, mv_b[0] = b[0] + 0.5f, mv_b[1] = b[1] + 0.5f;
    float len = sqrtf((mv_b[0] - mv_a[0]) * (mv_b[0] - mv_a[0]) + (mv_b[1] - mv_a[1]) * (mv_b[1] - mv_a[1]));
    Light *L = &mv_light;
    memset(L, 0, sizeof *L);
    L->pos[0] = (mv_a[0] + mv_b[0]) * 0.5f, L->pos[2] = (mv_a[1] + mv_b[1]) * 0.5f;
    L->pos[1] = level_floor_y(&lv, L->pos[0], L->pos[2]) + 0.6f;
    L->color[0] = 1.0f, L->color[1] = 0.85f, L->color[2] = 0.6f;
    L->intensity = 1.0f, L->range = 4.0f; /* (1.6 blew the picture out passing the camera) */
    L->kind = LIGHT_ORB;
    mv_index = 0; /* placed by lights_rebuild, last */
    lights_rebuild();
    RLOG_I("view", "the moving light: along %.1f,%.1f to %.1f,%.1f (%.1f tiles)", mv_a[0], mv_a[1], mv_b[0], mv_b[1], len);
}

/* --- the things in the level: the objects lying about (world.c's picture cards) and the creatures, from the
 * game's own level block as it is now; rebuilt every frame (the block is the game's, read while it plays) --- */
static Vertex cards[RENDER_MAX_SPRITE_VERTS];

/* the doors (objects 0x140-0x147 shut, 0x148-0x14F open: game.c's game_level), each turning to where the game has
 * it now (a moving door, 0x1CF, by the way it goes), about a third of a second to swing */
static Door doors[128];
static int ndoors;
/* the level's doors from the game's block (up to 128), each as it stands now */
static void doors_find(void)
{
    ndoors = 0;
    for (int t = 0; t < 64 * 64 && ndoors < 128; t++)
        for (int i = lv.tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = lv.obj[i].next, guard++) {
            const Object *o = &lv.obj[i];
            if (o->item < 0x140 || o->item > 0x14F || ndoors >= 128) continue;
            Door *d = &doors[ndoors++];
            d->tx = t % 64, d->tz = t / 64, d->heading = o->heading & 6, d->item = o->item, d->obj = i;
            d->open = o->item >= 0x148 ? 1.0f : 0.0f;
        }
}
/* the doors' leaves, each swung toward the way the game has it now: their quads into out, the count */
static int door_quads(Vertex *out, int cap)
{
    static unsigned long long last;
    unsigned long long now = GetTickCount64();
    float step = last ? (float)(now - last) / 1000.0f * 3.0f : 0; /* about a third of a second a swing: the original's
                                                                   * 5 animation frames (USEITEMS.C's moveDoor) */
    last = now;
    int n = 0;
    for (int k = 0; k < ndoors; k++) {
        Door *d = &doors[k];
        int item = lv.obj[d->obj].item;
        float want;
        if (item == 0x1CF) {
            /* a moving door (USEITEMS.C's moveDoor: ITEM_MOVING_DOOR, the door's own kind kept in its owner field):
             * from shut (below 8) it is opening, from open it is closing, 5 frames (a portcullis 4) */
            int own = lv.obj[d->obj].owner & 15;
            d->item = 0x140 + (own & 7);
            want = own < 8 ? 1.0f : 0.0f;
        } else {
            if (item >= 0x140 && item <= 0x14F) d->item = item;
            want = d->item >= 0x148 ? 1.0f : 0.0f;
        }
        d->open = d->open < want ? MIN(want, d->open + step) : MAX(want, d->open - step);
        /* the door through the original's models (frame, leaf), unless UWVIEW_OLD_DOORS asks for our old slab */
        static int old_doors = -1;
        if (old_doors < 0) { const char *e = getenv("UWVIEW_OLD_DOORS"); old_doors = e && *e && *e != '0'; }
        if (old_doors || !models_ready()) n += world_door_quads(&lv, &m, d, out + n, cap - n);
        else n += world_door_model_quads(&lv, &m, d, &lv.obj[d->obj], out + n, cap - n);
    }
    return n;
}

/* --- the level for the sound (docs\superpowers\plans\2026-11-14-acoustics-ours.md, Task 2): the tiles and the doors as
 * the picture has them, for the game's side to cast its sound rays through (port\win32\uwacoustic.c). Presentation
 * only: read, never written. The structs are repeated in port\win32\plat_win32.c (the DLL has no header there). --- */
typedef struct { unsigned char type, height; unsigned short wall_tex, floor_tex; } UwvAcTile;   /* level.h's T_*, steps */
typedef struct { int id, tx, tz, heading, item, picture, shut; } UwvAcDoor;                     /* id: the door's object number */
static unsigned ac_epoch;

/* the tiles, [z * 64 + x] (z 0 = south); the textures are their file numbers (data\textures\walls\NNN.png, floors\NNN.png)
 * picked as level_mesh picks them for the tile's faces; *ceil_tex: the level's one ceiling, a floor texture (the
 * level's floor slot 9). 0 when no level is built. */
UWV_API int uwv_acoustic_grid(UwvAcTile out[64 * 64], int *ceil_tex)
{
    if (!lv_number || !out) return 0;
    for (int i = 0; i < 64 * 64; i++) {
        const Tile *t = &lv.tile[i];
        out[i].type = t->type, out[i].height = t->height;
        out[i].wall_tex = (unsigned short)MIN(lv.walls[t->wall % 48], 209);
        out[i].floor_tex = (unsigned short)MIN(lv.floors[t->floor % 10], 51);
    }
    if (ceil_tex) *ceil_tex = MIN(lv.floors[9], 51);
    return 1;
}

/* the doors as they stand this frame; the count (at most cap). item: 0x140-0x147 shut, 0x148-0x14F open, as the
 * picture has it now (a moving door by the way it goes); picture: the door's picture on this level
 * (data\sprites\doors\NNN.png: world.c's lv->doors[item & 7] under 6, the portcullis 12), -1 the secret door (it is
 * the wall); shut: less than half open. The portcullis is bars: never shut for the sound. */
UWV_API int uwv_acoustic_doors(UwvAcDoor *out, int cap)
{
    int n = 0;
    if (!lv_number || !out) return 0;
    for (int k = 0; k < ndoors && n < cap; k++) {
        const Door *d = &doors[k];
        int kind = (d->item - 0x140) & 7;
        UwvAcDoor *o = &out[n++];
        o->id = d->obj, o->tx = d->tx, o->tz = d->tz, o->heading = d->heading, o->item = d->item;
        o->picture = kind < 6 ? lv.doors[kind] : kind == 6 ? 12 : -1;
        o->shut = kind != 6 && d->open < 0.5f;
    }
    return n;
}

/* changes whenever level_build made a level (a new level, or this one's shape changed): the grid is to be read again */
UWV_API unsigned uwv_acoustic_epoch(void) { return ac_epoch; }
/* the level's 3D model objects (benches, tables, barrels, chests, pillars, boulders, the shrine...) as boxes, for the
 * sound: what stands in a room changes how it rings. As they stand when asked. */
typedef struct { int item; float lo[3], hi[3]; } UwvAcBox;
UWV_API int uwv_acoustic_things(UwvAcBox *out, int cap)
{
    static int item[256];
    static float lo[256][3], hi[256][3];
    if (!lv_number || !out) return 0;
    int n = world_model_boxes(&lv, item, lo, hi, cap < 256 ? cap : 256);
    for (int i = 0; i < n; i++) out[i].item = item[i], memcpy(out[i].lo, lo[i], sizeof lo[i]), memcpy(out[i].hi, hi[i], sizeof hi[i]);
    return n;
}

/* a level was built; UWVIEW_ACOUSTIC_DUMP=FILE (the checks): what the three calls above give, added to FILE */
static void acoustic_built(void)
{
    static UwvAcTile t[64 * 64];
    static UwvAcDoor d[128];
    const char *path = getenv("UWVIEW_ACOUSTIC_DUMP");
    int ceil = -1, nd;
    FILE *f;
    ac_epoch++;
    if (!path || !*path || !uwv_acoustic_grid(t, &ceil)) return;
    if (!(f = fopen(path, "a"))) {
        RLOG_E("view", "cannot open %s: %s (the acoustic grid not written)", path, rlog_errno());
        return;
    }
    nd = uwv_acoustic_doors(d, 128);
    fprintf(f, "acoustic epoch %u level %d ceil %d doors %d\n", uwv_acoustic_epoch(), lv_number, ceil, nd);
    for (int i = 0; i < 64 * 64; i++)
        fprintf(f, "tile %d %d %d %d %d %d\n", i % 64, i / 64, t[i].type, t[i].height, t[i].wall_tex, t[i].floor_tex);
    for (int i = 0; i < nd; i++)
        fprintf(f, "door %d %d %d %d %d %d %d\n", d[i].id, d[i].tx, d[i].tz, d[i].heading, d[i].item, d[i].picture, d[i].shut);
    fprintf(f, "end\n");
    {
        bool bad = ferror(f) != 0;
        if (fclose(f) != 0) bad = true;
        if (bad) RLOG_E("view", "cannot write %s: %s", path, rlog_errno());
    }
}

/* the level's shape (each square's type, heights, textures; not its objects): a change (a trigger that opens a
 * passage, a secret door) builds the mesh again */
static unsigned shape_hash(const u8 *b)
{
    unsigned h = 2166136261u;
    for (int t = 0; t < 4096; t++) {
        unsigned w = rd16(b + t * 4) | (unsigned)(rd16(b + t * 4 + 2) & 63) << 16;
        h = (h ^ w) * 16777619u;
    }
    return h;
}
static bool level_build(int n);
static int crit_frames; /* frames with a creature in front, in a row (UWVIEW_SHOT_CRIT) */
static int crit_in_view; /* creatures within 4 tiles in front of the camera this frame */

/* a creature's frame as UW.EXE's DRAWOBJ.C chooses it (game.c's core_creature_frame, the same): the sequence
 * (byte 0x15, bits 0-5) and the frame in it (the word at 0x0b, bits 12-15); standing and walking (sequence 0x20 and
 * up) by the creature's heading against the viewer's (dirtab, eight views: page 1 from slot 0x20 + ((sequence -
 * 0x20) << 3) + view), the other sequences but dying from the side or behind as the standing view, and the rest
 * (combat, attacks, firing, backing off, dying, casting) from page 0's slot */
static const u8 DIRTAB[32] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 0, 0};
/* --- the light of burning things lying in the world (ours: the original lights nothing but the player): a lit
 * lantern, torch, candle or taper (0x94-0x97, items.h's FIRST_LIT_LIGHT: a lit light is its unlit item + 4) gives
 * the light our engine gives the player's (inventory.c's inv_light): the brightness from the game's light table
 * (data\objects\lights.txt, as extracted; the defaults are its values), intensity 0.5 + 0.4 a level, the reach the
 * original's view distance for the level (SHADES.DAT, 3 3 4 4 5 6 7 7 tiles), each light's colour. A campfire
 * (0x12A) has no light in the game's data: ours gives it a torch's colour at brightness 4. Made again when
 * what burns where changes. */
static u8 light_bright[16] = {0, 0, 0, 0, 4, 2, 1, 3};
static bool light_table_read;
/* our light table (which things burn, and how bright) from our data, once a level has been built */
static void read_light_table(void)
{
    char data[MAX_PATH], game[MAX_PATH], p[MAX_PATH], line[256];
    if (!arena_ready) return;   /* (file_read's arena: after the first level is built) */
    light_table_read = true;
    data_dirs(data, game);
    snprintf(p, sizeof p, "%s\\objects\\lights.txt", data);
    size_t mark = arena_mark();
    Bytes f = file_read(p);   /* (the disk's, or the one kept in memory) */
    for (size_t at = 0; at < f.size;) {
        size_t n = 0;
        while (at < f.size && f.data[at] != '\n') {
            if (n + 1 < sizeof line) line[n++] = (char)f.data[at];
            at++;
        }
        at++, line[n] = 0;
        int item, dur, bright;
        if (sscanf(line, "light %d %d %d", &item, &dur, &bright) == 3 && item >= 0x90 && item <= 0x9F)
            light_bright[item - 0x90] = (u8)(bright < 0 ? 0 : bright > 15 ? 15 : bright);
    }
    arena_reset(mark);
}
static Light burning[256];
static int nburning;
/* the burning and glowing things in the level now, as lights (burning[]); the signature of what burns where */
static unsigned burning_find(void)
{
    static const float REACH[8] = {3, 3, 4, 4, 5, 6, 7, 7};
    static const float C[4][3] = {{1.0f, 0.84f, 0.58f}, {1.0f, 0.70f, 0.40f}, {1.0f, 0.78f, 0.50f}, {1.0f, 0.80f, 0.55f}};
    if (!light_table_read) read_light_table();
    unsigned sig = 2166136261u;
    sig = (sig ^ (unsigned)(GL.glow_things + (int)(GL.moonstone * 1000.0f) * 2 + (int)(GL.elemental * 1000.0f) * 4096)) * 16777619u;   /* (the Esc rows) */
    nburning = 0;
    for (int t = 0; t < 4096; t++)
        for (int i = lv.tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024 && nburning < 256; i = lv.obj[i].next, guard++) {
            const Object *o = &lv.obj[i];
            int b;
            if (o->item >= 0x94 && o->item <= 0x97) b = light_bright[o->item - 0x90];
            else if (o->item == 0x12A) b = 4;
            else if (o->item == 0xAD || o->item == 0x157) {   /* an ankh (the pendant, a shrine's standing ankh): a soft, steady,
                                                               golden holy light round it (ours) */
                Light *L = &burning[nburning++];
                memset(L, 0, sizeof *L);
                world_object_pos(&lv, t % 64, t / 64, o, L->pos);
                L->pos[1] += o->item == 0x157 ? 1.15f : 0.2f;   /* the standing ankh: above its loop (the mesh is lit from outside, not from within) */
                L->color[0] = 1.0f, L->color[1] = 0.92f, L->color[2] = 0.7f;
                L->intensity = o->item == 0x157 ? 0.35f : 1.0f, L->range = o->item == 0x157 ? 2.5f : 2.0f;   /* the standing ankh is a mesh now (world_model_quads): a soft glow round it, not a blaze on it */
                L->kind = LIGHT_WALL_TORCH;
                L->flame = 0.0f;
                L->kelvin = 0.0f;
                sig = (sig ^ (unsigned)(i * 131 + o->item)) * 16777619u;
                continue;
            }
            else if (o->item == 0x1CB || (o->item >= 0x1C2 && o->item <= 0x1C4) || o->item == 0x1C7) {
                /* a spark or an explosion: a flash of light while it lasts (hot white-orange; an explosion brighter
                   and wider), lighting and shadowing what is round it through the ray tracing */
                bool spark = o->item == 0x1CB;
                Light *L = &burning[nburning++];
                memset(L, 0, sizeof *L);
                world_object_pos(&lv, t % 64, t / 64, o, L->pos);
                L->pos[1] += 0.1f - (spark ? 8.0f / 32.0f : 0.0f);   /* a spark's flash where the blade struck (as its drawing) */
                L->color[0] = 1.0f, L->color[1] = spark ? 0.85f : 0.7f, L->color[2] = spark ? 0.6f : 0.35f;
                L->intensity = spark ? 1.6f : 3.0f, L->range = spark ? 2.0f : 3.0f;
                L->kind = LIGHT_WALL_TORCH;   /* no orb drawn (the spark card glows itself), a slight flicker */
                L->flame = spark ? 0.01f : 0.3f;   /* a spark crackles fast; an explosion billows */
                L->kelvin = spark ? 2600.0f : 2200.0f;   /* hot metal / a fireball, whiter than a flame */
                sig = (sig ^ (unsigned)(i * 131 + o->item)) * 16777619u;
                continue;
            }
            else if ((o->item == 0x129 || o->item == 0x117 || o->item == 0x112) && GL.glow_things) {
                /* a glowing rock, an orb, an orb rock: a steady glow
                   in the picture's own colour reaching a foot (a quarter of a tile: 1.2 m a tile) */
                static const float GC[3][3] = {{0.45f, 0.65f, 1.0f}, {0.3f, 0.8f, 0.95f}, {0.75f, 0.75f, 0.95f}};
                Light *L = &burning[nburning++];
                memset(L, 0, sizeof *L);
                world_object_pos(&lv, t % 64, t / 64, o, L->pos);
                L->pos[1] += o->item == 0x117 ? 0.2f : 0.12f;   /* (an orb sits on its stand) */
                memcpy(L->color, GC[o->item == 0x129 ? 0 : o->item == 0x117 ? 1 : 2], 12);
                L->intensity = 1.5f, L->range = 0.25f, L->kind = LIGHT_GLOW;
                sig = (sig ^ (unsigned)(i * 131 + o->item)) * 16777619u;
                sig = (sig ^ (unsigned)(t * 8 + o->x + o->y * 64 + o->z * 4096)) * 16777619u;
                continue;
            }
            else if (o->item == 0x126 && GL.moonstone > 0) {
                /* the moonstone (a deep blue light that floods its room like a liquid, a tenth of a
                   candle): LIGHT_FLOOD, every open tile near it lit alike, round corners too (lights.c). A candle is
                   0.5 + 0.4 x 1 = 0.9 */
                Light *L = &burning[nburning++];
                memset(L, 0, sizeof *L);
                world_object_pos(&lv, t % 64, t / 64, o, L->pos);
                L->pos[1] += 0.1f;
                L->color[0] = 0.12f, L->color[1] = 0.25f, L->color[2] = 1.0f;
                L->intensity = 0.9f * GL.moonstone, L->range = 7.0f, L->kind = LIGHT_FLOOD;
                sig = (sig ^ (unsigned)(i * 131 + o->item)) * 16777619u;
                sig = (sig ^ (unsigned)(t * 8 + o->x + o->y * 64 + o->z * 4096)) * 16777619u;
                continue;
            }
            else if (o->item == 0x78 && GL.elemental > 0) {
                /* a fire elemental (a smouldering orange-yellow, 30% of a candle): a low flame's light going
                   about with it, a candle's reach (3 tiles), 1500 K (a smouldering orange-yellow), flickering */
                Light *L = &burning[nburning++];
                memset(L, 0, sizeof *L);
                world_object_pos(&lv, t % 64, t / 64, o, L->pos);
                L->pos[1] += 0.5f;
                L->color[0] = 1.0f, L->color[1] = 0.62f, L->color[2] = 0.22f;
                L->intensity = 0.9f * GL.elemental, L->range = 3.0f, L->kind = LIGHT_WALL_TORCH;
                L->flame = 0.05f, L->kelvin = 1500.0f;
                sig = (sig ^ (unsigned)(i * 131 + o->item)) * 16777619u;
                sig = (sig ^ (unsigned)(t * 8 + o->x + o->y * 64 + o->z * 4096)) * 16777619u;
                continue;
            }
            else continue;
            if (b <= 0) continue;
            Light *L = &burning[nburning++];
            memset(L, 0, sizeof *L);
            world_object_pos(&lv, t % 64, t / 64, o, L->pos);
            L->pos[1] += o->item == 0x12A ? 0.3f : 0.15f;
            memcpy(L->color, C[o->item == 0x12A ? 1 : (o->item - 0x94) & 3], 12);
            /* as far as the player's own torch reaches (they light like ours), 3 tiles; a campfire, which
               the game's light table does not have (ours: a good light), about twice a torch's
               brightness out to 5 tiles */
            if (o->item == 0x12A) L->intensity = 4.0f, L->range = 5.0f, L->flame = 0.5f;   /* a campfire: a big slow flame */
            else L->intensity = 0.5f + 0.4f * (float)b, L->range = REACH[b & 7];   /* the original's reach for its level (a torch 4) */
            /* the flame's size, for its flicker: a lantern's behind glass (small, steadier), a torch's, a candle's, a taper's */
            if (o->item >= 0x94 && o->item <= 0x97) { static const float W[4] = {0.02f, 0.05f, 0.012f, 0.015f}; L->flame = W[o->item - 0x94]; }
            /* the temperature, its colour: an oil lantern 2000 K, a pitch torch 1700 K, a wax candle or taper 1850 K, a
               wood campfire 1800 K (the soot's incandescence) */
            if (o->item >= 0x94 && o->item <= 0x97) { static const float K[4] = {2000, 1700, 1850, 1850}; L->kelvin = K[o->item - 0x94]; }
            else if (o->item == 0x12A) L->kelvin = 1800;
            (void)REACH;
            L->kind = LIGHT_WALL_TORCH; /* they flicker */
            sig = (sig ^ (unsigned)(i * 131 + o->item)) * 16777619u;
            sig = (sig ^ (unsigned)(t * 8 + o->x + o->y * 64 + o->z * 4096)) * 16777619u;
        }
    return sig;
}
/* the level's lights, then the burning things, then the moving light (its tiles from its run's middle, its reach
 * widened to the whole run so that it lights wherever along it it is) */
static void lights_rebuild(void)
{
    int n = nbase;
    for (int k = 0; k < nburning && n < MAX_LIGHTS - 1; k++) lights[n++] = burning[k];
    float keep = mv_light.range;
    if (mv_index >= 0 && mv_light.kind == LIGHT_ORB && mv_light.range > 0) {
        float len = sqrtf((mv_b[0] - mv_a[0]) * (mv_b[0] - mv_a[0]) + (mv_b[1] - mv_a[1]) * (mv_b[1] - mv_a[1]));
        lights[n] = mv_light, lights[n].range = keep + len * 0.5f;
        mv_index = n++;
    }
    lights_tiles(&lv, lights, n, tiles);
    if (mv_index >= 0) lights[mv_index].range = keep;
    render_set_lights(lights, n, tiles);
    nlights = n;
}

/* the animated objects (items 0x1C0-0x1CE: blood, mist, explosions, splashes, spell effects, smoke, a fountain, the
 * silver tree, damage; render type 0 in COMOBJ.DAT, the moving door 0x1CF is a model and drawn as its door): as
 * DRAWOBJ.C's do_obj draws them, ANIMO.GR's frame given by the owner field (EFFECT.C's do_animobj moves it on),
 * or the object's own class when that is 0; not when invisible */
/* the effects that glow and flash (ours): a blow's spark on a wall (ITEM_DAMAGE 0x1CB), the explosions
 * (0x1C2-0x1C4), a spell's effect (0x1C7); blood, mist, splashes, smoke do not */
static bool animo_glows(int item) { return item == 0x1CB || (item >= 0x1C2 && item <= 0x1C4) || item == 0x1C7; }
/* ---- the particles (our sparks): spawned when one of the original's effect objects first appears (a blow's
 * spark on a wall 0x1CB, blood 0x1C0, the explosions 0x1C2-0x1C4, the splashes 0x1C5-0x1C6, smoke 0x1C8, mist 0x1C1),
 * each kind its own burst; moved by their velocity and gravity, stopped by the floor (a spark bounces, a drop dies)
 * and by the solid tiles; drawn as small cards of the soft dot (level.c: ANIMO_CELL + 63) through the sprite path,
 * glowing (kind 4) but for smoke and mist (kind 3, lit as the things are). No engine: a few lines of arithmetic. ---- */
#define NPART 2048
typedef struct { float p[3], v[3], age, life, size, col0[3], col1[3]; int glow, bounce; } Particle;
static Particle parts[NPART];
static int nparts;
static unsigned long long part_t;
static float frand(void) { return (float)rand() / (float)RAND_MAX; }
static void gpu_part(const float p[3], const float v[3], float life, float size, const float c[3], int kind, float grav, float drag);
/* one particle of a burst: on the GPU when the effects' particles are on, else in our own list */
static void part_add(const float p[3], const float v[3], float life, float size, const float c0[3], const float c1[3], int glow, int bounce)
{
    if (FX.particles) {   /* the effects' particles (render_particles.c): the same burst on the GPU, twice as thick */
        int kind = glow && bounce ? 1 : size > 0.08f ? 3 : 2;
        float g = glow ? 7.0f : size > 0.08f ? -0.3f : 9.0f;
        for (int k = 0; k < 2; k++) {
            float w[3] = {v[0] * (0.8f + 0.4f * frand()), v[1] * (0.8f + 0.4f * frand()), v[2] * (0.8f + 0.4f * frand())};
            gpu_part(p, k ? w : v, life * (0.8f + 0.4f * frand()), size, c0, kind, g, 0.2f);
        }
        (void)c1;
        return;
    }
    if (nparts >= NPART) return;
    Particle *q = &parts[nparts++];
    memcpy(q->p, p, 12), memcpy(q->v, v, 12), memcpy(q->col0, c0, 12), memcpy(q->col1, c1, 12);
    q->age = 0, q->life = life, q->size = size, q->glow = glow, q->bounce = bounce;
}
/* the effects: the blood's colour by who bleeds (the creature's item, OBJECTS.DAT's names): green for the
 * slugs, the lizardmen and the spiders, grey dust for the undead, the golems and the wisp, else red */
static void blood_of(int item, float rgb[3])
{
    static const float RED[3] = {0.55f, 0.03f, 0.02f}, GREEN[3] = {0.16f, 0.42f, 0.05f}, DUST[3] = {0.16f, 0.15f, 0.14f};
    const float *c = RED;
    switch (item) {
    case 65: case 69: case 85: case 88: case 89: case 68: case 83: case 92: c = GREEN; break;   /* slugs, lizardmen, spiders */
    case 74: case 97: case 99: case 100: case 101: case 105: case 110: case 113: case 118:     /* skeletons, ghosts, ghouls */
    case 114: case 119: case 121: case 122: c = DUST; break;                                     /* golems, the wisp */
    }
    memcpy(rgb, c, 12);
}
/* the creature nearest the place x, z (tiles) within a tile and a half, from the game's block: its item, or 0 */
static int creature_near(float x, float z)
{
    const u8 *b = lv_block;
    int best = 0;
    float bd = 2.25f;
    if (!b) return 0;
    for (int i = 2; i < 256; i++) {
        size_t off = 0x4000 + (size_t)i * 27;
        int item = rd16(b + off) & 511;
        if (item < 0x40 || item >= 0x80) continue;
        u32 pw = rd16(b + off + 2), home = rd16(b + off + 0x16);
        float cx = (float)(home >> 10) + ((pw >> 13) + 0.5f) / 8.0f, cz = (float)((home >> 4) & 63) + (((pw >> 10) & 7) + 0.5f) / 8.0f;
        float d = (cx - x) * (cx - x) + (cz - z) * (cz - z);
        if (d < bd) bd = d, best = item;
    }
    return best;
}
/* the burst for an effect object at p; o's place in its tile tells the wall a blow's spark came off */
static void part_burst(const Object *o, const float p[3])
{
    static const float SPARK0[3] = {1.0f, 0.62f, 0.18f}, SPARK1[3] = {0.55f, 0.08f, 0.0f};   /* hot orange, cooling to a dull red */
    static const float BLOOD0[3] = {0.55f, 0.03f, 0.02f}, BLOOD1[3] = {0.3f, 0.01f, 0.01f};
    static const float EMBER0[3] = {1.0f, 0.65f, 0.25f}, EMBER1[3] = {0.7f, 0.12f, 0.0f};
    static const float SMOKE0[3] = {0.30f, 0.27f, 0.24f}, SMOKE1[3] = {0.18f, 0.17f, 0.16f};
    static const float DROP0[3] = {0.45f, 0.55f, 0.65f}, DROP1[3] = {0.25f, 0.32f, 0.4f};
    static const float MIST0[3] = {0.35f, 0.37f, 0.4f}, MIST1[3] = {0.2f, 0.21f, 0.23f};
    float nx = 0, nz = 0;   /* the wall the thing touches (as the writings hang): the spark flies off it */
    if (o->x == 0) nx = 1; else if (o->x == 7) nx = -1; else if (o->y == 0) nz = 1; else if (o->y == 7) nz = -1;
    int item = o->item;
    if (item == 0x1CB) {
        for (int k = 0; k < 28; k++) {
            float a = frand() * 6.2831853f, up = frand(), sp = 1.5f + frand() * 3.0f;
            float v[3] = {nx * (1.0f + frand()) + cosf(a) * 0.6f, 0.4f + up * 1.6f, nz * (1.0f + frand()) + sinf(a) * 0.6f};
            float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) + 1e-4f;
            for (int j = 0; j < 3; j++) v[j] = v[j] / l * sp;
            part_add(p, v, 0.3f + frand() * 0.5f, 0.011f + frand() * 0.006f, SPARK0, SPARK1, 1, 1);
        }
    } else if (item == 0x1C0) {
        float b0[3], b1[3];
        memcpy(b0, BLOOD0, 12), memcpy(b1, BLOOD1, 12);
        if (FX.particles) blood_of(creature_near(p[0], p[2]), b0), memcpy(b1, b0, 12);   /* the effects: by who bleeds */
        for (int k = 0; k < 14; k++) {
            float a = frand() * 6.2831853f, sp = 0.6f + frand() * 1.6f;
            float v[3] = {cosf(a) * sp * 0.7f, 0.8f + frand() * 1.4f, sinf(a) * sp * 0.7f};
            part_add(p, v, 0.5f + frand() * 0.4f, 0.02f, b0, b1, 0, 0);
        }
    } else if (item >= 0x1C2 && item <= 0x1C4) {
        vol_blast(p, 1.0f);   /* the fog and smoke: a puff of hot smoke, the air thrown outward */
        for (int k = 0; k < 44; k++) {
            float a = frand() * 6.2831853f, e = (frand() - 0.3f) * 3.1416f * 0.5f, sp = 1.5f + frand() * 4.0f;
            float v[3] = {cosf(a) * cosf(e) * sp, sinf(e) * sp + 1.0f, sinf(a) * cosf(e) * sp};
            part_add(p, v, 0.5f + frand() * 0.8f, 0.014f + frand() * 0.01f, EMBER0, EMBER1, 1, 1);
        }
        for (int k = 0; k < 10; k++) {
            float a = frand() * 6.2831853f, sp = 0.2f + frand() * 0.5f;
            float v[3] = {cosf(a) * sp, 0.5f + frand() * 0.5f, sinf(a) * sp};
            float q[3] = {p[0], p[1] + 0.2f, p[2]};
            part_add(q, v, 1.2f + frand() * 1.0f, 0.10f + frand() * 0.08f, SMOKE0, SMOKE1, 0, 0);
        }
    } else if (item == 0x1C5 || item == 0x1C6) {
        for (int k = 0; k < 22; k++) {
            float a = frand() * 6.2831853f, sp = 0.4f + frand() * 1.2f;
            float v[3] = {cosf(a) * sp, 1.8f + frand() * 2.2f, sinf(a) * sp};
            part_add(p, v, 0.5f + frand() * 0.4f, 0.016f, DROP0, DROP1, 1, 0);
        }
    } else if (item == 0x1C8 || item == 0x1C1) {
        for (int k = 0; k < 8; k++) {
            float a = frand() * 6.2831853f, sp = 0.1f + frand() * 0.3f;
            float v[3] = {cosf(a) * sp, 0.25f + frand() * 0.35f, sinf(a) * sp};
            float q[3] = {p[0] + (frand() - 0.5f) * 0.3f, p[1] + 0.1f + frand() * 0.3f, p[2] + (frand() - 0.5f) * 0.3f};
            part_add(q, v, 1.5f + frand() * 1.5f, 0.12f + frand() * 0.1f, item == 0x1C8 ? SMOKE0 : MIST0, item == 0x1C8 ? SMOKE1 : MIST1, 0, 0);
        }
    }
}
/* our own particles moved one frame: velocity, gravity, the floor; dead ones dropped */
static void part_update(void)
{
    unsigned long long now = GetTickCount64();
    float dt = part_t ? (float)(now - part_t) / 1000.0f : 0;
    part_t = now;
    if (dt > 0.05f) dt = 0.05f;
    for (int i = 0; i < nparts;) {
        Particle *q = &parts[i];
        q->age += dt;
        if (q->age >= q->life) { parts[i] = parts[--nparts]; continue; }
        float g = q->glow ? 7.0f : q->size > 0.08f ? -0.3f : 9.0f;   /* sparks and embers fall fast, drops faster, smoke rises */
        q->v[1] -= g * dt;
        float nx = q->p[0] + q->v[0] * dt, ny = q->p[1] + q->v[1] * dt, nz = q->p[2] + q->v[2] * dt;
        int tx = (int)floorf(nx), tz = (int)floorf(nz);
        if (tx < 0 || tx > 63 || tz < 0 || tz > 63 || lv.tile[tz * 64 + tx].type == 0) {   /* a wall: it stops */
            q->v[0] = -q->v[0] * 0.3f, q->v[2] = -q->v[2] * 0.3f;
            nx = q->p[0], nz = q->p[2];
        }
        float fy = level_floor_y(&lv, nx, nz) + q->size;
        if (ny < fy) {
            if (q->bounce && q->v[1] < -0.3f) ny = fy, q->v[1] = -q->v[1] * 0.35f, q->v[0] *= 0.6f, q->v[2] *= 0.6f;
            else if (q->bounce) ny = fy, q->v[1] = 0, q->v[0] *= 0.8f, q->v[2] *= 0.8f;
            else { parts[i] = parts[--nparts]; continue; }   /* a drop: gone on the floor */
        }
        q->p[0] = nx, q->p[1] = ny, q->p[2] = nz;
        i++;
    }
}
/* a halo: a large soft dot card at p, glowing in colour c (the ankhs' holy gold) */
static int halo_card(Vertex *out, const float p[3], float s, const float c[3])
{
    const int cell = DOT_CELL;
    float rect[4] = {(float)(cell % 32 * 32), (float)(1024 + cell / 32 * 32), 32, 32};
    static const float CX[6] = {-1, 1, 1, -1, 1, -1}, CY[6] = {0, 0, 1, 0, 1, 1};
    float base[3] = {p[0], p[1] - s, p[2]};
    for (int k = 0; k < 6; k++) {
        Vertex *v = &out[k];
        memset(v, 0, sizeof *v);
        memcpy(v->pos, base, 12), memcpy(v->prev, base, 12);
        v->nrm[0] = CX[k] * s, v->nrm[1] = CY[k] * 2 * s, v->nrm[2] = 0;
        v->uv[0] = CX[k] > 0 ? 31.999f : 0.0f, v->uv[1] = CY[k] > 0 ? 0.0f : 31.999f;
        memcpy(v->rect, rect, 16);
        v->color[0] = c[0], v->color[1] = c[1], v->color[2] = c[2], v->color[3] = 4.0f;
    }
    return 6;
}
/* the ankhs' halos (our holy glow): the pendant lying about, the shrine's standing ankh; breathing slowly */
static int halo_cards(Vertex *out, int cap)
{
    int n = 0;
    float t = steady_check() ? 0 : (float)(GetTickCount64() % 100000) / 1000.0f;
    for (int tt = 0; tt < 4096; tt++)
        for (int i = lv.tile[tt].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024 && n + 6 <= cap; i = lv.obj[i].next, guard++) {
            const Object *o = &lv.obj[i];
            if ((o->item != 0xAD && o->item != 0x157) || ((o->flags >> 5) & 1)) continue;
            float p[3];
            world_object_pos(&lv, tt % 64, tt / 64, o, p);
            float b = 0.85f + 0.15f * sinf(t * 1.3f + (float)i);
            float c[3] = {0.95f * b, 0.78f * b, 0.32f * b};
            if (o->item == 0x157) { p[1] += 0.69f; n += halo_card(out + n, p, 0.16f, c); }   /* in the loop's hole, out to the arms */   /* the standing ankh (model 11, drawn): a glint at its head */
            else { p[1] += 0.14f; n += halo_card(out + n, p, 0.16f, c); }               /* the pendant: a glint above it; the floor's glow is its light's */
        }
    return n;
}
/* our own particles as picture cards into out: the count of vertices */
static int part_cards(Vertex *out, int cap)
{
    int n = 0;
    static int cell = -1;   /* (UWVIEW_PARTCELL=N, a check's: another atlas cell for the particles' picture) */
    if (cell < 0) cell = getenv("UWVIEW_PARTCELL") ? atoi(getenv("UWVIEW_PARTCELL")) : DOT_CELL;
    float rect[4] = {(float)(cell % 32 * 32), (float)(1024 + cell / 32 * 32), 32, 32};
    static const float CX[6] = {-1, 1, 1, -1, 1, -1}, CY[6] = {0, 0, 1, 0, 1, 1};
    for (int i = 0; i < nparts && n + 6 <= cap; i++) {
        const Particle *q = &parts[i];
        float f = q->age / q->life, fade = f < 0.7f ? 1.0f : (1.0f - f) / 0.3f;
        float s = q->size * (q->glow ? 1.0f : 1.0f + f * 1.5f);   /* smoke grows */
        float c[3];
        for (int j = 0; j < 3; j++) c[j] = (q->col0[j] + (q->col1[j] - q->col0[j]) * f) * fade;
        float base[3] = {q->p[0], q->p[1] - s, q->p[2]};
        for (int k = 0; k < 6; k++) {
            Vertex *v = &out[n + k];
            memset(v, 0, sizeof *v);
            memcpy(v->pos, base, 12), memcpy(v->prev, base, 12);
            v->nrm[0] = CX[k] * s, v->nrm[1] = CY[k] * 2 * s, v->nrm[2] = 0;
            v->uv[0] = CX[k] > 0 ? 31.999f : 0.0f, v->uv[1] = CY[k] > 0 ? 0.0f : 31.999f;
            memcpy(v->rect, rect, 16);
            v->color[0] = c[0], v->color[1] = c[1], v->color[2] = c[2], v->color[3] = q->glow ? 4.0f : 3.0f;
        }
        n += 6;
    }
    return n;
}

/* the original's effect objects (0x1C0-0x1CE) seen this frame: a burst when one first appears, the glowing ones drawn */
static int animo_cards(Vertex *out, int cap)
{
    int n = 0;
    static unsigned seen[1024], seen_frame[1024];
    static unsigned frame;
    static int dbg = -1;
    if (dbg < 0) dbg = getenv("UWVIEW_ANIMODBG") != NULL;
    frame++;
    for (int t = 0; t < 4096; t++)
        for (int i = lv.tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024 && n + 6 <= cap; i = lv.obj[i].next, guard++) {
            const Object *o = &lv.obj[i];
            if (o->item < 0x1C0 || o->item > 0x1CE || ((o->flags >> 5) & 1)) continue;
            int f = o->owner > 0 ? o->owner : o->item - 0x1C0;
            float p[3];
            world_object_pos(&lv, t % 64, t / 64, o, p);
            /* a blow's spark: the original makes it 8 above where the blade met the wall (COMBAT.C find_wall_coll,
               SET_Z(curP->z + 8)); drawn where the blade struck (an inch above), the object untouched */
            if (o->item == 0x1CB) p[1] -= 8.0f / 32.0f;
            if (i < 1024) {   /* the effect as it first appears (or appears again in a reused object): its burst */
                seen_frame[i] = frame;
                if ((seen[i] >> 8) != (unsigned)o->item) {
                    part_burst(o, p);
                    if (FX.decals && o->item >= 0x1C2 && o->item <= 0x1C4) fx_decal(p[0], p[2], RD_SCORCH, 0.45f, NULL, 0);   /* (the effects) */
                    if (FX.water && (o->item == 0x1C5 || o->item == 0x1C6)) render_water_ripple(p[0], p[2], 1.5f);   /* a splash's ring */
                    if (dbg) RLOG_I("view", "effect 0x%03x frame %d (object %d) at %llu: %d particles", o->item, f, i, (unsigned long long)GetTickCount64(), nparts);
                }
                seen[i] = (unsigned)(o->item << 8 | f);
            }
            n += world_animo_card(&m, f, p, out + n, animo_glows(o->item));
        }
    for (int i = 0; i < 1024; i++) if (seen_frame[i] != frame) seen[i] = 0;   /* gone: the next one in its place is new */
    {   /* UWVIEW_PARTTEST=1 (a check's): a blow's spark burst a tile in front of the eye every second */
        static int pt = -1;
        static unsigned long long last;
        if (pt < 0) pt = getenv("UWVIEW_PARTTEST") != NULL;
        if (pt && GetTickCount64() - last > 1000) {
            Object fake;
            float p[3] = {cam.pos[0] + sinf(cam.yaw) * 1.2f, cam.pos[1] - 0.2f, cam.pos[2] + cosf(cam.yaw) * 1.2f};
            memset(&fake, 0, sizeof fake);
            fake.item = 0x1CB, fake.x = 3, fake.y = 7;
            part_burst(&fake, p);
            last = GetTickCount64();
            {
                const int cell = DOT_CELL;
                const u8 *px = m.atlas + ((size_t)(1024 + cell / 32 * 32 + 8) * ATLAS_SIZE + (size_t)(cell % 32 * 32 + 8)) * 4;
                RLOG_I("view", "parttest: burst at %.2f %.2f %.2f (eye %.2f %.2f %.2f yaw %.2f): %d particles; dot texel %d %d %d %d",
                        p[0], p[1], p[2], cam.pos[0], cam.pos[1], cam.pos[2], cam.yaw, nparts, px[0], px[1], px[2], px[3]);
            }
        }
    }
    part_update();
    {
        int n0 = n;
        n += part_cards(out + n, cap - n);
        n += halo_cards(out + n, cap - n);
        if (getenv("UWVIEW_PARTTEST") && n > n0) { static unsigned long long lt; if (GetTickCount64() - lt > 500) RLOG_I("view", "%d particle/halo cards", (n - n0) / 6), lt = GetTickCount64(); }
    }
    return n;
}

/* the picture (frame) for creature kind k in animation seq, frame fr, seen from view; -1 when it has none */
static int creature_frame(int k, int seq, int fr, int view)
{
    if (!m.crit[k].loaded) return -1;
    {   /* UWVIEW_CRIT_FORCE=SEQ,FR (a diagnosis): every creature drawn in that sequence and frame */
        static int force_seq = -2, force_fr;
        if (force_seq == -2) {
            const char *e = getenv("UWVIEW_CRIT_FORCE");
            force_seq = -1;
            if (e && *e) sscanf(e, "%x,%d", &force_seq, &force_fr);
        }
        if (force_seq >= 0) seq = force_seq, fr = force_fr;
    }
    /* the entry: the original (pgcache_x.c parse_cpg, L7D04) reads segment * 8 + the object's frame (0-15), so a
     * frame past 7 reads on into the next segment; an empty entry (0xFF) draws the page's frame 0 (L7D0B-L7D11),
     * only a missing slot (0xFF) draws nothing (L7CF3) */
    int slot1 = -1;
    if (seq >= 0x20) slot1 = ((seq - 0x20) << 3) + view;
    else if (seq != 0xC && ((view + 5) & 7) >= 3) slot1 = view;
    if (slot1 >= 0) {
        if (slot1 >= 128 || m.crit[k].base1 < 0) return -1;
        int sg = m.crit[k].slots1[slot1];
        if (sg == 255 || sg >= m.crit[k].nsegs1) return -1;
        int idx = sg * 8 + (fr & 15);
        int f = idx < m.crit[k].nsegs1 * 8 ? ((const u8 *)m.crit[k].segs1)[idx] : 255;
        if (f == 255) f = 0;
        return m.crit[k].base1 + f < m.crit[k].nframes ? m.crit[k].base1 + f : -1;
    }
    int sg = seq < 32 ? m.crit[k].slots[seq] : 255;
    if (sg == 255 || sg >= m.crit[k].nsegs) return -1;
    int idx = sg * 8 + (fr & 15);
    int f = idx < m.crit[k].nsegs * 8 ? ((const u8 *)m.crit[k].segs)[idx] : 255;
    if (f == 255) f = 0;
    return f >= m.crit[k].nframes ? -1 : f;
}
static unsigned crit_pass;   /* creature_cards's calls counted (a frame each) */
static int creature_cards(const u8 *b, Vertex *out, int cap)
{
    unsigned vh = (unsigned)(int)(cam.yaw * (65536.0f / 6.2831853f)) & 0xffff;
    int n = 0;
    crit_pass++;
    for (int t = 0; t < 4096; t++)
        for (int i = rd16(b + t * 4 + 2) >> 6, guard = 0; i > 0 && i < 1024 && guard < 1024; guard++) {
            size_t off = 0x4000 + (i < 256 ? (size_t)i * 27 : 256 * 27 + (size_t)(i - 256) * 8);
            int next = rd16(b + off + 4) >> 6, item = rd16(b + off) & 511;
            if (i > 1 && i < 256 && item >= 0x40 && item < 0x80 && n + 6 <= cap) { /* (1 is the player) */
                u32 pw = rd16(b + off + 2), home = rd16(b + off + 0x16);
                int seq = b[off + 0x15] & 0x3f, fr = rd16(b + off + 0x0b) >> 12, heading = (pw >> 7) & 7;
                int view = DIRTAB[(unsigned)((heading << 2) + 0x20 - (vh >> 11)) % 0x20];
                int f = creature_frame(item - 0x40, seq, fr, view);
                {   /* UWVIEW_CRIT_LOG=1 (a diagnosis): each creature within 8 tiles, a line when what we would draw changes */
                    static int crit_log = -1;
                    static int last[256];
                    if (crit_log < 0) crit_log = getenv("UWVIEW_CRIT_LOG") != NULL && *getenv("UWVIEW_CRIT_LOG") != '0';
                    if (crit_log) {
                        float cx = (float)(home >> 10) + ((float)(pw >> 13) + 0.5f) / 8.0f, cz = (float)((home >> 4) & 63) + ((float)((pw >> 10) & 7) + 0.5f) / 8.0f;
                        float dx = cx - cam.pos[0], dz = cz - cam.pos[2];
                        int state = seq << 16 | fr << 12 | view << 8 | (f & 0xff);
                        if (dx * dx + dz * dz < 64 && state != last[i]) {
                            last[i] = state;
                            RLOG_I("view", "crit %d item %03x t %d seq %02x fr %d view %d -> f %d at %.2f,%.2f z %d (home %d,%d) ms %llu cam %.2f,%.2f,%.2f yaw %.0f", i, item, t, seq, fr, view, f,
                                    cx, cz, (int)(pw & 127), home >> 10, (home >> 4) & 63, (unsigned long long)GetTickCount64(),
                                    cam.pos[0], cam.pos[1], cam.pos[2], cam.yaw * 57.2958f);
                        }
                    }
                }
                if (f >= 0) {
                    Object o = {0};
                    o.item = (u16)item, o.z = (u8)(pw & 127), o.heading = (u8)heading, o.y = (u8)((pw >> 10) & 7), o.x = (u8)(pw >> 13);
                    float p[3];
                    world_object_pos(&lv, home >> 10, (home >> 4) & 63, &o, p);
                    /* where it stood last frame (its motion vectors, for DLSS); a jump of a tile or more (a teleport, a
                     * new creature in the slot) and a creature not drawn last frame start afresh */
                    static float prev[256][3];
                    static unsigned seen[256];
                    const float *pv = p;
                    if (seen[i] == crit_pass - 1 && fabsf(prev[i][0] - p[0]) < 1.0f && fabsf(prev[i][2] - p[2]) < 1.0f) pv = prev[i];
                    n += world_creature_card_mv(&m, item - 0x40, f, p, pv, out + n);
                    memcpy(prev[i], p, 12), seen[i] = crit_pass;
                    float dx = p[0] - cam.pos[0], dz = p[2] - cam.pos[2];
                    if (dx * dx + dz * dz < 16 && dx * sinf(cam.yaw) + dz * cosf(cam.yaw) > 0.5f) crit_in_view++;
                }
            }
            i = next;
        }
    return n;
}
/* each frame: the objects, creatures, doors and burning lights from the game's block to the renderer */
static void things_update(void)
{
    if (!lv_number || !lv_block || lv_wait) return;
    crit_in_view = 0;
    unsigned shape = shape_hash(lv_block);
    /* the level's shape changed: nothing read from the block this frame; the front end's next uwv_level_state sees
       it, waits for the game's next frame and has the level built once, the game held (built here at
       once, it would be built from a block the game is still reading a new level into) */
    if (shape != lv_shape) return;
    level_from_block(&lv, lv_block);      /* the objects where the game has them now */
    unsigned sig = burning_find();
    if (sig != burn_sig) {
        burn_sig = sig;
        lights_rebuild();
        static int last_n = -1;
        if (nburning != last_n) RLOG_I("view", "%d burning things give light", nburning), last_n = nburning;
    }
    int n = world_sprites(&lv, &m, lights, nlights, cards, RENDER_MAX_SPRITE_VERTS - 84 * 128);
    n += door_quads(cards + n, RENDER_MAX_SPRITE_VERTS - n);
    n += world_model_quads(&lv, &m, cards + n, RENDER_MAX_SPRITE_VERTS - n);   /* the original's 3D models: the shrine's ankh, benches, boulders... */
    n += animo_cards(cards + n, RENDER_MAX_SPRITE_VERTS - n);
    int nc = creature_cards(lv_block, cards + n, RENDER_MAX_SPRITE_VERTS - n);
    render_set_sprites(cards, n + nc);
    static int last_nc = -1;
    if (nc / 6 != last_nc) RLOG_I("view", "%d object cards, %d creatures", n / 6, nc / 6), last_nc = nc / 6;
    crit_frames = crit_in_view > 0 ? crit_frames + 1 : 0;
}

/* What the window point (fx, fy: 0..1 across and down) shows of the level's things, as drawn: the ray through it
 * against each upright card (the shader's: through its bottom middle, facing the eye across the floor) of the
 * objects lying about (world_sprites) and the creatures (creature_cards), a little margin round each; one with a
 * solid square or the floor between the eye and it left out. out: up to max, nearest first, each the game's object
 * index | its tile << 10 | its distance in eighths of a tile << 22 (| bit 31: a door, by its drawn frame and leaf).
 * The window's uwlook.c picks from these. */
static float pick_card(const float o[3], const float d[3], const float pos[3], float x0, float x1, float y0, float y1)
{
    float n[3] = {cam.pos[0] - pos[0] + 1e-4f, 0, cam.pos[2] - pos[2]};
    float len = sqrtf(n[0] * n[0] + n[2] * n[2]), den, t, h[3], lx, ly, mg = 0.03f;
    n[0] /= len, n[2] /= len;
    den = d[0] * n[0] + d[2] * n[2];
    if (fabsf(den) < 1e-6f) return -1;
    t = ((pos[0] - o[0]) * n[0] + (pos[2] - o[2]) * n[2]) / den;
    if (t <= 0.02f) return -1;
    h[0] = o[0] + d[0] * t, h[1] = o[1] + d[1] * t, h[2] = o[2] + d[2] * t;
    lx = (h[0] - pos[0]) * -n[2] + (h[2] - pos[2]) * n[0], ly = h[1] - pos[1];
    if (lx < x0 - mg || lx > x1 + mg || ly < y0 - mg || ly > y1 + mg) return -1;
    /* the way there clear: no solid square, not under the floor */
    for (float s = 0.05f; s < t - 0.05f; s += 0.05f) {
        float x = o[0] + d[0] * s, y = o[1] + d[1] * s, z = o[2] + d[2] * s;
        if (x < 0 || z < 0 || x >= 64 || z >= 64 || level_solid_at(&lv, x, z) || y < level_floor_y(&lv, x, z) - 0.02f) return -1;
    }
    return t;
}
/* what lies under the screen point fx, fy (0..1): up to max objects, the nearest first; the count */
UWV_API int uwv_pick(float fx, float fy, int *out, int max)
{
    if (!started || !lv_number || !lv_block || max <= 0) return 0;
    float cy = cosf(cam.yaw), sy = sinf(cam.yaw), cp = cosf(cam.pitch), sp = sinf(cam.pitch);
    float f[3] = {sy * cp, sp, cy * cp}, r[3] = {cy, 0, -sy};
    float u[3] = {f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2], f[0] * r[1] - f[1] * r[0]};
    if (cam.roll != 0) {
        float c = cosf(cam.roll), s = sinf(cam.roll), r2[3], u2[3];
        for (int k = 0; k < 3; k++) r2[k] = r[k] * c + u[k] * s, u2[k] = u[k] * c - r[k] * s;
        memcpy(r, r2, sizeof r2), memcpy(u, u2, sizeof u2);
    }
    int ow, oh, os;
    render_ui_size(&ow, &oh, &os);
    float ty = lens_ty > 0 ? lens_ty : tanf(cam.fov_y * 0.5f), tx = lens_tx > 0 ? lens_tx : ty * (float)ow / (float)(oh > 0 ? oh : 1);
    float ax = (fx * 2.0f - 1.0f) * tx, ay = (1.0f - fy * 2.0f) * ty;
    float d[3] = {f[0] + r[0] * ax + u[0] * ay, f[1] + r[1] * ax + u[1] * ay, f[2] + r[2] * ax + u[2] * ay};
    float dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    d[0] /= dl, d[1] /= dl, d[2] /= dl;
    struct { float t; int v; } hit[64];
    int nh = 0;
    unsigned vh = (unsigned)(int)(cam.yaw * (65536.0f / 6.2831853f)) & 0xffff;
    const u8 *b = lv_block;
    for (int tz = 0; tz < 64; tz++)
        for (int tx2 = 0; tx2 < 64; tx2++) {
            int tile = tz * 64 + tx2;
            for (int i = lv.tile[tile].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024 && nh < 64; i = lv.obj[i].next, guard++) {
                const Object *o = &lv.obj[i];
                float pos[3], t = -1;
                if (world_item_shown(o) && o->item < MAX_SPRITES && m.sprite_w[o->item]) {
                    float hw = m.sprite_w[o->item] * sprite_unit(&m, o->item) * 0.5f;
                    world_object_pos(&lv, tx2, tz, o, pos);
                    t = pick_card(cam.pos, d, pos, -hw, hw, 0, m.sprite_h[o->item] * sprite_unit(&m, o->item));
                } else if (i > 1 && i < 256 && o->item >= 0x40 && o->item < 0x80) {   /* a creature (creature_cards) */
                    size_t off = 0x4000 + (size_t)i * 27;
                    u32 pw = rd16(b + off + 2), home = rd16(b + off + 0x16);
                    int seq = b[off + 0x15] & 0x3f, fr = rd16(b + off + 0x0b) >> 12, heading = (pw >> 7) & 7, k = o->item - 0x40;
                    int view = DIRTAB[(unsigned)((heading << 2) + 0x20 - (vh >> 11)) % 0x20];
                    int fi = creature_frame(k, seq, fr, view);
                    if (fi >= 0 && m.crit[k].loaded && fi < m.crit[k].nframes) {
                        Object c = {0};
                        float s = CREATURE_SCALE, w = m.crit[k].w[fi], h = m.crit[k].h[fi], hx = m.crit[k].hx[fi], hy = m.crit[k].hy[fi];
                        c.item = (u16)o->item, c.z = (u8)(pw & 127), c.heading = (u8)heading, c.y = (u8)((pw >> 10) & 7), c.x = (u8)(pw >> 13);
                        world_object_pos(&lv, home >> 10, (home >> 4) & 63, &c, pos);
                        t = pick_card(cam.pos, d, pos, -hx * s, (w - hx) * s, -(h - 1 - hy) * s, (hy + 1) * s);
                    }
                }
                if (t > 0) {
                    int q = (int)(t * 8.0f);
                    hit[nh].t = t, hit[nh].v = i | tile << 10 | (q > 511 ? 511 : q) << 22, nh++;
                }
            }
        }
    /* the doors as drawn (door_quads' frame and leaf: the empty opening is no part of them), marked with bit 31: the
     * window's uwl_pick takes one only when nothing else is there and the original's own pick finds no object (its
     * pick frame has the leaf alone, the frame in the wall's colour: otherwise the doors are hard to
     * target). Not a secret door (it would give itself away). The use and its reach stay the original's. */
    {
        static Vertex dv[1024];
        for (int k = 0; k < ndoors && nh < 64; k++) {
            const Door *dr = &doors[k];
            int item = lv.obj[dr->obj].item;
            if ((dr->item & 7) == 7 || (item != 0x1CF && (item < 0x140 || item > 0x14F))) continue;
            if (fabsf(dr->tx + 0.5f - cam.pos[0]) > 6.0f || fabsf(dr->tz + 0.5f - cam.pos[2]) > 6.0f) continue;
            int nv = models_ready() ? world_door_model_quads(&lv, &m, dr, &lv.obj[dr->obj], dv, 1024) : world_door_quads(&lv, &m, dr, dv, 1024);
            float best = -1;
            for (int v = 0; v + 2 < nv; v += 3) {   /* the ray against each triangle, either face */
                const float *p0 = dv[v].pos, *p1 = dv[v + 1].pos, *p2 = dv[v + 2].pos;
                float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]}, e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
                float pv[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
                float det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
                if (fabsf(det) < 1e-9f) continue;
                float tv[3] = {cam.pos[0] - p0[0], cam.pos[1] - p0[1], cam.pos[2] - p0[2]};
                float bu = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) / det;
                if (bu < 0 || bu > 1) continue;
                float qv[3] = {tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0]};
                float bv = (d[0] * qv[0] + d[1] * qv[1] + d[2] * qv[2]) / det;
                if (bv < 0 || bu + bv > 1) continue;
                float t = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) / det;
                if (t > 0.02f && (best < 0 || t < best)) best = t;
            }
            if (best < 0) continue;
            bool clear = true;                      /* the way there clear, as pick_card's */
            for (float s = 0.05f; s < best - 0.05f && clear; s += 0.05f) {
                float x = cam.pos[0] + d[0] * s, y = cam.pos[1] + d[1] * s, z = cam.pos[2] + d[2] * s;
                if (x < 0 || z < 0 || x >= 64 || z >= 64 || level_solid_at(&lv, x, z) || y < level_floor_y(&lv, x, z) - 0.02f) clear = false;
            }
            if (!clear) continue;
            int q = (int)(best * 8.0f);
            hit[nh].t = best, hit[nh].v = (int)(0x80000000u | (unsigned)(dr->obj | (dr->tz * 64 + dr->tx) << 10 | (q > 511 ? 511 : q) << 22)), nh++;
        }
    }
    for (int a = 1; a < nh; a++)   /* nearest first */
        for (int c = a; c > 0 && hit[c].t < hit[c - 1].t; c--) {
            float tt = hit[c].t; int vv = hit[c].v;
            hit[c].t = hit[c - 1].t, hit[c].v = hit[c - 1].v, hit[c - 1].t = tt, hit[c - 1].v = vv;
        }
    int n = nh < max ? nh : max;
    for (int k = 0; k < n; k++) out[k] = hit[k].v;
    return n;
}

/* The interface layer from the game's picture (sw by sh, 0x00RRGGBB) placed at x0..x1, y0..y1 of the layer (the rest
 * black), clear where clear[] (sw by sh) says the 3D view shows (NULL: nowhere). Here, in the DLL built with -O2,
 * by tables of columns and rows: the port's own C is built without optimising, where this took 17 ms a frame at
 * 1920x1080. */
static unsigned char *ui_buf;
static int ui_w, ui_h;
UWV_API const unsigned char *uwv_compose(const unsigned int *src, int sw, int sh, const unsigned char *clear,
                                         int x0, int y0, int x1, int y1)
{
    int uw, uh, us;
    if (!started) return NULL;
    render_ui_size(&uw, &uh, &us);
    if (uw != ui_w || uh != ui_h || !ui_buf) {
        free(ui_buf);
        ui_buf = malloc((size_t)uw * uh * 4);
        ui_w = uw, ui_h = uh;
        if (!ui_buf) {
            static int said;
            if (!said) said = 1, RLOG_E("view", "the %dx%d interface layer: out of memory", uw, uh);
            return NULL;
        }
    }
    static int *colx;
    static int ncol;
    if (ncol != uw) free(colx), colx = malloc(sizeof(int) * (size_t)uw), ncol = uw;
    if (!colx) {
        static int said;
        if (!said) said = 1, RLOG_E("view", "the interface layer's columns: out of memory");
        return NULL;
    }
    for (int x = 0; x < uw; x++) colx[x] = x >= x0 && x < x1 && x1 > x0 ? (x - x0) * sw / (x1 - x0) : -1;
    for (int y = 0; y < uh; y++) {
        unsigned *row = (unsigned *)(ui_buf + (size_t)y * uw * 4);
        if (y < y0 || y >= y1 || y1 <= y0) {
            for (int x = 0; x < uw; x++) row[x] = 0xFF000000u;
            continue;
        }
        int sy = (y - y0) * sh / (y1 - y0);
        const unsigned *srow = src + (size_t)sy * sw;
        const unsigned char *crow = clear ? clear + (size_t)sy * sw : NULL;
        for (int x = 0; x < uw; x++) {
            int sx = colx[x];
            if (sx < 0) { row[x] = 0xFF000000u; continue; }
            unsigned c = srow[sx];
            /* RGBA in memory: R, G, B, A bytes (the layer's format) from 0x00RRGGBB */
            unsigned a = crow && crow[sx] ? 0u : 0xFF000000u;
            row[x] = a | (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
        }
    }
    return ui_buf;
}

/* The full-screen HUD: the layer clear (our 3D view everywhere) but for the original's own pieces, cut from its
 * screen (sw by sh) where its code puts them (UI's PANELS.C: sprites placed in rows counted up from the screen's
 * foot) and drawn at the window's scale:
 *   the compass, change_sprite(level[2], 0x70, 0x44, 0x38, 0x20): x 112-167, rows 131-162 -> the bottom's middle;
 *   the flasks, FL_X = 0xF8 and 0x11C, 0x18 wide, from row 0x4A (screen row 125) to the liquid's foot (0x2C, row
 *   155, and the stand below) -> the red one at the bottom left, the blue at the bottom right, as ours had them;
 *   the message scroll's text (rows 169-199, x 16-303), its parchment keyed out -> above the compass.
 * Returns the layer, or NULL. */
/* the colours along a piece's border: what it stands on (stone, parchment), keyed out */
static int border_keys(const unsigned *src, int sw, int sx, int sy, int w, int h, unsigned *keys, int cap)
{
    int n = 0;
    for (int i = 0; i < 2 * (w + h); i++) {
        int x = i < w ? i : i < 2 * w ? i - w : i < 2 * w + h ? 0 : w - 1;
        int y = i < w ? 0 : i < 2 * w ? h - 1 : i < 2 * w + h ? i - 2 * w : i - 2 * w - h;
        unsigned c = src[(size_t)(sy + y) * sw + sx + x] & 0xFFFFFF;
        int k = 0;
        while (k < n && keys[k] != c) k++;
        if (k == n && n < cap) keys[n++] = c;
    }
    return n;
}
/* a piece drawn: the keyed colours left clear; ink >= 0 draws every pixel kept in that colour (0x00RRGGBB) */
static void hud_piece(unsigned *dst, int uw, int uh, const unsigned *src, int sw, int sh, int sx, int sy, int w, int h,
                      int dx, int dy, int scale, const unsigned *keys, int nkeys, long ink)
{
    for (int y = 0; y < h * scale; y++) {
        int ty = dy + y, ry = sy + y / scale;
        if (ty < 0 || ty >= uh || ry < 0 || ry >= sh) continue;
        for (int x = 0; x < w * scale; x++) {
            int tx = dx + x, rx = sx + x / scale;
            if (tx < 0 || tx >= uw || rx < 0 || rx >= sw) continue;
            unsigned c = src[(size_t)ry * sw + rx] & 0xFFFFFF;
            int k = 0;
            while (k < nkeys && keys[k] != c) k++;
            if (k < nkeys) continue;
            if (ink >= 0) c = (unsigned)ink;
            dst[(size_t)ty * uw + tx] = 0xFF000000u | (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
        }
    }
}
/* the rune shelf's runes and the active spells' icons as PANELS.C last showed them (the backend, each frame): runes
 * 0..0x17 (0x18 and up: an empty place), icons 0..0x14 (else none) */
static unsigned char hud_runes[3] = {0x18, 0x18, 0x18}, hud_spells[3] = {0xFF, 0xFF, 0xFF};
UWV_API void uwv_hud_magic(const unsigned char *runes, const unsigned char *spells)
{
    for (int i = 0; i < 3; i++) hud_runes[i] = runes[i], hud_spells[i] = spells[i];
    const char *t = getenv("UWVIEW_MAGIC_TEST");   /* (a check's: "R0,R1,R2,S0,S1,S2" shown in place of the game's) */
    int v[6];
    if (t && sscanf(t, "%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6)
        for (int i = 0; i < 3; i++) hud_runes[i] = (unsigned char)v[i], hud_spells[i] = (unsigned char)v[3 + i];
}
/* a picture (its alpha kept) at dx, dy, scale times */
static void hud_pic(unsigned *dst, int uw, int uh, const Pic *p, int dx, int dy, int scale)
{
    if (!p) return;
    for (int y = 0; y < p->h * scale; y++) {
        int ty = dy + y;
        if (ty < 0 || ty >= uh) continue;
        for (int x = 0; x < p->w * scale; x++) {
            int tx = dx + x;
            if (tx < 0 || tx >= uw) continue;
            const unsigned char *a = p->rgba + ((size_t)(y / scale) * p->w + x / scale) * 4;
            if (a[3] < 128) continue;
            dst[(size_t)ty * uw + tx] = 0xFF000000u | a[0] | (unsigned)a[1] << 8 | (unsigned)a[2] << 16;
        }
    }
}
/* the crosshair as uw.exe drew it: CURSORS.GR's picture 0 (the cross, 16 x 15, its centre pixel 7, 7), from our
 * ui.c, in the middle of the window at the HUD's scale. As the old version: picture 10 (the blue pointer) on
 * something to look at, 9 (the red ring) on a hostile creature in reach, none (-1) while a blow charges; each
 * centred on the window's middle (the cross by its centre pixel) */
static bool ui_ok;
static int hud_cursor;                 /* the picture (uwv_readout2), -1 none */
static int hud_held = -1;              /* the item held on the crosshair (uwv_held), -1 none */
UWV_API void uwv_held(int item) { hud_held = item; }
/* a file of the game's folder (rel, e.g. "DATA\\BLNKMAP.BYT") into out, at most cap bytes: the bytes read, 0 none */
int uwv_game_file(const char *rel, unsigned char *out, int cap)
{
    char data[MAX_PATH], game[MAX_PATH], p[MAX_PATH * 2];
    data_dirs(data, game);
    snprintf(p, sizeof p, "%s\\%s", game, rel);
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    int n = (int)fread(out, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

/* the interface's pictures and the game's fonts loaded (once, when first wanted): 1 when they are */
int uwv_ui_ready(void)
{
    if (!ui_ok) {
        char data[MAX_PATH], game[MAX_PATH];
        data_dirs(data, game);
        ui_ok = ui_start(data, out_w, out_h);
    }
    return ui_ok;
}
/* our crosshair (or the object held on it) drawn into the layer d; false when the interface is not ready */
static bool crosshair_ours(unsigned *d, int uw, int uh, int s)
{
    if (!uwv_ui_ready()) return false;
    if (hud_held >= 0) {   /* an object held on the crosshair (uwv_held): its picture there, as the old version drew the
                              thing on its pointer (inventory.c's draw_thing: twice its size, centred) */
        int odiv = 1;
        const Pic *o = ui_item_pic(hud_held, &odiv);   /* (ours when the Sprites tab says so: half the pixel) */
        if (o) {
            int k = 2 * s / odiv > 0 ? 2 * s / odiv : 1, x0 = uw / 2 - o->w * k / 2, y0 = uh / 2 - o->h * k / 2;
            for (int y = 0; y < o->h * k; y++)
                for (int x = 0; x < o->w * k; x++) {
                    const u8 *p = o->rgba + ((size_t)(y / k) * o->w + (size_t)(x / k)) * 4;
                    int tx = x0 + x, ty = y0 + y;
                    if (p[3] < 128 || tx < 0 || tx >= uw || ty < 0 || ty >= uh) continue;
                    d[(size_t)ty * uw + tx] = 0xFF000000u | p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16;
                }
            return true;
        }
    }
    if (hud_cursor < 0) return true;
    if (hud_cursor == 0) {
        /* the plain cross, thinner (to see more of what is under it): CURSORS.GR picture 0's size, gap and
         * colours along its arms (row 7 / column 7: the dark tips to the gold), but drawn at the window's pixels:
         * a gold core about a third of the picture's arm thickness with a one-pixel black edge */
        const Pic *c0 = ui_pic("ui\\cursors", 0);
        if (!c0 || c0->w < 15 || c0->h < 15) goto picture;
        int cx = uw / 2, cy = uh / 2, core = 2, half = core / 2;   /* 4 pixels thick in all: 2 gold + an edge each side */
        for (int axis = 0; axis < 2; axis++)
            for (int i = 0; i < 15; i++) {   /* along the arm: the picture's pixel i (centre 7) */
                const u8 *p = axis == 0 ? c0->rgba + ((size_t)7 * c0->w + (size_t)i) * 4 : c0->rgba + ((size_t)i * c0->w + 7) * 4;
                if (p[3] < 128 || i == 7) continue;   /* the gap, and the centre (drawn below) */
                unsigned col = 0xFF000000u | p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16;
                bool tip = !p[0] && !p[1] && !p[2];
                for (int a = 0; a < s; a++) {
                    int along = (i - 7) * s + a - s / 2;
                    for (int t = -half - 1; t <= core - half; t++) {
                        bool edge = t == -half - 1 || t == core - half;
                        int tx = axis == 0 ? cx + along : cx + t, ty = axis == 0 ? cy + t : cy + along;
                        if (tx < 0 || tx >= uw || ty < 0 || ty >= uh) continue;
                        d[(size_t)ty * uw + tx] = edge || tip ? 0xFF000000u : col;
                    }
                }
            }
        for (int y = -1; y <= 1; y++)   /* the centre: a small gold dot with its edge */
            for (int x = -1; x <= 1; x++) {
                int tx = cx + x, ty = cy + y;
                if (tx >= 0 && tx < uw && ty >= 0 && ty < uh) d[(size_t)ty * uw + tx] = x || y ? 0xFF000000u : 0xFF10C7FFu;
            }
        return true;
    }
picture:;
    const Pic *c = ui_pic("ui\\cursors", hud_cursor);
    if (!c) c = ui_pic("ui\\cursors", 0);
    if (!c) return false;
    int x0 = uw / 2 - (c->w - 1) / 2 * s, y0 = uh / 2 - (c->h - 1) / 2 * s;
    for (int y = 0; y < c->h * s; y++)
        for (int x = 0; x < c->w * s; x++) {
            const u8 *p = c->rgba + ((size_t)(y / s) * c->w + (size_t)(x / s)) * 4;
            int tx = x0 + x, ty = y0 + y;
            if (p[3] < 128 || tx < 0 || tx >= uw || ty < 0 || ty >= uh) continue;
            d[(size_t)ty * uw + tx] = 0xFF000000u | p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16;
        }
    return true;
}

/* uw.exe's mouse settings for the port's mouse-look: how fast (1 = as uw.exe) and whether inverted */
UWV_API void uwv_mouse(float *sensitivity, int *invert) { *sensitivity = S.mouse, *invert = S.invert_mouse; }

/* uw.exe's renderer keys: 0 DLSS 5 on / off (DLSS itself without it), 1 ray tracing High / Normal / Off, 2 the DLSS 5
 * style, 3 the normal maps on / off */
static void settings_save(const char *key, const char *value);
UWV_API void uwv_toggle(int what)
{
    if (!started) return;
    if (what == 0) {
        if (render_dlss5_available()) render_set_dlss5(!render_dlss5_on());
        else render_set_dlss(!render_dlss_on());
    } else if (what == 1) {
        bool high = render_rt_high_wanted(), on = render_raytracing();
        if (on && high) render_set_rt_high(false);                      /* High -> Normal */
        else if (on) render_set_raytracing(false);                       /* Normal -> Off */
        else render_set_raytracing(true), render_set_rt_high(true);      /* Off -> High */
    } else if (what == 2) {
        S.dlss5_style = (S.dlss5_style + 1) % 3;
        render_set_dlss5_style(S.dlss5_style);
    } else if (what == 3)
        S.normal_maps = !S.normal_maps;
    else if (what == 5) {   /* the objects' shadows ('.') */
        render_set_object_shadows(!render_object_shadows());
        settings_save("object_shadows", render_object_shadows() ? "1" : "0");
        RLOG_I("view", "object shadows %s", render_object_shadows() ? "on" : "off");
    }
    else if (what == 4) {   /* full bright (Shift+L) */
        render_set_fullbright(!render_fullbright());
        RLOG_I("view", "full bright %s", render_fullbright() ? "on" : "off");
    }
    RLOG_I("view", "%s", render_dlss_text());
}

/* a piece of the source picture (sx, sy, w x h) into dst at dx, dy, scale times; key: keycol left out */
static void hud_put(unsigned *dst, int uw, int uh, const unsigned *src, int sw, int sh, int sx, int sy, int w, int h,
                    int dx, int dy, int scale, bool key, unsigned keycol)
{
    for (int y = 0; y < h * scale; y++) {
        int ty = dy + y, ry = sy + y / scale;
        if (ty < 0 || ty >= uh || ry < 0 || ry >= sh) continue;
        for (int x = 0; x < w * scale; x++) {
            int tx = dx + x, rx = sx + x / scale;
            if (tx < 0 || tx >= uw || rx < 0 || rx >= sw) continue;
            unsigned c = src[(size_t)ry * sw + rx] & 0xFFFFFF;
            if (key && c == keycol) continue;
            dst[(size_t)ty * uw + tx] = 0xFF000000u | (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
        }
    }
}
/* the name of what the crosshair is on (the window's uwl_info, from the game's own pick and strings) and what can be
 * done with it, set each frame; drawn as the old version did: at the bottom right, out of the way, the name in its
 * gold and the hint line under it small and grey */
static char hud_readout[96], hud_hint[96];
UWV_API void uwv_readout2(const char *name, const char *hint, int cursor)
{
    snprintf(hud_readout, sizeof hud_readout, "%s", name ? name : "");
    snprintf(hud_hint, sizeof hud_hint, "%s", hint ? hint : "");
    hud_cursor = cursor;
}
UWV_API void uwv_readout(const char *text) { uwv_readout2(text, "", 0); }
static int pnl_x, pnl_y, pnl_w, pnl_h;   /* (the bottom panel's place: below, with its pictures) */
static void hud_readout_draw(unsigned *d, int uw, int uh, int s)
{
    (void)s;
    if ((!hud_readout[0] && !hud_hint[0]) || !ui_ok) return;
    Overlay *o = ui_overlay();
    ui_clear();
    int rx = o->w - 36;
    int top = pnl_w > 0 ? pnl_y / o->scale - 4 : o->h - 44;   /* (just above our bottom panel) */
    if (hud_readout[0]) ui_text(FONT_NORMAL, hud_readout, rx - ui_text_width(FONT_NORMAL, hud_readout), top - 20, 255, 230, 160);
    for (int yy = top > 26 ? (top - 26) * o->scale : 0; yy < uh; yy++) {
        int oy = yy / o->scale;
        if (oy >= o->h) break;
        const u8 *orow = o->px + (size_t)oy * o->w * 4;
        for (int xx = 0; xx < uw; xx++) {
            int ox = xx / o->scale;
            if (ox >= o->w) break;
            const u8 *p = orow + ox * 4;
            if (p[3] == 0) continue;
            unsigned a = p[3], na = 255 - a, c = d[(size_t)yy * uw + xx];
            unsigned r = (p[0] * a + (c & 0xFF) * na) / 255, g = (p[1] * a + (c >> 8 & 0xFF) * na) / 255, b = (p[2] * a + (c >> 16 & 0xFF) * na) / 255;
            d[(size_t)yy * uw + xx] = 0xFF000000u | r | g << 8 | b << 16;
        }
    }
    ui_text_apply(d, uw, uh);   /* (the interface font's smooth text over the overlay: ui.c) */
    /* the hint line (small, detailed and easy to see): the smooth font at the
       window's own pixels, not the game's small font blown up, light with a dark outline so it reads over any wall */
    if (hud_hint[0]) {
        int em = st_game_em(uh), tw = st_width(ST_REGULAR, em, hud_hint);   /* (the one size: smoothtext.h) */
        int tx = rx * o->scale - tw, ty = (top - 10) * o->scale, k = o->scale >= 3 ? 2 : 1;
        for (int oy = -k; oy <= k; oy++)
            for (int ox = -k; ox <= k; ox++)
                if (ox || oy) st_draw(d, uw, uh, tx + ox, ty + oy, ST_REGULAR, em, hud_hint, 0x000000);
        st_draw(d, uw, uh, tx, ty, ST_REGULAR, em, hud_hint, 0xECECF2);
    }
}

/* ---- our own HUD art (tools\hud_art.py cuts them into the headers) ---- */
#include "hud_gargoyle.h"
static int hud_eyes, hud_pow = -1;   /* the game's eyes (PANELS.C setting[SCR_EYES]: 1..3 while they blink after a
                                         blow, 1 the creature healthy, 3 near death; 0 at rest) and the blow's charge
                                         (COMBAT.C play_pow 0..100 while a swing charges, -1 none) */
UWV_API void uwv_hud_combat(int eyes, int pow)
{
    const char *t = getenv("UWVIEW_HUD_TEST");   /* (a check's: "EYES,POW" shown in place of the game's) */
    if (t && *t) sscanf(t, "%d,%d", &eyes, &pow);
    hud_eyes = eyes, hud_pow = pow;
}
/* a picture's channels at (fx, fy) in its pixels, bilinear */
static void art_sample(const u8 *p, int ch, int w, int h, float fx, float fy, float *o)
{
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > w - 1) fx = (float)(w - 1);
    if (fy > h - 1) fy = (float)(h - 1);
    int x0 = (int)fx, y0 = (int)fy, x1 = x0 + 1 < w ? x0 + 1 : x0, y1 = y0 + 1 < h ? y0 + 1 : y0;
    float ax = fx - x0, ay = fy - y0;
    for (int c = 0; c < ch; c++) {
        float a = p[((size_t)y0 * w + x0) * ch + c], b = p[((size_t)y0 * w + x1) * ch + c];
        float e = p[((size_t)y1 * w + x0) * ch + c], f = p[((size_t)y1 * w + x1) * ch + c];
        o[c] = (a + (b - a) * ax) + ((e + (f - e) * ax) - (a + (b - a) * ax)) * ay;
    }
}
/* colour (0..255) at alpha a (0..1) over the HUD's pixel (straight alpha, 0xAABBGGRR) */
static void hud_over(unsigned *px, float r, float g, float b, float a)
{
    if (a <= 0.002f) return;
    if (a > 1) a = 1;
    unsigned c = *px;
    float da = (c >> 24) / 255.0f, oa = a + da * (1 - a);
    float dr = (float)(c & 255), dg = (float)(c >> 8 & 255), db = (float)(c >> 16 & 255);
    float nr = (r * a + dr * da * (1 - a)) / oa, ng = (g * a + dg * da * (1 - a)) / oa, nb = (b * a + db * da * (1 - a)) / oa;
    nr = nr > 255 ? 255 : nr, ng = ng > 255 ? 255 : ng, nb = nb > 255 ? 255 : nb;
    *px = (unsigned)(oa * 255 + 0.5f) << 24 | (unsigned)nr | (unsigned)ng << 8 | (unsigned)nb << 16;
}
/* the gargoyle bar (flush with the top of the screen, centred, its eye holes showing the enemy's
   health): 168 * s wide (its own pixels at 1080 rows), its top on the screen's top. A
   blow that lands lights its eye holes, a little spilling onto the brow, in the struck creature's health, as the
   original's eyes blink in it (COMBAT.C set_screen_frame(7, 3 - hp * 3 / avghit)): green healthy, amber hurt, red
   near death; held 2.5 s after the blink, then fading. */
__attribute__((unused)) static void hud_gargoyle(unsigned *d, int uw, int uh, int s, int m)
{
    (void)m;
    static int lvl;
    static unsigned long long t_lvl;
    unsigned long long now = GetTickCount64();
    if (hud_eyes >= 1 && hud_eyes <= 3) lvl = hud_eyes, t_lvl = now;
    float age = (float)(now - t_lvl), I = !lvl ? 0 : age < 2500 ? 1 : age < 4000 ? 1 - (age - 2500) / 1500 : 0;
    if (I > 0) I *= 0.82f + 0.18f * sinf(age * 0.008f);
    static const float cols[4][3] = { {0, 0, 0}, {90, 255, 110}, {255, 185, 40}, {255, 45, 30} };
    const float *col = cols[lvl];
    int dw = 168 * s, dh = dw * GARG_H / GARG_W;
    int x0 = uw / 2 - dw / 2, y0 = 0;
    for (int y = 0; y < dh; y++) {
        int ty = y0 + y;
        if (ty < 0 || ty >= uh) continue;
        for (int x = 0; x < dw; x++) {
            int tx = x0 + x;
            if (tx < 0 || tx >= uw) continue;
            float fx = (x + 0.5f) * GARG_W / dw - 0.5f, fy = (y + 0.5f) * GARG_H / dh - 0.5f, c[4], e, h;
            art_sample(garg_rgba, 4, GARG_W, GARG_H, fx, fy, c);
            art_sample(garg_eyes, 1, GARG_W, GARG_H, fx, fy, &e);
            art_sample(garg_halo, 1, GARG_W, GARG_H, fx, fy, &h);
            float a = c[3] / 255, ee = e / 255 * I, hh = h / 255 * I * 0.45f;
            if (a <= 0.002f) continue;
            for (int k = 0; k < 3; k++)   /* the eye holes lit, their light spilling a little onto the stone round them */
                c[k] = c[k] * (1 - ee) + col[k] * ee * 1.4f + col[k] * hh * (1 - ee);
            hud_over(&d[(size_t)ty * uw + tx], c[0], c[1], c[2], a);
        }
    }
}
/* the bottom panel (the swing indicator a glowing light on top, the side tanks health and mana, the left shelf the
   active spells, the right shelf the runes, the paper area the text box, a heading strip in the compass's place in
   the middle): stone and brass across the bottom's middle with a glass tank at each end, 232 * s wide (928 at
   1080 rows), flush with the bottom, the black above its ends cut away (tools\hud_art.py panel2). In it: the swing
   light in the housing on top (hud_swing_light), the liquid in the tanks (tank_liquid), the heading strip in the middle
   plaque (hud_heading), the active spells in the left plaque and the runes in the right (pnl_icons), our message lines
   on the parchment, always shown (uwv_log). Its parts by the source picture's 1024 x 318 pixels. */
#include "hud_panel.h"
#define PNL_SRCW 1024
#define PNL_SRCH 318
static int pnl_x, pnl_y, pnl_w, pnl_h;   /* the panel in the layer's pixels this frame (uwv_hud; pnl_w 0: none) */
#define PNL_X(v) (pnl_x + (v) * pnl_w / PNL_SRCW)
#define PNL_Y(v) (pnl_y + (v) * pnl_h / PNL_SRCH)
__attribute__((unused)) static void pnl_place(int uw, int uh, int s, int m)
{
    int pw = 232 * s, room = uw - 2 * m;
    if (pw > room) pw = room;
    pnl_w = pw, pnl_h = pw * PNL_H / PNL_W, pnl_x = (uw - pw) / 2, pnl_y = uh - pnl_h;
}

/* health, mana and poison (PLAYER's health, maxhealth, play_mana, max_mana, poison: uwinv.c uwi_vitals through
   uwv_hud_vitals) for the tanks */
static int hud_vit[5] = { 1, 1, 1, 1, 0 };
UWV_API void uwv_hud_vitals(const int *v)
{
    for (int i = 0; i < 5; i++) hud_vit[i] = v[i];
    const char *t = getenv("UWVIEW_VIT_TEST");   /* (a check's: "HP,MAXHP,MANA,MAXMANA,POISON" in place of the game's) */
    if (t && *t) sscanf(t, "%d,%d,%d,%d,%d", &hud_vit[0], &hud_vit[1], &hud_vit[2], &hud_vit[3], &hud_vit[4]);
}
/* r, g, b (0..255, clamped) as a layer pixel */
static unsigned px_rgb(float r, float g, float b)
{
    r = r < 0 ? 0 : r > 255 ? 255 : r, g = g < 0 ? 0 : g > 255 ? 255 : g, b = b < 0 ? 0 : b > 255 ? 255 : b;
    return 0xFF000000u | (unsigned)r | (unsigned)g << 8 | (unsigned)b << 16;
}
static float bk_hash(int k) { unsigned h = (unsigned)k * 2654435761u; h ^= h >> 13; h *= 0x5BD1E995u; h ^= h >> 15; return (h & 0xFFFF) / 65535.0f; }

/* the tanks' liquid at the source's (sx, sy), one of the tanks' glass pixels: the level health / max (left: red, green
   while poisoned, as the original's flask) or mana / max (right: blue) from the bottom of the glass (rows 107-265), its
   surface rippling, a few bubbles rising; 0 above the level (the glass left as drawn) */
#define TANK_Y0 107
#define TANK_Y1 265
static int tank_liquid(int sx, int sy, float tm, float *rgb)
{
    static const float red[3][3] = {{255, 96, 76}, {196, 26, 22}, {96, 6, 8}}, grn[3][3] = {{170, 245, 100}, {60, 168, 40}, {18, 70, 12}},
                       blu[3][3] = {{140, 190, 255}, {42, 84, 228}, {14, 26, 110}};
    int right = sx >= PNL_SRCW / 2;
    float frac = right ? (hud_vit[3] > 0 ? (float)hud_vit[2] / hud_vit[3] : 0) : (hud_vit[1] > 0 ? (float)hud_vit[0] / hud_vit[1] : 0);
    frac = frac < 0 ? 0 : frac > 1 ? 1 : frac;
    if (frac <= 0) return 0;
    const float (*liq)[3] = right ? blu : hud_vit[4] ? grn : red;
    float tx0 = right ? 932 : 15, tw = right ? 80 : 75, th = TANK_Y1 - TANK_Y0;
    float gx = (sx - tx0) / tw, gy = (sy - TANK_Y0) / th;
    float level = 1 - frac + 0.010f * sinf(gx * 9.0f + tm * 3.1f + right * 2) + 0.006f * sinf(gx * 17.0f - tm * 4.3f);
    if (gy < level) return 0;
    float u = (gy - level) / (1 - level + 0.001f);
    for (int k = 0; k < 3; k++) rgb[k] = liq[1][k] + (liq[2][k] - liq[1][k]) * u;
    if ((gy - level) * th < 3) for (int k = 0; k < 3; k++) rgb[k] = liq[0][k];   /* its surface */
    for (int k = 0; k < 5; k++) {   /* bubbles */
        float bx = 0.15f + 0.7f * bk_hash(right * 31 + k), sp = 0.10f + 0.12f * bk_hash(right * 17 + k * 7);
        float by = 1 - fmodf(tm * sp + bk_hash(right * 7 + k * 3), 1.0f) * frac;
        float dx = (gx - bx) * tw, dy = (gy - by) * th;
        if (by > level && dx * dx + dy * dy < 6.0f) for (int q = 0; q < 3; q++) rgb[q] += 70;
    }
    return 1;
}
/* the bottom panel drawn into d, its tanks' liquid in them (kept, not drawn now: pnl_w stays 0) */
__attribute__((unused)) static void pnl_draw(unsigned *d, int uw, int uh)
{
    float tm = steady_check() ? 0 : (float)(GetTickCount64() % 1000000) / 1000.0f, liq[3];
    for (int y = 0; y < pnl_h; y++) {
        int ty = pnl_y + y, sy = y * PNL_H / pnl_h;
        if (ty < 0 || ty >= uh) continue;
        for (int x = 0; x < pnl_w; x++) {
            int tx = pnl_x + x, sx = x * PNL_W / pnl_w;   /* nearest: the pixel art's edges kept */
            if (tx < 0 || tx >= uw) continue;
            const u8 *p = pnl_rgba + ((size_t)sy * PNL_W + sx) * 4;
            if (p[3] < 128) continue;
            unsigned t = pnl_tank[(size_t)sy * PNL_W + sx];
            if (t && tank_liquid(sx, sy, tm, liq)) {   /* the liquid in the glass, a little of the glass's grain kept */
                float k = t / 255.0f * 0.9f;
                d[(size_t)ty * uw + tx] = px_rgb(p[0] + (liq[0] - p[0]) * k, p[1] + (liq[1] - p[1]) * k, p[2] + (liq[2] - p[2]) * k);
                continue;
            }
            d[(size_t)ty * uw + tx] = 0xFF000000u | p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16;
        }
    }
}

/* the swing indicator built into the housing on the panel's top (source x 430-602, rows 42-82): a lens, a capsule
   across its middle, dark at rest; as a blow charges (play_pow 0..100) it lights from a dim ember to orange and at full
   to a yellow-white that pulses, its glow spilling round the housing */
__attribute__((unused)) static void hud_swing_light(unsigned *d, int uw, int uh)
{
    const float cx = 516, cy = 62, hl = 46, rr = 8, R = 26;   /* (source pixels) the lens and its glow's reach */
    float c = hud_pow < 0 ? 0 : hud_pow / 100.0f, pulse = 1;
    if (hud_pow >= 100 && !steady_check()) pulse = 0.82f + 0.18f * sinf((float)(GetTickCount64() % 100000) * 0.012f);
    float col[3];
    {   /* ember -> orange -> yellow-white */
        static const float ember[3] = {120, 32, 10}, orange[3] = {255, 140, 30}, white[3] = {255, 238, 175};
        for (int k = 0; k < 3; k++)
            col[k] = c < 0.85f ? ember[k] + (orange[k] - ember[k]) * (c / 0.85f) : orange[k] + (white[k] - orange[k]) * ((c - 0.85f) / 0.15f);
    }
    float I = c > 0 ? (0.35f + 0.65f * c) * pulse : 0;
    int x0 = PNL_X((int)(cx - hl - rr - R)), x1 = PNL_X((int)(cx + hl + rr + R)), y0 = PNL_Y((int)(cy - rr - R)), y1 = PNL_Y((int)(cy + rr + R));
    for (int ty = y0; ty <= y1; ty++) {
        if (ty < 0 || ty >= uh) continue;
        for (int tx = x0; tx <= x1; tx++) {
            if (tx < 0 || tx >= uw) continue;
            float fx = (tx - pnl_x + 0.5f) * PNL_SRCW / pnl_w, fy = (ty - pnl_y + 0.5f) * PNL_SRCH / pnl_h;
            float ax = fabsf(fx - cx) - hl, dx = ax > 0 ? ax : 0, dy = fy - cy, dist = sqrtf(dx * dx + dy * dy) - rr;
            unsigned *p = &d[(size_t)ty * uw + tx];
            if (dist < -1.6f) {   /* the lens: brightest along its middle */
                float core = 1 - fabsf(dy) / rr;
                core = core < 0 ? 0 : core;
                float r = 28 + 10 * core, g = 14 + 4 * core, b = 10;   /* (dark glass at rest) */
                if (I > 0) r += (col[0] - r) * I * (0.6f + 0.4f * core), g += (col[1] - g) * I * (0.6f + 0.4f * core), b += (col[2] - b) * I * (0.6f + 0.4f * core);
                if (dy < -rr * 0.45f && dy > -rr * 0.75f && ax < -hl * 0.1f) r += 30, g += 30, b += 30;   /* a glint */
                *p = px_rgb(r, g, b);
            } else if (dist < 0.6f) {   /* its brass rim, lit above */
                *p = dy < 0 ? px_rgb(214, 172, 80) : px_rgb(84, 58, 20);
            } else if (I > 0 && dist < R) {   /* the glow round it */
                float g = 1 - dist / R;
                hud_over(p, col[0], col[1], col[2], g * g * 0.55f * I);
            }
        }
    }
}

/* the heading strip in the middle plaque (source x 432-596, rows 106-152), in place of the original's compass: the
   view's heading (cam.yaw: 0 north, east
   positive) with the eight points and a mark every 15 degrees sliding past a fixed pointer, 150 degrees across, fading
   at its ends; north in red */
__attribute__((unused)) static void hud_heading(unsigned *d, int uw, int uh, int s)
{
    int ax = PNL_X(432), ay = PNL_Y(106), aw = PNL_X(596) - ax, ah = PNL_Y(152) - ay;
    if (aw <= 4 || ah <= 4) return;
    static unsigned *buf;
    static int buf_n;
    if (aw * ah > buf_n) { free(buf); buf_n = aw * ah; buf = malloc((size_t)buf_n * sizeof *buf); if (!buf) { static int said; if (!said) said = 1, RLOG_E("view", "the heading strip: out of memory"); buf_n = 0; return; } }
    for (int i = 0; i < aw * ah; i++) buf[i] = 0xFF000000u | 9 | 7u << 8 | 5u << 16;   /* the strip's dark ground */
    float hdg = cam.yaw * 57.29578f, span = 120, ppd = aw / span, mid = aw / 2.0f;
    static const char *const pts[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    int em_big = 4 * s + 4, em_small = 3 * s + 3;
    for (int a = 0; a < 360; a += 15) {
        float rel = fmodf(a - hdg + 540.0f, 360.0f) - 180;
        if (fabsf(rel) > span / 2 + 10) continue;
        int x = (int)(mid + rel * ppd);
        int major = a % 45 == 0, len = major ? ah / 5 : ah / 9;
        for (int y = ah - 2 - len; y < ah - 2; y++)   /* the mark, up from the strip's foot */
            for (int w = 0; w < (major ? 2 : 1) * (s > 2 ? 2 : 1); w++)
                if (x + w >= 0 && x + w < aw) buf[(size_t)y * aw + x + w] = px_rgb(196, 156, 70);
        if (major) {
            const char *t = pts[a / 45];
            int em = a % 90 == 0 ? em_big : em_small, tw = st_width(ST_REGULAR, em, t), th = st_height(ST_REGULAR, em);
            unsigned rgb = a == 0 ? 0x70D8F0 :   /* (north cyan: no red text) */ a % 90 == 0 ? 0xF4D27A : 0xC8A65E;
            st_draw(buf, aw, ah, x - tw / 2, (ah - len - th) / 2 - s, ST_REGULAR, em, t, rgb);
        }
    }
    for (int y = 0; y < ah; y++)   /* the strip's ends fade into the ground; then onto the layer */
        for (int x = 0; x < aw; x++) {
            int ty = ay + y, tx = ax + x;
            if (ty < 0 || ty >= uh || tx < 0 || tx >= uw) continue;
            float e = fabsf(x - mid) / mid, f = e < 0.6f ? 1 : 1 - (e - 0.6f) / 0.4f;
            unsigned c = buf[(size_t)y * aw + x];
            float r = (float)(c & 255), g = (float)(c >> 8 & 255), b = (float)(c >> 16 & 255);
            d[(size_t)ty * uw + tx] = px_rgb(9 + (r - 9) * f, 7 + (g - 7) * f, 5 + (b - 5) * f);
        }
    /* the fixed pointer: a brass notch at the top and a thin line down the middle */
    int px = ax + aw / 2, k = 3 * s;
    for (int y = 0; y < k; y++)
        for (int x = -(k - y); x <= k - y; x++) {
            int tx = px + x, ty = ay + y;
            if (tx >= 0 && tx < uw && ty >= 0 && ty < uh) d[(size_t)ty * uw + tx] = px_rgb(236, 200, 104);
        }
    for (int y = ay + k; y < ay + ah; y++)
        if (y >= 0 && y < uh && px >= 0 && px < uw) hud_over(&d[(size_t)y * uw + px], 236, 200, 104, 0.45f);
}

/* three icons (pics, NULL an empty place) side by side, step source pixels apart, centred in the recess (x0..x1,
   y0..y1 of the source picture) at the largest whole scale that fits; first: which pic is leftmost (the spells: the
   first is the rightmost, as active_spells has them) */
__attribute__((unused)) static void pnl_icons(unsigned *d, int uw, int uh, const Pic *const pics[3], int step, int rev, int x0, int y0, int x1, int y1)
{
    int pw = 0, ph = 0;
    for (int i = 0; i < 3; i++) if (pics[i]) { if (pics[i]->w > pw) pw = pics[i]->w; if (pics[i]->h > ph) ph = pics[i]->h; }
    if (!pw) return;
    int rw = PNL_X(x1) - PNL_X(x0), rh = PNL_Y(y1) - PNL_Y(y0), tw = 2 * step + pw;
    int sc = rw * 9 / 10 / tw, sc2 = rh * 9 / 10 / ph;
    if (sc2 < sc) sc = sc2;
    if (sc < 1) sc = 1;
    int bx = PNL_X(x0) + (rw - tw * sc) / 2, by = PNL_Y(y0) + (rh - ph * sc) / 2;
    for (int i = 0; i < 3; i++) {
        int slot = rev ? 2 - i : i;
        if (pics[i]) hud_pic(d, uw, uh, pics[i], bx + slot * step * sc, by + (ph - pics[i]->h) * sc / 2, sc);
    }
}

static unsigned hud_frames, inv_frame = (unsigned)-100;   /* the HUD's frames, and the last our inventory panel was drawn in */
/* the original's 320 x 200 screen as the last uwv_hud had it (0x00RRGGBB): our inventory panel shows the top of the
 * original's own right-hand panel from it (uwinv_draw.c) */
static unsigned g_screen[320 * 200];
static int g_screen_ok;
const unsigned *uwv_screen(void) { return g_screen_ok ? g_screen : NULL; }
static int log_ours;   /* our message box is drawn (uwv_log): the original's scroll text not cut out */
static int readout_on; /* the crosshair's name and hint were drawn this frame (uwv_log draws them again over its box) */
static int hud_fight;  /* fight mode (uwv_fight): the first-person weapon shown */
UWV_API void uwv_fight(int on) { hud_fight = on; }
/* the interface layer for full-screen play: the original's panel pieces cut from its 320x200 screen, our crosshair
   and readout; NULL when not started or the screen is not 320x200 */
UWV_API const unsigned char *uwv_hud(const unsigned int *src, int sw, int sh)
{
    int uw, uh, us;
    if (!started || sw != 320 || sh != 200) return NULL;
    memcpy(g_screen, src, sizeof g_screen), g_screen_ok = 1;   /* the original's screen now (our panel's top) */
    render_ui_size(&uw, &uh, &us);
    if (uw != ui_w || uh != ui_h || !ui_buf) {
        free(ui_buf);
        ui_buf = malloc((size_t)uw * uh * 4);
        ui_w = uw, ui_h = uh;
        if (!ui_buf) {
            static int said;
            if (!said) said = 1, RLOG_E("view", "the %dx%d interface layer: out of memory", uw, uh);
            return NULL;
        }
    }
    unsigned *d = (unsigned *)ui_buf;
    memset(d, 0, (size_t)uw * uh * 4);                       /* clear: the 3D view */
    int s = uh / 270 > 0 ? uh / 270 : 1;                      /* 4 at 1080 rows */
    int m = 4 * s;                                            /* the margin */
    unsigned keys[96];
    int nk;
    /* (the
       original's gargoyle, flasks, compass, rune shelf, spells and power gem below, cut from its screen, and our
       message box above the compass. Our own art (the gargoyle bars, the bottom panels with their tanks, swing light
       and heading strip, the pedestal: hud_gargoyle, pnl_*, hud_swing_light, hud_heading, hud_charge) is kept but
       not drawn; pnl_w stays 0, so the message box, the readout and the weapon are placed as before it.) */
    /* the compass (the cross and its needle, rows 131-154), the bottom's middle; the stone round it keyed out */
    nk = border_keys(src, sw, 112, 131, 56, 24, keys, 96);
    hud_piece(d, uw, uh, src, sw, sh, 112, 131, 56, 24, uw / 2 - 28 * s, uh - 24 * s - m, s, keys, nk, -1);
    /* the flasks (the glass and its stand), the bottom corners */
    nk = border_keys(src, sw, 248, 125, 24, 36, keys, 96);
    hud_piece(d, uw, uh, src, sw, sh, 248, 125, 24, 36, m, uh - 36 * s - m, s, keys, nk, -1);
    nk = border_keys(src, sw, 284, 125, 24, 36, keys, 96);
    hud_piece(d, uw, uh, src, sw, sh, 284, 125, 24, 36, uw - 24 * s - m, uh - 36 * s - m, s, keys, nk, -1);
    /* the gargoyle's face above the view (its eyes show a struck creature's health): PANELS.C's
       eyes sprite at 0x80, row 4 in the frame's gargoyle (x 125-150, rows 0-17); COMBAT.C set_screen_frame(7, 3 - fx)
       after a hit, fx = the creature's hp * 3 / its average, so adjust_eyes blinks set 1 (healthy) to 3 (near death).
       Cut from the original's screen at the top's middle, whole but for the screen's black (the face shares its
       colours with the frame round it, so the compass's border keying ate it) */
    {
        static const unsigned blacks[2] = { 0x000000, 0x000004 };
        hud_piece(d, uw, uh, src, sw, sh, 127, 0, 23, 18, uw / 2 - 12 * s, m, s, blacks, 2, -1);
    }
    /* the rune shelf and the active spells, as PANELS.C shows them
       (uwv_hud_magic): set_runes' three (the rune stones, OBJECTS.GR 0xE8 + rune, at rune_x 0xB0 0xBF 0xCE: 15 apart)
       right of the compass, and active_spells' three (SPELLS.GR, spell_x 0x56 0x45 0x34: the first rightmost) left of
       it, as on the original's screen; the spells left of the power gem when it shows. The game's own pictures. */
    if (uwv_ui_ready()) {
        int ry = uh - 22 * s - m, rx = uw / 2 + 28 * s + m;
        for (int i = 0; i < 3; i++)
            if (hud_runes[i] < 0x18) hud_pic(d, uw, uh, ui_pic("sprites\\objects", 0xE8 + hud_runes[i]), rx + 15 * i * s, ry, s);
        int sx0 = uw / 2 - 28 * s - m - (0x56 + 16 - 0x34) * s - (hud_fight ? 31 * s + m : 0);
        static const int spell_x[3] = { 0x56, 0x45, 0x34 };
        for (int i = 0; i < 3; i++)
            if (hud_spells[i] < 0x15) hud_pic(d, uw, uh, ui_pic("ui\\spells", hud_spells[i]), sx0 + (spell_x[i] - 0x34) * s, ry - 2 * s, s);
    }
    /* the power gem (PANELS.C adjust_power: POWER.GR's frames at 4, 60 counted from the bottom, i.e. rows 139-150 of
       the original's picture, 31 x 12: grey at rest, filling as a blow charges, pulsing at full), in fight mode only,
       at the compass's left, its stone keyed out (the charge cannot be judged without it) */
    if (hud_fight) {
        nk = border_keys(src, sw, 4, 139, 31, 12, keys, 96);
        hud_piece(d, uw, uh, src, sw, sh, 4, 139, 31, 12, uw / 2 - 28 * s - m - 31 * s, uh - 24 * s - m + 6 * s, s, keys, nk, -1);
    }
    /* the message scroll's lines, the parchment (its most common colour) keyed out, above the compass (till our own
       message box draws them: uwv_log) */
    if (!log_ours) {
        static unsigned cols[64], cnt[64];
        int nc = 0;
        for (int y = 169; y < 200; y++)
            for (int x = 16; x < 304; x++) {
                unsigned c = src[y * sw + x] & 0xFFFFFF;
                int k = 0;
                while (k < nc && cols[k] != c) k++;
                if (k == nc && nc < 64) cols[nc] = c, cnt[nc++] = 0;
                if (k < 64) cnt[k]++;
            }
        unsigned bg = 0, best = 0;
        for (int k = 0; k < nc; k++) if (cnt[k] > best) best = cnt[k], bg = cols[k];
        /* the letters drawn in the parchment's own colour, light on the dark view; the ruled line under them left */
        hud_piece(d, uw, uh, src, sw, sh, 14, 169, 290, 28, uw / 2 - 145 * s, uh - 24 * s - m - 32 * s, s, &bg, 1, (long)bg);
    }
    /* the first-person weapon in fight mode: PANELS.C draws it into the 3D window (fbshow at weapons.dat's x, y), which
       is black but for it now that our renderer draws the view; its lower part (rows 70-127 of the window at x 53-222,
       clear of the cursor in the middle and the window's edge pictures) lifted out, black keyed, and drawn from our
       view's foot at the view's scale (the window's 113 rows to the layer's height) */
    if (hud_fight) {
        int k = uh / 113 > 0 ? uh / 113 : 1;
        static unsigned cols[64], cnt[64];   /* the window's ground: its most common colour (not quite black) */
        int nc = 0;
        unsigned bg = 0, best = 0;
        for (int y = 70; y < 128; y += 2)
            for (int x = 64; x < 212; x += 2) {
                unsigned c = src[(size_t)y * sw + x] & 0xFFFFFF;
                int q = 0;
                while (q < nc && cols[q] != c) q++;
                if (q == nc && nc < 64) cols[nc] = c, cnt[nc++] = 0;
                if (q < 64) cnt[q]++;
            }
        for (int q = 0; q < nc; q++) if (cnt[q] > best) best = cnt[q], bg = cols[q];
        for (int y = 70; y < 128; y++)
            for (int x = 64; x < 212; x++) {
                if (y < 86 && x > 128 && x < 148) continue;   /* the cursor at the window's middle */
                unsigned c = src[(size_t)y * sw + x] & 0xFFFFFF;
                if (c == bg) continue;
                int dx0 = uw / 2 + (x - 138) * k, dy0 = uh - (128 - y) * k;
                unsigned px = 0xFF000000u | (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
                for (int yy = 0; yy < k; yy++)
                    for (int xx = 0; xx < k; xx++) {
                        int tx = dx0 + xx, ty = dy0 + yy;
                        if (tx >= 0 && tx < uw && ty >= 0 && ty < uh) d[(size_t)ty * uw + tx] = px;
                    }
            }
    }
    hud_frames++;
    readout_on = 0;
    if (hud_frames - inv_frame <= 2) {}   /* our inventory panel has the pointer: no crosshair */
    else if (crosshair_ours(d, uw, uh, s)) hud_readout_draw(d, uw, uh, s), readout_on = 1;
    else {   /* (when the game's pictures cannot be read: a small cross, light with a dark edge) */
        int cx = uw / 2, cy = uh / 2, a = 3 * s, t = s > 1 ? s / 2 : 1;
        for (int pass = 0; pass < 2; pass++) {
            unsigned col = pass ? 0xFFD8E0E8u : 0xC0000000u;
            int g = pass ? 0 : 1;
            for (int y = -a - g; y <= a + g; y++)
                for (int x = -t - g; x <= t + g; x++)
                    if (cx + x >= 0 && cx + x < uw && cy + y >= 0 && cy + y < uh) d[(size_t)(cy + y) * uw + cx + x] = col;
            for (int x = -a - g; x <= a + g; x++)
                for (int y = -t - g; y <= t + g; y++)
                    if (cx + x >= 0 && cx + x < uw && cy + y >= 0 && cy + y < uh) d[(size_t)(cy + y) * uw + cx + x] = col;
        }
    }
    return ui_buf;
}

/* Our message box (whole lines, an opaque parchment that fades away 10 s after the newest message, at
 * full while the Tab panel is open, the wheel scrolling back): lines (n of them, w bytes each, oldest first) in the
 * game's own font on its parchment, where the original's scroll text was shown (above the compass), at alpha (0..1);
 * more_up: older lines above. Drawn into the layer the last uwv_hud made. */
static bool ui_ready(void);
/* the message lines on the bottom panel's parchment (text always on the screen, seeming to scroll): never faded; the newest at the bottom; a new line (or the wheel) slides them up a
   line over LOG_SLIDE_MS. Smooth text by default, the game's letters with log_font original. */
#define LOG_SLIDE_MS 220
/* the message lines on the bottom panel's parchment (when the panel is drawn) */
static void log_panel(const char *lines, int n, int w, int more_up)
{
    int s = ui_h / 270 > 0 ? ui_h / 270 : 1, smooth = !log_font_original;
    int ax = PNL_X(160), ay = PNL_Y(192), aw = PNL_X(878) - ax, ah = PNL_Y(302) - ay;   /* (the parchment) */
    if (aw <= 0 || ah <= 0) return;
    const int em = st_game_em(ui_h), lhp = smooth ? st_height(ST_REGULAR, em) + s / 2 : (ui_font_height(FONT_NORMAL) + 1) * s;
    int rows = ah / lhp;
    if (rows < 1) rows = 1;
    /* the slide: the newest line changed (a new message, or the wheel) */
    static char last[256];
    static int last_n;
    static unsigned long long t0;
    unsigned long long now = GetTickCount64();
    const char *newest = lines + (size_t)(n - 1) * w;
    if (n != last_n || strncmp(newest, last, sizeof last - 1)) {
        if (last_n) t0 = now;
        snprintf(last, sizeof last, "%.*s", w, newest), last_n = n;
    }
    float k = (float)(now - t0) / LOG_SLIDE_MS;
    int off = k >= 1 ? 0 : (int)((1 - k) * (1 - k) * lhp);   /* eased out */
    int shown = n < rows + 1 ? n : rows + 1;                    /* (one more above, sliding out) */
    int pad = (ah - rows * lhp) / 2, y_new = ah - pad - lhp + off;   /* the newest line's top in the area */
    static unsigned *txt;
    static int txt_n;
    if (aw * ah > txt_n) { free(txt); txt_n = aw * ah; txt = malloc((size_t)txt_n * sizeof *txt); if (!txt) { static int said; if (!said) said = 1, RLOG_E("view", "the message box's text layer: out of memory"); txt_n = 0; return; } }
    memset(txt, 0, (size_t)aw * ah * sizeof *txt);
    if (smooth) {
        for (int i = 0; i < shown; i++)
            st_draw(txt, aw, ah, 0, y_new - i * lhp, ST_REGULAR, em, lines + (size_t)(n - 1 - i) * w, 0x3C2820);
        if (more_up) st_draw(txt, aw, ah, aw - st_width(ST_REGULAR, em, "^") - s, s, ST_REGULAR, em, "^", 0x3C2820);
    } else {   /* the game's letters: drawn on the overlay at its scale, then taken into the area */
        ui_set_scale(s);
        Overlay *o = ui_overlay();
        ui_clear();
        for (int i = 0; i < shown; i++) {
            int ty = ay + y_new - i * lhp;
            ui_text(FONT_NORMAL, lines + (size_t)(n - 1 - i) * w, ax / s, ty / s, UI_INK);
        }
        if (more_up) ui_text(FONT_SMALL, "^", (ax + aw) / s - 6, ay / s + 1, UI_INK);
        for (int y = 0; y < ah; y++) {
            int oy = (ay + y) / s;
            if (oy < 0 || oy >= o->h) continue;
            for (int x = 0; x < aw; x++) {
                int ox = (ax + x) / s;
                if (ox < 0 || ox >= o->w) continue;
                const u8 *p = o->px + ((size_t)oy * o->w + ox) * 4;
                if (p[3]) txt[(size_t)y * aw + x] = (unsigned)p[3] << 24 | p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16;
            }
        }
    }
    unsigned *d = (unsigned *)ui_buf;
    for (int y = 0; y < ah; y++) {
        int ty = ay + y;
        if (ty < 0 || ty >= ui_h) continue;
        for (int x = 0; x < aw; x++) {
            int tx = ax + x;
            if (tx < 0 || tx >= ui_w) continue;
            unsigned t = txt[(size_t)y * aw + x], a = t >> 24;
            if (!a) continue;
            unsigned *px = &d[(size_t)ty * ui_w + tx], c = *px, na = 255 - a;
            unsigned r = ((t & 255) * a + (c & 255) * na) / 255, g = ((t >> 8 & 255) * a + (c >> 8 & 255) * na) / 255,
                     b = ((t >> 16 & 255) * a + (c >> 16 & 255) * na) / 255;
            *px = 0xFF000000u | r | g << 8 | b << 16;
        }
    }
}
/* our message box over the layer: n lines of w bytes, oldest first, at alpha; more_up: older lines above */
UWV_API void uwv_log(const char *lines, int n, int w, float alpha, int more_up)
{
    log_ours = 1;
    if (pnl_w > 0) {   /* the bottom panel's parchment: always shown */
        if (ui_ready() && ui_buf && n > 0) log_panel(lines, n, w, more_up);
        return;
    }
    if (!ui_ready() || !ui_buf || n <= 0 || alpha <= 0.003f) return;
    int s = ui_h / 270 > 0 ? ui_h / 270 : 1, m = 4 * s;
    ui_set_scale(s);
    Overlay *o = ui_overlay();
    ui_clear();
    /* (the box, where Ctrl+L's AI lines show too, in a smaller font that is easier to read, with
       more lines): UWV_LOG_ROWS lines; smooth text (smoothtext.c, at the window's resolution) by default, the game's
       letters as they were with settings.txt log_font original (the Esc menu's "Message text") */
    /* (less wasted room, margins of 1-5 pixels: the blank lines left out, the box as tall as the
       lines it shows and as wide as the game's 49-letter lines, the text one overlay pixel inside the frame) */
    const int ROWS = UWV_LOG_ROWS, smooth = !log_font_original;
    const int em = st_game_em(ui_h), lhp = smooth ? st_height(ST_REGULAR, em) + s / 2 : 0;   /* the smooth line, window
                                                                                                pixels; the one size */
    const int fh = smooth ? (lhp + s - 1) / s : ui_font_height(FONT_NORMAL) + 1;
    const char *show[UWV_LOG_ROWS];
    int shown = 0;
    for (int i = n - 1; i >= 0 && shown < ROWS; i--) {   /* the newest lines that have words, newest last */
        const char *l = lines + (size_t)i * w;
        int blank = 1;
        for (const char *c = l; *c && c < l + w; c++) if (*c != ' ' && *c != '\t' && *c != '\n') { blank = 0; break; }
        if (!blank) show[ROWS - 1 - shown++] = l;
    }
    if (!shown) { ui_set_scale(0); return; }
    const char *const *sh = show + (ROWS - shown);
    int bw = (smooth ? (st_width(ST_REGULAR, em, "Thou art on the first level of the Abyss, at its gate") + s - 1) / s
                     : ui_text_width(FONT_NORMAL, "Thou art on the first level of the Abyss, at its gate")) + 4;
    for (int i = 0; i < shown; i++) {   /* (a longer line: the box as wide as it) */
        int lw = smooth ? (st_width(ST_REGULAR, em, sh[i]) + s - 1) / s : ui_text_width(FONT_NORMAL, sh[i]);
        if (lw + 4 > bw) bw = lw + 4;
    }
    if (bw > o->w - 4) bw = o->w - 4;
    const int bh = smooth ? (shown * lhp + s - 1) / s + 4 : shown * fh + 4;
    int bx = (o->w - bw) / 2, by = (ui_h - 24 * s - m) / s - bh - 2;
    ui_parchment(bx, by, bw, bh);
    if (!smooth)
        for (int i = 0; i < shown; i++) ui_text(FONT_NORMAL, sh[i], bx + 2, by + 2 + i * fh, UI_INK);
    if (more_up) ui_text(FONT_SMALL, "^", bx + bw - 6, by + 1, UI_INK);
    unsigned *d = (unsigned *)ui_buf;
    unsigned A = (unsigned)(alpha * 256.0f);
    if (A > 256) A = 256;
    /* the smooth lines: drawn into a clear layer of the box's size, then laid over it with the box's fade */
    static unsigned *txt;
    static int txt_n;
    int tw = bw * s, th = bh * s;
    if (smooth && tw * th > txt_n) { free(txt); txt_n = tw * th; txt = malloc((size_t)txt_n * sizeof *txt); if (!txt) { static int said; if (!said) said = 1, RLOG_E("view", "the message box's smooth text layer: out of memory (that text not drawn)"); txt_n = 0; } }
    if (smooth && txt) {
        memset(txt, 0, (size_t)tw * th * sizeof *txt);
        for (int i = 0; i < shown; i++)
            st_draw(txt, tw, th, 2 * s, 2 * s + i * lhp, ST_REGULAR, em, sh[i], 0x3C2820);
    }
    for (int yy = by * s; yy < (by + bh) * s && yy < ui_h; yy++) {
        if (yy < 0) continue;
        const u8 *orow = o->px + (size_t)(yy / s) * o->w * 4;
        for (int xx = bx * s; xx < (bx + bw) * s && xx < ui_w; xx++) {
            if (xx < 0) continue;
            const u8 *p = orow + (xx / s) * 4;
            unsigned a = p[3] * A >> 8;
            if (!a) continue;
            unsigned na = 255 - a, c = d[(size_t)yy * ui_w + xx];
            if (!(c >> 24)) {   /* the 3D view shows here: the box's own alpha */
                d[(size_t)yy * ui_w + xx] = a << 24 | p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16;
                continue;
            }
            unsigned r = (p[0] * a + (c & 0xFF) * na) / 255, g = (p[1] * a + (c >> 8 & 0xFF) * na) / 255, b = (p[2] * a + (c >> 16 & 0xFF) * na) / 255;
            d[(size_t)yy * ui_w + xx] = 0xFF000000u | r | g << 8 | b << 16;
        }
    }
    ui_text_apply_a(d, ui_w, ui_h, A);   /* (the frame and any smooth text: ui.c; at the box's fade) */
    if (smooth && txt)   /* the text layer (its colour already times its coverage: drawn over black) at the box's fade */
        for (int ty = 0; ty < th; ty++) {
            int yy = by * s + ty;
            if (yy < 0 || yy >= ui_h) continue;
            for (int tx = 0; tx < tw; tx++) {
                int xx = bx * s + tx;
                unsigned t = txt[(size_t)ty * tw + tx], ta = (t >> 24) * A >> 8;
                if (!ta || xx < 0 || xx >= ui_w) continue;
                unsigned c = d[(size_t)yy * ui_w + xx], nt = 255 - ta, ca = c >> 24;
                unsigned r = ((t & 0xFF) * A >> 8) + (c & 0xFF) * nt / 255, g = ((t >> 8 & 0xFF) * A >> 8) + (c >> 8 & 0xFF) * nt / 255;
                unsigned b = ((t >> 16 & 0xFF) * A >> 8) + (c >> 16 & 0xFF) * nt / 255;
                if (ta > ca) ca = ta;
                d[(size_t)yy * ui_w + xx] = ca << 24 | (r > 255 ? 255 : r) | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b) << 16;
            }
        }
    /* the crosshair's name and hint again, over the box (its right end covered them: the box comes after the HUD) */
    if (readout_on) hud_readout_draw((unsigned *)ui_buf, ui_w, ui_h, s);
    ui_set_scale(0);
}

/* The Esc menu (ours, as uw.exe's options were): on the original's parchment (ui.c, the game's own fonts), drawn
 * over the interface layer the last uwv_hud / uwv_compose made. mode: 0 ours (full screen), 1 the original's. sel:
 * the row lit; mx, my: the pointer in window pixels. Returns the row under the pointer, or -1. */
UWV_API int uwv_menu(int mode, int sel, int mx, int my)
{
    if (!started || !ui_buf) return -1;
    if (!ui_ok) {
        char data[MAX_PATH], game[MAX_PATH];
        data_dirs(data, game);
        ui_ok = ui_start(data, out_w, out_h);
        if (!ui_ok) return -1;
    }
    Overlay *o = ui_overlay();
    ui_clear();
    static const char *rows[2][2] = {{"Display: ours (full screen, mouse look)", "Display: the original's (its screen and controls)"},
                                     {"Return to the game", "Return to the game"}};
    int w = 230, h = 92, x0 = (o->w - w) / 2, y0 = (o->h - h) / 2, hit = -1;
    int px = mx / o->scale, py = my / o->scale;
    ui_parchment(x0, y0, w, h);
    ui_text(FONT_BIG, "Options", x0 + 10, y0 + 8, UI_INK_HEAD);
    for (int r = 0; r < 2; r++) {
        int ry = y0 + 34 + r * 22, fh = ui_font_height(FONT_NORMAL) + 6;
        bool over = px >= x0 + 6 && px < x0 + w - 6 && py >= ry - 3 && py < ry - 3 + fh;
        if (over) hit = r;
        ui_inset(x0 + 6, ry - 3, w - 12, fh, r == sel || over);
        ui_text(FONT_NORMAL, rows[r][r == 0 ? mode : 0], x0 + 12, ry, UI_INK);
    }
    ui_text(FONT_SMALL, "ENTER OR A CLICK CHOOSES, ESC RETURNS", x0 + 10, y0 + h - 12, UI_INK_FAINT);
    /* the overlay onto the layer, its pixels o->scale window pixels each */
    unsigned *d = (unsigned *)ui_buf;
    for (int y = 0; y < ui_h; y++) {
        int oy = y / o->scale;
        if (oy >= o->h) break;
        const u8 *orow = o->px + (size_t)oy * o->w * 4;
        for (int x = 0; x < ui_w; x++) {
            int ox = x / o->scale;
            if (ox >= o->w) break;
            const u8 *p = orow + ox * 4;
            if (p[3] == 0) continue;
            unsigned a = p[3], na = 255 - a, c = d[(size_t)y * ui_w + x];
            unsigned r = (p[0] * a + (c & 0xFF) * na) / 255, g = (p[1] * a + (c >> 8 & 0xFF) * na) / 255, b = (p[2] * a + (c >> 16 & 0xFF) * na) / 255;
            d[(size_t)y * ui_w + x] = 0xFF000000u | r | g << 8 | b << 16;
        }
    }
    ui_text_apply(d, ui_w, ui_h);   /* (the interface font's smooth text over the overlay: ui.c) */
    return hit;
}

/* --- our options (uw.exe's menu, made again): the renderer's rows are the DLL's own; each change is kept in
 * data\settings.txt as uw.exe kept it (a line "key value", replaced or added) --- */
static void settings_save(const char *key, const char *value)
{
    char data[MAX_PATH], game[MAX_PATH], p[MAX_PATH], line[512];
    data_dirs(data, game);
    snprintf(p, sizeof p, "%s\\settings.txt", data);
    {   /* UWVIEW_SETTINGS=FILE (the checks): the options kept there, never in the user's own file */
        const char *e = getenv("UWVIEW_SETTINGS");
        if (e && *e) snprintf(p, sizeof p, "%s", e);
    }
    static char out[64 * 1024];
    size_t n = 0;
    bool done = false;
    FILE *f = fopen(p, "r");
    if (!f && errno != ENOENT) RLOG_E("view", "cannot read %s: %s (it is written again with %s alone)", p, rlog_errno(), key);
    if (f) {
        while (fgets(line, sizeof line, f)) {
            size_t kl = strlen(key);
            if (!done && !strncmp(line, key, kl) && (line[kl] == ' ' || line[kl] == '\t')) {
                n += (size_t)snprintf(out + n, sizeof out - n, "%s %s\n", key, value), done = true;
                continue;
            }
            n += (size_t)snprintf(out + n, sizeof out - n, "%s", line);
            if (n >= sizeof out - 600) {
                RLOG_E("view", "%s is over %zu bytes: the rest of it is dropped when it is written again", p, sizeof out - 600);
                break;
            }
        }
        fclose(f);
    }
    if (!done) n += (size_t)snprintf(out + n, sizeof out - n, "%s %s\n", key, value);
    f = fopen(p, "w");
    if (!f) RLOG_E("view", "cannot open %s to write: %s (%s not kept)", p, rlog_errno(), key);
    if (f) {
        bool bad = fwrite(out, 1, n, f) != n;
        if (fclose(f) != 0) bad = true;
        if (bad) RLOG_E("view", "cannot write %s: %s (%s not kept)", p, rlog_errno(), key);
    }
}
/* The Sprites tab (its own Esc tab, a row only for an object that has a picture of ours): an object picture of ours, data\newsprites\objects\NNN.png (NNN the object, 000-463; up to 32 x 32: twice
 * the original's detail in the same place), each with a row of its own, there only when its file is. settings.txt
 * sprite_new_NNN 1; the original's picture unless set. Looked for once a run. Presentation only: the picture in the
 * world (level.c) and in our inventory and on the crosshair (ui_item_pic); the original's own panel keeps its own. */
static signed char spr_have[512];   /* 0 not looked for yet, 1 the file is there, -1 it is not */
static int spr_there(int k)
{
    if (!spr_have[k]) {
        char data[MAX_PATH], game[MAX_PATH], p[MAX_PATH + 64];
        data_dirs(data, game);
        snprintf(p, sizeof p, "%s\\newsprites\\objects\\%03d.png", data, k);
        spr_have[k] = file_exists(p) ? 1 : -1;
    }
    return spr_have[k] > 0;
}
/* object k's name for its row: STRINGS.PAK block 4 as extracted (data\strings\block0004.txt, "k: a_hand axe&hand
 * axes"): the singular without its article, a capital first; 0 when there is none */
static int spr_label(int k, char *value, int n)
{
    static char name[512][40];
    static bool read;
    if (!read) {
        char data[MAX_PATH], game[MAX_PATH], p[MAX_PATH + 64];
        read = true;
        data_dirs(data, game);
        snprintf(p, sizeof p, "%s\\strings\\block0004.txt", data);
        size_t mark = arena_mark();
        Bytes f = file_read(p);
        for (const char *q = (const char *)f.data, *end = q ? q + f.size : q; q && q < end;) {
            const char *e = memchr(q, '\n', (size_t)(end - q));
            if (!e) e = end;
            if (*q >= '0' && *q <= '9') {
                int i = atoi(q);
                const char *t = memchr(q, ':', (size_t)(e - q));
                if (t && i >= 0 && i < 512) {
                    const char *u;
                    int len = 0;
                    t++;
                    while (t < e && *t == ' ') t++;
                    u = memchr(t, '_', (size_t)(e - t));
                    if (u) t = u + 1;
                    while (t < e && *t != '&' && *t != '\r' && len < 39) name[i][len++] = *t++;
                    name[i][len] = 0;
                    if (name[i][0] >= 'a' && name[i][0] <= 'z') name[i][0] = (char)(name[i][0] - 32);
                }
            }
            q = e + 1;
        }
        arena_reset(mark);
    }
    if (k < 0 || k >= 512 || !name[k][0]) return 0;
    snprintf(value, (size_t)n, "%s", name[k]);
    return 1;
}
/* the Sprites tab's row for object k: its value (New / Original); step != 0 switches it and builds the scene again */
static int spr_option(int k, int step, char *value, int n)
{
    if (k < 0 || k >= 512 || !spr_there(k)) return 0;
    if (step) {
        char key[24];
        level_new_sprite[k] = ui_new_sprite[k] = !level_new_sprite[k];
        snprintf(key, sizeof key, "sprite_new_%03d", k);
        settings_save(key, level_new_sprite[k] ? "1" : "0");
        lv_number = 0;   /* the scene is built again with the other picture (uwv_level, the front end's next frame) */
    }
    snprintf(value, (size_t)n, "%s", level_new_sprite[k] ? "New" : "Original");
    return 1;
}
/* the field of view (radians, vertical), the one our full-screen view uses */
UWV_API float uwv_fov(void) { return S.fov * 3.14159265f / 180.0f; }
/* a number's row: one step up or down, kept to lo..hi on the step's grid, written to
 * settings.txt as key; restart: it takes hold at the next start (run: its value this run, shown when they differ) */
static int opt_num(float *v, int step, float lo, float hi, float st, const char *key, const char *unit, int restart, float run,
                   char *value, int n)
{
    if (step) {
        char t[32];
        float x = lo + roundf((*v + (step > 0 ? st : -st) - lo) / st) * st;
        *v = x < lo ? lo : x > hi ? hi : x;
        snprintf(t, sizeof t, "%.2f", *v), settings_save(key, t);
    }
    snprintf(value, (size_t)n, "%.2f%s%s", *v, unit, restart && fabsf(*v - run) > 1e-4f ? " (from the next start)" : "");
    return 1;
}
/* uwv_option's rows for DLSS, DLSS 5, ray tracing and Ray Reconstruction, the DLSS mode and the resolution (ids 0-3, 21-25): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_rendering(int id, int step, char *value, int n)
{
    static const char *const STYLE[3] = {"A - Default", "B - Natural", "C - Cinematic"};
    char t[72];   /* a row's new value as text, for settings_save */
    switch (id) {
    case 0:
        if (!render_dlss_available()) { snprintf(value, (size_t)n, "Not on this card"); return 1; }
        if (step) render_set_dlss(!render_dlss_on()), S.dlss = render_dlss_on(), settings_save("dlss", S.dlss ? "1" : "0");
        snprintf(value, (size_t)n, "%s", render_dlss_on() ? "On" : "Off");
        return 1;
    case 1:
        if (!render_dlss5_available()) return 0;
        if (step) render_set_dlss5(!render_dlss5_on()), S.dlss5 = render_dlss5_on(), settings_save("dlss5", S.dlss5 ? "1" : "0");
        snprintf(value, (size_t)n, "%s", render_dlss5_on() ? "On" : "Off");
        return 1;
    case 2:
        if (!render_dlss5_available()) return 0;
        if (step) S.dlss5_style = (S.dlss5_style + (step > 0 ? 1 : 2)) % 3, render_set_dlss5_style(S.dlss5_style),
                  snprintf(t, sizeof t, "%d", S.dlss5_style), settings_save("dlss5_style", t);
        snprintf(value, (size_t)n, "%s", STYLE[S.dlss5_style % 3]);
        return 1;
    case 3: {   /* Ray tracing: On / Off (Ray Reconstruction has its own row: 22) */
        if (!render_raytracing_available()) { snprintf(value, (size_t)n, "Not on this card"); return 1; }
        if (step) render_set_raytracing(!render_raytracing()), settings_save("raytracing", render_raytracing() ? "1" : "0");
        snprintf(value, (size_t)n, "%s", render_raytracing() ? "On" : "Off");
        return 1;
    }
    case 21: {  /* DLSS mode: the size the scene is drawn at; changed at once (render_set_dlss_quality) */
        static const int ORDER[5] = {DLSS_Q_DLAA, DLSS_Q_QUALITY, DLSS_Q_BALANCED, DLSS_Q_PERF, DLSS_Q_ULTRA};
        static const char *const NAME[5] = {"DLAA", "Quality", "Balanced", "Performance", "Ultra Performance"};
        static const char *const KEY[5] = {"dlaa", "quality", "balanced", "performance", "ultra"};
        int at = 0, rw, rh;
        if (!render_dlss_available() || !render_dlss_on()) return 0;
        for (int i = 0; i < 5; i++) if (ORDER[i] == render_dlss_quality()) at = i;
        if (step) {
            int to = (at + (step > 0 ? 1 : 4)) % 5;
            if (render_set_dlss_quality(ORDER[to])) at = to, S.port_quality = ORDER[to], settings_save("port_dlss_quality", KEY[to]);
        }
        render_size_for(ORDER[at], &rw, &rh);
        snprintf(value, (size_t)n, "%s (%d x %d)", NAME[at], rw, rh);
        return 1;
    }
    case 22: {  /* DLSS Ray Reconstruction: the ray-traced light denoised and enlarged in one go, in Super Resolution's place */
        if (!render_raytracing_available()) return 0;
        if (!render_dlss_available() || !render_dlss_on()) { snprintf(value, (size_t)n, "Needs DLSS"); return 1; }
        if (!render_raytracing()) { snprintf(value, (size_t)n, "Needs ray tracing"); return 1; }
        if (step) render_set_rt_high(!render_rt_high_wanted()), S.rt_high = render_rt_high_wanted(),
                  settings_save("rt_quality", S.rt_high ? "high" : "normal");
        snprintf(value, (size_t)n, "%s", render_rt_high_wanted() ? "On" : "Off");
        return 1;
    }
    case 23: {  /* Resolution (information): the size drawn now -> the size shown */
        int rw, rh, ow, oh;
        render_sizes(&rw, &rh, &ow, &oh);
        snprintf(value, (size_t)n, "%d x %d -> %d x %d", rw, rh, ow, oh);
        return 1;
    }
    case 24: case 25: {  /* DLSS 5's intensity and structure, in fives of a hundred */
        float *v = id == 24 ? &S.dlss5_intensity : &S.dlss5_structure;
        if (!render_dlss5_available()) return 0;
        if (step) {
            int pc = (int)(*v * 100.0f + 0.5f) + (step > 0 ? 5 : -5);
            pc = pc < 0 ? 0 : pc > 100 ? 100 : pc;
            *v = (float)pc / 100.0f;
            render_set_dlss5_params(S.dlss5_intensity, S.dlss5_structure);
            snprintf(t, sizeof t, "%.2f", *v), settings_save(id == 24 ? "dlss5_intensity" : "dlss5_structure", t);
        }
        snprintf(value, (size_t)n, "%d%%", (int)(*v * 100.0f + 0.5f));
        return 1;
    }
    }
    return -1;
}
/* uwv_option's rows for bounced light, stone relief and its strength, field of view, mouse sensitivity (ids 4-8): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_light_view(int id, int step, char *value, int n)
{
    char t[72];   /* a row's new value as text, for settings_save */
    switch (id) {
    case 4:
        if (step) S.bounce = S.bounce > 0 ? 0 : 1, settings_save("bounce", S.bounce > 0 ? "1.0" : "0");
        snprintf(value, (size_t)n, "%s", S.bounce > 0 ? "On" : "Off");
        return 1;
    case 5:
        if (step) S.normal_maps = !S.normal_maps, settings_save("normal_maps", S.normal_maps ? "1" : "0");
        snprintf(value, (size_t)n, "%s", S.normal_maps ? "On" : "Off");
        return 1;
    case 6:
        if (step) {
            S.fov += step > 0 ? 5 : -5;
            if (S.fov < 50) S.fov = 50;
            if (S.fov > 120) S.fov = 120;
            snprintf(t, sizeof t, "%d", (int)S.fov), settings_save("fov", t);
        }
        snprintf(value, (size_t)n, "%d", (int)(S.fov + 0.5f));
        return 1;
    case 7:
        if (step) {
            S.mouse += step > 0 ? 0.1f : -0.1f;
            if (S.mouse < 0.1f) S.mouse = 0.1f;
            if (S.mouse > 4.0f) S.mouse = 4.0f;
            snprintf(t, sizeof t, "%.1f", S.mouse), settings_save("mouse_sensitivity", t);
        }
        snprintf(value, (size_t)n, "%.1f", S.mouse);
        return 1;
    case 8:
        if (step) {
            S.normal_strength += step > 0 ? 0.25f : -0.25f;
            if (S.normal_strength < 0.25f) S.normal_strength = 0.25f;
            if (S.normal_strength > 4.0f) S.normal_strength = 4.0f;
            snprintf(t, sizeof t, "%.2f", S.normal_strength), settings_save("port_normal_strength", t);
        }
        snprintf(value, (size_t)n, "%.2f", S.normal_strength);
        return 1;
    }
    return -1;
}
/* uwv_option's rows for the particles and decals, the glowing things, campfire smoke and water fog (ids 10, 11, 80-84): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_effects_glow(int id, int step, char *value, int n)
{
    char t[72];   /* a row's new value as text, for settings_save */
    switch (id) {
    case 10:  /* the effects' particles (render_particles.c) */
        if (step) FX.particles = !FX.particles, settings_save("fx_particles", FX.particles ? "1" : "0"),
                  render_set_fx(FX.particles, FX.decals, FX.water, FX.fog);
        if (!FX.particles) render_particles_clear();
        snprintf(value, (size_t)n, "%s", FX.particles ? "On" : "Off");
        return 1;
    case 80:  /* Glowing stones: the glowing rocks, orbs and orb rocks glow a foot round them */
        if (step) GL.glow_things = !GL.glow_things, world_glow_things = GL.glow_things, settings_save("light_glowing_things", GL.glow_things ? "1" : "0");
        snprintf(value, (size_t)n, "%s", GL.glow_things ? "On" : "Off");
        return 1;
    case 81:    /* Moonstone light: Off, then 5 to 50 % of a candle (10) */
    case 82: {  /* Fire elemental light: Off, then 10 to 100 % of a candle (30) */
        float *f = id == 81 ? &GL.moonstone : &GL.elemental;
        int by = id == 81 ? 5 : 10, top = id == 81 ? 50 : 100, pc = (int)(*f * 100.0f + 0.5f);
        if (step) {
            pc += step > 0 ? by : -by;
            pc = pc < 0 ? 0 : pc > top ? top : pc;
            *f = (float)pc / 100.0f;
            snprintf(t, sizeof t, "%.2f", *f), settings_save(id == 81 ? "light_moonstone" : "light_fire_elemental", t);
        }
        if (pc <= 0) snprintf(value, (size_t)n, "Off");
        else snprintf(value, (size_t)n, "%d%% of a candle", pc);
        return 1;
    }
    case 83:  /* Campfire smoke (particles) */
        if (step) GL.camp_smoke = !GL.camp_smoke, settings_save("fx_campfire_smoke", GL.camp_smoke ? "1" : "0");
        snprintf(value, (size_t)n, "%s", GL.camp_smoke ? "On" : "Off");
        return 1;
    case 84:  /* Water fog (particles) */
        if (step) GL.water_mist = !GL.water_mist, settings_save("fx_water_mist", GL.water_mist ? "1" : "0");
        snprintf(value, (size_t)n, "%s", GL.water_mist ? "On" : "Off");
        return 1;
    case 11:  /* the effects' decals (render_decals.c) */
        if (step) FX.decals = !FX.decals, settings_save("fx_decals", FX.decals ? "1" : "0"),
                  render_set_fx(FX.particles, FX.decals, FX.water, FX.fog);
        if (!FX.decals) render_decals_clear();
        snprintf(value, (size_t)n, "%s", FX.decals ? "On" : "Off");
        return 1;
    }
    return -1;
}
/* uwv_option's rows for the light's colour and adaptation, HDR output, peak brightness and paper white (ids 31-35): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_colour_hdr(int id, int step, char *value, int n)
{
    char t[72];   /* a row's new value as text, for settings_save */
    switch (id) {
    case 31:  /* Light colour: the flames' adaptation transform, CAT16 (the default) or Bradford */
        if (step) render_set_light_cat(!render_light_cat()), settings_save("light_colour", render_light_cat() ? "bradford" : "cat16");
        snprintf(value, (size_t)n, "%s", render_light_cat() ? "Bradford" : "CAT16");
        return 1;
    case 32: {  /* Light adaptation: Automatic (CAM16's degree, the default), then 0 to 100 % in tens (0 the raw fire, 100 white) */
        int pc = render_adaptation() < 0 ? -10 : (int)(render_adaptation() * 100.0f + 0.5f);   /* -10: Automatic */
        if (step) {
            pc += step > 0 ? 10 : -10, pc = pc < -10 ? 100 : pc > 100 ? -10 : pc;
            render_set_adaptation(pc < 0 ? -1.0f : (float)pc / 100.0f);
            if (pc < 0) snprintf(t, sizeof t, "auto"); else snprintf(t, sizeof t, "%.2f", (float)pc / 100.0f);
            settings_save("light_adaptation", t);
        }
        if (pc < 0) snprintf(value, (size_t)n, "Automatic (%d%%)", (int)(render_adaptation_auto() * 100.0f + 0.5f));
        else snprintf(value, (size_t)n, "%d%%", pc);
        return 1;
    }
    case 33:  /* HDR output: on an HDR monitor (Windows' HDR on), the highlights past white */
        if (step) render_set_hdr(!render_hdr_wanted()), settings_save("hdr", render_hdr_wanted() ? "1" : "0");
        if (!render_hdr_wanted()) snprintf(value, (size_t)n, "Off");
        else if (render_hdr_available()) snprintf(value, (size_t)n, "On");
        else snprintf(value, (size_t)n, "On (this monitor: SDR)");
        return 1;
    case 34: {  /* HDR peak brightness: the monitor's own (0, the default), then 400 to 4000 nits in hundreds */
        int nits = (int)(render_hdr_peak_setting() + 0.5f);
        if (step) {
            nits = nits == 0 ? (step > 0 ? 400 : 4000) : nits + (step > 0 ? 100 : -100);
            if (nits < 400 || nits > 4000) nits = 0;
            render_set_hdr_levels((float)nits, render_hdr_paper());
            snprintf(t, sizeof t, "%d", nits), settings_save("hdr_peak", t);
        }
        if (nits == 0) snprintf(value, (size_t)n, "The monitor's (%d nits)", (int)(render_hdr_peak() + 0.5f));
        else snprintf(value, (size_t)n, "%d nits", nits);
        return 1;
    }
    case 35: {  /* HDR paper white: the SDR picture's and the interface's white, 80 to 500 nits in tens (200) */
        int nits = (int)(render_hdr_paper() + 0.5f);
        if (step) {
            nits += step > 0 ? 10 : -10, nits = nits < 80 ? 80 : nits > 500 ? 500 : nits;
            render_set_hdr_levels(render_hdr_peak_setting(), (float)nits);
            snprintf(t, sizeof t, "%d", nits), settings_save("hdr_paper_white", t);
        }
        snprintf(value, (size_t)n, "%d nits", nits);
        return 1;
    }
    }
    return -1;
}
/* uwv_option's rows for water, positional sound, the conversation and menu text (ids 12, 14, 17, 18): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_water_sound_text(int id, int step, char *value, int n)
{
    switch (id) {
    case 12:  /* the effects' water (the scene's shader) */
        if (step) FX.water = !FX.water, settings_save("fx_water", FX.water ? "1" : "0"), render_set_fx(FX.particles, FX.decals, FX.water, FX.fog);
        snprintf(value, (size_t)n, "%s", FX.water ? "On" : "Off");
        return 1;
    case 14:  /* the effects' positional sound (fx_audio: the backend's uwaudio.c reads it again after a change) */
        if (step) FX.audio = !FX.audio, settings_save("fx_audio", FX.audio ? "1" : "0");
        snprintf(value, (size_t)n, "%s", FX.audio ? "On" : "Off");
        return 1;
    case 17:  /* Conversation text (uwconv_draw.c): a smooth font, the default, or the original's */
        if (step) uwv_conv_font_original = !uwv_conv_font_original,
                  settings_save("conv_font", uwv_conv_font_original ? "original" : "smooth");
        snprintf(value, (size_t)n, "%s", uwv_conv_font_original ? "Original" : "Smooth");
        return 1;
    case 18:  /* Menu text: a smooth font, the default, or the game's small letters */
        if (step) menu_font_original = !menu_font_original, settings_save("menu_font", menu_font_original ? "original" : "smooth");
        snprintf(value, (size_t)n, "%s", menu_font_original ? "The original's" : "Smooth");
        return 1;
    }
    return -1;
}
/* uwv_option's rows for acoustics and the sound rows that go with it (ids 15, 19, 20, 26-30, 56-60): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_acoustics(int id, int step, char *value, int n)
{
    char t[72];   /* a row's new value as text, for settings_save */
    switch (id) {
    case 15:  /* Acoustics (fx_acoustics: the backend's uwaudio.c reads it again after a change, the audio started again) */
        if (step) FX.acoustics = (FX.acoustics + (step > 0 ? 1 : 2)) % 3, snprintf(t, sizeof t, "%d", FX.acoustics), settings_save("fx_acoustics", t);
        snprintf(value, (size_t)n, "%s", FX.acoustics == 2 ? "High" : FX.acoustics ? "On" : "Off");   /* High: Steam Audio's echoes for each sound */
        return 1;
    case 26:  /* Reverb level (acoustics_wet): 0 to 200 % in tens; 0 is no reverb at all. Shown while Acoustics is on. */
        if (!FX.acoustics) return 0;
        if (step) ac_wet_set = ac_wet_set + (step > 0 ? 10 : -10), ac_wet_set = ac_wet_set < 0 ? 0 : ac_wet_set > 200 ? 200 : ac_wet_set,
                  snprintf(t, sizeof t, "%d", ac_wet_set), settings_save("acoustics_wet", t);
        if (ac_wet_set) snprintf(value, (size_t)n, "%d%%", ac_wet_set); else snprintf(value, (size_t)n, "Off");
        return 1;
    case 27: {  /* Room size (acoustics_tile_m): the metres a tile, 1.5 to 4.0 in tenths: how long every room rings */
        if (!FX.acoustics) return 0;
        int tenths = (int)(ac_tile_set * 10.0f + 0.5f);
        if (step) tenths += step > 0 ? 1 : -1, tenths = tenths < 15 ? 15 : tenths > 40 ? 40 : tenths, ac_tile_set = (float)tenths / 10.0f,
                  snprintf(t, sizeof t, "%d.%d", tenths / 10, tenths % 10), settings_save("acoustics_tile_m", t);
        snprintf(value, (size_t)n, "%d.%d m a tile", tenths / 10, tenths % 10);
        return 1;
    }
    case 28:  /* Muffling by walls (acoustics_occlusion): 0 to 100 % in tens; 0 is no filter at all */
        if (!FX.acoustics) return 0;
        if (step) ac_occl_set = ac_occl_set + (step > 0 ? 10 : -10), ac_occl_set = ac_occl_set < 0 ? 0 : ac_occl_set > 100 ? 100 : ac_occl_set,
                  snprintf(t, sizeof t, "%d", ac_occl_set), settings_save("acoustics_occlusion", t);
        if (ac_occl_set) snprintf(value, (size_t)n, "%d%%", ac_occl_set); else snprintf(value, (size_t)n, "Off");
        return 1;
    case 29:  /* Sound round corners (acoustics_detour): a sound round a corner comes along the way a person would walk */
        if (!FX.acoustics) return 0;
        if (step) ac_detour_set = !ac_detour_set, settings_save("acoustics_detour", ac_detour_set ? "1" : "0");
        snprintf(value, (size_t)n, "%s", ac_detour_set ? "On" : "Off");
        return 1;
    case 30:  /* Footsteps by floor (fx_footsteps): shown while Positional sound is on (it is that module's) */
        if (!FX.audio) return 0;
        if (step) fx_footsteps_set = !fx_footsteps_set, settings_save("fx_footsteps", fx_footsteps_set ? "1" : "0");
        snprintf(value, (size_t)n, "%s", fx_footsteps_set ? "On" : "Off");
        return 1;
    case 56: {  /* Hearing range (fx_hear_range): the original's 6 tiles, or further */
        static const int R[5] = {6, 9, 12, 16, 24};
        int k = 0;
        if (!FX.audio) return 0;
        for (int i = 0; i < 5; i++) if (fx_range_set >= R[i]) k = i;
        if (step) k += step > 0 ? 1 : -1, k = k < 0 ? 0 : k > 4 ? 4 : k, fx_range_set = R[k],
                  snprintf(t, sizeof t, "%d", fx_range_set), settings_save("fx_hear_range", t);
        if (k) snprintf(value, (size_t)n, "%d tiles", R[k]); else snprintf(value, (size_t)n, "Original (6 tiles)");
        return 1;
    }
    case 57:  /* Footstep width (fx_feet_spread): the two feet's share of the way to a side, a percentage of the game's own */
        if (!FX.audio) return 0;
        if (step) fx_feet_set += step > 0 ? 50 : -50, fx_feet_set = fx_feet_set < 0 ? 0 : fx_feet_set > 800 ? 800 : fx_feet_set,
                  snprintf(t, sizeof t, "%d", fx_feet_set), settings_save("fx_feet_spread", t);
        if (fx_feet_set == 100) snprintf(value, (size_t)n, "Original");
        else if (fx_feet_set) snprintf(value, (size_t)n, "%d%%", fx_feet_set);
        else snprintf(value, (size_t)n, "Middle");
        return 1;
    case 59: case 60: {  /* Footstep volume (fx_step_volume), Footstep echo (fx_step_echo; Acoustics High's): 0 to 200 % in tens */
        int *v = id == 59 ? &fx_stepvol_set : &fx_stepecho_set;
        if (!FX.audio || (id == 60 && FX.acoustics != 2)) return 0;
        if (step) *v += step > 0 ? 10 : -10, *v = *v < 0 ? 0 : *v > 200 ? 200 : *v,
                  snprintf(t, sizeof t, "%d", *v), settings_save(id == 59 ? "fx_step_volume" : "fx_step_echo", t);
        if (*v) snprintf(value, (size_t)n, "%d%%", *v); else snprintf(value, (size_t)n, "Off");
        return 1;
    }
    case 58:  /* Echo detail (acoustics_quality): Acoustics High's rays and bounces a run */
        if (FX.acoustics != 2) return 0;
        if (step) ac_quality_set += step > 0 ? 1 : -1, ac_quality_set = ac_quality_set < 0 ? 0 : ac_quality_set > 3 ? 3 : ac_quality_set,
                  snprintf(t, sizeof t, "%d", ac_quality_set), settings_save("acoustics_quality", t);
        snprintf(value, (size_t)n, "%s", ac_quality_set == 0 ? "Low (1,024 rays)" : ac_quality_set == 1 ? "Medium (2,048 rays)" : ac_quality_set == 2 ? "High (4,096 rays)" : "Very high (8,192 rays)");
        return 1;
    case 19:  /* Headphone 3D sound (fx_hrtf: the backend's uwaudio.c reads it again after a change): On, Off, Automatic */
        if (step) fx_hrtf_set = fx_hrtf_set == 1 ? (step > 0 ? 0 : 2) : fx_hrtf_set == 0 ? (step > 0 ? 2 : 1) : (step > 0 ? 1 : 0),
                  snprintf(t, sizeof t, "%d", fx_hrtf_set), settings_save("fx_hrtf", t);
        snprintf(value, (size_t)n, "%s", fx_hrtf_set == 1 ? "On" : fx_hrtf_set == 0 ? "Off" : "Automatic");
        return 1;
    case 20:  /* Effects' sides (fx_pan_fix): the Roland effects' pan as the game meant it, or as the MT-32 reads it */
        if (step) fx_panfix_set = !fx_panfix_set, settings_save("fx_pan_fix", fx_panfix_set ? "1" : "0");
        snprintf(value, (size_t)n, "%s", fx_panfix_set ? "Corrected" : "As the MT-32 plays them");
        return 1;
    }
    return -1;
}
/* uwv_option's rows for the character sheet, the font, its size, the message text (ids 40-43): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_text(int id, int step, char *value, int n)
{
    char t[72];   /* (a face's name: up to 63) */
    switch (id) {
    case 40:  /* Character sheet (uwinv_draw.c): the full sheet, the default, or the game's page */
        if (step) uwv_sheet_full = !uwv_sheet_full, settings_save("char_sheet", uwv_sheet_full ? "full" : "original");
        snprintf(value, (size_t)n, "%s", uwv_sheet_full ? "Full" : "The game's page");
        return 1;
    case 42: {  /* Font (ui.c: one easy-to-read font for everything): the game's letters or a face */
        int nf = st_face_count() + 1, f = ui_font();
        if (step) {
            f = (f + (step > 0 ? 1 : nf - 1)) % nf, ui_set_font(f), settings_save("ui_font", f ? "1" : "0");
            if (f) {   /* kept by name: fonts added to the folder later do not move it */
                snprintf(t, sizeof t, "%s", st_face_label(f - 1));
                for (char *c = t; *c; c++) if (*c == ' ') *c = '_';
                settings_save("ui_font_name", t);
            }
        }
        snprintf(value, (size_t)n, "%s", f ? st_face_label(f - 1) : "The game's own");
        return 1;
    }
    case 43: {  /* Font size (down as far as the chosen face allows): in 5 %
                   steps of the size for the game's resolution (19 pixels at 1080 rows), from the face's smallest (6
                   pixels) to 60 */
        int h = ui_h > 0 ? ui_h : 1080, pct = st_size_pct();
        if (step) {
            int np = pct + (step > 0 ? 5 : -5), px = (h * 19 * np / 100 + 540) / 1080;   /* (st_game_em's, unclamped) */
            if (px >= 6 && px <= 60 && np >= 20 && np <= 300)                            /* (at an end: kept) */
                st_set_size_pct(np), snprintf(t, sizeof t, "%d", np), settings_save("ui_font_size", t);
        }
        snprintf(value, (size_t)n, "%d px (%d%%)", st_game_em(h), st_size_pct());
        return 1;
    }
    case 41:  /* Message text (uwv_log): a smooth smaller font, the default, or the game's letters */
        if (step) log_font_original = !log_font_original, settings_save("log_font", log_font_original ? "original" : "smooth");
        snprintf(value, (size_t)n, "%s", log_font_original ? "The original's" : "Smooth");
        return 1;
    }
    return -1;
}
/* uwv_option's rows for smooth motion, object shadows, the menu background, light fade, fire breathing (ids 9, 16, 61-63): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_motion_fire(int id, int step, char *value, int n)
{
    char t[72];   /* a row's new value as text, for settings_save */
    switch (id) {
    case 16:  /* Smooth motion: the camera eased between the game's steps (smooth_pose) */
        if (step) smooth_on = !smooth_on, settings_save("smooth_motion", smooth_on ? "1" : "0");
        snprintf(value, (size_t)n, "%s", smooth_on ? "On" : "Off");
        return 1;
    case 9:   /* the objects' shadows (ray tracing) */
        if (step) render_set_object_shadows(!render_object_shadows()), settings_save("object_shadows", render_object_shadows() ? "1" : "0");
        snprintf(value, (size_t)n, "%s", render_object_shadows() ? "On" : "Off");
        return 1;
    case 63:  /* Menu background: one dark colour, the default, or the leather */
        if (step) menu_leather = !menu_leather, md_set_solid(!menu_leather), settings_save("menu_background", menu_leather ? "leather" : "solid");
        snprintf(value, (size_t)n, "%s", menu_leather ? "Leather" : "Solid colour");
        return 1;
    case 62:  /* Light fade: 50% at 2 tiles, 30% at 3, gone at 4, every light; or as before */
        if (step) render_set_light_fade(!render_light_fade()), settings_save("light_fade", render_light_fade() ? "1" : "0");
        snprintf(value, (size_t)n, "%s", render_light_fade() ? "2 / 3 / 4 tiles" : "Original");
        return 1;
    case 61: {  /* Fire breathing: the fires' slow breath now and then, Off then 10 to 40 % */
        int pc = (int)(render_breath_depth() * 100.0f + 0.5f);
        if (step) {
            pc += step > 0 ? 10 : -10;
            pc = pc < 0 ? 0 : pc > 40 ? 40 : pc;
            render_set_breath_depth((float)pc / 100.0f);
            snprintf(t, sizeof t, "%.2f", render_breath_depth()), settings_save("flame_breath", t);
        }
        if (pc <= 0) snprintf(value, (size_t)n, "Off");
        else snprintf(value, (size_t)n, "%d%%", pc);
        return 1;
    }
    }
    return -1;
}
/* uwv_option's rows for joint and relief shadows, the textures and the three relief maps' detail (ids 50-55): 1 or 0 as uwv_option returns, -1 when id is not one of them */
static int opt_textures(int id, int step, char *value, int n)
{
    char t[72];   /* a row's new value as text, for settings_save */
    switch (id) {
    case 50: {  /* Joint shadows (AO): the walls' and floors' occlusion maps, Off then 25 to 200 % */
        int pc = (int)(S.texture_ao * 100.0f + 0.5f);
        if (step) {
            pc += step > 0 ? 25 : -25;
            pc = pc < 0 ? 0 : pc > 200 ? 200 : pc;
            S.texture_ao = (float)pc / 100.0f;
            snprintf(t, sizeof t, "%.2f", S.texture_ao), settings_save("texture_ao_strength", t);
        }
        if (pc <= 0) snprintf(value, (size_t)n, "Off");
        else snprintf(value, (size_t)n, "%d%%", pc);
        return 1;
    }
    case 51: {  /* Relief shadows: the relief's depth its shadows are cast from, Off then 1 to 8 */
        int d = (int)(S.texture_shadow + 0.5f);
        if (step) {
            d += step > 0 ? 1 : -1;
            d = d < 0 ? 0 : d > 8 ? 8 : d;
            S.texture_shadow = (float)d;
            snprintf(t, sizeof t, "%d", d), settings_save("texture_shadow", t);
        }
        if (d <= 0) snprintf(value, (size_t)n, "Off");
        else snprintf(value, (size_t)n, "Depth %d", d);
        return 1;
    }
    case 52: {  /* Textures: the walls' and floors' own, or upscaled 256 / 512; at once (hires_apply) */
        static const int RES[3] = {0, 256, 512};
        int k = tex_res == 512 ? 2 : tex_res == 256 ? 1 : 0;
        if (step) {
            k = (k + (step > 0 ? 1 : 2)) % 3;
            tex_res = RES[k];
            snprintf(t, sizeof t, "%d", tex_res), settings_save("texture_res", t);
            hires_apply();
        }
        snprintf(value, (size_t)n, "%s", k == 0 ? "Original" : k == 1 ? "Upscaled 256" : "Upscaled 512");
        return 1;
    }
    case 53: case 54: case 55: {  /* Normal / Joint shadow / Relief height map detail (a row for
                                   * each kind of map, with the textures' choices); at once (hires_apply) */
        static const int RES[4] = {-1, 0, 256, 512};
        static const char *const KEY[3] = {"normal_map_res", "ao_map_res", "height_map_res"};
        int c = id - 53, k = map_res[c] == 512 ? 3 : map_res[c] == 256 ? 2 : map_res[c] == 0 ? 1 : 0;
        if (step) {
            k = (k + (step > 0 ? 1 : 3)) % 4;
            map_res[c] = RES[k];
            snprintf(t, sizeof t, "%d", map_res[c]), settings_save(KEY[c], t);
            hires_apply();
        }
        snprintf(value, (size_t)n, "%s", k == 0 ? "As the textures" : k == 1 ? "Original" : k == 2 ? "256" : "512");
        return 1;
    }
    }
    return -1;
}
/* uwv_option's rows for fog, vertical sync, exposure, gamma and the other rows for settings.txt keys (ids 13, 90-104),
   and the level of this DLL's log (id 105, the front end's Log detail row): 1 or 0 as uwv_option returns, -1 when id
   is not one of them */
static int opt_more(int id, int step, char *value, int n)
{
    char t[72];   /* a row's new value as text, for settings_save */
    switch (id) {
    /* (rows for settings.txt keys the menu had no row for) */
    case 13: {  /* Fog and light shafts: off, or its quality 1-3 (fx_fog) */
        static const char *const Q[4] = {"Off", "Low", "Medium", "High"};
        if (step) FX.fog = (FX.fog + (step > 0 ? 1 : 3)) % 4, snprintf(t, sizeof t, "%d", FX.fog), settings_save("fx_fog", t),
                  render_set_fx(FX.particles, FX.decals, FX.water, FX.fog);
        snprintf(value, (size_t)n, "%s", Q[FX.fog & 3]);
        return 1;
    }
    case 90: {  /* Vertical sync (vsync): the swap chain is made with it, at the start */
        static int run = -1;
        if (run < 0) run = S.vsync != 0;
        if (step) S.vsync = !S.vsync, settings_save("vsync", S.vsync ? "1" : "0");
        snprintf(value, (size_t)n, "%s%s", S.vsync ? "On" : "Off", (S.vsync != 0) != run ? " (from the next start)" : "");
        return 1;
    }
    case 91: {  /* Exposure (the renderer's, at the start) */
        static float run = -1;
        if (run < 0) run = S.exposure;
        return opt_num(&S.exposure, step, 0.5f, 4.0f, 0.25f, "exposure", "", 1, run, value, n);
    }
    case 92: return opt_num(&S.gamma, step, 0.5f, 2.0f, 0.05f, "gamma", "", 0, 0, value, n);   /* the lights' gamma (Look, every frame) */
    case 93:    /* Invert mouse (our view's up and down: uwv_mouse) */
        if (step) S.invert_mouse = !S.invert_mouse, settings_save("invert_mouse", S.invert_mouse ? "1" : "0");
        snprintf(value, (size_t)n, "%s", S.invert_mouse ? "On" : "Off");
        return 1;
    case 94:    /* Your light: held up over the head (1) or at the eye */
        if (step) S.hand = !S.hand, settings_save("torch_in_hand", S.hand ? "1" : "0");
        snprintf(value, (size_t)n, "%s", S.hand ? "Over the head" : "At the eye");
        return 1;
    case 95: return opt_num(&S.sheen, step, 0, 1.0f, 0.05f, "sheen", "", 0, 0, value, n);                     /* the stone's sheen */
    case 96: {  /* Light bounces: 1 one; 2 and more the probes' light fed back; 0 as many as build up */
        if (step) S.bounces = (S.bounces + (step > 0 ? 1 : 4)) % 5, snprintf(t, sizeof t, "%d", S.bounces), settings_save("rt_bounces", t);
        if (S.bounces) snprintf(value, (size_t)n, "%d", S.bounces);
        else snprintf(value, (size_t)n, "As many as build up");
        return 1;
    }
    case 97: return opt_num(&S.ray_length, step, 2, 32, 1, "rt_ray_length", " tiles", 0, 0, value, n);   /* a bounce ray's reach */
    case 98: return opt_num(&S.sprite_strength, step, 0, 1.0f, 0.05f, "sprite_normal_strength", "", 0, 0, value, n);
    case 99: return opt_num(&S.sprite_ao, step, 0, 1.0f, 0.05f, "sprite_ao_strength", "", 0, 0, value, n);
    case 100: return opt_num(&S.sprite_height, step, 0, 4.0f, 0.25f, "sprite_height", " texels", 0, 0, value, n);
    case 101: case 102: {  /* DLSS 5's tone and saturation (the renderer's, at the start) */
        static float run[2] = {-1, -1};
        float *v = id == 101 ? &S.dlss5_tone : &S.dlss5_saturation;
        if (!render_dlss5_available()) return 0;
        if (run[id - 101] < 0) run[id - 101] = *v;
        return opt_num(v, step, 0, 2.0f, 0.05f, id == 101 ? "dlss5_tone" : "dlss5_saturation", "", 1, run[id - 101], value, n);
    }
    case 103: {  /* the campfires' swell */
        int r = opt_num(&camp_swell, step, 0, 0.6f, 0.05f, "campfire_swell", "", 0, 0, value, n);
        render_set_campfire_depth(camp_swell);
        return r;
    }
    case 104: {  /* the other fires' swell */
        int r = opt_num(&flame_swell, step, 0, 0.5f, 0.05f, "flame_swell", "", 0, 0, value, n);
        render_set_puff_depth(flame_swell);
        return r;
    }
    case 105: {  /* Log detail (the front end's own row, kept in its uw1port.cfg): this DLL's logger has its own level,
                    set here. step = the level + 1 (1 Errors only to 4 Everything); 0 reads it */
        static const char *const L[4] = {"Errors only", "Warnings", "Normal", "Everything"};
        if (step >= 1 && step <= 4) rlog_set_level(step - 1);
        snprintf(value, (size_t)n, "%s", L[rlog_level()]);
        return 1;
    }
    }
    return -1;
}
/* one of our renderer's Esc rows by id: its value as text, step != 0 changes it (and keeps it in settings.txt); 0 when
   the row is not offered. The Sprites tab's ids first, then each group of rows in turn */
UWV_API int uwv_option(int id, int step, char *value, int n)
{
    int r;
    if (!started) return 0;
    if (id >= 1000 && id < 1512) return spr_option(id - 1000, step, value, n);   /* the Sprites tab: a picture's row */
    if (id >= 3000 && id < 3512) return spr_label(id - 3000, value, n);          /* and the row's name */
    if ((r = opt_rendering(id, step, value, n)) >= 0) return r;
    if ((r = opt_light_view(id, step, value, n)) >= 0) return r;
    if ((r = opt_effects_glow(id, step, value, n)) >= 0) return r;
    if ((r = opt_colour_hdr(id, step, value, n)) >= 0) return r;
    if ((r = opt_water_sound_text(id, step, value, n)) >= 0) return r;
    if ((r = opt_acoustics(id, step, value, n)) >= 0) return r;
    if ((r = opt_text(id, step, value, n)) >= 0) return r;
    if ((r = opt_motion_fire(id, step, value, n)) >= 0) return r;
    if ((r = opt_textures(id, step, value, n)) >= 0) return r;
    if ((r = opt_more(id, step, value, n)) >= 0) return r;
    return 0;
}

/* the menu's drawing: src\menu_draw.c (there so the backends without this renderer have it too) */
UWV_API void uwv_menu_wheel(int notches) { md_wheel(notches); }
UWV_API int uwv_menu_click(int mx, int my, int down) { return md_click(ui_ok, mx, my, down); }
/* the rows' sliders (the Fog and smoke page; menu_draw.c): the places along their tracks for the next uwv_menu_rows2,
   and the pointer on them */
UWV_API void uwv_menu_sliders(const float *frac, int n) { md_sliders(frac, n); }
UWV_API int uwv_menu_slider(int mx, int my, int row, float *frac) { return md_slider(mx, my, row, frac); }
/* the menu's tabs (menu_draw.c): their names and the open one for the next uwv_menu_rows2; the one under the pointer */
UWV_API void uwv_menu_tabs(const char *const *names, int n, int open) { md_tabs(names, n, open); }
UWV_API int uwv_menu_tab_at(int mx, int my) { return md_tab_at(mx, my); }
UWV_API int uwv_menu_rows2(const char *title, const char *const *labels, const char *const *values,
                           const char *const *hints, int n, int sel, int mx, int my)
{
    if (!started || !ui_buf) return -1;
    if (!ui_ok) {
        char data[MAX_PATH], game[MAX_PATH];
        data_dirs(data, game);
        ui_ok = ui_start(data, out_w, out_h);
        if (!ui_ok) return -1;
    }
    return md_rows(ui_buf, ui_w, ui_h, !menu_font_original, title, labels, values, hints, n, sel, mx, my);
}
/* (an older backend: no hints) */
UWV_API int uwv_menu_rows(const char *title, const char *const *labels, const char *const *values, int n, int sel, int mx, int my)
{
    return uwv_menu_rows2(title, labels, values, NULL, n, sel, mx, my);
}

/* the interface layer ready for our panels: the renderer's overlay started (once) */
static bool ui_ready(void)
{
    if (!started || !ui_buf) return false;
    if (!ui_ok) {
        char data[MAX_PATH], game[MAX_PATH];
        data_dirs(data, game);
        ui_ok = ui_start(data, out_w, out_h);
    }
    return ui_ok;
}
/* the overlay onto the layer, its pixels o->scale window pixels each */
static void ui_blend_overlay(void)
{
    Overlay *o = ui_overlay();
    unsigned *d = (unsigned *)ui_buf;
    for (int yy = 0; yy < ui_h; yy++) {
        int oy = yy / o->scale;
        if (oy >= o->h) break;
        const u8 *orow = o->px + (size_t)oy * o->w * 4;
        for (int xx = 0; xx < ui_w; xx++) {
            int ox = xx / o->scale;
            if (ox >= o->w) break;
            const u8 *p = orow + ox * 4;
            if (p[3] == 0) continue;
            unsigned a = p[3], na = 255 - a, c = d[(size_t)yy * ui_w + xx];
            unsigned r = (p[0] * a + (c & 0xFF) * na) / 255, g = (p[1] * a + (c >> 8 & 0xFF) * na) / 255, b = (p[2] * a + (c >> 16 & 0xFF) * na) / 255;
            d[(size_t)yy * ui_w + xx] = 0xFF000000u | r | g << 8 | b << 16;
        }
    }
    ui_text_apply(d, ui_w, ui_h);   /* (the interface font's smooth text over the overlay: ui.c) */
}

/* Our inventory panel (Tab, full-screen play; src/uwinv_draw.c): the player's things as the original holds them
 * (the snapshot from the game's thread), drawn over the interface layer the last uwv_hud made; mx, my the pointer in
 * window pixels; sheet: the character sheet beside it. Returns what the pointer is on (uwinv_draw.c's uwi_panel). */
int uwi_panel(const UwiSnap *s, int mx, int my, int sheet, int draw, float *fx, float *fy);
/* the size of the snapshot this DLL was built with: the backend uses the panel only when its own is the same (an
   exe and a DLL of different builds can read the panel's words from the wrong places and crash) */
UWV_API int uwv_snap_size(void) { return (int)sizeof(UwiSnap); }
UWV_API int uwv_inventory(const UwiSnap *s, int mx, int my, int sheet)
{
    if (!ui_ready() || !s) return -1;
    ui_clear();
    int hit = uwi_panel(s, mx, my, sheet, 1, NULL, NULL);
    ui_blend_overlay();
    inv_frame = hud_frames;
    return hit;
}
/* the open bags' rows scrolled (uwinv_draw.c): the wheel over the panel, and a left press / release on its slider */
void uwi_inv_wheel(int notches);
int uwi_inv_click(int mx, int my, int down);
UWV_API void uwv_inv_wheel(int notches) { uwi_inv_wheel(notches); }
UWV_API int uwv_inv_click(int mx, int my, int down) { return ui_ready() ? uwi_inv_click(mx, my, down) : 0; }
/* what the pointer is on, without drawing (for a click between two frames) */
UWV_API int uwv_inv_hit(const UwiSnap *s, int mx, int my, int sheet, float *fx, float *fy)
{
    if (!ui_ready() || !s) return -1;
    return uwi_panel(s, mx, my, sheet, 0, fx, fy);
}

/* uwv_frame's step: the checks' UWVIEW_COMPARE_FX, the effect on in the first picture and off in the second (in
   the normal maps' place), and the scene's clock held while they are taken */
static void frame_compare_fx(Look *look)
{
    const char *cf = getenv("UWVIEW_COMPARE_FX");   /* the checks: the effect on / off in place of the normal maps */
    if (cmp_phase == 2 && !(cf && *cf)) look->normal_maps = false;
    /* "texture": the walls' and floors' AO and relief shadows on, then off (menucheck textures) */
    if (cmp_phase == 2 && cf && !strcmp(cf, "texture")) look->texture_ao = 0, look->texture_shadow = 0;
    if (cf && *cf && (cmp_phase == 1 || cmp_phase == 2)) {
        int on = cmp_phase == 1;
        render_set_fx(!strcmp(cf, "particles") ? on : FX.particles, !strcmp(cf, "decals") ? on : FX.decals,
                      !strcmp(cf, "water") ? on : FX.water || !strcmp(cf, "ripple"), !strcmp(cf, "fog") ? (on ? (FX.fog ? FX.fog : 2) : 0) : FX.fog);
        render_water_ripples_shown(strcmp(cf, "ripple") || on);
    } else if (cmp_phase == 3 && cf && *cf) render_set_fx(FX.particles, FX.decals, FX.water, FX.fog);
    if (steady_check()) render_hold_time(true);   /* (from the first frame: the frame count stays 0) */
    else if (cf && *cf) render_hold_time(cmp_phase == 1 || cmp_phase == 2);
}
/* uwv_frame's step: the player's light (the torch) for this frame: where it is, its motion, its colour */
static void frame_torch(void)
{
    /* the player's light, as our engine makes it from the original's light level (inventory.c's inv_light): the
     * intensity 0.5 + 0.4 a level, the reach the original's view distance for the level (SHADES.DAT's 4th word,
     * UW.EXE 0x810f8: 3 3 4 4 5 6 7 7 tiles), a torch's warm colour */
    static const float REACH[8] = {3, 3, 4, 4, 5, 6, 7, 7};
    static const float warm[3] = {1.0f, 0.70f, 0.40f};
    /* held up over the head, in the very centre of the body (above the player, not off to the side): the light
     * above the eye, so that it still crosses
     * floors and walls at an angle from above and the normal maps read, but the same to the left and the right.
     * (Not a hand's reach to the right: a door seen edge-on would light its right face at once and its left only
     * 0.1 tile further on, as the light crossed the door's plane.) */
    float tp[3] = {cam.pos[0], cam.pos[1], cam.pos[2]};
    /* no light carried (level 0): the light is the player's own eyes: steady, at the eye, white, a fifth
       shorter (common.hlsli's original_light for level 0); a torch, lantern, candle or spell keeps its flame */
    /* UWVIEW_STEADY=1 (the checks: tools\vkcompare.py): a carried light steady too, at the eye and white as with none
       (its flame's puffs go by the clock, so no two runs' pictures were the same: 815 235 pixels of a 1920 x 1080
       corridor apart between two D3D12 runs) */
    const bool eyes = light_level == 0 || steady_check();
    render_set_torch_steady(eyes);
    if (S.hand && !eyes) {
        const float above = 0.1f, top = level_ceil_y() - 0.05f;   /* a tenth of a tile over the eye, under the ceiling */
        tp[1] = cam.pos[1] + above < top ? cam.pos[1] + above : (top > cam.pos[1] ? top : cam.pos[1]);
    }
    {   /* never under the floor nor in a wall (in water the player sinks: a torch below the floor lit nothing, every
           shadow ray blocked): at least a little above the floor where it is, else back at the eye */
        float fy = level_floor_y(&lv, tp[0], tp[2]);
        if (fy >= level_ceil_y() - 0.01f) tp[0] = cam.pos[0], tp[2] = cam.pos[2], fy = level_floor_y(&lv, tp[0], tp[2]);
        if (tp[1] < fy + 0.15f) tp[1] = fy + 0.15f < cam.pos[1] ? fy + 0.15f : cam.pos[1];
    }
    /* the original's light: its shades by the distance from the eye, the light level's (SHADES.DAT curvrad: 3 3 4 4
     * 5 6 7 7 tiles, then nothing) */
    (void)warm, (void)REACH;
    {
        static const float CURVRAD[8] = {3, 3, 4, 4, 5, 6, 7, 7};
        float eye[3] = {cam.pos[0], cam.pos[1], cam.pos[2]};
        /* the torch's flicker (the physics of fire): it puffs at about 7 a second (a flame 5 cm across:
           1.5 / sqrt(0.05)), a little at rest; moving, the air past it tears at it, deeper the faster, and leans the
           flame back (the light a little behind the hand) */
        static float last[3], speed;
        static unsigned long long lt;
        unsigned long long now = GetTickCount64();
        float dt = lt ? (float)(now - lt) / 1000.0f : 0;
        lt = now;
        if (dt > 0 && dt < 0.25f) {
            float dx = cam.pos[0] - last[0], dz = cam.pos[2] - last[2], v = sqrtf(dx * dx + dz * dz) / dt;
            if (v > 8) v = 8;   /* a teleport */
            speed += (v - speed) * (dt * 6 > 1 ? 1 : dt * 6);
            {   /* the torch's velocity, smoothed (tiles a second), for the flame's lean (render.c's render_flame) */
                static float vx, vz;
                float a = dt * 6 > 1 ? 1 : dt * 6, nx = v < 8 ? dx / dt : 0, nz = v < 8 ? dz / dt : 0;
                vx += (nx - vx) * a, vz += (nz - vz) * a;
                render_set_torch_motion(vx, vz);
            }
        }
        memcpy(last, cam.pos, 12);
        float s = (float)(now % 1000000ull) / 1000.0f, w = 1.5f / sqrtf(0.05f) * 6.2832f;
        float depth = 0.04f + 0.10f * (speed > 2 ? 1 : speed / 2);
        float flick = 1.0f + depth * (0.55f * sinf(s * w) + 0.25f * sinf(s * w * 1.73f + 1.3f) + 0.12f * sinf(s * w * 3.17f + 2.9f) +
                                      0.08f * sinf(s * w * 5.9f + 0.7f));
        flick = 1;   /* the flicker, the sway and the lean are the flame model's now (render.c's render_flame) */
        (void)depth;
        {   /* its colour: a pitch torch, 1700 K, hotter as it flares, cooler as it ebbs (white in full bright) */
            float rgb[3] = {1, 1, 1};
            if (!render_fullbright() && !eyes) render_blackbody(1700.0f + (flick - 1.0f) * 900.0f, rgb);
            render_set_torch_colour(rgb);
        }
        render_set_torch_original(lv_number > 0, tp, eye, light_level, flick, 2.2f * S.gamma);   /* 3 tiles (common.hlsli TORCH_REACH) + the hand's offset */
        (void)CURVRAD;
    }
}
/* uwv_frame's step: the moving light (settings.txt moving_light, level 1) along its hall and back */
static void frame_moving_light(void)
{
    if (S.moving_light && lv_number == 1 && !mv_tried) moving_light_place();
    if (mv_index >= 0) { /* along the hall and back, a tile and a half a second, on repeat */
        float dx = mv_b[0] - mv_a[0], dz = mv_b[1] - mv_a[1], len = sqrtf(dx * dx + dz * dz);
        static float t;
        if (!cmp_phase) t = (float)(GetTickCount64() % 1000000ull) / 1000.0f * 1.5f; /* held still for a comparison */
        float ph = fmodf(t, 2.0f * len);
        float f = (ph < len ? ph : 2.0f * len - ph) / len;
        float p[3] = {mv_a[0] + dx * f, 0, mv_a[1] + dz * f};
        p[1] = level_floor_y(&lv, p[0], p[2]) + 0.6f;
        render_set_light_pos(mv_index, p);
    }
}
/* uwv_frame's step after the frame: the checks' UWVIEW_COMPARE pictures (on, then off) written, and the two
   compared */
static void frame_compare_shots(bool ok)
{
    if (ok && (cmp_frames == cmp_at(1) || cmp_frames == cmp_at(2)) && (cmp_phase == 1 || cmp_phase == 2)) {
        const char *pre = getenv("UWVIEW_COMPARE");
        unsigned char *px = malloc((size_t)out_w * out_h * 4);
        char p[MAX_PATH];
        bool wrote = false;
        if (!px) RLOG_E("view", "the comparison's picture: out of memory");
        if (px && render_readback(px)) {
            snprintf(p, sizeof p, "%s-%s.png", pre, cmp_phase == 1 ? "on" : "off");
            wrote = png_write(p, px, out_w, out_h, 4);
            if (!wrote) RLOG_E("view", "cannot write %s", p);
            if (cmp_phase == 1) {
                free(cmp_a);
                cmp_a = px, px = NULL;
            } else if (cmp_a) {
                long long sum = 0;
                int changed = 0, n = out_w * out_h;
                for (int i = 0; i < n; i++) {
                    int d = abs(cmp_a[i * 4] - px[i * 4]) + abs(cmp_a[i * 4 + 1] - px[i * 4 + 1]) + abs(cmp_a[i * 4 + 2] - px[i * 4 + 2]);
                    sum += d, changed += d > 12;
                }
                RLOG_I("view", "%s on / off: %d of %d pixels differ by more than 12 (%.1f%%), mean difference %.2f",
                        getenv("UWVIEW_COMPARE_FX") && *getenv("UWVIEW_COMPARE_FX") ? getenv("UWVIEW_COMPARE_FX") : "normal maps",
                        changed, n, 100.0 * changed / n, (double)sum / n / 3.0);
            }
            if (wrote) RLOG_I("view", "wrote %s", p);
        }
        free(px);
        cmp_phase++;
    }
}
/* one frame: the interface layer (uwv_ui_size, RGBA, rows from the top) over the scene, presented */
static bool clip_wanted;                /* P: the next frame to the clipboard */
UWV_API int uwv_frame(const unsigned char *ui_rgba)
{
    if (!started) return 0;
    int uw, uh, us;
    render_ui_size(&uw, &uh, &us);
    if (cmp_phase == 1 && cmp_frames == 0 && steady_check()) fx_steady_reset = 1;   /* (the emitters from nothing: fx_frame) */
    if (cmp_phase == 1 && cmp_frames == 0) { /* the picture as it is now, kept */
        free(cmp_ui);
        cmp_ui = malloc((size_t)uw * uh * 4);
        if (cmp_ui) memcpy(cmp_ui, ui_rgba, (size_t)uw * uh * 4);
        else RLOG_E("view", "the comparison's interface layer: out of memory (the live one used)");
    }
    render_set_ui(cmp_phase && cmp_phase < 3 && cmp_ui ? cmp_ui : ui_rgba);
    Look look = {S.gamma, S.normal_maps != 0, S.normal_strength, S.bounce, S.sheen, S.ray_length, S.bounces, S.sprite_strength, S.sprite_ao, S.sprite_height, S.texture_ao, S.texture_shadow};
    frame_compare_fx(&look);
    fx_frame();
    if (cmp_phase == 1 || cmp_phase == 2) {
        cmp_frames++;
        if (cmp_frames == cmp_at(1) || cmp_frames == cmp_at(2)) render_want_screenshot();
    }
    frame_torch();
    frame_moving_light();
    if (!cmp_phase) things_update();
    /* UWVIEW_SHOT_CRIT=FILE (the checks): the picture once a creature has been drawn for 30 frames */
    const char *crit_shot = getenv("UWVIEW_SHOT_CRIT");
    static bool crit_shot_done;
    bool crit_now = crit_shot && *crit_shot && !crit_shot_done && crit_frames == 30;
    if (crit_now && getenv("UWVIEW_COMPARE") && *getenv("UWVIEW_COMPARE")) { /* the comparison there instead */
        crit_shot_done = true, crit_now = false;
        if (!cmp_phase) cmp_phase = 1, cmp_frames = 0;
    }
    if (crit_now) render_want_screenshot();
    bool reset = cam_reset;
    cam_reset = false;
    bool shot_clip = clip_wanted;
    if (uwrec_active() || shot_clip) render_want_screenshot();   /* P / Shift+P: this frame's picture */
    {   /* UWVIEW_NOSCENE=1 (the checks: tools\vkcompare.py): the scene tinted black, the interface alone compared
           between the renderers while the Vulkan one has no scene yet */
        static int noscene = -1;
        if (noscene < 0) { const char *e = getenv("UWVIEW_NOSCENE"); noscene = e && *e && *e != '0'; }
        if (noscene) render_set_tint(0, 0, 0, true);
    }
    bool ok = render_frame(&cam, &look, reset);
    if (ok && shot_clip) clip_wanted = false, uwrec_clipboard(g_hwnd, out_w, out_h, render_readback);
    if (ok) uwrec_frame(render_readback);
    if (ok && crit_now) {
        unsigned char *px = malloc((size_t)out_w * out_h * 4);
        if (px && render_readback(px) && png_write(crit_shot, px, out_w, out_h, 4)) RLOG_I("view", "wrote %s", crit_shot);
        else RLOG_E("view", "cannot write %s%s", crit_shot, px ? " (no picture read back, or the PNG not written)" : ": out of memory");
        free(px);
        crit_shot_done = true;
    }
    frame_compare_shots(ok);
    return ok;
}

/* --- the effects' particles (render_particles.c draws them): spawned here --- */
static void gpu_part(const float p[3], const float v[3], float life, float size, const float c[3], int kind, float grav, float drag)
{
    RParticle q;
    memcpy(q.pos, p, 12), memcpy(q.vel, v, 12), memcpy(q.col, c, 12);
    q.life = life, q.age = 0, q.size = size, q.kind = (float)kind, q.grav = grav, q.drag = drag;
    q.floor_y = lv_number ? level_floor_y(&lv, p[0], p[2]) : -99.0f;
    render_particles_spawn(&q, 1);
}
/* a mark on the floor at x, z (tiles), as the effects' decals (render_decals.c) */
static void fx_decal(float x, float z, int kind, float size, const float rgb[3], int who)
{
    if (!lv_number) return;
    render_decals_add(x, level_floor_y(&lv, x, z), z, kind, size, frand() * 6.2831853f, rgb);
    if (getenv("UWVIEW_FX_LOG")) RLOG_I("view", "decal %d at %.2f %.2f (creature %d), %d in all", kind, x, z, who, render_decals_count());
}
static int fx_emitters;    /* the flames giving embers this frame */
static float fx_emit_screen[2] = {-1, -1};   /* the nearest one in view (0..1 across and down): the checks' region */
/* each frame: embers and the odd spark above every flame within 14 tiles (the wall torches, the burning things), each
 * at its flame's own rate (a torch about 6 a second, a campfire about 25) */
#include "volfog.inc"

/* each frame: the embers, sparks, smoke and water fog for this frame (the fog and smoke's own frame first) */
static void fx_frame(void)
{
    static float acc[MAX_LIGHTS];
    static unsigned long long t0;
    static int log = -1;
    static unsigned long long log_t;
    unsigned long long now = GetTickCount64();
    float dt = t0 ? (float)(now - t0) / 1000.0f : 0;
    t0 = now;
    if (dt > 0.1f) dt = 0.1f;
    if (steady_check()) dt = 0.016f;   /* (a check's: a frame is 16 ms to the emitters, as UWVIEW_PART_DT=16 to the particles) */
    vol_shot(dt), vol_frame(dt);   /* the fog and smoke: this frame's dials, fires and bodies to the renderer */
    if (fx_steady_reset) {
        fx_steady_reset = 0;
        render_particles_clear();
        memset(acc, 0, sizeof acc);
        srand(1);
    }
    if (log < 0) log = getenv("UWVIEW_FX_LOG") != NULL;
    {
        static int tested;
        const char *t = getenv("UWVIEW_FX_TEST");
        if (t && !strcmp(t, "splash") && lv_number && cam_frames_still == 40) {   /* a splash's ring 1.5 tiles ahead */
            float x = cam.pos[0] + sinf(cam.yaw) * 1.5f, z = cam.pos[2] + cosf(cam.yaw) * 1.5f;
            render_water_ripple(x, z, 1.5f);
            fx_decal_screen(x, level_floor_y(&lv, x, z), z);
            tested = 1;
        }
        if (t && !strcmp(t, "decal") && lv_number && cam_frames_still == 40) {   /* (each time the eye has settled) */
            float dd = 1.5f, x, z;   /* (on open floor: nearer while the place is in a wall) */
            do x = cam.pos[0] + sinf(cam.yaw) * dd, z = cam.pos[2] + cosf(cam.yaw) * dd, dd -= 0.1f; while (dd > 0.5f && level_solid_at(&lv, x, z));
            float rgb[3];
            blood_of(0, rgb);
            if (steady_check()) srand(1);   /* (a check's: the mark's turn, frand, the same in every run) */
            fx_decal(x, z, RD_BLOOD, 0.3f, rgb, 0);
            fx_decal_screen(x, level_floor_y(&lv, x, z), z);
            tested = 1;
        }
    }
    if (!FX.particles || !lv_number || cmp_phase == 2) return;
    fx_emitters = 0;
    float best = 1e9f, fwd[3] = {sinf(cam.yaw) * cosf(cam.pitch), sinf(cam.pitch), cosf(cam.yaw) * cosf(cam.pitch)};
    for (int k = 0; k < nlights && k < MAX_LIGHTS; k++) {
        const Light *L = &lights[k];
        if (L->kind != LIGHT_WALL_TORCH || L->kelvin <= 0 || L->flame < 0.011f) continue;
        float d[3] = {L->pos[0] - cam.pos[0], L->pos[1] - cam.pos[1], L->pos[2] - cam.pos[2]}, dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
        if (dd > 196) continue;
        fx_emitters++;
        float ahead = d[0] * fwd[0] + d[1] * fwd[1] + d[2] * fwd[2];
        if (ahead > 0.3f && dd < best) {   /* where it shows (the camera's own projection, as uwv_pick's ray) */
            float r[3] = {cosf(cam.yaw), 0, -sinf(cam.yaw)}, u[3] = {fwd[1] * r[2] - fwd[2] * r[1], fwd[2] * r[0] - fwd[0] * r[2], fwd[0] * r[1] - fwd[1] * r[0]};
            int ow, oh, os;
            render_ui_size(&ow, &oh, &os);
            float ty = lens_ty > 0 ? lens_ty : tanf(cam.fov_y * 0.5f), tx = lens_tx > 0 ? lens_tx : ty * (float)ow / (float)(oh > 0 ? oh : 1);
            float sx = (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / ahead / tx, sy = (d[0] * u[0] + d[1] * u[1] + d[2] * u[2]) / ahead / ty;
            if (fabsf(sx) < 1 && fabsf(sy) < 1) best = dd, fx_emit_screen[0] = sx * 0.5f + 0.5f, fx_emit_screen[1] = 0.5f - sy * 0.5f;
        }
        acc[k] += dt * (3.0f + 45.0f * L->flame);
        while (acc[k] >= 1.0f) {
            acc[k] -= 1.0f;
            float base[3] = {L->pos[0] + (frand() - 0.5f) * L->flame * 0.6f, L->pos[1] + 0.03f + frand() * 0.05f, L->pos[2] + (frand() - 0.5f) * L->flame * 0.6f};
            bool spark = frand() < 0.12f;   /* now and then one pops out and falls */
            float v[3] = {(frand() - 0.5f) * (spark ? 0.9f : 0.25f), spark ? 0.6f + frand() * 0.6f : 0.35f + frand() * 0.45f, (frand() - 0.5f) * (spark ? 0.9f : 0.25f)};
            float c[3] = {1.0f, 0.55f + frand() * 0.2f, 0.18f};
            gpu_part(base, v, spark ? 0.7f + frand() * 0.5f : 0.9f + frand() * 0.9f, 0.006f + frand() * 0.005f, c, 1, spark ? 3.0f : -0.35f, spark ? 0.3f : 1.2f);
        }
        /* a campfire's smoke (smoke that thins out over time): grey puffs
           rising from above the flames, slowing, widening and thinning out over 5 to 8 seconds, lit from below by the
           fire while young (particles.hlsl's kind 5); under the ceiling they spread */
        if (GL.camp_smoke && L->flame >= 0.4f && !vol_covers(L->pos[0], L->pos[2], 1)) {   /* (inside the box of air: the real smoke) */
            static float sacc[MAX_LIGHTS];
            sacc[k] += dt * 5.0f;
            while (sacc[k] >= 1.0f) {
                sacc[k] -= 1.0f;
                float a = frand() * 6.2831853f, sp = frand() * 0.07f;
                float base[3] = {L->pos[0] + (frand() - 0.5f) * 0.16f, L->pos[1] + 0.22f + frand() * 0.12f, L->pos[2] + (frand() - 0.5f) * 0.16f};
                float v[3] = {cosf(a) * sp, 0.26f + frand() * 0.16f, sinf(a) * sp};
                float g = 0.26f + frand() * 0.08f, c[3] = {g, g * 0.97f, g * 0.93f};
                gpu_part(base, v, 5.0f + frand() * 3.0f, 0.06f + frand() * 0.04f, c, 5, -0.03f, 0.25f);
            }
        }
    }
    /* the fog over the water: soft pale wisps lying just above every
       water tile within 7 tiles of the eye, drifting slowly, each coming and going over 7 to 12 seconds (kind 4) */
    if (GL.water_mist) {
        static float macc;
        static u16 wt[225];
        int nw = 0, cx = (int)cam.pos[0], cz = (int)cam.pos[2];
        for (int z = cz - 7; z <= cz + 7; z++)
            for (int x = cx - 7; x <= cx + 7; x++)
                if (x >= 0 && x < 64 && z >= 0 && z < 64 && m.grid[(z * 64 + x) * 4 + 2] == 1 && !vol_covers(x + 0.5f, z + 0.5f, 0)) wt[nw++] = (u16)(z * 64 + x);
        macc += dt * (float)nw * 0.8f;
        if (!nw) macc = 0;
        while (macc >= 1.0f && nw) {
            macc -= 1.0f;
            int t = wt[rand() % nw];
            float p[3] = {(float)(t % 64) + frand(), 0, (float)(t / 64) + frand()};
            p[1] = level_floor_y(&lv, p[0], p[2]) + 0.14f + frand() * 0.2f;
            float v[3] = {(frand() - 0.5f) * 0.07f, (frand() - 0.5f) * 0.01f, (frand() - 0.5f) * 0.07f};
            static const float MIST[3] = {0.72f, 0.78f, 0.84f};
            gpu_part(p, v, 7.0f + frand() * 5.0f, 0.2f + frand() * 0.12f, MIST, 4, 0.0f, 0.0f);
        }
    }
    if (log && now - log_t > 1000) log_t = now, fprintf(stderr, "uwview: particles: %d live, %d flames, the nearest in view at %.3f %.3f\n",
                                                        render_particles_live(), fx_emitters, fx_emit_screen[0], fx_emit_screen[1]);
}
/* the checks: where a place shows on the screen (0..1 across and down), the camera's own projection as uwv_pick's */
static void fx_decal_screen(float x, float y, float z)
{
    float fwd[3] = {sinf(cam.yaw) * cosf(cam.pitch), sinf(cam.pitch), cosf(cam.yaw) * cosf(cam.pitch)};
    float r[3] = {cosf(cam.yaw), 0, -sinf(cam.yaw)}, u[3] = {fwd[1] * r[2] - fwd[2] * r[1], fwd[2] * r[0] - fwd[0] * r[2], fwd[0] * r[1] - fwd[1] * r[0]};
    float d[3] = {x - cam.pos[0], y - cam.pos[1], z - cam.pos[2]}, ahead = d[0] * fwd[0] + d[1] * fwd[1] + d[2] * fwd[2];
    int ow, oh, os;
    render_ui_size(&ow, &oh, &os);
    float ty = lens_ty > 0 ? lens_ty : tanf(cam.fov_y * 0.5f), tx = lens_tx > 0 ? lens_tx : ty * (float)ow / (float)(oh > 0 ? oh : 1);
    if (ahead > 0.05f)
        fprintf(stderr, "uwview: decal test at screen %.3f %.3f\n", (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / ahead / tx * 0.5f + 0.5f,
                0.5f - (d[0] * u[0] + d[1] * u[1] + d[2] * u[2]) / ahead / ty * 0.5f);
}
/* a new level: the old level's particles and marks gone */
static void fx_level_change(int n)
{
    render_particles_clear();
    render_decals_clear();
    if (getenv("UWVIEW_FX_LOG")) fprintf(stderr, "uwview: decals: level %d: cleared, %d\n", n, render_decals_count());
    if (getenv("UWVIEW_FX_LOG")) fprintf(stderr, "uwview: particles: level %d: cleared, %d live\n", n, render_particles_live());
}

/* the next frame read back (w*h RGBA, the window's size): call before uwv_frame, read after it */
UWV_API void uwv_want_shot(void)
{
    const char *pre = getenv("UWVIEW_COMPARE");
    if (pre && *pre) {
        if (!cmp_phase) cmp_phase = 1, cmp_frames = 0;
        return;
    }
    if (started) render_want_screenshot();
}
UWV_API int uwv_readback(unsigned char *rgba) { return started && render_readback(rgba); }

/* Shift+P: the next seconds of the window to videos\uw-DATE-TIME.mp4 beside the game, Explorer opened on it
 * when done; 1 if it started */
UWV_API int uwv_record(int seconds)
{
    char dir[MAX_PATH];
    if (!started) return 0;
    snprintf(dir, sizeof dir, "%s\\videos", root);
    return uwrec_start(dir, out_w, out_h, seconds);
}
/* the seconds left of the video being made (0: none) */
UWV_API int uwv_recording(void) { return uwrec_seconds_left(); }
/* P: the next frame (what the window shows) to the clipboard */
UWV_API void uwv_clipboard_shot(void) { if (started) clip_wanted = true; }

/* the renderer stopped (a video being made stopped first) */
UWV_API void uwv_shutdown(void)
{
    uwrec_shutdown();
    if (started) render_shutdown();
    started = false;
}

/* an effect event from the game (fxevent.c, drained by the backend once a picture): x, y, z the renderer's tiles, fx the
 * SOUNDS.DAT entry or the item, mob the mobile or 0. The renderer's effects take what they need of them */
UWV_API void uwv_fx_event(int kind, float x, float y, float z, int fx, int mob)
{
    static int log = -1;
    if (log < 0) log = getenv("UWVIEW_FX_LOG") != NULL;
    (void)mob;
    if (!started || !lv_number || (kind & 0xFF) != 1) return;
    if (log) fprintf(stderr, "uwview: fx event sound %d at %.2f %.2f %.2f\n", fx, x, y, z);
    if (FX.decals && fx == 4 && x >= 0) {   /* a blow landed on a creature (COMBAT.C's sound 4 at it): its blood on the floor */
        int who = creature_near(x, z);
        float rgb[3];
        blood_of(who, rgb);
        fx_decal(x, z, RD_BLOOD, 0.16f + frand() * 0.12f, rgb, who);
        if (frand() < 0.5f) fx_decal(x + (frand() - 0.5f) * 0.4f, z + (frand() - 0.5f) * 0.4f, RD_BLOOD, 0.08f + frand() * 0.06f, rgb, who);
    }
    if (FX.water && fx == 0 && x >= 0) render_water_ripple(x, z, 1.0f);   /* the player in water: a ring where they wade */
    if (FX.particles && fx == 0 && x >= 0) {   /* the player in water (PLAYMOVE.C's water sound): a splash at the feet */
        static const float DROP[3] = {0.45f, 0.55f, 0.65f};
        float p[3] = {x, level_floor_y(&lv, x, z) + 0.02f, z};
        for (int k = 0; k < 24; k++) {
            float a = frand() * 6.2831853f, sp = 0.3f + frand() * 0.9f;
            float v[3] = {cosf(a) * sp, 1.2f + frand() * 1.8f, sinf(a) * sp};
            gpu_part(p, v, 0.5f + frand() * 0.4f, 0.012f + frand() * 0.006f, DROP, 2, 9.0f, 0.3f);
        }
    }
}

UWV_API const char *uwv_status(void) { return started ? render_status() : "not started"; }
