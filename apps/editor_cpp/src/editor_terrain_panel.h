#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace dse::editor {

enum class TerrainBrushMode {
    Raise,
    Lower,
    Smooth,
    Flatten
};

struct TerrainEditorState {
    TerrainBrushMode brush_mode = TerrainBrushMode::Raise;
    float brush_radius = 5.0f;
    float brush_strength = 0.3f;
    float brush_falloff = 0.5f;      // 0 = hard, 1 = soft gaussian
    float flatten_target_height = 0.0f;
    entt::entity active_terrain = entt::null;
    bool editing_active = false;
    bool painting = false;            // Mouse is held down and painting

    // Splat layer editing
    int active_splat_layer = 0;       // 0-3
};

TerrainEditorState& GetTerrainEditorState();

/// Draw the Terrain Brush panel (brush tools + parameters)
void DrawTerrainEditorPanel(entt::registry& registry, entt::entity selected_entity);

/// Draw the brush circle overlay on the Scene viewport.
void DrawTerrainBrushOverlay(entt::registry& registry,
                             const glm::vec2& window_pos,
                             const glm::vec2& panel_size,
                             const glm::mat4& view,
                             const glm::mat4& proj);

/// Handle mouse sculpting in the Scene viewport for terrain editing.
/// Returns true if a paint action was consumed.
bool HandleTerrainViewportSculpt(entt::registry& registry,
                                 const glm::vec2& window_pos,
                                 const glm::vec2& panel_size,
                                 const glm::mat4& view,
                                 const glm::mat4& proj,
                                 float delta_time);

} // namespace dse::editor
