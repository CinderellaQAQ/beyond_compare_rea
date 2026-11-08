WORKSPACE := $(CURDIR)
LOCAL_MINGW_ROOT := $(WORKSPACE)/runtime/mingw/root
LOCAL_MINGW_BIN := $(LOCAL_MINGW_ROOT)/usr/bin
LOCAL_GCC_LIB := $(LOCAL_MINGW_ROOT)/usr/lib/gcc/i686-w64-mingw32/9.3-win32
MINGW_PREFIX ?= i686-w64-mingw32

ifneq ($(wildcard $(LOCAL_MINGW_BIN)/i686-w64-mingw32-gcc-win32),)
CC := $(LOCAL_MINGW_BIN)/i686-w64-mingw32-gcc-win32
WINDRES := $(LOCAL_MINGW_BIN)/i686-w64-mingw32-windres
TOOLCHAIN_CFLAGS := -B $(LOCAL_GCC_LIB)/
else
CC := $(MINGW_PREFIX)-gcc
WINDRES := $(MINGW_PREFIX)-windres
TOOLCHAIN_CFLAGS :=
endif
PLUGIN_SRC := src/plugin/version_proxy.c src/plugin/bc3_target.c \
	src/plugin/detour.c src/plugin/log.c src/plugin/multidetails.c
BUILD_DIR := build
DIST_DIR := dist
PLUGIN_DLL := $(DIST_DIR)/version.dll
SETUP_EXE := $(DIST_DIR)/BC3MultiDetails-Setup-0.2.1.exe

CFLAGS := $(TOOLCHAIN_CFLAGS) -Os -Wall -Wextra -ffunction-sections \
	-fdata-sections -DUNICODE -D_UNICODE
LDFLAGS := -shared -Wl,--kill-at -Wl,--gc-sections
WINLIBS := -luser32 -lgdi32 -lkernel32

.PHONY: all plugin installer test-tools clean

all: plugin installer

plugin: $(PLUGIN_DLL)

installer: $(SETUP_EXE)

test-tools: $(BUILD_DIR)/wincontrol.exe $(BUILD_DIR)/winscreenshot.exe \
	$(BUILD_DIR)/winscroll.exe $(BUILD_DIR)/winclipboard.exe

$(BUILD_DIR) $(DIST_DIR):
	mkdir -p $@

$(BUILD_DIR)/version-resource.o: src/plugin/version.rc | $(BUILD_DIR)
	$(WINDRES) -i $< -o $@

$(PLUGIN_DLL): $(PLUGIN_SRC) $(BUILD_DIR)/version-resource.o | $(DIST_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(PLUGIN_SRC) \
		$(BUILD_DIR)/version-resource.o $(WINLIBS)

$(BUILD_DIR)/installer-resource.o: src/installer/installer.rc \
		src/installer/installer.manifest src/installer/resource.h $(PLUGIN_DLL) | $(BUILD_DIR)
	cd src/installer && $(WINDRES) -i installer.rc \
		-o $(WORKSPACE)/$(BUILD_DIR)/installer-resource.o

$(SETUP_EXE): src/installer/installer.c src/installer/resource.h \
		$(BUILD_DIR)/installer-resource.o | $(DIST_DIR)
	$(CC) $(CFLAGS) -municode -mwindows -o $@ src/installer/installer.c \
		$(BUILD_DIR)/installer-resource.o -lcomctl32 -lshell32 -lole32 \
		-ladvapi32 -luser32 -lkernel32

$(BUILD_DIR)/wincontrol.exe: tools/wincontrol.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -municode -mconsole -o $@ $< -luser32 -lkernel32

$(BUILD_DIR)/winscreenshot.exe: tools/winscreenshot.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -municode -mconsole -o $@ $< -lgdi32 -luser32 -lkernel32

$(BUILD_DIR)/winscroll.exe: tools/winscroll.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -municode -mconsole -o $@ $< -luser32 -lkernel32

$(BUILD_DIR)/winclipboard.exe: tools/winclipboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -mconsole -o $@ $< -luser32 -lkernel32

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
