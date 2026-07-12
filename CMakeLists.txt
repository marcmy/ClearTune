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
    src/core/WizardModel.cpp
)
target_include_directories(cleartune_core PUBLIC src)

if(MSVC)
    target_compile_options(cleartune_core PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(cleartune_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

if(WIN32)
    add_executable(ClearTune WIN32
        src/win32/App.cpp
        src/win32/ClearTypeSettings.cpp
        src/win32/MainWindow.cpp
        src/win32/MonitorService.cpp
        src/win32/Preferences.cpp
        src/win32/SampleRenderer.cpp
        src/win32/ThemeService.cpp
        src/win32/Win32Error.cpp
        src/win32/app.rc
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
        d2d1
        dwrite
        dwmapi
        gdi32
        ole32
        user32
        uxtheme
    )
    if(MSVC)
        target_compile_options(ClearTune PRIVATE /W4 /permissive- /EHsc /utf-8)
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
