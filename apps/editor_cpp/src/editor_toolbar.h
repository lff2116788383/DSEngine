#pragma once

#include <entt/entt.hpp>

namespace dse::runtime {
class EngineInstance;
}
namespace dse::editor { struct EditorContext; }

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
void ExitPlayMode(entt::registry& registry, entt::entity& selected_entity);

void DrawEditorToolbar(dse::editor::EditorContext& ctx);
