param(
    [string]$Action = ""
)

$toolchainPath = "D:\Tools\w64devkit\bin"
if (Test-Path $toolchainPath) {
    $env:PATH = "$toolchainPath;$env:PATH"
}

function Run-Tests {
    Write-Host "[1/2] Configuring CMake..." -ForegroundColor Cyan
    cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed!"
        return
    }

    Write-Host "[2/2] Building test_core..." -ForegroundColor Cyan
    cmake --build build --target test_core --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build test_core failed!"
        return
    }

    Write-Host "==================== RUNNING TESTS ====================" -ForegroundColor Green
    & "build\bin\test_core.exe"
    Write-Host "=======================================================" -ForegroundColor Green
}

function Build-And-Run {
    Write-Host "[1/2] Configuring CMake..." -ForegroundColor Cyan
    cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed!"
        return
    }

    Write-Host "[2/2] Building YanlvTimer..." -ForegroundColor Cyan
    cmake --build build --target YanlvTimer --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build YanlvTimer failed!"
        return
    }

    Write-Host "[SUCCESS] Launching YanlvTimer.exe..." -ForegroundColor Green
    Start-Process "build\bin\YanlvTimer.exe"
}

function Debug-GDB {
    Write-Host "Configuring Debug build..." -ForegroundColor Cyan
    cmake -B build_debug -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
    cmake --build build_debug --target YanlvTimer --config Debug
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Debug build failed!"
        return
    }
    Write-Host "Starting GDB..." -ForegroundColor Green
    & gdb "build_debug\bin\YanlvTimer.exe"
}

function Clean-Build {
    Write-Host "Cleaning build directories..." -ForegroundColor Yellow
    if (Test-Path "build") { Remove-Item -Recurse -Force "build" }
    if (Test-Path "build_debug") { Remove-Item -Recurse -Force "build_debug" }
    Write-Host "Done!" -ForegroundColor Green
}

if ($Action -eq "test") {
    Run-Tests
    exit $LASTEXITCODE
}
if ($Action -eq "run") {
    Build-And-Run
    exit $LASTEXITCODE
}

while ($true) {
    Write-Host ""
    Write-Host "=======================================================" -ForegroundColor Cyan
    Write-Host "      Yanlv-timer (言律时钟) Local Dev & Test Menu     " -ForegroundColor Yellow
    Write-Host "=======================================================" -ForegroundColor Cyan
    Write-Host "  1. Build and Run Automated Tests (test_core.exe)"
    Write-Host "  2. Build and Launch Application (YanlvTimer.exe)"
    Write-Host "  3. Build Debug and Launch GDB Debugger"
    Write-Host "  4. Clean Build Directories"
    Write-Host "  5. Exit"
    Write-Host "=======================================================" -ForegroundColor Cyan
    $choice = Read-Host "Please enter option (1-5)"

    switch ($choice) {
        "1" { Run-Tests }
        "2" { Build-And-Run }
        "3" { Debug-GDB }
        "4" { Clean-Build }
        "5" { exit 0 }
        default { Write-Warning "Invalid option. Please enter 1-5." }
    }
}
