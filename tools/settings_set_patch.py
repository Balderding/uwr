"""One-off: main.c settings lines applied by one function (apply_setting), and a --set=key=value flag for
the headless checks (tries a setting without touching the user's settings.txt)."""
import sys

p = sys.argv[1] + '/src/main.c'
s = open(p, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'


def rep(a, b):
    global s
    a, b = a.replace('\n', nl), b.replace('\n', nl)
    assert s.count(a) == 1, a[:70]
    s = s.replace(a, b)


# the chain of key tests becomes apply_setting()
start = s.index('            if (!strcmp(key, "width")) s->width = (int)v;')
end = s.index('            else if (!strcmp(key, "exposure")) s->exposure = v;') + len('            else if (!strcmp(key, "exposure")) s->exposure = v;')
chain = s[start:end]
body = nl.join(line[8:] if line.startswith('        ') else line for line in chain.split(nl))
func = ('/* one "key value" setting into s (unknown keys are left alone) */' + nl +
        'static void apply_setting(Settings *s, const char *key, const char *val) {' + nl +
        '    float v = (float)atof(val);' + nl + body + nl + '}' + nl + nl)
s = s[:start] + '            apply_setting(s, key, val);' + s[end:]
rep('''            float v = (float)atof(val);
            for (int i = 0; i < COUNT(SETTING); i++)
                if (!strcmp(key, SETTING[i].key)) have[i] = true;
            apply_setting(s, key, val);''',
    '''            for (int i = 0; i < COUNT(SETTING); i++)
                if (!strcmp(key, SETTING[i].key)) have[i] = true;
            apply_setting(s, key, val);''')
at = s.index('static void read_settings(const char *path, Settings *s) {')
s = s[:at] + func + s[at:]

# --set=key=value (several allowed)
rep('    const char *choose_arg = NULL;',
    '    const char *choose_arg = NULL;\n    const char *set_args[16];\n    int nset = 0;')
rep('        else if (starts(a, "--choose=")) choose_arg = a + 9;     /* its answers: 1,2,... */',
    '        else if (starts(a, "--choose=")) choose_arg = a + 9;     /* its answers: 1,2,... */\n'
    '        else if (starts(a, "--set=") && nset < 16) set_args[nset++] = a + 6; /* a setting just for this run: key=value */')
rep('    if (torch_arg >= 0) s.torch = torch_arg;',
    '    if (torch_arg >= 0) s.torch = torch_arg;\n'
    '    for (int i = 0; i < nset; i++) {\n'
    '        char key[64];\n'
    '        const char *eq = strchr(set_args[i], \'=\');\n'
    '        if (!eq || eq - set_args[i] >= 63) continue;\n'
    '        memcpy(key, set_args[i], (size_t)(eq - set_args[i]));\n'
    '        key[eq - set_args[i]] = 0;\n'
    '        apply_setting(&s, key, eq + 1);\n'
    '    }')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
