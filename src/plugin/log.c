#include "log.h"

#include <stdarg.h>
#include <stdio.h>

static WCHAR g_log_path[MAX_PATH];

void log_initialize(void) {
    WCHAR temp[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, temp);
    if (!length || length >= MAX_PATH - 32) {
        g_log_path[0] = L'\0';
        return;
    }
    lstrcpyW(g_log_path, temp);
    lstrcatW(g_log_path, L"BC3MultiDetails.log");
}

void log_line(const char *format, ...) {
    char buffer[1024];
    DWORD written;
    HANDLE file;
    int length;
    va_list args;

    if (!g_log_path[0]) {
        return;
    }
    va_start(args, format);
    length = _vsnprintf(buffer, sizeof(buffer) - 3, format, args);
    va_end(args);
    if (length < 0) {
        length = (int)sizeof(buffer) - 3;
    }
    buffer[length++] = '\r';
    buffer[length++] = '\n';
    file = CreateFileW(g_log_path, FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    WriteFile(file, buffer, (DWORD)length, &written, NULL);
    CloseHandle(file);
}
