#pragma once

#include <entt/entt.hpp>

namespace dse::runtime {
class EngineInstance;
}

enum class EditorState {
    Edit,
    Play,
    Pause
};

void MarkAllUILabelsDirty(entt::registry& registry);
EditorState GetEditorState();
bool IsEditorInPlayMode();

void DrawEditorToolbar(dse::runtime::EngineInstance& engine,
                       entt::registry& registry,
                       entt::entity& selected_entity);
