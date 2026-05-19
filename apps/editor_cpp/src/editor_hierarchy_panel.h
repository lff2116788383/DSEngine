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

/// 触发 Hierarchy 内联重命名（由 F2 快捷键调用）
void BeginHierarchyRename(entt::entity entity, const std::string& current_name);

} // namespace dse::editor
