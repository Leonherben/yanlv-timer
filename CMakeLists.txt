cmake_minimum_required(VERSION 3.20)
project(YanlvTimer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# MSVC 静态运行时链接 (/MT)，完全摆脱外部 VC 运行库依赖
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /utf-8 /permissive- /Zc:__cplusplus)
    add_link_options(/SUBSYSTEM:WINDOWS)
else()
    # MinGW / GCC 配置
    add_compile_options(-Wall -Wextra -O3)
    add_link_options(-mwindows -static -static-libgcc -static-libstdc++)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 源码列表
set(SOURCES
    src/db/sqlite3.c
    src/db/repository.cpp
    src/core/timer_engine.cpp
    src/core/power_listener.cpp
    src/ui/d2d_renderer.cpp
    src/ui/floating_clock.cpp
    src/ui/quick_start_popup.cpp
    src/ui/break_fullscreen.cpp
    src/ui/management_window.cpp
    src/app/tray_icon.cpp
    src/app/main.cpp
)

add_executable(YanlvTimer WIN32 ${SOURCES})

# 链接 Windows 原生系统库
target_link_libraries(YanlvTimer PRIVATE
    d2d1
    dwrite
    windowscodecs
    comctl32
    comdlg32
    shell32
    ole32
    uuid
    shlwapi
    mf
    mfplat
    mfuuid
    mfreadwrite
)

# 安装与发布配置
install(TARGETS YanlvTimer DESTINATION .)
install(DIRECTORY assets DESTINATION .)
