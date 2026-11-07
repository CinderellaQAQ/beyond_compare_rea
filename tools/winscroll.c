#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct Search {
    const wchar_t *title_part;
    HWND top;
    HWND overlay;
    HWND horizontal;
} Search;

static BOOL CALLBACK find_top(HWND window, LPARAM parameter) {
    Search *search = (Search *)parameter;
    wchar_t title[512];
    GetWindowTextW(window, title, 512);
    if (wcsstr(title, search->title_part)) {
        search->top = window;
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK find_overlay(HWND window, LPARAM parameter) {
    Search *search = (Search *)parameter;
    wchar_t class_name[128];
    GetClassNameW(window, class_name, 128);
    if (lstrcmpW(class_name, L"BC3MultiDetailsOverlay") == 0) {
        search->overlay = window;
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK find_scrollbar(HWND window, LPARAM parameter) {
    Search *search = (Search *)parameter;
    wchar_t class_name[128];
    LONG style;
    GetClassNameW(window, class_name, 128);
    style = GetWindowLongW(window, GWL_STYLE);
    if (lstrcmpiW(class_name, L"ScrollBar") == 0 && !(style & SBS_VERT)) {
        search->horizontal = window;
        return FALSE;
    }
    return TRUE;
}

int wmain(int argc, wchar_t **argv) {
    Search search;
    int count = 1;
    int index;
    int before;
    int after;
    if (argc < 2 || argc > 3) {
        fwprintf(stderr, L"usage: winscroll <title-part> [page-count]\n");
        return 2;
    }
    if (argc == 3) count = _wtoi(argv[2]);
    ZeroMemory(&search, sizeof(search));
    search.title_part = argv[1];
    EnumWindows(find_top, (LPARAM)&search);
    if (!search.top) return 3;
    EnumChildWindows(search.top, find_overlay, (LPARAM)&search);
    if (!search.overlay) return 4;
    EnumChildWindows(search.overlay, find_scrollbar, (LPARAM)&search);
    if (!search.horizontal) return 5;
    before = GetScrollPos(search.horizontal, SB_CTL);
    for (index = 0; index < count; ++index) {
        SendMessageW(search.overlay, WM_HSCROLL,
                     MAKEWPARAM(count >= 0 ? SB_PAGERIGHT : SB_PAGELEFT, 0),
                     (LPARAM)search.horizontal);
    }
    after = GetScrollPos(search.horizontal, SB_CTL);
    wprintf(L"horizontal scroll: %d -> %d\n", before, after);
    return before != after ? 0 : 6;
}
