# uo-new: rules for every session

Ultima Underworld rebuilt in plain C (clang) with a D3D12 renderer (DLSS, DLSS 5, ray tracing). The game's rules
must be EXACTLY UW.EXE's (mechanics, numbers, rolls, the clock and frame pace). Everything else may be ours when
it works better: graphics, menus, screens, cutscenes, panels, the cursor, file reading, the presentation's
scheduling; and the original's routines may be modified to work right with ours (the user, 2026-11-12: the limit
on what we replace is lifted; the real backend, plat_win32.c, stays). A replacement is off in every proof and
leaves the replays byte-identical when off. work\REVIEW-2026-11-12-replaceable.md sorts the code into mechanics
and replaceable parts. Status and next steps: NOTES.md. The routine checklist: ROUTINES.md (made by
tools\checklist.py).

## Direction (the user, 2026-11-09)

"Continue with what we have and reference DECOMP_RULESET.md for direction. We gotta make more progress: almost every
time we work on something it is half done. We need the routines done, everything done; then we can continue with
modifications that make the game easier and more fun to play."

So: **finish before adding.** Work through ROADMAP.md subsystem by subsystem; a subsystem is done only when every
routine in it is ported, its quirks reproduced, and it is proved (or, where no verifier is possible yet, listed in
NOTES.md with exactly why). No new comforts, options or features until the rules are complete, unless the user asks.
Do not leave a subsystem half done to start another; if blocked, say so and record precisely what remains.

## The ruleset: DECOMP_RULESET.md, as this project applies it

DECOMP_RULESET.md is a ruleset for a *byte-matching* decompilation (rebuilding the DOS UW.EXE with Borland C). That
job is already finished for this exact file by underworld-exhumed (O:\uw\uo-new\underworld-exhumed, MIT, UW.EXE MD5
8afc3c29ef5667a3ab6667e8cbc8d63a = ours). We do not rebuild the DOS program; we follow the ruleset's discipline with
**emulator proof in place of the byte diff**:

**Baseline (never changed):** game\UW.EXE, 547248 bytes, MD5 8afc3c29ef5667a3ab6667e8cbc8d63a, SHA-1
226fed4dba9718a7c972de5451b520fcd16546c0. The original files, the verifiers' comparisons and the expected values are
read-only: change our code to fit the original, never the check.

**Terms (use them consistently):**

| Term | Meaning here |
| --- | --- |
| proved | our C routine run beside the original routine (Unicorn emulator, tools\emu.py) on the same inputs gives the same results, calls and state: listed in re\proved.txt with its verifier (the ruleset's "matching") |
| unproved | our source ports it (names its address) but no verifier compares it yet: a claim, never reported as done |
| replaced | graphics, sound output, input, interface: our engine's own way (allowed: creature comforts) |
| notneeded | DOS, memory, overlays, hardware, C runtime |
| todo | not ported |

**Three overriding rules:**
1. Never change the target, a verifier's comparison, or an expected value to make a check pass.
2. Never claim progress without the verifier's verdict. Quote it (e.g. "songs 4365 operations same") in the commit
   message and NOTES.md. "Looks right" or "reads the same" is not proof.
3. tools\verify.cmd (inside run-tests.cmd) passes at every commit. A commit that breaks it is reverted, not fixed later.

**The per-routine loop (the ruleset's phase 5):**
1. Pick a routine from ROUTINES.md (leaf routines and callees first; finish a segment when possible).
2. Read the original: underworld-exhumed's matched C for it (uw1\src; find it by address in
   uw1\map\crosswalk.tsv: listing address segNNN_XXXX / ovrNNN_XXXX, or grep the name) AND the disassembly
   (tools\dis16.py). Their C is the exact specification (it compiles to our UW.EXE byte for byte). Their code is MIT
   (the user, 2026-11-09: yes, use it): adapt it straight into ours where that is quickest (their far pointers and
   structs become our data), cite "underworld-exhumed FILE.C:line" in a comment, and keep THIRD-PARTY-NOTICES.md.
   Adapted code is still proved in the emulator before it counts.
3. Port it into our code, reproducing every quirk and bug of the original (document each; never "fix" behaviour).
4. Prove it: run the original routine in the emulator beside ours on many random and edge inputs (extend
   tests\rtest.c and a tools\*_verify.py; add the verifier to tools\verify.cmd). Iterate until identical.
5. Add the routine to re\proved.txt, regenerate ROUTINES.md
   (`uv run --no-project --with capstone python -I tools/checklist.py game/UW.EXE re/uw_map.txt re/routines_status.txt src ROUTINES.md`),
   run run-tests.cmd, commit with the verdict in the message.
6. Stuck: keep it unproved, write in NOTES.md exactly what differs, move on.

Anything of ours that stands in for a part not ported yet (e.g. our creature AI, our movement speed) is labelled
in the code and in NOTES.md as "ours until the X port"; it is replaced when that part is proved.

**Conduct:** one routine at a time; record guesses with their confidence, never as fact; prefer evidence from the
binary over assumptions; ask the user before changing a verifier, the emulator harness, or anything every check
relies on; commit after each proved routine.

**Session start:** read NOTES.md and the head of ROUTINES.md; run run-tests.cmd (it runs tools\verify.cmd) and
confirm "all tests passed" before changing anything.

**Session end:** run run-tests.cmd, regenerate ROUTINES.md, update NOTES.md (what was learned, dead ends), and list
the next three routines to attempt.

## The rules core (from 2026-11-09): how routines are ported now

Hand-porting routine by routine was too slow for the user's "these need to be done today". The rules are now
underworld-exhumed's byte-matched C **compiled unchanged** (tools\core.py: its game-logic files, plus the sys and ui
helpers that are rules or loaders, through Exhume's portability headers in external\exhume; -mno-ms-bitfields for
Turbo C's layout) into build\core\core.a. What it needs from outside is src\core_glue.c (Borland's rand, file calls
rooted at the game folder with writes kept apart, DOS reads, the far heap, MODEX.ASM's string helpers, mouse_getxy),
src\imath.c (IMATH.ASM's sin/cos/atan2/sqrt with the original tables) and generated stubs (build\gen\core_stubs.c:
the front end's screen, sound and input, each still to provide). underworld-exhumed's files are never edited: a
host-ABI or uninitialised-local fix is a PATCHES entry in tools\core.py, applied to a copy, documented there.

**Proof:** tools\coreproof.py drives the same operations on UW.EXE in the emulator and on the core
(build\coretest.exe) and compares everything: returns, the whole level block, named globals, rand's state, and the
outside calls (with text and sound arguments, build\gen\core_sinks.txt). One script per source file:
tools\proof_<file>.py, run by tools\proofs.cmd; each reports which of the file's routines the original actually ran
(coverage) and writes work\ran\<script>.txt; tools\proved.py adds those to re\proved.txt after a passing run.
A difference is fixed on our side (harness input, glue, a PATCHES entry), never by changing the comparison.
The core may be rebuilt while proofs run: work\rebuild_core.sh renames a running build\coretest.exe aside.

**Next:** the game (uw.exe) runs on the core: the original's world step is PLAYMOVE.C's check_physics (player,
critters through move_mobile, effects, sound); the front end feeds GAME_TIME (core_time), the player's input
(PlayerInput/PlayerTurn as parse_playin sets them) and the mouse, implements the stubs, and draws from mapdata.

## Standing rules from the user (all sessions)

- End every update with ONLY the full-path exe command, no quotes, in one code block: `X:\uwr\dist\uwr\uwr.exe`
- Never delete anything without asking; only create/add (since 2026-11-22: unused old scripts and test output may go,
  by the safe-delete steps: references first, quarantine in X:\_trash\uwr\<date>, prove, then empty).
- Nothing on C:. Since 2026-11-22 the project is X:\uwr (was O:\uw\uo-new). The compilers and SDKs are in
  X:\tools\<tool>, read from ONE variable, UWR_TOOLS (default X:\tools: tools\uwr_env.cmd and TOOLS in the build
  scripts); everything of ours is found relative to the script. Scratch and test files go in X:\uwr\work (gitignored).
- The original game files are in X:\uw-gamedata, read-only; all our work goes in X:\uwr. Never the GOG install. No DOSBox.
- Headless tests only, no focus stealing. Never kill the user's running uw.exe: build with `build.cmd new` if locked.
- Never download Roland ROMs; the user's own are in uo-new\roms; never commit roms\.
- Python tools run with uv (`UV_CACHE_DIR=O:\appsnotonc\sentinel\uv\cache`, `python -I`); pin unicorn==2.0.1.post1.
- Shell heredocs mangle backslashes: write patch scripts with the editor into work\, or edit with the Edit tool.
