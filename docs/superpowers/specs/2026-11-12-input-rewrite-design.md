# Input rewrite: one module for keys, buttons, mouse-look and the display's fallback

Date: 2026-11-12. Approved by the user in conversation ("yes rewrite"; approach A; "the ` shouldn't be used to keep the
mouse look, we should write the mouse to be mouse look"; "do all that you asked, make sure it doesn't change gameplay";
visible test runs on the user's screen allowed for this work).

## Why

The user's reports (2026-11-12): Z slept, dreamed and slept again forever; after a new character the pointer would
randomly not pick up, mouse-look stayed on in a menu, C only sometimes drew the weapon, right-click pickup got harder the
longer they played; the original's interface shows up at random in our display.

Causes found so far: the window's input is decided in many places of port\win32\plat_win32.c (our_keys, menu_key,
pointer_msg, world_click, inv_pointer, the uwc path, key_later, shortcut_pump) and a key's or a button's release is routed
separately from its press, so a screen change in between splits them (the sleep loop: F10 left down and repeated by
keyqueue.c's typematic; a Space left the port's Shift down). Mouse-look is kept in step with our wishes by typing ` into
the game every 500 ms (mouse_look_follow), which races every screen change. The display falls back to the original's
picture whenever view_world cannot match the frame. Fixing each path in turn (b9ba607, e0aad8d and earlier) has not
stopped them.

## Goals

1. Every press has one owner, chosen when it goes down; its release always goes to the same owner and sends the matching
   release, whatever has changed since. Focus lost: every owner gets its releases at once.
2. Mouse-look is the mouse's own mode, decided by our state and applied by the game's own mouse-look code on the game's
   thread. No ` keys are sent by us, ever.
3. In our display the original's interface is never shown in place of our picture by accident: each screen state has
   a fixed presentation, and when the 3D frame cannot be matched our last picture stays. Every fallback is logged.
4. Every control behaves as now (the user: keep them as they are).
5. Gameplay does not change: the game receives the same key and button input for the same actions (proved below), the
   original's routines are untouched except the one mouse-look entry point named here, and every existing replay and
   proof stays byte-identical (run-tests.cmd).

## Non-goals

Rebindable keys; new controls; the original display's (display=original) behaviour beyond what goals 1-2 require; the
plain build (UWPORT_PLAIN) beyond still compiling and behaving as before.

## Design

### Units

- **port\win32\uwinput.c / uwinput.h (new)**: the input module. Pure C with no Win32 calls of its own, so it can be
  unit-tested: plat_win32.c hands it events and a context, and it calls back through a small table of outputs
  (send a key byte to the game, send a pointer event to the game, our menu's handlers, our panel's handlers, the world's
  handlers, the settings screen's, capture on/off). It holds:
  - **the state**, recomputed for every event from: the game's screen state (screen.h's PS_*), our overlays (settings
    screen, Esc menu, inventory panel, the original's options panel), focus, display mode, full_live.
  - **the owner table**: per physical key (set-1 code, 0x200 entries) and per mouse button (3), the owner that took the
    press and the code(s) it sent. Owners in priority order: SETTINGS, MENU, PANEL, WORLD, CENTRED (a conversation, the
    map, typed text shown centred), GAME (the rest: the original's own screens, cutscenes, menus).
  - **the key table**: our display's controls exactly as today's our_keys (W/Up walk and Shift run, S/Down back, A/D,
    PageUp/PageDown, Space jump, E use, L look, Shift+L full bright, Ctrl+L AI log, Q cast, Z sleep, K track, F5/F9 our
    save/load pages, P / Shift+P, ., Shift+O, C fight mode, I panel, R runes, , character, M map, ' ; ] F6 renderer
    toggles, Tab, F11, Esc). Each entry: the key, the action (translate to code X held while held / a tap of code X /
    our function), and the screens it applies on.
- **plat_win32.c**: the window's messages (WM_KEY*, WM_*BUTTON*, WM_INPUT, focus, size) turned into uwinput events
  and nothing else decided there; the old paths (our_keys, our_release, xl_down, menu_key's routing, pointer_msg's
  routing, world_click's routing, mouse_look_follow, look_auto, panel_wait's look handling, the follower's sends,
  shortcut_pump's ` dance) removed. The handlers themselves (the menu's rows, the panel's hit testing, carry/throw,
  the world's look/use/attack, uwc's mapping of a point onto a centred piece) stay where they are and are called by
  uwinput as owners.
- **ui\MOUSE.C (underworld-exhumed)**: one new port-only entry, `port_mouse_look_want(int on)`, called on the game's
  thread (from the game's own per-frame mouse poll, moveMouse, where mouse-look's motion is already applied): it calls
  the existing mouse_look_set(on) when it differs, without the scroll message. The window thread only publishes the
  wanted value (an atomic int); the game thread applies it. port_mouse_look_screen's save/restore and the follower's
  toggling are replaced by it. The ` key binding stays (PLAYER.C) for the original display and for old recordings.

### Mouse-look

Wanted on exactly when: our display, the game's screen is PS_GAME, no overlay of ours is open (Esc menu, panel,
settings), the original's options panel is not up, the window has focus, the mouse-look enhancement is on, and the
game is not replaying a recording. The window's capture (ClipCursor, hidden cursor, raw motion to the game) follows the
game's applied state (port_mouse_look()), read each frame, so the pointer is never held while the game is not looking
and never free while it is. In the original display ` keeps its own toggle as now.

Replays: a recording made before this change carries ` keys: the binding stays, so it replays as recorded. A recording
made after it has no ` keys: the want must be reproduced in a replay. The plan finds how the runtime records input
(O:\appsnotonc\exhume\runtime, read only) and records the want's changes so a replay applies them at the same game
tick; if the runtime offers no way, the change is applied at the game's own key poll from a key byte the runtime
already records (a code no binding of the game uses), decided in the plan with the evidence.

### The display

Per screen state in our display:

| screen | shown |
| --- | --- |
| PS_GAME | our 3D view; when the frame cannot be matched (view_world returns 0), our last frame stays (any length) |
| PS_MAP, PS_CONV, PS_TEXT | the original's piece centred over our dimmed last frame (uwc, as now) |
| PS_CUT_WINDOW, PS_DEATH | the cutscene alone over the dimmed last frame / over black (as now) |
| PS_CUT_FULL, PS_MENU, PS_CHARGEN, PS_ENDING | the original's picture whole (by design: these are its art) |

The original's 3D-screen interface (panels, the view's frame) is never shown in our display. Each time PS_GAME keeps
the last frame because the frame did not match, the log gets one line "display: kept our last frame (reason) at N ms"
(at most one a second), so cases not yet understood show up in the user's home\logs\uw-new.log.

### Errors and edges

- A release with no recorded owner (pressed before the window had focus, or before start): nothing sent.
- Focus lost: every owned press released through its owner, mouse-look wanted off, capture off; focus back: the want
  recomputed.
- The game paused under our menu: keys owned by MENU; held game keys were released when MENU took over (the screen
  change rule: an owner change for a held key releases it at once).
- Key repeats: only the walk keys repeat to the game (as now); other repeats are dropped.

## Proof that gameplay does not change

1. run-tests.cmd passes (every replay and proof byte-identical).
2. **Key trace comparison**: a test build flag logs every byte and pointer event the window thread gives the game.
   For a script that uses every control once in play (tools\inputcheck.py trace), the trace of the new build equals
   the old build's (b9ba607), except the ` keys the old follower sent.
3. The game's mouse-look code is not changed except the new entry point; mouse_look_step (the turning math) untouched.

## Tests

- tests\uwinput_test.c: the module alone with a fake context: each scenario below as a sequence of events and screen
  changes, asserting the outputs (bytes sent, owners called, want).
- tools\inputcheck.py (hidden runs, the test home work\in2-4 and a new character): Z held into the dream (one dream);
  Space held into the Esc menu (no Shift left down); W held into a conversation and out (no walking after); C into the
  map; keys held through death and a new character; a click on the panel released over the world; the display's
  fallback lines counted on a level change and after a conversation.
- Visible runs (allowed by the user for this work, never while uw-new.exe is running): the real capture: play
  (captured, crosshair), Esc menu (pointer free, view still), panel, conversation, map, death, new character, Alt-Tab
  away and back; each asserted from the log (capture on/off, want, screen) and a window shot.

## Done when

All the above pass; the user's scenarios (sleep loop, pickup, C, menu, the original's interface) are each a test; the
old routing code is gone from plat_win32.c; NOTES.md says what changed.
