# Fine-tuning LFM2.5-350M and LFM2.5-1.2B into creature deciders (plan, 2026-11-13)

Goal: the two LFM models the port stages (dist\UW1\llm\models) pick the right goal for a creature more often than the
stock instruct models do, with the same uwmind.c readout (one forward pass, the goal whose first token scores highest
after "goal="), the same speed, and files small enough for any card. The rules core stays untouched: the model is the
optional `llm` mod, off in every test and proof (CLAUDE.md).

Why both: the 1.2B is the tactical model on any PC with a GPU; the 350M is the floor for CPU-only and weak cards. Same
architecture (`lfm2`: 10 double-gated conv blocks + 6 GQA, 65,536 vocabulary), same chat template, same licence (LFM
Open License v1.0: free below 10 M USD revenue, redistribution allowed with the LICENSE file; derivatives may be
ours). One data set and one training script serve both.

What the decision-model projects (AWS Strands Decider, Mapika decider, Together Tev1, Geuter et al. "Releasing the
Inner Jev") found, and we follow: the fine-tune is where the accuracy comes from, not a bigger base; shuffle the option
order in every example or the model learns "the first option is usually right"; diversity of situations beats volume;
train and run with the identical prompt template; measure on a held-out set written by hand, not by the teacher.

## 0. Ground rules for this work
- Nothing on C:. The Python environment goes to O:\appsnotonc\finetune (uv venv), caches (HF_HOME, TORCH_HOME,
  UNSLOTH cache) set to O:\appsnotonc\finetune\cache.
- Ask before downloading the bf16 base weights (LiquidAI/LFM2.5-350M ~0.7 GB, LiquidAI/LFM2.5-1.2B-Instruct ~2.3 GB)
  and before installing torch + unsloth (several GB).
- The auto-mode classifier blocks running downloaded binaries; the training command itself may have to be started by
  the user (train.cmd), as with llama-server.
- Never delete: new models go into new folders beside the stock ones; the stock GGUFs stay.
- No local LLM test runs unless the user asks (2026-11-13). Evaluation of the tuned models is a run the user starts.

## 1. The task format (one day)
Freeze exactly what the game sends, because training must use the same bytes:
- Read uwmind.c / uwmindhost.c / hunt.c: the shared rules prompt (system), the per-creature situation lines (user),
  and the answer prefix (`goal=`) the scorer appends. Write them out as one Python function `format_example(state,
  options)` that produces the identical chat-template string (tools\mind_format.py), and a test that compares its
  output to a dump from the game (UWPORT_AILOG=1 prints question and answer; one captured question is the fixture).
- The state fields the game knows today (hunt.c): creature kind and attitude, own health band, allies within hearing,
  the event (lost the player / noise / light / hurt / ally died), last seen position relative to the creature, distance
  and direction, a door or corner near, light level, time since last contact. Keep the fields the game can fill;
  add none the game cannot.
- The options: the smart-hunting goals the game accepts at that event (hunt / ambush / return; investigate / ambush /
  ignore; plus flee, rally, guard, follow where the event allows). The label is the goal's first token (all distinct in
  the 65K vocabulary: check with tests\uwmind_tokcheck.c; if two goals share a first token, rename one in the prompt).
- Options are listed in a random order per example; the answer is the goal word, not a letter, so the readout stays
  what uwm_score does.

## 2. Data (two to three days, mostly machine time)
Three sources, kept apart in the files:
1. **Generated situations (bulk, ~4,000):** tools\mind_situations.py enumerates combinations of the state fields with
   weights that match the game (most events are "lost the player" and "noise"; health mostly full), and writes the
   prompt for each. Dedupe. This gives coverage of the corners (low health + allies near + lost the player ...).
2. **Real situations (~500-1,000):** play with UWPORT_AILOG=1 (the user), or the off-screen hunter checks
   (tools\huntercheck.py) with the log on; collect every question the game actually asked. This is the distribution
   that matters most; the held-out set comes mainly from here.
3. **Hand-written gold set (150-200, never trained on):** the user and I write the situations where the right goal is
   clear (cornered at low health with no allies: flee; heard a fight where kin are: rally; lost the player at a door
   with full health: ambush ...), each with its gold goal and one line why. This is the only number we trust.

Labels for 1 and 2 (the teacher): the big local model (Qwen3.8-27B on 127.0.0.1:8081, thinking on) answers each
situation twice with the options in two different orders and a rubric prompt that explains the creature's aims
(a goblin wants to live and to hurt the player; it knows its kin; it cannot see through walls ...). Keep an example
only when both answers agree (Mapika's teacher agreed with itself on 72 % of generic items; disagreement marks a
genuinely ambiguous situation, which we drop or send to the hand set). Store as JSONL: state, options (in the order
shown), answer, source, teacher confidence. Target 3,000-5,000 kept rows; Tev1-4B used 37,840 for a far broader task,
Brooker found diversity mattered more than count.

Optional second label pass: a few hundred rows where the teacher's choice looks wrong get the user's verdict; those
override.

## 3. Training (an afternoon per model on the 4090)
Environment: O:\appsnotonc\finetune: uv venv, torch (CUDA 12.x wheels), unsloth, trl, transformers v5, datasets,
sentencepiece. Unsloth has LFM2.5 support and notebooks (docs.liquid.ai/lfm/fine-tuning/unsloth).

Per model (tools\mind_train.py, one script, the model name an argument):
- Load bf16 base: LiquidAI/LFM2.5-1.2B-Instruct; for the 350M LiquidAI/LFM2.5-350M (the post-trained one, not Base).
- LoRA r=16, alpha=32, dropout 0, on q/k/v/o and the conv in/out and gate/up/down projections (Unsloth's default list
  for LFM2); gradient checkpointing "unsloth". 1.2B LoRA needs ~3-4 GB VRAM, 350M ~2 GB. Full fine-tune of the 350M is
  also possible on 24 GB (try it as a second run if LoRA plateaus).
- Completion-only loss: the loss counts only the answer tokens after `goal=`, not the prompt (TRL's
  completion-only collator with the exact assistant prefix).
- One epoch over the kept rows (~3-5k), batch 16, lr 2e-4 (1.2B) / 5e-4 (350M), cosine, warmup 5 %. Minutes, not hours.
- Shuffled option order already baked into the data; add a second copy of each row with another order if the set is
  under 3k.
- Keep general ability: hold out 200 plain instruct prompts (from the LFM card's style) and check the tuned model's
  answers are still sane; if they degrade, mix 10 % generic instruct data back in (Brooker used KL to the frozen base
  for this; the mix is the cheap version).
- Save: LoRA adapter; merged 16-bit safetensors (Unsloth save_pretrained_merged).

## 4. Export to GGUF (an hour)
- external\llama.cpp (b12131 source already on disk): `convert_hf_to_gguf.py --outtype f16` on the merged folder, then
  `llama-quantize` to Q4_K_M and Q8_0 (the quantize tool comes from tools\buildllama.cmd's build). Unsloth's
  save_pretrained_gguf does the same with its own llama.cpp; use ours so the GGUF matches the runtime we ship.
- Ship Q4_K_M for the 1.2B (0.73 GB). For the 350M ship Q8_0 (0.38 GB) unless the eval shows Q4_K_M within a point:
  4-bit costs small models more, and 160 MB is not worth a wrong goal.
- Folders: dist\UW1\llm\models\LFM2.5-1.2B-UW\ and LFM2.5-350M-UW\ with LICENSE (LFM) and a README naming the base,
  the data set version and the training run; hashes into dist\UW1\llm\README.md. The stock files stay.

## 5. Evaluation (the only step that decides anything)
- tests\uwmind_score.c already loads a model and scores questions; give it a `--set FILE` mode that reads the JSONL
  gold set and prints accuracy, per-event accuracy, and the confusion pairs (ambush chosen where flee was gold ...).
  This runs through the same llama.cpp Vulkan/CPU path as the game, so the numbers are the game's.
- Compare on the hand-written gold set and on the real held-out set: stock 1.2B vs tuned 1.2B, stock 350M vs tuned
  350M, Q8 vs Q4 of each. Report also the position-bias check: the gold set with options reversed must give the same
  answers (a model that flips answers with the order has learned position, not situation).
- Speed: the tuned models are the same size, so the 2026-11-11 numbers hold; confirm with one `uwmind_score` run on
  the 4090 (GPU) and CPU.
- Acceptance: the tuned model beats its stock version on the gold set by a clear margin (target: stock around chance
  on the hard items, tuned above 80 % overall) and loses nothing on the generic prompts. Otherwise: more diverse data
  first, then r=32, then the full fine-tune of the 350M. Do not ship a model that only memorised the generated set.
- These runs are started by the user (no local LLM tests from the agent, 2026-11-13).

## 6. In the game
- Setting `llm_model = LFM2.5-1.2B-UW | LFM2.5-350M-UW | ...` (uwmindhost.c already takes UWPORT_LLM_MODEL=FILE);
  auto picks the 1.2B when the GPU fits it, else the 350M (the placement logic in LLM_NPC_AI.md).
- The Ctrl+L AI log keeps printing question and answer; new real situations flow back into source 2 for the next
  round. Plan a second training round after a week of play logs.

## Order and estimate
1. Format freeze + tools\mind_format.py + fixture test: 1 day.
2. Situation generator, real-log collection, hand gold set: 2-3 days (the gold set is the user's time, ~2 hours).
3. Teacher labelling on :8081: a few hours of machine time (5k rows x 2 passes, ~1 s each).
4. Environment + training both models: half a day.
5. Export + uwmind_score --set + evaluation: half a day.
6. Ship + README + settings: 1 hour.
About a week of calendar time, most of it data; the training itself is minutes per model.

## Open questions for the user
- Teacher: the local 27B (free, slower, unknown accuracy on tactics) or Claude (better, costs tokens)? Default: the
  27B, with Claude for the hand set review.
- Which goals may the model choose at each event: the smart-hunting set only (today), or also the original's 13 goals
  (flee, guard, follow ...)? More options = more data per option.
- OK to download the two bf16 base checkpoints (~3 GB) and install torch + unsloth to O:\appsnotonc\finetune?

## Status 2026-11-13 evening
- Environment up in O:\appsnotonc\finetune (its own Python 3.12 copy: a venv's launcher is blocked on this PC; torch
  2.11 cu128, unsloth 2026.11.4, trl, transformers 5.17, peft, datasets, gguf), the two bf16 bases downloaded and
  hash-checked, README.md there with the order of work.
- Step 1 done: tools\mind_format.py (every question builder byte-identical: self-test PASS against uwmind_score.c's
  logged questions; the ChatML + BOS text the same as both models' own chat template).
- Step 2 begun: tools\mind_situations.py wrote 4,000 generated rows (kinds 2/1/4/3/6 = 1603/1026/591/377/403, the
  choices' order random); gold_seed.py 38 hand cases (the user to review and extend). label.py written, not run (the
  teacher is the user's decision; no model run started by the agent).
- Step 3-4 written: train.py (Unsloth LoRA, completion-only, first-token scoring on held-out and gold), train.cmd,
  eval.cmd, export.cmd (llama-quantize built into build\llama\bin for it).
- To do: the real-log collector (data\real.jsonl), tests\uwmind_score.c --set, the in-game llm_model setting.
