@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\dev_run.ps1"
if %errorlevel% neq 0 (
    echo.
    echo ------------------------------------------
    echo 程序退出，错误代码: %errorlevel%
    pause
)
