"""Second half of lights_original_patch.py (lights.c has CRLF line ends): the default lights.txt and
lights_load without the wall torches; the user's floating light becomes a comment."""
import sys

ROOT = sys.argv[1]


def edit_crlf(p, reps):
    p = ROOT + '/' + p
    s = open(p, encoding='utf-8', newline='').read()
    for a, b in reps:
        a, b = a.replace('\n', '\r\n'), b.replace('\n', '\r\n')
        assert s.count(a) == 1, (p, a[:80])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='').write(s)


edit_crlf('src/lights.c', [
    ('''static const char DEFAULT_LIGHTS[] =
    "# uo-new lights placed by hand (each level also has data\\\\lights\\\\levelN.txt, placed automatically).\\r\\n"
    "#   light LEVEL X Z HEIGHT  R G B  INTENSITY RANGE\\r\\n"''',
     '''static const char DEFAULT_LIGHTS[] =
    "# uo-new lights placed by hand. There are none: as in the original, the only light is the one the player\\r\\n"
    "# carries. Lines added here put lights in the world:\\r\\n"
    "#   light LEVEL X Z HEIGHT  R G B  INTENSITY RANGE\\r\\n"'''),
    ('''    "# Delete this file to get this one back.\\r\\n"
    "#\\r\\n"
    "# floating where the first hallway leaves the entrance room of level 1 (warm, like a lantern)\\r\\n"
    "light 1 32.0 3.0 14.0  1.0 0.78 0.52  1.6 7.0\\r\\n";''',
     '''    "#   walltorch LEVEL X Z HEIGHT  R G B  INTENSITY RANGE  FX FZ   (a burning torch on a wall, facing FX FZ)\\r\\n"
    "# Delete this file to get this one back.\\r\\n";'''),
    ('''    int n = 0;
    read_file(path, lv->number, out, &n, cap);
    snprintf(path, sizeof path, "%s\\\\lights\\\\level%d.txt", data_dir, lv->number);
    size_t mark = arena_mark();
    if (!auto_current(path) && !write_auto(path, lv)) return arena_reset(mark), -1;
    arena_reset(mark);
    read_file(path, lv->number, out, &n, cap);
    return n;''',
     '''    int n = 0;
    read_file(path, lv->number, out, &n, cap);
    return n;'''),
])

p = ROOT + '/data/lights.txt'
s = open(p, encoding='utf-8', newline='').read()
old = 'light 1 32.0 3.0 14.0  1.0 0.78 0.52  1.6 7.0'
nl = '\r\n' if '\r\n' in s else '\n'
if s.count('\n' + old) == 1:
    s = s.replace('\n' + old, '\n# (off since 2026-11-07: the original has no lights but the player\'s own;'
                  ' remove the "# " below to bring it back)' + nl + '# ' + old)
    open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
