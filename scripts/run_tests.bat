@echo off
chcp 65001 >nul
title 言律时钟 - 单元测试
cd /d "%~dp0.."

set "PATH=D:\Tools\w64devkit\bin;%PATH%"
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target test_core --config Release
if %errorlevel% neq 0 (
    echo.
    echo ? 单元测试构建失败！
    pause
    exit /b %errorlevel%
)

echo.
build\bin\test_core.exe
echo.
pause
