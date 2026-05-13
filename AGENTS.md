# AGENTS.md

本文件是 Roo Code 在本仓库中的首选项目规则入口。若与会话中的 system / developer 指令冲突，以更高优先级指令为准。

## 1. 沟通与输出

- 默认使用简体中文交流。
- 回答保持直接、专业，避免无意义客套。
- 仅在复杂逻辑、架构决策、兼容性修复处添加必要注释，避免逐行注释。
- 修改代码时，说明应尽量引用具体文件与符号。

## 2. 项目结构与依赖方向

- 依赖方向必须保持：`apps/ -> modules/ -> engine/ -> depends/`。
- `engine/` 不得依赖 `modules/` 或 `apps/`。
- `modules/` 只能依赖 `engine/` 与 `depends/`。
- 根构建入口是 [`CMakeLists.txt`](CMakeLists.txt)。

## 3. 关键架构约束

- 运行时核心服务通过 [`ServiceLocator`](engine/core/service_locator.h) 获取，生命周期由 [`EngineInstance`](engine/runtime/engine_app.h) 管理。
- 渲染功能优先走 [`RenderGraph`](engine/render/render_graph.h) + [`IRenderPass`](engine/render/passes/render_pass_interface.h) 扩展，不要把新 pass 直接硬编码进 [`FramePipeline`](engine/runtime/frame_pipeline.h) 流程控制里。
- 修改渲染相关功能时，必须同步检查 OpenGL / Vulkan / D3D11 三后端。
- 新增或调整后处理/渲染参数时，必须同步检查：
  - CPU 侧参数传递
  - GL inline shader
  - Vulkan shader 源
  - DX11 shader 源
  - 三后端执行器绑定逻辑
- 新增 [`RhiDevice`](engine/render/rhi/rhi_device.h) 接口或行为时，所有后端与测试桩都要同步。

## 4. 代码风格

- C++ 标准由 [`CMakeLists.txt`](CMakeLists.txt) 设为 C++20，但新代码优先保持与现有工程一致的保守风格。
- 命名遵循现有代码：
  - 类型/函数多为 `PascalCase`
  - 普通变量多为 `snake_case`
  - 成员变量多为 `trailing_underscore`
- 不在头文件中写 `using namespace`。
- 头文件优先保持轻量，能前向声明就不要额外包含大头文件。

## 5. 构建与验证

- 默认构建目录为 `build_vs2022`。
- 常用命令：
  - 配置：`cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64`
  - 构建测试：[`build_fast_tests.bat`](build_fast_tests.bat)
  - 完整构建：[`build_all.bat`](build_all.bat)
  - 全量验证：[`verify_all.bat`](verify_all.bat)
- 如果工作区根目录不存在 `CMakeCache.txt`，不要直接对 `.` 执行 [`cmake --build`](CMakeLists.txt)；应优先使用已有构建目录，如 `build_vs2022`。
- 修改渲染代码后，至少做编译验证；如条件允许，再补最小运行时验证路径。

## 6. 测试约束

- 测试框架以 GoogleTest 为主，测试位于 `tests/gtest/`。
- 新增功能、修复回归或改动关键渲染路径时，应补对应测试或最小验证路径。
- 提交前优先保证受影响目标可编译，其次再扩大验证范围。

## 7. 文档与规则文件

- 详细项目规则参考 [`.trae/rules/project_rules.md`](.trae/rules/project_rules.md)。
- 若发现 [`.trae/rules/project_rules.md`](.trae/rules/project_rules.md) 与真实代码、构建脚本、目录结构不一致，应以代码现状为准，并优先修正文档。
- 若会话开始时需要强调规则，建议在首条提示中显式要求遵守 [`AGENTS.md`](AGENTS.md)。

## 8. 当前仓库特别注意事项

- [`FramePipeline`](engine/runtime/frame_pipeline.h) 仍保留部分兼容入口，但新逻辑优先通过 [`EngineInstance`](engine/runtime/engine_app.h) 与运行时注入路径接入。
- Gameplay3D 相关代码已静态编入 `dse_engine`，不要按旧 DLL 模块思路设计新依赖。
- Vulkan 后端默认是可选能力，修改时要注意 `DSE_ENABLE_VULKAN` 条件编译。
- D3D11 后端仅在 Windows 下启用，修改时注意 `DSE_ENABLE_D3D11` 条件编译。
