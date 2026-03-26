@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
echo ==============================================
echo  人形3D工作流 全自动安装脚本
echo  自动安装：Git + Python + Node.js + 全部依赖
echo ==============================================
echo.

:: 设置路径
set "BASE_DIR=%~dp0"
cd /d "%BASE_DIR%"

:: 检查管理员权限
fltmc >nul 2>&1
if %errorlevel% neq 0 (
    echo 请以【管理员身份】运行此脚本！
    pause
    exit
)

echo 正在安装系统依赖...
echo.

:: 安装 Chocolatey（Windows包管理器）
if not exist "%ProgramData%\chocolatey\bin\choco.exe" (
    echo 正在安装 Chocolatey...
    @"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
    set "PATH=%PATH%;%ProgramData%\chocolatey\bin"
)

:: 安装必备工具
echo 正在安装 Git Python Node.js...
choco install -y git python311 nodejs
refreshenv

echo.
echo 环境安装完成，开始部署AI工作流...
echo.

:: ==================== 部署 TripoSR ====================
echo 正在部署 TripoSR（图生3D）...
git clone https://github.com/VAST-AI-Research/TripoSR
cd TripoSR
pip install torch torchvision
pip install -r requirements.txt
cd ..

:: ==================== 部署 Shap-E ====================
echo 正在部署 Shap-E（文生3D）...
git clone https://github.com/openai/shap-e
cd shap-e
pip install torch
pip install -e .
pip install gradio trimesh

:: 生成 WebUI 脚本
(
echo import gradio as gr
echo import torch
echo from shap_e.diffusion.sample import sample_latents
echo from shap_e.diffusion.gaussian_diffusion import diffusion_from_config
echo from shap_e.models.download import load_model, load_config
echo from shap_e.util.notebooks import decode_latent_mesh
echo.
echo device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
echo.
echo xm = load_model("transmitter", device=device)
echo model = load_model("text300M", device=device)
echo diffusion = diffusion_from_config(load_config("diffusion"))
echo.
echo def generate(prompt):
echo     latents = sample_latents(batch_size=1, model=model, diffusion=diffusion, guidance_scale=10.0, model_kwargs=dict(texts=[prompt]), progress=True)
echo     mesh = decode_latent_mesh(xm, latents[0]).tri_mesh()
echo     path = "shap_output.glb"
echo     mesh.export(path, include_texture=True)
echo     return path
echo.
echo with gr.Blocks() as demo:
echo     gr.Markdown("# Shap-E 文生3D（人形专用）")
echo     prompt = gr.Textbox(label="提示词（英文）")
echo     btn = gr.Button("生成")
echo     out = gr.Model3D()
echo     btn.click(generate, inputs=prompt, outputs=out)
echo demo.launch(server_port=7861)
) > app.py

cd ..

:: ==================== 部署 FastRig ====================
echo 正在部署 FastRig（自动绑骨）...
git clone https://github.com/realsoft3d/FastRig
cd FastRig
pip install trimesh numpy

:: 生成人形绑骨脚本
(
echo @echo off
echo python rig.py --input model.glb --output rigged.glb --human --auto-scale
echo echo 绑骨完成！
echo pause
) > 人形一键绑骨.bat

cd ..

:: ==================== 部署 Mesh2Motion ====================
echo 正在部署 Mesh2Motion（动画）...
git clone https://github.com/scottpetrovic/mesh2motion-app
cd mesh2motion-app
npm install
cd ..

:: ==================== 生成主启动脚本 ====================
echo.
echo 生成 一键启动工具...
(
echo @echo off
echo chcp 65001 >nul
echo echo ==============================================
echo echo  人形3D工作流 一键启动
echo echo  1. TripoSR  : http://localhost:7860
echo echo  2. Shap-E   : http://localhost:7861
echo echo  3. 动画工具 : http://localhost:3000
echo ==============================================
echo echo.
echo cd /d "%~dp0"
echo start "TripoSR" cmd /k "cd TripoSR && python gradio_app.py --device cuda --half"
echo timeout /t 2 >nul
echo start "Shap-E" cmd /k "cd shap-e && python app.py"
echo timeout /t 2 >nul
echo start "Mesh2Motion" cmd /k "cd mesh2motion-app && npm run dev"
echo echo 启动完成！
echo pause
) > 启动全套工作流.bat

echo.
echo ==============================================
echo  安装全部完成！
echo  文件夹内已生成：
echo  1. 启动全套工作流.bat
echo  2. FastRig\人形一键绑骨.bat
echo ==============================================
echo  使用流程：
echo  1. 双击启动全套
echo  2. 生成模型 → 放进FastRig改名model.glb
echo  3. 双击人形一键绑骨
echo  4. 去动画工具加动作
echo ==============================================
pause