# KF_Framework 下一轮对齐任务指令

## 总目标
严格对比 KF 源码，修复 DSEngine KF_Framework Demo 中 **骑士攻击动画/过渡**、**移动速度**、**Mutant 纹理**、**Mutant 血条** 四大问题，实现与原版 1:1 复刻。

---

## 重要路径
- **DSEngine 根目录**: `C:\Users\Administrator\Desktop\Engine\DSEngine\`
- **KF 源码目录**: `C:\Users\Administrator\Desktop\temp_analysis\KF_Framework\source_code\`
- **KF 数据目录**: `C:\Users\Administrator\Desktop\temp_analysis\KF_Framework\data\`
- **DSE 示例目录**: `examples\KF_Framework\`
- **视觉对比脚本**: `examples\KF_Framework\tools\visual_compare.py`
- **构建命令**: `cmake --build build_vs2022 --target dse_standalone --config Release`
- **运行命令**: `.\bin\DSEngine_Game_release.exe --script=examples\KF_Framework\script\main.lua --rhi=opengl`

## 核心 Lua 文件
- `script/player.lua` — 骑士 FSM + 输入 + 移动 + 摄像机
- `script/enemy.lua` — Mutant AI + 纹理 + 血条
- `script/config.lua` — 所有参数定义 (ASSET, CAMERA, PLAYER, ENEMY)
- `script/scene.lua` — 场景 (光照, 天空盒, 地形)
- `script/main.lua` — 入口 + 战斗判定

---

## 任务 A: 骑士攻击动画与过渡修复

### 问题描述
骑士的攻击动画 (attack1/attack2/attack3) 过渡完全不正常，表现为动画跳切、连招不流畅、退出时机错误。

### 排查步骤
1. **查看 KF 源码 FSM 定义**:
   - `source_code/actor/knight/` 下的各 state 文件 (idle_state, motion_state, step1_state, step2_state, step3_state 等)
   - 重点关注: 每个攻击 state 的 `Init()`/`Update()`/`Uninit()` 逻辑
   - KF 的攻击连招是 step1→step2→step3，每个 step 有 `kBeginAttackFrame` 和 `kEndAttackFrame`
   - 注意 KF 动画速率 (`SetSpeed`)、混合时间 (`kBlendFrame`)、连招输入窗口

2. **核对 DSE 当前 FSM**:
   - `player.lua` 行 153-213 定义了 13 个 state 和所有 transition
   - 当前 attack1→attack2→attack3 的 exit_time=0.85, blend=0.02，需与 KF 源码逐一对比
   - 攻击→idle 的 exit_time=0.95, blend=0.167，可能过早/过晚

3. **修复方向**:
   - 从 KF 源码提取精确的 blend frame 数、动画总帧数、attack window
   - 将 KF 帧数转换为 DSE normalized_time: `norm_time = frame / total_frames`
   - 调整 `ecs.add_animator_3d_state` 的 speed 参数
   - 调整 `ecs.add_animator_3d_transition` 的 blend_duration, exit_time 参数
   - 检查 trigger 参数 "attack" 的消耗时机是否正确

4. **KF 关键源文件**:
   - `source_code/actor/knight/step1_attack_state.cpp/h`
   - `source_code/actor/knight/step2_attack_state.cpp/h`
   - `source_code/actor/knight/step3_attack_state.cpp/h`
   - `source_code/actor/knight/motion_state.cpp/h`
   - `source_code/actor/knight/idle_state.cpp/h`
   - `source_code/actor/actor_state_machine.cpp/h`
   - `source_code/actor/actor_parameter.h` (动画速度、帧数常量)

---

## 任务 B: 骑士移动速度修复

### 问题描述
移动速度可能仍未精确对齐 KF，跑步时感觉偏快或偏慢。

### 排查步骤
1. **查看 KF 源码移动计算**:
   - `source_code/actor/actor_controller.cpp` 中 `Move()` 函数
   - `source_code/actor/actor_parameter.h` 中 `move_speed` 定义
   - KF 的 `move_amount = min(movement.Magnitude(), 1.0) * movement_multiplier`
   - `movement_multiplier` 在不同 state 下可能不同 (idle=1.0, attack=0.0 等)

2. **核对 DSE 当前实现**:
   - `player.lua` 行 231: `KF_MOVE_SPEED = 1000.0` (KF 10.0 × 100)
   - 注意: KF 的 `movement_multiplier` 是否在攻击/格挡时为 0？DSE 当前攻击时仍能移动？

3. **修复方向**:
   - 确认 KF 各 state 的 `movement_multiplier` 值
   - 在 DSE 中，攻击/格挡/受击/死亡状态应禁止移动 (设 speed=0)
   - 检查 walk 和 run 的动画播放速度是否与移速匹配

---

## 任务 C: Mutant 纹理修复

### 问题描述
Mutant 模型纹理显示异常 (可能是缺少纹理、UV 错误、或纹理路径错误)。

### 排查步骤
1. **查看 KF 源码 Mutant 资源配置**:
   - `source_code/actor/` 下 mutant 相关文件
   - KF `data/texture/` 下 mutant 的纹理文件列表
   - 确认 diffuse, normal, specular 三张纹理的正确路径

2. **核对 DSE 当前配置**:
   - `config.lua` 中 `ASSET` 表的 mutant 纹理路径
   - `enemy.lua` 中 `ecs.set_mesh_texture()` 调用
   - 确认纹理文件是否实际存在于 `assets/textures/` 目录

3. **修复方向**:
   - 对比 KF 的 mutant 材质参数 (diffuse color, specular, shininess)
   - 确认 shader variant 是否正确 (`MESH_HALFLAMBERT` vs 其他)
   - 检查 UV 是否需要翻转 (DSE OpenGL 加载纹理时 flip_v)

---

## 任务 D: Mutant 血条修复

### 问题描述
Mutant 头顶血条显示可能有位置偏移、大小不对、颜色不正确、或不随摄像机朝向更新等问题。

### 排查步骤
1. **查看 KF 源码 HP 条实现**:
   - `source_code/` 中 `EnemyUiController` 或类似的 UI 控制器
   - KF HP 条的世界坐标→屏幕坐标投影方式
   - HP 条的尺寸、颜色 (满血绿→残血红)、背景色、边框

2. **核对 DSE 当前实现**:
   - `enemy.lua` 中 HP 条相关代码 (HP_BAR_W=80, HP_BAR_H=8, HP_OFFSET_Y=400)
   - 世界坐标到屏幕坐标的转换逻辑
   - HP 条的 UI 元素创建和更新

3. **修复方向**:
   - 确保世界→屏幕投影使用正确的 MVP 矩阵
   - HP 条颜色应随 HP 百分比变化 (KF 通常: 绿→黄→红)
   - HP 条在敌人超出视距 (HP_DISPLAY_DIST=5000) 时隐藏
   - HP 条应始终面朝摄像机 (billboard)

---

## 工作流程

1. **每个任务开始前**: 先用 `grep_search` / `code_search` 定位 KF 源码中的精确实现
2. **修改前**: 阅读 KF 源码中的关键常量和逻辑，记录到注释中
3. **修改后**: 
   - 如果是引擎 C++ 改动: `cmake --build build_vs2022 --target dse_standalone --config Release`
   - 运行游戏验证: `.\bin\DSEngine_Game_release.exe --script=examples\KF_Framework\script\main.lua --rhi=opengl`
   - 用 `python tools/visual_compare.py` 截图对比
4. **全部完成后**: `git add` + `git commit -m "中文提交信息"` + `git push origin master`

## 约束
- **严格对齐源码**: 所有参数必须从 KF 源码提取，不得猜测
- **使用中文**: 所有提交信息和回复使用中文
- **最小改动**: 优先修改 Lua 脚本，仅在必要时修改引擎 C++
- **不删注释**: 不增删任何现有注释，除非用户要求
- **验证**: 每个任务完成后必须运行游戏验证效果
