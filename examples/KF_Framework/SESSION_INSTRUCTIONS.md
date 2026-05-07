# DSEngine Demo 复刻 — 新会话任务指令

> 每个新会话开始时将此文件内容提供给 AI，以获得连续的开发体验。

---

## 项目背景

DSEngine 是一个 C++/OpenGL 游戏引擎，使用 Lua 作为脚本语言。引擎已完成全部核心功能，包括：
- ECS 架构（基于 EnTT）
- 3D PBR 渲染（OpenGL 3.3+）
- 3D 骨骼动画 + FSM 状态机
- 3D 物理（PhysX 可选）+ CharacterController
- 音频系统
- Lua 脚本绑定（~145 个 API）
- 编辑器（ImGui）

**当前任务：** 复刻 KF_Framework（一个 DX9 3D 格斗游戏 demo）为纯 Lua 脚本驱动的 demo，展示引擎全栈能力。  
**资产策略：** 直接复用 KF_Framework 原始资产，通过 Python 转换器 → glTF → AssetBuilder → DSEngine 格式。可截图 1:1 对比。

---

## 关键文件位置

```
引擎根目录:     c:\Users\wenbilin\Desktop\Engine\DSEngine\
工作目录:       examples/KF_Framework/
详细方案:       examples/KF_Framework/PLAN.md
Lua API 文档:   docs/LUA_API.md
引擎绑定源码:   engine/scripting/lua/bindings/lua_binding_*.cpp
AssetBuilder:   apps/tools/asset_builder/main.cpp
原始参考项目:   C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\
```

---

## Session 0 任务：Phase 0 资产转换工具

### 目标
编写 Python 转换器，将 KF_Framework 原始二进制资产转为 DSEngine 可用格式。

### 前置条件
1. 确认 AssetBuilder 已编译（检查 `bin/` 或构建目录）
2. 确认 Python 3 可用，安装 `pygltflib` 或用纯二进制写 glTF
3. 原始资产位于 `C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\`

### 任务清单

1. **创建目录结构**
   ```
   examples/KF_Framework/tools/
   examples/KF_Framework/scripts/
   examples/KF_Framework/assets/textures/
   examples/KF_Framework/assets/audio/bgm/
   examples/KF_Framework/assets/audio/se/
   examples/KF_Framework/assets/raw/
   examples/KF_Framework/cooked/
   ```

2. **复制可直接使用的资产**
   - `data/TEXTURE/*.jpg,*.png,*.tga` → `assets/textures/`
   - `data/bgm/*.wav` → `assets/audio/bgm/`
   - `data/se/*.wav` → `assets/audio/se/`
   - `data/mesh/`, `data/skin/`, `data/MODEL/actor/`, `data/motion/` → `assets/raw/`

3. **编写 `kf_to_gltf.py`**
   已逆向的二进制格式（详见 PLAN.md 第 3.2 节）：
   - `.mesh` → glTF mesh (position + normal + uv + color + indices)
   - `.skin` → glTF mesh + skin (bone_indices + bone_weights + joints)
   - `.model` → glTF node hierarchy (递归解析)
   - `.motion` → glTF animation (逐帧 TRS 作为 keyframes)
   - 注意 DX9 左手坐标系 → OpenGL 右手坐标系（翻转 Z 轴）

4. **编写 `batch_convert.py`**
   - 遍历 `assets/raw/` 中所有资产
   - 调用 `kf_to_gltf.py` 生成 .glb
   - 调用 AssetBuilder 生成 .dmesh/.dskel/.danim/.dmat
   - 输出到 `cooked/`

5. **验证转换结果**
   - 在 DSEngine standalone 中加载 knight.dmesh 验证渲染
   - 检查骨骼和动画是否正确

---

## Session 1 任务：Phase 1 场景搭建

### 前置条件
1. Phase 0 完成，`cooked/` 目录有可用的 .dmesh/.dskel/.danim
2. 纹理/音频已复制到 `assets/`

### 任务清单

1. **编写入口脚本 `examples/KF_Framework/scripts/main.lua`**
   - 游戏状态管理（title/battle/result）
   - 每帧 update 分发
   - 场景搭建函数

2. **最小 3D 场景**
   - 加载地面 mesh (demoField.dmesh)
   - 添加天空盒 (skybox000.jpg)
   - 添加平行光 + 阴影
   - 添加 3D 摄像机
   - 加载若干场景装饰物 (.dmesh)

3. **验证引擎加载**
   - 使用 standalone 应用加载 `main.lua`
   - 确认场景渲染正确

### 注意事项

- 工作目录：`examples/KF_Framework/`
- 所有游戏逻辑纯 Lua，不修改 C++ 引擎代码
- Lua 脚本使用 `dse.*` 命名空间调用引擎 API
- 参考 `docs/LUA_API.md` 获取所有可用 API
- 资产路径使用相对于引擎根目录的 `examples/KF_Framework/cooked/*.dmesh` 等
- 如果引擎缺少某功能，先记录到 gap list 中，用占位方案继续
- 脚本文件保持每个 <300 行，复杂逻辑分文件

---

## Session 2 任务：Phase 2 角色 + 动画

### 前置条件
- Phase 0 已产出 knight.dmesh/.dskel + knight_*.danim
- Phase 1 场景可渲染

### 任务清单

1. 加载 Knight 模型 (knight.dmesh)
2. 配置 Animator3D FSM (knight.dskel + knight_idle.danim)
3. 实现 Idle ↔ Run 切换
4. 添加 Jump / Attack / Block 状态
5. 配置所有过渡条件

---

## Session 3 任务：Phase 3 角色控制

### 任务清单

1. 编写 `player.lua`
2. WASD 移动（CharacterController3D）
3. 空格跳跃
4. 鼠标左键攻击
5. 鼠标右键格挡
6. 摄像机第三人称跟随
7. 角色朝向跟随移动方向

---

## Session 4 任务：Phase 4 敌人 AI

### 任务清单

1. 编写 `enemy_zombie.lua`
2. Zombie 模型 + 动画 FSM
3. AI 状态机：Idle/Follow/Attack/Damaged/Dying
4. 距离检测触发追踪
5. 多 Zombie 生成管理

---

## Session 5 任务：Phase 5 战斗系统

### 任务清单

1. 编写 `combat.lua`
2. 攻击碰撞检测
3. 伤害计算
4. HP 管理
5. 受击/死亡触发
6. 胜负判定

---

## Session 6 任务：Phase 6 游戏流程 + UI

### 任务清单

1. Title/Result UI 实现
2. BGM 管理
3. HP 条 HUD
4. Fade 过渡
5. 完整 Title → Battle → Result 循环

---

## Session 7 任务：Phase 7 音效 + 打磨

### 任务清单

1. 攻击/受击/格挡/死亡/UI 音效
2. 3D 空间音频
3. 场景装饰补充
4. 材质调整
5. 最终验收

---

## 通用规则

- 所有回复使用**中文**
- 优先使用现有 Lua API，不轻易修改 C++ 引擎代码
- 每个 Session 结束时：总结进度 + 更新 `examples/KF_Framework/PLAN.md` + 提交推送
- 遇到引擎能力不足时：记录到 gap list，用占位方案继续，后续 Session 补充
- 脚本文件保持每个 <300 行，复杂逻辑分文件

---

## 参考：KF_Framework 核心架构

```
KF_Framework 核心循环：
MainSystem::Update()  → Input → Physics → GameObjects → AI
MainSystem::LateUpdate() → Camera → PostPhysics
MainSystem::Render()  → ShadowMap → 3D → 2D → UI

角色系统：
GameObject → [Transform, MeshRenderer3DSkin, Rigidbody3D, Animator, PlayerController/EnemyController]
Controller → ActorState (状态机) → MotionState (动画状态机)

Knight 动作：idle, run, walk, jump, fall, land, light_attack x3, strong_attack x3, block, impact, death, magic, skill
Zombie 动作：idle, walk, running, attack(kick/punch), damaged, death, scream, warning, crawl

战斗参数：
Player: HP=100, ATK=15, DEF=3, Speed=5, JumpSpeed=8
Zombie: HP=30, ATK=8, DEF=1, Speed=2.5
```
