@echo off
setlocal enabledelayedexpansion

:: Change working directory to the script's location
pushd "%~dp0"

echo ========================================================
echo         DSEngine Full Build and Verification Script
echo ========================================================
echo.

set BUILD_DIR=build_vs2022
set GENERATOR="Visual Studio 17 2022"
set ARCH=x64
set BUILD_EDITOR=1
set BUILD_LAUNCHER=1
set PACKAGE_EDITOR_EXE=1
set PACKAGE_LAUNCHER_EXE=1
set PACKAGE_SDK=1

:parse_args
if "%~1"=="" goto parse_done
if /I "%~1"=="editor" (
    set BUILD_EDITOR=1
    shift
    goto parse_args
)
if /I "%~1"=="launcher" (
    set BUILD_LAUNCHER=1
    shift
    goto parse_args
)
if /I "%~1"=="--with-editor" (
    set BUILD_EDITOR=1
    shift
    goto parse_args
)
if /I "%~1"=="--no-editor" (
    set BUILD_EDITOR=0
    shift
    goto parse_args
)
if /I "%~1"=="--with-launcher" (
    set BUILD_LAUNCHER=1
    shift
    goto parse_args
)
if /I "%~1"=="--no-launcher" (
    set BUILD_LAUNCHER=0
    shift
    goto parse_args
)
if /I "%~1"=="--package-editor-exe" (
    set PACKAGE_EDITOR_EXE=1
    shift
    goto parse_args
)
if /I "%~1"=="--package-launcher-exe" (
    set PACKAGE_LAUNCHER_EXE=1
    shift
    goto parse_args
)
if /I "%~1"=="--package-sdk" (
    set PACKAGE_SDK=1
    shift
    goto parse_args
)
if /I "%~1"=="--no-sdk" (
    set PACKAGE_SDK=0
    shift
    goto parse_args
)
if /I "%~1"=="--all" (
    set BUILD_EDITOR=1
    set BUILD_LAUNCHER=1
    shift
    goto parse_args
)
if /I "%~1"=="-h" goto usage
if /I "%~1"=="--help" goto usage
echo [ERROR] Unknown option: %~1
goto usage_error

:parse_done
set CMAKE_EDITOR_OPTION=-DDSE_BUILD_EDITOR=OFF
if "%BUILD_EDITOR%"=="1" set CMAKE_EDITOR_OPTION=-DDSE_BUILD_EDITOR=ON
set CMAKE_LAUNCHER_OPTION=-DDSE_BUILD_LAUNCHER=OFF
if "%BUILD_LAUNCHER%"=="1" set CMAKE_LAUNCHER_OPTION=-DDSE_BUILD_LAUNCHER=ON

:: Check Administrator Privileges if we need to package EXE
if "%PACKAGE_EDITOR_EXE%"=="1" set NEED_ADMIN=1
if "%PACKAGE_LAUNCHER_EXE%"=="1" set NEED_ADMIN=1

if "%NEED_ADMIN%"=="1" (
    net session >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        powershell -NoProfile -Command "Write-Host '[WARN] You are not running as Administrator.' -ForegroundColor Yellow"
        powershell -NoProfile -Command "Write-Host '[WARN] Electron-builder may fail to extract macOS dependencies (symlink error) when packaging the EXE.' -ForegroundColor Yellow"
        powershell -NoProfile -Command "Write-Host '[WARN] If the build fails with \"Cannot create symbolic link\", please run this script as Administrator.' -ForegroundColor Yellow"
        echo.
        :: Give user 3 seconds to read the warning before continuing
        timeout /t 3 /nobreak >nul
    )
)

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

if "%BUILD_EDITOR%"=="1" (
    echo Checking editor build environment...
)
if "%BUILD_LAUNCHER%"=="1" (
    echo Checking launcher build environment...
)
if "%BUILD_EDITOR%"=="1" (
    where node >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Node.js is not installed or not in PATH!
        echo Please install Node.js 16+ from https://nodejs.org/
        pause
        exit /b 1
    )
    where npm >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] npm is not available in PATH!
        echo Please reinstall Node.js to include npm.
        pause
        exit /b 1
    )
    where npx >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] npx is not available in PATH!
        echo Please reinstall Node.js to include npx.
        pause
        exit /b 1
    )
    where python >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Python is not installed or not in PATH!
        echo Please install Python 3 and ensure python is in PATH.
        pause
        exit /b 1
    )
)
if "%BUILD_LAUNCHER%"=="1" (
    where node >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Node.js is not installed or not in PATH!
        echo Please install Node.js 16+ from https://nodejs.org/
        pause
        exit /b 1
    )
    where npm >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] npm is not available in PATH!
        echo Please reinstall Node.js to include npm.
        pause
        exit /b 1
    )
)
set NPM_CACHE=%CD%\.npm-cache
if not exist "!NPM_CACHE!" mkdir "!NPM_CACHE!" >nul 2>&1
echo write_test > "!NPM_CACHE!\_dse_write_test.tmp" 2>nul
if !ERRORLEVEL! neq 0 (
    echo [ERROR] npm cache directory is not writable: !NPM_CACHE!
    echo Please ensure the directory is writable or run the script in a writable workspace.
    pause
    exit /b 1
)
del "!NPM_CACHE!\_dse_write_test.tmp" >nul 2>&1
set NPM_CONFIG_CACHE=!NPM_CACHE!

:: 1. Checking and cleaning old build cache
echo [1/4] Checking and cleaning old build cache...
if exist %BUILD_DIR%\CMakeCache.txt (
    echo Found existing CMakeCache.txt, removing it...
    del /f /q %BUILD_DIR%\CMakeCache.txt
)

:: 2. Configure CMake project
echo.
echo [2/4] Configuring CMake project...
cmake -S . -B %BUILD_DIR% -G %GENERATOR% -A %ARCH% %CMAKE_EDITOR_OPTION% %CMAKE_LAUNCHER_OPTION%
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
    echo 2. Missing third-party tools required by optional submodules.
    echo ========================================================
    pause
    exit /b %ERRORLEVEL%
)

if "%BUILD_EDITOR%"=="1" (
    cmake -E make_directory .\bin\editor
    cmake -E copy_if_different .\editor\main.js .\editor\preload.js .\editor\index.html .\editor\package.json .\bin\editor
    if exist .\editor\dist cmake -E copy_directory .\editor\dist .\bin\editor\dist
    if exist .\editor\scripts cmake -E copy_directory .\editor\scripts .\bin\editor\scripts
)
if "%BUILD_LAUNCHER%"=="1" (
    cmake -E make_directory .\bin\launcher
    cmake -E copy_if_different .\launcher\main.js .\launcher\preload.js .\launcher\index.html .\launcher\package.json .\bin\launcher
    if exist .\launcher\dist cmake -E copy_directory .\launcher\dist .\bin\launcher\dist
)
if "%PACKAGE_EDITOR_EXE%"=="1" (
    if "%BUILD_EDITOR%"=="1" (
        cmake --build %BUILD_DIR% --config Debug --target dse_editor_exe
    ) else (
        echo [WARN] Skip dse_editor_exe because BUILD_EDITOR is OFF.
    )
)
if "%PACKAGE_LAUNCHER_EXE%"=="1" (
    if "%BUILD_LAUNCHER%"=="1" (
        cmake --build %BUILD_DIR% --config Debug --target dse_launcher_exe
    ) else (
        echo [WARN] Skip dse_launcher_exe because BUILD_LAUNCHER is OFF.
    )
)

:: 4. Install SDK
echo.
echo [*] Installing C++ SDK...
if "%PACKAGE_SDK%"=="1" (
    cmake --install %BUILD_DIR% --config Debug --prefix ".\bin\sdk"
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] SDK Installation failed!
        pause
        exit /b !ERRORLEVEL!
    )
    echo [OK] SDK installed to .\bin\sdk
) else (
    echo [WARN] Skip SDK installation because PACKAGE_SDK is OFF.
)

:: 5. Verify Execution
echo.
echo [*] Running Verification Tests...

set CPP_EXE=.\bin\DSEngine_c++_debug.exe
if not exist "%CPP_EXE%" set CPP_EXE=.\bin\DSEngine_example_cpp.exe

set LUA_EXE=.\bin\DSEngine_lua_debug.exe
if not exist "%LUA_EXE%" set LUA_EXE=.\bin\DSEngine_lua.exe

echo -- Running C++ Example --
"%CPP_EXE%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] C++ Example failed with exit code %ERRORLEVEL%!
    pause
    exit /b %ERRORLEVEL%
)

echo -- Running Lua Example --
"%LUA_EXE%"
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

:usage
echo Usage: build_all.bat [options...]
echo Options:
echo   --with-editor          Enable editor build (default: ON)
echo   --no-editor            Disable editor build
echo   --with-launcher        Enable launcher build (default: ON)
echo   --no-launcher          Disable launcher build
echo   --package-editor-exe   Package editor executable (default: ON)
echo   --package-launcher-exe Package launcher executable (default: ON)
echo   --package-sdk          Install and package C++ SDK (default: ON)
echo   --no-sdk               Disable SDK packaging
echo   --all                  Enable editor and launcher build together
echo   -h, --help             Show this help message
echo.
echo Notes:
echo   1) No arguments: full build + editor/launcher exe packaging + SDK installation.
echo   2) Multiple options can be combined in one command.
exit /b 0

:usage_error
echo Usage: build_all.bat [options...]
echo Use --help to list all options.
exit /b 1
