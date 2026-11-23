# editor_update.py: bring Karl Clinckspoor's newest Underworld Editor into our copy on O:.
#
# Our copy is X:\uw-editor. His work is `origin/main` (github.com/KarlClinckspoor/
# UltimaUnderworldEditorAvalonia); ours is the local branch `uwr-fog` on top of it (the fog window, later the
# MCP server). This script fetches his newest, merges it into ours, builds, and runs our tests. It never
# pushes anything anywhere.
#
#   python -I tools\editor_update.py          fetch, merge, build, test
#   python -I tools\editor_update.py --check  only say what is new upstream
#
# On a merge conflict it stops, leaves the merge open and lists the files; fix them, `git add`, `git commit`,
# then run it again (it then builds and tests).
import os, subprocess, sys
TOOLS = os.environ.get('UWR_TOOLS', r'X:\tools')   # the compilers and SDKs (one variable: the move to X:, 2026-11-22)

ED = r'X:\uw-editor'
BRANCH = 'uwr-fog'
DOTNET = TOOLS + r'\dotnet10\dotnet.exe'
WORK = r'O:\uw\uo-new\work\editorfog'
TESTS = 'FullyQualifiedName~UwrFog|FullyQualifiedName~Mcp'   # ours; his own suite is his to run
WHO = ['-c', 'user.name=balderding', '-c', 'user.email=51464052+Balderding@users.noreply.github.com']

def git(*a, check=True):
    r = subprocess.run(['git', '-C', ED, *WHO, *a], capture_output=True, text=True)
    if check and r.returncode:
        sys.exit('git %s failed:\n%s%s' % (' '.join(a), r.stdout, r.stderr))
    return r

def main():
    if git('rev-parse', '--abbrev-ref', 'HEAD').stdout.strip() != BRANCH:
        sys.exit('the editor is not on branch %s; switch to it first' % BRANCH)
    if git('rev-parse', '-q', '--verify', 'MERGE_HEAD', check=False).returncode == 0:
        sys.exit('a merge is still open: fix the files, git add, git commit, then run this again')
    dirty = [l for l in git('status', '--porcelain').stdout.splitlines() if not l.startswith('??')]
    if dirty:
        sys.exit('uncommitted changes in the editor; commit them on %s first:\n  %s' % (BRANCH, '\n  '.join(dirty)))
    git('fetch', '--tags', 'origin')
    new = git('log', '--oneline', '%s..origin/main' % BRANCH).stdout.strip()
    print('new from Karl:\n' + (new or '  (nothing)'))
    if '--check' in sys.argv:
        return
    if new:
        git('branch', '-f', 'main', 'origin/main')          # keep `main` as his, untouched
        r = git('merge', '--no-edit', 'origin/main', check=False)
        if r.returncode:
            files = git('diff', '--name-only', '--diff-filter=U').stdout.split()
            sys.exit('merge conflict in:\n  ' + '\n  '.join(files) + '\nfix them, git add, git commit, run again')
        print('merged:', git('log', '--oneline', '-1').stdout.strip())
    env = dict(os.environ, TEMP=WORK + r'\tmp', TMP=WORK + r'\tmp', UW_GAMEDATA=WORK + r'\gamedata')
    os.makedirs(env['TEMP'], exist_ok=True)
    b = subprocess.run([DOTNET, 'build', '-c', 'Release', '-v', 'q', '-nologo'], cwd=ED, env=env,
                       capture_output=True, text=True)
    errs = [l for l in b.stdout.splitlines() if ' error ' in l]
    if b.returncode or errs:
        sys.exit('build FAILED:\n' + '\n'.join(sorted(set(errs))[:20]))
    print('build: ok')
    t = subprocess.run([DOTNET, 'test', '-c', 'Release', '--no-build', '-nologo', '--filter', TESTS], cwd=ED,
                       env=env, capture_output=True, text=True)
    lines = [l.strip() for l in t.stdout.splitlines() if l.strip().startswith(('Passed!', 'Failed!')) or ' FAIL' in l]
    print('our tests:\n  ' + '\n  '.join(lines or ['(none found)']))
    if t.returncode or any(l.startswith('Failed!') or ' FAIL' in l for l in lines):
        sys.exit('tests FAILED')
    print('done: our copy has Karl\'s newest with our work on top')

main()
