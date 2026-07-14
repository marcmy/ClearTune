cmake_minimum_required(VERSION 3.24)
project(ClearTune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cleartune_core
    src/core/Theme.cpp
    src/core/DisplayKey.cpp
    src/core/Candidates.cpp
    src/core/Conversions.cpp
    src/core/MonitorIdentity.cpp
    src/core/MonitorLayout.cpp
    src/core/WizardModel.cpp
)
target_include_directories(cleartune_core PUBLIC src)

if(MSVC)
    target_compile_options(cleartune_core PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(cleartune_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

if(WIN32)
    set(CLEARTUNE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${CLEARTUNE_GENERATED_DIR}")

    set(CLEARTUNE_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/win32/ClearTune.ico")
    set(CLEARTUNE_ICON_PATH "${CLEARTUNE_GENERATED_DIR}/ClearTune.ico")
    execute_process(
        COMMAND certutil.exe -f -decode "${CLEARTUNE_ICON_SOURCE}" "${CLEARTUNE_ICON_PATH}"
        RESULT_VARIABLE CLEARTUNE_ICON_DECODE_RESULT
        OUTPUT_QUIET
        ERROR_VARIABLE CLEARTUNE_ICON_DECODE_ERROR
    )
    if(NOT CLEARTUNE_ICON_DECODE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to decode ClearTune application icon: ${CLEARTUNE_ICON_DECODE_ERROR}")
    endif()

    set(CLEARTUNE_MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/win32/app.manifest")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/win32/app.rc"
        "${CLEARTUNE_GENERATED_DIR}/app.rc"
        @ONLY
    )

    add_executable(ClearTune WIN32
        src/win32/App.cpp
        src/win32/ClearTypeSettings.cpp
        src/win32/ClearTypeSettingsAccessors.cpp
        src/win32/MainWindow.cpp
        src/win32/MainWindowControls.cpp
        src/win32/MainWindowNavigation.cpp
        src/win32/MainWindowView.cpp
        src/win32/MonitorMapControl.cpp
        src/win32/MonitorService.cpp
        src/win32/Preferences.cpp
        src/win32/SampleRenderer.cpp
        src/win32/ThemeService.cpp
        src/win32/Win32Error.cpp
        "${CLEARTUNE_GENERATED_DIR}/app.rc"
    )
    target_include_directories(ClearTune PRIVATE src src/win32)
    target_compile_definitions(ClearTune PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(ClearTune PRIVATE
        cleartune_core
        advapi32
        comctl32
        dwrite
        dwmapi
        gdi32
        ole32
        user32
        uxtheme
    )
    if(MSVC)
        target_compile_options(ClearTune PRIVATE /W4 /permissive- /EHsc /utf-8)
        target_link_options(ClearTune PRIVATE /MANIFEST:NO)
    endif()
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ClearTune)
endif()

enable_testing()
add_executable(cleartune_core_tests tests/CoreTests.cpp)
target_link_libraries(cleartune_core_tests PRIVATE cleartune_core)
if(MSVC)
    target_compile_options(cleartune_core_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME core COMMAND cleartune_core_tests)

add_executable(cleartune_navigation_review_tests tests/NavigationReviewTests.cpp)
target_link_libraries(cleartune_navigation_review_tests PRIVATE cleartune_core)
if(MSVC)
    target_compile_options(cleartune_navigation_review_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME navigation_review COMMAND cleartune_navigation_review_tests)

add_executable(cleartune_final_polish_tests tests/FinalPolishTests.cpp)
target_link_libraries(cleartune_final_polish_tests PRIVATE cleartune_core)
if(MSVC)
    target_compile_options(cleartune_final_polish_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_test(NAME final_polish COMMAND cleartune_final_polish_tests)
