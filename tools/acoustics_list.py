# acoustics_list.py: which acoustic material every wall and floor texture of levels 1-9 gets (the acoustics plan's
# Task 1, docs\superpowers\plans\2026-11-14-acoustics-ours.md): the textures in use from data\levels\levelN.txt, the
# table from data\acoustics.txt (port\win32\uwacmat.h has its format; this reads it the same way: the last matching
# line wins), the game's own name for each texture from data\strings\block0010.txt.
#   python -I tools\acoustics_list.py             data\acoustics.txt (the table in the code when that file is not there)
#   python -I tools\acoustics_list.py --default   the table in the code (port\win32\uwacmat.c DEFAULT_TEXT), whatever
#                                                 data\acoustics.txt says: what tools\fxcheck.py materials checks
# Prints one line a texture in use, then the counts. Exit code 1 when a texture resolves to no preset, a line of the
# table is bad, or a numbered line names a picture that does not exist; else 0.
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, 'data')
KINDS = ('floor', 'ceiling', 'wall', 'door')
PICTURES = {'wall': ('textures', 'walls'), 'floor': ('textures', 'floors'), 'ceiling': ('textures', 'floors'), 'door': ('sprites', 'doors')}

def default_text():
    """DEFAULT_TEXT of uwacmat.c: its string literals joined, the C escapes undone"""
    src = open(os.path.join(ROOT, 'port', 'win32', 'uwacmat.c'), encoding='utf-8').read()
    body = src[src.index('DEFAULT_TEXT[] ='):]
    body = body[:body.index(';\n')]
    out = ''
    for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', body):
        out += lit.replace('\\n', '\n').replace('\\"', '"').replace('\\\\', '\\')
    return out

def table_of(text):
    """(presets by name, {kind: {number or '*': (preset, line number)}} with the last line winning, errors)"""
    presets, table, errors = {}, {k: {} for k in KINDS}, []
    for no, line in enumerate(text.splitlines(), 1):
        w = line.split('#', 1)[0].split()
        if not w: continue
        if w[0] == 'preset':
            m = re.fullmatch(r'preset (\S+) absorb (\S+) (\S+) (\S+) transmit ([0-9.]+)(?: ([0-9.]+) ([0-9.]+))?(?: step (-?\d+))?', ' '.join(w))
            try: vals = [float(x) for x in m.groups()[1:7] if x is not None] if m else None
            except ValueError: vals = None
            if not vals or any(v < 0 or v > 1 for v in vals): errors.append('line %d: a bad preset' % no); continue
            presets[m.group(1)] = vals
        elif w[0] in KINDS and len(w) == 3:
            if w[2] not in presets: errors.append('line %d: preset %s is not defined above' % (no, w[2])); continue
            if w[1] == '*':
                table[w[0]] = {'*': (w[2], no)}
                continue
            try: n = int(w[1][2:], 16) if w[1][:2].lower() == '0x' else int(w[1], 10)
            except ValueError: errors.append('line %d: %s is not a number' % (no, w[1])); continue
            if not 0 <= n < 512: errors.append('line %d: %d is out of range' % (no, n)); continue
            table[w[0]][n] = (w[2], no)
        else:
            errors.append('line %d: not a preset or a texture line' % no)
    return presets, table, errors

def main():
    use_default = '--default' in sys.argv[1:]
    path = os.path.join(DATA, 'acoustics.txt')
    if use_default or not os.path.isfile(path):
        text, source = default_text(), 'the table in port\\win32\\uwacmat.c' + ('' if use_default else ' (no data\\acoustics.txt yet)')
    else:
        text, source = open(path, encoding='utf-8', errors='replace').read(), 'data\\acoustics.txt'
    presets, table, errors = table_of(text)
    names = {}
    sp = os.path.join(DATA, 'strings', 'block0010.txt')
    if os.path.isfile(sp):
        for l in open(sp, encoding='latin-1'):
            m = re.match(r'(\d+): (.*)', l)
            if m: names[int(m.group(1))] = m.group(2).replace('\\n', '').strip()
    used = {'wall': {}, 'floor': {}, 'ceiling': {}}        # number -> the levels using it
    for lv in range(1, 10):
        for l in open(os.path.join(DATA, 'levels', 'level%d.txt' % lv)):
            w = l.split()
            if w and w[0] in ('walls', 'floors'):
                for n in map(int, w[1:]): used[w[0][:-1]].setdefault(n, []).append(lv)
                # the level's one ceiling texture: its floor slot 9 (the original's GRIDDB.C:606)
                if w[0] == 'floors' and len(w) > 10: used['ceiling'].setdefault(int(w[10]), []).append(lv)
    print('acoustic materials from %s' % source)
    star = unresolved = 0
    counts = {}
    for kind in ('wall', 'floor', 'ceiling'):
        for n in sorted(used[kind]):
            hit = table[kind].get(n) or table[kind].get('*')
            # a ceiling without a line of its own is what its picture is as a floor (uwacmat_for)
            floors_line = kind == 'ceiling' and n not in table['ceiling'] and n in table['floor']
            if floors_line: hit = table['floor'][n]
            # UW1's names: walls from 0 up, floors from 510 down (block 10)
            name = names.get(n if kind == 'wall' else 510 - n, '')
            if not hit: unresolved += 1; print('%-5s %03d  (no preset)   %s' % (kind, n, name)); continue
            own = n in table[kind] or floors_line
            if not own: star += 1
            counts[hit[0]] = counts.get(hit[0], 0) + 1
            print('%-7s %03d  %-8s%s %-44s levels %s' % (kind, n, hit[0], ' ' if own else '*', name, ' '.join(map(str, used[kind][n]))))
    # a numbered line must name a picture that exists (a mistyped number would silently do nothing)
    for kind in KINDS:
        for n in table[kind]:
            if n == '*' or (kind == 'door' and n >= 0x140): continue
            if not os.path.isfile(os.path.join(DATA, *PICTURES[kind], '%03d.png' % n)):
                errors.append('line %d: %s %d names no picture in data\\%s' % (table[kind][n][1], kind, n, '\\'.join(PICTURES[kind])))
    for e in errors: print('acoustics_list: %s' % e)
    print('acoustics_list: %d wall, %d floor and %d ceiling textures in use on levels 1-9; by preset: %s; %d on the "*" of their kind; %d unresolved' % (
        len(used['wall']), len(used['floor']), len(used['ceiling']), ', '.join('%s %d' % kv for kv in sorted(counts.items())), star, unresolved))
    return 1 if unresolved or errors else 0

if __name__ == '__main__':
    sys.exit(main())
