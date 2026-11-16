"""One-off: locks and keys as UW.EXE has them (file 0x36b55, 0x3831b): a lock (0x10F) is locked when its flags bit 0
(item word bit 9) is set; a key (0x100, 0x102-0x10E) fits when its owner field = the lock's link & 0x1ff."""
import sys

ROOT = sys.argv[1]


def patch(p, reps):
    s = open(ROOT + '/' + p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in s else '\n'
    for a, b in reps:
        a, b = a.replace('\n', nl), b.replace('\n', nl)
        assert s.count(a) == 1, (p, a[:70], s.count(a))
        s = s.replace(a, b)
    open(ROOT + '/' + p, 'w', encoding='utf-8', newline='').write(s)


patch('src/game.c', [
    ('''static bool locked(const Door *d) {
    int l = G.lv->obj[d->obj].link;
    return l > 0 && l < 1024 && G.lv->obj[l].item == 0x10F && !(G.lv->obj[l].flags & 1);
}''', '''/* the door's lock: the lock object (0x10F) its link leads to, or 0. UW.EXE (file 0x3831b): locked when the
 * lock's item word bit 9 (our flags bit 0) is set; its number is its link & 0x1ff */
static int door_lock(const Door *d) {
    int l = G.lv->obj[d->obj].link;
    return l > 0 && l < 1024 && G.lv->obj[l].item == 0x10F ? l : 0;
}
static bool locked(const Door *d) {
    int l = door_lock(d);
    return l && (G.lv->obj[l].flags & 1);
}'''),
    ('''    if (G.door_target[k] < 0.5f && locked(d)) return game_message("The door is locked.");''',
     '''    if (G.door_target[k] < 0.5f && locked(d)) {
        /* a key in the pack that fits turns (the original asks for the key to be used on the door; here E on
         * the door tries the keys carried) */
        Object *lock = &G.lv->obj[door_lock(d)];
        if (inv_has_key(lock->link & 0x1ff)) {
            lock->flags &= (u8)~1;
            return game_message("%s", str_get(1, 5)); /* "The key unlocks the lock." */
        }
        return game_message("%s", str_get(9, 3)); /* "The doors are securely locked." (block 9) */
    }'''),
])
patch('src/inventory.h', [
    ('bool inv_holding(void);       /* a thing is on the mouse pointer */',
     'bool inv_holding(void);       /* a thing is on the mouse pointer */\n'
     '/* is a key with this number (its owner field) carried, anywhere in the pack or its containers */\n'
     'bool inv_has_key(int id);'),
])
patch('src/inventory.c', [
    ('bool inv_holding(void) { return I.held != 0; }',
     '''bool inv_holding(void) { return I.held != 0; }

static bool tree_has_key(int p, int id, int depth) {
    for (int guard = 0; p && guard < POOL && depth < 8; guard++, p = I.pool[p].next) {
        int item = I.pool[p].item & 511;
        if (item >= 0x100 && item <= 0x10e && item != 0x101 && (I.pool[p].owner & 0x3f) == id) return true;
        if (holds(&I.pool[p]) && tree_has_key(I.pool[p].link, id, depth + 1)) return true;
    }
    return false;
}
bool inv_has_key(int id) {
    for (int s = 0; s < INV_SLOTS; s++) {
        int p = I.slot[s];
        if (!p) continue;
        int item = I.pool[p].item & 511;
        if (item >= 0x100 && item <= 0x10e && item != 0x101 && (I.pool[p].owner & 0x3f) == id) return true;
        if (holds(&I.pool[p]) && tree_has_key(I.pool[p].link, id, 0)) return true;
    }
    return false;
}'''),
])
print('ok')
