/**
 * @file editor_test_stubs.cpp
 * @brief 编辑器无头测试的桩实现
 *
 * 提供 editor_scene_tabs.cpp 等模块需要但在测试环境中不可用的符号。
 * 这些函数在完整编辑器中由各自的 .cpp 文件定义，
 * 但测试 target 不链接 ImGui/GLFW 相关的编辑器面板代码。
 */

#include <string>

#include "apps/editor_cpp/src/editor_shortcuts.h"
#include "apps/editor_cpp/src/editor_shell.h"
#include "apps/editor_cpp/src/editor_console_panel.h"
#include "apps/editor_cpp/src/editor_toolbar.h"
#include "apps/editor_cpp/src/editor_context.h"

namespace dse::editor {

// Stub: global undo/redo manager singleton (same as editor_shortcuts.cpp)
UndoRedoManager& GetUndoRedoManager() {
    static UndoRedoManager instance(200);
    return instance;
}

// Stub: scene path tracking (same as editor_shell.cpp)
namespace {
std::string g_stub_scene_path;
}

const std::string& GetCurrentScenePath() {
    return g_stub_scene_path;
}

void SetCurrentScenePath(const std::string& path) {
    g_stub_scene_path = path;
}

// Stub: editor console log (no-op in tests)
void EditorLog(LogLevel /*level*/, const std::string& /*message*/) {
    // No-op: tests don't need console output
}

void InstallEditorLogSink() {
    // No-op
}

// ─── editor_toolbar stubs (no ImGui) ────────────────────────────────────────

static EditorState g_editor_state_stub = EditorState::Edit;

EditorState GetEditorState() { return g_editor_state_stub; }
bool IsEditorInPlayMode() { return g_editor_state_stub == EditorState::Play; }

void MarkAllUILabelsDirty(entt::registry& /*registry*/) {}

void EnterPlayMode(entt::registry& /*registry*/) {
    g_editor_state_stub = EditorState::Play;
}

void ExitPlayMode(entt::registry& /*registry*/, entt::entity& /*selected*/,
                  dse::runtime::EngineInstance* /*engine*/) {
    g_editor_state_stub = EditorState::Edit;
}

void DrawEditorToolbar(EditorContext& /*ctx*/) {}

void ResetEditorStateStub() { g_editor_state_stub = EditorState::Edit; }

} // namespace dse::editor
