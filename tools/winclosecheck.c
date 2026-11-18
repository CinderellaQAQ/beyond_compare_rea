#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct Search {
    const wchar_t *title_part;
    HWND target;
    HWND crash;
} Search;

static BOOL CALLBACK find_windows(HWND window, LPARAM parameter) {
    Search *search = (Search *)parameter;
    wchar_t title[512];
    wchar_t class_name[128];
    GetWindowTextW(window, title, 512);
    GetClassNameW(window, class_name, 128);
    if (!search->target && wcsstr(title, search->title_part))
        search->target = window;
    if (!search->crash &&
        lstrcmpiW(class_name, L"madExceptWndClass") == 0 &&
        wcsstr(title, L"BCompare.exe")) {
        search->crash = window;
    }
    return TRUE;
}

static void refresh(Search *search) {
    search->target = NULL;
    search->crash = NULL;
    EnumWindows(find_windows, (LPARAM)search);
}

int wmain(int argc, wchar_t **argv) {
    Search search;
    HWND target;
    int elapsed;
    if (argc != 2) {
        fwprintf(stderr, L"usage: winclosecheck <title-part>\n");
        return 2;
    }
    ZeroMemory(&search, sizeof(search));
    search.title_part = argv[1];
    refresh(&search);
    if (!search.target) return 3;
    target = search.target;
    if (!PostMessageW(target, WM_CLOSE, 0, 0)) return 4;
    for (elapsed = 0; elapsed <= 7000; elapsed += 100) {
        Sleep(100);
        refresh(&search);
        if (search.crash) {
            fwprintf(stderr,
                     L"FAIL: BC3 opened madExcept while closing after detail selection/copy\n");
            return 5;
        }
        if (!search.target) {
            wprintf(L"PASS: BC3 closed without an exception window (%d ms)\n",
                    elapsed + 100);
            return 0;
        }
    }
    fwprintf(stderr, L"FAIL: BC3 did not close within 7 seconds\n");
    return 6;
}
