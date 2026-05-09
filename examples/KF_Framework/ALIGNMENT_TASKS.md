# KF_Framework 源码对齐 — 后续任务指令

> **⚠️ 本文档基于 KF 源码与 DSE 实现的逐行对比生成，不依赖 PLAN.md（已过时）。**
> **日期**: 2026-05-10
> **KF 源码位置**: `C:\Users\Administrator\Desktop\temp_analysis\KF_Framework\source_code\`
> **DSE 项目位置**: `C:\Users\Administrator\Desktop\Engine\DSEngine\`
> **DSE 示例位置**: `examples\KF_Framework\`

---

## 已完成对齐清单

| # | 功能 | 对应 KF 源码 | DSE 文件 |
|---|------|-------------|----------|
| 1 | Title 画面 | `mode/mode_title.cpp` | `script/gameflow.lua` |
| 2 | Battle 画面 | `mode/mode_demo.cpp` | `script/gameflow.lua` |
| 3 | Result 画面 | `mode/mode_result.cpp` | `script/gameflow.lua` |
| 4 | Fade 系统 (5 状态完整状态机) | `fade_system.cpp` | `script/fade.lua` |
| 5 | Loading 动画 (2×15 sprite UV scroll) | `fade_system.cpp` Init | `script/fade.lua` |
| 6 | 玩家 HP 条 (Cover/Gauge/Warning) | `ui/player_ui_controller.cpp` | `script/hud.lua` |
| 7 | 操作提示按钮 (text + icon, V-flip UV) | `ui/player_ui_controller.cpp` | `script/hud.lua` |
| 8 | FlashButton alpha 振荡 | `ui/flash_button_controller.cpp` | `script/gameflow.lua` |
| 9 | ButtonController 颜色 lerp (0.1s) | `ui/button_controller.cpp` | `script/gameflow.lua` |
| 10 | 敌人 AI (idle/chase/attack/damaged/dead) | `actor_state/zombie/enemy/*.cpp` | `script/enemy.lua` |
| 11 | 第三人称摄像机 (rig/pivot) | `camera/third_person_camera.cpp` | `script/player.lua` |
| 12 | 风车旋转 (Fan RotateByRoll) | `other/windmill_controller.cpp` | `script/main.lua` |
| 13 | 球体碰撞推开 | `physics/collision_detector.cpp` | `script/main.lua` |
| 14 | 每敌人独立参数 (demo.enemy) | `game_object/stage_spawner.cpp` | `script/main.lua` Awake |
| 15 | 音效系统 (19 个 wav) | 各 state cpp 内 Play() | `script/audio.lua` |

---

## 待对齐任务 — 按优先级排序

---

### P1: ✅ 战斗中 Enter 跳过 (已完成)

**KF 源码**: `mode/mode_demo.cpp:103-109`
```cpp
if (main_system.GetInput().GetJoystick()->GetButtonTrigger(XboxButton::kXboxMenu)
    || main_system.GetInput().GetKeyboard()->GetTrigger(DIK_RETURN))
{
    main_system.GetSoundSystem().Play(kSubmitSe);
    time_counter_ = GameTime::kTimeInterval;  // ≈1帧后进入 Result
    return;
}
```

**DSE 当前**: `gameflow.lua` 的 `state == "battle"` 分支无跳过逻辑。

**修改方案**:
- 在 `gameflow.lua` 的 `elseif state == "battle"` 分支中添加:
```lua
-- Battle 中按 Enter/Esc 跳过 (KF: mode_demo.cpp:103-109)
if result_timer <= 0 then
    if app.get_key_down(257) or app.get_key_down(256) then  -- Enter / Escape
        Audio.play_se("submit")
        result_timer = 0.017  -- KF: GameTime::kTimeInterval ≈ 1帧
    end
end
```

---

### P2: ✅ FadeOut 时冻结游戏 (已基本对齐)

**KF 源码**: `fade_system.cpp:143`
```cpp
GameTime::Instance().SetTimeScale(0.0f);  // FadeOut 完成进入 WaitOut 时
```
`fade_system.cpp:130`:
```cpp
GameTime::Instance().SetTimeScale(1.0f);  // FadeIn 完成时恢复
```

**DSE 当前**: `main.lua:155` 仅在 `Fade.is_fading()` 时跳过玩家/敌人更新，但风车等仍在旋转。`gameflow.lua` 的 title/result 状态下也应冻结战斗逻辑。

**修改方案**: 当前 `main.lua:151-156` 的逻辑已经近似正确（非 battle 或 fading 时跳过），但需确认:
1. FadeOut 期间 title 按钮 lerp 仍在运行 — 这与 KF 一致（KF: TimeScale 影响 ScaledDeltaTime，但 DeltaTime 不受影响，fade 用 DeltaTime）
2. ✅ 当前实现已基本对齐，不需要大改

---

### P3: ✅ 敌人巡逻状态 walk (已完成)

**KF 源码**: `enemy_zombie_walk_state.cpp:33-51`
- zombie idle 超时后/follow 脱离后进入 walk 状态
- walk 状态: 向 `next_position_` 移动，到达后回到 idle
- `kMovementMultiplier = 0.1f` (walk state header, 与 follow 相同 ⚠️ 文档之前误写为 0.05)
- follow 脱离后不回 idle 而是回 walk (走回出生点附近)

**KF**: `enemy_zombie_idle_state.cpp:44-53` (被注释但 walk 仍被 follow 使用)
```cpp
// 注释掉的 idle→walk 随机巡逻:
// if (time_counter_ >= kWaitTime) {
//     next_position = Random::Range(born - patrol_range, born + patrol_range);
//     enemy.Change(MY_NEW EnemyZombieWalkState);
// }
```
但 `enemy_zombie_follow_state.cpp:49`:
```cpp
enemy.Change(MY_NEW EnemyZombieWalkState);  // 脱离追击后进 walk
```

**DSE 当前**: `enemy.lua:200-202` — 脱离追击后直接回 idle (站桩)

**修改方案**:
- 在 `enemy.lua` 中添加 `"return"` 状态 (对应 KF 的 walk)
- 脱离追击后 `state = "return"` 而非 `"idle"`
- return 状态: 向出生点移动，到达后切回 idle
```lua
elseif data.state == "return" then
    -- KF: EnemyZombieWalkState — 向出生点移动
    ecs.set_animator_3d_param_float(data.entity, "speed", 0.3)
    local dx = data.spawn_x - ex
    local dz = data.spawn_z - ez
    local dist_to_spawn = math.sqrt(dx*dx + dz*dz)
    if dist_to_spawn < 100 then  -- KF: kArriveDistance
        data.state = "idle"
        ecs.set_animator_3d_param_float(data.entity, "speed", 0)
    else
        dx, dz = dx/dist_to_spawn, dz/dist_to_spawn
        local spd = ENEMY.move_speed * 0.5  -- KF: kMovementMultiplier=0.05
        local new_x = ex + dx * spd * dt
        local new_z = ez + dz * spd * dt
        local new_y = TerrainHeight.get_height(new_x, new_z)
        ecs.set_transform_position(data.entity, new_x, new_y, new_z)
        local target_yaw = math.deg(math.atan(dx, dz))
        ecs.set_transform_rotation(data.entity, 0, target_yaw, 0)
    end
    -- return 途中被发现也进 chase
    if dist < data.warning_range then
        data.state = "chase"
        Audio.play_se("zombie_warning")
    end
end
```

- 还需添加 `idle→run` / `run→walk` 动画过渡 (mutant walk 动画)
- 敌人 FSM 需要 `walk` 状态: 目前已有 `ecs.add_animator_3d_state(e, "walk", ...)` 但未使用

---

### P4: ✅ 敌人 damaged 无敌时间 (已完成)

**KF 源码**: `enemy_zombie_damaged_state.h`
```cpp
static constexpr float kInvincibleTime = 0.3f;  // ⚠️ 实际源码是 0.3f, 不是 0.5f
```
`enemy_zombie_damaged_state.cpp:62-64`:
```cpp
void EnemyZombieDamagedState::OnDamaged(EnemyController& enemy, const float& damage) {
    if (time_counter_ <= kInvincibleTime) return;  // 无敌期间忽略伤害
```

**DSE 当前**: `enemy.lua:129-143` — `Enemy.damage()` 无无敌检查:
```lua
function Enemy.damage(data, amount)
    if data.state == "dead" then return end
    -- 缺少: if data.state == "damaged" then return end
```

**修改方案**:
```lua
function Enemy.damage(data, amount)
    if data.state == "dead" then return end
    if data.state == "damaged" and data.damaged_timer > 0 then return end  -- KF 无敌
```

---

### P5: ✅ 敌人 dying 延迟销毁 (已完成)

**KF 源码**: `enemy_zombie_dying_state.h`
```cpp
static constexpr float kWaitTime = 10.0f;  // ⚠️ 实际源码是 10.0f, 不是 5.0f
```
`enemy_zombie_dying_state.cpp:34-40`:
```cpp
void EnemyZombieDyingState::Update(EnemyController& enemy) {
    time_counter_ += GameTime::Instance().DeltaTime();
    if (time_counter_ >= kWaitTime) {
        enemy.GetGameObject().SetAlive(false);  // 10秒后销毁
    }
}
```

**DSE 当前**: `enemy.lua` dead 状态不销毁，敌人尸体永远存在（动画停在最后一帧）。

**修改方案**: 可选。KF 销毁是为了释放资源，DSE 保持尸体也可接受。如需对齐:
```lua
-- 在 new_enemy_data 中添加: dead_timer = 0
-- 在 Enemy.update_all 中:
if data.state == "dead" then
    data.dead_timer = (data.dead_timer or 0) + dt
    if data.dead_timer >= 10.0 then
        ecs.set_entity_visible(data.entity, false)  -- 或 destroy
    end
end
```

---

### P6: ✅ 摄像机鼠标/手柄旋转 (已完成)

**KF 源码**: `camera/third_person_camera.cpp:40-61`
```cpp
void ThirdPersionCamera::Update(void) {
    float rotation_x = input.RotationHorizontal();  // 鼠标/右摇杆水平
    float rotation_y = input.RotationVertical();    // 鼠标/右摇杆垂直
    if (fabsf(rotation_x) > kStartRotationMin) yaw_amount = kRotationSpeed * rotation_x;
    if (fabsf(rotation_y) > kStartRotationMin) pitch_amount = kRotationSpeed * rotation_y;
    pitch_speed_ = Math::Lerp(pitch_speed_, pitch_amount, kRotationLerpTime);
    yaw_speed_ = Math::Lerp(yaw_speed_, yaw_amount, kRotationLerpTime);
    Pitch(pitch_speed_);
    Yaw(yaw_speed_);
}
```
**KF 参数** (third_person_camera.h):
- `kRotationSpeed = 0.05f`
- `kStartRotationMin = 0.2f`
- `kRotationLerpTime = 0.1f`
- `kPitchMin = -5°`, `kPitchMax = 60°`
- `kDistanceMin = 2`, `kDistanceMax = 10`

**DSE 当前**: `player.lua:392-429` — 摄像机完全锁定在玩家背后，无旋转输入。

**修改方案**:
1. 添加独立的 `cam_yaw` / `cam_pitch` 变量 (不再绑定到 `state.facing_yaw`)
2. 鼠标移动 (`app.get_mouse_dx()`, `app.get_mouse_dy()`) 驱动 yaw/pitch
3. Pitch 限制: `[-5°, 60°]`
4. 摄像机位置从 `cam_yaw` / `cam_pitch` 计算（而非从角色 facing_yaw）
5. 移动方向也需要改为基于 `cam_yaw` 的世界空间（当前 `player.lua:258-273` 已用摄像机方向，需确认是否需调整）

**注意**: 需要确认 DSE 是否暴露了 `app.get_mouse_dx()` / `app.get_mouse_dy()` Lua API。如果没有，需要先在引擎侧添加。

**是否需要 API**: 检查 `engine/runtime/lua_binding*.cpp` 中是否有鼠标增量接口。

---

### P7: ✅ 阴影参数精确对齐 (已完成)

**KF 源码**: `render_system/shadow_map_system.h:110-119`
```cpp
ShadowMapSystem(const LPDIRECT3DDEVICE9 device)
    : offset_(Vector3(20.0f, 80.0f, -20.0f))  // 光源偏移
    , range_(20.0f)                              // 正交投影范围
    , near_(0.0f)
    , far_(200.0f)
    , bias_(0.00001f)
```
- 阴影跟随玩家: `shadow_map_system.cpp:44` `look_at = target_->GetPosition()`
- 1024×1024 阴影贴图

**DSE 当前**: `scene.lua:22`
```lua
ecs.set_directional_light_shadow(sun, true, 1.0, 800, 3000, 15000)
-- 参数: (entity, enabled, bias, shadow_range, shadow_distance, shadow_far)
```

**KF→DSE 精确换算** (×100):
- `offset` = (2000, 8000, -2000) → DSE 阴影方向已从光照方向计算，此处是 shadow_distance 的近似
- `range` = 20 × 100 = 2000 → DSE `shadow_range` 应为 **2000** (当前 800 偏小)
- `far` = 200 × 100 = 20000 → DSE `shadow_far` 应为 **20000** (当前 15000)
- `bias` = 0.00001 → DSE 当前 1.0 (单位体系不同，需实际测试)

**修改方案**:
```lua
ecs.set_directional_light_shadow(sun, true, 0.0005, 2000, 8000, 20000)
```
需实际运行调参。阴影是否跟随玩家取决于 DSE 引擎实现 — 需确认 `set_directional_light_shadow` 是否支持 target 跟随。

---

### P8: ✅ Ambient 亮度调整 (已完成)

**KF 源码**: `light/light.cpp:14`
```cpp
DirectionalLight::DirectionalLight(const Vector3& direction)
    : Light(kDirectionalLight, Color(0.8f, 0.8f, 0.8f, 1.0f), Color::kGray, Color::kWhite, direction)
```
- `diffuse = (0.8, 0.8, 0.8)` ✅
- `ambient = Color::kGray = (0.5, 0.5, 0.5)` ← **KF ambient = 0.5**
- `specular = Color::kWhite = (1.0, 1.0, 1.0)`

**DSE 当前**: `scene.lua:21`
```lua
ecs.add_directional_light_3d(sun, ..., 0.8, 0.8, 0.8, 1.0, 0.20, 0.35)
-- 最后两个参数: ambient_intensity=0.20, shadow_softness=0.35
```

**问题**: DSE ambient=0.20 远低于 KF 的 0.5。这是导致 "DSE 比 KF 暗 65 luma" 的主要原因之一。

**修改方案**:
```lua
ecs.add_directional_light_3d(sun, ..., 0.8, 0.8, 0.8, 1.0, 0.50, 0.35)
```
同时 sky_light 的 intensity 也可能需要调整:
```lua
-- 当前: ecs.add_sky_light(sky_light, 0.38, 0.45, 0.55, 0.12, 0.11, 0.10, 1.1)
-- KF 无 sky_light，全靠 directional_light 的 ambient
-- 可能需要降低 sky_light 并提高 directional ambient
```

---

### P9: ✅ 敌人攻击多次命中 BUG (已完成)

**KF 源码**: `enemy_zombie_attack_state.cpp:84-98`
```cpp
void EnemyZombieAttackState::OnTrigger(EnemyController& enemy, Collider& self, Collider& other) {
    if (self.GetTag()._Equal(L"Weapon")) {
        // 碰撞器触发 = 单次命中
        static_cast<PlayerController*>(player_controller)->Hit(enemy.GetParameter().GetAttack());
    }
}
```
KF 用碰撞器 Awake/Sleep 确保每次攻击只命中一次。

**DSE 当前**: `enemy.lua:240-253` `Enemy.check_attacks()` 使用 `cooldown > 1.2` (前0.3秒) 每帧检测，**可能多次命中**。

**修改方案**: 添加 `data.hit_player_this_attack` 标记:
```lua
function Enemy.check_attacks(player_x, player_z)
    local hits = {}
    for _, data in ipairs(Enemy.instances) do
        if data.state == "attack" and data.attack_cooldown > 1.2
           and not data.hit_player_this_attack then
            ...
            if dist < ENEMY.attack_range * 1.5 then
                table.insert(hits, data.atk or ENEMY.attack)
                data.hit_player_this_attack = true  -- 本次攻击只命中一次
            end
        end
        -- 攻击结束时重置
        if data.state ~= "attack" then
            data.hit_player_this_attack = false
        end
    end
    return hits
end
```

---

### P10: ✅ 摄像机碰撞 (已完成)

**KF 源码**: `third_person_camera.h:50`
```cpp
static constexpr float kCollisionRadius = 0.1f;
```
KF 摄像机有碰墙检测，防止穿墙。

**DSE 当前**: 无。摄像机可能穿入建筑内部。

**修改方案**: 如 DSE 引擎有 raycast API，可在 `Player.update_camera()` 中:
```lua
-- 从 pivot 点向摄像机目标位置发射射线
-- 如果中途碰到物体，缩短距离
```
优先级低，因为 KF 原始实现也比较简单（仅用球体半径 0.1）。

---

### P11: ✅ ReceiveDamage 无 defence 计算 (已完成)

**KF 源码**: `actor_controller.cpp:116-121`
```cpp
void ActorController::ReceiveDamage(const float& damage) {
    float current_life = parameter_.GetCurrentLife();
    current_life = max(0.0f, current_life - damage);  // 直接扣血，无减伤
    parameter_.SetCurrentLife(current_life);
}
```

**DSE 当前**: `enemy.lua:131`
```lua
local dmg = math.max(1, amount - (data.def or ENEMY.defence))
```
DSE 敌人有减伤，KF 没有。

**修改方案**:
```lua
local dmg = amount  -- KF: ReceiveDamage 无 defence 计算
```
但这影响平衡性，由用户决定。玩家侧 `player.lua:104` 已正确 (`local dmg = amount`)。

---

### P12: ✅ 玩家移动速度微调 (已完成)

**DSE 当前**: `player.lua:222`
```lua
local KF_MOVE_SPEED = 1500.0  -- 注释: KF: 10.0 × 100=1000, ×1.5 补偿感知差异
```
KF 精确值应该是 1000，DSE 用了 1500 作为补偿。

**KF 完整计算**:
```
move_speed = 10.0 (ActorParameter)
movement_multiplier = 1.0 (idle/walk state)
actual = move_amount * multiplier * move_speed * dt
```

**建议**: 保持 1500 或回到 1000 后配合调整转向速度，需实际测试手感。

---

### P13: ✅ Title 按钮位置精确对齐 (已基本对齐)

**KF 源码**: `mode/mode_title.cpp:47-53`
```cpp
auto button = GameObjectSpawner::CreateButton2d(
    Vector3(400.0f, 70.0f, 0.0f) * 0.8f,        // size: 320×56
    Vector3(-200.0f, SCREEN_HEIGHT * 0.25f, 0.0f), // pos: (-200, 180) (KF center-based)
    L"play_game.png", ...);
```

**DSE 当前**: `gameflow.lua:255-266`
```lua
ui.add_renderer(title_btn_game_ui, play_game_tex, 1, 1, 1, 1, 101, 320, 56)
ui.set_position(title_btn_game_ui, -200, -180)
```

尺寸 320×56 ✅，位置 ±200 ✅，Y=-180 (对应 KF 的 SCREEN_HEIGHT*0.25=180) ✅。
**已基本对齐**。

---

### P14: ✅ DemoPlay 模式完善 (保持AI方案)

**KF 源码**: `mode/mode_demo_play.cpp:115-118`
```cpp
void ModeDemoPlay::OnCompleteLoading(void) {
    MainSystem::Instance().GetSoundSystem().Play(kGameBgm);
    MainSystem::Instance().GetInput().SetDemoPlayMode(true);  // 输入回放
}
```
KF 的 DemoPlay 使用录制的输入数据回放玩家操作。

**DSE 当前**: `autoplay.lua` 使用 AI 自动战斗代替输入回放。功能上不同但效果近似。
**建议**: 保持当前 AI 方案，不需要实现录制回放。

---

## 建议执行顺序

1. **P8 + P7** (ambient + shadow) — 最大视觉改善，修改量小
2. **P1** (Enter 跳过) — 一行代码
3. **P4** (敌人 damaged 无敌) — 一行代码
4. **P9** (敌人攻击单次命中) — 几行代码
5. **P3** (敌人 return 巡逻) — 中等改动
6. **P6** (摄像机旋转) — 需确认引擎 API，较大改动
7. **P5, P10~P14** — 低优先，视情况处理

---

## 关键源码对照表 (供新会话快速查阅)

### KF 源码文件 → 功能

| KF 文件路径 | 功能 | 关键常量/逻辑 |
|------------|------|--------------|
| `mode/mode_title.cpp` | Title 画面 | kWaitTime=0.25s, button size=320×56 |
| `mode/mode_demo.cpp` | Battle 画面 | kWaitTime=8.0s(死亡), Enter跳过 |
| `mode/mode_demo_play.cpp` | DemoPlay | SetDemoPlayMode(true) |
| `mode/mode_result.cpp` | Result 画面 | kWaitTime=1.0s, flash_speed=15.0 |
| `fade_system.cpp` | Fade 系统 | 5状态, fade_time=1.0s, wait_fade=0.5s, TimeScale=0/1 |
| `camera/third_person_camera.h` | 摄像机参数 | dist=5, pitch=15°, offsetY=3.5, lerp=0.075 |
| `camera/third_person_camera.cpp` | 摄像机旋转 | rotSpeed=0.05, rotLerp=0.1, pitchMin=-5°, pitchMax=60° |
| `camera/camera.h` | 摄像机基类 | rig/pivot 层级, fov, near=0.1, far=1000 |
| `light/light.cpp` | 灯光 | diffuse=(0.8,0.8,0.8), ambient=gray(0.5), dir=(-1,-4,+1) |
| `render_system/shadow_map_system.h` | 阴影 | offset=(20,80,-20), range=20, far=200, bias=1e-5, 1024px |
| `ui/player_ui_controller.cpp` | 玩家 HUD | SCREEN_RATE=1280/1920, gauge/cover/warning/buttons |
| `ui/enemy_ui_controller.cpp` | 敌人 HP 条 | 3D billboard, offset=(0,4,0), size=(1.5,0.25,1), dist²=2500 |
| `ui/button_controller.cpp` | 按钮颜色 | color lerp, change_time=0.1s |
| `ui/flash_button_controller.cpp` | 闪烁按钮 | alpha 0↔1 振荡 |
| `actor/actor_controller.cpp` | 角色基类 | Move(), Jump(), CheckGrounded(), ReceiveDamage(无defence) |
| `actor/actor_parameter.h` | 角色参数 | max_life=10, attack=1, moveSpeed=10, jumpSpeed=1 |
| `actor/player_controller.cpp` | 玩家控制 | state machine, Hit(), OnAnimationOver() |
| `actor/enemy_controller.cpp` | 敌人控制 | warning_range=10, patrol_range=20, state machine |
| `actor_state/zombie/enemy/enemy_zombie_idle_state.cpp` | 敌人 idle | 碰撞器探测, 巡逻注释 |
| `actor_state/zombie/enemy/enemy_zombie_follow_state.cpp` | 敌人追击 | multiplier=0.1, attackRange=2, warningMul=1.5 |
| `actor_state/zombie/enemy/enemy_zombie_attack_state.cpp` | 敌人攻击 | frame-based collider, kBegin/kEndAttackFrame |
| `actor_state/zombie/enemy/enemy_zombie_damaged_state.cpp` | 敌人受击 | kInvincibleTime=**0.3**, 无敌期忽略伤害 |
| `actor_state/zombie/enemy/enemy_zombie_dying_state.cpp` | 敌人死亡 | kWaitTime=**10s** 后 SetAlive(false) |
| `actor_state/zombie/enemy/enemy_zombie_walk_state.cpp` | 敌人巡逻 | 向 next_position 移动, kArriveDistance=1.0, kMovementMul=**0.1** |
| `actor_state/knight/player/*.cpp` | 骑士 13 状态 | idle/walk/run/attack×3/block/jump×2/fall/land×2/impact/death |
| `other/windmill_controller.cpp` | 风车 | Fan RotateByRoll, rotate_speed×dt |
| `game_object/stage_spawner.cpp` | 场景加载 | 环境/玩家/敌人从二进制文件, 风车特殊处理 |

### DSE Lua 文件 → 功能

| DSE 文件 | 功能 | 行数 |
|----------|------|------|
| `script/config.lua` | 全局配置 (资产/参数/枚举) | 123 |
| `script/scene.lua` | 场景搭建 (光照/天空/地面/装饰物) | 100 |
| `script/player.lua` | 骑士 FSM + 输入 + 摄像机 | 432 |
| `script/enemy.lua` | Mutant AI + HP 条 | 361 |
| `script/gameflow.lua` | 游戏流程 (title/battle/result) | 415 |
| `script/hud.lua` | 玩家 HP 条 + 操作提示 | 190 |
| `script/fade.lua` | Fade 过渡 + Loading 动画 | 249 |
| `script/audio.lua` | 音效系统 | ~100 |
| `script/autoplay.lua` | AI 自动战斗 | ~150 |
| `script/terrain_height.lua` | 地形高度查询 | ~80 |
| `script/main.lua` | 入口 + 风车 + 碰撞 + 战斗判定 | 236 |

### 构建 & 运行

```bash
# 编译
cmake --build build_vs2022 --target dse_standalone --config Release

# 运行 (正常)
build_vs2022/bin/Release/dse_standalone.exe --project examples/KF_Framework

# 运行 (自动战斗截图模式)
set DSE_AUTO_BATTLE=1
set DSE_DISABLE_STARTUP_SCENE_REGRESSION=1
build_vs2022/bin/Release/dse_standalone.exe --project examples/KF_Framework

# 视觉对比
cd examples/KF_Framework
python tools/visual_compare.py
```
