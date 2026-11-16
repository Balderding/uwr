"""One-off: extract.c writes data\\objects\\objects.txt (COMOBJ.DAT + the armour and container tables of
OBJECTS.DAT), the table objdata.c reads."""
import sys

p = sys.argv[1] + '/src/extract.c'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'

CODE = r'''    /* every kind of object: COMOBJ.DAT (11 bytes each from offset 2) and OBJECTS.DAT's armour (0xb2: 32 x 4
     * for 0x20-0x3F) and container (0xd32: 16 x 3 for 0x80-0x8F) tables -> data\objects\objects.txt */
    snprintf(path, sizeof path, "%s\\objects\\objects.txt", data_dir);
    if (!file_exists(path)) {
        char src[700];
        snprintf(src, sizeof src, "%s\\DATA\\COMOBJ.DAT", game_dir);
        mark = arena_mark();
        Bytes c = file_read(src);
        snprintf(src, sizeof src, "%s\\DATA\\OBJECTS.DAT", game_dir);
        Bytes od = file_read(src);
        snprintf(src, sizeof src, "%s\\strings\\block0004.txt", data_dir);
        Bytes names = file_read(src);
        if (c.size >= 2 + 512 * 11 && od.size >= 0xd32 + 48) {
            size_t cap = 1 << 18, n = 0;
            char *text = arena_alloc(cap);
            n += (size_t)snprintf(text + n, cap - n,
                "# uo-new objects: what the original knows about each kind (from COMOBJ.DAT and OBJECTS.DAT). The engine\r\n"
                "# reads this file; edit freely; delete to extract it again.\r\n"
                "#   obj N  pick (can be picked up) stack (0 no, else how it stacks) mass (tenths of a stone) value (gold)\r\n"
                "#          height radius  qclass (3: always the last quality word) qtype (quality words: strings block 5\r\n"
                "#          from qtype * 6)  look (1: \"You see ...\", 0: a decoration with its own text) owner (can belong\r\n"
                "#          to someone)   # raw COMOBJ bytes, name\r\n"
                "#   armour N  slot (0 helm, 1 chest, 2 gloves, 3 leggings, 4 boots, 9 ring, -1 a shield: a hand)\r\n"
                "#          protection durability\r\n"
                "#   container N  capacity (tenths of a stone, 0 no limit) accepts (-1 anything, an object, 512 runes,\r\n"
                "#          513 ammunition, 514 maps and scrolls, 515 food)\r\n");
            for (int k = 0; k < 512; k++) {
                const u8 *e = c.data + 2 + k * 11;
                u32 w = rd16(e + 1);
                char name[80] = "";
                /* the name: "k: text" in block 4 */
                char key[12];
                int kl = snprintf(key, sizeof key, "\n%d: ", k);
                for (size_t i = 0; i + (size_t)kl <= names.size; i++)
                    if (!memcmp(names.data + i, key, (size_t)kl)) {
                        size_t j = i + (size_t)kl, m = 0;
                        while (j < names.size && names.data[j] != '\r' && names.data[j] != '\n' && m < sizeof name - 1) name[m++] = (char)names.data[j++];
                        name[m] = 0;
                        break;
                    }
                n += (size_t)snprintf(text + n, cap - n,
                                      "obj %3d  pick %d stack %d mass %4u value %4u height %3d radius %d qclass %d qtype %2d look %d owner %d"
                                      "   # %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x  %s\r\n",
                                      k, (e[3] >> 5) & 1, (e[3] >> 6) & 3, (w >> 4) & 0xFFF, rd16(e + 4), e[0], w & 7, (e[6] >> 2) & 3,
                                      e[10] & 15, (e[10] >> 4) & 1, (e[7] >> 7) & 1, e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7],
                                      e[8], e[9], e[10], name);
            }
            static const int SLOT[10] = {-1, 1, -1, 3, 2, 4, -1, -1, 0, 9}; /* OBJECTS.DAT's body part -> our slot */
            for (int k = 0; k < 32; k++) {
                const u8 *e = od.data + 0xb2 + k * 4;
                if (!e[0] && !e[3] && e[1] == 0xff) continue;
                n += (size_t)snprintf(text + n, cap - n, "armour %3d  slot %d protection %d durability %d   # raw %02x %02x %02x %02x\r\n",
                                      0x20 + k, e[3] < 10 ? SLOT[e[3]] : -1, e[0], e[1], e[0], e[1], e[2], e[3]);
            }
            for (int k = 0; k < 16; k++) {
                const u8 *e = od.data + 0xd32 + k * 3;
                n += (size_t)snprintf(text + n, cap - n, "container %3d  capacity %d accepts %d\r\n", 0x80 + k, e[0], rd16s(e + 1));
            }
            make_dirs_for(path);
            if (file_write(path, text, n)) total++;
        }
        arena_reset(mark);
    }
'''

anchor = '    return total;' + nl + '}' + nl + nl + 'int extract_missing('
assert s.count(anchor) == 1
s = s.replace(anchor, CODE.replace('\n', nl) + anchor)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
