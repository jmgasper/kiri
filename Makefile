# Native Haiku build; needs only the development tools bundled with Haiku.
CXX ?= g++
CC ?= gcc
BUILD ?= build-haiku
CPPFLAGS += -Isrc -Ivendor -Ivendor/libvterm/include -I/boot/system/develop/headers/scintilla -I/boot/system/develop/headers/lexilla
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wno-multichar -Wno-misleading-indentation
CFLAGS ?= -O2
CFLAGS += -std=c99
CORE = $(wildcard src/core/*.cpp)
UI = $(wildcard src/ui/*.cpp) src/main.cpp
VTERM = $(wildcard vendor/libvterm/src/*.c)
CORE_OBJ = $(CORE:%.cpp=$(BUILD)/%.o)
UI_OBJ = $(UI:%.cpp=$(BUILD)/%.o)
VTERM_OBJ = $(VTERM:%.c=$(BUILD)/%.o)
LIBS = -lpcre2-8 -lbe -ltracker -ltranslation -lscintilla -llexilla
.PHONY: all check check-native check-language check-workspace workspace-smoke launcher-smoke package clean
all: $(BUILD)/Kiri
$(BUILD)/Kiri: $(CORE_OBJ) $(UI_OBJ) $(VTERM_OBJ) resources/Kiri.rdef resources/branding/kiri-icon.hvif
	$(CXX) -o $@.new $(CORE_OBJ) $(UI_OBJ) $(VTERM_OBJ) $(LIBS)
	rc -o $(BUILD)/Kiri.rsrc resources/Kiri.rdef
	xres -o $@.new $(BUILD)/Kiri.rsrc
	mimeset -f $@.new
	mv $@.new $@
$(BUILD)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/kiri_tests: $(CORE_OBJ) $(VTERM_OBJ) $(BUILD)/tests/CoreTests.o
	$(CXX) -o $@ $^ -lbe -lpcre2-8
$(BUILD)/kiri_search_tests: $(CORE_OBJ) $(VTERM_OBJ) $(BUILD)/tests/SearchTests.o
	$(CXX) -o $@ $^ -lbe -lpcre2-8
$(BUILD)/kiri_refresh_diff_tests: $(CORE_OBJ) $(VTERM_OBJ) $(BUILD)/tests/RefreshDiffTests.o
	$(CXX) -o $@ $^ -lbe -lpcre2-8
check: $(BUILD)/kiri_tests $(BUILD)/kiri_search_tests $(BUILD)/kiri_refresh_diff_tests
	$(BUILD)/kiri_tests
	$(BUILD)/kiri_search_tests
	$(BUILD)/kiri_refresh_diff_tests
$(BUILD)/kiri_language_tests: $(CORE_OBJ) $(VTERM_OBJ) $(BUILD)/tests/LanguageTests.o
	$(CXX) -o $@ $^ -lbe -lpcre2-8
check-language: $(BUILD)/kiri_language_tests
	$(BUILD)/kiri_language_tests
$(BUILD)/kiri_native_tests: $(CORE_OBJ) $(VTERM_OBJ) $(BUILD)/src/ui/Editor.o $(BUILD)/src/ui/Theme.o $(BUILD)/src/ui/EditorSettings.o $(BUILD)/src/ui/FileIcons.o $(BUILD)/src/ui/RecentItems.o $(BUILD)/tests/NativeTests.o
	$(CXX) -o $@ $^ $(LIBS)
check-native: $(BUILD)/kiri_native_tests
	$(BUILD)/kiri_native_tests
$(BUILD)/kiri_workspace_smoke: $(CORE_OBJ) $(VTERM_OBJ) $(filter-out $(BUILD)/src/main.o,$(UI_OBJ)) $(BUILD)/tests/WorkspaceSmoke.o
	$(CXX) -o $@.new $^ $(LIBS)
	mv $@.new $@
workspace-smoke: $(BUILD)/kiri_workspace_smoke
$(BUILD)/kiri_workspace_tests: $(CORE_OBJ) $(VTERM_OBJ) $(filter-out $(BUILD)/src/main.o,$(UI_OBJ)) $(BUILD)/tests/WorkspaceTests.o
	$(CXX) -o $@ $^ $(LIBS)
check-workspace: $(BUILD)/kiri_workspace_tests
	$(BUILD)/kiri_workspace_tests
$(BUILD)/kiri_launcher_smoke: $(CORE_OBJ) $(VTERM_OBJ) $(filter-out $(BUILD)/src/main.o,$(UI_OBJ)) $(BUILD)/tests/LauncherSmoke.o
	$(CXX) -o $@.new $^ $(LIBS)
	mv $@.new $@
launcher-smoke: $(BUILD)/kiri_launcher_smoke
package: all
	bash tools/package-haiku.sh
clean:
	rm -rf $(BUILD)
-include $(CORE_OBJ:.o=.d) $(UI_OBJ:.o=.d) $(VTERM_OBJ:.o=.d) $(BUILD)/tests/CoreTests.d $(BUILD)/tests/NativeTests.d $(BUILD)/tests/WorkspaceSmoke.d $(BUILD)/tests/LauncherSmoke.d $(BUILD)/tests/LanguageTests.d $(BUILD)/tests/WorkspaceTests.d $(BUILD)/tests/SearchTests.d $(BUILD)/tests/RefreshDiffTests.d
