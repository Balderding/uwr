"""proof_chargen.py: CHARGEN.C (ovr: character creation) proved beside the rules core: init_char (the starting record: fixed
values and the random portrait, sex and skills), roll_stats (a class's attributes and bonus points from SKILLS.DAT's table),
set_skills and add_to_skill, set_sklmnu (walking the class's skill entries: the options record built with pointers), the
drawing routines show_atts and show_skills (their outside calls compared) and the picture bookkeeping adr_chrpic and
move_chrpic, on random sound player records (see proof_bags.py) with SKILLS.DAT as the class table. After every call: the
return, the outside calls, rand, the level block, the player record, the creature record and every global both sides have.

Not proved, and why: drawopt, selopt, mousopt, pickopt, gen_char and strt_chargen (the question screens: they draw pictures
from the chargen art, follow the mouse and read typed keys in loops that wait for input: mouse_get_input, a stub, answers 0).

    python tools/proof_chargen.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inv_common import Inv, main_wrap, put_ptr_s, scenarios, set_pn, wfar  # noqa: E402

ROUTINES = ['init_char', 'set_sklmnu', 'show_atts', 'show_skills', 'set_skills', 'roll_stats', 'adr_chrpic', 'move_chrpic']
DAT = 0xB000  # a skill-list table in the level block's free part (the original's dat)


def main():
    game = sys.argv[1]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 300
    w = Inv(game, 99, 'chargen')
    p, rng = w.p, w.rng
    w.PTRS = dict(w.PTRS)
    w.PTRS.update({'chrbuf': False, 'chroff': True})
    w.SKIP = set(w.SKIP) | {'chrbuf', 'chroff'}
    w.gnames = [(n, s) for n, s in w.gnames if n not in ('chrbuf', 'chroff')]
    skills_dat = open(os.path.join(game, 'DATA', 'SKILLS.DAT'), 'rb').read()

    def setup(w, level, scen):
        set_pn(w)
        # the class table (strt_chargen reads SKILLS.DAT's first 0x348 bytes into stdat)
        wfar(w, 'stdat', 0, skills_dat[:0x348])

    def op(w, level, scen, k, ctx):
        kind = rng.choice(['init', 'init', 'roll', 'roll', 'skills', 'sklmnu', 'sklmnu', 'atts', 'sk', 'adr', 'move'])
        p.w('PlayerDat', 0x1e, bytes(rng.randint(0, 30) for _ in range(0x40)))
        p.w('PlayerDat', 0x64, bytes([rng.getrandbits(8)]))
        p.w('Creature', 63 * 48 + 5, bytes(rng.randint(5, 35) for _ in range(3)))
        p.w('L', 0x4000 + 27 + 8, bytes([rng.randint(150, 250)]))
        if w.trace:
            print(ctx, kind, flush=True)
        if kind == 'init':
            blank = rng.randint(0, 1)
            w.check('init_char', p.call('init_char', [('w', blank)], None), ctx + ' blank %d' % blank)
        elif kind == 'roll':
            w.check('roll_stats', p.call('roll_stats', [], None), ctx)
        elif kind == 'skills':
            start = rng.randint(0, 6)
            sk = [rng.choice([rng.randint(0, 19), 20, 20]) for _ in range(6)]
            p.w('S', 0, bytes(sk))
            w.check('set_skills', p.call('set_skills', [('w', start), ('n', 'S+0')], 'w'), ctx + ' start %d %s' % (start, sk))
        elif kind == 'sklmnu':
            # a table of skill lists for the 8 classes, 5 entries each: a count and that many skills
            table = bytearray()
            for e in range(40):
                cnt = rng.choice([0, 1, 1, 2, 3, 5])
                table.append(cnt)
                table += bytes(rng.randint(0, 19) for _ in range(cnt))
            table += bytes(8)
            p.w('L', DAT, bytes(table))
            idx = rng.randint(0, 5)
            p.w('S', 0, bytes([idx]))
            p.w('S', 8, bytes([20] * 8))
            # the options record at S+0x40: question, name, strings (to a word array in the level block), count, ...
            p.w('S', 0x40, struct.pack('<h', rng.randint(0, 3)))
            put_ptr_s(w, 0x42, 0x48, '0', far=False)
            put_ptr_s(w, 0x44, 0x50, 'L+%x' % (DAT + 0x400))
            p.w('L', DAT + 0x400, bytes(40))
            cls = rng.randint(0, 7)
            b = bytes.fromhex(p.r('PlayerDat', 0x64, 1)[0])[0]
            p.w('PlayerDat', 0x64, bytes([(b & 0xc7) | cls << 3]))
            w.check('set_sklmnu', p.call('set_sklmnu', [('n', 'S+0'), ('n', 'S+8'), ('f', 'S+40'), ('f', 'L+%x' % DAT)], 'b'),
                    ctx + ' class %d idx %d' % (cls, idx))
            p.same('idx and skills', p.r('S', 0, 16), ctx)
            p.same('strings', p.r('L', DAT + 0x400, 40), ctx)
            p.same('opt.count', (p.r('S', 0x48, 2)[0], p.r('S', 0x58, 2)[1]), ctx)  # DOS: S+40+8, host: S+40+0x18
        elif kind == 'atts':
            w.check('show_atts', p.call('show_atts', [], None), ctx)
        elif kind == 'sk':
            w.check('show_skills', p.call('show_skills', [], None), ctx)
        elif kind == 'adr':
            p.p('chrbuf', 0, 'L+%x' % rng.randint(0xC000, 0xD000))
            size = rng.randint(0, 3000)
            w.check('adr_chrpic', p.call('adr_chrpic', [('w', size)], 'f'), ctx + ' size %d' % size)
        else:
            p.p('chroff', 0, 'S+100', near=True)
            n = rng.randint(0, 26)
            p.w('S', 0x100, bytes(rng.getrandbits(8) for _ in range(0x40)))
            size = rng.randint(0, 5000)
            w.check('move_chrpic', p.call('move_chrpic', [('f', '0'), ('w', size), ('w', n)], 'w'), ctx + ' n %d size %d' % (n, size))
            p.same('chroff', p.r('S', 0x100, 0x40), ctx)

    scenarios(w, per, op, setup=setup, nobj=(2, 6))
    w.summary('chargen', ROUTINES)


if __name__ == '__main__':
    main_wrap('chargen', main)
