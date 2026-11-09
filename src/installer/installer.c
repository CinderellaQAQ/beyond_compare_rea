#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <stdio.h>
#include <wchar.h>

#include "resource.h"

#define PRODUCT_NAME L"BC3 MultiDetails"
#define PRODUCT_VERSION L"0.2.2"
#define TARGET_EXE L"BCompare.exe"
#define PLUGIN_NAME L"version.dll"
#define UNINSTALLER_NAME L"BC3MultiDetailsUninstall.exe"
#define TARGET_SIZE 9571304ULL
#define UNINSTALL_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\BC3MultiDetails"

enum InstallResult {
    RESULT_OK = 0,
    RESULT_UNSUPPORTED = 2,
    RESULT_RUNNING = 3,
    RESULT_CONFLICT = 4,
    RESULT_IO = 5
};

typedef struct Options {
    BOOL quiet;
    BOOL uninstall;
    WCHAR directory[MAX_PATH];
} Options;

static const BYTE target_sha256[32] = {
    0x1d, 0x82, 0x2b, 0x3f, 0xaa, 0xbb, 0x8f, 0x65,
    0xfc, 0x30, 0x07, 0x6d, 0x32, 0xa9, 0x57, 0x57,
    0xa2, 0xc3, 0x69, 0xcc, 0xb6, 0x4a, 0xe5, 0x45,
    0x72, 0xe9, 0xf5, 0x62, 0x28, 0x0a, 0xe8, 0x45
};

/* Exact v0.2.0 and v0.2.1 release artifacts.  These are the only
   pre-existing files that the v0.2.2 installer may replace. */
static const BYTE v020_plugin_sha256[32] = {
    0x2f, 0x74, 0x88, 0x93, 0x2c, 0x86, 0xa2, 0x6d,
    0x91, 0x07, 0x8b, 0x1c, 0x26, 0x79, 0x1e, 0x9e,
    0x7a, 0x18, 0x3e, 0xb6, 0xad, 0xaf, 0xf8, 0xfb,
    0x22, 0x2f, 0xa8, 0x4a, 0xfd, 0x18, 0xf4, 0x0c
};

static const BYTE v020_setup_sha256[32] = {
    0xeb, 0x6c, 0xf8, 0x67, 0x6f, 0xf7, 0x95, 0x96,
    0xac, 0x79, 0x7e, 0xd7, 0x58, 0x7f, 0xde, 0xdd,
    0x68, 0xc8, 0x64, 0x5e, 0x75, 0xf7, 0x06, 0x1c,
    0x2d, 0xbb, 0xdb, 0x38, 0xaa, 0x8c, 0x8e, 0x7b
};

static const BYTE v021_plugin_sha256[32] = {
    0x12, 0x85, 0xc1, 0x3f, 0x46, 0x06, 0xba, 0x6c,
    0x88, 0xe7, 0x16, 0x67, 0xcc, 0x13, 0xa7, 0xff,
    0x9d, 0xdf, 0x45, 0x8e, 0xdc, 0x6a, 0x44, 0x84,
    0xdc, 0x0d, 0xf0, 0xae, 0xe7, 0x04, 0xc8, 0x69
};

static const BYTE v021_setup_sha256[32] = {
    0x2b, 0xa7, 0x2b, 0x0f, 0x13, 0xa0, 0x90, 0x6d,
    0x8c, 0x4e, 0x4d, 0x0b, 0x4f, 0xb4, 0x7a, 0x3f,
    0x36, 0x5c, 0x06, 0xc4, 0x0b, 0x4f, 0xe7, 0x7b,
    0x1d, 0x15, 0x24, 0xb3, 0x55, 0x5f, 0x2a, 0xcd
};

static HINSTANCE instance_handle;
static Options options;

static BOOL same_path(const WCHAR *left, const WCHAR *right);

static void join_path(WCHAR *output, size_t capacity,
                      const WCHAR *directory, const WCHAR *name) {
    size_t length;
    if (!capacity) return;
    lstrcpynW(output, directory, (int)capacity);
    length = wcslen(output);
    while (length && (output[length - 1] == L'\\' ||
                      output[length - 1] == L'/')) {
        output[--length] = L'\0';
    }
    if (length + 1 < capacity) {
        output[length++] = L'\\';
        output[length] = L'\0';
    }
    lstrcpynW(output + length, name, (int)(capacity - length));
}

static BOOL file_exists(const WCHAR *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL directory_has_target(const WCHAR *directory) {
    WCHAR path[MAX_PATH];
    join_path(path, MAX_PATH, directory, TARGET_EXE);
    return file_exists(path);
}

static BOOL hash_file(const WCHAR *path, BYTE digest[32],
                      ULONGLONG *size_out) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    HANDLE file = INVALID_HANDLE_VALUE;
    BYTE buffer[32768];
    DWORD read_count;
    DWORD digest_size = 32;
    LARGE_INTEGER size;
    BOOL success = FALSE;

    file = CreateFileW(path, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto cleanup;
    if (!GetFileSizeEx(file, &size)) goto cleanup;
    if (!CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT)) goto cleanup;
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) goto cleanup;
    do {
        if (!ReadFile(file, buffer, sizeof(buffer), &read_count, NULL)) {
            goto cleanup;
        }
        if (read_count && !CryptHashData(hash, buffer, read_count, 0)) {
            goto cleanup;
        }
    } while (read_count);
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &digest_size, 0) ||
        digest_size != 32) goto cleanup;
    if (size_out) *size_out = (ULONGLONG)size.QuadPart;
    success = TRUE;

cleanup:
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return success;
}

static BOOL verify_target(const WCHAR *directory, WCHAR *message,
                          size_t message_capacity) {
    WCHAR path[MAX_PATH];
    BYTE digest[32];
    ULONGLONG size = 0;
    join_path(path, MAX_PATH, directory, TARGET_EXE);
    if (!hash_file(path, digest, &size)) {
        _snwprintf(message, message_capacity,
                   L"Cannot read %ls (Windows error %lu).", path,
                   GetLastError());
        message[message_capacity - 1] = L'\0';
        return FALSE;
    }
    if (size != TARGET_SIZE ||
        memcmp(digest, target_sha256, sizeof(target_sha256)) != 0) {
        _snwprintf(message, message_capacity,
                   L"Unsupported BCompare.exe. Expected Windows BC3 3.3.5 "
                   L"build 15075 (SHA-256 1d822b3f...0ae845). Found %llu bytes.",
                   size);
        message[message_capacity - 1] = L'\0';
        return FALSE;
    }
    return TRUE;
}

static BOOL bcompare_is_running(const WCHAR *directory) {
    PROCESSENTRY32W entry;
    WCHAR target[MAX_PATH];
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    join_path(target, MAX_PATH, directory, TARGET_EXE);
    if (snapshot == INVALID_HANDLE_VALUE) return FALSE;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (lstrcmpiW(entry.szExeFile, TARGET_EXE) == 0) {
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                             FALSE, entry.th32ProcessID);
                if (process) {
                    WCHAR image[MAX_PATH];
                    DWORD image_length = MAX_PATH;
                    BOOL matches = QueryFullProcessImageNameW(
                        process, 0, image, &image_length) &&
                        same_path(image, target);
                    CloseHandle(process);
                    if (matches) {
                        CloseHandle(snapshot);
                        return TRUE;
                    }
                }
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return FALSE;
}

static BOOL plugin_resource(const void **data, DWORD *size) {
    HRSRC resource = FindResourceW(instance_handle,
                                    MAKEINTRESOURCEW(IDR_PLUGIN_DLL), RT_RCDATA);
    HGLOBAL loaded;
    if (!resource) return FALSE;
    loaded = LoadResource(instance_handle, resource);
    if (!loaded) return FALSE;
    *data = LockResource(loaded);
    *size = SizeofResource(instance_handle, resource);
    return *data != NULL && *size != 0;
}

static BOOL file_matches_memory(const WCHAR *path, const void *data, DWORD size) {
    HANDLE file;
    LARGE_INTEGER file_size;
    BYTE buffer[32768];
    const BYTE *expected = (const BYTE *)data;
    DWORD offset = 0;
    DWORD read_count;
    BOOL same = FALSE;
    file = CreateFileW(path, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    if (!GetFileSizeEx(file, &file_size) ||
        file_size.QuadPart != (LONGLONG)size) goto cleanup;
    while (offset < size) {
        DWORD requested = size - offset;
        if (requested > sizeof(buffer)) requested = sizeof(buffer);
        if (!ReadFile(file, buffer, requested, &read_count, NULL) ||
            read_count != requested ||
            memcmp(buffer, expected + offset, requested) != 0) goto cleanup;
        offset += requested;
    }
    same = TRUE;
cleanup:
    CloseHandle(file);
    return same;
}

static BOOL file_has_sha256(const WCHAR *path, const BYTE expected[32]) {
    BYTE digest[32];
    return hash_file(path, digest, NULL) &&
           memcmp(digest, expected, sizeof(digest)) == 0;
}

static BOOL write_plugin(const WCHAR *directory, WCHAR *message,
                         size_t message_capacity) {
    const void *data;
    DWORD size;
    WCHAR destination[MAX_PATH];
    WCHAR temporary[MAX_PATH];
    HANDLE file;
    DWORD written;
    BOOL replace_existing = FALSE;
    if (!plugin_resource(&data, &size)) {
        lstrcpynW(message, L"The embedded extension payload is missing.",
                  (int)message_capacity);
        return FALSE;
    }
    join_path(destination, MAX_PATH, directory, PLUGIN_NAME);
    if (file_exists(destination)) {
        if (file_matches_memory(destination, data, size)) return TRUE;
        if (file_has_sha256(destination, v020_plugin_sha256) ||
            file_has_sha256(destination, v021_plugin_sha256)) {
            replace_existing = TRUE;
        } else {
            lstrcpynW(message,
                      L"A different version.dll already exists beside "
                      L"BCompare.exe. It was not overwritten.",
                      (int)message_capacity);
            SetLastError(ERROR_ALREADY_EXISTS);
            return FALSE;
        }
    }
    _snwprintf(temporary, MAX_PATH, L"%ls.bc3md.%08lx.tmp", destination,
               GetCurrentProcessId());
    temporary[MAX_PATH - 1] = L'\0';
    file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto io_error;
    if (!WriteFile(file, data, size, &written, NULL) || written != size ||
        !FlushFileBuffers(file)) {
        CloseHandle(file);
        DeleteFileW(temporary);
        goto io_error;
    }
    CloseHandle(file);
    if (!MoveFileExW(temporary, destination, MOVEFILE_WRITE_THROUGH |
                     (replace_existing ? MOVEFILE_REPLACE_EXISTING : 0))) {
        DeleteFileW(temporary);
        goto io_error;
    }
    return TRUE;

io_error:
    _snwprintf(message, message_capacity,
               L"Could not install %ls (Windows error %lu).",
               destination, GetLastError());
    message[message_capacity - 1] = L'\0';
    return FALSE;
}

static BOOL same_path(const WCHAR *left, const WCHAR *right) {
    WCHAR full_left[MAX_PATH];
    WCHAR full_right[MAX_PATH];
    if (!GetFullPathNameW(left, MAX_PATH, full_left, NULL) ||
        !GetFullPathNameW(right, MAX_PATH, full_right, NULL)) return FALSE;
    return lstrcmpiW(full_left, full_right) == 0;
}

static BOOL files_equal(const WCHAR *left_path, const WCHAR *right_path) {
    HANDLE left = INVALID_HANDLE_VALUE;
    HANDLE right = INVALID_HANDLE_VALUE;
    LARGE_INTEGER left_size;
    LARGE_INTEGER right_size;
    BYTE left_buffer[32768];
    BYTE right_buffer[32768];
    DWORD left_read;
    DWORD right_read;
    BOOL equal = FALSE;
    left = CreateFileW(left_path, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    right = CreateFileW(right_path, GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (left == INVALID_HANDLE_VALUE || right == INVALID_HANDLE_VALUE ||
        !GetFileSizeEx(left, &left_size) ||
        !GetFileSizeEx(right, &right_size) ||
        left_size.QuadPart != right_size.QuadPart) goto cleanup;
    do {
        if (!ReadFile(left, left_buffer, sizeof(left_buffer), &left_read, NULL) ||
            !ReadFile(right, right_buffer, sizeof(right_buffer), &right_read,
                      NULL) ||
            left_read != right_read ||
            memcmp(left_buffer, right_buffer, left_read) != 0) goto cleanup;
    } while (left_read);
    equal = TRUE;
cleanup:
    if (left != INVALID_HANDLE_VALUE) CloseHandle(left);
    if (right != INVALID_HANDLE_VALUE) CloseHandle(right);
    return equal;
}

static BOOL write_uninstaller(const WCHAR *directory) {
    WCHAR source[MAX_PATH];
    WCHAR destination[MAX_PATH];
    WCHAR temporary[MAX_PATH];
    GetModuleFileNameW(NULL, source, MAX_PATH);
    join_path(destination, MAX_PATH, directory, UNINSTALLER_NAME);
    if (same_path(source, destination)) return TRUE;
    if (file_exists(destination)) {
        if (files_equal(source, destination)) return TRUE;
        if (!file_has_sha256(destination, v020_setup_sha256) &&
            !file_has_sha256(destination, v021_setup_sha256)) {
            SetLastError(ERROR_ALREADY_EXISTS);
            return FALSE;
        }
        _snwprintf(temporary, MAX_PATH, L"%ls.bc3md.%08lx.tmp",
                   destination, GetCurrentProcessId());
        temporary[MAX_PATH - 1] = L'\0';
        if (!CopyFileW(source, temporary, TRUE)) return FALSE;
        if (!MoveFileExW(temporary, destination,
                         MOVEFILE_REPLACE_EXISTING |
                         MOVEFILE_WRITE_THROUGH)) {
            /* Wine and a few older Windows filesystem filters can reject an
               atomic executable replacement even when neither file is open.
               The existing file was hash-authorized above, so remove only
               that exact release artifact and move the prepared copy into
               place.  Keep CopyFile as a final compatibility fallback. */
            if (DeleteFileW(destination) &&
                (MoveFileExW(temporary, destination,
                             MOVEFILE_WRITE_THROUGH) ||
                 CopyFileW(temporary, destination, FALSE))) {
                DeleteFileW(temporary);
                return TRUE;
            }
            DeleteFileW(temporary);
            return FALSE;
        }
        return TRUE;
    }
    return CopyFileW(source, destination, FALSE);
}

static BOOL set_reg_string(HKEY key, const WCHAR *name, const WCHAR *value) {
    return RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value,
                          ((DWORD)wcslen(value) + 1) * sizeof(WCHAR)) == ERROR_SUCCESS;
}

static BOOL register_uninstaller(const WCHAR *directory) {
    HKEY key;
    WCHAR uninstaller[MAX_PATH];
    WCHAR command[MAX_PATH + 32];
    DWORD one = 1;
    DWORD estimated_size = 700;
    LONG result;
    join_path(uninstaller, MAX_PATH, directory, UNINSTALLER_NAME);
    result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, NULL, 0,
                             KEY_SET_VALUE, NULL, &key, NULL);
    if (result != ERROR_SUCCESS) return FALSE;
    _snwprintf(command, MAX_PATH + 32, L"\"%ls\" /uninstall", uninstaller);
    command[MAX_PATH + 31] = L'\0';
    if (!set_reg_string(key, L"DisplayName", PRODUCT_NAME) ||
        !set_reg_string(key, L"DisplayVersion", PRODUCT_VERSION) ||
        !set_reg_string(key, L"Publisher", L"BC3 MultiDetails Project") ||
        !set_reg_string(key, L"InstallLocation", directory) ||
        !set_reg_string(key, L"DisplayIcon", uninstaller) ||
        !set_reg_string(key, L"UninstallString", command)) {
        RegCloseKey(key);
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY);
        return FALSE;
    }
    _snwprintf(command, MAX_PATH + 32,
               L"\"%ls\" /uninstall /quiet", uninstaller);
    command[MAX_PATH + 31] = L'\0';
    set_reg_string(key, L"QuietUninstallString", command);
    RegSetValueExW(key, L"NoModify", 0, REG_DWORD,
                   (const BYTE *)&one, sizeof(one));
    RegSetValueExW(key, L"NoRepair", 0, REG_DWORD,
                   (const BYTE *)&one, sizeof(one));
    RegSetValueExW(key, L"EstimatedSize", 0, REG_DWORD,
                   (const BYTE *)&estimated_size, sizeof(estimated_size));
    RegCloseKey(key);
    return TRUE;
}

static BOOL read_install_registry(WCHAR *directory, DWORD capacity) {
    HKEY key;
    DWORD type = 0;
    DWORD bytes = capacity * sizeof(WCHAR);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, KEY_QUERY_VALUE,
                      &key) != ERROR_SUCCESS) return FALSE;
    if (RegQueryValueExW(key, L"InstallLocation", NULL, &type,
                         (BYTE *)directory, &bytes) != ERROR_SUCCESS ||
        type != REG_SZ) {
        RegCloseKey(key);
        return FALSE;
    }
    directory[capacity - 1] = L'\0';
    RegCloseKey(key);
    return TRUE;
}

static BOOL read_value(HKEY key, const WCHAR *name, WCHAR *value,
                       DWORD capacity) {
    DWORD type = 0;
    DWORD bytes = capacity * sizeof(WCHAR);
    if (RegQueryValueExW(key, name, NULL, &type, (BYTE *)value, &bytes) !=
        ERROR_SUCCESS || type != REG_SZ) return FALSE;
    value[capacity - 1] = L'\0';
    return TRUE;
}

static BOOL find_from_uninstall_root(HKEY root, REGSAM view,
                                     WCHAR *directory) {
    HKEY uninstall;
    DWORD index = 0;
    WCHAR subkey_name[256];
    DWORD name_length;
    if (RegOpenKeyExW(root,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        0, KEY_ENUMERATE_SUB_KEYS | view, &uninstall) != ERROR_SUCCESS) {
        return FALSE;
    }
    for (;;) {
        HKEY subkey;
        WCHAR display_name[256];
        name_length = 256;
        if (RegEnumKeyExW(uninstall, index++, subkey_name, &name_length,
                          NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
        if (RegOpenKeyExW(uninstall, subkey_name, 0,
                          KEY_QUERY_VALUE | view, &subkey) != ERROR_SUCCESS) {
            continue;
        }
        if (read_value(subkey, L"DisplayName", display_name, 256) &&
            wcsstr(display_name, L"Beyond Compare 3") &&
            read_value(subkey, L"InstallLocation", directory, MAX_PATH) &&
            directory_has_target(directory)) {
            RegCloseKey(subkey);
            RegCloseKey(uninstall);
            return TRUE;
        }
        RegCloseKey(subkey);
    }
    RegCloseKey(uninstall);
    return FALSE;
}

static BOOL try_common_location(const WCHAR *environment_name,
                                const WCHAR *suffix, WCHAR *directory) {
    WCHAR base[MAX_PATH];
    DWORD length = GetEnvironmentVariableW(environment_name, base, MAX_PATH);
    if (!length || length >= MAX_PATH) return FALSE;
    join_path(directory, MAX_PATH, base, suffix);
    return directory_has_target(directory);
}

static void detect_install_directory(WCHAR *directory) {
    WCHAR module[MAX_PATH];
    WCHAR *slash;
    if (read_install_registry(directory, MAX_PATH) &&
        directory_has_target(directory)) return;
    GetModuleFileNameW(NULL, module, MAX_PATH);
    slash = wcsrchr(module, L'\\');
    if (slash) *slash = L'\0';
    if (directory_has_target(module)) {
        lstrcpynW(directory, module, MAX_PATH);
        return;
    }
    if (find_from_uninstall_root(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY,
                                 directory) ||
        find_from_uninstall_root(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY,
                                 directory) ||
        find_from_uninstall_root(HKEY_CURRENT_USER, KEY_WOW64_32KEY,
                                 directory) ||
        find_from_uninstall_root(HKEY_CURRENT_USER, KEY_WOW64_64KEY,
                                 directory) ||
        try_common_location(L"ProgramFiles(x86)", L"Beyond Compare 3",
                            directory) ||
        try_common_location(L"ProgramFiles", L"Beyond Compare 3",
                            directory) ||
        try_common_location(L"ProgramFiles(x86)",
                            L"Scooter Software\\Beyond Compare 3", directory) ||
        try_common_location(L"ProgramFiles",
                            L"Scooter Software\\Beyond Compare 3", directory)) {
        return;
    }
    directory[0] = L'\0';
}

static int perform_install(const WCHAR *directory, WCHAR *message,
                           size_t message_capacity) {
    WCHAR plugin[MAX_PATH];
    WCHAR uninstaller[MAX_PATH];
    BOOL plugin_preexisting;
    BOOL uninstaller_preexisting;
    if (!directory[0] || !verify_target(directory, message, message_capacity)) {
        return RESULT_UNSUPPORTED;
    }
    if (bcompare_is_running(directory)) {
        lstrcpynW(message,
                  L"Beyond Compare is running. Close every BCompare.exe "
                  L"process and try again.", (int)message_capacity);
        return RESULT_RUNNING;
    }
    join_path(plugin, MAX_PATH, directory, PLUGIN_NAME);
    join_path(uninstaller, MAX_PATH, directory, UNINSTALLER_NAME);
    plugin_preexisting = file_exists(plugin);
    uninstaller_preexisting = file_exists(uninstaller);
    if (!write_plugin(directory, message, message_capacity)) {
        return GetLastError() == ERROR_ALREADY_EXISTS ?
               RESULT_CONFLICT : RESULT_IO;
    }
    if (!write_uninstaller(directory)) {
        DWORD error = GetLastError();
        if (!plugin_preexisting) DeleteFileW(plugin);
        if (!uninstaller_preexisting) DeleteFileW(uninstaller);
        _snwprintf(message, message_capacity,
                   L"The extension could not update the uninstaller "
                   L"(Windows error %lu).", error);
        message[message_capacity - 1] = L'\0';
        return error == ERROR_ALREADY_EXISTS ? RESULT_CONFLICT : RESULT_IO;
    }
    if (!register_uninstaller(directory)) {
        if (!plugin_preexisting) DeleteFileW(plugin);
        if (!uninstaller_preexisting) DeleteFileW(uninstaller);
        lstrcpynW(message,
                  L"The extension was rolled back because uninstall "
                  L"registration could not be created.",
                  (int)message_capacity);
        return RESULT_IO;
    }
    lstrcpynW(message,
              L"Installation completed. Start Beyond Compare and open a "
              L"Text Compare session.", (int)message_capacity);
    return RESULT_OK;
}

static int perform_uninstall(const WCHAR *directory, WCHAR *message,
                             size_t message_capacity) {
    const void *data;
    DWORD size;
    WCHAR plugin[MAX_PATH];
    WCHAR self[MAX_PATH];
    WCHAR installed_uninstaller[MAX_PATH];
    if (bcompare_is_running(directory)) {
        lstrcpynW(message,
                  L"Beyond Compare is running. Close it before uninstalling.",
                  (int)message_capacity);
        return RESULT_RUNNING;
    }
    if (!plugin_resource(&data, &size)) {
        lstrcpynW(message, L"The embedded extension payload is missing.",
                  (int)message_capacity);
        return RESULT_IO;
    }
    join_path(plugin, MAX_PATH, directory, PLUGIN_NAME);
    if (file_exists(plugin)) {
        if (!file_matches_memory(plugin, data, size)) {
            lstrcpynW(message,
                      L"version.dll no longer matches this extension. It was "
                      L"left untouched; remove it manually after inspection.",
                      (int)message_capacity);
            return RESULT_CONFLICT;
        }
        if (!DeleteFileW(plugin)) {
            _snwprintf(message, message_capacity,
                       L"Could not remove %ls (Windows error %lu).",
                       plugin, GetLastError());
            message[message_capacity - 1] = L'\0';
            return RESULT_IO;
        }
    }
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY);
    GetModuleFileNameW(NULL, self, MAX_PATH);
    join_path(installed_uninstaller, MAX_PATH, directory, UNINSTALLER_NAME);
    if (same_path(self, installed_uninstaller)) {
        MoveFileExW(self, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    } else if (file_exists(installed_uninstaller) &&
               files_equal(self, installed_uninstaller)) {
        DeleteFileW(installed_uninstaller);
    }
    lstrcpynW(message, L"BC3 MultiDetails was removed.",
              (int)message_capacity);
    return RESULT_OK;
}

static int CALLBACK browse_callback(HWND window, UINT message,
                                    LPARAM parameter, LPARAM data) {
    (void)parameter;
    if (message == BFFM_INITIALIZED && data) {
        SendMessageW(window, BFFM_SETSELECTIONW, TRUE, data);
    }
    return 0;
}

static void browse_for_directory(HWND dialog) {
    BROWSEINFOW info;
    PIDLIST_ABSOLUTE item;
    WCHAR current[MAX_PATH];
    WCHAR selected[MAX_PATH];
    ZeroMemory(&info, sizeof(info));
    GetDlgItemTextW(dialog, IDC_INSTALL_PATH, current, MAX_PATH);
    info.hwndOwner = dialog;
    info.lpszTitle = L"Select the folder containing BCompare.exe";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    info.lpfn = browse_callback;
    info.lParam = (LPARAM)current;
    item = SHBrowseForFolderW(&info);
    if (item) {
        if (SHGetPathFromIDListW(item, selected)) {
            SetDlgItemTextW(dialog, IDC_INSTALL_PATH, selected);
        }
        CoTaskMemFree(item);
    }
}

static INT_PTR CALLBACK installer_dialog(HWND dialog, UINT message,
                                         WPARAM wparam, LPARAM lparam) {
    (void)lparam;
    if (message == WM_INITDIALOG) {
        SetDlgItemTextW(dialog, IDC_INSTALL_PATH, options.directory);
        SetDlgItemTextW(dialog, IDC_STATUS,
                        L"No BC program files are bundled with this setup.");
        return TRUE;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(wparam)) {
        case IDC_BROWSE:
            browse_for_directory(dialog);
            return TRUE;
        case IDC_INSTALL: {
            WCHAR directory[MAX_PATH];
            WCHAR status[512];
            int result;
            GetDlgItemTextW(dialog, IDC_INSTALL_PATH, directory, MAX_PATH);
            EnableWindow(GetDlgItem(dialog, IDC_INSTALL), FALSE);
            SetDlgItemTextW(dialog, IDC_STATUS, L"Verifying and installing...");
            result = perform_install(directory, status, 512);
            SetDlgItemTextW(dialog, IDC_STATUS, status);
            if (result == RESULT_OK) {
                MessageBoxW(dialog, status, PRODUCT_NAME,
                            MB_OK | MB_ICONINFORMATION);
                EndDialog(dialog, RESULT_OK);
            } else {
                MessageBoxW(dialog, status, PRODUCT_NAME,
                            MB_OK | MB_ICONERROR);
                EnableWindow(GetDlgItem(dialog, IDC_INSTALL), TRUE);
            }
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, 1);
            return TRUE;
        }
    }
    return FALSE;
}

static void parse_options(void) {
    int count = 0;
    int index;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    ZeroMemory(&options, sizeof(options));
    for (index = 1; arguments && index < count; ++index) {
        if (lstrcmpiW(arguments[index], L"/quiet") == 0 ||
            lstrcmpiW(arguments[index], L"/S") == 0) {
            options.quiet = TRUE;
        } else if (lstrcmpiW(arguments[index], L"/uninstall") == 0) {
            options.uninstall = TRUE;
        } else if (_wcsnicmp(arguments[index], L"/dir=", 5) == 0) {
            lstrcpynW(options.directory, arguments[index] + 5, MAX_PATH);
        }
    }
    if (arguments) LocalFree(arguments);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous,
                    PWSTR command_line, int show_command) {
    WCHAR message[512];
    int result;
    (void)previous;
    (void)command_line;
    (void)show_command;
    instance_handle = instance;
    InitCommonControls();
    CoInitialize(NULL);
    parse_options();
    if (!options.directory[0]) detect_install_directory(options.directory);

    if (options.uninstall) {
        if (!options.directory[0] &&
            !read_install_registry(options.directory, MAX_PATH)) {
            lstrcpynW(message, L"No BC3 MultiDetails installation was found.",
                      512);
            result = RESULT_IO;
        } else if (!options.quiet &&
                   MessageBoxW(NULL, L"Remove BC3 MultiDetails?", PRODUCT_NAME,
                               MB_YESNO | MB_ICONQUESTION) != IDYES) {
            result = 1;
        } else {
            result = perform_uninstall(options.directory, message, 512);
            if (!options.quiet) {
                MessageBoxW(NULL, message, PRODUCT_NAME,
                            MB_OK | (result == RESULT_OK ?
                                     MB_ICONINFORMATION : MB_ICONERROR));
            }
        }
    } else if (options.quiet) {
        result = perform_install(options.directory, message, 512);
        if (result != RESULT_OK) fwprintf(stderr, L"%ls\n", message);
    } else {
        result = (int)DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_INSTALLER),
                                     NULL, installer_dialog, 0);
        if (result == -1) {
            MessageBoxW(NULL, L"Could not create the setup window.",
                        PRODUCT_NAME, MB_OK | MB_ICONERROR);
            result = RESULT_IO;
        }
    }
    CoUninitialize();
    return result;
}
