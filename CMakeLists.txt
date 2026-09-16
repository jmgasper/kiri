cmake_minimum_required(VERSION 3.18)
project(Kiri VERSION 0.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
add_library(kiri_core
    src/core/Markdown.cpp
    src/core/DocumentSettings.cpp src/core/EditorConfig.cpp
    src/core/ColorTheme.cpp src/core/LanguageAnalysis.cpp
    src/core/Process.cpp src/core/Git.cpp src/core/GitDiff.cpp src/core/Diff.cpp src/core/FileIO.cpp
    src/core/Search.cpp src/core/EditTransaction.cpp src/core/Project.cpp src/core/ProjectMonitor.cpp src/core/Terminal.cpp src/core/Recovery.cpp
    src/core/LanguageTools.cpp src/core/LanguageProtocol.cpp src/core/LanguageServer.cpp)
target_include_directories(kiri_core PUBLIC src vendor)
target_compile_options(kiri_core PRIVATE -Wall -Wextra -Wpedantic)
find_path(PCRE2_INCLUDE_DIR pcre2.h REQUIRED)
find_library(PCRE2_LIBRARY pcre2-8 REQUIRED)
target_include_directories(kiri_core PRIVATE ${PCRE2_INCLUDE_DIR})
target_link_libraries(kiri_core PUBLIC Threads::Threads ${PCRE2_LIBRARY})
add_library(kiri_md4c STATIC vendor/md4c/md4c.c vendor/md4c/entity.c)
target_link_libraries(kiri_core PUBLIC kiri_md4c)

file(GLOB VTERM_SOURCES CONFIGURE_DEPENDS vendor/libvterm/src/*.c)
add_library(kiri_vterm STATIC ${VTERM_SOURCES})
target_include_directories(kiri_vterm PUBLIC vendor/libvterm/include)
set_property(TARGET kiri_vterm PROPERTY C_STANDARD 99)
target_link_libraries(kiri_core PUBLIC kiri_vterm)

include(CTest)
if(BUILD_TESTING)
    add_executable(kiri_markdown_tests tests/MarkdownTests.cpp)
    target_link_libraries(kiri_markdown_tests PRIVATE kiri_core)
    add_test(NAME kiri_markdown COMMAND kiri_markdown_tests)
    add_executable(kiri_document_settings_tests tests/DocumentSettingsTests.cpp)
    target_link_libraries(kiri_document_settings_tests PRIVATE kiri_core)
    add_test(NAME kiri_document_settings COMMAND kiri_document_settings_tests)
    add_executable(kiri_analysis_theme_tests tests/AnalysisThemeTests.cpp)
    target_link_libraries(kiri_analysis_theme_tests PRIVATE kiri_core)
    add_test(NAME kiri_analysis_theme COMMAND kiri_analysis_theme_tests)
    add_executable(kiri_refresh_diff_tests tests/RefreshDiffTests.cpp)
    target_link_libraries(kiri_refresh_diff_tests PRIVATE kiri_core)
    add_test(NAME kiri_refresh_diff COMMAND kiri_refresh_diff_tests)
    add_executable(kiri_search_tests tests/SearchTests.cpp)
    target_link_libraries(kiri_search_tests PRIVATE kiri_core)
    add_test(NAME kiri_search COMMAND kiri_search_tests)
    add_executable(kiri_tests tests/CoreTests.cpp)
    target_link_libraries(kiri_tests PRIVATE kiri_core)
    add_test(NAME kiri_core COMMAND kiri_tests)
    add_executable(kiri_language_tests tests/LanguageTests.cpp)
    target_link_libraries(kiri_language_tests PRIVATE kiri_core)
    add_test(NAME kiri_language COMMAND kiri_language_tests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    find_path(SCINTILLA_INCLUDE_DIR ScintillaView.h PATH_SUFFIXES scintilla REQUIRED)
    find_path(LEXILLA_INCLUDE_DIR SciLexer.h PATH_SUFFIXES lexilla REQUIRED)
    find_library(SCINTILLA_LIBRARY scintilla REQUIRED)
    find_library(LEXILLA_LIBRARY lexilla REQUIRED)
    set(KIRI_UI_SOURCES src/ui/Workspace.cpp src/ui/MarkdownView.cpp src/ui/WorkspaceMarkdown.cpp src/ui/Editor.cpp src/ui/EditorAnalysis.cpp src/ui/EditorIndentation.cpp
        src/ui/WorkspaceSettings.cpp src/ui/DocumentSettingsWindow.cpp src/ui/MinimapView.cpp
        src/ui/Theme.cpp src/ui/Explorer.cpp src/ui/TerminalView.cpp
        src/ui/PreviewView.cpp src/ui/GitView.cpp src/ui/DiffView.cpp src/ui/TabStrip.cpp
        src/ui/TerminalPanel.cpp src/ui/RefreshButton.cpp
        src/ui/EditorSettings.cpp src/ui/PreferencesWindow.cpp src/ui/FileIcons.cpp
        src/ui/Application.cpp src/ui/LauncherWindow.cpp src/ui/RecentItems.cpp
        src/ui/WorkspaceAnalysis.cpp src/ui/ProblemsView.cpp src/ui/WorkspaceSearch.cpp src/ui/WorkspaceRefresh.cpp src/ui/WorkspaceRename.cpp src/ui/EditPreviewWindow.cpp src/ui/WorkspaceLanguage.cpp src/ui/WorkspacePanes.cpp src/ui/WorkspaceTabs.cpp src/ui/SymbolBar.cpp src/ui/LanguageToolsWindow.cpp)
    add_executable(Kiri src/main.cpp ${KIRI_UI_SOURCES})
    target_include_directories(Kiri PRIVATE ${SCINTILLA_INCLUDE_DIR} ${LEXILLA_INCLUDE_DIR})
    target_compile_options(Kiri PRIVATE -Wall -Wextra -Wno-multichar)
    target_link_libraries(Kiri PRIVATE kiri_core ${SCINTILLA_LIBRARY} ${LEXILLA_LIBRARY}
        be tracker translation)
    if(BUILD_TESTING)
        add_executable(kiri_markdown_native_tests tests/MarkdownNativeTests.cpp ${KIRI_UI_SOURCES})
        target_include_directories(kiri_markdown_native_tests PRIVATE ${SCINTILLA_INCLUDE_DIR} ${LEXILLA_INCLUDE_DIR})
        target_compile_options(kiri_markdown_native_tests PRIVATE -Wno-multichar)
        target_link_libraries(kiri_markdown_native_tests PRIVATE kiri_core ${SCINTILLA_LIBRARY} ${LEXILLA_LIBRARY} be tracker translation)
        add_test(NAME kiri_markdown_native COMMAND kiri_markdown_native_tests)
        set_tests_properties(kiri_markdown_native PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        add_executable(kiri_native_tests tests/NativeTests.cpp src/ui/Editor.cpp src/ui/EditorAnalysis.cpp src/ui/EditorIndentation.cpp src/ui/MinimapView.cpp src/ui/Theme.cpp src/ui/EditorSettings.cpp src/ui/FileIcons.cpp src/ui/RecentItems.cpp)
        target_include_directories(kiri_native_tests PRIVATE ${SCINTILLA_INCLUDE_DIR} ${LEXILLA_INCLUDE_DIR})
        target_link_libraries(kiri_native_tests PRIVATE kiri_core ${SCINTILLA_LIBRARY} ${LEXILLA_LIBRARY} be)
        add_test(NAME kiri_native COMMAND kiri_native_tests)
        add_executable(kiri_workspace_smoke EXCLUDE_FROM_ALL tests/WorkspaceSmoke.cpp ${KIRI_UI_SOURCES})
        add_executable(kiri_workspace_tests tests/WorkspaceTests.cpp ${KIRI_UI_SOURCES})
        target_include_directories(kiri_workspace_tests PRIVATE ${SCINTILLA_INCLUDE_DIR} ${LEXILLA_INCLUDE_DIR})
        target_compile_options(kiri_workspace_tests PRIVATE -Wno-multichar)
        target_link_libraries(kiri_workspace_tests PRIVATE kiri_core ${SCINTILLA_LIBRARY} ${LEXILLA_LIBRARY} be tracker translation)
        add_test(NAME kiri_workspace COMMAND kiri_workspace_tests)
        target_include_directories(kiri_workspace_smoke PRIVATE ${SCINTILLA_INCLUDE_DIR} ${LEXILLA_INCLUDE_DIR})
        target_compile_options(kiri_workspace_smoke PRIVATE -Wno-multichar)
        target_link_libraries(kiri_workspace_smoke PRIVATE kiri_core ${SCINTILLA_LIBRARY} ${LEXILLA_LIBRARY} be tracker translation)
        add_executable(kiri_launcher_smoke EXCLUDE_FROM_ALL tests/LauncherSmoke.cpp ${KIRI_UI_SOURCES})
        target_include_directories(kiri_launcher_smoke PRIVATE ${SCINTILLA_INCLUDE_DIR} ${LEXILLA_INCLUDE_DIR})
        target_compile_options(kiri_launcher_smoke PRIVATE -Wno-multichar)
        target_link_libraries(kiri_launcher_smoke PRIVATE kiri_core ${SCINTILLA_LIBRARY} ${LEXILLA_LIBRARY} be tracker translation)
    endif()
    find_program(RC_EXECUTABLE rc REQUIRED)
    find_program(XRES_EXECUTABLE xres REQUIRED)
    find_program(MIMESET_EXECUTABLE mimeset REQUIRED)
    set_property(TARGET Kiri APPEND PROPERTY LINK_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/Kiri.rdef
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/branding/kiri-icon.hvif)
    add_custom_command(TARGET Kiri POST_BUILD
        COMMAND ${RC_EXECUTABLE} -o ${CMAKE_CURRENT_BINARY_DIR}/Kiri.rsrc ${CMAKE_CURRENT_SOURCE_DIR}/resources/Kiri.rdef
        COMMAND ${XRES_EXECUTABLE} -o $<TARGET_FILE:Kiri> ${CMAKE_CURRENT_BINARY_DIR}/Kiri.rsrc
        COMMAND ${MIMESET_EXECUTABLE} -f $<TARGET_FILE:Kiri>)
    install(TARGETS Kiri RUNTIME DESTINATION apps)
    install(FILES README.md LICENSE DESTINATION documentation/packages/kiri)
    install(DIRECTORY docs DESTINATION documentation/packages/kiri)
    install(FILES vendor/libvterm/LICENSE vendor/libvterm/UPSTREAM.md
        DESTINATION documentation/packages/kiri/vendor/libvterm)
    install(FILES vendor/nlohmann/LICENSE.MIT vendor/nlohmann/UPSTREAM.md
        DESTINATION documentation/packages/kiri/vendor/nlohmann)
    install(FILES vendor/md4c/LICENSE.md vendor/md4c/UPSTREAM.md
        DESTINATION documentation/packages/kiri/vendor/md4c)
    install(PROGRAMS tools/install-language-tools.sh DESTINATION documentation/packages/kiri/tools)
else()
    message(STATUS "Host build: portable core and tests. Build on Haiku for the native UI.")
endif()
