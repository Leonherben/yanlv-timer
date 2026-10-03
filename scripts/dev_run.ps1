# ========================================================
#   言律时钟 (Yanlv-timer) Windows 开发调试启动器
# ========================================================

# 确保 UTF-8 编码输出
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$host.UI.RawUI.WindowTitle = "言律时钟 - 开发调试模式"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "   言律时钟 (Yanlv-timer) Windows 开发调试启动器" -ForegroundColor Yellow
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

# 1. 检查便携工具链环境
$toolchainPath = "D:\Tools\w64devkit\bin"
if (Test-Path $toolchainPath) {
    $env:PATH = "$toolchainPath;$env:PATH"
}

# 2. 检查是否有旧实例残留，有则先关闭以防编译文件被占用
$existingProc = Get-Process -Name "YanlvTimer" -ErrorAction SilentlyContinue
if ($existingProc) {
    Write-Host "[提示] 检测到已有时钟进程运行，正在关闭旧进程..." -ForegroundColor Yellow
    Stop-Process -Name "YanlvTimer" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300
}

# 3. 检查 CMake / 编译器
$cmakeCmd = Get-Command "cmake" -ErrorAction SilentlyContinue
if (-not $cmakeCmd) {
    Write-Error "未找到 CMake！请确保 D:\Tools\w64devkit\bin 或系统环境 PATH 中包含 cmake。"
    Read-Host "按回车键退出..."
    exit 1
}

# 4. 自动检查增量编译 (若未修改则 0.2 秒完成，若代码修改则自动重新编译)
Write-Host "[1/2] 正在检查代码更新并编译..." -ForegroundColor Cyan

if (-not (Test-Path "build")) {
    cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake 生成构建配置失败！"
        Read-Host "按回车键退出..."
        exit $LASTEXITCODE
    }
}

cmake --build build --target YanlvTimer --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Error "代码编译失败，请根据上方编译提示修改代码！"
    Read-Host "按回车键退出..."
    exit $LASTEXITCODE
}

$exePath = "build\bin\YanlvTimer.exe"
if (-not (Test-Path $exePath)) {
    Write-Error "未找到可执行文件: $exePath"
    Read-Host "按回车键退出..."
    exit 1
}

# 5. 启动时钟并绑定父控制台监控
Write-Host "[2/2] 正在启动言律时钟..." -ForegroundColor Green
Write-Host ""
Write-Host "========================================================" -ForegroundColor DarkGray
Write-Host " ★ 时钟已成功运行！" -ForegroundColor Green
Write-Host " ★ 调试提示：直接点击本控制台窗口右上角 [X] 即可关闭时钟软件！" -ForegroundColor Yellow
Write-Host " ★ 修改源码后，再次双击此脚本即可自动编译并运行最新效果。" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor DarkGray
Write-Host ""

# 启动并绑定当前 PowerShell 进程 PID ($PID)
$timerProcess = Start-Process -FilePath $exePath -ArgumentList "--parent-pid $PID" -PassThru

# 保持前台等待主程序退出
$timerProcess.WaitForExit()

Write-Host "言律时钟已退出。" -ForegroundColor Gray
