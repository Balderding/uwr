/* coretest.c: the rules core (underworld-exhumed's C, tools/core.py) driven by name from stdin, for the proofs: the
 * same commands go to UW.EXE in the emulator (tools/coreproof.py), and every answer must be the same.
 *
 *   seed HEX              Borland rand's state            gseed            -> the state
 *   w NAME OFF HEX        bytes into a name               r NAME OFF N     -> its bytes (hex)
 *   p NAME OFF SPEC       a pointer into a name           q NAME OFF       -> the pointer there, as a SPEC
 *   call NAME ARG...      a call                          -> "ret HEX" and "calls N a,b,..." (the outside calls)
 *   exe PATH              the math tables from UW.EXE
 *   dirs GAME WRITE       the folders the core reads (the game's) and writes
 * A SPEC is 0 (null), L+HEX (the level block), @NAME+HEX (a name); an ARG is a number or a SPEC. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct core_sym { const char *name; void *addr; char fn; };
extern const struct core_sym core_syms[];
extern const int core_nsyms;
extern uint32_t core_seed;
extern char core_calls[];
extern int core_ncalls, core_callpos;
int core_math_init(const unsigned char *exe, long size);
extern char core_game_dir[260], core_write_dir[260];

static unsigned char block[0x10000] __attribute__((aligned(16))); /* the level block (mapdata) */
static unsigned char scratch[0x800] __attribute__((aligned(16))); /* S+: scratch both sides have (DOS: DS:E000) */

static const struct core_sym *find(const char *name)
{
    int lo = 0, hi = core_nsyms - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2, c = strcmp(core_syms[m].name, name);
        if (!c) return &core_syms[m];
        if (c < 0) lo = m + 1; else hi = m - 1;
    }
    fprintf(stderr, "no name %s\n", name);
    exit(2);
}

static void *spec(const char *s)
{
    if (!strcmp(s, "0")) return 0;
    if (s[0] == 'L' && s[1] == '+') return block + strtoul(s + 2, 0, 16);
    if (s[0] == 'S' && s[1] == '+') return scratch + strtoul(s + 2, 0, 16);
    if (s[0] == '@') {
        char n[128];
        const char *plus = strchr(s, '+');
        size_t k = plus ? (size_t)(plus - s - 1) : strlen(s + 1);
        memcpy(n, s + 1, k), n[k] = 0;
        return (unsigned char *)find(n)->addr + (plus ? strtoul(plus + 1, 0, 16) : 0);
    }
    return (void *)(intptr_t)strtoll(s, 0, 0);
}

/* a pointer back to a SPEC: in the block, or the nearest name at or below it (data only) */
static char known[8192];
static void unspec(void *p, char *out)
{
    unsigned char *q = p;
    if (!q) { strcpy(out, "0"); return; }
    if (q >= block && q < block + sizeof block) { sprintf(out, "L+%x", (unsigned)(q - block)); return; }
    if (q >= scratch && q < scratch + sizeof scratch) { sprintf(out, "S+%x", (unsigned)(q - scratch)); return; }
    const struct core_sym *best = 0;
    for (int i = 0; i < core_nsyms; i++)
        if (!core_syms[i].fn && known[i] && (unsigned char *)core_syms[i].addr <= q
            && (!best || (unsigned char *)core_syms[i].addr > (unsigned char *)best->addr))
            best = &core_syms[i];
    if (best && q - (unsigned char *)best->addr < 0x10000) sprintf(out, "@%s+%x", best->name, (unsigned)(q - (unsigned char *)best->addr));
    else strcpy(out, "?");
}

/* known: the names both sides have (a pointer is said relative to one of them) */
static void mark(const char *name) { known[find(name) - core_syms] = 1; }

/* the trace (a core built with -finstrument-functions, tools/core.py's CORE_INSTR): each function entered, in order */
static void *trace_fn[200000];
static int trace_n, trace_on;
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void *fn, void *caller)
{
    (void)caller;
    if (trace_on && trace_n < (int)(sizeof trace_fn / sizeof *trace_fn)) trace_fn[trace_n++] = fn;
}
__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void *fn, void *caller) { (void)fn; (void)caller; }

static int hexval(int c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; }

typedef long long (*Fn)(long long, long long, long long, long long, long long, long long, long long, long long,
                        long long, long long, long long, long long);

int main(void)
{
    static char line[0x40000];
    setvbuf(stdout, 0, _IOFBF, 1 << 16);
    static long nlines;
    static char last[64];
    while (fgets(line, sizeof line, stdin)) {
        nlines++;
        memcpy(last, line, 63);
        static char *tok[4096];
        int n = 0;
        for (char *t = strtok(line, " \r\n"); t && n < 4096; t = strtok(0, " \r\n")) tok[n++] = t;
        if (!n) continue;
        if (!strcmp(tok[0], "seed")) core_seed = (uint32_t)strtoul(tok[1], 0, 16);
        else if (!strcmp(tok[0], "gseed")) printf("%08x\n", core_seed);
        else if (!strcmp(tok[0], "exe")) {
            FILE *f = fopen(tok[1], "rb");
            static unsigned char exe[0x80000];
            long k = f ? (long)fread(exe, 1, sizeof exe, f) : 0;
            if (f) fclose(f);
            if (!core_math_init(exe, k)) { fprintf(stderr, "no UW.EXE\n"); return 2; }
        } else if (!strcmp(tok[0], "w")) {
            unsigned char *d = !strncmp(tok[1], "L+", 2) || !strncmp(tok[1], "S+", 2) ? spec(tok[1]) : find(tok[1])->addr;
            d += strtoul(tok[2], 0, 16);
            for (const char *h = tok[3]; h[0] && h[1]; h += 2) *d++ = (unsigned char)(hexval(h[0]) << 4 | hexval(h[1]));
        } else if (!strcmp(tok[0], "r")) {
            unsigned char *d = !strncmp(tok[1], "L+", 2) || !strncmp(tok[1], "S+", 2) ? spec(tok[1]) : find(tok[1])->addr;
            d += strtoul(tok[2], 0, 16);
            unsigned k = (unsigned)strtoul(tok[3], 0, 16);
            for (unsigned i = 0; i < k; i++) printf("%02x", d[i]);
            printf("\n");
        } else if (!strcmp(tok[0], "p")) {
            unsigned char *d = (unsigned char *)find(tok[1])->addr + strtoul(tok[2], 0, 16);
            void *v = spec(tok[3]);
            memcpy(d, &v, sizeof v);
        } else if (!strcmp(tok[0], "q")) {
            unsigned char *d = (unsigned char *)find(tok[1])->addr + strtoul(tok[2], 0, 16);
            void *v;
            char out[160];
            memcpy(&v, d, sizeof v);
            unspec(v, out);
            printf("%s\n", out);
        } else if (!strcmp(tok[0], "call") || !strcmp(tok[0], "callp")) {
            const struct core_sym *s = find(tok[1]);
            long long a[12] = {0};
            for (int i = 2; i < n && i < 14; i++) a[i - 2] = (long long)(intptr_t)spec(tok[i]);
            core_ncalls = core_callpos = 0;
            long long r = ((Fn)s->addr)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]);
            if (tok[0][4] == 'p') { char out[160]; unspec((void *)(intptr_t)r, out); printf("ret %s\n", out); }
            else printf("ret %llx\n", (unsigned long long)r);
            printf("calls %d", core_ncalls);
            for (int i = 0; i < core_callpos; i += (int)strlen(core_calls + i) + 1) printf("%c%s", i ? ',' : ' ', core_calls + i);
            printf("\n");
        } else if (!strcmp(tok[0], "dirs")) {
            snprintf(core_game_dir, 260, "%s", tok[1]);
            snprintf(core_write_dir, 260, "%s", tok[2]);
        } else if (!strcmp(tok[0], "traceon")) {
            trace_n = 0, trace_on = 1;
        } else if (!strcmp(tok[0], "traceoff")) {
            /* the functions entered, by name (those the table names; the others as ?) */
            trace_on = 0;
            printf("trace %d", trace_n);
            for (int i = 0; i < trace_n; i++) {
                const char *nm = "?";
                for (int k = 0; k < core_nsyms; k++)
                    if (core_syms[k].fn && core_syms[k].addr == trace_fn[i]) { nm = core_syms[k].name; break; }
                printf(" %s", nm);
            }
            printf("\n");
        } else if (!strcmp(tok[0], "known")) {
            for (int i = 1; i < n; i++) mark(tok[i]);
        } else { fprintf(stderr, "what: %s\n", tok[0]); return 2; }
        fflush(stdout);
    }
    fprintf(stderr, "end of input (%s) after %ld lines, the last %.60s\n", ferror(stdin) ? "an error" : "eof", nlines, last);
    return 0;
}
