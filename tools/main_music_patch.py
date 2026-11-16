"""One-off patch: music settings and start/stop in main.c."""
import sys

p = sys.argv[1] + '/src/main.c'
s = open(p, encoding='utf-8').read()


def rep(a, b):
    global s
    assert s.count(a) == 1, a[:80]
    s = s.replace(a, b)


rep('    float fov, mouse, gamma, normal_strength, bounce, torch_intensity, torch_range;',
    '    float fov, mouse, gamma, normal_strength, bounce, torch_intensity, torch_range, music_volume;\n    int music;')
rep('    {"torch_range", "torch_range 6.0       # tiles the torch reaches"},',
    '    {"torch_range", "torch_range 6.0       # tiles the torch reaches"},\n'
    '    {"music", "music 1               # the game\'s music (data\\\\music\\\\playlist.txt)"},\n'
    '    {"music_volume", "music_volume 0.6"},')
rep('            else if (!strcmp(key, "torch_range")) s->torch_range = v;',
    '            else if (!strcmp(key, "torch_range")) s->torch_range = v;\n'
    '            else if (!strcmp(key, "music")) s->music = (int)v;\n'
    '            else if (!strcmp(key, "music_volume")) s->music_volume = v;')
rep('                    .torch_range = 6, .dlss = 1,', '                    .torch_range = 6, .music = 1, .music_volume = 0.6f, .dlss = 1,')
rep('#include "ui.h"', '#include "ui.h"\n#include "audio.h"')
rep('''    logf_("%s\\n", render_status());
    show_title();''', '''    logf_("%s\\n", render_status());
    if (s.music && !music_start(data_dir, s.music_volume)) logf_("no music (no MIDI device, or no songs in data\\\\music\\\\playlist.txt)\\n");
    show_title();''')
rep('''    capture(false);
    render_shutdown();
    return 0;''', '''    capture(false);
    music_stop();
    render_shutdown();
    return 0;''')
open(p, 'w', encoding='utf-8').write(s)
print('ok')
