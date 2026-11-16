/* mapsmake.h: the relief maps (normal, occlusion, height: data\laigter) made on the player's own machine, once, on
 * the first run (the user, 2026-11-16: the public package carries none of them, as nothing in it may come from the
 * game's art). Included by uwview.c only.
 *
 * The maps are made by a program of its own, <root>\laigter\laigter-cli (tools\laigter-cli: a port of Laigter,
 * GPL v3, never linked into the game), from the player's own game files with the settings beside it
 * (<root>\laigter\*.preset): `laigter-cli --uwr-game GAME DATA`. They come out the same, pixel for pixel, as the
 * ones we made ourselves (work\announce\cmp_pixels.py).
 *
 * Nothing is done when the maps are there (data\laigter\made.txt, the program's last file), when a folder of maps
 * without that file is there (ours from before, or someone's own: left alone), or when the program is not in the
 * folder (the game then runs with the engine's own flat lighting, as it does with no maps). */
#pragma once
#include "base.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static void maps_make_if_missing(const char *root_dir, const char *data, const char *game)
{
    char p[1024], tool[1024];
    if (!root_dir[0] || !data[0] || !game[0]) return;
    snprintf(p, sizeof p, "%s\\laigter\\made.txt", data);
    if (file_exists(p)) return;
    snprintf(p, sizeof p, "%s\\laigter\\walls\\000_n.png", data);
    if (file_exists(p)) return;
#ifdef _WIN32
    snprintf(tool, sizeof tool, "%s\\laigter\\laigter-cli.exe", root_dir);
    if (GetFileAttributesA(tool) == INVALID_FILE_ATTRIBUTES) return;
    char cmd[4096];
    STARTUPINFOA si = {sizeof si};
    PROCESS_INFORMATION pi;
    snprintf(cmd, sizeof cmd, "\"%s\" --uwr-game \"%s\" \"%s\"", tool, game, data);
    fprintf(stderr, "uwview: first run: making the relief maps from the game's own pictures (%s)\n", tool);
    if (!CreateProcessA(tool, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "uwview: the relief maps were not made: laigter-cli did not start (error %lu)\n", GetLastError());
        return;
    }
    DWORD code = 1;
    if (WaitForSingleObject(pi.hProcess, 10 * 60 * 1000) != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 1);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
    fprintf(stderr, "uwview: the relief maps: %s\n", code ? "NOT made (laigter-cli failed)" : "made");
#else
    char g[1024], d[1024];
    size_t i;
    snprintf(tool, sizeof tool, "%s/laigter/laigter-cli", root_dir);
    for (i = 0; tool[i]; i++) if (tool[i] == '\\') tool[i] = '/';
    for (i = 0; game[i] && i + 1 < sizeof g; i++) g[i] = game[i] == '\\' ? '/' : game[i];
    g[i] = 0;
    for (i = 0; data[i] && i + 1 < sizeof d; i++) d[i] = data[i] == '\\' ? '/' : data[i];
    d[i] = 0;
    struct stat st;
    if (stat(tool, &st)) return;
    chmod(tool, st.st_mode | 0111);   /* (a zip does not keep the run permission) */
    fprintf(stderr, "uwview: first run: making the relief maps from the game's own pictures (%s)\n", tool);
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "uwview: the relief maps were not made: laigter-cli did not start\n");
        return;
    }
    if (!pid) {
        execl(tool, tool, "--uwr-game", g, d, (char *)NULL);
        _exit(127);
    }
    int status = 1;
    waitpid(pid, &status, 0);
    fprintf(stderr, "uwview: the relief maps: %s\n", WIFEXITED(status) && !WEXITSTATUS(status) ? "made" : "NOT made (laigter-cli failed)");
#endif
}
