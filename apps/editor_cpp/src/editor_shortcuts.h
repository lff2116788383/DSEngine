#pragma once

#include "editor_undo.h"
#include "editor_context.h"

namespace dse::editor {

/// Global undo/redo manager singleton
UndoRedoManager& GetUndoRedoManager();

/// Process global editor shortcuts each frame (call after ImGui::NewFrame)
void ProcessShortcuts(EditorContext& ctx);

/// Entity operations (used by shortcuts and menus)
void CreateEmptyEntity(EditorContext& ctx);
void DuplicateSelectedEntity(EditorContext& ctx);
void DeleteSelectedEntity(EditorContext& ctx);

} // namespace dse::editor
