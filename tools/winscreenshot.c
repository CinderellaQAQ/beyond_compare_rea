#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct Search {
    const wchar_t *title_part;
    HWND result;
} Search;

static BOOL CALLBACK find_window(HWND window, LPARAM parameter) {
    Search *search = (Search *)parameter;
    wchar_t title[512];
    GetWindowTextW(window, title, 512);
    if (wcsstr(title, search->title_part)) {
        search->result = window;
        return FALSE;
    }
    return TRUE;
}

static BOOL write_bitmap(const wchar_t *path, const BITMAPINFO *info,
                         const void *pixels, DWORD pixel_bytes) {
    BITMAPFILEHEADER file_header;
    HANDLE file;
    DWORD written;
    ZeroMemory(&file_header, sizeof(file_header));
    file_header.bfType = 0x4d42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(info->bmiHeader);
    file_header.bfSize = file_header.bfOffBits + pixel_bytes;
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    if (!WriteFile(file, &file_header, sizeof(file_header), &written, NULL) ||
        !WriteFile(file, &info->bmiHeader, sizeof(info->bmiHeader), &written,
                   NULL) ||
        !WriteFile(file, pixels, pixel_bytes, &written, NULL)) {
        CloseHandle(file);
        return FALSE;
    }
    CloseHandle(file);
    return TRUE;
}

int wmain(int argc, wchar_t **argv) {
    Search search;
    RECT rect;
    BITMAPINFO info;
    HDC window_dc;
    HDC memory_dc;
    HBITMAP bitmap;
    HGDIOBJ previous;
    void *pixels;
    DWORD pixel_bytes;
    if (argc != 3) {
        fwprintf(stderr, L"usage: winscreenshot <title-part> <output.bmp>\n");
        return 2;
    }
    search.title_part = argv[1];
    search.result = NULL;
    EnumWindows(find_window, (LPARAM)&search);
    if (!search.result || !GetWindowRect(search.result, &rect)) return 3;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = rect.right - rect.left;
    info.bmiHeader.biHeight = rect.bottom - rect.top;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    info.bmiHeader.biCompression = BI_RGB;
    pixel_bytes = (((DWORD)info.bmiHeader.biWidth * 3 + 3) & ~3u) *
                  (DWORD)info.bmiHeader.biHeight;
    window_dc = GetWindowDC(search.result);
    memory_dc = CreateCompatibleDC(window_dc);
    bitmap = CreateDIBSection(window_dc, &info, DIB_RGB_COLORS, &pixels,
                              NULL, 0);
    previous = SelectObject(memory_dc, bitmap);
    PrintWindow(search.result, memory_dc, 0);
    BitBlt(memory_dc, 0, 0, info.bmiHeader.biWidth,
           info.bmiHeader.biHeight, window_dc, 0, 0, SRCCOPY);
    SelectObject(memory_dc, previous);
    DeleteDC(memory_dc);
    ReleaseDC(search.result, window_dc);
    if (!write_bitmap(argv[2], &info, pixels, pixel_bytes)) {
        DeleteObject(bitmap);
        return 4;
    }
    DeleteObject(bitmap);
    return 0;
}
