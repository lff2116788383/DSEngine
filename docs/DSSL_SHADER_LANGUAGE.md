# DSSL — DS Shading Language 设计方案

> DSEngine 自定义着色语言，面向游戏开发者的表面材质编程接口。
> 更新日期: 2025-05-11

---

## 一、为什么需要 DSSL

### 当前问题

DSEngine 现有 shader 方案要求开发者编写**完整的 GLSL 450 代码**：
- 必须手动声明全部 UBO layout（PerFrame/PerScene/PerMaterial，共 ~50 个字段）
- 必须自己实现光照计算、阴影采样、PBR BRDF
- 必须理解 set/binding 编号、clip_correction 等 RHI 细节
- 改一个材质表面颜色，需要写 385 行 `pbr.frag`

### 目标

开发者只需要关心**"这个表面长什么样"**，引擎自动处理光照、阴影、GI、后处理集成。

**之前（纯 GLSL 450）**：
```glsl
#version 450
layout(std140, set = 0, binding = 0) uniform PerFrame { mat4 vp; mat4 view; vec4 camera_pos; };
layout(std140, set = 1, binding = 0) uniform PerScene { /* 12个字段 */ };
layout(std140, set = 2, binding = 0) uniform PerMaterial { /* 8个字段 */ };
layout(set = 2, binding = 1) uniform sampler2D u_texture;
// ... 还有 6 个采样器、阴影贴图、点光源/聚光灯 UBO ...
// ... 200 行光照函数 ...
void main() {
    // 开发者真正关心的逻辑只有 10 行，但需要写 385 行
}
```

**之后（DSSL）**：
```dssl
shader_type surface;
render_mode blend_mix, cull_back, depth_draw_opaque;

uniform vec4 albedo_color : color = vec4(1.0, 1.0, 1.0, 1.0);
uniform sampler2D albedo_tex : filter_linear_mipmap;
uniform float roughness : range(0.0, 1.0) = 0.5;

void surface() {
    vec4 tex = sample(albedo_tex, UV);
    ALBEDO = tex.rgb * albedo_color.rgb;
    ALPHA = tex.a * albedo_color.a;
    ROUGHNESS = roughness;
    METALLIC = 0.0;
    NORMAL_MAP = sample(normal_tex, UV).rgb;
}
```

---

## 二、语言设计

### 2.1 文件格式

- 扩展名: `.dssl`
- 编码: UTF-8
- 语法: GLSL 子集 + 声明式元数据

### 2.2 Shader 类型

```dssl
shader_type <type>;
```

| 类型 | 用途 | 引擎注入内容 |
|------|------|-------------|
| `surface` | 3D 物体表面材质 | PBR 光照、阴影、GI、雾效 |
| `unlit` | 无光照材质 | 仅输出颜色，无光照计算 |
| `particle` | 粒子材质 | 粒子 billboard 变换、软粒子 |
| `sky` | 天空盒 | 天空采样、大气散射框架 |
| `postprocess` | 后处理效果 | 全屏四边形、场景颜色/深度输入 |
| `canvas` | 2D/UI 材质 | 正交投影、sprite batch 框架 |

### 2.3 渲染模式声明

```dssl
render_mode <mode1>, <mode2>, ...;
```

| 模式 | 类别 | 说明 |
|------|------|------|
| `blend_mix` | 混合 | Alpha 混合（默认） |
| `blend_add` | 混合 | 叠加混合 |
| `blend_mul` | 混合 | 乘法混合 |
| `blend_disabled` | 混合 | 不透明，禁用混合 |
| `cull_back` | 剔除 | 背面剔除（默认） |
| `cull_front` | 剔除 | 正面剔除 |
| `cull_disabled` | 剔除 | 双面渲染 |
| `depth_draw_opaque` | 深度 | 仅不透明物体写深度（默认） |
| `depth_draw_always` | 深度 | 始终写深度 |
| `depth_draw_disabled` | 深度 | 禁用深度写入 |
| `depth_test_disabled` | 深度 | 禁用深度测试 |
| `diffuse_burley` | 漫反射 | Burley 漫反射（默认） |
| `diffuse_lambert` | 漫反射 | Lambert 漫反射 |
| `diffuse_half_lambert` | 漫反射 | Half-Lambert（KF 风格） |
| `specular_schlick_ggx` | 高光 | Schlick GGX（默认 PBR） |
| `specular_disabled` | 高光 | 禁用高光 |
| `shadows_disabled` | 阴影 | 不接收阴影 |
| `alpha_test` | Alpha | 启用 Alpha 裁剪 |
| `wireframe` | 调试 | 线框模式 |

### 2.4 Uniform 声明

```dssl
uniform <type> <name> : <hint> = <default>;
```

**类型**：`float`, `vec2`, `vec3`, `vec4`, `mat4`, `int`, `bool`, `sampler2D`, `samplerCube`

**Hint（提示）**：

| Hint | 适用类型 | 说明 | 编辑器表现 |
|------|---------|------|-----------|
| `color` | vec3/vec4 | 颜色值（编辑器显示拾色器） | 🎨 Color Picker |
| `range(min, max)` | float/int | 数值范围（编辑器显示滑块） | ─●─ Slider |
| `filter_linear` | sampler | 线性过滤 | — |
| `filter_linear_mipmap` | sampler | 线性 + Mipmap | — |
| `filter_nearest` | sampler | 最近邻过滤 | — |
| `normal_map` | sampler2D | 法线贴图（自动 unpack） | — |
| `white_default` | sampler2D | 未赋值时使用白色默认纹理 | — |
| `black_default` | sampler2D | 未赋值时使用黑色默认纹理 | — |

**示例**：
```dssl
uniform vec4 albedo_color : color = vec4(1.0, 1.0, 1.0, 1.0);
uniform float metallic : range(0.0, 1.0) = 0.0;
uniform float roughness : range(0.04, 1.0) = 0.5;
uniform sampler2D albedo_tex : filter_linear_mipmap, white_default;
uniform sampler2D normal_tex : normal_map, filter_linear_mipmap;
```

### 2.5 内置变量（引擎自动注入）

#### surface() 可读输入

| 变量 | 类型 | 说明 |
|------|------|------|
| `UV` | vec2 | 第一套纹理坐标 |
| `UV2` | vec2 | 第二套纹理坐标 |
| `COLOR` | vec4 | 顶点颜色 |
| `VERTEX` | vec3 | 模型空间顶点位置 |
| `NORMAL` | vec3 | 模型空间法线 |
| `TANGENT` | vec3 | 模型空间切线 |
| `WORLD_POSITION` | vec3 | 世界空间位置 |
| `WORLD_NORMAL` | vec3 | 世界空间法线 |
| `VIEW_DIR` | vec3 | 指向相机方向（归一化） |
| `SCREEN_UV` | vec2 | 屏幕空间 UV |
| `TIME` | float | 引擎运行时间（秒） |
| `DELTA_TIME` | float | 帧间隔时间 |

#### surface() 可写输出

| 变量 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `ALBEDO` | vec3 | vec3(1.0) | 表面基色 |
| `ALPHA` | float | 1.0 | 透明度 |
| `METALLIC` | float | 0.0 | 金属度 |
| `ROUGHNESS` | float | 0.5 | 粗糙度 |
| `AO` | float | 1.0 | 环境光遮蔽 |
| `EMISSION` | vec3 | vec3(0.0) | 自发光颜色 |
| `NORMAL_MAP` | vec3 | — | 切线空间法线（自动 unpack 和应用） |
| `NORMAL_MAP_STRENGTH` | float | 1.0 | 法线强度 |
| `ALPHA_SCISSOR` | float | 0.5 | Alpha 裁剪阈值 |
| `RIM` | float | 0.0 | 边缘光强度 |
| `RIM_COLOR` | vec3 | vec3(1.0) | 边缘光颜色 |
| `DISPLACEMENT` | float | 0.0 | 顶点位移（沿法线） |

#### vertex() 可读写（可选覆写）

| 变量 | 类型 | 说明 |
|------|------|------|
| `VERTEX` | vec3 | 模型空间顶点位置（可修改） |
| `NORMAL` | vec3 | 模型空间法线（可修改） |
| `UV` | vec2 | 纹理坐标（可修改） |
| `COLOR` | vec4 | 顶点颜色（可修改） |
| `MODEL_MATRIX` | mat4 | 模型矩阵（只读） |
| `VIEW_MATRIX` | mat4 | 视图矩阵（只读） |
| `PROJECTION_MATRIX` | mat4 | 投影矩阵（只读） |

### 2.6 函数入口

```dssl
// 顶点阶段（可选）— 修改顶点属性
void vertex() {
    VERTEX += NORMAL * displacement_amount;
    UV = UV * uv_scale + uv_offset;
}

// 表面阶段（必需）— 定义材质属性
void surface() {
    ALBEDO = sample(albedo_tex, UV).rgb;
    ROUGHNESS = 0.5;
}

// 光照阶段（可选）— 自定义光照模型（覆盖引擎默认 PBR）
void light() {
    // 引擎为每个光源调用一次
    // 输入: LIGHT_DIR, LIGHT_COLOR, LIGHT_INTENSITY, ATTENUATION, SHADOW
    // 输出: DIFFUSE_LIGHT, SPECULAR_LIGHT
    float NdotL = max(dot(NORMAL, LIGHT_DIR), 0.0);
    DIFFUSE_LIGHT += LIGHT_COLOR * LIGHT_INTENSITY * NdotL * ATTENUATION * SHADOW;
}
```

**关键设计**：
- `surface()` 是**必需的**，定义材质表面属性
- `vertex()` 是可选的，不写则使用引擎默认顶点变换
- `light()` 是可选的，不写则使用引擎默认 PBR 光照
- 引擎为每个影响当前片元的光源调用一次 `light()`（与 Godot 同款机制）

### 2.7 内置函数

```dssl
// 纹理采样（封装 texture()，自动处理 Y-flip 等后端差异）
vec4 sample(sampler2D tex, vec2 uv);
vec4 sample_lod(sampler2D tex, vec2 uv, float lod);
vec4 sample_cube(samplerCube tex, vec3 dir);

// 数学工具
float remap(float value, float from_min, float from_max, float to_min, float to_max);
float fresnel(float power, vec3 normal, vec3 view);
vec3 unpack_normal(vec4 normal_map_sample);
```

---

## 三、完整示例

### 3.1 基础 PBR 材质

```dssl
shader_type surface;
render_mode blend_disabled, cull_back, specular_schlick_ggx;

uniform vec4 albedo_color : color = vec4(1.0);
uniform sampler2D albedo_tex : filter_linear_mipmap, white_default;
uniform sampler2D normal_tex : normal_map, filter_linear_mipmap;
uniform sampler2D orm_tex : filter_linear_mipmap;  // ORM: AO/Roughness/Metallic
uniform float roughness : range(0.04, 1.0) = 0.5;
uniform float metallic : range(0.0, 1.0) = 0.0;

void surface() {
    vec4 tex = sample(albedo_tex, UV);
    ALBEDO = tex.rgb * albedo_color.rgb * COLOR.rgb;
    ALPHA = tex.a * albedo_color.a;

    NORMAL_MAP = sample(normal_tex, UV).rgb;

    vec3 orm = sample(orm_tex, UV).rgb;
    AO = orm.r;
    ROUGHNESS = orm.g * roughness;
    METALLIC = orm.b * metallic;
}
```

### 3.2 自发光材质

```dssl
shader_type surface;
render_mode blend_disabled;

uniform vec4 emission_color : color = vec4(1.0, 0.2, 0.0, 1.0);
uniform float emission_strength : range(0.0, 10.0) = 2.0;
uniform sampler2D emission_tex : filter_linear_mipmap, black_default;

void surface() {
    ALBEDO = vec3(0.0);
    METALLIC = 0.0;
    ROUGHNESS = 1.0;
    EMISSION = sample(emission_tex, UV).rgb * emission_color.rgb * emission_strength;
}
```

### 3.3 风格化 Half-Lambert（KF 风格）

```dssl
shader_type surface;
render_mode diffuse_half_lambert, specular_disabled, blend_disabled;

uniform sampler2D albedo_tex : filter_linear_mipmap;
uniform sampler2D spec_tex : filter_linear_mipmap, black_default;

void surface() {
    ALBEDO = sample(albedo_tex, UV).rgb * COLOR.rgb;
    ROUGHNESS = 1.0;
}

void light() {
    float half_lambert = dot(WORLD_NORMAL, LIGHT_DIR) * 0.5 + 0.5;
    DIFFUSE_LIGHT += ALBEDO * half_lambert * LIGHT_COLOR * SHADOW;

    vec3 R = reflect(-LIGHT_DIR, WORLD_NORMAL);
    float spec = pow(max(dot(R, VIEW_DIR), 0.0), 100.0);
    SPECULAR_LIGHT += sample(spec_tex, UV).rgb * spec;
}
```

### 3.4 顶点动画（旗帜飘动）

```dssl
shader_type surface;
render_mode cull_disabled;

uniform sampler2D albedo_tex : filter_linear_mipmap;
uniform float wind_strength : range(0.0, 2.0) = 0.5;
uniform float wind_speed : range(0.0, 5.0) = 2.0;

void vertex() {
    float wave = sin(VERTEX.x * 2.0 + TIME * wind_speed) * wind_strength;
    wave *= UV.x;  // 旗杆端固定
    VERTEX.z += wave;
}

void surface() {
    ALBEDO = sample(albedo_tex, UV).rgb;
}
```

### 3.5 全屏后处理（灰度化）

```dssl
shader_type postprocess;

uniform float intensity : range(0.0, 1.0) = 1.0;

void postprocess() {
    vec3 scene = sample(SCREEN_TEXTURE, SCREEN_UV).rgb;
    float gray = dot(scene, vec3(0.299, 0.587, 0.114));
    FRAG_COLOR = vec4(mix(scene, vec3(gray), intensity), 1.0);
}
```

---

## 四、前置依赖

> **✅ 前置依赖已满足（2026-05-11）。**
>
> 三后端统一 Shader 改造已完成并合入 master（commit d142240）。
> DSSL transpiler 输出的 GLSL 450 可直接喂给 `tools/shader_compiler`。
> 代码模板中的 UBO layout（set/binding 编号、字段名）须与 `engine/render/shaders/src/pbr.frag` 对齐。
>
> **注意**：若先实施 Clustered Forward+（`docs/RENDER_PIPELINE_OPTIMIZATION.md` Phase 1），
> `pbr.frag` 的光源遍历将大改（UBO → SSBO + cluster 查找）。建议 DSSL 在 Clustered Forward+
> 完成后再实施，以避免模板代码二次返工。

---

## 五、编译架构

### 5.1 编译流水线

```
.dssl 源文件
    │
    ▼ DSSL 前端（文本预处理 + 代码注入）
完整 GLSL 450 源码
    │
    ▼ 复用现有 shader_compiler
 SPIR-V ─┬→ .spv         → Vulkan
         ├→ GLSL 330     → OpenGL
         └→ HLSL SM5     → DX11
```

**DSSL 编译器本质是一个代码生成器（transpiler）**，不是完整的编译器前端。
它将 `.dssl` 转换为完整的 GLSL 450，然后复用已有的 SPIRV-Cross 管线。

### 5.2 DSSL → GLSL 450 代码生成过程

以 surface shader 为例：

```
输入: example.dssl
├── 解析 shader_type → surface
├── 解析 render_mode → blend_disabled, cull_back
├── 解析 uniform 声明 → 收集到 PerMaterial UBO + 采样器绑定
├── 解析 vertex() 函数体 → 提取用户代码
├── 解析 surface() 函数体 → 提取用户代码
├── 解析 light() 函数体 → 提取用户代码（可选）
│
▼ 代码生成
├── 注入引擎 UBO 声明（PerFrame / PerScene / PerMaterial）
├── 注入内置变量定义（UV, COLOR, WORLD_POSITION, ...）
├── 注入引擎光照框架代码（PBR BRDF / 阴影采样 / GI 采样）
├── 将用户 surface() 代码嵌入材质属性填充位置
├── 将用户 vertex() 代码嵌入顶点变换位置
├── 将用户 light() 代码替换默认光照循环体
│
输出: example.vert.glsl (GLSL 450) + example.frag.glsl (GLSL 450)
```

### 5.3 生成的 GLSL 450 结构（伪代码）

```glsl
// ===== 引擎自动生成 — 开发者不可见 =====
#version 450

// 引擎注入: UBO 声明
layout(std140, set = 0, binding = 0) uniform PerFrame { ... };
layout(std140, set = 1, binding = 0) uniform PerScene { ... };

// 引擎注入: 用户 uniform → PerMaterial UBO + 采样器
layout(std140, set = 2, binding = 0) uniform PerMaterial {
    vec4 albedo_color;
    float roughness;
    float metallic;
    // ... 从 .dssl uniform 声明自动收集
};
layout(set = 2, binding = 1) uniform sampler2D albedo_tex;
layout(set = 2, binding = 2) uniform sampler2D normal_tex;

// 引擎注入: 内置变量
#define UV vTexCoord
#define COLOR vColor
#define WORLD_POSITION vFragPos
// ...

// 引擎注入: 光照函数库（PBR BRDF / Shadow / GI）
float DistributionGGX(...) { ... }
float ShadowCalculation(...) { ... }
// ...

// 引擎注入: surface 输出变量
vec3 ALBEDO = vec3(1.0);
float ALPHA = 1.0;
float ROUGHNESS = 0.5;
float METALLIC = 0.0;
float AO = 1.0;
vec3 EMISSION = vec3(0.0);
vec3 NORMAL_MAP_VALUE;
bool _has_normal_map = false;

void main() {
    // ===== 用户代码注入点: surface() =====
    {
        vec4 tex = texture(albedo_tex, UV);
        ALBEDO = tex.rgb * albedo_color.rgb * COLOR.rgb;
        ALPHA = tex.a * albedo_color.a;
        NORMAL_MAP_VALUE = texture(normal_tex, UV).rgb;
        _has_normal_map = true;
        // ...
    }
    // ===== 用户代码结束 =====

    // 引擎注入: 法线处理
    vec3 N = WORLD_NORMAL;
    if (_has_normal_map) {
        N = normalize(vTBN * (NORMAL_MAP_VALUE * 2.0 - 1.0));
    }

    // 引擎注入: 光照循环
    vec3 Lo = vec3(0.0);
    // 方向光
    {
        // 如果用户定义了 light()，插入用户代码
        // 否则使用引擎默认 PBR BRDF
    }
    // 点光源 / 聚光灯（同理）

    // 引擎注入: 合成
    vec3 color = ambient + Lo + EMISSION;
    color = color / (color + vec3(1.0));  // Reinhard
    color = pow(color, vec3(1.0/2.2));    // Gamma
    FragColor = vec4(color, ALPHA);
}
```

### 5.4 编译器实现复杂度

| 模块 | 工作量 | 说明 |
|------|--------|------|
| DSSL 解析器 | 1-2 周 | 解析 shader_type/render_mode/uniform/函数体，不需要完整 GLSL 语法树 |
| 代码模板库 | 1 周 | 为每种 shader_type 准备 GLSL 450 模板（vert + frag） |
| uniform 打包 | 2-3 天 | 将用户 uniform 自动布局到 UBO + 采样器绑定 |
| render_mode 映射 | 2-3 天 | mode → pipeline state 枚举 + #define 开关 |
| CMake / 工具集成 | 1-2 天 | 集成到现有 shader_compiler 工作流 |
| 编辑器集成 | 1 周 | Inspector 中根据 hint 自动生成 UI（滑块/拾色器/纹理槽） |
| **总计** | **4-5 周** | — |

---

## 六、运行时集成

### 6.1 材质实例化

```
.dssl 文件        → 编译 → ShaderProgram（引擎缓存，同一 .dssl 共享）
MaterialInstance  → 存储该材质的 uniform 值（每个物体可不同）
```

```cpp
// C++ 侧
auto* mat = asset_manager->LoadMaterial("materials/brick.dssl");
mat->SetVec4("albedo_color", {0.8f, 0.3f, 0.2f, 1.0f});
mat->SetFloat("roughness", 0.7f);
mat->SetTexture("albedo_tex", brick_texture);
```

```lua
-- Lua 侧
local mat = assets.load_material("materials/brick.dssl")
mat:set_color("albedo_color", 0.8, 0.3, 0.2, 1.0)
mat:set_float("roughness", 0.7)
mat:set_texture("albedo_tex", "textures/brick_albedo.png")
ecs.set_material(entity, mat)
```

### 6.2 与现有组件的对接

```
MeshRendererComponent
├── mesh_path           → 不变
├── shader_variant      → 废弃（由 .dssl 的 shader_type + render_mode 替代）
├── material_instance_id → 指向 DSSL MaterialInstance
├── albedo/roughness/... → 废弃（由 MaterialInstance uniform 值替代）
```

**迁移策略**：
- 旧的 `shader_variant = "MESH_LIT"` 等映射到内置 `.dssl` 文件
- `engine/render/shaders/builtin/pbr_default.dssl` — 替代 MESH_LIT
- `engine/render/shaders/builtin/unlit_default.dssl` — 替代 MESH_UNLIT
- 旧组件字段标记 `[[deprecated]]`，新材质全部走 DSSL

### 6.3 编辑器 Inspector 自动生成

DSSL 的 uniform hint 直接驱动编辑器 UI：

```
uniform vec4 albedo_color : color = vec4(1.0);
  → Inspector 显示: [Color Picker] Albedo Color

uniform float roughness : range(0.04, 1.0) = 0.5;
  → Inspector 显示: [Slider 0.04─●─1.0] Roughness

uniform sampler2D albedo_tex : filter_linear_mipmap;
  → Inspector 显示: [Texture Slot □] Albedo Tex
```

---

## 七、与现有 Shader 管线的关系

```
                    ┌──────────────────────┐
开发者写的:         │   .dssl 源文件        │
                    └──────────┬───────────┘
                               │ DSSL 前端（transpiler）
                               ▼
                    ┌──────────────────────┐
引擎内部:           │   GLSL 450 完整源码    │  ← 和手写 pbr.frag 同级别
                    └──────────┬───────────┘
                               │ 复用现有 shader_compiler
                               ▼
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
         .spv (Vulkan)   GLSL 330 (GL)    HLSL SM5 (DX11)
```

**DSSL 不替换现有 shader_compiler，而是在它上层增加一层**。
引擎内置 shader（PBR/Skybox/Bloom 等）仍可用 GLSL 450 直写，保持最大控制力。

---

## 八、实施路线图

```
阶段          时间     交付物
──────────────────────────────────────────────
Phase 1      2 周     DSSL 解析器 + surface shader 代码生成
Phase 2      1 周     内置模板库（surface/unlit/particle/postprocess）
Phase 3      1 周     运行时 MaterialInstance + Lua 绑定
Phase 4      1 周     编辑器 Inspector 自动 UI 生成
Phase 5      持续     文档 + 示例库 + 开发者指南
```

**Phase 1 的最小可行产品**：
- 解析 `shader_type surface` + `uniform` + `surface()` 函数体
- 生成完整 GLSL 450（复用现有 pbr.frag 作为模板）
- 通过现有 shader_compiler 编译为三后端
- 在 KF_Framework demo 中用 `.dssl` 替换一个手写材质并验证视觉一致

---

## 九、技术决策记录

| 决策 | 选择 | 备选 | 理由 |
|------|------|------|------|
| 语言名称 | **DSSL** (DS Shading Language) | DSShader / DSFx | 简短，与 GLSL/HLSL 命名惯例一致 |
| 文件扩展名 | **`.dssl`** | `.ds` / `.dshader` | `.ds` 有歧义，`.dshader` 太长 |
| 实现方式 | **Transpiler → GLSL 450** | 完整编译器前端 | 复用现有 SPIRV-Cross 管线，工作量减少 80% |
| 语法基础 | **GLSL 子集 + 声明式元数据** | 全新语法 | 降低学习曲线，GLSL 开发者零成本上手 |
| 光照入口 | **注入式 `light()` 回调** | 完全自定义 | Godot 验证过的模式，平衡灵活性和易用性 |
| 内置 shader | **保留 GLSL 450 直写** | 全部迁移到 DSSL | 引擎内部需要最大控制力 |
