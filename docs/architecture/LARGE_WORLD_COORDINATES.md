# 大世界场景坐标方案

## 现状分析

| 组件 | 精度 | 关键文件 |
|------|------|---------|
| TransformComponent::position | `glm::vec3` (float) | `engine/ecs/transform.h` |
| local_to_world | `glm::mat4` (float) | 同上 |
| Camera position | `glm::vec3` (float) | `engine/render/render_snapshot.h` |
| View matrix | `glm::lookAt` (float) | `engine/runtime/frame_pipeline.cpp` |
| Physics (PhysX/Jolt) | float | `engine/physics/physics3d/` |
| GPU Driven instance buffer | float | builtin_passes + SSBO |

当前全链路 float32，在距原点 ~10km 处精度约 1mm，~100km 处退化到 ~1cm，超过这个范围会出现明显的顶点抖动和物理穿模。

---

## 方案选型

| 方案 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| **Camera-Relative + Floating Origin** | 全链路 float 不改类型，性能友好，GPU 无额外开销 | rebase 帧微卡，所有系统需响应 rebase 事件 | DSEngine 当前架构 ✓ |
| dvec3 + Camera-Relative | 无 rebase 抖动，坐标直觉清晰 | 大量 double→float 转换边界，内存翻倍，Physics 仍是 float | 纯渲染引擎 |
| 纯 Floating Origin（无 Camera-Relative） | 实现简单 | GPU 精度问题未解决 | 不推荐 |

**选定方案：Camera-Relative Rendering + Floating Origin**

理由：
- Physics (PhysX/Jolt) 内部是 float，dvec3 存了也得截断
- GPU buffer 全是 float，dvec3 只增加转换复杂度
- Floating Origin 保证"所有坐标始终在原点附近"这个不变量，验证简单
- 仅在序列化和 streaming LOD 判断等少数场景使用 `dvec3` 绝对坐标

---

## 坐标空间约定与类型安全

为防止后续开发混用坐标空间，引入显式类型区分：

```cpp
// engine/core/world_types.h

/// 绝对世界坐标（double），用于序列化、streaming、origin 累加
struct AbsoluteWorldPos {
    glm::dvec3 value{0.0};
};

/// 近原点世界坐标（float），ECS TransformComponent 使用
/// Floating Origin 保证此值始终在原点 ±threshold 范围内
struct LocalWorldPos {
    glm::vec3 value{0.0f};
};

/// 相机相对坐标（float），仅用于传给 GPU 的 model matrix
struct CameraRelativePos {
    glm::vec3 value{0.0f};
};

/// 转换：local world → camera relative
inline CameraRelativePos ToCameraRelative(LocalWorldPos pos, LocalWorldPos camera) {
    return {pos.value - camera.value};
}
```

编译期区分可防止意外混用，且无运行时开销。

---

## 关键技术点

### 1. Camera-Relative Rendering（渲染侧）

**核心思路**：GPU 永远以相机为原点渲染，model matrix 在提交前减去相机世界坐标。

```cpp
// RenderSnapshot 中
struct Camera3D {
    bool valid = false;
    float fov = 60.0f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    glm::vec3 position{0.0f};       // 近原点坐标（Floating Origin 后）
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::mat4 view{1.0f};           // lookAt(vec3(0), forward, up) — 相机在原点
};

// 提交 model matrix 时
glm::vec3 camera_offset = snapshot.camera_3d.position;
glm::mat4 model_relative = entity.local_to_world;
model_relative[3] -= glm::vec4(camera_offset, 0.0f);  // 平移部分减去相机
cmd.SetModelMatrix(model_relative);
```

View matrix 构建改为：
```cpp
// 相机始终在原点，只有朝向
glm::mat4 view = glm::lookAt(glm::vec3(0.0f), forward, up);
```

### 2. Shadow Map Camera-Relative 处理

CSM / Spot Shadow 的 light view-projection 必须也在 camera-relative 空间构建：

```cpp
// CSM light matrix（camera-relative 空间）
glm::vec3 shadow_center_relative = snap.camera_3d.shadow_center - camera_offset;
// light view matrix 基于 relative center
glm::mat4 light_view = glm::lookAt(
    shadow_center_relative - light_dir * distance,
    shadow_center_relative,
    up);
// shadow pass 中的 model matrix 已经是 camera-relative，无需额外处理
```

点光源/聚光灯位置同理：
```cpp
glm::vec3 spot_pos_relative = spot_light.position - camera_offset;
```

### 3. Floating Origin System（物理 + ECS）

当相机远离原点超过阈值时，整体平移所有实体和物理 body：

```cpp
class FloatingOriginSystem {
    glm::dvec3 accumulated_origin_{0.0};  // 累积偏移（绝对坐标 = local + accumulated）
    float rebase_threshold_ = 5000.0f;    // 可配置，默认 5km

public:
    void Tick(World& world, IPhysics3DSystem* physics) {
        glm::vec3 cam_pos = GetMainCameraPosition(world);
        if (glm::length(cam_pos) < rebase_threshold_) return;

        glm::vec3 offset = cam_pos;  // rebase 量 = 当前相机位置
        accumulated_origin_ += glm::dvec3(offset);

        // 1. ECS 所有 TransformComponent
        for (auto& [e, t] : world.registry().view<TransformComponent>().each()) {
            t.position -= offset;
            t.dirty = true;
        }

        // 2. Physics
        physics->RebaseOrigin(offset);

        // 3. 广播事件，其他子系统响应
        world.event_bus().Publish(OriginRebasedEvent{offset});
    }

    /// 本地坐标 → 绝对坐标（序列化用）
    glm::dvec3 ToAbsolute(glm::vec3 local) const {
        return accumulated_origin_ + glm::dvec3(local);
    }
};
```

### 4. Physics Backend 实现差异

| 后端 | Origin Rebase 支持 | 实现方式 |
|------|-------------------|---------|
| **PhysX** | ✓ 原生 API | `PxScene::shiftOrigin(PxVec3(-offset))` |
| **Jolt** | ✗ 无原生 API | 需手动遍历所有 Body 减去 offset，会触发 broadphase 重建 |

Jolt rebase 实现注意事项：
- 大量 body 时分帧处理（如每帧 rebase 1000 个 body）
- 或在 loading/低负载帧执行
- 设较大阈值（5-10km）降低 rebase 频率
- Character Controller 的 ground position 也需偏移

```cpp
// Jolt backend RebaseOrigin 实现
void Physics3DSystemJolt::RebaseOrigin(glm::vec3 offset) {
    JPH::Vec3 jolt_offset(offset.x, offset.y, offset.z);
    JPH::BodyInterface& body_iface = physics_system_->GetBodyInterface();
    // 遍历所有 active + inactive bodies
    for (auto& body_id : tracked_bodies_) {
        JPH::Vec3 pos = body_iface.GetPosition(body_id);
        body_iface.SetPosition(body_id, pos - jolt_offset, JPH::EActivation::DontActivate);
    }
    // Character controllers
    for (auto& [entity, character] : characters_) {
        character->SetPosition(character->GetPosition() - jolt_offset);
    }
}
```

### 5. Shader 侧（无改动）

顶点着色器已经是 `projection * view * model * vertex`，只要 CPU 侧 model 是 camera-relative 值即可。所有 shader 无需修改。

---

## 受影响子系统清单

| 子系统 | 需要响应 Rebase | 需要 Camera-Relative | 改动量 |
|--------|----------------|---------------------|--------|
| TransformSystem | ✓ (dirty flag) | - | 无（dirty 自动触发） |
| mesh_render_system | - | ✓ model matrix | 小 |
| terrain_system | - | ✓ model matrix | 小 |
| grass_system | - | ✓ instance position | 小 |
| hair_system | - | ✓ model matrix | 小 |
| lod_system | ✓ 距离判断 | - | 小 |
| GPU Driven (SSBO) | - | ✓ instance matrix | 中 |
| CSM Shadow | - | ✓ light VP matrix | 中 |
| Spot/Point Shadow | - | ✓ light position | 小 |
| 3D Audio | ✓ listener + source pos | - | 小 |
| NavMesh / AI | ✓ navmesh vertices | - | 中 |
| Particle System | ✓ world-space emitters | ✓ 渲染位置 | 中 |
| Streaming Manager | ✓ 用 AbsoluteWorldPos | - | 小 |
| Editor 属性面板 | 显示 AbsoluteWorldPos | - | 小 |
| 序列化 (.dscene) | 存 AbsoluteWorldPos | - | 小 |

---

## 改动范围（修正后）

| 层级 | 改动 | 文件数 | 难度 |
|------|------|--------|------|
| 类型定义 | 新增 `world_types.h` | 1 | 低 |
| RenderSnapshot | camera position 保持 float，view 改为 camera-at-origin | 1 | 低 |
| FramePipeline | view matrix 构建 + camera offset 传递 | 1 | 低 |
| builtin_passes | 所有 pass model matrix 减 camera offset + shadow light matrix | 1（多处） | 中 |
| modules/gameplay_3d | mesh_render, terrain, grass, hair, lod | 5 | 中 |
| GPU Driven | instance SSBO 写入时减 camera offset | 2-3 | 中 |
| FloatingOriginSystem | 新系统，事件广播 | 2-3 | 中 |
| Physics (PhysX) | 调用 shiftOrigin | 1 | 低 |
| Physics (Jolt) | 手动遍历 body 偏移 | 1 | 中 |
| Audio / NavMesh / Particles | 响应 OriginRebasedEvent | 3-5 | 低-中 |
| Lua bindings | 坐标接口兼容 | 2-3 | 低 |
| 序列化 | 存储/加载时转换 absolute ↔ local | 1-2 | 低 |

**总计约 20-28 个文件，核心逻辑集中在 6 个。**

---

## 实施阶段

### Phase 1：Camera-Relative Rendering ✅ 已完成

1. ✅ 新增 `engine/core/world_types.h`，定义 `AbsoluteWorldPos` / `LocalWorldPos` / `CameraRelativePos` 类型
2. ✅ `RenderSnapshot::Camera3D::view` 改为 `lookAt(vec3(0), forward, up)` camera-at-origin
3. ✅ `RenderScene::ApplyCameraOffset()` 集中处理所有 CPU mesh model matrix
4. ✅ Shadow pass（CSM / Spot / Point / RSM）light matrix 在 camera-relative 空间构建
5. ✅ `modules/gameplay_3d/rendering/` 下 terrain, grass model matrix 减 offset
6. ✅ Hair / Particle 3D 通过 `world_to_view = view * translate(-offset)` 方案适配
7. ✅ GPU Driven instance SSBO model matrix + Hi-Z AABB 减 camera offset
8. ✅ LightBuffer 光源位置减 offset，ClusterGrid 用 camera-at-origin view
9. ✅ Editor camera view matrix 反向补偿 `editor_view * translate(+offset)`
10. ✅ Decal position、Water water_level / cam_pos、Volumetric Fog height_offset / cam_pos 全部适配
11. ✅ PerFrame UBO camera_pos 三后端（GL/VK/DX11）自动从 `inverse(view)[3]` 提取 = vec3(0)

**验证**：Release + Debug 全量编译零错误，1896/1917 单元测试通过（3 个失败为动画/DX11 投影校正预存问题，与本改动无关）。

### Phase 2：Floating Origin System ✅ 已完成

1. ✅ `FloatingOriginSystem`（`engine/ecs/floating_origin_system.h/.cpp`），阈值可配置（默认 5km）
2. ✅ `OriginRebasedEvent` 定义（`event_bus.h`）+ EventId 注册（`event_id.h`）
3. ✅ `IPhysics3DSystem::RebaseOrigin()` 纯虚接口
4. ✅ PhysX 后端：`PxScene::shiftOrigin(PxVec3(-offset))`
5. ✅ Jolt 后端：遍历 `entity_to_body` + `character_virtuals` 减去 offset
6. ✅ FramePipeline 集成：在 `RunRuntimeFixedUpdateGraph` 中物理 `FixedUpdate` 前调用 `Tick`
7. ✅ `accumulated_origin_`（dvec3）累积绝对偏移，提供 `ToAbsolute()`/`ToLocal()` 转换

**验证**：Release + Debug 全量编译零错误，1896/1917 单元测试通过（同 Phase 1）。

### Phase 3：子系统适配（1-2 天）

1. 3D Audio listener/source 响应 rebase
2. NavMesh 顶点偏移（或重新 bake）
3. Particle emitter 世界坐标偏移
4. Editor 属性面板显示绝对坐标
5. Streaming Manager 使用 `AbsoluteWorldPos` 做 LOD/chunk 判断

**验证**：完整场景功能回归。

---

## 风险与缓解

| 风险 | 严重性 | 缓解措施 |
|------|--------|---------|
| GPU Driven SSBO 忘记减 offset | 高 | Phase 1 末尾统一验证 indirect draw |
| Jolt rebase 大量 body 卡帧 | 中 | 分帧处理 + 大阈值 + 低负载帧执行 |
| Shadow map 偏移 | 中 | Phase 1 专项验证 CSM + Spot shadow |
| 三后端不一致 | 中 | GL/DX11/Vulkan 各跑一次远距离场景 |
| 子系统遗漏未 rebase | 低 | OriginRebasedEvent 强制订阅检查 |
| 序列化坐标混乱 | 低 | 文件头标记坐标系版本 |

---

## 不做的事（避免过度工程）

- ❌ 不改 `TransformComponent::position` 为 `dvec3` — Floating Origin 已保证近原点
- ❌ 不实现分区 Physics World — 复杂度过高，当前规模不需要
- ❌ 不改 shader 为 double — GPU double 性能极差且无必要
- ❌ 不做 64-bit vertex position — 同上

---

## 结论

| 问题 | 回答 |
|------|------|
| 可行吗？ | 完全可行，架构上没有阻碍 |
| 改动大吗？ | Phase 1 (渲染) 2 天 + Phase 2 (物理) 2-3 天 + Phase 3 (子系统) 1-2 天 ≈ **5-7 天** |
| 最佳方案？ | Camera-Relative + Floating Origin 是工业界标准，性价比最高 |
| 技术债？ | 通过类型安全（world_types.h）+ 事件机制（OriginRebasedEvent）避免 |
