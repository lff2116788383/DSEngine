@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
echo ======================================================
echo      全AI像素风2D游戏超级工作流 一键部署
echo      图像+3D+动画+音效+文案 一站式本地安装
echo      开源免费 | 离线运行 | 可商用 | 低配适配
echo ======================================================
echo.

:: 检查管理员权限
fltmc >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误：请以【管理员身份】运行此脚本！
    pause
    exit
)

:: 安装Chocolatey包管理器
echo 正在安装系统依赖管理器Chocolatey...
@powershell -NoProfile -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
set "PATH=%PATH%;%ProgramData%\chocolatey\bin"
refreshenv

:: 安装基础环境
echo.
echo 正在安装Git、Python3.11、Node.js、FFmpeg...
choco install -y git python311 nodejs ffmpeg
refreshenv

:: 切换到脚本所在目录
cd /d "%~dp0"
echo 当前部署目录：%~dp0

:: 部署模块1：像素图像生成
echo.
echo [1/5] 部署AI像素图像生成模块
git clone https://github.com/playbegt/PixelDiffusion
cd PixelDiffusion
pip install torch torchvision gradio pillow timm --index-url https://download.pytorch.org/whl/cu118
pip install -r requirements.txt
cd ..

:: 部署模块2：3D轻量化适配
echo.
echo [2/5] 部署AI轻量化3D生成模块
git clone https://github.com/VAST-AI-Research/TripoSR
cd TripoSR
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu118
pip install -r requirements.txt
cd ..

:: 部署模块3：2D骨骼动画生成
echo.
echo [3/5] 部署AI2D骨骼动画模块
git clone https://github.com/realsoft3d/FastRig
cd FastRig
pip install trimesh numpy
cd ..
git clone https://github.com/scottpetrovic/mesh2motion-app
cd mesh2motion-app
npm install
cd ..

:: 部署模块4：AI音效+BGM生成
echo.
echo [4/5] 部署AI音频生成模块
git clone https://github.com/facebookresearch/audiocraft
cd audiocraft
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
pip install -e .
pip install gradio
cd ..

:: 部署模块5：AI剧情文案生成
echo.
echo [5/5] 部署AI剧情文案模块
git clone https://github.com/oobabooga/text-generation-webui
cd text-generation-webui
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
pip install -r requirements.txt
cd ..

:: 生成一键启动脚本
echo.
echo 生成全工作流一键启动脚本...
(
echo @echo off
echo chcp 65001 ^>nul
echo echo ======================================================
echo echo         全AI像素游戏工作流 一键启动面板
echo echo ======================================================
echo echo 1. 启动像素图像生成 (http://localhost:7860)
echo echo 2. 启动3D轻量化适配 (http://localhost:7861)
echo echo 3. 启动2D骨骼动画 (http://localhost:3000)
echo echo 4. 启动AI音效BGM (http://localhost:7862)
echo echo 5. 启动AI剧情文案 (http://localhost:7863)
echo echo 6. 启动全模块同步运行
echo echo ======================================================
echo echo.
echo cd /d "%%~dp0"
echo choice /c 123456 /m "请选择启动模式："
echo if errorlevel 6 goto all
echo if errorlevel 5 goto story
echo if errorlevel 4 goto audio
echo if errorlevel 3 goto anim
echo if errorlevel 2 goto 3d
echo if errorlevel 1 goto pixel
echo.
echo :pixel
echo start "像素图像生成" cmd /k "cd PixelDiffusion && python app.py"
echo pause
echo exit
echo.
echo :3d
echo start "3D适配" cmd /k "cd TripoSR && python gradio_app.py --device cuda --half"
echo pause
echo exit
echo.
echo :anim
echo start "骨骼动画" cmd /k "cd mesh2motion-app && npm run dev"
echo pause
echo exit
echo.
echo :audio
echo start "音效BGM" cmd /k "cd audiocraft && python app.py"
echo pause
echo exit
echo.
echo :story
echo start "剧情文案" cmd /k "cd text-generation-webui && python server.py --model Qwen-1.8B-Chat-GGUF --loader llama.cpp --n-gpu-layers 20"
echo pause
echo exit
echo.
echo :all
echo start "像素图像" cmd /k "cd PixelDiffusion && python app.py"
echo timeout /t 3 ^>nul
echo start "3D适配" cmd /k "cd TripoSR && python gradio_app.py --device cuda --half"
echo timeout /t 3 ^>nul
echo start "骨骼动画" cmd /k "cd mesh2motion-app && npm run dev"
echo timeout /t 3 ^>nul
echo start "音效BGM" cmd /k "cd audiocraft && python app.py"
echo timeout /t 3 ^>nul
echo start "剧情文案" cmd /k "cd text-generation-webui && python server.py --model Qwen-1.8B-Chat-GGUF --loader llama.cpp --n-gpu-layers 20"
echo echo 全模块启动完成！浏览器访问对应端口即可使用
echo pause
echo exit
) > 启动全AI游戏工作流.bat

:: 生成专用提示词文档
(
echo # 像素风游戏全AI生成专用提示词
echo ## 像素角色
echo pixel art, 16-bit, full body character, standing, front view, game asset, clean lines, white background, high contrast, retro style
echo.
echo ## 序列帧动画
echo pixel art, 8-frame walk cycle, sprite sheet, 32x32, seamless loop, 8-bit chiptune style, game rpg
echo.
echo ## 地图图块
echo pixel art tileset, seamless, 16x16, grass/stone/dirt, top-down view, platformer game, clean
echo.
echo ## 音效BGM
echo 8-bit pixel game jump sound, retro NES style; upbeat 8-bit chiptune battle theme, fast pace
echo.
echo ## 剧情文案
echo 像素冒险游戏，主线剧情简洁有起伏，NPC对话口语化，道具描述简短有画面感
) > 专用提示词合集.md

echo.
echo ======================================================
echo          ? 部署全部完成！
echo.
echo  生成文件：
echo  1. 启动全AI游戏工作流.bat（核心启动脚本）
echo  2. 专用提示词合集.md（直接复制使用）
echo.
echo  使用方式：右键以管理员身份运行启动脚本，选择对应模块即可
echo ======================================================
pause
