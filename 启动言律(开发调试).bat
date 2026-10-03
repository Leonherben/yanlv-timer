@echo off
chcp 65001 >nul
title 言律时钟 - 开发调试模式
cd /d "%~dp0"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\dev_run.ps1"
if %errorlevel% neq 0 (
    echo.
    echo ? 进程退出，错误代码: %errorlevel%
    pause
)
