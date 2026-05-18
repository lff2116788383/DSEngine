#pragma once

#include "editor_context.h"
#include "editor_shared_components.h"

namespace dse {
struct Camera3DComponent;
struct DirectionalLight3DComponent;
struct MeshRendererComponent;
struct ParticleSystem3DComponent;
}

struct ParentComponent;
struct TransformComponent;
struct UIRendererComponent;

namespace dse::editor {

void DrawHierarchyPanel(EditorContext& ctx);

} // namespace dse::editor
