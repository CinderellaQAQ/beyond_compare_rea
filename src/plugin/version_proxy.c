#include <windows.h>
#include <winver.h>

#include "bc3_target.h"
#include "detour.h"
#include "log.h"
#include "multidetails.h"

typedef DWORD (WINAPI *GetFileVersionInfoSizeAProc)(LPCSTR, LPDWORD);
typedef DWORD (WINAPI *GetFileVersionInfoSizeWProc)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *GetFileVersionInfoAProc)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *GetFileVersionInfoWProc)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *VerQueryValueAProc)(LPCVOID, LPCSTR, LPVOID *, PUINT);
typedef BOOL (WINAPI *VerQueryValueWProc)(LPCVOID, LPCWSTR, LPVOID *, PUINT);

static HMODULE g_real_version;
static HMODULE g_self;
static InlineDetour g_click_detour;
static InlineDetour g_resize_detour;
static InlineDetour g_line_parser_detour;
static InlineDetour g_current_changed_detour;
void *g_click_trampoline;
void *g_resize_trampoline;
void *g_line_parser_trampoline;
void *g_current_changed_trampoline;
static void *g_click_frame;
static void *g_click_sender;
static void *g_resize_frame;
LONG g_skip_click_original;
LONG g_skip_current_changed_original;

/* Keep one physical input line in one BC3-native editor item.  The parser
   constructor already supports a 65536-character maximum; BC3 normally
   supplies 4096, which is why its detail editor shows only the first soft
   continuation chunk of a long line. */
__attribute__((naked)) static void stream_line_parser_hook(void) {
    __asm__ __volatile__(
        "movl $0x10000, 0x1c(%esp)\n\t"
        "jmp *_g_line_parser_trampoline\n\t");
}

static BOOL load_real_version(void) {
    WCHAR path[MAX_PATH];
    UINT length;
    length = GetSystemDirectoryW(path, MAX_PATH);
    if (!length || length + 13 >= MAX_PATH) return FALSE;
    lstrcatW(path, L"\\version.dll");
    g_real_version = LoadLibraryW(path);
    return g_real_version != NULL;
}

static FARPROC real_proc(const char *name) {
    if (!g_real_version) load_real_version();
    return g_real_version ? GetProcAddress(g_real_version, name) : NULL;
}

__declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeA(
    LPCSTR filename, LPDWORD handle) {
    GetFileVersionInfoSizeAProc proc =
        (GetFileVersionInfoSizeAProc)real_proc("GetFileVersionInfoSizeA");
    return proc ? proc(filename, handle) : 0;
}

__declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeW(
    LPCWSTR filename, LPDWORD handle) {
    GetFileVersionInfoSizeWProc proc =
        (GetFileVersionInfoSizeWProc)real_proc("GetFileVersionInfoSizeW");
    return proc ? proc(filename, handle) : 0;
}

__declspec(dllexport) BOOL WINAPI GetFileVersionInfoA(
    LPCSTR filename, DWORD handle, DWORD length, LPVOID data) {
    GetFileVersionInfoAProc proc =
        (GetFileVersionInfoAProc)real_proc("GetFileVersionInfoA");
    return proc ? proc(filename, handle, length, data) : FALSE;
}

__declspec(dllexport) BOOL WINAPI GetFileVersionInfoW(
    LPCWSTR filename, DWORD handle, DWORD length, LPVOID data) {
    GetFileVersionInfoWProc proc =
        (GetFileVersionInfoWProc)real_proc("GetFileVersionInfoW");
    return proc ? proc(filename, handle, length, data) : FALSE;
}

__declspec(dllexport) BOOL WINAPI VerQueryValueA(
    LPCVOID block, LPCSTR sub_block, LPVOID *buffer, PUINT length) {
    VerQueryValueAProc proc =
        (VerQueryValueAProc)real_proc("VerQueryValueA");
    return proc ? proc(block, sub_block, buffer, length) : FALSE;
}

__declspec(dllexport) BOOL WINAPI VerQueryValueW(
    LPCVOID block, LPCWSTR sub_block, LPVOID *buffer, PUINT length) {
    VerQueryValueWProc proc =
        (VerQueryValueWProc)real_proc("VerQueryValueW");
    return proc ? proc(block, sub_block, buffer, length) : FALSE;
}

__attribute__((used)) void __cdecl click_pre(void *frame, void *sender) {
    g_click_frame = frame;
    g_click_sender = sender;
    g_skip_click_original = multidetails_before_click(frame, sender);
}

__attribute__((used)) void __cdecl click_post(void) {
    multidetails_on_click(g_click_frame, g_click_sender);
}

__attribute__((used)) void __cdecl current_changed_pre(
    void *frame, void *sender) {
    g_skip_current_changed_original =
        multidetails_on_current_changed(frame, sender);
}

__attribute__((used)) void __cdecl resize_pre(void *frame) {
    g_resize_frame = frame;
}

__attribute__((used)) void __cdecl resize_post(void) {
    multidetails_on_details_resize(g_resize_frame);
}

__attribute__((naked)) static void editor_click_hook(void) {
    __asm__ __volatile__(
        "pushfl\n\t"
        "pushal\n\t"
        "pushl %edx\n\t"
        "pushl %eax\n\t"
        "call _click_pre\n\t"
        "addl $8, %esp\n\t"
        "popal\n\t"
        "popfl\n\t"
        "cmpl $0, _g_skip_click_original\n\t"
        "jne 1f\n\t"
        "call *_g_click_trampoline\n\t"
        "1:\n\t"
        "pushfl\n\t"
        "pushal\n\t"
        "call _click_post\n\t"
        "popal\n\t"
        "popfl\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void editor_current_changed_hook(void) {
    __asm__ __volatile__(
        "pushfl\n\t"
        "pushal\n\t"
        "pushl %edx\n\t"
        "pushl %eax\n\t"
        "call _current_changed_pre\n\t"
        "addl $8, %esp\n\t"
        "popal\n\t"
        "popfl\n\t"
        "cmpl $0, _g_skip_current_changed_original\n\t"
        "jne 1f\n\t"
        "call *_g_current_changed_trampoline\n\t"
        "1:\n\t"
        "ret\n\t");
}

__attribute__((naked)) static void details_resize_hook(void) {
    __asm__ __volatile__(
        "pushfl\n\t"
        "pushal\n\t"
        "pushl %eax\n\t"
        "call _resize_pre\n\t"
        "addl $4, %esp\n\t"
        "popal\n\t"
        "popfl\n\t"
        "call *_g_resize_trampoline\n\t"
        "pushfl\n\t"
        "pushal\n\t"
        "call _resize_post\n\t"
        "popal\n\t"
        "popfl\n\t"
        "ret\n\t");
}

static BOOL install_hooks(void) {
    static const uint8_t click_expected[10] = {
        0x53, 0x8b, 0xd8, 0x83, 0xbb, 0xd4, 0x07, 0x00, 0x00, 0x00
    };
    static const uint8_t resize_expected[7] = {
        0x53, 0x56, 0x57, 0x8b, 0xf0, 0x33, 0xdb
    };
    static const uint8_t line_parser_expected[6] = {
        0x55, 0x8b, 0xec, 0x83, 0xc4, 0xe4
    };
    static const uint8_t current_changed_expected[8] = {
        0x55, 0x8b, 0xec, 0x83, 0xc4, 0xec, 0x53, 0x56
    };
    uint8_t *base = g_bc3.module_base;
    if (!detour_install(&g_line_parser_detour,
                        base + BC3_RVA_STREAM_LINE_PARSER_CREATE,
                        stream_line_parser_hook,
                        sizeof(line_parser_expected),
                        line_parser_expected)) {
        log_line("TStreamLineParser native line-limit hook failed");
        return FALSE;
    }
    g_line_parser_trampoline = g_line_parser_detour.trampoline;
    if (!detour_install(&g_click_detour,
                        base + BC3_RVA_EDITOR_CLICK,
                        editor_click_hook, sizeof(click_expected),
                        click_expected)) {
        log_line("EditorClick hook verification/install failed");
        detour_remove(&g_line_parser_detour);
        g_line_parser_trampoline = NULL;
        return FALSE;
    }
    g_click_trampoline = g_click_detour.trampoline;
    if (!detour_install(&g_current_changed_detour,
                        base + BC3_RVA_EDITOR_CUR_CHANGED,
                        editor_current_changed_hook,
                        sizeof(current_changed_expected),
                        current_changed_expected)) {
        log_line("EditorCurrentChanged hook verification/install failed");
        detour_remove(&g_click_detour);
        detour_remove(&g_line_parser_detour);
        g_click_trampoline = NULL;
        g_line_parser_trampoline = NULL;
        return FALSE;
    }
    g_current_changed_trampoline =
        g_current_changed_detour.trampoline;
    if (!detour_install(&g_resize_detour,
                        base + BC3_RVA_DETAILS_PANEL_RESIZE,
                        details_resize_hook, sizeof(resize_expected),
                        resize_expected)) {
        log_line("DetailsPanelResize hook verification/install failed");
        detour_remove(&g_current_changed_detour);
        detour_remove(&g_click_detour);
        detour_remove(&g_line_parser_detour);
        g_click_trampoline = NULL;
        g_current_changed_trampoline = NULL;
        g_line_parser_trampoline = NULL;
        return FALSE;
    }
    g_resize_trampoline = g_resize_detour.trampoline;
    return TRUE;
}

static DWORD WINAPI initialize_extension(LPVOID unused) {
    (void)unused;
    log_initialize();
    log_line("BC3 MultiDetails loading");
    if (!bc3_target_initialize()) {
        log_line("Unsupported BCompare.exe; extension not activated");
        return 1;
    }
    if (!multidetails_initialize()) {
        log_line("Native detail initialization failed");
        return 2;
    }
    if (!install_hooks()) {
        multidetails_shutdown();
        return 3;
    }
    log_line("BC3 MultiDetails active");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE thread;
        g_self = instance;
        DisableThreadLibraryCalls(instance);
        thread = CreateThread(NULL, 0, initialize_extension, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH && !reserved) {
        detour_remove(&g_resize_detour);
        detour_remove(&g_current_changed_detour);
        detour_remove(&g_click_detour);
        detour_remove(&g_line_parser_detour);
        multidetails_shutdown();
        if (g_real_version) FreeLibrary(g_real_version);
    }
    return TRUE;
}
