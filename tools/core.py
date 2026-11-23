"""core.py: builds the rules core, underworld-exhumed's byte-matched C (uw1/src, MIT) compiled unchanged with our clang
through Exhume's portability headers (external/exhume: compat.h force-included, as exhumed's own port does), into
build/core/core.a. Every source here compiles under Turbo C to exactly its segment of UW.EXE (exhumed's gate,
uw1/matched.txt); on the host the portable.h types keep DOS's widths.

What the core uses and does not define (the front end's: screen, sound, input, files, Borland's library) is linked
from src/core_glue.c where we have written it, and otherwise from build/gen/core_stubs.c, which this writes: a
function stub records its name (core_stub_log) and returns 0, a data stub is a zeroed block. Whether a name is a
function or data is read from clang's AST of the sources that use it.

    uv run --no-project python -I tools/core.py
"""
import os
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
X = os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'src')
E = os.path.join(ROOT, 'external', 'exhume')
# CORE_INSTR=1: a second build for tracing (tools/coreproof.py's trace): every function reports its entry
INSTR = os.environ.get('CORE_INSTR') == '1'
OUT = os.path.join(ROOT, 'build', 'core_instr' if INSTR else 'core')
GEN = os.path.join(ROOT, 'build', 'gen')
LLVM = TOOLS + r'\llvm\23.1.4\bin'
DIRS = ['motion', 'combat', 'critter', 'obj', 'event', 'inv', 'conv', 'map', 'game']
EXTRA = ['sys/UTIL.C', 'sys/ARC.C', 'sys/MISCUTIL.C', 'sys/DEBUG.C', 'sys/ERROR.C',  # the rules' helpers and loaders
         'ui/GAMESTRN.C',  # the strings (STRINGS.PAK), object names and the game's messages
         'ui/INPUT.C',  # the input list (inplist, and the input modes the rules test)
         'ui/PANELS.C', 'ui/INTERACT.C']  # the screen's state the rules step: the weapon's swing, the fight's input, display_scr
# outside calls whose arguments the proofs compare (the text printed, the sounds asked for): each argument's kind,
# F a far string, N a near string, w a word, b a byte (a char: its low byte), X a far pointer (not compared)
SINKS = {'scroll_print': 'F', 'play_effect': 'bwwb', 'play_effect_here': 'bbb', 'play_effect_on_mobile': 'bXb',
         'play_instrument': 'w', 'runcutscene': 'w', 'look_nothing': 'bw', 'newscr': 'w'}
SKIP = set()  # (UWEDIT.C's main is renamed uw1_main by compat.h: the front end's start-up calls the parts it needs)
GLUE = ['core_glue.c', 'imath.c', 'core_world.c', 'core_dos.c']
FLAGS = ['--target=x86_64-w64-windows-gnu', '--sysroot=' + TOOLS.replace('\\', '/') + '/gcc/mingw64', '-x', 'c', '-std=gnu17', '-O2',
         '-w', '-fwrapv', '-fno-strict-aliasing', '-mno-ms-bitfields',  # Turbo C's bitfield layout (as exhumed's port)
         '-Dstr_copy=core_str_copy',  # MODEX.ASM's str_copy, renamed: the game's base.c has its own
         '-include', os.path.join(E, 'port', 'compat.h'), '-I', os.path.join(E, 'port'),
         '-I', os.path.join(E, 'port', 'include'), '-I', os.path.join(E, 'include'),
         '-I', os.path.join(OUT, 'inc'), '-I', os.path.join(X, 'port'), '-I', X]
if INSTR:
    FLAGS = FLAGS[:5] + ['-O1', '-finstrument-functions'] + FLAGS[6:]



def sources():
    out = []
    for d in DIRS:
        for f in sorted(os.listdir(os.path.join(X, d))):
            if f.endswith('.C') and d + '/' + f not in SKIP:
                out.append(d + '/' + f)
    return out + EXTRA


def clang():
    return os.path.join(LLVM, 'clang.exe')


# Host-ABI fixes, applied to a copy (build/core/src) and never to underworld-exhumed's files: places where the DOS
# code relied on cdecl's stack (a function called through a pointer of another type, its arguments still lining up
# on the stack) that a register-argument host does not reproduce. Each replaces text with what DOS did.
PATCHES = {
    # find_good_x_and_y clears only 9 of visited[]'s 18 bytes (memset(visited, 0, 9)) and indexes visited[9]: the rest
    # is the stack's old contents, zero as the proofs zero the emulator's stack (found by tools/proof_worldev.py)
    'event/WORLDEV.C': [
        ('    uint16 visited[9];', '    uint16 visited[10] = { 0 };  /* port: see tools/core.py */'),
    ],
    # the inventory panel's drawing (the slots, the paperdoll, an open bag): the front end draws its own panel, so
    # these return at once (they draw only; the rules round them, RearrangeInventory, OpenTheBag, are kept)
    'inv/INVPANEL.C': [
        ('void far load_inventory_pix(void)\n{\n', 'void far load_inventory_pix(void)\n{\n' + '    if (1) return;  /* port: the front end draws the panel (tools/core.py) */\n'),
        ('void far DisplayInvSpecial(void)\n{\n', 'void far DisplayInvSpecial(void)\n{\n' + '    if (1) return;  /* port: the front end draws the panel (tools/core.py) */\n'),
        ('void far DisplayInvObject(int slot)\n{\n', 'void far DisplayInvObject(int slot)\n{\n' + '    if (1) return;  /* port: the front end draws the panel (tools/core.py) */\n'),
        ('void far DisplayInventory(void)\n{\n', 'void far DisplayInventory(void)\n{\n' + '    if (1) return;  /* port: the front end draws the panel (tools/core.py) */\n'),
        ('void far displayInventoryArray(int from, int to)\n{\n', 'void far displayInventoryArray(int from, int to)\n{\n' + '    if (1) return;  /* port: the front end draws the panel (tools/core.py) */\n'),
    ],
    'inv/BAGS.C': [
        ('void far DisplayOpenBag(void)\n{\n', 'void far DisplayOpenBag(void)\n{\n' + '    if (1) return;  /* port: the front end draws the panel (tools/core.py) */\n'),
    ],
    # the 3D view's change handler 9 redraws the inventory panel: the front end draws its own (replaced, not a rule)
    'game/UWEDIT.C': [
        # the death screen and the title menu are the front end's (src/core_glue.c's core_death_fn)
        ('void far real_death(int how)\n{\n',
         'extern void (*core_death_fn)(int how);\nvoid far real_death(int how)\n{\n'
         '    if (core_death_fn) { core_death_fn(how); return; }  /* port: the front end\'s (tools/core.py) */\n'),
        ('      RedispInv, check_victory, check_physics,',
         '      0 /* port: RedispInv, the front end\'s (tools/core.py) */, check_victory, check_physics,'),
    ],
    # Turbo C's unsigned is 16 bits; these locals were left plain unsigned in the shared source, which on the host is
    # 32 bits (found by tools/proof_ai.py): in turn_real_fine, (int32 << 15) / dist must divide as a signed long (a
    # 16-bit unsigned converts to long), and crit_avoid_player's squared distance wraps at 16 bits
    'critter/AI.C': [
        ('    unsigned char done = 0;\n    register unsigned dist;',
         '    unsigned char done = 0;\n    register uint16 dist;  /* port: Turbo C\'s unsigned (tools/core.py) */'),
        ('    register int py;\n    register unsigned dist;',
         '    register int py;\n    register uint16 dist;  /* port: Turbo C\'s unsigned (tools/core.py) */'),
        ('    if (how * how > dist) {',
         '    if ((uint16)(how * how) > dist) {  /* port: a 16-bit int product against a 16-bit unsigned */'),
    ],
    # DOS has tiles[] right after oCollisions[8] (DS:26C6, 26F6), and obj_coll_check can fill a ninth entry, which
    # lands on tiles[0..2] (found by tools/proof_collide.py): the two in grouped sections, in that order
    'motion/COLLIDE.C': [
        ('struct Collision oCollisions[8];\nstatic int16 tiles[9];',
         '/* port (tools/core.py): DOS\'s layout, tiles[] straight after oCollisions[8] */\n'
         'struct Collision oCollisions[8] __attribute__((section(".data$coll_a"), aligned(16)));\n'
         'static int16 tiles[9] __attribute__((section(".data$coll_b"), aligned(2)));'),
    ],
    # a 16-bit int product and quotient in Turbo C (found by tools/proof_motion.py: movers of speed 1024 and more)
    'motion/MOTION.C': [
        ('        MP.step[MP.minor] = MP.step[MP.major] / 0x100 * MP.vel[MP.minor] / MP.vel[MP.major] << 8;',
         '        MP.step[MP.minor] = (int16)((int16)(MP.step[MP.major] / 0x100 * MP.vel[MP.minor]) / MP.vel[MP.major]) << 8;  /* port: 16-bit */'),
    ],
    # a fate-9 object that leaves no static copy: DOS reads the interrupt vector table through the null pointer
    'motion/OBJPHYS.C': [
        # res_to_terr[1 << terrain code] past its 18 bytes (codes 5 to 7, the shore bit): what DOS read there
        # (src/core_dos.c)
        ('#include "ui.h"\n', '#include "ui.h"\nunsigned char core_res_to_terr(unsigned i);  /* port: src/core_dos.c */\n'),
        ('SET_TERRAIN(obj, res_to_terr[pp->terrain]);', 'SET_TERRAIN(obj, core_res_to_terr(pp->terrain));  /* port */'),
        ('    if (fate == 9 && !(char)mts_doanim(st, XP, YP, who)) {',
         '    if (fate == 9 && !(char)mts_doanim(FARNULLTRAP(st), XP, YP, who)) {  /* port: a null st */'),
    ],
    # beeline and add_to_beeline_path hand hyp_move an uninitialised local as the distance it adds to (found by
    # tools/proof_ai.py): zero, as the proofs zero the emulator's stack before each call
    'critter/PATHFIND.C': [
        ('    unsigned char unused;\n', '    unsigned char unused = 0;  /* port: see tools/core.py */\n'),
        ('    unsigned char traversable, unused;\n', '    unsigned char traversable, unused = 0;  /* port: see tools/core.py */\n'),
    ],
    # an uninitialised local the original reads (its stack's old contents): zero, as the proofs zero the emulator's
    # stack before each call (tools/coreproof.py)
    'obj/LOOK.C': [
        ('void far BonesLook(struct Object far *obj, int print)\n{\n    struct Object tmp;',
         'void far BonesLook(struct Object far *obj, int print)\n{\n    struct Object tmp = { 0 };  /* port: see tools/core.py */'),
        # and its text: get_name leaves it unwritten for a creature with no name (owner 15, item 0x4F), which is
        # printed as it stands
        ('    struct Object tmp = { 0 };  /* port: see tools/core.py */\n    char text[40];',
         '    struct Object tmp = { 0 };  /* port: see tools/core.py */\n    char text[40] = { 0 };'),
    ],
    'conv/CONVERSE.C': [
        # read-only accessors for the front end, which draws the conversation (the menu's strings are statics);
        # appended after the last routine, nothing of the original changed
        ('    obj->qn.f.quality = getmem(stack[-1]) & 0x3f;\n    return 1;\n}\n',
         '    obj->qn.f.quality = getmem(stack[-1]) & 0x3f;\n    return 1;\n}\n\n'
         '/* port (tools/core.py): the front end\'s view of the menu, read only */\n'
         'int core_conv_menu(void) { return convo_mode; }\n'
         'int core_conv_nopts(void) { return convo_mode ? bablOptCount - 1 : 0; }\n'
         'char far *core_conv_opt(int i) { return babl_display[i + 1]; }\n'),
    ],
    'combat/SPELLS.C': [
        # area_spells calls each entry as fn(x, y, target, tile, src); DOS gave sp_true_sight (caster, target) the
        # words x, y as caster (unused) and the third argument as target
        ('SpellFn area_spells[4] = {\n    (SpellFn)sp_true_sight,',
         '/* port (tools/core.py): DOS passed area_spells\' third argument as sp_true_sight\'s target */\n'
         'static char far sp_true_sight_area(int x, int y, struct Object far *target, struct Tile far *tile,\n'
         '                                   unsigned char src) { return sp_true_sight(0, target); }\n'
         'SpellFn area_spells[4] = {\n    sp_true_sight_area,'),
    ],
}


# Turbo C's unsigned (and unsigned int) is 16 bits; the shared sources still say plain unsigned in places where the
# width matters (tools/proof_ai.py found two in AI.C), so every source and header the core compiles is copied with
# them made uint16 (unsigned char, short and long are left alone). The copies are build/core/src and build/core/inc.
TURBO_UNSIGNED = re.compile(r'\bunsigned(\s+int\b)?(?!\s*\b(char|short|long)\b)')


def turbo(text):
    return TURBO_UNSIGNED.sub('uint16', text)


def write_if_changed(path, text):
    if not os.path.exists(path) or open(path, encoding='latin-1').read() != text:
        open(path, 'w', encoding='latin-1', newline='').write(text)


def headers():
    inc = os.path.join(OUT, 'inc')
    os.makedirs(inc, exist_ok=True)
    for f in os.listdir(os.path.join(X, 'include')):
        if f.lower().endswith('.h'):
            write_if_changed(os.path.join(inc, f), turbo(open(os.path.join(X, 'include', f), encoding='latin-1').read()))
    return inc


def compile_one(rel):
    src = os.path.join(X, rel)
    obj = os.path.join(OUT, rel.replace('/', '_')[:-2] + '.o')
    text = open(src, encoding='latin-1').read()
    for old, new in PATCHES.get(rel, []):
        if old not in text:
            return obj, 'patch for %s no longer applies: %r' % (rel, old[:60])
        text = text.replace(old, new)
    os.makedirs(os.path.join(OUT, 'src'), exist_ok=True)
    patched = os.path.join(OUT, 'src', rel.replace('/', '_'))
    write_if_changed(patched, turbo(text))
    src = patched
    if os.path.exists(obj) and os.path.getmtime(obj) > os.path.getmtime(src) \
            and os.path.getmtime(obj) > os.path.getmtime(os.path.abspath(__file__)):
        return obj, ''
    r = subprocess.run([clang()] + FLAGS + ['-c', src, '-o', obj], capture_output=True, text=True,
                       env=dict(os.environ, TMP=TOOLS + r'\llvm\tmp', TEMP=TOOLS + r'\llvm\tmp'))
    return obj, r.stderr if r.returncode else ''


def decls(rel):
    """The names the source declares at file scope: name -> 'f' (function) or 'v' (variable)."""
    r = subprocess.run([clang()] + FLAGS + ['-fsyntax-only', '-Xclang', '-ast-dump', os.path.join(X, rel)],
                       capture_output=True, text=True, errors='replace',
                       env=dict(os.environ, TMP=TOOLS + r'\llvm\tmp', TEMP=TOOLS + r'\llvm\tmp'))
    out = {}
    for line in r.stdout.split('\n'):
        m = re.match(r"^[|`]-(FunctionDecl|VarDecl) .*? (?:used |referenced )?(\w+) '", line)
        if m:
            out.setdefault(m.group(2), 'f' if m.group(1) == 'FunctionDecl' else 'v')
    return out


def nm(args):
    r = subprocess.run([os.path.join(LLVM, 'llvm-nm.exe')] + args, capture_output=True, text=True)
    return r.stdout


def main():
    os.makedirs(OUT, exist_ok=True)
    headers()  # Turbo C's unsigned in the headers too
    os.makedirs(GEN, exist_ok=True)
    srcs = sources()
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        res = list(ex.map(compile_one, srcs))
    bad = [(o, e) for o, e in res if e]
    for o, e in bad:
        print(o, e[:2000])
    if bad:
        sys.exit(1)
    objs = [o for o, _ in res]
    # the glue (src/core_glue.c, src/imath.c): compiled the same way, into the library with the core
    gobjs = []
    for g in GLUE:
        gobj = os.path.join(OUT, 'glue_' + os.path.basename(g)[:-2] + '.o')
        r = subprocess.run([clang()] + FLAGS + ['-c', os.path.join(ROOT, 'src', g), '-o', gobj], capture_output=True,
                           text=True, env=dict(os.environ, TMP=TOOLS + r'\llvm\tmp', TEMP=TOOLS + r'\llvm\tmp'))
        if r.returncode:
            print(r.stderr[:3000])
            sys.exit(1)
        gobjs.append(gobj)
    defined, dkind = set(), {}
    for line in nm(objs + gobjs).split('\n'):
        p = line.split()
        if len(p) == 3 and p[1] in 'TDBRCS':
            defined.add(p[2])
            dkind[p[2]] = 'f' if p[1] == 'T' else 'v'
    undef = set()
    for line in nm(['-u'] + objs + gobjs).split('\n'):
        p = line.split()
        if len(p) == 2 and p[0] == 'U':
            undef.add(p[1])
        elif len(p) == 1 and not p[0].endswith(':'):
            undef.add(p[0])
    # what the host's C library has
    libc = set('''memset memcpy memmove memcmp strlen strcpy strncpy strcat strcmp strncmp strchr strrchr strstr strtol
        atoi atol abs labs malloc calloc realloc free sprintf vsprintf printf fprintf fputs puts putchar getenv qsort
        toupper tolower isalpha isdigit isspace isupper islower ispunct isalnum isxdigit fclose fflush exit abort
        sscanf snprintf vsnprintf wcslen __acrt_iob_func __mingw_vsprintf __mingw_vfprintf __mingw_vsnprintf
        __mingw_vprintf ConvertThreadToFiber CreateFiber SwitchToFiber GetTickCount64 __chkstk_ms ___chkstk_ms __main fgets fgetc fputc fputs fread fwrite fopen fseek ftell rewind
        setvbuf getc putc ungetc feof ferror remove rename tmpfile time clock difftime mktime localtime gmtime strftime
        memchr strncat strspn strcspn strpbrk strtok strtoul strerror div ldiv rand srand bsearch sqrt sin cos atan2
        floor ceil fabs pow exp log setjmp longjmp _setjmp signal raise atexit system vfprintf'''.split())
    need = sorted(n for n in undef - defined if n not in libc and not n.startswith('__imp_'))
    kinds = {}
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        for d in ex.map(decls, srcs):
            for k, v in d.items():
                kinds.setdefault(k, v)
    lines = ['/* core_stubs.c: written by tools/core.py. The names the rules core (underworld-exhumed\'s C) uses and',
             '   neither it nor src/core_glue.c defines: each function records its name and returns 0, each variable',
             '   is a zeroed block. Each one is front-end work still to do (or, in a proof, an outside call both sides',
             '   stand in for alike). */',
             'void core_stub_log(const char *name);', 'void core_stub_logv(const char *name, const char *kinds, long long a0, long long a1, long long a2, long long a3);', '']
    for n in need:
        if kinds.get(n) == 'v':
            lines.append('__attribute__((weak, aligned(16))) unsigned char %s[0x10000];' % n)
        else:
            if n in SINKS:
                lines.append('__attribute__((weak)) long long %s(long long a0, long long a1, long long a2, long long a3) '
                             '{ core_stub_logv("%s", "%s", a0, a1, a2, a3); return 0; }' % (n, n, SINKS[n]))
            else:
                lines.append('__attribute__((weak)) long long %s() { core_stub_log("%s"); return 0; }' % (n, n))
    open(os.path.join(GEN, 'core_stubs.c'), 'w').write('\n'.join(lines) + '\n')
    # every name the core, the glue and the stubs define, with its address: what the proofs read and write by name
    allk = dict(dkind)
    for n in need:
        allk[n] = 'v' if kinds.get(n) == 'v' else 'f'
    names = sorted(n for n in allk if re.match(r'^[A-Za-z_]\w*$', n) and not n.startswith('.'))
    tab = ['/* core_tab.c: written by tools/core.py: every name of the rules core, for the proofs (tests/coretest.c) */',
           'struct core_sym { const char *name; void *addr; char fn; };']
    for n in names:
        tab.append('extern %s %s%s;' % ('void' if allk[n] == 'f' else 'unsigned char', n, '(void)' if allk[n] == 'f' else '[]'))
    tab.append('const struct core_sym core_syms[] = {')
    for n in names:
        tab.append('    { "%s", (void *)%s, %d },' % (n, n, allk[n] == 'f'))
    tab.append('};')
    tab.append('const int core_nsyms = %d;' % len(names))
    open(os.path.join(GEN, 'core_tab.c'), 'w').write('\n'.join(tab) + '\n')
    open(os.path.join(GEN, 'core_stubs.txt'), 'w').write('\n'.join('%s %s' % (allk[n], n) for n in need) + '\n')
    open(os.path.join(GEN, 'core_sinks.txt'), 'w').write(''.join('%s %s\n' % (n, SINKS[n]) for n in need if n in SINKS))
    lib = os.path.join(OUT, 'core.a')
    if os.path.exists(lib):
        os.remove(lib)
    subprocess.run([os.path.join(LLVM, 'llvm-ar.exe'), 'rcs', lib] + objs + gobjs, check=True)
    nf = sum(kinds.get(n) != 'v' for n in need)
    print('core: %d sources, %d stubs (%d functions, %d data)' % (len(srcs), len(need), nf, len(need) - nf))


if __name__ == '__main__':
    main()
