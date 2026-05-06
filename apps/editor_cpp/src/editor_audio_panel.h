#pragma once

#include <entt/entt.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>

namespace dse::editor {

/// Draw Audio Source / Audio Listener inspector section for the selected entity
void DrawAudioSection(entt::registry& registry, entt::entity selected_entity);

/// Draw 3D audio range visualization (min/max distance spheres) in Scene viewport
void DrawAudioRangeOverlay(entt::registry& registry,
                           entt::entity selected_entity,
                           const glm::vec2& viewport_pos,
                           const glm::vec2& viewport_size,
                           const glm::mat4& view,
                           const glm::mat4& proj);

} // namespace dse::editor
