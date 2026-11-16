#include "conv.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- extraction: CNV.ARK -> data\conversations\NNN.txt ---------------------------------------------- */
int conv_extract(const char *data_dir, const char *game_dir) {
    char path[700];
    snprintf(path, sizeof path, "%s\\DATA\\CNV.ARK", game_dir);
    size_t mark = arena_mark();
    Bytes d = file_read(path);
    if (d.size < 2) return arena_reset(mark), 0;
    int n = (int)rd16(d.data), made = 0;
    for (int k = 0; k < n && 2 + 4 * (size_t)(k + 1) <= d.size; k++) {
        u32 o = rd32(d.data + 2 + 4 * k);
        if (!o || o + 16 > d.size) continue;
        char dst[700];
        snprintf(dst, sizeof dst, "%s\\conversations\\%03d.txt", data_dir, k);
        if (file_exists(dst)) continue;
        size_t cap = 1 << 20, tn = 0;
        char *t = arena_alloc(cap);
        tn += (size_t)snprintf(t + tn, cap - tn, "# conversation %d from CNV.ARK, as the game's machine runs it (see src\\conv.h); edit freely\r\nheader", k);
        for (int i = 0; i < 8; i++) tn += (size_t)snprintf(t + tn, cap - tn, " %04x", rd16(d.data + o + 2 * i));
        tn += (size_t)snprintf(t + tn, cap - tn, "\r\n");
        int words = (int)rd16(d.data + o + 4), imports = (int)rd16(d.data + o + 14);
        size_t p = o + 16;
        for (int i = 0; i < imports && p + 2 <= d.size; i++) {
            int len = (int)rd16(d.data + p);
            if (p + 2 + (size_t)len + 8 > d.size) break;
            tn += (size_t)snprintf(t + tn, cap - tn, "import %.*s %d %d %04x %04x\r\n", len, (const char *)d.data + p + 2, rd16(d.data + p + 2 + len),
                                   rd16(d.data + p + 4 + len), rd16(d.data + p + 6 + len), rd16(d.data + p + 8 + len));
            p += 2 + (size_t)len + 8;
        }
        tn += (size_t)snprintf(t + tn, cap - tn, "code %d\r\n", words);
        for (int i = 0; i < words && p + 2 <= d.size; i++, p += 2)
            tn += (size_t)snprintf(t + tn, cap - tn, "%04x%s", rd16(d.data + p), (i % 16 == 15 || i == words - 1) ? "\r\n" : " ");
        make_dirs_for(dst);
        if (file_write(dst, t, tn)) made++;
    }
    arena_reset(mark);
    return made;
}

/* --- the machine -------------------------------------------------------------------------------------- */
enum {
    OP_NOP, OP_ADD, OP_MUL, OP_SUB, OP_DIV, OP_MOD, OP_OR, OP_AND, OP_NOT, OP_TSTGT, OP_TSTGE, OP_TSTLT, OP_TSTLE, OP_TSTEQ,
    OP_TSTNE, OP_JMP, OP_BEQ, OP_BNE, OP_BRA, OP_CALL, OP_CALLI, OP_RET, OP_PUSHI, OP_PUSHI_EFF, OP_POP, OP_SWAP, OP_PUSHBP,
    OP_POPBP, OP_SPTOBP, OP_BPTOSP, OP_ADDSP, OP_FETCHM, OP_STO, OP_OFFSET, OP_START, OP_SAVE_REG, OP_PUSH_REG, OP_STRCMP,
    OP_EXIT, OP_SAY, OP_RESPOND, OP_NEG
};

#define STACK 4096
static struct {
    Conv c;
    u16 *code;
    int ncode, ip, sp, bp, calls, nvars;
    i16 st[STACK];
    i16 reg;
    char fname[128][32]; /* imported functions by number */
    /* strings: the conversation's block, then any made while talking (names) */
    char *str[1024];
    int nstr;
    int menu_ids[CONV_MAX_OPTIONS]; /* the string id (babl_fmenu) or the place (babl_menu) of each option shown */
    bool fmenu;
    char data[600];
    i16 quest[64];
} V;

static void push(int v) {
    if (V.sp < STACK - 1) V.st[++V.sp] = (i16)v;
}
static int pop(void) { return V.sp > 0 ? V.st[V.sp--] : 0; }
static int mem(int a) { return a >= 0 && a < STACK ? V.st[a] : 0; }
static void set_mem(int a, int v) {
    if (a >= 0 && a < STACK) V.st[a] = (i16)v;
}
static const char *string(int id) { return id >= 0 && id < V.nstr && V.str[id] ? V.str[id] : ""; }
static int add_string(const char *s) {
    if (V.nstr >= 1024) return 0;
    V.str[V.nstr] = malloc(strlen(s) + 1);
    if (V.str[V.nstr]) strcpy(V.str[V.nstr], s);
    return V.nstr++;
}

/* "@GS8": a global string (the string id in memory 8), "@GI3" a global number; others not yet */
static void expand(const char *s, char *out, size_t n) {
    size_t o = 0;
    for (; *s && o + 1 < n; s++) {
        if (s[0] == '@' && (s[1] == 'G' || s[1] == 'S' || s[1] == 'P') && (s[2] == 'S' || s[2] == 'I') && isdigit((u8)s[3])) {
            int num = atoi(s + 3), v = s[1] == 'G' ? mem(num) : s[1] == 'S' ? mem(V.bp + num) : mem(mem(V.bp + num));
            char tmp[160];
            if (s[2] == 'S') snprintf(tmp, sizeof tmp, "%s", string(v));
            else snprintf(tmp, sizeof tmp, "%d", v);
            for (char *q = tmp; *q && o + 1 < n; q++) out[o++] = *q;
            s += 3;
            while (isdigit((u8)s[1])) s++;
            continue;
        }
        out[o++] = *s;
    }
    out[o] = 0;
}

static void say(const char *s) {
    Conv *c = &V.c;
    if (c->nlines >= CONV_MAX_LINES) {
        memmove(c->lines[0], c->lines[1], sizeof c->lines[0] * (CONV_MAX_LINES - 1));
        c->nlines--;
    }
    expand(s, c->lines[c->nlines++], sizeof c->lines[0]);
}

/* a built-in. On the stack: the argument count on top, the arguments (addresses) under it. */
static void builtin(int f) {
    const char *name = f >= 0 && f < 128 ? V.fname[f] : "";
    int argc = mem(V.sp);
#define ARG(i) mem(V.sp - (i))
    V.reg = 0;
    if (!strcmp(name, "babl_menu") || !strcmp(name, "babl_fmenu")) {
        /* the options: string ids from the array at arg 1 (ending at 0); babl_fmenu shows those whose flag (array
         * at arg 2) is set and answers with the string id */
        V.fmenu = name[5] == 'f';
        int a = ARG(1), fl = V.fmenu ? ARG(2) : 0;
        V.c.noptions = 0;
        for (int i = 0; i < 32 && V.c.noptions < CONV_MAX_OPTIONS; i++) {
            int id = mem(a + i);
            if (!id) break;
            if (V.fmenu && !mem(fl + i)) continue;
            expand(string(id), V.c.options[V.c.noptions], sizeof V.c.options[0]);
            V.menu_ids[V.c.noptions++] = V.fmenu ? id : i + 1;
        }
        V.c.waiting = V.c.noptions > 0;
    } else if (!strcmp(name, "print")) say(string(mem(ARG(1))));
    else if (!strcmp(name, "compare")) V.reg = !_stricmp(string(mem(ARG(1))), string(mem(ARG(2))));
    else if (!strcmp(name, "contains")) V.reg = strstr(string(mem(ARG(2))), string(mem(ARG(1)))) != NULL;
    else if (!strcmp(name, "random")) V.reg = 1 + rand() % (mem(ARG(1)) > 0 ? mem(ARG(1)) : 1);
    else if (!strcmp(name, "get_quest")) {
        int q = mem(ARG(1));
        V.reg = q >= 0 && q < 64 ? V.quest[q] : 0;
    } else if (!strcmp(name, "set_quest")) {
        int q = mem(ARG(2));
        if (q >= 0 && q < 64) V.quest[q] = (i16)mem(ARG(1));
    } else if (!strcmp(name, "sex")) V.reg = mem(ARG(2)); /* the male form (no character creation yet) */
    else if (!strcmp(name, "length")) V.reg = (int)strlen(string(mem(ARG(1))));
    else if (!strcmp(name, "babl_ask")) V.reg = add_string("");
    /* the rest (barter, inventory, traps, skills, the clock...) answer 0 for now */
    (void)argc;
#undef ARG
}

static void run(void) {
    for (int steps = 0; steps < 200000 && V.c.running && !V.c.waiting; steps++) {
        if (V.ip < 0 || V.ip >= V.ncode) {
            V.c.running = false;
            break;
        }
        int op = V.code[V.ip], a, b;
        int arg = V.ip + 1 < V.ncode ? (i16)V.code[V.ip + 1] : 0;
        if (getenv("UW_CONV_STEP")) logf_("    %04x op %02x arg %d sp %d top %d\n", V.ip, op, arg, V.sp, mem(V.sp));
        switch (op) {
        case OP_NOP: break;
        case OP_ADD: a = pop(), b = pop(), push(b + a); break;
        case OP_MUL: a = pop(), b = pop(), push(b * a); break;
        case OP_SUB: a = pop(), b = pop(), push(b - a); break;
        case OP_DIV: a = pop(), b = pop(), push(a ? b / a : 0); break;
        case OP_MOD: a = pop(), b = pop(), push(a ? b % a : 0); break;
        case OP_OR: a = pop(), b = pop(), push(b || a); break;
        case OP_AND: a = pop(), b = pop(), push(b && a); break;
        case OP_NOT: push(!pop()); break;
        case OP_TSTGT: a = pop(), b = pop(), push(b > a); break;
        case OP_TSTGE: a = pop(), b = pop(), push(b >= a); break;
        case OP_TSTLT: a = pop(), b = pop(), push(b < a); break;
        case OP_TSTLE: a = pop(), b = pop(), push(b <= a); break;
        case OP_TSTEQ: a = pop(), b = pop(), push(b == a); break;
        case OP_TSTNE: a = pop(), b = pop(), push(b != a); break;
        case OP_JMP: V.ip = (u16)arg - 1; break;
        case OP_BEQ: V.ip += pop() == 0 ? arg : 1; break;
        case OP_BNE: V.ip += pop() != 0 ? arg : 1; break;
        case OP_BRA: V.ip += arg; break;
        case OP_CALL:
            if (getenv("UW_CONV_TRACE")) logf_("  [%04x] call %04x sp %d bp %d\n", V.ip, (u16)arg, V.sp, V.bp);
            push(V.ip + 1), V.ip = (u16)arg - 1, V.calls++;
            break;
        case OP_CALLI:
            if (getenv("UW_CONV_TRACE")) logf_("  [%04x] call %s (%d args) sp %d\n", V.ip, arg >= 0 && arg < 128 ? V.fname[arg] : "?", mem(V.sp), V.sp);
            V.ip++, builtin(arg);
            break;
        case OP_RET:
            if (--V.calls < 0) V.c.running = false;
            else V.ip = pop();
            if (getenv("UW_CONV_TRACE")) logf_("  ret to %04x sp %d bp %d\n", V.ip + 1, V.sp, V.bp);
            break;
        case OP_PUSHI: push(arg), V.ip++; break;
        case OP_PUSHI_EFF: push(V.bp + arg), V.ip++; break;
        case OP_POP: pop(); break;
        case OP_SWAP:
            a = pop(), b = pop(), push(a), push(b);
            break;
        case OP_PUSHBP: push(V.bp); break;
        case OP_POPBP: V.bp = pop(); break;
        case OP_SPTOBP: V.bp = V.sp; break;
        case OP_BPTOSP: V.sp = V.bp; break;
        case OP_ADDSP:
            a = pop();
            for (int i = 0; i < a; i++) push(0);
            break;
        case OP_FETCHM: push(mem(pop())); break;
        case OP_STO: a = pop(), b = pop(), set_mem(b, a); break;
        case OP_OFFSET: a = pop(), b = pop(), push(a + b - 1); break;
        case OP_START: break;
        case OP_SAVE_REG: V.reg = (i16)pop(); break;
        case OP_PUSH_REG: push(V.reg); break;
        case OP_EXIT: V.c.running = false; break;
        case OP_SAY: say(string(pop())); break;
        case OP_RESPOND: break;
        case OP_NEG: push(-pop()); break;
        default:
            if (getenv("UW_CONV_TRACE")) logf_("  [%04x] unknown opcode %04x: the conversation stops\n", V.ip, op);
            V.c.running = false;
            break;
        }
        if (getenv("UW_CONV_TRACE") && !V.c.running) logf_("  [%04x] stopped (op %02x, calls %d)\n", V.ip, op, V.calls);
        V.ip++;
    }
}

static void load_block(int block) {
    for (int i = 0; i < V.nstr; i++) free(V.str[i]), V.str[i] = NULL;
    V.nstr = 0;
    char path[700];
    snprintf(path, sizeof path, "%s\\strings\\block%04d.txt", V.data, block);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        const char *end = nl ? nl : (const char *)f.data + f.size;
        int id;
        const char *colon = memchr(p, ':', (size_t)(end - p));
        if (sscanf(p, "%d:", &id) == 1 && colon && id >= 0 && id < 1024) {
            const char *s = colon + 2 <= end ? colon + 2 : end;
            char buf[2048];
            size_t n = 0;
            for (; s < end && *s != '\r' && n + 1 < sizeof buf; s++) {
                if (s[0] == '\\' && s + 1 < end && s[1] == 'n') buf[n++] = '\n', s++;
                else if (s[0] == '\\' && s + 1 < end && s[1] == '\\') buf[n++] = '\\', s++;
                else buf[n++] = *s;
            }
            buf[n] = 0;
            while (V.nstr <= id) V.str[V.nstr++] = NULL;
            V.str[id] = malloc(n + 1);
            if (V.str[id]) memcpy(V.str[id], buf, n + 1);
        }
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
}

/* the conversations' own variables, kept between talks (and in saves) */
#define CONV_N 512
#define CONV_GLOBALS 128
static i16 g_glob[CONV_N][CONV_GLOBALS];
static int g_glob_n[CONV_N]; /* how many the conversation has (its memory size), 0: never spoken */
static int g_cur = -1;       /* the conversation running */
static void keep_globals(void) {
    if (g_cur < 0 || g_cur >= CONV_N) return;
    int n = MIN(V.nvars, CONV_GLOBALS);
    for (int i = 0; i < n; i++) g_glob[g_cur][i] = (i16)mem(i);
    g_glob_n[g_cur] = n;
}
void conv_forget(void) { memset(g_glob, 0, sizeof g_glob), memset(g_glob_n, 0, sizeof g_glob_n); }
bool conv_save(const char *dir) {
    char path[700];
    snprintf(path, sizeof path, "%s\\conversations.txt", dir);
    size_t cap = 512 * 1024, n = 0, mark = arena_mark();
    char *out = arena_alloc(cap);
    n += (size_t)snprintf(out + n, cap - n, "# what each conversation remembers: conversation, then its variables\r\n");
    for (int c = 0; c < CONV_N; c++) {
        if (!g_glob_n[c]) continue;
        n += (size_t)snprintf(out + n, cap - n, "conv %d", c);
        for (int i = 0; i < g_glob_n[c] && n + 16 < cap; i++) n += (size_t)snprintf(out + n, cap - n, " %d", g_glob[c][i]);
        n += (size_t)snprintf(out + n, cap - n, "\r\n");
    }
    bool ok = file_write(path, out, n);
    arena_reset(mark);
    return ok;
}
bool conv_load(const char *dir) {
    conv_forget();
    char path[700];
    snprintf(path, sizeof path, "%s\\conversations.txt", dir);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        int c, used;
        if (sscanf(p, "conv %d%n", &c, &used) == 1 && c >= 0 && c < CONV_N) {
            const char *q = p + used;
            int k = 0, v, u;
            while (k < CONV_GLOBALS && sscanf(q, "%d%n", &v, &u) == 1 && (!nl || q + u <= nl)) g_glob[c][k++] = (i16)v, q += u;
            g_glob_n[c] = k;
        }
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
    return f.size > 0;
}

static int g_attitude = 2, g_attitude_addr = -1;
void conv_set_attitude(int a) { g_attitude = a; }
int conv_attitude(void) { return g_attitude_addr >= 0 ? mem(g_attitude_addr) : g_attitude; }
bool conv_start(const char *data_dir, int n, const char *npc_name, const char *player_name) {
    g_attitude_addr = -1;
    str_copy(V.data, sizeof V.data, data_dir);
    memset(&V.c, 0, sizeof V.c);
    char path[700];
    snprintf(path, sizeof path, "%s\\conversations\\%03d.txt", data_dir, n);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    if (!f.size) return arena_reset(mark), false;
    int head[8] = {0}, words = 0, k = 0;
    static u16 code[65536];
    memset(V.fname, 0, sizeof V.fname);
    bool in_code = false;
    /* the imported variables: their names and addresses, to set them up */
    static char vname[128][32];
    static int vaddr[128];
    int nv = 0;
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        char name[64];
        int id, unk, type, ret;
        if (in_code) {
            const char *q = p, *end = nl ? nl : (const char *)f.data + f.size;
            while (k < words && k < 65536) {
                while (q < end && (*q == ' ' || *q == '\t')) q++;
                if (q >= end || *q == '\r' || *q == '\n') break; /* (strtol would skip the newline into the next line) */
                char *e;
                long v = strtol(q, &e, 16);
                if (e == q || e > end) break;
                code[k++] = (u16)v, q = e;
            }
        } else if (sscanf(p, "header %x %x %x %x %x %x %x %x", &head[0], &head[1], &head[2], &head[3], &head[4], &head[5], &head[6], &head[7]) == 8) {
        } else if (sscanf(p, "import %63s %d %d %x %x", name, &id, &unk, &type, &ret) == 5) {
            if (type == 0x111 && id >= 0 && id < 128) str_copy(V.fname[id], 32, name);
            else if (type == 0x10F && nv < 128) str_copy(vname[nv], 32, name), vaddr[nv++] = id;
        } else if (sscanf(p, "code %d", &words) == 1) in_code = true;
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
    if (getenv("UW_CONV_TRACE")) logf_("  conversation %d: %d of %d code words, %d variables, block %d\n", n, k, words, head[6], head[5]);
    V.code = code, V.ncode = k;
    V.nvars = head[6];
    load_block(head[5]);
    memset(V.st, 0, sizeof V.st);
    V.sp = MAX(0, V.nvars - 1), V.bp = V.sp, V.ip = 0, V.calls = 0, V.reg = 0;
    /* what it remembers from before (the imports below are set fresh over their own places) */
    g_cur = n;
    if (n >= 0 && n < CONV_N)
        for (int i = 0; i < g_glob_n[n] && i < V.nvars; i++) set_mem(i, g_glob[n][i]);
    /* the imported variables the programs read */
    for (int i = 0; i < nv; i++) {
        const char *s = vname[i];
        int a = vaddr[i];
        if (!strcmp(s, "play_name")) set_mem(a, add_string(player_name));
        else if (!strcmp(s, "npc_name")) set_mem(a, add_string(npc_name));
        else if (!strcmp(s, "npc_attitude")) set_mem(a, g_attitude), g_attitude_addr = a;
        else if (!strcmp(s, "play_hp") || !strcmp(s, "play_health")) set_mem(a, 30);
        else if (!strcmp(s, "play_level")) set_mem(a, 1);
        else if (!strcmp(s, "dungeon_level")) set_mem(a, 1);
    }
    V.c.running = true;
    str_copy(V.c.who, sizeof V.c.who, npc_name);
    run();
    keep_globals();
    return true;
}

const Conv *conv_state(void) { return &V.c; }

void conv_choose(int option) {
    if (!V.c.waiting || option < 0 || option >= V.c.noptions) return;
    char line[200];
    snprintf(line, sizeof line, "> %s", V.c.options[option]);
    say(line);
    V.reg = (i16)V.menu_ids[option];
    V.c.waiting = false, V.c.noptions = 0;
    run();
    keep_globals();
}

void conv_end(void) { V.c.running = false, V.c.waiting = false; }
