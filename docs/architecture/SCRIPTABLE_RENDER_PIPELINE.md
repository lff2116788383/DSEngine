# 自定义后处理效果注入方案

> 日期：2026-05-26（v4，经三轮代码验证）  
> 状态：设计方案（待实施）

> **⚠️ 命名说明**：本文档描述的是「从 Lua 注入自定义屏幕空间后处理效果」，
> 不是完整的 SRP（可脚本化渲染管线）。DSEngine 的渲染管线本身已是可编程的（DSSL/GLSL），
> 并非固定管线；真正的 SRP 需要脚本控制 Pass 执行顺序和 RT 分配，超出本方案范畴。

---

## 一、现状诊断

### 已有能力（不需要重新实现）

DSEngine 已有完整的 `IRenderPass` + `RenderGraph`(DAG) 架构，并非固定管线。  
Pass 的**运行时配置**已通过 `PostProcessComponent`（ECS）+ `RenderThinSnapshot` 双缓冲机制完整覆盖：

| 已有字段 | Lua API |
|----------|---------|
| `bloom_enabled / bloom_threshold / bloom_intensity` | `ecs.set_post_process_bloom()` |
| `ssao_enabled / ssao_radius / ssao_bias` | `ecs.set_post_process_ssao()` |
| `fxaa_enabled` | `ecs.set_post_process_fxaa()` |
| `taa_enabled / taa_blend_factor` | `ecs.set_post_process_*` |
| `dof_enabled / ssr_enabled / motion_blur_enabled` | 同上 |
| `fog_enabled / fog_density / fog_color ...` | `ecs.set_post_process_fog()` |
| `light_shaft_* / outline_* / vignette_*` | 对应 `set_post_process_*` |

**所有 Pass 的 `Execute()` 已通过 `snap.post_process` 读取上述字段**，线程安全由双缓冲快照保证。

### 现有机制的工作方式

- `BuildRenderGraphInternal()` 在 `Init()` 时调用一次，DAG 静态构建
- 每帧 `CaptureThinSnapshot()` 将 `PostProcessComponent` 读入快照
- 各 Pass 在 `Execute()` 中读 `snap.post_process.*` 判断 enable，禁用则 early-return
- RT 显存在 `Init()` 时**全部预分配**，与 DAG 构建解耦

这是正确且高效的设计。禁用 Pass 的开销仅为一次 early-return 函数调用，RT 显存占用（PC 端约 10-20 MB）在可接受范围内。

### 真正的唯一缺口

**Lua 侧无法注入自定义屏幕空间后处理效果**——想在 Lua 里写 scanlines、自定义 outline 或其他全屏效果，目前必须改 C++。  
（C++ 侧已有 `IModule::RegisterRenderPasses()`，Lua 侧无等价路径）

**注意**：DSSL 已有 `shader_type postprocess`（Phase 1-3 全部完成），`DSSLMaterialInstance` 和 Lua 绑定也已就绪。
本方案的核心工作是把两者**桥接起来**：让 Lua 能通过 `.dssl` 文件在预定义 Hook 点注入全屏效果。

---

## 二、被否决的方案

### 方案 X1：PassRegistry 类

新增 `PassRegistry`，Lua 侧通过 `render.set_pass_enabled("bloom", false)` 控制 Pass。

**否决原因（7 项）**：

1. **重复配置系统**：`PassRegistry.SetParam("bloom","threshold",1.0)` 与 `ecs.set_post_process_bloom()` 功能完全重叠，两套数据源，技术债。
2. **全量重建不必要**：现有 Execute() early-return 已正确处理，引入 dirty→rebuild 机制是无效开销。
3. **线程安全漏洞**：SetParam()（主线程）与 RenderThreadFunc()（渲染线程）并发访问 map，无同步。
4. **float-only 参数残缺**：fog_color/outline_color 是 vec3，从第一天起就是不完整的 API。
5. **Factory 返回 nullptr 是反模式**：能力检查属于 Pass 自身逻辑。
6. **PassRegistry* 侵入 RenderPassContext**：三角依赖，Context 已有 174 行。
7. **注册顺序与 DAG 拓扑顺序混淆**：无声明的潜在 Bug。

### 方案 X2：LuaRenderPass（完整 Pass 注入）

允许 Lua 传入 `setup` + `execute` 回调，包装为 `IRenderPass`。

**否决原因（3 项）**：

1. **渲染线程调用 Lua 是数据竞争**：`Execute()` 运行在渲染线程，主线程同时运行 Lua 游戏逻辑，`lua_State*` 不是线程安全的，无法在 Execute() 内直接回调 Lua。
2. **需要把 RenderGraph + CommandBuffer 完整暴露给 Lua**：API 面极大，lifetime 管理复杂（`graph` 指针仅在 Setup() 期间有效）。
3. **`after` 字段与 DAG 排序语义冲突**：DAG 顺序由资源读写依赖决定，`after` 字段无法保证排序，需注入伪依赖资源，对用户不透明。

### 方案 X3：Setup() 感知的 Pass Culling

在 Setup() 中检查 enable 标志，不声明资源 → DAG 自动 cull → 释放 RT 显存。

**否决原因（2 项）**：

1. **Setup() 调用时 snapshot 为 null**：`BuildRenderGraphInternal()` 在 `Init()` 时执行，此时场景未加载，`render_pass_context_.snapshot = nullptr`，无法读取 PostProcessComponent。
2. **RT 分配与 DAG 构建解耦**：所有 RT 在 `Init()` 里无条件预分配，DAG cull Pass 不影响 RT 显存。要真正释放显存需改 `RenderPipelineResources` 整个分配流程，远超"小改"范畴。

---

## 三、正确方案：PostEffect Hook 点

### 目标

允许 Lua 注册自定义屏幕空间效果 shader，在内置 Pass 的特定阶段插入执行。

### 核心设计原则

- **沿用已有的 `PostProcessComponent` + 快照机制**，不引入新的配置系统
- **Lua 只在主线程运行**，渲染线程仅读快照，无并发问题
- **不暴露 RenderGraph 或 CommandBuffer 给 Lua**

### 数据流

```
主线程（Lua）                        渲染线程
─────────────────────────────────   ──────────────────────────────
ecs.add_post_effect(e, {            CaptureThinSnapshot()
    slot   = "after_tonemap",         → snap.post_process.custom_effects[]
    shader = "shaders/scan.glsl",
    params = {intensity=0.5}        CompositePass::Execute()
})                                    → 遍历 snap.post_process.custom_effects
                                      → 按 slot 调用 DrawPostProcess()
```

### 实施细节

#### 1. 扩展 `PostProcessComponent`（`engine/ecs/components_3d.h`）

```cpp
struct PostEffectEntry {
    std::string slot;           // "after_bloom" | "after_tonemap" | "after_fxaa" | "before_ui"
    std::string dssl_path;      // .dssl 文件路径（shader_type postprocess）
    int dssl_mat_id = -1;       // DSSLMaterialInstance ID（运行时缓存，-1 表示未加载）
    bool enabled = true;
    // uniform 值由 DSSLMaterialInstance 管理，不在此存储
};

struct PostProcessComponent {
    // ... 现有字段不变 ...

    // 自定义屏幕空间效果列表（使用 DSSL postprocess shader）
    std::vector<PostEffectEntry> custom_effects;
};
```

**关键设计**：uniform 参数由 `DSSLMaterialInstance` 管理（已有 Lua API `dssl.set_float/set_color/...`），
`PostEffectEntry` 只存路径和实例 ID，不重复造参数存储。

#### 2. 快照中携带自定义效果（`engine/render/render_snapshot.h`）

`RenderThinSnapshot::post_process` 已是 `PostProcessComponent` 的副本，扩展后自动携带 `custom_effects`。

#### 3. 在内置 Pass 的预定义 Hook 点执行（`builtin_passes.cpp`）

```cpp
// CompositePass::Execute() 末尾（after_tonemap hook 点示例）
for (const auto& effect : snap.post_process.custom_effects) {
    if (!effect.enabled || effect.slot != "after_tonemap") continue;
    if (effect.dssl_mat_id < 0) continue;

    // DSSLMaterialInstance 持有编译好的 shader program + uniform 值
    auto* mat = ctx_.asset_manager->GetDSSLMaterial(effect.dssl_mat_id);
    if (!mat || !mat->IsReady()) continue;

    unsigned int shader_prog = mat->GetShaderProgram();
    // 上传 uniform（DSSLMaterialInstance 统一处理）
    mat->BindUniforms(ctx_.rhi_device);
    cmd_buffer.DrawPostProcessWithShader(shader_prog, scene_tex);
}
```

#### 4. Lua API（`lua_binding_ecs_rendering.cpp` 追加）

```lua
-- 注册自定义后处理效果（使用 DSSL postprocess shader）
local e = ecs.create_entity()
ecs.add_post_process(e, true, false, 1.0, 1.0)

-- 加载 DSSL material 并注入到 Hook 点
local mat = dssl.load_material("effects/scanlines.dssl")
dssl.set_float(mat, "intensity", 0.5)
dssl.set_float(mat, "frequency", 100.0)
ecs.add_post_effect(e, "after_tonemap", mat)

-- 动态修改参数（直接操作 DSSLMaterialInstance，下一帧生效）
dssl.set_float(mat, "intensity", 0.8)

-- 开关效果
ecs.set_post_effect_enabled(e, "after_tonemap", false)
```

### 支持的 Hook 点

| slot 名称 | 插入位置 | 典型用途 |
|-----------|---------|---------|
| `after_bloom` | BloomPass 之后 | 自定义光晕叠加 |
| `after_tonemap` | CompositePass 之后 | scanlines、色调调整 |
| `after_fxaa` | FXAAPass/TAAPass 之后 | 最终输出叠加 |
| `before_ui` | UIPass 之前 | 屏幕空间效果，不影响 UI |

---

## 四、与 DSSL 结合的完整示例

### 示例 1：扫描线（Scanlines）

**第一步：写 `.dssl` 文件**（`samples/effects/scanlines.dssl`）

```dssl
shader_type postprocess;

uniform float intensity : range(0.0, 1.0) = 0.5;
uniform float frequency : range(10.0, 500.0) = 100.0;

void postprocess() {
    vec3 scene = sample(SCREEN_TEXTURE, SCREEN_UV).rgb;
    float line = mod(FRAGCOORD.y, frequency) < 1.0 ? (1.0 - intensity) : 1.0;
    FRAG_COLOR = vec4(scene * line, 1.0);
}
```

**第二步：Lua 中注册**

```lua
local pp_entity = ecs.create_entity()
ecs.add_post_process(pp_entity, true, false, 1.0, 1.0)

local scanlines = dssl.load_material("samples/effects/scanlines.dssl")
dssl.set_float(scanlines, "intensity", 0.4)
dssl.set_float(scanlines, "frequency", 80.0)
ecs.add_post_effect(pp_entity, "after_tonemap", scanlines)
```

---

### 示例 2：夜视仪（Night Vision）

**`samples/effects/night_vision.dssl`**

```dssl
shader_type postprocess;

uniform float noise_strength : range(0.0, 0.3) = 0.08;
uniform vec3 tint_color : color = vec3(0.1, 0.95, 0.2);
uniform float vignette_radius : range(0.3, 1.0) = 0.6;

void postprocess() {
    vec3 scene = sample(SCREEN_TEXTURE, SCREEN_UV).rgb;
    float lum = dot(scene, vec3(0.299, 0.587, 0.114));

    // 随机噪点
    float noise = fract(sin(dot(SCREEN_UV + TIME * 0.1, vec2(12.9898, 78.233))) * 43758.5453);
    lum = clamp(lum + (noise - 0.5) * noise_strength, 0.0, 1.0);

    // 绿色 tint
    vec3 color = tint_color * lum;

    // 晕影
    float dist = length(SCREEN_UV - vec2(0.5));
    float vignette = smoothstep(vignette_radius, vignette_radius - 0.3, dist);
    FRAG_COLOR = vec4(color * vignette, 1.0);
}
```

**Lua 中注册（并绑定键盘切换）**

```lua
local nv_mat = dssl.load_material("samples/effects/night_vision.dssl")
ecs.add_post_effect(pp_entity, "after_fxaa", nv_mat)
ecs.set_post_effect_enabled(pp_entity, "after_fxaa", false)  -- 默认关闭

-- 按 N 键切换
function on_update(dt)
    if input.key_just_pressed("N") then
        local enabled = ecs.get_post_effect_enabled(pp_entity, "after_fxaa")
        ecs.set_post_effect_enabled(pp_entity, "after_fxaa", not enabled)
    end
end
```

---

### 示例 3：像素化（Pixelate）—— 带运行时参数热更新

**`samples/effects/pixelate.dssl`**

```dssl
shader_type postprocess;

uniform float pixel_size : range(1.0, 32.0) = 4.0;

void postprocess() {
    vec2 uv = floor(SCREEN_UV * (1.0 / pixel_size)) * pixel_size;
    // 注意：SCREEN_SIZE 是引擎注入的 vec2 内置变量
    uv = floor(SCREEN_UV * SCREEN_SIZE / pixel_size) / (SCREEN_SIZE / pixel_size);
    FRAG_COLOR = vec4(sample(SCREEN_TEXTURE, uv).rgb, 1.0);
}
```

**Lua 中动态调节像素大小**

```lua
local px_mat = dssl.load_material("samples/effects/pixelate.dssl")
ecs.add_post_effect(pp_entity, "before_ui", px_mat)

-- 滑动条控制（编辑器或游戏内调试 UI）
function on_update(dt)
    local size = some_slider_value  -- 来自游戏内 UI
    dssl.set_float(px_mat, "pixel_size", size)  -- 直接改 MaterialInstance，下一帧生效
end
```

---

### DSSL postprocess 内置变量速查

| 变量 | 类型 | 说明 |
|------|------|------|
| `SCREEN_TEXTURE` | `sampler2D` | 当前帧场景颜色 |
| `SCREEN_UV` | `vec2` | 全屏 UV（左下 0,0 → 右上 1,1）|
| `SCREEN_SIZE` | `vec2` | 屏幕分辨率（像素）|
| `FRAGCOORD` | `vec4` | 片元屏幕坐标（像素）|
| `TIME` | `float` | 引擎运行时间（秒）|
| `DELTA_TIME` | `float` | 帧间隔 |
| `FRAG_COLOR` | `vec4` | **输出**：写入最终颜色 |

---

## 五、实施计划

```
Phase（单阶段，约 1 天）
  前置：DSSL Phase 1-3 已完成（Parser/Codegen/MaterialInstance/Lua 绑定全部就绪）

  C++ 改动：
    engine/ecs/components_3d.h
      — PostEffectEntry（slot + dssl_path + dssl_mat_id + enabled）
      — PostProcessComponent 加 custom_effects 字段
    engine/render/render_snapshot.h
      — 确认快照副本包含 custom_effects（含 dssl_mat_id）
    engine/render/passes/builtin_passes.cpp
      — 4 个 Hook 点各加 DSSLMaterialInstance 查找 + DrawPostProcessWithShader（~15 行/处）
    engine/runtime/frame_pipeline.cpp
      — CaptureThinSnapshot() 复制 custom_effects

  Lua 绑定（lua_binding_ecs_rendering.cpp 追加）：
    — ecs.add_post_effect(entity, slot, dssl_mat_id)
    — ecs.set_post_effect_enabled(entity, slot, bool)
    — ecs.get_post_effect_enabled(entity, slot) → bool
    （约 60 行）

  测试：
    — 注册 scanlines.dssl → 验证截图含扫描线
    — set_post_effect_enabled(false) → 验证效果消失
    — dssl.set_float(mat, ...) → 验证参数下一帧生效
    — 多个效果同一 slot → 验证叠加顺序正确
```

---

## 六、路径 B：Lua-Configured SRP（按需扩展）

> 当前 PostEffect Hook 覆盖约 70% 的需求。以下场景需要路径 B：
> - Render to Texture（镜子、传送门、小地图）
> - 多 Camera 不同画质（主摄像机 HDR + UI 摄像机 LDR）
> - 完全跳过某个内置 Pass（如去掉 SSAO 但保留所有其他 Pass）
> - 自定义 RT 格式（如需要 HDR 中间缓冲做特殊合成）

### 核心设计原则

**Lua 只在主线程描述管线，渲染线程只执行，两者完全解耦。**

```
场景初始化（主线程）          每帧执行（渲染线程）
────────────────────         ──────────────────────
render.begin_pipeline()  →   BuildRenderGraphFromDescriptor()
render.add_pass(...)     →   → 生成 DAG（一次性）
render.end_pipeline()    →   → 每帧 Execute()（不涉及 Lua）
                             参数调整走 PostProcessComponent 快照
```

`IModule::RegisterRenderPasses()` 已有 C++ 扩展点（`engine/core/module.h`），路径 B 是它的 Lua 等价层。

---

### 路径 B-1：内置 Pass 的重排与裁剪（最小工作量，约 3 天）

只允许 Lua 选择哪些内置 Pass 参与，不涉及自定义 RT 分配。

> **⚠️ 重要约束**：Lua 声明的列表顺序决定 Pass 的**注册顺序**，
> 但最终执行顺序由 RenderGraph DAG 的拓扑排序（资源读写依赖）决定。
> 引擎会忽略违反依赖关系的用户顺序并静默纠正。用户应理解这一点。

> **⚠️ Pass 名称设计**：列表中的字符串是**稳定逻辑名**，由 C++ 映射表维护，
> 不等于内部类名（如 `"forward"` 映射到 `ForwardScenePass`）。
> 内部类改名不影响 Lua 脚本。稳定逻辑名一旦发布不可更改。

#### Lua API

```lua
-- 声明当前场景使用的 Pass 列表（只列出需要的，未列出的不实例化）
render.configure_pipeline({
    passes = {
        render.PASS_SHADOW_CSM,       -- 稳定逻辑名常量（避免字符串拼写错误）
        render.PASS_SHADOW_SPOT,
        render.PASS_SHADOW_POINT,
        render.PASS_PREZ,
        render.PASS_GPU_CULL,
        render.PASS_GBUFFER,
        render.PASS_DEFERRED_LIGHTING,
        render.PASS_FORWARD,
        render.PASS_BLOOM,            -- 去掉 SSAO、SSR、DOF，只保留 Bloom
        render.PASS_AUTO_EXPOSURE,
        render.PASS_COMPOSITE,
        render.PASS_FXAA,
        render.PASS_UI,
    }
})
```

未列出的内置 Pass（SSAO/SSR/DOF/MotionBlur 等）**直接不实例化**，比 Execute() early-return 更彻底（但 RT 内存仍预分配，见下方说明）。

#### C++ 实现要点

```cpp
// 新增：稳定逻辑名映射表（frame_pipeline.cpp）
static const std::unordered_map<std::string, PassFactory> kBuiltinPassFactories = {
    {"shadow_csm",        [](auto& ctx){ return std::make_unique<CSMShadowPass>(ctx); }},
    {"shadow_spot",       [](auto& ctx){ return std::make_unique<SpotShadowPass>(ctx); }},
    {"forward",           [](auto& ctx){ return std::make_unique<ForwardScenePass>(ctx); }},
    // ... 逻辑名 → 工厂函数，与 C++ 类名解耦
};

// frame_pipeline.cpp：BuildRenderGraphInternal() 加条件分支
void FramePipeline::BuildRenderGraphInternal() {
    render_graph_dag_.Reset();
    registered_passes_.clear();

    if (pipeline_config_.enabled_passes.empty()) {
        BuildDefaultPasses();   // 走原有默认路径，完全向后兼容
        return;
    }
    for (const auto& name : pipeline_config_.enabled_passes) {
        auto it = kBuiltinPassFactories.find(name);
        if (it != kBuiltinPassFactories.end())
            registered_passes_.push_back(it->second(render_pass_context_));
    }
}
```

**改动范围**：
- `frame_pipeline.h`：加 `RenderPipelineConfig pipeline_config_` 成员
- `frame_pipeline.cpp`：加 `kBuiltinPassFactories` 映射表，`BuildRenderGraphInternal()` 加条件分支
- `lua_binding_render_pipeline.cpp`（新增 ~80 行）：`render.configure_pipeline()` + `render.PASS_*` 常量

**不需要改 RT 分配**：RT 仍全部预分配，只是部分 Pass 不被实例化。如需节省 RT 显存，需配合 Path B-2 的自定义 RT 分配机制。

---

### 路径 B-2：自定义 RT + 完整 Lua 管线（约 3-4 周）

允许 Lua 声明自己的 RT，并自由组合内置 Pass 和 DSSL 自定义 Pass。

> **关于"完整 SRP"**：Path B-2 实现后约覆盖 SRP 的 55-60%。
> 不支持的能力：自定义几何 Pass（需 C++ `IModule`）、每帧动态插入 Pass（游戏实践中几乎不需要）。

#### Lua API（完整版）

```lua
render.begin_pipeline()

-- 声明自定义 RT
-- 尺寸用相对值（1.0 = 屏幕全尺寸），窗口 resize 时引擎自动按比例重建
local rt_scene_hdr  = render.create_rt("scene_hdr",  "rgba16f", 1.0, 1.0)
local rt_depth      = render.create_rt("depth",      "d32f",    1.0, 1.0)
local rt_bloom      = render.create_rt("bloom_out",  "rgba16f", 0.5, 0.5)
local rt_outline    = render.create_rt("my_outline", "rgba8",   1.0, 1.0)

-- 添加内置 Pass（可覆盖默认 RT 绑定）
render.add_builtin_pass(render.PASS_SHADOW_CSM)
render.add_builtin_pass(render.PASS_PREZ,    { depth_rt = rt_depth })
render.add_builtin_pass(render.PASS_FORWARD, { color_rt = rt_scene_hdr, depth_rt = rt_depth })
render.add_builtin_pass(render.PASS_BLOOM,   { color_rt = rt_scene_hdr, output_rt = rt_bloom })

-- 添加 DSSL 自定义 Pass（slot-based，与 PostEffect Hook 保持一致）
local outline_mat = dssl.load_material("effects/outline.dssl")
dssl.set_float(outline_mat, "threshold", 0.3)
render.add_custom_pass("my_outline", {
    after      = render.PASS_FORWARD,   -- 位置（内部转换为 DAG 依赖边）
    shader     = outline_mat,
    input      = rt_scene_hdr,          -- 注入为 SCREEN_TEXTURE
    output     = rt_outline,            -- 写入目标
    also_reads = { rt_depth },          -- 可选额外输入（如深度）
})

render.add_builtin_pass(render.PASS_COMPOSITE, {
    scene_rt   = rt_scene_hdr,
    bloom_rt   = rt_bloom,
    overlay_rt = rt_outline,
})
render.add_builtin_pass(render.PASS_UI)

render.end_pipeline()  -- 提交 descriptor，下一帧由渲染线程编译 DAG，之后不再调用 Lua
```

#### 数据结构设计

```cpp
// engine/render/render_pipeline_descriptor.h（新增）
struct RTDesc {
    std::string name;
    std::string format;          // "rgba16f" | "rgba8" | "d32f" | ...
    float scale_w = 1.0f;        // 相对屏幕宽度（1.0 = 100%），resize 时自动重建
    float scale_h = 1.0f;
    unsigned int handle = 0;     // 运行时填充，build 后只读
};

struct CustomPassDesc {
    std::string name;
    int dssl_mat_id = -1;
    std::string after_pass;      // 逻辑名，内部转换为 DAG 依赖边
    std::string input_rt;        // 注入为 SCREEN_TEXTURE
    std::string output_rt;       // 写入目标
    std::vector<std::string> also_reads;  // 额外只读 RT（如深度）
};

struct BuiltinPassDesc {
    std::string pass_name;       // 稳定逻辑名
    std::unordered_map<std::string, std::string> rt_overrides;  // "color_rt" → RT 名称
};

struct RenderPipelineDescriptor {
    std::vector<RTDesc>          custom_rts;
    std::vector<BuiltinPassDesc> pass_sequence;
    std::vector<CustomPassDesc>  custom_passes;
    bool is_lua_configured = false;
};
```

#### RT Override 实现方式

内置 Pass 直接访问 `ctx_.render_targets.scene` 等 typed struct 字段（共 ~20 个 Pass，~40-50 处访问）。
支持 override 的最小侵入改法：在 `RenderPassContext` 加 helper，各 Pass 改为一行：

```cpp
// RenderPassContext 新增（render_pass_context.h）：
std::unordered_map<std::string, unsigned int> rt_overrides;
unsigned int GetRT(const char* name, unsigned int default_val) const {
    auto it = rt_overrides.find(name);
    return (it != rt_overrides.end()) ? it->second : default_val;
}

// 各 Pass 从（以 ForwardScenePass 为例）：
cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, bg_color, true});
// 改为：
cmd_buffer.BeginRenderPass({ctx_.GetRT("scene", ctx_.render_targets.scene), bg_color, true});
```

改动是纯机械的，每处一行，不影响逻辑。`BuildRenderGraphFromDescriptor()` 在实例化每个 Pass 之前，
将该 Pass 的 `rt_overrides` 写入 `render_pass_context_`（build 期，主线程安全）。

#### 窗口 Resize 处理

`RTDesc` 存相对比例（`scale_w`, `scale_h`）而非绝对像素。`FramePipeline::Resize()` 改为：

```cpp
void FramePipeline::Resize(int w, int h) {
    // 1. 重建默认 RT（现有逻辑不变）
    ResizeDefaultRTs(w, h);
    // 2. 重建 descriptor 中的自定义 RT
    for (auto& rt : pipeline_descriptor_.custom_rts) {
        rhi_device->DestroyRenderTarget(rt.handle);
        rt.handle = rhi_device->CreateRenderTarget(
            {(int)(w * rt.scale_w), (int)(h * rt.scale_h), ...});
    }
}
```

不需要重新 `end_pipeline()` 或重建 DAG，只重建 RT handle。

#### 关键实现路径

```
render.end_pipeline()（主线程）
    │ 设置 pipeline_descriptor_.is_lua_configured = true
    ▼
渲染线程帧首（BuildRenderGraphInternal 检查 dirty flag）
    ├── 1. 按 RTDesc 分配 custom_rts（scale × screen_size）
    ├── 2. 按 pass_sequence 实例化内置 Pass
    │      └── 写入 rt_overrides → render_pass_context_
    ├── 3. 按 CustomPassDesc 实例化 LuaCustomPass（DSSL shader + RT 绑定）
    └── 4. Setup() → 编译 DAG

每帧（渲染线程）：Execute() 按 DAG 顺序，完全无 Lua 调用
每帧（主线程）：dssl.set_float() 等参数调整走 RenderThinSnapshot 双缓冲
```

#### 线程安全保证

- `RenderPipelineDescriptor` 由主线程写入（`end_pipeline()`），渲染线程在下一帧帧首读取并编译，
  两者通过已有的帧同步机制（mutex + condition variable）天然隔离
- RT handle 在 build 时分配，写入 descriptor 后只读
- `rt_overrides` 写入 `render_pass_context_` 发生在 build 阶段（渲染线程，帧首），Execute() 期间只读

#### 改动范围

| 文件 | 改动 | 工作量 |
|------|------|--------|
| `engine/render/render_pipeline_descriptor.h`（新增）| RTDesc / BuiltinPassDesc / CustomPassDesc / RenderPipelineDescriptor | ~60 行 |
| `engine/render/passes/render_pass_context.h` | 加 `rt_overrides` map + `GetRT()` helper | ~10 行 |
| `engine/render/passes/builtin_passes.cpp` | 全部 RT 访问改为 `GetRT()`（约 40-50 处，纯机械）| ~50 行 |
| `engine/runtime/frame_pipeline.h/.cpp` | 加 `pipeline_descriptor_`，`BuildRenderGraphFromDescriptor()`，`Resize()` 扩展 | ~150 行 |
| `engine/scripting/lua/bindings/lua_binding_render_pipeline.cpp`（新增）| 完整 Lua API | ~150 行 |

---

### 路径选择矩阵

| 需求 | 现有方案 | 路径 B-1 | 路径 B-2 |
|------|---------|---------|---------|
| 内置 Pass enable/disable + params | ✅ | ✅ | ✅ |
| 自定义全屏后处理 DSSL | ✅（PostEffect Hook）| ✅ | ✅ |
| 彻底移除 Pass（不占 Setup 开销）| ❌ | ✅ | ✅ |
| 自定义 RT 格式/尺寸 | ❌ | ❌ | ✅ |
| Render to Texture | ❌ | ❌ | ✅ |
| 多 Camera 不同管线 | ❌ | ❌ | ✅（需额外工作）|
| 自定义几何 Pass（从 Lua 发 draw call）| ❌ | ❌ | ❌（需 C++ IModule）|
| **实施工作量** | **1 天** | **3 天** | **2-3 周** |

---

### 实施建议

```
第一步（现在）：实施 PostEffect Hook ──────── 1 天，解决自定义后处理
第二步（按需）：实施路径 B-1 ─────────────── 3 天，解决 Pass 裁剪需求
第三步（按需）：实施路径 B-2 ─────────────── 3-4 周，解决 RT 自定义和 RtT
```

路径 B-1 和 B-2 有清晰的前后依赖关系，可以分阶段推进，  
每一步都是对已有系统的**加法**，不修改已有 PostProcessComponent + 快照机制。

---

## 七、不做什么（明确边界）

- **不新增 PassRegistry 类**：与 PostProcessComponent 重复，是技术债
- **不实现 LuaRenderPass（Execute 内回调 Lua）**：渲染线程调用 Lua 是数据竞争
- **不在 Setup() 里做 Pass Culling**：Init() 时 snapshot 为 null，RT 分配与 DAG 解耦，改动不成比例
- **不改 Lua 的 set_post_process_* API**：现有 API 覆盖完整，保持稳定
- **不支持从 Lua 发 draw call（几何 Pass）**：需用 C++ `IModule::RegisterRenderPasses()`

---

## 八、关键文件索引

| 文件 | 用途 |
|------|------|
| `engine/ecs/components_3d.h` | `PostProcessComponent` + 新增 `PostEffectEntry` |
| `engine/render/render_snapshot.h` | `RenderThinSnapshot::post_process` 快照副本 |
| `engine/render/passes/builtin_passes.cpp` | 4 个 Hook 点实现位置 |
| `engine/render/material/dssl_material_instance.h/.cpp` | DSSL uniform 存储 + shader 绑定（已有）|
| `engine/render/material/dssl_material_loader.h/.cpp` | `.dssl` 加载 + 编译缓存（已有）|
| `engine/scripting/lua/bindings/lua_binding_dssl.cpp` | DSSL Lua API（已有：`dssl.load_material/set_float/...`）|
| `engine/scripting/lua/bindings/lua_binding_ecs_rendering.cpp` | 新增 PostEffect Lua 绑定 |
| `engine/runtime/frame_pipeline.cpp` | `CaptureThinSnapshot()` / `BuildRenderGraphInternal()` |
| `docs/architecture/SHADER_SYSTEM.md` | DSSL 完整规格文档 |
| `engine/core/module.h` | `IModule::RegisterRenderPasses()`（C++ 扩展点，路径 B 的参考原型）|
| `engine/render/render_pipeline_descriptor.h`（路径 B-2 新增）| `RTDesc` / `RenderPipelineDescriptor` |
| `engine/scripting/lua/bindings/lua_binding_render_pipeline.cpp`（路径 B 新增）| `render.configure_pipeline()` / `render.begin_pipeline()` 等 |

---

## 九、三者的层级关系

DSSL、PostEffect Hook、路径 B 处于不同抽象层级，不是替代关系：

```
┌──────────────────────────────────────────────────────┐
│                      DSSL                            │  ← 语言层
│  shader_type postprocess / surface / unlit / ...     │
│  描述"shader 写什么"，PostEffect Hook 和路径 B 都用它  │
└──────────────────────────────────────────────────────┘
            ↑ 两者的 shader 来源均为 DSSL
┌────────────────────────┐  ┌────────────────────────┐
│    PostEffect Hook     │  │       路径 B            │  ← 机制层
│  在固定管线的预定义槽位 │  │  配置/替换管线本身       │
│  插入全屏 DSSL 效果     │  │  B-1: 过滤内置 Pass     │
│                        │  │  B-2: 自定义 RT + Pass  │
└────────────────────────┘  └────────────────────────┘
```

**PostEffect Hook ⊂ 路径 B-2**：路径 B-2 的 `add_custom_pass` 是 PostEffect Hook 的超集——
能做 Hook 做的一切，还能指定任意 RT。但 PostEffect Hook 实现更简单，在不需要自定义管线时是首选。

**推荐使用场景**：

| 需求 | 推荐路径 |
|------|---------|
| 加 scanlines、夜视仪等屏幕效果 | PostEffect Hook + DSSL |
| 移动端优化，去掉 SSAO/DOF 等 Pass | 路径 B-1 |
| Render to Texture（镜子、小地图）| 路径 B-2 |
| 完全自定义渲染逻辑（自定义几何 Pass）| C++ `IModule::RegisterRenderPasses()` |

---

## 十、横向对比：与主流引擎的差距

### 全部实施后的能力覆盖

```
脚本可编程程度（0% = 固定管线，100% = 完整 SRP）

DSEngine 现状（仅 DSSL 材质）  ████░░░░░░░░░░░░░░  ~15%
DSEngine 全部实施后             ████████████░░░░░░  ~55-60%
Godot 4 RenderingServer        █████████████░░░░░  ~60-65%
Unity URP (ScriptableRenderPass)  ████████████████████  ~90%
```

### 与各引擎对比

| 能力 | DSEngine 全部实施后 | Godot 4 | Unity URP |
|------|-------------------|---------|-----------|
| 材质着色（DSSL / ShaderMaterial / ShaderLab）| ✅ | ✅ | ✅ |
| 全屏后处理效果 | ✅ | ✅ | ✅ |
| 内置 Pass 过滤/裁剪 | ✅ | ✅ | ✅ |
| 自定义 RT 格式/尺寸 | ✅ | ✅ | ✅ |
| Render to Texture | ✅ | ✅ | ✅ |
| **从脚本渲染几何体子集** | ❌ | 部分 | ✅ |
| Per-Camera 不同管线 | ❌ | ✅ | ✅ |
| 自定义 Compute Pass | ❌ | ❌ | ✅ |
| 自定义阴影 Caster | ❌ | ❌ | ✅ |

### 剩余差距（若要继续追平）

| 能力 | 估算工作量 | 实际需求频率 |
|------|----------|------------|
| `render.draw_scene()`（脚本渲染几何体，用于描边/高亮）| ~4 周 | 中 |
| Per-Camera pipeline descriptor | ~2 周 | 低 |
| 脚本 Compute Pass | ~2-3 周 | 低 |

**结论**：全部实施后，DSEngine 的 Lua 可编程性达到 **Godot 4 水平**，
覆盖典型独立游戏项目约 **90% 的实际渲染需求**。
与 Unity URP 的主要差距集中在"从脚本控制几何体渲染"——  
该能力在独立游戏中需求较低，且可通过 C++ `IModule` 补足，按需追加即可。
