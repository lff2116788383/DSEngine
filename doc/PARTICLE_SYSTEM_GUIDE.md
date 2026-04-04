# 粒子系统使用指南

## 概述

当前 DSEngine 的 2D 粒子系统支持以下能力：

- 基础持续发射与 Burst 发射
- 随机速度、生命周期、尺寸、旋转、角速度
- 生命周期曲线驱动（尺寸 / 透明度 / 速度缩放）
- 颜色渐变
- 重力影响
- 统一碰撞语义：`None / GroundPlane / Box2D(预留)`

当前实现重点是 **P0 可交付版本**：

- 已完成结构化 `ParticleCurve`
- 已兼容旧字段 `use_size_curve` / `use_alpha_curve` / `use_speed_curve`
- 已支持简易地面碰撞
- 已为后续 Box2D 正式集成预留 `ParticleCollisionMode::Box2D`

---

## 相关类型

头文件位置：

```cpp
#include "engine/ecs/components_2d.h"
#include "modules/gameplay_2d/particle/particle_system.h"
```

核心类型：

- `ParticleEmitterComponent`
- `Particle2D`
- `ParticleCurve`
- `ParticleCurveType`
- `ParticleCollisionMode`
- `ParticleSystem`

---

## 快速开始

### 1. 创建粒子发射器

```cpp
World world;
ParticleSystem particle_system;

Entity entity = world.CreateEntity();
auto& transform = world.registry().emplace<TransformComponent>(entity);
auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);

transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
emitter.emit_rate = 20.0f;
emitter.max_particles = 200;
emitter.start_life_time = 2.0f;
emitter.start_size = 1.0f;
emitter.start_color = glm::vec4(1.0f, 0.8f, 0.3f, 1.0f);
```

### 2. 每帧更新

```cpp
float dt = 1.0f / 60.0f;
particle_system.Update(world, dt);
```

### 3. 渲染

```cpp
particle_system.Render(world, cmd_buffer);
```

---

## 发射模式

### 持续发射

```cpp
emitter.emitting = true;
emitter.emit_rate = 30.0f;
```

系统会根据 `emit_rate` 自动按时间累积发射。

### Burst 发射

```cpp
emitter.emitting = false;
emitter.pending_burst = 16;
```

适合：

- 爆炸
- 命中特效
- 一次性法术特效

---

## 随机参数

如果希望每个粒子有不同初值，可开启随机参数：

```cpp
emitter.use_random_params = true;

emitter.velocity_min = glm::vec3(-2.0f, 3.0f, 0.0f);
emitter.velocity_max = glm::vec3( 2.0f, 8.0f, 0.0f);

emitter.life_time_min = 0.8f;
emitter.life_time_max = 1.5f;

emitter.size_min = 0.5f;
emitter.size_max = 1.4f;

emitter.rotation_min = 0.0f;
emitter.rotation_max = 6.2832f;

emitter.angular_velocity_min = -2.0f;
emitter.angular_velocity_max = 2.0f;
```

当前随机项包括：

- `velocity_min/max`
- `life_time_min/max`
- `size_min/max`
- `rotation_min/max`
- `angular_velocity_min/max`

---

## 生命周期曲线（ParticleCurve）

### 曲线类型

`ParticleCurveType` 当前支持：

- `Linear`
- `EaseIn`
- `EaseOut`
- `EaseInOut`

### 配置尺寸曲线

```cpp
emitter.size_curve.enabled = true;
emitter.size_curve.type = ParticleCurveType::EaseOut;
emitter.size_curve.start_value = 1.5f;
emitter.size_curve.end_value = 0.0f;
```

效果：粒子从较大尺寸逐步缩小。

### 配置透明度曲线

```cpp
emitter.alpha_curve.enabled = true;
emitter.alpha_curve.type = ParticleCurveType::Linear;
emitter.alpha_curve.start_value = 1.0f;
emitter.alpha_curve.end_value = 0.0f;
```

效果：粒子在生命周期内逐渐淡出。

### 配置速度缩放曲线

```cpp
emitter.speed_curve.enabled = true;
emitter.speed_curve.type = ParticleCurveType::EaseInOut;
emitter.speed_curve.start_value = 1.0f;
emitter.speed_curve.end_value = 0.2f;
```

效果：粒子速度随生命周期逐渐衰减。

---

## 兼容旧字段

为避免影响现有逻辑，以下旧字段仍然可用：

- `use_size_curve + size_curve_end`
- `use_alpha_curve + alpha_curve_end`
- `use_speed_curve + speed_curve_end_scale`

例如：

```cpp
emitter.use_size_curve = true;
emitter.size_curve_end = 0.0f;
```

但新代码建议统一迁移到结构化曲线：

```cpp
emitter.size_curve.enabled = true;
emitter.size_curve.start_value = 1.0f;
emitter.size_curve.end_value = 0.0f;
```

优先级规则：

- 若 `size_curve.enabled == true`，优先使用新曲线
- 否则回退到旧字段逻辑

`alpha_curve` 与 `speed_curve` 同理。

---

## 颜色渐变

颜色曲线当前仍采用起点到终点的线性插值：

```cpp
emitter.use_color_curve = true;
emitter.start_color = glm::vec4(1.0f, 1.0f, 0.3f, 1.0f);
emitter.color_curve_end = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f);
```

这适合：

- 火焰由亮黄过渡到暗红
- 烟雾由深色过渡到透明

后续如果需要，也可以把颜色曲线进一步结构化。

---

## 重力

```cpp
emitter.gravity = glm::vec3(0.0f, -9.8f, 0.0f);
```

常见用法：

- 火花下坠
- 碎片掉落
- 雨雪重力效果

---

## 碰撞模式

### 1. 不启用碰撞

```cpp
emitter.collision_mode = ParticleCollisionMode::None;
```

### 2. 简易地面碰撞

```cpp
emitter.enable_collision = true;
emitter.collision_mode = ParticleCollisionMode::GroundPlane;
emitter.ground_y = 0.0f;
emitter.collision_bounce = 0.5f;
emitter.collision_friction = 0.1f;
emitter.collision_life_loss = 0.2f;
```

说明：

- 当粒子 `position.y <= ground_y` 时触发碰撞
- `collision_bounce` 控制 Y 方向反弹强度
- `collision_friction` 衰减 X/Z 方向速度
- `collision_life_loss` 可用于碰撞后快速消亡

### 3. Box2D（预留）

```cpp
emitter.collision_mode = ParticleCollisionMode::Box2D;
```

当前版本已支持 **最小可用 Box2D 世界碰撞检测**，通过 `Physics2DSystem::Raycast(...)` 检测粒子轨迹与静态/动态碰撞体的命中关系，并基于法线做反射响应。当前能力包括：

- 粒子路径线段与 Box2D 碰撞体命中检测
- 根据法线反射速度
- `collision_bounce` 控制反弹强度
- `collision_friction` 控制切向衰减
- `collision_life_loss` 控制命中后生命损耗

后续仍可继续增强为：

- 射线或采样检测最近碰撞体
- 根据法线方向反射粒子速度
- 可选吸收 / 穿透 / 反弹策略

使用方式示例：

```cpp
Physics2DSystem physics_system;
physics_system.Init(world);

emitter.collision_mode = ParticleCollisionMode::Box2D;
emitter.collision_bounce = 0.8f;
emitter.collision_friction = 0.15f;
emitter.collision_life_loss = 0.2f;

particle_system.Update(world, dt, &physics_system);
```

---

## 推荐配置示例

### 1. 爆炸火花

```cpp
emitter.emitting = false;
emitter.pending_burst = 24;
emitter.use_random_params = true;
emitter.velocity_min = glm::vec3(-8.0f, 2.0f, 0.0f);
emitter.velocity_max = glm::vec3( 8.0f, 8.0f, 0.0f);
emitter.life_time_min = 0.4f;
emitter.life_time_max = 1.0f;
emitter.size_min = 0.2f;
emitter.size_max = 0.8f;
emitter.start_color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f);

emitter.alpha_curve.enabled = true;
emitter.alpha_curve.type = ParticleCurveType::EaseOut;
emitter.alpha_curve.start_value = 1.0f;
emitter.alpha_curve.end_value = 0.0f;

emitter.gravity = glm::vec3(0.0f, -12.0f, 0.0f);
```

### 2. 烟雾

```cpp
emitter.emitting = true;
emitter.emit_rate = 12.0f;
emitter.use_random_params = true;
emitter.velocity_min = glm::vec3(-0.2f, 0.4f, 0.0f);
emitter.velocity_max = glm::vec3( 0.2f, 1.0f, 0.0f);
emitter.life_time_min = 1.5f;
emitter.life_time_max = 3.0f;
emitter.start_color = glm::vec4(0.4f, 0.4f, 0.4f, 0.8f);

emitter.size_curve.enabled = true;
emitter.size_curve.type = ParticleCurveType::EaseOut;
emitter.size_curve.start_value = 0.4f;
emitter.size_curve.end_value = 1.6f;

emitter.alpha_curve.enabled = true;
emitter.alpha_curve.type = ParticleCurveType::Linear;
emitter.alpha_curve.start_value = 0.8f;
emitter.alpha_curve.end_value = 0.0f;
```

### 3. 地面弹跳碎片

```cpp
emitter.pending_burst = 12;
emitter.emitting = false;
emitter.use_random_params = true;
emitter.velocity_min = glm::vec3(-3.0f, 4.0f, 0.0f);
emitter.velocity_max = glm::vec3( 3.0f, 8.0f, 0.0f);
emitter.gravity = glm::vec3(0.0f, -18.0f, 0.0f);

emitter.enable_collision = true;
emitter.collision_mode = ParticleCollisionMode::GroundPlane;
emitter.ground_y = 0.0f;
emitter.collision_bounce = 0.35f;
emitter.collision_friction = 0.25f;
emitter.collision_life_loss = 0.3f;
```

---

## 调试建议

如果你发现粒子表现不对，可以优先检查：

1. `emit_rate` 是否过低
2. `max_particles` 是否太小
3. `start_life_time` / `life_time_min/max` 是否过短
4. `size_curve.start_value` 是否被设置得过小
5. `alpha_curve.end_value` 是否过早变成 0
6. `ground_y` 是否高于粒子生成位置
7. `collision_life_loss` 是否过大导致粒子碰撞后立刻死亡

---

## 当前测试覆盖

已覆盖以下内容：

- 随机速度差异
- 随机生命周期范围
- 重力影响
- 地面碰撞反弹
- 碰撞生命损失
- 旧尺寸曲线 / 旧透明度曲线
- 新 `ParticleCurve` 评估
- 新结构化尺寸曲线 / 透明度曲线
- 统一碰撞模式 `GroundPlane`
- 角速度旋转

对应测试文件：

```text
tests/modules/gameplay_2d/particle/particle_system_test.cpp
tests/modules/gameplay_2d/particle/particle_advanced_test.cpp
```

---

## 后续演进建议

下一步建议继续补完：

1. `ParticleCollisionMode::Box2D` 正式接入
2. 颜色曲线结构化
3. 编辑器粒子参数面板
4. 粒子预设资源化
5. 粒子与 Profiler 联动，统计发射量与存活量
