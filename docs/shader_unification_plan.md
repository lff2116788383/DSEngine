# Shader 统一方案：SPIRV-Cross 离线编译

> 更新日期：2026-05-11
> 方案选型：**SPIRV-Cross 离线编译**（行业主流，bgfx/Filament/The Forge 同类方案）
> 废弃：Phase 1 预处理宏方案（临时方案，会成技术债）

---

## 现状

三后端各自内嵌独立 shader 源码：

| 后端 | 语言 | 存放位置 |
|------|------|---------|
| OpenGL | GLSL 330 | `gl_shader_manager.cpp` (内嵌字符串) |
| Vulkan | GLSL 450 | `vulkan_shader_sources.h` |
| DX11 | HLSL SM5.0 | `dx11_shader_sources.h` |

内置 shader 共 7 套：PBR、Skybox、Sprite、Particle、Shadow、PostProcess、Bloom。

**问题**：同一套光照逻辑维护 3 份，修改一处需同步三端，极易产生不一致。

## 方案决策

| 方案 | 代表 | 工作量 | 维护成本 | 自定义 shader | 决策 |
|------|------|--------|----------|--------------|------|
| 预处理宏+共享字符串 | — | 2-3 天 | 中 | ❌ | ~~废弃~~ |
| **SPIRV-Cross 离线编译** | bgfx, Filament | 3-5 天 | **极低** | ✅ | **✅ 选定** |
| 自定义 DSL | Unity, Godot | 4-8 周 | 低 | ✅ | 过度工程 |

**选择理由**：
- 已有 glslang（`depends/glslang/`），只需新增 spirv-cross
- 写标准 GLSL 450，IDE/RenderDoc 原生支持
- 构建时离线生成，零运行时开销
- 未来加 Metal 后端零成本（spirv-cross 原生支持 MSL）

---

## 架构设计

### 编译流水线

```
                        ┌─── .spv ──────────────→ Vulkan (直接加载)
                        │
GLSL 450 源文件 → glslang → SPIR-V ─┤
                        │
                        └─── spirv-cross ──┬──→ GLSL 330 ──→ OpenGL
                                           └──→ HLSL SM5 ──→ DX11
```

### 目录结构

```
engine/render/shaders/
├── src/                          # 唯一 shader 源码目录 (GLSL 450)
│   ├── pbr.vert                  # PBR 顶点
│   ├── pbr.frag                  # PBR 片段
│   ├── skybox.vert
│   ├── skybox.frag
│   ├── sprite.vert
│   ├── sprite.frag
│   ├── particle.vert
│   ├── particle.frag
│   ├── shadow.vert
│   ├── shadow.frag
│   ├── postprocess.vert
│   ├── postprocess.frag
│   ├── bloom_downsample.comp     # Compute shader
│   └── bloom_upsample.comp
│
├── generated/                    # 构建时自动生成（.gitignore）
│   ├── spv/                      # Vulkan SPIR-V 字节码
│   │   ├── pbr.vert.spv
│   │   └── pbr.frag.spv ...
│   ├── glsl330/                  # OpenGL GLSL 330 源码
│   │   ├── pbr.vert.glsl
│   │   └── pbr.frag.glsl ...
│   └── hlsl/                     # DX11 HLSL SM5 源码
│       ├── pbr.vs.hlsl
│       └── pbr.ps.hlsl ...
│
└── compiler/                     # 离线编译工具
    ├── CMakeLists.txt
    └── shader_compiler.cpp       # CLI: glslang + spirv-cross
```

### 编译工具 (`shader_compiler`)

```
用法: dse_shader_compiler [options]
  --input-dir <path>      GLSL 450 源文件目录
  --output-dir <path>     输出目录
  --target <all|spv|glsl330|hlsl>  目标格式
  --embed                 生成 C++ 内嵌头文件 (constexpr string)
```

CMake 集成：
```cmake
add_custom_command(
    OUTPUT ${GENERATED_SHADER_HEADERS}
    COMMAND dse_shader_compiler
        --input-dir ${SHADER_SRC_DIR}
        --output-dir ${SHADER_GEN_DIR}
        --target all --embed
    DEPENDS ${SHADER_SOURCE_FILES}
    COMMENT "Compiling shaders (GLSL 450 → SPIR-V/GLSL 330/HLSL)"
)
```

### 后端加载方式变化

| 后端 | 之前 | 之后 |
|------|------|------|
| Vulkan | 字符串 → glslang → SPIR-V (运行时) | 直接加载 `.spv` (构建时已编译) |
| OpenGL | 内嵌 GLSL 330 字符串 | 加载生成的 GLSL 330 字符串 |
| DX11 | 内嵌 HLSL 字符串 | 加载生成的 HLSL 字符串 |

**额外收益**：Vulkan 后端不再需要运行时 glslang 编译，启动更快。

---

## 实施计划

### Step 1：引入 spirv-cross（~30 分钟）

```bash
cd depends
git clone --depth 1 --branch vulkan-sdk-1.3.283.0 \
    https://github.com/KhronosGroup/SPIRV-Cross.git spirv-cross
```

新增 `cmake/CMakeLists.txt.spirv_cross`，模式同 glslang 集成。

### Step 2：实现 shader_compiler 工具（~2 小时）

- `tools/shader_compiler/shader_compiler.cpp`
- 功能：遍历 `--input-dir`，对每个 `.vert/.frag/.comp` 文件：
  1. glslang 编译 → SPIR-V (`std::vector<uint32_t>`)
  2. spirv-cross 反编译 → GLSL 330 + HLSL SM5
  3. 输出 `.spv` 二进制 + `.h` 内嵌字符串头文件

### Step 3：提取 shader 源文件（~1 小时）

将 `vulkan_shader_sources.h` 中的 GLSL 450 字符串提取为独立 `.vert/.frag` 文件。
Vulkan 的 GLSL 450 是最完整的版本，以它为基准。

### Step 4：CMake 集成 + 后端适配（~2 小时）

- 添加 `add_custom_command` 在构建时调用 shader_compiler
- 修改三后端 ShaderManager 加载生成的头文件
- 删除旧的内嵌 shader 源码

### Step 5：验证（~1 小时）

- Release 编译零错误
- 三后端 visual_compare RMSE 无回归
- Vulkan 启动时间对比（应有微量提升，免去运行时编译）

---

## SPIRV-Cross 生成注意事项

### OpenGL 330 降级处理

spirv-cross 生成 GLSL 330 时需配置：
- `set_target_environment(glsl, 330)`
- `flatten_multidimensional_arrays(true)` — GLSL 330 不支持多维数组
- `set_options().version = 330`
- `set_options().es = false`
- 阴影采样器：spirv-cross 会自动将 `sampler2DShadow` 降级为 `sampler2D` + 手动比较

### HLSL SM5 转换处理

spirv-cross 生成 HLSL 时需配置：
- `set_hlsl_options().shader_model = 50`
- `set_hlsl_options().point_size_compat = false`
- Register 分配：spirv-cross 自动映射 set/binding → register(bN/tN/sN)
- Push constant → cbuffer PerObject

### Vulkan SPIR-V 特殊处理

- 保留 push_constant layout
- 保留 descriptor set/binding 语义
- Vulkan 后端直接加载 `.spv`，不经过 spirv-cross

---

## 前置条件

- 三端渲染结果已对齐（RMSE < 25，亮度差 < 3）✅
- 坐标系修正已通过 `GetProjectionCorrection()` 在 CPU 侧统一 ✅
- 颜色空间已统一为 UNORM（无自动 gamma 转换）✅
- glslang 已集成（`depends/glslang/`, `cmake/CMakeLists.txt.glslang`）✅

---

## 风险与回退

| 风险 | 缓解 |
|------|------|
| spirv-cross 生成的 GLSL 330 语法不兼容老驱动 | 对比生成结果与手写版本，必要时手动 fixup |
| spirv-cross HLSL 输出 register 冲突 | 通过 `set_decoration` 显式指定 register |
| 构建时间增长 | shader_compiler 仅在源文件变更时重新编译（DEPENDS） |
| 旧方案回退 | 保留旧 `*_shader_sources.h` 直到全量验证通过后再删除 |
