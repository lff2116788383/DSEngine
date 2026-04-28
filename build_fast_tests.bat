@echo off
setlocal enabledelayedexpansion

::: Change working directory to the script's location
pushd "%~dp0"

echo ========================================================
echo         DSEngine Fast Build Script (GTest)
echo ========================================================
echo.

set BUILD_DIR=build_vs2022

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [INFO] CMakeCache.txt not found. Running initial configure...
    cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_GTESTS=ON -DDSE_ENABLE_SPINE=ON -DDSE_ENABLE_3D=OFF

    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Initial CMake configure failed!
        pause
        exit /b !ERRORLEVEL!
    )
)

echo [INFO] Building gtest targets (Debug)...
cmake --build %BUILD_DIR% --config Debug --target dse_gtest_unit_tests
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Unit test build failed! Check the output above.
    popd
    pause
    exit /b !ERRORLEVEL!
)

echo [INFO] Building integration test target (Debug)...
cmake --build %BUILD_DIR% --config Debug --target dse_gtest_integration_tests
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Integration test build failed! Check the output above.
    popd
    pause
    exit /b !ERRORLEVEL!
)

echo [INFO] Building smoke test target (Debug)...
cmake --build %BUILD_DIR% --config Debug --target dse_gtest_smoke_tests
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Smoke test build failed! Check the output above.
    popd
    pause
    exit /b !ERRORLEVEL!
)

echo.
echo [INFO] Running gtest via CTest...
ctest --test-dir %BUILD_DIR% -C Debug --output-on-failure -L gtest
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] GTest failed.
    popd
    pause
    exit /b !ERRORLEVEL!
)

echo.
echo [OK] GTest passed.

echo.
echo [HINT] Run full engine label suite with:
echo       ctest --test-dir %BUILD_DIR% -C Debug --output-on-failure -L engine

echo.
popd
pause
