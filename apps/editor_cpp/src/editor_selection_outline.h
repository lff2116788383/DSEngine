#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace dse::editor {

struct EditorContext;

/// Draw wireframe bounding-box outlines around all selected entities in the Scene viewport.
/// Call after rendering the scene texture but before ImGui::End() of the Scene window.
void DrawSelectionOutlines(EditorContext& ctx,
                           const glm::vec2& window_pos,
                           const glm::vec2& panel_size,
                           const glm::mat4& view,
                           const glm::mat4& proj);

} // namespace dse::editor
