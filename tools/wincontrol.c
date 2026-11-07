#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef struct Context {
    const wchar_t *title_part;
    const wchar_t *click_part;
    HWND top;
    HWND clicked;
} Context;

static BOOL CALLBACK find_top(HWND window, LPARAM parameter) {
    Context *context = (Context *)parameter;
    wchar_t title[512];
    GetWindowTextW(window, title, 512);
    if (!wcscmp(context->title_part, L"--list")) {
        wchar_t class_name[256];
        GetClassNameW(window, class_name, 256);
        wprintf(L"TOP 0x%08lx  %-24ls  %ls\n",
                (unsigned long)(ULONG_PTR)window, class_name, title);
        return TRUE;
    }
    if (wcsstr(title, context->title_part)) {
        context->top = window;
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK inspect_child(HWND window, LPARAM parameter) {
    Context *context = (Context *)parameter;
    wchar_t title[512];
    wchar_t class_name[256];
    RECT rect;
    GetWindowTextW(window, title, 512);
    GetClassNameW(window, class_name, 256);
    GetWindowRect(window, &rect);
    wprintf(L"0x%08lx  %-24ls  %-40ls  %ld,%ld %ldx%ld\n",
            (unsigned long)(ULONG_PTR)window, class_name, title,
            rect.left, rect.top, rect.right - rect.left,
            rect.bottom - rect.top);
    if (context->click_part && !context->clicked &&
        wcsstr(title, context->click_part)) {
        context->clicked = window;
    }
    return TRUE;
}

int wmain(int argc, wchar_t **argv) {
    Context context;
    ZeroMemory(&context, sizeof(context));
    if (argc < 2) {
        fwprintf(stderr, L"usage: wincontrol <top-title-part> [child-text-to-click]\n");
        return 2;
    }
    context.title_part = argv[1];
    if (argc > 2) context.click_part = argv[2];
    EnumWindows(find_top, (LPARAM)&context);
    if (!wcscmp(context.title_part, L"--list")) return 0;
    if (!context.top) {
        fwprintf(stderr, L"top-level window not found\n");
        return 3;
    }
    if (context.click_part && !wcscmp(context.click_part, L"--close")) {
        SendMessageW(context.top, WM_CLOSE, 0, 0);
        wprintf(L"CLOSED 0x%08lx\n",
                (unsigned long)(ULONG_PTR)context.top);
        return 0;
    }
    wprintf(L"TOP 0x%08lx\n", (unsigned long)(ULONG_PTR)context.top);
    EnumChildWindows(context.top, inspect_child, (LPARAM)&context);
    if (context.click_part) {
        if (!context.clicked) {
            fwprintf(stderr, L"child text not found\n");
            return 4;
        }
        SendMessageW(context.clicked, BM_CLICK, 0, 0);
        wprintf(L"CLICKED 0x%08lx\n",
                (unsigned long)(ULONG_PTR)context.clicked);
    }
    return 0;
}
