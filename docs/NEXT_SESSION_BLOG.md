# DSEngine Blog 写作会话指令

> 生成日期：2026-05-15
> 目标：补齐 `docs/blog/` 系列科普文章，向小白讲清楚 DSEngine 涉及的核心技术

---

## 一、已有文章（不要重复写）

| 文件 | 主题 |
|:----|:-----|
| `computer-graphics-basics-for-beginners.md` | 图形学全景入门（渲染管线/MVP/光栅化） |
| `anti-aliasing-for-beginners.md` | 抗锯齿（SSAA/MSAA/FXAA/TAA） |
| `game-animation-from-traditional-to-deep-learning.md` | 游戏动画进化史 |
| `photon-mapping-for-beginners.md` | 光子映射全局光照 |
| `rtxgi-ddgi-for-beginners.md` | RTXGI/DDGI 实时全局光照 |
| `differentiable-rendering-for-beginners.md` | 可微渲染前沿技术 |
| `pbr-for-beginners.md` | PBR 基于物理的渲染 |
| `shader-for-beginners.md` | 着色器（Vertex/Fragment Shader） |
| `deferred-vs-forward-for-beginners.md` | 延迟渲染 vs 前向渲染 |
| `shadow-mapping-for-beginners.md` | 阴影贴图（CSM/PCSS） |
| `ecs-for-beginners.md` | ECS 实体-组件-系统 |
| `service-locator-for-beginners.md` | ServiceLocator 服务定位器 |
| `job-system-for-beginners.md` | JobSystem 工作窃取算法 |
| `bloom-for-beginners.md` | Bloom 泛光效果 |
| `physx-for-beginners.md` | PhysX 物理引擎 |
| `ik-for-beginners.md` | IK 反向运动学 |
| `lod-for-beginners.md` | LOD 细节级别 |
| `gpu-instancing-for-beginners.md` | GPU Instancing 实例化 |
| `lighting-models-evolution-for-beginners.md` | 光照模型进化史 |

---

## 二、写作规范

### 风格要求

沿用 `computer-graphics-basics-for-beginners.md` 和 `anti-aliasing-for-beginners.md` 的风格：

- **标题**：用"大白话"式标题，带问号或幽默感（如"反正是锯齿，干掉就完事了"）
- **目录**：文章头部有目录，锚点链接到各章节
- **行文**：用"你"来拉近距离，用生活场景打比方
- **类比**：每个技术点至少配一个生活场景类比
- **表格**：对比类内容用 markdown 表格
- **代码块**：不贴实际引擎源码（除非特别必要），用伪代码或流程图
- **emoji**：适当使用 ✅ ❌ ⭐ 🎯 等辅助阅读
- **结尾**：总结段落 + 一句幽默收尾
- **字数**：每篇 2000~4000 字，太短讲不透，太长没人看
- **难度**：不假定读者有任何图形学基础，能让高中生看懂

### 文件命名

`docs/blog/<英文短横线命名>.md`，如 `pbr-for-beginners.md`

### 必须包含的要素

1. 目录（锚点链接）
2. 至少 3 个生活场景类比
3. 至少 1 个对比表格（如有对比内容）
4. "一句话总结"性质的结束语

### 禁止

- 不要在文章里写 "DSEngine 实现了 XXX"（除非特别标记为"DSEngine 实测"章节）
- 不要大段贴源码
- 不要用数学公式（不要 LaTeX）
- 不要假设读者懂编程

---

## 三、文章清单（按优先级排序）

### Session 1：渲染核心（4 篇，已写完）

> DSEngine 渲染管线的理论基础，读完能理解引擎的渲染流程怎么工作的。

#### 第 1 篇：PBR 渲染到底是个啥？

**目标**：彻底讲清楚 PBR 的核心思想——金属/粗糙度工作流、能量守恒、微表面模型。

**参考代码位置（供素材提取，不直接引用）：**
- [pbr.frag](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/shaders/src/pbr.frag) — PBR 着色器主逻辑
- [ubo_types.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/ubo_types.h) — PBR 材质参数定义

**建议内容大纲：**
1. PBR 之前：游戏画面为什么"假"？（固定函数光照的局限性）
2. 核心一：能量守恒——光不会凭空产生也不会消失
3. 核心二：金属 vs 非金属——反光方式完全不一样
4. 核心三：粗糙度——决定了高光是一个点还是一大片
5. PBR 的"三件套"：Albedo（底色）、Normal（凹凸）、MR（金属度+粗糙度）
6. 一句话总结

#### 第 2 篇：着色器（Shader）到底是啥？

**目标**：讲清楚 GPU 编程和 CPU 编程的区别，vertex shader 和 fragment shader 分别干什么。

**参考代码位置：**
- [builtin_passes.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/passes/builtin_passes.cpp) — 各个 Pass 的执行逻辑
- [gl_draw_executor.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/gl_draw_executor.cpp) — inline shader 源码示例

**建议内容大纲：**
1. CPU vs GPU：一个像几个博士生（串行），一个像一万个小学生（并行）
2. 什么是"着色器"？——给 GPU 的"怎么画"指令
3. Vertex Shader：每个顶点怎么动（位置/变形）
4. Fragment Shader：每个像素什么颜色（光照/纹理）
5. 渲染管线的完整流水图（引用现有 blog 渲染管线章节）
6. 一句话总结：着色器 = 告诉显卡"怎么画"的程序

#### 第 3 篇：延迟渲染 vs 前向渲染

**目标**：讲清楚两种渲染路径的区别、各自的优劣势、为什么 DSEngine 两者都支持。

**参考代码位置：**
- [frame_pipeline.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/runtime/frame_pipeline.cpp) — 渲染流程调度
- [builtin_passes.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/passes/builtin_passes.cpp) — 含 DeferredLightingPass

**建议内容大纲：**
1. 前向渲染：每个物体自己算光照——简单直接
2. 问题来了：100 盏灯，每个物体都要算 100 次——性能炸了
3. 延迟渲染：先把物体存进 GBuffer，再统一算光照
4. GBuffer 是什么？——几个"图层"各存各的信息
5. 延迟渲染的优缺点：性能好但吃显存、不支持透明物体
6. 对比表格：前向 vs 延迟
7. DSEngine 实际做法：Clustered Forward+ + 可选 Deferred 路径

#### 第 4 篇：游戏里的影子是怎么来的？

**目标**：讲清楚 Shadow Mapping 的核心原理、CSM 级联、PCSS 软阴影。

**参考代码位置：**
- [builtin_passes.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/passes/builtin_passes.cpp) — CSMShadowPass/SpotShadowPass/PointShadowPass
- [frame_pipeline.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/runtime/frame_pipeline.cpp#L241-L247) — 阴影贴图分辨率配置

**建议内容大纲：**
1. 影子本质：光源看不到的地方就是阴影
2. Shadow Mapping：从光源拍一张"深度照片"
3. 阴影的质量问题：锯齿（阴影痤疮）和飘动（Peter Panning）
4. CSM：近处高清阴影、远处低清阴影——分成三层
5. PCSS：阴天和晴天的阴影为什么不一样软？
6. Contact Shadow：物体接触地面的精细阴影
7. 一句话总结：影子 = 从光源角度看物体被挡住了多少

---

### Session 2：引擎架构（3 篇，已写完）

> DSEngine 的核心设计理念，讲清楚它和传统引擎的架构差异。

#### 第 5 篇：ECS 到底好在哪？

**目标**：用对比方式讲清楚 ECS 和传统 OOP 继承的区别，为什么 DSEngine 选 ECS。

**参考代码位置：**
- [ecs/](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/ecs/) — 组件定义
- [world.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/ecs/world.h) — World 接口

**建议内容大纲：**
1. 传统 OOP 做法：`class Car extends Vehicle`——继承链越深越难维护
2. ECS 做法：实体 = 空盒子，组件 = 零件，系统 = 装配工人
3. 为什么游戏喜欢 ECS：性能好（内存连续）、灵活（随意组合）、易扩展
4. 类比：乐高 vs 传统积木
5. 对比表格：OOP vs ECS

#### 第 6 篇：ServiceLocator（服务定位器）是啥？

**目标**：讲清楚为什么需要服务定位器，它怎么替代了"到处写全局变量"。

**参考代码位置：**
- [service_locator.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/core/service_locator.h)

**建议内容大纲：**
1. 全局变量的痛点：谁都能改，改出问题找不到是谁干的
2. 依赖注入的思路：你要的东西，别人从外面给你
3. ServiceLocator：一个"前台"，所有服务都去前台拿
4. 好处：测试时可以用假服务代替真服务
5. 生活中的类比：酒店前台、公司总机

#### 第 7 篇：JobSystem——怎么让 CPU 所有核心一起干活？

**目标**：讲清楚多线程任务系统的基本原理，工作窃取算法。

**参考代码位置：**
- [job_system.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/core/job_system.h)

**建议内容大纲：**
1. 单线程的问题：一个工人干所有活
2. 多线程的问题：分工难、抢资源、死锁
3. JobSystem：把大任务拆成小任务，自动分配给空闲核心
4. 工作窃取：你的活干完了？去帮别人干
5. 生活中的类比：快递分拣中心、搬家公司

---

### Session 3：后处理 + 物理 + 动画（3 篇，已写完）

> 覆盖面拉宽，补齐素材引擎已有功能的科普。

#### 第 8 篇：Bloom——为什么亮的东西会发光？

**目标**：讲清楚泛光效果的原理：提取亮部 → 模糊 → 叠加。

**参考代码位置：**
- [builtin_passes.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/passes/builtin_passes.cpp) — BloomPass
- [gl_draw_executor.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/gl_draw_executor.cpp#L1174L1293) — Bloom composite shader

**建议内容大纲：**
1. 人眼对亮部有"光晕"感——Bloom 模拟这个
2. 第一步：把画面里的亮部提取出来
3. 第二步：不断缩小再放大（降采样+升采样）——产生模糊
4. 第三步：把模糊的亮部和原图叠加
5. 生活中的类比：手机拍夜景、阳光下的灯

#### 第 9 篇：PhysX 在后台偷偷干了什么？

**目标**：讲清楚游戏物理引擎的基本工作原理。

**参考代码位置：**
- [physics3d_system.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/physics/physics3d/physics3d_system.h)

**建议内容大纲：**
1. 物理引擎的"三步曲"：检测碰撞 → 算力 → 更新位置
2. 刚体 vs 软体 vs 布料：不同材质的物理模拟方式
3. 碰撞体类型：盒子、球体、胶囊、网格——精度 vs 性能的取舍
4. 关节：铰链、弹簧、滑轨——物体之间的连接方式
5. 布娃娃：角色死亡时的物理模拟
6. 生活中的类比：台球（刚体碰撞）、果冻（软体）、窗帘（布料）

#### 第 10 篇：IK——手怎么自动去抓东西？

**目标**：讲清楚正向运动学和反向运动学的区别。

**参考代码位置：**
- [ik_solver_system.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/modules/gameplay_3d/animation/ik_solver_system.h)
- [anim_layer_ik_test.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/tests/gtest/unit/modules/gameplay_3d/anim_layer_ik_test.cpp)

**建议内容大纲：**
1. 正向运动学（FK）：从肩膀到手指，一步步计算每个关节的位置
2. 反向运动学（IK）：知道手要放哪，反推肩膀/手肘怎么弯
3. FABRIK 算法：来回"够"几次就找到答案
4. 生活中的类比：伸手拿杯子、踩踏板
5. FK vs IK 对比表格

---

### Session 4：性能 + 工具（3 篇，已写完）

> 游戏引擎中的性能优化技巧与工具。

#### 第 11 篇：LOD——远处的山为什么看不清？

**参考代码位置：**
- [lod_system.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/modules/gameplay_3d/rendering/lod_system.h)
- [lod_system_test.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/tests/gtest/unit/modules/gameplay_3d/lod_system_test.cpp)

**大纲要点：** 近处高清/远处低清、hysteresis 防抖动、屏幕空间投影公式的直观理解。

#### 第 12 篇：GPU Instancing——一千棵树一次画完

**参考代码位置：**
- [rhi_types.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/rhi_types.h) — MeshDrawItem::instance_transforms
- [mesh_render_system.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/modules/gameplay_3d/rendering/mesh_render_system.cpp)

**大纲要点：** Draw Call 是什么、为什么 Draw Call 多了会卡、Instancing 怎么合并。

#### 第 13 篇：光照模型进化史——从 Lambert 到 PBR

**大纲要点：** Lambert（漫反射）→ Phong（高光）→ Blinn-Phong（更快的高光）→ PBR GGX（真实物理）一路串起来。不需要代码参考，纯概念科普。

---

### Session 5：扩展覆盖（7 篇，待写）

> 把覆盖面拉得更宽，覆盖 DSEngine 中的其他重要子系统——粒子、音频、动画、脚本、资源管理、事件系统、输入系统。

#### 第 14 篇：粒子系统——火焰、烟雾、魔法特效是怎么飞起来的？

**目标**：讲清楚粒子系统的核心概念：发射器、粒子生命周期、GPU 粒子。

**参考代码位置：**
- [components_3d_particle.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/ecs/components_3d_particle.h) — 3D 粒子组件
- [particle_2d.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/ecs/particle_2d.h) — 2D 粒子组件
- [gl_draw_executor.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/gl_draw_executor.cpp) — 粒子绘制

**建议内容大纲：**
1. 粒子就是"很多很多小东西"——烟花、火焰、魔法都是粒子
2. 发射器：粒子的"出生点"
3. 粒子的生命周期：出生 → 运动 → 变化 → 死亡
4. CPU 粒子 vs GPU 粒子
5. 生活中的类比：烟花、喷泉、星空的星星
6. 对比表格：粒子 vs 普通模型渲染

#### 第 15 篇：3D 音效——游戏里的声音怎么让你感觉"从背后来的"？

**目标**：讲清楚 3D 音频的基本原理：空间化、距离衰减、多普勒效应。

**参考代码位置：**
- [audio_system.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/audio/audio_system.h) — 音频系统接口
- [audio.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/ecs/audio.h) — 音频 ECS 组件

**建议内容大纲：**
1. 人耳怎么判断声音方向？（双耳效应、头部相关传输函数 HRTF）
2. 距离衰减：越远越轻
3. 多普勒效应：赛车经过时"呜——嗡——"的变化
4. 声锥与遮挡：墙后面的声音听起来不一样
5. 生活中的类比：蒙眼找人、火车经过的声音变化
6. 对比表格：2D 音效 vs 3D 音效

#### 第 16 篇：骨骼动画——游戏角色是怎么"活"起来的？

**目标**：讲清楚骨骼动画的完整工作流：骨骼、蒙皮、动画混合。

**参考代码位置：**
- [animation.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/ecs/animation.h) — 动画组件
- [lua_binding_ecs_animation.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/scripting/lua/bindings/lua_binding_ecs_animation.cpp) — 动画的 Lua 绑定

**建议内容大纲：**
1. 骨骼：角色的"内部骨架"
2. 蒙皮：让模型跟着骨骼动
3. 动画帧与插值：两个关键帧之间平滑过渡
4. 动画混合：走路到跑步的自然过渡
5. 生活中的类比：木偶戏、皮影戏
6. 对比表格：传统逐帧动画 vs 骨骼动画

#### 第 17 篇：Lua 脚本——游戏引擎为什么要"外挂"一门脚本语言？

**目标**：讲清楚脚本语言和引擎语言的关系、为什么游戏喜欢 Lua。

**参考代码位置：**
- [lua_runtime.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/scripting/lua/lua_runtime.h) — Lua 运行时
- [lua_binding_registry.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/scripting/lua/bindings/lua_binding_registry.cpp) — 绑定注册

**建议内容大纲：**
1. C++ 像"厨房大厨"——性能好但学起来难
2. Lua 像"点菜单"——简单灵活，改起来快
3. 为什么游戏喜欢 Lua：热更新、简单、安全
4. 绑定：让 Lua 和 C++ 能够"对话"
5. 生活中的类比：餐馆的点餐系统、App 的插件
6. 对比表格：C++ vs Lua

#### 第 18 篇：Asset 管理——游戏里的几千张贴图是怎么被管得井井有条的？

**目标**：讲清楚资源管理的核心概念：导入、打包、加载、缓存、生命周期。

**参考代码位置：**
- [asset_manager.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/assets/asset_manager.h) — 资产管理器
- [pak_writer.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/assets/pak_writer.h) / [pak_reader.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/assets/pak_reader.h) — PAK 打包读取

**建议内容大纲：**
1. 资源管理的"五步走"：发现 → 导入 → 打包 → 加载 → 卸载
2. PAK 打包：把几千个小文件合并成一个大文件
3. 异步加载：边玩游戏边加载，不卡顿
4. 引用计数：没人用的资源自动卸载
5. 生活中的类比：图书馆管理系统、快递仓库
6. 对比表格：散装文件 vs PAK 打包

#### 第 19 篇：EventBus（事件总线）——游戏里的"八卦消息传播系统"

**目标**：讲清楚发布-订阅模式，为什么比直接调用更好。

**参考代码位置：**
- [event_bus.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/core/event_bus.h) — EventBus 接口
- [event_id.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/core/event_id.h) — 事件 ID 定义

**建议内容大纲：**
1. 直接调用的痛点：A 通知 B，但 B 可能不存在
2. 发布-订阅模式：有人广播，感兴趣的人自己听
3. EventBus 怎么工作：发布者不关心谁在听
4. 好处：解耦合、易扩展、易测试
5. 生活中的类比：微信群聊、广播站、论坛
6. 对比表格：直接调用 vs EventBus

#### 第 20 篇：输入系统——键盘、鼠标、手柄，游戏怎么知道你按了什么？

**目标**：讲清楚输入系统的层次：硬件输入 → 抽象映射 → 动作响应。

**参考代码位置：**
- [input.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/input/input.h) — 输入系统
- [action_mapping.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/input/action_mapping.h) — 动作映射
- [input_recorder.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/input/input_recorder.h) — 输入录制回放

**建议内容大纲：**
1. 从物理按键到游戏动作：键盘的 W 键 → 角色"向前走"
2. Action Mapping：把"按 W"映射为"向前移动"
3. 输入录制与回放：自动测试和过场动画的基石
4. 输入缓冲：格斗游戏里的"提前输入"是怎么回事
5. 生活中的类比：翻译官、遥控器、录音机
6. 对比表格：轮询 vs 事件驱动

#### 第 21 篇：图形 API——OpenGL、Vulkan、Direct3D、Metal……都是啥？

**目标**：讲清楚市面上所有主流图形 API 的区别——OpenGL/OpenGL ES、D3D11/D3D12、Vulkan、Metal、WebGL/WebGPU，各自的优劣势和适用平台。

**参考代码位置：**
- [rhi_factory.cpp](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/rhi_factory.cpp) — RHI 后端工厂创建逻辑
- [rhi_device.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/rhi_device.h) — RHI 抽象接口
- [vulkan_rhi_device.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/vulkan/vulkan_rhi_device.h) — Vulkan 后端
- [dx11_rhi_device.h](file:///c:/Users/wenbilin/Desktop/Engine/DSEngine/engine/render/rhi/dx11/dx11_rhi_device.h) — Direct3D 11 后端

**建议内容大纲：**
1. 什么是图形 API？—— 游戏和显卡之间的"翻译官"
2. OpenGL + OpenGL ES：跨平台的"老大哥"
3. Direct3D 11：Windows 上的"老将"
4. Direct3D 12：D3D11 的"激进升级版"
5. Vulkan：跨平台的"效率狂魔"
6. Metal：苹果的"自家看门人"
7. WebGPU + WebGL：浏览器里的图形世界
8. 一张表看懂所有图形 API（8 大 API 全对比）
9. 游戏引擎的 RHI 抽象层：一次编写，多端运行

---

## 四、执行建议

```
Session 1 (渲染核心，4篇，已写完):
  ├── PBR 到底是个啥
  ├── Shader 到底是啥
  ├── 延迟渲染 vs 前向渲染
  └── 影子是怎么来的

Session 2 (引擎架构，3篇，已写完):
  ├── ECS 到底好在哪
  ├── ServiceLocator 是啥
  └── JobSystem 怎么让所有核心一起干活

Session 3 (后处理+物理+动画，3篇，已写完):
  ├── Bloom 为什么亮的东西会发光
  ├── PhysX 在后台偷偷干了什么
  └── IK 手怎么自动去抓东西

Session 4 (性能+工具，3篇，已写完):
  ├── LOD 远处的山为什么看不清
  ├── GPU Instancing 一千棵树一次画完
  └── 光照模型进化史

Session 5 (扩展覆盖，8篇，待写):
  ├── 粒子系统 火焰烟雾魔法特效
  ├── 3D 音效 声音怎么从背后来的
  ├── 骨骼动画 角色怎么活起来的
  ├── Lua 脚本 为什么要外挂脚本语言
  ├── Asset 管理 资源怎么管得井井有条
  ├── EventBus 八卦消息传播系统
  ├── 输入系统 怎么知道你按了什么
  └── 图形API OpenGL/Vulkan/D3D11 都是啥
```

---

## 五、质量检查清单

每篇写完后确认：

- [ ] 标题是否足够"小白友好"（不要让读者一开始就被吓跑）
- [ ] 正文是否出现"顾名思义""显然""简单来说"等对小白不友好的词？
- [ ] 是否每 2~3 段就有一个生活类比？
- [ ] 是否有至少一张对比表格？
- [ ] 是否有目录（锚点链接）？
- [ ] 结尾是否有总结？
- [ ] 是否没有出现 LaTeX 数学公式？
- [ ] 是否没有大段贴源码？
- [ ] 字数是否在 2000~4000 字之间？
- [ ] 读一遍：一个完全不懂图形学的人能看懂吗？
