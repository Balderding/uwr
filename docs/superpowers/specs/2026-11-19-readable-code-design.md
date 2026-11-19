# Readable code, logging and error handling (uwr's own code): design

2026-11-19. Approved direction from the user: make our code more understandable, add logging and error handling.
No object orientation (dropped by the user). Never touch the rules code.

## Why

The code works but is hard to follow: a few very long functions, comments that tell history instead of purpose,
and no single page that shows how the parts fit. Failures often leave no trace in uwr.log, so problems take
guessing to track down. The game must look and play exactly the same afterwards.

## Goals

1. A code map anyone can start from.
2. The longest functions broken into short, well-named steps.
3. Comments that say plainly what the code does and why (history lives in git).
4. uwr.log lines that say how serious, where and when; every failure in our code logged with its cause.

## Non-goals / anti-goals

- No object orientation, no C++, no new libraries or build steps. Plain C17 on clang, as now.
- The rules code is never touched: underworld-exhumed's tree (combat, critter, motion, obj, event, conv VM, game,
  inv data, map, the clock and frame pace) and anything that carries a mechanic. If a function's job is in doubt,
  leave it.
- No behaviour change. A refactor that changes what the player sees, hears or plays is a failure even if it
  compiles. Same statements, same order, same thread; only moved into named functions.
- The old standalone engine (src/game.c and the uw.exe-only files) is out of scope: uwr does not use it.

## Scope (uwr's own code only)

- port/win32: uwfront.c, uwaudio.c, uwinput.c, uwinv.c, uwconv.c, uwlook.c, uwsteam.c, uwacoustic.c, uwgame.c
  (the wrappers only, not what they wrap), uwlog.c, plat_win32.c (comments and logging only: it is the real
  backend and stays as it is in structure).
- port/sdl3: plat_sdl3.c, uwmenu.c (same rules as plat_win32.c).
- the renderer DLL: src/uwview.c, render.c, render_vk.c, render_*.h/.c, menu_draw.c, ui.c, base.c (the logger).

## The work, in order

1. **Code map** (docs/CODE_MAP.md): each file's job in a line or two; how a frame flows (input -> game thread
   -> picture -> uwview.dll -> screen; sound); where the rules code is and why it is off-limits; how it is built
   (tools\portwin32.py, buildview.cmd, UWR_AREA).
2. **Logging** in base.c / a small shared header used by port/ and src/:
   `log_msg(level, tag, fmt, ...)` with levels error / warn / info / debug, each line
   `[12.4s] warn render: ...`. `logf_` stays and logs as info, so existing calls keep working. The level kept is
   set by an Esc-menu row (default info), with its plain-words hint and a docs/ESC_MENU.md entry.
   `fatal()` writes file and line to uwr.log before it exits.
3. **Error handling**: every file open/read/write and allocation in the scoped files logs what failed, the path
   and the reason (errno / GetLastError text). Where a sensible fallback exists (a missing texture -> the
   original art), use it and carry on; otherwise keep today's behaviour, now logged.
4. **Long functions**, longest first: uwv_option (452 lines), render_frame (335), view_present (236),
   menu_choose_step (183), uwaudio_event (169), uwv_frame (167), then any other over ~150 lines in scope. Each
   becomes a short top-level function calling named steps.
5. **Comments**: per file as it is worked on; a header saying what the file is for, and per function what and
   why. Dates and "moved from" notes go.

## Checks

One change at a time; each builds in a private area (UWR_AREA=readable) and the exe launches hidden
(UWPORT_OFFSCREEN=1). No other tests (the user checks the rest). Each step committed by explicit path.

## Approaches considered

- Object-style C or a C++ move: dropped by the user.
- Logging and error handling only: safe but leaves the code as hard to follow; the user asked for readability.
- Chosen: code map + logging + error handling + breaking up long functions + plain comments, in that order.

## Open for the user

- Other sessions have uncommitted changes in many scoped files (uwfront.c, uwview.c, uwaudio.c and 14 more).
  How to proceed is the user's call (see the handover message).

## Left to the builder

Names of the new functions, the exact split points, the log tags (one per file is the default), which failures
have a real fallback.
