#include "detour.h"

#include <string.h>

static void write_relative_jump(uint8_t *at, const void *destination) {
    intptr_t delta = (const uint8_t *)destination - (at + 5);
    at[0] = 0xE9;
    *(int32_t *)(at + 1) = (int32_t)delta;
}

BOOL detour_install(InlineDetour *detour, void *target, void *replacement,
                    size_t patch_length, const uint8_t *expected) {
    DWORD old_protect;
    uint8_t *trampoline;
    size_t index;

    if (!detour || !target || !replacement || patch_length < 5 ||
        patch_length > sizeof(detour->original) || detour->installed) {
        return FALSE;
    }
    if (expected && memcmp(target, expected, patch_length) != 0) {
        return FALSE;
    }

    trampoline = (uint8_t *)VirtualAlloc(NULL, patch_length + 5,
                                         MEM_COMMIT | MEM_RESERVE,
                                         PAGE_EXECUTE_READWRITE);
    if (!trampoline) {
        return FALSE;
    }
    memcpy(detour->original, target, patch_length);
    memcpy(trampoline, target, patch_length);
    write_relative_jump(trampoline + patch_length,
                        (uint8_t *)target + patch_length);

    if (!VirtualProtect(target, patch_length, PAGE_EXECUTE_READWRITE,
                        &old_protect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return FALSE;
    }
    write_relative_jump((uint8_t *)target, replacement);
    for (index = 5; index < patch_length; ++index) {
        ((uint8_t *)target)[index] = 0x90;
    }
    FlushInstructionCache(GetCurrentProcess(), target, patch_length);
    VirtualProtect(target, patch_length, old_protect, &old_protect);

    detour->target = (uint8_t *)target;
    detour->replacement = replacement;
    detour->trampoline = trampoline;
    detour->patch_length = patch_length;
    detour->installed = TRUE;
    return TRUE;
}

BOOL detour_remove(InlineDetour *detour) {
    DWORD old_protect;
    if (!detour || !detour->installed) {
        return TRUE;
    }
    if (!VirtualProtect(detour->target, detour->patch_length,
                        PAGE_EXECUTE_READWRITE, &old_protect)) {
        return FALSE;
    }
    memcpy(detour->target, detour->original, detour->patch_length);
    FlushInstructionCache(GetCurrentProcess(), detour->target,
                          detour->patch_length);
    VirtualProtect(detour->target, detour->patch_length, old_protect,
                   &old_protect);
    VirtualFree(detour->trampoline, 0, MEM_RELEASE);
    detour->trampoline = NULL;
    detour->installed = FALSE;
    return TRUE;
}
