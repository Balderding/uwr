# The creature model: a small language model trained to decide what a creature does

In November 2026 we trained a small language model to pick what a dungeon creature does next in uwr, got it to
follow our rulebook almost perfectly, and then set it aside: the game now makes the same decisions with ordinary
code. This folder holds the scripts we used, so that anyone can repeat the training or train a model of their own.

The trained model file is not in this repository and not in the download.

## What we trained

| | |
| --- | --- |
| Base model | **LFM2.5-350M** by Liquid AI (350 million parameters; its own licence, the LFM Open License v1.0) |
| Our model | **LFM2.5-350M-UW**, release 5 ("r5"), as GGUF files: f16, Q8_0 and Q4_K_M |
| Method | LoRA (rank 16) with [Unsloth](https://github.com/unslothai/unsloth); the loss counts only the answer, not the question |
| Training data | 6,958 questions with the right answer, in the exact text the game sends |
| Settings | 3 epochs, learning rate 5e-4, questions up to 1,536 tokens |
| Hardware and time | one NVIDIA RTX 4090; the final run took about 15 minutes, training and scoring together |
| Whole project | started 2026-11-13, final model trained 2026-11-14, set aside 2026-11-16; most of the time went into the rulebook and the labels, not into training |

We also ran LFM2.5-1.2B-Instruct once; the 350M was enough for this task and is the one we kept.

### The task

The game describes a creature's situation in a few sentences (what it is, how hurt it is, whether it sees the
player, what it heard, which kin are near) and lists the goals it may choose from, such as attack, flee, hunt,
ambush or call for help. The model answers with one word. The game does not let the model write freely: it reads
only the first token after `goal=` and takes the most likely of the offered words (`src/uwmind.c`).

The right answer comes from a rulebook of nine numbered rules where the first rule that fits wins
(`SYSTEM.txt` here; the same text is the model's system prompt).

### How accurate it was

Measured the way the game uses the model (the first token, among the offered words):

| Model | Questions held out of training | Hand-written test set ("gold") |
| --- | --- | --- |
| Run 4: 6,000 questions | 240 of 276 (87.0%) | 47 of 50 |
| **Run 5 (r5): 6,958 questions, cleaner labels** | **294 of 311 (94.5%)** | **50 of 50** |

Run 4 was at 96% on every kind of question except the level hunters' plan steps (66 of 96). Most of those misses
were faults in our labels, not in the model: two labelling rounds disagreed where the rulebook was silent. For
run 5 the plan steps were labelled by a small program (`tools/hunter_policy.py`) and the rule was rewritten to say
what that program does.

How well the teachers knew the rulebook, on the gold set: DeepSeek V4.1-Flash 25 of 38 and Qwen3.8-27B 22 of 38
(both run locally), Claude Sonnet 5.5 39 of 46 with the first rulebook and 50 of 50 once the rulebook's gaps were
closed. Every miss by the good teacher pointed at a rule that was unclear, so the gold set mostly improved the rules.

### Why we set it aside

The model was only imitating a rulebook that we had written ourselves. Code follows that rulebook exactly, answers
at once and needs no graphics memory, while the model was right 94.5% of the time, needed a moment for each answer
and shared the graphics card with the game. So the nine rules were ported to C (`cunning.c`, `pack.inc`) and
grew from there into the game's Creature AI levels (Original, Devious, Monstrous).

The model path is still in the game, off by default: in the Esc menu's AI section, **Decisions by** can be set to
**Language model**, and **Language model** picks any GGUF file you put in the game's `llm/models` folder. See
[LLM_NPC_AI.md](../../LLM_NPC_AI.md) and [docs/MODELS.md](../../docs/MODELS.md).

## Do it yourself

You need a PC with an NVIDIA card (we used 24 GB; the 350M needs far less), Windows (the `.cmd` files; the Python
files run anywhere), and your own copy of the game for one small file.

### Set up

1. Copy this folder somewhere with a few GB free.
2. Put a Python 3.12 in `python\` inside it and install: `torch` (a CUDA build), `unsloth`, `trl`, `transformers`,
   `peft`, `datasets`, `gguf`. `python check_env.py` prints the versions it finds. `env.cmd` keeps every download
   cache inside the folder.
3. Download the base model LFM2.5-350M from Liquid AI's page on Hugging Face into `models\LFM2.5-350M`.
4. Copy `DATA\OBJECTS.DAT` from your game into `data\` (the creatures' table; it is the game's file, so it is not
   here).

### The steps

1. **Make the questions.** `python -I mind_situations.py data\OBJECTS.DAT data\generated.jsonl 4000 1` writes 4,000
   situations in the game's own wording, weighted roughly as play produces them, with the choices in a random order
   in each (a fixed order teaches a model that the first choice is right). `mind_format.py` holds the wording and
   the rulebook; `python -I mind_format.py` tests itself.
2. **Write a gold set.** `gold_seed.py` holds our 50 hand-written cases with the answer and the rule that gives it.
   They are never trained on. Check every teacher against them before trusting its labels.
3. **Label the questions** (a teacher gives each its answer). Two ways:
   - `label.py IN.jsonl OUT.jsonl --url ... --model ...` asks any OpenAI-compatible server. Each question is asked
     twice with the choices in two orders and kept only when both answers agree.
   - What we did: `tools\chunks.py split` cuts the questions into files of 200, a strong model answers each file
     as a CSV (id, answer, rule) with `SYSTEM.txt` beside it, and `tools\chunks.py merge` puts the answers back
     and rejects any that is not among a question's choices. `tools\chunks.py score` checks a teacher on the gold set.
   - The level hunters' plan steps are better labelled by code: `tools\hunter_policy.py` (`check`, `build`).
4. **Score the untrained model first**, so you know what training bought:
   `eval.cmd 350M data\labelled.jsonl --gold data\gold.jsonl`
5. **Train.** Our final run: `train.cmd 350M data\labelled.jsonl runs\my-run --lr 5e-4 --epochs 3 --gold data\gold.jsonl`
   It holds 5% of the questions out, trains on the rest and prints the score on the held-out questions and on the
   gold set. `--errors FILE.csv` (with `--eval`) lists the questions it got wrong.
6. **Read the misses before training again.** Ours were mostly wrong or contradictory labels and questions cut
   off by the length limit, not the model. Fix the rulebook or the labels, then train again.
7. **Export for the game.** `export.cmd runs\my-run\merged MY-NAME` writes `gguf\MY-NAME-f16.gguf`, `-Q8_0.gguf`
   and `-Q4_K_M.gguf`. It needs llama.cpp's `convert_hf_to_gguf.py` and `llama-quantize` in `llama-tools\`.
8. **Use it.** Put a GGUF in the game's `llm\models\<any name>\` folder, set **Decisions by** to
   **Language model** in the Esc menu and pick the file in the **Language model** row.

### What we would tell anyone trying this

- The rulebook is the work. A teacher's mistakes on the gold set showed us where a rule was unclear far more
  often than where the teacher was weak.
- If a program can give the right answer, you do not need the model for that answer. That is how this ended.
- A model this small is fast enough to ask several times a second, but its answer still comes later than code's,
  and the game has to cope with an answer that arrives after the moment has passed.
