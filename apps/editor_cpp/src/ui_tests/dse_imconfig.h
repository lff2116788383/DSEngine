// dse_imconfig.h —— 编辑器 Dear ImGui 用户配置（仅在 DSE_EDITOR_UI_TESTS 构建生效）
//
// 经 CMake `target_compile_definitions(... IMGUI_USER_CONFIG="dse_imconfig.h")` 注入，
// 对整个编辑器 target（含 imgui.cpp / imgui_widgets.cpp / ImGuizmo.cpp 等所有引用
// imgui.h 的 TU）统一生效，确保 ImGuiContext / ImGuiIO 的 ABI 在引擎侧与测试引擎侧一致。
//
// 这里先按需调节测试引擎的编译期开关，再 #include 官方模板 imgui_te_imconfig.h
// （它会 `#define IMGUI_ENABLE_TEST_ENGINE`，把测试钩子编进 imgui 核心）。
#pragma once

// 用 std::thread 实现协程，无需应用自行提供 CoroutineFuncs（无头 CI 友好）。
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1

// 关闭截图/录像：CI 无头跑只看断言结果，避免 PNG/ffmpeg 依赖与额外 IO 开销。
#define IMGUI_TEST_ENGINE_ENABLE_CAPTURE 0

#include "imgui_te_imconfig.h"
