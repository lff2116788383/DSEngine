# 编辑器视口渲染正确性验证方案

## Devin 云端穿透验证模式

### 核心工作流

```
┌─────────────┐     隧道API      ┌──────────────────────────────────┐
│   Devin     │ ◄──────────────► │  远程 Windows (RTX 3070)         │
│  (验证者)   │                  │                                   │
│             │  1.启动编辑器     │  DSEngine Editor (GUI + GPU渲染)  │
│  看图判断   │  2.执行测试脚本   │  Scene视口: 3D渲染输出            │
│  像素分析   │  3.截图保存PNG    │  ImGui Test Engine: 构建场景      │
│             │  4.读回PNG        │  PowerShell截图: 捕获视口         │
└─────────────┘                  └──────────────────────────────────┘
```

**关键设计**：
- 不改引擎渲染管线
- 不需要外部 AI API Key
- Devin 自身作为视觉验证者
- C++ 代码提供确定性像素断言（P0 层）
- Devin 视觉判断提供语义级验证（P1 层）

---

## 1. 验证流程

### Step 1：启动编辑器（GUI 渲染测试模式）

```powershell
# 启动编辑器，渲染测试模式（最大化Scene视口，固定布局）
Start-Process -FilePath "bin\dsengine-editor.exe" `
    -ArgumentList "--project samples\test_scenes --render-test-mode"
Start-Sleep -Seconds 5
```

`--render-test-mode` 行为：
- 最大化 Scene 视口，隐藏/最小化其他面板
- 固定窗口分辨率 1920x1080
- 固定 Scene 视口区域坐标（便于精确截取）
- 禁用 UI 动画（避免截图时序问题）

### Step 2：测试代码构建场景

ImGui Test Engine 中构建预定义测试场景：

```cpp
t->TestFunc = [](ImGuiTestContext* ctx) {
    ClearScene(ctx);
    ctx->Yield(4);

    // 创建测试实体
    auto cube = NewSelectedEntity(ctx);
    AddComponent(ctx, "Mesh Renderer");
    ctx->Yield(2);

    auto light = NewSelectedEntity(ctx);
    AddComponent(ctx, "Directional Light");
    ctx->Yield(4);

    // 聚焦 Scene 视口
    ctx->WindowFocus("//Scene");
    ctx->Yield(20);  // 等待渲染稳定

    // 渲染稳定性检测：连续两帧截图，确认像素差异 < 阈值
    WaitForRenderStable(ctx, 5);

    // 触发截图
    CaptureSceneViewport("C:\\temp\\renders\\cube_lit.png");
    ctx->Yield(2);

    // 自动像素断言
    auto px = LoadCapturedPNG("C:\\temp\\renders\\cube_lit.png");
    IM_CHECK(px.valid);
    IM_CHECK(px.NonBlackRatio() > 0.05f);

    ClearScene(ctx);
};
```

### Step 3：精确截取 Scene 视口区域

```cpp
// 测试代码获取 Scene 视口的精确屏幕坐标
ImGuiWindow* scene_window = ImGui::FindWindowByName("Scene");
ImVec2 pos = scene_window->Pos;    // 窗口左上角屏幕坐标
ImVec2 size = scene_window->Size;  // 窗口尺寸

// 写坐标到文件供 PowerShell 读取
WriteViewportCoords("C:\\temp\\viewport_rect.txt", pos.x, pos.y, size.x, size.y);
```

PowerShell 只截取该区域：
```powershell
$coords = Get-Content "C:\temp\viewport_rect.txt"  # "x,y,w,h"
$parts = $coords.Split(",")
CaptureRegion -X $parts[0] -Y $parts[1] -W $parts[2] -H $parts[3] -Output $OutputPath
```

### Step 4：Devin 读回 PNG 验证

```powershell
# 读取截图 base64
$bytes = [System.IO.File]::ReadAllBytes("C:\temp\renders\cube_lit.png")
[Convert]::ToBase64String($bytes)
# Devin 解码后视觉判断
```

---

## 2. 双层验证策略

### Layer 1：自动化像素断言（确定性，C++ 内）

每个测试场景都有对应的确定性断言，无需人工介入：

```cpp
struct CapturedPixels {
    // 基础断言
    float NonBlackRatio(uint8_t threshold = 10) const;
    float AverageBrightness() const;
    float RegionBrightness(int x0, int y0, int x1, int y1) const;

    // 颜色断言
    float RedDominance() const;    // R / (R+G+B)
    float GreenDominance() const;
    float BlueDominance() const;
    bool IsColorDominant(char channel, float threshold = 0.45f) const;

    // 对比断言
    static float BrightnessDiff(const CapturedPixels& a, const CapturedPixels& b);
    static bool IsBrighter(const CapturedPixels& lit, const CapturedPixels& dark);

    // 稳定性断言
    static float PixelDifference(const CapturedPixels& frame1, const CapturedPixels& frame2);
    static bool IsStable(const CapturedPixels& f1, const CapturedPixels& f2, float threshold = 0.01f);
};
```

### Layer 2：Devin 视觉验证（语义级）

Devin 下载每个场景的截图后判断：
- 几何体形状是否可辨认
- 光照方向和强度是否符合预期
- 颜色是否匹配
- 有无渲染错误（全黑/全白/花屏/Z-fighting/穿模）
- 阴影/后处理效果是否可见

---

## 3. 完整测试场景（25 个）

### A. 基础几何体渲染（5 个）

| # | 测试ID | 场景构建 | 自动断言 | Devin 视觉验证 |
|:--|:-------|:---------|:---------|:---------------|
| 1 | `render_cube_default` | Cube + DirLight + 默认相机 | NonBlack > 10% | 可见立方体，有棱角 |
| 2 | `render_sphere_default` | Sphere + DirLight | NonBlack > 10% | 可见球体，光滑曲面轮廓 |
| 3 | `render_plane_default` | Plane(10x10) + DirLight + 俯视相机 | NonBlack > 20% | 可见平面，无穿模 |
| 4 | `render_cylinder_default` | Cylinder + DirLight | NonBlack > 10% | 可见柱体形状 |
| 5 | `render_multi_objects` | 3 Cubes @ (-2,0,0), (0,0,0), (2,0,0) + DirLight | NonBlack > 15% | 三个独立物体，空间布局正确 |

### B. 光照系统（6 个）

| # | 测试ID | 场景构建 | 自动断言 | Devin 视觉验证 |
|:--|:-------|:---------|:---------|:---------------|
| 6 | `render_dirlight_white` | Cube + 白色 DirLight(-1,-1,-1) | Brightness > 0.05 | 明暗面对比明显 |
| 7 | `render_pointlight_red` | Cube + 红色 PointLight(2,2,2) | RedDominance > 0.45 | 物体被红光照射 |
| 8 | `render_spotlight_cone` | Cube + SpotLight(0,3,0 → 0,-1,0, 30°) | 中心比边缘亮 | 可见聚光锥形区域 |
| 9 | `render_no_light_ambient` | Cube，无光源 | Brightness < 0.03 | 极暗但隐约可见轮廓 |
| 10 | `render_multi_lights` | Cube + 红PointLight(左) + 蓝PointLight(右) | NonBlack > 15% | 左侧偏红，右侧偏蓝 |
| 11 | `render_light_intensity` | Cube + DirLight(强度0.5) vs Cube + DirLight(强度2.0) | bright > dim | 高强度更亮 |

### C. 材质与纹理（5 个）

| # | 测试ID | 场景构建 | 自动断言 | Devin 视觉验证 |
|:--|:-------|:---------|:---------|:---------------|
| 12 | `render_material_red` | Cube + 红色 Albedo 材质 + 白光 | RedDominance > 0.45 | 立方体呈红色 |
| 13 | `render_material_blue` | Sphere + 蓝色材质 + 白光 | BlueDominance > 0.45 | 球体呈蓝色 |
| 14 | `render_texture_checker` | Cube + 棋盘格纹理 + 白光 | 像素方差 > 阈值（非纯色） | 表面有黑白棋盘格图案 |
| 15 | `render_material_metallic` | Sphere + 金属材质(metallic=1) + 白光 | 高光区域亮度 > 0.8 | 有明显镜面高光点 |
| 16 | `render_material_transparent` | 蓝色半透明 Cube(前) + 红色 Cube(后) + 白光 | 中心同时有R和B分量 | 可透过前方物体看到后方物体 |

### D. 阴影与后处理（5 个）

| # | 测试ID | 场景构建 | 自动断言 | Devin 视觉验证 |
|:--|:-------|:---------|:---------|:---------------|
| 17 | `render_shadow_ground` | Cube(y=1) + Plane(y=0) + DirLight + Shadow开 | 地面亮暗区差异 > 0.05 | 地面上可见阴影 |
| 18 | `render_shadow_self` | 大 Cube + 小 Cube(在后方) + DirLight | 被遮挡区域更暗 | 小物体被大物体遮挡区域更暗 |
| 19 | `render_bloom_effect` | 高发光 Sphere(emissive=5) + Bloom PostProcess | 高光区域像素 > 1.0(HDR映射后扩散) | 发光物体周围有光晕/泛光 |
| 20 | `render_fog_distance` | 3 Cubes(近/中/远) + Fog组件 | 远处物体亮度趋向雾色 | 远处物体被雾模糊/变淡 |
| 21 | `render_ao_corners` | 凹形房间(多 Plane) + AO | 角落亮度 < 开阔处 | 角落/凹处比开阔面更暗 |

### E. 相机与视角（4 个）

| # | 测试ID | 场景构建 | 自动断言 | Devin 视觉验证 |
|:--|:-------|:---------|:---------|:---------------|
| 22 | `render_camera_perspective` | 近处大 Cube + 远处小 Cube + 透视相机 | 近处物体像素占比 > 远处 | 近大远小，透视正确 |
| 23 | `render_camera_orthographic` | 同上 + 正交相机 | 两物体像素占比接近 | 物体大小不因距离变化 |
| 24 | `render_camera_closeup` | 相机距 Cube 0.5 单位 | NonBlack > 60% | 物体占满大部分视口 |
| 25 | `render_camera_far` | 相机距 Cube 20 单位 | NonBlack < 20% (物体小) | 物体可见但很小 |

---

## 4. 实现架构

### 4.1 文件结构

```
apps/editor_cpp/src/
├── ui_tests/
│   ├── ui_tests_render_validation.cpp   # 25 个渲染测试用例
│   ├── render_capture_helper.h          # 截图 + 像素分析
│   └── render_capture_helper.cpp
├── editor_app.cpp                       # 添加 --render-test-mode 支持
│
tools/
└── capture_viewport.ps1                 # Windows 窗口/区域截图脚本

docs/
└── editor_render_validation_plan.md     # 本文档
```

### 4.2 截图辅助类完整定义

```cpp
// render_capture_helper.h
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>

namespace dse::editor::uitest {

struct CapturedPixels {
    std::vector<uint8_t> data;  // RGBA, row-major
    int width = 0, height = 0;
    bool valid = false;

    struct RGBA {
        uint8_t r, g, b, a;
        float Brightness() const {
            return (r * 0.299f + g * 0.587f + b * 0.114f) / 255.0f;
        }
        bool IsBlack(uint8_t threshold = 10) const {
            return r < threshold && g < threshold && b < threshold;
        }
    };

    RGBA GetPixel(int x, int y) const;
    int PixelCount() const { return width * height; }

    // === 基础断言 ===
    float NonBlackRatio(uint8_t threshold = 10) const;
    float AverageBrightness() const;
    float RegionBrightness(int x0, int y0, int x1, int y1) const;
    float CenterBrightness(float radius_ratio = 0.25f) const;

    // === 颜色断言 ===
    float RedDominance() const;
    float GreenDominance() const;
    float BlueDominance() const;
    bool IsColorDominant(char channel, float threshold = 0.45f) const;
    float ColorVariance() const;  // 纹理检测：纯色方差低，有纹理方差高

    // === 区域对比 ===
    float LeftHalfBrightness() const;
    float RightHalfBrightness() const;
    float TopHalfBrightness() const;
    float BottomHalfBrightness() const;

    // === 静态对比 ===
    static float BrightnessDiff(const CapturedPixels& a, const CapturedPixels& b);
    static bool IsBrighter(const CapturedPixels& brighter, const CapturedPixels& darker);
    static float PixelDifference(const CapturedPixels& a, const CapturedPixels& b);
    static bool IsStable(const CapturedPixels& f1, const CapturedPixels& f2, float threshold = 0.01f);
};

// === 截图操作 ===

/// 触发 PowerShell 截取编辑器 Scene 视口区域
bool CaptureSceneViewport(const std::string& output_path, int stabilize_ms = 300);

/// 截取指定屏幕区域
bool CaptureScreenRegion(const std::string& output_path, int x, int y, int w, int h);

/// 加载 PNG 为像素数据
CapturedPixels LoadCapturedPNG(const std::string& path);

// === 渲染稳定性 ===

/// 连续截两帧，确认像素差异 < 阈值（避免渲染还没完成就截图）
bool WaitForRenderStable(int max_attempts = 5, float threshold = 0.02f);

// === 场景辅助 ===

/// 清空当前场景所有实体
void ClearTestScene(ImGuiTestContext* ctx);

/// 设置相机位置/朝向
void SetTestCamera(ImGuiTestContext* ctx, float px, float py, float pz,
                   float tx, float ty, float tz, float fov = 60.0f);

} // namespace
```

### 4.3 PowerShell 截图脚本

```powershell
# tools/capture_viewport.ps1
param(
    [Parameter(Mandatory=$true)]
    [string]$OutputPath,

    [string]$Mode = "viewport",     # "viewport" | "window" | "region"
    [string]$WindowTitle = "DSEngine",
    [string]$CoordsFile = "C:\temp\viewport_rect.txt",
    [int]$DelayMs = 200
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

Start-Sleep -Milliseconds $DelayMs

function Capture-Region($x, $y, $w, $h, $path) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $gfx = [System.Drawing.Graphics]::FromImage($bmp)
    $gfx.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $gfx.Dispose()
    $dir = [System.IO.Path]::GetDirectoryName($path)
    if (!(Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "OK: $path ($w x $h)"
}

switch ($Mode) {
    "viewport" {
        # 读取测试代码写出的视口坐标
        if (Test-Path $CoordsFile) {
            $parts = (Get-Content $CoordsFile).Trim().Split(",")
            $x = [int]$parts[0]; $y = [int]$parts[1]
            $w = [int]$parts[2]; $h = [int]$parts[3]
            Capture-Region $x $y $w $h $OutputPath
        } else {
            Write-Error "Coords file not found: $CoordsFile"
            exit 1
        }
    }
    "window" {
        $proc = Get-Process | Where-Object { $_.MainWindowTitle -like "*$WindowTitle*" } | Select-Object -First 1
        if ($proc) {
            $rect = New-Object Win32+RECT
            [Win32]::GetWindowRect($proc.MainWindowHandle, [ref]$rect) | Out-Null
            $w = $rect.Right - $rect.Left
            $h = $rect.Bottom - $rect.Top
            Capture-Region $rect.Left $rect.Top $w $h $OutputPath
        } else {
            Write-Error "Window not found: $WindowTitle"
            exit 1
        }
    }
    "region" {
        $parts = $CoordsFile.Split(",")  # 直接传坐标 "x,y,w,h"
        Capture-Region ([int]$parts[0]) ([int]$parts[1]) ([int]$parts[2]) ([int]$parts[3]) $OutputPath
    }
}
```

### 4.4 渲染测试模式（editor_app.cpp 修改）

```cpp
// 在编辑器启动参数解析中添加
if (HasArg("--render-test-mode")) {
    // 固定窗口分辨率
    glfwSetWindowSize(window, 1920, 1080);
    glfwSetWindowPos(window, 0, 0);

    // 最大化 Scene 视口的 docking 布局
    // （加载预设的 render-test imgui.ini 布局文件）
    ImGui::LoadIniSettingsFromDisk("configs/render_test_layout.ini");

    // 禁用 UI 动画
    ImGui::GetStyle().AnimationDuration = 0.0f;

    // 记录 Scene 视口坐标到文件（每帧更新）
    if (ImGuiWindow* scene = ImGui::FindWindowByName("Scene")) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d,%d,%d,%d",
            (int)scene->Pos.x, (int)scene->Pos.y,
            (int)scene->Size.x, (int)scene->Size.y);
        WriteSmallFile("C:\\temp\\viewport_rect.txt", buf);
    }
}
```

---

## 5. Devin 执行验证完整流程

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 Phase A: 自动化验证（C++ 像素断言）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. 构建项目
   > cmake --build build --config Release --target dse_editor_cpp

2. 启动编辑器（渲染测试模式）
   > Start-Process "bin\dsengine-editor.exe" "--render-test-mode"
   > Start-Sleep 5

3. 运行渲染测试（自动执行所有25个场景 + 像素断言）
   > bin\dsengine-editor-uitest.exe --run-ui-tests --filter "dse-render*"
   
   输出示例：
   [render-tests] tested=25 passed=25 failed=0
     render_cube_default ............ PASS (NonBlack=34.2%)
     render_dirlight_white .......... PASS (Brightness=0.12)
     render_material_red ............ PASS (RedDom=0.61)
     ...

4. 截图文件保存在 C:\temp\renders\
   - render_cube_default.png
   - render_sphere_default.png
   - render_dirlight_white.png
   - ...（共25个PNG）

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 Phase B: Devin 视觉验证
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

5. Devin 通过隧道逐一下载 PNG（base64）

6. Devin 查看每张截图，判断：
   □ 几何体形状是否正确
   □ 光照效果是否符合预期
   □ 颜色/材质是否匹配
   □ 有无渲染错误
   □ 特效（阴影/泛光/雾）是否可见

7. 生成验证报告：
   
   | 场景 | 自动断言 | 视觉验证 | 备注 |
   |------|----------|----------|------|
   | cube_default | PASS | PASS | 立方体清晰可见 |
   | spotlight_cone | PASS | PASS | 锥形光斑可见 |
   | shadow_ground | PASS | FAIL | 阴影未出现 |
   
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 Phase C: 问题修复与回归
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

8. 如果视觉验证发现问题：
   - 检查测试场景构建代码
   - 检查组件属性设置
   - 调整相机角度/距离
   - 重新运行

9. 全部通过后提交
```

---

## 6. 渲染稳定性保障

### 6.1 帧间稳定检测

避免截图时渲染还没完成（异步加载纹理、阴影 cascade 构建等）：

```cpp
bool WaitForRenderStable(int max_attempts, float threshold) {
    CapturedPixels prev, curr;
    for (int i = 0; i < max_attempts; i++) {
        CaptureSceneViewport("C:\\temp\\_stable_check.png");
        curr = LoadCapturedPNG("C:\\temp\\_stable_check.png");
        if (prev.valid && CapturedPixels::IsStable(prev, curr, threshold)) {
            return true;
        }
        prev = curr;
        Sleep(100);  // 等待 100ms 再截一帧
    }
    return false;  // 5次都不稳定，可能有问题
}
```

### 6.2 首帧跳过

编辑器首次渲染可能有初始化 artifact，跳过前 N 帧：

```cpp
ctx->Yield(20);  // 等待 20 帧再截图
```

### 6.3 固定随机种子

如果有粒子系统或随机效果，测试时使用固定种子：

```cpp
srand(42);  // 或引擎内部 SetRandomSeed(42)
```

---

## 7. 技术债分析与缓解

| 潜在技术债 | 严重程度 | 缓解措施 |
|:-----------|:---------|:---------|
| PowerShell 截图仅限 Windows | 低 | 引擎本身 Windows Only；如需跨平台用 platform ifdef |
| 截图依赖窗口可见（不能最小化） | 低 | --render-test-mode 保证窗口全屏 |
| Scene 视口坐标随 DPI 缩放变化 | 中 | 测试模式固定 DPI=100%，或读取实际坐标 |
| 截图时序（渲染未完成） | 中 | WaitForRenderStable() + 充足 Yield |
| stb_image 已在引擎中 | 无 | 直接使用，无额外依赖 |
| Devin 视觉验证非确定性 | 低 | P0 自动断言是确定性的；Devin 验证是附加层 |
| 未来 CI 自动化需求 | 低 | 预留 Golden Image 对比接口，后续可加 |

---

## 8. 与现有测试框架的集成

### 8.1 注册

```cpp
// ui_tests_internal.h
void RegisterRenderValidationTests(ImGuiTestEngine* engine);

// ui_tests_common.cpp
#ifdef DSE_RENDER_TESTS
    RegisterRenderValidationTests(engine);
#endif
```

### 8.2 CMake

```cmake
option(DSE_RENDER_TESTS "Enable viewport render validation tests" OFF)

if(DSE_RENDER_TESTS)
    target_compile_definitions(dse_editor_cpp PRIVATE DSE_RENDER_TESTS=1)
    target_sources(dse_editor_cpp PRIVATE
        src/ui_tests/ui_tests_render_validation.cpp
        src/ui_tests/render_capture_helper.cpp
    )
endif()
```

### 8.3 独立运行

```bash
# 仅 UI 控件测试（headless，无需 GPU）
bin\dsengine-editor-uitest.exe --run-ui-tests

# 仅渲染验证测试（需要 GPU + 窗口）
bin\dsengine-editor-uitest.exe --run-ui-tests --filter "dse-render*"

# 全部测试
bin\dsengine-editor-uitest.exe --run-ui-tests
```

---

## 9. 实现阶段

| Phase | 内容 | 预估 | 依赖 |
|:------|:-----|:-----|:-----|
| 1 | 文档推送 + capture_viewport.ps1 | 0.5h | 无 |
| 2 | render_capture_helper.h/cpp | 2h | stb_image |
| 3 | editor_app.cpp --render-test-mode | 1h | 无 |
| 4 | 25 个测试用例实现 | 4h | Phase 2-3 |
| 5 | 构建验证 | 0.5h | Phase 4 |
| 6 | 启动编辑器 + 运行测试 + 自动断言 | 1h | GPU 环境 |
| 7 | Devin 下载截图 + 视觉验证 | 1h | Phase 6 |
| 8 | 修复问题 + 回归 | 1-2h | Phase 7 |
| **总计** | | **~12h** | |

---

## 10. 成功标准

- [ ] 25 个渲染测试场景全部实现
- [ ] C++ 自动像素断言全部 PASS（25/25）
- [ ] Devin 视觉验证全部 PASS（25/25）
- [ ] 无渲染错误（全黑/花屏/穿模/Z-fighting）
- [ ] 3x 稳定性运行无 flaky（像素断言确定性通过）
- [ ] 代码提交到 feature/engine-lib
