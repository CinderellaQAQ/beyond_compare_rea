#ifndef BC3_DETOUR_H
#define BC3_DETOUR_H

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

typedef struct InlineDetour {
    uint8_t *target;
    void *replacement;
    void *trampoline;
    size_t patch_length;
    uint8_t original[16];
    BOOL installed;
} InlineDetour;

BOOL detour_install(InlineDetour *detour, void *target, void *replacement,
                    size_t patch_length, const uint8_t *expected);
BOOL detour_remove(InlineDetour *detour);

#endif
