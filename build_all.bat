@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo         DSEngine Full Build and Verification Script
echo ========================================================
echo.

set BUILD_DIR=build_vs2022
set GENERATOR="Visual Studio 17 2022"
set ARCH=x64

:: 0. Environment Check
echo [0/4] Checking build environment...

:: Check CMake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH!
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

:: Check CMake execution
cmake --version | findstr /C:"cmake version" >nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake execution failed!
    pause
    exit /b 1
)
echo [OK] CMake found.

:: 1. Checking and cleaning old build cache
echo [1/4] Checking and cleaning old build cache...
if exist %BUILD_DIR%\CMakeCache.txt (
    echo Found existing CMakeCache.txt, removing it...
    del /f /q %BUILD_DIR%\CMakeCache.txt
)

:: 2. Configure CMake project
echo.
echo [2/4] Configuring CMake project...
cmake -S . -B %BUILD_DIR% -G %GENERATOR% -A %ARCH%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake Configure failed!
    pause
    exit /b %ERRORLEVEL%
)

:: 3. Build all targets
echo.
echo [3/4] Building all targets (Debug)...
cmake --build %BUILD_DIR% --config Debug
if %ERRORLEVEL% neq 0 (
    echo.
    echo ========================================================
    echo [ERROR] CMake Build failed!
    echo Please check the error messages above.
    echo Common reasons:
    echo 1. Missing header files or syntax errors in source code.
    echo 2. Missing third-party tools ^(like PowerShell Core "pwsh.exe" if used by submodules^).
    echo ========================================================
    pause
    exit /b %ERRORLEVEL%
)

:: 4. Verify Execution
echo.
echo [4/4] Running Verification Tests...

echo -- Running C++ Example --
.\bin\DSEngine_example_cpp.exe
if %ERRORLEVEL% neq 0 (
    echo [ERROR] C++ Example failed with exit code %ERRORLEVEL%!
    pause
    exit /b %ERRORLEVEL%
)

echo -- Running Lua Example --
.\bin\DSEngine_lua.exe
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Lua Example failed with exit code %ERRORLEVEL%!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================================
echo        [SUCCESS] All builds and verifications passed!
echo ========================================================
pause
exit /b 0
