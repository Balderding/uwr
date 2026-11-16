/* uwmind.c: our own Strata's runner (LLM_NPC_AI.md), as uwmind.dll beside llama.cpp's DLLs (dist\UW1\llm\bin). The
 * game loads it at run time (port/win32/uwmindhost.c): when it or the model is missing the game plays on with its
 * rules alone. Every call is made on the game's worker thread, never its own.
 *
 *   uwm_load(bindir, model, ngl, info, n)   the backends found in bindir (ggml picks its CPU DLL for this processor,
 *                                           Vulkan when there is a GPU), the model with ngl layers on the GPU (999
 *                                           all), one context; the model or the context failing on the GPU: both again
 *                                           on the CPU. On the CPU a quarter of the cores at low priority (a thread
 *                                           pool of our own: the game keeps the rest). info says where it went
 *   uwm_score(system, n, users, choices, picks, info, ni)
 *                                           n questions (at most UWM_BATCH) at once, each answered by one forward pass:
 *                                           the standing instructions (system) are read once into sequence 0 and kept;
 *                                           each question is a copy of that sequence plus its own words and "goal=",
 *                                           all in one batch; its answer is the choice (choices[i]: "a|b|c") whose
 *                                           first token scores highest there. picks[i] the index, -1 when two choices
 *                                           begin with the same token (then the caller asks uwm_ask). Returns the ms
 *   uwm_ask(system, user, grammar, out, n)  one question generated under a grammar (GBNF), greedy: the fallback and
 *                                           the setting llm_mode=grammar. Returns the ms, or -1
 *   uwm_free()                              everything let go (the game's exit, the idle unload)
 *
 * Built by tools\buildmind.cmd (clang, against build\llama). On Linux (the user, 2026-11-16): libuwmind.so, built by
 * tools\buildmind_linux.py against llama.cpp's own Linux build of the same tag (libllama.so.0 and the libggml*.so
 * beside it in llm/bin); the same code but the clock, the processors' count and the export mark. */
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#define strtok_s strtok_r   /* (the same three arguments) */
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "llama.h"
#include "ggml-backend.h"

#ifdef _WIN32
#define UWM_API __declspec(dllexport)
#else
#define UWM_API __attribute__((visibility("default")))
#endif
#define UWM_BATCH 8

static struct llama_model *model;
static struct llama_context *ctx;
static const struct llama_vocab *vocab;
static struct ggml_threadpool *pool;
static void (*pool_free)(struct ggml_threadpool *);
static int backend_up;
static char cached_system[8192];        /* the instructions held in sequence 0 (2026-11-14: was 2048, shorter than
                                           FORMAT 2's text, so the copy was cut, never matched, and the instructions
                                           were read again for every question) */
static int prefix_len;                  /* their tokens, 0 when sequence 0 is not set up */
static char suffix[256];                /* the template's text after the user's words (the assistant's start) */
static char prefix_text[8192];          /* the template's text before the user's words (template_split's full) */
static int choice_token_ok(const char *word);   /* (below: a choice word begins alike alone and after "goal=") */
static int score_off;                   /* the load's check failed: every pick -1 (the caller asks the grammar way) */

static double now_ms(void)
{
#ifndef _WIN32
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
#else
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#endif
}

static void quiet(enum ggml_log_level level, const char *text, void *user) { (void)level, (void)text, (void)user; }

UWM_API void uwm_free(void)
{
    if (ctx) llama_free(ctx), ctx = NULL;
    if (pool && pool_free) pool_free(pool);
    pool = NULL;
    if (model) llama_model_free(model), model = NULL;
    if (backend_up) llama_backend_free(), backend_up = 0;
    prefix_len = 0, cached_system[0] = 0;
}

static struct llama_context *make_ctx(int threads)
{
    struct llama_context_params cp = llama_context_default_params();
    /* n_batch = n_ctx: llama_decode aborts the program (GGML_ASSERT) on a batch larger than n_batch, and eight long
       questions together reach ~2500 tokens (the second review); uwm_score also splits what would not fit */
    cp.n_ctx = 8192, cp.n_batch = 8192, cp.n_ubatch = 512, cp.n_seq_max = UWM_BATCH + 1, cp.kv_unified = true;
    cp.n_threads = cp.n_threads_batch = threads;
    return llama_init_from_model(model, cp);
}

UWM_API int uwm_load(const char *bindir, const char *path, int ngl, char *info, int n)
{
    double t0 = now_ms();
    llama_log_set(quiet, NULL);
    if (!backend_up) {
        ggml_backend_load_all_from_path(bindir);
        llama_backend_init();
        backend_up = 1;
    }
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int threads = (int)si.dwNumberOfProcessors / 4;   /* a quarter of the logical processors: the game keeps the rest */
#else
    int threads = (int)sysconf(_SC_NPROCESSORS_ONLN) / 4;   /* (the same quarter) */
#endif
    if (threads < 1) threads = 1;
    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = ngl;
    model = llama_model_load_from_file(path, mp);
    if (model) ctx = make_ctx(threads);
    if ((!model || !ctx) && ngl) {      /* no room on the GPU for the model or its context (beside DLSS, ray tracing) */
        if (model) llama_model_free(model);
        mp.n_gpu_layers = ngl = 0;
        model = llama_model_load_from_file(path, mp);
        if (model) ctx = make_ctx(threads);
    }
    if (!model || !ctx) {
        snprintf(info, n, "could not load %s%s", path, model ? " (no context)" : "");
        uwm_free();
        return 0;
    }
    vocab = llama_model_get_vocab(model);
    {   /* the check at load: hunt.c's choice words tokenised alone and after "goal=" must begin alike, else every
           question goes the grammar way (uwm_score's picks -1) */
        static const char *const W[] = {"hunt", "ambush", "rally", "return", "attack", "investigate", "flee", "wait"};
        score_off = 0;
        for (size_t i = 0; i < sizeof W / sizeof W[0]; i++) if (!choice_token_ok(W[i])) score_off = 1;
    }
    if (!ngl) {                         /* the CPU's threads: a pool of our own, low priority */
        ggml_backend_reg_t reg = ggml_backend_reg_by_name("CPU");
        struct ggml_threadpool *(*pool_new)(struct ggml_threadpool_params *) = NULL;
        if (reg) {
            pool_new = (struct ggml_threadpool *(*)(struct ggml_threadpool_params *))ggml_backend_reg_get_proc_address(reg, "ggml_threadpool_new");
            pool_free = (void (*)(struct ggml_threadpool *))ggml_backend_reg_get_proc_address(reg, "ggml_threadpool_free");
        }
        if (pool_new && pool_free) {
            static struct ggml_threadpool_params tp;
            memset(&tp, 0, sizeof tp);
            tp.n_threads = threads, tp.prio = GGML_SCHED_PRIO_LOW, tp.poll = 0;
            pool = pool_new(&tp);
            if (pool) llama_attach_threadpool(ctx, pool, pool);
        }
    }
    const char *dev = "the CPU";
    for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
        ggml_backend_dev_t d = ggml_backend_dev_get(i);
        if (ngl && ggml_backend_dev_type(d) == GGML_BACKEND_DEVICE_TYPE_GPU) { dev = ggml_backend_dev_description(d); break; }
    }
    snprintf(info, n, "%.2f GB, %d layers, on %s (%d threads%s), loaded in %.0f ms%s", llama_model_size(model) / 1073741824.0,
             llama_model_n_layer(model), dev, threads, pool ? ", low priority" : "", now_ms() - t0,
             score_off ? "; its tokenizer joins a choice to \"goal=\": every question the grammar way" : "");
    return 1;
}

/* the chat template split around the user's words: prefix (the instructions and the user's opening) and suffix */
static int template_split(const char *system)
{
    static char full[8192];
    const char *mark = "\x01UWMARK\x01";
    llama_chat_message chat[2] = {{"system", system}, {"user", mark}};
    const char *tmpl = llama_model_chat_template(model, NULL);
    int pl = llama_chat_apply_template(tmpl, chat, 2, true, full, sizeof full);
    if (pl < 0 || pl >= (int)sizeof full)
        pl = snprintf(full, sizeof full, "<|im_start|>system\n%s<|im_end|>\n<|im_start|>user\n%s<|im_end|>\n<|im_start|>assistant\n", system, mark);
    full[pl] = 0;
    char *m = strstr(full, mark);
    if (!m) return 0;
    *m = 0;
    snprintf(prefix_text, sizeof prefix_text, "%s", full);
    snprintf(suffix, sizeof suffix, "%s", m + strlen(mark));
    return 1;
}

/* sequence 0: the instructions, read once (again when they change, or after uwm_ask cleared the memory) */
static int prefix_ready(const char *system)
{
    static llama_token toks[4096];
    if (prefix_len && !strcmp(cached_system, system)) return 1;
    llama_memory_clear(llama_get_memory(ctx), true);
    prefix_len = 0;
    if (!template_split(system)) return 0;
    int nt = llama_tokenize(vocab, prefix_text, (int)strlen(prefix_text), toks, 4096, true, true);
    if (nt <= 0) return 0;
    struct llama_batch b = llama_batch_init(nt, 0, 1);
    for (int i = 0; i < nt; i++) {
        b.token[i] = toks[i], b.pos[i] = i, b.n_seq_id[i] = 1, b.seq_id[i][0] = 0, b.logits[i] = 0;
    }
    b.n_tokens = nt;
    int ok = llama_decode(ctx, b) == 0;
    llama_batch_free(b);
    if (!ok) return 0;
    prefix_len = nt;
    snprintf(cached_system, sizeof cached_system, "%s", system);
    return 1;
}

static llama_token first_token(const char *word)
{
    llama_token t[16];
    int n = llama_tokenize(vocab, word, (int)strlen(word), t, 16, false, false);
    return n > 0 ? t[0] : -1;
}

/* the word's first token as it follows "goal=" (the third review: a SentencePiece model tokenises "hunt" alone as
   "_hunt", which never follows "goal="); -2 when "goal=" itself is not kept whole before it (Qwen's pre-tokenizer
   joins "=hunt" into one piece), -1 when it cannot be told */
static llama_token token_after_goal(const char *word)
{
    llama_token p[16], t[48];
    char s[160];
    int np = llama_tokenize(vocab, "goal=", 5, p, 16, false, false);
    snprintf(s, sizeof s, "goal=%s", word);
    int nt = llama_tokenize(vocab, s, (int)strlen(s), t, 48, false, false);
    if (np <= 0 || nt <= np) return -1;
    for (int i = 0; i < np; i++) if (p[i] != t[i]) return -2;
    return t[np];
}
/* a choice scored by its first token only when that token is the one after "goal=": the same alone and in context;
   where the tokenizer joins the boundary (no in-context token to compare: Qwen, whose picks match the grammar way's,
   LLM_NPC_AI.md) the token alone must at least be the word's own start, no leading-space mark (SentencePiece's) */
static int choice_token_ok(const char *word)
{
    llama_token a = first_token(word), c = token_after_goal(word);
    if (a < 0) return 0;
    if (c == -2) {
        char piece[64];
        int k = llama_token_to_piece(vocab, a, piece, sizeof piece - 1, 0, false);
        return k > 0 && !strncmp(piece, word, (size_t)k);
    }
    return a == c;
}

UWM_API int uwm_score(const char *system, int n, const char *const *users, const char *const *choices, int *picks,
                      char *info, int ni)
{
    static llama_token q[UWM_BATCH][1024];
    int nq[UWM_BATCH], last[UWM_BATCH], total = 0;
    if (!ctx || n < 1) return -1;
    if (n > UWM_BATCH) n = UWM_BATCH;
    double a = now_ms();
    if (!prefix_ready(system)) return -1;
    llama_memory_t mem = llama_get_memory(ctx);
    for (int i = 0; i < n; i++) {
        char text[2048];
        snprintf(text, sizeof text, "%s%sgoal=", users[i], suffix);
        nq[i] = llama_tokenize(vocab, text, (int)strlen(text), q[i], 1024, false, true);
        if (nq[i] <= 0) return -1;
        total += nq[i];
        llama_memory_seq_rm(mem, i + 1, -1, -1);
        llama_memory_seq_cp(mem, 0, i + 1, -1, -1);   /* the instructions, already read */
    }
    if (total > (int)llama_n_batch(ctx) || prefix_len + total > (int)llama_n_ctx(ctx)) {
        /* too big for one decode (it would abort the program): one question at a time */
        int all = 0;
        for (int i = 0; i < n; i++) llama_memory_seq_rm(mem, i + 1, -1, -1);
        if (n == 1) return -1;          /* (one question alone too long: the caller takes the grammar way or drops it) */
        for (int i = 0; i < n; i++) {
            int ms1 = uwm_score(system, 1, &users[i], &choices[i], &picks[i], NULL, 0);
            if (ms1 < 0) picks[i] = -1;
            else all += ms1;
        }
        if (info && ni > 0) snprintf(info, ni, "%d questions one at a time (%d tokens together)", n, total);
        return all;
    }
    struct llama_batch b = llama_batch_init(total, 0, 1);
    int k = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < nq[i]; j++, k++) {
            b.token[k] = q[i][j], b.pos[k] = prefix_len + j, b.n_seq_id[k] = 1, b.seq_id[k][0] = i + 1;
            b.logits[k] = j == nq[i] - 1;
            if (j == nq[i] - 1) last[i] = k;
        }
    b.n_tokens = total;
    int ok = llama_decode(ctx, b) == 0;
    llama_batch_free(b);
    for (int i = 0; i < n; i++) {
        picks[i] = -1;
        if (!ok) continue;
        const float *lg = llama_get_logits_ith(ctx, last[i]);
        char c[256], *w, *save = NULL;
        llama_token seen[8];
        int nc = 0, clash = 0, best = -1;
        float bestv = -1e30f;
        snprintf(c, sizeof c, "%s", choices[i]);
        for (w = strtok_s(c, "|", &save); w && nc < 8; w = strtok_s(NULL, "|", &save), nc++) {
            llama_token t = first_token(w);
            if (score_off || !choice_token_ok(w)) clash = 1;   /* not the token that would follow "goal=": the grammar way */
            for (int s = 0; s < nc; s++) if (seen[s] == t) clash = 1;
            seen[nc] = t;
            if (t >= 0 && lg && lg[t] > bestv) bestv = lg[t], best = nc;
        }
        picks[i] = clash ? -1 : best;
    }
    for (int i = 0; i < n; i++) llama_memory_seq_rm(mem, i + 1, -1, -1);
    if (info && ni > 0) snprintf(info, ni, "%d question%s, %d tokens", n, n == 1 ? "" : "s", total);
    return ok ? (int)(now_ms() - a) : -1;
}

UWM_API int uwm_ask(const char *system, const char *user, const char *grammar, char *out, int n)
{
    static char prompt[8192];
    static llama_token toks[4096];
    if (!ctx || n < 2) return -1;
    double a = now_ms();
    out[0] = 0;
    llama_chat_message chat[2] = {{"system", system}, {"user", user}};
    const char *tmpl = llama_model_chat_template(model, NULL);
    int pl = llama_chat_apply_template(tmpl, chat, 2, true, prompt, sizeof prompt);
    if (pl < 0 || pl >= (int)sizeof prompt)   /* a template llama.cpp does not know: ChatML */
        pl = snprintf(prompt, sizeof prompt, "<|im_start|>system\n%s<|im_end|>\n<|im_start|>user\n%s<|im_end|>\n<|im_start|>assistant\n", system, user);
    if (pl <= 0 || pl >= (int)sizeof prompt) return -1;
    prompt[pl] = 0;
    int nt = llama_tokenize(vocab, prompt, pl, toks, 4096, true, true);
    if (nt <= 0) return -1;
    llama_memory_clear(llama_get_memory(ctx), true);
    prefix_len = 0;                     /* (sequence 0 read again by the next uwm_score) */
    struct llama_sampler *s = llama_sampler_chain_init(llama_sampler_chain_default_params());
    struct llama_sampler *g = llama_sampler_init_grammar(vocab, grammar, "root");
    if (!g) { llama_sampler_free(s); return -1; }
    llama_sampler_chain_add(s, g);
    llama_sampler_chain_add(s, llama_sampler_init_greedy());
    int ok = llama_decode(ctx, llama_batch_get_one(toks, nt)) == 0, ol = 0;
    for (int i = 0; ok && i < 24; i++) {
        llama_token t = llama_sampler_sample(s, ctx, -1);
        if (llama_vocab_is_eog(vocab, t)) break;
        char piece[64];
        int k = llama_token_to_piece(vocab, t, piece, sizeof piece, 0, false);
        if (k > 0 && ol + k < n - 1) memcpy(out + ol, piece, k), ol += k, out[ol] = 0;
        if (llama_decode(ctx, llama_batch_get_one(&t, 1))) break;
    }
    llama_sampler_free(s);
    return ok ? (int)(now_ms() - a) : -1;
}
