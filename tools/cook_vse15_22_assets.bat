@echo off
setlocal enabledelayedexpansion

pushd "%~dp0\.."

set ASSET_BUILDER=bin\AssetBuilder.exe
set OUT_DIR=data\vse_demo\15_22\cooked

if not exist "%ASSET_BUILDER%" (
    echo [INFO] AssetBuilder not found. Building Debug target...
    if not exist build_vs2022\CMakeCache.txt (
        cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_GTESTS=OFF
        if !ERRORLEVEL! neq 0 goto :error
    )
    cmake --build build_vs2022 --config Debug --target AssetBuilder
    if !ERRORLEVEL! neq 0 goto :error
)

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo [INFO] Cooking VSEngine2.1 Demo 15.22 FBX assets to %OUT_DIR%

"%ASSET_BUILDER%" data\vse_demo\15_22\raw\FBXResource\Monster.FBX --out-dir "%OUT_DIR%"
if !ERRORLEVEL! neq 0 goto :error

"%ASSET_BUILDER%" reference\VSEngine2.1\FBXResource\Walk.FBX --out-dir "%OUT_DIR%"
if !ERRORLEVEL! neq 0 goto :error

"%ASSET_BUILDER%" reference\VSEngine2.1\FBXResource\Attack.FBX --out-dir "%OUT_DIR%"
if !ERRORLEVEL! neq 0 goto :error

"%ASSET_BUILDER%" reference\VSEngine2.1\FBXResource\Attack2.FBX --out-dir "%OUT_DIR%"
if !ERRORLEVEL! neq 0 goto :error

"%ASSET_BUILDER%" reference\VSEngine2.1\FBXResource\OceanPlane.FBX --out-dir "%OUT_DIR%"
if !ERRORLEVEL! neq 0 goto :error

echo [OK] VSE 15.22 cooked assets are ready under %OUT_DIR%.
popd
exit /b 0

:error
echo [ERROR] Failed to cook VSE 15.22 assets.
popd
exit /b 1
