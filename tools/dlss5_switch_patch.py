"""One-off: DLSS 5 has its own switch (render_set_dlss5) and availability; an RTX 20-series card (or older) does
not try DLSS 5 at all (plain DLSS only)."""
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


patch('src/render.c', [
    ('    bool dlss_ok, nr_ok, on;', '    bool dlss_ok, nr_ok, on, nr_on; /* nr_on: DLSS 5 wanted (the options) */\n    char gpu[128];'),
    ('''                                                    : R.nr_ok  ? "DLSS 5 on"''',
     '''                                                    : R.nr_ok && R.nr_on ? "DLSS 5 on"
                                                    : R.nr_ok  ? "DLSS on (DLSS 5 off)"'''),
    ('''    R.dlss_ok = true;
    if (R.cfg.dlss5) {''', '''    R.dlss_ok = true;
    /* DLSS 5 is not tried on an RTX 20-series card or older (DLSS upscaling works there; DLSS 5 is taken not to) */
    bool old_card = strstr(R.gpu, "RTX 20") || strstr(R.gpu, "GTX") || strstr(R.gpu, "Quadro RTX");
    if (old_card && R.cfg.dlss5) logf_("render: %s: DLSS 5 is not tried on this card (DLSS only)\\n", R.gpu);
    if (R.cfg.dlss5 && !old_card) {'''),
    ('''            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, gpu, sizeof gpu, NULL, NULL);''',
     '''            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, gpu, sizeof gpu, NULL, NULL);
            memcpy(R.gpu, gpu, sizeof R.gpu);'''),
    ('''            if (R.nr_ok) {
                barrier(R.nr_out''', '''            if (R.nr_ok && R.nr_on) {
                barrier(R.nr_out'''),
    ('''bool render_set_dlss(bool on) {''', '''bool render_dlss_available(void) { return R.dlss_ok; }
bool render_dlss5_available(void) { return R.nr_ok; }
bool render_dlss5_on(void) { return R.nr_ok && R.nr_on; }
bool render_set_dlss5(bool on) {
    R.nr_on = on;
    R.have_prev = false;
    set_dlss_text();
    return R.nr_ok && R.nr_on;
}
bool render_set_dlss(bool on) {'''),
    ('''    R.on = R.dlss_ok;''', '''    R.on = R.dlss_ok, R.nr_on = true;'''),
])
patch('src/render.h', [
    ('bool render_set_dlss(bool on);', 'bool render_set_dlss(bool on);\n'
     '/* DLSS / DLSS 5 started on this card (false: the options do not offer it); DLSS 5 on / off by itself */\n'
     'bool render_dlss_available(void);\nbool render_dlss5_available(void);\nbool render_dlss5_on(void);\nbool render_set_dlss5(bool on);'),
])
print('ok')
