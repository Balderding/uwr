/* uw_vulkan_launch.c: uwr-vulkan.exe, a starter for trying the Vulkan renderer (docs\superpowers\plans\
   2026-11-15-vulkan-renderer.md). It starts uwr.exe from its own folder with UWPORT_RENDERER=vulkan for that one run
   (uwfront.c's view_start loads uwview_vk.dll then; nothing is written to the settings, so uwr.exe itself stays
   Direct3D 12), hands on its command line and returns the game's exit code.
   Build: clang -O2 -municode -mwindows tools\uw_vulkan_launch.c -o uwr-vulkan.exe */
#include <windows.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show)
{
    static wchar_t dir[MAX_PATH], exe[MAX_PATH + 16], cmd[32768];
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    wchar_t *s;
    (void)inst, (void)prev, (void)show;
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    if ((s = wcsrchr(dir, L'\\')) != NULL) *s = 0;
    swprintf(exe, MAX_PATH + 16, L"%ls\\uwr.exe", dir);
    swprintf(cmd, 32768, L"\"%ls\" %ls", exe, args ? args : L"");
    SetEnvironmentVariableW(L"UWPORT_RENDERER", L"vulkan");
    si.dwFlags = STARTF_USESTDHANDLES;   /* (a check's redirected log goes on to the game) */
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessW(exe, cmd, NULL, NULL, TRUE, 0, NULL, dir, &si, &pi)) {
        MessageBoxW(NULL, L"uwr.exe was not found beside uwr-vulkan.exe.", L"uwr-vulkan", MB_OK | MB_ICONWARNING);
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
    return (int)code;
}
