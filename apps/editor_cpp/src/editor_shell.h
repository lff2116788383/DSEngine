#pragma once

#include <entt/entt.hpp>

namespace dse::runtime {
class EngineInstance;
}

namespace dse::editor {

struct EditorShellContext {
    dse::runtime::EngineInstance& engine;
    entt::registry& registry;
    entt::entity& selected_entity;
    bool read_only = false;
};

void BeginEditorShell();
void EndEditorShell();
void DrawEditorMainMenu(EditorShellContext& context);

} // namespace dse::editor
