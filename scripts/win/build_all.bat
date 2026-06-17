@echo off
setlocal enabledelayedexpansion

:: Change working directory to the script's location
pushd "%~dp0..\.."

echo ========================================================
echo         DSEngine Full Build and Verification Script
echo ========================================================
echo.

:: 记录开始时间
set "START_TIME=%TIME%"
set "START_H=%START_TIME:~0,2%"
set "START_M=%START_TIME:~3,2%"
set "START_S=%START_TIME:~6,2%"
set "START_MS=%START_TIME:~9,2%"
:: 处理小时的前导空格
if "%START_H:~0,1%"==" " set "START_H=0%START_H:~1,1%"
:: 将时间转换为厘秒（1/100秒）以便于计算
set /a START_TOTAL_CS=1%START_H%*360000 + 1%START_M%*6000 + 1%START_S%*100 + 1%START_MS% - 36610100

set BUILD_DIR=build_vs2022
set GENERATOR="Visual Studio 17 2022"
set ARCH=x64
set BUILD_CONFIG=Debug
set BUILD_EDITOR=0
set BUILD_LAUNCHER=0
set BUILD_ENGINE_TESTS=1
set PACKAGE_EDITOR_EXE=0
set PACKAGE_LAUNCHER_EXE=0
set PACKAGE_SDK=1
set VERIFY_EXECUTABLES=0
set VERIFY_EXE_TIMEOUT_SECONDS=3
set CLEAN_BUILD=0

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
if /I "%~1"=="--with-tests" (
    set BUILD_GTESTS=1
    shift
    goto parse_args
)
if /I "%~1"=="--no-tests" (
    set BUILD_GTESTS=0
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
if /I "%~1"=="--with-verify-exe" (
    set VERIFY_EXECUTABLES=1
    shift
    goto parse_args
)
if /I "%~1"=="--no-verify-exe" (
    set VERIFY_EXECUTABLES=0
    shift
    goto parse_args
)
if /I "%~1"=="--all" (
    set BUILD_EDITOR=1
    set BUILD_LAUNCHER=1
    shift
    goto parse_args
)
if /I "%~1"=="--clean" (
    set CLEAN_BUILD=1
    shift
    goto parse_args
)
if /I "%~1"=="--release" (
    set BUILD_CONFIG=Release
    shift
    goto parse_args
)
if /I "%~1"=="--relwithdebinfo" (
    set BUILD_CONFIG=RelWithDebInfo
    shift
    goto parse_args
)
if /I "%~1"=="-h" goto usage
if /I "%~1"=="--help" goto usage
echo [ERROR] Unknown option: %~1
echo Usage: build_all.bat [options...]
echo Use --help to list all options.
popd
pause
exit /b 1

:parse_done
set CMAKE_EDITOR_OPTION=-DDSE_BUILD_EDITOR=OFF
if "%BUILD_EDITOR%"=="1" set CMAKE_EDITOR_OPTION=-DDSE_BUILD_EDITOR=ON
set CMAKE_LAUNCHER_OPTION=-DDSE_BUILD_LAUNCHER=OFF
if "%BUILD_LAUNCHER%"=="1" set CMAKE_LAUNCHER_OPTION=-DDSE_BUILD_LAUNCHER=ON
set CMAKE_GTEST_OPTION=-DDSE_BUILD_GTESTS=OFF
if "%BUILD_GTESTS%"=="1" set CMAKE_GTEST_OPTION=-DDSE_BUILD_GTESTS=ON
set CMAKE_SPINE_OPTION=-DDSE_ENABLE_SPINE=ON
set CMAKE_BOX2D_LINKAGE_OPTION=-DBUILD_SHARED_LIBS=OFF
set CMAKE_MSVC_RUNTIME_OPTION="-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>"


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

set "VERIFY_EDITOR_EXE=0"
if "%BUILD_EDITOR%"=="1" if "%VERIFY_EXECUTABLES%"=="1" set "VERIFY_EDITOR_EXE=1"

:: 0. Environment Check
echo [0/5] Checking build environment...

:: Check CMake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH!
    echo Please install CMake from https://cmake.org/download/
    popd
    pause
    exit /b 1
)

:: Check CMake execution
cmake --version | findstr /C:"cmake version" >nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake execution failed!
    popd
    pause
    exit /b 1
)
echo [OK] CMake found.
if "%BUILD_EDITOR%"=="1" (


    where python >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Python is not installed or not in PATH.
        echo Please install Python 3 and ensure python is in PATH.
        popd
        pause
        exit /b 1
    )
    set "PYTHON_EXE="
    for /f "delims=" %%I in ('where python') do if not defined PYTHON_EXE set "PYTHON_EXE=%%~fI"
    if defined PYTHON_EXE set "npm_config_python=!PYTHON_EXE!"
    call :setup_editor_native_build_env
    if !ERRORLEVEL! neq 0 (
        set "SETUP_RC=!ERRORLEVEL!"
        popd
        pause
        exit /b !SETUP_RC!
    )
)
if "%BUILD_LAUNCHER%"=="1" (
    where node >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Node.js is not installed or not in PATH!
        echo Please install Node.js 16+ from https://nodejs.org/
        popd
        pause
        exit /b 1
    )
    where npm >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] npm is not available in PATH!
        echo Please reinstall Node.js to include npm.
        popd
        pause
        exit /b 1
    )
    :: 检查 Rust 与 Cargo 是否存在
    where cargo >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        if exist "%USERPROFILE%\.cargo\bin\cargo.exe" (
            set "PATH=!USERPROFILE!\.cargo\bin;!USERPROFILE!\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin;!PATH!"
            echo [OK] Found Cargo in USERPROFILE, added to PATH temporarily.
        ) else (
            echo [WARN] Rust / Cargo is not installed or not in PATH!
            echo [*] Downloading and installing Rust toolchain automatically...
            powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://win.rustup.rs/x86_64' -OutFile 'rustup-init.exe'"
            if exist "rustup-init.exe" (
                .\rustup-init.exe -y --default-toolchain stable --profile minimal
                del /f /q "rustup-init.exe"
                set "PATH=!USERPROFILE!\.cargo\bin;!USERPROFILE!\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin;!PATH!"
                echo [OK] Rust toolchain installed successfully.
            ) else (
                echo [ERROR] Failed to download rustup-init.exe. Please install Rust manually from https://rustup.rs/
                popd
                pause
                exit /b 1
            )
        )
    )
)
set NPM_CACHE=%CD%\.npm-cache
if not exist "!NPM_CACHE!" mkdir "!NPM_CACHE!" >nul 2>&1
echo write_test > "!NPM_CACHE!\_dse_write_test.tmp" 2>nul
if !ERRORLEVEL! neq 0 (
    echo [ERROR] npm cache directory is not writable: !NPM_CACHE!
    echo Please ensure the directory is writable or run the script in a writable workspace.
    popd
    pause
    exit /b 1
)
del "!NPM_CACHE!\_dse_write_test.tmp" >nul 2>&1
set NPM_CONFIG_CACHE=!NPM_CACHE!

:: 1. Checking build cache
echo [1/5] Checking build cache...
if "%CLEAN_BUILD%"=="1" (
    if exist %BUILD_DIR%\CMakeCache.txt (
        echo [INFO] --clean: removing CMakeCache.txt for fresh configure...
        del /f /q %BUILD_DIR%\CMakeCache.txt
    )
) else (
    if exist %BUILD_DIR%\CMakeCache.txt (
        echo [INFO] Reusing existing CMakeCache.txt, use --clean to force reconfigure.
    )
)

:: 2. Configure CMake project
echo.
echo [2/5] Configuring CMake project...
cmake -Wno-dev -Wno-deprecated -S . -B %BUILD_DIR% -G %GENERATOR% -A %ARCH% %CMAKE_EDITOR_OPTION% %CMAKE_LAUNCHER_OPTION% %CMAKE_ENGINE_TEST_OPTION% %CMAKE_SPINE_OPTION% %CMAKE_BOX2D_LINKAGE_OPTION% %CMAKE_MSVC_RUNTIME_OPTION%

if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake Configure failed!
    popd
    pause
    exit /b %ERRORLEVEL%
)

:: 3. Compile shaders → regenerate gen.h embed headers
echo.
echo [3/5] Compiling shaders (dse_shader_compiler)...
cmake --build %BUILD_DIR% --config %BUILD_CONFIG% --target dse_shader_compiler
if %ERRORLEVEL% neq 0 (
    echo [ERROR] dse_shader_compiler build failed!
    popd
    pause
    exit /b %ERRORLEVEL%
)
if exist ".\bin\dse_shader_compiler.exe" (
    .\bin\dse_shader_compiler.exe --input-dir engine\render\shaders\src --output-dir engine\render\shaders\generated --target all --embed
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Shader compilation failed! Check shader source errors above.
        popd
        pause
        exit /b !ERRORLEVEL!
    )
    echo [OK] Shaders regenerated.
    if exist ".\bin\data\shader_cache" (
        rd /s /q ".\bin\data\shader_cache"
        echo [OK] Runtime shader cache cleared.
    )
    if exist ".\data\shader_cache" (
        rd /s /q ".\data\shader_cache"
        echo [OK] Root shader cache cleared.
    )
    if exist ".\examples\KF_Framework\data\shader_cache" (
        rd /s /q ".\examples\KF_Framework\data\shader_cache"
        echo [OK] KF shader cache cleared.
    )
) else (
    echo [WARN] dse_shader_compiler.exe not found in .\bin, skipping shader regeneration.
)

:: 4. Build all targets
echo.
echo [4/5] Building all targets (%BUILD_CONFIG%)...
cmake --build %BUILD_DIR% --config %BUILD_CONFIG% --parallel
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
        echo [HINT] Check if Visual Studio 2022 is installed with "Desktop development with C++"
        echo   - Re-run: cmake --build %BUILD_DIR% --config Debug --target dse_editor_cpp
    )
    echo ========================================================
    popd
    pause
    exit /b %ERRORLEVEL%
)

if "%PACKAGE_LAUNCHER_EXE%"=="1" (
    if "%BUILD_LAUNCHER%"=="1" (
        echo.
        echo [*] Packaging Tauri Launcher EXE...
        pushd apps\launcher_tauri
        
        :: 确保 NPM 依赖已安装
        if not exist "node_modules" (
            echo [*] Installing NPM dependencies for Launcher...
            call npm install
            if not "!ERRORLEVEL!"=="0" (
                echo [ERROR] Failed to install NPM dependencies for Launcher.
                popd
                popd
                pause
                exit /b 1
            )
        )

        :: 确保 Rust / Cargo 在 PATH 中（兼容刚才修复的本地环境）
        set "PATH=!USERPROFILE!\.cargo\bin;!USERPROFILE!\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin;!PATH!"
        
        call npm run tauri build
        set "PACK_LAUNCHER_RC=!ERRORLEVEL!"
        popd
        if not "!PACK_LAUNCHER_RC!"=="0" (
            echo.
            echo ========================================================
            echo [ERROR] Tauri Launcher EXE packaging failed.
            echo [ROOT CAUSE] Check tauri build logs above.
            echo [HINT] Ensure Rust toolchain and webview2 dependencies are installed.
            echo ========================================================
            popd
            pause
            exit /b !PACK_LAUNCHER_RC!
        )
        :: Copy the built executable to bin directory
        if not exist ".\bin" mkdir ".\bin" >nul 2>&1
        copy /Y "apps\launcher_tauri\src-tauri\target\release\dsengine-launcher.exe" ".\bin\" >nul
        echo [OK] Tauri Launcher EXE copied to bin directory.
    ) else (
        echo [WARN] Skip launcher build because BUILD_LAUNCHER is OFF.
    )
)

:: 4. Install SDK
echo.
echo [*] Installing C++ SDK...
if "%PACKAGE_SDK%"=="1" (
    cmake --install %BUILD_DIR% --config %BUILD_CONFIG% --prefix ".\bin\sdk"
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] SDK Installation failed!
        popd
        pause
        exit /b !ERRORLEVEL!
    )
    echo [OK] SDK installed to .\bin\sdk
) else (
    echo [WARN] Skip SDK installation because PACKAGE_SDK is OFF.
)

:: 5. Verify Execution
echo.
if "%VERIFY_EXECUTABLES%"=="1" (
    echo [*] Running Verification Tests...

    if "%VERIFY_EDITOR_EXE%"=="1" (
        set "EDITOR_EXE=.\bin\dsengine-editor.exe"
        if not exist "!EDITOR_EXE!" (
            echo [ERROR] Cannot find editor executable in .\bin
            popd
            pause
            exit /b 1
        )

        call :run_verify_exe "!EDITOR_EXE!" "Editor"
        if !ERRORLEVEL! neq 0 (
            set "VERIFY_RC=!ERRORLEVEL!"
            echo [ERROR] Editor failed with exit code !VERIFY_RC!!
            echo [WARN] Auto verification is only checking process startup within %VERIFY_EXE_TIMEOUT_SECONDS%s.
            echo [WARN] The editor may require a graphics context, assets, or manual interaction in some environments.
            echo [WARN] Use --no-verify-exe if you only want to complete the build/package stage.
            popd
            pause
            exit /b !VERIFY_RC!
        )
    )

    call :resolve_first_existing_exe CPP_EXE ".\bin\dsengine_cpp_debug.exe" ".\bin\dsengine_cpp.exe"
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Cannot find C++ example executable in .\bin
        popd
        pause
        exit /b 1
    )

    call :resolve_first_existing_exe LUA_EXE ".\bin\dsengine_lua_debug.exe" ".\bin\dsengine_lua.exe"
    if !ERRORLEVEL! neq 0 (
        echo [ERROR] Cannot find Lua example executable in .\bin
        popd
        pause
        exit /b 1
    )

    call :run_verify_exe "!CPP_EXE!" "C++ Example"
    if !ERRORLEVEL! neq 0 (
        set "VERIFY_RC=!ERRORLEVEL!"
        echo [ERROR] C++ Example failed with exit code !VERIFY_RC!!
        popd
        pause
        exit /b !VERIFY_RC!
    )

    if exist ".\bin\lua.dll" (
        call :run_verify_exe "!LUA_EXE!" "Lua Example"
        if !ERRORLEVEL! neq 0 (
            set "VERIFY_RC=!ERRORLEVEL!"
            echo [ERROR] Lua Example failed with exit code !VERIFY_RC!!
            popd
            pause
            exit /b !VERIFY_RC!
        )
    ) else (
        echo [WARN] Skip Lua Example verification because lua.dll is not present in .\bin.
    )
) else (
    echo [WARN] Skip executable verification because VERIFY_EXECUTABLES is OFF.
)

if "%BUILD_ENGINE_TESTS%"=="1" (
    echo.
    echo [*] Running CTest ^(-L engine; full engine-labeled suite^) ...
    if not exist "%BUILD_DIR%\Testing\Temporary" mkdir "%BUILD_DIR%\Testing\Temporary"
    set "ENGINE_TEST_LOG=%BUILD_DIR%\Testing\Temporary\engine_ctest_output.log"
    ctest --test-dir %BUILD_DIR% -C %BUILD_CONFIG% --output-on-failure --verbose -L engine -E "^engine\.lua_runtime$|^engine\.lua_runtime\.smoke$|^engine\.resource_injection$" > "!ENGINE_TEST_LOG!" 2>&1
    set "ENGINE_TEST_RC=!ERRORLEVEL!"
    type "!ENGINE_TEST_LOG!"
    powershell -NoProfile -Command "$logPath = '!ENGINE_TEST_LOG!'; $rc = [int]'!ENGINE_TEST_RC!'; $total = 0; $passed = 0; $failed = 0; $found = $false; if (Test-Path $logPath) { $raw = Get-Content -Raw -Encoding UTF8 $logPath; $summaryMatches = [regex]::Matches($raw, '(?im)^\s*test cases:\s*(\d+)\s*\|\s*(\d+)\s*passed\s*\|\s*(\d+)\s*failed\s*$'); if ($summaryMatches.Count -gt 0) { $last = $summaryMatches[$summaryMatches.Count - 1]; $total = [int]$last.Groups[1].Value; $passed = [int]$last.Groups[2].Value; $failed = [int]$last.Groups[3].Value; $found = $true } else { $allPassedMatches = [regex]::Matches($raw, '(?im)^\s*All tests passed\s*\(\d+ assertions in (\d+) test cases\)\s*$'); if ($allPassedMatches.Count -gt 0) { $last = $allPassedMatches[$allPassedMatches.Count - 1]; $total = [int]$last.Groups[1].Value; $passed = $total; $failed = 0; $found = $true } } }; $color = if ($rc -eq 0) { 'Green' } else { 'Red' }; Write-Host ('[TEST_CASE] Total: ' + $total + ', Passed: ' + $passed + ', Failed: ' + $failed) -ForegroundColor $color; if (-not $found) { Write-Host '[WARN] Catch2 summary not found in log. TEST_CASE stats may be incomplete.' -ForegroundColor Yellow }"
    if "!ENGINE_TEST_RC!"=="0" (
        powershell -NoProfile -Command "Write-Host '[PASS] Engine CTest passed ^(-L engine full suite^).' -ForegroundColor Green"
    ) else (
        powershell -NoProfile -Command "Write-Host '[FAIL] Engine CTest failed. See output above.' -ForegroundColor Red"
        powershell -NoProfile -Command "$logPath = '!ENGINE_TEST_LOG!'; if (Test-Path $logPath) { $raw = Get-Content -Raw -Encoding UTF8 $logPath; $pattern = '(?ms)^-{5,}\r?\n(?<name>[^\r\n]+)\r?\n-{5,}\r?\n(?<location>[^\r\n]+\.cpp)\((?<line>\d+)\)\r?\n.*?\r?\n\s*[^\r\n]+\.cpp\((?<failLine>\d+)\): FAILED:'; $matches = [regex]::Matches($raw, $pattern); if ($matches.Count -gt 0) { Write-Host '[FAIL] Failed test cases:' -ForegroundColor Red; $seen = New-Object 'System.Collections.Generic.HashSet[string]'; foreach ($m in $matches) { $line = if ($m.Groups['failLine'].Success) { $m.Groups['failLine'].Value } else { $m.Groups['line'].Value }; $item = '- ' + $m.Groups['name'].Value.Trim() + ' (' + $m.Groups['location'].Value + ':' + $line + ')'; if ($seen.Add($item)) { Write-Host $item -ForegroundColor Red } } } else { Write-Host '[WARN] No failed testcase name/file matched in CTest log.' -ForegroundColor Yellow } } else { Write-Host '[WARN] CTest log file not found for failure parsing.' -ForegroundColor Yellow }"
        popd
        pause
        exit /b !ENGINE_TEST_RC!
    )
)

:: 计算并打印总耗时
set "END_TIME=%TIME%"
set "END_H=%END_TIME:~0,2%"
set "END_M=%END_TIME:~3,2%"
set "END_S=%END_TIME:~6,2%"
set "END_MS=%END_TIME:~9,2%"
if "%END_H:~0,1%"==" " set "END_H=0%END_H:~1,1%"

set /a END_TOTAL_CS=1%END_H%*360000 + 1%END_M%*6000 + 1%END_S%*100 + 1%END_MS% - 36610100

:: 处理跨天的情况
if %END_TOTAL_CS% lss %START_TOTAL_CS% set /a END_TOTAL_CS+=8640000

set /a DIFF_CS=%END_TOTAL_CS% - %START_TOTAL_CS%
set /a DIFF_S=%DIFF_CS% / 100
set /a DIFF_M=%DIFF_S% / 60
set /a DIFF_S=%DIFF_S% %% 60
set /a DIFF_H=%DIFF_M% / 60
set /a DIFF_M=%DIFF_M% %% 60

echo.
echo ========================================================
echo        [SUCCESS] All builds and verifications passed!
echo        [*] Total Build Time: %DIFF_H%h %DIFF_M%m %DIFF_S%s
echo ========================================================
popd
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

:resolve_first_existing_exe
setlocal enabledelayedexpansion
set "OUT_VAR=%~1"
shift
:resolve_first_existing_exe_loop
if "%~1"=="" (
    endlocal & exit /b 1
)
if exist "%~1" (
    endlocal & set "%OUT_VAR%=%~1" & exit /b 0
)
shift
goto resolve_first_existing_exe_loop

:usage
echo Usage: build_all.bat [options...]
echo Options:
echo   --with-editor          Enable editor build (default: ON)
echo   --no-editor            Disable editor build
echo   --with-launcher        Enable launcher build (default: ON)
echo   --no-launcher          Disable launcher build
echo   --with-tests           Enable engine regression tests via CTest ^(engine.unit + engine.lua_runtime + engine.spine^)
echo   --no-tests             Disable engine unit tests
echo   --package-editor-exe   Package editor executable (default: ON)
echo   --package-launcher-exe Package launcher executable (default: ON)
echo   --package-sdk          Install and package C++ SDK (default: ON)
echo   --no-sdk               Disable SDK packaging
echo   --with-verify-exe      Enable executable verification (default: ON)
echo   --no-verify-exe        Disable executable verification
echo   --all                  Enable editor and launcher build together
echo   --clean                Force clean reconfigure (remove CMakeCache.txt)
echo   --release              Build Release config (with LTCG, no debug info)
echo   --relwithdebinfo       Build RelWithDebInfo config (with LTCG + debug info)
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
popd
pause
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
    exit /b 1
)
call "!VS_DEV_CMD!" -arch=%ARCH% -host_arch=%ARCH% >nul
if !ERRORLEVEL! neq 0 (
    echo [ERROR] Failed to initialize Visual Studio 2022 developer environment.
    echo [HINT] Check VsDevCmd.bat and the installed VC++ workload.
    exit /b !ERRORLEVEL!
)
set "npm_config_msvs_version=2022"
set "GYP_MSVS_VERSION=2022"
echo [OK] Editor native build environment ready.
exit /b 0

:usage_error
echo Usage: build_all.bat [options...]
echo Use --help to list all options.
popd
pause
exit /b 1
