#include "bc3_target.h"

#include "log.h"

#include <stddef.h>
#include <stdlib.h>

Bc3Api g_bc3;

void *g_raw_get_cur_item;
void *g_raw_set_cur_item;
void *g_raw_get_x_count;
void *g_raw_get_left_x;
void *g_raw_set_left_x;
void *g_raw_get_max_left_x_core;
void *g_raw_get_top_item;
void *g_raw_set_top_item;
void *g_raw_set_control_height;
void *g_raw_current_changed;

__attribute__((naked)) static int call_get_cur_item(
    void *editor __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "call *_g_raw_get_cur_item\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void call_set_cur_item(
    void *editor __attribute__((unused)), int item __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "call *_g_raw_set_cur_item\n\t"
        "ret\n\t");
}

static void set_cached_caret_x(void *editor, int x) {
    void *core;
    if (!editor) return;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return;
    if (x < 0) x = 0;
    *(int *)((uint8_t *)core + 0x2a8) = x;
}

__attribute__((naked)) static int call_get_x_count(
    void *editor __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "call *_g_raw_get_x_count\n\t"
        "ret\n\t");
}

__attribute__((naked)) static int call_get_left_x(
    void *editor __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "call *_g_raw_get_left_x\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void call_set_left_x(
    void *editor __attribute__((unused)), int x __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "call *_g_raw_set_left_x\n\t"
        "ret\n\t");
}

__attribute__((naked)) static int call_get_max_left_x(
    void *editor __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 0x280(%eax), %eax\n\t"
        "call *_g_raw_get_max_left_x_core\n\t"
        "ret\n\t");
}

__attribute__((naked)) static int call_get_top_item(
    void *editor __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "call *_g_raw_get_top_item\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void call_set_top_item(
    void *editor __attribute__((unused)), int item __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "call *_g_raw_set_top_item\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void call_set_control_height(
    void *control __attribute__((unused)), int height __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "call *_g_raw_set_control_height\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void call_current_changed(
    void *frame __attribute__((unused)), void *sender __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "call *_g_raw_current_changed\n\t"
        "ret\n\t");
}

static BOOL validate_executable_identity(void) {
    WCHAR path[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    ULARGE_INTEGER size;

    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) {
        return FALSE;
    }
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attributes)) {
        return FALSE;
    }
    size.HighPart = attributes.nFileSizeHigh;
    size.LowPart = attributes.nFileSizeLow;
    if (size.QuadPart != BC3_EXPECTED_EXE_SIZE) {
        log_line("Target size mismatch: %lu:%lu", size.HighPart, size.LowPart);
        return FALSE;
    }
    return TRUE;
}

BOOL bc3_target_initialize(void) {
    uint8_t *base;
    if (!validate_executable_identity()) {
        return FALSE;
    }
    base = (uint8_t *)GetModuleHandleW(NULL);
    if (!base) {
        return FALSE;
    }
    g_bc3.module_base = base;
    g_raw_get_cur_item = base + BC3_RVA_EDITOR_GET_CUR_ITEM;
    g_raw_set_cur_item = base + BC3_RVA_EDITOR_SET_CUR_ITEM;
    g_raw_get_x_count = base + BC3_RVA_EDITOR_GET_X_COUNT;
    g_raw_get_left_x = base + BC3_RVA_EDITOR_GET_LEFT_X;
    g_raw_set_left_x = base + BC3_RVA_EDITOR_SET_LEFT_X;
    g_raw_get_max_left_x_core =
        base + BC3_RVA_EDITOR_GET_MAX_LEFT_X_CORE;
    g_raw_get_top_item = base + BC3_RVA_EDITOR_GET_TOP_ITEM;
    g_raw_set_top_item = base + BC3_RVA_EDITOR_SET_TOP_ITEM;
    g_raw_set_control_height = base + BC3_RVA_CONTROL_SET_HEIGHT;
    g_raw_current_changed = base + BC3_RVA_EDITOR_CUR_CHANGED;
    g_bc3.get_cur_item = call_get_cur_item;
    g_bc3.set_cur_item = call_set_cur_item;
    g_bc3.set_caret_x = set_cached_caret_x;
    g_bc3.get_x_count = call_get_x_count;
    g_bc3.get_left_x = call_get_left_x;
    g_bc3.set_left_x = call_set_left_x;
    g_bc3.get_max_left_x = call_get_max_left_x;
    g_bc3.get_top_item = call_get_top_item;
    g_bc3.set_top_item = call_set_top_item;
    g_bc3.set_control_height = call_set_control_height;
    g_bc3.notify_current_changed = call_current_changed;
    return TRUE;
}

void *bc3_frame_main_editor(void *frame, int index) {
    if (!frame || index < 0 || index > 3) {
        return NULL;
    }
    return *(void **)((uint8_t *)frame + BC3_FRAME_MAIN_EDITORS + index * 4);
}

void *bc3_frame_detail_editor(void *frame, int index) {
    if (!frame || index < 0 || index > 3) {
        return NULL;
    }
    return *(void **)((uint8_t *)frame + BC3_FRAME_DETAIL_EDITORS + index * 4);
}

void *bc3_frame_details_panel(void *frame) {
    if (!frame) {
        return NULL;
    }
    return *(void **)((uint8_t *)frame + BC3_FRAME_DETAILS_PANEL);
}

HWND bc3_find_control_window(void *control) {
    DWORD process_id = GetCurrentProcessId();
    HWND best = NULL;
    int best_score = 0x7fffffff;
    int expected_width;
    int expected_height;
    size_t offset;

    if (!control) {
        return NULL;
    }
    expected_width = *(int *)((uint8_t *)control + 0x48);
    expected_height = *(int *)((uint8_t *)control + 0x4c);
    for (offset = 0; offset < 0x300; offset += sizeof(void *)) {
        HWND candidate = *(HWND *)((uint8_t *)control + offset);
        DWORD owner = 0;
        RECT rect;
        int score;
        if (!candidate || !IsWindow(candidate)) {
            continue;
        }
        GetWindowThreadProcessId(candidate, &owner);
        if (owner != process_id || !GetWindowRect(candidate, &rect)) {
            continue;
        }
        score = abs((rect.right - rect.left) - expected_width) +
                abs((rect.bottom - rect.top) - expected_height);
        if (score < best_score) {
            best_score = score;
            best = candidate;
        }
    }
    return best_score <= 12 ? best : NULL;
}
