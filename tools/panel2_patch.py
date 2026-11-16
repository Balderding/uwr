"""One-off: the inventory panel as one combined grid with a bag bar (tools/panel2_new.c.txt replaces the panel
section of inventory.c, tools/panel2_draw.c.txt its inv_draw)."""
import sys

ROOT = sys.argv[1]
p = ROOT + '/src/inventory.c'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
new = open(ROOT + '/tools/panel2_new.c.txt', encoding='utf-8').read().replace('\r\n', '\n').replace('\n', nl)
draw = open(ROOT + '/tools/panel2_draw.c.txt', encoding='utf-8').read().replace('\r\n', '\n').replace('\n', nl)

a = s.index('/* --- the panel ---')
b = s.index('/* --- tooltips ---')
s = s[:a] + new + s[b:]
a = s.index('void inv_draw(void) {')
s = s[:a] + draw

def rep(x, y):
    global s
    x, y = x.replace('\n', nl), y.replace('\n', nl)
    assert s.count(x) == 1, x[:60]
    s = s.replace(x, y)

rep('        if (I.nopen < 8) I.open[I.nopen++] = p, I.scroll = 0;\n', '        I.hidden[p] = !I.hidden[p]; /* shown in the grid, or not */\n')
rep('    bool shift;\n', '    bool shift;\n    bool hidden[POOL];  /* a bag whose part of the grid is hidden */\n')
open(p, 'w', encoding='utf-8', newline='').write(s)

p = ROOT + '/src/inventory.h'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
x = 'void inv_draw(void);'
assert s.count(x) == 1
s = s.replace(x, x + nl + '/* is the point (overlay pixels) on the panel (else the world is under the mouse) */' + nl + 'bool inv_mouse_over(int x, int y);')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
