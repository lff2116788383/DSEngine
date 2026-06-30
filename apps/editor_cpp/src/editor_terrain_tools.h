#pragma once

#include <glm/glm.hpp>

namespace dse::editor {

struct EditorContext;

/// Draw the Terrain Editing tools panel (brushes, foliage, road, partition vis)
void DrawTerrainToolsPanel(EditorContext& ctx);

/// Draw extended terrain tools brush gizmo overlay in Scene viewport
void DrawTerrainToolsOverlay(EditorContext& ctx,
                              const glm::vec2& window_pos,
                              const glm::vec2& panel_size,
                              const glm::mat4& view,
                              const glm::mat4& proj);

} // namespace dse::editor
