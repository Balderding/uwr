/* volfog.inc: the fog and smoke's dials and what the game gives the air each frame (the user, 2026-11-17: "physical
 * simulation of smoke and water haze fog ... displaced by you when you move through it"; "they both need esc menu
 * sliders and inputs"; "exhausting the options for the feature"). Included by uwview.c (it uses its settings_save,
 * cam, lights, lv_block, doors). The renderer's half: render_vk_vol.h and shaders\volume.hlsl (render.h RenderVolume).
 *
 * Every number the simulation and its picture use is a row of the Esc menu's Fog and smoke page (VD below: its key in
 * settings.txt, its label, its hint, its range), kept in settings.txt as the other rows are. The page is drawn by the
 * front end from the exports at the foot of this file (uwv_vol_*). */

enum { VK_HEAD, VK_SWITCH, VK_SLIDER, VK_CHOICE, VK_ACTION };
typedef struct {
    const char *key, *label, *hint;
    int kind;
    float lo, hi, step, dflt;
    const char *unit;   /* a slider's unit; a choice's names, "A|B|C" (the value is the name's number) */
    int dec;            /* a slider's decimals */
} VolDial;

enum {
    VH_MAIN, V_ON, V_LEVEL,
    VH_FOG, F_ON, F_AMOUNT, F_EMIT, F_SRC_WATER, F_SRC_LAVA, F_RISE, F_HEAT, F_SETTLE, F_LAVA_T, F_SPREAD, F_DECAY, F_LAYER,
    F_R, F_G, F_B, F_BRIGHT, F_EXT, F_GLOW, F_SOFT, F_DETAIL, F_SHAPE, F_STREAM, F_STIR, F_COLD, F_EDGEDRY,
    VH_SMOKE, S_ON, S_AMOUNT, S_EMIT, S_SRC_CAMP, S_SRC_TORCH, S_SRC_BLAST, S_TEMP_CAMP, S_TEMP_TORCH, S_RISE, S_HEAT, S_SPREAD, S_DECAY, S_COOL,
    S_R, S_G, S_B, S_BRIGHT, S_EXT, S_GLOW, S_SOFT, S_DETAIL, S_SHAPE, S_PUFF, S_PUFFRATE, S_KICK,
    VH_AIR, A_ROOM_T, A_SHARP, A_DRAG, A_SWIRL, A_DETSIZE, A_CORE, A_STRETCH, A_SHEAR, A_DRAFT, A_DRAFTDIR,
    VH_PUSH, P_YOU, P_PUSH, P_WAKE, P_CREATURES, P_DOORS, P_MISSILES, P_BLAST,
    VH_LIGHT, L_SHADOW, L_SAMPLES, L_SHAFTS, L_TORCH, L_OTHERS, L_SELF, L_MULTI, L_BOUNCE,
    VH_QUALITY, Q_CELLS, Q_BOX, Q_RATE, Q_PRESSURE, Q_VIEW, Q_HALF, Q_LREFRESH, Q_LKEEP, Q_FAR,
    VD_N
};

static const VolDial VD[VD_N] = {
    [VH_MAIN] = {"", "#Fog and smoke", "", VK_HEAD},
    [V_ON] = {"vol_on", "Fog and smoke", "On: fog and smoke are real clouds in the air. They drift, rise, sink, thin out, catch the light, and you push them aside. Vulkan renderer only.", VK_SWITCH, 0, 1, 1, 1},

    [V_LEVEL] = {"vol_level_own", "Each level's own fog", "On: a level can have its own fog and smoke, made in the Underworld Editor (Level, Fog and smoke): more or less haze and smoke, a draught, its air's temperature, and fog and smoke placed on the map. Kept in DATA\\UWRFOG.TXT. Off: every level by these rows alone.", VK_SWITCH, 0, 1, 1, 1},

    [VH_FOG] = {"", "#Fog (haze over water)", "", VK_HEAD},
    [F_ON] = {"vol_fog", "Fog", "On: a haze lies over water and steam rises over lava. Off: no fog; smoke is not changed.", VK_SWITCH, 0, 1, 1, 1},
    [F_AMOUNT] = {"vol_fog_amount", "Fog thickness", "How thick the fog looks for the same amount of it in the air: 0 (unseen) to 4. Default 1.", VK_SLIDER, 0, 4, 0.05f, 1.0f, " x", 2},
    [F_EMIT] = {"vol_fog_emit", "Fog given off", "How much haze water and lava give off: 0 (none) to 3. Default 1.", VK_SLIDER, 0, 3, 0.05f, 1.0f, " x", 2},
    [F_SRC_WATER] = {"vol_fog_water", "Haze over water", "On: every water square gives off haze. Off: water gives none.", VK_SWITCH, 0, 1, 1, 1},
    [F_SRC_LAVA] = {"vol_fog_lava", "Steam over lava", "On: lava gives off hot steam that rises. Off: none.", VK_SWITCH, 0, 1, 1, 1},
    [F_RISE] = {"vol_fog_chill", "Water chills the air", "How much colder than the room the air just over water is: 0 to 10 degrees. Cold air is heavy, so the fog lies low and creeps out across the floor. Default 3.", VK_SLIDER, 0, 10, 0.5f, 3.0f, " K", 1},
    [F_HEAT] = {"vol_fog_dry", "Fog dries off in warm air", "How fast fog vanishes for each degree the air is warmer than the room: real droplets are gone within a second in air a few degrees warmer. 0 (never) to 1. Default 0.2.", VK_SLIDER, 0, 1, 0.01f, 0.2f, " /s K", 2},
    [F_SETTLE] = {"vol_fog_settle", "Fog droplets settle", "How fast the fog's droplets fall through still air: 0 to 5 cm a second. Real fog droplets of 20 thousandths of a millimetre fall about 1. Default 1.", VK_SLIDER, 0, 5, 0.1f, 1.0f, " cm/s", 1},
    [F_LAVA_T] = {"vol_fog_lava_heat", "Lava heats the air", "How much hotter than the room the air just over lava is: 0 to 400 degrees. Hot air rises and takes the steam up with it. Default 120.", VK_SLIDER, 0, 400, 10, 120, " K", 0},
    [F_SPREAD] = {"vol_fog_spread", "Fog spreads", "How fast fog seeps sideways into clear air by itself: 0 (holds its shape) to 0.5. Default 0.05.", VK_SLIDER, 0, 0.5f, 0.01f, 0.05f, "", 2},
    [F_DECAY] = {"vol_fog_decay", "Fog thins out", "How fast fog fades away: the share lost each second, 0 (never) to 2. Default 0.10.", VK_SLIDER, 0, 2, 0.01f, 0.10f, " /s", 2},
    [F_LAYER] = {"vol_fog_layer", "Fog layer height", "How high above the water the haze forms: 0.1 to 2 map squares. Default 0.45.", VK_SLIDER, 0.1f, 2, 0.05f, 0.45f, " sq", 2},
    [F_R] = {"vol_fog_r", "Fog colour: warm", "The fog's colour, first of three sliders (the warm part of the light it sends back): 0 to 1. Default 0.97.", VK_SLIDER, 0, 1, 0.01f, 0.97f, "", 2},
    [F_G] = {"vol_fog_g", "Fog colour: green", "The fog's colour, second of three sliders: 0 to 1. Default 0.98.", VK_SLIDER, 0, 1, 0.01f, 0.98f, "", 2},
    [F_B] = {"vol_fog_b", "Fog colour: blue", "The fog's colour, third of three sliders: 0 to 1. Default 1.00.", VK_SLIDER, 0, 1, 0.01f, 1.00f, "", 2},
    [F_BRIGHT] = {"vol_fog_bright", "Fog brightness", "How brightly fog shows in light: 0 (it only dims what is behind it) to 4. Default 1.", VK_SLIDER, 0, 4, 0.05f, 1.0f, " x", 2},
    [F_EXT] = {"vol_fog_block", "Fog blocks light", "How much a square's depth of full fog hides what is behind it: 0 (clear) to 8. Default 1.6.", VK_SLIDER, 0, 8, 0.1f, 1.6f, "", 1},
    [F_GLOW] = {"vol_fog_droplet", "Fog droplet size", "The size of the fog's water droplets: 5 to 50 thousandths of a millimetre. It sets how fog scatters light: a bright glow round lights seen through it. Default 15.", VK_SLIDER, 5, 50, 1, 15, " um", 0},
    [F_DETAIL] = {"vol_fog_billow", "Fog billows", "Fine cloud shapes in the fog, smaller than the simulation can hold: wisps at the edges and uneven clumps that drift with the air. 0 (smooth, even haze) to 1. Default 0.7.", VK_SLIDER, 0, 1, 0.05f, 0.7f, "", 2},
    [F_STREAM] = {"vol_fog_wisps", "Mist rises in wisps", "0: water gives off its haze evenly. Higher: it comes off in separate, shifting wisps that climb a little before they sink back, as mist does over a real pond. The same amount of haze overall. 0 to 1. Default 0.6.", VK_SLIDER, 0, 1, 0.05f, 0.6f, "", 2},
    [F_COLD] = {"vol_fog_cold_keep", "Cold air warms up", "How fast the cold air over water warms to the room's temperature: 0.02 (it stays cold and heavy, so the fog runs far across the floor and down steps) to 3 (it warms at once and the fog stays by the water). Default 0.05.", VK_SLIDER, 0.02f, 3, 0.01f, 0.05f, " /s", 2},
    [F_SHAPE] = {"vol_fog_veils", "Fog in veils", "The form of the fog's fine shapes: 0 (soft round clumps) to 1 (thin drifting veils and threads, as mist has). Needs the Fog billows row above 0. Default 0.4.", VK_SLIDER, 0, 1, 0.05f, 0.4f, "", 2},
    [F_STIR] = {"vol_fog_stir", "Moving air raises more mist", "Air that moves across water takes up more haze than still air, as a light breeze does over a pond: your own passing and any draught thicken the mist. 0 (the same in still and moving air) to 1. Default 0.5.", VK_SLIDER, 0, 1, 0.05f, 0.5f, "", 2},
    [F_EDGEDRY] = {"vol_fog_edge_dry", "Fog dries from its edges", "0: thin and thick fog fade at the same rate. Higher: the thin outer parts, mixed with drier air, vanish sooner and the thick middle lasts longer, so a bank of fog keeps a clearer shape. 0 to 1. Default 0.5.", VK_SLIDER, 0, 1, 0.05f, 0.5f, "", 2},
    [F_SOFT] = {"vol_fog_soft", "Fog edge softness", "0: thin fog shows as it is. Higher: thin wisps at the edges are hidden, so the fog has a softer, later start. 0 to 1. Default 0.2.", VK_SLIDER, 0, 1, 0.05f, 0.2f, "", 2},

    [VH_SMOKE] = {"", "#Smoke", "", VK_HEAD},
    [S_ON] = {"vol_smoke", "Smoke", "On: fires give off smoke that rises, gathers under the ceiling and thins out. Off: no smoke; fog is not changed.", VK_SWITCH, 0, 1, 1, 1},
    [S_AMOUNT] = {"vol_smoke_amount", "Smoke thickness", "How thick the smoke looks for the same amount of it in the air: 0 (unseen) to 4. Default 1.", VK_SLIDER, 0, 4, 0.05f, 1.0f, " x", 2},
    [S_EMIT] = {"vol_smoke_emit", "Smoke given off", "How much smoke the fires give off: 0 (none) to 3. Default 1.", VK_SLIDER, 0, 3, 0.05f, 1.0f, " x", 2},
    [S_SRC_CAMP] = {"vol_smoke_campfires", "Campfires smoke", "On: campfires give off smoke. Off: they give none.", VK_SWITCH, 0, 1, 1, 1},
    [S_SRC_TORCH] = {"vol_smoke_torches", "Torches and lanterns smoke", "On: wall torches, lanterns and other small flames give off a thin smoke. Off: none.", VK_SWITCH, 0, 1, 1, 1},
    [S_SRC_BLAST] = {"vol_smoke_blasts", "Blasts smoke", "On: an explosion leaves a puff of hot smoke. Off: none.", VK_SWITCH, 0, 1, 1, 1},
    [S_TEMP_CAMP] = {"vol_smoke_campfire_heat", "Campfire heat", "How much hotter than the room the air over a campfire is: 50 to 800 degrees. Hotter: the smoke climbs faster and thinner. A real campfire (about 30 kW): 400 just over the flames, 140 a metre up. Default 400.", VK_SLIDER, 50, 800, 10, 400, " K", 0},
    [S_TEMP_TORCH] = {"vol_smoke_torch_heat", "Torch heat", "How much hotter than the room the air over a torch or lantern flame is: 20 to 500 degrees. Default 120.", VK_SLIDER, 20, 500, 10, 120, " K", 0},
    [S_RISE] = {"vol_smoke_rise", "Smoke's own weight", "Real smoke weighs next to nothing and goes where the warm air goes: 0. Below 0 it sinks by itself, above 0 it floats up. -1 to 1. Default 0.", VK_SLIDER, -1, 1, 0.01f, 0.0f, "", 2},
    [S_HEAT] = {"vol_smoke_heat", "Heat lifts the air", "How strongly warm air rises and cold air sinks, against the real force (gravity x the difference in temperature): 0 to 3. Default 1 (real).", VK_SLIDER, 0, 3, 0.05f, 1.0f, " x", 2},
    [S_SPREAD] = {"vol_smoke_spread", "Smoke spreads", "How fast smoke seeps sideways into clear air by itself: 0 (holds its shape) to 0.5. Default 0.03.", VK_SLIDER, 0, 0.5f, 0.01f, 0.03f, "", 2},
    [S_DECAY] = {"vol_smoke_decay", "Smoke thins out", "How fast smoke fades away: the share lost each second, 0 (never) to 2. Default 0.20.", VK_SLIDER, 0, 2, 0.01f, 0.20f, " /s", 2},
    [S_COOL] = {"vol_smoke_cool", "Air loses its heat", "How fast warm air cools to the room's temperature (faster still beside rock): 0.02 (stays hot, climbs far) to 3 (cools at once). Default 0.25.", VK_SLIDER, 0.02f, 3, 0.01f, 0.25f, " /s", 2},
    [S_R] = {"vol_smoke_r", "Smoke colour: warm", "The smoke's colour, first of three sliders (the warm part of the light it sends back): 0 to 1. Default 0.80.", VK_SLIDER, 0, 1, 0.01f, 0.80f, "", 2},
    [S_G] = {"vol_smoke_g", "Smoke colour: green", "The smoke's colour, second of three sliders: 0 to 1. Default 0.79.", VK_SLIDER, 0, 1, 0.01f, 0.79f, "", 2},
    [S_B] = {"vol_smoke_b", "Smoke colour: blue", "The smoke's colour, third of three sliders: 0 to 1. Default 0.77.", VK_SLIDER, 0, 1, 0.01f, 0.77f, "", 2},
    [S_BRIGHT] = {"vol_smoke_bright", "Smoke brightness", "How brightly smoke shows in light: 0 (black smoke that only hides) to 4. Default 1.", VK_SLIDER, 0, 4, 0.05f, 1.0f, " x", 2},
    [S_EXT] = {"vol_smoke_block", "Smoke blocks light", "How much a square's depth of full smoke hides what is behind it: 0 (clear) to 8. Default 3.", VK_SLIDER, 0, 8, 0.1f, 3.0f, "", 1},
    [S_GLOW] = {"vol_smoke_glow", "Smoke glow toward lights", "How much brighter smoke is when you look through it toward a light: 0 (the same every way) to 0.9. Wood smoke is about 0.6. Default 0.6.", VK_SLIDER, 0, 0.9f, 0.05f, 0.6f, "", 2},
    [S_DETAIL] = {"vol_smoke_billow", "Smoke billows", "Fine cloud shapes in the smoke, smaller than the simulation can hold: wisps at the edges and uneven clumps that drift with the air. 0 (smooth, even smoke) to 1. Default 0.85.", VK_SLIDER, 0, 1, 0.05f, 0.85f, "", 2},
    [S_PUFF] = {"vol_smoke_puff", "Smoke comes in puffs", "0: every fire gives off smoke evenly, as a smooth ball. Higher: it comes off in uneven puffs, as from a real fire. The same amount of smoke overall. 0 to 1. Default 0.8.", VK_SLIDER, 0, 1, 0.05f, 0.8f, "", 2},
    [S_PUFFRATE] = {"vol_smoke_puff_rate", "Puff rate by fire size", "On: a fire puffs at the rate real flames do, set by its width: a torch about three times a second, a campfire about twice. Off: every fire puffs slowly and unevenly. Needs the Smoke comes in puffs row above 0.", VK_SWITCH, 0, 1, 1, 1},
    [S_SHAPE] = {"vol_smoke_round", "Smoke in round puffs", "The form of the smoke's fine shapes: 0 (soft clumps) to 1 (round puffs with dark creases between them, as rolling smoke has). Needs the Smoke billows row above 0. Default 0.4.", VK_SLIDER, 0, 1, 0.05f, 0.4f, "", 2},
    [S_KICK] = {"vol_smoke_puff_kick", "Puffs kick the air", "On: a fire's heat comes in the same beats as its puffs, so each puff is thrown up as a rolling ring, as a real flame sheds them. Off: steady heat. Needs the Smoke comes in puffs row above 0.", VK_SWITCH, 0, 1, 1, 1},
    [S_SOFT] = {"vol_smoke_soft", "Smoke edge softness", "0: thin smoke shows as it is. Higher: thin wisps at the edges are hidden. 0 to 1. Default 0.1.", VK_SLIDER, 0, 1, 0.05f, 0.1f, "", 2},

    [VH_AIR] = {"", "#The air", "", VK_HEAD},
    [A_ROOM_T] = {"vol_air_room", "Room temperature", "The temperature of the dungeon's still air: 0 to 30 degrees Celsius. Warm air rises faster in a cold room. Default 12.", VK_SLIDER, 0, 30, 1, 12, " C", 0},
    [A_DETSIZE] = {"vol_air_billow_size", "Billow size", "How large the fine cloud shapes of the Fog billows and Smoke billows rows are, in map squares (a square is 2.4 m). Small: fine and wispy. Large: big soft clumps. 0.2 to 2. Default 0.6.", VK_SLIDER, 0.2f, 2, 0.05f, 0.6f, " sq", 2},
    [A_CORE] = {"vol_air_billow_core", "Billows spare the thick middle", "The fine cloud shapes eat into the thin outer parts of fog and smoke and leave the thick middle whole, as in a real cloud. 0 (the whole cloud is broken up) to 1 (only its edges). Default 0.5.", VK_SLIDER, 0, 1, 0.05f, 0.5f, "", 2},
    [A_STRETCH] = {"vol_air_wisp_stretch", "Wisps stretch with the air", "Where the air moves, the fine cloud shapes are drawn out into streaks along its way; still air keeps its round puffs. 0 (round everywhere, a little faster) to 1. Default 0.6.", VK_SLIDER, 0, 1, 0.05f, 0.6f, "", 2},
    [A_SHEAR] = {"vol_air_billow_shear", "Billows where the air shears", "0: the fine cloud shapes are equally strong everywhere. Higher: stronger where layers of air slide past each other (the rolls on top of a moving sheet of fog, the edge of a rising column) and weaker in still air. 0 to 1. Default 0.5.", VK_SLIDER, 0, 1, 0.05f, 0.5f, "", 2},
    [A_SHARP] = {"vol_air_sharp", "Sharp wisps", "On: the moving air keeps fine wisps and edges (a second, correcting pass each step). Off: the simpler method, softer and blurred, a little faster.", VK_SWITCH, 0, 1, 1, 1},
    [A_DRAG] = {"vol_air_drag", "Air comes to rest", "How soon moving air slows down by itself: 0 (it keeps going, as real air nearly does) to 3 (it stops at once). Default 0.1.", VK_SLIDER, 0, 3, 0.05f, 0.1f, " /s", 2},
    [A_SWIRL] = {"vol_air_curl", "Curls kept", "A coarse grid of air loses its small curls and eddies; this puts them back (vorticity confinement): 0 (smooth, lazy smoke) to 1 (lively). Default 0.35.", VK_SLIDER, 0, 1, 0.05f, 0.35f, "", 2},
    [A_DRAFT] = {"vol_air_draught", "Draught", "A steady draught through the whole level that carries fog and smoke along: 0 (still air) to 1 map square a second. Default 0.", VK_SLIDER, 0, 1, 0.01f, 0.0f, "", 2},
    [A_DRAFTDIR] = {"vol_air_draught_dir", "Draught direction", "Which way the draught blows: 0 north, 90 east, 180 south, 270 west. Default 0.", VK_SLIDER, 0, 355, 5, 0, " deg", 0},

    [VH_PUSH] = {"", "#Pushing it", "", VK_HEAD},
    [P_YOU] = {"vol_push_you", "You push it", "On: your body moves the air, so fog and smoke part in front of you and curl in behind. Off: you pass through it.", VK_SWITCH, 0, 1, 1, 1},
    [P_PUSH] = {"vol_push_strength", "Push strength", "How hard a moving body shoves the air aside: 0 to 3. Default 1.", VK_SLIDER, 0, 3, 0.05f, 1.0f, " x", 2},
    [P_WAKE] = {"vol_push_wake", "Wake", "How much of the air a moving body drags along behind it: 0 (none) to 1 (all of it). Default 0.6.", VK_SLIDER, 0, 1, 0.05f, 0.6f, "", 2},
    [P_CREATURES] = {"vol_push_creatures", "Creatures push it", "On: creatures move the air as you do. Off: they pass through fog and smoke.", VK_SWITCH, 0, 1, 1, 1},
    [P_DOORS] = {"vol_push_doors", "Doors push it", "On: a door that swings open or shut gives the air a shove. Off: none.", VK_SWITCH, 0, 1, 1, 1},
    [P_MISSILES] = {"vol_push_missiles", "Missiles push it", "On: arrows, thrown things and spell bolts leave a trail through fog and smoke. Off: none.", VK_SWITCH, 0, 1, 1, 1},
    [P_BLAST] = {"vol_push_blast", "Blast force", "How hard an explosion throws the air outward: 0 (not at all) to 5. Default 1.", VK_SLIDER, 0, 5, 0.1f, 1.0f, " x", 1},

    [VH_LIGHT] = {"", "#Light in it", "", VK_HEAD},
    [L_SHADOW] = {"vol_light_shadows", "Shadows in fog and smoke", "On: walls, pillars and creatures cast shadows into the fog, so light falls through it in shafts. Off: lit evenly, faster.", VK_SWITCH, 0, 1, 1, 1},
    [L_SAMPLES] = {"vol_light_rays", "Light rays a cell", "Shadow rays for each light at each cell of air: 1 to 4. More: smoother shafts from wide flames, slower. Default 1.", VK_SLIDER, 1, 4, 1, 1, "", 0},
    [L_SHAFTS] = {"vol_light_shafts", "Light shafts strength", "How brightly torches, fires and other placed lights show in fog and smoke: 0 (not at all) to 4. Default 1.", VK_SLIDER, 0, 4, 0.05f, 1.0f, " x", 2},
    [L_TORCH] = {"vol_light_torch", "Your light in it", "How brightly the light you carry shows in fog and smoke round you: 0 (not at all) to 2. 1 is real; too high hides the view. Default 1.", VK_SLIDER, 0, 2, 0.05f, 1.0f, " x", 2},
    [L_OTHERS] = {"vol_light_others", "Placed lights light it", "On: torches, fires and glowing things light fog and smoke. Off: only the light you carry does.", VK_SWITCH, 0, 1, 1, 1},
    [L_SELF] = {"vol_light_self", "It shadows itself", "How much thick fog and smoke keep light from the fog and smoke behind them: 0 (off, faster) to 2. Default 1.", VK_SLIDER, 0, 2, 0.05f, 1.0f, " x", 2},
    [L_MULTI] = {"vol_light_multi", "Light scattered many times", "In thick fog light bounces from droplet to droplet and gets deeper in than a straight beam: 0 (straight beams only: hard, dark cores) to 1 (real). Default 1.", VK_SLIDER, 0, 1, 0.05f, 1.0f, "", 2},
    [L_BOUNCE] = {"vol_light_bounce", "Room light in it", "How much of the light bounced off the walls also lights fog and smoke: 0 (only direct light) to 2. Needs Ray tracing and Bounced light. Default 1.", VK_SLIDER, 0, 2, 0.05f, 1.0f, " x", 2},

    [VH_QUALITY] = {"", "#Quality and speed", "", VK_HEAD},
    [Q_CELLS] = {"vol_cells", "Fineness", "Cells of air to a map square: 3, 4, 6 or 8. More: finer wisps, more work for the graphics card. A change starts the air again. Default 6.", VK_CHOICE, 0, 3, 1, 2, "3|4|6|8"},
    [Q_BOX] = {"vol_box", "Reach", "The simulated air is a box round you this many map squares wide: 12 to 32. Beyond it there is no fog or smoke. Default 20.", VK_CHOICE, 0, 4, 1, 2, "12|16|20|24|32"},
    [Q_RATE] = {"vol_rate", "Simulation rate", "How often the air is moved on: 30, 60 or 120 times a second, or once every picture. Default 60.", VK_CHOICE, 0, 3, 1, 1, "30|60|120|Every picture"},
    [Q_PRESSURE] = {"vol_pressure", "Pressure passes", "How exactly air flows round walls and bodies instead of into them: 4 to 80 passes. More: truer swirls, slower. Default 20.", VK_SLIDER, 4, 80, 2, 20, "", 0},
    [Q_VIEW] = {"vol_view_steps", "View steps", "Samples taken along each line of sight through the fog: 8 to 128. More: smoother, slower. Default 48.", VK_SLIDER, 8, 128, 4, 48, "", 0},
    [Q_HALF] = {"vol_half", "Fog drawn at half size", "On: the fog and smoke are worked out for a quarter of the screen's pixels and brought up to full size by distance, with full detail kept at the edges of walls and creatures: much faster, and DLSS smooths it. Off: every pixel worked out, slower.", VK_SWITCH, 0, 1, 1, 1},
    [Q_LREFRESH] = {"vol_light_refresh", "Light refresh", "The share of the air's cells whose light is worked out again each picture: all, half or a quarter. Less is faster, the light follows later.", VK_CHOICE, 0, 2, 1, 1, "All|Half|Quarter"},
    [Q_LKEEP] = {"vol_light_keep", "Light smoothing", "How much of the light from earlier pictures is kept in each cell: 0 (none: grainy, instant) to 0.95 (smooth, slow to follow). Default 0.8.", VK_SLIDER, 0, 0.95f, 0.05f, 0.8f, "", 2},
    [Q_FAR] = {"vol_far", "Beyond the reach", "What stands in for fog and smoke outside the simulated box: the older Particles (Water fog, Campfire smoke rows), or Nothing.", VK_CHOICE, 0, 1, 1, 0, "Particles|Nothing"},
};

static float vol_val[VD_N];
static bool vol_inited;
static void vol_defaults(void) { for (int i = 0; i < VD_N; i++) vol_val[i] = VD[i].dflt; }
static float vol_clamp(int i, float v);
/* UWVIEW_VOL_SET="key=value,key=value" (the checks): rows set for the run, over the settings' own, not kept */
static void vol_env(void)
{
    const char *e = getenv("UWVIEW_VOL_SET");
    char t[1024];
    if (!e || !*e) return;
    snprintf(t, sizeof t, "%s", e);
    for (char *k = strtok(t, ","); k; k = strtok(NULL, ",")) {
        char *eq = strchr(k, '=');
        if (!eq) continue;
        *eq = 0;
        for (int i = 0; i < VD_N; i++)
            if (VD[i].key[0] && !strcmp(k, VD[i].key)) vol_val[i] = vol_clamp(i, (float)atof(eq + 1));
    }
}
static void vol_init(void) { if (!vol_inited) vol_defaults(), vol_inited = true; }
static float vol_clamp(int i, float v)
{
    const VolDial *d = &VD[i];
    if (d->kind == VK_SWITCH) return v != 0 ? 1.0f : 0.0f;
    if (d->step > 0) v = d->lo + floorf((v - d->lo) / d->step + 0.5f) * d->step;
    return v < d->lo ? d->lo : v > d->hi ? d->hi : v;
}
/* a line of settings.txt (settings_load): 1 when it is one of these */
static int vol_setting(const char *key, float v)
{
    vol_init();
    if (strncmp(key, "vol_", 4)) return 0;
    for (int i = 0; i < VD_N; i++)
        if (VD[i].key[0] && !strcmp(key, VD[i].key)) { vol_val[i] = vol_clamp(i, v); return 1; }
    return 0;
}
static void vol_keep(int i)
{
    char t[32];
    if (!VD[i].key[0]) return;
    if (VD[i].kind == VK_SLIDER && VD[i].dec > 0) snprintf(t, sizeof t, "%.*f", VD[i].dec + 1, vol_val[i]);
    else snprintf(t, sizeof t, "%d", (int)lroundf(vol_val[i]));
    settings_save(VD[i].key, t);
}
static int vol_choice_n(int i, int k, char *out, size_t n)   /* a choice's k-th name; the count */
{
    const char *s = VD[i].unit;
    int c = 0;
    if (out && n) out[0] = 0;
    while (s && *s) {
        const char *e = strchr(s, '|');
        size_t l = e ? (size_t)(e - s) : strlen(s);
        if (c == k && out && n) snprintf(out, n, "%.*s", (int)l, s);
        c++;
        if (!e) break;
        s = e + 1;
    }
    return c;
}
static int vol_choice_num(int i)   /* a choice whose names are numbers: the one chosen */
{
    char t[32];
    vol_choice_n(i, (int)vol_val[i], t, sizeof t);
    return atoi(t);
}
static void vol_set(int i, float v)
{
    vol_init();
    v = vol_clamp(i, v);
    if (v == vol_val[i]) return;
    vol_val[i] = v;
    vol_keep(i);
}

/* ---- the tab's presets (the user, 2026-11-17: "give each tab a default button and a few presets ... when they change
 * something it goes to custom"): every row at its default but for the preset's own; 0 is Default ---- */
typedef struct { int dial; float v; } VolPre;
static const VolPre VP_LIGHT[] = {{F_AMOUNT, 0.5f}, {F_EMIT, 0.6f}, {S_AMOUNT, 0.5f}, {S_EMIT, 0.6f}, {-1, 0}};
static const VolPre VP_HEAVY[] = {{F_AMOUNT, 2.0f}, {F_EMIT, 1.8f}, {S_AMOUNT, 2.0f}, {S_EMIT, 1.8f}, {-1, 0}};
static const VolPre VP_FAST[] = {{A_SHARP, 0}, {A_STRETCH, 0}, {A_SHEAR, 0}, {Q_CELLS, 1}, {Q_BOX, 1}, {Q_PRESSURE, 12}, {Q_VIEW, 32}, {Q_LREFRESH, 2}, {L_SELF, 0}, {-1, 0}};
static const VolPre VP_FINE[] = {{Q_HALF, 0}, {Q_CELLS, 3}, {Q_BOX, 3}, {Q_PRESSURE, 40}, {Q_VIEW, 96}, {Q_LREFRESH, 0}, {L_SAMPLES, 2}, {-1, 0}};
static const VolPre VP_NONE[] = {{-1, 0}};
static const struct { const char *name; const VolPre *p; } VOL_PRESETS[] = {
    {"Default", VP_NONE}, {"Light", VP_LIGHT}, {"Heavy", VP_HEAVY}, {"Faster", VP_FAST}, {"Finest", VP_FINE}};
#define VOL_NPRESETS ((int)(sizeof VOL_PRESETS / sizeof VOL_PRESETS[0]))
static float vol_preset_val(int k, int i)
{
    for (const VolPre *q = VOL_PRESETS[k].p; q->dial >= 0; q++) if (q->dial == i) return q->v;
    return VD[i].dflt;
}
/* preset i's name; NULL past the last */
UWV_API const char *uwv_vol_preset_name(int i) { return i >= 0 && i < VOL_NPRESETS ? VOL_PRESETS[i].name : NULL; }
/* set >= 0: that preset taken (every row but the main switch set). Returns the preset the rows now match, -1: none
   (Custom) */
UWV_API int uwv_vol_preset(int set)
{
    vol_init();
    if (set >= 0 && set < VOL_NPRESETS)
        for (int i = 0; i < VD_N; i++)
            if (VD[i].kind != VK_HEAD && i != V_ON) vol_set(i, vol_preset_val(set, i));
    for (int k = 0; k < VOL_NPRESETS; k++) {
        bool same = true;
        for (int i = 0; i < VD_N && same; i++)
            if (VD[i].kind != VK_HEAD && i != V_ON && fabsf(vol_val[i] - vol_clamp(i, vol_preset_val(k, i))) > 1e-4f) same = false;
        if (same) return k;
    }
    return -1;
}

/* ---- the page's rows for the front end (uwfront.c's Fog and smoke page; menu_draw.c's sliders) ---- */
UWV_API int uwv_vol_rows(void) { return VD_N; }
/* row i: its label (a heading's starts with '#'), its hint, its kind (0 a heading, 1 a switch, 2 a slider, 3 a choice,
   4 an action: Enter does it), a slider's place along its track (0..1; -1 for the rest). 0: no such row, or not shown */
UWV_API int uwv_vol_row(int i, const char **label, const char **hint, int *kind, float *frac)
{
    vol_init();
    if (i < 0 || i >= VD_N) return 0;
    if (label) *label = VD[i].label;
    if (hint) *hint = VD[i].hint;
    if (kind) *kind = VD[i].kind;
    if (frac) *frac = VD[i].kind == VK_SLIDER && VD[i].hi > VD[i].lo ? (vol_val[i] - VD[i].lo) / (VD[i].hi - VD[i].lo) : -1.0f;
    return 1;
}
/* row i's value as text; step -1 / +1 first: a slider one step, a switch turned, a choice the next, an action done */
UWV_API int uwv_vol_value(int i, int step, char *value, int n)
{
    vol_init();
    if (i < 0 || i >= VD_N || !value || n <= 0) return 0;
    const VolDial *d = &VD[i];
    value[0] = 0;
    if (step) {
        if (d->kind == VK_SWITCH) vol_set(i, vol_val[i] != 0 ? 0.0f : 1.0f);
        else if (d->kind == VK_SLIDER) vol_set(i, vol_val[i] + (step > 0 ? d->step : -d->step));
        else if (d->kind == VK_CHOICE) { int c = vol_choice_n(i, 0, NULL, 0); vol_set(i, (float)(((int)vol_val[i] + (step > 0 ? 1 : c - 1)) % c)); }
    }
    if (d->kind == VK_SWITCH) snprintf(value, (size_t)n, "%s", vol_val[i] != 0 ? "On" : "Off");
    else if (d->kind == VK_SLIDER) snprintf(value, (size_t)n, "%.*f%s", d->dec, vol_val[i], d->unit ? d->unit : "");
    else if (d->kind == VK_CHOICE) vol_choice_n(i, (int)vol_val[i], value, (size_t)n);
    else if (d->kind == VK_ACTION) snprintf(value, (size_t)n, "%s", "Enter");
    if (i == V_ON && !render_volume_available()) snprintf(value, (size_t)n, "%s", vol_val[i] != 0 ? "On (needs the Vulkan renderer)" : "Off");
    return 1;
}
/* a slider's number typed in (the unit, if typed, ignored): 1 taken, 0 not a number or not a slider */
UWV_API int uwv_vol_set(int i, const char *text)
{
    char *end;
    vol_init();
    if (i < 0 || i >= VD_N || VD[i].kind != VK_SLIDER || !text) return 0;
    while (*text == ' ') text++;
    float v = strtof(text, &end);
    if (end == text) return 0;
    vol_set(i, v);
    return 1;
}
/* a slider dragged to frac (0..1) of its track */
UWV_API int uwv_vol_set_frac(int i, float frac)
{
    vol_init();
    if (i < 0 || i >= VD_N || VD[i].kind != VK_SLIDER) return 0;
    frac = frac < 0 ? 0 : frac > 1 ? 1 : frac;
    vol_set(i, VD[i].lo + frac * (VD[i].hi - VD[i].lo));
    return 1;
}

/* ---- each level's own fog and smoke: DATA\UWRFOG.TXT (the Underworld Editor's Level > Fog and smoke writes it; the
 * original game does not read it). Plain text, a block a level:
 *     level 3
 *     fog 1.5                        the haze water and lava give off, x the menu's
 *     smoke 0.5                      the smoke fires give off, x the menu's
 *     draught 0.3 90                 a draught, map squares a second, its direction (0 north, 90 east), when the menu has none
 *     temperature 8                  the level's still air, degrees Celsius
 *     fogspot 31.5 12.5 0.2 1 0.8    fog given off at x y (squares), that high over the floor (squares), how much, how wide (squares)
 *     smokespot 20.5 40.5 0.3 1 0.4 60   smoke the same way, and its heat (K over the room's air; 0: none)
 * '#' starts a note. ---- */
int uwv_game_file(const char *rel, unsigned char *out, int cap);
typedef struct { float x, z, h, amount, r, heat; int smoke; } VolSpot;
#define VOL_SPOTS 48
static struct {
    int level;           /* the level read for (0: none yet) */
    float fog, smoke, draught, dir, temp;
    bool has_draught, has_temp;
    int nspot;
    VolSpot spot[VOL_SPOTS];
} vol_lv;
static void vol_level_read(int n)
{
    static unsigned char buf[65536];
    memset(&vol_lv, 0, sizeof vol_lv);
    vol_lv.level = n, vol_lv.fog = vol_lv.smoke = 1;
    int len = uwv_game_file("DATA\\UWRFOG.TXT", buf, (int)sizeof buf - 1);
    if (len <= 0) return;
    buf[len] = 0;
    int cur = 0;
    for (char *line = strtok((char *)buf, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char key[24];
        float a[6] = {0};
        int k = sscanf(line, " %23s %f %f %f %f %f %f", key, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
        if (k < 2) continue;
        if (!strcmp(key, "level")) { cur = (int)a[0]; continue; }
        if (cur != n) continue;
        if (!strcmp(key, "fog")) vol_lv.fog = a[0] < 0 ? 0 : a[0] > 4 ? 4 : a[0];
        else if (!strcmp(key, "smoke")) vol_lv.smoke = a[0] < 0 ? 0 : a[0] > 4 ? 4 : a[0];
        else if (!strcmp(key, "draught")) vol_lv.draught = a[0] < 0 ? 0 : a[0] > 2 ? 2 : a[0], vol_lv.dir = k > 2 ? a[1] : 0, vol_lv.has_draught = true;
        else if (!strcmp(key, "temperature")) vol_lv.temp = a[0] < -20 ? -20 : a[0] > 50 ? 50 : a[0], vol_lv.has_temp = true;
        else if ((!strcmp(key, "fogspot") || !strcmp(key, "smokespot")) && k >= 3 && vol_lv.nspot < VOL_SPOTS) {
            VolSpot *s = &vol_lv.spot[vol_lv.nspot++];
            s->smoke = key[0] == 's', s->x = a[0], s->z = a[1];
            s->h = k > 3 ? a[2] : 0.2f, s->amount = k > 4 ? a[3] : 1, s->r = k > 5 ? a[4] : 0.5f, s->heat = k > 6 ? a[5] : 0;
            if (s->r < 0.1f) s->r = 0.1f;
            if (s->r > 3) s->r = 3;
            if (s->amount < 0) s->amount = 0;
            if (s->amount > 10) s->amount = 10;
        }
    }
    fprintf(stderr, "uwview: level %d's own fog and smoke (DATA\\UWRFOG.TXT): fog x%.2f, smoke x%.2f, %d spots%s%s\n", n,
            vol_lv.fog, vol_lv.smoke, vol_lv.nspot, vol_lv.has_draught ? ", a draught" : "", vol_lv.has_temp ? ", its own air's temperature" : "");
}

/* ---- each frame: the dials as volume.hlsl's constants, and what gives off, heats and pushes the air ---- */
static RenderVolume vol_frame_v;
static struct { float pos[3]; float t; float smoke, heat, push, r; } vol_puff[16];   /* blasts under way (a third of a second each) */
static int vol_puff_at;
/* a blast at p (uwview.c: an explosion's burst): a puff of hot smoke and a shove outward */
static void vol_blast(const float p[3], float size)
{
    vol_init();
    if (vol_val[V_ON] == 0) return;
    int k = vol_puff_at++ % 16;
    memcpy(vol_puff[k].pos, p, 12);
    vol_puff[k].t = 0.3f, vol_puff[k].r = 0.45f * size;
    vol_puff[k].smoke = vol_val[S_SRC_BLAST] != 0 ? 6.0f * size : 0, vol_puff[k].heat = 500.0f, vol_puff[k].push = 4.0f * vol_val[P_BLAST];
}
static RvEmit *vol_emit(void) { return vol_frame_v.nemit < RV_EMIT_MAX ? &vol_frame_v.emit[vol_frame_v.nemit++] : NULL; }
static float vol_lin(float c) { return powf(c < 0 ? 0 : c, 2.2f); }
/* is the place inside the box of air (the far stand-ins, uwview.c's particles: only outside it) */
static bool vol_covers(float x, float z, int smoke)
{
    vol_init();
    if (vol_val[V_ON] == 0 || vol_val[smoke ? S_ON : F_ON] == 0 || !render_volume_available()) return false;
    if (vol_val[Q_FAR] != 0) return true;   /* (Nothing beyond: the particles' stand-ins off everywhere) */
    float half = (float)vol_choice_num(Q_BOX) * 0.5f - 1.5f;
    return fabsf(x - cam.pos[0]) < half && fabsf(z - cam.pos[2]) < half;
}
static void vol_frame(float dt)
{
    RenderVolume *v = &vol_frame_v;
    vol_init();
    {
        static bool env_done;
        if (!env_done) vol_env(), env_done = true;   /* (after the settings were read) */
    }
    v->nemit = 0;
    v->on = vol_val[V_ON] != 0 && lv_number != 0;
    {   /* UWVIEW_VOL=0 / 1 (the checks): off / on whatever the settings say */
        const char *e = getenv("UWVIEW_VOL");
        if (e && *e) v->on = atoi(e) != 0 && lv_number != 0;
    }
    if (!v->on) { render_volume(NULL); return; }
    if (vol_lv.level != lv_number) vol_level_read(lv_number);
    const bool own = vol_val[V_LEVEL] != 0;
    const float lfog = own ? vol_lv.fog : 1, lsmoke = own ? vol_lv.smoke : 1;
    static const float RATE[4] = {30, 60, 120, 0};
    v->cells = vol_choice_num(Q_CELLS), v->box = vol_choice_num(Q_BOX);
    v->rate = RATE[(int)vol_val[Q_RATE] & 3];
    v->half = vol_val[Q_HALF] != 0;
    v->pressure = (int)vol_val[Q_PRESSURE], v->view_steps = (int)vol_val[Q_VIEW], v->light_share = 1 << ((int)vol_val[Q_LREFRESH] & 3);
    const bool fog = vol_val[F_ON] != 0, smoke = vol_val[S_ON] != 0;
    float (*p)[4] = v->par;
    float dspeed = vol_val[A_DRAFT], dir = vol_val[A_DRAFTDIR] * 3.14159265f / 180.0f;
    if (own && vol_lv.has_draught && dspeed == 0) dspeed = vol_lv.draught, dir = vol_lv.dir * 3.14159265f / 180.0f;   /* (the menu's own draught wins) */
    p[RVP_AIR][0] = vol_val[A_DRAG], p[RVP_AIR][1] = vol_val[A_SWIRL], p[RVP_AIR][2] = dspeed * sinf(dir), p[RVP_AIR][3] = dspeed * cosf(dir);
    p[RVP_FOGA][0] = vol_val[F_RISE], p[RVP_FOGA][1] = vol_val[F_HEAT], p[RVP_FOGA][2] = vol_val[F_SPREAD], p[RVP_FOGA][3] = vol_val[F_DECAY];
    p[RVP_FOGB][0] = vol_val[F_LAYER], p[RVP_FOGB][1] = fog ? vol_val[F_EMIT] * lfog : 0, p[RVP_FOGB][2] = vol_val[F_SRC_WATER], p[RVP_FOGB][3] = vol_val[F_SRC_LAVA];
    p[RVP_SMKA][0] = vol_val[S_RISE], p[RVP_SMKA][1] = vol_val[S_HEAT], p[RVP_SMKA][2] = vol_val[S_SPREAD], p[RVP_SMKA][3] = vol_val[S_DECAY];
    p[RVP_SMKB][0] = vol_val[S_COOL], p[RVP_SMKB][1] = vol_val[S_PUFFRATE], p[RVP_SMKB][2] = vol_val[F_COLD], p[RVP_SMKB][3] = vol_val[A_CORE];
    p[RVP_FOGL][0] = vol_lin(vol_val[F_R]), p[RVP_FOGL][1] = vol_lin(vol_val[F_G]), p[RVP_FOGL][2] = vol_lin(vol_val[F_B]), p[RVP_FOGL][3] = vol_val[F_BRIGHT];
    p[RVP_FOGM][0] = fog ? vol_val[F_EXT] * vol_val[F_AMOUNT] : 0, p[RVP_FOGM][1] = vol_val[F_DETAIL], p[RVP_FOGM][2] = vol_val[F_SOFT], p[RVP_FOGM][3] = vol_val[S_PUFF];
    p[RVP_SMKL][0] = vol_lin(vol_val[S_R]), p[RVP_SMKL][1] = vol_lin(vol_val[S_G]), p[RVP_SMKL][2] = vol_lin(vol_val[S_B]), p[RVP_SMKL][3] = vol_val[S_BRIGHT];
    p[RVP_SMKM][0] = smoke ? vol_val[S_EXT] * vol_val[S_AMOUNT] : 0, p[RVP_SMKM][1] = vol_val[S_GLOW], p[RVP_SMKM][2] = vol_val[S_SOFT], p[RVP_SMKM][3] = vol_val[S_DETAIL];
    p[RVP_LIGHTP][0] = vol_val[L_SHADOW], p[RVP_LIGHTP][1] = vol_val[L_SAMPLES], p[RVP_LIGHTP][2] = vol_val[L_OTHERS] != 0 ? vol_val[L_SHAFTS] : 0, p[RVP_LIGHTP][3] = vol_val[L_SELF];
    p[RVP_LIGHTQ][0] = vol_val[L_TORCH], p[RVP_LIGHTQ][1] = vol_val[L_OTHERS], p[RVP_LIGHTQ][2] = vol_val[Q_LKEEP], p[RVP_LIGHTQ][3] = vol_val[L_MULTI];
    p[RVP_FOGC][0] = vol_val[F_SETTLE] * 0.01f / 2.4f, p[RVP_FOGC][1] = vol_val[F_LAVA_T], p[RVP_FOGC][2] = vol_val[F_STREAM], p[RVP_FOGC][3] = vol_val[A_STRETCH];
    {   /* the droplets' phase function by their size (Jendersie and d'Eon 2023's fit for 5 to 50 micrometres) */
        float d = vol_val[F_GLOW];
        p[RVP_FOGP][0] = expf(-0.0990567f / (d - 1.67154f));
        p[RVP_FOGP][1] = expf(-2.20679f / (d + 3.91029f) - 0.428934f);
        p[RVP_FOGP][2] = expf(3.62489f - 8.29288f / (d + 5.52825f));
        p[RVP_FOGP][3] = expf(-0.599085f / (d - 0.641583f) - 0.665888f);
    }
    p[RVP_EXTRA][0] = vol_val[A_SHARP], p[RVP_EXTRA][1] = vol_val[L_BOUNCE], p[RVP_EXTRA][2] = 273.15f + (own && vol_lv.has_temp ? vol_lv.temp : vol_val[A_ROOM_T]), p[RVP_EXTRA][3] = vol_val[A_DETSIZE];
    p[RVP_MORE][0] = vol_val[A_SHEAR], p[RVP_MORE][1] = vol_val[F_EDGEDRY], p[RVP_MORE][2] = vol_val[F_STIR], p[RVP_MORE][3] = vol_val[S_KICK];
    p[RVP_SHAPE][0] = vol_val[F_SHAPE], p[RVP_SHAPE][1] = vol_val[S_SHAPE], p[RVP_SHAPE][2] = p[RVP_SHAPE][3] = 0;

    const float half = (float)v->box * 0.5f, push = vol_val[P_PUSH], wake = vol_val[P_WAKE];
    if (dt < 1e-4f) dt = 1e-4f;
    {   /* the player's body: from the floor to the eye, a third of a square thick */
        static float last[3];
        static bool have;
        float vel[3] = {(cam.pos[0] - last[0]) / dt, 0, (cam.pos[2] - last[2]) / dt}, sp = sqrtf(vel[0] * vel[0] + vel[2] * vel[2]);
        if (!have || sp > 12.0f) vel[0] = vel[2] = sp = 0;   /* (a teleport, a level change: no gust) */
        memcpy(last, cam.pos, 12), have = true;
        RvEmit *e = vol_val[P_YOU] != 0 ? vol_emit() : NULL;
        if (e) {
            float fy = level_floor_y(&lv, cam.pos[0], cam.pos[2]);
            memset(e, 0, sizeof *e);
            e->pr[0] = cam.pos[0], e->pr[1] = fy, e->pr[2] = cam.pos[2], e->pr[3] = 0.32f;
            e->vh[0] = vel[0], e->vh[2] = vel[2], e->vh[3] = cam.pos[1] - fy > 0.3f ? cam.pos[1] - fy : 0.3f;
            e->q[3] = 0.6f * push * sp, e->k[0] = wake * (push > 1 ? 1 : push);
        }
    }
    if (lv_block && !lv_wait && (vol_val[P_CREATURES] != 0 || vol_val[P_MISSILES] != 0)) {   /* the mobile objects: creatures, and what flies */
        /* (the game moves them in eighths of a tile: the speed is a move over the time since the move before, and it
           is kept until the next move, a third of a second at most; a move in one frame was a spike or nothing) */
        static float last[256][2], vel[256][2], since[256];
        static u8 seen[256];
        static int last_level;
        if (last_level != lv_number) memset(seen, 0, sizeof seen), memset(vel, 0, sizeof vel), last_level = lv_number;
        for (int i = 2; i < 256; i++) {
            size_t off = 0x4000 + (size_t)i * 27;
            int item = rd16(lv_block + off) & 511;
            u32 pw = rd16(lv_block + off + 2), home = rd16(lv_block + off + 0x16);
            bool crit = item >= 0x40 && item < 0x80;
            if (!item || (crit ? vol_val[P_CREATURES] == 0 : vol_val[P_MISSILES] == 0)) { seen[i] = 0; continue; }
            float x = (float)(home >> 10) + ((pw >> 13) + 0.5f) / 8.0f, z = (float)((home >> 4) & 63) + (((pw >> 10) & 7) + 0.5f) / 8.0f;
            if (!seen[i]) last[i][0] = x, last[i][1] = z, vel[i][0] = vel[i][1] = 0, since[i] = 0, seen[i] = 1;
            since[i] += dt;
            if (x != last[i][0] || z != last[i][1]) {
                float t = since[i] > dt ? since[i] : dt, vx = (x - last[i][0]) / t, vz = (z - last[i][1]) / t;
                if (vx * vx + vz * vz > 14.0f * 14.0f || t > 1.0f) vx = vz = 0;   /* (a jump, or the first move after a rest) */
                vel[i][0] = vx, vel[i][1] = vz, last[i][0] = x, last[i][1] = z, since[i] = 0;
            } else if (since[i] > 0.3f) vel[i][0] = vel[i][1] = 0;
            float vx = vel[i][0], vz = vel[i][1], sp = sqrtf(vx * vx + vz * vz);
            if (sp < 0.05f || fabsf(x - cam.pos[0]) > half || fabsf(z - cam.pos[2]) > half) continue;
            RvEmit *e = vol_emit();
            if (!e) break;
            float fy = level_floor_y(&lv, x, z);
            memset(e, 0, sizeof *e);
            e->pr[0] = x, e->pr[2] = z, e->vh[0] = vx, e->vh[2] = vz;
            if (crit) e->pr[1] = fy, e->pr[3] = 0.3f, e->vh[3] = 0.9f;
            else e->pr[1] = fy + 0.6f, e->pr[3] = 0.16f, e->vh[3] = 0;   /* (what flies: about chest high) */
            e->q[3] = 0.6f * push * sp, e->k[0] = wake * (push > 1 ? 1 : push);
        }
    }
    if (vol_val[P_DOORS] != 0) {   /* a door on the move: a shove along its swing */
        static float last[128];
        static int last_n = -1;
        if (last_n != ndoors) { for (int k = 0; k < ndoors; k++) last[k] = doors[k].open; last_n = ndoors; }
        for (int k = 0; k < ndoors; k++) {
            float d = (doors[k].open - last[k]) / dt;
            last[k] = doors[k].open;
            if (fabsf(d) < 0.05f || fabsf(doors[k].tx + 0.5f - cam.pos[0]) > half || fabsf(doors[k].tz + 0.5f - cam.pos[2]) > half) continue;
            RvEmit *e = vol_emit();
            if (!e) break;
            float x = doors[k].tx + 0.5f, z = doors[k].tz + 0.5f, a = (float)doors[k].heading * 0.7853982f, s = d * 0.5f * push;
            memset(e, 0, sizeof *e);
            e->pr[0] = x, e->pr[1] = level_floor_y(&lv, x, z), e->pr[2] = z, e->pr[3] = 0.5f, e->vh[3] = 0.9f;
            e->vh[0] = cosf(a) * s, e->vh[2] = -sinf(a) * s, e->k[0] = 0.8f;
        }
    }
    if (smoke)   /* the fires: campfires (flames 0.4 m and wider) and the small flames */
        for (int k = 0; k < nlights && k < MAX_LIGHTS; k++) {
            const Light *L = &lights[k];
            if (L->kind != LIGHT_WALL_TORCH || L->kelvin <= 0 || L->flame < 0.011f) continue;
            bool camp = L->flame >= 0.4f;
            if (camp ? vol_val[S_SRC_CAMP] == 0 : vol_val[S_SRC_TORCH] == 0) continue;
            if (fabsf(L->pos[0] - cam.pos[0]) > half || fabsf(L->pos[2] - cam.pos[2]) > half) continue;
            RvEmit *e = vol_emit();
            if (!e) break;
            memset(e, 0, sizeof *e);
            e->pr[0] = L->pos[0], e->pr[1] = L->pos[1] + (camp ? 0.3f : 0.12f), e->pr[2] = L->pos[2], e->pr[3] = camp ? 0.28f : 0.1f;
            e->q[1] = vol_val[S_EMIT] * lsmoke * (camp ? 5.0f : 0.8f), e->q[2] = camp ? vol_val[S_TEMP_CAMP] : vol_val[S_TEMP_TORCH];
        }
    if (own)   /* the level's own fog and smoke spots (DATA\UWRFOG.TXT) */
        for (int k = 0; k < vol_lv.nspot; k++) {
            const VolSpot *s = &vol_lv.spot[k];
            if (s->smoke ? !smoke : !fog) continue;
            if (fabsf(s->x - cam.pos[0]) > half || fabsf(s->z - cam.pos[2]) > half) continue;
            RvEmit *e = vol_emit();
            if (!e) break;
            memset(e, 0, sizeof *e);
            e->pr[0] = s->x, e->pr[1] = level_floor_y(&lv, s->x, s->z) + s->h, e->pr[2] = s->z, e->pr[3] = s->r;
            if (s->smoke) e->q[1] = vol_val[S_EMIT] * s->amount * 2.0f, e->q[2] = s->heat;
            else e->q[0] = vol_val[F_EMIT] * s->amount * 1.5f, e->q[2] = -vol_val[F_RISE];   /* (cold and damp: it lies low, as over water) */
        }
    for (int k = 0; k < 16; k++) {   /* the blasts under way */
        if (vol_puff[k].t <= 0) continue;
        vol_puff[k].t -= dt;
        RvEmit *e = vol_emit();
        if (!e) break;
        memset(e, 0, sizeof *e);
        memcpy(e->pr, vol_puff[k].pos, 12), e->pr[3] = vol_puff[k].r;
        e->q[1] = smoke ? vol_puff[k].smoke * vol_val[S_EMIT] : 0, e->q[2] = vol_puff[k].heat, e->q[3] = vol_puff[k].push;
    }
    render_volume(v);
}

/* the hot air near the eye, for the sound's heat shimmer (2026-11-18, the user: "how would mist and fog and smoke
   affect sound?"; port\win32\uwaudio.c uwaudio_heat): a sound whose way crosses the rising hot air over a flame or
   lava is bent about by its uneven warmth and flickers in loudness, the high notes most. Fog's damp air and smoke's
   particles change nothing a player could hear over a dungeon's few dozen metres, so they are not given. Five
   floats a spot: x, y, z (the hot air's foot), its radius in tiles, and how much hotter than the room it is (K),
   the same flames, lava and blasts the fog and smoke heat the air with (the rows' own temperatures), whether or not
   the fog and smoke are shown. The count given; within 12 tiles of the eye. */
UWV_API int uwv_heat_spots(float *out, int max)
{
    int n = 0;
    vol_init();
    if (!out || max <= 0 || !lv_number) return 0;
#define HEAT_PUT(X, Y, Z, R, K) do { if (n < max) { float *o_ = out + 5 * n++; o_[0] = (X), o_[1] = (Y), o_[2] = (Z), o_[3] = (R), o_[4] = (K); } } while (0)
    for (int k = 0; k < nlights && k < MAX_LIGHTS; k++) {   /* the flames (as vol_frame's smoke: campfires and small flames) */
        const Light *L = &lights[k];
        if (L->kind != LIGHT_WALL_TORCH || L->kelvin <= 0 || L->flame < 0.011f) continue;
        if (fabsf(L->pos[0] - cam.pos[0]) > 12 || fabsf(L->pos[2] - cam.pos[2]) > 12) continue;
        bool camp = L->flame >= 0.4f;
        HEAT_PUT(L->pos[0], L->pos[1] + (camp ? 0.3f : 0.12f), L->pos[2], camp ? 0.28f : 0.1f, camp ? vol_val[S_TEMP_CAMP] : vol_val[S_TEMP_TORCH]);
    }
    {   /* lava (the floor's terrain 2), the squares near the eye */
        int cx = (int)cam.pos[0], cz = (int)cam.pos[2];
        for (int z = cz - 8; z <= cz + 8; z++)
            for (int x = cx - 8; x <= cx + 8; x++)
                if (x >= 0 && x < 64 && z >= 0 && z < 64 && m.grid[(z * 64 + x) * 4 + 2] == 2)
                    HEAT_PUT(x + 0.5f, level_floor_y(&lv, x + 0.5f, z + 0.5f), z + 0.5f, 0.5f, vol_val[F_LAVA_T]);
    }
    for (int k = 0; k < 16; k++)   /* the blasts under way */
        if (vol_puff[k].t > 0) HEAT_PUT(vol_puff[k].pos[0], vol_puff[k].pos[1], vol_puff[k].pos[2], vol_puff[k].r, vol_puff[k].heat);
#undef HEAT_PUT
    return n;
}

/* UWVIEW_VOL_SHOT=FILE.png with UWVIEW_VOL_SHOT_AT=SECONDS (a check's): the renderer's own picture written that long
   after the level came up, the fog and smoke as they then are (called each frame from fx_frame, after vol_frame) */
static void vol_shot(float dt)
{
    static float t;
    static int state, level;
    const char *f = getenv("UWVIEW_VOL_SHOT"), *at = getenv("UWVIEW_VOL_SHOT_AT");
    if (!f || !*f || state == 2) return;
    if (level != lv_number) level = lv_number, t = 0;
    if (!lv_number) return;
    t += dt;
    if (state == 0 && t >= (at && *at ? (float)atof(at) : 20.0f)) { render_want_screenshot(), state = 1; return; }
    if (state == 1) {   /* (the frame asked for has been drawn: its picture) */
        unsigned char *px = malloc((size_t)out_w * out_h * 4);
        if (px && render_readback(px) && png_write(f, px, out_w, out_h, 4)) fprintf(stderr, "uwview: wrote %s\n", f);
        free(px);
        state = 2;
    }
}
