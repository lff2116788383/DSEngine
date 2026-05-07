# DSEngine 下一步方向规划

> 生成日期：2026-05-07
> 前提：编辑器功能已全部完成（Phase 1–7.6），无需继续扩展编辑器。

---

## 结论：编辑器功能已足够

当前编辑器覆盖独立游戏开发全流程所需的核心编辑能力：

- Inspector (20+ 组件) / Hierarchy (搜索/拖拽/父子) / Viewport (Gizmo/框选/Color-ID)
- Tilemap / Terrain (Sculpt+Splat) / Animation Timeline / Material PBR
- Prefab / 多场景标签 / Undo-Redo (深度集成) / Play Mode
- Particle Curve Editor / Light Probe 可视化 / Console 跳转源码

功能密度已超过同级别轻量引擎（Hazel、Razix、Piccolo 等），**不需要继续加编辑器功能。**

---

## 优先级排序

| 优先级 | 方向 | 理由 |
|--------|------|------|
| **P0** | Game Build/Export 管线 | 引擎有完整 runtime+editor，但无法打包为可发布游戏 |
| **P0** | Lua 热重载 + 调试体验 | 22 个 Lua binding 是引擎最大脚本面，编辑器缺 REPL/热重载按钮 |
| **P1** | 示例/文档完善 | 32 个 3D demo 丰富，但缺入门教程和 API 文档 |
| **P1** ✅ | 测试稳定性 | 12 个编辑器功能测试 + 218 集成测试全部通过 |
| **P2** | 跨平台 (Linux/macOS) | 当前仅 Windows MSVC，可渐进支持 |
| **P2** | 网络/多人同步 | 如需联网游戏，需 ECS 网络同步层 |

---

## P0-A: Game Build/Export 管线

### 目标
编辑器 File → Build Game，将场景+资源+Lua 脚本打包为独立可执行文件。

### 实施方案

1. **Asset Packing** — 将 `data/` 中引用到的资源打包为 `.dpak` 文件（类 zip 容器）
   - 场景 JSON 扫描引用的资源路径
   - 递归收集 + 去重 + 压缩 (zstd/lz4)
   - 生成 TOC (Table of Contents) 供 runtime 随机访问

2. **Runtime Loader** — `AssetManager` 新增 `.dpak` 读取路径
   - 优先从 pak 读，fallback 到文件系统（开发模式）
   - 内存映射 (mmap/MapViewOfFile) 零拷贝读取

3. **Standalone Executable** — 静态链接 `dse_engine` + `lua_host` → `game.exe`
   - CMake 新增 `apps/standalone/` target
   - 启动参数：`--scene=main.dscene --pak=game.dpak`
   - 无编辑器 UI，纯 runtime 循环

4. **Editor Integration**
   - File → Build Game 对话框：选择平台/输出路径/压缩级别
   - 调用 `asset_builder` 工具 + CMake 构建 standalone target
   - 进度条 + Console 输出构建日志

### 预期文件结构
```
apps/standalone/
├── CMakeLists.txt
├── main.cpp          # 纯 runtime 入口
└── pak_loader.h/cpp  # .dpak 文件读取

apps/tools/asset_builder/
├── CMakeLists.txt
├── main.cpp          # CLI 工具：扫描场景 → 打包资源
└── pak_writer.h/cpp  # .dpak 写入
```

---

## P0-B: Lua 开发体验提升

### 目标
编辑器内嵌 Lua REPL + 脚本热重载，缩短 Lua 开发迭代周期。

### 实施方案

1. **Lua Console (REPL)**
   - 编辑器新增 Lua Console 面板（类似 Unity Immediate Window）
   - 输入框 + 历史记录 + 自动补全（基于 registry 中的全局表）
   - 执行结果/错误显示在 Console 面板

2. **Lua 热重载**
   - 利用现有 `AssetManager::PumpHotReloads()` 监听 `.lua` 文件变更
   - 变更时重新 `dofile()` 对应脚本（保持 ECS 状态不变）
   - Play 模式下自动热重载，Edit 模式下仅标记 dirty

3. **错误定位**
   - Lua 运行时错误自动解析 traceback → 跳转到源码行（复用 Console 跳转功能）
   - `pcall` 包装 + 格式化错误消息带 `file.lua:line`

4. **Lua 断点 (可选, P2)**
   - 集成 `lua_sethook` + 简易调试协议
   - 编辑器面板显示调用栈 + 局部变量

---

## P1-A: 文档与示例

### 目标
让新开发者 10 分钟内能运行引擎并创建一个场景。

### 内容

1. **README.md 重写** ✅ — Quick Start (克隆→构建→运行编辑器→新建场景)
2. **docs/GETTING_STARTED.md** ✅ — 详细构建指南 (依赖/CMake 选项/IDE 配置)
3. **docs/LUA_API.md** ✅ — Lua binding 参考文档（自动从 14 个 binding 源文件生成，~145 个 API）
4. **docs/ARCHITECTURE.md** — 引擎架构图 (模块依赖/数据流)
5. **截图/GIF** — 编辑器 UI 截图（满足 ROADMAP 最后一项验收标准）

---

## P1-B: 编辑器自动化测试 ✅

### 目标
防止编辑器回归，无头功能测试直接调用编辑器 API 验证核心功能。

### 实施结果

采用 **无头功能测试** 方案（直接 API 调用，非 GUI 模拟），已完成全部集成：

1. **CLI 参数支持** — `main.cpp` 支持 `--headless`（隐藏窗口）、`--scene`（指定场景）、`--max-frames`（帧数限制自动退出）、`--replay`/`--verify`（预留）
2. **EditorTestHarness** — `editor_test_harness.h/cpp`：CLI 参数解析 + 测试配置
3. **RegistrySnapshot** — `editor_snapshot.h/cpp`：注册表快照导出为 JSON + 差异比较（float 容差）
4. **GTest 功能测试** — 12 个测试用例，覆盖：
   - 实体创建/销毁
   - Undo/Redo（PropertyChange / Lambda / Compound / Merge / History+Clear）
   - 场景 Save/Load 往返
   - Prefab 保存/实例化往返
   - SceneTabManager 标签切换/脏状态
   - 注册表快照导出/比较
   - CLI 参数解析
   - CopyRegistry 深拷贝
5. **CI 集成** — `verify_all.bat` 步骤 3b 独立运行 `EditorFunctional*` 测试

### 测试结果
- **12/12 编辑器测试全部通过**
- **218/218 集成测试无回归**（含 206 个原有测试）

### 新增文件
```
apps/editor_cpp/src/editor_test_harness.h/cpp   # CLI 解析 + 测试配置
apps/editor_cpp/src/editor_snapshot.h/cpp        # 注册表快照导出/比较
tests/gtest/integration/editor/
├── editor_functional_test.cpp                   # 12 个 GTest 用例
└── editor_test_stubs.cpp                        # 无头测试桩（UndoRedo/EditorLog/ScenePath）
```

---

## P2: 跨平台与网络（远期）

- **Linux**: 替换 `OPENFILENAMEW` → tinyfiledialogs, 替换 `ReadDirectoryChangesW` → inotify
- **macOS**: Metal RHI 后端（结构同 Vulkan 后端，5 子系统模式）
- **网络**: ECS 快照同步 + 客户端预测 + 服务端权威

---

## 建议执行顺序

```
Session 1: P0-B Lua Console (REPL) — 编辑器面板 + 执行引擎 ✅
Session 2: P0-B Lua 热重载 — Edit 模式 PumpLuaScriptHotReloads ✅
Session 3: P0-A Asset Packing (.dpak) — pak_format/writer/reader/scanner + AssetManager 集成 ✅
Session 4: P0-A Standalone exe — apps/standalone/ + /utf-8 修复 ✅
Session 5: P0-A Editor Build 对话框 — File → Build Game... + BrowseFolderDialog ✅
Session 6: P1-A 文档 — README 重写 + GETTING_STARTED.md ✅
Session 7: P1-B 编辑器自动化测试 — 无头功能测试 12 用例 + CI 集成 ✅
```
