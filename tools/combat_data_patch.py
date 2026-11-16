"""One-off: objects.txt v5 with the weapons' power bytes and the critter table (UW.EXE's combat decoded 2026-11-08)."""
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


patch('src/extract.c', [
    ('"# uo-new objects v4: what the original knows', '"# uo-new objects v5: what the original knows'),
    ('!file_starts_with(path, "# uo-new objects v4")', '!file_starts_with(path, "# uo-new objects v5")'),
    ('                "#          durability (255 never breaks)   # raw: the 8 bytes (3-5 not decoded yet)\\r\\n"',
     '                "#          durability (255 never breaks) power_min rate power_max (the swing: power from min to max\\r\\n"\n'
     '                "#          as the charge goes 0-100, the charge rising by rate every 1/16 s; damage x power / 128)\\r\\n"\n'
     '                "#   critter N  hp armour (body arms legs head; 255 none) attack (skill) defense strength\\r\\n"\n'
     '                "#          a1 a2 a3 (each: bonus damage chance%) xp (experience value: a kill gives 5-6 x it)\\r\\n"'),
    ('                n += (size_t)snprintf(text + n, cap - n, "weapon %3d  slash %d bash %d stab %d skill %d durability %d   # raw %02x %02x %02x %02x %02x %02x %02x %02x\\r\\n",\n'
     '                                      k, e[0], e[1], e[2], e[6], e[7], e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);',
     '                n += (size_t)snprintf(text + n, cap - n, "weapon %3d  slash %d bash %d stab %d skill %d durability %d power_min %d rate %d power_max %d\\r\\n",\n'
     '                                      k, e[0], e[1], e[2], e[6], e[7], e[3], e[4], e[5]);'),
    ('            for (int k = 8; k < 16; k++) { /* OBJECTS.DAT 0x82: 0x18-0x1F launchers, byte 2 = the ammunition */',
     '''            for (int k = 0; k < 64; k++) { /* OBJECTS.DAT 0x132: 64 critters x 48 bytes (0x40-0x7F) */
                const u8 *e = od.data + 0x132 + k * 48;
                if (!e[4]) continue;
                n += (size_t)snprintf(text + n, cap - n,
                                      "critter %3d  hp %d armour %d %d %d %d attack %d defense %d strength %d a1 %d %d %d a2 %d %d %d a3 %d %d %d xp %d\\r\\n",
                                      0x40 + k, e[4], e[0], e[1], e[2], e[3], e[0x11], e[0x12], e[5], (int)(i8)e[0x13], e[0x14], e[0x15],
                                      (int)(i8)e[0x16], e[0x17], e[0x18], (int)(i8)e[0x19], e[0x1a], e[0x1b], (int)rd16(e + 0x28));
            }
            for (int k = 8; k < 16; k++) { /* OBJECTS.DAT 0x82: 0x18-0x1F launchers, byte 2 = the ammunition */'''),
])
patch('src/objdata.h', [
    ('    u8 slash, bash, stab, skill, wdurability;', '    u8 slash, bash, stab, skill, wdurability;\n    u8 power_min, rate, power_max; /* the swing (UW.EXE file 0x2574b, 0x257c8) */\n'
     '    /* a critter (OBJECTS.DAT 0x132, 48 bytes; UW.EXE file 0x24b68, 0x259ee) */\n'
     '    bool critter;\n    u8 hp, carmour[4], attack, defense, strength;\n    i8 a_bonus[3];\n    u8 a_damage[3], a_chance[3];\n    u16 xp;'),
])
patch('src/objdata.c', [
    ('                else if (!strcmp(key, "ammo")) o->ammo = (i16)v;',
     '''                else if (!strcmp(key, "ammo")) o->ammo = (i16)v;
                else if (!strcmp(key, "power_min")) o->power_min = (u8)v;
                else if (!strcmp(key, "rate")) o->rate = (u8)v;
                else if (!strcmp(key, "power_max")) o->power_max = (u8)v;
                else if (!strcmp(key, "hp")) o->hp = (u8)v, o->critter = true;
                else if (!strcmp(key, "attack")) o->attack = (u8)v;
                else if (!strcmp(key, "defense")) o->defense = (u8)v;
                else if (!strcmp(key, "strength")) o->strength = (u8)v;
                else if (!strcmp(key, "xp")) o->xp = (u16)v;
                else if (!strcmp(key, "armour") || !strcmp(key, "a1") || !strcmp(key, "a2") || !strcmp(key, "a3")) {
                    /* several numbers: armour b a l h; aN bonus damage chance */
                    int w[4] = {v, 0, 0, 0}, m = 0, used2 = 0;
                    if (!strcmp(key, "armour")) m = sscanf(q + k, " %i %i %i%n", &w[1], &w[2], &w[3], &used2);
                    else m = sscanf(q + k, " %i %i%n", &w[1], &w[2], &used2);
                    if (!strcmp(key, "armour") && m == 3) for (int j = 0; j < 4; j++) o->carmour[j] = (u8)w[j];
                    else if (m == 2) {
                        int a = key[1] - '1';
                        o->a_bonus[a] = (i8)w[0], o->a_damage[a] = (u8)w[1], o->a_chance[a] = (u8)w[2];
                    }
                    k += used2;
                }'''),
])
print('ok')
