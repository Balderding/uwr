/* uwmindhost.c: the language model in the game (LLM_NPC_AI.md, "Our own Strata"). uwmind.dll (src\uwmind.c, over
 * llama.cpp) is loaded at run time from llm\bin beside the exe, with a model from llm\models; when either is missing,
 * or llm=0 in the settings, or UWPORT_LLM=0 (every off-screen check and proof), nothing is loaded and the rules
 * decide alone. One worker thread loads the model and answers the questions hunt.c puts in (port_mind_ask): up to
 * eight at a time in one pass (uwm_score: the instructions kept, each choice scored by its first token), the grammar
 * way (uwm_ask) when two choices begin alike or llm_mode=grammar. The answers come back through port_mind_take; the
 * game never waits. Status lines go to stdout ("AI: ..."), every question and answer to home\logs\ai-<date>.jsonl.
 *
 * Settings (uw1port.cfg): llm=0 off; llm_model=<file under llm\models, a subfolder allowed> the model (the Esc menu's
 * Language model row writes it; lfm the 1.2B as before; absent or not found: our trained LFM2.5-350M-UW, else Qwen3.5-2B, else the first found); llm_gpu=0 the CPU only; llm_mode=
 * grammar; llm_rate=N questions a second at most (default 20); llm_stale_ms=N answers older than that dropped
 * (default 3000); llm_idle_min=N minutes without a question and the model is let go (default 10; 0 never), loaded
 * again on the next. The model goes into the graphics card's memory (VRAM) unless llm_gpu=0 (2026-11-13, the user: no
 * first-run calibration any more; llm_calibrated is not read); the first start writes the default (LFM2.5-350M-UW since
 * 2026-11-14) as llm_model without asking. UWPORT_LLM_MODEL=FILE another model. */
/* Linux (the user, 2026-11-16: "build it in"): the same code over uwos.h's Windows names. The library is
 * llm/bin/libuwmind.so beside llama.cpp's Linux libraries (tools\buildmind_linux.py). The folder is the one Windows
 * uses, its names lower-cased by makedist.py and the settings' llm_model written by either system, so on Linux every
 * path this file builds (with '\', in the case the code names) is turned into the one on the disk where it meets
 * the system: mind_native, which makes '\' a '/' and finds each name whatever its case. */
#ifdef _WIN32
#include <windows.h>
#define MIND_LIB "uwmind.dll"
#define MIND_SEP "\\"
#define MIND_NATIVE(p) ((void)0)
#else
#include "uwos.h"
#include <dirent.h>
#include <strings.h>
#include <unistd.h>
#define MIND_LIB "libuwmind.so"
#define MIND_SEP "/"
#define MIND_NATIVE(p) mind_native((p), sizeof(p))
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define strtok_s strtok_r
#define THREAD_PRIORITY_BELOW_NORMAL 0
#define SetThreadPriority(t, p) ((void)(t))   /* (the worker waits on the GPU or runs ggml's own low-priority pool) */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "plat.h"
#ifndef _WIN32
/* p as the disk has it: '\' made '/', then each name of the path looked up in its folder whatever its case (the name
   as given when nothing matches: a file to be made, or one that is not there) */
static void mind_native(char *p, size_t n)
{
    char out[MAX_PATH * 2] = "", name[512];
    size_t len = 0;
    for (char *c = p; *c; c++) if (*c == '\\') *c = '/';
    const char *s = p;
    if (*s == '/') out[0] = '/', out[1] = 0, len = 1;
    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;
        size_t k = 0;
        while (s[k] && s[k] != '/') k++;
        snprintf(name, sizeof name, "%.*s", (int)k, s);
        s += k;
        if (strcmp(name, ".") && strcmp(name, "..")) {
            char probe[MAX_PATH * 2 + 520];
            struct stat st;
            snprintf(probe, sizeof probe, "%s%s%s", out, len && out[len - 1] != '/' ? "/" : "", name);
            if (stat(len ? probe : name, &st)) {   /* not there as written: any case */
                DIR *d = opendir(len ? out : ".");
                if (d) {
                    for (struct dirent *e; (e = readdir(d));)
                        if (!strcasecmp(e->d_name, name)) { snprintf(name, sizeof name, "%s", e->d_name); break; }
                    closedir(d);
                }
            }
        }
        len += (size_t)snprintf(out + len, sizeof out - len, "%s%s", len && out[len - 1] != '/' ? "/" : "", name);
        if (len >= sizeof out) return;   /* (too long: left as it was, with '/') */
    }
    snprintf(p, n, "%s", out);
}
/* GetFileAttributesA and LoadLibraryExA of such a path (uwos.h's own take a Linux path) */
static DWORD mind_attr(const char *path)
{
    char p[MAX_PATH * 2];
    struct stat st;
    snprintf(p, sizeof p, "%s", path);
    mind_native(p, sizeof p);
    return stat(p, &st) == 0 ? (DWORD)(S_ISDIR(st.st_mode) ? 0x10 : 0x80) : INVALID_FILE_ATTRIBUTES;
}
static HMODULE mind_lib(const char *path)
{
    char p[MAX_PATH * 2];
    snprintf(p, sizeof p, "%s", path);
    mind_native(p, sizeof p);
    return dlopen(p, RTLD_NOW | RTLD_LOCAL);   /* (its RUNPATH $ORIGIN finds libllama.so.0 and the rest beside it) */
}
#define GetFileAttributesA(p) mind_attr(p)
#define LoadLibraryExA(p, a, b) mind_lib(p)
#endif
int port_config_get(const char *home, const char *key, char *out, size_t outsz);   /* the runtime's port.h (which clashes with windows.h) */
int port_config_set(const char *home, const char *key, const char *value);

typedef int (*load_fn)(const char *, const char *, int, char *, int);
typedef int (*ask_fn)(const char *, const char *, const char *, char *, int);
typedef int (*score_fn)(const char *, int, const char *const *, const char *const *, int *, char *, int);
typedef void (*free_fn)(void);
static load_fn m_load;
static ask_fn m_ask;
static score_fn m_score;
static free_fn m_free;

enum { QN = 32, BATCH = 8 };
struct Q { int level, mob, kind; char user[1200], grammar[200], choices[160], answer[64]; int ms; DWORD asked; };
static struct Q asks[QN], answers[QN], busy[BATCH];
static int na, nans, nbusy;
static CRITICAL_SECTION cs;
static HANDLE wake, thread;
static volatile LONG state;             /* 0 not started, 4 starting, 1 loading, 2 ready, 3 off / failed, 5 idle (let go) */
static volatile LONG quit;
static int use_grammar, rate = 20, stale_ms = 3000, idle_min = 10;
static DWORD last_ask;
static FILE *jsonl;

/* FORMAT 2 (2026-11-13, the user: the fine-tuned LFM2.5-350M-UW was trained on it; rule 8 rules v4 on
   2026-11-14, its plan steps as the fine-tune folder's tools\hunter_policy.py answers them, model r5): the fine-tune folder's
   O:\appsnotonc\finetune\mind_format.py SYSTEM, byte for byte (tools\mind_format.py the same copy) */
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

static void say(const char *s)
{
    printf("AI: %s\n", s);
    fflush(stdout);
}

static int cfg_int(const char *key, int def)
{
    char v[32];
    return plat_home() && port_config_get(plat_home(), key, v, sizeof v) == 0 ? atoi(v) : def;
}

/* the Esc menu's model rows (2026-11-16: every setting of the model has its row). which: 0 llm_gpu (1 the graphics
   card, 0 the processor; taken when the model is next loaded), 1 llm_mode (1 grammar, 0 scoring), 2 llm_rate
   (questions a second), 3 llm_stale_ms (0 never dropped), 4 llm_idle_min (0 never let go). v < 0 reads; else it is
   set, kept in the settings, and (1 to 4) taken at once: the worker reads these plain ints each pass */
int port_mind_tune(int which, int v)
{
    static int have, gpu = 1;
    static const char *const KEY[5] = { "llm_gpu", "llm_mode", "llm_rate", "llm_stale_ms", "llm_idle_min" };
    int *const AT[5] = { &gpu, &use_grammar, &rate, &stale_ms, &idle_min };
    if (which < 0 || which > 4) return 0;
    if (!have) {                        /* (port_mind_ready reads the same keys when the worker starts) */
        char mode[16] = "";
        have = 1;
        gpu = cfg_int("llm_gpu", 1) != 0;
        if (plat_home()) port_config_get(plat_home(), "llm_mode", mode, sizeof mode);
        use_grammar = !strcmp(mode, "grammar");
        rate = cfg_int("llm_rate", 20), stale_ms = cfg_int("llm_stale_ms", 3000), idle_min = cfg_int("llm_idle_min", 10);
    }
    if (v >= 0) {
        char b[16];
        *AT[which] = v;
        if (which == 1) snprintf(b, sizeof b, "%s", v ? "grammar" : "score");
        else snprintf(b, sizeof b, "%d", v);
        if (plat_home()) port_config_set(plat_home(), KEY[which], b);
    }
    return *AT[which];
}

/* the choices of a grammar root ::= "goal=" ("a" | "b" | "c"): "a|b|c" */
static void grammar_choices(const char *g, char *out, int n)
{
    const char *p = strstr(g, "goal=\"");
    int o = 0;
    out[0] = 0;
    if (!p) return;
    p += 6;                             /* past "goal=" and its closing quote */
    while ((p = strchr(p, '"')) != NULL) {
        const char *e = strchr(p + 1, '"');
        if (!e) break;
        if (e > p + 1 && o + (e - p) + 1 < n) {
            if (o) out[o++] = '|';
            memcpy(out + o, p + 1, (size_t)(e - p - 1)), o += (int)(e - p - 1), out[o] = 0;
        }
        p = e + 1;
    }
}

static void json_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f), fputc(*s, f);
        else if ((unsigned char)*s >= 32) fputc(*s, f);
    }
    fputc('"', f);
}
static void log_jsonl(const struct Q *q, const char *how)
{
    if (!jsonl) return;
    fprintf(jsonl, "{\"t\":%lu,\"level\":%d,\"mob\":%d,\"kind\":%d,\"how\":\"%s\",\"ms\":%d,\"question\":", (unsigned long)time(NULL),
            q->level, q->mob, q->kind, how, q->ms);
    json_str(jsonl, q->user);
    fputs(",\"choices\":", jsonl);
    json_str(jsonl, q->choices);
    fputs(",\"answer\":", jsonl);
    json_str(jsonl, q->answer);
    fputs("}\n", jsonl);
    fflush(jsonl);
}

static char exe_dir[MAX_PATH], bin[MAX_PATH], model[MAX_PATH];
static int want_gpu;
static volatile LONG repick;            /* the Esc menu chose another model: the worker loads it */
static volatile LONG off_fixed;         /* off by UWPORT_LLM=0 or llm=0: never started again */

/* ---- the model files: main.c's check (smart-hunting off without them), the Esc menu's Language model row ---- */
enum { MAXM = 32 };
static char mfile[MAXM][MAX_PATH], mrel[MAXM][MAX_PATH];
static const char *mfile_p[MAXM], *mrel_p[MAXM];
static int mn = -1;
static void exe_dir_init(void)
{
    if (exe_dir[0]) return;
    char d[MAX_PATH];
#ifndef _WIN32
    ssize_t n = readlink("/proc/self/exe", d, MAX_PATH - 1);
    if (n <= 0 || n >= MAX_PATH - 1) return;
    d[n] = 0;
    char *s = strrchr(d, '/');
#else
    DWORD n = GetModuleFileNameA(NULL, d, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    char *s = strrchr(d, '\\');
#endif
    if (s) *s = 0;
    snprintf(exe_dir, sizeof exe_dir, "%s", d);
}
static int is_gguf(const char *n)
{
    size_t l = strlen(n);
    return l > 5 && !_stricmp(n + l - 5, ".gguf");
}
static void model_add(const char *sub, const char *name)
{
    if (mn >= MAXM) return;
    snprintf(mfile[mn], MAX_PATH, "%s", name);
    if (sub) snprintf(mrel[mn], MAX_PATH, "%s\\%s", sub, name);
    else snprintf(mrel[mn], MAX_PATH, "%s", name);
    mn++;
}
static int rel_cmp(const void *a, const void *b) { return strcmp(mrel[*(const int *)a], mrel[*(const int *)b]); }
/* once: the .gguf files under <exe>\llm\models and one level of its subfolders, sorted by their path there; none
   when llm\bin\uwmind.dll is missing */
static void models_scan(void)
{
    char p[MAX_PATH * 2];
    if (mn >= 0) return;
    mn = 0;
    exe_dir_init();
    snprintf(p, sizeof p, "%s\\llm\\bin\\" MIND_LIB, exe_dir);
    if (!exe_dir[0] || GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) return;
#ifndef _WIN32
    {   /* the same two levels by opendir: the names as the disk has them (model_add joins them with '\' as on
           Windows: what the settings hold, read by either system) */
        snprintf(p, sizeof p, "%s\\llm\\models", exe_dir);
        mind_native(p, sizeof p);
        DIR *d = opendir(p);
        if (!d) return;
        for (struct dirent *e; (e = readdir(d));) {
            char q[MAX_PATH * 2 + 300];
            struct stat st;
            if (e->d_name[0] == '.') continue;
            snprintf(q, sizeof q, "%s/%s", p, e->d_name);
            if (stat(q, &st)) continue;
            if (S_ISDIR(st.st_mode)) {
                DIR *ds = opendir(q);
                if (!ds) continue;
                for (struct dirent *es; (es = readdir(ds));) {
                    char f[MAX_PATH * 2 + 600];
                    struct stat sf;
                    snprintf(f, sizeof f, "%s/%s", q, es->d_name);
                    if (is_gguf(es->d_name) && !stat(f, &sf) && S_ISREG(sf.st_mode)) model_add(e->d_name, es->d_name);
                }
                closedir(ds);
            } else if (is_gguf(e->d_name))
                model_add(NULL, e->d_name);
        }
        closedir(d);
    }
#else
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(p, sizeof p, "%s\\llm\\models\\*", exe_dir);
    if ((h = FindFirstFileA(p, &fd)) == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char sub[MAX_PATH], q[MAX_PATH * 2];
            WIN32_FIND_DATAA fs;
            HANDLE hs;
            if (fd.cFileName[0] == '.') continue;
            snprintf(sub, sizeof sub, "%s", fd.cFileName);
            snprintf(q, sizeof q, "%s\\llm\\models\\%s\\*", exe_dir, sub);
            if ((hs = FindFirstFileA(q, &fs)) == INVALID_HANDLE_VALUE) continue;
            do
                if (!(fs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && is_gguf(fs.cFileName)) model_add(sub, fs.cFileName);
            while (FindNextFileA(hs, &fs));
            FindClose(hs);
        } else if (is_gguf(fd.cFileName))
            model_add(NULL, fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#endif
    {   /* sorted (strcmp on the path under llm\models) */
        int idx[MAXM];
        static char f2[MAXM][MAX_PATH], r2[MAXM][MAX_PATH];
        for (int i = 0; i < mn; i++) idx[i] = i;
        qsort(idx, (size_t)mn, sizeof idx[0], rel_cmp);
        for (int i = 0; i < mn; i++) memcpy(f2[i], mfile[idx[i]], MAX_PATH), memcpy(r2[i], mrel[idx[i]], MAX_PATH);
        memcpy(mfile, f2, sizeof f2[0] * (size_t)mn), memcpy(mrel, r2, sizeof r2[0] * (size_t)mn);
    }
    for (int i = 0; i < mn; i++) mfile_p[i] = mfile[i], mrel_p[i] = mrel[i];
}
/* the model files found (names: the file names; paths: under llm\models), sorted; 0 when none or no llm\bin\uwmind.dll */
int port_mind_models(const char **names, const char **paths, int max)
{
    models_scan();
    for (int i = 0; i < mn && i < max; i++) {
        if (names) names[i] = mfile_p[i];
        if (paths) paths[i] = mrel_p[i];
    }
    return mn;
}
/* the model to load: UWPORT_LLM_MODEL; else llm_model (lfm, or a file under llm\models); else our trained
   LFM2.5-350M-UW; else (that file gone) Qwen3.5-2B; else the first found */
static void model_resolve(char *out, size_t n)
{
    char mv[MAX_PATH] = "";
    const char *env = getenv("UWPORT_LLM_MODEL");
    exe_dir_init();
    if (env && *env) { snprintf(out, n, "%s", env); return; }
    if (plat_home()) port_config_get(plat_home(), "llm_model", mv, sizeof mv);
    if (!strcmp(mv, "lfm")) {   /* the fast 1.2B (the old value) */
        snprintf(out, n, "%s\\llm\\models\\LFM2.5-1.2B-Instruct\\LFM2.5-1.2B-Instruct-Q4_K_M.gguf", exe_dir);
        return;
    }
    if (mv[0]) {
        for (char *c = mv; *c; c++) if (*c == '/') *c = '\\';
        snprintf(out, n, "%s\\llm\\models\\%s", exe_dir, mv);
        if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) return;
    }
    /* the default (the user, 2026-11-14: "we want lfm to be the default"): the LFM2.5-350M-UW we trained, run 5's f16
       (the file the user plays with); that file gone, Qwen3.5-2B (the default from 2026-11-11 until then) */
    snprintf(out, n, "%s\\llm\\models\\LFM2.5-350M-UW\\LFM2.5-350M-UW-r5-f16.gguf", exe_dir);
    if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) return;
    snprintf(out, n, "%s\\llm\\models\\Qwen3.5-2B\\Qwen3.5-2B-Q4_K_M.gguf", exe_dir);
    if (GetFileAttributesA(out) == INVALID_FILE_ATTRIBUTES && port_mind_models(NULL, NULL, 0) > 0)
        snprintf(out, n, "%s\\llm\\models\\%s", exe_dir, mrel[0]);
}
/* the model in use (the file name, for the menu's row) and its path under llm\models ("" when it is elsewhere) */
static char cur_name[MAX_PATH], cur_rel[MAX_PATH];
static void current_update(void)
{
    char p[MAX_PATH * 2], pre[MAX_PATH + 16];
    model_resolve(p, sizeof p);
    const char *b = strrchr(p, '\\');
    snprintf(cur_name, sizeof cur_name, "%s", b ? b + 1 : p);
    snprintf(pre, sizeof pre, "%s\\llm\\models\\", exe_dir);
    cur_rel[0] = 0;
    if (!_strnicmp(p, pre, strlen(pre))) snprintf(cur_rel, sizeof cur_rel, "%s", p + strlen(pre));
}
const char *port_mind_current(void)
{
    if (!cur_name[0]) current_update();
    return cur_name;
}
const char *port_mind_current_path(void)
{
    if (!cur_name[0]) current_update();
    return cur_rel;
}
int port_mind_ready(void);
/* the Esc menu's choice: llm_model=<path under llm\models> written; the worker lets the loaded model go and loads this
   one (a worker that ended on a failed load is started again) */
void port_mind_pick(const char *path)
{
    char msg[MAX_PATH + 64];
    if (plat_home()) port_config_set(plat_home(), "llm_model", path);
    current_update();
    snprintf(msg, sizeof msg, "language model chosen: %s (llm_model in the settings)", cur_name);
    say(msg);
    InterlockedExchange(&repick, 1);
    if (thread && WaitForSingleObject(thread, 0) == WAIT_OBJECT_0 && state == 3 && !off_fixed) {
        CloseHandle(thread);            /* ended (its model failed): again with the new one */
        thread = NULL;
        InterlockedExchange(&repick, 0);
        InterlockedExchange(&state, 0);
        port_mind_ready();
    } else if (wake)
        SetEvent(wake);
}
void port_mind_keep(void);
/* the first start: 1 when the player should be asked which model (no llm_model yet, more than one model, llm not 0) */
int port_mind_first_start(void)
{
    /* never now (the user, 11-13: no question; 11-14: our trained LFM2.5-350M-UW the default): no llm_model yet,
       model_resolve's default is written as the choice; the Esc menu's Language model row still changes it */
    port_mind_keep();
    return 0;
}
/* the first start's picker closed: the model shown kept (llm_model written), so it never asks again */
void port_mind_keep(void)
{
    char v[MAX_PATH];
    if (!plat_home() || port_config_get(plat_home(), "llm_model", v, sizeof v) == 0) return;
    port_mind_current();
    if (cur_rel[0]) port_config_set(plat_home(), "llm_model", cur_rel);
}

/* the model up (load + warm-up); the first time on a PC also the calibration. 1 when ready */
static int bring_up(int first)
{
    char info[256], msg[480];
    {   /* UWPORT_LLM_STALLTEST=MS (tools\llmcheck.py g): a load that cannot be interrupted, as a long real one */
        const char *st = getenv("UWPORT_LLM_STALLTEST");
        if (st && atoi(st) > 0) Sleep((DWORD)atoi(st));
    }
    if (first) {
        /* the model in the graphics card's memory (VRAM) always, unless llm_gpu=0 (the user, 11-13: no calibration that
           could settle on the CPU; an llm_calibrated=cpu left by an older run is not read) */
        int gpu_cfg = cfg_int("llm_gpu", 1);
        want_gpu = gpu_cfg;
        snprintf(msg, sizeof msg, "loading the language model %s (%s)", strrchr(model, '\\') ? strrchr(model, '\\') + 1 : model,
                 want_gpu ? "into VRAM" : "CPU");
        say(msg);
        if (0) {                        /* (the old first-run calibration, the GPU against the CPU: kept, not run) */
            char cal[16] = "";
            const char *u[1] = {"Calibration. Choices: ready (be ready)."}, *c[1] = {"ready"};
            int pk, gpu_ms = -1, cpu_ms = -1;
            if (m_load(bin, model, 999, info, sizeof info)) {
                m_score(SYSTEM, 1, u, c, &pk, NULL, 0);                     /* (the shaders, the first time) */
                gpu_ms = m_score(SYSTEM, 1, u, c, &pk, NULL, 0);
                m_free();
            }
            if (quit) return 0;                 /* (the game closing: between the loads, so that the exit can let go) */
            if (m_load(bin, model, 0, info, sizeof info)) {
                m_score(SYSTEM, 1, u, c, &pk, NULL, 0);
                cpu_ms = m_score(SYSTEM, 1, u, c, &pk, NULL, 0);
                m_free();
            }
            want_gpu = gpu_ms >= 0 && (cpu_ms < 0 || gpu_ms <= cpu_ms);
            if (gpu_ms >= 0 && cpu_ms >= 0 && !cal[0]) {   /* a verdict kept only on both timings (the third review's item 2) */
                if (plat_home()) port_config_set(plat_home(), "llm_calibrated", want_gpu ? "gpu" : "cpu");
                snprintf(msg, sizeof msg, "calibrated: GPU %d ms, CPU %d ms a question: the %s from now on (llm_calibrated in the settings)",
                         gpu_ms, cpu_ms, want_gpu ? "GPU" : "CPU");
            } else
                snprintf(msg, sizeof msg, "calibration incomplete (GPU %d ms, CPU %d ms; -1 failed): the %s this time, nothing kept "
                         "(the next start calibrates again)", gpu_ms, cpu_ms, want_gpu ? "GPU" : "CPU");
            say(msg);
        }
    }
    if (quit) return 0;
    if (!m_load(bin, model, want_gpu ? 999 : 0, info, sizeof info)) {
        snprintf(msg, sizeof msg, "language model failed: %s (the rules decide alone)", info);
        say(msg);
        return 0;
    }
    snprintf(msg, sizeof msg, "language model ready: %s", info);
    say(msg);
    {   /* the GPU compiles its pipelines the first time each shape runs (~10 s on a new PC): one question, then
           batches of 2, 4 and 8, all here and not on a creature's time (the second review: the first batch of three
           took 13 s in the game and its answers went stale) */
        const char *u[BATCH], *c[BATCH];
        int pk[BATCH], total = 0;
        /* as long as a real question (a few hundred tokens): short ones left the real batches' shapes uncompiled
           (the first in-game batch of four, beside the renderer, still took 9 s) */
        static const char *LONG =
            "You are a goblin, wounded: health 12 of 27. You are of average wits. You hate the adventurer and want to "
            "kill them. You are good at ambushes. You were fighting the player, an armed adventurer, and lost sight of "
            "them 4 squares to your north-west. Earlier (40 seconds ago) you went to look and found no one. You last saw "
            "the adventurer 12 seconds ago. 2 of your kin are within hearing, some of them fighting. Choices: hunt (go "
            "where you last saw them and search), ambush (hide in the dark near there and wait to strike), rally (call "
            "your kin and go there together), return (give up and go back to your post).";
        /* the pipelines go by the tokens in a batch (the second review): 1, 2, 4, 8 of these (~150 tokens each), then
           8 doubled (~2400 tokens, the most eight real questions hold) */
        static char LONG2[1400];
        snprintf(LONG2, sizeof LONG2, "%s %s", LONG, LONG);
        for (int i = 0; i < BATCH; i++) u[i] = LONG, c[i] = "hunt|ambush|rally|return";
        for (int nb = 1; nb <= BATCH && !quit; nb *= 2) {
            int ms = m_score(SYSTEM, nb, u, c, pk, NULL, 0);
            if (ms > 0) total += ms;
        }
        for (int i = 0; i < BATCH; i++) u[i] = LONG2;
        if (!quit) { int ms = m_score(SYSTEM, BATCH, u, c, pk, NULL, 0); if (ms > 0) total += ms; }
        snprintf(msg, sizeof msg, "language model warmed up (batches of ~150 to ~2400 tokens: %d ms)", total);
        say(msg);
    }
    {   /* questions that waited through the load start their time now (else they would be dropped as stale) */
        DWORD now = GetTickCount();
        EnterCriticalSection(&cs);
        for (int i = 0; i < na; i++) asks[i].asked = now;
        LeaveCriticalSection(&cs);
    }
    return 1;
}

static DWORD WINAPI worker(void *arg)
{
    (void)arg;
    char msg[400];
    exe_dir_init();
    snprintf(bin, sizeof bin, "%s\\llm\\bin", exe_dir);
    MIND_NATIVE(bin);                     /* (Linux: as the disk has it; the library is handed it) */
    char dll[MAX_PATH];
    snprintf(dll, sizeof dll, "%s" MIND_SEP MIND_LIB, bin);
    model_resolve(model, sizeof model);   /* UWPORT_LLM_MODEL, llm_model, Qwen3.5-2B */
    MIND_NATIVE(model);
    HMODULE h = LoadLibraryExA(dll, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (h) {
        m_load = (load_fn)(void *)GetProcAddress(h, "uwm_load");
        m_ask = (ask_fn)(void *)GetProcAddress(h, "uwm_ask");
        m_score = (score_fn)(void *)GetProcAddress(h, "uwm_score");
        m_free = (free_fn)(void *)GetProcAddress(h, "uwm_free");
    }
    if (!h || !m_load || !m_ask || !m_score || !m_free || GetFileAttributesA(model) == INVALID_FILE_ATTRIBUTES) {
        snprintf(msg, sizeof msg, "no language model: %s not found (the rules decide alone)", !h || !m_score ? dll : model);
        say(msg);
        InterlockedExchange(&state, 3);
        return 0;
    }
    if (plat_home() && !jsonl) {   /* home\logs\ai-<date>.jsonl: every question and answer (tuning, the later distillation) */
        char path[1200], day[16];
        time_t now = time(NULL);
        strftime(day, sizeof day, "%Y-%m-%d", localtime(&now));
        snprintf(path, sizeof path, "%s" MIND_SEP "logs", plat_home());
#ifdef _WIN32
        CreateDirectoryA(path, NULL);
#else
        mkdir(path, 0755);
#endif
        snprintf(path, sizeof path, "%s" MIND_SEP "logs" MIND_SEP "ai-%s.jsonl", plat_home(), day);
        jsonl = fopen(path, "a");
    }
    if (!bring_up(1)) {
        InterlockedExchange(&state, 3);
        return 0;
    }
    InterlockedExchange(&state, 2);
    last_ask = GetTickCount();
    while (!quit) {
        WaitForSingleObject(wake, 500);
        if (quit) break;
        if (InterlockedExchange(&repick, 0)) {   /* the Esc menu chose another model: the one loaded let go, that one loaded */
            char want[MAX_PATH];
            model_resolve(want, sizeof want);
            MIND_NATIVE(want);
            if (_stricmp(want, model)) {
                if (state == 2) m_free();
                snprintf(model, sizeof model, "%s", want);
                if (GetFileAttributesA(model) == INVALID_FILE_ATTRIBUTES) {
                    snprintf(msg, sizeof msg, "no language model: %s not found (the rules decide alone)", model);
                    say(msg);
                    InterlockedExchange(&state, 3);
                    break;
                }
                snprintf(msg, sizeof msg, "loading the language model %s", strrchr(model, '\\') ? strrchr(model, '\\') + 1 : model);
                say(msg);
                if (!bring_up(0)) { InterlockedExchange(&state, 3); break; }
                InterlockedExchange(&state, 2);
                last_ask = GetTickCount();
            }
            continue;
        }
        EnterCriticalSection(&cs);
        int n = na < BATCH ? na : BATCH;
        memcpy(busy, asks, sizeof asks[0] * (size_t)n);
        memmove(asks, asks + n, sizeof asks[0] * (size_t)(na - n));
        na -= n, nbusy = n;
        LeaveCriticalSection(&cs);
        if (!n) {   /* nothing asked: let the model go after llm_idle_min minutes (VRAM back to the game) */
            if (state == 2 && idle_min > 0 && GetTickCount() - last_ask > (DWORD)idle_min * 60000u) {
                m_free();
                InterlockedExchange(&state, 5);
                say("language model let go (idle); loaded again at the next question");
            }
            continue;
        }
        last_ask = GetTickCount();
        if (state == 5) {
            if (!bring_up(0)) { InterlockedExchange(&state, 3); break; }
            InterlockedExchange(&state, 2);
        }
        const char *u[BATCH], *c[BATCH];
        int picks[BATCH], ms = -1;
        for (int i = 0; i < n; i++) u[i] = busy[i].user, c[i] = busy[i].choices;
        if (!use_grammar) ms = m_score(SYSTEM, n, u, c, picks, NULL, 0);
        for (int i = 0; i < n; i++) {
            struct Q *q = &busy[i];
            const char *how = "score";
            if (!use_grammar && ms >= 0 && picks[i] >= 0) {   /* the picked choice's word */
                char cc[160], *t, *save = NULL;
                int k = 0;
                snprintf(cc, sizeof cc, "%s", q->choices);
                for (t = strtok_s(cc, "|", &save); t; t = strtok_s(NULL, "|", &save), k++)
                    if (k == picks[i]) snprintf(q->answer, sizeof q->answer, "goal=%s", t);
                q->ms = ms / n;
            } else {                    /* two choices begin alike, or llm_mode=grammar: the grammar way */
                q->ms = m_ask(SYSTEM, q->user, q->grammar, q->answer, sizeof q->answer);
                how = "grammar";
                if (q->ms < 0) continue;
            }
            log_jsonl(q, how);
            EnterCriticalSection(&cs);
            if (nans < QN) answers[nans++] = *q;
            else say("an answer dropped: the answer queue is full (32)");
            LeaveCriticalSection(&cs);
        }
        EnterCriticalSection(&cs);
        nbusy = 0;
        LeaveCriticalSection(&cs);
    }
    return 0;
}

/* the game's or the window's thread: started once (at the first frame); 1 when the model is ready */
int port_mind_ready(void)
{
    if (state == 0 && InterlockedCompareExchange(&state, 4, 0) == 0) {   /* one thread starts it (4: starting) */
        const char *off = getenv("UWPORT_LLM"), *env = getenv("UWPORT_LLM_MODEL");
        static int inited;
        if ((off && !strcmp(off, "0")) || cfg_int("llm", 1) == 0) {
            InterlockedExchange(&off_fixed, 1);
            InterlockedExchange(&state, 3);
            say(off && !strcmp(off, "0") ? "language model off (UWPORT_LLM=0)" : "language model off (llm=0 in the settings)");
            return 0;
        }
        if (!(env && *env) && port_mind_models(NULL, NULL, 0) == 0) {   /* main.c said so (smart-hunting off): no load tried */
            InterlockedExchange(&off_fixed, 1);
            InterlockedExchange(&state, 3);
            return 0;
        }
        char mode[16] = "";
        if (plat_home()) port_config_get(plat_home(), "llm_mode", mode, sizeof mode);
        use_grammar = !strcmp(mode, "grammar");
        rate = cfg_int("llm_rate", 20), stale_ms = cfg_int("llm_stale_ms", 3000), idle_min = cfg_int("llm_idle_min", 10);
        if (!inited) {   /* (once: a worker started again after a failed load keeps them) */
            InitializeCriticalSection(&cs);
            wake = CreateEventA(NULL, FALSE, FALSE, NULL);
            inited = 1;
        }
        InterlockedExchange(&state, 1);
        thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        if (thread) SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
        else InterlockedExchange(&state, 3);
    }
    return state == 2 || state == 5;
}

/* the program's end: the worker stopped (it may be inside a decode: waited for, at most 5 s) and the model let go */
void port_mind_stop(void)
{
    if (!thread) return;
    InterlockedExchange(&quit, 1);
    SetEvent(wake);
    if (WaitForSingleObject(thread, 5000) != WAIT_OBJECT_0) {
        /* the worker still inside a load or a decode (it cannot be interrupted): returning into the normal exit
           would unload uwmind.dll and the ggml DLLs under it (the third review's item 1). The process ends here
           instead: no DLL_PROCESS_DETACH, no static destructors, nothing left racing. The game has saved what it
           saves before this (plat_run's loop has ended); the recording's end (main.c's port_blackbox_close after
           plat_run) is written here first. */
        void port_blackbox_close(int);
        say("the language model is still loading: the program ends without waiting for it");
        port_blackbox_close(0);
        fflush(stdout), fflush(stderr);
        if (jsonl) fflush(jsonl);
#ifdef _WIN32
        TerminateProcess(GetCurrentProcess(), 0);
#else
        _exit(0);
#endif
    }
    if (m_free) m_free();
    CloseHandle(thread);
    thread = NULL;
    if (jsonl) fclose(jsonl), jsonl = NULL;
}

/* a question for creature mob of level; kind is hunt.c's (what it is about); 0 when it is not taken: the model not
   ready, one for this creature already waiting or being answered, the rate cap reached, the queue full */
int port_mind_ask(int level, int mob, int kind, const char *user, const char *grammar)
{
    static DWORD window;
    static int in_window;
    int ok = 0;
    if (state != 2 && state != 5) return 0;
    DWORD now = GetTickCount();
    if (now - window >= 1000) window = now, in_window = 0;
    if (in_window >= rate) return 0;
    EnterCriticalSection(&cs);
    int dup = 0;
    for (int i = 0; i < na; i++) if (asks[i].level == level && asks[i].mob == mob) dup = 1;
    for (int i = 0; i < nbusy; i++) if (busy[i].level == level && busy[i].mob == mob) dup = 1;
    if (!dup && na < QN) {
        struct Q *q = &asks[na++];
        q->level = level, q->mob = mob, q->kind = kind, q->ms = 0, q->answer[0] = 0, q->asked = now;
        snprintf(q->user, sizeof q->user, "%s", user);
        snprintf(q->grammar, sizeof q->grammar, "%s", grammar);
        grammar_choices(grammar, q->choices, sizeof q->choices);
        ok = 1, in_window++;
    }
    LeaveCriticalSection(&cs);
    if (!dup && !ok) say("a question dropped: the question queue is full (32)");
    if (ok) SetEvent(wake);
    return ok;
}

/* an answer, oldest first: 1 when one was given (answers older than llm_stale_ms are dropped: the moment passed) */
int port_mind_take(int *level, int *mob, int *kind, char *answer, int n, int *ms)
{
    int got = 0;
    if (state != 2 && state != 5) return 0;
    EnterCriticalSection(&cs);
    while (nans && !got) {
        struct Q q = answers[0];
        memmove(answers, answers + 1, sizeof answers[0] * (size_t)--nans);
        if (stale_ms > 0 && GetTickCount() - q.asked > (DWORD)stale_ms) {
            char msg[160];
            snprintf(msg, sizeof msg, "an answer for #%d dropped: %lu ms after its question (llm_stale_ms %d)", q.mob,
                     (unsigned long)(GetTickCount() - q.asked), stale_ms);
            say(msg);
            continue;
        }
        *level = q.level, *mob = q.mob, *kind = q.kind, *ms = q.ms;
        snprintf(answer, (size_t)n, "%s", q.answer);
        got = 1;
    }
    LeaveCriticalSection(&cs);
    return got;
}

