@echo off
setlocal enabledelayedexpansion

REM Round 2 dependency submoduleization script
REM Target refs:
REM - glm -> 1.0.1
REM - rapidjson -> v1.1.0
REM - spdlog -> v1.14.1
REM - eventpp -> v0.1.3
REM - stb -> current stable commit on master
REM - lua -> v5.4.6
REM - luasocket-3.0.0 -> v3.1.0

cd /d "%~dp0"
if errorlevel 1 (
    echo [ERROR] Failed to switch to the repository root.
    pause
    exit /b 1
)

title DSEngine Round2 Submoduleize
color 0A

echo [INFO] Current directory: %CD%

git rev-parse --is-inside-work-tree >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Current directory is not a Git repository.
    pause
    exit /b 1
)

call :submoduleize "depends/glm" "https://github.com/g-truc/glm.git" "1.0.1"
if errorlevel 1 exit /b 1

call :submoduleize "depends/rapidjson" "https://github.com/Tencent/rapidjson.git" "v1.1.0"
if errorlevel 1 exit /b 1

call :submoduleize "depends/spdlog" "https://github.com/gabime/spdlog.git" "v1.14.1"
if errorlevel 1 exit /b 1

call :submoduleize "depends/eventpp" "https://github.com/wqking/eventpp.git" "v0.1.3"
if errorlevel 1 exit /b 1

call :submoduleize "depends/stb" "https://github.com/nothings/stb.git" "master"
if errorlevel 1 exit /b 1

call :submoduleize "depends/lua" "https://github.com/lua/lua.git" "v5.4.6"
if errorlevel 1 exit /b 1

call :submoduleize "depends/luasocket-3.0.0" "https://github.com/lunarmodules/luasocket.git" "v3.1.0"
if errorlevel 1 exit /b 1

echo.
echo [INFO] All submodule commands completed.
echo [INFO] Current submodule status:
git submodule status
if errorlevel 1 (
    pause
    exit /b 1
)

echo.
echo [INFO] Current Git status:
git status --short --ignore-submodules=none
if errorlevel 1 (
    pause
    exit /b 1
)

echo.
echo [INFO] If you still see many D depends/... entries, that is a normal staged deletion pattern
echo [INFO] when converting tracked directories into submodules.
echo [INFO] Those entries will become correct gitlink changes after the final commit.
echo [INFO] Please verify the following after the script finishes:
echo [INFO] 1. .gitmodules contains the new target submodules
echo [INFO] 2. git submodule status shows the intended refs
echo [INFO] 3. git status shows M .gitmodules, A depends/... and batches of D depends/...

echo.
echo [INFO] Script completed. Press any key to exit.
pause
exit /b 0

:submoduleize
set "TARGET=%~1"
set "URL=%~2"
set "REF=%~3"
set "ACTUAL_REF="

echo.
echo ==================================================
echo [INFO] Processing !TARGET!
echo [INFO] Repository: !URL!
echo [INFO] Ref: !REF!
echo ==================================================

for %%I in ("!TARGET!") do (
    set "TARGET_WIN=%%~fI"
)

REM Remove the old cached directory entry if the path is still tracked in the index
call git ls-files --error-unmatch "!TARGET!" >nul 2>nul
if not errorlevel 1 (
    echo [INFO] Removing old cached directory entry: !TARGET!
    git rm -r --cached "!TARGET!"
    if errorlevel 1 (
        echo [ERROR] git rm failed: !TARGET!
        pause
        exit /b 1
    )
)

REM Remove the working tree directory
if exist "!TARGET!" (
    echo [INFO] Removing working tree directory: !TARGET!
    rmdir /s /q "!TARGET!"
    if exist "!TARGET!" (
        echo [ERROR] Failed to remove working tree directory: !TARGET!
        pause
        exit /b 1
    )
)

REM Remove leftover metadata under .git/modules
if exist ".git\modules\!TARGET!" (
    echo [INFO] Removing leftover Git module metadata: .git\modules\!TARGET!
    rmdir /s /q ".git\modules\!TARGET!"
    if exist ".git\modules\!TARGET!" (
        echo [ERROR] Failed to remove leftover Git module metadata: .git\modules\!TARGET!
        pause
        exit /b 1
    )
)

REM Add the submodule
if /I "!REF!"=="master" (
    echo [INFO] Adding submodule ^(track default branch and record the current commit later^): !TARGET!
    git submodule add "!URL!" "!TARGET!"
) else (
    echo [INFO] Adding submodule: !TARGET!
    git submodule add "!URL!" "!TARGET!"
)
if errorlevel 1 (
    echo [ERROR] git submodule add failed: !TARGET!
    pause
    exit /b 1
)

REM Pin to the requested tag or branch reference
if /I not "!REF!"=="master" (
    echo [INFO] Checking out requested ref: !REF!
    git -C "!TARGET!" checkout "!REF!"
    if errorlevel 1 (
        echo [ERROR] checkout failed: !TARGET! -> !REF!
        pause
        exit /b 1
    )
) else (
    echo [INFO] Keeping the current commit on the default branch as the pinned commit: !TARGET!
)

REM Resolve actual checked out ref for validation
for /f "usebackq delims=" %%R in (`git -C "!TARGET!" describe --tags --exact-match 2^>nul`) do set "ACTUAL_REF=%%R"
if not defined ACTUAL_REF (
    for /f "usebackq delims=" %%R in (`git -C "!TARGET!" symbolic-ref --short HEAD 2^>nul`) do set "ACTUAL_REF=%%R"
)
if not defined ACTUAL_REF (
    for /f "usebackq delims=" %%R in (`git -C "!TARGET!" rev-parse --short HEAD 2^>nul`) do set "ACTUAL_REF=%%R"
)

echo [INFO] Actual resolved ref: !ACTUAL_REF!

if /I not "!REF!"=="master" (
    if /I not "!ACTUAL_REF!"=="!REF!" (
        echo [ERROR] Ref validation failed for !TARGET!
        echo [ERROR] Expected ref: !REF!
        echo [ERROR] Actual ref:   !ACTUAL_REF!
        echo [ERROR] The repository may use a different tag naming scheme. Please verify the correct tag first.
        pause
        exit /b 1
    )
)

REM Stage the submodule entry and .gitmodules
git add .gitmodules "!TARGET!"
if errorlevel 1 (
    echo [ERROR] git add failed: !TARGET!
    pause
    exit /b 1
)

exit /b 0
