# ROADMAP: everything left, subsystem by subsystem

Made by tools/roadmap.py from re/crosswalk.tsv (underworld-exhumed's matched C on our addresses) and ROUTINES.md. A file is finished when no routine in it is todo or unproved (CLAUDE.md: finish before adding). Statuses: proved, unproved (claimed, no verifier), replaced, notneeded, todo.

| # | Source file (underworld-exhumed uw1/src) | Routines | Bytes | proved | unproved | replaced / notneeded | todo |
|---|---|---|---|---|---|---|---|
| 1 | game/SKILLCHK.C | 3 | 520 | 2 | 0 | 1 | 0 |
| 2 | motion/COLLIDE.C | 14 | 5303 | 14 | 0 | 0 | 0 |
| 3 | motion/MOTION.C | 19 | 6152 | 19 | 0 | 0 | 0 |
| 4 | motion/OBJPHYS.C | 10 | 5000 | 10 | 0 | 0 | 0 |
| 5 | motion/PHYSICS.C | 14 | 3837 | 14 | 0 | 0 | 0 |
| 6 | motion/PLAYMOVE.C | 13 | 3753 | 12 | 0 | 0 | 1 |
| 7 | combat/COMBAT.C | 20 | 6051 | 20 | 0 | 0 | 0 |
| 8 | critter/AI.C | 34 | 14634 | 34 | 0 | 0 | 0 |
| 9 | critter/CREATURE.C | 4 | 825 | 2 | 0 | 0 | 2 |
| 10 | critter/CRITTIME.C | 15 | 3809 | 15 | 0 | 0 | 0 |
| 11 | critter/CRPAGES.C | 6 | 1051 | 6 | 0 | 0 | 0 |
| 12 | critter/PATHFIND.C | 25 | 12242 | 25 | 0 | 0 | 0 |
| 13 | obj/ANIMOBJ.C | 2 | 44 | 2 | 0 | 0 | 0 |
| 14 | obj/COMBINE.C | 5 | 806 | 5 | 0 | 0 | 0 |
| 15 | obj/HACK.C | 2 | 149 | 2 | 0 | 0 | 0 |
| 16 | obj/LOOK.C | 10 | 3432 | 10 | 0 | 0 | 0 |
| 17 | obj/MISC.C | 2 | 138 | 2 | 0 | 0 | 0 |
| 18 | obj/OBJCLASS.C | 3 | 299 | 3 | 0 | 0 | 0 |
| 19 | obj/OBJECTS.C | 29 | 4529 | 29 | 0 | 0 | 0 |
| 20 | obj/OVR139.C | 1 | 7 | 1 | 0 | 0 | 0 |
| 21 | obj/OVR144.C | 1 | 7 | 1 | 0 | 0 | 0 |
| 22 | obj/OVR146.C | 1 | 7 | 1 | 0 | 0 | 0 |
| 23 | obj/TREASURE.C | 7 | 1437 | 7 | 0 | 0 | 0 |
| 24 | obj/USEITEMS.C | 39 | 10224 | 39 | 0 | 0 | 0 |
| 25 | inv/BAGS.C | 13 | 4239 | 0 | 0 | 0 | 13 |
| 26 | inv/INVDATA.C | 20 | 3734 | 0 | 1 | 0 | 19 |
| 27 | inv/INVPANEL.C | 20 | 6472 | 0 | 1 | 0 | 19 |
| 28 | inv/INVSAVE.C | 11 | 1840 | 0 | 0 | 0 | 11 |
| 29 | event/TRIGGER.C | 16 | 5637 | 14 | 0 | 0 | 2 |
| 30 | event/TRIGSAVE.C | 1 | 23 | 1 | 0 | 0 | 0 |
| 31 | event/WORLDEV.C | 21 | 6697 | 20 | 0 | 0 | 1 |
| 32 | combat/RUNES.C | 11 | 1503 | 11 | 0 | 0 | 0 |
| 33 | combat/SPELLS.C | 32 | 6367 | 32 | 0 | 0 | 0 |
| 34 | game/CHARGEN.C | 15 | 6069 | 0 | 6 | 0 | 9 |
| 35 | game/GAMEWRAP.C | 13 | 2539 | 0 | 0 | 0 | 13 |
| 36 | game/MAINLOOP.C | 3 | 180 | 0 | 0 | 0 | 3 |
| 37 | game/OVR126.C | 1 | 24 | 0 | 0 | 0 | 1 |
| 38 | game/PLAYDATA.C | 12 | 2848 | 0 | 5 | 0 | 7 |
| 39 | game/PLAYER.C | 16 | 3821 | 0 | 0 | 0 | 16 |
| 40 | game/PLAYTIME.C | 5 | 1671 | 0 | 4 | 0 | 1 |
| 41 | game/SKILLS.C | 24 | 6913 | 0 | 9 | 0 | 15 |
| 42 | game/UWEDIT.C | 18 | 1907 | 0 | 1 | 0 | 17 |
| 43 | conv/BABL.C | 53 | 9042 | 0 | 0 | 0 | 53 |
| 44 | conv/BABLHACK.C | 15 | 2950 | 0 | 0 | 0 | 15 |
| 45 | conv/BARTER.C | 33 | 7758 | 0 | 1 | 0 | 32 |
| 46 | conv/CONVERSE.C | 31 | 6533 | 0 | 3 | 0 | 28 |
| 47 | conv/CONVVARS.C | 2 | 1667 | 0 | 0 | 0 | 2 |
| 48 | map/LIGHTING.C | 3 | 553 | 0 | 1 | 0 | 2 |
| 49 | map/MAP.C | 4 | 606 | 0 | 0 | 0 | 4 |
| 50 | map/MAPADDR.C | 2 | 325 | 0 | 0 | 0 | 2 |
| 51 | map/TEXTMAPS.C | 7 | 1671 | 0 | 0 | 0 | 7 |
| 52 | sound/SOUND.C | 43 | 7026 | 11 | 9 | 0 | 23 |
| 53 | ui/AUTOMAP.C | 19 | 4422 | 0 | 0 | 0 | 19 |
| 54 | ui/GAMESTRN.C | 19 | 2399 | 0 | 0 | 0 | 19 |
| 55 | ui/INTERACT.C | 34 | 5473 | 0 | 5 | 0 | 29 |
| 56 | ui/MAINMENU.C | 9 | 2716 | 0 | 0 | 0 | 9 |
| 57 | ui/MOUSE.C | 40 | 4029 | 0 | 0 | 0 | 40 |
| 58 | ui/OPTIONS.C | 21 | 2094 | 0 | 0 | 0 | 21 |
| 59 | ui/OVR096.C | 7 | 38 | 0 | 0 | 0 | 7 |
| 60 | ui/OVR137.C | 1 | 5 | 0 | 0 | 0 | 1 |
| 61 | ui/PANELS.C | 33 | 9710 | 0 | 3 | 0 | 30 |
| 62 | ui/SCROLLIO.C | 21 | 3910 | 0 | 0 | 0 | 21 |
| 63 | ui/STATS.C | 9 | 1292 | 0 | 0 | 0 | 9 |
| 64 | gfx/CUTS.C | 33 | 5714 | 0 | 0 | 0 | 33 |
| 65 | gfx/GRFX.C | 17 | 1362 | 0 | 0 | 0 | 17 |
| 66 | gfx/LOADGR.C | 24 | 2603 | 0 | 0 | 0 | 24 |
| 67 | gfx/MODEX.ASM | 14 | 941 | 0 | 0 | 14 | 0 |
| 68 | gfx/SCRSHOT.C | 6 | 1191 | 0 | 0 | 0 | 6 |
| 69 | gfx/SHOWPIC.C | 1 | 250 | 0 | 0 | 0 | 1 |
| 70 | gfx/VALLOC.ASM | 6 | 301 | 0 | 0 | 6 | 0 |
| 71 | gfx/VSTATS.ASM | 1 | 185 | 0 | 0 | 1 | 0 |
| 72 | 3d/DRAWOBJ.C | 3 | 4692 | 0 | 0 | 0 | 3 |
| 73 | 3d/GAMESORT.C | 10 | 2455 | 0 | 0 | 0 | 10 |
| 74 | 3d/GRIDDB.C | 11 | 5104 | 0 | 0 | 0 | 11 |
| 75 | 3d/SETPNT.ASM | 1 | 53 | 0 | 0 | 0 | 1 |
| 76 | sys/ARC.C | 6 | 2122 | 0 | 0 | 0 | 6 |
| 77 | sys/DEBUG.C | 3 | 37 | 0 | 0 | 0 | 3 |
| 78 | sys/ERROR.C | 4 | 404 | 0 | 0 | 0 | 4 |
| 79 | sys/MISCUTIL.C | 20 | 1734 | 0 | 0 | 0 | 20 |
| 80 | sys/OVR090.C | 7 | 44 | 0 | 0 | 0 | 7 |
| 81 | sys/OVR127.C | 1 | 7 | 0 | 0 | 0 | 1 |
| 82 | sys/OVR152.C | 3 | 24 | 0 | 0 | 0 | 3 |
| 83 | sys/UTIL.C | 4 | 355 | 1 | 1 | 0 | 2 |
| 84 |  | 172 | 14091 | 1 | 1 | 163 | 7 |

## game/SKILLCHK.C

- [x] 03419c skill_check (60 bytes) proved
- [x] 0341d8 player_get_exp (369 bytes) proved
- [ ] 034349 panel_check_hpmp (91 bytes) replaced

## motion/COLLIDE.C

- [x] 028494 GetHgt (417 bytes) proved
- [x] 028635 GetSlopeHgt (104 bytes) proved
- [x] 02869d SolvePnt (183 bytes) proved
- [x] 028754 SolveCenter (181 bytes) proved
- [x] 028809 TerrainCheck (1149 bytes) proved
- [x] 028c86 ComputeHeading (618 bytes) proved
- [x] 028ef0 obj_coll_check (406 bytes) proved
- [x] 029086 ObjectCheck (612 bytes) proved
- [x] 0292ea oCswap (101 bytes) proved
- [x] 02934f process_objlist (329 bytes) proved
- [x] 029498 can_place (547 bytes) proved
- [x] 0296bb put_at (186 bytes) proved
- [x] 029775 near_mob_put_at (90 bytes) proved
- [x] 0297cf drop_around_place (380 bytes) proved

## motion/MOTION.C

- [x] 02c59b do_physics (134 bytes) proved
- [x] 02c621 space_to_pos (123 bytes) proved
- [x] 02c69c set_targz (772 bytes) proved
- [x] 02c9a0 space_to_motion (888 bytes) proved
- [x] 02cd18 recalc_vecs (53 bytes) proved
- [x] 02cd4d stop_me (65 bytes) proved
- [x] 02cd8e back_to_space (166 bytes) proved
- [x] 02ce34 flat_move (409 bytes) proved
- [x] 02cfd7 rehead (408 bytes) proved
- [x] 02d16f do_2dbounce (106 bytes) proved
- [x] 02d1d9 bounce_that_guy (74 bytes) proved
- [x] 02d223 do_zbounce (844 bytes) proved
- [x] 02d56f full_move (384 bytes) proved
- [x] 02d6ef grid_move (147 bytes) proved
- [x] 02d782 set_resterr (103 bytes) proved
- [x] 02d7e9 get_pcoll (999 bytes) proved
- [x] 02dbd0 check_positions (204 bytes) proved
- [x] 02dc9c IsaDoor (233 bytes) proved
- [x] 02dd85 CollObject (40 bytes) proved

## motion/OBJPHYS.C

- [x] 02b213 bounce_obj (170 bytes) proved
- [x] 02b2bd missile_newhit (198 bytes) proved
- [x] 02b383 do_objhit (601 bytes) proved
- [x] 02b5dc get_phys_data (590 bytes) proved
- [x] 02b82a set_phys_data (1029 bytes) proved
- [x] 02bc2f static_to_mob (283 bytes) proved
- [x] 02bd4a mob_init (309 bytes) proved
- [x] 02be7f mob_to_static (924 bytes) proved
- [x] 02c21b update_hack_vecs (66 bytes) proved
- [x] 02c25d obj_deal (830 bytes) proved

## motion/PHYSICS.C

- [x] 01c5db water_set (50 bytes) proved
- [x] 01c60d parse_player_terr (275 bytes) proved
- [x] 01c720 EtherealVoidSpecialEffects_seg008_150 (198 bytes) proved
- [x] 01c7e6 simple_fizix (973 bytes) proved
- [x] 01cbb3 set_player_phys_params (238 bytes) proved
- [x] 01cca1 player_setup (572 bytes) proved
- [x] 01cedd phys_affect_player (678 bytes) proved
- [x] 01d1bc do_player_input (367 bytes) proved
- [x] 01d32b phys_bounce_up (42 bytes) proved
- [x] 01d355 fizix_update (25 bytes) proved
- [x] 01d36e BouncePlayer_seg008_D9E (56 bytes) proved
- [x] 01d3a6 DoTrapScreenShake_seg008_DD6 (17 bytes) proved
- [x] 01d3b7 QuakeTrap_seg008_DE7 (37 bytes) proved
- [x] 01d3dc newFPS (309 bytes) proved

## motion/PLAYMOVE.C

- [x] 030bc0 player_mous_move (78 bytes) proved
- [x] 030c0e parse_playin (396 bytes) proved
- [x] 030d9a do_player_keyboard (346 bytes) proved
- [ ] 030ef4 player_simple_move (210 bytes) 
- [x] 030fc6 check_physics (274 bytes) proved
- [x] 0310d8 move_physics (172 bytes) proved
- [x] 031184 finish_player (64 bytes) proved
- [x] 0311c4 move_player (271 bytes) proved
- [x] 0312d3 make_noise (389 bytes) proved
- [x] 031458 set_sound (165 bytes) proved
- [x] 0314fd parse_effect (557 bytes) proved
- [x] 03172a set_effect (47 bytes) proved
- [x] 031759 get_eye (784 bytes) proved

## combat/COMBAT.C

- [x] 02441a pickloc (121 bytes) proved
- [x] 024493 set_hitobj (591 bytes) proved
- [x] 0246e2 find_wall_coll (404 bytes) proved
- [x] 024876 resolve_attack (595 bytes) proved
- [x] 024ac9 frp_check (492 bytes) proved
- [x] 024cb5 do_damage (900 bytes) proved
- [x] 025039 do_miss (287 bytes) proved
- [x] 025158 compute_hitangle (84 bytes) proved
- [x] 0251ac do_attack (220 bytes) proved
- [x] 025288 check_ammo (158 bytes) proved
- [x] 025326 GetPlayerWeapon (224 bytes) proved
- [x] 025406 DoPlayerWeapon (288 bytes) proved
- [x] 025526 missile_finish (60 bytes) proved
- [x] 025562 fin_attack (49 bytes) proved
- [x] 025593 clear_fight_state (55 bytes) proved
- [x] 0255ca player_attack (773 bytes) proved
- [x] 0258cf missile_thwack (287 bytes) proved
- [x] 0259ee critter_attack (279 bytes) proved
- [x] 025b05 seg022_230E_16F5 (48 bytes) proved
- [x] 025b35 player_killed_a (136 bytes) proved

## critter/AI.C

- [x] 018cb1 crit_drunkwalk (995 bytes) proved
- [x] 019094 seg007_1798_3E4 (507 bytes) proved
- [x] 01928f crit_mill (209 bytes) proved
- [x] 019360 crit_guard (481 bytes) proved
- [x] 019541 crit_offense (622 bytes) proved
- [x] 0197af crit_attack (862 bytes) proved
- [x] 019b0d crit_offense_find_target (405 bytes) proved
- [x] 019ca2 maybe_cast_defensive_spell (173 bytes) proved
- [x] 019d4f crit_magik_attack (348 bytes) proved
- [x] 019eab crit_missile_attack (203 bytes) proved
- [x] 019f76 crit_defense (308 bytes) proved
- [x] 01a0aa crit_flee (1059 bytes) proved
- [x] 01a4cd crit_avoid_player (282 bytes) proved
- [x] 01a5e7 crit_talk (447 bytes) proved
- [x] 01a7a6 check_out_player (254 bytes) proved
- [x] 01a8a4 crit_hover (247 bytes) proved
- [x] 01a99b target_found (559 bytes) proved
- [x] 01abca look_for_target (194 bytes) proved
- [x] 01ac8c constrain_movement (327 bytes) proved
- [x] 01add3 turn_real_fine (414 bytes) proved
- [x] 01af71 compute_trz_or_try_rather (149 bytes) proved
- [x] 01b006 set_critter_vars (306 bytes) proved
- [x] 01b138 critter_ai (1986 bytes) proved
- [x] 01b8fa critter_mv (1569 bytes) proved
- [x] 01bf1b set_up_target (280 bytes) proved
- [x] 01c033 should_i_flee (135 bytes) proved
- [x] 01c0ba acceptable_danger (132 bytes) proved
- [x] 01c13e critter_set_goal (111 bytes) proved
- [x] 01c1ad critter_discard_goal (122 bytes) proved
- [x] 01c227 go_into_dying_sequence (84 bytes) proved
- [x] 01c27b crit_die (87 bytes) proved
- [x] 01c2d2 damage_critter (515 bytes) proved
- [x] 01c4d5 timetodo (67 bytes) proved
- [x] 01c518 move_mobile (195 bytes) proved

## critter/CREATURE.C

- [ ] 0700b0 creature_init (23 bytes) 
- [ ] 0700c7 creature_save_ovr101_17 (23 bytes) 
- [x] 0700de creature_class_data (48 bytes) proved
- [x] 07010e creature_obj_init (731 bytes) proved

## critter/CRITTIME.C

- [x] 070af0 change_critter_goal (62 bytes) proved
- [x] 070b2e yearly_checkup (125 bytes) proved
- [x] 070bab set_gridx_and_y_based_on_tile_type (90 bytes) proved
- [x] 070c05 up_crit (760 bytes) proved
- [x] 070efd up_mob (297 bytes) proved
- [x] 071026 update_all_critters_whilst_player_snoozes (328 bytes) proved
- [x] 07116e check_for_hostile_creature (115 bytes) proved
- [x] 0711e1 hostile_creatures_near (43 bytes) proved
- [x] 07120c wander_that_monster (1124 bytes) proved
- [x] 071670 wandering_monster_check (43 bytes) proved
- [x] 07169b set_creatures_from_saved_game (52 bytes) proved
- [x] 0716cf set_creatures_to_saved_game (68 bytes) proved
- [x] 071713 init_level_creature_stuff (20 bytes) proved
- [x] 071727 critter_get_told (533 bytes) proved
- [x] 07193c player_grabbed (149 bytes) proved

## critter/CRPAGES.C

- [x] 075b90 preload_cr (462 bytes) proved
- [x] 075d5e NightCleanCritPages (92 bytes) proved
- [x] 075dba punt_idle_crpage (120 bytes) proved
- [x] 075e32 ovr113_2A2 (185 bytes) proved
- [x] 075eeb swap_ws_out (114 bytes) proved
- [x] 075f5d unswap_ws (78 bytes) proved

## critter/PATHFIND.C

- [x] 015aa5 build_corpse (351 bytes) proved
- [x] 015c04 move_me_joe (298 bytes) proved
- [x] 015d2e init_ai (217 bytes) proved
- [x] 015e07 get_terrain (202 bytes) proved
- [x] 01609e crit_hndlr_fly (96 bytes) proved
- [x] 01616b do_crit_phys (43 bytes) proved
- [x] 016196 hyp_move (2400 bytes) proved
- [x] 016af6 hyp_see (613 bytes) proved
- [x] 016d5b flood_path (1416 bytes) proved
- [x] 0172e3 make_path_from_flood_data (245 bytes) proved
- [x] 0173d8 beeline (665 bytes) proved
- [x] 017671 line_of_sight (1136 bytes) proved
- [x] 017ae1 add_to_beeline_path (400 bytes) proved
- [x] 017c71 find_free_path (57 bytes) proved
- [x] 017caa clear_paths (58 bytes) proved
- [x] 017ce4 store_path (391 bytes) proved
- [x] 017e6b set_next_square_on_path (221 bytes) proved
- [x] 017f48 close_to_square (92 bytes) proved
- [x] 017fa4 move_along_path (373 bytes) proved
- [x] 018119 do_that_jump_kinda_thing (489 bytes) proved
- [x] 018302 deltatotheta (136 bytes) proved
- [x] 01838a set_loc (183 bytes) proved
- [x] 018441 crit_head_for_loc (1728 bytes) proved
- [x] 018b01 adjust_height (194 bytes) proved
- [x] 018bc3 try_to_open_door (238 bytes) proved

## obj/ANIMOBJ.C

- [x] 065710 animobj_load (23 bytes) proved
- [x] 065727 animobj_class_data (21 bytes) proved

## obj/COMBINE.C

- [x] 06e140 init_combinables (22 bytes) proved
- [x] 06e156 ObjsBeCombinable (324 bytes) proved
- [x] 06e29a CombineObjs (31 bytes) proved
- [x] 06e2b9 RemoveAfterCombine (57 bytes) proved
- [x] 06e2f2 make_stew (372 bytes) proved

## obj/HACK.C

- [x] 077070 hack_init (58 bytes) proved
- [x] 0770aa hack_class_data (91 bytes) proved

## obj/LOOK.C

- [x] 07ba10 LookAt (842 bytes) proved
- [x] 07bd5a do_mods (98 bytes) proved
- [x] 07bdbc do_of (474 bytes) proved
- [x] 07bf96 BookLook (315 bytes) proved
- [x] 07c0d1 RectLook (602 bytes) proved
- [x] 07c32b BonesLook (219 bytes) proved
- [x] 07c406 KeyLook (66 bytes) proved
- [x] 07c448 CritterLook (473 bytes) proved
- [x] 07c621 SpecialLook (178 bytes) proved
- [x] 07c6d3 talisman_desc (165 bytes) proved

## obj/MISC.C

- [x] 07cac0 misc_init (58 bytes) proved
- [x] 07cafa misc_class_data (80 bytes) proved

## obj/OBJCLASS.C

- [x] 07cb90 init_objects (238 bytes) proved
- [x] 07cc7e Return0_ovr129_EE (7 bytes) proved
- [x] 07cc85 get_class_data (54 bytes) proved

## obj/OBJECTS.C

- [x] 02994b Map_ObjFix (268 bytes) proved
- [x] 029a57 Obj_Check (143 bytes) proved
- [x] 029ae6 chkTenacious (128 bytes) proved
- [x] 029b66 Obj_Elem_Fate (195 bytes) proved
- [x] 029c29 Obj_Fate (64 bytes) proved
- [x] 029c69 Obj_GarbageCollect (285 bytes) proved
- [x] 029d86 Obj_Alloc (151 bytes) proved
- [x] 029e1d Obj_Free (132 bytes) proved
- [x] 029ea1 Obj_Add (110 bytes) proved
- [x] 029f0f Obj_AddEnd (138 bytes) proved
- [x] 029f99 Obj_Rem (132 bytes) proved
- [x] 02a01d Obj_Punt (149 bytes) proved
- [x] 02a0b2 Obj_FreeChain (191 bytes) proved
- [x] 02a171 Obj_FreeLinkChain (92 bytes) proved
- [x] 02a1cd Obj_PtrTMem (105 bytes) proved
- [x] 02a236 Obj_MemTPtr (74 bytes) proved
- [x] 02a280 Obj_IntTMem (65 bytes) proved
- [x] 02a2c1 Obj_Find (209 bytes) proved
- [x] 02a392 IsMobElem (30 bytes) proved
- [x] 02a3b0 active_critter (19 bytes) proved
- [x] 02a3c3 free_critter (76 bytes) proved
- [x] 02a476 Obj_InList (256 bytes) proved
- [x] 02a576 HasOrIsObj (105 bytes) proved
- [x] 02a5df Obj_FindInMap (165 bytes) proved
- [x] 02a684 flog_list (233 bytes) proved
- [x] 02a76d count_list (204 bytes) proved
- [x] 02a839 seg027_2861_EF9 (53 bytes) proved
- [x] 02a897 Obj_ListOkay (397 bytes) proved
- [x] 02aa24 seg027_2861_10E4 (360 bytes) proved

## obj/OVR139.C

- [x] 080400 rect_class_data (7 bytes) proved

## obj/OVR144.C

- [x] 082f30 spec_class_data (7 bytes) proved

## obj/OVR146.C

- [x] 0834b0 stuff_class_data (7 bytes) proved

## obj/TREASURE.C

- [x] 083500 drop_link_chain (302 bytes) proved
- [x] 08362e drop_some_objects (44 bytes) proved
- [x] 08365a generate_treasure (353 bytes) proved
- [x] 0837bb generate_food (102 bytes) proved
- [x] 083821 generate_weapons (289 bytes) proved
- [x] 083942 generate_equipment (212 bytes) proved
- [x] 083a16 generate_inventory (135 bytes) proved

## obj/USEITEMS.C

- [x] 0365e2 UseObj (850 bytes) proved
- [x] 036934 using_punt (228 bytes) proved
- [x] 036a18 place_new (96 bytes) proved
- [x] 036a78 UseLockpickOn (133 bytes) proved
- [x] 036afd UseKeyOn (88 bytes) proved
- [x] 036b55 UseKey (69 bytes) proved
- [x] 036b9a UseThing (168 bytes) proved
- [x] 036c42 SpikeDoor_seg040_662 (151 bytes) proved
- [x] 036cd9 SpikeDoor_seg040_352B_6F9 (32 bytes) proved
- [x] 036cf9 UseBonesOn (361 bytes) proved
- [x] 036e62 UsePoleOn (77 bytes) proved
- [x] 036eaf UseAnvilOn (74 bytes) proved
- [x] 036ef9 UseUtil (209 bytes) proved
- [x] 036fca seg040_352B_9EA (40 bytes) proved
- [x] 036ff2 TybalsOrb_seg040_A12 (237 bytes) proved
- [x] 0370df seg040_352B_AFF (133 bytes) proved
- [x] 037164 UseUnique (258 bytes) proved
- [x] 037266 UseLight (411 bytes) proved
- [x] 037401 UseWand (237 bytes) proved
- [x] 0374ee UseFood (1168 bytes) proved
- [x] 03797e UseRockHammerOn (482 bytes) proved
- [x] 037b60 UseOilOn (324 bytes) proved
- [x] 037ca4 UseMagic (740 bytes) proved
- [x] 037f88 UseBook (397 bytes) proved
- [x] 038115 UseRect (518 bytes) proved
- [x] 03831b checkLock (457 bytes) proved
- [x] 0384e4 checkSpell (242 bytes) proved
- [x] 0385d6 checkTrap (215 bytes) proved
- [x] 0386ad moveDoor (97 bytes) proved
- [x] 03870e changeDoor (184 bytes) proved
- [x] 0387c6 OpenDoor (279 bytes) proved
- [x] 0388dd CloseDoor (186 bytes) proved
- [x] 038997 ToggleDoor (53 bytes) proved
- [x] 0389cc DumpTheBag (168 bytes) proved
- [x] 038a74 UseCont (183 bytes) proved
- [x] 038b2b BlastFunction (49 bytes) proved
- [x] 038b5c decode_obj_spell (372 bytes) proved
- [x] 038cd0 remove_spell (51 bytes) proved
- [x] 038d03 useNSpellCharges (207 bytes) proved

## inv/BAGS.C

- [ ] 077110 DoSpecialActions (402 bytes) 
- [ ] 0772a2 MakeBagClose (77 bytes) 
- [ ] 0772ef CloseAllBags (140 bytes) 
- [ ] 07737b FixBagArea (182 bytes) 
- [ ] 077431 CloseTheBag (201 bytes) 
- [ ] 0774fa DisplayOpenBag (180 bytes) 
- [ ] 0775ae FixOpenBag (478 bytes) 
- [ ] 07778c OpenTheBag (944 bytes) 
- [ ] 077b3c ScrollItemsUp (36 bytes) 
- [ ] 077b60 ScrollItemsDown (221 bytes) 
- [ ] 077c3d PutObjectInBag (796 bytes) 
- [ ] 077f59 SwapItemsInBag (392 bytes) 
- [ ] 0780e1 BagWeight (190 bytes) 

## inv/INVDATA.C

- [ ] 079070 RedisplayInvSlot (21 bytes) 
- [ ] 079085 AskInventory (23 bytes) 
- [ ] 07909c FindEmptySlot (43 bytes) 
- [ ] 0790c7 AddToInventory (243 bytes) 
- [ ] 0791ba FindSlot (210 bytes) 
- [ ] 07928c FindObj (560 bytes) 
- [ ] 0794bc find_obj (243 bytes) 
- [ ] 0795af pick_inv (101 bytes) 
- [ ] 079614 WhatsInSlot (23 bytes) 
- [ ] 07962b InvRemoveObject (21 bytes) 
- [ ] 079640 InvRemoveOneObject (21 bytes) 
- [ ] 079655 invRemoveObject (432 bytes) 
- [ ] 079805 RemoveAllFromSlot (136 bytes) 
- [ ] 07988d RemoveOneFromSlot (136 bytes) 
- [ ] 079915 removeFromSlot (48 bytes) 
- [ ] 079945 takeFromSlot (717 bytes) 
- [ ] 079c12 ObjWorn (109 bytes) unproved
- [ ] 079c7f DamageInventory (462 bytes) 
- [ ] 079e4d ItemWeight (151 bytes) 
- [ ] 079ee4 EncumCheck (34 bytes) 

## inv/INVPANEL.C

- [ ] 079fa0 load_inventory_pix (72 bytes) 
- [ ] 079fe8 BeginInventory (413 bytes) 
- [ ] 07a185 EndInventory (5 bytes) 
- [ ] 07a18a ClearInventory (87 bytes) 
- [ ] 07a1e1 DoInventoryMouse (834 bytes) 
- [ ] 07a523 DoInventoryDrag (180 bytes) 
- [ ] 07a5d7 load_inv_pic (58 bytes) 
- [ ] 07a611 DisplayInvSpecial (555 bytes) 
- [ ] 07a83c DisplayInvObject (190 bytes) 
- [ ] 07a8fa SetCursorObj (186 bytes) 
- [ ] 07a9b4 AskHowMany (311 bytes) 
- [ ] 07aaeb ItemFitsSlot (1472 bytes) unproved
- [ ] 07b0ab RearrangeInventory (84 bytes) 
- [ ] 07b0ff AddToEmptySlot (80 bytes) 
- [ ] 07b14f AddTogether (364 bytes) 
- [ ] 07b2bb AddToOccupiedSlot (773 bytes) 
- [ ] 07b5c0 DisplayInventory (70 bytes) 
- [ ] 07b606 displayInventoryArray (449 bytes) 
- [ ] 07b7c7 displayEnc (133 bytes) 
- [ ] 07b84c FindInventoryHit (156 bytes) 

## inv/INVSAVE.C

- [ ] 078270 makePlayerInvCopy (300 bytes) 
- [ ] 07839c SavePlayerInv (218 bytes) 
- [ ] 078476 InvSaveNexts (224 bytes) 
- [ ] 078556 replaceInInv (84 bytes) 
- [ ] 0785aa allocSaveObj (26 bytes) 
- [ ] 0785c4 getSaveObj (34 bytes) 
- [ ] 0785e6 putInInv (15 bytes) 
- [ ] 07863b InvRestoreNexts (243 bytes) 
- [ ] 07872e FreePlayerInv (181 bytes) 
- [ ] 0787e3 getPlayerInvCopy (198 bytes) 
- [ ] 0788a9 RestorePlayerInv (317 bytes) 

## event/TRIGGER.C

- [x] 083b00 trap_init (23 bytes) proved
- [x] 083b17 trap_class_data (36 bytes) proved
- [x] 083b3b UseTrigger (527 bytes) proved
- [x] 083d4a SetOffTrap (76 bytes) proved
- [x] 083d96 UseTrap (2860 bytes) proved
- [x] 084983 delete_trap (190 bytes) proved
- [x] 084a41 gronkify_talkto_player (58 bytes) proved
- [x] 084a7b do_trap_hack (399 bytes) proved
- [ ] 084bee cast_trap_spell (622 bytes) 
- [x] 084e5c trigger_obj_del (166 bytes) proved
- [x] 084f02 trap_obj_del (59 bytes) proved
- [x] 084f3d is_this_wandering (81 bytes) proved
- [ ] 084f8e dont_create_wandering_monster_here (54 bytes) 
- [x] 084fc4 check_alert (83 bytes) proved
- [x] 085017 DoWanderingMonsters (173 bytes) proved
- [x] 0850c4 DoClosingDoors (230 bytes) proved

## event/TRIGSAVE.C

- [x] 0834e0 trap_save (23 bytes) proved

## event/WORLDEV.C

- [x] 073170 find_good_x_and_y (2377 bytes) proved
- [x] 073ab9 do_teleport (175 bytes) proved
- [x] 073b68 change_terrain (698 bytes) proved
- [x] 073e22 whack_thing (194 bytes) proved
- [x] 073ee4 inanimate_spell (80 bytes) proved
- [x] 073f34 go_fish (221 bytes) proved
- [x] 074011 work_bullfrog_tiles (362 bytes) proved
- [x] 07417b emerald_trap (430 bytes) proved
- [x] 074329 player_did_bad (25 bytes) proved
- [x] 074342 eight_pos_switch (135 bytes) proved
- [x] 0743c9 ExplodingBook_ovr107_1259 (192 bytes) proved
- [ ] 074489 talking_door_trap (90 bytes) 
- [x] 0744e3 do_arial_talking (23 bytes) proved
- [x] 0744fa remove_whoami (71 bytes) proved
- [x] 074541 TyballDeath_ovr107_13D1 (206 bytes) proved
- [x] 07460f death_check (304 bytes) proved
- [x] 07473f get_rep_diff (109 bytes) proved
- [x] 0747ac do_repair (280 bytes) proved
- [x] 0748c4 repair_item (498 bytes) proved
- [x] 074ab6 clear_loretry (118 bytes) proved
- [x] 074b2c clear_all_loretries (109 bytes) proved

## combat/RUNES.C

- [x] 078a40 add_rune (100 bytes) proved
- [x] 078aa4 clear_runes (25 bytes) proved
- [x] 078abd ShowRune (98 bytes) proved
- [x] 078b1f RedispRune (97 bytes) proved
- [x] 078b80 clear_shelf (46 bytes) proved
- [x] 078bae mous_in_rune (330 bytes) proved
- [x] 078cf8 not_a_spell (17 bytes) proved
- [x] 078d09 try_clear (208 bytes) proved
- [x] 078dd9 try_cast (204 bytes) proved
- [x] 078ea5 fail_spell (35 bytes) proved
- [x] 078ec8 player_cast (343 bytes) proved

## combat/SPELLS.C

- [x] 0343a4 anti_magic_p (35 bytes) proved
- [x] 0343c7 cast (81 bytes) proved
- [x] 034418 do_spell (574 bytes) proved
- [x] 034656 restore_mana (108 bytes) proved
- [x] 0346c2 restore_hp (152 bytes) proved
- [x] 03475a get_hp_back (114 bytes) proved
- [x] 0347cc healing (71 bytes) proved
- [x] 034813 backfire (123 bytes) proved
- [x] 03488e release_missile (118 bytes) proved
- [x] 034904 build_new_obj (95 bytes) proved
- [x] 034963 sp_true_sight (171 bytes) proved
- [x] 034a0e sp_sheet_light (141 bytes) proved
- [x] 034a9b sp_meteor (239 bytes) proved
- [x] 034b8a sp_ward_undead (74 bytes) proved
- [x] 034bd4 sp_poison (85 bytes) proved
- [x] 034c29 hit_critter_goal (111 bytes) proved
- [x] 034c98 sp_charm (123 bytes) proved
- [x] 034d13 sp_confusion (28 bytes) proved
- [x] 034d2f sp_fear (28 bytes) proved
- [x] 034d4b sp_hold (28 bytes) proved
- [x] 034d67 process_area (688 bytes) proved
- [x] 035017 gronk_area (215 bytes) proved
- [x] 0350ee gronk_whoami (108 bytes) proved
- [x] 03515a nail_area (70 bytes) proved
- [x] 0351a0 nail_1area (69 bytes) proved
- [x] 0351e5 creat_spell (972 bytes) proved
- [x] 0355b1 tremor_area (236 bytes) proved
- [x] 03569d print_monster (81 bytes) proved
- [x] 0356ee mdetect (530 bytes) proved
- [x] 035900 obj_spells (229 bytes) proved
- [x] 0359e5 xt_spells (485 bytes) proved
- [x] 035bca damage_square (185 bytes) proved

## game/CHARGEN.C

- [ ] 06c830 init_char (600 bytes) unproved
- [ ] 06ca88 set_sklmnu (239 bytes) unproved
- [ ] 06cb77 show_atts (336 bytes) 
- [ ] 06ccc7 show_skills (190 bytes) 
- [ ] 06cd85 set_skills (45 bytes) unproved
- [ ] 06cdb2 roll_stats (217 bytes) unproved
- [ ] 06ce8b drawopt (751 bytes) 
- [ ] 06d17a selopt (357 bytes) 
- [ ] 06d2df mousopt (405 bytes) 
- [ ] 06d474 pickopt (803 bytes) unproved
- [ ] 06d797 gen_char (1350 bytes) 
- [ ] 06dcdd adr_chrpic (34 bytes) 
- [ ] 06dcff move_chrpic (63 bytes) 
- [ ] 06dd3e strt_chargen (649 bytes) unproved
- [ ] 06dfc7 create_player (30 bytes) 

## game/GAMEWRAP.C

- [ ] 080410 init_save (146 bytes) 
- [ ] 0804a2 GetLevel (155 bytes) 
- [ ] 08053d SaveLevel (252 bytes) 
- [ ] 080639 get_save_descs (220 bytes) 
- [ ] 080715 ShowSaveRest (152 bytes) 
- [ ] 0807ad DoSaveRest (174 bytes) 
- [ ] 08085b RestoreGame (201 bytes) 
- [ ] 080924 SaveGame (341 bytes) 
- [ ] 080a79 clear_dir (131 bytes) 
- [ ] 080afc copy_file (207 bytes) 
- [ ] 080bcb copy_dir (198 bytes) 
- [ ] 080c91 ChangeLevel (133 bytes) 
- [ ] 080d16 do_level_hacks (229 bytes) 

## game/MAINLOOP.C

- [ ] 01dd67 mainloop (37 bytes) 
- [ ] 01dd8c do_changes (131 bytes) 
- [ ] 01de0f editchng (12 bytes) 

## game/OVR126.C

- [ ] 07cb50 show_credits (24 bytes) 

## game/PLAYDATA.C

- [ ] 07dcc0 MaybePlayerDayLoadrelated_ovr142_0 (5 bytes) 
- [ ] 07dcc5 save_player_data (251 bytes) 
- [ ] 07ddc0 read_player_data (228 bytes) 
- [ ] 07dea4 init_spells (120 bytes) unproved
- [ ] 07df1c swap_tmap (61 bytes) 
- [ ] 07df59 set_drugged (174 bytes) 
- [ ] 07e007 player_affected_by (567 bytes) unproved
- [ ] 07e23e parse_aspells (132 bytes) 
- [ ] 07e2c2 set_maze (91 bytes) 
- [ ] 07e31d parse_spells (208 bytes) unproved
- [ ] 07e3ed armor_val (87 bytes) unproved
- [ ] 07e444 FixPlayerEquips (924 bytes) unproved

## game/PLAYER.C

- [ ] 07e830 Punt_player_inv (34 bytes) 
- [ ] 07e852 InitPlayerRec (176 bytes) 
- [ ] 07e902 init_player (1545 bytes) 
- [ ] 07ef0b mous_player (605 bytes) 
- [ ] 07f168 demous_player (110 bytes) 
- [ ] 07f1d6 report_loc (109 bytes) 
- [ ] 07f243 show_version (27 bytes) 
- [ ] 07f25e home_cam (201 bytes) 
- [ ] 07f327 move_cam (184 bytes) 
- [ ] 07f3df attach_eye (249 bytes) 
- [ ] 07f4d8 release_camera (22 bytes) 
- [ ] 07f4ee crystal_ball (148 bytes) 
- [ ] 07f582 Vortex_ovr143_E09 (244 bytes) 
- [ ] 07f676 chg_plys (91 bytes) 
- [ ] 07f6d1 RollView_ovr143_F58 (37 bytes) 
- [ ] 07f6f6 chg_plyp (39 bytes) 

## game/PLAYTIME.C

- [ ] 02ab8c dispel_spell (305 bytes) 
- [ ] 02acbd duration_check (633 bytes) unproved
- [ ] 02af36 DegradeLights (248 bytes) unproved
- [ ] 02b02e sink_sink_sink (219 bytes) unproved
- [ ] 02b109 set_curmagic (266 bytes) unproved

## game/SKILLS.C

- [ ] 081270 use_skill (112 bytes) 
- [ ] 0812e0 player_use_skill (37 bytes) 
- [ ] 081305 player_compute (133 bytes) unproved
- [ ] 08138a advance (120 bytes) unproved
- [ ] 081402 prime (29 bytes) unproved
- [ ] 08141f add_to_skill (194 bytes) unproved
- [ ] 0814e1 get_skill (335 bytes) unproved
- [ ] 081630 great_advance (70 bytes) 
- [ ] 081676 report_advance (135 bytes) 
- [ ] 0816fd mantra_advance (738 bytes) unproved
- [ ] 0819df game_stats (1028 bytes) 
- [ ] 081de3 dream (263 bytes) 
- [ ] 081eea drop_drunk_player (165 bytes) 
- [ ] 081f8f player_sleep (950 bytes) unproved
- [ ] 082345 player_key_sleep (50 bytes) 
- [ ] 082377 player_eat (127 bytes) 
- [ ] 0823f6 check_victory (373 bytes) 
- [ ] 08256b plant_seed (434 bytes) 
- [ ] 08271d moveto (116 bytes) 
- [ ] 082791 do_resurrect (211 bytes) unproved
- [ ] 082864 do_mstone (28 bytes) 
- [ ] 082880 player_is_dead (537 bytes) unproved
- [ ] 082a99 DetectedTrap (164 bytes) 
- [ ] 082b3d RemoveTrap (564 bytes) 

## game/UWEDIT.C

- [ ] 074c70 main (74 bytes) 
- [ ] 074cba init_world (362 bytes) unproved
- [ ] 074e24 free_world (64 bytes) 
- [ ] 074e64 titlescr (24 bytes) 
- [ ] 074e7c init_edit (148 bytes) 
- [ ] 074f10 editexit (11 bytes) 
- [ ] 074f1b clearobj (25 bytes) 
- [ ] 074f34 change_screen (59 bytes) 
- [ ] 074f6f newscr (111 bytes) 
- [ ] 074fde ovr112_389 (13 bytes) 
- [ ] 074feb strt_demscr (173 bytes) 
- [ ] 075098 free_demscr (20 bytes) 
- [ ] 0750ac reset_game (191 bytes) 
- [ ] 07516b real_death (217 bytes) 
- [ ] 075244 do_3d_view (10 bytes) 
- [ ] 07524e div_zero_ovr112_661 (79 bytes) 
- [ ] 07529d new_player_pos (275 bytes) 
- [ ] 0753b0 ReadCfg_ovr112_839 (51 bytes) 

## conv/BABL.C

- [ ] 0672f1 bab_malloc (418 bytes) 
- [ ] 067493 bab_free (410 bytes) 
- [ ] 06762d bab_realloc (477 bytes) 
- [ ] 06780a init_babl (272 bytes) 
- [ ] 06791a bab_get_globals (187 bytes) 
- [ ] 0679d5 bab_put_globals (178 bytes) 
- [ ] 067a87 load_script (489 bytes) 
- [ ] 067c70 unbound (72 bytes) 
- [ ] 067cb8 ovr093_9F8 (85 bytes) 
- [ ] 067d0d BabRand_ovr095_A5B (48 bytes) 
- [ ] 067d9b ovr093_ADB (268 bytes) 
- [ ] 067ea7 ovr093_BE7 (346 bytes) 
- [ ] 068001 bab_compare_ovr095_A8B (266 bytes) 
- [ ] 06814e babPluralize_ovr095_B95 (70 bytes) 
- [ ] 068194 StringContains_ovr095_BDB (303 bytes) 
- [ ] 0682c3 babl_str_append_ovr095_D0A (189 bytes) 
- [ ] 068380 STRING_COPY_ovr095_DC7 (111 bytes) 
- [ ] 0683ef conv_find_ovr095_E36 (87 bytes) 
- [ ] 068446 conv_length_ovr095_E8D (37 bytes) 
- [ ] 06846b DoVal_ovr095_EB2 (37 bytes) 
- [ ] 068490 convert_string (600 bytes) 
- [ ] 0686e8 AtIndex_ovr095_11C0 (259 bytes) 
- [ ] 0687eb DoReadHeader_ovr095_12C3 (511 bytes) 
- [ ] 0689ea DoCopyCode_ovr095_14B1 (35 bytes) 
- [ ] 068a2e babl_run (986 bytes) 
- [ ] 068e08 vmAdd_ovr095_18CF (57 bytes) 
- [ ] 068e41 babl_neg_ovr095_1908 (36 bytes) 
- [ ] 068e65 MUL_OPCODE_ovr095_192C (57 bytes) 
- [ ] 068e9e subOpcode_ovr095_1965 (57 bytes) 
- [ ] 068fbc babl_and_ovr095_1A83 (69 bytes) 
- [ ] 069001 VM_GT_ovr095_1AC8 (66 bytes) 
- [ ] 069043 vmTstge_ovr095_1B0A (66 bytes) 
- [ ] 069085 vmTstlt_ovr095_1B4C (66 bytes) 
- [ ] 0690c7 VmTstle_ovr095_1B8E (66 bytes) 
- [ ] 069109 ExecTsteq_ovr095_1BD0 (66 bytes) 
- [ ] 06914b opcode_tstne_ovr095_1C12 (66 bytes) 
- [ ] 06918d babl_call_ovr095_1C54 (46 bytes) 
- [ ] 0691bb bab_ret_ovr095_1C82 (40 bytes) 
- [ ] 0691e3 exec_fetchm_ovr095_1CAA (45 bytes) 
- [ ] 069210 vmOffset_ovr095_1CD7 (58 bytes) 
- [ ] 06924a BABL_STORE_ovr095_1D11 (51 bytes) 
- [ ] 06927d talk_calli_ovr095_1D44 (125 bytes) 
- [ ] 0692fa vm_strcmp_ovr095_1DC1 (225 bytes) 
- [ ] 0693db vmSay_ovr095_1EA2 (168 bytes) 
- [ ] 069483 babl_respond_ovr095_1F4A (168 bytes) 
- [ ] 06952b getmem_addr (21 bytes) 
- [ ] 069540 getmem (19 bytes) 
- [ ] 069553 babl_setmem (22 bytes) 
- [ ] 069569 conv_local_ovr095_2030 (22 bytes) 
- [ ] 06957f bab_fun (110 bytes) 
- [ ] 0695ed bab_var (167 bytes) 
- [ ] 069694 bab_var_out (127 bytes) 
- [ ] 069713 bab_var_clear (215 bytes) 

## conv/BABLHACK.C

- [ ] 069980 set_mob_att (33 bytes) 
- [ ] 0699a1 set_attitude (59 bytes) 
- [ ] 0699dc set_race_attitude (375 bytes) 
- [ ] 069b53 x_skills (99 bytes) 
- [ ] 069bb6 x_traps (73 bytes) 
- [ ] 069bff place_object (471 bytes) 
- [ ] 069dd6 take_from_npc_inv (104 bytes) 
- [ ] 069e3e add_to_npc_inv (52 bytes) 
- [ ] 069e72 remove_talker (75 bytes) 
- [ ] 069ebd set_quest (123 bytes) 
- [ ] 069f38 get_quest (108 bytes) 
- [ ] 069fa4 sex (37 bytes) 
- [ ] 069fc9 gronk_door (257 bytes) 
- [ ] 06a0ca x_obj_stuff (697 bytes) 
- [ ] 06a383 x_obj_pos (387 bytes) 

## conv/BARTER.C

- [ ] 06a590 setup_to_barter (421 bytes) 
- [ ] 06a735 barter_init (541 bytes) unproved
- [ ] 06a952 end_barter (202 bytes) 
- [ ] 06aa1c play_barter (71 bytes) 
- [ ] 06aa63 play_slot_hit_abs (92 bytes) 
- [ ] 06aabf npc_slot_hit_abs (92 bytes) 
- [ ] 06ab1b ovr095_58B (110 bytes) 
- [ ] 06ab89 conv_inv_special (68 bytes) 
- [ ] 06abcd npc_barter (70 bytes) 
- [ ] 06ac13 UseTradeSlot_ovr097_6E8 (899 bytes) 
- [ ] 06af96 drawTradeSlot_ovr097_A91 (378 bytes) 
- [ ] 06b110 PickUpFromSlot_ovr097_C39 (162 bytes) 
- [ ] 06b1b2 ovr097_CDB (85 bytes) 
- [ ] 06b207 CombineToSlot_ovr097_D30 (339 bytes) 
- [ ] 06b35a showSelection_ovr097_E83 (169 bytes) 
- [ ] 06b66c nothing_there (51 bytes) 
- [ ] 06b69f do_offer (427 bytes) 
- [ ] 06b84a do_demand (535 bytes) 
- [ ] 06ba61 do_decline (13 bytes) 
- [ ] 06ba6e do_judgement (361 bytes) 
- [ ] 06bbd7 total_offering_ovr097_17CB (106 bytes) 
- [ ] 06bc41 assess_value (264 bytes) 
- [ ] 06bd49 range (62 bytes) 
- [ ] 06bd87 player_barter_items (99 bytes) 
- [ ] 06bdea npc_inv_add (315 bytes) 
- [ ] 06bf25 player_barter_give (104 bytes) 
- [ ] 06bf8d npc_barter_find (180 bytes) 
- [ ] 06c041 npc_barter_give (401 bytes) 
- [ ] 06c1d2 npc_barter_give_id (341 bytes) 
- [ ] 06c327 npc_inv_create (357 bytes) 
- [ ] 06c48c npc_inv_delete (127 bytes) 
- [ ] 06c50b npc_likes_dislikes (50 bytes) 
- [ ] 06c53d does_npc_like (266 bytes) 

## conv/CONVERSE.C

- [ ] 06e490 TalkTo (407 bytes) 
- [ ] 06e627 adr_convpic (34 bytes) 
- [ ] 06e649 move_convpic (40 bytes) 
- [ ] 06e671 strt_converse (1020 bytes) unproved
- [ ] 06ea6d free_converse (38 bytes) unproved
- [ ] 06ea93 do_escape_key (5 bytes) 
- [ ] 06ea98 Converse (885 bytes) 
- [ ] 06ee0d converse_event_loop (208 bytes) unproved
- [ ] 06eedd conv_choice_ovr103_A13 (479 bytes) 
- [ ] 06f0bc conv_fmenu_ovr103_BF2 (548 bytes) 
- [ ] 06f2e0 conv_play_menu (252 bytes) 
- [ ] 06f3dc npc_say (103 bytes) 
- [ ] 06f443 play_respond (83 bytes) 
- [ ] 06f496 play_say (103 bytes) 
- [ ] 06f4fd conv_print (183 bytes) 
- [ ] 06f5b4 conv_pause_ovr103_10DD (51 bytes) 
- [ ] 06f5e7 getInputText_ovr103_1117 (142 bytes) 
- [ ] 06f675 conv_check_inv (135 bytes) 
- [ ] 06f6fc find_barter (174 bytes) 
- [ ] 06f7aa find_barter_total (331 bytes) 
- [ ] 06f8f5 conv_give_inv (232 bytes) 
- [ ] 06f9dd give_ptr_npc (210 bytes) 
- [ ] 06faaf conv_inv_delete (27 bytes) 
- [ ] 06faca conv_find_inv (43 bytes) 
- [ ] 06faf5 conv_take_inv (27 bytes) 
- [ ] 06fb10 conv_take_inv_id (27 bytes) 
- [ ] 06fb2b conv_inv_create (27 bytes) 
- [ ] 06fb46 conv_inv_name (498 bytes) 
- [ ] 06fd38 count_inv (92 bytes) 
- [ ] 06fd94 check_inv_quality (54 bytes) 
- [ ] 06fdca set_inv_quality (75 bytes) 

## conv/CONVVARS.C

- [ ] 070400 setup_converse_data (1170 bytes) 
- [ ] 070892 update_converse_data (497 bytes) 

## map/LIGHTING.C

- [ ] 081010 set_light (282 bytes) unproved
- [ ] 08112a init_lighting (103 bytes) 
- [ ] 081191 random_light (168 bytes) 

## map/MAP.C

- [ ] 07c830 Map_Init (55 bytes) 
- [ ] 07c867 OverwriteAllTiles_ovr128_37 (63 bytes) 
- [ ] 07c8a6 Map_Load (248 bytes) 
- [ ] 07c99e Map_Save (240 bytes) 

## map/MAPADDR.C

- [ ] 031a69 Map_GetAddr (53 bytes) 
- [ ] 031a9e CreateObj (272 bytes) 

## map/TEXTMAPS.C

- [ ] 07d5b0 init_txtlib (477 bytes) 
- [ ] 07d78d ovr131_1DD (48 bytes) 
- [ ] 07d7bd Txm_Load (213 bytes) 
- [ ] 07d892 Txm_Save (147 bytes) 
- [ ] 07d925 load_txtmaps (295 bytes) 
- [ ] 07da4c load_tr_mem (325 bytes) 
- [ ] 07db91 Load_Terrains (166 bytes) 

## sound/SOUND.C

- [ ] 01ef8b cllbck_tst (18 bytes) 
- [ ] 01ef9d init_timers (68 bytes) 
- [ ] 01efe1 init_sounds (437 bytes) 
- [ ] 01f196 load_global_timbre (216 bytes) 
- [ ] 01f26e init_timbres (211 bytes) unproved
- [ ] 01f341 install_timbre (91 bytes) unproved
- [x] 01f39c load_new_music (343 bytes) proved
- [ ] 01f4f3 load_sound_driver (154 bytes) 
- [x] 01f58d play_music (85 bytes) proved
- [ ] 01f65b read_file_to_mbuf (93 bytes) 
- [ ] 01f6b8 get_current_music (8 bytes) unproved
- [ ] 01f6c0 music_is_on (20 bytes) 
- [ ] 01f6d4 fx_is_on (20 bytes) unproved
- [x] 01f6e8 turn_music (119 bytes) proved
- [ ] 01f77c turn_fx (44 bytes) 
- [ ] 01f7cb stop_music (37 bytes) 
- [x] 01f82e play_effect (621 bytes) proved
- [x] 01fa9b play_effect_here (129 bytes) proved
- [ ] 01fb1c play_effect_on_mobile (97 bytes) 
- [x] 01fb7d kill_effect (127 bytes) proved
- [x] 01fbfc seg014_1DC5_C7C (153 bytes) proved
- [ ] 01fd8e read_fx_data (267 bytes) unproved
- [ ] 01fe99 init_fx (80 bytes) unproved
- [ ] 01fee9 fx_play (556 bytes) unproved
- [ ] 020115 play_instrument (814 bytes) 
- [ ] 020443 play_cup_tune (139 bytes) 
- [ ] 0204ce free_timers (27 bytes) 
- [ ] 0204e9 free_sounds (81 bytes) 
- [x] 02053a set_new_music (11 bytes) proved
- [x] 020545 seg014_1DC5_15C5 (23 bytes) proved
- [x] 02055c loop_music_maybe (49 bytes) proved
- [x] 02058d change_music_maybe (436 bytes) proved
- [ ] 020741 music_over (44 bytes) unproved
- [ ] 02076d do_settings (77 bytes) 
- [ ] 0207ba init_speech (227 bytes) 
- [ ] 02089d speech_available (8 bytes) 
- [ ] 0208a5 init_voc (92 bytes) unproved
- [ ] 020901 play_speech (448 bytes) 
- [ ] 020ac1 update_speech (311 bytes) 
- [ ] 020bf8 speech_over (78 bytes) 
- [ ] 020c46 stop_speech (24 bytes) 
- [ ] 020c5e free_speech_stuff (47 bytes) 
- [ ] 020c8d seg014_1DC5_1D0D (96 bytes) 

## ui/AUTOMAP.C

- [ ] 066050 AutoMap (151 bytes) 
- [ ] 0660e7 SaveAutoMapLevel (135 bytes) 
- [ ] 06616e GetAutoMapLevel (48 bytes) 
- [ ] 06619e ExitAutoMap (130 bytes) 
- [ ] 066220 ClearAutoMap (21 bytes) 
- [ ] 066235 ShowDungeonMap (322 bytes) 
- [ ] 066377 ShadeSide (248 bytes) 
- [ ] 06646f PixelDarken (108 bytes) 
- [ ] 0664db DoTile (484 bytes) 
- [ ] 0666bf DoDoorTile (248 bytes) 
- [ ] 0667b7 CornerShade_ovr094_9D2 (71 bytes) 
- [ ] 0667fe DetermineBetterHit (248 bytes) 
- [ ] 0668f6 ManageDungeonMap (1242 bytes) 
- [ ] 066dd0 RedisplayStrings (188 bytes) 
- [ ] 066e8c SaveTheWords (206 bytes) 
- [ ] 066f5a GetTheWords (92 bytes) 
- [ ] 066fb6 ShowAutoMapLevel (386 bytes) 
- [ ] 067138 ChangeAutoMapLevel (84 bytes) 
- [ ] 06718c automap_scr (10 bytes) 

## ui/GAMESTRN.C

- [ ] 035c83 init_strings (128 bytes) 
- [ ] 035d03 free_strings (10 bytes) 
- [ ] 035d0d get_string (137 bytes) 
- [ ] 035d96 make_string (260 bytes) 
- [ ] 035e9a replace_string (123 bytes) 
- [ ] 035f15 clear_dynamics (145 bytes) 
- [ ] 035fa6 get_name (184 bytes) 
- [ ] 03605e fix_name_string (182 bytes) 
- [ ] 036114 game_sprint (41 bytes) 
- [ ] 03613d game_strings_3 (122 bytes) 
- [ ] 0361b7 seg039_3495_537 (173 bytes) 
- [ ] 036264 seg039_3495_5E4 (32 bytes) 
- [ ] 036284 read_string (384 bytes) 
- [ ] 036404 seg039_3495_784 (49 bytes) 
- [ ] 036435 seg039_3495_7B5 (98 bytes) 
- [ ] 036497 seg039_3452_814 (67 bytes) 
- [ ] 0364da seg039_3495_85A (67 bytes) 
- [ ] 03651d seg039_3495_89D (113 bytes) 
- [ ] 03658e str_cat (84 bytes) 

## ui/INTERACT.C

- [ ] 0260ff pull_chain (35 bytes) 
- [ ] 026122 print_info (221 bytes) unproved
- [ ] 0261ff flask_info (352 bytes) 
- [ ] 02635f start_gameinp (162 bytes) 
- [ ] 026401 clear_gameinp (60 bytes) 
- [ ] 02643d init_gamedisp (68 bytes) 
- [ ] 026481 clear_gamedisp (19 bytes) 
- [ ] 026494 display_scr (99 bytes) 
- [ ] 0265f6 RedispInv (37 bytes) 
- [ ] 02661b InPickRange (306 bytes) 
- [ ] 02674d BridgeHeight (91 bytes) 
- [ ] 0267a8 BlockingTerrain (530 bytes) 
- [ ] 0269ba pick_3d (262 bytes) 
- [ ] 026ac0 look_nothing (126 bytes) 
- [ ] 026b3e release_3d (80 bytes) 
- [ ] 026b8e player_3dget (590 bytes) 
- [ ] 026ddc player_3dtalk (30 bytes) 
- [ ] 026dfa seg024_24DC_D0A (22 bytes) 
- [ ] 026e10 player_3dlook (287 bytes) 
- [ ] 026f2f player_3duse (122 bytes) 
- [ ] 026fa9 player_3dattack (70 bytes) unproved
- [ ] 026fef mous_in_3d (412 bytes) unproved
- [ ] 02718b inv_look (277 bytes) 
- [ ] 0272ae seg026_2716_F98 (65 bytes) 
- [ ] 0272ef mous_in_inv (286 bytes) 
- [ ] 02740d mous_in_panel (44 bytes) 
- [ ] 027439 new_IconSelect (70 bytes) 
- [ ] 02747f new_IconUnselect (70 bytes) 
- [ ] 0274c5 deal_with_icons (348 bytes) unproved
- [ ] 027621 pick_fightmode (143 bytes) unproved
- [ ] 0276b0 punt_fightmode (72 bytes) 
- [ ] 0276f8 toggle_fightmode (31 bytes) 
- [ ] 027717 check_save (57 bytes) 
- [ ] 027750 check_rest (29 bytes) 

## ui/MAINMENU.C

- [ ] 07f8c0 adr_opbtn (34 bytes) 
- [ ] 07f8e2 move_opbtn (114 bytes) 
- [ ] 07f954 ovr138_94 (57 bytes) 
- [ ] 07f98d do_intro_scene (47 bytes) 
- [ ] 07f9bc draw_start_buttons (332 bytes) 
- [ ] 07fb08 real_start (809 bytes) 
- [ ] 07fe31 parse_start_mouse (503 bytes) 
- [ ] 080028 parse_start_input (445 bytes) 
- [ ] 0801e5 do_journey (375 bytes) 

## ui/MOUSE.C

- [ ] 01dfce init_mouse (103 bytes) 
- [ ] 01e035 _actual_mhide (76 bytes) 
- [ ] 01e081 mouse_show (22 bytes) 
- [ ] 01e097 mouse_hide (33 bytes) 
- [ ] 01e0b8 mous_3d_set (29 bytes) 
- [ ] 01e0d5 mous_in_3d_p (37 bytes) 
- [ ] 01e0fa mous_3d_hide (282 bytes) 
- [ ] 01e214 mous_3d_show (101 bytes) 
- [ ] 01e279 mouse_getxy (21 bytes) 
- [ ] 01e28e mouse_Qgetxy (42 bytes) 
- [ ] 01e2b8 mouse_clearQ (18 bytes) 
- [ ] 01e2ca flush_keys (14 bytes) 
- [ ] 01e2d8 mouse_putxy (45 bytes) 
- [ ] 01e305 mouse_getbut (34 bytes) 
- [ ] 01e327 mouse_release (125 bytes) 
- [ ] 01e3a4 mouse_dragged (137 bytes) 
- [ ] 01e42d mouse_constrain (29 bytes) 
- [ ] 01e44a mouse_freereign (25 bytes) 
- [ ] 01e463 do_mouse_input (76 bytes) 
- [ ] 01e4af do_keyarray_input (124 bytes) 
- [ ] 01e52b do_keyboard_input (153 bytes) 
- [ ] 01e5c4 get_input (77 bytes) 
- [ ] 01e611 mouse_get_input (13 bytes) 
- [ ] 01e61e mouse_get_input_sp (13 bytes) 
- [ ] 01e62b defineMouseRegion (113 bytes) 
- [ ] 01e69c undefineMouseRegion (94 bytes) 
- [ ] 01e6fa force_mouse_cursor (50 bytes) 
- [ ] 01e72c unforce_mouse_cursor (74 bytes) 
- [ ] 01e776 mouse_check_reg (76 bytes) 
- [ ] 01e7c2 set_mouse_data (102 bytes) 
- [ ] 01e828 checkMouse (212 bytes) 
- [ ] 01e8fc moveMouse (620 bytes) 
- [ ] 01eb68 MousQUp (64 bytes) 
- [ ] 01eba8 MousReSave (71 bytes) 
- [ ] 01ebef MousReSave3d (195 bytes) 
- [ ] 01ecb2 drawMouse (100 bytes) 
- [ ] 01ed16 draw3dMouse (104 bytes) 
- [ ] 01ed7e warpMouse (107 bytes) 
- [ ] 01ede9 keyboard_mouse (356 bytes) 
- [ ] 01ef4d mouse_btns (62 bytes) 

## ui/OPTIONS.C

- [ ] 07cce0 run_options_panel (289 bytes) 
- [ ] 07ce01 draw_options_back (42 bytes) 
- [ ] 07ce2b draw_options_btn (59 bytes) 
- [ ] 07ce66 hilite (56 bytes) 
- [ ] 07ce9e close_option_panel (86 bytes) 
- [ ] 07cef4 draw_top_page (30 bytes) 
- [ ] 07cf12 draw_save_page (90 bytes) 
- [ ] 07cf6c draw_quit_yn_page (30 bytes) 
- [ ] 07cf8a draw_music_and_sound_page (169 bytes) 
- [ ] 07d033 draw_detail_page (98 bytes) 
- [ ] 07d095 choose_music (88 bytes) 
- [ ] 07d0ed choose_sounds (88 bytes) 
- [ ] 07d145 choose_detail (178 bytes) 
- [ ] 07d1f7 choose_top (87 bytes) 
- [ ] 07d24e choose_save (170 bytes) 
- [ ] 07d2f8 choose_quit (91 bytes) 
- [ ] 07d353 set_options_page (22 bytes) 
- [ ] 07d369 press_options_btn (77 bytes) 
- [ ] 07d3b6 options_mouse_click (28 bytes) 
- [ ] 07d3d2 options_handle_key (246 bytes) 
- [ ] 07d4c8 do_option_shortcut (70 bytes) 

## ui/OVR096.C

- [ ] 06c7f0 init_browse (5 bytes) 
- [ ] 06c7f5 free_browse (5 bytes) 
- [ ] 06c7fa place_browse (8 bytes) 
- [ ] 06c802 browse_show (5 bytes) 
- [ ] 06c807 browse_hide (5 bytes) 
- [ ] 06c80c browse_redisplay (5 bytes) 
- [ ] 06c811 browse_chain (5 bytes) 

## ui/OVR137.C

- [ ] 07f8b0 ovr137_0 (5 bytes) 

## ui/PANELS.C

- [ ] 031bae set_flask (165 bytes) 
- [ ] 031c53 set_compass (84 bytes) 
- [ ] 031ca7 reset_scrgr (103 bytes) 
- [ ] 031d0e init_scrgr (591 bytes) 
- [ ] 031f5d hold_scrgr (24 bytes) 
- [ ] 031f75 free_scrgr (10 bytes) 
- [ ] 031f7f set_screen_frame (454 bytes) 
- [ ] 032145 update_screen (327 bytes) 
- [ ] 03228c adjust_flasks (820 bytes) 
- [ ] 0325c0 adjust_dragons (1632 bytes) 
- [ ] 032c20 adjust_compass (141 bytes) 
- [ ] 032cad adjust_power (166 bytes) 
- [ ] 032d53 adjust_panel (84 bytes) 
- [ ] 032da7 adjust_eyes (191 bytes) 
- [ ] 032e66 adr_weapon (88 bytes) 
- [ ] 032ebe move_weapon (41 bytes) 
- [ ] 032ee7 load_weapon (37 bytes) 
- [ ] 032f0c do_weapload (348 bytes) unproved
- [ ] 033068 jiggle_weapon (97 bytes) 
- [ ] 0330c9 adjust_weapon (412 bytes) unproved
- [ ] 033265 load_weapcm (173 bytes) 
- [ ] 033312 set_runes (169 bytes) 
- [ ] 0333bb active_spells (182 bytes) 
- [ ] 033471 init_panelflip (340 bytes) 
- [ ] 0335c5 pretty_panelagain (123 bytes) 
- [ ] 033640 free_panelflip (53 bytes) 
- [ ] 033675 do_panel_frame (1638 bytes) 
- [ ] 033cdb flip_scale (468 bytes) 
- [ ] 033eaf flip_column (334 bytes) 
- [ ] 033ffd do_fbuf_bms (349 bytes) unproved
- [ ] 03415a send_FB (21 bytes) 
- [ ] 03416f player_look_shaft (20 bytes) 
- [ ] 034183 player_look_grave (25 bytes) 

## ui/SCROLLIO.C

- [ ] 039236 init_scroll (41 bytes) 
- [ ] 03925f set_mouse_in (31 bytes) 
- [ ] 03927e do_main_scroll (22 bytes) 
- [ ] 039294 do_npc_scroll (22 bytes) 
- [ ] 0392aa do_play_scroll (22 bytes) 
- [ ] 0392c0 scroll_wait (142 bytes) 
- [ ] 03934e draw_edges (69 bytes) 
- [ ] 039393 draw_conv_edges (103 bytes) 
- [ ] 0393fa scroll_up (141 bytes) 
- [ ] 039487 scroll_more (147 bytes) 
- [ ] 03951a scroll_print (304 bytes) 
- [ ] 03964a scroll_print1 (82 bytes) 
- [ ] 03969c scroll_print2 (83 bytes) 
- [ ] 0396ef scroll_print3 (490 bytes) 
- [ ] 0398d9 scroll_wrap (248 bytes) 
- [ ] 0399d1 draw_scroll (82 bytes) 
- [ ] 039b1b scroll_clear (144 bytes) 
- [ ] 039bab wd_replace (118 bytes) 
- [ ] 039c21 wd_bool (109 bytes) 
- [ ] 039c8e wdialog (1267 bytes) 
- [ ] 03a181 wyorn (243 bytes) 

## ui/STATS.C

- [ ] 082f40 sp_hdr (243 bytes) 
- [ ] 083033 sp_att (92 bytes) 
- [ ] 08308f sp_hp (116 bytes) 
- [ ] 083103 sp_mp (115 bytes) 
- [ ] 083176 sp_xp (78 bytes) 
- [ ] 0831c4 sp_skill (219 bytes) 
- [ ] 08329f RedispStat (230 bytes) 
- [ ] 083385 mous_in_stat (182 bytes) 
- [ ] 08343b ovr145_4FB (17 bytes) 

## gfx/CUTS.C

- [ ] 071a40 get_cut_banks (16 bytes) 
- [ ] 071a50 get_cuts_block (68 bytes) 
- [ ] 071a94 free_cuts_ems (10 bytes) 
- [ ] 071a9e makeFourChars_ovr108_671 (60 bytes) 
- [ ] 071ada anm_lptab (163 bytes) 
- [ ] 071b7d read_anim_hdr (103 bytes) 
- [ ] 071be4 readlp (72 bytes) 
- [ ] 071c2c read_lp_inc (137 bytes) 
- [ ] 071cb5 conv_anmpal (108 bytes) 
- [ ] 071d21 anm_cycle (202 bytes) 
- [ ] 071deb free_block (11 bytes) 
- [ ] 071df6 cutsop_txt (512 bytes) 
- [ ] 071ff6 cutsop_erase (15 bytes) 
- [ ] 072005 cutsop_func (8 bytes) 
- [ ] 07200d cutsop_pause (45 bytes) 
- [ ] 07203a cutsop_wait (71 bytes) 
- [ ] 072081 cutsop_skip (65 bytes) 
- [ ] 0720c2 cutsop_stop (15 bytes) 
- [ ] 0720d1 cutsop_end (20 bytes) 
- [ ] 0720e5 cutsop_loop (39 bytes) 
- [ ] 07210c cutsop_data (77 bytes) 
- [ ] 072159 cutsop_say (99 bytes) 
- [ ] 0721bc cutsop_fadeout (34 bytes) 
- [ ] 0721de cutsop_fadein (34 bytes) 
- [ ] 072200 cutsop_jump (61 bytes) 
- [ ] 07223d cutsop_punt (30 bytes) 
- [ ] 07225b cutsop_clang (21 bytes) 
- [ ] 072270 do_sound (57 bytes) 
- [ ] 0722a9 show_anm (2969 bytes) 
- [ ] 072e42 runcutscene (225 bytes) 
- [ ] 072f23 init_cutscene (19 bytes) 
- [ ] 072f36 value_cuts (323 bytes) 
- [ ] 073079 cuts_skipline (25 bytes) 

## gfx/GRFX.C

- [ ] 075ff0 grfx_init (53 bytes) 
- [ ] 076025 grfx_load_font (136 bytes) 
- [ ] 0760ad grfx_close (15 bytes) 
- [ ] 0760bc grfx_clear (47 bytes) 
- [ ] 0760eb read_quikpal (95 bytes) 
- [ ] 07614a grfx_quikpal (46 bytes) 
- [ ] 076178 grfx_setpal (43 bytes) 
- [ ] 0761a3 fadeout (262 bytes) 
- [ ] 0762a9 fadein (232 bytes) 
- [ ] 076391 out3d (67 bytes) 
- [ ] 0763d4 in3d (150 bytes) 
- [ ] 07646a fadeout3d (23 bytes) 
- [ ] 076481 fadein3d (23 bytes) 
- [ ] 076498 steps_fadeout3d_ovr118_534 (23 bytes) 
- [ ] 0764af steps_fadein3d_ovr118_54B (23 bytes) 
- [ ] 0764c6 fill_FB (40 bytes) 
- [ ] 0764ee cameras_fade (84 bytes) 

## gfx/LOADGR.C

- [ ] 0765c0 get_pals (137 bytes) 
- [ ] 076649 _ld_open (290 bytes) 
- [ ] 07676b _ld_close (27 bytes) 
- [ ] 076786 _ld_gr (185 bytes) 
- [ ] 07683f adrnew_ems (84 bytes) 
- [ ] 076893 adrold_ems (65 bytes) 
- [ ] 0768d4 movenew_ems (64 bytes) 
- [ ] 076914 moveobj_ems (111 bytes) 
- [ ] 076983 move_reload_obj_ems (117 bytes) 
- [ ] 0769f8 adrnew_vram (11 bytes) 
- [ ] 076a03 adr_const (12 bytes) 
- [ ] 076a0f movenew_vram (125 bytes) 
- [ ] 076a8c move_vram (89 bytes) 
- [ ] 076ae5 gronk_gr (209 bytes) 
- [ ] 076bb6 load_gr_ems (46 bytes) 
- [ ] 076be4 load_obj_ems (36 bytes) 
- [ ] 076c08 reload_obj_ems (38 bytes) 
- [ ] 076c2e load_tr_ems (259 bytes) 
- [ ] 076d31 load_gr_video (46 bytes) 
- [ ] 076d5f GrLoadAt_ovr119_949 (61 bytes) 
- [ ] 076d9c reload_gr_vpic (55 bytes) 
- [ ] 076dd3 read_gr_far (43 bytes) 
- [ ] 076dfe load_all_gr (405 bytes) 
- [ ] 076f93 load_doors (88 bytes) 

## gfx/MODEX.ASM

- [ ] 020cee grab (218 bytes) replaced
- [ ] 020dc8 seg015_1F9B_E8 (16 bytes) replaced
- [ ] 020dd8 mem_set (22 bytes) replaced
- [ ] 020dee str_len (22 bytes) replaced
- [ ] 020e04 str_ncopy (32 bytes) replaced
- [ ] 020e24 str_copy (53 bytes) replaced
- [ ] 020e59 FindStringDelimiter (40 bytes) replaced
- [ ] 020e81 str_str (91 bytes) replaced
- [ ] 020edc str_cmp (41 bytes) replaced
- [ ] 020f05 local_do_palette (53 bytes) replaced
- [ ] 020f3a seg015_1F9B_25A (77 bytes) replaced
- [ ] 020f87 seg015_1F9B_2A7 (126 bytes) replaced
- [ ] 021005 seg015_1F9B_325 (65 bytes) replaced
- [ ] 021046 seg015_1F9B_366 (85 bytes) replaced

## gfx/SCRSHOT.C

- [ ] 0756c0 save_screenshot (329 bytes) 
- [ ] 075809 ovr112_149 (75 bytes) 
- [ ] 075854 ovr112_194 (47 bytes) 
- [ ] 075883 ovr112_1C3 (224 bytes) 
- [ ] 075963 ovr112_2A3 (381 bytes) 
- [ ] 075ae0 GifPixel_ovr116_420 (135 bytes) 

## gfx/SHOWPIC.C

- [ ] 080ef0 LoadBitMap_ovr141_0 (250 bytes) 

## gfx/VALLOC.ASM

- [ ] 0036ec seg001_023B_C (62 bytes) notneeded
- [ ] 00372a valloc (16 bytes) notneeded
- [ ] 00373a seg001_5A (171 bytes) notneeded
- [ ] 0037e5 vfree (13 bytes) notneeded
- [ ] 0038b5 save_rect (26 bytes) notneeded
- [ ] 00393b restore_rect (13 bytes) notneeded

## gfx/VSTATS.ASM

- [ ] 0039b1 valloc_stats (185 bytes) notneeded

## 3d/DRAWOBJ.C

- [ ] 02efd5 do_obj (1426 bytes) 
- [ ] 02f567 do_rect (1727 bytes) 
- [ ] 02fc26 do_door (1539 bytes) 

## 3d/GAMESORT.C

- [ ] 030229 sort_setup (228 bytes) 
- [ ] 03030d sort_obj (86 bytes) 
- [ ] 030363 build_sort (27 bytes) 
- [ ] 03037e do_partition (213 bytes) 
- [ ] 030453 z_part (78 bytes) 
- [ ] 0304a1 door_part (168 bytes) 
- [ ] 030549 set_sds (164 bytes) 
- [ ] 0305ed set_osum (57 bytes) 
- [ ] 030626 clear_objsort (53 bytes) 
- [ ] 03065b do_objsort (1381 bytes) 

## 3d/GRIDDB.C

- [ ] 021136 set_pix_xfer (26 bytes) 
- [ ] 021150 set_graphics_level (117 bytes) 
- [ ] 0211c5 process_grid (638 bytes) 
- [ ] 021443 do_3d_pickup (483 bytes) 
- [ ] 021626 subprocess (628 bytes) 
- [ ] 02189a polyflr (230 bytes) 
- [ ] 021980 polycie (228 bytes) 
- [ ] 021a64 polywal (230 bytes) 
- [ ] 021b4a txtflr (465 bytes) 
- [ ] 021d1b txtwal (417 bytes) 
- [ ] 021ebc grdb_elem (1642 bytes) 

## 3d/SETPNT.ASM

- [ ] 021101 SetPnt (53 bytes) 

## sys/ARC.C

- [ ] 065790 open_arc (339 bytes) 
- [ ] 0658e3 close_arc (217 bytes) 
- [ ] 0659bc put_arc (1006 bytes) 
- [ ] 065daa get_arc (276 bytes) 
- [ ] 065ebe count_arc (106 bytes) 
- [ ] 065f28 check_arc (178 bytes) 

## sys/DEBUG.C

- [ ] 073140 init_debug (27 bytes) 
- [ ] 07315b dprintf (5 bytes) 
- [ ] 073160 ovr106_20 (5 bytes) 

## sys/ERROR.C

- [ ] 0754f0 error_code (171 bytes) 
- [ ] 07559b first_punt (28 bytes) 
- [ ] 0755b7 pfatal_code (140 bytes) 
- [ ] 075643 pfatal (65 bytes) 

## sys/MISCUTIL.C

- [ ] 085290 flip_bool (23 bytes) 
- [ ] 0852a7 solve_compass (101 bytes) 
- [ ] 08530c mpos (133 bytes) 
- [ ] 085391 postodir (26 bytes) 
- [ ] 0853ab print_path_to (192 bytes) 
- [ ] 08546b unsolve_compass (193 bytes) 
- [ ] 08552c errmsg (53 bytes) 
- [ ] 085561 do_beep (32 bytes) 
- [ ] 085581 dbg_break (23 bytes) 
- [ ] 085598 dir_exist (43 bytes) 
- [ ] 0855c3 check_dirs (65 bytes) 
- [ ] 085604 memcheck (51 bytes) 
- [ ] 085637 check_fds (167 bytes) 
- [ ] 0856de blttodrive (86 bytes) 
- [ ] 085734 bltfromdrive (83 bytes) 
- [ ] 085787 intoFarBuffer_ovr167_5DA (77 bytes) 
- [ ] 0857d4 FarWrite_ovr167_627 (77 bytes) 
- [ ] 085821 xorread (124 bytes) 
- [ ] 08589d xorwrite (132 bytes) 
- [ ] 085921 data_fopen (53 bytes) 

## sys/OVR090.C

- [ ] 065750 ReturnFar_ovr090_0 (5 bytes) 
- [ ] 065755 ovr090_5 (14 bytes) 
- [ ] 065763 ovr090_13 (5 bytes) 
- [ ] 065768 Nop_ovr090_18 (5 bytes) 
- [ ] 06576d ovr090_1D (5 bytes) 
- [ ] 065772 DoNothing_ovr090_22 (5 bytes) 
- [ ] 065777 ReturnFar_ovr090_27 (5 bytes) 

## sys/OVR127.C

- [ ] 07cb70 ovr127_0 (7 bytes) 

## sys/OVR152.C

- [ ] 083ae0 ovr152_0 (5 bytes) 
- [ ] 083ae5 ovr152_5 (14 bytes) 
- [ ] 083af3 ovr152_13 (5 bytes) 

## sys/UTIL.C

- [ ] 038dd2 mvcheck (75 bytes) 
- [ ] 038e1d move_along (155 bytes) unproved
- [ ] 038eb8 seg041_35D7_E9 (54 bytes) 
- [x] 038eee rollem (71 bytes) proved

## 

- [ ] 011f63 seg005_10B2_113 (32 bytes) notneeded
- [ ] 011f83 seg005_10B2_133 (64 bytes) notneeded
- [ ] 011fc3 seg005_10B2_173 (67 bytes) notneeded
- [ ] 012006 seg005_10B2_1B6 (45 bytes) notneeded
- [ ] 012076 seg005_10B2_226 (35 bytes) notneeded
- [ ] 012099 seg005_10B2_249 (55 bytes) notneeded
- [ ] 0120fd seg005_10B2_2AD (81 bytes) notneeded
- [ ] 01214e CloseFile_seg005_10B2_2FE (46 bytes) notneeded
- [ ] 01217c seg005_10B2_32C (37 bytes) notneeded
- [ ] 0121a1 seg005_10B2_351 (128 bytes) notneeded
- [ ] 012221 seg005_10B2_3D1 (63 bytes) notneeded
- [ ] 012260 seg005_10B2_410 (109 bytes) notneeded
- [ ] 0122cd CloseFile_seg005_10B2_47D (128 bytes) notneeded
- [ ] 01234d seg005_10B2_4FD (82 bytes) notneeded
- [ ] 01239f seg005_10B2_54F (134 bytes) notneeded
- [ ] 012425 seg005_10B2_5D5 (111 bytes) notneeded
- [ ] 012494 seg005_10B2_644 (56 bytes) notneeded
- [ ] 0124cc seg005_10B2_67C (198 bytes) notneeded
- [ ] 012592 seg005_10B2_742 (152 bytes) notneeded
- [ ] 01262a seg005_10B2_7DA (34 bytes) notneeded
- [ ] 01264c AddFileToTable_seg005_10B2_7FC (30 bytes) notneeded
- [ ] 0126b8 FileReadToAddress_seg005_10B2_868 (236 bytes) notneeded
- [ ] 0127a4 FileReadToAddress_seg005_10B2_954 (77 bytes) notneeded
- [ ] 0127f1 seg005_10B2_9A1 (102 bytes) notneeded
- [ ] 012857 SeekToAFile_Location_seg005_10B2_A07 (102 bytes) notneeded
- [ ] 0128bd seg005_10B2_A6D (67 bytes) notneeded
- [ ] 012900 seg005_10B2_AB0 (73 bytes) notneeded
- [ ] 012949 seg005_10B2_AF9 (19 bytes) notneeded
- [ ] 01295c seg005_10B2_B0C (19 bytes) notneeded
- [ ] 01296f seg005_10B2_B1F (40 bytes) notneeded
- [ ] 012997 seg005_10B2_B47 (23 bytes) notneeded
- [ ] 0129ae seg005_10B2_B5E (28 bytes) notneeded
- [ ] 0129ca seg005_10B2_B7A (263 bytes) notneeded
- [ ] 012ae6 seg005_10B2_C96 (320 bytes) notneeded
- [ ] 012c26 InitRNG_seg005_10B2_DD6 (17 bytes) unproved
- [x] 012c37 RNG_seg005_DE7 (38 bytes) proved
- [ ] 012c5d seg005_10B2_E0D (143 bytes) notneeded
- [ ] 012cec seg005_10B2_E9C (216 bytes) notneeded
- [ ] 012ded seg005_10B2_F9D (30 bytes) notneeded
- [ ] 012e60 seg005_10B2_1010 (77 bytes) notneeded
- [ ] 012ead seg005_10B2_105D (51 bytes) notneeded
- [ ] 012ee0 seg005_10B2_1090 (26 bytes) notneeded
- [ ] 012efa seg005_10B2_10AA (291 bytes) notneeded
- [ ] 01301d seg005_10B2_11CD (464 bytes) notneeded
- [ ] 0131ed seg005_10B2_139D (72 bytes) notneeded
- [ ] 01326e seg005_10B2_141E (44 bytes) notneeded
- [ ] 01329a seg005_10B2_144A (44 bytes) notneeded
- [ ] 0132c6 seg005_10B2_1476 (319 bytes) notneeded
- [ ] 013405 ParseNumber_seg005_10B2_15B5 (112 bytes) notneeded
- [ ] 013482 seg005_10B2_1632 (34 bytes) notneeded
- [ ] 0134a4 seg005_10B2_1654 (49 bytes) notneeded
- [ ] 0134f1 seg005_10B2_16A1 (28 bytes) notneeded
- [ ] 01351a seg005_10B2_16CA (27 bytes) notneeded
- [ ] 013535 seg005_10B2_16E5 (30 bytes) notneeded
- [ ] 013553 seg005_10B2_1703 (16 bytes) notneeded
- [ ] 013563 seg005_10B2_1713 (60 bytes) notneeded
- [ ] 01359f seg005_10B2_174F (65 bytes) notneeded
- [ ] 0135e0 seg005_10B2_1790 (198 bytes) notneeded
- [ ] 0136a6 seg005_10B2_1856 (104 bytes) notneeded
- [ ] 01370e seg005_10B2_18BE (51 bytes) notneeded
- [ ] 013741 seg005_10B2_18F1 (45 bytes) notneeded
- [ ] 01376e seg005_10B2_191E (65 bytes) notneeded
- [ ] 0137af seg005_10B2_195F (41 bytes) notneeded
- [ ] 0137d8 seg005_10B2_1988 (89 bytes) notneeded
- [ ] 013831 seg005_10B2_19E1 (24 bytes) notneeded
- [ ] 013849 seg005_10B2_19F9 (184 bytes) notneeded
- [ ] 013901 seg005_10B2_1AB1 (27 bytes) notneeded
- [ ] 01391c seg005_10B2_1ACC (94 bytes) notneeded
- [ ] 01398b seg005_10B2_1B3B (29 bytes) notneeded
- [ ] 0139a8 seg005_10B2_1B58 (32 bytes) notneeded
- [ ] 0139c8 seg005_10B2_1B78 (15 bytes) notneeded
- [ ] 0139d7 seg005_10B2_1B87 (17 bytes) notneeded
- [ ] 0139e8 seg005_10B2_1B98 (40 bytes) notneeded
- [ ] 013a10 seg005_10B2_1BC0 (156 bytes) notneeded
- [ ] 013ad1 seg005_10B2_1C81 (99 bytes) notneeded
- [ ] 013b34 seg005_10B2_1CE4 (39 bytes) notneeded
- [ ] 013b5b seg005_10B2_1D0B (61 bytes) notneeded
- [ ] 013b98 seg005_10B2_1D48 (17 bytes) notneeded
- [ ] 013ba9 MoveFilePointer_seg005_10B2_1D59 (41 bytes) notneeded
- [ ] 013bd2 seg005_10B2_1D82 (125 bytes) notneeded
- [ ] 013c4f HexToDecimalString_seg005_10B2_1DFF (39 bytes) notneeded
- [ ] 013c76 seg005_10B2_1E26 (26 bytes) notneeded
- [ ] 013c90 seg005_10B2_1E40 (37 bytes) notneeded
- [ ] 013cb5 seg005_10B2_1E65 (33 bytes) notneeded
- [ ] 013cd6 seg005_10B2_1E86 (40 bytes) notneeded
- [ ] 013cfe SetArrayValues_seg005_10B2_1EAE (25 bytes) notneeded
- [ ] 013d17 seg005_10B2_1EC7 (22 bytes) notneeded
- [ ] 013d2d CopyDataBySize_seg005_1EDD (34 bytes) notneeded
- [ ] 013d4f seg005_10B2_1EFF (25 bytes) notneeded
- [ ] 013d68 seg005_10B2_1F18 (18 bytes) notneeded
- [ ] 013d7a CreateOrOpenFile_seg005_1F2A (315 bytes) notneeded
- [ ] 013eb5 seg005_10B2_2065 (76 bytes) notneeded
- [ ] 013f01 MaybeFileSeek_seg005_20B1 (187 bytes) notneeded
- [ ] 013fbc seg005_10B2_216C (26 bytes) notneeded
- [ ] 013fd6 seg005_10B2_2186 (31 bytes) notneeded
- [ ] 013ff5 seg005_10B2_21A5 (20 bytes) notneeded
- [ ] 014009 seg005_10B2_21B9 (117 bytes) notneeded
- [ ] 014488 seg005_10B2_2638 (43 bytes) notneeded
- [ ] 0144b3 seg005_10B2_2663 (383 bytes) notneeded
- [ ] 014632 seg005_10B2_27E2 (24 bytes) notneeded
- [ ] 01464a seg005_10B2_27FA (28 bytes) notneeded
- [ ] 014666 seg005_10B2_2816 (19 bytes) notneeded
- [ ] 014679 seg005_10B2_2829 (19 bytes) notneeded
- [ ] 01468c seg005_10B2_283C (44 bytes) notneeded
- [ ] 0146b8 seg005_10B2_2868 (7 bytes) notneeded
- [ ] 0146bf seg005_10B2_286F (155 bytes) notneeded
- [ ] 01475a CombineStrings_seg005_10B2_290A (57 bytes) notneeded
- [ ] 014793 seg005_10B2_2943 (54 bytes) notneeded
- [ ] 0147c9 StoreStringInLocation_seg005_2979 (36 bytes) notneeded
- [ ] 0147ed StringLen_seg005_10B2_299D (26 bytes) notneeded
- [ ] 014807 seg005_10B2_29B7 (44 bytes) notneeded
- [ ] 014833 ToUpperCase_seg005_10B2_29E3 (32 bytes) notneeded
- [ ] 014853 seg005_10B2_2A03 (385 bytes) notneeded
- [ ] 0149d4 seg005_10B2_2B84 (226 bytes) notneeded
- [ ] 014aff seg005_10B2_2CAF (22 bytes) notneeded
- [ ] 014b15 seg005_10B2_2CC5 (7 bytes) notneeded
- [ ] 014b1c seg005_10B2_2CCC (9 bytes) notneeded
- [ ] 014b25 seg005_10B2_2CD5 (8 bytes) notneeded
- [ ] 014b2d seg005_10B2_2CDD (26 bytes) notneeded
- [ ] 014b47 seg005_10B2_2CF7 (13 bytes) notneeded
- [ ] 014b54 seg005_10B2_2D04 (9 bytes) notneeded
- [ ] 014b5d seg005_10B2_2D0D (1175 bytes) notneeded
- [ ] 014ff4 seg005_10B2_31A4 (120 bytes) notneeded
- [ ] 01503d seg005_10B2_31ED (47 bytes) 
- [ ] 01506c seg005_10B2_321C (98 bytes) notneeded
- [ ] 0150ce seg005_10B2_327E (113 bytes) notneeded
- [ ] 01513f seg005_10B2_32EF (41 bytes) notneeded
- [ ] 015168 seg005_10B2_3318 (54 bytes) notneeded
- [ ] 01519e seg005_10B2_334E (41 bytes) notneeded
- [ ] 0151c7 seg005_10B2_3377 (97 bytes) notneeded
- [ ] 015228 seg005_10B2_33D8 (57 bytes) notneeded
- [ ] 015261 seg005_10B2_3411 (35 bytes) notneeded
- [ ] 015284 seg005_10B2_3434 (125 bytes) notneeded
- [ ] 015301 seg005_10B2_34B1 (122 bytes) notneeded
- [ ] 01537b seg005_10B2_352B (98 bytes) notneeded
- [ ] 01544b MultiplyDXCXAXBX_seg005_35FB (23 bytes) notneeded
- [ ] 015462 seg005_10B2_3612 (33 bytes) notneeded
- [ ] 015483 seg005_10B2_3633 (3 bytes) notneeded
- [ ] 015486 Divide32BitNumbers_seg005_3636 (7 bytes) notneeded
- [ ] 01548d Divide32BitNumbers_seg005_363D (5 bytes) notneeded
- [ ] 015492 seg005_10B2_3642 (11 bytes) notneeded
- [ ] 01549d seg005_10B2_364D (151 bytes) notneeded
- [ ] 015534 BitShiftL_AXDX_BY_CX_seg005_10B2_36E4 (33 bytes) notneeded
- [ ] 015555 seg005_10B2_3705 (32 bytes) notneeded
- [ ] 015575 BitShiftRight_seg005_10B2_3725 (30 bytes) notneeded
- [ ] 015593 seg005_10B2_3743 (96 bytes) notneeded
- [ ] 0155f3 seg005_10B2_37A3 (43 bytes) notneeded
- [ ] 01561e CopyArray_seg005_10B2_37CE (34 bytes) notneeded
- [ ] 015640 seg005_10B2_37F0 (29 bytes) notneeded
- [ ] 01565d seg005_10B2_380D (58 bytes) notneeded
- [ ] 015697 seg005_10B2_3847 (57 bytes) notneeded
- [ ] 0156d0 seg005_10B2_3880 (28 bytes) notneeded
- [ ] 0156ec seg005_10B2_389C (35 bytes) notneeded
- [ ] 01570f seg005_10B2_38BF (96 bytes) notneeded
- [ ] 01576f seg005_10B2_391F (64 bytes) notneeded
- [ ] 0157af seg005_10B2_395F (41 bytes) notneeded
- [ ] 0157d8 seg005_10B2_3988 (25 bytes) notneeded
- [ ] 0157f1 seg005_10B2_39A1 (53 bytes) notneeded
- [ ] 015826 seg005_10B2_39D6 (66 bytes) notneeded
- [ ] 015868 seg005_10B2_3A18 (2 bytes) notneeded
- [ ] 0158bb seg005_10B2_3A6B (23 bytes) notneeded
- [ ] 0158d2 seg005_10B2_3A82 (33 bytes) notneeded
- [ ] 0158f3 seg005_10B2_3AA3 (4 bytes) notneeded
- [ ] 0158f7 seg005_10B2_3AA7 (4 bytes) notneeded
- [ ] 0158fb seg005_10B2_3AAB (4 bytes) notneeded
- [ ] 0158ff seg005_10B2_3AAF (149 bytes) notneeded
- [ ] 016097 SetALto0_seg006_1477_5F7 (7 bytes) 
- [ ] 0672c0 arc_buffer (49 bytes) 
- [ ] 068a0d ovr093_174D (33 bytes) 
- [ ] 06b403 do_decline_ovr095_E73 (148 bytes) 
- [ ] 06b497 RedisplayBarterSlots (469 bytes) 
- [ ] 084892 ovr153_D92 (241 bytes) 

