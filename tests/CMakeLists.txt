cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_PREFIX_PATH /run/current-system/sw)
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found — tests disabled")
    return()
endif()

enable_testing()

set(TEST_SOURCES
    test_preset.cpp
    test_chiptune.cpp
    test_input.cpp
    test_renderer.cpp
    test_faust_bridge.cpp
    test_fx_chain.cpp
    test_terminal.cpp
    test_editor_syntax.cpp
)

add_executable(demodoom_tests
    ${TEST_SOURCES}
    # Source files needed for linking
    ${CMAKE_SOURCE_DIR}/src/renderer/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/font.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/widgets.cpp
    ${CMAKE_SOURCE_DIR}/src/input/input_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/input/gamepad_device.cpp
    ${CMAKE_SOURCE_DIR}/src/terminal/terminal.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/editor_screen.cpp
)

target_include_directories(demodoom_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(demodoom_tests PRIVATE
    $<TARGET_OBJECTS:demodoom_core>
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
target_link_libraries(demodoom_tests PRIVATE
    Catch2::Catch2WithMain
    PkgConfig::SDL2
    pthread dl m
)

if(nlohmann_json_FOUND)
    target_compile_definitions(demodoom_tests PRIVATE HAVE_NLOHMANN_JSON=1)
    target_link_libraries(demodoom_tests PRIVATE nlohmann_json::nlohmann_json)
endif()

if(RTMIDI_FOUND)
    target_compile_definitions(demodoom_tests PRIVATE HAVE_RTMIDI=1)
endif()

target_compile_options(demodoom_tests PRIVATE -Wall -Wextra -O2)

include(CTest)
include(Catch)
catch_discover_tests(demodoom_tests)
