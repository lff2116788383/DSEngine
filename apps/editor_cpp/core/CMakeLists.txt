# ─────────────────────────────────────────────────────────────────────────────
# dse_editor_core — 编辑器核心门面层（命令/查询），**不依赖任何 UI 框架**
#
# 这是"逻辑与 UI 隔离"的物理边界：本库只链接 header-only 的 rapidjson，
# 不链接 imgui / glfw / opengl。任何把 UI 框架泄漏进 core/ 的改动都会被
# scripts/check_core_ui_free + EditorCoreIsolation gtest 拦下。
#
# 无条件构建（不受 DSE_BUILD_EDITOR 影响），以便编辑器 app 与 gtest 集成测试
# 都能链接它（gtest CI 通常在 editor 未构建的配置下运行）。
# ─────────────────────────────────────────────────────────────────────────────

add_library(dse_editor_core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/command_bus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/query_service.cpp
)

target_compile_features(dse_editor_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(dse_editor_core PRIVATE /utf-8)
endif()

# PUBLIC 包含目录 → 链接方自动继承，可直接 #include "apps/editor_cpp/core/..."
target_include_directories(dse_editor_core PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/depends/rapidjson/include
)

# 仅 header-only 依赖（rapidjson + 标准库）。
# 刻意不链接 dse_engine / imgui / glfw：EngineInstance 在头文件中仅前置声明，
# 命令/查询全部经 ToolSink 透传，core 不直接调用引擎符号 → 保持最小耦合。
