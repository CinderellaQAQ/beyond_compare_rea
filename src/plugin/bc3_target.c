#include "bc3_target.h"

#include "log.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

Bc3Api g_bc3;

void *g_raw_get_caret_x;
void *g_raw_set_caret_x;
void *g_raw_get_cur_item;
void *g_raw_set_cur_item;
void *g_raw_get_x_count;
void *g_raw_get_left_x;
void *g_raw_set_left_x;
void *g_raw_get_max_left_x_core;
void *g_raw_get_top_item;
void *g_raw_set_top_item;
void *g_raw_set_data_source;
void *g_raw_set_option;
void *g_raw_execute_command;
void *g_raw_set_control_height;
void *g_raw_current_changed;
void *g_raw_source_line_category;

__attribute__((naked)) static int call_get_caret_x(
    void *editor __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "call *_g_raw_get_caret_x\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void call_set_caret_x(
    void *editor __attribute__((unused)), int x __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "call *_g_raw_set_caret_x\n\t"
        "ret\n\t");
}

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

__attribute__((naked)) static void call_set_data_source(
    void *editor __attribute__((unused)),
    void *source __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "call *_g_raw_set_data_source\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void call_set_option(
    void *editor __attribute__((unused)),
    int option __attribute__((unused)),
    BOOL enabled __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 0x280(%eax), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "movl 12(%esp), %ecx\n\t"
        "call *_g_raw_set_option\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void call_execute_command(
    void *editor __attribute__((unused)),
    int command __attribute__((unused)),
    void *parameter __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "movl 12(%esp), %ecx\n\t"
        "call *_g_raw_execute_command\n\t"
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

__attribute__((naked)) static int call_source_line_category(
    void *line_source __attribute__((unused)),
    int item __attribute__((unused))) {
    __asm__ __volatile__(
        "movl 4(%esp), %eax\n\t"
        "movl 8(%esp), %edx\n\t"
        "call *_g_raw_source_line_category\n\t"
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
    g_raw_get_caret_x = base + BC3_RVA_EDITOR_GET_CARET_X;
    g_raw_set_caret_x = base + BC3_RVA_EDITOR_SET_CARET_X;
    g_raw_get_cur_item = base + BC3_RVA_EDITOR_GET_CUR_ITEM;
    g_raw_set_cur_item = base + BC3_RVA_EDITOR_SET_CUR_ITEM;
    g_raw_get_x_count = base + BC3_RVA_EDITOR_GET_X_COUNT;
    g_raw_get_left_x = base + BC3_RVA_EDITOR_GET_LEFT_X;
    g_raw_set_left_x = base + BC3_RVA_EDITOR_SET_LEFT_X;
    g_raw_get_max_left_x_core =
        base + BC3_RVA_EDITOR_GET_MAX_LEFT_X_CORE;
    g_raw_get_top_item = base + BC3_RVA_EDITOR_GET_TOP_ITEM;
    g_raw_set_top_item = base + BC3_RVA_EDITOR_SET_TOP_ITEM;
    g_raw_set_data_source = base + BC3_RVA_EDITOR_SET_DATA_SOURCE;
    g_raw_set_option = base + BC3_RVA_EDITOR_SET_OPTION;
    g_raw_execute_command =
        base + BC3_RVA_EDITOR_EXECUTE_COMMAND;
    g_raw_set_control_height = base + BC3_RVA_CONTROL_SET_HEIGHT;
    g_raw_current_changed = base + BC3_RVA_EDITOR_CUR_CHANGED;
    g_raw_source_line_category =
        base + BC3_RVA_SOURCE_LINE_CATEGORY;
    g_bc3.get_caret_x = call_get_caret_x;
    g_bc3.set_caret_x = call_set_caret_x;
    g_bc3.get_cur_item = call_get_cur_item;
    g_bc3.set_cur_item = call_set_cur_item;
    g_bc3.get_x_count = call_get_x_count;
    g_bc3.get_left_x = call_get_left_x;
    g_bc3.set_left_x = call_set_left_x;
    g_bc3.get_max_left_x = call_get_max_left_x;
    g_bc3.get_top_item = call_get_top_item;
    g_bc3.set_top_item = call_set_top_item;
    g_bc3.set_data_source = call_set_data_source;
    g_bc3.set_option = call_set_option;
    g_bc3.execute_command = call_execute_command;
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

int bc3_editor_item_count(void *editor) {
    void *core;
    if (!editor) return 0;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return 0;
    return *(int *)((uint8_t *)core + 0x304);
}

void *bc3_editor_data_source(void *editor) {
    void *core;
    if (!editor) return NULL;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return NULL;
    return *(void **)((uint8_t *)core + 0x310);
}

void bc3_editor_override_current_item(void *editor, int item) {
    void *core;
    if (!editor) return;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return;
    *(int *)((uint8_t *)core + 0x2ac) = item;
}

void bc3_editor_override_top_item(void *editor, int item) {
    void *core;
    if (!editor) return;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return;
    *(int *)((uint8_t *)core + 0x398) = item;
}

__attribute__((naked)) static void call_source_item_info(
    void *source __attribute__((unused)),
    int context __attribute__((unused)),
    int item __attribute__((unused)),
    int *first __attribute__((unused)),
    int *second __attribute__((unused))) {
    __asm__ __volatile__(
        "pushl %ebx\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %edx\n\t"
        "movl 16(%esp), %ecx\n\t"
        "pushl 24(%esp)\n\t"
        "pushl 24(%esp)\n\t"
        "movl (%eax), %ebx\n\t"
        "call *0x44(%ebx)\n\t"
        "popl %ebx\n\t"
        "ret\n\t");
}

BOOL bc3_editor_source_item_info(void *editor, int item,
                                 int *first, int *second) {
    void *core;
    void *source;
    int context;
    if (!editor || !first || !second) return FALSE;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return FALSE;
    source = *(void **)((uint8_t *)core + 0x310);
    if (!source) return FALSE;
    context = *(int *)((uint8_t *)core + 0x38c);
    call_source_item_info(source, context, item, first, second);
    return TRUE;
}

int bc3_editor_context(void *editor) {
    void *core;
    if (!editor) return -1;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return -1;
    return *(int *)((uint8_t *)core + 0x38c);
}

BOOL bc3_source_item_info(void *source, int context, int item,
                          int *first, int *second) {
    if (!source || !first || !second) return FALSE;
    call_source_item_info(source, context, item, first, second);
    return TRUE;
}

__attribute__((naked)) static int call_source_item_metric(
    void *source __attribute__((unused)),
    int item __attribute__((unused)),
    int mode __attribute__((unused))) {
    __asm__ __volatile__(
        "pushl %ebx\n\t"
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %ecx\n\t"
        "movl 16(%esp), %edx\n\t"
        "movl (%eax), %ebx\n\t"
        "call *0x14(%ebx)\n\t"
        "popl %ebx\n\t"
        "ret\n\t");
}

int bc3_source_item_metric(void *source, int item, int mode) {
    if (!source) return -1;
    return call_source_item_metric(source, item, mode);
}

BOOL bc3_editor_group_bounds(void *editor, int *first, int *last) {
    void *core;
    if (!editor || !first || !last) return FALSE;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return FALSE;
    *first = *(int *)((uint8_t *)core + 0x268);
    *last = *(int *)((uint8_t *)core + 0x26c);
    return TRUE;
}

BOOL bc3_editor_native_group(void *editor, int item,
                             int *first, int *last) {
    void *core;
    void *source;
    void *source_owner;
    void *filter;
    void *filter_lines;
    void *line_source;
    int count;
    int *mapping;
    int category;
    if (!editor || !first || !last) return FALSE;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return FALSE;
    source = *(void **)((uint8_t *)core + 0x310);
    if (!source) return FALSE;
    source_owner = *(void **)((uint8_t *)source + 4);
    if (!source_owner) return FALSE;
    filter = *(void **)((uint8_t *)source_owner + 0x11c);
    if (!filter) return FALSE;
    count = *(int *)((uint8_t *)filter + 0x0c);
    mapping = *(int **)((uint8_t *)filter + 0x20);
    filter_lines = *(void **)((uint8_t *)filter + 0x34);
    if (!mapping || !filter_lines || item < 0 || item >= count)
        return FALSE;
    line_source = *(void **)((uint8_t *)filter_lines + 0x120);
    if (!line_source) return FALSE;
    category = call_source_line_category(line_source, mapping[item]);
    *first = item;
    while (*first > 0 &&
           mapping[*first - 1] + 1 == mapping[*first] &&
           call_source_line_category(line_source, mapping[*first - 1]) ==
               category) {
        --*first;
    }
    *last = item;
    while (*last + 1 < count &&
           mapping[*last] + 1 == mapping[*last + 1] &&
           call_source_line_category(line_source, mapping[*last + 1]) ==
               category) {
        ++*last;
    }
    return TRUE;
}

BOOL bc3_editor_filter_mapping(void *editor, int item,
                               int *mapped, int *count) {
    void *core;
    void *source;
    void *source_owner;
    void *filter;
    int filter_count;
    int *mapping;
    if (!editor || !mapped || !count) return FALSE;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return FALSE;
    source = *(void **)((uint8_t *)core + 0x310);
    if (!source) return FALSE;
    source_owner = *(void **)((uint8_t *)source + 4);
    if (!source_owner) return FALSE;
    filter = *(void **)((uint8_t *)source_owner + 0x11c);
    if (!filter) return FALSE;
    filter_count = *(int *)((uint8_t *)filter + 0x0c);
    mapping = *(int **)((uint8_t *)filter + 0x20);
    if (!mapping || item < 0 || item >= filter_count) return FALSE;
    *mapped = mapping[item];
    *count = filter_count;
    return TRUE;
}

BOOL bc3_editor_raw_record(void *editor, int item, uint32_t record[5]) {
    void *core;
    void *source;
    void *source_owner;
    void *filter;
    void *filter_lines;
    void *line_source;
    int count;
    int *mapping;
    uint8_t *records;
    if (!editor || !record) return FALSE;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return FALSE;
    source = *(void **)((uint8_t *)core + 0x310);
    if (!source) return FALSE;
    source_owner = *(void **)((uint8_t *)source + 4);
    if (!source_owner) return FALSE;
    filter = *(void **)((uint8_t *)source_owner + 0x11c);
    if (!filter) return FALSE;
    count = *(int *)((uint8_t *)filter + 0x0c);
    mapping = *(int **)((uint8_t *)filter + 0x20);
    filter_lines = *(void **)((uint8_t *)filter + 0x34);
    if (!mapping || !filter_lines || item < 0 || item >= count)
        return FALSE;
    line_source = *(void **)((uint8_t *)filter_lines + 0x120);
    if (!line_source) return FALSE;
    records = *(uint8_t **)((uint8_t *)line_source + 0x48);
    if (!records) return FALSE;
    memcpy(record, records + mapping[item] * 20, 20);
    return TRUE;
}

static uint8_t *line_list_for_side(void *line_source, int side) {
    uint8_t *session;
    uint8_t *side_text;
    if (!line_source || side < 0 || side > 2) return NULL;
    session = *(uint8_t **)((uint8_t *)line_source + 0x50);
    if (!session) return NULL;
    side_text = *(uint8_t **)(session + 0x88 + side * 4);
    return side_text ? *(uint8_t **)(side_text + 0x58) : NULL;
}

static uint8_t *raw_line_record(uint8_t *line_list, int line) {
    uint8_t *pages;
    uint8_t *page;
    uint8_t *page_data;
    int page_count;
    int low;
    int high;
    int page_index = -1;
    int offset;
    if (!line_list || line < 0 || line >= *(int *)(line_list + 0x40))
        return NULL;
    page_count = *(int *)(line_list + 0x60);
    pages = *(uint8_t **)(line_list + 0x64);
    if (!pages || page_count <= 0) return NULL;
    low = 0;
    high = page_count - 1;
    while (low <= high) {
        int middle = low + (high - low) / 2;
        int start = *(int *)(pages + middle * 12);
        if (start <= line) {
            page_index = middle;
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    if (page_index < 0) return NULL;
    page = pages + page_index * 12;
    offset = line - *(int *)page;
    if (offset < 0) return NULL;
    if (page[4] != 0) {
        page_data = *(uint8_t **)(page + 8);
        return page_data ? *(uint8_t **)(page_data + offset * 8) : NULL;
    }
    {
        int index = *(int *)(page + 8) + offset;
        int item_count = *(int *)(line_list + 0x10);
        int block_index;
        int local_index;
        uint8_t **blocks;
        uint8_t *block;
        uint8_t **items;
        if (index < 0 || index >= item_count) return NULL;
        block_index = *(int *)(line_list + 0x0c) == 1
                          ? 0 : index >> 7;
        local_index = index - (block_index << 7);
        blocks = *(uint8_t ***)(line_list + 0x08);
        if (!blocks || local_index < 0) return NULL;
        block = blocks[block_index];
        if (!block) return NULL;
        items = *(uint8_t ***)(block + 0x08);
        return items ? items[local_index] : NULL;
    }
}

static int underlying_line_length(void *line_source, int side, int line) {
    uint8_t *line_list = line_list_for_side(line_source, side);
    uint8_t *record;
    uint8_t *text;
    int count;
    if (!line_list || line < 0) return -1;
    count = *(int *)(line_list + 0x40);
    if (line >= count) return -1;
    record = raw_line_record(line_list, line);
    if (!record) return -1;
    text = *(uint8_t **)record;
    return text ? *(int *)(text - 4) : 0;
}

static int underlying_line_ending(void *line_source, int side, int line) {
    uint8_t *line_list = line_list_for_side(line_source, side);
    uint8_t *record;
    int count;
    if (!line_list || line < 0) return -1;
    count = *(int *)(line_list + 0x40);
    if (line >= count) return -1;
    record = raw_line_record(line_list, line);
    return record ? (int)record[4] : -1;
}

static BOOL physical_side_extent(void *line_source, int side,
                                 int line, int *first, int *last) {
    uint8_t *line_list = line_list_for_side(line_source, side);
    uint8_t *record;
    int count;
    if (!line_list || line < 0 || !first || !last) return FALSE;
    count = *(int *)(line_list + 0x40);
    if (line >= count) return FALSE;
    *first = line;
    while (*first > 0) {
        record = raw_line_record(line_list, *first - 1);
        /* SsEditLineList uses ending kind 5 for a 4096-byte continuation
           chunk.  Other values terminate the physical input line. */
        if (!record || record[4] != 5) break;
        --*first;
    }
    *last = line;
    while (*last + 1 < count) {
        record = raw_line_record(line_list, *last);
        if (!record || record[4] != 5) break;
        ++*last;
    }
    return TRUE;
}

int bc3_editor_underlying_line_length(void *editor, int item, int side) {
    void *core;
    void *source;
    uint8_t *owner;
    uint8_t *filter;
    uint8_t *filter_lines;
    void *line_source;
    int count;
    int *mapping;
    uint32_t *record;
    if (!editor || side < 0 || side > 1) return -1;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return -1;
    source = *(void **)((uint8_t *)core + 0x310);
    if (!source) return -1;
    owner = *(uint8_t **)((uint8_t *)source + 4);
    if (!owner) return -1;
    filter = *(uint8_t **)(owner + 0x11c);
    if (!filter) return -1;
    count = *(int *)(filter + 0x0c);
    mapping = *(int **)(filter + 0x20);
    filter_lines = *(uint8_t **)(filter + 0x34);
    if (!mapping || !filter_lines || item < 0 || item >= count ||
        mapping[item] < 0) {
        return -1;
    }
    line_source = *(void **)(filter_lines + 0x120);
    if (!line_source) return -1;
    record = (uint32_t *)(*(uint8_t **)((uint8_t *)line_source + 0x48) +
                          mapping[item] * 20);
    if (!record || (int)record[side] < 0) return -1;
    return underlying_line_length(line_source, side, (int)record[side]);
}

int bc3_editor_underlying_line_ending(void *editor, int item, int side) {
    void *core;
    void *source;
    uint8_t *owner;
    uint8_t *filter;
    uint8_t *filter_lines;
    void *line_source;
    int count;
    int *mapping;
    uint32_t *record;
    if (!editor || side < 0 || side > 1) return -1;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return -1;
    source = *(void **)((uint8_t *)core + 0x310);
    if (!source) return -1;
    owner = *(uint8_t **)((uint8_t *)source + 4);
    if (!owner) return -1;
    filter = *(uint8_t **)(owner + 0x11c);
    if (!filter) return -1;
    count = *(int *)(filter + 0x0c);
    mapping = *(int **)(filter + 0x20);
    filter_lines = *(uint8_t **)(filter + 0x34);
    if (!mapping || !filter_lines || item < 0 || item >= count ||
        mapping[item] < 0) {
        return -1;
    }
    line_source = *(void **)(filter_lines + 0x120);
    if (!line_source) return -1;
    record = (uint32_t *)(*(uint8_t **)((uint8_t *)line_source + 0x48) +
                          mapping[item] * 20);
    if (!record || (int)record[side] < 0) return -1;
    return underlying_line_ending(line_source, side, (int)record[side]);
}

BOOL bc3_editor_underlying_line_text(void *editor, int item, int side,
                                     const char **text, int *length) {
    void *core;
    void *source;
    uint8_t *owner;
    uint8_t *filter;
    uint8_t *filter_lines;
    void *line_source;
    uint8_t *line_list;
    uint8_t *raw_record;
    int count;
    int *mapping;
    uint32_t *record;
    int line;
    uint8_t *value;
    if (!editor || !text || !length || side < 0 || side > 1)
        return FALSE;
    *text = NULL;
    *length = 0;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return FALSE;
    source = *(void **)((uint8_t *)core + 0x310);
    if (!source) return FALSE;
    owner = *(uint8_t **)((uint8_t *)source + 4);
    if (!owner) return FALSE;
    filter = *(uint8_t **)(owner + 0x11c);
    if (!filter) return FALSE;
    count = *(int *)(filter + 0x0c);
    mapping = *(int **)(filter + 0x20);
    filter_lines = *(uint8_t **)(filter + 0x34);
    if (!mapping || !filter_lines || item < 0 || item >= count ||
        mapping[item] < 0) {
        return FALSE;
    }
    line_source = *(void **)(filter_lines + 0x120);
    if (!line_source) return FALSE;
    record = (uint32_t *)(*(uint8_t **)((uint8_t *)line_source + 0x48) +
                          mapping[item] * 20);
    if (!record || (int)record[side] < 0) return FALSE;
    line = (int)record[side];
    line_list = line_list_for_side(line_source, side);
    if (!line_list) return FALSE;
    raw_record = raw_line_record(line_list, line);
    if (!raw_record) return FALSE;
    value = *(uint8_t **)raw_record;
    if (!value) return TRUE;
    *text = (const char *)value;
    *length = *(int *)(value - 4);
    return *length >= 0;
}

static BOOL record_in_physical_extent(const uint32_t record[5],
                                      const BOOL valid[2],
                                      const int first[2],
                                      const int last[2]) {
    int side;
    BOOL has_line = FALSE;
    for (side = 0; side < 2; ++side) {
        int line = (int)record[side];
        if (!valid[side] || line < 0) continue;
        has_line = TRUE;
        if (line < first[side] || line > last[side]) return FALSE;
    }
    return has_line;
}

BOOL bc3_editor_physical_line_bounds(void *editor, int item,
                                     int *first, int *last) {
    void *core;
    void *source;
    uint8_t *owner;
    uint8_t *filter;
    uint8_t *filter_lines;
    void *line_source;
    int count;
    int *mapping;
    uint8_t *records;
    uint32_t current[5];
    BOOL valid[2] = {FALSE, FALSE};
    int side_first[2] = {0, 0};
    int side_last[2] = {0, 0};
    int side;
    if (!editor || !first || !last) return FALSE;
    core = *(void **)((uint8_t *)editor + 0x280);
    if (!core) return FALSE;
    source = *(void **)((uint8_t *)core + 0x310);
    if (!source) return FALSE;
    owner = *(uint8_t **)((uint8_t *)source + 4);
    if (!owner) return FALSE;
    filter = *(uint8_t **)(owner + 0x11c);
    if (!filter) return FALSE;
    count = *(int *)(filter + 0x0c);
    mapping = *(int **)(filter + 0x20);
    filter_lines = *(uint8_t **)(filter + 0x34);
    if (!mapping || !filter_lines || item < 0 || item >= count)
        return FALSE;
    line_source = *(void **)(filter_lines + 0x120);
    if (!line_source) return FALSE;
    records = *(uint8_t **)((uint8_t *)line_source + 0x48);
    if (!records || mapping[item] < 0) return FALSE;
    memcpy(current, records + mapping[item] * 20, sizeof(current));
    for (side = 0; side < 2; ++side) {
        int line = (int)current[side];
        if (line >= 0) {
            valid[side] = physical_side_extent(
                line_source, side, line,
                &side_first[side], &side_last[side]);
        }
    }
    if (!valid[0] && !valid[1]) return FALSE;
    *first = item;
    while (*first > 0) {
        uint32_t previous[5];
        int mapped = mapping[*first - 1];
        if (mapped < 0) break;
        memcpy(previous, records + mapped * 20, sizeof(previous));
        if (!record_in_physical_extent(previous, valid,
                                       side_first, side_last)) {
            break;
        }
        --*first;
    }
    *last = item;
    while (*last + 1 < count) {
        uint32_t next[5];
        int mapped = mapping[*last + 1];
        if (mapped < 0) break;
        memcpy(next, records + mapped * 20, sizeof(next));
        if (!record_in_physical_extent(next, valid,
                                       side_first, side_last)) {
            break;
        }
        ++*last;
    }
    return TRUE;
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
