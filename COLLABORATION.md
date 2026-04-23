# DSEngine 协作开发规范

**生效日期:** 2026-04-23
**适用范围:** DSEngine 当前主线（Lua 驱动 3D 原型实战化闭环）
**版本:** v2.0

---

## 1. 文档目标

本文档用于统一 DSEngine 当前阶段的协作方式、决策边界、开发流程与验证口径，确保多人协作或 AI 参与开发时，始终围绕以下现实目标推进：

- 先稳住引擎内部 bug、崩溃与 runtime 主链
- 用多重测试矩阵约束实现，降低 AI 幻觉与错误完成的风险
- 优先验证 **Lua + 3D runtime + 资产链** 是否足以支撑真实游戏原型开发
- 在技术路线未明确前，将编辑器投入后置

> **当前最高优先级不是继续扩功能面，而是提高“真实可运行、真实可验证、真实可迭代”的能力。**

---

## 2. 当前项目目标与阶段认知

### 2.1 当前项目定位

DSEngine 当前不是“成熟商用品质 3D 引擎”，而是一个已经具备以下基础的 brownfield 引擎仓库：

- C++ / Lua 双宿主 runtime
- 2D 稳定主线
- 已接入的 3D 组件、3D 场景与 3D 测试 gate
- 基础资产导入与烹饪链
- 基础原生编辑器外壳
- Windows + CMake + CTest 的最小工程化验证体系

当前项目的核心任务是验证：

**现有 runtime、3D 模块、Lua 绑定和资产链，是否足以支撑一个可持续迭代的 Lua 驱动 3D 游戏原型。**

### 2.2 当前阶段优先级

当前阶段的优先级顺序为：

1. **引擎稳定性优先**：先解决 bug、崩溃、超时、目标缺失和核心链路不稳定问题
2. **3D runtime 真实性优先**：先确认 3D gate 和最小场景是真能跑，而不是文档存在
3. **Lua 绑定真实性优先**：所有 Lua 能力必须来自真实绑定、真实样例和真实测试
4. **资产链可复现优先**：glTF/GLB/FBX 到运行时资产链必须能真实复现
5. **2D 稳定基线守护**：3D 推进不能把默认稳定主线拖坏
6. **编辑器后置**：在技术路线明确前，不把编辑器体验打磨作为前置核心任务

---

## 3. 角色与职责划分

### 3.1 角色定义

| 角色 | 职责范围 |
|------|---------|
| **项目负责人 (Owner)** | 决定里程碑目标、阶段优先级、范围边界、架构裁决、最终验收口径 |
| **开发者 / 执行者 (Developer / Executor)** | 按当前规划推进 Phase、修 bug、写测试、跑验证、更新文档、形成可交付结果 |
| **AI 协作者 (AI Pair / Agent)** | 基于真实代码库和测试体系做检索、规划、实现、验证与文档同步，但不得臆造 API、能力或测试结果 |

### 3.2 决策边界

| 决策类型 | 决策人 | 说明 |
|---------|--------|------|
| 项目核心目标与阶段优先级 | 项目负责人 | 需同步更新 `.planning/PROJECT.md`、`ROADMAP.md`、`STATE.md` |
| 是否将某能力定义为“已支持” | 项目负责人 + 事实依据 | 必须同时有代码/绑定/测试/样例中的至少两类证据 |
| Phase 内实施顺序 | 开发者 / AI 协作者 | 但必须服从当前 roadmap 的阶段目标 |
| Bug 修复方案 | 开发者 / AI 协作者 | 涉及架构边界时需按现有文档约束执行 |
| 新增第三方依赖、重大技术路线切换 | 项目负责人 | 禁止私自引入未授权依赖 |
| 编辑器路线是否恢复高优先级 | 项目负责人 | 需先证明其对 Lua 3D 内容生产有明确价值 |

### 3.3 核心协作原则

- **项目负责人定方向，执行者定落地方案**
- **AI 不拥有“宣称支持某能力”的权力，只拥有“基于证据验证某能力”的职责**
- **没有验证结果的实现，不算完成**
- **没有真实绑定的 Lua API，不允许进入计划、代码或文档**

---

## 4. 防幻觉协作原则

### 4.1 什么叫“AI 幻觉”

在本项目中，以下行为都属于高风险幻觉：

- 臆造某个 Lua API、3D 组件接口或资产链能力已存在
- 只看文档名或测试名，就断言功能真实可用
- 没有跑测试却声称“验证通过”
- 把 reference 工程能力误当成主仓真实能力
- 把“能编译”误当成“能运行”
- 把“有 demo”误当成“能支持真实项目开发”

### 4.2 防幻觉底线

所有协作者，尤其是 AI，必须遵守以下底线：

1. **无源码证据，不宣称 API 存在**
2. **无绑定证据，不宣称 Lua 可调用**
3. **无测试结果，不宣称已验证通过**
4. **无样例或 smoke，不宣称具备实战能力**
5. **无文档同步，不宣称能力边界已稳定**

### 4.3 Lua / 3D 相关特别规则

当判断“Lua 是否能驱动某个 3D 能力”时，至少要同时检查：

- `engine/scripting/lua/bindings/` 中是否存在真实绑定
- 相关能力是否在 `samples/lua/` 或现有 demo 中被真实使用
- 是否有对应的测试、smoke 或最小场景入口

> **任何只存在于设想、注释、旧文档或 reference 工程中的能力，都不能算主仓已支持。**

---

## 5. 当前测试策略：多重测试矩阵

### 5.1 目标

测试的目标不只是发现 bug，更是防止协作者（尤其是 AI）对代码状态产生错误判断。

因此，DSEngine 当前采用 **多重测试矩阵**，而不是依赖单一测试命令。

### 5.2 四层测试矩阵

#### 第一层：源码事实层

用于确认“功能是否真实存在”：

- 检索真实源码
- 检索真实绑定
- 检索真实样例
- 检索真实测试入口

适用场景：

- 判断 Lua API 是否存在
- 判断 3D 组件是否真实接入
- 判断资产链支持范围是否真实成立

#### 第二层：构建层

用于确认“代码是否能真实编过”：

```bat
cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 -DDSE_BUILD_EDITOR=OFF -DDSE_BUILD_LAUNCHER=OFF -DDSE_BUILD_ENGINE_TESTS=ON
cmake --build build_vs2022 --config Debug --target dse_engine_unit_tests dse_lua_runtime_tests dse_lua_runtime_core_single_test dse_lua_runtime_smoke_single_test_v2 dse_lua_resource_injection_single_test -- /m:1 /nologo
```

适用场景：

- 修改 runtime
- 修改 Lua runtime
- 修改核心测试入口
- 修改 3D / asset chain 相关 target

#### 第三层：最小回归层

用于确认“核心主链是否还活着”。

**runtime / 双宿主最小回归：**

```bat
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.cpp_runtime|engine.lua_runtime|engine.resource_injection|engine.unit|engine.lua_runtime.smoke"
```

**3D 最小回归矩阵：**

```bat
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.3d.unit|engine.3d.scene_mvp|engine.3d.runtime_mvp_smoke"
```

**3D 资产链门禁：**

```bat
ctest --test-dir build_vs2022 -C Debug --output-on-failure -R "engine.asset_compiler"
```

#### 第四层：实战样例层

用于确认“不是只会过测试，而是真的能支撑原型开发”：

- 启动最小 3D 场景
- 启动 Lua 3D sample / reference demo
- 验证相机、灯光、mesh、材质、场景、脚本更新链是否真实跑通
- 在有图形上下文环境时补跑环境依赖型 smoke

### 5.3 测试结论表达规范

输出测试结果时必须区分：

- **已通过**：实际执行且通过
- **已构建但未运行**：目标产物存在，但未执行
- **环境依赖暂缓**：例如图形上下文缺失
- **失败**：实际执行失败
- **未知**：未检查，禁止写成通过

禁止使用以下模糊说法：

- “应该没问题”
- “理论上支持”
- “看起来已经接入”
- “大概率可以用”

---

## 6. 当前阶段的工作顺序

### 6.1 总原则

**先清 bug / 崩溃 / 回归，再扩功能面。**

当前第一个后续任务，不是继续追加新 3D 功能，而是：

- 跑通当前 runtime / Lua / 3D / asset 的测试矩阵
- 找出 bug、崩溃、超时、目标缺失和假阳性
- 修复阻塞 3D 原型推进的内部问题
- 记录哪些能力是真正稳定的，哪些只是“已接入未收口”

### 6.2 当前推荐优先级

1. **引擎稳定性与崩溃清零**
2. **3D runtime 最小矩阵真实性确认**
3. **Lua 3D 绑定真实性确认**
4. **资产导入与烹饪链复现能力确认**
5. **Lua 3D playable prototype 基线建立**
6. **2D 稳定主线守护**
7. **测试与文档统一**
8. **编辑器路线重估**

---

## 7. GSD 协作流程（适配当前项目）

### 7.1 推荐命令顺序

当前项目推进建议优先使用以下 GSD 流程：

```text
/gsd-progress
    ↓
gsd-discuss-phase 明确本 Phase 的拆解方式
    ↓
/gsd-plan-phase N
    ↓
/gsd-execute-phase
    ↓
执行最小验证矩阵
    ↓
/gsd-verify-work
    ↓
更新 SUMMARY / STATE / 文档
```

### 7.2 Phase 规划要求

每个新 Phase 的 `PLAN.md` 至少应包含：

- 目标能力边界
- 真实依赖文件
- 风险点
- 需要补跑的测试矩阵
- 手工验证项（若有图形环境依赖）
- 明确哪些能力是“已支持”、哪些只是“待验证”

### 7.3 当前阶段特别要求

若 Phase 涉及以下任一方向，必须先补充或执行对应验证：

| 修改方向 | 最少验证 |
|---------|---------|
| `engine/runtime/` | `engine.cpp_runtime` + `engine.lua_runtime` + `engine.resource_injection` + `engine.unit` |
| `modules/gameplay_3d/` | `engine.3d.unit` + `engine.3d.scene_mvp` + `engine.3d.runtime_mvp_smoke` |
| `engine/scripting/lua/` | 绑定源码检查 + `engine.lua_runtime` + 对应 Lua sample / smoke |
| `engine/assets/` / importer / cooker | `engine.asset_compiler` + 相关 runtime smoke |
| `assets/scenes/*.scene.json` | `engine.3d.scene_mvp` 或对应 scene / smoke gate |

---

## 8. 文档同步规则

当发生以下情况时，必须同步更新文档：

- 阶段优先级改变
- 测试矩阵改变
- 3D / Lua / asset 的支持边界改变
- 新增或删除某个核心 gate
- 把某项能力从“已接入”提升为“已稳定支持”

优先同步的文档包括：

- `.planning/PROJECT.md`
- `.planning/REQUIREMENTS.md`
- `.planning/ROADMAP.md`
- `.planning/STATE.md`
- `doc-archive/TESTING_CTEST_GUIDE.md`
- 与 3D / Lua / asset 相关的专题文档

---

## 9. 禁止事项

以下行为在当前项目阶段明确禁止：

- 未经确认就宣称 Lua 已支持某个 3D API
- 未经测试就标记任务完成
- 为了“看起来先进”而盲目扩大 3D 能力范围
- 在 bug / 崩溃 / gate 未收口前继续大规模铺新功能
- 把 reference 工程代码或资源直接视作主仓已支持能力
- 为了编辑器视觉效果而提前转移主线精力
- 未经授权引入新第三方依赖或大改技术栈

---

## 10. 协作输出格式要求

提交结果或阶段总结时，应尽量按以下结构输出：

1. **本次改动目标**
2. **已修改文件**
3. **已执行验证命令**
4. **测试结果分类**（通过 / 失败 / 暂缓 / 未执行）
5. **当前真实能力边界**
6. **剩余风险与下一步**

> **协作的核心不是“看起来推进了很多”，而是始终清楚：现在到底真的能做什么。**

---

## 11. 当前一句话协作共识

**DSEngine 当前阶段的一切协作，都应服务于“先把引擎内部问题和测试矩阵收稳，再验证 Lua 驱动 3D 原型是否真实成立”，而不是继续凭感觉扩面。**
