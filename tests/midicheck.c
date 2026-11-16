/* midicheck: can uw.exe's music open Windows' MIDI output? (opens it and closes it again; plays nothing) */
#include <stdio.h>
#include <windows.h>

int main(void) {
    UINT n = midiOutGetNumDevs();
    printf("MIDI output devices: %u\n", n);
    for (UINT i = 0; i < n; i++) {
        MIDIOUTCAPSA caps;
        if (midiOutGetDevCapsA(i, &caps, sizeof caps) == MMSYSERR_NOERROR) printf("  %u: %s\n", i, caps.szPname);
    }
    HMIDIOUT h;
    MMRESULT r = midiOutOpen(&h, MIDI_MAPPER, 0, 0, CALLBACK_NULL);
    printf("open the MIDI mapper: %s (%u)\n", r == MMSYSERR_NOERROR ? "ok" : "FAILED", r);
    if (r == MMSYSERR_NOERROR) {
        DWORD vol = 0;
        MMRESULT g = midiOutGetVolume(h, &vol);
        printf("the mapper's volume: result %u, left %lu%% right %lu%%\n", g, (vol & 0xffff) * 100 / 0xffff, (vol >> 16) * 100 / 0xffff);
        midiOutClose(h);
    }
    if (midiOutOpen(&h, 0, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
        DWORD vol = 0;
        MMRESULT g = midiOutGetVolume(h, &vol);
        printf("device 0's volume: result %u, left %lu%% right %lu%%\n", g, (vol & 0xffff) * 100 / 0xffff, (vol >> 16) * 100 / 0xffff);
        midiOutClose(h);
    }
    UINT w = waveOutGetNumDevs();
    printf("wave output devices: %u\n", w);
    return 0;
}
