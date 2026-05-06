#pragma once

#include "editor_undo.h"
#include <entt/entt.hpp>

class World;

namespace dse::editor {

/// Global undo/redo manager singleton
UndoRedoManager& GetUndoRedoManager();

struct ShortcutContext {
    World& world;
    entt::registry& registry;
    entt::entity& selected_entity;
    bool read_only;
};

/// Process global editor shortcuts each frame (call after ImGui::NewFrame)
void ProcessShortcuts(ShortcutContext& context);

} // namespace dse::editor
