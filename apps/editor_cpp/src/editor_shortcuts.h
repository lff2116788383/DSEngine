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

/// Copy/Paste entity clipboard
void CopySelectedEntity(EditorContext& ctx);
void CutSelectedEntity(EditorContext& ctx);
void PasteEntity(EditorContext& ctx);
bool HasEntityClipboard();

/// Entity template creation helpers
void CreateEntity3DCube(EditorContext& ctx);
void CreateEntity3DSphere(EditorContext& ctx);
void CreateEntity3DPlane(EditorContext& ctx);
void CreateEntity3DCamera(EditorContext& ctx);
void CreateEntity3DDirectionalLight(EditorContext& ctx);
void CreateEntity3DPointLight(EditorContext& ctx);
void CreateEntity3DSpotLight(EditorContext& ctx);
void CreateEntity3DAudioSource(EditorContext& ctx);
void CreateEntity3DAudioListener(EditorContext& ctx);
void CreateEntity3DPhysicsBox(EditorContext& ctx);
void CreateEntity3DPhysicsSphere(EditorContext& ctx);
void CreateEntity2DSprite(EditorContext& ctx);

} // namespace dse::editor
