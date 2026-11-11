#include "multidetails.h"

#include "bc3_target.h"
#include "log.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windowsx.h>

#define MAX_VIEWS 8
#define MAX_SELECTIONS 128
#define MAX_PROXY_ITEMS 2048
#define SOURCE_VTABLE_SLOTS 21
#define MAX_EDITOR_SIDES 4
#define MAX_VISIBLE_ROWS 8
#define MAX_PANEL_HEIGHT 360
#define MESSAGE_CLASS L"BC3MultiDetailsNativeMessages"
#define WM_MD_APPLY_SELECTION (WM_APP + 0x310)
#define WM_MD_SYNC_DETAIL (WM_APP + 0x311)
#define SELECTION_CONTROL 0x01
#define SELECTION_SHIFT 0x02
#define EDITOR_OPTION_SELECTION 0x0c
#define EDITOR_OPTION_SINGLE_ROW 0x13
#define EDITOR_COMMAND_COPY 1
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020e
#endif

typedef struct NativeSourceProxy {
    void **vtable;
    void *owner;
    int count;
    int map[MAX_PROXY_ITEMS];
} NativeSourceProxy;

typedef struct SelectedRange {
    int key;
    int first;
    int last;
} SelectedRange;

typedef struct ViewState {
    void *frame;
    void *panel_object;
    void *original_source;
    NativeSourceProxy proxy;
    HWND message_window;
    int initial_panel_height;
    int original_item_count;
    int base_detail_height[MAX_EDITOR_SIDES];
    BYTE base_single_row[MAX_EDITOR_SIDES];
    BOOL base_single_row_captured[MAX_EDITOR_SIDES];
    BYTE base_selection[MAX_EDITOR_SIDES];
    BOOL base_selection_captured[MAX_EDITOR_SIDES];
    int selection_count;
    SelectedRange selections[MAX_SELECTIONS];
    int anchor;
    int active_key;
    LONG sequence;
    LONG last_applied_sequence;
    BOOL proxy_active;
    BOOL syncing;
    BOOL suppress_main_click;
    HWND marker_window[2];
    WNDPROC marker_original[2];
    void *marker_editor[2];
    HWND detail_inner[MAX_EDITOR_SIDES];
    WNDPROC detail_inner_original[MAX_EDITOR_SIDES];
} ViewState;

typedef struct SelectionWork {
    HWND target;
    int source_count;
    int key;
    int flags;
    LONG sequence;
    int first;
    int last;
} SelectionWork;

static HINSTANCE g_instance;
static ViewState g_views[MAX_VIEWS];
static BOOL g_internal_update;
static HHOOK g_message_hook;

/* Recovered TTextEditorItems interface. Native methods still render, select,
   copy, and scroll; only their item coordinate is translated. */
void *g_source_original_vtable[SOURCE_VTABLE_SLOTS];
static void *g_source_proxy_vtable[SOURCE_VTABLE_SLOTS];
static BOOL g_source_vtable_ready;

#define STRINGIZE_INNER(value) #value
#define STRINGIZE(value) STRINGIZE_INNER(value)

__attribute__((naked)) static int proxy_get_count(void) {
    __asm__ __volatile__(
        "movl 8(%eax), %eax\n\t"
        "ret\n\t");
}

#define DEFINE_MAP_THUNK(name, byte_offset)                              \
    __attribute__((naked)) static void name(void) {                      \
        __asm__ __volatile__(                                            \
            "testl %ecx, %ecx\n\t"                                     \
            "js 1f\n\t"                                                \
            "cmpl 8(%eax), %ecx\n\t"                                   \
            "jae 1f\n\t"                                               \
            "movl 12(%eax,%ecx,4), %ecx\n\t"                           \
            "jmp *_g_source_original_vtable+" STRINGIZE(byte_offset)    \
            "\n\t"                                                      \
            "1:\n\t"                                                    \
            "xorl %ecx, %ecx\n\t"                                      \
            "jmp *_g_source_original_vtable+" STRINGIZE(byte_offset)    \
            "\n\t");                                                    \
    }

DEFINE_MAP_THUNK(proxy_method_03, 12)
DEFINE_MAP_THUNK(proxy_method_04, 16)
DEFINE_MAP_THUNK(proxy_method_05, 20)
DEFINE_MAP_THUNK(proxy_method_07, 28)
DEFINE_MAP_THUNK(proxy_method_08, 32)
DEFINE_MAP_THUNK(proxy_method_09, 36)
DEFINE_MAP_THUNK(proxy_method_12, 48)
DEFINE_MAP_THUNK(proxy_method_17, 68)

__attribute__((used)) static void __cdecl proxy_range_to_original(
    NativeSourceProxy *proxy, int *range) {
    int field;
    if (!proxy || !range) return;
    for (field = 1; field <= 3; field += 2) {
        int item = range[field];
        if (item >= 0 && item < proxy->count)
            range[field] = proxy->map[item];
    }
}

__attribute__((used)) static void __cdecl proxy_range_to_local(
    NativeSourceProxy *proxy, int *range) {
    int field;
    if (!proxy || !range) return;
    for (field = 1; field <= 3; field += 2) {
        int original = range[field];
        int item;
        for (item = 0; item < proxy->count; ++item) {
            if (proxy->map[item] == original) break;
        }
        range[field] = item < proxy->count ? item : 0;
    }
}

/* Method 20 receives a single [column, item] position, not a four-field
   selection range. Reading range[3] here corrupts the editor's adjacent
   caret state when the user clicks a later proxy row. */
__attribute__((used)) static void __cdecl proxy_position_to_original(
    NativeSourceProxy *proxy, int *position) {
    int item;
    if (!proxy || !position) return;
    item = position[1];
    if (item >= 0 && item < proxy->count)
        position[1] = proxy->map[item];
}

__attribute__((used)) static void __cdecl proxy_position_to_local(
    NativeSourceProxy *proxy, int *position) {
    int original;
    int item;
    if (!proxy || !position) return;
    original = position[1];
    for (item = 0; item < proxy->count; ++item) {
        if (proxy->map[item] == original) break;
    }
    position[1] = item < proxy->count ? item : 0;
}

/* Methods 18 and 19 receive a four-integer text range:
   [start column, start item, end column, end item]. */
__attribute__((naked)) static void proxy_method_18(void) {
    __asm__ __volatile__(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl %eax, %ebx\n\t"
        "movl %ecx, %esi\n\t"
        "movl %edx, %edi\n\t"
        "pushl %esi\n\t"
        "pushl %ebx\n\t"
        "call _proxy_range_to_original\n\t"
        "addl $8, %esp\n\t"
        "movl %ebx, %eax\n\t"
        "movl %edi, %edx\n\t"
        "movl %esi, %ecx\n\t"
        "call *_g_source_original_vtable+72\n\t"
        "pushl %eax\n\t"
        "pushl %esi\n\t"
        "pushl %ebx\n\t"
        "call _proxy_range_to_local\n\t"
        "addl $8, %esp\n\t"
        "popl %eax\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "leave\n\t"
        "ret\n\t");
}

#define DEFINE_RANGE_STACK_THUNK(name, byte_offset)                     \
    __attribute__((naked)) static void name(void) {                     \
        __asm__ __volatile__(                                           \
            "pushl %ebp\n\t"                                          \
            "movl %esp, %ebp\n\t"                                     \
            "pushl %ebx\n\t"                                         \
            "pushl %esi\n\t"                                         \
            "pushl %edi\n\t"                                         \
            "movl %eax, %ebx\n\t"                                    \
            "movl %ecx, %esi\n\t"                                    \
            "movl %edx, %edi\n\t"                                    \
            "pushl %esi\n\t"                                         \
            "pushl %ebx\n\t"                                         \
            "call _proxy_range_to_original\n\t"                      \
            "addl $8, %esp\n\t"                                      \
            "movl %ebx, %eax\n\t"                                    \
            "movl %edi, %edx\n\t"                                    \
            "movl %esi, %ecx\n\t"                                    \
            "pushl 8(%ebp)\n\t"                                      \
            "call *_g_source_original_vtable+" STRINGIZE(byte_offset) \
            "\n\t"                                                    \
            "pushl %eax\n\t"                                         \
            "pushl %esi\n\t"                                         \
            "pushl %ebx\n\t"                                         \
            "call _proxy_range_to_local\n\t"                         \
            "addl $8, %esp\n\t"                                      \
            "popl %eax\n\t"                                          \
            "popl %edi\n\t"                                          \
            "popl %esi\n\t"                                          \
            "popl %ebx\n\t"                                          \
            "leave\n\t"                                              \
            "ret $4\n\t");                                           \
    }

DEFINE_RANGE_STACK_THUNK(proxy_method_19, 76)

/* Method 20 receives [column, item] plus one stack argument. */
__attribute__((naked)) static void proxy_method_20(void) {
    __asm__ __volatile__(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl %eax, %ebx\n\t"
        "movl %ecx, %esi\n\t"
        "movl %edx, %edi\n\t"
        "pushl %esi\n\t"
        "pushl %ebx\n\t"
        "call _proxy_position_to_original\n\t"
        "addl $8, %esp\n\t"
        "movl %ebx, %eax\n\t"
        "movl %edi, %edx\n\t"
        "movl %esi, %ecx\n\t"
        "pushl 8(%ebp)\n\t"
        "call *_g_source_original_vtable+80\n\t"
        "pushl %eax\n\t"
        "pushl %esi\n\t"
        "pushl %ebx\n\t"
        "call _proxy_position_to_local\n\t"
        "addl $8, %esp\n\t"
        "popl %eax\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "leave\n\t"
        "ret $4\n\t");
}

static LRESULT CALLBACK message_window_proc(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK marker_window_proc(HWND window, UINT message,
                                           WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK detail_inner_proc(HWND window, UINT message,
                                          WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK ui_message_hook(int code, WPARAM wparam,
                                        LPARAM lparam);

static ViewState *view_find(void *frame, BOOL create) {
    int index;
    ViewState *empty = NULL;
    for (index = 0; index < MAX_VIEWS; ++index) {
        if (g_views[index].frame == frame) return &g_views[index];
        if (!g_views[index].frame && !empty) empty = &g_views[index];
    }
    if (!create || !empty) return NULL;
    ZeroMemory(empty, sizeof(*empty));
    empty->frame = frame;
    empty->anchor = -1;
    empty->active_key = -1;
    return empty;
}

static BOOL CALLBACK find_editor_window_child(HWND window,
                                               LPARAM parameter) {
    HWND *result = (HWND *)parameter;
    WCHAR class_name[128];
    GetClassNameW(window, class_name, 128);
    if (wcsstr(class_name, L"TUiEditorWindow")) {
        *result = window;
        return FALSE;
    }
    return TRUE;
}

static HWND editor_inner_window(void *editor) {
    HWND outer = bc3_find_control_window(editor);
    HWND inner = NULL;
    if (!outer) return NULL;
    EnumChildWindows(outer, find_editor_window_child, (LPARAM)&inner);
    return inner ? inner : outer;
}

static ViewState *view_from_marker(HWND window, int *side_out) {
    int view_index;
    int side;
    for (view_index = 0; view_index < MAX_VIEWS; ++view_index) {
        for (side = 0; side < 2; ++side) {
            if (g_views[view_index].marker_window[side] == window) {
                if (side_out) *side_out = side;
                return &g_views[view_index];
            }
        }
    }
    return NULL;
}

static ViewState *view_from_detail(HWND window, int *side_out) {
    int view_index;
    int side;
    for (view_index = 0; view_index < MAX_VIEWS; ++view_index) {
        for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
            if (g_views[view_index].detail_inner[side] == window) {
                if (side_out) *side_out = side;
                return &g_views[view_index];
            }
        }
    }
    return NULL;
}

static void initialize_proxy_vtable(void *source) {
    void **vtable;
    if (g_source_vtable_ready || !source) return;
    vtable = *(void ***)source;
    if (!vtable) return;
    memcpy(g_source_original_vtable, vtable,
           sizeof(g_source_original_vtable));
    memcpy(g_source_proxy_vtable, vtable, sizeof(g_source_proxy_vtable));
    g_source_proxy_vtable[1] = proxy_get_count;
    g_source_proxy_vtable[3] = proxy_method_03;
    g_source_proxy_vtable[4] = proxy_method_04;
    g_source_proxy_vtable[5] = proxy_method_05;
    g_source_proxy_vtable[7] = proxy_method_07;
    g_source_proxy_vtable[8] = proxy_method_08;
    g_source_proxy_vtable[9] = proxy_method_09;
    g_source_proxy_vtable[12] = proxy_method_12;
    /* Keep the model's native read lock/unlock pair (slots 15 and 16).
       Painting can overlap BC3's background comparison worker on Windows;
       bypassing this pair can expose a half-updated item array to the detail
       editor.  Both methods only dereference source->owner, which the proxy
       preserves at offset 4. */
    g_source_proxy_vtable[17] = proxy_method_17;
    g_source_proxy_vtable[18] = proxy_method_18;
    g_source_proxy_vtable[19] = proxy_method_19;
    g_source_proxy_vtable[20] = proxy_method_20;
    g_source_vtable_ready = TRUE;
}

static void ensure_message_window(ViewState *view) {
    if (view->message_window && IsWindow(view->message_window)) return;
    view->message_window = CreateWindowExW(
        0, MESSAGE_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL,
        g_instance, view);
}

static void ensure_window_hooks(ViewState *view) {
    int side;
    for (side = 0; side < 2; ++side) {
        void *editor = bc3_frame_main_editor(view->frame, side);
        HWND inner = editor_inner_window(editor);
        if (inner && (!view->marker_window[side] ||
                      !IsWindow(view->marker_window[side]))) {
            view->marker_editor[side] = editor;
            view->marker_window[side] = inner;
            view->marker_original[side] = (WNDPROC)SetWindowLongPtrW(
                inner, GWLP_WNDPROC, (LONG_PTR)marker_window_proc);
        }
    }
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        HWND inner = editor ? editor_inner_window(editor) : NULL;
        if (inner && (!view->detail_inner[side] ||
                      !IsWindow(view->detail_inner[side]))) {
            view->detail_inner[side] = inner;
            view->detail_inner_original[side] =
                (WNDPROC)SetWindowLongPtrW(
                    inner, GWLP_WNDPROC, (LONG_PTR)detail_inner_proc);
        }
    }
    if (!g_message_hook) {
        DWORD thread = GetCurrentThreadId();
        g_message_hook = SetWindowsHookExW(
            WH_GETMESSAGE, ui_message_hook, g_instance,
            thread);
        if (!g_message_hook)
            log_line("Native detail copy command hook failed");
    }
}

static BOOL ensure_view(ViewState *view) {
    void *detail;
    void *observed_source;
    int side;
    if (!view || !view->frame) return FALSE;
    view->panel_object = bc3_frame_details_panel(view->frame);
    detail = bc3_frame_detail_editor(view->frame, 0);
    if (!view->panel_object || !detail) return FALSE;
    if (!view->initial_panel_height) {
        view->initial_panel_height =
            *(int *)((uint8_t *)view->panel_object + 0x4c);
    }
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        if (editor && !view->base_detail_height[side]) {
            view->base_detail_height[side] =
                *(int *)((uint8_t *)editor + 0x4c);
        }
        if (editor && !view->base_single_row_captured[side]) {
            void *core = *(void **)((uint8_t *)editor + 0x280);
            if (core) {
                view->base_single_row[side] =
                    *(BYTE *)((uint8_t *)core + 0x284 +
                              EDITOR_OPTION_SINGLE_ROW);
                view->base_single_row_captured[side] = TRUE;
                view->base_selection[side] =
                    *(BYTE *)((uint8_t *)core + 0x284 +
                              EDITOR_OPTION_SELECTION);
                view->base_selection_captured[side] = TRUE;
            }
        }
    }
    observed_source = bc3_editor_data_source(detail);
    if (observed_source && observed_source != &view->proxy &&
        observed_source != view->original_source) {
        /* A comparison reload can reuse the same frame/editor while replacing
           its TTextEditorItems instance.  Never retain the previous owner's
           pointer: it is freed as soon as BC3 finishes rebinding the view. */
        view->original_source = observed_source;
        view->original_item_count = bc3_editor_item_count(detail);
        view->selection_count = 0;
        view->anchor = -1;
        view->active_key = -1;
        view->proxy_active = FALSE;
        initialize_proxy_vtable(view->original_source);
        if (view->original_source && g_source_vtable_ready) {
            view->proxy.vtable = g_source_proxy_vtable;
            view->proxy.owner =
                *(void **)((uint8_t *)view->original_source + 4);
        }
    } else if (observed_source == view->original_source &&
               view->original_source) {
        /* The interface wrapper is usually stable, but its model owner may be
           replaced by an in-place session reload. */
        view->original_item_count = bc3_editor_item_count(detail);
        view->proxy.owner =
            *(void **)((uint8_t *)view->original_source + 4);
    }
    ensure_message_window(view);
    ensure_window_hooks(view);
    return view->original_source && view->message_window &&
           g_source_vtable_ready;
}

static int selection_index(const ViewState *view, int key) {
    int index;
    for (index = 0; index < view->selection_count; ++index) {
        if (view->selections[index].first <= key &&
            key <= view->selections[index].last) {
            return index;
        }
    }
    return -1;
}

static void remove_selection(ViewState *view, int index) {
    if (index < 0 || index >= view->selection_count) return;
    if (index + 1 < view->selection_count) {
        memmove(&view->selections[index], &view->selections[index + 1],
                (size_t)(view->selection_count - index - 1) *
                    sizeof(view->selections[0]));
    }
    --view->selection_count;
}

static int compare_selection(const void *left, const void *right) {
    const SelectedRange *a = (const SelectedRange *)left;
    const SelectedRange *b = (const SelectedRange *)right;
    return (a->first > b->first) - (a->first < b->first);
}

static void normalize_selections(ViewState *view) {
    int read_index;
    int write_index = 0;
    if (view->selection_count <= 1) return;
    qsort(view->selections, (size_t)view->selection_count,
          sizeof(view->selections[0]), compare_selection);
    for (read_index = 0; read_index < view->selection_count; ++read_index) {
        SelectedRange current = view->selections[read_index];
        if (write_index > 0 &&
            current.first <= view->selections[write_index - 1].last + 1) {
            if (current.last > view->selections[write_index - 1].last)
                view->selections[write_index - 1].last = current.last;
            if (current.key == view->active_key)
                view->selections[write_index - 1].key = current.key;
        } else {
            view->selections[write_index++] = current;
        }
    }
    view->selection_count = write_index;
}

static void add_selection(ViewState *view, int key, int first, int last) {
    SelectedRange *selection;
    if (view->selection_count >= MAX_SELECTIONS) return;
    selection = &view->selections[view->selection_count++];
    selection->key = key;
    selection->first = first;
    selection->last = last;
    normalize_selections(view);
}

static void invalidate_native_editors(ViewState *view) {
    int side;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        HWND outer = editor ? bc3_find_control_window(editor) : NULL;
        if (outer) InvalidateRect(outer, NULL, TRUE);
    }
    for (side = 0; side < 2; ++side) {
        if (view->marker_window[side])
            /* This native window has no background brush of its own.  Asking
               Win32 to erase it exposes an unpainted black rectangle between
               BC3's row-level paint passes.  The native current-item setter
               already invalidates the old and new row geometry; this broad
               refresh only needs to schedule paint without erasure. */
            InvalidateRect(view->marker_window[side], NULL, FALSE);
    }
}

static void update_native_layout(ViewState *view, BOOL reset) {
    int side;
    int rows = reset ? 1 : view->proxy.count;
    int visible_rows;
    int desired = 3;
    int active_count = 0;
    if (!view->panel_object) return;
    if (rows < 1) rows = 1;
    visible_rows = rows > MAX_VISIBLE_ROWS ? MAX_VISIBLE_ROWS : rows;
    g_internal_update = TRUE;
    for (side = 0; side < 2; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        int base = view->base_detail_height[side];
        int height;
        if (!editor || base <= 0) continue;
        height = reset ? base : base * visible_rows;
        g_bc3.set_control_height(editor, height);
        desired += height + 1;
        ++active_count;
    }
    if (active_count == 0) desired = view->initial_panel_height;
    if (desired < view->initial_panel_height)
        desired = view->initial_panel_height;
    if (desired > MAX_PANEL_HEIGHT) {
        int each = (MAX_PANEL_HEIGHT - 3 - active_count) /
                   (active_count ? active_count : 1);
        for (side = 0; side < 2; ++side) {
            void *editor = bc3_frame_detail_editor(view->frame, side);
            if (editor && view->base_detail_height[side] > 0)
                g_bc3.set_control_height(editor, each);
        }
        desired = MAX_PANEL_HEIGHT;
    }
    if (*(int *)((uint8_t *)view->panel_object + 0x4c) != desired)
        g_bc3.set_control_height(view->panel_object, desired);
    g_internal_update = FALSE;
}

static void restore_original_source(ViewState *view) {
    int side;
    if (!view || !view->proxy_active || !view->original_source) return;
    view->syncing = TRUE;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        int proxy_item;
        int original_item = 0;
        int left_x;
        if (!editor) continue;
        proxy_item = g_bc3.get_cur_item(editor);
        if (proxy_item >= 0 && proxy_item < view->proxy.count)
            original_item = view->proxy.map[proxy_item];
        left_x = g_bc3.get_left_x(editor);
        log_line("Restoring native detail side %d source", side);
        g_bc3.set_data_source(editor, view->original_source);
        if (view->base_single_row_captured[side]) {
            g_bc3.set_option(editor, EDITOR_OPTION_SINGLE_ROW,
                             view->base_single_row[side] != 0);
        }
        if (view->base_selection_captured[side]) {
            g_bc3.set_option(editor, EDITOR_OPTION_SELECTION,
                             view->base_selection[side] != 0);
        }
        log_line("Native detail side %d source restored", side);
        if (original_item < bc3_editor_item_count(editor))
            g_bc3.set_cur_item(editor, original_item);
        g_bc3.set_left_x(editor, left_x);
    }
    view->proxy_active = FALSE;
    view->syncing = FALSE;
}

static int build_proxy_map(const ViewState *view, int *map, int capacity) {
    int selection_index_value;
    int count = 0;
    for (selection_index_value = 0;
         selection_index_value < view->selection_count;
         ++selection_index_value) {
        int item;
        const SelectedRange *selection =
            &view->selections[selection_index_value];
        for (item = selection->first; item <= selection->last; ++item) {
            if (count >= capacity) {
                log_line("Native selected-row view capped at %d internal items",
                         capacity);
                return count;
            }
            map[count++] = item;
        }
    }
    return count;
}

static int proxy_index_for_original(const int *map, int count, int item) {
    int index;
    for (index = 0; index < count; ++index) {
        if (map[index] == item) return index;
    }
    return 0;
}

static void apply_proxy_source(ViewState *view) {
    int pending_map[MAX_PROXY_ITEMS];
    int pending_count;
    int side;
    int active_proxy_item;
    int shared_left_x = 0;
    if (!ensure_view(view)) return;
    pending_count = build_proxy_map(view, pending_map, MAX_PROXY_ITEMS);
    log_line("Applying native selected-row source: %d item(s)",
             pending_count);
    if (pending_count <= 0) {
        /* Restore while the old map still exists so the current proxy row can
           be translated back to its original comparison row. */
        restore_original_source(view);
        update_native_layout(view, TRUE);
        invalidate_native_editors(view);
        return;
    }
    active_proxy_item = proxy_index_for_original(
        pending_map, pending_count, view->active_key);
    view->syncing = TRUE;

    /* First detach every detail editor.  The proxy object has a stable
       address, so changing its map while another editor is still bound lets
       native paint/caret callbacks observe a new count with the old caches. */
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        if (!editor) continue;
        if (side == 0) shared_left_x = g_bc3.get_left_x(editor);
        g_bc3.set_option(editor, EDITOR_OPTION_SELECTION, TRUE);
        g_bc3.set_option(editor, EDITOR_OPTION_SINGLE_ROW, FALSE);
        g_bc3.set_data_source(editor, NULL);
    }

    memcpy(view->proxy.map, pending_map,
           (size_t)pending_count * sizeof(pending_map[0]));
    view->proxy.count = pending_count;

    /* SetDataSource synchronously calls CurrentChanged and both native style
       callbacks.  Publish the active state before the first bind so those
       callbacks translate local proxy coordinates instead of feeding them
       back into the shorter proxy as original comparison coordinates. */
    view->proxy_active = TRUE;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        if (!editor) continue;
        log_line("Binding native detail side %d", side);
        /* SetDataSource rebuilds the native editor's item/scroll caches from
           the already-published proxy count. */
        g_bc3.set_data_source(editor, &view->proxy);
        log_line("Native detail side %d source rebound", side);
        /* SetDataSource has already rebuilt the native editor caches.  Do not
           run the public current-item transaction here: its callback and
           selection walk address the frame's original TextContent rows, but
           active_proxy_item is a local proxy coordinate.  The transaction is
           used for main-editor synchronization below, where the coordinate
           is native. */
        bc3_editor_override_current_item(editor, active_proxy_item);
        log_line("Native detail side %d current item set", side);
        bc3_editor_override_top_item(editor, 0);
        log_line("Native detail side %d top item set", side);
    }
    update_native_layout(view, FALSE);
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        if (editor) g_bc3.set_left_x(editor, shared_left_x);
    }
    view->syncing = FALSE;
    invalidate_native_editors(view);
    log_line("Native selected-row view active: %d selection(s), %d internal item(s)",
             view->selection_count, view->proxy.count);
}

static void apply_selection_result(ViewState *view, SelectionWork *work) {
    BOOL control;
    BOOL shift;
    int index;
    int first;
    int last;
    if (!view || !work || work->sequence <= view->last_applied_sequence)
        return;
    view->last_applied_sequence = work->sequence;
    log_line("Applying click selection key=%d flags=%d sequence=%ld",
             work->key, work->flags, (long)work->sequence);
    first = work->first;
    last = work->last;
    if (first > last) {
        int temporary = first;
        first = last;
        last = temporary;
    }
    if (first < 0 || last >= work->source_count ||
        last - first > 32 || work->key < first || work->key > last) {
        first = work->key;
        last = work->key;
    }
    control = (work->flags & SELECTION_CONTROL) != 0;
    shift = (work->flags & SELECTION_SHIFT) != 0;
    if (shift && view->anchor >= 0) {
        int range_first = view->anchor < first ? view->anchor : first;
        int range_last = view->anchor > last ? view->anchor : last;
        if (!control) view->selection_count = 0;
        add_selection(view, work->key, range_first, range_last);
    } else if (control) {
        index = selection_index(view, work->key);
        if (index >= 0)
            remove_selection(view, index);
        else
            add_selection(view, work->key, first, last);
    } else {
        view->selection_count = 0;
        add_selection(view, work->key, first, last);
    }
    view->anchor = work->key;
    view->active_key = work->key;
    apply_proxy_source(view);
}

static void queue_selection(ViewState *view, int key, int flags) {
    SelectionWork *work;
    void *model;
    if (!ensure_view(view)) return;
    model = bc3_frame_main_editor(view->frame, 0);
    if (!model) return;
    work = (SelectionWork *)calloc(1, sizeof(*work));
    if (!work) return;
    work->target = view->message_window;
    work->source_count = bc3_editor_item_count(model);
    work->key = key;
    work->flags = flags;
    work->sequence = InterlockedIncrement(&view->sequence);
    work->first = key;
    work->last = key;
    if (bc3_editor_physical_line_bounds(model, key,
                                        &work->first, &work->last)) {
        log_line("Native physical line bounds for item %d: %d..%d", key,
                 work->first, work->last);
    }
    if (!PostMessageW(work->target, WM_MD_APPLY_SELECTION, 0,
                      (LPARAM)work)) {
        free(work);
    }
}

static void sync_detail_horizontal(ViewState *view, int source_side) {
    void *source_editor;
    int left_x;
    int side;
    if (!view || view->syncing) return;
    source_editor = bc3_frame_detail_editor(view->frame, source_side);
    if (!source_editor) return;
    left_x = g_bc3.get_left_x(source_editor);
    view->syncing = TRUE;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(view->frame, side);
        HWND inner;
        if (!editor || editor == source_editor) continue;
        g_bc3.set_left_x(editor, left_x);
        inner = editor_inner_window(editor);
        if (inner) InvalidateRect(inner, NULL, FALSE);
    }
    view->syncing = FALSE;
}

static void sync_main_current(ViewState *view, void *sender,
                              int original_item) {
    BOOL was_syncing;
    int side;
    if (!view || original_item < 0) return;
    was_syncing = view->syncing;
    view->syncing = TRUE;
    for (side = 0; side < 2; ++side) {
        void *editor = bc3_frame_main_editor(view->frame, side);
        if (!editor || editor == sender ||
            original_item >= bc3_editor_item_count(editor)) {
            continue;
        }
        /* Use BC3's native setters.  Besides the logical item, these update
           the visual-row/caret caches and invalidate the old and new item
           ranges.  Writing core+0x2ac directly leaves the current-row frame
           at a stale Y coordinate after scrolling, wrapping, or resize. */
        g_bc3.set_cur_item(editor, original_item);
    }
    view->syncing = was_syncing;
}

static void sync_upper_from_detail(ViewState *view, int detail_side) {
    void *detail;
    int proxy_item;
    int original_item;
    if (!view || view->syncing || !view->proxy_active) return;
    detail = bc3_frame_detail_editor(view->frame, detail_side);
    if (!detail) return;
    proxy_item = g_bc3.get_cur_item(detail);
    if (proxy_item < 0 || proxy_item >= view->proxy.count) return;
    original_item = view->proxy.map[proxy_item];
    log_line("Native detail item %d maps to original item %d",
             proxy_item, original_item);
    view->active_key = original_item;
    sync_main_current(view, NULL, original_item);
    /* Updating the upper current items can move Win32 focus back to a main
       editor.  Return it to the native detail window that owns the caret so
       Home/End, drag selection, and Ctrl+C continue in the clicked row. */
    if (detail_side >= 0 && detail_side < MAX_EDITOR_SIDES &&
        view->detail_inner[detail_side] &&
        IsWindow(view->detail_inner[detail_side])) {
        SetFocus(view->detail_inner[detail_side]);
    }
}

BOOL multidetails_initialize(void) {
    WNDCLASSW window_class;
    g_instance = (HINSTANCE)GetModuleHandleW(L"version.dll");
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.lpfnWndProc = message_window_proc;
    window_class.hInstance = g_instance;
    window_class.lpszClassName = MESSAGE_CLASS;
    if (!RegisterClassW(&window_class) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return FALSE;
    }
    return TRUE;
}

void multidetails_shutdown(void) {
    int view_index;
    for (view_index = 0; view_index < MAX_VIEWS; ++view_index) {
        ViewState *view = &g_views[view_index];
        int side;
        if (!view->frame) continue;
        restore_original_source(view);
        for (side = 0; side < 2; ++side) {
            if (view->marker_window[side] && view->marker_original[side] &&
                IsWindow(view->marker_window[side])) {
                SetWindowLongPtrW(view->marker_window[side], GWLP_WNDPROC,
                                  (LONG_PTR)view->marker_original[side]);
            }
        }
        for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
            if (view->detail_inner[side] &&
                view->detail_inner_original[side] &&
                IsWindow(view->detail_inner[side])) {
                SetWindowLongPtrW(view->detail_inner[side], GWLP_WNDPROC,
                                  (LONG_PTR)view->detail_inner_original[side]);
            }
        }
        if (view->message_window && IsWindow(view->message_window))
            DestroyWindow(view->message_window);
        ZeroMemory(view, sizeof(*view));
    }
    if (g_message_hook) {
        UnhookWindowsHookEx(g_message_hook);
        g_message_hook = NULL;
    }
}

BOOL multidetails_is_internal_update(void) {
    return g_internal_update;
}

BOOL multidetails_before_click(void *frame, void *sender) {
    ViewState *view;
    void *model;
    int key;
    int side;
    int flags = 0;
    if (g_internal_update || !frame) return FALSE;
    view = view_find(frame, TRUE);
    if (!view || !ensure_view(view)) return FALSE;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        if (sender == bc3_frame_detail_editor(frame, side))
            return FALSE;
    }
    model = bc3_frame_main_editor(frame, 0);
    if (!model || (sender != bc3_frame_main_editor(frame, 0) &&
                   sender != bc3_frame_main_editor(frame, 1))) {
        return FALSE;
    }
    key = g_bc3.get_cur_item(model);
    if (key < 0 || key >= bc3_editor_item_count(model)) return FALSE;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
        flags |= SELECTION_CONTROL;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
        flags |= SELECTION_SHIFT;
    log_line("Queueing native main click key=%d flags=%d", key, flags);
    /* This hook runs while BC3's main editor still owns the comparison-model
       read lock.  Rebinding the detail editors here either recurses into the
       same non-recursive lock or forces the proxy to paint without locking.
       Post the work so the frame click unwinds first; the proxy can then use
       the model's real lock/unlock pair on every native paint/caret request. */
    queue_selection(view, key, flags);
    view->suppress_main_click = TRUE;
    /* The main TUiEditorWindow has already updated its native current row.
       Skip BC3's frame-level handler so it cannot schedule a competing
       one-row detail rebuild before the queued multi-row bind runs. */
    return TRUE;
}

void multidetails_on_click(void *frame, void *sender) {
    ViewState *view;
    void *driver;
    int key;
    int side;
    int flags = 0;
    if (g_internal_update || !frame) return;
    view = view_find(frame, TRUE);
    if (!view || !ensure_view(view)) return;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        if (sender == bc3_frame_detail_editor(frame, side)) {
            log_line("Native detail click on side %d", side);
            /* The detail window posts synchronization after WM_LBUTTONUP.
               Waiting preserves BC3's native caret/drag-selection state. */
            return;
        }
    }
    /* A main-editor mouse click is handled by marker_window_proc after
       BC3 updates its current item. VCL raises OnClick later for the same
       gesture; do not apply that gesture twice (the detail editors may
       already be rebound to the selected-row proxy by then). */
    if (view->suppress_main_click) {
        view->suppress_main_click = FALSE;
        return;
    }
    driver = bc3_frame_main_editor(frame, 0);
    if (!driver) return;
    key = g_bc3.get_cur_item(driver);
    if (key < 0) return;
    log_line("Queued native click key=%d", key);
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
        flags |= SELECTION_CONTROL;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
        flags |= SELECTION_SHIFT;
    queue_selection(view, key, flags);
}

BOOL multidetails_on_current_changed(void *frame, void *sender) {
    ViewState *view;
    int detail_side;
    int side;
    int key;
    int caret_x;
    int left_length;
    int right_length;
    if (!frame || !sender) return FALSE;
    view = view_find(frame, TRUE);
    if (!view || !ensure_view(view)) return FALSE;
    if (view->syncing) {
        /* A native setter invoked by this module must still complete its
           internal geometry/cache transaction, but its frame-level callback
           must not fan the original item index back into the proxy editors. */
        return TRUE;
    }
    for (side = 0; side < 2; ++side) {
        if (sender == bc3_frame_main_editor(frame, side)) break;
    }
    if (side < 2) {
        key = g_bc3.get_cur_item(sender);
        if (key < 0) return FALSE;
        left_length = bc3_editor_underlying_line_length(sender, key, 0);
        right_length = bc3_editor_underlying_line_length(sender, key, 1);
        if (left_length <= 4096 && right_length <= 4096 &&
            !view->proxy_active) {
            return FALSE;
        }
        sync_main_current(view, sender, key);
        log_line("Native upper current item synchronized without proxy fan-out: %d",
                 key);
        return TRUE;
    }

    if (!view->proxy_active) return FALSE;
    for (detail_side = 0; detail_side < MAX_EDITOR_SIDES;
         ++detail_side) {
        if (sender == bc3_frame_detail_editor(frame, detail_side)) break;
    }
    if (detail_side >= MAX_EDITOR_SIDES) return FALSE;
    key = g_bc3.get_cur_item(sender);
    if (key < 0 || key >= view->proxy.count) return TRUE;
    caret_x = g_bc3.get_caret_x(sender);
    view->active_key = view->proxy.map[key];
    view->syncing = TRUE;
    for (side = 0; side < MAX_EDITOR_SIDES; ++side) {
        void *editor = bc3_frame_detail_editor(frame, side);
        if (!editor || editor == sender) continue;
        g_bc3.set_cur_item(editor, key);
        if (caret_x >= 0) g_bc3.set_caret_x(editor, caret_x);
    }
    view->syncing = FALSE;
    sync_main_current(view, NULL, view->active_key);
    log_line("Native proxy item %d synchronized to original item %d",
             key, view->active_key);
    return TRUE;
}

int multidetails_map_proxy_item(void *core, int item) {
    void *source;
    int view_index;
    if (!core || item < 0) return item;
    source = *(void **)((uint8_t *)core + 0x310);
    for (view_index = 0; view_index < MAX_VIEWS; ++view_index) {
        ViewState *view = &g_views[view_index];
        if (!view->frame) {
            continue;
        }
        if (source != &view->proxy) continue;
        if (item < view->proxy.count)
            return view->proxy.map[item];
        return item;
    }
    return item;
}

void multidetails_on_details_resize(void *frame) {
    ViewState *view;
    if (g_internal_update || !frame) return;
    view = view_find(frame, FALSE);
    if (!view) return;
    invalidate_native_editors(view);
}

static LRESULT CALLBACK message_window_proc(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) {
    ViewState *view =
        (ViewState *)GetWindowLongPtrW(window, GWLP_USERDATA);
    (void)wparam;
    if (message == WM_NCCREATE) {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          (LONG_PTR)create->lpCreateParams);
        return TRUE;
    }
    if (message == WM_MD_APPLY_SELECTION) {
        SelectionWork *work = (SelectionWork *)lparam;
        log_line("Native selection message received");
        if (view && work) apply_selection_result(view, work);
        free(work);
        return 0;
    }
    if (message == WM_MD_SYNC_DETAIL) {
        int side = (int)wparam;
        if (view && side >= 0 && side < MAX_EDITOR_SIDES) {
            log_line("Synchronizing upper editors from native detail side %d",
                     side);
            sync_upper_from_detail(view, side);
            log_line("Upper editors synchronized from native detail side %d",
                     side);
        }
        return 0;
    }
    if (message == WM_NCDESTROY && view)
        view->message_window = NULL;
    return DefWindowProcW(window, message, wparam, lparam);
}

static LRESULT CALLBACK marker_window_proc(HWND window, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
    int side = -1;
    ViewState *view = view_from_marker(window, &side);
    WNDPROC original = view && side >= 0 ? view->marker_original[side]
                                        : NULL;
    if (message == WM_LBUTTONDOWN && view) {
        view->suppress_main_click = TRUE;
    }
    LRESULT result = original
                         ? CallWindowProcW(original, window, message, wparam,
                                           lparam)
                         : DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_NCDESTROY && view && side >= 0) {
        view->marker_window[side] = NULL;
        view->marker_original[side] = NULL;
        view->marker_editor[side] = NULL;
    }
    if (message == WM_LBUTTONUP && view)
        view->suppress_main_click = FALSE;
    return result;
}

static LRESULT CALLBACK detail_inner_proc(HWND window, UINT message,
                                          WPARAM wparam, LPARAM lparam) {
    int side = -1;
    ViewState *view = view_from_detail(window, &side);
    WNDPROC original = view && side >= 0
                           ? view->detail_inner_original[side]
                           : NULL;
    LRESULT result = original
                         ? CallWindowProcW(original, window, message, wparam,
                                           lparam)
                         : DefWindowProcW(window, message, wparam, lparam);
    if (view && (message == WM_LBUTTONUP || message == WM_KEYUP ||
                 message == WM_HSCROLL || message == WM_MOUSEHWHEEL)) {
        if (message == WM_LBUTTONUP) {
            log_line("Native detail window click on side %d", side);
            PostMessageW(view->message_window, WM_MD_SYNC_DETAIL,
                         (WPARAM)side, 0);
        } else {
            sync_detail_horizontal(view, side);
        }
    }
    if (message == WM_NCDESTROY && view && side >= 0) {
        view->detail_inner[side] = NULL;
        view->detail_inner_original[side] = NULL;
    }
    return result;
}

static LRESULT CALLBACK ui_message_hook(int code, WPARAM wparam,
                                        LPARAM lparam) {
    if (code >= 0 && wparam == PM_REMOVE) {
        MSG *message = (MSG *)lparam;
        if (message && message->message == WM_KEYDOWN &&
            (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
            (message->wParam == 'C' || message->wParam == VK_INSERT)) {
            int side = -1;
            HWND focused = GetFocus();
            ViewState *view = view_from_detail(focused, &side);
            if (view && view->proxy_active && side >= 0) {
                void *editor =
                    bc3_frame_detail_editor(view->frame, side);
                if (editor) {
                    g_bc3.execute_command(editor, EDITOR_COMMAND_COPY,
                                          NULL);
                    message->message = WM_NULL;
                    message->wParam = 0;
                    message->lParam = 0;
                }
            }
        }
    }
    return CallNextHookEx(g_message_hook, code, wparam, lparam);
}
