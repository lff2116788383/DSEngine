# Shader 统一方案

## 现状

三后端各自内嵌独立 shader 源码：

| 后端 | 语言 | 存放位置 |
|------|------|---------|
| OpenGL | GLSL 330 | `gl_shader_manager.cpp` (内嵌字符串) |
| Vulkan | GLSL 450 | `vulkan_shader_sources.h` |
| DX11 | HLSL SM5.0 | `dx11_shader_sources.h` |

内置 shader 共 6-7 套：PBR、Skybox、Sprite、Particle、Shadow、PostProcess（DX11 额外有 Bloom CS）。

## 差异分析

### 核心光照逻辑：三端完全一致 ✅

PBR (GGX/Smith/Schlick)、Half-Lambert、点光/聚光、tone-mapping (Reinhard + gamma 2.2) 的数学公式三端相同。

### 主要差异点

#### 1. 资源绑定语法

| | OpenGL | Vulkan | DX11 |
|---|--------|--------|------|
| UBO | `layout(std140) uniform PerFrame` | `layout(set=0, binding=0) uniform PerFrame` | `cbuffer PerFrame : register(b0)` |
| 纹理 | `uniform sampler2D name` | `layout(set=N, binding=N) uniform sampler2D` | `Texture2D : register(tN)` + `SamplerState : register(sN)` |
| 逐对象数据 | `uniform mat4 u_model` | `push_constant` | `cbuffer PerObject : register(b1)` |

#### 2. 坐标系 / NDC

| | OpenGL | Vulkan | DX11 |
|---|--------|--------|------|
| NDC Z | [-1, 1] | [0, 1] | [0, 1] |
| NDC Y | ↑ | ↓ | ↑ |
| 阴影 proj remap | `xyz * 0.5 + 0.5` | `xy * 0.5 + 0.5` (z 已在 [0,1]) | `x * 0.5 + 0.5; y = -y * 0.5 + 0.5` |

#### 3. 采样器类型

| | OpenGL | Vulkan | DX11 |
|---|--------|--------|------|
| CSM 阴影 | `sampler2D` + 手动比较 | `sampler2DShadow` + 硬件比较 | `SamplerComparisonState` + `SampleCmpLevelZero` |

#### 4. 语法差异

- HLSL `mul(a, b)` vs GLSL `a * b`
- HLSL `saturate()` vs GLSL `clamp(x, 0.0, 1.0)`
- HLSL `float3(1,1,1)` vs GLSL `vec3(1.0)`
- HLSL 无 `bool` uniform（用 `int` + 比较）
- HLSL struct semantics (`:SV_POSITION`, `:TEXCOORD0`) vs GLSL `layout(location=N)`

## 推荐方案

### Phase 1：预处理宏 + 共享核心（最小工作量）

将光照数学提取到共享 `.inc` 文件，通过宏适配平台差异：

```
engine/render/shaders/
├── shared/
│   ├── pbr_lighting.inc      # GGX/Smith/Schlick/Fresnel
│   ├── shadow_sampling.inc   # CSM/Spot/Point 阴影计算
│   ├── half_lambert.inc      # Half-Lambert shading
│   └── platform_defs.inc     # 平台宏定义
├── opengl/
│   ├── pbr.vert.glsl         # 绑定声明 + #include shared
│   └── pbr.frag.glsl
├── vulkan/
│   ├── pbr.vert.glsl
│   └── pbr.frag.glsl
└── dx11/
    ├── pbr.vs.hlsl
    └── pbr.ps.hlsl
```

关键宏定义示例 (`platform_defs.inc`)：

```glsl
// OpenGL
#define DSE_UBO(name)           layout(std140) uniform name
#define DSE_TEXTURE2D(name)     uniform sampler2D name
#define DSE_MODEL_MATRIX        uniform mat4 u_model
#define DSE_SHADOW_PROJ(p)      (p * 0.5 + 0.5)
#define DSE_SHADOW_SAMPLE(tex, uv, depth)  ((depth - bias) > texture(tex, uv).r ? 1.0 : 0.0)

// Vulkan
#define DSE_UBO(name)           layout(std140, set=?, binding=?) uniform name
#define DSE_TEXTURE2D(name)     layout(set=?, binding=?) uniform sampler2D name
#define DSE_MODEL_MATRIX        layout(push_constant) uniform PushConstants { mat4 model; } pc
#define DSE_SHADOW_PROJ(p)      vec3(p.xy * 0.5 + 0.5, p.z)
#define DSE_SHADOW_SAMPLE(tex, uv, depth)  texture(tex, vec3(uv, depth - bias))

// DX11 (HLSL)
#define DSE_UBO(name)           cbuffer name : register(b?)
#define DSE_TEXTURE2D(name)     Texture2D name : register(t?)
#define DSE_MODEL_MATRIX        // 在 PerObject cbuffer 中
#define DSE_SHADOW_PROJ(p)      float3(p.x * 0.5 + 0.5, -p.y * 0.5 + 0.5, p.z)
#define DSE_SHADOW_SAMPLE(tex, uv, depth)  tex.SampleCmpLevelZero(cmp_sampler, uv, depth - bias)
```

### Phase 2：SPIRV-Cross 交叉编译（支持用户自定义 shader）

1. 以 **GLSL 450** 作为 shader 源语言（开发者只写一份）
2. **glslang** 编译 → SPIR-V
3. **SPIRV-Cross** 反编译 → GLSL 330 (OpenGL) / HLSL SM5 (DX11)
4. 需要引入 spirv-cross 第三方库依赖

## 工作量估算

| 阶段 | 工作量 | 收益 |
|------|--------|------|
| Phase 1 | 2-3 天 | 内置 shader 去重 ~60%，维护成本降低 |
| Phase 2 | 5-7 天 | 开发者只写一份 shader，自动跨平台 |

## 前置条件

- 三端渲染结果已对齐（RMSE < 25，亮度差 < 3）✅
- 坐标系修正已通过 `GetProjectionCorrection()` 在 CPU 侧统一 ✅
- 颜色空间已统一为 UNORM（无自动 gamma 转换）✅
