#pragma once

#include <glm/glm.hpp>

namespace dse::editor {

struct EditorContext;

/// Draw the NavMesh bake configuration and visualization panel
void DrawNavMeshPanel(EditorContext& ctx);

/// Draw the NavMesh wireframe overlay in the Scene viewport
void DrawNavMeshOverlay(EditorContext& ctx,
                         const glm::vec2& window_pos,
                         const glm::vec2& panel_size,
                         const glm::mat4& view,
                         const glm::mat4& proj);

} // namespace dse::editor
