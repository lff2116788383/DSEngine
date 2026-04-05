@echo off
setlocal enabledelayedexpansion

:: Change working directory to the script's location
pushd "%~dp0"

echo ========================================================
echo         DSEngine Fast Build Script (Lua Runtime)
echo ========================================================
echo.

set BUILD_DIR=build_vs2022
set TARGET_NAME=dse_example_lua

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [INFO] CMakeCache.txt not found. Running initial configure...
    cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_ENGINE_TESTS=OFF
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Initial CMake configure failed!
        pause
        exit /b !ERRORLEVEL!
    )
)

echo [INFO] Building %TARGET_NAME% (Debug)...
cmake --build %BUILD_DIR% --config Debug --target %TARGET_NAME%

if %ERRORLEVEL% equ 0 (
    echo.
    echo [OK] Build successful! You can find the executable in the bin\ directory.
    echo Run bin\DSEngine_lua_debug.exe or bin\DSEngine_lua.exe to start.
) else (
    echo.
    echo [ERROR] Build failed! Check the output above.
)

popd
pause
