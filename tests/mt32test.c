/* mt32test: mt32emu.dll with the CM-32L ROMs in uo-new\roms renders a C major chord to build\mt32test.wav (the check
 * that the DLL, the ROMs and the loading the engine does all work). */
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

typedef void *ctx_t;
typedef ctx_t (*create_fn)(void *report_handler, void *instance);
typedef int (*addrom_fn)(ctx_t, const char *);
typedef int (*open_fn)(ctx_t);
typedef int (*msg_fn)(ctx_t, uint32_t);
typedef void (*render_fn)(ctx_t, int16_t *, uint32_t);
typedef uint32_t (*rate_fn)(ctx_t);

int main(void) {
    HMODULE m = LoadLibraryA("build\\mt32emu.dll");
    if (!m) return printf("no mt32emu.dll\n"), 1;
    create_fn create = (create_fn)GetProcAddress(m, "mt32emu_create_context");
    addrom_fn addrom = (addrom_fn)GetProcAddress(m, "mt32emu_add_rom_file");
    open_fn open = (open_fn)GetProcAddress(m, "mt32emu_open_synth");
    msg_fn msg = (msg_fn)GetProcAddress(m, "mt32emu_play_msg");
    render_fn render = (render_fn)GetProcAddress(m, "mt32emu_render_bit16s");
    rate_fn rate = (rate_fn)GetProcAddress(m, "mt32emu_get_actual_stereo_output_samplerate");
    ctx_t c = create(NULL, NULL);
    int r1 = addrom(c, "roms\\cm32l_ctrl_1_02.rom"), r2 = addrom(c, "roms\\cm32l_pcm.rom"), r3 = open(c);
    printf("roms %d %d open %d rate %u\n", r1, r2, r3, rate(c));
    if (r3 != 0) return 1;
    static int16_t buf[32000 * 2 * 3];
    msg(c, 0x00007fc1);                                                  /* channel 2: program 0x7f? no: program 0 */
    msg(c, 0x00000c1u);
    msg(c, 0x007f3c91), msg(c, 0x007f4091), msg(c, 0x007f4391);           /* C E G on channel 2 */
    render(c, buf, 32000 * 2);
    msg(c, 0x00003c81), msg(c, 0x00004081), msg(c, 0x00004381);
    render(c, buf + 32000 * 2 * 2, 32000);
    int peak = 0;
    for (int i = 0; i < 32000 * 2 * 3; i++) peak = buf[i] > peak ? buf[i] : -buf[i] > peak ? -buf[i] : peak;
    printf("peak %d\n", peak);
    FILE *f = fopen("build\\mt32test.wav", "wb");
    uint32_t n = sizeof buf, sr = rate(c);
    uint32_t hdr[11] = {0x46464952, 36 + n, 0x45564157, 0x20746d66, 16, 0x00020001, sr, sr * 4, 0x00100004, 0x61746164, n};
    fwrite(hdr, 4, 11, f), fwrite(buf, 1, n, f), fclose(f);
    return peak > 1000 ? 0 : 1;
}
