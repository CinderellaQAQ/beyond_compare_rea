#include <windows.h>
#include <stdio.h>
#include <string.h>

static HWND find_top_window;

static BOOL CALLBACK find_beyond_compare(HWND window, LPARAM parameter) {
    char title[512];
    (void)parameter;
    GetWindowTextA(window, title, sizeof(title));
    if (strstr(title, "Beyond Compare")) {
        find_top_window = window;
        return FALSE;
    }
    return TRUE;
}

int main(int argc, char **argv) {
    HANDLE data;
    const WCHAR *wide;
    const char *ansi;
    if (argc > 1 && !strcmp(argv[1], "--send-copy")) {
        GUITHREADINFO info;
        DWORD thread;
        char class_name[128];
        EnumWindows(find_beyond_compare, 0);
        if (!find_top_window) return 4;
        thread = GetWindowThreadProcessId(find_top_window, NULL);
        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(info);
        if (!GetGUIThreadInfo(thread, &info) || !info.hwndFocus) return 5;
        GetClassNameA(info.hwndFocus, class_name, sizeof(class_name));
        printf("FOCUS 0x%08lx %s\n",
               (unsigned long)(ULONG_PTR)info.hwndFocus, class_name);
        SendMessageA(info.hwndFocus, WM_COPY, 0, 0);
        Sleep(100);
    }
    if (!OpenClipboard(NULL)) return 2;
    data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
        wide = (const WCHAR *)GlobalLock(data);
        if (wide) {
            printf("CF_UNICODETEXT %lu\n", (unsigned long)lstrlenW(wide));
            GlobalUnlock(data);
            CloseClipboard();
            return 0;
        }
    }
    data = GetClipboardData(CF_TEXT);
    if (data) {
        ansi = (const char *)GlobalLock(data);
        if (ansi) {
            printf("CF_TEXT %lu\n", (unsigned long)lstrlenA(ansi));
            GlobalUnlock(data);
            CloseClipboard();
            return 0;
        }
    }
    CloseClipboard();
    return 3;
}
