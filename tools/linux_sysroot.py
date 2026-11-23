r"""linux_sysroot.py: a Linux x86-64 sysroot for our clang's cross-builds (the Linux / SDL plan, step 4), made from
Debian 13 "trixie" amd64's own .deb packages, on this Windows machine with nothing installed.

  python -I tools\linux_sysroot.py [ROOT]     (default X:\tools\linux-sysroot)

1. The suite's InRelease from deb.debian.org (https); its OpenPGP signature checked with gpg (Git for Windows' own)
   against Debian's archive and release keys from ftp-master.debian.org/keys, when gpg is there (said either way).
2. main/binary-amd64/Packages.xz, checked against InRelease's SHA256; each package's .deb from its Filename, checked
   against the Packages index's SHA256, kept in ROOT\debs\ (downloaded once); ROOT\debs\MANIFEST.tsv lists them.
3. Each .deb unpacked by hand (an `ar` archive: data.tar.xz, or .zst with a Python zstandard module) into ROOT.
   Symbolic and hard links cannot be made on Windows without rights: each is resolved inside ROOT (an absolute
   target is ROOT + target) and its target copied in its place. /lib and /lib64 (usrmerge: base-files' symlinks to
   usr/lib, usr/lib64, which libc.so's linker script names) are made directory junctions. Names that differ only in
   case (the kernel headers have some) are logged to ROOT\debs\case-collisions.txt: the second one written wins.
Nothing outside ROOT is written."""
import hashlib, io, lzma, os, re, shutil, subprocess, sys, tarfile, urllib.request
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)

MIRROR = 'https://deb.debian.org/debian'
SUITE = 'trixie'
KEYS = ['https://ftp-master.debian.org/keys/archive-key-13.asc', 'https://ftp-master.debian.org/keys/release-13.asc']
# a C program (libc, its headers, the kernel's headers, GCC's startup files and libgcc), C++'s runtime for later, and the
# library upstream's port links: SDL3 (its backend). (Munt, mt32emu, which the port links when pkg-config finds it, is
# not in trixie's main: the Linux build has no MT-32 for now)
PACKAGES = ['libc6', 'libc6-dev', 'linux-libc-dev', 'libgcc-s1', 'libgcc-14-dev', 'libstdc++6', 'libstdc++-14-dev',
            'libsdl3-0', 'libsdl3-dev',
            'libsndio7.0']   # (SDL3 NEEDS libsndio.so.7, which a stock Mint 22 lacks: shipped beside it, makelinuxdist.py)

ROOT = sys.argv[1] if len(sys.argv) > 1 else TOOLS + r'\linux-sysroot'
DEBS = os.path.join(ROOT, 'debs')
os.makedirs(DEBS, exist_ok=True)


def fetch(url, path=None):
    """url's bytes (kept at path when given, and taken from there when it is already there)"""
    if path and os.path.exists(path):
        return open(path, 'rb').read()
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'linux_sysroot.py'}), timeout=120) as r:
        data = r.read()
    if path:
        open(path + '.part', 'wb').write(data)
        os.replace(path + '.part', path)
    return data


def sha256(b):
    return hashlib.sha256(b).hexdigest()


# ---- 1. the signed index
inrel = fetch('%s/dists/%s/InRelease' % (MIRROR, SUITE), os.path.join(DEBS, 'InRelease'))
gpg = shutil.which('gpg') or (r'C:\Program Files\Git\usr\bin\gpg.exe' if os.path.exists(r'C:\Program Files\Git\usr\bin\gpg.exe') else None)
if gpg:
    home = os.path.join(DEBS, 'gnupg')
    os.makedirs(home, exist_ok=True)
    for k in KEYS:
        kp = os.path.join(DEBS, os.path.basename(k))
        fetch(k, kp)
        subprocess.run([gpg, '--homedir', home, '--batch', '--quiet', '--import', kp], capture_output=True)
    r = subprocess.run([gpg, '--homedir', home, '--batch', '--status-fd', '1', '--verify', os.path.join(DEBS, 'InRelease')],
                       capture_output=True, text=True)
    good = re.findall(r'\[GNUPG:\] VALIDSIG (\w+)', r.stdout)
    # (InRelease carries signatures by more keys than the two imported, an older archive key's too: gpg exits non-zero
    # for the one it cannot check; a VALIDSIG by an imported key is the proof)
    if not good:
        sys.exit('linux_sysroot.py: InRelease\'s signature did not verify:\n' + r.stdout + r.stderr)
    print('linux_sysroot.py: InRelease signed by %s (verified with %s)' % (', '.join(good), gpg))
else:
    print('linux_sysroot.py: no gpg: InRelease\'s signature NOT checked (its hashes still check everything below)')
text = inrel.decode('utf-8', 'replace')
m = re.search(r'^ ([0-9a-f]{64})\s+\d+\s+main/binary-amd64/Packages\.xz$', text.split('SHA256:', 1)[1], re.M)
if not m:
    sys.exit('linux_sysroot.py: no main/binary-amd64/Packages.xz in InRelease')
pxz = fetch('%s/dists/%s/main/binary-amd64/Packages.xz' % (MIRROR, SUITE), os.path.join(DEBS, 'Packages.xz'))
if sha256(pxz) != m.group(1):
    sys.exit('linux_sysroot.py: Packages.xz does not match InRelease\'s SHA256')
index = {}
for stanza in lzma.decompress(pxz).decode('utf-8', 'replace').split('\n\n'):
    f = dict(re.findall(r'^([A-Za-z0-9-]+): (.*)$', stanza, re.M))
    if 'Package' in f:
        index[f['Package']] = f

# ---- 2. the packages
manifest = ['package\tversion\turl\tsha256']
debfiles = []
for p in PACKAGES:
    if p not in index:
        sys.exit('linux_sysroot.py: no package %s in %s main' % (p, SUITE))
    f = index[p]
    url = '%s/%s' % (MIRROR, f['Filename'])
    path = os.path.join(DEBS, os.path.basename(f['Filename']))
    data = fetch(url, path)
    if sha256(data) != f['SHA256']:
        os.replace(path, path + '.bad')
        sys.exit('linux_sysroot.py: %s does not match the index\'s SHA256 (kept as .bad)' % path)
    manifest.append('%s\t%s\t%s\t%s' % (p, f['Version'], url, f['SHA256']))
    debfiles.append((p, data))
    print('linux_sysroot.py: %s %s (%d bytes) checked' % (p, f['Version'], len(data)))
open(os.path.join(DEBS, 'MANIFEST.tsv'), 'w').write('\n'.join(manifest) + '\n')


# ---- 3. unpacked
def ar_members(data):
    if data[:8] != b'!<arch>\n':
        raise ValueError('not an ar archive')
    i = 8
    while i + 60 <= len(data):
        h = data[i:i + 60]
        name, size = h[:16].decode().strip().rstrip('/'), int(h[48:58].decode().strip())
        yield name, data[i + 60:i + 60 + size]
        i += 60 + size + (size & 1)


def data_tar(data):
    for name, body in ar_members(data):
        if name.startswith('data.tar'):
            if name.endswith('.xz'):
                return tarfile.open(fileobj=io.BytesIO(lzma.decompress(body)))
            if name.endswith('.zst'):
                try:
                    import zstandard
                except ImportError:
                    sys.exit('linux_sysroot.py: a .zst package and no Python zstandard module')
                return tarfile.open(fileobj=io.BytesIO(zstandard.ZstdDecompressor().decompressobj().decompress(body)))
            if name.endswith('.gz'):
                return tarfile.open(fileobj=io.BytesIO(body), mode='r:gz')
            return tarfile.open(fileobj=io.BytesIO(body))
    raise ValueError('no data.tar')


links, seen, collide, nfiles = [], {}, [], 0
for p, data in debfiles:
    t = data_tar(data)
    for mem in t.getmembers():
        rel = os.path.normpath(mem.name.lstrip('./')) if mem.name not in ('.', './') else ''
        if not rel or rel == '.':
            continue
        dest = os.path.join(ROOT, rel)
        low = rel.lower()
        if not mem.isdir():
            if low in seen and seen[low] != rel:
                collide.append('%s\t%s (%s)' % (seen[low], rel, p))
            seen[low] = rel
        if mem.isdir():
            os.makedirs(dest, exist_ok=True)
        elif mem.issym():
            links.append((rel, mem.linkname, False))
        elif mem.islnk():
            links.append((rel, mem.linkname, True))
        elif mem.isfile():
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with t.extractfile(mem) as src, open(dest, 'wb') as out:
                shutil.copyfileobj(src, out)
            nfiles += 1

# usrmerge: /lib and /lib64 are base-files' symlinks into usr (junctions here, made before the links are resolved)
import _winapi  # noqa: E402
for top in ('lib', 'lib64'):
    j, to = os.path.join(ROOT, top), os.path.join(ROOT, 'usr', top)
    if os.path.isdir(to) and not os.path.lexists(j):
        _winapi.CreateJunction(to, j)
        print('linux_sysroot.py: %s -> %s (a junction: usrmerge)' % (j, to))


def target_of(rel, link, hard):
    if hard or link.startswith('/'):
        return os.path.normpath(os.path.join(ROOT, link.lstrip('/')))
    return os.path.normpath(os.path.join(ROOT, os.path.dirname(rel), link))


left, copied = links, 0
for _ in range(8):   # (a link to a link: resolved once its target is there)
    later = []
    for rel, link, hard in left:
        dest, src = os.path.join(ROOT, rel), target_of(rel, link, hard)
        if os.path.isfile(src):
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(src, dest)
            copied += 1
        elif os.path.isdir(src):
            shutil.copytree(src, dest, dirs_exist_ok=True)
            copied += 1
        else:
            later.append((rel, link, hard))
    if len(later) == len(left):
        break
    left = later
else:
    left = later
with open(os.path.join(DEBS, 'case-collisions.txt'), 'w') as f:
    f.write('\n'.join(collide) + ('\n' if collide else ''))
with open(os.path.join(DEBS, 'dangling-links.txt'), 'w') as f:
    f.write(''.join('%s -> %s\n' % (r, l) for r, l, h in left))
size = 0
for d, ds, fs in os.walk(ROOT):   # (not through the junctions: they are usr's again)
    ds[:] = [x for x in ds if not os.path.isjunction(os.path.join(d, x)) and os.path.join(d, x) != DEBS]
    size += sum(os.path.getsize(os.path.join(d, x)) for x in fs)
print('linux_sysroot.py: %d files, %d links resolved as copies, %d dangling (debs\\dangling-links.txt), %d case '
      'collisions (debs\\case-collisions.txt); the tree %.1f MB at %s'
      % (nfiles, copied, len(left), len(collide), size / 1e6, ROOT))
