"""One-off: with the inventory open the world can still be used under the mouse pointer (look with a left click or
L, use / take / talk with a right click or E); Esc backs out one step (game_escape) and else opens the options."""
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


patch('src/game.c', [
    ('''    if (G.open) {
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        InvInput ii = {in->click_left, in->click_right, in->look, in->shift, in->mouse_x, in->mouse_y};
        if (inv_update(G.lv, &ii, eye, fwd) || in->click_left) rebuild_sprites();
        G.hover_obj = -1;
    } else {
        pick(eye, fwd);
        float best = REACH;
        if (G.hover_obj >= 0) {
            float p[3];
            world_object_pos(G.lv, G.hover_tx, G.hover_tz, &G.lv->obj[G.hover_obj], p);
            best = sqrtf((p[0] - eye[0]) * (p[0] - eye[0]) + (p[2] - eye[2]) * (p[2] - eye[2]));
        }
        pick_door(eye, fwd, &best);
        if (G.hover_door >= 0) G.hover_obj = -1;
        G.hover_creature = creature_at(eye, fwd, MIN(best + 0.05f, REACH));
        if (G.hover_creature >= 0) G.hover_obj = -1, G.hover_door = -1;
        if (in->use && G.hover_creature >= 0) {
            if (!g_cr[G.hover_creature].hostile) talk(G.hover_creature);
            else game_message("It is in no mood to talk.");
            return G.talking;
        }''', '''    if (G.open) {
        /* the panel; the world under the mouse pointer when it is off the panel (look: a left click or L; use,
         * take, open, talk: a right click or E). A left click in the world with a thing held drops it. */
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        bool was_holding = inv_holding(), over = inv_mouse_over(in->mouse_x, in->mouse_y);
        InvInput ii = {in->click_left, in->click_right, in->look && over, in->shift, in->mouse_x, in->mouse_y};
        if (inv_update(G.lv, &ii, eye, fwd) || in->click_left) rebuild_sprites();
        G.hover_obj = G.hover_door = G.hover_creature = -1, G.tag = false;
        if (!over && in->mouse_dir_ok) {
            hover_world(eye, in->mouse_dir);
            if (!was_holding && (in->click_left || in->look)) look_world(eye, in->mouse_dir);
            if (!was_holding && (in->click_right || in->use)) {
                use_hovered(eye, in->mouse_dir);
                if (G.talking) return true;
            }
        }
    } else {
        hover_world(eye, fwd);
        if (in->use && G.hover_creature >= 0) {
            use_hovered(eye, fwd);
            return G.talking;
        }'''),
    ('''        if (in->look) look_world(eye, fwd);
        if (in->use && G.hover_obj >= 0) {
            /* what cannot be picked up (writing, blood, a fountain) is looked at */
            int it = G.lv->obj[G.hover_obj].item;
            if (it >= 0x170 && it <= 0x17F) {
                use_switch(G.hover_obj);
                rebuild_sprites();
            } else if (obj_info(it)->pick) {
                if (inv_take(G.lv, G.hover_tx, G.hover_tz, G.hover_obj)) rebuild_sprites();
            } else look_world(eye, fwd);
        } else if (in->use && G.hover_door >= 0) use_door(G.hover_door);
    }''', '''        if (in->look) look_world(eye, fwd);
        if (in->use) use_hovered(eye, fwd);
    }'''),
    ('bool game_update(float dt, const float eye[3], float yaw, const float fwd[3], const GameInput *in) {',
     '''/* what the ray from eye along dir meets within reach: an item or decoration, a door, a creature */
static void hover_world(const float eye[3], const float dir[3]) {
    pick(eye, dir);
    float best = REACH;
    if (G.hover_obj >= 0) {
        float p[3];
        world_object_pos(G.lv, G.hover_tx, G.hover_tz, &G.lv->obj[G.hover_obj], p);
        best = sqrtf((p[0] - eye[0]) * (p[0] - eye[0]) + (p[2] - eye[2]) * (p[2] - eye[2]));
    }
    pick_door(eye, dir, &best);
    if (G.hover_door >= 0) G.hover_obj = -1;
    G.hover_creature = creature_at(eye, dir, MIN(best + 0.05f, REACH));
    if (G.hover_creature >= 0) G.hover_obj = -1, G.hover_door = -1;
}

/* E or a right click on what is hovered: talk, flip a switch, take, look at what cannot be taken, open a door */
static void use_hovered(const float eye[3], const float dir[3]) {
    if (G.hover_creature >= 0) {
        if (!g_cr[G.hover_creature].hostile) talk(G.hover_creature);
        else game_message("It is in no mood to talk.");
        return;
    }
    if (G.hover_obj >= 0) {
        int it = G.lv->obj[G.hover_obj].item;
        if (it >= 0x170 && it <= 0x17F) {
            use_switch(G.hover_obj);
            rebuild_sprites();
        } else if (obj_info(it)->pick) {
            if (inv_take(G.lv, G.hover_tx, G.hover_tz, G.hover_obj)) rebuild_sprites();
        } else look_world(eye, dir);
    } else if (G.hover_door >= 0) use_door(G.hover_door);
}

/* Esc: one step back out of what is open (reading, a conversation, a thing on the pointer, the inventory);
 * false when nothing was (then the options open) */
bool game_escape(void) {
    if (G.reading) return G.reading = false, true;
    if (G.talking) return conv_end(), G.talking = false, true;
    if (G.open) {
        if (inv_holding()) return inv_put_back(), true;
        return G.open = false, true;
    }
    return false;
}

bool game_update(float dt, const float eye[3], float yaw, const float fwd[3], const GameInput *in) {'''),
    # the name of what the mouse points at in the world, by the pointer, while the panel is open
    ('''    if (!G.open) {
        /* The crosshair:''', '''    if (G.open && !inv_holding()) {
        char name[96] = "";
        if (G.hover_creature >= 0) creature_name(&g_cr[G.hover_creature], name, sizeof name);
        else if (G.hover_door >= 0) snprintf(name, sizeof name, "%s", G.doors[G.hover_door].open > 0.5f ? "an open door" : "a door");
        else if (G.hover_obj >= 0) item_name(&G.lv->obj[G.hover_obj], name, sizeof name);
        if (name[0]) {
            int w = ui_text_width(FONT_NORMAL, name);
            ui_fill(G.mouse_x + 8, G.mouse_y + 8, w + 8, 12, 8, 10, 24, 220);
            ui_text(FONT_NORMAL, name, G.mouse_x + 12, G.mouse_y + 11, 255, 230, 160);
        }
    }
    if (!G.open) {
        /* The crosshair:'''),
])
patch('src/game.h', [
    ('    bool shift;            /* a shift key is down (shift + right click drops; shift shows both tooltips) */',
     '    bool shift;            /* a shift key is down (shift + right click drops; shift shows both tooltips) */\n'
     '    bool mouse_dir_ok;     /* mouse_dir: the view ray through the mouse pointer (the mouse is free) */\n'
     '    float mouse_dir[3];'),
    ('bool game_travel(int *level, float *x, float *z);',
     'bool game_travel(int *level, float *x, float *z);\n'
     '/* Esc: closes one thing (reading, a conversation, a held thing back in the pack, the inventory); false when\n'
     ' * nothing was open */\nbool game_escape(void);'),
])
patch('src/inventory.h', [
    ('bool inv_holding(void);       /* a thing is on the mouse pointer */',
     'bool inv_holding(void);       /* a thing is on the mouse pointer */\n'
     'void inv_put_back(void);      /* the thing on the pointer back into the pack (else it stays) */'),
])
patch('src/inventory.c', [
    ('bool inv_holding(void) { return I.held != 0; }',
     'bool inv_holding(void) { return I.held != 0; }\n'
     'static bool place_new(int p);\n'
     'void inv_put_back(void) {\n'
     '    if (I.held && place_new(I.held)) I.held = 0;\n'
     '}'),
])
print('ok')
