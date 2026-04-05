@echo off
setlocal enabledelayedexpansion

:: Change working directory to the script's location
pushd "%~dp0"

echo ========================================================
echo         DSEngine Fast Build Script (Engine Tests)
echo ========================================================
echo.

set BUILD_DIR=build_vs2022
set GATE_REGEX=engine.unit|engine.lua_runtime|engine.cpp_runtime|engine.resource_injection|engine.spine|engine.2d.ui|engine.2d.physics2d|engine.2d.particle|engine.2d.localization

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [INFO] CMakeCache.txt not found. Running initial configure...
    cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_ENGINE_TESTS=ON
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Initial CMake configure failed!
        pause
        exit /b !ERRORLEVEL!
    )
)

echo [INFO] Building engine test targets ^(Debug^) via Visual Studio solution...
cmake --build %BUILD_DIR% --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_spine_tests
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Build failed! Check the output above.
    popd
    pause
    exit /b !ERRORLEVEL!
)

echo.
echo [INFO] Running minimal regression gate via CTest...
ctest --test-dir %BUILD_DIR% -C Debug --output-on-failure -R "%GATE_REGEX%"
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Minimal regression gate failed.
    popd
    pause
    exit /b !ERRORLEVEL!
)

echo.
echo [OK] Minimal regression gate passed.
echo [OK] Gate set: engine.unit, engine.lua_runtime, engine.cpp_runtime, engine.resource_injection, engine.spine, engine.2d.ui, engine.2d.physics2d, engine.2d.particle, engine.2d.localization

echo.
echo [HINT] Run full engine label suite with:
echo        ctest --test-dir %BUILD_DIR% -C Debug --output-on-failure -L engine

echo.
popd
pause
