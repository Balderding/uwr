# Smart-hunting on by default, off without the model files, a model picker: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. One commit (two repos), run-tests.cmd's verdict quoted.
> Builder: the uwr session, after the hunters' Task 3. Reviewer: the second uwr session's loop.

**Goal:** Smart-hunting (and with it the model) is on by default and stays on, whatever the plain build writes into the shared enhance= line; it turns itself off when the llm\ directory or its files are missing; our Esc menu has a "Language model" row listing the .gguf files found under llm\models, and the first start with more than one model opens that row once so the player picks.

**Architecture:** uw-new owns its own setting, `smart-hunting=0|1` in uw1port.cfg (absent = on), decided in main.c after enhance_load regardless of the enhance= line's token; the model files are checked there too. The model choice is `llm_model=<file>` (a .gguf name under llm\models, any subfolder; the old values `lfm` / absent keep their meaning) read by uwmindhost.c. The Esc menu row cycles the files found at start (a directory scan, once).

**Spec:** the user, 2026-11-12: "smart-hunting should be on by default but if the llm directory or files aren't there turn it off, also you might be able to add in the first time start a menu item to select the llm that is in the llm directory".

## Global Constraints
- Off in hidden runs and replays as today (the --hidden branch does not add it); UWPORT_LLM=0 still keeps the model off; proofs unchanged.
- The plain uw-original.exe is untouched (UWPORT_PLAIN keeps the original's behaviour); the shared enhance= line may say what it likes.
- No change to the game's rules; the Esc menu row is our display's only.

## Review Focus
1. A home whose enhance= line lacks smart-hunting after a UW-Original.cmd run: uw-new must still play with it on (the regression this fixes). Test in Step 1.
2. llm\models empty or llm\bin\uwmind.dll missing: smart-hunting off, one log line, no model load attempt, no menu row. Test in Step 1.
3. `smart-hunting=0` written by the menu: stays off across runs even with the files present. Test in Step 1.
4. A model file name with spaces or in a subfolder: the picker shows the file name, llm_model stores the path relative to llm\models, uwmindhost resolves it. Test in Step 1.
5. The first-start prompt must not appear in hidden runs, in replays, or when only one model exists. Test in Step 1.

### Task 1: the setting, the file check, the picker

**Files:**
- Modify: `underworld-exhumed/uw1/src/port/sys/main.c` (the default-on block from b672973: replace the "no enhance= line" test), `port/win32/uwmindhost.c` (uo-new: llm_model as a file; the scan for the menu), `port/win32/plat_win32.c` (the menu row MO_LLM; the first-start opening), `tools/makedist.py` (README line), `LLM_NPC_AI.md`.
- Test: `tools\llmcheck.py` (uo-new), hidden runs with UWPORT_LLM=0 asserting the stderr lines; the menu row by a scripted Esc + keys in a hidden run with `UWPORT_HUNTER_PLAN` unset.

**Interfaces:**
```c
/* uwmindhost.c */
int  port_mind_models(const char **names, const char **paths, int max);   /* the .gguf files under <exe>\llm\models (one level of subfolders), sorted; 0 when none or no llm\bin\uwmind.dll */
void port_mind_pick(const char *path);     /* writes llm_model=<path relative to llm\models> and makes the next load use it (the model let go if loaded) */
const char *port_mind_current(void);       /* the file name in use (for the row) */
/* main.c: after enhance_load, in place of the enhance-line test */
if (port_config_get(home, "smart-hunting", v, sizeof v) != 0 || atoi(v) != 0) {
    if (port_mind_models(NULL, NULL, 0) > 0) m |= 1u << ENH_SMART_HUNTING;
    else fprintf(stderr, "uw1port: smart-hunting off: no language model under llm\\ (llm\\bin\\uwmind.dll and llm\\models\\*.gguf)\n");
} else m &= ~(1u << ENH_SMART_HUNTING);
```
- Consumes: port_config_get / port_config_set (the runtime), the Esc menu's row table (plat_win32.c menu_rows_build / menu_choose_step, the `R[]` and `OPT[]` tables: add MO_LLM after MO_MOUSE with opt -1; its value port_mind_current(); menu_choose_step cycles through port_mind_models and calls port_mind_pick), the pause the menu already does (port_pause).

- [ ] **Step 1: Write `tools\llmcheck.py`** with five hidden runs (each from a fresh copy of work\in2-4, UWPORT_LLM=0 so no model loads; the checks read stderr):
  a. the home's enhance= line edited to `mouse-look,modern-keys` (as UW-Original.cmd leaves it): assert `enhance: ... smart-hunting` in the enhancements line;
  b. a copy of dist\UW1 with `llm\models` emptied, run from there: assert `smart-hunting off: no language model`;
  c. `smart-hunting=0` in uw1port.cfg: assert the enhancements line lacks smart-hunting;
  d. a model file copied to `llm\models\sub dir\My Model.gguf` and `llm_model=sub dir\My Model.gguf`: assert `AI: loading the language model My Model.gguf` (UWPORT_LLM unset for this one; the load may fail on the fake file: the line is what is asserted);
  e. the first start (no llm_model key, two models present) in a hidden run: assert the menu did NOT open (no `our menu` line) and that an interactive flag would: `port_mind_first_start()` returns 1 (log `AI: first start: pick the language model` printed only when interactive).
  Expected now: a, b, c, e FAIL.
- [ ] **Step 2: Implement** main.c's block, port_mind_models (FindFirstFileA over llm\models and its subfolders, `.gguf` only, names sorted with strcmp), port_mind_pick (port_config_set llm_model; if the worker holds a model: set state 5 so the next question reloads), uwmindhost's path resolution (`llm_model` = `lfm` → the LFM file as today; else if it names an existing file under llm\models → that; else the Qwen default), the menu row (label "Language model", value the file name, left/right or Enter cycles), and the first start: when interactive, smart-hunting on, no llm_model key, and port_mind_models > 1: at the first frame of play open our menu with menu_sel on the row and a hint line "Choose the language model (Enter keeps this one)"; any choice (or Esc) writes llm_model so it never asks again.
- [ ] **Step 3: Run llmcheck.py: all PASS. Run run-tests.cmd: all tests passed.**
- [ ] **Step 4: makedist's README line ("Smart-hunting and the language model are on; Esc menu: Language model picks the file under llm\models; smart-hunting=0 in home\uw1port.cfg turns it off"); LLM_NPC_AI.md paragraph; commit both repos with the verdicts; dist refreshed.**
