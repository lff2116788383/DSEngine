#pragma once

#include <string>
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

/// Get/Set the current scene file path (for window title display)
const std::string& GetCurrentScenePath();
void SetCurrentScenePath(const std::string& path);

} // namespace dse::editor
