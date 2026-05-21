#pragma once

#include <glm/glm.hpp>

namespace dse::editor {

struct EditorContext;

/// Global toggle for lighting debug gizmos
bool& GetLightingGizmosEnabled();

/// Draw gizmos for all light probes, reflection probes, and light sources
void DrawLightingGizmos(EditorContext& ctx,
                         const glm::vec2& window_pos,
                         const glm::vec2& panel_size,
                         const glm::mat4& view,
                         const glm::mat4& proj);

} // namespace dse::editor
