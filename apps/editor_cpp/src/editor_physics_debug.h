#pragma once

#include <glm/glm.hpp>

namespace dse::editor {

struct EditorContext;

/// Global toggle for physics debug overlay
bool& GetPhysicsDebugEnabled();

/// Draw wireframe overlays for all physics colliders (Box, Sphere, Capsule, Mesh)
/// in the scene. Call from within the Scene viewport window.
void DrawPhysicsDebugOverlay(EditorContext& ctx,
                              const glm::vec2& window_pos,
                              const glm::vec2& panel_size,
                              const glm::mat4& view,
                              const glm::mat4& proj);

} // namespace dse::editor
