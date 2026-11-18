#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define MAX_DETAIL_WINDOWS 8

typedef struct Search {
    const wchar_t *title_part;
    HWND top;
    HWND horizontal;
    LONG horizontal_y;
    HWND detail[MAX_DETAIL_WINDOWS];
    LONG detail_y[MAX_DETAIL_WINDOWS];
    int detail_count;
} Search;

typedef struct Capture {
    int width;
    int height;
    COLORREF *pixels;
} Capture;

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

static BOOL CALLBACK find_controls(HWND window, LPARAM parameter) {
    Search *search = (Search *)parameter;
    wchar_t class_name[128];
    LONG style;
    RECT rect;
    int width;
    int height;
    GetClassNameW(window, class_name, 128);
    style = GetWindowLongW(window, GWL_STYLE);
    GetWindowRect(window, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    if (lstrcmpiW(class_name, L"ScrollBar") == 0 &&
        !(style & SBS_VERT) && width > 100 &&
        (!search->horizontal || rect.top > search->horizontal_y)) {
        search->horizontal = window;
        search->horizontal_y = rect.top;
    }
    if (wcsstr(class_name, L"TUiEditorWindow") && width > 800 &&
        height > 10 && search->detail_count < MAX_DETAIL_WINDOWS) {
        int index = search->detail_count++;
        search->detail[index] = window;
        search->detail_y[index] = rect.top;
    }
    return TRUE;
}

static void sort_details(Search *search) {
    int left;
    for (left = 0; left + 1 < search->detail_count; ++left) {
        int right;
        for (right = left + 1; right < search->detail_count; ++right) {
            if (search->detail_y[right] < search->detail_y[left]) {
                HWND window = search->detail[left];
                LONG y = search->detail_y[left];
                search->detail[left] = search->detail[right];
                search->detail_y[left] = search->detail_y[right];
                search->detail[right] = window;
                search->detail_y[right] = y;
            }
        }
    }
}

static BOOL capture_window(HWND window, Capture *capture) {
    RECT rect;
    HDC source;
    HDC memory;
    HBITMAP bitmap;
    HBITMAP old_bitmap;
    BITMAPINFO info;
    int x;
    int y;
    if (!GetClientRect(window, &rect)) return FALSE;
    capture->width = rect.right - rect.left;
    capture->height = rect.bottom - rect.top;
    if (capture->width <= 0 || capture->height <= 0) return FALSE;
    capture->pixels = (COLORREF *)calloc(
        (size_t)capture->width * (size_t)capture->height,
        sizeof(COLORREF));
    if (!capture->pixels) return FALSE;
    source = GetDC(window);
    memory = CreateCompatibleDC(source);
    bitmap = CreateCompatibleBitmap(source, capture->width, capture->height);
    old_bitmap = (HBITMAP)SelectObject(memory, bitmap);
    BitBlt(memory, 0, 0, capture->width, capture->height,
           source, 0, 0, SRCCOPY);
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = capture->width;
    info.bmiHeader.biHeight = -capture->height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    if (!GetDIBits(memory, bitmap, 0, (UINT)capture->height,
                   capture->pixels, &info, DIB_RGB_COLORS)) {
        free(capture->pixels);
        capture->pixels = NULL;
    }
    SelectObject(memory, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(window, source);
    if (!capture->pixels) return FALSE;
    /* Ignore the one-pixel native border; it can be repainted independently
       of the text surface and is not evidence of synchronized scrolling. */
    for (x = 0; x < capture->width; ++x) {
        capture->pixels[x] = 0;
        capture->pixels[(capture->height - 1) * capture->width + x] = 0;
    }
    for (y = 0; y < capture->height; ++y) {
        capture->pixels[y * capture->width] = 0;
        capture->pixels[y * capture->width + capture->width - 1] = 0;
    }
    return TRUE;
}

static size_t changed_pixels(const Capture *before,
                             const Capture *after) {
    size_t index;
    size_t count = 0;
    size_t total;
    if (before->width != after->width || before->height != after->height)
        return 0;
    total = (size_t)before->width * (size_t)before->height;
    for (index = 0; index < total; ++index) {
        if (before->pixels[index] != after->pixels[index]) ++count;
    }
    return count;
}

static void release_capture(Capture *capture) {
    free(capture->pixels);
    ZeroMemory(capture, sizeof(*capture));
}

int wmain(int argc, wchar_t **argv) {
    Search search;
    Capture before_upper;
    Capture before_lower;
    Capture after_upper;
    Capture after_lower;
    HWND upper;
    HWND lower;
    size_t upper_changes;
    size_t lower_changes;
    int index;
    int before_position;
    int after_position;
    if (argc != 2) {
        fwprintf(stderr, L"usage: winscrollsync <title-part>\n");
        return 2;
    }
    ZeroMemory(&search, sizeof(search));
    ZeroMemory(&before_upper, sizeof(before_upper));
    ZeroMemory(&before_lower, sizeof(before_lower));
    ZeroMemory(&after_upper, sizeof(after_upper));
    ZeroMemory(&after_lower, sizeof(after_lower));
    search.title_part = argv[1];
    EnumWindows(find_top, (LPARAM)&search);
    if (!search.top) return 3;
    EnumChildWindows(search.top, find_controls, (LPARAM)&search);
    if (!search.horizontal || search.detail_count < 2) return 4;
    sort_details(&search);
    upper = search.detail[search.detail_count - 2];
    lower = search.detail[search.detail_count - 1];

    for (index = 0; index < 64; ++index) {
        SendMessageW(GetParent(search.horizontal), WM_HSCROLL,
                     MAKEWPARAM(SB_PAGELEFT, 0),
                     (LPARAM)search.horizontal);
    }
    Sleep(150);
    before_position = GetScrollPos(search.horizontal, SB_CTL);
    if (!capture_window(upper, &before_upper) ||
        !capture_window(lower, &before_lower)) {
        return 5;
    }
    for (index = 0; index < 4; ++index) {
        SendMessageW(GetParent(search.horizontal), WM_HSCROLL,
                     MAKEWPARAM(SB_PAGERIGHT, 0),
                     (LPARAM)search.horizontal);
    }
    Sleep(250);
    after_position = GetScrollPos(search.horizontal, SB_CTL);
    if (!capture_window(upper, &after_upper) ||
        !capture_window(lower, &after_lower)) {
        return 6;
    }
    upper_changes = changed_pixels(&before_upper, &after_upper);
    lower_changes = changed_pixels(&before_lower, &after_lower);
    wprintf(L"horizontal scroll: %d -> %d\n", before_position,
            after_position);
    wprintf(L"upper detail changed pixels: %lu\n",
            (unsigned long)upper_changes);
    wprintf(L"lower detail changed pixels: %lu\n",
            (unsigned long)lower_changes);
    release_capture(&before_upper);
    release_capture(&before_lower);
    release_capture(&after_upper);
    release_capture(&after_lower);
    if (after_position == before_position) return 7;
    if (lower_changes < 1000) return 8;
    if (upper_changes < 1000) {
        fwprintf(stderr, L"FAIL: upper detail rows did not follow the shared horizontal scrollbar\n");
        return 9;
    }
    wprintf(L"PASS: both native detail editors moved together\n");
    return 0;
}
