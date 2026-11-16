r"""vklayout.py: the structs the C side fills against the SPIR-V shaders' reading of them (the Vulkan renderer).

The D3D12 shaders read a StructuredBuffer's struct packed as C packs it (a Vertex 76 bytes); SPIR-V's own rule
(std430) would round a float3 up to 16 bytes (a Vertex 96 bytes), and every vertex after the first would be read from
the wrong place. tools\buildview.cmd compiles the SPIR-V with -fvk-use-dx-layout, which keeps D3D12's packing; this
check proves it at every build, so a struct changed later on one side only cannot slip through:

  Constants  (render.c, copied in render_vk_cpu.h)  against  cbuffer C      (cbuf.hlsli)
  GpuLight   (the same)                             against  GLight         (common.hlsli, t6)
  Vertex     (level.h)                              against  Vtx and SVtx   (rthit.hlsli t5, scene.hlsl t10: ray tracing)
  RParticle  (render.h)                             against  P              (particles.hlsl)

C's side: a small program built with clang that prints each member's offsetof and the struct's sizeof. The shaders'
side: DXC -spirv with the build's flags, spirv-dis, the OpMemberDecorate ... Offset and ArrayStride lines.

    python -I tools\vklayout.py      (run by tools\buildview.cmd, clang on the PATH; exit 1 and the differences when not alike)
"""
import os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DXC = r'O:\appsnotonc\dxc\v1.9.2609\bin\x64\dxc.exe'
DIS = r'O:\appsnotonc\vulkan-sdk\1.4.363.1\Bin\spirv-dis.exe'
SPV = '-spirv -fspv-target-env=vulkan1.3 -fvk-use-dx-layout -fvk-b-shift 0 0 -fvk-t-shift 16 0 -fvk-u-shift 48 0 -fvk-s-shift 64 0'.split()
WORK = os.path.join(ROOT, 'build', 'gen', 'vklayout')

PROBE = r'''
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render.h"
static unsigned long long rv_ticks(void) { return 0; }
#include "render_vk_cpu.h"
#define M(s, m) printf(" %zu", offsetof(s, m))
int main(void) {
    printf("Constants %zu:", sizeof(Constants));
    M(Constants, vp), M(Constants, vp_nojit), M(Constants, vp_prev), M(Constants, eye), M(Constants, look), M(Constants, size), M(Constants, post),
    M(Constants, lpos), M(Constants, lcol), M(Constants, linfo), M(Constants, outp), M(Constants, tint), M(Constants, rtlim), M(Constants, torchc),
    M(Constants, sprite), M(Constants, fxw), M(Constants, ripple), M(Constants, texres), M(Constants, hdr);
    printf("\nGpuLight %zu:", sizeof(GpuLight));
    M(GpuLight, pr), M(GpuLight, col), M(GpuLight, info), M(GpuLight, puff), M(GpuLight, pcol);
    printf("\nVertex %zu:", sizeof(Vertex));
    M(Vertex, pos), M(Vertex, prev), M(Vertex, uv), M(Vertex, rect), M(Vertex, nrm), M(Vertex, color);
    printf("\nRParticle %zu:", sizeof(RParticle));
    M(RParticle, pos), M(RParticle, life), M(RParticle, vel), M(RParticle, age), M(RParticle, col), M(RParticle, size), M(RParticle, floor_y),
    M(RParticle, kind), M(RParticle, drag), M(RParticle, grav);
    printf("\n");
    return 0;
}
'''

# (the shader, DXC's target and entry and defines, {the SPIR-V struct: (the C struct, its size checked against: 'stride' or None)})
SHADERS = [
    ('scene.hlsl', ['-T', 'ps_6_0', '-E', 'ps'], {'type_C': ('Constants', None), 'GLight': ('GpuLight', 'stride')}),
    ('scene.hlsl', ['-T', 'ps_6_5', '-E', 'ps', '-D', 'RT'], {'Vtx': ('Vertex', 'stride'), 'SVtx': ('Vertex', 'stride'), 'GLight': ('GpuLight', 'stride')}),
    ('particles.hlsl', ['-T', 'cs_6_0', '-E', 'cs_update', '-D', 'CS'], {'P': ('RParticle', 'stride')}),
]


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, errors='replace', **kw)
    if r.returncode:
        sys.exit('vklayout.py: %s failed:\n%s' % (os.path.basename(cmd[0]), (r.stdout + r.stderr)[-1500:]))
    return r.stdout


def main():
    os.makedirs(WORK, exist_ok=True)
    c = os.path.join(WORK, 'probe.c')
    exe = os.path.join(WORK, 'probe.exe')
    open(c, 'w').write(PROBE)
    run(['clang', '-std=c17', '-w', '-I', os.path.join(ROOT, 'src'), '-I', os.path.join(ROOT, 'dlss', 'include'), '-o', exe, c] +
        (os.environ.get('CLANG_FLAGS', '').split()))
    cs = {}
    for l in run([exe]).splitlines():
        name, size, offs = re.match(r'(\w+) (\d+):(.*)', l).groups()
        cs[name] = (int(size), [int(x) for x in offs.split()])
    bad = 0
    for k, (sh, args, structs) in enumerate(SHADERS):
        spv = os.path.join(WORK, 's%d.spv' % k)
        run([DXC] + SPV + args + ['-Fo', spv, os.path.join(ROOT, 'src', 'shaders', sh)])
        dis = run([DIS, spv])
        for sname, (cname, sized) in structs.items():
            offs = [int(o) for _, o in sorted((int(m.group(1)), m.group(2)) for m in re.finditer(r'OpMemberDecorate %%%s (\d+) Offset (\d+)' % re.escape(sname), dis))]
            m = re.search(r'OpDecorate %%_runtimearr_%s ArrayStride (\d+)' % re.escape(sname), dis)
            stride = int(m.group(1)) if m else None
            csize, coffs = cs[cname]
            ok = offs == coffs and (sized is None or stride == csize)
            print('vklayout: %-14s %-9s (%s %s): %s' % (sh, sname, cname, ' '.join(args[1:4:2]),
                                                       'the same (%d members%s)' % (len(offs), ', %d bytes each' % stride if stride else '') if ok else 'DIFFERENT'))
            if not ok:
                bad += 1
                print('    C:      size %d, offsets %s' % (csize, coffs))
                print('    SPIR-V: stride %s, offsets %s' % (stride, offs))
    if bad:
        print('vklayout: %d structs are not read as the C side writes them: the Vulkan renderer would read wrong data' % bad)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
