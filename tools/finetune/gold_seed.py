"""gold_seed.py: a first hand-written gold set (the plan's step 2, source 3): situations whose right goal is clear,
built with the game's own builders (mind_format.py) so the text is exactly the game's, each with the gold answer and
one line why. Never trained on; the only number the evaluation trusts. The user reviews and extends this
(data\\gold.jsonl is written from here; edit this file, not the JSONL).

    python gold_seed.py data\\gold.jsonl
"""
import json, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # mind_format.py beside this file
import mind_format as F

G = []
def add(kind, answer, why, built, **state):
    user, choices = built
    G.append(dict(id="gold%03d" % len(G), kind=kind, user=user, choices=choices, answer=answer, why=why, state=state, source="gold"))

# kind 4: the player seen
add(4, "attack", "unhurt, average wits, player close: the rules say attack at once",
    F.sees_player("goblin", 10, 2, 0, hp=27, avghit=27, ambusher=True))
add(4, "flee", "badly wounded (3 of 27), no kin: do not pick a fight you lose",
    F.sees_player("goblin", 10, 2, 0, hp=3, avghit=27, ambusher=True))
add(4, "attack", "badly wounded but 3 kin fighting beside it: join the fight",
    F.sees_player("goblin", 10, 2, 0, hp=5, avghit=27, ambusher=True, known=F.facts_words(kin=3, fighting=3)))
add(4, "attack", "dim-witted rat, unhurt: instinct, attack what it sees",
    F.sees_player("rat", 2, 1, 4, hp=6, avghit=6))
add(4, "flee", "dim-witted and badly hurt: instinct says run",
    F.sees_player("rat", 2, 1, 4, hp=1, avghit=6))
add(4, "attack", "a troll at full health, the player at 1 square: fight",
    F.sees_player("troll", 8, 1, 2, hp=70, avghit=70))
add(4, "ambush", "a clever ambusher, lightly hurt, the player 8 squares off: pull back into the dark and strike when they come",
    F.sees_player("fighter", 20, 8, 6, hp=20, avghit=40, ambusher=True))
add(4, "attack", "brilliant mage, unhurt, the player 3 squares away: attack",
    F.sees_player("mage", 25, 3, 1, hp=30, avghit=30))

# kind 1: lost sight of the player
add(1, "hunt", "unhurt, lost them 2 squares away a moment ago: go and search",
    F.lost_player("lizardman", 14, 2, 3, hp=25, avghit=25, ambusher=True))
add(1, "return", "lost them 14 squares away, last seen 9 minutes ago: the trail is cold, go back",
    F.lost_player("lizardman", 14, 14, 3, hp=25, avghit=25, ambusher=True, known=F.facts_words(seen_ago=540)))
add(1, "ambush", "a wounded (not badly) ambusher lost them close by: do not chase into their weapon, lie in wait (rule 5)",
    F.lost_player("giant spider", 7, 3, 0, hp=12, avghit=20, ambusher=True))
add(1, "rally", "clever fighter with 3 kin within hearing: call them and go together",
    F.lost_player("fighter", 20, 4, 5, hp=40, avghit=40, ambusher=True, known=F.facts_words(kin=3), kin=3))
add(1, "return", "badly wounded, alone: give up and go back",
    F.lost_player("goblin", 10, 3, 2, hp=4, avghit=27, ambusher=True))
add(1, "hunt", "dim-witted worm: no ambush on offer, unhurt, close: hunt",
    F.lost_player("worm", 1, 1, 0, hp=10, avghit=10))

# kind 2: on guard, a noise or a light
add(2, "investigate", "heard a door 5 squares away, cannot tell who: look carefully",
    F.on_guard("goblin", 10, 5, 2, what=F.NOISES["door"], hp=27, avghit=27, ambusher=True))
add(2, "attack", "heard the adventurer's weapon striking (it is the player) 2 squares away, unhurt: attack",
    F.on_guard("goblin", 10, 2, 2, what=F.PLAYER_NOISES["fight"], hp=27, avghit=27, ambusher=True))
add(2, "ignore", "badly wounded, a noise 8 squares away: stay put",
    F.on_guard("goblin", 10, 8, 2, what=F.NOISES["noise"], hp=3, avghit=27, ambusher=True))
add(2, "attack", "dim-witted slug, a noise next to it: instinct",
    F.on_guard("slug", 0, 1, 0, what=F.NOISES["noise"], hp=5, avghit=5))
add(2, "ambush", "an ambusher heard the player working a door 4 squares away: wait by the door in the dark",
    F.on_guard("giant spider", 7, 4, 6, what=F.PLAYER_NOISES["door"], door=True, hp=20, avghit=20, ambusher=True))
add(2, "investigate", "the sounds of a fight 6 squares away, not known to be the adventurer: look first (rule 5 needs the adventurer known); rally would need the adventurer's own noise",
    F.on_guard("fighter", 20, 6, 1, what=F.NOISES["fight"], hp=40, avghit=40, ambusher=True, known=F.facts_words(kin=2), kin=2))
add(2, "investigate", "saw a torch's light 6 squares away, unhurt, average wits: go and look",
    F.on_guard("lizardman", 14, 6, 7, light=3, hp=25, avghit=25, ambusher=True))
add(2, "ignore", "a candle's light 10 squares off and badly wounded: stay",
    F.on_guard("lizardman", 14, 10, 7, light=2, hp=4, avghit=25, ambusher=True))
add(2, "attack", "the adventurer swinging a weapon right where it stands (0 squares), unhurt troll: attack",
    F.on_guard("troll", 8, 0, 0, what=F.PLAYER_NOISES["weapon"], hp=70, avghit=70))

# kind 3: a friendly creature
add(3, "greet", "friendly, average wits, milling about, the player 2 squares away: greet",
    F.friendly("goblin", 10, 2, 4))
add(3, "warn", "friendly and clever, an enemy in sight: warn the player",
    F.friendly("goblin", 14, 3, 4, foe="giant spider", foe_d=5, foe_dir=2))
add(3, "keep_away", "on its way home with the player 9 squares off: keep going home",
    F.friendly("mountainman", 10, 9, 1, doing="home"))
add(3, "ignore", "dim-witted and the player far off: carry on",
    F.friendly("goblin", 4, 8, 3, doing="wander"))
add(3, "greet", "a named friendly: greet is a nod where it stands",
    F.friendly("outcast", 14, 2, 0, doing="stand", named=True))
add(3, "warn", "a named friendly that can see an enemy: warn",
    F.friendly("outcast", 14, 2, 0, doing="stand", foe="ghoul", foe_d=4, foe_dir=6, named=True))

# kind 6: a hunter's plan
add(6, "goto_last", "knew where the adventurer was 20 seconds ago, 2 squares away, unhurt: go there",
    F.plan_step("goblin", 10, False, F.hunter_facts(last=(20, 2, 2)), [], [1, 2, 3, 5, 6, 10]))
add(6, "goto_door", "rule 8: last known 6 squares away 40 seconds ago, unhurt: take the door on the way as a chokepoint",
    F.plan_step("goblin", 10, False, F.hunter_facts(last=(40, 6, 2)), [], [1, 2, 3, 5, 6, 10]))
add(6, "wait", "rule 8: at the door (step 1 = goto_door): lie in wait there",
    F.plan_step("goblin", 10, False, F.hunter_facts(last=(60, 6, 2)), [2], [1, 2, 3, 5, 6, 10]))
add(6, "goto_water", "rule 8: a wounded swimmer: hide in the water",
    F.plan_step("lizardman", 14, True, F.hunter_facts(last=(90, 5, 1)), [], [1, 2, 3, 4, 5, 6, 9, 10]))
add(3, "follow", "rule 9: wandering about, the player 4 squares off: go along with them",
    F.friendly("goblin", 10, 4, 6, doing="wander"))
add(6, "search", "already went to the last known square (step 1); now search around it",
    F.plan_step("goblin", 10, False, F.hunter_facts(last=(40, 1, 2)), [1], [1, 2, 3, 5, 6, 10]))
add(6, "recover", "wounded, the trail 20 minutes cold: go home and rest",
    F.plan_step("troll", 8, True, F.hunter_facts(last=(1200, 18, 5), ended=2), [], [1, 2, 3, 5, 6, 9, 10]))
add(6, "follow_light", "a light noticed 10 seconds ago 6 squares away, nothing else known: go toward it",
    F.plan_step("lizardman", 14, False, F.hunter_facts(noticed=[(10, "light", 6, 0)]), [], [3, 5, 8, 10]))
add(6, "fetch_kin", "the adventurer fighting nearby, kin within hearing, a fighter of good wits: fetch kin first",
    F.plan_step("fighter", 20, False, F.hunter_facts(last=(15, 5, 3), noticed=[(15, "fight", 5, 3)]), [], [1, 2, 3, 5, 6, 7, 10]))
add(6, "give_up", "nothing known, last plan lost him, two ways blocked: give up",
    F.plan_step("goblin", 10, False, F.hunter_facts(blocked=2, ended=2), [3], [3, 5, 10]))
add(6, "rush_in", "on a wait step, just noticed the adventurer casting a spell 3 squares away (busy, not on guard): go in now",
    F.reconsider("goblin", False, [1, 5], 2, F.hunter_facts(last=(30, 3, 1)), "spell", 3, 1))
add(6, "keep_plan", "on the way to the last known square, noticed a door opened far off: carry on",
    F.reconsider("goblin", False, [1, 6], 1, F.hunter_facts(last=(30, 3, 1)), "door", 14, 5))
add(6, "new_plan", "wounded, on a search step, noticed one of its kin dead close by: think again",
    F.reconsider("goblin", True, [1, 6], 2, F.hunter_facts(last=(30, 3, 1)), "kin_dead", 2, 3))

# FORMAT 2: what the creature sees of the adventurer (player_words)
add(4, "attack", "rule 3: the adventurer looks near death; a wounded (not badly) goblin attacks anyway",
    F.sees_player("goblin", 10, 3, 2, hp=14, avghit=27, ambusher=True, look=3, drawn=1, light=3))
add(4, "flee", "rule 1 before rule 3: badly wounded itself, alone; even a hurt adventurer is not worth dying for",
    F.sees_player("goblin", 10, 3, 2, hp=4, avghit=27, ambusher=True, look=2, drawn=1, light=3))
add(4, "ambush", "rule 5: the adventurer carries a bright lantern into the dark 7 squares off; the ambusher waits",
    F.sees_player("lizardman", 14, 7, 6, hp=25, avghit=25, ambusher=True, look=0, drawn=1, light=4))
add(4, "attack", "rule 6: unhurt troll, the adventurer 2 squares away with no weapon in hand: attack",
    F.sees_player("troll", 8, 2, 0, hp=70, avghit=70, look=0, drawn=0, light=3))
add(1, "hunt", "rule 3: lost sight of them close by and they looked badly hurt when last seen: press on",
    F.lost_player("goblin", 10, 2, 4, hp=20, avghit=27, ambusher=True, look=2, drawn=1, light=3))
add(1, "ambush", "rule 5: an unhurt adventurer with a weapon drawn walked into the dark 6 squares off; lie in wait",
    F.lost_player("fighter", 20, 6, 1, hp=40, avghit=40, ambusher=True, look=0, drawn=1, light=0))
add(6, "goto_last", "rule 8 and 3: knew where they were 15 seconds ago, 3 squares, and they looked near death: go",
    F.plan_step("troll", 8, True, F.hunter_facts(last=(15, 3, 2), look=3), [], [1, 2, 3, 5, 6, 9, 10]))
add(6, "rush_in", "rule 8: on the way there, noticed the adventurer at a door 2 squares off; last seen looking badly hurt",
    F.reconsider("lizardman", False, [1, 6], 1, F.hunter_facts(last=(20, 3, 1), look=2), "at_door", 2, 1))

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "data/gold.jsonl"
    with open(out, "w", encoding="utf-8") as f:
        for g in G: f.write(json.dumps(g, ensure_ascii=False) + "\n")
    print("%d gold rows -> %s" % (len(G), out))
