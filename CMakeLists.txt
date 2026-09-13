cmake_minimum_required(VERSION 3.18)
project(Kiri VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
add_library(kiri_core
    src/core/Process.cpp src/core/Git.cpp src/core/FileIO.cpp
    src/core/Project.cpp src/core/Terminal.cpp src/core/Recovery.cpp)
target_include_directories(kiri_core PUBLIC src)
target_compile_options(kiri_core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(kiri_core PUBLIC Threads::Threads)

file(GLOB VTERM_SOURCES CONFIGURE_DEPENDS vendor/libvterm/src/*.c)
add_library(kiri_vterm STATIC ${VTERM_SOURCES})
target_include_directories(kiri_vterm PUBLIC vendor/libvterm/include)
set_property(TARGET kiri_vterm PROPERTY C_STANDARD 99)
target_link_libraries(kiri_core PUBLIC kiri_vterm)

include(CTest)
if(BUILD_TESTING)
    add_executable(kiri_tests tests/CoreTests.cpp)
    target_link_libraries(kiri_tests PRIVATE kiri_core)
    add_test(NAME kiri_core COMMAND kiri_tests)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    find_path(SCINTILLA_INCLUDE_DIR ScintillaView.h PATH_SUFFIXES scintilla REQUIRED)
    find_path(LEXILLA_INCLUDE_DIR SciLexer.h PATH_SUFFIXES lexilla REQUIRED)
    find_library(SCINTILLA_LIBRARY scintilla REQUIRED)
    find_library(LEXILLA_LIBRARY lexilla REQUIRED)
    set(KIRI_UI_SOURCES src/ui/Workspace.cpp src/ui/Editor.cpp
        src/ui/Theme.cpp src/ui/Explorer.cpp src/ui/TerminalView.cpp
        src/ui/PreviewView.cpp src/ui/GitView.cpp src/ui/TabStrip.cpp
        src/ui/TerminalPanel.cpp src/ui/RefreshButton.cpp
        src/ui/EditorSettings.cpp src/ui/PreferencesWindow.cpp src/ui/FileIcons.cpp
        src/ui/Application.cpp src/ui/LauncherWindow.cpp src/ui/RecentItems.cpp)
    add_executable(Kiri src/main.cpp ${KIRI_UI_SOURCES})
    target_include_directories(Kiri PRIVATE ${SCINTILLA_INCLUDE_DIR} ${LEXILLA_INCLUDE_DIR})
    target_compile_options(Kiri PRIVATE -Wall -Wextra -Wno-multichar)
    target_link_libraries(Kiri PRIVATE kiri_core ${SCINTILLA_LIBRARY} ${LEXILLA_LIBRARY}
        be tracker translation)
    if(BUILD_TESTING)
        add_executable(kiri_native_tests tests/NativeTests.cpp src/ui/Editor.cpp src/ui/Theme.cpp src/ui/EditorSettings.cpp src/ui/FileIcons.cpp src/ui/RecentItems.cpp)
        target_include_directories(kiri_native_tests PRIVATE ${SCINTILLA_INCLUDE_DIR} ${LEXILLA_INCLUDE_DIR})
        target_link_libraries(kiri_native_tests PRIVATE kiri_core ${SCINTILLA_LIBRARY} ${LEXILLA_LIBRARY} be)
        add_test(NAME kiri_native COMMAND kiri_native_tests)
        add_executable(kiri_workspace_smoke EXCLUDE_FROM_ALL tests/WorkspaceSmoke.cpp ${KIRI_UI_SOURCES})
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
    add_custom_command(TARGET Kiri POST_BUILD
        COMMAND ${RC_EXECUTABLE} -o ${CMAKE_CURRENT_BINARY_DIR}/Kiri.rsrc ${CMAKE_CURRENT_SOURCE_DIR}/resources/Kiri.rdef
        COMMAND ${XRES_EXECUTABLE} -o $<TARGET_FILE:Kiri> ${CMAKE_CURRENT_BINARY_DIR}/Kiri.rsrc)
    install(TARGETS Kiri RUNTIME DESTINATION apps)
    install(FILES README.md LICENSE DESTINATION documentation/packages/kiri)
    install(DIRECTORY docs DESTINATION documentation/packages/kiri)
    install(FILES vendor/libvterm/LICENSE vendor/libvterm/UPSTREAM.md
        DESTINATION documentation/packages/kiri/vendor/libvterm)
else()
    message(STATUS "Host build: portable core and tests. Build on Haiku for the native UI.")
endif()
