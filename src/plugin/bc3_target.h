#ifndef BC3_TARGET_H
#define BC3_TARGET_H

#include <windows.h>
#include <stdint.h>

#define BC3_EXPECTED_EXE_SIZE 9571304ULL

/* RVAs for Windows BC3 3.3.5.15075, BCompare.exe SHA-256:
   1d822b3faabb8f65fc30076d32a95757a2c369ccb64ae54572e9f562280ae845 */
enum {
    BC3_RVA_DETAILS_PANEL_RESIZE = 0x006661a8,
    BC3_RVA_EDITOR_CLICK         = 0x0066f44c,
    BC3_RVA_EDITOR_CUR_CHANGED   = 0x0066f470,
    BC3_RVA_EDITOR_GET_CUR_ITEM  = 0x0025eef0,
    BC3_RVA_EDITOR_SET_CUR_ITEM  = 0x0025ef00,
    BC3_RVA_EDITOR_GET_X_COUNT   = 0x0025ef48,
    BC3_RVA_EDITOR_GET_LEFT_X    = 0x0025f058,
    BC3_RVA_EDITOR_SET_LEFT_X    = 0x0025f068,
    BC3_RVA_EDITOR_GET_MAX_LEFT_X_CORE = 0x0025a2cc,
    BC3_RVA_EDITOR_GET_TOP_ITEM  = 0x0025f340,
    BC3_RVA_EDITOR_SET_TOP_ITEM  = 0x0025f350,
    BC3_RVA_EDITOR_SET_DATA_SOURCE = 0x0025f040,
    BC3_RVA_EDITOR_SET_OPTION    = 0x00259068,
    BC3_RVA_EDITOR_EXECUTE_COMMAND = 0x0025ed64,
    BC3_RVA_CONTROL_SET_HEIGHT   = 0x000b4a80,
    BC3_RVA_STREAM_LINE_PARSER_CREATE = 0x00156838,
    BC3_RVA_SOURCE_LINE_CATEGORY = 0x0050a230
};

/* TTextViewFrame fields recovered from ViewText.pas RTTI and code use. */
enum {
    BC3_FRAME_DETAILS_PANEL = 0x0550,
    BC3_FRAME_MAIN_EDITORS  = 0x0850,
    BC3_FRAME_DETAIL_EDITORS = 0x0878
};

typedef struct Bc3Api {
    uint8_t *module_base;
    int (__cdecl *get_cur_item)(void *editor);
    void (__cdecl *set_cur_item)(void *editor, int item);
    void (__cdecl *set_caret_x)(void *editor, int x);
    int (__cdecl *get_x_count)(void *editor);
    int (__cdecl *get_left_x)(void *editor);
    void (__cdecl *set_left_x)(void *editor, int x);
    int (__cdecl *get_max_left_x)(void *editor);
    int (__cdecl *get_top_item)(void *editor);
    void (__cdecl *set_top_item)(void *editor, int item);
    void (__cdecl *set_data_source)(void *editor, void *source);
    void (__cdecl *set_option)(void *editor, int option, BOOL enabled);
    void (__cdecl *execute_command)(void *editor, int command,
                                    void *parameter);
    void (__cdecl *set_control_height)(void *control, int height);
    void (__cdecl *notify_current_changed)(void *frame, void *sender);
} Bc3Api;

extern Bc3Api g_bc3;

BOOL bc3_target_initialize(void);
void *bc3_frame_main_editor(void *frame, int index);
void *bc3_frame_detail_editor(void *frame, int index);
void *bc3_frame_details_panel(void *frame);
HWND bc3_find_control_window(void *control);
int bc3_editor_item_count(void *editor);
void *bc3_editor_data_source(void *editor);
void bc3_editor_override_current_item(void *editor, int item);
void bc3_editor_override_top_item(void *editor, int item);
BOOL bc3_editor_source_item_info(void *editor, int item,
                                 int *first, int *second);
int bc3_editor_context(void *editor);
BOOL bc3_source_item_info(void *source, int context, int item,
                          int *first, int *second);
int bc3_source_item_metric(void *source, int item, int mode);
BOOL bc3_editor_group_bounds(void *editor, int *first, int *last);
BOOL bc3_editor_native_group(void *editor, int item,
                             int *first, int *last);
BOOL bc3_editor_filter_mapping(void *editor, int item,
                               int *mapped, int *count);
BOOL bc3_editor_raw_record(void *editor, int item, uint32_t record[5]);
BOOL bc3_editor_physical_line_bounds(void *editor, int item,
                                     int *first, int *last);
int bc3_editor_underlying_line_length(void *editor, int item, int side);
int bc3_editor_underlying_line_ending(void *editor, int item, int side);
BOOL bc3_editor_underlying_line_text(void *editor, int item, int side,
                                     const char **text, int *length);

#endif
