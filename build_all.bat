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
set VERIFY_EXE_TIMEOUT_SECONDS=3
set "DSE_ELECTRON_MIRROR="
set "DSE_ELECTRON_CUSTOM_DIR="
set "DSE_ELECTRON_CACHE_DIR=%CD%\.cache\electron"
set "DSE_ELECTRON_BUILDER_CACHE_DIR=%CD%\.cache\electron-builder"
set "DSE_ELECTRON_BUILDER_BINARIES_MIRROR=https://npmmirror.com/mirrors/electron-builder-binaries/"
set "DSE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR="

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
if /I "%~1"=="--electron-mirror" (
    if "%~2"=="" (
        echo [ERROR] --electron-mirror requires a URL value.
        goto usage_error
    )
    set "DSE_ELECTRON_MIRROR=%~2"
    shift
    shift
    goto parse_args
)
if /I "%~1"=="--electron-custom-dir" (
    if "%~2"=="" (
        echo [ERROR] --electron-custom-dir requires a directory name value.
        goto usage_error
    )
    set "DSE_ELECTRON_CUSTOM_DIR=%~2"
    shift
    shift
    goto parse_args
)
if /I "%~1"=="--electron-cache-dir" (
    if "%~2"=="" (
        echo [ERROR] --electron-cache-dir requires a directory path value.
        goto usage_error
    )
    set "DSE_ELECTRON_CACHE_DIR=%~2"
    shift
    shift
    goto parse_args
)
if /I "%~1"=="--electron-builder-cache-dir" (
    if "%~2"=="" (
        echo [ERROR] --electron-builder-cache-dir requires a directory path value.
        goto usage_error
    )
    set "DSE_ELECTRON_BUILDER_CACHE_DIR=%~2"
    shift
    shift
    goto parse_args
)
if /I "%~1"=="--electron-builder-binaries-mirror" (
    if "%~2"=="" (
        echo [ERROR] --electron-builder-binaries-mirror requires a URL value.
        goto usage_error
    )
    set "DSE_ELECTRON_BUILDER_BINARIES_MIRROR=%~2"
    shift
    shift
    goto parse_args
)
if /I "%~1"=="--electron-builder-binaries-custom-dir" (
    if "%~2"=="" (
        echo [ERROR] --electron-builder-binaries-custom-dir requires a directory name value.
        goto usage_error
    )
    set "DSE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR=%~2"
    shift
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
set "CMAKE_ELECTRON_MIRROR_OPTION="
if defined DSE_ELECTRON_MIRROR set "CMAKE_ELECTRON_MIRROR_OPTION=-DDSE_ELECTRON_MIRROR=%DSE_ELECTRON_MIRROR%"
set "CMAKE_ELECTRON_CUSTOM_DIR_OPTION="
if defined DSE_ELECTRON_CUSTOM_DIR set "CMAKE_ELECTRON_CUSTOM_DIR_OPTION=-DDSE_ELECTRON_CUSTOM_DIR=%DSE_ELECTRON_CUSTOM_DIR%"
set "CMAKE_ELECTRON_BUILDER_BINARIES_MIRROR_OPTION="
if defined DSE_ELECTRON_BUILDER_BINARIES_MIRROR set "CMAKE_ELECTRON_BUILDER_BINARIES_MIRROR_OPTION=-DDSE_ELECTRON_BUILDER_BINARIES_MIRROR=%DSE_ELECTRON_BUILDER_BINARIES_MIRROR%"
set "CMAKE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR_OPTION="
if defined DSE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR set "CMAKE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR_OPTION=-DDSE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR=%DSE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR%"

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
    set "PYTHON_EXE="
    for /f "delims=" %%I in ('where python') do if not defined PYTHON_EXE set "PYTHON_EXE=%%~fI"
    if defined PYTHON_EXE set "npm_config_python=!PYTHON_EXE!"
    call :setup_editor_native_build_env
    if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!
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
if not exist "!DSE_ELECTRON_CACHE_DIR!" mkdir "!DSE_ELECTRON_CACHE_DIR!" >nul 2>&1
echo write_test > "!DSE_ELECTRON_CACHE_DIR!\_dse_write_test.tmp" 2>nul
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Electron cache directory is not writable: !DSE_ELECTRON_CACHE_DIR!
    pause
    exit /b 1
)
del "!DSE_ELECTRON_CACHE_DIR!\_dse_write_test.tmp" >nul 2>&1
if not exist "!DSE_ELECTRON_BUILDER_CACHE_DIR!" mkdir "!DSE_ELECTRON_BUILDER_CACHE_DIR!" >nul 2>&1
echo write_test > "!DSE_ELECTRON_BUILDER_CACHE_DIR!\_dse_write_test.tmp" 2>nul
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Electron-builder cache directory is not writable: !DSE_ELECTRON_BUILDER_CACHE_DIR!
    pause
    exit /b 1
)
del "!DSE_ELECTRON_BUILDER_CACHE_DIR!\_dse_write_test.tmp" >nul 2>&1

:: 1. Checking and cleaning old build cache
echo [1/4] Checking and cleaning old build cache...
if exist %BUILD_DIR%\CMakeCache.txt (
    echo Found existing CMakeCache.txt, removing it...
    del /f /q %BUILD_DIR%\CMakeCache.txt
)

:: 2. Configure CMake project
echo.
echo [2/4] Configuring CMake project...
cmake -S . -B %BUILD_DIR% -G %GENERATOR% -A %ARCH% %CMAKE_EDITOR_OPTION% %CMAKE_LAUNCHER_OPTION% "-DDSE_ELECTRON_CACHE_DIR=%DSE_ELECTRON_CACHE_DIR%" "-DDSE_ELECTRON_BUILDER_CACHE_DIR=%DSE_ELECTRON_BUILDER_CACHE_DIR%" %CMAKE_ELECTRON_MIRROR_OPTION% %CMAKE_ELECTRON_CUSTOM_DIR_OPTION% %CMAKE_ELECTRON_BUILDER_BINARIES_MIRROR_OPTION% %CMAKE_ELECTRON_BUILDER_BINARIES_CUSTOM_DIR_OPTION%
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
    if "%BUILD_EDITOR%"=="1" (
        echo.
        echo [HINT] If the failure is from dse_editor / node-gyp / dsengine_bridge:
        echo   - Check Node.js version and local node-gyp version in apps\editor
        echo   - Check Python path: npm_config_python or python in PATH
        echo   - Check Visual Studio 2022 is installed with "Desktop development with C++"
        echo   - Check VsDevCmd.bat exists under Visual Studio 2022\Common7\Tools
        echo   - Re-run: cmake --build %BUILD_DIR% --config Debug --target dse_editor -- /v:minimal
    )
    echo ========================================================
    pause
    exit /b %ERRORLEVEL%
)

:: Editor / Launcher 资源拷贝已由 CMake 目标负责打包到 .\bin 目录
if "%PACKAGE_EDITOR_EXE%"=="1" (
    if "%BUILD_EDITOR%"=="1" (
        echo.
        echo [*] Packaging Editor EXE...
        cmake --build %BUILD_DIR% --config Debug --target dse_editor_exe
        set "PACK_EDITOR_RC=!ERRORLEVEL!"
        if not "!PACK_EDITOR_RC!"=="0" (
            echo.
            echo ========================================================
            echo [ERROR] Editor EXE packaging failed ^(target: dse_editor_exe^).
            echo [ROOT CAUSE] Check electron-builder / electron-rebuild / node-gyp logs above.
            echo [HINT] Most common root causes on Windows:
            echo   1. Visual Studio 2022 missing "Desktop development with C++".
            echo   2. node-gyp cannot find VS toolchain or Python.
            echo   3. Environment variables for npm/node-gyp are not initialized correctly.
            echo [RETRY] cmake --build %BUILD_DIR% --config Debug --target dse_editor_exe -- /v:minimal
            echo ========================================================
            pause
            exit /b !PACK_EDITOR_RC!
        )
    ) else (
        echo [WARN] Skip dse_editor_exe because BUILD_EDITOR is OFF.
    )
)
if "%PACKAGE_LAUNCHER_EXE%"=="1" (
    if "%BUILD_LAUNCHER%"=="1" (
        echo.
        echo [*] Packaging Launcher EXE...
        cmake --build %BUILD_DIR% --config Debug --target dse_launcher_exe
        set "PACK_LAUNCHER_RC=!ERRORLEVEL!"
        if not "!PACK_LAUNCHER_RC!"=="0" (
            echo.
            echo ========================================================
            echo [ERROR] Launcher EXE packaging failed ^(target: dse_launcher_exe^).
            echo [ROOT CAUSE] Check electron-builder download/packaging logs above.
            echo [HINT] Most common root causes on Windows:
            echo   1. Network timeout when downloading Electron from GitHub.
            echo   2. Proxy/firewall blocks electron release download.
            echo   3. Temporary network instability during app-builder download.
            echo [RETRY] cmake --build %BUILD_DIR% --config Debug --target dse_launcher_exe -- /v:minimal
            echo ========================================================
            pause
            exit /b !PACK_LAUNCHER_RC!
        )
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
if not exist "%CPP_EXE%" (
    echo [ERROR] Cannot find C++ example executable in .\bin
    pause
    exit /b 1
)

set LUA_EXE=.\bin\DSEngine_lua_debug.exe
if not exist "%LUA_EXE%" set LUA_EXE=.\bin\DSEngine_lua.exe
if not exist "%LUA_EXE%" (
    echo [ERROR] Cannot find Lua example executable in .\bin
    pause
    exit /b 1
)

call :run_verify_exe "%CPP_EXE%" "C++ Example"
if !ERRORLEVEL! neq 0 (
    set "VERIFY_RC=!ERRORLEVEL!"
    echo [ERROR] C++ Example failed with exit code !VERIFY_RC!!
    pause
    exit /b !VERIFY_RC!
)

call :run_verify_exe "%LUA_EXE%" "Lua Example"
if !ERRORLEVEL! neq 0 (
    set "VERIFY_RC=!ERRORLEVEL!"
    echo [ERROR] Lua Example failed with exit code !VERIFY_RC!!
    pause
    exit /b !VERIFY_RC!
)

if "%BUILD_EDITOR%"=="1" (
    echo -- Running Editor Smoke Regression --
    pushd ".\apps\editor" >nul
    call npm run regress:editor-smoke
    set "EDITOR_SMOKE_RC=!ERRORLEVEL!"
    popd >nul
    if not "!EDITOR_SMOKE_RC!"=="0" (
        echo [ERROR] Editor smoke regression failed with exit code !EDITOR_SMOKE_RC!!
        pause
        exit /b !EDITOR_SMOKE_RC!
    )
)

echo.
echo ========================================================
echo        [SUCCESS] All builds and verifications passed!
echo ========================================================
pause
exit /b 0

:run_verify_exe
setlocal
set "VERIFY_EXE_PATH=%~1"
set "VERIFY_EXE_NAME=%~2"
echo -- Running %VERIFY_EXE_NAME% --
powershell -NoProfile -Command "$p = Start-Process -FilePath \"%VERIFY_EXE_PATH%\" -PassThru; if ($p.WaitForExit(%VERIFY_EXE_TIMEOUT_SECONDS%000)) { exit $p.ExitCode }; Stop-Process -Id $p.Id -Force; exit 124"
set "RUN_RC=%ERRORLEVEL%"
if "%RUN_RC%"=="124" (
    echo [WARN] %VERIFY_EXE_NAME% did not exit within %VERIFY_EXE_TIMEOUT_SECONDS%s, terminated automatically.
    endlocal & exit /b 0
)
endlocal & exit /b %RUN_RC%

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
echo   --electron-mirror URL  Electron download mirror URL for electron-builder
echo   --electron-custom-dir NAME  Optional custom dir name under mirror for Electron zip
echo   --electron-cache-dir PATH  Electron zip cache directory
echo   --electron-builder-cache-dir PATH  electron-builder cache directory
echo   --electron-builder-binaries-mirror URL  Mirror URL for electron-builder binaries ^(default: npmmirror^)
echo   --electron-builder-binaries-custom-dir NAME  Optional custom dir name for electron-builder binaries mirror
echo   -h, --help             Show this help message
echo.
echo Notes:
echo   1) No arguments: full build + editor/launcher exe packaging + SDK installation.
echo   2) Multiple options can be combined in one command.
echo   3) Offline mode: pre-place electron-vXX.Y.Z-win32-x64.zip in --electron-cache-dir.
exit /b 0

:setup_editor_native_build_env
set "VSWHERE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_INSTALL_DIR="
set "VS_DEV_CMD="
if exist "%VSWHERE_EXE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE_EXE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_INSTALL_DIR=%%I"
)
if not defined VS_INSTALL_DIR if defined VSINSTALLDIR set "VS_INSTALL_DIR=%VSINSTALLDIR%"
if defined VS_INSTALL_DIR (
    set "VS_DEV_CMD=!VS_INSTALL_DIR!\Common7\Tools\VsDevCmd.bat"
    if not exist "!VS_DEV_CMD!" set "VS_DEV_CMD="
)
if not defined VS_DEV_CMD (
    echo [ERROR] Visual Studio 2022 with "Desktop development with C++" was not found.
    echo [HINT] Editor native module ^(node-gyp / dsengine_bridge^) requires VS2022 tools.
    pause
    exit /b 1
)
call "!VS_DEV_CMD!" -arch=%ARCH% -host_arch=%ARCH% >nul
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Failed to initialize Visual Studio 2022 developer environment.
    echo [HINT] Check VsDevCmd.bat and the installed VC++ workload.
    pause
    exit /b !ERRORLEVEL!
)
set "npm_config_msvs_version=2022"
set "GYP_MSVS_VERSION=2022"
echo [OK] Editor native build environment ready.
exit /b 0

:usage_error
echo Usage: build_all.bat [options...]
echo Use --help to list all options.
exit /b 1
