@echo off
setlocal enabledelayedexpansion

:: Change working directory to the script's location
pushd "%~dp0"

echo ========================================================
echo         DSEngine Fast Build Script (SDK Install)
echo ========================================================
echo.

set BUILD_DIR=build_vs2022
set INSTALL_DIR=.\bin\sdk

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [INFO] CMakeCache.txt not found. Running initial configure...
    cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_GTESTS=OFF
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Initial CMake configure failed!
        pause
        exit /b !ERRORLEVEL!
    )
)

echo [INFO] Installing SDK from %BUILD_DIR% (Debug) to %INSTALL_DIR%...
cmake --install %BUILD_DIR% --config Debug --prefix "%INSTALL_DIR%"

if %ERRORLEVEL% equ 0 (
    echo.
    echo [OK] SDK install successful! Output directory: %INSTALL_DIR%
) else (
    echo.
    echo [ERROR] SDK install failed! Check the output above.
)

popd
pause
