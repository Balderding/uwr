r"""render_vk_cpu.py: src\render_vk_cpu.h, the Vulkan renderer's copy of render.c's CPU side, made from render.c.

render.c (the D3D12 renderer) is not to be changed while it is the default, so what both renderers must work out the
same way (the constants, a light, the effects' state, the flame, the black body, the matrices) is copied for
render_vk.c. The parts are found in render.c by MARKERS (the text a part starts with and the text it ends at), not by
line numbers, so edits elsewhere in render.c move nothing; an edit inside a part changes the copy.

    python -I tools\render_vk_cpu.py            writes src\render_vk_cpu.h
    python -I tools\render_vk_cpu.py --check    exit 1 (and says so) when the file is not what render.c gives now
                                                (tools\buildview.cmd runs this before it builds uwview_vk.dll)

A marker not found, or found twice, is an error: render.c changed there and the list below must be looked at again.
The one change made to the text: GetTickCount64() becomes rv_ticks() (render_vk.c's: the same count on Windows, the
monotonic clock elsewhere)."""
import io, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src', 'render.c')
OUT = os.path.join(ROOT, 'src', 'render_vk_cpu.h')

# (what the part's first line starts with, what its end line starts with, whether the end line is part of it)
PARTS = [
    ('static struct { int particles, decals, water, fog; } g_fx;', '#define PROBE_LAYERS', False),   # the effects' state, the water's rings, the held time
    ("/* the frame's constants:", '} Constants;', True),
    ('/* a light as the shaders take it', '} GpuLight;', True),
    ('/* ---- the flame (ours', 'void render_set_torch_motion(', True),                              # the flame, the black body's table
    ('static void mat_mul(', '/* world: x east, y up, z north.', False),
    ('/* world: x east, y up, z north.', 'void render_set_fullbright(bool on)', True),              # the view's rectangle, the shade scale, full bright
    ("/* A flame's colour from its temperature", 'void render_set_tint(', True),                    # the black body and its adaptation, the torch, the lens, the tint
    ('void render_set_debug(int what)', 'bool render_frame(', False),                               # the debug view, the lens, the view's rectangle, the matrices
]

HEAD = '''/* render_vk_cpu.h: render.c's CPU side, COPIED for the Vulkan renderer (render_vk.c includes it; nothing else does).
 * MADE BY tools\\render_vk_cpu.py FROM render.c: do not edit it here. render.c is not to be changed while D3D12 is the
 * default (the Vulkan plan's "beside, not instead"), so what both renderers must work out the same way is copied word
 * for word: the effects' state, the water's rings and the held time; the constants and a light as the shaders take
 * them; the flame (render_flame); the matrices; the view's rectangle, the shade scale, full bright; the black body and
 * its adaptation (render_blackbody); the torch's colour, the lens, the tint, the debug view.
 * The one change: GetTickCount64 is rv_ticks. tools\\buildview.cmd checks this file against render.c at every build
 * (render_vk_cpu.py --check): when render.c changes in these parts, run the tool and read the difference. */
static const RenderFxPasses *g_fxp;
'''


def make():
    lines = io.open(SRC, encoding='utf-8', newline='').read().replace('\r\n', '\n').split('\n')
    out = []
    for first, last, incl in PARTS:
        a = [i for i, l in enumerate(lines) if l.startswith(first)]
        if len(a) != 1:
            sys.exit('render_vk_cpu.py: render.c has %d lines starting "%s" (one wanted): look at PARTS again' % (len(a), first))
        b = [i for i, l in enumerate(lines) if i >= a[0] and l.startswith(last)]
        if not b:
            sys.exit('render_vk_cpu.py: render.c has no line starting "%s" after "%s": look at PARTS again' % (last, first))
        part = lines[a[0]:b[0] + (1 if incl else 0)]
        while part and not part[-1].strip():
            part.pop()
        out += part + ['']
    return HEAD + '\n'.join(out).replace('GetTickCount64()', 'rv_ticks()')


def main():
    text = make()
    if '--check' in sys.argv:
        have = io.open(OUT, encoding='utf-8', newline='').read().replace('\r\n', '\n') if os.path.exists(OUT) else ''
        if have != text:
            a, b = have.split('\n'), text.split('\n')
            k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
            print('render_vk_cpu.py: src\\render_vk_cpu.h is not what render.c gives now (first at its line %d):' % (k + 1))
            print('  the file:  %s' % (a[k][:150] if k < len(a) else '(ends)'))
            print('  render.c:  %s' % (b[k][:150] if k < len(b) else '(ends)'))
            print('  render.c changed in a part the Vulkan renderer copies: run tools\\render_vk_cpu.py, read `git diff src\\render_vk_cpu.h`,')
            print('  and add the change to the ledger in docs\\superpowers\\plans\\2026-11-15-vulkan-renderer.md')
            return 1
        print('render_vk_cpu.py: src\\render_vk_cpu.h is render.c\'s (%d lines)' % text.count('\n'))
        return 0
    io.open(OUT, 'w', encoding='utf-8', newline='').write(text)
    print('render_vk_cpu.py: wrote %s (%d lines)' % (OUT, text.count('\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
