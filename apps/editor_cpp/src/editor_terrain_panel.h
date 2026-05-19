#pragma once

#include <vector>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace dse::editor { struct EditorContext; }

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
    bool splat_paint_mode = false;    // true = painting splat, false = sculpting heights
    int active_splat_layer = 0;       // 0-3
    float splat_brush_opacity = 0.5f; // splat paint opacity

    // Last brush hit position in world space (updated every frame in brush overlay)
    glm::vec3 last_brush_hit{0.0f};
    bool last_brush_hit_valid = false;

    // Undo state: snapshot at stroke start
    std::vector<float> height_snapshot;
    std::vector<float> splat_snapshot;
};

TerrainEditorState& GetTerrainEditorState();

/// Draw the Terrain Brush panel (brush tools + parameters)
void DrawTerrainEditorPanel(EditorContext& ctx);

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
