/* uwmind_bench.c: the first step of our own Strata (LLM_NPC_AI.md, "Our own Strata"): probe this PC, place a
 * model (all on the GPU when it fits, split, or CPU), and time one creature-sized decision.
 *
 *   uwmind_bench MODEL.gguf [cpu|gpu|N] [runs]
 *     cpu = no layers on the GPU, gpu = all, N = that many layers; default cpu (no GPU backend built yet)
 *
 * It prints the PC (each DXGI adapter's VRAM and the budget left now, cores, RAM), the backends ggml found, the
 * model's size and layers, then for each run: the prompt read (prefill), the answer (tokens, ms) and the answer.
 * The answer is held to a fixed form by a grammar, as the game's will be: the model cannot ramble or "think". */
#define COBJMACROS
#include <windows.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llama.h"
#include "ggml-backend.h"

static double now_ms(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

/* the PC: every GPU (dedicated VRAM, and what the OS budgets for us now: what is left after everything else) */
static void probe(void)
{
    SYSTEM_INFO si;
    MEMORYSTATUSEX ms = {sizeof ms};
    GetSystemInfo(&si);
    GlobalMemoryStatusEx(&ms);
    printf("pc: %lu logical processors, RAM %.1f GB (%.1f GB free)\n", si.dwNumberOfProcessors,
           ms.ullTotalPhys / 1073741824.0, ms.ullAvailPhys / 1073741824.0);
    IDXGIFactory1 *f = NULL;
    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&f))) return;
    IDXGIAdapter1 *a;
    for (UINT i = 0; IDXGIFactory1_EnumAdapters1(f, i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 d;
        IDXGIAdapter1_GetDesc1(a, &d);
        if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            char name[128];
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, sizeof name, NULL, NULL);
            printf("gpu %u: %s, VRAM %.1f GB", i, name, d.DedicatedVideoMemory / 1073741824.0);
            IDXGIAdapter3 *a3;
            if (SUCCEEDED(IDXGIAdapter1_QueryInterface(a, &IID_IDXGIAdapter3, (void **)&a3))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO m;
                if (SUCCEEDED(IDXGIAdapter3_QueryVideoMemoryInfo(a3, 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m)))
                    printf(", budget %.1f GB, in use by us %.2f GB", m.Budget / 1073741824.0, m.CurrentUsage / 1073741824.0);
                IDXGIAdapter3_Release(a3);
            }
            printf("\n");
        }
        IDXGIAdapter1_Release(a);
    }
    IDXGIFactory1_Release(f);
}

/* the game's question for one creature, as the plan has it: only what it could know, a fixed answer form */
static const char *SYSTEM =
    "You decide what a creature in a dungeon does next. Answer only in the form: goal=<goal> target=<number>. "
    "Goals: attack (fight the target), flee (run from the target), follow (stay near the target), wander, guard "
    "(stay at your post), hide. target is the number of the thing the goal is about, 0 if none.";
static const char *USER =
    "You are a goblin (number 12), health 9 of 20, armed with a club. You see: the player (number 1), health 30 "
    "of 30, carrying a lit torch and a longsword, 3 steps away, coming closer. A wounded goblin ally (number 14), "
    "health 4 of 20, is behind you. You heard the player kill a giant rat a moment ago. What do you do?";
static const char *GRAMMAR =
    "root ::= \"goal=\" goal \" target=\" num\n"
    "goal ::= \"attack\" | \"flee\" | \"follow\" | \"wander\" | \"guard\" | \"hide\"\n"
    "num ::= [0-9] | [1-9] [0-9] | [1-9] [0-9] [0-9]\n";

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "uwmind_bench MODEL.gguf [cpu|gpu|N] [runs]\n");
        return 2;
    }
    int ngl = 0;
    if (argc > 2) ngl = !strcmp(argv[2], "gpu") ? 999 : !strcmp(argv[2], "cpu") ? 0 : atoi(argv[2]);
    int runs = argc > 3 ? atoi(argv[3]) : 3;
    probe();

    /* the backends beside the exe: ggml picks its best CPU DLL for this processor, and a GPU DLL if there is one */
    char dir[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    *strrchr(dir, '\\') = 0;
    ggml_backend_load_all_from_path(dir);
    llama_backend_init();
    for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
        ggml_backend_dev_t d = ggml_backend_dev_get(i);
        size_t fr, tot;
        ggml_backend_dev_memory(d, &fr, &tot);
        printf("backend device %zu: %s (%s), %.1f of %.1f GB free\n", i, ggml_backend_dev_name(d),
               ggml_backend_dev_description(d), fr / 1073741824.0, tot / 1073741824.0);
    }
    printf("cpu features: %s\n", llama_print_system_info());

    struct llama_model_params mp = llama_model_default_params();
    mp.n_gpu_layers = ngl;
    double t0 = now_ms();
    struct llama_model *model = llama_model_load_from_file(argv[1], mp);
    if (!model) {
        fprintf(stderr, "could not load %s\n", argv[1]);
        return 1;
    }
    printf("model: %s, %.2f GB, %d layers, loaded in %.0f ms, gpu layers asked %d\n", argv[1],
           llama_model_size(model) / 1073741824.0, llama_model_n_layer(model), now_ms() - t0, ngl);
    const struct llama_vocab *vocab = llama_model_get_vocab(model);

    /* the prompt through the model's own chat template */
    llama_chat_message chat[2] = {{"system", SYSTEM}, {"user", USER}};
    static char prompt[8192];
    const char *tmpl = llama_model_chat_template(model, NULL);
    int pl = llama_chat_apply_template(tmpl, chat, 2, true, prompt, sizeof prompt);
    if (pl < 0 || pl >= (int)sizeof prompt) {   /* a template llama.cpp does not know: ChatML */
        pl = snprintf(prompt, sizeof prompt, "<|im_start|>system\n%s<|im_end|>\n<|im_start|>user\n%s<|im_end|>\n<|im_start|>assistant\n", SYSTEM, USER);
        printf("template: not recognised, ChatML used\n");
    }
    prompt[pl] = 0;
    static llama_token toks[4096];
    int nt = llama_tokenize(vocab, prompt, pl, toks, 4096, true, true);
    printf("prompt: %d tokens\n", nt);

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int threads = (int)si.dwNumberOfProcessors / 2;   /* physical cores, about: the game keeps the rest */
    if (threads < 1) threads = 1;
    for (int r = 0; r < runs; r++) {
        struct llama_context_params cp = llama_context_default_params();
        cp.n_ctx = 1024, cp.n_batch = 512, cp.n_threads = cp.n_threads_batch = threads;
        struct llama_context *ctx = llama_init_from_model(model, cp);
        struct llama_sampler *s = llama_sampler_chain_init(llama_sampler_chain_default_params());
        llama_sampler_chain_add(s, llama_sampler_init_grammar(vocab, GRAMMAR, "root"));
        llama_sampler_chain_add(s, llama_sampler_init_greedy());
        double a = now_ms();
        if (llama_decode(ctx, llama_batch_get_one(toks, nt))) {
            fprintf(stderr, "decode failed\n");
            return 1;
        }
        double b = now_ms();
        char out[256] = "";
        int n = 0, ol = 0;
        for (; n < 24; n++) {
            llama_token t = llama_sampler_sample(s, ctx, -1);
            if (llama_vocab_is_eog(vocab, t)) break;
            char piece[64];
            int k = llama_token_to_piece(vocab, t, piece, sizeof piece, 0, false);
            if (k > 0 && ol + k < (int)sizeof out - 1) memcpy(out + ol, piece, k), ol += k, out[ol] = 0;
            if (llama_decode(ctx, llama_batch_get_one(&t, 1))) break;
        }
        double c = now_ms();
        printf("run %d: prompt %.0f ms (%.0f tok/s), answer %d tokens %.0f ms, total %.0f ms: \"%s\"\n", r + 1, b - a,
               nt * 1000.0 / (b - a), n, c - b, c - a, out);
        llama_sampler_free(s);
        llama_free(ctx);
    }
    llama_model_free(model);
    return 0;
}
