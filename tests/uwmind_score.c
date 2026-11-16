/* uwmind_score.c: times uwmind.dll as the game uses it (LLM_NPC_AI.md's before/after table): the old way (uwm_ask,
 * the whole prompt read and the answer generated under a grammar) against the new (uwm_score: the instructions kept,
 * one forward pass, the choices' first tokens scored), alone and batched.
 *   uwmind_score MODEL.gguf gpu|cpu [runs]          run from build\llama\bin (uwmind.dll beside it) */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*load_fn)(const char *, const char *, int, char *, int);
typedef int (*ask_fn)(const char *, const char *, const char *, char *, int);
typedef int (*score_fn)(const char *, int, const char *const *, const char *const *, int *, char *, int);
typedef void (*free_fn)(void);

/* FORMAT 2 (2026-11-13): O:\appsnotonc\finetune\mind_format.py SYSTEM and its self_test questions, byte for byte */
static const char *SYSTEM =
    "You decide what one creature in a dark dungeon does next. You know only what the creature itself saw and "
    "heard. Badly wounded means under a third of its health. Rules; the first that fits wins. 1. Badly wounded, "
    "with no kin fighting beside it (kin within hearing that are fighting count as beside it): flee, return, "
    "recover or stay put; never attack, hunt or ambush. 2. The dim-witted act on instinct: attack or hunt what is "
    "near, flee when hurt, else carry on; they never ambush, rally or investigate. 3. An adventurer who looks "
    "badly hurt or near death is prey: attack, hunt or rush in, even when wounded (if not badly). 4. A trail is "
    "cold after 5 minutes or beyond 10 squares: return, recover or give up; do not hunt or ambush. 5. Kin within "
    "hearing that are not yet fighting (rally or fetch_kin is offered only when kin are near), average wits or "
    "better, and the adventurer known to be there (seen, lost from sight, or a noise known to be theirs): rally or "
    "fetch kin rather than go alone; kin already fighting within hearing: join them (attack, or hunt when the "
    "adventurer was lost from sight). 6. When ambush is among the choices, take it if any one of these holds: the "
    "creature is wounded; the adventurer is known to be 6 squares or more away; the adventurer is known to be busy "
    "(at a door, a lever, a spell); the adventurer was seen carrying a light while the creature is in the dark. "
    "Not when the creature cannot tell who made a noise or carries a light. 7. Otherwise a hostile creature that "
    "knows where the adventurer is attacks at once, or hunts when it lost sight of them close by a moment ago; one "
    "that cannot tell who made a noise or carries a light investigates. 8. A hunter plans its steps in order from "
    "the freshest thing it knows to be the adventurer's: the last sighting, or a later noise or act of theirs (a "
    "light, something taken, a door opened, a trap or kin found dead do not say where the adventurer is). The "
    "trail is cold after 5 minutes or beyond 10 squares. Once the plan has a goto_home, recover or goto_water "
    "step, the next step is give_up. A wounded hunter, unless the adventurer looked hurt when last seen: "
    "goto_water when offered, else recover; then give_up. Nothing known of the adventurer, or the trail cold: "
    "follow_light and then wait when it noticed a light less than 5 minutes ago; then goto_water when offered, "
    "else give_up when nothing is known and a way was blocked or the last plan lost him, else goto_home; then "
    "give_up. The trail fresh: a hunter of good or great wits takes fetch_kin first when it is offered and the "
    "adventurer did not look hurt. Then a hunter of few wits, or the adventurer within 3 squares or looking hurt: "
    "goto_last, search, goto_home, give_up. Otherwise: goto_door (the nearest door on the way is a chokepoint), "
    "wait, goto_last, search, goto_home, give_up. When goto_last is not offered: wait, goto_home, give_up. "
    "Reconsidering: rush_in when the adventurer is busy or hurt close by (within 4 squares); new_plan when it is "
    "wounded, finds kin dead, or the adventurer's light went out; otherwise keep_plan. 9. A friendly creature: "
    "warn when it can see an enemy of the player; greet when the player is within 2 squares; follow when the "
    "player is 3 to 6 squares off and it is wandering or milling about; keep_away when it is on its way home, or "
    "standing at its place with the player 3 squares or more off, or the player is 7 squares or more off; the "
    "dim-witted carry on (ignore), and a named one (offered only greet, warn, ignore) greets when within 4 "
    "squares. Answer only in the form goal=<choice>, with one of the choices offered.";
static const char *USER[4] = {
    "You are a lurker, unhurt. You are dim-witted and act on instinct. You hate the adventurer and want to kill "
    "them. You are on guard. You heard the adventurer's weapon striking (the player) 6 squares to your south-west, "
    "but saw no one. Choices: attack (charge there at once and fight), ignore (stay where you are).",
    "You are a goblin, unhurt. You are of average wits. You hate the adventurer and want to kill them. You are good "
    "at ambushes. You were fighting the player, an armed adventurer, and lost sight of them 3 squares to your west. "
    "When you last saw them the adventurer looked unhurt, with a weapon drawn, carrying a torch. Choices: hunt (go "
    "where you last saw them and search), ambush (hide in the dark near there and wait to strike), return (give up "
    "and go back to your post).",
    "You are a goblin, friendly toward the player. You are of average wits. You are milling about near your home. "
    "The player, an armed adventurer, is 3 squares to your south. Choices: greet (go up to the player and wait "
    "politely), follow (go along with the player for a while), keep_away (back off to your home), ignore (carry on "
    "as you were).",
    "You are a goblin, wounded: health 3 of 27. You are of average wits. You hate the adventurer and want to kill "
    "them. You see the adventurer, the player, 2 squares to your north. The adventurer looks hurt, no weapon in "
    "hand, in the dark. Choices: attack (fight them now), ambush (pull back into the dark nearby and wait to "
    "strike), flee (run away)."};
static const char *CHOICES[4] = {"attack|ignore", "hunt|ambush|rally|return", "greet|follow|keep_away|ignore", "attack|ambush|flee"};
static const char *GRAMMAR[4] = {
    "root ::= \"goal=\" (\"attack\" | \"ignore\")", "root ::= \"goal=\" (\"hunt\" | \"ambush\" | \"return\")",
    "root ::= \"goal=\" (\"greet\" | \"follow\" | \"keep_away\" | \"ignore\")",
    "root ::= \"goal=\" (\"attack\" | \"ambush\" | \"flee\")"};

static const char *pick_word(int q, int p)
{
    static char w[64];
    char c[128], *t, *save = NULL;
    int i = 0;
    snprintf(c, sizeof c, "%s", CHOICES[q]);
    for (t = strtok_s(c, "|", &save); t; t = strtok_s(NULL, "|", &save), i++)
        if (i == p) { snprintf(w, sizeof w, "%s", t); return w; }
    return "?";
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uwmind_score MODEL.gguf gpu|cpu [runs]\n"); return 2; }
    int runs = argc > 3 ? atoi(argv[3]) : 5;
    char dir[MAX_PATH], info[256];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    *strrchr(dir, '\\') = 0;
    HMODULE h = LoadLibraryA("uwmind.dll");
    if (!h) { fprintf(stderr, "no uwmind.dll\n"); return 1; }
    load_fn load = (load_fn)(void *)GetProcAddress(h, "uwm_load");
    ask_fn ask = (ask_fn)(void *)GetProcAddress(h, "uwm_ask");
    score_fn score = (score_fn)(void *)GetProcAddress(h, "uwm_score");
    free_fn ufree = (free_fn)(void *)GetProcAddress(h, "uwm_free");
    if (!load(dir, argv[1], strcmp(argv[2], "cpu") ? 999 : 0, info, sizeof info)) { printf("load failed: %s\n", info); return 1; }
    printf("model: %s\n", info);
    char out[64];
    ask(SYSTEM, USER[0], GRAMMAR[0], out, sizeof out);   /* warm-up */
    for (int q = 0; q < 4; q++) {                        /* the old way */
        int best = 1 << 30, ms = 0;
        for (int r = 0; r < runs; r++) { ms = ask(SYSTEM, USER[q], GRAMMAR[q], out, sizeof out); if (ms < best) best = ms; }
        printf("grammar  q%d: %4d ms  %s\n", q, best, out);
    }
    for (int q = 0; q < 4; q++) {                        /* the new way, one question */
        int best = 1 << 30, pick = -1;
        for (int r = 0; r < runs; r++) {
            const char *u[1] = {USER[q]}, *c[1] = {CHOICES[q]};
            int ms = score(SYSTEM, 1, u, c, &pick, info, sizeof info);
            if (ms < best) best = ms;
        }
        printf("score    q%d: %4d ms  goal=%s\n", q, best, pick >= 0 ? pick_word(q, pick) : "(clash)");
    }
    for (int nb = 4; nb <= 8; nb += 4) {                 /* the new way, batched */
        const char *u[8], *c[8];
        int picks[8], best = 1 << 30;
        for (int i = 0; i < nb; i++) u[i] = USER[i % 4], c[i] = CHOICES[i % 4];
        for (int r = 0; r < runs; r++) { int ms = score(SYSTEM, nb, u, c, picks, info, sizeof info); if (ms < best) best = ms; }
        printf("score x%d: %4d ms for all (%.0f each):", nb, best, (double)best / nb);
        for (int i = 0; i < nb; i++) printf(" %s", picks[i] >= 0 ? pick_word(i % 4, picks[i]) : "?");
        printf("\n");
    }
    {   /* too big for one decode (the second review's crash: llama_decode aborts past n_batch): must split, not abort */
        static char big[3600];
        const char *u[8], *c[8];
        int picks[8];
        snprintf(big, sizeof big, "%s %s %s %s %s %s %s %s %s", USER[1], USER[1], USER[1], USER[1], USER[1], USER[1],
                 USER[1], USER[1], USER[1]);
        for (int i = 0; i < 8; i++) u[i] = big, c[i] = CHOICES[1];
        int ms = score(SYSTEM, 8, u, c, picks, info, sizeof info);
        printf("score x8 oversize: %d ms (%s): %s\n", ms, info, picks[0] >= 0 ? pick_word(1, picks[0]) : "?");
    }
    ufree();
    return 0;
}
