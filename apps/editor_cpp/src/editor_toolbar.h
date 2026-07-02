#pragma once

#include <entt/entt.hpp>

namespace dse::runtime { class EngineInstance; }

namespace dse::editor {

struct EditorContext;

enum class EditorState {
    Edit,
    Play,
    Pause
};

void MarkAllUILabelsDirty(entt::registry& registry);
EditorState GetEditorState();
bool IsEditorInPlayMode();

/// Play 模式控制（Tool handler 可调用）
void EnterPlayMode(entt::registry& registry);
void ExitPlayMode(entt::registry& registry, entt::entity& selected_entity,
                  dse::runtime::EngineInstance* engine = nullptr);

bool IsEditorPaused();
void ToggleEditorPause();
bool ConsumeStepFrame();

void DrawEditorToolbar(EditorContext& ctx);

} // namespace dse::editor
