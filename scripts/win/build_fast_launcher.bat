@echo off
setlocal enabledelayedexpansion

:: Change working directory to the script's location
pushd "%~dp0..\.."

echo ========================================================
echo         DSEngine Fast Build Script (Tauri Launcher)
echo ========================================================
echo.

set "LAUNCHER_DIR=apps\launcher_tauri"
set "NPM_CACHE=%CD%\.npm-cache"

where node >nul 2>&1
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Node.js is not installed or not in PATH!
    pause
    exit /b 1
)

where npm >nul 2>&1
if !ERRORLEVEL! neq 0 (
    echo [ERROR] npm is not available in PATH!
    pause
    exit /b 1
)

where cargo >nul 2>&1
if !ERRORLEVEL! neq 0 (
    if exist "%USERPROFILE%\.cargo\bin\cargo.exe" (
        set "PATH=!USERPROFILE!\.cargo\bin;!USERPROFILE!\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin;!PATH!"
        echo [OK] Found Cargo in USERPROFILE, added to PATH temporarily.
    ) else (
        echo [ERROR] Rust / Cargo is not installed or not in PATH!
        pause
        exit /b 1
    )
)

if not exist "!NPM_CACHE!" mkdir "!NPM_CACHE!" >nul 2>&1
set "NPM_CONFIG_CACHE=!NPM_CACHE!"

pushd "%LAUNCHER_DIR%"

if not exist "node_modules" (
    echo [INFO] node_modules not found. Running npm install...
    call npm install
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] npm install failed!
        popd
        popd
        pause
        exit /b !ERRORLEVEL!
    )
)

echo [INFO] Building launcher frontend assets...
call npm run build
if !ERRORLEVEL! neq 0 (
    echo [ERROR] npm run build failed!
    popd
    popd
    pause
    exit /b !ERRORLEVEL!
)

echo [INFO] Building Tauri launcher bundle...
call npm run tauri build
if !ERRORLEVEL! neq 0 (
    echo [ERROR] npm run tauri build failed!
    popd
    popd
    pause
    exit /b !ERRORLEVEL!
)

popd

echo.
echo [OK] Launcher build successful! Output is under [apps/launcher_tauri/src-tauri/target/release](apps/launcher_tauri/src-tauri/target/release).

popd
pause
