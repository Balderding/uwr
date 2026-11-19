# Readable code, logging and error handling: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** make uwr's own code easier to understand and its failures visible in uwr.log, with no change in how the game behaves.

**Architecture:** one small shared logger (port/win32/rlog.c) compiled into both uw1port.exe and the renderer DLLs; then file by file: break long functions into named steps, rewrite comments in plain words, route the file's messages through the logger, and log every failure with its cause.

**Tech Stack:** plain C17, clang 23 (MinGW target), tools\portwin32.py build (Python 3.13), Win32 + SDL3 backends.

**Spec:** docs/superpowers/specs/2026-11-19-readable-code-design.md

## Global Constraints

- NEVER touch the rules code: anything under underworld-exhumed\ (combat, critter, motion, obj, event, conv VM, game, inv data, map, clock/pace) and any function that carries a mechanic. In doubt: leave it, note it in the report.
- No behaviour change: same statements, same order, same thread; only moved into named functions, renamed locals, comments, and added log lines / error checks. Never change a number, a condition, or call order.
- Plain C17, no new libraries, no OS headers in rlog.c/rlog.h (it must also build for Linux).
- **Other sessions' work:** before editing a file run `git status --short <file>`. If it shows changes, do NOT edit it; report it as blocked. Commit only by explicit path (`git commit <paths>`), never `git add -A` / `git commit -a`.
- Never delete files. Never write the user's real name; author attribution is "balderding".
- Text in docs: plain words. No red colours anywhere.
- No tests other than: it compiles, and the exe launches hidden. No run-tests.cmd / verify.cmd.
- Commit messages end with: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`

## Build and launch check (used by every code task)

Each agent uses its own area name, given in its task (`readable1`, `readable2`).

```bash
cd /o/uw/uo-new
PY=/o/appsnotonc/sentinel/uv/python/cpython-3.13.17-windows-x86_64-none/python.exe
UWR_AREA=readable1 $PY -I tools/portwin32.py 2>&1 | tail -20        # expect no "error:"; exit 0
[ -d work/readable/home-readable1 ] || cp -r work/in2-4 work/readable/home-readable1
UWPORT_OFFSCREEN=1 underworld-exhumed/uw1/build/port-readable1/uw1port.exe --data dist/UW1/game \
  --home work/readable/home-readable1 --exit-after 12000 > work/readable/launch-readable1.txt 2>&1; echo exit=$?
grep -niE "fatal|error" work/readable/launch-readable1.txt | head
```
Passed = build exit 0 with no `error:` lines, launch `exit=0`, no new fatal/error lines compared with the run before the change (run the launch once before editing to have that baseline). Never run it visible.

## Review Focus

- A split function that changes order or drops a statement (e.g. an early `return` that used to skip the rest now only leaves the helper): reviewer compares old vs new statement by statement.
- A local used after the split point (state that must be passed to or returned from the helper).
- Logging inside per-frame code floods uwr.log: anything per frame logs at debug only, or once (a `static int said` guard).
- An error check that changes the result (e.g. returning early where the old code carried on): add the log line, keep the old flow unless the task names a fallback.
- rlog.c used from two threads (game thread + render thread): one `fprintf` call per line so lines do not interleave mid-line.

---

### Task 1: Code map (agent A, docs only, runs alongside Task 2)

**Files:** Create: `docs/CODE_MAP.md`

- [ ] **Step 1:** Read tools\portwin32.py, tools\buildview.cmd, port\README.md, port\win32\*.h, the head comment of every port\win32\*.c, port\sdl3\*.c, src\uwview.c, src\render*.c, src\menu_draw.c, src\ui.c, and work\REVIEW-2026-11-12-replaceable.md section 1.
- [ ] **Step 2:** Write docs/CODE_MAP.md with: (1) what uwr is made of: underworld-exhumed's game (the rules, off-limits) + our backend + front end + renderer DLL; (2) a table: file -> its job in one or two lines, for every .c in port\win32, port\sdl3 and the renderer DLL's sources; (3) how one frame flows (input -> uwinput -> game thread -> picture -> uwview.dll / uwview_vk.dll -> screen; sound: uwaudio, uwsteam, uwacoustic); (4) the rules code: where it lives, the list from REVIEW section 1, and why it is never edited; (5) how to build (portwin32.py, UWR_AREA, buildview.cmd, makeflat.py) and where uwr.log is. Plain words, no history.
- [ ] **Step 3:** Check every file path in the doc exists (`ls` each).
- [ ] **Step 4:** Commit: `git commit docs/CODE_MAP.md -m "CODE_MAP.md: which file does what, how a frame flows, where the rules code is"` (add the file first by path).

### Task 2: The shared logger (agent B, area readable1)

**Files:**
- Create: `port/win32/rlog.h`, `port/win32/rlog.c`
- Modify: `tools/buildview.cmd` (add `port\win32\rlog.c` and `-I port\win32` to the three DLL lines: uwview.dll, uwview_vk.dll, uwtour.exe; via one `set RLOG=...` variable)
- Modify (logging only): `port/win32/uwinv.c`, `port/win32/uwlook.c`, `port/win32/uwconvrec.c`, `port/win32/uwacmat.c`, `port/win32/uwgame.c` (each must be clean in git status)

**Interfaces (Produces, used by every later task):**
```c
enum { RLOG_ERROR = 0, RLOG_WARN = 1, RLOG_INFO = 2, RLOG_DEBUG = 3 };
void rlog(int level, const char *tag, const char *fmt, ...);      /* one line: "[HH:MM:SS.mmm] warn  tag: text\n" to stderr */
void rlog_set_level(int level);                                    /* lines above this level are dropped; default RLOG_INFO */
int  rlog_level(void);
const char *rlog_errno(void);                                      /* strerror(errno) text, for "cannot open %s: %s" */
#define RLOG_E(tag, ...) rlog(RLOG_ERROR, tag, __VA_ARGS__)
#define RLOG_W(tag, ...) rlog(RLOG_WARN,  tag, __VA_ARGS__)
#define RLOG_I(tag, ...) rlog(RLOG_INFO,  tag, __VA_ARGS__)
#define RLOG_D(tag, ...) rlog(RLOG_DEBUG, tag, __VA_ARGS__)
```
- The line is formatted into one buffer (1024 bytes, truncated with "...") and written with ONE `fputs` + `fflush(stderr)`, so threads never interleave mid-line. Time from `timespec_get` (C11) + `localtime`, wall clock, so the exe and the DLL agree.
- A message that already ends in `\n` gets no second one.
- Level at start: env `UWR_LOGLEVEL` (error|warn|info|debug or 0-3), else info.
- The DLL and the exe each hold their own level; that is fine (both read the same env var).

- [ ] **Step 1:** Baseline launch check (section above) before any edit; keep the output.
- [ ] **Step 2:** Write rlog.h / rlog.c as above (head comment: what it is, one line per function).
- [ ] **Step 3:** Hook buildview.cmd; build. Expected: builds.
- [ ] **Step 4:** In the five files above, turn each `fprintf(stderr, ...)` into the matching `rlog` call with the file's tag (`inv`, `look`, `convrec`, `acmat`, `game`); keep the text; pick the level by meaning (failures error/warn, normal notes info, chatty per-frame notes debug). Keep `printf` that is not a log line (e.g. writing a file) as it is.
- [ ] **Step 5:** Build + launch check. Expected: exit=0, rlog lines visible in launch output with the new prefix.
- [ ] **Step 6:** Commit by path: rlog.h rlog.c buildview.cmd and the five files: "rlog: one shared logger with levels, tags and times (exe and renderer DLLs); five front-end files use it".

### Task 3: render.c (area readable1, after Task 2)

**Files:** Modify: `src/render.c` (must be clean)
- [ ] **Step 1:** Baseline launch check.
- [ ] **Step 2:** Split `render_frame` (335 lines) into a short top-level function calling named static steps in the same order (one per pass it does: e.g. setup, scene, RT, effects, UI, present: names by what the code does). Pass state as parameters or a local `struct` on the stack; no new globals.
- [ ] **Step 3:** Every other function over ~150 lines in the file: same treatment.
- [ ] **Step 4:** `logf_` -> `rlog` with tag `render` (include rlog.h); every D3D12 call that returns HRESULT and is not checked yet: log `"%s failed: 0x%08lx"` at error, keep the old flow.
- [ ] **Step 5:** Comments: file head says what the file does; each function one or two lines of what and why; drop dates and "moved from" notes.
- [ ] **Step 6:** Build + launch check; commit by path.

### Task 4: small clean front-end files (area readable1)

**Files:** Modify (each only if clean): `port/win32/uwinput.c`, `port/win32/uwconv.c`, `port/win32/uwacoustic.c`, `port/win32/uwlog.c`, `port/win32/uwmindhost.c`, `port/win32/uwinv.c`, `port/win32/uwlook.c`
- [ ] **Step 1:** Baseline launch check.
- [ ] **Step 2:** Per file: plain comments (Task 3 step 5), functions over ~150 lines split (Task 3 step 2), every fopen/fread/fwrite/malloc/calloc/realloc result checked and a failure logged as `RLOG_E(tag, "cannot open %s: %s", path, rlog_errno())` (or "out of memory (%zu bytes)") keeping the old flow; uwmindhost.c's printf log lines -> rlog tag `mind`.
- [ ] **Step 3:** Build + launch check; commit by path, one commit per file.

### Task 5: renderer helpers (area readable1)

**Files:** Modify (each only if clean): `src/menu_draw.c`, `src/ui.c`, `src/uwinv_draw.c`, `src/uwconv_draw.c`, `src/smoothtext.c`
- [ ] Same steps as Task 4 (tags `menu`, `ui`, `invdraw`, `convdraw`, `text`); a missing font or picture file falls back to what the code does today, now logged at warn.

### Task 6: plat_sdl3.c (area readable1)

**Files:** Modify: `port/sdl3/plat_sdl3.c`, `port/sdl3/uwmenu.c` (only if clean)
- [ ] Comments and logging only (it is a backend: no restructuring). SDL failures logged with `SDL_GetError()`. Build check: `UWR_AREA=readable1 $PY -I tools/portwin32.py --backend sdl3` compiles. Commit by path.

### Tasks 7-10: blocked until the other sessions commit (check git status first; if still dirty, report blocked and stop)

- **Task 7 uwfront.c:** split `view_present` (236) and `menu_choose_step` (183) and any other >150; its 48 `fprintf(stderr)` -> rlog tag `front`; comments.
- **Task 8 uwview.c:** split `uwv_option` (452: one step per Esc tab / row group) and `uwv_frame` (167); 38 fprintf + log printf -> rlog tag `view`; comments.
- **Task 9 uwaudio.c + uwsteam.c:** split `uwaudio_event` (169); fprintf -> rlog tags `audio`, `steam`; comments.
- **Task 10 log level row + fatal + plat_win32.c:** an Esc-menu row "Log detail" (Errors only / Warnings / Normal / Everything; default Normal) calling `rlog_set_level` in the exe and, through the existing option channel, in the DLL; its plain-words hint; docs/ESC_MENU.md entry. `fatal()` in src/base.c/.h gets the caller's file and line via a macro (`#define fatal(...) fatal_at(__FILE__, __LINE__, __VA_ARGS__)`) and logs through rlog at error before exiting. plat_win32.c: comments + fprintf -> rlog tag `win32`, no restructuring.

Each: baseline launch, the change, build + launch check, commit by path.

## Execution

Task 1 and Task 2 run at the same time (two agents; Task 1 writes no code). Tasks 3-6 run one after another (they share one working tree and one build: two agents compiling at once would see each other's half edits). Each task gets a fresh review agent (Opus, medium) that compares old vs new statement by statement for behaviour change before the next starts.
