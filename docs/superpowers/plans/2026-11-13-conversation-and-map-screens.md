# Our own conversation and map screens (plan, 2026-11-13)

Status: A1 done 2026-11-13 (the record and tools\convcheck.py; NOTES.md "Our conversation screen, step A1"), the trade
slots left for A4. A2 done 2026-11-13 (our text, smooth / original font, history and wheel; answer clicks, from A3,
done with it: NOTES.md "step A2"). A3 done 2026-11-13 (typed answers: NOTES.md "step A3").
A4 not needed with the look the user chose: the trade slots, their marks and the items are the original's own art in
the pieces we keep, and every click there (and every drag from the inventory) goes to the original's BARTER.C /
INVPANEL.C at the original's point (uwc_point); ours takes only a left press on an answer line. A trade of our own
would come back only if the conversation screen leaves the original's layout.
Part B done 2026-11-13 (the user: the original's look, sharper; no zoom or pan; the original's font for notes): our
map drawn over the original's from the same data and rules, the original's drawing kept for its rand() calls; the
input stays the original's, so B3 needed nothing (NOTES.md "Our map"). Notes made on ours save the same bytes.

## Goal
Draw the conversation screen and the automap ourselves, at window resolution, with our own input handling. The
original's code stays the back end: the conversation VM (CONVERSE.C, BABLHACK, BARTER.C) and the automap's data
(seen bits, tile types, notes) are untouched. Our screens only read their state and send them the same choices
the original's screens sent. That is allowed by the 2026-11-12 decision: mechanics stay exact, presentation may
be ours (work\REVIEW-2026-11-12-replaceable.md, section 1).

## Today
- port\win32\uwconv.c + src\uwconv_draw.c cut three pieces out of the original's 320x200 screen (the conversation
  area, the inventory panel, the message scroll), scale them by a whole number and lay them over our dimmed 3D view.
  The text, portraits and panels are still the original's pixels and fonts.
- The automap (scrnum 1) is shown the same way: the original's drawing (AUTOMAP.C ShowDungeonMap / DoTile),
  centred and scaled.
- Input on both screens is the CENTRED owner in port\win32\uwinput.c: keys and clicks go to the game as they are.

## Rules (from DECOMP_RULESET.md and the review)
- With our screens turned off, every replay stays byte-identical (run-tests.cmd). The old cut-piece path stays
  as the "off" path.
- With them on, the mechanics' outputs for the same inputs are unchanged: quest bits, conversation variables
  (conversations.txt), what is given or taken, attitudes, the automap's seen bits and notes. A replay recorded with
  them on is checked on those values, not on pixels.
- Our screens never decide anything: what an answer does, what a trade is worth and what the map reveals are the
  original's. A click on our screen calls the routine the original's click called, with the same argument.
- The original's save bytes (the automap and notes blocks in LEV.ARK) are written only by the original's routines.

## Part A: the conversation

### A1. Back-end taps (read only)
Hooks in the port's glue that copy what the original prints into a conversation record, without changing what the
original does:
- npc_say (the NPC's line, `\P`), play_respond and play_say (the player's lines, `\1`), conv_print (narration,
  `\2`). Keep the colour codes and the line breaks as the record's own fields.
- The answer menu: babl_display[1..bablOptCount-1] at converse_event_loop's menu build, and the babl_menu /
  babl_fmenu built-ins (conv_choice_ovr103_A13, conv_fmenu_ovr103_BF2), with which answers have keys.
- conv_pause_ovr103_10DD (the "more" wait) and getInputText_ovr103_1117 (a typed answer: the prompt, then the text).
- Who is talking: the NPC's name and portrait (adr_convpic / move_convpic), the player's name and portrait, from
  strt_converse.
- Barter (BARTER.C): the NPC's offered slots, the player's offered slots, and which are selected.
- The conversation's start and end (Converse, free_converse) so the screen knows when to open and close.

Test: headless `--converse=N --choose=...` writes the record as a transcript. Check it against the text the
original's scroll shows for the same choices (conversation 1, the ghost of Cabirus' dream, and a trader). With the
taps compiled in, every replay must still be byte-identical.

### A2. Our screen (drawing only)
- Layout at window resolution: the two portraits and names at the top, the conversation text as a scrolling pane
  with its whole history (the original keeps only a few lines), the numbered answers below it, the trade area and
  the player's inventory at the side.
- Text in our font at window resolution, our own word wrap. The original's colour codes map to our colours.
- Portraits are the original's art (CHARHEAD.GR / the convpic), scaled with filtering, in the original 16 EGA
  colours or the game palette as now.
- The 3D view stays dimmed behind the screen, as today.
- Turned on by data\settings.txt `our_conversation` (off until A3 is proved, then on by default).

### A3. Our input
- Answers: click a line or press 1-9. This calls conv_play_menu(option) (the original's own path when a key is
  pressed), so the VM sees exactly the original's choice.
- "More": a click or key that releases conv_pause the original's way.
- Typed answers: our text box. On Enter, the text goes into getInputText's buffer as the original's typing would
  have put it (same length limit, same characters allowed).
- Esc / ending the conversation: the original's own exit (do_escape_key and what it leads to).
- uwinput.c gets a new owner for our conversation screen. The game's key codes reach the VM only through the calls
  above.
- Test: the A1 transcripts give the same quest bits and variables with our input as with the original's.

### A4. Trade
- Click an NPC slot or one of the player's items to offer or withdraw it, through the same BARTER.C routine the
  original's click calls. "Show" and "barter" answers stay VM answers.
- Moving items between the player's inventory and the trade area goes through INVDATA / BAGS, as in uwinv.c today.
- Test: a trade replay gives the same items, the same NPC inventory and the same attitude, with ours on and off.

## Part B: the map

### B1. Back-end taps (read only)
- The level's automap block: each tile's seen bit and the tile type that DoTile / DoDoorTile / ShadeSide draw
  from. The same data ManageDungeonMap uses, read from it, never computed by us.
- The notes: what GetTheWords loads and what RedisplayStrings shows (text and position).
- The level being shown (ShowAutoMapLevel / ChangeAutoMapLevel) and the player's position and facing.

### B2. Our drawing
- Vector or tile drawing at window resolution from the seen bits only: walls, floors, water and lava, doors,
  slopes, diagonals. It shows exactly what the original's map shows (no unseen tiles, no creatures), only sharper.
- The player's arrow, the level name, the notes in our font.
- Zoom (mouse wheel) and pan (drag), recentring on the player. The original has neither: presentation only.
- Style: the original's map colours drawn crisply, or a parchment look. To be chosen with the user before B2.

### B3. Our input
- Level up and down: ChangeAutoMapLevel, within the levels the original allows.
- Notes: add, edit and erase them through the original's note routines (the ones ManageDungeonMap calls), so
  SaveTheWords writes them in the original's format and the save bytes match.
- Close: port_map_key, the original's exit.
- Test: notes made on our map, saved and loaded, give LEV.ARK bytes identical to the same notes made on the
  original's map. Pictures of our map and the original's from the same save show the same tiles.

### Out of scope
Marking tiles as seen from our renderer instead of the original's grid walk (review section 2b) is a mechanic and
a separate job. This plan does not change what is seen.

## Order
1. A1 (conversation taps + transcript test)
2. A2 (drawing only, the original still takes the input)
3. A3 (our input; then `our_conversation` on by default)
4. A4 (trade)
5. B1, then the style choice with the user, then B2
6. B3 (map input and notes)
7. NOTES.md, the settings in the Esc menu, tools\verify.cmd (transcript, trade and notes checks)

## Decided (the user, 2026-11-13)
- The conversation first.
- The original's look (its frame art, colours, fonts' style), only sharper; the original's proportions, centred.

## Questions for the user before starting (answered for the conversation above; the map's still open)
- The look of both screens: like the original (its frame art and colours, only sharper) or our own cleaner style?
- Keep the original's fixed 320x200 proportions centred, or use the whole window?
- On the map: zoom and pan wanted? Show the notes in our font or keep the original's handwriting font, scaled?
