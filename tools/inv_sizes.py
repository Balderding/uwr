"""inv_sizes.py: the sizes of the globals both sides have (sizeof of each name as the core's headers declare it), written
to work/inv_sizes.json by a small program built with clang: the inventory proofs compare a global's bytes up to its
real size, not up to the next symbol. A name the headers do not declare keeps the gap to the next symbol (0x40 at most).
    python tools/inv_sizes.py
"""
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import ROOT, symbols  # noqa: E402

X = os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'src')
E = os.path.join(ROOT, 'external', 'exhume')
CLANG = r'O:\appsnotonc\llvm\23.1.4\bin\clang.exe'
OUT = os.path.join(ROOT, 'work', 'inv_sizes.json')


def sizes():
    tab = open(os.path.join(ROOT, 'build', 'gen', 'core_tab.c')).read()
    host = dict((m.group(1), m.group(2) == '1') for m in re.finditer(r'\{ "(\w+)", \(void \*\)\w+, (\d) \}', tab))
    data, _ = symbols()
    names = sorted(n for n in host if not host[n] and n in data)
    heads = [f for f in sorted(os.listdir(os.path.join(X, 'include'))) if f.endswith('.h')]
    inc = ''.join('#include "%s"\n' % h for h in heads)
    flags = ['--target=x86_64-w64-windows-gnu', '--sysroot=O:/appsnotonc/gcc/mingw64', '-x', 'c', '-std=gnu17', '-w',
             '-fwrapv', '-fno-strict-aliasing', '-mno-ms-bitfields', '-include', os.path.join(E, 'port', 'compat.h'),
             '-I', os.path.join(E, 'port'), '-I', os.path.join(E, 'port', 'include'), '-I', os.path.join(E, 'include'),
             '-I', os.path.join(ROOT, 'build', 'core', 'inc'), '-I', os.path.join(X, 'port'), '-I', X,
             '-I', os.path.join(X, 'include')]
    env = dict(os.environ)
    env['TMP'] = env['TEMP'] = env['TMPDIR'] = r'O:\appsnotonc\llvm\tmp'
    todo = list(names)
    res = {}
    for _ in range(40):
        body = inc + '#include <stdio.h>\n#undef main\nint main(void){\n' + ''.join('printf("%s %%zu\\n", sizeof(%s));\n' % (n, n) for n in todo) + 'return 0;}\n'
        src = os.path.join(ROOT, 'work', 'inv_sizes_probe.c')
        open(src, 'w').write(body)
        r = subprocess.run([CLANG] + flags + ['-o', os.path.join(ROOT, 'work', 'inv_sizes_probe.exe'), src], capture_output=True, text=True, env=env)
        if r.returncode == 0:
            out = subprocess.run([os.path.join(ROOT, 'work', 'inv_sizes_probe.exe')], capture_output=True, text=True).stdout
            for line in out.split('\n'):
                if line.strip():
                    n, s = line.split()
                    res[n] = int(s)
            break
        bad = set()
        for line in r.stderr.split('\n'):
            m = re.match(r'.*inv_sizes_probe\.c:(\d+):\d+: error', line)
            if m:
                k = int(m.group(1)) - (len(heads) + 4)
                if 0 <= k < len(todo):
                    bad.add(todo[k])
        if not bad:
            print(r.stderr[:3000])
            raise SystemExit('no progress')
        todo = [n for n in todo if n not in bad]
    json.dump(res, open(OUT, 'w'), indent=0, sort_keys=True)
    return res


if __name__ == '__main__':
    r = sizes()
    print(len(r), 'sizes')
