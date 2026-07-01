/**
 * @file world_canvas.h
 * @brief 3D world-space UI system — health bars, name plates, damage numbers.
 *
 * Unlike the screen-space UISystem in modules/gameplay_2d/ui/, WorldCanvas
 * renders UI elements at 3D world positions. Elements are billboarded toward
 * the camera and optionally scaled by distance.
 *
 * Usage:
 *   1. Attach WorldCanvasComponent to an entity with a TransformComponent.
 *   2. Add WorldCanvasElement children describing bars, labels, or icons.
 *   3. Call WorldCanvasSystem::Update() each frame before rendering.
 *   4. WorldCanvasSystem::CollectDrawItems() produces MeshDrawItems for
 *      the existing batch renderer pipeline.
 */
#ifndef DSE_UI_WORLD_CANVAS_H
#define DSE_UI_WORLD_CANVAS_H

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "engine/core/dse_export.h"

class World;

namespace dse::ui {

/// Anchor point relative to the WorldCanvas origin.
enum class WorldCanvasAnchor : uint8_t {
    Center = 0,
    Top,
    Bottom,
    Left,
    Right,
};

/// Element type within a world canvas.
enum class WorldCanvasElementKind : uint8_t {
    Bar = 0,     ///< Horizontal fill bar (health, mana, etc.)
    Label,       ///< Text label (name plate, damage number)
    Icon,        ///< Textured quad (status icon, class emblem)
};

/// A single UI element within a WorldCanvas.
struct WorldCanvasElement {
    WorldCanvasElementKind kind = WorldCanvasElementKind::Bar;
    WorldCanvasAnchor      anchor = WorldCanvasAnchor::Center;
    glm::vec2 offset{0.0f};          ///< Pixel offset from anchor
    glm::vec2 size{100.0f, 10.0f};   ///< Element size in canvas-local pixels

    // Bar fields
    float fill = 1.0f;               ///< Fill ratio [0, 1]
    glm::vec4 fill_color{0.2f, 0.8f, 0.2f, 1.0f};
    glm::vec4 bg_color{0.15f, 0.15f, 0.15f, 0.8f};

    // Label fields
    std::string text;
    glm::vec4 text_color{1.0f};
    float font_size = 14.0f;
    unsigned int font_texture = 0;

    // Icon fields
    unsigned int icon_texture = 0;
    glm::vec4 icon_tint{1.0f};
};

/// Component attached to a 3D entity to display world-space UI.
struct DSE_EXPORT WorldCanvasComponent {
    bool enabled = true;
    glm::vec3 world_offset{0.0f, 2.0f, 0.0f}; ///< Offset above entity origin
    float canvas_scale = 0.01f;                 ///< World units per canvas pixel
    bool billboard = true;                      ///< Always face camera
    bool scale_by_distance = true;              ///< Keep constant screen size
    float max_distance = 50.0f;                 ///< Hide beyond this distance
    float min_scale = 0.5f;                     ///< Minimum distance scale factor
    float max_scale = 2.0f;                     ///< Maximum distance scale factor
    float reference_distance = 10.0f;           ///< Distance at which scale = 1.0

    std::vector<WorldCanvasElement> elements;
};

/// Computed per-frame data for a visible world canvas (output of Update).
struct WorldCanvasInstance {
    glm::mat4 transform{1.0f};    ///< Billboard model matrix
    float distance = 0.0f;        ///< Distance to camera
    float applied_scale = 1.0f;   ///< Computed distance scale
    uint32_t entity_index = 0;    ///< Source entity
    const WorldCanvasComponent* canvas = nullptr;
};

/// System that updates and collects world-space UI for rendering.
class DSE_EXPORT WorldCanvasSystem {
public:
    WorldCanvasSystem() = default;

    /// Update all world canvases: cull by distance, compute billboard transforms.
    void Update(const World& world,
                const glm::vec3& camera_position,
                const glm::mat4& view,
                const glm::mat4& projection);

    /// Get visible canvas instances (populated by Update).
    const std::vector<WorldCanvasInstance>& GetVisibleInstances() const {
        return visible_;
    }

    /// Clear cached state.
    void Clear() { visible_.clear(); }

private:
    std::vector<WorldCanvasInstance> visible_;
};

} // namespace dse::ui

#endif // DSE_UI_WORLD_CANVAS_H
