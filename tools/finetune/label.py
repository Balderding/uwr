"""label.py: the teacher's answers for the creature situations (the fine-tune plan, step 2). Each row is asked twice,
with the choices in two different orders; a row is kept only when both answers agree (the decision-model projects'
lesson: disagreement marks an ambiguous situation). The teacher is any OpenAI-compatible chat server: the local
llama-server on :8081 by default.

    python label.py IN.jsonl OUT.jsonl [--url http://127.0.0.1:8081/v1/chat/completions] [--model NAME]
                    [--limit N] [--think 0|1] [--resume]

Rows out: the input row plus "answer" (the choice word), "teacher" (the model name), "passes" ([a1, a2]).
Dropped rows go to OUT.dropped.jsonl with both answers. --resume skips ids already in OUT.
"""
import json, sys, os, time, random, urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # mind_format.py beside this file
import mind_format as F

RUBRIC = ("You are the referee of a dungeon game, judging what one creature should do. You know the game's rules: a "
          "creature knows only what it saw and heard itself; hostile creatures want the adventurer dead but also want to "
          "live; a badly wounded creature (under a third of its health) should not pick a fight it can lose and prefers "
          "to flee, recover or lie in wait; an unhurt creature that knows where the adventurer is attacks; one that only "
          "heard a noise it cannot place investigates (if it has the wits) rather than charging; an ambusher with wits "
          "prefers ambush where the adventurer will come to it (lost sight nearby, a door, a corridor); kin within hearing "
          "make rallying worth it against a strong foe, and a creature with kin fighting nearby joins them; dim-witted "
          "creatures act on instinct (attack what they sense, flee when hurt); a creature that lost the adventurer long "
          "ago or far away returns to its post. A friendly creature helps the adventurer: greets when near, warns of an "
          "enemy it can see, follows if it has the wits and is not busy, keeps away when it is on its way home or the "
          "adventurer is far. For a hunter's plan, choose the step that most likely finds the adventurer given the facts "
          "(go to where they were last known or search there if that is recent; go toward a light; fetch kin against a "
          "strong foe; recover when wounded; give up only when the trail is cold). "
          "Answer with exactly one line of the form goal=<choice>, choosing one of the choices offered, nothing else.")

def ask(url, model, user, think):
    body = {"model": model, "messages": [{"role": "system", "content": RUBRIC + "\n\nThe creature's own instructions were: " + F.SYSTEM},
                                         {"role": "user", "content": user}],
            "temperature": 0.0, "max_tokens": 4000 if think else 40}
    if not think:
        body["chat_template_kwargs"] = {"enable_thinking": False}
    req = urllib.request.Request(url, json.dumps(body).encode(), {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=600) as r:
        j = json.load(r)
    text = j["choices"][0]["message"]["content"] or ""
    if "</think>" in text: text = text.rsplit("</think>", 1)[1]
    for line in reversed([l.strip() for l in text.splitlines() if l.strip()]):
        if "goal=" in line:
            return line.split("goal=", 1)[1].strip().strip(".").split()[0].strip("`*\"'"), text
    return text.strip().split()[-1] if text.strip() else "", text

def reorder(user, choices):
    """the text's choices in another order (mind_situations.shuffled's parsing; the row's list unchanged)"""
    if "Choices: " not in user: return user
    head, tail = user.rsplit("Choices: ", 1)
    items = tail.rstrip(".").split("), ")
    items = [it if it.endswith(")") else it + ")" for it in items]
    by_word = {it.split(" (", 1)[0]: it for it in items}
    if set(by_word) != set(choices): return user
    order = choices[:]
    random.shuffle(order)
    if order == choices: order.reverse()
    return head + "Choices: " + ", ".join(by_word[w] for w in order) + "."

def main():
    a = sys.argv[1:]
    if len(a) < 2: print(__doc__); return 2
    src, out = a[0], a[1]
    url = a[a.index("--url") + 1] if "--url" in a else "http://127.0.0.1:8081/v1/chat/completions"
    model = a[a.index("--model") + 1] if "--model" in a else "local"
    limit = int(a[a.index("--limit") + 1]) if "--limit" in a else 10 ** 9
    think = int(a[a.index("--think") + 1]) if "--think" in a else 1
    done = set()
    if "--resume" in a and os.path.exists(out):
        done = {json.loads(l)["id"] for l in open(out, encoding="utf-8")}
    random.seed(7)
    rows = [json.loads(l) for l in open(src, encoding="utf-8")]
    kept = dropped = 0
    t0 = time.time()
    with open(out, "a", encoding="utf-8") as fo, open(out.replace(".jsonl", ".dropped.jsonl"), "a", encoding="utf-8") as fd:
        for i, r in enumerate(rows[:limit]):
            if r["id"] in done: continue
            a1, _ = ask(url, model, r["user"], think)
            a2, _ = ask(url, model, reorder(r["user"], r["choices"]), think)
            ok = a1 == a2 and a1 in r["choices"]
            r = dict(r, teacher=model, passes=[a1, a2])
            if ok:
                r["answer"] = a1
                fo.write(json.dumps(r, ensure_ascii=False) + "\n"); fo.flush(); kept += 1
            else:
                fd.write(json.dumps(r, ensure_ascii=False) + "\n"); fd.flush(); dropped += 1
            if (kept + dropped) % 50 == 0:
                print("%d asked, %d kept, %d dropped, %.1f s each" % (kept + dropped, kept, dropped, (time.time() - t0) / (kept + dropped)), flush=True)
    print("done: %d kept, %d dropped -> %s" % (kept, dropped, out))
    return 0

if __name__ == "__main__":
    sys.exit(main())
