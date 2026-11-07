#include "multidetails.h"

#include "bc3_target.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAX_VIEWS 8
#define MAX_SELECTIONS 128
#define MAX_EDITOR_SIDES 4
#define OVERLAY_CLASS L"BC3MultiDetailsOverlay"
#define HEADER_HEIGHT 24
#define CAPTION_HEIGHT 18
#define SCROLLBAR_HEIGHT 17
#define FIXED_GUTTER_WIDTH 18
#define MAX_CAPTURE_WIDTH 32768

typedef struct CapturedStrip {
    HBITMAP bitmap;
    HDC dc;
    void *bits;
    int width;
    int height;
} CapturedStrip;

typedef struct SelectedPair {
    int key;
    int item[MAX_EDITOR_SIDES];
    CapturedStrip left;
    CapturedStrip right;
} SelectedPair;

typedef struct ViewState {
    void *frame;
    void *panel_object;
    HWND panel_window;
    HWND overlay;
    HWND horizontal_scroll;
    int initial_panel_height;
    int anchor;
    int horizontal_offset;
    int vertical_offset;
    int pair_count;
    HWND marker_window[MAX_EDITOR_SIDES];
    WNDPROC marker_original[MAX_EDITOR_SIDES];
    void *marker_editor[MAX_EDITOR_SIDES];
    SelectedPair pairs[MAX_SELECTIONS];
} ViewState;

typedef struct TempSurface {
    HBITMAP bitmap;
    HDC dc;
    void *bits;
    int width;
    int height;
} TempSurface;

static HINSTANCE g_instance;
static ViewState g_views[MAX_VIEWS];
static BOOL g_internal_update;
static HFONT g_ui_font;

static LRESULT CALLBACK overlay_window_proc(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK marker_window_proc(HWND window, UINT message,
                                           WPARAM wparam, LPARAM lparam);

static ViewState *view_from_marker(HWND window, int *side_out) {
    int view_index;
    int side;
    for (view_index = 0; view_index < MAX_VIEWS; ++view_index) {
        for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
            if (g_views[view_index].marker_window[side] == window) {
                if (side_out) *side_out = side;
                return &g_views[view_index];
            }
        }
    }
    return NULL;
}

static BOOL CALLBACK find_editor_window_child(HWND window, LPARAM parameter) {
    HWND *result = (HWND *)parameter;
    WCHAR class_name[128];
    GetClassNameW(window, class_name, 128);
    if (wcsstr(class_name, L"TUiEditorWindow")) {
        *result = window;
        return FALSE;
    }
    return TRUE;
}

static HWND editor_paint_window(void *editor) {
    HWND outer = bc3_find_control_window(editor);
    HWND inner = NULL;
    if (!outer) return NULL;
    EnumChildWindows(outer, find_editor_window_child, (LPARAM)&inner);
    return inner ? inner : outer;
}

static void ensure_marker_windows(ViewState *view) {
    int side;
    for (side = 0; side < 2; ++side) {
        void *editor = bc3_frame_main_editor(view->frame, side);
        HWND outer;
        HWND inner = NULL;
        if (!editor || (view->marker_window[side] &&
                        IsWindow(view->marker_window[side]))) {
            continue;
        }
        outer = bc3_find_control_window(editor);
        if (!outer) continue;
        EnumChildWindows(outer, find_editor_window_child, (LPARAM)&inner);
        if (!inner) continue;
        view->marker_editor[side] = editor;
        view->marker_window[side] = inner;
        view->marker_original[side] = (WNDPROC)SetWindowLongPtrW(
            inner, GWLP_WNDPROC, (LONG_PTR)marker_window_proc);
    }
}

static void strip_destroy(CapturedStrip *strip) {
    if (strip->dc) {
        DeleteDC(strip->dc);
    }
    if (strip->bitmap) {
        DeleteObject(strip->bitmap);
    }
    ZeroMemory(strip, sizeof(*strip));
}

static BOOL surface_create(TempSurface *surface, int width, int height) {
    BITMAPINFO info;
    HDC screen;
    ZeroMemory(surface, sizeof(*surface));
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    screen = GetDC(NULL);
    surface->dc = CreateCompatibleDC(screen);
    surface->bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                                       &surface->bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!surface->dc || !surface->bitmap) {
        if (surface->dc) DeleteDC(surface->dc);
        if (surface->bitmap) DeleteObject(surface->bitmap);
        ZeroMemory(surface, sizeof(*surface));
        return FALSE;
    }
    SelectObject(surface->dc, surface->bitmap);
    surface->width = width;
    surface->height = height;
    return TRUE;
}

static void surface_destroy(TempSurface *surface) {
    if (surface->dc) DeleteDC(surface->dc);
    if (surface->bitmap) DeleteObject(surface->bitmap);
    ZeroMemory(surface, sizeof(*surface));
}

static BOOL strip_create(CapturedStrip *strip, int width, int height) {
    TempSurface surface;
    if (!surface_create(&surface, width, height)) {
        return FALSE;
    }
    strip_destroy(strip);
    strip->bitmap = surface.bitmap;
    strip->dc = surface.dc;
    strip->bits = surface.bits;
    strip->width = width;
    strip->height = height;
    return TRUE;
}

static ViewState *view_from_overlay(HWND overlay) {
    return (ViewState *)GetWindowLongPtrW(overlay, GWLP_USERDATA);
}

static ViewState *view_find(void *frame, BOOL create) {
    int index;
    ViewState *empty = NULL;
    for (index = 0; index < MAX_VIEWS; ++index) {
        if (g_views[index].frame == frame) {
            return &g_views[index];
        }
        if (!g_views[index].frame && !empty) {
            empty = &g_views[index];
        }
    }
    if (!create || !empty) {
        return NULL;
    }
    ZeroMemory(empty, sizeof(*empty));
    empty->frame = frame;
    empty->anchor = -1;
    return empty;
}

static int pair_compare(const void *left, const void *right) {
    const SelectedPair *a = (const SelectedPair *)left;
    const SelectedPair *b = (const SelectedPair *)right;
    return (a->key > b->key) - (a->key < b->key);
}

static int pair_index(const ViewState *view, int key) {
    int index;
    for (index = 0; index < view->pair_count; ++index) {
        if (view->pairs[index].key == key) return index;
    }
    return -1;
}

static void pair_remove(ViewState *view, int index) {
    int move_count;
    if (index < 0 || index >= view->pair_count) return;
    strip_destroy(&view->pairs[index].left);
    strip_destroy(&view->pairs[index].right);
    move_count = view->pair_count - index - 1;
    if (move_count > 0) {
        memmove(&view->pairs[index], &view->pairs[index + 1],
                (size_t)move_count * sizeof(view->pairs[0]));
    }
    --view->pair_count;
    ZeroMemory(&view->pairs[view->pair_count], sizeof(view->pairs[0]));
}

static void pairs_clear(ViewState *view) {
    while (view->pair_count > 0) {
        pair_remove(view, view->pair_count - 1);
    }
}

static BOOL ensure_overlay(ViewState *view) {
    RECT rect;
    if (!view || !view->frame) return FALSE;
    view->panel_object = bc3_frame_details_panel(view->frame);
    if (!view->panel_object) return FALSE;
    if (!view->initial_panel_height) {
        view->initial_panel_height =
            *(int *)((uint8_t *)view->panel_object + 0x4c);
    }
    if (!view->panel_window || !IsWindow(view->panel_window)) {
        view->panel_window = bc3_find_control_window(view->panel_object);
    }
    if (!view->panel_window) return FALSE;
    GetClientRect(view->panel_window, &rect);
    if (!view->overlay || !IsWindow(view->overlay)) {
        view->overlay = CreateWindowExW(
            0, OVERLAY_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0, rect.right, rect.bottom,
            view->panel_window, NULL, g_instance, view);
        if (!view->overlay) {
            log_line("CreateWindow overlay failed: %lu", GetLastError());
            return FALSE;
        }
        SetWindowLongPtrW(view->overlay, GWLP_USERDATA, (LONG_PTR)view);
        view->horizontal_scroll = CreateWindowExW(
            0, L"SCROLLBAR", L"", WS_CHILD | WS_VISIBLE | SBS_HORZ,
            0, rect.bottom - SCROLLBAR_HEIGHT, rect.right, SCROLLBAR_HEIGHT,
            view->overlay, (HMENU)1, g_instance, NULL);
    }
    SetWindowPos(view->overlay, HWND_TOP, 0, 0, rect.right, rect.bottom,
                 SWP_SHOWWINDOW);
    ensure_marker_windows(view);
    return TRUE;
}

static BOOL capture_editor(HWND window, TempSurface *surface) {
    RECT rect;
    HBRUSH brush;
    if (!window || !GetClientRect(window, &rect)) return FALSE;
    if (!surface_create(surface, rect.right, rect.bottom)) return FALSE;
    brush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    FillRect(surface->dc, &rect, brush);
    DeleteObject(brush);
    SendMessageW(window, WM_PRINTCLIENT, (WPARAM)surface->dc,
                 PRF_CLIENT | PRF_ERASEBKGND | PRF_CHILDREN);
    return TRUE;
}

static BOOL capture_current_pair(ViewState *view, SelectedPair *pair) {
    void *left_editor = bc3_frame_detail_editor(view->frame, 0);
    void *right_editor = bc3_frame_detail_editor(view->frame, 1);
    HWND left_window = editor_paint_window(left_editor);
    HWND right_window = editor_paint_window(right_editor);
    RECT left_rect;
    RECT right_rect;
    int old_left_x;
    int left_x_count;
    int right_x_count;
    int left_max_scroll;
    int right_max_scroll;
    int max_scroll;
    int visible_columns;
    int cell_width;
    int content_width;
    int viewport;
    int offset;
    BOOL ok = FALSE;

    if (!left_editor || !right_editor || !left_window || !right_window ||
        !GetClientRect(left_window, &left_rect) ||
        !GetClientRect(right_window, &right_rect)) {
        log_line("Detail editor HWND discovery failed");
        return FALSE;
    }
    left_x_count = g_bc3.get_x_count(left_editor);
    right_x_count = g_bc3.get_x_count(right_editor);
    left_max_scroll = g_bc3.get_max_left_x(left_editor);
    right_max_scroll = g_bc3.get_max_left_x(right_editor);
    max_scroll = left_max_scroll > right_max_scroll
                     ? left_max_scroll : right_max_scroll;
    viewport = left_rect.right - FIXED_GUTTER_WIDTH;
    if (viewport < 32) return FALSE;
    if (max_scroll < 0) max_scroll = 0;
    if (right_x_count > left_x_count) {
        visible_columns = right_x_count - right_max_scroll;
    } else {
        visible_columns = left_x_count - left_max_scroll;
    }
    if (visible_columns < 1) visible_columns = 1;
    cell_width = (viewport + visible_columns - 1) / visible_columns;
    if (cell_width < 1) cell_width = 1;
    content_width = viewport + max_scroll * cell_width;
    if (content_width > MAX_CAPTURE_WIDTH) content_width = MAX_CAPTURE_WIDTH;
    if (!strip_create(&pair->left, FIXED_GUTTER_WIDTH + content_width,
                      left_rect.bottom) ||
        !strip_create(&pair->right, FIXED_GUTTER_WIDTH + content_width,
                      right_rect.bottom)) {
        return FALSE;
    }

    old_left_x = g_bc3.get_left_x(left_editor);
    for (offset = 0; offset < content_width; offset += viewport) {
        TempSurface left_capture;
        TempSurface right_capture;
        int scroll_position = offset / cell_width;
        int source_x;
        int copy_width = content_width - offset;
        if (copy_width > viewport) copy_width = viewport;
        if (scroll_position > max_scroll) scroll_position = max_scroll;
        source_x = FIXED_GUTTER_WIDTH + offset -
                   scroll_position * cell_width;
        g_bc3.set_left_x(left_editor, scroll_position);
        if (!capture_editor(left_window, &left_capture)) break;
        if (!capture_editor(right_window, &right_capture)) {
            surface_destroy(&left_capture);
            break;
        }
        if (offset == 0) {
            BitBlt(pair->left.dc, 0, 0, FIXED_GUTTER_WIDTH,
                   pair->left.height, left_capture.dc, 0, 0, SRCCOPY);
            BitBlt(pair->right.dc, 0, 0, FIXED_GUTTER_WIDTH,
                   pair->right.height, right_capture.dc, 0, 0, SRCCOPY);
        }
        BitBlt(pair->left.dc, FIXED_GUTTER_WIDTH + offset, 0, copy_width,
               pair->left.height, left_capture.dc, source_x, 0,
               SRCCOPY);
        BitBlt(pair->right.dc, FIXED_GUTTER_WIDTH + offset, 0, copy_width,
               pair->right.height, right_capture.dc, source_x, 0,
               SRCCOPY);
        surface_destroy(&left_capture);
        surface_destroy(&right_capture);
        ok = TRUE;
    }
    g_bc3.set_left_x(left_editor, old_left_x);
    InvalidateRect(left_window, NULL, TRUE);
    InvalidateRect(right_window, NULL, TRUE);
    return ok;
}

static void read_editor_items(ViewState *view, SelectedPair *pair) {
    int side;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_main_editor(view->frame, side);
        pair->item[side] = editor ? g_bc3.get_cur_item(editor) : -1;
    }
}

static BOOL add_current_pair(ViewState *view, int key) {
    SelectedPair *pair;
    if (pair_index(view, key) >= 0 || view->pair_count >= MAX_SELECTIONS) {
        return pair_index(view, key) >= 0;
    }
    pair = &view->pairs[view->pair_count];
    ZeroMemory(pair, sizeof(*pair));
    pair->key = key;
    read_editor_items(view, pair);
    if (!capture_current_pair(view, pair)) {
        strip_destroy(&pair->left);
        strip_destroy(&pair->right);
        return FALSE;
    }
    ++view->pair_count;
    qsort(view->pairs, (size_t)view->pair_count, sizeof(view->pairs[0]),
          pair_compare);
    return TRUE;
}

static void update_scrollbars(ViewState *view) {
    RECT client;
    SCROLLINFO info;
    int index;
    int maximum_width = 0;
    int total_height = HEADER_HEIGHT + SCROLLBAR_HEIGHT + 2;
    int page_height;

    if (!view->overlay || !GetClientRect(view->overlay, &client)) return;
    for (index = 0; index < view->pair_count; ++index) {
        int width = view->pairs[index].left.width;
        int pair_height = CAPTION_HEIGHT + view->pairs[index].left.height +
                          view->pairs[index].right.height + 3;
        if (view->pairs[index].right.width > width)
            width = view->pairs[index].right.width;
        if (width > maximum_width) maximum_width = width;
        total_height += pair_height;
    }
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = maximum_width > FIXED_GUTTER_WIDTH
                    ? maximum_width - FIXED_GUTTER_WIDTH - 1
                    : 0;
    info.nPage = client.right > FIXED_GUTTER_WIDTH
                     ? (UINT)(client.right - FIXED_GUTTER_WIDTH)
                     : 1;
    info.nPos = view->horizontal_offset;
    SetScrollInfo(view->horizontal_scroll, SB_CTL, &info, TRUE);
    view->horizontal_offset = GetScrollPos(view->horizontal_scroll, SB_CTL);

    page_height = client.bottom - HEADER_HEIGHT - SCROLLBAR_HEIGHT;
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = total_height - HEADER_HEIGHT - SCROLLBAR_HEIGHT;
    info.nPage = page_height > 0 ? (UINT)page_height : 1;
    info.nPos = view->vertical_offset;
    SetScrollInfo(view->overlay, SB_VERT, &info, TRUE);
    view->vertical_offset = GetScrollPos(view->overlay, SB_VERT);
}

static void resize_panel_for_selection(ViewState *view) {
    int index;
    int desired = HEADER_HEIGHT + SCROLLBAR_HEIGHT + 4;
    int maximum = 360;
    if (!view->panel_object || !view->initial_panel_height) return;
    for (index = 0; index < view->pair_count; ++index) {
        desired += CAPTION_HEIGHT + view->pairs[index].left.height +
                   view->pairs[index].right.height + 3;
    }
    if (view->pair_count == 0) desired = view->initial_panel_height;
    if (desired > maximum) desired = maximum;
    if (desired < view->initial_panel_height) desired = view->initial_panel_height;
    if (*(int *)((uint8_t *)view->panel_object + 0x4c) != desired) {
        g_internal_update = TRUE;
        g_bc3.set_control_height(view->panel_object, desired);
        g_internal_update = FALSE;
    }
}

static void refresh_view(ViewState *view) {
    int side;
    if (!ensure_overlay(view)) return;
    resize_panel_for_selection(view);
    ensure_overlay(view);
    update_scrollbars(view);
    InvalidateRect(view->overlay, NULL, TRUE);
    for (side = 0; side < 2; ++side) {
        if (view->marker_window[side])
            InvalidateRect(view->marker_window[side], NULL, FALSE);
    }
}

static void restore_main_editors(ViewState *view, int items[4], int tops[4]) {
    int side;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_main_editor(view->frame, side);
        if (!editor) continue;
        if (items[side] >= 0) g_bc3.set_cur_item(editor, items[side]);
        if (tops[side] >= 0) g_bc3.set_top_item(editor, tops[side]);
        {
            HWND window = bc3_find_control_window(editor);
            if (window) {
                SendMessageW(window, WM_SETREDRAW, TRUE, 0);
                InvalidateRect(window, NULL, FALSE);
            }
        }
    }
}

static void save_main_editors(ViewState *view, int items[4], int tops[4]) {
    int side;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_main_editor(view->frame, side);
        HWND window;
        items[side] = editor ? g_bc3.get_cur_item(editor) : -1;
        tops[side] = editor ? g_bc3.get_top_item(editor) : -1;
        window = editor ? bc3_find_control_window(editor) : NULL;
        if (window) SendMessageW(window, WM_SETREDRAW, FALSE, 0);
    }
}

static void select_range(ViewState *view, int start, int end, BOOL append) {
    void *driver = bc3_frame_main_editor(view->frame, 0);
    int saved_items[4];
    int saved_tops[4];
    int direction = start <= end ? 1 : -1;
    int key;
    if (!driver) return;
    if (!append) pairs_clear(view);
    save_main_editors(view, saved_items, saved_tops);
    g_internal_update = TRUE;
    for (key = start;; key += direction) {
        g_bc3.set_cur_item(driver, key);
        add_current_pair(view, key);
        if (key == end || view->pair_count >= MAX_SELECTIONS) break;
    }
    restore_main_editors(view, saved_items, saved_tops);
    g_internal_update = FALSE;
}

BOOL multidetails_initialize(void) {
    WNDCLASSW window_class;
    NONCLIENTMETRICSW metrics;
    g_instance = (HINSTANCE)GetModuleHandleW(L"version.dll");
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = overlay_window_proc;
    window_class.hInstance = g_instance;
    window_class.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    window_class.lpszClassName = OVERLAY_CLASS;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return FALSE;
    }
    ZeroMemory(&metrics, sizeof(metrics));
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                              &metrics, 0)) {
        g_ui_font = CreateFontIndirectW(&metrics.lfMessageFont);
    }
    return TRUE;
}

void multidetails_shutdown(void) {
    int view_index;
    for (view_index = 0; view_index < MAX_VIEWS; ++view_index) {
        ViewState *view = &g_views[view_index];
        int side;
        pairs_clear(view);
        for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
            if (view->marker_window[side] && view->marker_original[side] &&
                IsWindow(view->marker_window[side])) {
                SetWindowLongPtrW(view->marker_window[side], GWLP_WNDPROC,
                                  (LONG_PTR)view->marker_original[side]);
            }
        }
        if (view->overlay && IsWindow(view->overlay)) DestroyWindow(view->overlay);
        ZeroMemory(view, sizeof(*view));
    }
    if (g_ui_font) DeleteObject(g_ui_font);
    g_ui_font = NULL;
}

BOOL multidetails_is_internal_update(void) {
    return g_internal_update;
}

void multidetails_on_click(void *frame, void *sender) {
    ViewState *view;
    void *driver;
    int key;
    int index;
    BOOL control;
    BOOL shift;
    (void)sender;
    if (g_internal_update || !frame) return;
    view = view_find(frame, TRUE);
    if (!view || !ensure_overlay(view)) return;
    driver = bc3_frame_main_editor(frame, 0);
    if (!driver) return;
    key = g_bc3.get_cur_item(driver);
    if (key < 0) return;
    control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (control && shift && view->anchor >= 0) {
        select_range(view, view->anchor, key, TRUE);
    } else if (control) {
        index = pair_index(view, key);
        if (index >= 0) {
            pair_remove(view, index);
        } else {
            add_current_pair(view, key);
        }
        view->anchor = key;
    } else if (shift && view->anchor >= 0) {
        select_range(view, view->anchor, key, FALSE);
    } else {
        pairs_clear(view);
        add_current_pair(view, key);
        view->anchor = key;
    }
    refresh_view(view);
}

void multidetails_on_current_changed(void *frame, void *sender) {
    (void)frame;
    (void)sender;
    /* Mouse selection is finalized by EditorClick. Keyboard navigation keeps
       the stock one-line detail panel until a click establishes a selection. */
}

void multidetails_on_details_resize(void *frame) {
    ViewState *view;
    if (g_internal_update || !frame) return;
    view = view_find(frame, TRUE);
    if (!view) return;
    if (ensure_overlay(view)) {
        update_scrollbars(view);
        InvalidateRect(view->overlay, NULL, TRUE);
    }
}

static void paint_strip(HDC destination, const CapturedStrip *strip,
                        int x, int y, int width, int horizontal_offset) {
    int content_width;
    int available;
    if (!strip->dc || strip->height <= 0) return;
    available = width - FIXED_GUTTER_WIDTH;
    if (available < 0) available = 0;
    content_width = strip->width - FIXED_GUTTER_WIDTH - horizontal_offset;
    if (content_width > available) content_width = available;
    if (content_width < 0) content_width = 0;
    BitBlt(destination, x, y, FIXED_GUTTER_WIDTH, strip->height,
           strip->dc, 0, 0, SRCCOPY);
    if (content_width > 0) {
        BitBlt(destination, x + FIXED_GUTTER_WIDTH, y, content_width,
               strip->height, strip->dc,
               FIXED_GUTTER_WIDTH + horizontal_offset, 0, SRCCOPY);
    }
}

static void paint_overlay(ViewState *view, HDC dc, const RECT *client) {
    HBRUSH face = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    HBRUSH window_brush = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    HPEN border_pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
    HFONT previous_font = g_ui_font ? SelectObject(dc, g_ui_font) : NULL;
    RECT header = {0, 0, client->right, HEADER_HEIGHT};
    WCHAR text[256];
    int y = HEADER_HEIGHT - view->vertical_offset;
    int index;
    FillRect(dc, client, window_brush);
    FillRect(dc, &header, face);
    MoveToEx(dc, 0, HEADER_HEIGHT - 1, NULL);
    LineTo(dc, client->right, HEADER_HEIGHT - 1);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    _snwprintf(text, 255,
               L"Text Details  |  %d comparison row%s selected  |  Ctrl+click: toggle, Shift+click: range",
               view->pair_count, view->pair_count == 1 ? L"" : L"s");
    TextOutW(dc, 7, 5, text, lstrlenW(text));

    SelectObject(dc, border_pen);
    for (index = 0; index < view->pair_count; ++index) {
        SelectedPair *pair = &view->pairs[index];
        int pair_height = CAPTION_HEIGHT + pair->left.height +
                          pair->right.height + 3;
        RECT caption = {0, y, client->right, y + CAPTION_HEIGHT};
        if (y + pair_height >= HEADER_HEIGHT &&
            y < client->bottom - SCROLLBAR_HEIGHT) {
            FillRect(dc, &caption, face);
            _snwprintf(text, 255, L"Comparison row %d  |  L / R pair",
                       pair->key + 1);
            TextOutW(dc, 7, y + 2, text, lstrlenW(text));
            paint_strip(dc, &pair->left, 0, y + CAPTION_HEIGHT,
                        client->right, view->horizontal_offset);
            paint_strip(dc, &pair->right, 0,
                        y + CAPTION_HEIGHT + pair->left.height,
                        client->right, view->horizontal_offset);
            MoveToEx(dc, 0, y + pair_height - 1, NULL);
            LineTo(dc, client->right, y + pair_height - 1);
        }
        y += pair_height;
    }
    if (view->pair_count == 0) {
        const WCHAR *empty = L"No rows selected. Click a comparison row to begin.";
        TextOutW(dc, 8, HEADER_HEIGHT + 8, empty, lstrlenW(empty));
    }
    if (previous_font) SelectObject(dc, previous_font);
    DeleteObject(border_pen);
    DeleteObject(face);
    DeleteObject(window_brush);
}

static void handle_scroll(ViewState *view, HWND bar, int scroll_bar,
                          WPARAM wparam) {
    SCROLLINFO info;
    int code = LOWORD(wparam);
    int position;
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SIF_ALL;
    GetScrollInfo(bar, scroll_bar, &info);
    position = info.nPos;
    switch (code) {
        case SB_LINELEFT: position -= 12; break;
        case SB_LINERIGHT: position += 12; break;
        case SB_PAGELEFT: position -= (int)info.nPage; break;
        case SB_PAGERIGHT: position += (int)info.nPage; break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: position = info.nTrackPos; break;
        case SB_TOP: position = info.nMin; break;
        case SB_BOTTOM: position = info.nMax; break;
        default: return;
    }
    info.fMask = SIF_POS;
    info.nPos = position;
    SetScrollInfo(bar, scroll_bar, &info, TRUE);
    position = GetScrollPos(bar, scroll_bar);
    if (scroll_bar == SB_CTL) view->horizontal_offset = position;
    else view->vertical_offset = position;
    InvalidateRect(view->overlay, NULL, FALSE);
}

static LRESULT CALLBACK overlay_window_proc(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) {
    ViewState *view = view_from_overlay(window);
    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              (LONG_PTR)create->lpCreateParams);
            return TRUE;
        }
        case WM_SIZE:
            if (view && view->horizontal_scroll) {
                MoveWindow(view->horizontal_scroll, 0,
                           HIWORD(lparam) - SCROLLBAR_HEIGHT,
                           LOWORD(lparam), SCROLLBAR_HEIGHT, TRUE);
                update_scrollbars(view);
            }
            return 0;
        case WM_HSCROLL:
            if (view) handle_scroll(view, (HWND)lparam, SB_CTL, wparam);
            return 0;
        case WM_VSCROLL:
            if (view) handle_scroll(view, window, SB_VERT, wparam);
            return 0;
        case WM_MOUSEWHEEL:
            if (view) {
                int delta = GET_WHEEL_DELTA_WPARAM(wparam);
                handle_scroll(view, window, SB_VERT,
                              MAKEWPARAM(delta > 0 ? SB_LINEUP : SB_LINEDOWN,
                                         0));
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            RECT client;
            HDC dc = BeginPaint(window, &paint);
            GetClientRect(window, &client);
            if (view) paint_overlay(view, dc, &client);
            EndPaint(window, &paint);
            return 0;
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static void paint_selection_markers(ViewState *view, int side, HWND window) {
    HDC dc;
    RECT client;
    TEXTMETRICW metrics;
    HFONT font;
    HGDIOBJ previous_font = NULL;
    HPEN pen;
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    int line_height = 16;
    int top_item;
    int index;
    if (!view || side < 0 || side >= 2 || !view->marker_editor[side]) return;
    dc = GetDC(window);
    if (!dc) return;
    GetClientRect(window, &client);
    font = (HFONT)SendMessageW(window, WM_GETFONT, 0, 0);
    if (font) previous_font = SelectObject(dc, font);
    if (GetTextMetricsW(dc, &metrics)) {
        int measured = metrics.tmHeight + metrics.tmExternalLeading + 1;
        if (measured >= 10 && measured <= 40) line_height = measured;
    }
    top_item = g_bc3.get_top_item(view->marker_editor[side]);
    pen = CreatePen(PS_SOLID, 1, RGB(0, 102, 204));
    old_pen = SelectObject(dc, pen);
    old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    for (index = 0; index < view->pair_count; ++index) {
        int item = view->pairs[index].item[side];
        int y;
        HBRUSH marker;
        RECT bar;
        if (item < 0) continue;
        y = (item - top_item) * line_height;
        if (y + line_height < 0 || y >= client.bottom) continue;
        Rectangle(dc, 0, y, client.right, y + line_height);
        bar.left = 0;
        bar.top = y;
        bar.right = 4;
        bar.bottom = y + line_height;
        marker = CreateSolidBrush(RGB(0, 102, 204));
        FillRect(dc, &bar, marker);
        DeleteObject(marker);
    }
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
    if (previous_font) SelectObject(dc, previous_font);
    ReleaseDC(window, dc);
}

static LRESULT CALLBACK marker_window_proc(HWND window, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
    int side = -1;
    ViewState *view = view_from_marker(window, &side);
    WNDPROC original = view && side >= 0 ? view->marker_original[side] : NULL;
    LRESULT result = original
                         ? CallWindowProcW(original, window, message, wparam,
                                           lparam)
                         : DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_PAINT && view) {
        paint_selection_markers(view, side, window);
    }
    if (message == WM_NCDESTROY && view && side >= 0) {
        view->marker_window[side] = NULL;
        view->marker_original[side] = NULL;
        view->marker_editor[side] = NULL;
    }
    return result;
}
