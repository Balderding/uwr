# uwr_export.py: the snapshot for the private GitHub repo "uwr" (the user, 2026-11-13). Only what we made ourselves:
#   - uo-new's committed files (git HEAD, so nobody's half-done edits go up), except:
#       game\ (the original game), external\ (Munt, llama.cpp and other third-party sources), dlss\ (NVIDIA's SDK),
#       tools\laigter-cli\thirdparty\ (CImg), re\dis_*.txt (disassembly listings of UW.EXE), exes/dlls/ROMs
#   - the files we added on underworld-exhumed's branch uw-new (HEAD; the ones not in upstream d8e0581: our
#     port\sys modules and tests) under underworld-exhumed\; exhumed's own files and our edits to them stay out
#   - never the HANDOFF*.md session notes (the user, 2026-11-14); the exported .gitignore gets HANDOFF*.md added
#   - our own texture maps from disk (git-ignored there): data\laigter\ (Laigter normal/height/occlusion maps) and
#     data\textures\{walls,floors}_{normal,mesh}\ + normal_maps.txt (never data\textures\walls|floors: the originals)
# Files over 95 MB are refused (GitHub's limit is 100 MB). OUT (default work\uwr) is ours alone: files no longer in
# the snapshot are removed from it, so a re-run gives the newest state; its .git is kept.
# Usage: python -I tools\uwr_export.py [OUT]
import io, os, sys, shutil, subprocess, tarfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXH = os.path.join(ROOT, 'underworld-exhumed')
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'work', 'uwr')
LIMIT = 95 * 1024 * 1024
UPSTREAM = 'd8e0581'
SKIP_DIRS = ('game/', 'external/', 'dlss/', 'tools/laigter-cli/thirdparty/', 'freefonts/')   # (freefonts: others' open fonts)
SKIP_EXT = ('.exe', '.dll', '.rom', '.lib', '.obj', '.o', '.gguf')
MAP_DIRS = ('data/laigter', 'data/textures/walls_normal', 'data/textures/floors_normal', 'data/textures/walls_mesh',
            'data/textures/floors_mesh')
MAP_FILES = ('data/textures/normal_maps.txt',)
THIRD_PARTY = ('port/win32/phonon.h', 'port/win32/phonon_version.h', 'src/stb_truetype.h')  # Valve's Steam Audio SDK headers

def git(repo, *args):
    return subprocess.run(['git', '-C', repo] + list(args), capture_output=True, check=True).stdout

def ours_uonew(p):
    if p.startswith(SKIP_DIRS): return False
    if p.lower().endswith(SKIP_EXT) and not p.lower().endswith('.obj'): return False
    if p.startswith('re/dis_'): return False
    if is_handoff(p): return False
    if p in THIRD_PARTY: return False
    return True

def is_handoff(p):
    n = p.rsplit('/', 1)[-1].lower()
    return n.startswith('handoff') and n.endswith('.md')

def from_head(repo, paths, prefix, files):
    """paths at git HEAD of repo into files {rel: bytes}"""
    for i in range(0, len(paths), 200):
        tar = tarfile.open(fileobj=io.BytesIO(git(repo, 'archive', '--format=tar', 'HEAD', '--', *paths[i:i + 200])))
        for m in tar.getmembers():
            if m.isfile(): files[prefix + m.name] = tar.extractfile(m).read()

def main():
    files = {}
    tracked = [p for p in git(ROOT, 'ls-tree', '-r', '--name-only', 'HEAD').decode('utf-8').splitlines() if ours_uonew(p)]
    from_head(ROOT, tracked, '', files)
    added = git(EXH, 'diff', '--diff-filter=A', '--name-only', UPSTREAM, 'HEAD').decode('utf-8').splitlines()
    added = [p for p in added if not is_handoff(p)]
    from_head(EXH, added, 'underworld-exhumed/', files)
    gi = files.get('.gitignore', b'')
    if b'HANDOFF*.md' not in gi:
        files['.gitignore'] = gi.rstrip(b'\r\n') + b'\nHANDOFF*.md\n'
    disk = []
    for d in MAP_DIRS:
        for dp, _, names in os.walk(os.path.join(ROOT, d)):
            for n in names:
                disk.append(os.path.relpath(os.path.join(dp, n), ROOT).replace('\\', '/'))
    disk += [f for f in MAP_FILES if os.path.isfile(os.path.join(ROOT, f))]
    big = [r for r, b in files.items() if len(b) > LIMIT] + [r for r in disk if os.path.getsize(os.path.join(ROOT, r)) > LIMIT]
    if big:
        print('uwr_export: REFUSED (over 95 MB):', ', '.join(big))
        return 1
    os.makedirs(OUT, exist_ok=True)
    for rel, b in files.items():
        dst = os.path.join(OUT, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, 'wb') as f: f.write(b)
    for rel in disk:
        dst = os.path.join(OUT, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(os.path.join(ROOT, rel), dst)
    keep = set(os.path.normcase(os.path.join(OUT, r)) for r in list(files) + disk)
    removed = 0
    for dp, dirs, names in os.walk(OUT):
        if '.git' in dirs: dirs.remove('.git')
        for n in names:
            f = os.path.join(dp, n)
            if os.path.normcase(f) not in keep: os.remove(f); removed += 1
    print('uwr_export: %d files from uo-new HEAD (%s), %d added on uw-new (%s), %d our maps from disk -> %s; %d stale removed'
          % (len(files) - len(added), git(ROOT, 'rev-parse', '--short', 'HEAD').decode().strip(), len(added),
             git(EXH, 'rev-parse', '--short', 'HEAD').decode().strip(), len(disk), OUT, removed))
    return 0

if __name__ == '__main__':
    sys.exit(main())
