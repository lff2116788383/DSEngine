# Visual Script 退场与迁移结论（P0-5）

> 基线分支：`devin/1783670170-release-foundation`
> 结论日期：随本提交生效

## 1. 背景

编辑器早期存在两套节点式脚本系统：

- **Visual Script**（`editor_visual_script*.{h,cpp}`）：节点图 → **即时生成 Lua 文本**，仅在面板内展示，供人工复制。
- **Blueprint**（`editor_blueprint*`, engine 侧单编译器）：节点图 → **bytecode（+ CompileToLua 导出）**，editor 编译与 runtime 加载共用同一编译器，已在 P0-3 完成单源化与完整运行闭环。

V1 计划 P0-5 要求：Blueprint 稳定后，将 Visual Script 数据迁移并彻底退场，所有入口统一到 Blueprint。

## 2. 是否存在持久化数据 —— 结论：**从未存在**

对整个仓库（源码、项目、场景、prefab、示例、测试数据、文档）做了穷尽扫描，结论如下：

| 检查项 | 结果 |
|:------|:-----|
| `.vs` / `.visualscript` / `.vscript` 等专有数据文件 | **0 个** |
| project / scene / prefab / `.json` / `.dbp` 中引用 `VisualScript` / `visual_script` | **0 处** |
| Visual Script 代码里的 save/load/serialize/ofstream/ifstream/文件写出 | **0 处**（图状态仅存在于内存，`Compile to Lua` 只把结果字符串显示在面板） |
| engine / modules（runtime）中的 `VisualScript*` 消费入口或 `VisualScriptComponent` | **0 处**（无任何 runtime 侧消费者） |

即：Visual Script **从未定义过任何持久化格式**，也**从未有 runtime 侧消费路径**——它是一个纯编辑器内、纯内存、把节点图翻译成 Lua 文本给人看的工具面板。

## 3. 迁移决策

因为不存在任何持久化产物，也不存在 runtime 消费者：

- **不需要**编写 `.vs → .dbp` 迁移器（没有源数据可迁移）。
- **不需要**保留兼容层 / 双系统共存尾巴（V1 计划明确禁止"无意义兼容层"）。
- **不需要**输出逐文件迁移报告（无文件）。

因此本阶段的处理是：**直接、彻底删除 Visual Script 的全部代码与接线，所有可视脚本入口统一到 Blueprint。**

## 4. 删除清单

### 删除的源文件
- `apps/editor_cpp/src/editor_visual_script.{h,cpp}`（面板）
- `apps/editor_cpp/src/editor_visual_script_compiler.{h,cpp}`（Lua 生成器）
- `apps/editor_cpp/src/editor_visual_script_debugger.{h,cpp}`（调试器面板）
- `tests/gtest/integration/editor/editor_visual_script_compiler_test.cpp`（编译器单测）

### 移除的接线 / 引用
- `editor_app.cpp`：include、PanelRegistry `Register("visual_script", …)`、`ui_services.show_visual_script`、面板 draw 调用、调试器 draw 调用。
- `editor_panel_registry.h`：`bool visual_script` 面板开关字段。
- `editor_shell.cpp`：主菜单 "Visual Script" 菜单项。
- `editor_blueprint.h`：历史注释 "enhanced from visual_script"。
- UI 测试：`ui_test_harness.h` 的 `show_visual_script` 字段；`ui_tests_common.cpp`、`ui_tests_editor_features.cpp`（vs_debugger 2 例）、`ui_tests_graph.cpp`（visual_script_node_link_compile 例）、`ui_tests_panels.cpp`（面板存在性 2 条）、`ui_tests_panel_deep.cpp`（node_palette / canvas_exists 2 例）。
- `tests/gtest/integration/CMakeLists.txt`：显式列出的 `editor_visual_script_compiler.cpp` 源。
- 文档：`docs/editor/EDITOR_ARCHITECTURE.md`、`docs/editor_ui_test_supplement_plan.md` 中的 Visual Script 条目改为指向 Blueprint / 标注已退场。

> 编辑器 app CMake 用 `GLOB_RECURSE src/*.cpp`（CONFIGURE_DEPENDS），删除 `.cpp` 后重新配置即自动移除；gtest 集成用例走 `GLOB editor/*.cpp`，删测试文件即自动移除。

## 5. 统一入口

所有节点式可视脚本需求统一到 **Blueprint**（工具菜单 "Blueprint"，`editor_blueprint*` + engine 单编译器）。Visual Script 不再编译进任何生产目标。

## 6. 验收

- 生产代码不再编译旧 Visual Script（源文件删除 + 接线清空 + 无残留 include）。
- 无未迁移的旧资产（因从未存在持久化格式）。
- 不保留双系统长期共存的尾巴。
- 三后端 headless 冒烟通过（编辑器可正常启动/退出，无 VS 相关符号未定义）。
