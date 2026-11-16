"""portverify.py: Exhume's own replay checks (tools/replay.py) on the port tools/portwin32.py built: every
recorded session of underworld-exhumed/uw1/tests/replay replayed in our build and compared with the goldens
recorded in DOS (replay.py verify), or any other replay.py command.

Runs replay.py unchanged, with: the game data from O:\\uw (UW1_DATA; only read); temporary files under
O:\\appsnotonc\\llvm\\tmp; and the data trees replay.py makes from symbolic links (which Windows refuses without
the privilege) made of copies instead, so nothing the port does can reach O:\\uw.

    O:\\appsnotonc\\sentinel\\uv\\python\\cpython-3.13.17-windows-x86_64-none\\python.exe -I tools\\portverify.py verify all -j 4
"""
import os, sys, shutil, runpy

# The user, 2026-11-10: no test runs like this again; underworld-exhumed's code is taken as right. It once
# filled O: (400 GB). Refuses to run.
sys.exit("portverify.py: retired at the user's word (2026-11-10): exhumed's code is taken as right, no replay tests")

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXHUME_TOOLS = r'O:\appsnotonc\exhume\tools'
UW1 = os.path.join(HERE, 'underworld-exhumed', 'uw1')
os.environ.setdefault('UW1_DATA', r'O:\uw')
for k in ('TMP', 'TEMP', 'TMPDIR'):
    os.environ[k] = r'O:\appsnotonc\llvm\tmp'


def symlink(src, dst, target_is_directory=False, **kw):
    if os.path.isdir(src):
        shutil.copytree(src, dst)
    else:
        shutil.copy2(src, dst)


os.symlink = symlink
sys.path.insert(0, EXHUME_TOOLS)
os.chdir(UW1)
sys.argv = [os.path.join(EXHUME_TOOLS, 'replay.py'), '--config', 'exhume.toml'] + sys.argv[1:]
runpy.run_path(sys.argv[0], run_name='__main__')
