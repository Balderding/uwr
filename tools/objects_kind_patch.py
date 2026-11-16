"""One-off: COMOBJ byte 3 bits 6-7 is a kind (0 thing, 1 weapon/armour, 3 container), not "stack"; objects.txt
gets a version line (v2) and the engine remakes its own older copy."""
import sys

ROOT = sys.argv[1]


def patch(p, reps):
    s = open(ROOT + '/' + p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in s else '\n'
    for a, b in reps:
        a, b = a.replace('\n', nl), b.replace('\n', nl)
        assert s.count(a) == 1, (p, a[:70], s.count(a))
        s = s.replace(a, b)
    open(ROOT + '/' + p, 'w', encoding='utf-8', newline='').write(s)


patch('src/extract.c', [
    ('"# uo-new objects: what the original knows', '"# uo-new objects v2: what the original knows'),
    ('"#   obj N  pick (can be picked up) stack (0 no, else how it stacks) mass',
     '"#   obj N  pick (can be picked up) kind (0 a thing: those with a plural stack, 1 a weapon or armour,\\r\\n"\n'
     '                "#          3 a container) mass'),
    ('"obj %3d  pick %d stack %d mass', '"obj %3d  pick %d kind %d mass'),
    ('    snprintf(path, sizeof path, "%s\\\\objects\\\\objects.txt", data_dir);\n    if (!file_exists(path)) {',
     '    snprintf(path, sizeof path, "%s\\\\objects\\\\objects.txt", data_dir);\n    if (!file_exists(path) || !file_starts_with(path, "# uo-new objects v2")) {'),
    ('static int extract_more(const char *data_dir, const char *game_dir) {',
     '/* is the file the engine\'s own of this version (its first line) */\n'
     'static bool file_starts_with(const char *path, const char *head) {\n'
     '    FILE *f = fopen(path, "rb");\n'
     '    if (!f) return false;\n'
     '    char line[64] = {0};\n'
     '    size_t n = fread(line, 1, sizeof line - 1, f);\n'
     '    fclose(f);\n'
     '    line[n] = 0;\n'
     '    return !strncmp(line, head, strlen(head));\n'
     '}\n\n'
     'static int extract_more(const char *data_dir, const char *game_dir) {'),
])
patch('src/objdata.c', [
    ('if (!strcmp(key, "stack")) o->stack = (u8)v;', 'if (!strcmp(key, "kind")) o->kind = (u8)v;'),
    ('int obj_mass(const Object *o) {',
     'bool obj_stacks(int item) { return obj_info(item)->kind == 0 && strchr(str_get(4, item & 511), \'&\') != NULL; }\n\n'
     'int obj_mass(const Object *o) {'),
])
patch('src/objdata.h', [
    ('u8 stack;        /* 0 single, 1 stacks, 3 ... (as COMOBJ has it) */',
     'u8 kind;         /* 0 a thing (stacks when its name has a plural), 1 a weapon or armour, 3 a container */'),
    ('int obj_mass(const Object *o); /* tenths of a stone, with the quantity (not the contents) */',
     'int obj_mass(const Object *o); /* tenths of a stone, with the quantity (not the contents) */\n'
     'bool obj_stacks(int item);     /* can several lie as one (a count): a thing whose name has a plural */'),
])
patch('src/inventory.c', [
    ('if (a->item == b->item && obj_info(a->item)->stack && a->quality == b->quality) {',
     'if (a->item == b->item && obj_stacks(a->item) && a->quality == b->quality) {'),
])
print('ok')
