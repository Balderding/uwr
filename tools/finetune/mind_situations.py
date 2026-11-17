"""mind_situations.py: generated creature situations for the fine-tune (the plan's step 2, source 1): every question
kind the game asks (mind_format.py's builders), over the state fields the game can fill, weighted as play has them
(most questions are noises and lost sights; health mostly full), the choices listed in a random order per row (the
decision-model lesson: a fixed order teaches "the first is right"). Writes JSONL rows:
  {"id", "kind", "user", "choices": [in the order shown], "state": {...}, "source": "generated"}
No label here: label.py (the teacher) adds "answer".

    python -I tools\\mind_situations.py OBJECTS.DAT OUT.jsonl [N] [seed]
"""
import json, random, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mind_format as F

def pick(r, pairs):
    """a weighted choice: pairs of (value, weight)"""
    t = sum(w for _, w in pairs)
    x = r.random() * t
    for v, w in pairs:
        x -= w
        if x <= 0: return v
    return pairs[-1][0]

def health(r, avghit):
    """hp, avghit: unhurt mostly; wounded at a fifth of the rows, badly at a tenth"""
    k = pick(r, [("full", 70), ("light", 15), ("bad", 15)])
    if k == "full" or not avghit: return avghit, avghit
    if k == "light": return max(1, int(avghit * r.uniform(0.5, 0.95))), avghit
    return max(1, int(avghit * r.uniform(0.05, 0.4))), avghit

def known(r, kin_max=4):
    """hunt.c facts(): a memory, the last sight, kin within hearing; returns (text, kin)"""
    mem = pick(r, [(None, 70), ("you heard the adventurer's weapon striking", 10), ("you saw a torch's light", 10),
                   ("one of your kin died", 10)])
    seen = pick(r, [(None, 60), (r.randint(2, 60), 25), (r.randint(60, 590), 15)])
    kin = pick(r, [(0, 55), (1, 20), (2, 15), (3, 7), (4, 3)])
    kin = min(kin, kin_max)
    fighting = 0 if not kin else pick(r, [(0, 60), (kin, 25), (r.randint(1, kin), 15)])
    return F.facts_words(mem, r.randint(5, 290) if mem else 0, seen, kin, fighting), kin

def seen_player(r):
    """FORMAT 2: how the adventurer looks to a creature that sees them: (look 0-3, weapon drawn, light reach 0-4);
    mostly unhurt and armed with a torch, as play has it"""
    return (pick(r, [(0, 55), (1, 25), (2, 13), (3, 7)]), pick(r, [(1, 75), (0, 25)]), pick(r, [(0, 15), (2, 15), (3, 55), (4, 15)]))

def shuffled(r, user, choices):
    """the choices' order in the text as listed; the row's choices list in a random order (the text's order stays the
    game's: the model must be right whatever order the list says, so the label is the word, not a position). Half
    the rows also reorder the text's choices, so neither the text's order nor the list's is a clue."""
    order = choices[:]
    r.shuffle(order)
    if r.random() < 0.5 and "Choices: " in user:
        head, tail = user.rsplit("Choices: ", 1)
        items = tail.rstrip(".").split("), ")
        items = [it if it.endswith(")") else it + ")" for it in items]
        by_word = {it.split(" (", 1)[0]: it for it in items}
        if set(by_word) == set(choices):
            user = head + "Choices: " + ", ".join(by_word[w] for w in order) + "."
    return user, order

def generate(objects_dat, n, seed, weights=None, prefix="g", skip=(), bias=0.0):
    """weights: the kinds' weights (default: as play has them); prefix: the ids' letter; skip: question heads (the text
    before "Choices: ") already in another file; bias: the share of hunter plan rows pushed toward the thin answers
    (later steps, a noticed light, swimmers: the handoff's remedy). The defaults give the first 4,000 rows unchanged."""
    r = random.Random(seed)
    C = F.creatures(objects_dat)
    hostile = [v for v in C.values() if v["name"] not in ("worm", "slug")] + [v for v in C.values()]
    rows, seen_users = [], set()
    swimmers = [v for v in C.values() if v["swims"]]
    while len(rows) < n:
        kind = pick(r, weights or [(2, 40), (1, 25), (4, 15), (3, 10), (6, 10)])
        c = r.choice(hostile)
        push = bias and kind == 6 and r.random() < bias
        if push and swimmers and r.random() < 0.3: c = r.choice(swimmers)
        name, wits, amb = c["name"], c["wits"], c["ambusher"]
        d, direction = pick(r, [(0, 3), (1, 10), (2, 15), (3, 20), (4, 15), (5, 12), (6, 10), (8, 8), (10, 5), (14, 2)]), r.randrange(8)
        hp, avghit = health(r, c["avghit"])
        kn, kin = known(r)
        state = dict(name=name, wits=wits, hp=hp, avghit=avghit, ambusher=amb, d=d, dir=direction, kin=kin)
        if kind == 1:
            look, drawn, light = seen_player(r)
            state.update(look=look, drawn=drawn, plight=light)
            user, choices = F.lost_player(name, wits, d, direction, hp, avghit, amb, kn, kin, look, drawn, light)
        elif kind == 2:
            light = pick(r, [(0, 75), (2, 8), (3, 12), (4, 5)])
            what = None
            if not light:
                src = pick(r, [("player", 60), ("other", 40)])
                key = pick(r, [("fight", 30), ("weapon", 30), ("door", 20), ("noise", 10), ("portcullis", 4), ("spell", 3), ("lever", 2), ("landing", 1)])
                what = F.PLAYER_NOISES[key if key in F.PLAYER_NOISES else "noise"] if src == "player" else F.NOISES[key]
                state["noise"] = (src, key)
            state["light"] = light
            door = r.random() < 0.25
            state["door"] = door
            user, choices = F.on_guard(name, wits, d, direction, what, light, door, hp, avghit, amb, kn, kin)
        elif kind == 4:
            look, drawn, light = seen_player(r)
            state.update(look=look, drawn=drawn, plight=light)
            user, choices = F.sees_player(name, wits, max(d, 1), direction, hp, avghit, amb, kn, look, drawn, light)
        elif kind == 3:
            fname = r.choice(["goblin", "lizardman", "mountainman", "fighter", "outcast"])
            doing = pick(r, [("mill", 50), ("wander", 25), ("stand", 15), ("home", 10)])
            foe = pick(r, [(None, 70), (r.choice(["goblin", "giant spider", "skeleton", "ghoul", "troll", "lurker", "rat"]), 30)])
            named = r.random() < 0.15
            fd, fdir = r.randint(2, 9), r.randrange(8)
            state = dict(name=fname, wits=pick(r, [(10, 50), (14, 20), (20, 15), (4, 15)]), d=max(d, 1), dir=direction, doing=doing, foe=foe, named=named)
            user, choices = F.friendly(fname, state["wits"], max(d, 1), direction, doing, foe, fd, fdir, named)
        else:
            wounded = hp < avghit
            last = pick(r, [(None, 25), ((r.randint(3, 59), r.randint(1, 9), r.randrange(8)), 50), ((r.randint(60, 1500), r.randint(2, 20), r.randrange(8)), 25)])
            keys = list(F.LEDGER)
            noticed = [(r.randint(3, 900), r.choice(keys), r.randint(1, 15), r.randrange(8)) for _ in range(pick(r, [(0, 40), (1, 35), (2, 15), (3, 7), (4, 3)]))]
            if push and r.random() < 0.35: noticed.append((r.randint(3, 240), "light", r.randint(1, 15), r.randrange(8)))
            noticed.sort()
            blocked, ended = pick(r, [(0, 80), (1, 15), (2, 5)]), pick(r, [(0, 60), (1, 10), (2, 15), (3, 7), (4, 4), (5, 4)])
            look = pick(r, [(None, 40), (0, 30), (1, 15), (2, 10), (3, 5)]) if last else None
            state["look"] = look
            facts = F.hunter_facts(last, noticed, blocked, ended, look)
            if r.random() < 0.8:
                allowed = [3, 5, 10] + ([1, 2, 6] if last else []) + ([4] if c["swims"] else []) + ([7] if kin else []) \
                          + ([8] if any(k == "light" for _, k, _, _ in noticed) else []) + ([9] if wounded else [])
                steps = [r.choice(allowed) for _ in range(pick(r, [(0, 20), (1, 45), (2, 25), (3, 10)] if push else [(0, 50), (1, 30), (2, 15), (3, 5)]))]
                allowed = sorted(set(allowed))
                user, choices = F.plan_step(name, wits, wounded, facts, steps, allowed)
                state.update(steps=steps, last=last, noticed=noticed, blocked=blocked, ended=ended)
            else:
                plan = [r.choice([1, 2, 5, 6, 3]) for _ in range(r.randint(1, 4))]
                step_no = r.randint(1, len(plan))
                nk = r.choice(keys)
                user, choices = F.reconsider(name, wounded, plan, step_no, facts, nk, r.randint(1, 12), r.randrange(8))
                state.update(plan=plan, step_no=step_no, noticed=nk)
        if user in seen_users or user.rsplit("Choices: ", 1)[0] in skip: continue
        seen_users.add(user)
        user, order = shuffled(r, user, choices)
        rows.append(dict(id="%s%05d" % (prefix, len(rows)), kind=kind, user=user, choices=order, state=state, source="generated"))
    return rows

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(2)
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 4000
    seed = int(sys.argv[4]) if len(sys.argv) > 4 else 1
    rows = generate(sys.argv[1], n, seed)
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        for row in rows: f.write(json.dumps(row, ensure_ascii=False) + "\n")
    from collections import Counter
    print("%d rows -> %s; kinds %s; choices per row %.2f" % (len(rows), sys.argv[2], dict(Counter(r["kind"] for r in rows)),
                                                             sum(len(r["choices"]) for r in rows) / len(rows)))
