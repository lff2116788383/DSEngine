# DSEngine Demo 复刻方案：KF_Framework 3D 格斗游戏

> 基于 KodFreedom/KF_Framework (DirectX9 C++) 复刻为 DSEngine Lua 脚本 Demo  
> 目标：展示 DSEngine 全栈能力（渲染 / 物理 / 动画 / 音频 / 输入 / AI / UI）  
> 工作目录：`examples/KF_Framework/`  
> 原始项目：`C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\`  
> 资产策略：**直接复用 KF_Framework 原始 FBX 资产**（FBX → AssetBuilder → DSEngine 格式）
> FBX 资产来源：`C:\Users\wenbilin\Desktop\temp_analysis\KF_ModelAnalyzer\data\FBX\`

---

## 0. KF → DSE 坐标/比例转换规则（已验证，禁止修改）

> ⚠️ **AI 注意**: 以下转换规则经过反复验证（解析 demo.stage 二进制 + 截图对比），是最终结论。
> 如需调整场景，仅修改各 Lua 模块中的具体数值，不要改动此规则区块。

| 项目 | KF_Framework (DX9 LH) | DSEngine (OpenGL RH) | 转换公式 |
|------|----------------------|---------------------|---------|
| **坐标系** | 左手系, 单位≈米 | 右手系, 单位=厘米 | `DSE_pos = KF_pos × 100, DSE_z = -KF_z` |
| **模型比例** | 角色 ~1.7m | 角色 ~172u (cm) | FBX 导入后天然 cm 单位 |
| **缩放规则** | 所有模型 scale=(1,1,1) | 建筑/栅栏/岩石/桶: `scale=1.0` | FBX 与角色同为 cm 单位 |
| **Pine_tree 例外** | scale=1.0 | `scale=0.1` | FBX 原始尺寸异常大 ~5000u, 目标 ~500u |
| **旋转** | quaternion(x,y,z,w) | euler_y 度 | `DSE_euler_y = -原始Y轴旋转角度` |
| **光照方向** | (-1, -4, +1) | (-1, -4, -1) | Z 取反 |
| **摄像机** | distance=5, offsetY=3.5, pitch=15° | pos=(0,350,500), pitch=-15°, fov=60° | ×100 + Z取反 |
| **移动速度** | moveSpeed=10 | 1000 | ×100 |
| **跳跃速度** | jumpSpeed=20 | 2000 | ×100 |
| **阴影** | offset=(20,80,-20), range=20, far=200 | shadow_range=800, distance=3000, far=15000 | ×100 + Z取反 |

**数据来源**:
- `data\stage\demo.stage` — 二进制文件: 18种模型的精确 position/rotation/scale
- `source_code\camera\third_person_camera.h` — 摄像机参数
- `source_code\game_object\stage_spawner.cpp` — 场景加载逻辑
- `source_code\light\light.cpp` — 灯光颜色/方向

**Lua 模块拆分**:
- `script/config.lua` — 全局配置 & 资产路径 & 游戏参数
- `script/scene.lua` — Phase 1 场景搭建
- `script/player.lua` — Phase 2+3 Knight 角色 + 摄像机
- `script/enemy.lua` — Phase 4 Mutant 敌人 AI
- `script/main.lua` — 入口，组装各模块

---

## 1. 原始项目分析

### 1.1 游戏流程

```
Title 画面 → [Play Game / Demo Play] → 战斗场景 → Result 画面 → 循环
```

### 1.2 原始功能清单

| 系统 | 具体实现 |
|------|----------|
| **场景** | 中世纪村庄（房屋/桥梁/风车/树木/岩石/水井/围栏） + 地面 + 天空盒 |
| **角色（Player）** | Knight 骑士 — 19 个动作状态（Idle/Run/Walk/Jump/3段轻攻击/3段重攻击/格挡/魔法/技能/受击/死亡...） |
| **角色（Enemy）** | Zombie 僵尸 — 6 个 AI 状态（Idle/Walk/Follow/Attack/Damaged/Dying） |
| **其他角色模型** | Mutant、Juggernaut（可选扩展） |
| **物理** | 3D 刚体 + OBB/AABB/Sphere 碰撞 + 地面检测 |
| **光照** | 平行光 + 深度阴影贴图 |
| **摄像机** | 第三人称跟随 + 自由编辑器摄像机 |
| **动画** | 蒙皮骨骼动画 + 状态机（Motion State） |
| **音频** | BGM (Title/Game/Result) + SE (攻击/格挡/受击/死亡/UI) |
| **UI** | Title 背景 + 按钮 / 战斗中 HP 条 / Result 界面 |
| **输入** | 键盘 + 手柄 (Xbox) |
| **关卡编辑** | Stage 编辑器（场景物体布局） |

### 1.3 原始资产清单

| 类型 | 数量 | 来源路径 |
|------|------|----------|
| 角色模型 (.model) | 4 (knight/zombie/mutant/juggernaut) | `data/MODEL/actor/` |
| 场景模型 (.model/.x) | 18+ (house/bridge/windmill/tree/rock...) | `data/MODEL/` |
| 动作数据 (.motion) | 65 个文件 | `data/motion/` |
| 纹理 (.jpg/.png/.tga) | 70+ 个文件 | `data/TEXTURE/` |
| BGM (.wav) | 3 (title/game/result) | `data/bgm/` |
| SE (.wav) | 16 个 | `data/se/` |
| 关卡数据 | demo.stage/demo.enemy/demo.player | `data/stage/` |
| Shader (HLSL) | skin/mesh/shadow/2d/billboard | `source_code/shader/` |

---

## 2. DSEngine 复刻策略

### 2.1 总体思路

- **游戏逻辑全部用 Lua 脚本** — 展示 DSEngine 脚本化能力
- **渲染使用引擎内建 PBR** — 不需要移植 HLSL shader
- **直接复用 KF_Framework 原始资产** — 通过 Python 转换工具转为 DSEngine 格式，可截图对比
- **简化范围** — 先做核心循环，再逐步补全

### 2.2 功能映射表

| KF_Framework | DSEngine Lua API | 备注 |
|--------------|------------------|------|
| GameObject | `ecs.create_entity()` | — |
| Transform | `ecs.add_transform / set_transform_position/rotation` | — |
| MeshRenderer3D + Skin | `ecs.add_mesh_renderer / set_mesh_path / set_mesh_material` | PBR 自动处理 |
| Animator + MotionState | `ecs.add_animator_3d + init_fsm + add_state + add_transition` | FSM 完美对应 |
| Rigidbody3D | `ecs.add_rigidbody_3d / add_box_collider_3d / add_sphere_collider_3d` | — |
| CharacterController | `ecs.add_character_controller_3d / move / jump / is_grounded` | — |
| DirectionalLight + Shadow | `ecs.add_directional_light_3d + set_directional_light_shadow` | — |
| Camera Follow | `ecs.add_camera_3d + set_camera_follow` | — |
| SkyBox | `ecs.add_skybox` | — |
| SoundSystem | `audio.add_source / set_playing / set_3d_mode` | — |
| Input (Key/Joystick) | `app.get_key / get_key_down` | — |
| Mode (Title/Demo/Result) | Lua 全局状态管理 | 脚本层实现 |
| 2D UI | `ui.add_renderer / add_label / add_button` | — |
| FieldCollider (地面) | `ecs.add_terrain` 或静态 mesh + collider | — |
| ActorParameter | Lua table | — |
| Observer Pattern | Lua 事件回调 | — |

### 2.3 不做 / 简化的部分

| 功能 | 处理方式 |
|------|----------|
| Inverse Kinematics | 跳过，不影响核心玩法 |
| DemoPlay 回放模式 | 跳过 |
| Stage Editor | 使用 DSEngine 编辑器代替 |
| OBB 精确碰撞 | 简化为 Box/Sphere |
| 多角色切换 (Mutant/Juggernaut) | Phase 8 可选扩展 |
| 骨骼层级碰撞体 | 简化为整体碰撞体 |

---

## 3. 资产准备方案

### 3.1 资产分类与处理

| 类型 | 原始格式 | 处理方式 | 说明 |
|------|----------|----------|------|
| **纹理** | .jpg/.png/.tga | ✅ 直接复制 | DSEngine 原生支持 |
| **音频** | .wav | ✅ 直接复制 | DSEngine 原生支持 |
| **角色模型** | .fbx (Mixamo 原始) | ✅ FBX → AssetBuilder → .dmesh + .dskel + .dmat | KF_ModelAnalyzer/data/FBX/ |
| **角色动画** | .fbx (Mixamo 原始) | ✅ FBX → AssetBuilder → .danim | 每角色独立动画 FBX |
| **场景静态模型** | .fbx | ✅ FBX → AssetBuilder → .dmesh | Baker_house, Bridge 等 |
| **关卡数据** | .stage/.enemy/.player | 🔧 手动在 DSEngine 编辑器中重建 | 二进制场景布局 |

### 3.2 FBX 资产来源

KF_Framework 的角色和场景模型均源自 Mixamo/外部 FBX 文件，后经 KF_ModelAnalyzer 工具转换为自定义二进制格式。
原始 FBX 保存在 `KF_ModelAnalyzer/data/FBX/` 目录中：

| 角色 | 基础模型 FBX | 动画 FBX 数量 | 纹理 |
|------|-------------|--------------|------|
| **knight** | `paladin_prop_j_nordstrom.fbx` | 35 个 | diffuse + normal + specular |
| **mutant** | `mutant.fbx` | 12 个 | diffuse + normal |
| **zombie** | `derrick.fbx` | 11 个 | diffuse + normal + specular |
| **场景** | Baker_house, Bridge, Windmill 等 | — | 各自附带 |

> 直接使用原始 FBX 比解析 KF 自定义二进制更可靠，因为 AssetBuilder 原生支持 FBX 导入（通过 Assimp 库）。

### 3.2.1 已逆向的 KF 自定义二进制格式（历史记录）

> 以下格式规格在逆向分析过程中整理，已实现为 `tools/kf_to_gltf.py`。
> 由于发现原始 FBX 文件可直接使用，当前主要转换路径已切换为 FBX 直通。
> 保留此节作为参考，未来如需处理仅有 KF 二进制格式的资产仍可使用。

<details>
<summary>展开查看 KF 二进制格式详情</summary>

**.mesh 格式：**
```
DrawType      : int32 (4=TriangleList)
vertex_count  : int32
index_count   : int32
polygon_count : int32
vertexes[]    : Vertex3d × vertex_count
                  position  : float × 3
                  normal    : float × 3
                  uv        : float × 2
                  color     : float × 4 (RGBA)
indices[]     : uint16 (WORD) × index_count
```

**.skin 格式：**
```
同 .mesh 头部，但顶点为 Vertex3dSkin:
  position    : float × 3
  normal      : float × 3
  tangent     : float × 3
  binormal    : float × 3
  uv          : float × 2
  bone_idx[5] : Short2 × 5  (每个 Short2 = 2 × int16)
  bone_wt[5]  : Vec2   × 5  (每个 Vec2   = 2 × float)
```

**.model 格式（递归）：**
```
name_size     : int32
name          : char[name_size]
position      : vec3 (float × 3)
rotation      : quat (float × 4)
scale         : vec3 (float × 3)
collider_count: int32
colliders[]   : (type:int + pos:vec3 + rot:vec3 + scale:vec3 + is_trigger:bool)
mesh_count    : int32
meshes[]      : (name + material_name + render_priority + shader_type
                 + cast_shadow:bool + bounding_sphere:vec3+float + mesh_type:int)
child_count   : int32
children[]    : 递归 CreateChildNode
```

**.motion 格式：**
```
is_loop       : bool
frame_count   : int32
frames[]      :
  bone_count  : int32
  bones[]     :
    translation : vec3 (float × 3)
    rotation    : quat (float × 4)
    scale       : vec3 (float × 3)
```

</details>

### 3.3 转换工具方案

**主要路径（FBX 直通）：**
```
KF_ModelAnalyzer/data/FBX/*.fbx
        ↓ (AssetBuilder 命令行，原生支持 FBX)
     .dmesh + .dmat + .dskel + .danim
```

直接调用 AssetBuilder 即可，无需额外脚本：
```bash
# 转换角色基础模型
bin/AssetBuilder.exe  KF_ModelAnalyzer/data/FBX/knight/paladin_prop_j_nordstrom.fbx  --out-dir examples/KF_Framework/cooked

# 转换动画
bin/AssetBuilder.exe  KF_ModelAnalyzer/data/FBX/knight/"Sword And Shield Idle.fbx"  --out-dir examples/KF_Framework/cooked
```

**备用路径（KF 二进制 → glTF，已实现）：**
```
tools/kf_to_gltf.py     # KF 二进制 → glTF 转换器
tools/batch_convert.py   # KF 二进制批量转换 + AssetBuilder
```

**优势：**
- FBX 直通零脚本开销，AssetBuilder 原生支持
- 纹理内嵌在 FBX 的 .fbm 目录中，可直接使用
- 纹理/音频直接复制不需转换

---

## 4. 分阶段实施计划

### Phase 0: 资产转换工具（预计 1 session）

**目标：** 将 KF_Framework 原始 FBX 资产转为 DSEngine 可用格式

**内容：**
- AssetBuilder 直接转换 FBX → .dmesh/.dskel/.danim/.dmat（命令行调用）
- `setup_assets.py`：复制纹理 (.jpg/.png/.tga) 和音频 (.wav)
- `kf_to_gltf.py` / `batch_convert.py`：备用 KF 二进制转换器（已实现）

**验收标准：**
- [x] Knight FBX → AssetBuilder → .dmesh + .dskel + 36 .danim ✅
- [x] Zombie FBX → .dmesh + .dskel + 11 .danim ✅
- [x] Mutant FBX → .dmesh + .dskel + 12 .danim ✅
- [x] 场景静态模型 FBX → 20 .dmesh ✅
- [x] 纹理/音频文件复制到位 ✅

---

### Phase 1: 场景搭建（预计 1 session）

**目标：** 搭建基础 3D 场景，验证渲染 pipeline

**内容：**
- 创建 demo 项目目录 `examples/KF_Framework/`
- 地面 Plane（可用 Terrain 或简单 mesh）
- 天空盒加载
- 平行光 + 阴影
- 第三人称摄像机
- 若干场景装饰物（cube 占位）

**对应 Lua API：**
```lua
-- 场景搭建
local ground = dse.ecs.create_entity()
dse.ecs.add_transform(ground, 0, 0, 0, 50, 1, 50)
dse.ecs.add_mesh_renderer(ground)
dse.ecs.set_mesh_path(ground, "examples/KF_Framework/cooked/demoField.dmesh")

local sky = dse.ecs.create_entity()
dse.ecs.add_skybox(sky, "examples/KF_Framework/assets/textures/skybox000.jpg")

local sun = dse.ecs.create_entity()
dse.ecs.add_transform(sun, 0, 0, 0)
dse.ecs.add_directional_light_3d(sun, -0.5, -1, -0.3, 1, 0.95, 0.9, 1.5, 0.15)
dse.ecs.set_directional_light_shadow(sun, true, 0.4, 10, 30, 80)

local cam = dse.ecs.create_entity()
dse.ecs.add_transform(cam, 0, 5, -10)
dse.ecs.add_camera_3d(cam, 60, 0, 0.1, 500)
```

**验收标准：**
- [ ] 地面 + 天空盒 + 阴影可见
- [ ] 自由摄像机可浏览场景

---

### Phase 2: 角色加载 + 动画 FSM（预计 1-2 sessions）

**目标：** Knight 角色加载，动画状态机工作

**内容：**
- Knight 模型加载（由 Phase 0 转换产出的 .dmesh）
- 骨骼动画器 (Animator3D) 加载 .dskel + .danim
- FSM 状态机配置：Idle ↔ Run ↔ Jump / Attack / Block / Death
- 动画过渡 (Blend Transition)

**状态机设计（简化版）：**
```
                    ┌─────────────┐
                    │    Idle     │ ←────┐
                    └──────┬──────┘      │
          speed>0.5 │              │ speed<0.1
                    ▼              │
                    ┌─────────────┐      │
                    │     Run     │ ─────┘
                    └──────┬──────┘
                           │ jump_trigger
                           ▼
                    ┌─────────────┐
                    │    Jump     │ → (exit_time) → Idle/Run
                    └─────────────┘

    Idle/Run + attack_trigger → LightAttack1 → LightAttack2 → LightAttack3 → Idle
    Idle/Run + block_trigger  → Block → Idle
    Any + damaged_trigger     → Impact → Idle
    Any + death               → Death (终态)
```

**对应 Lua API：**
```lua
local player = dse.ecs.create_entity()
dse.ecs.add_transform(player, 0, 0, 0)
dse.ecs.add_mesh_renderer(player)
dse.ecs.set_mesh_path(player, "examples/KF_Framework/cooked/knight.dmesh")
dse.ecs.add_animator_3d(player, "", "examples/KF_Framework/cooked/knight.dskel")
dse.ecs.init_animator_3d_fsm(player)
dse.ecs.add_animator_3d_state(player, "idle", "examples/KF_Framework/cooked/knight_idle.danim", true)
dse.ecs.add_animator_3d_state(player, "run", "examples/KF_Framework/cooked/knight_run.danim", true, 1.2)
dse.ecs.add_animator_3d_transition(player, "idle", "run", 0.2, false, 1.0,
    { {"speed", 0, 0.5} })
dse.ecs.add_animator_3d_transition(player, "run", "idle", 0.2, false, 1.0,
    { {"speed", 1, 0.1} })
```

**验收标准：**
- [x] Knight 模型加载并显示
- [x] Idle/Run 动画切换流畅
- [x] 至少 3 个状态可正确过渡（13 个状态 + 完整 FSM）

---

### Phase 3: 角色控制（预计 1 session）

**目标：** WASD 移动 + 跳跃 + 攻击输入

**内容：**
- CharacterController3D 添加
- WASD 移动（相对摄像机方向）
- 空格跳跃
- 鼠标左键 → 轻攻击（3段连击）
- 鼠标右键 → 格挡
- 角色朝向跟随移动方向
- 第三人称摄像机跟随

**对应 Lua 脚本逻辑：**
```lua
function update_player(dt)
    local move_x = 0
    local move_z = 0
    if dse.app.get_key(87) then move_z = 1 end   -- W
    if dse.app.get_key(83) then move_z = -1 end  -- S
    if dse.app.get_key(65) then move_x = -1 end  -- A
    if dse.app.get_key(68) then move_x = 1 end   -- D

    local speed = math.sqrt(move_x*move_x + move_z*move_z)
    dse.ecs.set_animator_3d_param_float(player, "speed", speed)

    if speed > 0 then
        -- 移动 + 旋转
        dse.ecs.character_controller_3d_move(player, move_x * 5 * dt, -9.8 * dt, move_z * 5 * dt)
    end

    if dse.app.get_key_down(32) then  -- Space
        dse.ecs.set_animator_3d_param_trigger(player, "jump")
        dse.ecs.character_controller_3d_jump(player, 8)
    end

    if dse.app.get_mouse_left_down() then
        dse.ecs.set_animator_3d_param_trigger(player, "attack")
    end
end
```

**验收标准：**
- [x] WASD 移动，角色面朝移动方向
- [x] 空格跳跃有物理效果
- [x] 攻击动画可触发
- [x] 摄像机第三人称跟随（KF rig/pivot 层级精确复刻）

---

### Phase 4: 敌人 AI（预计 1 session）

**目标：** Zombie 敌人，简单 AI 行为

**AI 状态机：**
```
Idle → (发现玩家 距离<15) → Follow → (距离<2) → Attack → (攻击结束) → Follow
                                  ↑
Follow → (距离>20) → Idle        │
Attack → (受击) → Damaged ────────┘
Any → (HP≤0) → Dying (终态)
```

**内容：**
- Zombie 模型 + 动画 FSM
- AI 脚本：距离检测 → 状态切换
- Follow 状态追踪玩家
- Attack 状态近距离攻击
- 多个 Zombie 生成

**验收标准：**
- [x] Mutant 在玩家靠近时开始追踪 (detect_range=1500)
- [x] 近距离触发攻击动画 (attack_range=200)
- [x] 被攻击后有受击反馈 (damaged 状态 + 硬直)
- [x] 4 个 Mutant 同时运行

---

### Phase 5: 战斗系统（预计 1 session）

**目标：** 完整的伤害 / HP / 碰撞系统

**内容：**
- 攻击碰撞检测（Sphere trigger）
- 伤害计算（attack - defence）
- HP 管理
- 受击动画触发
- 死亡处理（Death 动画 → 销毁实体）
- 玩家死亡 → 进入 Result

**参数设计：**
```lua
player_params = {
    max_life = 100,
    attack = 15,
    defence = 3,
    move_speed = 5,
    jump_speed = 8,
}

zombie_params = {
    max_life = 30,
    attack = 8,
    defence = 1,
    move_speed = 2.5,
}
```

**验收标准：**
- [x] 玩家攻击可命中敌人并扣血 (前方120°扇形 + 距离检测)
- [x] 敌人攻击可命中玩家并扣血 (距离 + 攻击帧窗口)
- [x] 敌人 HP 归零后死亡动画
- [ ] 所有敌人死亡或玩家死亡 → 触发结局 (Phase 6)

---

### Phase 6: 游戏流程 + UI（预计 1 session）

**目标：** Title → 战斗 → Result 完整流程

**内容：**
- Title 画面（背景图 + "Play Game" 按钮 + BGM）
- 战斗中 UI：玩家 HP 条
- Result 画面（胜利/失败 + "Press Any Key" → 返回 Title）
- Fade 过渡效果
- 场景加载/卸载管理

**对应 Lua：**
```lua
-- 模式管理
game_state = "title"  -- "title" | "battle" | "result"

function on_title_enter()
    -- 加载 title 背景图、按钮、BGM
    dse.audio.set_playing(title_bgm, true)
end

function on_battle_enter()
    -- 加载战斗场景、角色、敌人
    dse.ecs.load_scene("data/demo/scenes/battle.dscene")
    dse.audio.set_playing(battle_bgm, true)
end

function on_result_enter()
    -- 显示结果 UI
    dse.audio.set_playing(result_bgm, true)
end
```

**验收标准：**
- [x] Battle → Result 流程（玩家死亡→DEFEAT / 全敌消灭→VICTORY）
- [x] 每个画面有对应 BGM（game.wav / result.wav）
- [ ] 战斗中显示 HP 条（待实现）
- [ ] Fade 过渡自然（待实现）
- [ ] Title 画面（待实现，需要 title 纹理）

---

### Phase 7: 音效 + 打磨（预计 1 session）

**目标：** 音效完善 + 视觉打磨

**内容：**
- 攻击音效 (sord_attack.wav)
- 受击音效 (damage_voice)
- 格挡音效 (block.wav)
- 死亡音效 (death_voice.wav / zombie_death.wav)
- UI 音效 (cursor.wav / submit.wav)
- 空间音频（敌人 3D 音源）
- 场景装饰物补充（树/石头/建筑）
- 材质调整（PBR 参数）
- 阴影质量调整

**验收标准：**
- [x] 玩家攻击音效（sord_attack + attack_voice 1-3随机）
- [x] 玩家格挡音效（block.wav）
- [x] 玩家受伤/死亡音效（damage_voice 1-2随机 / death_voice）
- [x] 敌人命中/死亡/警告音效（zombie_beat / zombie_death / zombie_warning）
- [x] BGM 切换（game → result）
- [ ] 3D 音源距离衰减（待实现）
- [x] 场景视觉丰富度达标（18种装饰物 + 天空盒 + 后处理）

---

### Phase 8: 源码对齐精修（源码验证后）

> 以下任务基于 KF_Framework 源码逐文件验证后整理。
> 源码路径: `C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\source_code\`

#### 8.1 格挡 = 100% 免疫 + Guard Voice（已验证） ✅

**KF 源码**: `player_knight_block_state.cpp:75-83`
```cpp
void PlayerKnightBlockState::OnDamaged(PlayerController& player, const float& damage)
{
    if (player.GetAnimator().GetIsDamaged()) return;
    player.GetAnimator().SetDamaged(true);
    // Se — 注意: 不调用 ReceiveDamage(), 格挡时零伤害
    sound_system.Play(kBlockSe);
    sound_system.Play(kGuardVoiceSe);
}
```

**修改**: `player.lua` → `Player.damage()` 检测当前是否在 block/block_idle 状态:
- 格挡中 → 不扣血，播放 block.wav + guard_voice.wav
- 非格挡 → 正常扣血
- `config.lua` 添加 `se_guard_voice` 路径

**验收**: 格挡状态下受击 HP 不减，听到 block + guard_voice 音效

---

#### 8.2 攻击语音对应步骤（已验证） ✅

**KF 源码**:
- `light_attack_step1_state.cpp:74`: `Play(kAttackVoice1Se)` + `Play(kSordAttackSe)`
- `light_attack_step2_state.cpp:73`: `Play(kAttackVoice2Se)` + `Play(kSordAttackSe)`
- `light_attack_step3_state.cpp:63`: `Play(kAttackVoice3Se)` + `Play(kSordAttackSe)`

**修改**: `player.lua` → 攻击触发时根据当前 FSM 状态名选择语音:
- 当前为 attack1 → attack_voice1, attack2 → attack_voice2, attack3 → attack_voice3
- 移除 `Audio.play_attack_voice()` 的随机逻辑

---

#### 8.3 受击无敌时间（已验证） ✅

**KF 源码**: `player_knight_impact_state.h:51`
```cpp
static constexpr float kInvincibleTime = 0.5f;
```
`player_knight_impact_state.cpp:65`: 无敌期间内再次受击无效。

**修改**: `player.lua` 添加 `invincible_timer`, 受击后 0.5 秒内不再受伤。

---

#### 8.4 受击音效修正（已验证） ✅

**KF 源码**: `player_knight_impact_state.cpp:27-28`
```cpp
sound_system.Play(kZombieBeatSe);      // 被打中时的打击音效
sound_system.Play(static_cast<SoundEffectLabel>(kDamageVoice1Se + Random::Range(0, 2)));
```

**修改**: `player.lua` → `Player.damage()` 中，非格挡受击时同时播放 zombie_beat + 随机 damage_voice。
当前实现只播 damage_voice，缺少 zombie_beat。

---

#### 8.5 死亡延迟进入 Result（已验证） ✅

**KF 源码**: `mode_demo.h:30` + `mode_demo.cpp:91-95`
```cpp
static constexpr float kWaitTime = 8.0f;
// プレイヤーが死んだらリザルトにいく
if (player->GetCurrentStateName().find(L"Death") != String::npos)
    time_counter_ = kWaitTime;
// エネミーが全滅 → 短延迟
if (main_system.GetActorObserver().GetEnemys().empty())
    time_counter_ = GameTime::kTimeInterval;  // 约1帧
```

**修改**: `gameflow.lua` → 玩家死亡延迟改为 8.0 秒（当前 2.0 秒），敌人全灭改为 ~0.1 秒。

---

#### 8.6 Fade 过渡系统（已验证） ✅

**KF 源码**: `fade_system.cpp`
- `FadeTo(next_mode, fade_time=1.0f)` — 触发转场
- FadeOut: alpha 0→1, 持续 fade_time(1s), 同时 TimeScale→0 冻结游戏
- FadeWait: 等待资源加载（DSE 不需要）
- FadeIn: alpha 1→0, 持续 fade_time(1s), 恢复 TimeScale=1
- 实现: 全屏黑色 2D 多边形, 修改 material.diffuse.a

**修改**: 新建 `script/fade.lua` 模块:
- 全屏黑色 UIRenderer (z_order=200, 最顶层)
- `Fade.fade_out(duration, callback)` — alpha 0→1, 完成后回调
- `Fade.fade_in(duration, callback)` — alpha 1→0, 完成后回调
- `gameflow.lua` 在模式切换时: fade_out(1s) → 切换 → fade_in(1s)

---

#### 8.7 Title 画面（已验证） ✅

**KF 源码**: `mode_title.cpp`
- Camera + DirectionalLight 初始化
- 全屏背景图 "title" 纹理
- 两个按钮: "play_game.png"(左) + "demo_play.png"(右), 初始左选中=白, 右=灰
- 左右键切换: 播放 kCursorSe, 选中→白 / 未选中→灰
- 确认键: 播放 kSubmitSe, 延迟 kWaitTime 后 FadeTo(ModeDemo/ModeDemoPlay)
- BGM: kTitleBgm

**修改**: `gameflow.lua` 扩展 Title 状态:
- 固定摄像机俯瞰场景
- 居中标题文字 + "Press Enter to Start" (DSE 无 title 纹理, 用 rich_text 替代)
- Enter/Space → kSubmitSe → fade_out → 进入 battle
- BGM: title.wav

**注意**: DSE 目前无 scene unload, Title 和 Battle 共用同一场景, Title 时隐藏角色/敌人。

---

#### 8.8 Result 画面完善（已验证） ✅

**KF 源码**: `mode_result.cpp`
- 全屏背景图 "result" 纹理
- "press_any_key" 闪烁按钮 (FlashButtonController)
- 任意键 → kSubmitSe → 延迟 → FadeTo(ModeTitle)
- BGM: kResultBgm

**修改**: `gameflow.lua` Result 状态:
- 添加 kSubmitSe 音效（当前缺失）
- "Press Any Key" 文字闪烁效果
- 按键 → submit.wav → fade_out → 重置/回 Title

---

#### 8.9 数值平衡对齐 ✅

当前问题: 3/4 只敌人在 detect_range(1500) 内, 玩家开局约 10 秒死亡。

**KF 源码对比**:
- `enemy_controller.h:18`: `warning_range_(10.0f)` = DSE 1000 (当前 detect_range=1500, 偏大)
- `ActorController::ReceiveDamage`: `life - damage`, **defence_ 未参与计算** (无减伤)
- `player_knight_impact_state.h:51`: 受击无敌 0.5s
- `mode_demo.h:30`: 死亡→Result 延迟 8s

**修改**:
- `config.lua`: `detect_range = 1000` (对齐 KF warning_range=10×100)
- `config.lua`: 移除 defence 减伤（原版无此机制）或保留作为 DSE 增强
- 添加受击无敌 0.5s (8.3)
- 调整敌人初始位置或数量

---

#### ~~8.10 以下功能 KF 源码已定义但未实际调用，不实现~~

| 功能 | 枚举定义位置 | 调用情况 |
|------|------------|---------|
| `kBeginVoiceSe` | `sound_system.h:31` | 全代码无 Play() 调用 |
| `kPinchVoiceSe` | `sound_system.h:36` | 全代码无 Play() 调用 |
| 敌人重生 | — | `mode_demo.cpp:97`: 全灭→Result, 无重生 |

---

### Phase 9: 扩展（可选）

- 添加 Zombie 角色（KF 原版敌人）
- 添加 Juggernaut Boss 战
- 手柄支持
- 粒子特效（攻击火花/血液）
- 技能特效（Magic/Skill 动画）
- DemoPlay 回放模式

---

## 5. 目录结构

```
examples/KF_Framework/
├── tools/
│   ├── setup_assets.py        # 复制纹理/音频
│   ├── kf_to_gltf.py         # KF 二进制 → glTF 2.0 转换器（备用）
│   └── batch_convert.py       # KF 二进制批量转换（备用）
├── script/
│   ├── main.lua               # 入口脚本 (Phase 1~7 组装)
│   ├── config.lua             # 全局配置 & 资产路径 & 游戏参数
│   ├── scene.lua              # Phase 1 场景搭建
│   ├── player.lua             # Phase 2+3 Knight FSM + 输入 + 摄像机
│   ├── enemy.lua              # Phase 4 Mutant AI
│   ├── gameflow.lua           # Phase 6+8 游戏流程 (Title→Battle→Result)
│   ├── audio.lua              # Phase 7 音效管理
│   ├── hud.lua                # Phase 6 血条 HUD
│   └── fade.lua               # Phase 8.6 Fade 过渡系统
├── assets/                    # 直接从 KF_Framework 复制的原始资产
│   ├── textures/              # .jpg/.png/.tga（直接复制）
│   ├── audio/
│   │   ├── bgm/               # title.wav, game.wav, result.wav
│   │   └── se/                # 所有音效
│   └── raw/                   # KF 原始二进制（.mesh/.skin/.model/.motion）供转换用
├── cooked/                    # AssetBuilder 输出的 DSEngine 格式
│   ├── knight.dmesh           # 蒙皮网格
│   ├── knight.dskel           # 骨骼
│   ├── knight_idle.danim      # 动画
│   ├── knight_run.danim
│   ├── knight_attack1.danim
│   ├── ...
│   ├── zombie.dmesh
│   ├── zombie.dskel
│   ├── zombie_idle.danim
│   ├── ...
│   ├── demoField.dmesh        # 地面
│   ├── *.dmesh                # 场景静态模型
│   └── *.dmat                 # 材质
├── scenes/
│   └── battle.dscene
└── README.md                  # 复刻说明 + 截图对比
```

---

## 6. 风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| FBX 导入坐标系差异 | 中 | AssetBuilder 内部通过 Assimp 库自动处理坐标系转换 |
| FBX 动画命名不统一 | 低 | fbx_convert.py 中维护 FBX→输出名映射表 |
| CharacterController 物理表现不稳定 | 中 | 用 rigidbody_3d 替代或调参 |
| 多 Zombie 同时动画性能 | 低 | DSEngine 已有实例化渲染，5 个 Zombie 应无压力 |
| Lua 脚本架构复杂度 | 中 | 分文件管理，保持每个文件 <300 行 |

---

## 7. 预计总工时

| Phase | 内容 | 预计 Sessions |
|-------|------|---------------|
| Phase 0 | 资产转换工具 (Python → glTF → AssetBuilder) | 1-2 |
| Phase 1 | 场景搭建 | 1 |
| Phase 2 | 角色 + 动画 FSM | 1-2 |
| Phase 3 | 角色控制 | 1 |
| Phase 4 | 敌人 AI | 1 |
| Phase 5 | 战斗系统 | 1 |
| Phase 6 | 游戏流程 + UI | 1 |
| Phase 7 | 音效 + 打磨 + 截图对比 | 1 |
| **合计** | | **8-10 sessions** |

---

## 8. 首要行动

1. ~~**复制纹理/音频**~~ ✅ `setup_assets.py` 已完成
2. **用 AssetBuilder 转换 knight FBX** — 基础模型 + 动画
3. **用 AssetBuilder 转换 zombie / 场景 FBX**
4. **验证加载** — 在 DSEngine standalone 中加载 Knight .dmesh 验证渲染
5. **Phase 1 实施** — 搭建最小场景
