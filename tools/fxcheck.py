# fxcheck.py: the effects (docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md), one check a task. Off-screen runs
# of the test exe (underworld-exhumed\uw1\build\port\uw1port.exe) with a fresh copy of the test home work\in2-4 under
# work\fx\, never a visible window, never a sound on the machine (--no-audio; the audio module's own check renders
# through OpenAL Soft's loopback device into memory).
#   audio       Task 1: tests\uwaudio_test.c over the loopback device (east / west / far / muted / none); a game run
#               with UWPORT_FX_LOG=1 gives "fx: sound 7 at X,Y" for a blow on a wall and "fx: sound 11 at X,Y" for a
#               door used; a run with fx_audio on and no soft_oal.dll says "fx: audio off (no soft_oal.dll)"
#   particles   Task 3: level 1's campfire at 8,42 (object 767) seen from 6,42
#               (the save faces it), compared with fx_particles on and off (UWVIEW_COMPARE_FX: the camera, the flames'
#               time and the frame count held): the pictures differ, and only round and above the flame (a region
#               test on the two pictures); a goto to level 2 clears them ("level 2: cleared, 0 live"); with GDI
#               (UWPORT_GDI=1) the run is clean; UWPORT_TIMING: the renderer's frame with them on within 0.4 ms of off
#   decals      Task 4: a test mark (UWVIEW_FX_TEST=decal: blood on the open floor ahead once the eye settles) compared
#               on / off: the pictures differ only round the mark; a fight (goblins of level 1 angered, fight mode,
#               blows): every blow landed (the game's sound 4 at the creature) leaves blood under that creature
#               (within half a tile, its kind named); a jump of the eye (a goto: as a load) and a level change clear
#               them. A save holds none (presentation only: nothing of them reaches the game)
#   water       Task 5: level 1's water south-east of 13,24 (the way the save faces), fx_water compared on / off: the
#               pictures differ, and only on the water (the water's own mask: UWVIEW_DEBUG=water); with ray tracing off
#               they still differ (the refraction is not the ray tracer's); a splash's ring (UWVIEW_FX_TEST=splash)
#               compared shown / hidden: it differs on the water round it
#   materials   the acoustics plan's Task 1 (docs\superpowers\plans\2026-11-14-acoustics-ours.md): tests\uwacmat_test.c
#               (port\win32\uwacmat.c alone: the table read, the last line winning, doors by item and picture, bad
#               files refused, the shipped table written when missing); tools\acoustics_list.py --default resolves
#               every wall and floor texture levels 1-9 use. No game run, no sound.
#   grid        the acoustics plan's Task 2: a hidden run on level 1 then level 2 with UWVIEW_ACOUSTIC_DUMP=FILE (the
#               renderer writes uwv_acoustic_grid / _doors / _epoch once a level built): 4,096 tiles, the open ones the
#               level file's, every texture a file, the doors the level file's door objects with their pictures, the
#               epoch moved by the level change; and the game's own line for each ("fx: acoustics: grid epoch ...")
# Usage: python -I tools\fxcheck.py [materials|grid|audio|particles|decals|water|fog|acoustics ...]. Exit code: the failures.
import os, sys, shutil, subprocess, struct, zlib, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, 'underworld-exhumed', 'uw1', 'build', 'port', 'uw1port.exe')
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'fx')
DATA = os.path.join(ROOT, 'dist', 'UW1', 'game')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"
OPENAL_BIN = r'O:\appsnotonc\openal-soft\1.25.2\extracted\openal-soft-1.25.2-bin\bin\Win64'
PHONON_BIN = r'O:\appsnotonc\steam-audio\4.8.1\extracted\steamaudio\lib\windows-x64'   # Steam Audio's phonon.dll (Acoustics High)
LLVM = r'O:\appsnotonc\llvm\23.1.4\bin'
TARGET = ['--target=x86_64-w64-windows-gnu', '--sysroot=O:/appsnotonc/gcc/mingw64', '-fuse-ld=lld']

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''))
    if not ok: fails += 1

last_rc = 0
def run(name, script, exit_ms, env=None, shot_ms=None, audio=False, cfg='', args=()):
    """an off-screen run of the test exe; returns (stderr lines, the window shot's path or None). cfg: lines added to
    the home's uw1port.cfg; args: more of the exe's options (--audio-wav FILE: the sound cards' mix, no device)"""
    os.makedirs(WORK, exist_ok=True)
    home = os.path.join(WORK, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f: f.write('settings-at-start=0\n' + cfg)
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', UWPORT_FOCUSTEST='1000,1500', **(env or {}))
    cmd = [EXE, '--data', DATA, '--home', home, '--input-script', sp, '--exit-after', str(exit_ms)] + list(args)
    if not audio: cmd.insert(5, '--no-audio')
    wshot = None
    if shot_ms is not None:
        wshot = os.path.join(WORK, name + '-w.png')
        if os.path.exists(wshot): os.remove(wshot)
        cmd += ['--screenshot-after', str(shot_ms), '--screenshot', os.path.join(WORK, name + '.png'), '--window-shot', wshot]
    r = subprocess.run(cmd, env=e, capture_output=True, text=True, errors='replace')
    global last_rc
    last_rc = r.returncode
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    open(os.path.join(WORK, name + '.out'), 'w').write(r.stdout)
    return r.stderr.splitlines() + r.stdout.splitlines(), wshot

def clang(out, srcs, libs=()):
    env = dict(os.environ, PATH=LLVM + os.pathsep + os.environ.get('PATH', ''), TMP=r'O:\appsnotonc\llvm\tmp', TEMP=r'O:\appsnotonc\llvm\tmp')
    cmd = [os.path.join(LLVM, 'clang.exe')] + TARGET + ['-std=c17', '-O2', '-Wall', '-static', '-o', out] + list(srcs) + list(libs)
    r = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if r.returncode: print(r.stdout + r.stderr)
    return r.returncode == 0

# ---- Task 1: audio ----
def test_audio():
    w = os.path.join(WORK, 'audio')
    os.makedirs(os.path.join(w, 'sfx'), exist_ok=True)
    exe = os.path.join(w, 'uwaudio_test.exe')
    ok = clang(exe, [os.path.join(ROOT, 'tests', 'uwaudio_test.c')] + [os.path.join(ROOT, 'port', 'win32', f) for f in ('uwaudio.c', 'uwacoustic.c', 'uwacmat.c', 'uwsteam.c', 'rlog.c')])
    check('audio: tests\\uwaudio_test.c builds', ok)
    if ok:
        # (DATA's parent, dist\UW1, holds mt32emu.dll and roms\: the Roland cases run too)
        r = subprocess.run([exe, OPENAL_BIN, w, DATA], capture_output=True, text=True,
                           env=dict(os.environ, UWAUDIO_REF=os.path.join(ROOT, 'tests', 'data', 'uwaudio_east_ref.raw'), UWPORT_FX_PHONON=PHONON_BIN,
                                    UWPORT_FX_MAKEUP='1'))   # (its levels and reference: taken before the makeup)
        print(r.stdout.rstrip())
        check("audio: the loopback cases (east, off, west, far, muted, none, hrtf on, hrtf front/back, hrtf off), the acoustics' (on, tail, occluded tail, dry, occluded, muffled, door, here, steal, round corner, step wav, jump, tune), Acoustics High's (high on, high tail, high dry, high occluded, high drain) and the Roland ones (rendered, east, water, stop, velocity, step stone, step wood, off)",
              r.returncode == 0 and r.stdout.count('PASS') == 35 and ' FAIL' not in r.stdout, r.stdout + r.stderr)
    # the water sound (effect 0, 25 s long) stops when the player leaves the water (PLAYMOVE.C make_noise's
    # kill_effect: "fx: stop 0 here"), and our copy of it with the game's (2026-11-13: it played on): into the water
    # at 13,23 of level 1, out onto 12,24 five seconds later; the loopback quiet within half a second of the stop
    # (under a fifth of the second before it)
    wav = os.path.join(WORK, 'audio_water-al.wav')
    if os.path.exists(wav): os.remove(wav)
    lines, _ = run('audio_water', MENU + "15000 cheat goto 1 13 23\n20000 cheat goto 1 12 24\n", 24000,
                   dict(UWPORT_FX_LOG='1', UWPORT_FX_AUDIO='1', UWPORT_FX_BIN=OPENAL_BIN, UWPORT_FX_LOOPBACK=wav), audio=True,
                   args=['--audio-wav', os.path.join(WORK, 'audio_water-mix.wav')])
    fxl = [l for l in lines if l.startswith('fx: sound 0') or l.startswith('fx: stop 0') or l.startswith('fx: al')]
    print('\n'.join('  ' + l for l in fxl[:8]))
    i_on = next((i for i, l in enumerate(fxl) if l.startswith('fx: sound 0 here')), -1)
    i_off = next((i for i, l in enumerate(fxl) if l.startswith('fx: stop 0 here')), -1)
    check('audio: the water sound starts ("fx: sound 0 here") and the game stops it ("fx: stop 0 here")', 0 <= i_on < i_off, '\n'.join(fxl[:8]))
    m = [re.match(r'fx: al stop 0 at frame (\d+)', l) for l in lines]
    m = [x for x in m if x]
    if m and os.path.exists(wav):
        f = int(m[-1].group(1))
        n0, l0, r0 = wav_rms(wav, max(0, f - 48000), 48000)
        n1, l1, r1 = wav_rms(wav, f + 24000, 48000)
        why = 'the second before the stop: left %.0f right %.0f; the second from half a second after: left %.0f right %.0f' % (l0, r0, l1, r1)
        print('  ' + why)
        check('audio: our copy of the water stops with the game\'s', n0 > 0 and n1 > 0 and l0 + r0 > 20 and l1 + r1 < (l0 + r0) / 5, why)
    else:
        check('audio: our copy of the water stops with the game\'s ("fx: al stop 0 at frame F" and the loopback WAV)', False, '\n'.join(lines[-5:]))
    # the event stream: a blow on the wall (play_effect 7) and a door used (play_effect 0xB)
    # (level 1's door 1012 at 22, 5 is east of the open square 21, 5, which the save's facing looks along: E and the
    # right button use it; then at 36, 7 the wall east of the player takes the blows of fight mode)
    script = (MENU + "15000 cheat goto 1 21 5\n17000 key e\n18000 move 160 90\n18500 mdown 160 90 right\n18700 mup 160 90 right\n"
              "20000 cheat goto 1 36 7\n22000 key c\n22500 move 160 90\n"
              "23000 mdown 160 90 right\n23900 mup 160 90 right\n25000 mdown 160 90 right\n25900 mup 160 90 right\n")
    lines, _ = run('audio_events', script, 30000, dict(UWPORT_FX_LOG='1'))
    snd = [l for l in lines if l.startswith('fx: sound')]
    print('\n'.join('  ' + l for l in snd[:12]))
    check('audio: "fx: sound 11 at X,Y" for the door', any(re.match(r'fx: sound 11 at \d+,\d+', l) for l in snd), '\n'.join(snd[:20]))
    check('audio: "fx: sound 7 at X,Y" for the blow on a wall', any(re.match(r'fx: sound 7 at \d+,\d+', l) for l in snd), '\n'.join(snd[:20]))
    # fx_audio on, no soft_oal.dll beside the exe (build\port\bin has none): off, said once, the game runs on
    lines, _ = run('audio_nodll', MENU + "15000 cheat goto 1 36 7\n", 20000, dict(UWPORT_FX_AUDIO='1', UWPORT_FX_BIN=os.path.join(w, 'none')))
    off = [l for l in lines if l.startswith('fx: audio')]
    check('audio: fx_audio on without the DLL: "fx: audio off (no soft_oal.dll)"', off == ['fx: audio off (no soft_oal.dll)'], '\n'.join(off))
    # fx_audio on, the DLL there, a hidden run: off all the same (nothing is ever heard from a check)
    lines, _ = run('audio_hidden', MENU, 16000, dict(UWPORT_FX_AUDIO='1', UWPORT_FX_BIN=OPENAL_BIN))
    off = [l for l in lines if l.startswith('fx: audio')]
    check('audio: fx_audio on in a hidden run: "fx: audio off (a hidden run)"', off == ['fx: audio off (a hidden run)'], ' / '.join(off))
    # fx_audio off (the default): not a word of it
    lines, _ = run('audio_default', MENU, 16000)
    check('audio: fx_audio off by default: no "fx: audio" line', not any(l.startswith('fx: audio') for l in lines))
    test_audio_sides()

def wav_rms(path, start=0, length=None):
    """a 16-bit stereo WAV's (frames, left rms, right rms), of the frames from start (length of them when given)"""
    import math
    d = open(path, 'rb').read()
    i = d.find(b'data'); n = struct.unpack('<I', d[i + 4:i + 8])[0]; pcm = d[i + 8:i + 8 + n]
    pcm = pcm[start * 4:] if length is None else pcm[start * 4:(start + length) * 4]
    frames = len(pcm) // 4
    if not frames: return 0, 0.0, 0.0
    s = struct.unpack('<%dh' % (frames * 2), pcm[:frames * 4])
    return frames, math.sqrt(sum(v * v for v in s[0::2]) / frames), math.sqrt(sum(v * v for v in s[1::2]) / frames)

def test_audio_sides():
    """which side a sound comes out (2026-11-13). Level 1's door 1012 at 22,5 from 21,5: the save faces south-east
    (heading 23450 of 65536), so the door, due east, is ahead-left; play_effect gives it pan 0x1A (General MIDI's
    left). The music is off (music_volume 0) so the mix is the effects alone (the door opened by E, closed by the
    right button: sounds 11 and 12 at the door).
      - the Roland mix (--audio-wav, the music driver through Munt), the pan fix on (the default): the LEFT channel
        louder; with UWPORT_FX_PANFIX=0 the RIGHT (Munt reads the pan controller inverted: Part.cpp)
      - fx_audio on with the loopback WAV (UWPORT_FX_LOOPBACK): "fx: Roland effects rendered by Munt"; the door comes
        out LEFT of OpenAL's output; and the Roland mix of the same run is near silent (the game's copy at volume 0,
        port_fx_control: under a fifth of the pan run's energy)"""
    door = (MENU + "15000 cheat goto 1 21 5\n16000 cheat where\n17000 key e\n18000 move 160 90\n18500 mdown 160 90 right\n"
            "18700 mup 160 90 right\n")
    cfg = 'music_volume=0\nsound_volume=100\n'
    def mix(name, env, lb=False):
        wav = os.path.join(WORK, name + '-mix.wav')
        if os.path.exists(wav): os.remove(wav)
        e = dict(UWPORT_FX_LOG='1', **env)
        lbw = None
        if lb:
            lbw = os.path.join(WORK, name + '-al.wav')
            if os.path.exists(lbw): os.remove(lbw)
            e['UWPORT_FX_LOOPBACK'] = lbw
        lines, _ = run(name, door, 22000, e, cfg=cfg, audio=True, args=['--audio-wav', wav])
        snd = [l for l in lines if l.startswith('fx:') or l.startswith('where')]
        print('\n'.join('  ' + l for l in snd[:8]))
        return lines, wav, lbw
    lines, wav, _ = mix('sides_fixed', {})
    ok = any(re.match(r'fx: sound 11 at \d+,\d+', l) for l in lines)
    check('sides: the door opened (sound 11)', ok, '\n'.join(lines[-5:]))
    n, l, r = wav_rms(wav)
    print('  Roland mix, pan fix on: %d frames, left %.0f right %.0f' % (n, l, r))
    check('sides: Roland mix, pan fix on: the door ahead-left comes out LEFT', n > 0 and l > 1.3 * r, 'left %.0f right %.0f' % (l, r))
    base = l + r
    lines, wav, _ = mix('sides_inverted', {'UWPORT_FX_PANFIX': '0'})
    n, l, r = wav_rms(wav)
    print('  Roland mix, pan fix off: %d frames, left %.0f right %.0f' % (n, l, r))
    check('sides: Roland mix, pan fix off: the door comes out RIGHT (Munt\'s inverted pan)', n > 0 and r > 1.3 * l, 'left %.0f right %.0f' % (l, r))
    lines, wav, lbw = mix('sides_positional', {'UWPORT_FX_AUDIO': '1', 'UWPORT_FX_BIN': OPENAL_BIN}, lb=True)
    on = [l for l in lines if l.startswith('fx: audio') or l.startswith('fx: Roland')]
    print('\n'.join('  ' + l for l in on))
    check('sides: positional on in the check (loopback) with Munt\'s renderings',
          any(l.startswith('fx: audio on (OpenAL Soft loopback') for l in on) and any(l.startswith('fx: Roland effects rendered') for l in on), ' / '.join(on))
    # the door's own window: from the frame the module started it ("fx: al 11 at frame F") up to its 1.5 s (its
    # length) or the next effect the module started (a creature's footsteps about, sounds 1 and 2, come when they
    # come: behind-right of this facing, they would even the sides out), whichever is sooner
    al = [(int(x.group(2)), int(x.group(1))) for x in (re.match(r'fx: al (\d+) at frame (\d+)', l) for l in lines) if x]
    door = [f for f, fx in al if fx == 11]
    f0 = door[0] if door else 0
    f1 = min([f for f, fx in al if f > f0] + [f0 + 48000 * 3 // 2])
    n, l, r = wav_rms(lbw, f0, f1 - f0) if lbw and os.path.exists(lbw) and door and f1 - f0 > 4800 else (0, 0, 0)
    print('  OpenAL, the door\'s %.2f s from frame %d: %d frames, left %.0f right %.0f' % ((f1 - f0) / 48000.0, f0, n, l, r))
    check('sides: OpenAL puts the door ahead-left on the LEFT', n > 0 and l > 50 and l > 1.3 * r, 'left %.0f right %.0f' % (l, r))
    # the game's copy on the music driver silent meanwhile: the Roland mix of this run far under what OpenAL put out
    # in it (both over the whole run), and under a third of the pan run (whose creature about varies run by run)
    n, l, r = wav_rms(wav)
    nl, ll, rl = wav_rms(lbw) if lbw and os.path.exists(lbw) else (0, 0, 0)
    print('  Roland mix with positional on: %d frames, left %.0f right %.0f (OpenAL\'s whole run %.0f, pan run %.0f)' % (n, l, r, ll + rl, base))
    # (2026-11-14, the review: this compared the whole Roland mix with a quarter of OpenAL's output and failed every
    # run, left ~28 right ~18. That level is not the door: it is the music at music_volume 0, which Munt, as the
    # unit, does not make wholly silent; it is there from the title on, the same with positional sound off. So: the
    # Roland mix in the door's three seconds (17 to 20 s) is no louder than that bed just before it (14.5 to 16.5 s),
    # and the whole run stays under a third of the pan run.)
    _, bl, br = wav_rms(wav, int(14.5 * 44100), 2 * 44100)
    _, dl, dr = wav_rms(wav, 17 * 44100, 3 * 44100)
    print('  Roland mix, positional on: the bed before the door left %.0f right %.0f; the door\'s 3 s left %.0f right %.0f' % (bl, br, dl, dr))
    check('sides: the Roland copy silent while OpenAL plays it (the door\'s 3 s no louder than the bed before it; the run under a third of the pan run)',
          n > 0 and nl > 0 and dl + dr < 1.5 * (bl + br) + 5 and l + r < base / 3,
          'bed %.0f + %.0f, the door\'s 3 s %.0f + %.0f; the run left %.0f right %.0f, OpenAL %.0f, pan run %.0f' % (bl, br, dl, dr, l, r, ll + rl, base))

def png_rgb(path):
    """a PNG's pixels as (w, h, bytes rgb) (8-bit RGB or RGBA, non-interlaced: what the port and uwview write)"""
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', path
    pos, idat = 8, b''
    while pos < len(d):
        n, = struct.unpack('>I', d[pos:pos + 4]); t = d[pos + 4:pos + 8]; c = d[pos + 8:pos + 8 + n]
        if t == b'IHDR': w, h, depth, ctype = struct.unpack('>IIBB', c[:10]); assert depth == 8 and ctype in (2, 6)
        elif t == b'IDAT': idat += c
        pos += 12 + n
    raw = zlib.decompress(idat); bpp = 3 if ctype == 2 else 4; stride = w * bpp
    out = bytearray(); prev = bytearray(stride); p = 0
    for y in range(h):
        f = raw[p]; line = bytearray(raw[p + 1:p + 1 + stride]); p += 1 + stride
        if f == 2: line = bytearray((a + b) & 255 for a, b in zip(line, prev))
        elif f:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0; b = prev[i]; cc = prev[i - bpp] if i >= bpp else 0
                if f == 1: line[i] = (line[i] + a) & 255
                elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
                elif f == 4:
                    pa, pb, pc = abs(b - cc), abs(a - cc), abs(a + b - 2 * cc)
                    line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else cc)) & 255
        prev = line
        out += line if bpp == 3 else bytes(line[j] for j in range(stride) if j % 4 != 3)
    return w, h, bytes(out)

def differ(a, b, thresh=12):
    """the pixels of two pictures that differ by more than thresh (the channels' sum): [(x, y)], w, h"""
    w, h, pa = png_rgb(a)
    w2, h2, pb = png_rgb(b)
    assert (w, h) == (w2, h2)
    pts = []
    for i in range(w * h):
        j = i * 3
        if abs(pa[j] - pb[j]) + abs(pa[j + 1] - pb[j + 1]) + abs(pa[j + 2] - pb[j + 2]) > thresh: pts.append((i % w, i // w))
    return pts, w, h

def compare(name, script, exit_ms, shot_ms, what, env):
    """an effect on / off at a held view (uwview's UWVIEW_COMPARE with UWVIEW_COMPARE_FX): the two pictures' paths and
    the run's lines"""
    pre = os.path.join(WORK, name)
    for k in ('-on.png', '-off.png'):
        if os.path.exists(pre + k): os.remove(pre + k)
    lines, wshot = run(name, script, exit_ms, dict(env, UWVIEW_COMPARE=pre, UWVIEW_COMPARE_FX=what, UWVIEW_FX_LOG='1'), shot_ms=shot_ms)
    return pre + '-on.png', pre + '-off.png', lines

def renderer_ms(lines):
    v = [float(m.group(1)) for m in (re.search(r'the renderer ([0-9.]+) ms', l) for l in lines) if m]
    v = v[2:]   # (the start: the level's first frames)
    return sum(v) / len(v) if v else None

# ---- Task 3: particles ----
def test_particles():
    # the campfire at 8,42 (level 1, object 767) two tiles ahead of 6,42, the way the save faces
    view = MENU + "15000 cheat goto 1 6 42\n"
    on, off, lines = compare('particles', view, 28000, 22000, 'particles', dict(UWVIEW_FX_PARTICLES='1'))
    em = [l for l in lines if l.startswith('uwview: particles:') and 'nearest in view' in l]
    ok = os.path.exists(on) and os.path.exists(off) and em
    check('particles: the comparison made (on / off pictures, the flame in view)', ok, '\n'.join(lines[-5:]))
    if ok:
        m = re.search(r'at ([0-9.-]+) ([0-9.-]+)$', em[-1])
        ex, ey = float(m.group(1)), float(m.group(2))
        pts, w, h = differ(on, off, 40)   # (bright: below 40 the lit stone's own noise between two held runs, DLSS's and the shadows')
        x0, x1, y0, y1 = (ex - 0.12) * w, (ex + 0.12) * w, (ey - 0.45) * h, (ey + 0.06) * h
        inside = sum(1 for x, y in pts if x0 <= x <= x1 and y0 <= y <= y1)
        why = '%d pixels differ, %d inside the flame\'s region x %d-%d y %d-%d (the flame at %d,%d)' % (len(pts), inside, x0, x1, y0, y1, ex * w, ey * h)
        print('  ' + why)
        check('particles: the pictures differ (sparks and embers drawn)', len(pts) >= 50, why)
        check('particles: only round and above the flame (97% of the changed pixels)', pts and inside >= 0.97 * len(pts), why)
    # a level change: every particle dropped
    lines, _ = run('particles_level', view + "20000 cheat goto 2 28 28\n", 26000, dict(UWVIEW_FX_PARTICLES='1', UWVIEW_FX_LOG='1'))
    before = [l for l in lines if l.startswith('uwview: particles:') and 'live' in l and 'cleared' not in l]
    cl = [l for l in lines if 'level 2: cleared' in l]
    check('particles: some live on level 1', any(int(re.search(r'(\d+) live', l).group(1)) > 0 for l in before), '\n'.join(before[:3]))
    check('particles: level 2 starts with none ("level 2: cleared, 0 live")', cl == ['uwview: particles: level 2: cleared, 0 live'], '\n'.join(cl))
    # GDI (the renderer not started): no crash, the run to its end
    lines, _ = run('particles_gdi', view, 20000, dict(UWVIEW_FX_PARTICLES='1', UWPORT_GDI='1'))
    check('particles: with GDI the run is clean (exit 0, GDI said)', last_rc == 0 and any('GDI' in l for l in lines), 'exit %d' % last_rc)
    # the frame's cost (UWPORT_TIMING: the renderer's share, averaged over the run after its start)
    t_on = renderer_ms(run('particles_t1', view, 40000, dict(UWVIEW_FX_PARTICLES='1', UWPORT_TIMING='1'))[0])
    t_off = renderer_ms(run('particles_t0', view, 40000, dict(UWVIEW_FX_PARTICLES='0', UWPORT_TIMING='1'))[0])
    why = 'the renderer %.2f ms with particles, %.2f ms without' % (t_on or -1, t_off or -1)
    print('  ' + why)
    check('particles: the frame within 0.4 ms of off', t_on is not None and t_off is not None and t_on - t_off < 0.4, why)

# ---- Task 4: decals ----
def test_decals():
    view = MENU + "15000 cheat goto 1 6 42\n"
    on, off, lines = compare('decals', view, 28000, 22000, 'decals', dict(UWVIEW_FX_DECALS='1', UWVIEW_FX_TEST='decal'))
    at = [l for l in lines if l.startswith('uwview: decal test at screen')]
    ok = os.path.exists(on) and os.path.exists(off) and at
    check('decals: the comparison made (the test mark placed)', ok, '\n'.join(lines[-5:]))
    if ok:
        m = re.search(r'screen ([0-9.-]+) ([0-9.-]+)$', at[-1])
        ex, ey = float(m.group(1)), float(m.group(2))
        pts, w, h = differ(on, off, 40)
        x0, x1, y0, y1 = (ex - 0.2) * w, (ex + 0.2) * w, (ey - 0.25) * h, (ey + 0.25) * h
        inside = sum(1 for x, y in pts if x0 <= x <= x1 and y0 <= y <= y1)
        why = '%d pixels differ, %d round the mark at %d,%d' % (len(pts), inside, ex * w, ey * h)
        print('  ' + why)
        check('decals: the mark drawn (the pictures differ)', len(pts) >= 200, why)
        check('decals: only round the mark (97%)', pts and inside >= 0.97 * len(pts), why)
    # a fight: blows on the goblins near 10,39 of level 1, then a goto (the eye jumps: cleared), then level 2
    s = MENU + "15000 cheat goto 1 10 39\n15500 cheat anger 218\n16000 key c\n16500 move 160 90\n"
    for t in range(17000, 44000, 1500): s += '%d mdown 160 90 right\n%d mup 160 90 right\n' % (t, t + 900)
    s += "25000 cheat set hp 30\n35000 cheat set hp 30\n45000 cheat goto 1 6 42\n48000 cheat goto 2 28 28\n"   # (alive to the end)
    s = ''.join(sorted(s.splitlines(True), key=lambda l: int(l.split()[0])))
    lines, _ = run('decals_fight', s, 52000, dict(UWVIEW_FX_DECALS='1', UWVIEW_FX_LOG='1'))
    hits = [tuple(map(float, re.search(r'at ([0-9.]+) [0-9.]+ ([0-9.]+)', l).groups())) for l in lines if l.startswith('uwview: fx event sound 4 at')]
    marks = [(float(m.group(1)), float(m.group(2)), int(m.group(3))) for m in (re.search(r'decal 0 at ([0-9.]+) ([0-9.]+) \(creature (\d+)\)', l) for l in lines) if m]
    under = all(any(abs(mx - hx) < 0.5 and abs(mz - hz) < 0.5 for mx, mz, _ in marks) for hx, hz in hits)
    why = '%d blows landed, %d marks (creatures %s)' % (len(hits), len(marks), sorted(set(c for _, _, c in marks)))
    print('  ' + why)
    check('decals: blows landed in the fight', len(hits) >= 1, why)
    check('decals: blood under each creature hit, 1-2 marks a blow, the creature known', under and len(hits) <= len(marks) <= 2 * len(hits) and all(c >= 64 for _, _, c in marks), why)
    check('decals: a jump of the eye clears them', any('the eye jumped: cleared' in l for l in lines))
    check('decals: level 2 starts with none', 'uwview: decals: level 2: cleared, 0' in lines)

# ---- Task 5: water ----
def water_mask(name, script, env):
    """the water's pixels at that view: a run with UWVIEW_DEBUG=water (water white, the rest black), as a set"""
    lines, wshot = run(name, script, 26000, dict(env, UWVIEW_DEBUG='water'), shot_ms=22000)
    w, h, px = png_rgb(wshot)
    return set(i for i in range(w * h) if px[i * 3] > 128 and px[i * 3 + 1] > 128), w, h

def near_mask(pts, mask, w, r=3):
    return sum(1 for x, y in pts if any((y + dy) * w + x + dx in mask for dx in (-r, 0, r) for dy in (-r, 0, r)))

def test_water():
    # (a creature near 13,24 fights: the player kept alive)
    view = MENU + "15000 cheat goto 1 13 24\n16000 cheat set hp 60\n19000 cheat set hp 60\n22000 cheat set hp 60\n25000 cheat set hp 60\n"
    mask, w, h = water_mask('water_mask', view, {})
    check('water: water in view (its mask)', len(mask) > 20000, '%d water pixels' % len(mask))
    for name, env in (('water', {}), ('water_nort', dict(UWVIEW_RT='0'))):
        on, off, lines = compare(name, view, 28000, 22000, 'water', dict(env, UWVIEW_FX_WATER='1'))
        if not (os.path.exists(on) and os.path.exists(off)):
            check('%s: the comparison made' % name, False, '\n'.join(lines[-5:]))
            continue
        pts, _, _ = differ(on, off, 30)
        inside = near_mask(pts, mask, w)
        why = '%d pixels differ by more than 30, %d on the water' % (len(pts), inside)
        print('  %s: %s' % (name, why))
        check('%s: the pictures differ%s' % (name, ' (ray tracing off: the refraction)' if env else ''), len(pts) >= 2000, why)
        check('%s: only on the water (97%%)' % name, pts and inside >= 0.97 * len(pts), why)
    on, off, lines = compare('water_ripple', view, 28000, 22000, 'ripple', dict(UWVIEW_FX_WATER='1', UWVIEW_FX_TEST='splash'))
    at = [l for l in lines if l.startswith('uwview: decal test at screen')]
    if not (os.path.exists(on) and os.path.exists(off) and at):
        check('water: the ripple compared', False, '\n'.join(lines[-5:]))
        return
    m = re.search(r'screen ([0-9.-]+) ([0-9.-]+)$', at[-1])
    ex, ey = float(m.group(1)) * w, float(m.group(2)) * h
    pts, _, _ = differ(on, off, 30)
    inside = near_mask(pts, mask, w)
    near = sum(1 for x, y in pts if abs(x - ex) < 0.3 * w and abs(y - ey) < 0.3 * h)
    why = '%d pixels differ, %d on the water, %d round the splash at %d,%d' % (len(pts), inside, near, ex, ey)
    print('  ripple: ' + why)
    check('water: a splash adds a ring (the pictures differ)', len(pts) >= 300, why)
    check('water: the ring on the water round the splash (97%)', pts and inside >= 0.97 * len(pts) and near >= 0.97 * len(pts), why)

def test_gem():
    """the power gem in our HUD (2026-11-13, uwv_hud): fight mode (C through the window's own input, UWPORT_MOUSETEST),
    the left button held from 18.6 s (the port's Space: a blow charging); the window shot at 18.3 s (the gem at rest)
    against one at 21.8 s (charged): the HUD's gem box, at the compass's left, differs; and the original's own picture
    shows the gem changed at rows 139-150 (PANELS.C adjust_power), which is what the HUD copies"""
    s = MENU + "15000 cheat goto 1 36 7\n"
    env = dict(UWPORT_MOUSETEST='17000:kd@46/0,17100:ku@46/0,18600:ld,22500:lu')
    _, wa = run('gem_rest', s, 24000, env, shot_ms=18300)
    lines, wb = run('gem_charged', s, 24000, env, shot_ms=21800)
    check('gem: fight mode asked (C)', any('fight mode on asked' in l for l in lines))
    w, h, pa = png_rgb(os.path.join(WORK, 'gem_rest.png')); _, _, pb = png_rgb(os.path.join(WORK, 'gem_charged.png'))
    n = sum(1 for y in range(139, 151) for x in range(4, 35) if pa[(y * w + x) * 3:(y * w + x) * 3 + 3] != pb[(y * w + x) * 3:(y * w + x) * 3 + 3])
    print('  the original\'s gem box (4,139 31x12): %d pixels differ' % n)
    check('gem: the original\'s picture shows the gem charging', n >= 100, '%d pixels' % n)
    if not (wa and wb and os.path.exists(wa) and os.path.exists(wb)):
        check('gem: window shots', False, 'no window shot'); return
    w, h, pa = png_rgb(wa); _, _, pb = png_rgb(wb)
    sc = max(h // 270, 1); m = 4 * sc
    x0, x1 = w // 2 - 28 * sc - m - 31 * sc, w // 2 - 28 * sc - m
    y0, y1 = h - 24 * sc - m + 6 * sc, h - 24 * sc - m + 18 * sc
    n = sum(1 for y in range(y0, y1) for x in range(x0, x1) if pa[(y * w + x) * 3:(y * w + x) * 3 + 3] != pb[(y * w + x) * 3:(y * w + x) * 3 + 3])
    print('  our HUD\'s gem box (%d,%d)-(%d,%d) of %dx%d: %d pixels differ' % (x0, y0, x1, y1, w, h, n))
    check('gem: our HUD shows the gem charging, at the compass\'s left', n >= 100 * sc * sc // 4, '%d pixels' % n)

# ---- acoustics, Task 1 (docs\superpowers\plans\2026-11-14-acoustics-ours.md): the materials table ----
def test_materials():
    w = os.path.join(WORK, 'materials')
    os.makedirs(w, exist_ok=True)
    exe = os.path.join(w, 'uwacmat_test.exe')
    ok = clang(exe, [os.path.join(ROOT, 'tests', 'uwacmat_test.c'), os.path.join(ROOT, 'port', 'win32', 'uwacmat.c'), os.path.join(ROOT, 'port', 'win32', 'rlog.c')])
    check('materials: tests\\uwacmat_test.c builds', ok)
    if ok:
        r = subprocess.run([exe, w], capture_output=True, text=True)
        print(r.stdout.rstrip())
        check('materials: the table\'s cases (count, wall, door, values, last, doors, ceiling, unknown, badline, missing, default, default in memory, write)',
              r.returncode == 0 and r.stdout.count('PASS') == 13 and ' FAIL' not in r.stdout, r.stdout + r.stderr)
    # the levels' own textures through the table the code ships (tools\acoustics_list.py --default: the code's text,
    # not a data\acoustics.txt someone edited): the tool exits 0 and every texture in use resolves to a preset
    r = subprocess.run([sys.executable, '-I', os.path.join(ROOT, 'tools', 'acoustics_list.py'), '--default'], capture_output=True, text=True)
    tail = [l for l in r.stdout.splitlines() if l.startswith('acoustics_list:')]
    print('\n'.join('  ' + l for l in tail))
    check('materials: tools\\acoustics_list.py exits 0 and every texture of levels 1-9 resolves',
          r.returncode == 0 and any('0 unresolved' in l for l in tail), (r.stdout[-600:] + r.stderr[-600:]))

# ---- acoustics, Task 2: the tile grid and the doors from the renderer (uwv_acoustic_grid / _doors / _epoch) ----
def level_txt(n):
    """our level file: (open tiles as {(x, z)}, the door objects on the tiles' chains as {object number: item})"""
    tiles, objs = {}, {}
    for l in open(os.path.join(ROOT, 'data', 'levels', 'level%d.txt' % n)):
        w = l.split()
        if w[:1] == ['tile']:
            v = [int(x) for x in w[1:]] + [0] * 10
            tiles[(v[0], v[1])] = (v[2], v[9])                 # type, first_object
        elif w[:1] == ['object']:
            objs[int(w[1])] = (int(w[2]), int(w[9]))           # item, next (after flags z heading y x quality)
    doors = {}
    for (x, z), (typ, i) in tiles.items():
        guard = 0
        while 0 < i < 1024 and guard < 1024:
            item, nxt = objs.get(i, (0, 0))
            if 0x140 <= item <= 0x14F: doors[i] = item
            i, guard = nxt, guard + 1
    return {p for p, (typ, _) in tiles.items() if typ != 0}, doors

def acoustic_dump(path):
    """the renderer's dump (UWVIEW_ACOUSTIC_DUMP): a list of blocks {epoch, level, ceil, tiles {(x, z): (type, height,
    wall, floor)}, doors [(id, tx, tz, heading, item, picture, shut)]}, one a level built"""
    blocks = []
    if not os.path.exists(path): return blocks
    for l in open(path):
        w = l.split()
        if w[:1] == ['acoustic']:
            blocks.append(dict(epoch=int(w[2]), level=int(w[4]), ceil=int(w[6]), tiles={}, doors=[], ended=False))
        elif w[:1] == ['tile'] and blocks:
            v = [int(x) for x in w[1:]]
            blocks[-1]['tiles'][(v[0], v[1])] = tuple(v[2:6])
        elif w[:1] == ['door'] and blocks:
            blocks[-1]['doors'].append(tuple(int(x) for x in w[1:8]))
        elif w[:1] == ['end'] and blocks:
            blocks[-1]['ended'] = True
    return blocks

def test_grid():
    dump = os.path.join(WORK, 'grid-dump.txt')
    os.makedirs(WORK, exist_ok=True)
    if os.path.exists(dump): os.remove(dump)
    # level 1 as the save has it, then a goto to level 2 (a level built again: the epoch moves)
    lines, _ = run('grid', MENU + "15000 cheat goto 1 6 42\n20000 cheat goto 2 28 28\n", 27000, dict(UWVIEW_ACOUSTIC_DUMP=dump, UWPORT_FX_LOG='1'))
    blocks = acoustic_dump(dump)
    l1 = [b for b in blocks if b['level'] == 1][:1]
    l2 = [b for b in blocks if b['level'] == 2][:1]
    check('grid: the renderer wrote level 1\'s grid and level 2\'s (UWVIEW_ACOUSTIC_DUMP)', bool(l1 and l2) and all(b['ended'] for b in blocks),
          '%d blocks; %s' % (len(blocks), '\n'.join(lines[-5:])))
    if not (l1 and l2): return
    for b in (l1[0], l2[0]):
        n = b['level']
        opened, doors = level_txt(n)
        mine = {p for p, t in b['tiles'].items() if t[0] != 0}
        why = 'level %d: %d tiles, %d open (the level file %d), ceiling texture %d, %d doors (the level file %d), epoch %d' % (
            n, len(b['tiles']), len(mine), len(opened), b['ceil'], len(b['doors']), len(doors), b['epoch'])
        print('  ' + why)
        check('grid: level %d has 4,096 tiles, the level file\'s open ones' % n, len(b['tiles']) == 4096 and mine == opened, why)
        bad = [(p, t) for p, t in b['tiles'].items() if not os.path.exists(os.path.join(ROOT, 'data', 'textures', 'walls', '%03d.png' % t[2]))
               or not os.path.exists(os.path.join(ROOT, 'data', 'textures', 'floors', '%03d.png' % t[3]))]
        check('grid: level %d: every wall and floor texture is a file of data\\textures, the ceiling\'s too' % n,
              not bad and os.path.exists(os.path.join(ROOT, 'data', 'textures', 'floors', '%03d.png' % b['ceil'])), str(bad[:3]))
        # the doors: the level file's door objects, each on its own tile; the picture the level's for the item
        # (level.c lv.doors[item & 7] under 6, the portcullis 12, the secret door none), a portcullis never shut
        ids = {d[0]: d for d in b['doors']}
        wrong = [d for d in b['doors'] if d[0] not in doors or (d[1], d[2]) not in b['tiles'] or d[3] not in (0, 2, 4, 6)
                 or not 0x140 <= d[4] <= 0x14F or not -1 <= d[5] <= 12 or d[6] not in (0, 1)
                 or ((d[4] & 7) == 6 and (d[5] != 12 or d[6] != 0)) or ((d[4] & 7) == 7 and d[5] != -1) or (d[4] >= 0x148 and d[6] != 0)]
        check('grid: level %d: the doors are the level file\'s door objects, each with its picture' % n,
              set(ids) == set(doors) and len(ids) == len(b['doors']) and not wrong, 'wrong: %s; missing %s' % (wrong[:3], sorted(set(doors) - set(ids))[:5]))
    check('grid: the tile the run went to (level 1, 6,42) is open', l1[0]['tiles'].get((6, 42), (0,))[0] != 0, str(l1[0]['tiles'].get((6, 42))))
    check('grid: the epoch differs after the level change', l1[0]['epoch'] != l2[0]['epoch'], '%d, %d' % (l1[0]['epoch'], l2[0]['epoch']))
    # the game's side (uwfront.c: V.ac_grid, V.ac_doors, V.ac_epoch): one log line a level it took, the same counts
    got = {}
    for l in lines:
        m = re.match(r'fx: acoustics: grid epoch (\d+): (\d+) open tiles, (\d+) doors \((\d+) shut\)', l)
        if m: got[int(m.group(1))] = tuple(int(x) for x in m.groups()[1:])
    want = {b['epoch']: (sum(1 for t in b['tiles'].values() if t[0] != 0), len(b['doors']), sum(d[6] for d in b['doors'])) for b in blocks}
    print('  the game took: %s' % ', '.join('epoch %d: %d open, %d doors, %d shut' % ((e,) + got[e]) for e in sorted(got)))
    check('grid: the game took the grid and the doors of each level built (the dump\'s counts)',
          all(e in got and got[e][:2] == want[e][:2] for e in (l1[0]['epoch'], l2[0]['epoch'])), 'the game %s, the dump %s' % (got, want))

# ---- acoustics, Tasks 3 and 4: our rays over the tile grid (port\win32\uwacoustic.c) ----
ACOUSTIC_CASES = ['ray wall', 'ray floor', 'ray ceiling', 'ray step', 'ray door', 'ray diagonal', 'inside', 'edge',
                  'room size', 'room mfp', 'room material', 'room open door', 'path clear', 'path rock', 'path door', 'path own door', 'path corner', 'path here',
                  'detour straight', 'detour corner', 'detour door', 'detour step', 'waypoint', 'path round']
def test_acoustics():
    w = os.path.join(WORK, 'acoustics')
    os.makedirs(w, exist_ok=True)
    exe = os.path.join(w, 'uwacoustic_test.exe')
    # (the bounds sanitizer, trapping: a read outside the 64 x 64 grid stops the test. The plan's address sanitizer
    # has no runtime for our MinGW target.)
    ok = clang(exe, [os.path.join(ROOT, 'tests', 'uwacoustic_test.c'), os.path.join(ROOT, 'port', 'win32', 'uwacoustic.c'),
                     os.path.join(ROOT, 'port', 'win32', 'uwacmat.c'),
                     os.path.join(ROOT, 'port', 'win32', 'rlog.c')], ['-fsanitize=bounds', '-fsanitize-trap=bounds'])
    check('acoustics: tests\\uwacoustic_test.c builds', ok)
    if not ok: return
    dump = os.path.join(WORK, 'grid-dump.txt')   # (the grid check's: level 1 as the renderer gave it; timed when there)
    r = subprocess.run([exe, dump, w], capture_output=True, text=True)
    print(r.stdout.rstrip())
    passed = [c for c in ACOUSTIC_CASES if (c + ' PASS') in r.stdout.splitlines()]
    check('acoustics: the rays\' and the room\'s cases (%s)' % ', '.join(ACOUSTIC_CASES),
          r.returncode == 0 and len(passed) == len(ACOUSTIC_CASES) and ' FAIL' not in r.stdout, r.stdout[-800:] + r.stderr[-400:])
    test_acoustics_game()

def test_acoustics_game():
    """Task 4 in the game (a hidden run, the loopback WAV, nothing heard): the acoustics start, the level reaches
    them, the great hall rings longer than a corridor, and a sound with rock between it and the player is muffled"""
    import math
    opened, _ = level_txt(1)
    def corridor():   # a north-south passage one tile wide, five long
        for (x, z) in sorted(opened):
            if all((x, z + d) in opened and (x - 1, z + d) not in opened and (x + 1, z + d) not in opened for d in range(-2, 3)): return x, z
        return None
    def roomy(x, z, r=3): return sum(1 for dx in range(-r, r + 1) for dz in range(-r, r + 1) if (x + dx, z + dz) in opened)
    hall = max(sorted(opened), key=lambda p: roomy(*p))   # the roomiest place of the level (the most open tiles in the 7 x 7 round it)
    cor = corridor()
    check('acoustics game: level 1 has a corridor to stand in, and the great hall', cor is not None, str(cor))
    if cor is None: return
    wav = os.path.join(WORK, 'acoustics_game-al.wav')
    if os.path.exists(wav): os.remove(wav)
    script = MENU + "15000 cheat goto 1 %d %d\n20000 cheat goto 1 %d %d\n" % (hall + cor)
    lines, _ = run('acoustics_game', script, 26000, dict(UWPORT_FX_LOG='1', UWPORT_FX_AUDIO='1', UWPORT_FX_BIN=OPENAL_BIN, UWPORT_FX_LOOPBACK=wav,
                                                         UWPORT_FX_ACOUSTICS='1'), audio=True)
    check('acoustics game: "fx: acoustics on (EFX)" and the level handed over ("fx: acoustics: grid epoch")',
          any(l.startswith('fx: acoustics on (EFX)') for l in lines) and any(l.startswith('fx: acoustics: grid epoch') for l in lines),
          '\n'.join(l for l in lines if 'acoustics' in l)[:600])
    rooms = [(float(m.group(1)), float(m.group(2)), [float(m.group(k)) for k in (3, 4, 5)], float(m.group(6))) for m in
             (re.match(r'fx: acoustics: room at \(([-0-9.]+), ([-0-9.]+)\): rt60 ([0-9.]+) ([0-9.]+) ([0-9.]+) s, mean free path ([0-9.]+) m', l) for l in lines) if m]
    def at(place): return [r for r in rooms if abs(r[0] - place[0] - 0.5) < 1.0 and abs(r[1] - place[1] - 0.5) < 1.0][-1:]
    rh, rc = at(hall), at(cor)
    why = 'the hall at %s: %s; the corridor at %s: %s' % (hall, rh and 'rt60 %.2f %.2f %.2f s, mean free path %.1f m' % (tuple(rh[0][2]) + (rh[0][3],)),
                                                          cor, rc and 'rt60 %.2f %.2f %.2f s, mean free path %.1f m' % (tuple(rc[0][2]) + (rc[0][3],)))
    print('  ' + why)
    check('acoustics game: the great hall rings longer than the corridor (the mid band\'s decay, the mean free path)',
          bool(rh and rc) and rh[0][2][1] > rc[0][2][1] and rh[0][3] > rc[0][3], why)
    # every sound the run placed: its gains sane; with half a tile of rock or more on the straight line to the player,
    # muffled (the mid band at most 0.8, what a sound coming round a corner by the shortest way has: Task 5)
    paths = [[float(x) for x in m.groups()[1:]] for m in
             (re.match(r'fx: acoustics: path of (\d+) from \(([-0-9.]+), ([-0-9.]+), ([-0-9.]+)\) to \(([-0-9.]+), ([-0-9.]+), ([-0-9.]+)\): ([0-9.]+) ([0-9.]+) ([0-9.]+)', l) for l in lines) if m]
    bad, behind, clear = [], 0, 0
    for sx, sy, sz, lx, ly, lz, g0, g1, g2 in paths:
        n = max(2, int(math.hypot(sx - lx, sz - lz) / 0.05))
        rock = sum(1 for i in range(n + 1) if (int(math.floor(lx + (sx - lx) * i / n)), int(math.floor(lz + (sz - lz) * i / n))) not in opened) * 0.05
        if not (0.029 <= g2 <= 1.0 and 0.099 <= g1 <= 1.0 and 0.149 <= g0 <= 1.0): bad.append(('range', sx, sz, lx, lz, g0, g1, g2))
        if rock >= 0.5:
            behind += 1
            if g1 > 0.81: bad.append(('rock %.2f' % rock, sx, sz, lx, lz, g0, g1, g2))   # (0.8: the most a way round gives, Task 5)
        elif g1 >= 0.99: clear += 1
    print('  %d sounds placed: %d with rock on the straight line (all muffled: %s), %d wholly clear' % (len(paths), behind, not [b for b in bad if b[0] != 'range'], clear))
    check('acoustics game: the placed sounds\' gains are sane, and those behind rock are muffled', bool(paths) and not bad, 'paths %d; %s' % (len(paths), bad[:3]))
    check('acoustics game: the run is clean (exit 0)', last_rc == 0, 'exit %d' % last_rc)
    # Task 7, Acoustics High in the game: the same run with fx_acoustics 2: Steam Audio starts, takes the level (its
    # triangles), the sounds the run places still get our muffling, and the run is clean
    wav2 = os.path.join(WORK, 'acoustics_high_game-al.wav')
    if os.path.exists(wav2): os.remove(wav2)
    lines, _ = run('acoustics_high_game', script, 26000, dict(UWPORT_FX_LOG='1', UWPORT_FX_AUDIO='1', UWPORT_FX_BIN=OPENAL_BIN, UWPORT_FX_LOOPBACK=wav2,
                                                              UWPORT_FX_ACOUSTICS='2', UWPORT_FX_PHONON=PHONON_BIN), audio=True)
    hi = [l for l in lines if l.startswith('fx: acoustics high')]
    paths2 = [l for l in lines if l.startswith('fx: acoustics: path of')]
    print('  %s; %d sounds placed' % ('; '.join(hi), len(paths2)))
    check('acoustics game: with fx_acoustics 2 "fx: acoustics high on (Steam Audio 4.8.1)", sounds placed, a clean run',
          any(l.startswith('fx: acoustics high on (Steam Audio') for l in hi) and bool(paths2) and last_rc == 0, 'exit %d; %s' % (last_rc, hi))
    if os.path.exists(wav2) and os.path.exists(wav):
        a1, a2 = wav_rms(wav)[1:], wav_rms(wav2)[1:]
        print('  the whole run\'s sound: On left %.0f right %.0f; High left %.0f right %.0f' % (a1 + a2))
        check('acoustics game: High\'s run has sound in it', a2[0] + a2[1] > 1, str(a2))

# ---- acoustics, Task 7: Steam Audio's reflections over our tile grid (port\win32\uwsteam.c) ----
STEAM_CASES = ['load', 'scene', 'tail', 'side', 'next room', 'drain', 'swap']
def test_steam():
    w = os.path.join(WORK, 'steam')
    os.makedirs(w, exist_ok=True)
    exe = os.path.join(w, 'uwsteam_test.exe')
    ok = clang(exe, [os.path.join(ROOT, 'tests', 'uwsteam_test.c')] + [os.path.join(ROOT, 'port', 'win32', f) for f in ('uwsteam.c', 'uwacoustic.c', 'uwacmat.c', 'rlog.c')])
    check('steam: tests\\uwsteam_test.c builds', ok)
    if not ok: return
    r = subprocess.run([exe, PHONON_BIN, os.path.join(WORK, 'grid-dump.txt')], capture_output=True, text=True)
    print(r.stdout.rstrip())
    if r.stdout.startswith('SKIP'): return
    passed = [c for c in STEAM_CASES if (c + ' PASS') in r.stdout.splitlines()]
    check('steam: Steam Audio\'s reflections on our grid (%s)' % ', '.join(STEAM_CASES),
          r.returncode == 0 and len(passed) == len(STEAM_CASES) and ' FAIL' not in r.stdout, r.stdout[-800:] + r.stderr[-400:])

TESTS = {'steam': test_steam, 'materials': test_materials, 'grid': test_grid, 'acoustics': test_acoustics, 'audio': test_audio, 'sides': test_audio_sides, 'gem': test_gem, 'particles': test_particles, 'decals': test_decals, 'water': test_water}

if __name__ == '__main__':
    names = sys.argv[1:] or list(TESTS)
    for n in names:
        TESTS[n]()
    print('fxcheck: %d failure(s)' % fails)
    sys.exit(fails)
