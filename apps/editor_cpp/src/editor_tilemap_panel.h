#pragma once

#include <vector>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct TilemapComponent;

namespace dse::editor {

enum class TilemapBrushTool {
    Paint,
    Erase,
    FloodFill
};

struct TilemapEditorState {
    TilemapBrushTool active_tool = TilemapBrushTool::Paint;
    int selected_tile_id = 1;
    entt::entity active_tilemap = entt::null;
    bool editing_active = false;
    int brush_size = 1;

    // Undo state: tracked during a paint stroke
    bool painting = false;
    std::vector<int> tiles_snapshot;
};

TilemapEditorState& GetTilemapEditorState();

/// Draw the Tile Palette panel (tileset grid + brush tools + tilemap selector)
void DrawTilemapEditorPanel(entt::registry& registry, entt::entity selected_entity);

/// Draw tilemap grid overlay on top of the Scene viewport.
/// Call after drawing the scene image. Uses current ImGui draw list.
void DrawTilemapGridOverlay(entt::registry& registry,
                            const glm::vec2& window_pos,
                            const glm::vec2& panel_size,
                            const glm::mat4& view,
                            const glm::mat4& proj);

/// Handle mouse painting in the Scene viewport for tilemap editing.
/// Returns true if a paint action was consumed (callers may skip picking).
bool HandleTilemapViewportPaint(entt::registry& registry,
                                const glm::vec2& window_pos,
                                const glm::vec2& panel_size,
                                const glm::mat4& view,
                                const glm::mat4& proj);

} // namespace dse::editor
