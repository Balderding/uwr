# huntercheck.py: the level hunters (docs\superpowers\plans\2026-11-12-level-hunters.md, Task 3). Off-screen runs of
# the test exe with a fresh copy of the test home work\in2-4, a fixed plan (UWPORT_HUNTER_PLAN, no model), the
# AI log on; asserted from the "AI: hunter" lines in stdout, in order.
#   plan1    goto_last,wait,search: near goblin 218 of level 1 (angered), the player strikes the wall (a noise it
#            hears): picked, noticed, plan, leg 1, then a step done or the plan ended
#   plan_giveup  give_up: plan ended: gave up, no leg
#   plan_blocked goto_water for a walker: no route, plan ended: blocked, no leg
#   routes   Task 7: every level entered and logged; every walker's route verdict the original walker's rule's (a
#            reference from the level file, walker_cells / walker_danger); a reachable case on each level with a
#            hunter (run by tools\verify.cmd)
# Usage: python -I tools\huntercheck.py [plan1|plan_giveup|plan_blocked ...]. Exit code: the failures.
import os, re, sys, shutil, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.environ.get('HUNTERCHECK_EXE') or os.path.join(ROOT, 'work', 'hunters', 'bin', 'uw1port.exe')   # the private copy (work\hunters\mkbin.cmd after each build): another session's run of the shared build never blocks a link; tools\verify.cmd sets HUNTERCHECK_EXE to the build's own
HOME0 = os.path.join(ROOT, 'work', 'in2-4')
WORK = os.path.join(ROOT, 'work', 'hunters')
MENU = "3000 key esc\n5000 key esc\n7000 key esc\n9000 key end\n9300 key enter\n12000 key home\n12600 key enter\n14000 key enter\n"
HUNTER = (11, 38)   # goblin 218 of level 1

def level_tiles(level):
    tiles = {}
    for line in open(os.path.join(ROOT, 'data', 'levels', 'level%d.txt' % level)):
        p = line.split()
        if p and p[0] == 'tile': tiles[(int(p[1]), int(p[2]))] = int(p[3])
    return tiles

def wall_spot():
    """an open tile 2-5 squares from the hunter with a solid tile to its north (the save faces north: a strike there
    hits the wall, effect 7, which the goblin hears)"""
    t = level_tiles(1)
    best = None
    for (x, y), ty in t.items():
        if ty != 1 or t.get((x, y + 1), 0) != 0: continue
        d = max(abs(x - HUNTER[0]), abs(y - HUNTER[1]))
        if 2 <= d <= 5 and (best is None or d < best[0]): best = (d, x, y)
    return best[1], best[2]

def run(name, script, exit_ms, plan):
    home = os.path.join(WORK, 'h-' + name)
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f: f.write('settings-at-start=0\n')
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', UWPORT_AILOG='1', UWPORT_HUNTER_PLAN=plan, UWPORT_AIDBG='1',
             UWPORT_FOCUSTEST='1000,1500')
    cmd = [EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio', '--input-script', sp,
           '--exit-after', str(exit_ms)]
    r = subprocess.run(cmd, env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, name + '.out'), 'w').write(r.stdout)
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    return [l for l in r.stdout.splitlines() if l.startswith('AI: hunter')]

fails = 0
def check(name, ok, why=''):
    global fails
    print('%s %s%s' % (name, 'PASS' if ok else 'FAIL', (': ' + why) if why and not ok else ''))
    if not ok: fails += 1

def in_order(lines, *needles):
    i = 0
    for n in needles:
        while i < len(lines) and n not in lines[i]: i += 1
        if i >= len(lines): return False
        i += 1
    return True

def strike_script(x, y):
    return (MENU + "15000 cheat goto 1 %d %d\n15500 cheat anger 218\n16000 cheat anger 219\n30000 key c\n30500 move 160 90\n"
            "31000 mdown 160 90 right\n31900 mup 160 90 right\n33000 mdown 160 90 right\n33900 mup 160 90 right\n"
            "35000 mdown 160 90 right\n35900 mup 160 90 right\n") % (x, y)

def test_plan1():
    x, y = wall_spot()
    lines = run('plan1', strike_script(x, y), 75000, 'goto_last,wait,search')
    check('plan1: picked, noticed, plan, leg 1, then a step done or the plan over',
          in_order(lines, 'picked', 'noticed', 'plan: goto_last, wait, search', 'leg 1')
          and any('step done' in l or 'plan ended' in l for l in lines), '\n'.join(lines[:12]))

def test_plan_giveup():
    x, y = wall_spot()
    lines = run('plan_giveup', strike_script(x, y), 75000, 'give_up')
    check('plan_giveup: plan ended: gave up, no leg', in_order(lines, 'plan: give_up', 'plan ended: gave up')
          and not any(' leg ' in l for l in lines), '\n'.join(lines[:12]))

def test_plan_blocked():
    x, y = wall_spot()
    lines = run('plan_blocked', strike_script(x, y), 75000, 'goto_water')
    check('plan_blocked: a walker to water: plan ended: blocked, no leg', in_order(lines, 'plan: goto_water', 'plan ended: blocked')
          and not any(' leg ' in l for l in lines), '\n'.join(lines[:12]))

def side(home, slot):
    path = os.path.join(home, 'SAVE%d' % slot, 'UWNEW.DAT')
    return open(path).read().splitlines() if os.path.exists(path) else None

def test_save():
    """Task 5: (A) a plan begun, the player away, F5 + Enter saves slot I mid-plan: SAVE1/UWNEW.DAT has uwnew 1, the
    hunter's line with a plan, a mem line; (B) a second run loads it: "resumed"; (C) the record's item changed in the
    file: "record dropped" """
    x, y = wall_spot()
    # The noise comes from UWPORT_HUNTER_NOISE at the goblin's own square (real strikes are plan1's: their timing
    # differs from run to run, and a goblin near the player often saw it and fought: "plan ended: the player seen").
    # The player at 10,32: near enough that the goblin is run (the original runs only the creatures near the player),
    # with walls between. The plan begins with its 45 s wait, where the goblin stands (no route that can be blocked),
    # so it still runs at the 30 s save. Up to three tries all the same.
    script = MENU + "15000 cheat goto 1 10 32\n15500 cheat anger 218\n30000 key f5\n31500 key enter\n"
    os.environ['UWPORT_HUNTER_NOISE'] = '%d,%d' % HUNTER
    home = os.path.join(WORK, 'h-saveA')
    for attempt in range(3):
        lines = run('saveA', script, 35000, 'wait,search,goto_last')
        f = side(home, 1)
        hl = [l for l in (f or []) if l.startswith('hunter 1 ')]
        planned = [l for l in hl if l.split()[7] != '0']
        if planned: break
        print('save A: try %d: no plan at the save (%s)' % (attempt + 1, '; '.join(lines[-2:])))
    del os.environ['UWPORT_HUNTER_NOISE']   # (run A's alone: the loads below must not hear it again)
    check('save A: SAVE1\\UWNEW.DAT written mid-plan (uwnew 1, a hunter with a plan, a mem line)',
          f is not None and f[0] == 'uwnew 1' and planned and any(l.startswith('mem ') for l in f),
          'file %s; hunter lines %s; AI lines:\n%s' % (f is not None, hl, '\n'.join(lines[-8:])))
    if not planned:
        return
    # (B) load it: the menu's Journey Onward lists slot I first
    src = home
    def load_run(name, edit=None, frm=None, extra='', exit_ms=30000):
        h = os.path.join(WORK, 'h-' + name)
        shutil.rmtree(h, ignore_errors=True)
        shutil.copytree(frm or src, h)
        if edit:
            pth = os.path.join(h, 'SAVE1', 'UWNEW.DAT')
            text = open(pth).read()         # (read before the 'w' open empties it)
            open(pth, 'w').write(edit(text))
        sp = os.path.join(WORK, name + '.script')
        open(sp, 'w').write(MENU + extra)
        e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', UWPORT_AILOG='1',
                 UWPORT_HUNTER_PLAN='goto_last,wait,search', UWPORT_FOCUSTEST='1000,1500')
        r = subprocess.run([EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', h, '--no-audio',
                            '--input-script', sp, '--exit-after', str(exit_ms)], env=e, capture_output=True, text=True, errors='replace')
        open(os.path.join(WORK, name + '.out'), 'w').write(r.stdout)
        return [l for l in r.stdout.splitlines() if l.startswith('AI: hunter')]
    lb = load_run('saveB')
    check('save B: the load resumes the plan', any('resumed: step' in l for l in lb), '\n'.join(lb[:8]))
    # (B2) what the hunter had noticed is saved with it ("seen L MOB t" lines) and not noticed again after the load:
    # no "noticed ... at X,Y" line in B for a saved ledger entry whose t the hunter's seen lines hold
    mob = planned[0].split()[2]
    seen_t = {l.split()[3] for l in f if l.startswith('seen 1 %s ' % mob)}
    old = {(l.split()[4], l.split()[5]) for l in f if l.startswith('ledger 1 ') and l.split()[3] in seen_t}
    again = [l for l in lb if l.startswith('AI: hunter #%s ' % mob) and ': noticed ' in l
             and any(l.endswith(' at %s,%s' % p) for p in old)]
    check('save B2: the hunter\'s noticed entries saved (seen lines) and not noticed again after the load',
          bool(seen_t) and bool(old) and not again, 'seen %s, entries %s, noticed again:\n%s' % (sorted(seen_t), sorted(old), '\n'.join(again)))
    def other_item(text):
        out = []
        for l in text.splitlines():
            q = l.split()
            if q and q[0] == 'hunter' and q[2] == planned[0].split()[2]:
                q[3] = str(int(q[3]) ^ 1)
                l = ' '.join(q)
            out.append(l)
        return '\n'.join(out) + '\n'
    lc = load_run('saveC', other_item)
    check('save C: a record whose mobile holds another creature is dropped', any('record dropped' in l for l in lc), '\n'.join(lc[:8]))
    # (D) A's save loaded, the player taken to level 2 and saved there (slot I again): the level 1 record is kept,
    # does nothing while the player is on level 2 (D2a: no line of the goblin's), and runs again once the player is
    # back on level 1 (D2b: a line of the goblin's after the goto)
    t2 = level_tiles(2)
    x2, y2 = min((x, y) for (x, y), ty in t2.items() if ty == 1 and all(t2.get((x + dx, y + dy), 0) == 1 for dx in (-1, 0, 1) for dy in (-1, 0, 1)))
    load_run('saveD1', extra="16000 cheat goto 2 %d %d\n20000 key f5\n21500 key enter\n" % (x2, y2), exit_ms=25000)
    hd = os.path.join(WORK, 'h-saveD1')
    fd = side(hd, 1) or []
    kept = [l for l in fd if l.startswith('hunter 1 218 ') and l.split()[7] != '0']
    check('save D1: saved on level 2, the level 1 hunter kept with its plan', bool(kept), '\n'.join(fd[:6]))
    if not kept:
        return
    goblin = lambda ls: [l for l in ls if l.startswith('AI: hunter #218 (')]
    la = load_run('saveD2a', frm=hd, exit_ms=25000)
    check('save D2a: loaded on level 2: resumed (level 1) and nothing more from the goblin',
          any('resumed' in l and '(level 1)' in l for l in la) and not goblin(la), '\n'.join(la[:8]))
    lb2 = load_run('saveD2b', frm=hd, extra="17000 cheat goto 1 9 36\n", exit_ms=40000)
    check('save D2b: back on level 1, the goblin acts again', bool(goblin(lb2)), '\n'.join(lb2[:8]))

STEPS = ('goto_last', 'goto_door', 'goto_home', 'goto_water', 'wait', 'search', 'fetch_kin', 'follow_light', 'recover', 'give_up')

def test_ask():
    """Task 6 (the model ON: UWPORT_LLM=1, no fixed plan; not in the default set, it loads the language model):
    the goblin hears the knob's noise, asks step 1, each answer is a word it was offered, the next step is asked with
    "So far", and a plan begins whose words are all in the vocabulary"""
    home = os.path.join(WORK, 'h-ask')
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f: f.write('settings-at-start=0\n')
    sp = os.path.join(WORK, 'ask.script')
    open(sp, 'w').write(MENU + "15000 cheat goto 1 10 32\n15500 cheat anger 218\n")
    e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='1', UWPORT_AILOG='1',
             UWPORT_FOCUSTEST='1000,1500', UWPORT_HUNTER_NOISE='%d,%d' % HUNTER)
    e.pop('UWPORT_HUNTER_PLAN', None)
    r = subprocess.run([EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio',
                        '--input-script', sp, '--exit-after', '75000'], env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, 'ask.out'), 'w').write(r.stdout)
    lines = [l for l in r.stdout.splitlines() if l.startswith('AI: hunter #218')]
    asks = [l for l in lines if ': asks: ' in l]
    answers = [l.split('"')[1].replace('goal=', '') for l in lines if 'the model answers' in l]
    plans = [l for l in lines if ': plan: ' in l]
    offered_ok = all(a in asks[i].split('Choices: ')[1] for i, a in enumerate(answers) if i < len(asks))
    plan_ok = bool(plans) and all(w.strip() in STEPS for w in plans[0].split(': plan: ')[1].split(','))
    sofar_ok = len(asks) < 2 or 'So far: step 1 = ' in asks[1]
    check('ask: asked, answered with offered words, "So far" on step 2, a plan of known words',
          bool(asks) and bool(answers) and offered_ok and plan_ok and sofar_ok,
          'asks %d answers %s plans %s\n%s' % (len(asks), answers, plans[:1], '\n'.join(lines[:10])))

def creatures(level):
    """the tiles holding a mobile creature (object index < 256, item 0x40-0x7E), from the level text's tile chains
    (tile ... first_object, object ... next)"""
    first, nxt, item = {}, {}, {}
    for line in open(os.path.join(ROOT, 'data', 'levels', 'level%d.txt' % level)):
        p = line.split()
        if p and p[0] == 'tile' and int(p[10]): first[(int(p[1]), int(p[2]))] = int(p[10])
        elif p and p[0] == 'object': item[int(p[1])] = int(p[2]); nxt[int(p[1])] = int(p[10])
    out = []
    for xy, o in first.items():
        n = 0
        while o and n < 200:
            if o < 256 and 0x40 <= item.get(o, 0) < 0x7F: out.append(xy)
            o, n = nxt.get(o, 0), n + 1
    return out

def walker_cells(lev_ark, data_dir, level):
    """the original walker's view of a level, from a LEV.ARK (the test save's: the level as the game loads it) and
    DATA's TERRAIN.DAT / COMOBJ.DAT, written apart from hunter.c's grid as the reference for the routes check:
    {(x, y): (height, terrain class, onobj, tile type)} for every open tile. underworld-exhumed critter\\PATHFIND.C
    hyp_move: the first solid object on the tile (COMOBJ byte 3 bit 1) whose top, (z + height) >> 3, is above the floor
    is stood on (onobj: the terrain not tested); a floor's class is TERRAIN.DAT's word at 0x200 + 2 * texture, shifted
    4 (1 water, 2 lava); the level's floor textures are LEV.ARK block 0x12 + level - 1's words 48-57; the tile type
    (the tile word's low 4 bits: 2-5 the diagonals, 6-9 the slopes N, S, E, W) for the walls and the slope rule"""
    import struct
    ark = open(lev_ark, 'rb').read()
    terr = open(os.path.join(data_dir, 'TERRAIN.DAT'), 'rb').read()
    com = open(os.path.join(data_dir, 'COMOBJ.DAT'), 'rb').read()
    n = struct.unpack_from('<H', ark, 0)[0]
    offs = struct.unpack_from('<%dI' % n, ark, 2)
    o = offs[level - 1]
    fl = struct.unpack_from('<10H', ark, offs[0x11 + level] + 96)
    fcls = [struct.unpack_from('<H', terr, 0x200 + f * 2)[0] >> 4 for f in fl]
    def obj(i):
        p = o + 0x4000 + (i * 27 if i < 256 else 256 * 27 + (i - 256) * 8)
        w = struct.unpack_from('<4H', ark, p)
        return w[0] & 0x1FF, w[1] & 0x7F, w[2] >> 6
    cells = {}
    for y in range(64):
        for x in range(64):
            w0, w1 = struct.unpack_from('<HH', ark, o + (y * 64 + x) * 4)
            if w0 & 15 == 0: continue
            h, i, objh, k = (w0 >> 4) & 15, w1 >> 6, 0, 0
            while i and objh == 0 and k < 200:
                it, z, nxt = obj(i)
                if (com[2 + it * 11 + 3] >> 1) & 1: objh = (z + com[2 + it * 11]) >> 3
                i, k = nxt, k + 1
            cells[(x, y)] = (max(h, objh), fcls[(w0 >> 10) & 15], objh > h, w0 & 15)
    return cells

def walker_flood(cells, start, mode='walk'):
    """the least danger of a route to every cell reached (PATHFIND.C hyp_move with init_ai's tables): 'walk' CT1, water
    forbidden unless onobj, lava 2 unless onobj, a climb of more than one refused, a drop of d costing d - 1; 'swim'
    CT4, water or onobj only, the same heights; 'fly' CT2, any open tile, no heights, no danger: {(x, y): danger}.
    Every mover: a step is refused when either tile has a wall on the side facing the other (TRIGGER.C tile_walls by
    tile type: a diagonal's two half walls; hyp_move's first tests). Walkers and swimmers: a slope entered from any
    side but the one it rises toward stands one unit higher (hyp_move's h23++ when slope_for_dir[path_turns[dx + 1]
    [dy + 1]] != type3)"""
    import heapq
    TW = [30, 0, 19, 21, 11, 13, 32, 32, 32, 32, 0, 0, 0, 0, 0, 30]   # TRIGGER.C tile_walls: 2 W, 4 E, 8 S, 16 N
    RISE = {6: (0, 1), 7: (0, -1), 8: (1, 0), 9: (-1, 0)}            # map.h TILE_SLOPE_N, S, E, W: the way each rises
    best, pq = {start: 0}, [(0, start)]
    while pq:
        d, c = heapq.heappop(pq)
        if d > best[c]: continue
        hc = cells[c][0]
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nc = (c[0] + dx, c[1] + dy)
            if nc not in cells: continue
            h, cls, onobj, t3 = cells[nc]
            w2, w3 = TW[cells[c][3]], TW[t3]
            if dx > 0 and ((w3 & 2) or (w2 & 4)): continue
            if dx < 0 and ((w3 & 4) or (w2 & 2)): continue
            if dy > 0 and ((w3 & 8) or (w2 & 16)): continue
            if dy < 0 and ((w3 & 16) or (w2 & 8)): continue
            if mode == 'fly':
                if 0 < best.get(nc, 1 << 30): best[nc] = 0; heapq.heappush(pq, (0, nc))
                continue
            if mode == 'swim' and not onobj and cls != 1: continue
            if t3 in RISE and RISE[t3] != (dx, dy): h += 1
            if h > hc + 1: continue
            nd = d + (hc - h - 1 if hc > h + 1 else 0)
            if not onobj and mode == 'walk':
                if cls == 1: continue
                if cls == 2: nd += 2
            if nd < best.get(nc, 1 << 30): best[nc] = nd; heapq.heappush(pq, (nd, nc))
    return best

def walker_danger(cells, start, goal, mode='walk'):
    """the least danger of a route from start to goal (walker_flood's modes), None when there is none"""
    if start not in cells or goal not in cells: return None
    return walker_flood(cells, start, mode).get(goal)

def line_mode(l):
    """the mover a route line names: ' (a swimmer)' / ' (a flier)' (hunter.c's routes_log), else a walker"""
    return 'swim' if '(a swimmer)' in l else 'fly' if '(a flier)' in l else 'walk'

def open_square(level, cells):
    """a dry open tile (not water, not lava, nothing stood on) whose eight neighbours are the same, 6 squares or more
    from every creature where there is one (a fight could end the run), nearest the level's middle (the level is
    entered whether or not a creature steps: hunter.c's port_hunter_tick)"""
    t = level_tiles(level)
    dry = lambda p: t.get(p, 0) == 1 and p in cells and cells[p][1] == 0 and not cells[p][2]
    ok = [(x, y) for (x, y) in t if all(dry((x + dx, y + dy)) for dx in (-1, 0, 1) for dy in (-1, 0, 1))]
    cr = creatures(level)
    far = [p for p in ok if all(max(abs(p[0] - c[0]), abs(p[1] - c[1])) >= 6 for c in cr)]   # (no creature needed near:
    return min(far or ok, key=lambda p: (p[0] - 32) ** 2 + (p[1] - 32) ** 2)                 # a fight could end the run)

def routes_run(home, name, gotos):
    """one off-screen run of the test exe: the menu's load, then each (level, x, y) in turn by cheat goto, 4 s apart
    (the route lines come at the level's first pass: port_hunter_tick); the 'routes: ' lines it printed
    (UWPORT_HUNTER_ROUTES=1); its stdout and stderr kept in work\\hunters\\<name>.out / .err"""
    script, t = MENU, 15000
    for lv, x, y in gotos:
        script += '%d cheat goto %d %d %d\n' % (t, lv, x, y)
        t += 4000
    sp = os.path.join(WORK, name + '.script')
    open(sp, 'w').write(script)
    e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='0', UWPORT_HUNTER_PLAN='wait',
             UWPORT_HUNTER_ROUTES=os.environ.get('UWPORT_HUNTER_ROUTES', '1'), UWPORT_FOCUSTEST='1000,1500')
    r = subprocess.run([EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio',
                        '--input-script', sp, '--exit-after', str(t + 3000)], env=e, capture_output=True, text=True, errors='replace')
    open(os.path.join(WORK, name + '.out'), 'w').write(r.stdout)
    open(os.path.join(WORK, name + '.err'), 'w').write(r.stderr)
    return [l for l in r.stdout.splitlines() if l.startswith('routes: ')]

ROUTE_RE = r'routes: level (\d+) hunter #(\d+) \(.*?\) at (\d+),(\d+) to the player at (\d+),(\d+): (.*)'

def routes_judge(lines, cells):
    """each route line printed with the reference's verdict; the lines whose verdict differs from the reference's (a
    route exactly when the least danger is within the hunter's range). Swimmers and fliers: listed, not judged (hnav
    has the original walker's rule alone)"""
    bad = []
    for l in lines:
        m = re.match(ROUTE_RE, l)
        if not m:
            print('  ' + l)
            continue
        lv, hx, hy, px, py, res = int(m.group(1)), int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6)), m.group(7)
        rg = int(re.search(r'(?:of|range) (\d+)', res).group(1))
        ref = walker_danger(cells[lv], (hx, hy), (px, py), line_mode(l))
        want, got = ref is not None and ref <= rg, 'UNREACHABLE' not in res
        print('  %s   [reference: %s]' % (l, 'no route' if ref is None else 'least danger %d' % ref))
        if got != want: bad.append('%s  -- the reference says %s' % (l, 'reachable' if want else 'unreachable'))
    return bad

def test_routes():
    """Task 7, two runs. Run 1: the player taken to a dry square of each of the nine levels in turn (cheat goto); at
    each level entered hunter.c logs (UWPORT_HUNTER_ROUTES=1) every hunter's route over the game's own grid to the
    player, or UNREACHABLE, with the hunter's danger range (AI.C acceptable_danger). Run 2: on each level the player
    put on a dry square the first judged hunter of run 1 can reach by the reference (the farthest of those whose route
    needs the most danger within its range: lava and drops walked), so every level with a hunter has a reachable case.
    The checks: every level logged in run 1 (port_hunter_tick enters a level whether or not a creature steps); every
    walker's verdict in both runs the reference's (walker_cells / walker_danger, the original walker's rule from the
    level file); run 2's chosen hunter reachable on every level that has one."""
    home = os.path.join(WORK, 'h-routes')
    shutil.rmtree(home, ignore_errors=True)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f: f.write('settings-at-start=0\n')
    lev, data = os.path.join(home, 'SAVE1', 'LEV.ARK'), os.path.join(ROOT, 'dist', 'UW1', 'game', 'DATA')
    cells = {lv: walker_cells(lev, data, lv) for lv in range(1, 10)}
    print('  run 1: a dry square on each level')
    lines = routes_run(home, 'routes', [(lv,) + open_square(lv, cells[lv]) for lv in range(1, 10)])
    seen = {int(l.split()[2].rstrip(':')) for l in lines}
    bad = routes_judge(lines, cells)
    for lv in range(1, 10):
        if lv not in seen: print('  routes: level %d: not entered' % lv)
    check('routes: levels logged %s of 9' % len(seen), len(seen) == 9, 'missing %s' % [lv for lv in range(1, 10) if lv not in seen])
    # run 2's squares: for each level, its first judged hunter, a dry square within its range
    t1, gotos, chosen = level_tiles, [], {}
    for l in lines:
        m = re.match(ROUTE_RE, l)
        if not m or 'swimmer' in l: continue        # (the player stands dry: a swimmer never reaches him there)
        lv, mob, hx, hy = int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))
        if lv in chosen or (hx, hy) not in cells[lv]: continue
        rg = int(re.search(r'(?:of|range) (\d+)', m.group(7)).group(1))
        t, c = t1(lv), cells[lv]
        dry = lambda p: t.get(p, 0) == 1 and p in c and c[p][1] == 0 and not c[p][2]
        reach = walker_flood(c, (hx, hy), line_mode(l))
        cand = [(d, max(abs(p[0] - hx), abs(p[1] - hy)), p) for p, d in reach.items() if d <= rg and dry(p)
                and all(dry((p[0] + dx, p[1] + dy)) for dx in (-1, 0, 1) for dy in (-1, 0, 1))]
        if not cand: continue
        # away from every creature where it can be (6 squares or more): a player stood among them can be killed before
        # the next goto, and the levels after it are then never entered (run 2 of 11-13 lost levels 4-8 so)
        cr = creatures(lv)
        safe = [q for q in cand if all(max(abs(q[2][0] - k[0]), abs(q[2][1] - k[1])) >= 6 for k in cr)]
        d, dist, p = max(safe or cand)
        chosen[lv] = (mob, d)
        gotos.append((lv,) + p)
    print('  run 2: a square each chosen hunter can reach (%s)' % ', '.join('level %d #%d danger %d' % ((lv,) + chosen[lv]) for lv in sorted(chosen)))
    shutil.rmtree(home, ignore_errors=True)         # (run 1's level changes wrote SAVE0: run 2 starts from the same home)
    shutil.copytree(HOME0, home)
    with open(os.path.join(home, 'uw1port.cfg'), 'a') as f: f.write('settings-at-start=0\n')
    lines2 = routes_run(home, 'routes2', gotos)
    bad += routes_judge(lines2, cells)
    missed = []
    for lv, (mob, d) in sorted(chosen.items()):
        got = [l for l in lines2 if l.startswith('routes: level %d hunter #%d ' % (lv, mob))]
        if not got or 'UNREACHABLE' in got[-1]: missed.append('level %d hunter #%d: %s' % (lv, mob, got[-1] if got else 'not logged'))
    check("routes: every walker's verdict the original rule's (the reference from the level file)", not bad, '\n'.join(bad))
    check('routes: run 2, each chosen hunter reaches the player (%d levels)' % len(chosen), bool(chosen) and not missed, '\n'.join(missed))

QUIET = MENU + "15000 cheat goto 1 10 32\n15500 cheat anger 218\n"   # the player near, walls between (save A's)

def test_react():
    """reconsidering (the user, 11-13: rush in when the moment calls for it), no model: a fixed plan wait,search and
    UWPORT_HUNTER_REACT=rush_in; the knob's noise begins the plan, UWPORT_HUNTER_NOISE2's weapon swing 20 s later is
    the new fact while it waits: "changes its plan: rush in", then the rush plan goto_last, search"""
    os.environ.update(UWPORT_HUNTER_REACT='rush_in', UWPORT_HUNTER_NOISE='%d,%d' % HUNTER, UWPORT_HUNTER_NOISE2='%d,%d' % (HUNTER[0] - 2, HUNTER[1]))
    try:
        lines = run('react', QUIET, 80000, 'wait,search')
    finally:
        for k in ('UWPORT_HUNTER_REACT', 'UWPORT_HUNTER_NOISE', 'UWPORT_HUNTER_NOISE2'): del os.environ[k]
    check('react: a new fact mid-plan: rush in, the rush plan begun',
          in_order(lines, 'plan: wait, search', 'noticed the adventurer swing', 'changes its plan: rush in', 'plan: goto_last, search'),
          '\n'.join(lines[:14]))

def test_react_ask():
    """the same with the model on (UWPORT_LLM=1, no fixed plan): the model's plan, then the second noise noticed
    mid-plan asks "reconsiders:", answered keep_plan / rush_in / new_plan, and the hunter acts on it"""
    # up to three tries: the model's loading races the first steps (the first noise can come late, with the second)
    for attempt in range(3):
        home = os.path.join(WORK, 'h-react_ask')
        shutil.rmtree(home, ignore_errors=True)
        shutil.copytree(HOME0, home)
        with open(os.path.join(home, 'uw1port.cfg'), 'a') as f: f.write('settings-at-start=0\n')
        sp = os.path.join(WORK, 'react_ask.script')
        open(sp, 'w').write(QUIET)
        e = dict(os.environ, UWPORT_OFFSCREEN='1', EXHUME_TEST_CHEATS='1', UWPORT_LLM='1', UWPORT_AILOG='1',
                 UWPORT_FOCUSTEST='1000,1500', UWPORT_HUNTER_NOISE='%d,%d' % HUNTER, UWPORT_HUNTER_NOISE2='%d,%d' % (HUNTER[0] - 2, HUNTER[1]))
        e.pop('UWPORT_HUNTER_PLAN', None)
        r = subprocess.run([EXE, '--data', os.path.join(ROOT, 'dist', 'UW1', 'game'), '--home', home, '--no-audio',
                            '--input-script', sp, '--exit-after', '80000'], env=e, capture_output=True, text=True, errors='replace')
        open(os.path.join(WORK, 'react_ask.out'), 'w').write(r.stdout)
        lines = [l for l in r.stdout.splitlines() if l.startswith('AI: hunter #')]
        words = [l.split('"')[1].replace('goal=', '') for l in lines if 'the model answers' in l]
        acted = any(s in l for l in lines for s in ('keeps its plan', 'changes its plan'))
        ok = any('reconsiders:' in l for l in lines) and any(w in ('keep_plan', 'rush_in', 'new_plan') for w in words) and acted
        if ok: break
        print('react_ask: try %d: no reconsidering (%s)' % (attempt + 1, lines[-1][:120] if lines else 'no lines'))
    print('react_ask: the model chose %s' % [w for w in words if w in ('keep_plan', 'rush_in', 'new_plan')])
    check('react_ask: reconsiders mid-plan, answered with one of its three words, acted on', ok,
          '\n'.join(l[:200] for l in lines[:16]))

if __name__ == '__main__':
    os.makedirs(WORK, exist_ok=True)
    which = sys.argv[1:] or ['plan1', 'plan_giveup', 'plan_blocked', 'save']
    if 'plan1' in which: test_plan1()
    if 'plan_giveup' in which: test_plan_giveup()
    if 'plan_blocked' in which: test_plan_blocked()
    if 'save' in which: test_save()
    if 'ask' in which: test_ask()
    if 'routes' in which: test_routes()
    if 'react' in which: test_react()
    if 'react_ask' in which: test_react_ask()
    print('huntercheck: %d failures' % fails)
    sys.exit(fails)
