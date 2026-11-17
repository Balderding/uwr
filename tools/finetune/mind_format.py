"""mind_format.py (FORMAT 2, the fine-tune folder's own copy, 2026-11-13): the creature questions for the fine-tune.
FORMAT 2 differs from what the game sends today (uo-new tools\\mind_format.py, FORMAT 1) in two ways, both still to be
put into the game (hunt.c, hunter.c, uwmindhost.c) when the user says so: the rules text (SYSTEM) is tightened, and a
creature that sees or saw the player is told how the adventurer looks (wounds, weapon, light: player_words).

The game (underworld-exhumed port\\sys\\hunt.c mind_ask / friend_ask / seen_ask, hunter.c plan_ask / recon_ask) builds
one `user` string a question; port\\win32\\uwmindhost.c holds the standing instructions (SYSTEM); src\\uwmind.c renders
the model's chat template around them (llama.cpp: LFM2.5's is ChatML with the BOS token first), appends "goal=" after
the assistant's start and scores each choice by its first token. Training must use the same bytes, so every string
here is copied from those files, and self_test() checks the builders against the questions tests\\uwmind_score.c
carries (which were copied from the game's log).

    python -I tools\\mind_format.py            runs the self-test
    from mind_format import *                   the builders (used by mind_situations.py, the trainer, the labeller)
"""
import struct, os, sys

# port\win32\uwmindhost.c SYSTEM (tests\uwmind_score.c has the same)
FORMAT = 2   # rules v4 (2026-11-14): rule 8's plan steps as tools\hunter_policy.py answers them
SYSTEM = ("You decide what one creature in a dark dungeon does next. You know only what the creature itself saw and heard. "
          "Badly wounded means under a third of its health. Rules; the first that fits wins. "
          "1. Badly wounded, with no kin fighting beside it (kin within hearing that are fighting count as beside it): flee, return, recover or stay put; never attack, hunt or ambush. "
          "2. The dim-witted act on instinct: attack or hunt what is near, flee when hurt, else carry on; they never ambush, rally or investigate. "
          "3. An adventurer who looks badly hurt or near death is prey: attack, hunt or rush in, even when wounded (if not badly). "
          "4. A trail is cold after 5 minutes or beyond 10 squares: return, recover or give up; do not hunt or ambush. "
          "5. Kin within hearing that are not yet fighting (rally or fetch_kin is offered only when kin are near), average wits or better, and the adventurer known to be there (seen, lost from sight, or a noise known to be theirs): rally or fetch kin rather than go alone; kin already fighting within hearing: join them (attack, or hunt when the adventurer was lost from sight). "
          "6. When ambush is among the choices, take it if any one of these holds: the creature is wounded; the adventurer is known to be 6 squares or more away; the adventurer is known to be busy (at a door, a lever, a spell); the adventurer was seen carrying a light while the creature is in the dark. Not when the creature cannot tell who made a noise or carries a light. "
          "7. Otherwise a hostile creature that knows where the adventurer is attacks at once, or hunts when it lost sight of them close by a moment ago; one that cannot tell who made a noise or carries a light investigates. "
          "8. A hunter plans its steps in order from the freshest thing it knows to be the adventurer's: the last sighting, or a later noise or act of theirs (a light, something taken, a door opened, a trap or kin found dead do not say where the adventurer is). The trail is cold after 5 minutes or beyond 10 squares. Once the plan has a goto_home, recover or goto_water step, the next step is give_up. A wounded hunter, unless the adventurer looked hurt when last seen: goto_water when offered, else recover; then give_up. Nothing known of the adventurer, or the trail cold: follow_light and then wait when it noticed a light less than 5 minutes ago; then goto_water when offered, else give_up when nothing is known and a way was blocked or the last plan lost him, else goto_home; then give_up. The trail fresh: a hunter of good or great wits takes fetch_kin first when it is offered and the adventurer did not look hurt. Then a hunter of few wits, or the adventurer within 3 squares or looking hurt: goto_last, search, goto_home, give_up. Otherwise: goto_door (the nearest door on the way is a chokepoint), wait, goto_last, search, goto_home, give_up. When goto_last is not offered: wait, goto_home, give_up. Reconsidering: rush_in when the adventurer is busy or hurt close by (within 4 squares); new_plan when it is wounded, finds kin dead, or the adventurer's light went out; otherwise keep_plan. "
          "9. A friendly creature: warn when it can see an enemy of the player; greet when the player is within 2 squares; follow when the player is 3 to 6 squares off and it is wandering or milling about; keep_away when it is on its way home, or standing at its place with the player 3 squares or more off, or the player is 7 squares or more off; the dim-witted carry on (ignore), and a named one (offered only greet, warn, ignore) greets when within 4 squares. "
          "Answer only in the form goal=<choice>, with one of the choices offered.")

DIRS = ["north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west"]   # hunt.c DIRS, hunter.c DIR8

# hunt.c port_ai_name: the item codes of each kind the model is told about
NAMES = {0x46: "goblin", 0x47: "goblin", 0x4C: "goblin", 0x4D: "goblin", 0x4E: "goblin", 0x50: "goblin",
         0x54: "mountainman", 0x56: "mountainman", 0x55: "lizardman", 0x58: "lizardman", 0x59: "lizardman",
         0x5A: "outcast", 0x5D: "fighter", 0x5E: "fighter", 0x5F: "fighter", 0x62: "fighter", 0x68: "fighter",
         0x44: "giant spider", 0x53: "giant spider", 0x5C: "giant spider", 0x40: "worm", 0x52: "worm",
         0x41: "slug", 0x45: "slug", 0x42: "bat", 0x49: "bat", 0x51: "bat", 0x43: "rat", 0x48: "rat",
         0x4A: "skeleton", 0x4B: "imp", 0x57: "lurker", 0x74: "lurker", 0x5B: "headless",
         0x60: "troll", 0x6F: "troll", 0x70: "troll", 0x61: "ghost", 0x64: "ghost", 0x65: "ghost", 0x71: "ghost",
         0x63: "ghoul", 0x69: "ghoul", 0x6E: "ghoul", 0x66: "gazer", 0x67: "mage", 0x6A: "mage", 0x6B: "mage",
         0x6C: "mage", 0x6D: "mage", 0x73: "mage", 0x72: "golem", 0x77: "golem", 0x79: "golem",
         0x78: "fire elemental", 0x7A: "wisp"}
# hunter.c AMBUSHERS (hunt.c is_ambusher has the same set)
AMBUSHERS = {0x46, 0x47, 0x4C, 0x4D, 0x4E, 0x50, 0x54, 0x56, 0x55, 0x58, 0x59, 0x5A, 0x5D, 0x5E, 0x5F, 0x62, 0x68,
             0x44, 0x53, 0x5C}

def creatures(objects_dat):
    """the creature table of OBJECTS.DAT (critter.h struct Creature, 48 bytes each, 64 kinds from item 0x40, after
    the 2-byte header and the Weapons 128, Missile 48 and Armor 128 tables): {item: dict(name, avghit, wits, swims,
    flier, hearing)} for the kinds the model is told about"""
    d = open(objects_dat, 'rb').read()
    out = {}
    for i in range(64):
        r = d[2 + 128 + 48 + 128 + i * 48:][:48]
        item = 0x40 + i
        if item not in NAMES: continue
        out[item] = dict(name=NAMES[item], avghit=r[4], wits=r[7], swims=(r[0x0A] >> 6) & 1, flier=(r[0x0A] >> 7) & 1,
                         hearing=r[0x1E] & 15, ambusher=item in AMBUSHERS)
    return out

# ---- hunt.c ----
AMBUSH_WORDS = "ambush (hide in the dark near there and wait to strike), "
RALLY_WORDS = "rally (call your kin and go there together), "

def wits_words(i):
    return (" You are dim-witted and act on instinct." if i < 5 else " You have animal cunning." if i < 10
            else " You are of average wits." if i < 15 else " You are clever." if i < 22 else " You are brilliant.")

def where_words(d, direction):
    """mind_ask's `where`: d squares (0: right where you stand); direction 0-7 (DIRS)"""
    return "right where you stand" if d < 1 else "%d squares to your %s" % (d, DIRS[direction & 7])

def facts_words(mem=None, mem_ago=0, seen_ago=None, kin=0, fighting=0):
    """hunt.c facts(): the memory (within 300 s), when the player was last seen (within 600 s), the kin within
    hearing and how many are fighting"""
    s = ""
    if mem: s += " Earlier (%d seconds ago) %s." % (mem_ago, mem)
    if seen_ago is not None: s += " You last saw the adventurer %d seconds ago." % seen_ago
    if kin:
        s += " %d of your kin %s within hearing%s." % (kin, "is" if kin == 1 else "are",
                                                      (", fighting" if fighting == kin else ", some of them fighting") if fighting else "")
    return s

def who_words(name, wits, hp=None, avghit=None, ambusher=False, with_ambush=True):
    """mind_ask's `who` (with the ambush sentence) and seen_ask's (without it)"""
    amb = " You are good at ambushes." if ambusher and wits >= 5 and with_ambush else ""
    if hp is None or avghit is None or avghit == 0 or hp >= avghit:
        return "You are a %s, unhurt.%s You hate the adventurer and want to kill them.%s" % (name, wits_words(wits), amb)
    return ("You are a %s, wounded: health %d of %d.%s You hate the adventurer and want to kill them.%s"
            % (name, hp, avghit, wits_words(wits), amb))

LOOKS = ["unhurt", "hurt", "badly hurt", "near death"]        # the adventurer's health as seen: >= 3/4, >= 1/3, >= 1/8, less
def look_of(hp, maxhp):
    return 0 if hp * 4 >= maxhp * 3 else 1 if hp * 3 >= maxhp else 2 if hp * 8 >= maxhp else 3

def player_words(look, drawn, light, past=False):
    """what the creature sees of the adventurer (FORMAT 2): look 0-3 (LOOKS), weapon drawn, light 0 none or its reach 1-4.
    past: as remembered from the last sighting (kind 1)"""
    lt = "in the dark" if not light else "carrying %s" % LIGHTS[min(light, 4)]
    return " %s %s, %s, %s." % ("When you last saw them the adventurer looked" if past else "The adventurer looks", LOOKS[look],
                                "with a weapon drawn" if drawn else "no weapon in hand", lt)

def lost_player(name, wits, d, direction, hp=None, avghit=None, ambusher=False, known="", kin=0, look=0, drawn=1, light=3):
    """kind 1: lost sight of the player at d squares. Returns (user, choices in the order shown)"""
    amb, rally = ambusher and wits >= 5, kin > 0 and wits >= 10
    choices = "hunt (go where you last saw them and search), %s%sreturn (give up and go back to your post)" % (
        AMBUSH_WORDS if amb else "", RALLY_WORDS if rally else "")
    user = "%s You were fighting the player, an armed adventurer, and lost sight of them %s.%s%s Choices: %s." % (
        who_words(name, wits, hp, avghit, ambusher), where_words(d, direction), player_words(look, drawn, light, past=True), known, choices)
    return user, ["hunt"] + (["ambush"] if amb else []) + (["rally"] if rally else []) + ["return"]

NOISES = {"fight": "the sounds of a fight", "weapon": "a weapon swung or a missile loosed", "door": "a door",
          "portcullis": "a portcullis", "spell": "someone casting a spell", "lever": "a lever or switch",
          "landing": "something landing", "noise": "a noise"}
PLAYER_NOISES = {"fight": "the adventurer's weapon striking (the player)", "weapon": "the adventurer swinging a weapon (the player)",
                 "door": "the adventurer working a door (the player)", "noise": "the adventurer (the player) making a noise"}
LIGHTS = {1: "a candle", 2: "a candle", 3: "a torch", 4: "a bright lantern"}

def on_guard(name, wits, d, direction, what=None, light=0, door=False, hp=None, avghit=None, ambusher=False, known="", kin=0):
    """kind 2: a noise (`what`: one of NOISES' or PLAYER_NOISES' texts) or a light (light 1-4: its reach) at d squares"""
    amb, look, rally = ambusher and wits >= 5, wits >= 5, kin > 0 and wits >= 10
    choices = "attack (charge there at once and fight), %s%s%signore (stay where you are)" % (
        "investigate (go and look carefully), " if look else "", AMBUSH_WORDS if amb else "", RALLY_WORDS if rally else "")
    who, where = who_words(name, wits, hp, avghit, ambusher), where_words(d, direction)
    if light:
        user = "%s You are on guard. You saw the light of %s %s, but not who carries it.%s Choices: %s." % (
            who, LIGHTS[min(light, 4)], where, known, choices)
    else:
        user = "%s You are on guard. You heard %s %s%s, but saw no one.%s Choices: %s." % (
            who, what, where, ", beside a door" if door else "", known, choices)
    return user, ["attack"] + (["investigate"] if look else []) + (["ambush"] if amb else []) + (["rally"] if rally else []) + ["ignore"]

DOING = {"wander": "wandering about", "mill": "milling about near your home", "home": "on your way home", "stand": "standing at your place"}

def friendly(name, wits, d, direction, doing="mill", foe=None, foe_d=0, foe_dir=0, named=False):
    """kind 3: a friendly creature with the player d squares away; foe: an enemy's name it can see (a warning needs
    wits 10). Returns (user, the choices the grammar allows: the text always names greet)"""
    seen = foe is not None and wits >= 10
    foe_s = (" You can see a %s, an enemy of the player, %d squares to your %s." % (foe, foe_d, DIRS[foe_dir & 7])) if foe else ""
    user = ("You are a %s, friendly toward the player.%s You are %s. The player, an armed adventurer, is %s.%s "
            "Choices: %s%signore (carry on as you were)." % (
                name, wits_words(wits), DOING[doing], where_words(d, direction), foe_s,
                "greet (nod to the player where you stand), " if named
                else "greet (go up to the player and wait politely), follow (go along with the player for a while), keep_away (back off to your home), ",
                "warn (point out the enemy to the player), " if seen else ""))
    if named: choices = ["greet"] + (["warn"] if seen else []) + ["ignore"]
    elif seen: choices = ["greet", "follow", "keep_away", "warn", "ignore"]
    elif wits >= 5: choices = ["greet", "follow", "keep_away", "ignore"]
    else: choices = ["follow", "keep_away", "ignore"]
    return user, choices

def sees_player(name, wits, d, direction, hp=None, avghit=None, ambusher=False, known="", look=0, drawn=1, light=3):
    """kind 4: the creature has just seen the player d squares away (d 0 is said as 1); look/drawn/light: player_words"""
    amb = ambusher and wits >= 5
    user = "%s You see the adventurer, the player, %s.%s%s Choices: attack (fight them now), %sflee (run away)." % (
        who_words(name, wits, hp, avghit, ambusher, with_ambush=False), where_words(max(d, 1), direction), player_words(look, drawn, light), known,
        "ambush (pull back into the dark nearby and wait to strike), " if amb else "")
    return user, ["attack"] + (["ambush"] if amb else []) + ["flee"]

# ---- hunter.c (kind 6) ----
STEP_WORDS = ["", "goto_last", "goto_door", "goto_home", "goto_water", "wait", "search", "fetch_kin", "follow_light", "recover", "give_up"]
STEP_SAYS = ["", "go to where the adventurer was last known", "go to the nearest door on the way there", "go back to your post",
             "go into the nearest water", "lie in wait, facing the way they would come", "search around where they were last known",
             "fetch your kin nearby to help", "go toward the light you saw", "go home and rest until your wounds heal",
             "give up (the plan ends here)"]
END_WORDS = ["", "done", "lost him", "blocked", "wounded", "gave up"]
LEDGER = {"kin_dead": "one of your kin dead", "door": "a door opened", "taken": "something taken", "light": "a light",
          "dark": "the adventurer's light go out (they are in the dark now)", "trap": "a trap set off",
          "fight": "the sounds of the adventurer fighting (busy with a fight, maybe hurt)",
          "weapon": "the adventurer swing a weapon or loose a missile", "at_door": "the adventurer at a door",
          "portcullis": "the adventurer at a portcullis", "spell": "the adventurer casting a spell (busy, not on guard)",
          "lever": "the adventurer working a lever or switch", "landing": "the adventurer landing from a jump or a fall",
          "noise": "a noise the adventurer made"}

def hunter_where(d, direction):
    """hunter.c where_words (Chebyshev squares, the singular said)"""
    return "right where you stand" if d < 1 else "%d square%s to your %s" % (d, "" if d == 1 else "s", DIRS[direction & 7])

def ago_words(s):
    return "%d seconds ago" % max(s, 1) if s < 60 else "%d minute%s ago" % (s // 60, "" if s // 60 == 1 else "s")

def hunter_facts(last=None, noticed=(), blocked=0, ended=0, look=None):
    """hunter.c facts_words: last = (ago seconds, d, direction) of the last sighting (look: how the adventurer looked then, LOOKS index, FORMAT 2); noticed = up to 4 of
    (ago seconds, LEDGER key, d, direction), newest first; blocked ways; how the last plan ended (END_WORDS index)"""
    s = ""
    if last: s += " %s you knew where the adventurer was: %s%s." % (ago_words(last[0]), hunter_where(last[1], last[2]),
                                                                  (", looking %s" % LOOKS[look]) if look is not None else "")
    for ago, key, d, direction in noticed[:4]:
        s += " %s you noticed %s %s." % (ago_words(ago), LEDGER[key], hunter_where(d, direction))
    if blocked: s += " %d way%s you tried %s blocked." % (blocked, "" if blocked == 1 else "s", "was" if blocked == 1 else "were")
    if ended: s += " Your last plan ended: %s." % END_WORDS[ended]
    return s

def plan_step(name, wits, wounded, facts, steps_so_far, allowed):
    """kind 6: step len(steps_so_far) + 1 of a plan; allowed: the STEP_WORDS indices step_allowed lets through
    (the previous step is never offered again). Returns (user, choices)"""
    k = len(steps_so_far)
    sofar = "".join("%s step %d = %s%s" % ("," if i else " So far:", i + 1, STEP_WORDS[st], "." if i == k - 1 else "")
                    for i, st in enumerate(steps_so_far))
    offered = [i for i in allowed if not (k and i == steps_so_far[-1])]
    choices = ", ".join("%s (%s)" % (STEP_WORDS[i], STEP_SAYS[i]) for i in offered)
    user = ("You plan ahead: answer each step in turn. You are a %s%s, of %s wits. You hate the adventurer and hunt them "
            "on this level.%s%s Step %d of your plan (at most 4). Choices: %s." % (
                name, ", wounded" if wounded else ", unhurt", "great" if wits >= 20 else "good" if wits >= 10 else "few",
                facts, sofar, k + 1, choices))
    return user, [STEP_WORDS[i] for i in offered]

def reconsider(name, wounded, plan, step_no, facts, noticed_key, d, direction):
    """kind 6 (recon_ask): a hunter on step step_no (1-based) of `plan` (STEP_WORDS indices) noticed something"""
    st = plan[step_no - 1] if step_no - 1 < len(plan) else 0
    user = ("You plan ahead, and you change your plan when the moment calls for it. You are a %s%s. You hate the "
            "adventurer and hunt them on this level. Your plan: %s. You are on step %d: %s (%s).%s Just now you noticed "
            "%s %s. Does this change things? Choices: keep_plan (carry on as planned), rush_in (this is the moment: go "
            "in now, fast, to where the adventurer is), new_plan (drop the plan and think again)." % (
                name, ", wounded" if wounded else ", unhurt", ", ".join(STEP_WORDS[i] for i in plan), step_no,
                STEP_WORDS[st] if st else "none", STEP_SAYS[st] if st else "", facts, LEDGER[noticed_key], hunter_where(d, direction)))
    return user, ["keep_plan", "rush_in", "new_plan"]

# ---- the model's text (src\uwmind.c template_split + uwm_score): LFM2.5's ChatML with its BOS token first ----
BOS = "<|startoftext|>"

def prompt_text(user, system=SYSTEM):
    """the whole text before the answer, as the model reads it (the BOS token spelt out: tokenise with
    add_special_tokens=False)"""
    return "%s<|im_start|>system\n%s<|im_end|>\n<|im_start|>user\n%s<|im_end|>\n<|im_start|>assistant\ngoal=" % (BOS, system, user)

def self_test():
    """the builders against the FORMAT 2 forms of tests\\uwmind_score.c's USER strings (FORMAT 1 = the game's log)"""
    want = [
        ("You are a lurker, unhurt. You are dim-witted and act on instinct. You hate the adventurer and want to kill them. "
         "You are on guard. You heard the adventurer's weapon striking (the player) 6 squares to your south-west, but saw "
         "no one. Choices: attack (charge there at once and fight), ignore (stay where you are).",
         on_guard("lurker", 3, 6, 5, what=PLAYER_NOISES["fight"])),
        ("You are a goblin, unhurt. You are of average wits. You hate the adventurer and want to kill them. You are good at "
         "ambushes. You were fighting the player, an armed adventurer, and lost sight of them 3 squares to your west. When you last saw them the adventurer looked unhurt, with a weapon drawn, carrying a torch. "
         "Choices: hunt (go where you last saw them and search), ambush (hide in the dark near there and wait to strike), "
         "return (give up and go back to your post).",
         lost_player("goblin", 10, 3, 6, ambusher=True)),
        ("You are a goblin, friendly toward the player. You are of average wits. You are milling about near your home. The "
         "player, an armed adventurer, is 3 squares to your south. Choices: greet (go up to the player and wait politely), "
         "follow (go along with the player for a while), keep_away (back off to your home), ignore (carry on as you were).",
         friendly("goblin", 10, 3, 4)),
        ("You are a goblin, wounded: health 3 of 27. You are of average wits. You hate the adventurer and want to kill them. "
         "You see the adventurer, the player, 2 squares to your north. The adventurer looks hurt, no weapon in hand, in the dark. Choices: attack (fight them now), ambush (pull back "
         "into the dark nearby and wait to strike), flee (run away).",
         sees_player("goblin", 10, 2, 0, hp=3, avghit=27, ambusher=True, look=1, drawn=0, light=0)),
    ]
    bad = 0
    for w, (u, c) in want:
        if u != w:
            bad += 1
            print("MISMATCH\n want: %r\n got:  %r" % (w, u))
    # the chat template against the model's own (transformers, when installed in the fine-tune environment)
    try:
        from transformers import AutoTokenizer
        for m in ("LFM2.5-1.2B-Instruct", "LFM2.5-350M"):
            p = os.path.join(r"O:\appsnotonc\finetune\models", m)
            if not os.path.isdir(p): continue
            t = AutoTokenizer.from_pretrained(p)
            s = t.apply_chat_template([{"role": "system", "content": SYSTEM}, {"role": "user", "content": "Q"}],
                                      tokenize=False, add_generation_prompt=True) + "goal="
            if s != prompt_text("Q"):
                bad += 1
                print("TEMPLATE MISMATCH for %s:\n %r\n %r" % (m, s, prompt_text("Q")))
            else:
                print("template %s: the same as the model's own" % m)
    except ImportError:
        print("(transformers not here: the template not checked against the model's)")
    print("mind_format self-test: %s" % ("PASS" if not bad else "FAIL (%d)" % bad))
    return bad

if __name__ == "__main__":
    sys.exit(1 if self_test() else 0)
