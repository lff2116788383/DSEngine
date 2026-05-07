# DSEngine Demo 复刻方案：KF_Framework 3D 格斗游戏

> 基于 KodFreedom/KF_Framework (DirectX9 C++) 复刻为 DSEngine Lua 脚本 Demo  
> 目标：展示 DSEngine 全栈能力（渲染 / 物理 / 动画 / 音频 / 输入 / AI / UI）  
> 工作目录：`examples/KF_Framework/`  
> 原始项目：`C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\`  
> 资产策略：**直接复用 KF_Framework 原始资产**（通过 Python 转换工具 → glTF → AssetBuilder → DSEngine 格式）

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
| **静态网格** | .mesh (自定义二进制) | 🔧 Python → glTF → AssetBuilder → .dmesh | 格式已逆向 |
| **蒙皮网格** | .skin (自定义二进制) | 🔧 Python → glTF → AssetBuilder → .dmesh | 含骨骼权重 |
| **模型层级** | .model (递归节点树) | 🔧 Python 解析 → Lua 生成脚本 | 含 Transform + Mesh/Skin 引用 |
| **动画** | .motion (逐帧骨骼) | 🔧 Python → glTF animation → AssetBuilder → .danim | 每帧每骨骼 TRS |
| **关卡数据** | .stage/.enemy/.player | 🔧 手动在 DSEngine 编辑器中重建 | 二进制场景布局 |

### 3.2 已逆向的二进制格式

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

### 3.3 转换工具方案

```
examples/KF_Framework/tools/kf_to_gltf.py
```

**转换链：**
```
KF .mesh/.skin/.model/.motion
        ↓ (Python 转换器)
     .glb (glTF 2.0 Binary)
        ↓ (DSEngine AssetBuilder)
     .dmesh + .dmat + .dskel + .danim
```

**优势：**
- 100% 复用原始资产，截图 1:1 对比
- 走 DSEngine 标准资产管线，不侵入引擎
- 格式已完全逆向，Python 实现简单
- 纹理/音频直接复制不需转换

---

## 4. 分阶段实施计划

### Phase 0: 资产转换工具（预计 1-2 sessions）

**目标：** 编写 Python 转换器，将 KF_Framework 原始二进制资产转为 DSEngine 可用格式

**内容：**
- `kf_to_gltf.py`：读取 .mesh/.skin/.model/.motion → 输出 .glb
- 处理静态网格（.mesh → glTF mesh）
- 处理蒙皮网格（.skin + .model 骨骼层级 → glTF skin + joints）
- 处理动画（.motion → glTF animation channels）
- 批量转换脚本：遍历所有资产并调用 AssetBuilder
- 直接复制纹理 (.jpg/.png/.tga) 和音频 (.wav)

**验收标准：**
- [ ] Knight .model + 全部 .motion → .glb → AssetBuilder → .dmesh + .dskel + .danim
- [ ] Zombie .model + 全部 .motion → 同上
- [ ] 场景静态模型 (.mesh) → .dmesh
- [ ] 纹理/音频文件复制到位

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
- [ ] Knight 模型加载并显示
- [ ] Idle/Run 动画切换流畅
- [ ] 至少 3 个状态可正确过渡

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
- [ ] WASD 移动，角色面朝移动方向
- [ ] 空格跳跃有物理效果
- [ ] 攻击动画可触发
- [ ] 摄像机第三人称跟随

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
- [ ] Zombie 在玩家靠近时开始追踪
- [ ] 近距离触发攻击动画
- [ ] 被攻击后有受击反馈
- [ ] 3-5 个 Zombie 同时运行

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
- [ ] 玩家攻击可命中敌人并扣血
- [ ] 敌人攻击可命中玩家并扣血
- [ ] 敌人 HP 归零后死亡动画 + 销毁
- [ ] 所有敌人死亡或玩家死亡 → 触发结局

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
- [ ] 完整 Title → Battle → Result → Title 循环
- [ ] 每个画面有对应 BGM
- [ ] 战斗中显示 HP 条
- [ ] Fade 过渡自然

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
- [ ] 所有交互有音效反馈
- [ ] 3D 音源距离衰减正常
- [ ] 场景视觉丰富度达标

---

### Phase 8: 扩展（可选）

- 添加 Mutant 角色（可切换角色）
- 添加 Juggernaut Boss 战
- 添加更多场景装饰
- 手柄支持
- 粒子特效（攻击火花/血液）
- 技能特效（Magic/Skill 动画）

---

## 5. 目录结构

```
examples/KF_Framework/
├── tools/
│   ├── kf_to_gltf.py         # KF 二进制 → glTF 2.0 转换器
│   ├── batch_convert.py       # 批量转换 + 调用 AssetBuilder
│   └── copy_assets.py         # 复制纹理/音频
├── scripts/
│   ├── main.lua               # 入口脚本 + 模式管理
│   ├── player.lua             # 玩家控制器
│   ├── enemy_zombie.lua       # Zombie AI
│   ├── combat.lua             # 战斗系统
│   ├── ui_manager.lua         # UI 管理
│   ├── camera_controller.lua  # 第三人称摄像机
│   └── config.lua             # 游戏参数配置
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
| KF .skin 骨骼权重精度丢失 | 中 | 转换时归一化权重，验证 glTF 输出 |
| .model 骨骼层级→glTF joints 映射 | 高 | 需仔细对照节点名称和 parent 关系 |
| .motion 帧率差异 | 中 | KF 是逐帧存储，glTF 用关键帧；可保留全帧作为关键帧 |
| DX9 左手坐标系 vs OpenGL 右手坐标系 | 高 | 转换时翻转 Z 轴（position.z 取反，旋转调整） |
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

1. **编写 Python 转换工具** — `kf_to_gltf.py` 读取 KF 二进制 → 输出 .glb
2. **批量转换资产** — 运行 AssetBuilder 生成 .dmesh/.dskel/.danim
3. **复制纹理/音频** — 直接从 KF_Framework 复制到 `examples/KF_Framework/assets/`
4. **验证加载** — 在 DSEngine standalone 中加载 Knight .dmesh 验证渲染
5. **Phase 1 实施** — 搭建最小场景
