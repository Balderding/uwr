"""hunter_policy.py (2026-11-14): the hunter's plan steps (kind 6, "Step N of your plan") answered by code, so that
every label follows one rule (rules v4, rule 8). The 2026-11-13 labels of these rows came from two rounds of agents
and disagreed with each other where the rulebook was silent (logs\\errors-350m-4.csv: 30 of 96 held-out rows missed,
most of them doubtful labels). The answer depends only on what the question's text says, so it works for the generated
rows, the gold rows and the game's own questions alike.

    python -I tools\\hunter_policy.py check                 the gold hunter steps, and the agreement with the old labels
    python -I tools\\hunter_policy.py build [N] [seed]      data\\labelled-3.jsonl: labelled-2's plan-step rows relabelled
                                                           (histories that go on after give_up dropped), plus N new
                                                           plan-step rows whose history is the policy's own plan so far

The user's three decisions (2026-11-14):
  1. a hunter of few wits goes straight to where the adventurer was last known and searches; no door, no kin;
  2. a wounded hunter recovers first (a swimmer in the water), unless the adventurer looked hurt at the last sighting;
  3. the freshest thing known to be the adventurer's (the last sighting, or a later noise or act of theirs) is what
     the hunter plans from; a light, something taken, a door opened, a trap or kin dead do not say where they are.
The plan, as a list; the answer is the step after the furthest one already taken:
  a goto_home, recover or goto_water step anywhere so far    -> give_up
  wounded, the adventurer not looking hurt                   -> goto_water if offered else recover; give_up
  nothing known of the adventurer, or the trail cold (5 minutes or more, or beyond 10 squares):
      a light noticed less than 5 minutes ago                -> follow_light, wait, then as below
      a swimmer                                              -> goto_water, give_up
      nothing known and a way blocked or the last plan lost him -> give_up
      otherwise                                              -> goto_home, give_up
  the trail fresh:
      good or great wits, kin near, not looking hurt         -> fetch_kin first
      goto_last not offered (known only from a noise)        -> wait, goto_home, give_up
      few wits, or within 3 squares, or looking hurt         -> goto_last, search, goto_home, give_up
      otherwise                                              -> goto_door, wait, goto_last, search, goto_home, give_up
"""
import json, os, re, sys, random
from collections import Counter
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import mind_format as F

DATA = os.path.join(os.path.dirname(HERE), "data")
HIS = {"fight", "weapon", "at_door", "portcullis", "spell", "lever", "landing", "noise", "dark"}   # the adventurer's own
BY_TEXT = {v: k for k, v in F.LEDGER.items()}
TERMINAL = ("goto_home", "recover", "goto_water")
WHERE = r"(right where you stand|(\d+) squares? to your [\w-]+)"
AGO = r"(\d+) (seconds|minutes?) ago"

def secs(n, unit):
    return int(n) * (60 if unit.startswith("minute") else 1)

def parse(user):
    """what a plan-step question says: None when it is not one"""
    if "of your plan (at most 4)" not in user: return None
    m = re.search(r"You are an? (.+?), (wounded|unhurt), of (few|good|great) wits\.", user)
    s = dict(name=m.group(1), wounded=m.group(2) == "wounded", wits=m.group(3), last=None, noticed=[], blocked=0, ended="", steps=[])
    m = re.search(AGO + r" you knew where the adventurer was: " + WHERE + r"(, looking ([a-z ]+))?\.", user)
    if m: s["last"] = (secs(m.group(1), m.group(2)), int(m.group(4) or 0), F.LOOKS.index(m.group(6)) if m.group(6) else None)
    for m in re.finditer(AGO + r" you noticed (.+?) " + WHERE + r"\.", user):
        s["noticed"].append((secs(m.group(1), m.group(2)), BY_TEXT[m.group(3)], int(m.group(5) or 0)))
    m = re.search(r"(\d+) ways? you tried", user)
    if m: s["blocked"] = int(m.group(1))
    m = re.search(r"Your last plan ended: ([a-z ]+)\.", user)
    if m: s["ended"] = m.group(1)
    s["steps"] = re.findall(r"step \d+ = (\w+)", user.split(" Choices:")[0])
    return s

def plan_of(s, offered):
    """the whole plan for the situation (a list ending in give_up); offered: every word the hunter may ever be offered
    here (the choices and the steps so far)"""
    last, wounded = s["last"], s["wounded"]
    hurt = bool(last and last[2] and last[0] < 300)            # looked hurt, badly hurt or near death, lately
    fix = (last[0], last[1]) if last else None                 # the freshest thing known to be the adventurer's
    for ago, key, d in s["noticed"]:
        if key in HIS and (fix is None or ago < fix[0]): fix = (ago, d)
    fresh = fix is not None and fix[0] < 300 and fix[1] <= 10
    if wounded and not (fresh and hurt):
        return ["goto_water" if "goto_water" in offered else "recover", "give_up"]
    if not fresh:
        p = ["follow_light", "wait"] if any(k == "light" and ago < 300 for ago, k, _ in s["noticed"]) else []
        if "goto_water" in offered: return p + ["goto_water", "give_up"]
        if not p and fix is None and (s["blocked"] or s["ended"] == "lost him"): return ["give_up"]
        return p + ["goto_home", "give_up"]
    p = ["fetch_kin"] if s["wits"] != "few" and not hurt and "fetch_kin" in offered else []
    if "goto_last" not in offered and "goto_last" not in s["steps"]: return p + ["wait", "goto_home", "give_up"]
    if s["wits"] == "few" or fix[1] <= 3 or hurt: return p + ["goto_last", "search", "goto_home", "give_up"]
    return p + ["goto_door", "wait", "goto_last", "search", "goto_home", "give_up"]

def answer(user, choices):
    """the policy's word for a plan-step question; None for other questions, or a history that went on after give_up"""
    s = parse(user)
    if s is None or "give_up" in s["steps"]: return None
    done = s["steps"]
    if any(t in done for t in TERMINAL): return "give_up"
    p = plan_of(s, set(choices) | set(done))
    at = max([i for i, w in enumerate(p) if w in done], default=-1)
    a = p[at + 1]
    assert a in choices, (a, choices, user)
    return a

def rows_of(p):
    return [json.loads(l) for l in open(p, encoding="utf-8")]

def check():
    gold = rows_of(os.path.join(DATA, "gold.jsonl"))
    n = ok = 0
    for g in gold:
        a = answer(g["user"], g["choices"])
        if a is None: continue
        n += 1; ok += a == g["answer"]
        if a != g["answer"]: print("  gold miss %s: policy %s, gold %s | %s" % (g["id"], a, g["answer"], g.get("why", "")))
    print("gold plan steps: %d of %d as the policy" % (ok, n))
    rows = rows_of(os.path.join(DATA, "labelled-2.jsonl"))
    n = same = dropped = 0
    moved = Counter()
    for r in rows:
        if r["kind"] != 6 or parse(r["user"]) is None: continue
        a = answer(r["user"], r["choices"])
        if a is None: dropped += 1; continue
        n += 1; same += a == r["answer"]
        if a != r["answer"]: moved[(r["answer"], a)] += 1
    print("labelled-2 plan steps: %d kept (%d as before, %d changed), %d dropped (a history past give_up)" % (n, same, n - same, dropped))
    for (was, now), c in moved.most_common(15): print("  %4d  %s -> %s" % (c, was, now))

def new_rows(n, seed, skip):
    """n plan-step rows whose steps so far are the policy's own plan (as the game has them: the model answers each
    step in turn), the situations from the generator (mind_situations.py, hunters only), picked so that the ten step
    words are answered equally often"""
    import mind_situations as S
    r = random.Random(seed)
    C = F.creatures(os.path.join(DATA, "OBJECTS.DAT"))
    swims = {v["name"]: v["swims"] for v in C.values()}
    out, seen, tries = [], set(skip), 0
    got, quota = Counter(), n // 10 + 1    # the ten step words in equal shares (the generator alone gives mostly give_up
                                           # and goto_home, and few doors, waits, kin and lights); after 150 tries, any
    while len(out) < n:
        tries += 1
        for g in S.generate(os.path.join(DATA, "OBJECTS.DAT"), 2000, seed * 1000 + tries, weights=[(6, 100)], prefix="x", bias=0.3):
            st = g["state"]
            if "steps" not in st or len(out) >= n: continue
            wounded, last, noticed = st["hp"] < st["avghit"], st["last"], [tuple(x) for x in st["noticed"]]
            allowed = sorted(set([3, 5, 10] + ([1, 2, 6] if last else []) + ([4] if swims[st["name"]] else []) + ([7] if st["kin"] else [])
                                 + ([8] if any(k == "light" for _, k, _, _ in noticed) else []) + ([9] if wounded else [])))
            facts = F.hunter_facts(tuple(last) if last else None, noticed, st["blocked"], st["ended"], st["look"])
            steps = []
            for _ in range(S.pick(r, [(0, 20), (1, 30), (2, 30), (3, 20)])):
                user, choices = F.plan_step(st["name"], st["wits"], wounded, facts, steps, allowed)
                a = answer(user, choices)
                if a == "give_up": break
                steps.append(F.STEP_WORDS.index(a))
            user, choices = F.plan_step(st["name"], st["wits"], wounded, facts, steps, allowed)
            head = user.rsplit("Choices: ", 1)[0]
            a = answer(user, choices)
            if head in seen or (got[a] >= quota and tries <= 150): continue
            seen.add(head); got[a] += 1
            user, order = S.shuffled(r, user, choices)
            out.append(dict(id="p%05d" % len(out), kind=6, user=user, choices=order, state=dict(st, steps=steps), source="generated"))
    return out

def build(n, seed):
    rows = rows_of(os.path.join(DATA, "labelled-2.jsonl"))
    teacher = "tools\\hunter_policy.py 2026-11-14 (rules v4)"
    out, changed, dropped = [], 0, 0
    gold_heads = {g["user"].rsplit("Choices: ", 1)[0] for g in rows_of(os.path.join(DATA, "gold.jsonl"))}
    n0 = len(rows)
    rows = [r for r in rows if r["user"].rsplit("Choices: ", 1)[0] not in gold_heads]   # (3 rows asked a gold question)
    print("%d rows that repeat a gold question left out" % (n0 - len(rows)))
    for r in rows:
        if r["kind"] == 6 and parse(r["user"]) is not None:
            a = answer(r["user"], r["choices"])
            if a is None: dropped += 1; continue
            if a != r["answer"]: changed += 1
            r = dict(r, answer=a, rule=8, teacher=teacher)
        out.append(r)
    heads = {r["user"].rsplit("Choices: ", 1)[0] for r in out}
    fresh = new_rows(n, seed, heads | gold_heads)
    for r in fresh: out.append(dict(r, answer=answer(r["user"], r["choices"]), rule=8, teacher=teacher))
    p = os.path.join(DATA, "labelled-3.jsonl")
    with open(p, "w", encoding="utf-8") as f:
        for r in out: f.write(json.dumps(r, ensure_ascii=False) + "\n")
    steps = [r for r in out if r["kind"] == 6 and parse(r["user"]) is not None]
    print("%d rows -> %s: %d plan-step labels changed, %d rows dropped, %d new rows" % (len(out), p, changed, dropped, len(fresh)))
    print("kinds:", dict(sorted(Counter(r["kind"] for r in out).items())))
    print("plan-step answers:", dict(Counter(r["answer"] for r in steps).most_common()))
    print("plan-step number:", dict(sorted(Counter(len(parse(r["user"])["steps"]) + 1 for r in steps).items())))

if __name__ == "__main__":
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(2)
    if a[0] == "check": check()
    elif a[0] == "build": build(int(a[1]) if len(a) > 1 else 800, int(a[2]) if len(a) > 2 else 3)
