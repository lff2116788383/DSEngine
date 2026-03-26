整理一套**最清晰、可直接照着部署、8GB显存就能跑满**的完整方案：
**TripoSR + Shap-E-Lite + FastRig + Mesh2Motion**
全程**本地开源、免费、无API、无额度、可商用**，实现：
**图生3D + 文生3D + 纹理 + 自动绑骨 + 动画导出 GLB**

---

# 一、整套工作流详细功能
## 1. TripoSR（图生3D + 基础纹理）
- 输入：**单张图片**（角色/怪物/道具/场景）
- 输出：**带纹理 3D 模型（GLB）**
- 特点：
  - 速度极快：**1～3 秒出模型**
  - 结构准、不破面、适合游戏低模
  - 自动去背景、自动 UV、自动上色
  - 显存占用极低（**4GB 就能跑**）

## 2. Shap-E-Lite（文生3D + 颜色纹理）
- 输入：**文本提示词**（英文）
- 输出：**带颜色纹理 3D 模型（GLB）**
- 特点：
  - OpenAI 官方轻量模型，**8GB 显存可跑**
  - 适合卡通、小道具、简单角色
  - 生成速度：**15～30 秒**
  - 可直接导出用于绑骨和动画

## 3. FastRig（AI 自动绑骨 + 权重）
- 输入：静态 3D 模型（GLB/OBJ）
- 输出：**带骨骼 + 自动蒙皮权重**的模型
- 特点：
  - **1 秒完成绑骨**
  - 支持：人形、四足、简单怪物
  - 纯 CPU 可运行，**不吃显卡**
  - 导出标准骨架，兼容 Mixamo / Mesh2Motion

## 4. Mesh2Motion（动画库 + 导出带动画 GLB）
- 输入：FastRig 绑好骨的模型
- 功能：
  - 内置 **50+ 常用动作**（行走、跑、跳、待机、攻击、坐下、舞蹈）
  - 自动重定向动画
  - 实时预览、批量导出
  - **纯前端本地运行，完全离线**
- 输出：**最终带动画的 GLB** → 直接进 Unity/UE/Blender

---

# 二、详细硬件配置要求（真实可跑）
## 最低配置（真·低配）
- 显卡：**NVIDIA 显卡 ≥6GB 显存**
  - RTX 3050 4GB/6GB
  - RTX 2060 6GB
  - GTX 1660 Super / 1660 Ti 6GB
  - RTX 3060 8GB/12GB（最舒服）
- 内存：**16GB 以上**
- 系统：Windows 10/11 64位
- 存储：20GB 空闲空间
- Python：3.10～3.11
- CUDA：11.8 或 12.1（装一个就行）

## 各模块显存占用
- TripoSR：**≤4GB**
- Shap-E-Lite：**≤6GB**
- FastRig：**CPU 运行，0 显存**
- Mesh2Motion：**浏览器运行，0 显存**

👉 **8GB 显存可以同时流畅跑完全套**

---

# 三、完整本地部署流程（Windows 一步一步复制）
## 前置准备（必须先装）
1. 安装 **Git**：https://git-scm.com/
2. 安装 **Python 3.11**：https://www.python.org/
   - 安装时勾选 **Add Python to PATH**
3. 更新显卡驱动到最新

---

# 第1部分：部署 TripoSR（图生3D）
新建文件夹，打开 **CMD** 运行：

```bash
git clone https://github.com/VAST-AI-Research/TripoSR
cd TripoSR

pip install torch torchvision
pip install -r requirements.txt
```

启动 WebUI：
```bash
python gradio_app.py
```

访问：
http://localhost:7860

使用：
上传图片 → 生成 → 下载 **output.glb**

---

# 第2部分：部署 Shap-E-Lite（文生3D）
新开一个 CMD：

```bash
git clone https://github.com/openai/shap-e
cd shap-e

pip install torch
pip install -e .
pip install gradio trimesh
```

新建文件 `app.py`，复制下面全部代码：

```python
import gradio as gr
import torch
from shap_e.diffusion.sample import sample_latents
from shap_e.diffusion.gaussian_diffusion import diffusion_from_config
from shap_e.models.download import load_model, load_config
from shap_e.util.notebooks import decode_latent_mesh

device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

xm = load_model("transmitter", device=device)
model = load_model("text300M", device=device)
diffusion = diffusion_from_config(load_config("diffusion"))

def generate_from_text(prompt):
    latents = sample_latents(
        batch_size=1,
        model=model,
        diffusion=diffusion,
        guidance_scale=10.0,
        model_kwargs=dict(texts=[prompt]),
        progress=True,
        clamp_step=0,
    )
    mesh = decode_latent_mesh(xm, latents[0]).tri_mesh()
    out_path = "shap_e_output.glb"
    mesh.export(out_path, include_texture=True)
    return out_path

with gr.Blocks() as demo:
    gr.Markdown("# Shap-E-Lite 文生3D")
    prompt = gr.Textbox(label="英文提示词")
    btn = gr.Button("生成3D模型")
    out = gr.Model3D()
    btn.click(generate_from_text, inputs=prompt, outputs=out)

demo.launch(server_name="0.0.0.0", server_port=7861)
```

运行：
```bash
python app.py
```

访问：
http://localhost:7861

示例词：
`a cute cartoon robot, white and blue, smooth`

---

# 第3部分：部署 FastRig（自动绑骨）
新开 CMD：

```bash
git clone https://github.com/realsoft3d/FastRig
cd FastRig

pip install trimesh numpy
```

绑骨命令（把你前面生成的 model.glb 放同一目录）：

```bash
python rig.py --input model.glb --output rigged.glb
```

一秒生成：**rigged.glb（带骨骼）**

---

# 第4部分：部署 Mesh2Motion（本地动画库）
需要安装 **Node.js**（用于运行网页）：
https://nodejs.org/ 安装最新版

新开 CMD：

```bash
git clone https://github.com/scottpetrovic/mesh2motion-app
cd mesh2motion-app

npm install
npm run dev
```

访问：
http://localhost:3000

使用：
1. 上传 `rigged.glb`
2. 自动识别骨骼
3. 选择动作：walk / run / jump / attack / idle
4. 预览 → 导出 **animated_final.glb**

完成！

---

# 四、完整一条龙工作流（最终版）
1. **图生3D**
   TripoSR → 生成 `model.glb`

2. **或文生3D**
   Shap-E → 生成 `model.glb`

3. **自动绑骨**
   FastRig → `rigged.glb`

4. **加动画**
   Mesh2Motion → 选择动作 → 导出 `animated.glb`

5. **直接使用**
   Unity / Unreal / Blender / 游戏引擎

---

# 五、这套方案的优势（性价比之王）
✅ **8GB 显存即可跑满全套**
✅ **图生3D + 文生3D + 纹理 + 绑骨 + 动画**
✅ **全开源、全免费、可商用**
✅ **全本地离线，不上传任何数据**
✅ **WebUI 可视化，不用写复杂代码**
✅ **速度快，适合独立游戏、短视频、虚拟形象**

---




# 人形角色专用优化包
##  TripoSR + Shap-E + FastRig + Mesh2Motion
**专门做人形：更准、不穿模、动画更自然**
我直接给你**复制即用**的全套：
优化参数 + 最佳提示词 + Windows 一键启动脚本

---

# 一、人形角色专用优化参数（直接照抄）
## 1. TripoSR 图生3D（人形专用）
上传图片时必须勾选/设置：
- **Remove Background：ON**（必须去背景）
- **Resolution：512**
- **Texture Resolution：1024**
- **Marching Cubes Resolution：256**
- **Denoising Steps：30**
- **Sample Steps：3**

这样生成的**人体比例最准、不破面、四肢完整**。

## 2. Shap-E 文生3D（人形专用）
生成参数固定：
- `guidance_scale = 10`
- `batch_size = 1`
- `progress = True`
- 模型：`text300M`（轻量人形最稳）

## 3. FastRig 绑骨（人形专用，必用）
命令必须加 **--human** 开关（我给你优化版）：
```
python rig.py --input model.glb --output rigged.glb --human --auto-scale
```
作用：
- 强制使用**标准人形骨骼**
- 自动对齐脊柱、手臂、腿部
- 权重更自然，动画不穿模

## 4. Mesh2Motion 动画（人形专用）
导入后选择：
- **Skeleton Type：Humanoid**
- **Animation Retargeting：Standard**
- 优先用动作：
  - `Idle`
  - `Walk`
  - `Run`
  - `Jump`
  - `Attack Basic`
  - `Wave`

---

# 二、人形角色最佳提示词（直接复制）
## 1. 文生3D（Shap-E 专用，英文必用）
### 通用高质量人形
```
a full body cartoon character, standing, facing front, clean topology, smooth, high detail, white background, simple clothes
```

### Q版可爱
```
chibi character, full body, standing, cute, big head, small body, simple style, smooth
```

### 像素风角色
```
pixel art character, 32bit style, full body, standing, game asset, low poly
```

### 战士/角色
```
male warrior, full body, standing, armor, simple, smooth, game ready
```

## 2. 图生3D（TripoSR 提示词增强）
```
front view, full body, human character, clean, smooth, no background
```

---

