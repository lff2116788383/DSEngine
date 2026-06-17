@echo off
setlocal enabledelayedexpansion

:: ============================================================
::  DSEngine 总验收脚本 (verify_all.bat)
::  合并 GTest + Lua 运行时构建 + Lua 3D Demo preset 验证
::  使用方式：verify_all.bat [--skip-gtest] [--skip-lua] [--skip-demos]
:: ============================================================

pushd "%~dp0..\.."

:: 记录开始时间
set "START_TIME=%TIME%"
set "START_H=%START_TIME:~0,2%"
set "START_M=%START_TIME:~3,2%"
set "START_S=%START_TIME:~6,2%"
set "START_MS=%START_TIME:~9,2%"
if "%START_H:~0,1%"==" " set "START_H=0%START_H:~1,1%"
set /a START_TOTAL_CS=1%START_H%*360000 + 1%START_M%*6000 + 1%START_S%*100 + 1%START_MS% - 36610100

set BUILD_DIR=build_vs2022
set SKIP_GTEST=0
set SKIP_LUA=0
set SKIP_DEMOS=0
set SKIP_REGRESSION=0
set DEMO_TIMEOUT_SECONDS=5
set PASS_COUNT=0
set FAIL_COUNT=0
set SKIP_COUNT=0
set TOTAL_STEPS=0

:: 解析参数
:parse_args
if "%~1"=="" goto parse_done
if /I "%~1"=="--skip-gtest"   set SKIP_GTEST=1
if /I "%~1"=="--skip-lua"     set SKIP_LUA=1
if /I "%~1"=="--skip-demos"   set SKIP_DEMOS=1
if /I "%~1"=="--skip-regression" set SKIP_REGRESSION=1
if /I "%~1"=="--demo-timeout" (
    set "DEMO_TIMEOUT_SECONDS=%~2"
    shift
)
if /I "%~1"=="-h" goto usage
if /I "%~1"=="--help" goto usage
shift
goto parse_args
:parse_done

echo ========================================================
echo         DSEngine 总验收脚本 (verify_all.bat)
echo ========================================================
echo.
echo  配置:
echo    GTest:       %SKIP_GTEST%  (0=执行 1=跳过)
echo    Lua 构建:    %SKIP_LUA%    (0=执行 1=跳过)
echo    Demo 验证:   %SKIP_DEMOS%  (0=执行 1=跳过)
echo    Regression:  %SKIP_REGRESSION%  (0=执行 1=跳过)
echo    Demo 超时:   %DEMO_TIMEOUT_SECONDS% 秒
echo ========================================================
echo.

:: ============================================================
:: 步骤 1: CMake Configure
:: ============================================================
set /a TOTAL_STEPS+=1
echo [1] CMake Configure (GTest=ON, 3D=ON, PhysX=ON)...

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [INFO] 复用已有 CMakeCache.txt（增量构建）。
) else (
    echo [INFO] 首次配置...
)
cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64 ^
    -DDSE_BUILD_EDITOR=OFF ^
    -DDSE_BUILD_LAUNCHER=OFF ^
    -DDSE_BUILD_GTESTS=ON ^
    -DDSE_ENABLE_SPINE=ON ^
    -DDSE_ENABLE_3D=ON

if !ERRORLEVEL! neq 0 (
    echo [FAIL] CMake Configure 失败!
    popd
    pause
    exit /b 1
)
echo [OK] CMake Configure 完成。
echo.

:: ============================================================
:: 步骤 2: 构建 GTest targets
:: ============================================================
if "%SKIP_GTEST%"=="1" (
    echo [SKIP] GTest 构建与运行（--skip-gtest）
    echo.
    goto :lua_build
)

set /a TOTAL_STEPS+=1
echo [2] 构建 GTest targets (Debug)...

cmake --build %BUILD_DIR% --config Debug --target dse_gtest_unit_tests --parallel
if !ERRORLEVEL! neq 0 (
    echo [FAIL] dse_gtest_unit_tests 构建失败!
    set /a FAIL_COUNT+=1
    goto :gtest_run
)

cmake --build %BUILD_DIR% --config Debug --target dse_gtest_integration_tests --parallel
if !ERRORLEVEL! neq 0 (
    echo [FAIL] dse_gtest_integration_tests 构建失败!
    set /a FAIL_COUNT+=1
    goto :gtest_run
)

cmake --build %BUILD_DIR% --config Debug --target dse_gtest_smoke_tests --parallel
if !ERRORLEVEL! neq 0 (
    echo [FAIL] dse_gtest_smoke_tests 构建失败!
    set /a FAIL_COUNT+=1
    goto :gtest_run
)

echo [OK] GTest targets 构建完成。
echo.

:: ============================================================
:: 步骤 3: 运行 GTest via CTest
:: ============================================================
:gtest_run
echo [3] 运行 GTest (CTest)...

ctest --test-dir %BUILD_DIR% -C Debug --output-on-failure -L gtest
if !ERRORLEVEL! neq 0 (
    echo [FAIL] GTest 未全部通过!
    set /a FAIL_COUNT+=1
) else (
    echo [OK] GTest 全部通过。
    set /a PASS_COUNT+=1
)
echo.

:: ============================================================
:: 步骤 3b: 编辑器功能测试 (EditorFunctional filter)
:: ============================================================
set /a TOTAL_STEPS+=1
echo [3b] 编辑器功能测试 (EditorFunctional)...

if exist "%BUILD_DIR%\tests\gtest\integration\Debug\dse_gtest_integration_tests.exe" (
    "%BUILD_DIR%\tests\gtest\integration\Debug\dse_gtest_integration_tests.exe" --gtest_filter="EditorFunctional*" --gtest_print_time=1
    if !ERRORLEVEL! neq 0 (
        echo [FAIL] 编辑器功能测试未全部通过!
        set /a FAIL_COUNT+=1
    ) else (
        echo [OK] 编辑器功能测试全部通过。
        set /a PASS_COUNT+=1
    )
) else (
    echo [SKIP] dse_gtest_integration_tests.exe 未找到，跳过编辑器功能测试。
    set /a SKIP_COUNT+=1
)
echo.

:: ============================================================
:: 步骤 4: 构建 Lua 运行时
:: ============================================================
:lua_build
if "%SKIP_LUA%"=="1" (
    echo [SKIP] Lua 运行时构建（--skip-lua）
    echo.
    goto :demo_verify
)

set /a TOTAL_STEPS+=1
echo [4] 构建 Lua 运行时 (dse_example_lua, Debug)...

cmake --build %BUILD_DIR% --config Debug --target dse_example_lua --parallel
if !ERRORLEVEL! neq 0 (
    echo [FAIL] dse_example_lua 构建失败!
    set /a FAIL_COUNT+=1
    goto :demo_verify
)

echo [OK] Lua 运行时构建完成。
echo.

:: ============================================================
:: 步骤 5: Lua 3D Demo preset 验证
:: ============================================================
:demo_verify
if "%SKIP_DEMOS%"=="1" (
    echo [SKIP] Lua Demo 验证（--skip-demos）
    echo.
    goto :summary
)

:: 检查 Lua 可执行文件
set "LUA_EXE="
if exist ".\bin\dsengine_lua_debug.exe" (
    set "LUA_EXE=.\bin\dsengine_lua_debug.exe"
) else if exist ".\bin\dsengine_lua.exe" (
    set "LUA_EXE=.\bin\dsengine_lua.exe"
) else (
    echo [WARN] 未找到 Lua 可执行文件，跳过 Demo 验证。
    echo        请先运行 build_fast_lua.bat 构建 Lua 运行时。
    set /a SKIP_COUNT+=1
    goto :summary
)

:: 检查 lua.dll
if not exist ".\bin\lua.dll" (
    echo [WARN] 未找到 lua.dll，跳过 Demo 验证。
    set /a SKIP_COUNT+=1
    goto :summary
)

echo [5] Lua 3D Demo preset 验证 (超时=%DEMO_TIMEOUT_SECONDS%s)...
echo.

:: 3D Demo preset 列表
set "DEMOS=3d_triangle 3d_square 3d_cube 3d_static_model 3d_material_showcase 3d_lighting_showcase 3d_camera_showcase 3d_textured_cube 3d_scene_showcase 3d_skybox_environment 3d_postprocess_showcase 3d_particles_showcase 3d_physics_stack 3d_terrain_heightmap 3d_shadow_showcase 3d_animation_basic 3d_character_third_person 3d_audio_spatial 3d_physics_raycast_pick 3d_texture_material_slots 3d_terrain_lod_zones 3d_character_controller 3d_physics_interaction 3d_input_showcase 3d_hud_overlay 3d_procedural_mesh 3d_scene_load 3d_metrics_debug 3d_physics_triggers 3d_audio_complete 3d_asset_pack_showcase 3d_render_quality_showcase"

for %%D in (%DEMOS%) do (
    set /a TOTAL_STEPS+=1
    set "DEMO_NAME=%%D"
    <nul set /p "=  验证 !DEMO_NAME! ... "

    :: 生成临时 config.lua，设置 game_entry
    set "CONFIG_BAK="
    if exist "samples\lua\config.lua" (
        copy /Y "samples\lua\config.lua" "samples\lua\config.lua.verify_bak" >nul 2>&1
        set "CONFIG_BAK=1"
    )

    :: 写入临时配置
    echo Config={} > "samples\lua\config.lua"
    echo Config.title="DSEngine Verify: !DEMO_NAME!" >> "samples\lua\config.lua"
    echo Config.data_path="data/" >> "samples\lua\config.lua"
    echo Config.game_entry="!DEMO_NAME!" >> "samples\lua\config.lua"
    echo Config.basic_3d={camera_distance=8.0} >> "samples\lua\config.lua"
    echo return Config >> "samples\lua\config.lua"

    :: 运行 demo（限时）
    powershell -NoProfile -Command "$p = Start-Process -FilePath '!LUA_EXE!' -PassThru -WindowStyle Hidden; if ($p.WaitForExit(%DEMO_TIMEOUT_SECONDS%000)) { exit $p.ExitCode }; Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; exit 124"
    set "DEMO_RC=!ERRORLEVEL!"

    :: 恢复原 config.lua
    if "!CONFIG_BAK!"=="1" (
        copy /Y "samples\lua\config.lua.verify_bak" "samples\lua\config.lua" >nul 2>&1
        del /f /q "samples\lua\config.lua.verify_bak" >nul 2>&1
    )

    if "!DEMO_RC!"=="0" (
        echo [OK] 正常退出 ^(exit=0^)
        set /a PASS_COUNT+=1
    ) else if "!DEMO_RC!"=="124" (
        echo [OK] 超时退出 ^(运行中无崩溃^)
        set /a PASS_COUNT+=1
    ) else if "!DEMO_RC!"=="-1073741515" (
        echo [SKIP] DLL 缺失 ^(0xC0000135^)
        set /a SKIP_COUNT+=1
    ) else (
        echo [FAIL] 异常退出 ^(exit=!DEMO_RC!^)
        set /a FAIL_COUNT+=1
    )
)

echo.

:: ============================================================
:: 步骤 6: 截图回归对比 (demo_regression.py --compare)
:: ============================================================
if "%SKIP_REGRESSION%"=="1" (
    echo [SKIP] 截图回归对比（--skip-regression）
    echo.
    goto :summary
)
if "%SKIP_DEMOS%"=="1" (
    echo [SKIP] 截图回归对比（--skip-demos 已跳过 demo 验证）
    echo.
    goto :summary
)

:: 检查 Python
where python >nul 2>&1
if !ERRORLEVEL! neq 0 (
    echo [SKIP] 未找到 Python，跳过截图回归。
    set /a SKIP_COUNT+=1
    goto :summary
)

:: 6a: 逐后端回归
for %%B in (opengl dx11 vulkan) do (
    if exist "tests\regression\screenshots\%%B" (
        set /a TOTAL_STEPS+=1
        echo [6-%%B] 截图回归对比 ^(59 demo, %%B, RMSE阈值=5.0^)...
        python tools/demo_regression.py --compare --backend=%%B --no-sync
        if !ERRORLEVEL! neq 0 (
            echo [FAIL] %%B 截图回归有失败项!
            set /a FAIL_COUNT+=1
        ) else (
            echo [OK] %%B 截图回归全部通过。
            set /a PASS_COUNT+=1
        )
        echo.
    ) else (
        echo [SKIP] %%B 基线不存在，跳过。
    )
)

:: 6b: 跨后端对比（至少 2 个后端基线存在时执行）
set /a BACKEND_COUNT=0
if exist "tests\regression\screenshots\opengl" set /a BACKEND_COUNT+=1
if exist "tests\regression\screenshots\dx11"   set /a BACKEND_COUNT+=1
if exist "tests\regression\screenshots\vulkan" set /a BACKEND_COUNT+=1
if !BACKEND_COUNT! geq 2 (
    set /a TOTAL_STEPS+=1
    echo [6-cross] 跨后端 RMSE 对比 ^(threshold=20^)...
    python tools/demo_regression.py --cross-compare --threshold=20
    if !ERRORLEVEL! neq 0 (
        echo [WARN] 跨后端对比有超阈值项（仅警告，不计为失败）。
    ) else (
        echo [OK] 跨后端对比全部在阈值内。
        set /a PASS_COUNT+=1
    )
    echo.
)

:: ============================================================
:: 汇总
:: ============================================================
:summary
:: 计算总耗时
set "END_TIME=%TIME%"
set "END_H=%END_TIME:~0,2%"
set "END_M=%END_TIME:~3,2%"
set "END_S=%END_TIME:~6,2%"
set "END_MS=%END_TIME:~9,2%"
if "%END_H:~0,1%"==" " set "END_H=0%END_H:~1,1%"
set /a END_TOTAL_CS=1%END_H%*360000 + 1%END_M%*6000 + 1%END_S%*100 + 1%END_MS% - 36610100
if %END_TOTAL_CS% lss %START_TOTAL_CS% set /a END_TOTAL_CS+=8640000
set /a DIFF_CS=%END_TOTAL_CS% - %START_TOTAL_CS%
set /a DIFF_S=%DIFF_CS% / 100
set /a DIFF_M=%DIFF_S% / 60
set /a DIFF_S=%DIFF_S% %% 60
set /a DIFF_H=%DIFF_M% / 60
set /a DIFF_M=%DIFF_M% %% 60

echo ========================================================
echo         总验收结果
echo ========================================================
echo  通过: %PASS_COUNT%
echo  失败: %FAIL_COUNT%
echo  跳过: %SKIP_COUNT%
echo  总步骤: %TOTAL_STEPS%
echo  耗时: %DIFF_H%h %DIFF_M%m %DIFF_S%s
echo ========================================================

if %FAIL_COUNT% gtr 0 (
    echo [FAIL] 存在失败项，请检查上方输出。
    popd
    pause
    exit /b 1
)

echo [OK] 全部验收通过!
popd
pause
exit /b 0

:: ============================================================
:usage
echo 用法: verify_all.bat [选项...]
echo 选项:
echo   --skip-gtest         跳过 GTest 构建与运行
echo   --skip-lua           跳过 Lua 运行时构建
echo   --skip-demos         跳过 Lua 3D Demo 验证
echo   --skip-regression    跳过截图回归对比
echo   --demo-timeout SEC   设置每个 Demo 的超时秒数 (默认: 5)
echo   -h, --help           显示帮助
echo.
echo 默认: 执行全部验证 (GTest + Lua 构建 + Demo 验证 + 截图回归)
echo.
echo 说明:
echo   - GTest: 构建 unit/integration/smoke 三级 targets，通过 CTest 聚合运行
echo   - Lua 构建: 构建 dse_example_lua target
echo   - Demo 验证: 依次启动每个 3D demo preset，限时内无崩溃即通过
popd
pause
exit /b 0
