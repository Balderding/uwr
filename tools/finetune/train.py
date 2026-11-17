"""train.py: LoRA fine-tune of an LFM2.5 model into a creature decider (the plan's step 3), and the first-token
evaluation the game uses (step 5's quick form: the same readout as src\\uwmind.c uwm_score, in Python).

    python train.py MODEL_DIR DATA.jsonl OUT_DIR [--lr 2e-4] [--epochs 1] [--r 16] [--bs 16] [--gold GOLD.jsonl]
    python train.py MODEL_DIR --eval DATA.jsonl [--held] [--gold GOLD.jsonl] [--errors FILE.csv]
                                             the stock (or merged) model scored, no training; --held: only the held-out
                                             5 %; --errors: the wrong rows written to FILE.csv and the confusions counted

MODEL_DIR: a bf16 checkpoint (models\\LFM2.5-1.2B-Instruct, models\\LFM2.5-350M, or a merged output). DATA: rows with
"user", "choices" and "answer" (label.py's). 5 % of the rows (by id hash) are held out and scored after training;
--gold scores the hand-written set too (never trained on). OUT_DIR gets the LoRA adapter (adapter\\) and the merged
16-bit model (merged\\), ready for export.cmd.

The text trained on is exactly what the game sends (mind_format.prompt_text: BOS, ChatML, "goal="), then the answer
word and <|im_end|>; the loss counts only the assistant's part (Unsloth's train_on_responses_only).
"""
import json, sys, os, zlib, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # mind_format.py beside this file
import mind_format as F

def arg(a, k, d):
    return type(d)(a[a.index(k) + 1]) if k in a else d

def load_rows(p):
    return [json.loads(l) for l in open(p, encoding="utf-8")]

def held_out(r):
    return zlib.crc32(r["id"].encode()) % 20 == 0

def first_token_ids(tok, choices):
    """each choice's first token as it follows "goal=" (uwmind.c choice_token_ok's idea: the token after "goal=")"""
    base = tok("goal=", add_special_tokens=False).input_ids
    out = []
    for w in choices:
        ids = tok("goal=" + w, add_special_tokens=False).input_ids
        out.append(ids[len(base)] if ids[:len(base)] == base and len(ids) > len(base) else ids[-1])
    return out

_VOCAB = {}
def vocab_by_text(tok):
    """every plain token's text -> its ids (what a grammar looks at: the text a token adds)"""
    if not _VOCAB:
        special = set(tok.all_special_ids)
        for i, t in enumerate(tok.batch_decode([[i] for i in range(len(tok))])):
            if t and i not in special: _VOCAB.setdefault(t, []).append(i)
    return _VOCAB

def grammar_pick(model, tok, text, choices):
    """the grammar way (uwmind.c uwm_ask's idea: greedy under a grammar of the choice words), for the rows whose
    choices share a first token (goto_last, goto_door, ...): token by token, the likeliest token that still spells a
    choice, until one choice is left"""
    import torch
    vocab = vocab_by_text(tok)
    ids = tok(text, add_special_tokens=False, return_tensors="pt").input_ids.to(model.device)
    left, done = list(choices), ""
    while len(left) > 1:
        allowed = {}
        for w in left:
            rest = w[len(done):]
            for k in range(1, len(rest) + 1):
                for t in vocab.get(rest[:k], ()): allowed[t] = rest[:k]
        if not allowed: break
        logits = model(input_ids=ids).logits[0, -1]
        t = max(allowed, key=lambda t: logits[t].item())
        done += allowed[t]
        left = [w for w in left if w.startswith(done)]
        if done in left: return done            # (a choice spelt out whole)
        ids = torch.cat([ids, torch.tensor([[t]], device=ids.device)], dim=1)
    return left[0] if left else ""

def score(model, tok, rows, label, bs=16, errors=None):
    """accuracy as the game picks: the first-token readout, and the grammar way for the rows whose choices share a
    first token (they were skipped before 2026-11-13: most of kind 6)"""
    import torch
    model.eval()
    right = n = gn = gright = 0
    per_kind = {}
    with torch.no_grad():
        for i in range(0, len(rows), bs):
            batch = rows[i:i + bs]
            texts = [F.prompt_text(r["user"]) for r in batch]
            enc = tok(texts, add_special_tokens=False, return_tensors="pt", padding=True, padding_side="left").to(model.device)
            logits = model(**enc).logits[:, -1, :]
            for j, r in enumerate(batch):
                ids = first_token_ids(tok, r["choices"])
                if len(set(ids)) < len(ids):    # a clash: the grammar way, as the game does
                    pick = grammar_pick(model, tok, texts[j], r["choices"])
                    gn += 1; gright += pick == r["answer"]
                else:
                    pick = r["choices"][max(range(len(ids)), key=lambda k: logits[j, ids[k]].item())]
                ok = pick == r["answer"]
                if errors is not None and not ok: errors.append((label, r["id"], r.get("kind", 0), r["answer"], pick, " ".join(r["choices"]), r["user"]))
                right += ok; n += 1
                pk = per_kind.setdefault(r.get("kind", 0), [0, 0]); pk[0] += ok; pk[1] += 1
    print("%s: %d of %d right (%.1f %%); by kind %s; first token %d/%d, grammar way %d/%d" % (
          label, right, n, 100.0 * right / max(n, 1), {k: "%d/%d" % tuple(v) for k, v in sorted(per_kind.items())},
          right - gright, n - gn, gright, gn), flush=True)
    return right / max(n, 1)

def main():
    a = sys.argv[1:]
    if len(a) < 2: print(__doc__); return 2
    model_dir = a[0]
    from unsloth import FastLanguageModel
    model, tok = FastLanguageModel.from_pretrained(model_dir, max_seq_length=1536, dtype=None, load_in_4bit=False)
    gold = load_rows(arg(a, "--gold", "")) if "--gold" in a else None
    if "--eval" in a:
        rows = load_rows(a[a.index("--eval") + 1])
        if "--held" in a: rows = [r for r in rows if held_out(r)]   # only the 5 % no run trains on
        FastLanguageModel.for_inference(model)
        errs = [] if "--errors" in a else None   # --errors FILE.csv: every wrong row, and the confusions counted
        score(model, tok, rows, "eval %s on %s" % (os.path.basename(model_dir), os.path.basename(a[a.index('--eval') + 1])), errors=errs)
        if gold: score(model, tok, gold, "gold", errors=errs)
        if errs is not None:
            import csv, collections
            with open(arg(a, "--errors", ""), "w", newline="", encoding="utf-8") as f:
                w = csv.writer(f); w.writerow(["set", "id", "kind", "answer", "picked", "choices", "question"]); w.writerows(errs)
            for (want, got), c in collections.Counter((e[3], e[4]) for e in errs).most_common(25):
                print("  %3d  wanted %-12s picked %s" % (c, want, got))
        return 0
    data, out = a[1], a[2]
    rows = load_rows(data)
    train, hold = [r for r in rows if not held_out(r)], [r for r in rows if held_out(r)]
    print("%d rows: %d train, %d held out" % (len(rows), len(train), len(hold)))
    lr, epochs, r_, bs = arg(a, "--lr", 2e-4), arg(a, "--epochs", 1), arg(a, "--r", 16), arg(a, "--bs", 16)
    FastLanguageModel.for_inference(model)
    score(model, tok, hold, "stock, held out")
    if gold: score(model, tok, gold, "stock, gold")
    model = FastLanguageModel.get_peft_model(
        model, r=r_, lora_alpha=2 * r_, lora_dropout=0, bias="none",
        target_modules=["q_proj", "k_proj", "v_proj", "out_proj", "in_proj", "w1", "w2", "w3"],
        use_gradient_checkpointing="unsloth", random_state=1)
    from datasets import Dataset
    from trl import SFTTrainer, SFTConfig
    from unsloth.chat_templates import train_on_responses_only
    texts = [F.prompt_text(r["user"]) + r["answer"] + "<|im_end|>" for r in train]
    ds = Dataset.from_dict({"text": texts})
    per_dev = min(bs, 8)
    trainer = SFTTrainer(
        model=model, tokenizer=tok, train_dataset=ds,
        args=SFTConfig(output_dir=os.path.join(out, "run"), per_device_train_batch_size=per_dev,
                       gradient_accumulation_steps=max(1, bs // per_dev), num_train_epochs=epochs, learning_rate=lr,
                       lr_scheduler_type="cosine", warmup_ratio=0.05, logging_steps=10, save_strategy="no", bf16=True,
                       dataset_text_field="text", max_length=1536, report_to="none", seed=1,
                       dataset_kwargs={"add_special_tokens": False}))
    trainer = train_on_responses_only(trainer, instruction_part="<|im_start|>user\n", response_part="<|im_start|>assistant\n")
    t0 = time.time()
    trainer.train()
    print("trained in %.0f s" % (time.time() - t0))
    model.save_pretrained(os.path.join(out, "adapter")); tok.save_pretrained(os.path.join(out, "adapter"))
    FastLanguageModel.for_inference(model)
    score(model, tok, hold, "tuned, held out")
    if gold: score(model, tok, gold, "tuned, gold")
    model.save_pretrained_merged(os.path.join(out, "merged"), tok, save_method="merged_16bit")
    print("saved %s\\adapter and %s\\merged" % (out, out))
    return 0

if __name__ == "__main__":
    sys.exit(main())
