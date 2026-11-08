#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct Search {
    const wchar_t *title_part;
    HWND top;
    HWND horizontal;
    LONG horizontal_y;
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

static BOOL CALLBACK find_scrollbar(HWND window, LPARAM parameter) {
    Search *search = (Search *)parameter;
    wchar_t class_name[128];
    LONG style;
    RECT rect;
    GetClassNameW(window, class_name, 128);
    style = GetWindowLongW(window, GWL_STYLE);
    GetWindowRect(window, &rect);
    if (lstrcmpiW(class_name, L"ScrollBar") == 0 &&
        !(style & SBS_VERT) && rect.right - rect.left > 100 &&
        (!search->horizontal || rect.top > search->horizontal_y)) {
        search->horizontal = window;
        search->horizontal_y = rect.top;
    }
    return TRUE;
}

int wmain(int argc, wchar_t **argv) {
    Search search;
    int count = 1;
    int index;
    int iterations;
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
    EnumChildWindows(search.top, find_scrollbar, (LPARAM)&search);
    if (!search.horizontal) return 5;
    before = GetScrollPos(search.horizontal, SB_CTL);
    iterations = count < 0 ? -count : count;
    for (index = 0; index < iterations; ++index) {
        SendMessageW(GetParent(search.horizontal), WM_HSCROLL,
                     MAKEWPARAM(count >= 0 ? SB_PAGERIGHT : SB_PAGELEFT, 0),
                     (LPARAM)search.horizontal);
    }
    after = GetScrollPos(search.horizontal, SB_CTL);
    wprintf(L"horizontal scroll: %d -> %d\n", before, after);
    return before != after ? 0 : 6;
}
