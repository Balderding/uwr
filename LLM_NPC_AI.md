# LLM-driven creatures — idea and plan (talked through 2026-11-09/10, NOTHING BUILT)

The user asked: could a language model control monsters and friendly NPCs in the port in close to real time,
so a creature could, say, work out how to hunt the player down. This file records the answers and the plan so
the next agent doesn't re-derive them. **Nothing below exists in code yet.**

## Order of work (the user's rules come first)
1. Port the original creature AI first, routine by routine, proved against UW.EXE (DECOMP_RULESET.md,
   "finish before adding"). The goal system it brings is what the model plugs into.
2. Only then the LLM, as an **optional mod**: a setting such as `ai = original | llm`, off by default and
   **always off in tests and proofs** (model output is not repeatable).

## Design
- **The model decides; it never drives.** Movement, pathfinding, combat rolls and animation stay in the port's
  C code, as the original does them. The model only picks what a creature does next, about 1-2 decisions a
  second or on events (heard a noise, got hurt, lost sight of the player, an ally died).
- **Plug into the original goal system.** UW1 creatures have a goal and a target (wander, stand, follow,
  attack, flee, go to a place, ...). Routines named in ROADMAP.md include `critter_set_goal`,
  `critter_discard_goal`, `change_critter_goal`, `set_attitude`, `set_race_attitude`. The model returns a goal
  and target from that list; the engine checks it and carries it out with the original's code. A bad or late
  answer is ignored.
- **Never block a frame.** The model runs on its own worker thread. Until its answer arrives, the original AI
  keeps the creature going.
- **Fixed answer format.** llama.cpp can force the reply into a fixed format (a JSON schema or a grammar), e.g.
  `{"creature":12,"goal":"ambush","target":[x,y]}`, so even a small model can only give a legal answer.
- **No cheating.** Give the model only what that creature could know: whether it saw the player's light or
  heard a fight or a door, where it last saw the player, its own health, which allies are near. Not the
  player's exact position. That allows tactics like waiting by a door, retreating to heal, fetching allies, or
  following the light to where the player was last seen.
- **Friends** (followers fighting beside the player) are new behaviour on top of the "follow" goal; UW1 has
  almost none. **Free-form talk** is a separate, bigger step: conversations are CNV.ARK scripts, and
  replacing them would change the game a lot.

## Running the model: inside the game, no web calls (the user's choice)
- **Recommended:** compile llama.cpp from source into the port with our clang
  (O:\appsnotonc\llvm, MinGW target). Its interface `llama.h` is plain C; its inside is C++, which our clang
  already builds.
  - **CPU backend first:** builds with what we have, uses no graphics memory, also works on the user's laptop.
  - **Vulkan backend later** for the GPU (also builds with clang; the Vulkan SDK would be installed on O:).
  - The CUDA backend needs nvcc plus MSVC, which our toolchain doesn't use.
  - Code we compile ourselves avoids auto mode's block on running downloaded binaries.
- Alternative: load the prebuilt llama.cpp DLLs already on G:\LLM\llama.cpp\b12131 (CUDA, about 600 MB).
  Quickest to prototype, but auto mode refused to run downloaded binaries before, so the user would do the
  test runs.
- Not recommended: our own model runner in C with D3D12 compute shaders (weeks of work).
- **Model:** a small instruct model, 1.5-4B parameters, Q4, GGUF. **Not chosen or downloaded yet: ask the user
  before downloading anything.** The user's existing local models (G:\LLM, Qwen 27B on llama-server :8081) are
  too big to run beside the game with DLSS and ray tracing.
- **Speed (estimates, never measured):** CPU about 0.3-1.5 s per decision; GPU about 50-150 ms. Cache the
  fixed start of the prompt, so each decision only reads the new facts (about 100-200 tokens) and writes about
  20. Several creatures can share one request.
- Disk: O: had plenty of free space on 2026-11-10 (the whole setup is under 6 GB).

## Effort (estimate)
After the AI port: a few days for the link (model loading, worker thread, writing out what each creature
knows, checking answers). Tuning the prompts until creatures act smart rather than random is the longer part.

## Model research (2026-11-11, web sources; speeds NOT measured on this PC yet)
Goal raised by the user: a decision "every few frames". At 60 fps that is ~50 ms per answer.
Budget per decision: ~150 new prompt tokens (rest cached) + ~10-15 answer tokens under a GBNF grammar
with literal field names (a generic JSON grammar is slower).

Shortlist (all GGUF, Q4, run by llama.cpp built from source):
1. **LFM2.5-1.2B-Instruct** (Liquid AI, ~0.8 GB): fastest; hybrid conv architecture made for on-device;
   tool use/instruction following strong for its size; 116 tok/s on a laptop Ryzen CPU; DSpark drafters
   (Aug 2026) ~2.5x decode, upstream in llama.cpp. -> the fast "tactical" model.
2. **Qwen3.5-0.8B / 2B** (Mar 2026, Apache 2.0): native tool calling; same `qwen35` arch our b12131 already
   runs. Turn thinking OFF.
3. **Gemma 4 E2B** (Apr 2026, Apache 2.0): good quality, ~1.3 GB on the GPU but 2.4 GB file (per-layer
   embedding table stays in RAM).
4. Smarter, ~2-3x slower: **LFM2.5-2.6B**, **Qwen3.5-4B**, Gemma 4 E4B, Phi-4-mini -> a slow "strategy"
   model every 1-2 s, if wanted.
Fallback: Llama 3.2 1B/3B (older, drifts).

Estimated times (my extrapolation from 7-8B 4090 numbers and vendor CPU numbers):
- 4090, 1.2B: ~10 ms prefill + ~30-40 ms answer = ~50 ms -> every ~3 frames. Several creatures as parallel
  sequences in one batch cost little extra. But it shares the GPU with DLSS + ray tracing -> frame-time spikes.
- CPU (i9-10980XE), 1.2B: ~150-250 ms -> every ~10-15 frames; no GPU contention.
Plan: two tiers (fast 1.2B for tactics, optional 2.6-4B for strategy); original AI keeps acting between
answers. Measure with llama-bench before choosing. Ask before downloading.

## Staged 2026-11-11 (not in use)
Models in dist\UW1\llm\models (README.md there has the hashes and licences): LFM2.5-1.2B-Instruct Q4_K_M
(0.73 GB) and Qwen3.5-2B Q4_K_M (1.28 GB). llama.cpp source b12131 in external\llama.cpp (gitignored, not
built). The next session adds the feature.

## Our own Strata: the model runner (the user's decision, 2026-11-11)
The user: "we are going to have to make our own strata" ... "and then we could have ours load completely if it
can". Strata itself (G:\LLM, a Python engine for huge mixture-of-experts models, a separate desktop app; the
partial source on G: has no licence file: ideas only, no code) is not shipped. We write our own small runner in
C over llama.cpp's library (MIT): `src\uwmind.c` (name open). What it takes from Strata, cut to our size:

1. **Probe the PC** at start: every GPU through DXGI (name, vendor, dedicated VRAM, the budget left now:
   IDXGIAdapter3::QueryVideoMemoryInfo), the processor (cores, AVX2 / AVX-512: ggml picks its CPU DLL itself),
   system RAM.
2. **Headroom for the game first** (Strata's "leave room for my work"): the VRAM the renderer, ray tracing and
   DLSS / DLSS 5 already use is measured after the renderer is up, plus a reserve (Strata: >= 20 % of the card);
   CPU threads for the model capped so the game's own threads keep their cores (Strata: <= 80 % of cores).
3. **Place the model** ("ours load completely if it can"): all layers on the GPU when model + KV cache fit the
   budget left; else as many layers as fit on the GPU and the rest on the CPU (n_gpu_layers); else CPU only, and
   then the small model (LFM 1.2B) even if Qwen was chosen. Setting `llm_device = auto | gpu | cpu`.
4. **Calibrate once per PC** (Strata's calibration): the first run times one real decision (prompt + answer) in
   each placement that fits, keeps the fastest that leaves the frame rate alone, and saves it in home\ (with
   the GPU, driver and model names, so a changed PC calibrates again).
5. **Run it the cheap way:** one worker thread, never waited on by the game; the shared rules prompt read once
   and kept in the KV cache (Strata's "reads only what is new"), each creature's question appended as its own
   sequence; several creatures in one batch; the answer held to a fixed form by a grammar (GBNF), a few tokens.
6. **Load late, unload when idle**, and if anything fails (no model, out of memory, too slow) the original AI
   simply runs alone.

Build: tools\buildllama.cmd -> build\llama\bin (llama.dll, ggml*.dll, one ggml-cpu-*.dll per processor
generation, loaded at run time: GGML_BACKEND_DL + GGML_CPU_ALL_VARIANTS). GPU: the Vulkan backend
(ggml-vulkan.dll; any NVIDIA, AMD or Intel card, the player needs only the driver). Building it needs glslc and the
Vulkan headers, not on this PC yet (ask before downloading the Vulkan SDK or shaderc). CUDA is not used: players
would need NVIDIA's runtime.

### First measurements (2026-11-11, this PC: i9-10980XE 18 cores, CPU only, 18 threads; tests\uwmind_bench.c)
Built: tools\buildllama.cmd (CPU, all variants; every DLL needs only Windows' own runtime, 32 MB) and
tools\buildmind.cmd. One goblin question (191 prompt tokens), the answer held by a grammar to
`goal=<attack|flee|follow|wander|guard|hide> target=<n>`:

| model | load | prompt read | answer | one decision | answer given |
| --- | --- | --- | --- | --- | --- |
| LFM2.5-1.2B Q4_K_M | 0.8 s | ~350 ms (~550 tok/s) | 8 tok, ~300 ms | ~0.65 s | goal=attack target=1 |
| Qwen3.5-2B Q4_K_M | 1.2 s | ~520 ms (~360 tok/s) | 6 tok, ~650 ms | ~1.2 s | goal=flee target=1 |

The grammar also keeps Qwen from "thinking" (it cannot start a think tag). Next, to make it fast:
- the GPU (Vulkan backend; needs the Vulkan SDK's glslc + headers to build: ask before downloading);
- the shared rules prompt read once and kept (only each creature's few lines read per decision);
- no free generation at all: the answer's choices scored directly (one pass for the goal's first token over
  the six goals, one for the target), instead of sampling 6-8 tokens through the grammar (the per-token cost
  above, 37-110 ms, is mostly the grammar checked against the whole vocabulary: 65K LFM, 248K Qwen);
- several creatures in one batch.

### GPU measurements (2026-11-11, RTX 4090, Vulkan backend; same bench, same question, 5 runs)
Vulkan SDK 1.4.363.1 installed in O:\appsnotonc\vulkan-sdk\1.4.363.1 (only for building: glslc + headers).
`tools\buildllama.cmd vulkan` builds ggml-vulkan.dll (44 MB; needs only Windows' runtime and vulkan-1.dll, which
every graphics driver installs). The shader generator is built through tools\clang-mingw-host.cmake.

| model | where | load | prompt read | answer | one decision |
| --- | --- | --- | --- | --- | --- |
| LFM2.5-1.2B | GPU | 0.65 s | ~57 ms (~3400 tok/s) | 8 tok, ~120 ms | **~0.18 s** |
| LFM2.5-1.2B | CPU | 0.15 s | ~230 ms (~830 tok/s) | 8 tok, ~440 ms | ~0.67 s |
| Qwen3.5-2B | GPU | 1.25 s | ~18 ms (~10000 tok/s) | 6 tok, ~350 ms | **~0.37 s** |
| Qwen3.5-2B | CPU | 0.5 s | ~415 ms (~450 tok/s) | 6 tok, ~530 ms | ~0.95 s |

- The **first GPU decision ever on a PC takes ~10 s**: the driver compiles the Vulkan shaders once and keeps them
  in its own cache; a new process after that starts at ~0.2 s. So: warm up at load time, on the worker thread.
- On the GPU the answer, not the prompt, is now nearly all the time (15-60 ms a token): the grammar is checked
  against the whole vocabulary on the CPU each token. Scoring the choices directly (one-token answers, below)
  is the next speed-up and should bring a decision to roughly the prompt time (~20-60 ms).
- Answers were the same on GPU and CPU (LFM attack, Qwen flee).
- Not yet measured: the GPU time with the game rendering (ray tracing + DLSS) at the same time.

## Goals and factions: what the original has, what we could add (research 2026-11-11, nothing built)
**Original (exhumed critter.h / AI.C):** 13 goals in the goal word's low nibble (0/7 stand, 1 go home, 2 wander,
3 follow, 4 guard, 5 attack, 6 flee, 8 mill, 9 cornered, 10 talk, 11 flutter, 12 hover); **13-15 unused** (only
slow the critter, rate 7). Goal target 8 bits (a mobile, 1 the player). Attitude to the player only (hostile,
upset, mellow, friendly). Factions = Creature.race (work\races.py): goblins 6 and 9 (two tribes) + 28 (some),
mountainmen 10, lizardmen 11, fighters 13, mages 19 (Tybal's: hostile on his level without the orb), outcasts 27;
animals mostly passive; undead and others each their own race. Faction behaviour: kin of the race within hearing
turn hostile when the player hits one (AI.C critter_mv, not loners); allies (b19 bit 6) attack what the player
hit; conversations set a whole race's attitude near home (set_race_attitude); owned things anger the owner race
(CRITTIME.C). Factions never fight each other.
**Could add:** hostile: hunt, investigate, ambush, rally kin, flank, keep distance, retreat-and-heal, steal and
run, scavenge, surrender, patrol. Friendly: escort, fight beside you, wait here, warn, help a friend, fetch, daily
routine. Faction-level: feuds (race x race table), territory, reputation per race, witnesses, demands. Animals:
swarm, nest, scavenge, avoid fire. Undead: tomb guard, hate the living, never flee.

## Faster: one forward pass, instructions kept, batches (2026-11-11 night; uo-new 7174c45)
uwm_score: the instructions read once into sequence 0 and copied per question; each answer = the choice whose first
token scores highest after "goal=" (no generation, no grammar; the grammar way is the fallback when two choices begin
alike, or llm_mode=grammar); up to 8 questions in one decode. tests\uwmind_score.c, RTX 4090 / i9-10980XE, best of 3:

| per decision | old (grammar) | new, one question | new, 8 at once (each) |
| --- | --- | --- | --- |
| Qwen3.5-2B, GPU | 206-303 ms | 18-19 ms | 11 ms |
| Qwen3.5-2B, CPU (9 threads, low priority) | 663-826 ms | 368-386 ms | 166 ms |
| LFM2.5-1.2B, GPU | 82-101 ms | 8-13 ms | 6 ms |
| LFM2.5-1.2B, CPU | 383-435 ms | 217-221 ms | 93 ms |

Qwen gave the same answers both ways; LFM's lurker question is a near-tie (attack / ignore flips with the batch).
**Do not trust the first in-game run of 7174c45's work (work\in\logs):** a parser bug (grammar_choices began one quote
early: choices " (| | ") sent every question the grammar way (how=grammar, ~190 ms); fixed before the commit, the
in-game 13-21 ms are from after the fix (work\in2-*). The first batch of several questions also cost ~13 s (the GPU
compiling its batch pipelines on a creature's time; its answers went stale): the warm-up now runs batches of 1, 2,
4 and 8.
In the game (off-screen, the lurker and friendly tests): 13-21 ms an answer, all by score. First-run calibration on
this PC: GPU 7 ms, CPU 118 ms a question (the warm-up question). Not measured yet: the frame time with DLSS + ray
tracing while creatures ask (the review's item 7).

**Beside the renderer (the second review's item 7, 2026-11-11):** off-screen at 1920x1080, ray tracing with bounced
light, DLSS DLAA, RTX 4090, near the friendly goblins of level 1 (UWPORT_TIMING=1: averages and worsts per 120
frames). Model off: the renderer 14.5 ms a frame, its worst 15.3-15.7, frame to frame worst 16.4-16.9 ms. Model on,
five questions (a batch of four at 15 ms each, then singles 23-26 ms): the same, 14.5 ms, worst 15.4-15.6, frame to
frame worst 16.5-16.7 ms: no hitch to measure. The first such run found the warm-up too short (the first real batch of
four took 2.3 s a question while the GPU compiled for its longer shapes, all dropped as stale): the warm-up questions
are now as long as real ones.

**The warm-up on later starts:** batches of ~150 to ~2400 tokens (the pipelines go by tokens in a batch) take ~310 ms
on the second and third launch alike (work\wl-1, wl-2): the driver keeps the compiled pipelines, so only the very first
start on a PC (or after a driver update) spends ~10 s before the model answers; the rules act alone meanwhile.
**Batch size:** n_batch = n_ctx = 8192 (llama_decode aborts the whole program past n_batch; eight long questions reach
~2500 tokens); a batch that still would not fit is answered one question at a time (tests\uwmind_score.c's oversize
case: 8 questions, 4008 tokens, in one pass, no abort).

**A save in the middle of a hunt (the second review's item 8, checked 2026-11-11 with the game's own save page):**
goblin 218 lost the player and set an ambush (goal 15 at 11,39); F5 + Enter saved slot I with goal 15 in its LEV.ARK
(work\rs-18500); loading it, the goblin's first step logged "a hunt from before a load: back to its own goal"
(original_holder: not one of the original's goal-13/14 creatures). Without that check it would have stood for good.

## WIRED IN 2026-11-11 (evening): the model chooses among the smart-hunting goals
- src\uwmind.c -> build\llama\bin\uwmind.dll (tools\buildmind.cmd): uwm_load (GPU when it fits, else CPU), uwm_ask
  (chat template, grammar, greedy, context cleared each time). makedist ships build\llama\bin\*.dll to dist\UW1\llm\bin.
- port\win32\uwmindhost.c: LoadLibrary at run time (missing DLL or model = rules alone), one worker thread, warm-up
  (shader compile on a new PC), queues; settings llm=0 (off), llm_gpu=0 (CPU); UWPORT_LLM_MODEL=FILE.
- hunt.c: at "lost the player" (hunt / ambush / return) and "noise or light" (investigate / ambush / ignore) the
  creature asks with only what it perceived; the rules act at once, the answer is acted on at its next step.
- Ctrl+L (or UWPORT_AILOG=1): every decision, question and answer as "AI: ..." on stdout and in the message box.
- Measured in the game (RTX 4090, LFM2.5-1.2B): loaded 0.9 s, warm-up 0.14 s (10 s the first time ever), ~94 ms an
  answer. Tested off-screen (work\hunt4.script): the lurker asked five times, answered "investigate" each time.
- Next: richer facts (allies near, wounds), more choices (flee, fetch kin), batching, one-token answers.

## BUILT 2026-11-11: smart-hunting (underworld-exhumed uw-new 8708ef0, in dist\UW1)
`--enhance smart-hunting` (or `enhance=...,smart-hunting` in home\uw1port.cfg), off by default.
Code: underworld-exhumed\uw1\src\port\sys\hunt.c + guarded hooks in AI.C (3) and SOUND.C (3);
docs\ENHANCEMENTS.md has the row. Where it differs from the plan below:
- **No side file.** Return goal = the game's old-goal field (a stander's as goal 7); destination = set_loc's
  fields; timers in memory only (per level and mobile), so after a load a creature saved mid-hunt just stands, the
  original's way for those goals, until something stirs it.
- **The original uses goals 13 and 14** on a few creatures (level 9 several; also stale unused slots): a goal 13-15
  is ours only when hunt.c set it on that creature (same goal, same type); others keep the original slow rate.
- **The player makes no noise** in the original (adventurer's noise 0, so target_found never "hears" him): the
  ambush triggers are "lost the player" (half the time for the ambush types) and a heard noise / seen light.
- Noises: effects 3, 4, 7, 8 (blows) loud 16; 0xB, 0xC, 0x14 (doors) 12; 0xA (miss, missile) 10; 0x12, 0x13 (casting,
  lever) 8; 0xF (landing, also creatures' jumps) 6; reach = hearing x loud / 16 squares, for 4 s. Footsteps (1, 2, 5,
  0xD, 0xE, 0x17) and water never count. Light: sight x brightness / 2 squares (max 16), ahead, line of sight.
- Ambush types by item: goblins 0x46 0x47 0x4C 0x4D 0x4E 0x50, mountainmen 0x54 0x56, lizardmen 0x55 0x58 0x59,
  outcast 0x5A, fighters 0x5D 0x5E 0x5F 0x62 0x68, giant spiders 0x44 0x53 0x5C (race 28 mixes goblins with a ghoul
  and a troll, so not by race). Investigate only for hostile creatures (quest NPCs stay put).
- Checks: **off = unchanged**: a session recorded with the old uw-new.exe (work\off-rec) replays to a byte-identical
  STATE.OUT with the new build (26 checkpoints). (An empty SAVE1\DESC shows leftover memory as slot 1's name, which
  differs between builds: give the slot a name for the comparison.) **On**, hidden scripted runs on level 1
  (work\hunt1.script, hunt3.script; UWPORT_AIDBG=1): goblin 218 lost the player -> ambush at 11,39 -> 59 s -> hunt;
  saw the torch -> investigate -> saw the player -> attack; lurker 223 heard the sword hit a wall 6 squares away out
  of sight -> investigate (a swimmer: turns and looks from its water) -> back to guarding.
- Not done: the LLM choosing among these goals; tuning by play.

## Plan: Hunt, Investigate, Ambush (the user's pick, 2026-11-11; the plan as written before building)
A mod, off by default (an exhumed "enhance" switch, e.g. `smart-hunting`): with it off, AI.C runs exactly as
UW.EXE. Rule-based first; later the LLM may choose among the same goals.
- Goal numbers 13 HUNT, 14 INVESTIGATE, 15 AMBUSH (the free slots: the save format is unchanged). Their timers,
  the goal to return to and the stimulus place in a port-only side table per mobile, saved beside the game's
  save (missing on load = defaults).
- New code in its own port file (uw1/src/port/ai/hunt.c); AI.C gets only guarded hooks: the do_goal switch
  (three cases), crit_offense_find_target (the give-up), crit_guard (the stimulus check).
- **Hunt:** where find_target gives up (target_found 1, or 2 and the coin flip), go HUNT instead: walk to the
  last known spot already stored by set_loc (OBJ_DESTX/Y, TARGETZ), turn and look about there for a while
  (target_found each step: seen = attack again), then discard as the original. Who: hostile, not passive.
- **Investigate:** a stimulus a creature can sense (hearing x the sound's loudness): the wall-hit sound and other
  play_effect sounds (their x, y already passed), a door, a fight, the player's torch seen at a distance
  (sight x the player's light level). Standing / milling / wandering / guarding creatures go and look, look
  about, then return to the saved goal; seeing the player there = the original's reaction by attitude.
- **Ambush:** a hostile, clever creature (a race list: goblins, lizardmen, mountainmen, outcasts, fighters) that
  hears the player but cannot see him picks a square next to a door or a corner on his way, out of his torch's
  reach (the light level's radius), goes there, stands still facing his way, and attacks when he comes within
  reach or sees it; it gives up after a while (then HUNT). No combat bonus (the rules stay the game's).
- Checks: off = unchanged (same input script, same positions as before the change); on: scripted off-screen
  runs near goblins with a goal log (UWPORT_AIDBG) for each case.
- **The user's answers (2026-11-11):**
  1. Investigate: clear noises and light only (wall hits, doors, fights, play_effect sounds, the torch seen at a
     distance). **No footsteps**, so sneaking past still works.
  2. Ambush races: goblins, lizardmen, mountainmen, outcasts, fighters **plus giant spiders** (they wait in
     corners). No other animals.
  3. Give-up times: hunt ~30 s without seeing the player, ambush ~60 s (then it turns into a hunt).

## Faster options (2026-11-11, estimates, not measured)
Smaller models: **LFM2.5-350M** (~0.25 GB Q4, ~3x faster than the 1.2B) and LFM2.5-230M (~4-5x). Untuned
they score only 34-63% on tool calling; fine-tuned (distil labs) 96-98% -> plan a fine-tune if used.
Qwen3.5-0.8B: ~1.5x faster, smarter untuned. NOT downloaded (user was offered LFM2.5-350M, no answer yet).

The bigger win is how we ask, with the staged models:
1. **One-token answer:** every goal is one letter/token (A chase, B flank, C ambush at door, D flee, ...).
   Run one forward pass over the prompt, take the best-scoring allowed token from the logits; nothing to
   generate. Estimate 1.2B: ~5-10 ms on the 4090, ~30-60 ms on the CPU.
2. **Batch** all awake creatures as parallel sequences in one pass (near free on the GPU).
3. **Cache** the fixed prompt start (rules, goal list); each call reads only the new facts.
Fastest of all: **distil.** Log (what the creature knew -> what the LLM chose) while playing, train a tiny
neural net on it; it answers in microseconds, every creature every frame, on the CPU. LLM then only for
development or occasional slow "strategy" calls.

Recommendation: LFM2.5-1.2B + one-token answers + batching first; LFM2.5-350M only if still too slow.

## 2026-11-12: on by default, off without the files, the model picker (plan 2026-11-12-llm-default-and-picker)
- smart-hunting is uw-new's own setting `smart-hunting=0|1` in uw1port.cfg (absent = on), decided in main.c after
  enhance_load whatever the shared enhance= line says (UW-Original.cmd's --no-enhance drops the token there). Off,
  with one line ("smart-hunting off: no language model under llm\"), when llm\bin\uwmind.dll or llm\models\*.gguf
  are missing beside the exe; the worker is then never started (no load tried). A --enhance / --no-enhance on the
  command line still wins over the setting; the settings screen's smart-hunting toggle writes only enhance=.
- `llm_model=<path under llm\models>` (a subfolder and spaces allowed; `lfm` as before; absent or not found:
  Qwen3.5-2B, or the first found if that is gone). The Esc menu's "Language model" row (after Mouse sensitivity)
  shows the file in use (no .gguf, cut at 18) and Left / Right / Enter / a click cycle the files found (one scan at
  start); the worker lets the loaded model go and loads the chosen one at once.
- The first start with more than one model (no llm_model yet, llm not 0, smart-hunting on, not a replay): after 3 s
  of play our menu opens once, titled "Pick a model", on that row; Enter or Esc keeps the one shown (llm_model
  written: never asked again). Not in off-screen runs (UWPORT_PICKTEST=1 lets a check ask). tools\llmcheck.py a-f.
- (mouse) plat_win32.c's mouse_look_follow sends its ` key only while the backend's screen state is PS_GAME (a
  cutscene takes a key as skip, the shrine's text prompt PS_TEXT = 10 as a typed character); PS_TEXT keeps the
  last-frame bridge as PS_GAME does. tools\followcheck.py death / play.
- (third review 1) port_mind_stop: when the worker is still inside a load or a decode after its 5 s wait, the
  recording's end is written and the process ends there (TerminateProcess, code 0) instead of the normal exit that
  unloaded the DLLs under it. tools\llmcheck.py g (UWPORT_LLM_STALLTEST=MS stands in for a long load).
- (third review 2) llm_calibrated is written only when both timings succeeded; else "calibration incomplete",
  nothing kept, the next start calibrates again. llmcheck.py h.
- (third review 4) uwmind.c checks at load that a choice word's first token alone is the one that follows "goal="
  (in context; where the tokenizer joins "=word", as Qwen's does, the token alone must be the word's own start, no
  leading-space mark); per choice in uwm_score too. A failing vocabulary (SentencePiece: llama-spm, phi-3) sends
  every question the grammar way and says so in the "ready" line. tools\buildtokcheck.cmd runs
  tests\uwmind_tokcheck.c on llama.cpp's vocab-only files: spm / phi-3 grammar, qwen2 / qwen35 / gpt-2 / llama-bpe
  scored. Qwen3.5-2B and LFM2.5-1.2B still score (uwmind_score: Qwen's picks = the grammar way's, 15 ms vs 190-270).
- (third review 6) measured 11-12, UWPORT_TIMING=1 from launch, off-screen, Qwen on the 4090: the worker starts at
  the first frame (the title), so the load and warm-up (1.3 s + 0.4 s; a first start's calibration ~5 s more) are
  over before play begins at ~12 s. Frame to frame worst during the load 9 ms (calibrated) and 18 ms (first start,
  calibrating), the same as a run without the model (16-18 ms); the one big gap (~500 ms) is the save's level load,
  there with UWPORT_LLM=0 too. No hitch: the worker's start is left where it is.

## Hunters (level hunters, built 2026-11-12/13; spec docs\superpowers\specs\2026-11-12-level-hunters-design.md)
- What: up to `llm_hunters` (0 off, 1 or 2; default 2; the Esc menu's "Level hunters" row sets it at once, its value
  saying "(smart hunting off)" or "(no model)" when they cannot run) clever hostile creatures a level that notice what happens on
  the level, follow a short plan, remember, and keep it all across save, load and level changes. On only with
  smart-hunting, the model loaded, not a replay (or, for the checks alone, `UWPORT_HUNTER_PLAN=step,...`). Code:
  underworld-exhumed uw1\src\port\sys\hunter.c (the layer), hunternav.c (grid and routes), hunterfile.c (side file);
  hunt.c's hooks; MAINLOOP.C's port_hunter_tick and GAMEWRAP.C's level/save/load hooks.
- The pick: hostile, no name (whoami 0), wits (intelligence) 10 or more, of the race with the most such creatures on
  the level; ambushers first, then wits, then the lowest index. The level is entered (grid, pick) at the first main
  loop pass on the game screen after a level change or a load, whatever creature steps (port_hunter_tick).
- Noticing: the ledger (the adventurer's noises with their sound effect, lights, his light going out, kin deaths,
  doors, things taken, traps), each entry noticed once by hearing (the creature's hearing), sight (line of sight) or
  4 squares for a corpse / door / missing thing. What a hunter noticed is saved with it (`seen` lines): a load does
  not have it notice the same entries again.
- The plan (question kind 6, one step at a time, at most 4): goto_last, goto_door, goto_home, goto_water, wait,
  search, fetch_kin, follow_light, recover, give_up. A new fact mid-plan asks keep_plan / rush_in / new_plan.
  A plan ends: done, lost him, blocked, wounded, gave up (180 s).
- Routes: a 64x64 grid by the original walker's terrain (critter\PATHFIND.C hyp_move with CT1): water closed unless
  a solid object stands on the tile (a bridge: its top is the floor), lava 2 danger, a climb of more than one height
  unit refused, a drop of d costing d - 1 danger; the route's danger within the creature's AI.C acceptable_danger
  (hp * 4 / avghit + nerve / 4). Fewest steps (a door 2) within that, up to 1024 cells, cut into waypoints of at most
  4 squares whose straight walk costs no danger; each leg is the original's own walk (crit_head_for_loc).
  Each mover by its own table (AI.C: a flier CT2, else a swimmer CT4, else a walker CT1): swimmers water only (or a
  stood-on object), heights tested, no lava; fliers any open tile, no heights, no danger. The walls between two tiles
  (TRIGGER.C tile_walls by tile type: a diagonal's two half walls) stop every mover, and a slope entered from any side
  but the one it rises toward stands one unit higher (hyp_move's h23++), both since 2026-11-13 (hunternav's side[]).
- Side file: SAVEn\UWNEW.DAT beside the game's save (text, `uwnew 1`; clock, hunter, mem, ledger, graphblock, seen
  lines; read tolerantly). Times are GAME_TIME, the 1/256 s tick counter, which restarts with each run of the program
  and is not in the game's save: the file keeps the clock it was written on and a load moves every time onto the
  clock running then (before this a plan saved in one run "gave up" at once when loaded in another).
- Logs: Ctrl+L (AI log) "hunter #N ..." lines; UWPORT_HUNTERDBG=1/2 on stderr; UWPORT_HUNTER_ROUTES=1 one line a
  hunter at each level entered (its route to the player: cells, danger of its range, waypoints, or UNREACHABLE),
  =2 the grid too ('#' closed, '~' water, 'L' lava, 'D' door, else the height in hex).
- Checks: build\hunternav_test.exe (10 cases: routes, doors, water, lava and the danger range, heights, a 600-cell
  route) and build\hunterfile_test.exe (9 cases, the side file incl. seen and clock), both in tools\verify.cmd;
  tools\huntercheck.py routes in tools\verify.cmd too (two off-screen runs, about 2.5 min: all nine levels logged,
  every walker's verdict the same as a reference written apart from hunter.c, walker_cells / walker_danger from the
  save's LEV.ARK; then the player put where each level's first hunter can reach him: reached on every level with a
  hunter). By hand: plan1, plan_giveup, plan_blocked, save (A, B, B2 seen kept, C, D1, D2a, D2b), react; with the
  model: ask, react_ask. plan1 depends on a real wall strike's timing and fails now and then (2 of 3 passed 11-13).
- The 2026-11-13 route run: level 1 goblins reach the player with danger 2 (drops; bridges), level 6 trolls over
  bridges across lava, level 5's goblins live on their own island (unreachable in the level file itself), level 7's
  fire elementals stand where no dry square is in reach, level 9 has no hunters.
