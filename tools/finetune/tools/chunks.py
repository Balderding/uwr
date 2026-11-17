"""chunks.py: the generated rows as plain-text question files for the labelling agents (2026-11-13: the user chose Claude
subagents through Claude Code as the teacher, one chunk per agent), and the merge of their answer CSVs back into
data\\labelled.jsonl and the answer column of data\\generated.csv.

    python -I tools\\chunks.py split  [ROWS_PER_CHUNK=200]      -> data\\chunks\\qNN.txt (id<TAB>choices line, question, blank)
    python -I tools\\chunks.py merge                            <- data\\chunks\\aNN.csv (id,answer[,rule]) -> labelled.jsonl, generated.csv
    python -I tools\\chunks.py score FILE.csv GOLD.jsonl         an answer CSV against a gold set (the gate)
"""
import json, csv, os, sys, glob
from collections import Counter

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(HERE, "data")

def rows_of(p):
    return [json.loads(l) for l in open(p, encoding="utf-8")]

def write_questions(rows, p):
    with open(p, "w", encoding="utf-8") as f:
        for r in rows:
            f.write("%s\tchoices: %s\n%s\n\n" % (r["id"], ", ".join(r["choices"]), r["user"]))

def read_answers(p):
    out = {}
    with open(p, encoding="utf-8-sig", newline="") as f:
        for row in csv.reader(f):
            if not row or row[0].strip().lower() == "id": continue
            out[row[0].strip()] = (row[1].strip().strip("`'\"").lower(), row[2].strip() if len(row) > 2 else "")
    return out

def split(n):
    rows = rows_of(os.path.join(DATA, "generated.jsonl"))
    d = os.path.join(DATA, "chunks"); os.makedirs(d, exist_ok=True)
    k = 0
    for i in range(0, len(rows), n):
        write_questions(rows[i:i + n], os.path.join(d, "q%02d.txt" % k)); k += 1
    print("%d rows -> %d chunk files %s\\q00.txt .. q%02d.txt" % (len(rows), k, d, k - 1))

def merge():
    rows = rows_of(os.path.join(DATA, "generated.jsonl"))
    by_id = {r["id"]: r for r in rows}
    # a*.csv: the first labelling (all rows); b*.csv: relabels of the friendly and hunter rows under rules v3 (2026-11-13),
    # read after and so overriding
    ans, bad, files = {}, [], sorted(glob.glob(os.path.join(DATA, "chunks", "a*.csv"))) + sorted(glob.glob(os.path.join(DATA, "chunks", "b*.csv")))
    for p in files:
        for i, (a, rule) in read_answers(p).items():
            r = by_id.get(i)
            if not r: bad.append((os.path.basename(p), i, a, "unknown id")); continue
            if a not in r["choices"]: bad.append((os.path.basename(p), i, a, "not a choice: " + "/".join(r["choices"]))); continue
            ans[i] = (a, rule)
    out = os.path.join(DATA, "labelled.jsonl")
    with open(out, "w", encoding="utf-8") as f:
        for r in rows:
            if r["id"] in ans:
                r2 = dict(r, answer=ans[r["id"]][0], rule=ans[r["id"]][1], teacher="claude-sonnet-5-5 agents 2026-11-13")
                f.write(json.dumps(r2, ensure_ascii=False) + "\n")
    with open(os.path.join(DATA, "generated.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f); w.writerow(["id", "kind", "choices", "answer", "question"])
        for r in rows: w.writerow([r["id"], r["kind"], " | ".join(r["choices"]), ans.get(r["id"], ("", ""))[0], r["user"]])
    print("%d answer files; %d of %d rows labelled -> %s (and the answer column of generated.csv); %d rejected" % (
        len(files), len(ans), len(rows), out, len(bad)))
    for b in bad[:30]: print("  rejected:", b)
    print("answers by kind:", {k: dict(Counter(ans[r["id"]][0] for r in rows if r["kind"] == k and r["id"] in ans)) for k in sorted(set(r["kind"] for r in rows))})
    return len(bad)

def score(csv_path, gold_path):
    gold = rows_of(gold_path)
    ans = read_answers(csv_path)
    right, miss, byk = 0, [], {}
    for g in gold:
        a = ans.get(g["id"], ("", ""))[0]
        ok = a == g["answer"]; right += ok
        k = byk.setdefault(g["kind"], [0, 0]); k[0] += ok; k[1] += 1
        if not ok: miss.append("  %s: answered %-12s gold %-12s | %s" % (g["id"], a or "(none)", g["answer"], g["why"]))
    print("%d of %d right; by kind %s" % (right, len(gold), {k: "%d/%d" % tuple(v) for k, v in sorted(byk.items())}))
    print("\n".join(miss))
    return right

if __name__ == "__main__":
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(2)
    if a[0] == "split": split(int(a[1]) if len(a) > 1 else 200)
    elif a[0] == "merge": sys.exit(1 if merge() else 0)
    elif a[0] == "score": score(a[1], a[2])
    else: print(__doc__); sys.exit(2)
