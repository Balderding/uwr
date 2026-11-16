/* uwmind_tokcheck.c: uwmind.c's load-time check of the choice tokens (the third review's item 4) on llama.cpp's
 * vocabulary-only test files (external\llama.cpp\models\ggml-vocab-*.gguf): a SentencePiece vocabulary (llama-spm,
 * phi-3) must fail it (every question the grammar way), the GPT-2 style ones (qwen2, qwen35, gpt-2, llama-bpe) pass.
 *   uwmind_tokcheck VOCAB.gguf expect(0|1) ...      built by tools\buildtokcheck.cmd; exit code: the mismatches */
#include "../src/uwmind.c"

int main(int argc, char **argv)
{
    static const char *const W[] = {"hunt", "ambush", "rally", "return", "attack", "investigate", "flee", "wait"};
    int bad = 0;
    llama_log_set(quiet, NULL);
    llama_backend_init();
    for (int a = 1; a + 1 < argc; a += 2) {
        struct llama_model_params mp = llama_model_default_params();
        mp.vocab_only = true;
        struct llama_model *m = llama_model_load_from_file(argv[a], mp);
        if (!m) { printf("%s: cannot load\n", argv[a]); bad++; continue; }
        vocab = llama_model_get_vocab(m);
        int ok = 1;
        for (size_t i = 0; i < sizeof W / sizeof W[0]; i++) if (!choice_token_ok(W[i])) ok = 0;
        int want = atoi(argv[a + 1]);
        printf("%s: the choice tokens %s (expected %s): %s\n", strrchr(argv[a], '\\') ? strrchr(argv[a], '\\') + 1 : argv[a],
               ok ? "scored" : "the grammar way", want ? "scored" : "the grammar way", ok == want ? "PASS" : "FAIL");
        if (ok != want) bad++;
        llama_model_free(m);
    }
    llama_backend_free();
    return bad;
}
