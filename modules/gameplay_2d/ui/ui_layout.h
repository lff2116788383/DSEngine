#ifndef DSE_UI_LAYOUT_H
#define DSE_UI_LAYOUT_H

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include "engine/ecs/components_2d.h"

namespace dse {
namespace gameplay2d {

enum class UIAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    Stretch  // 拉伸填充
};

enum class GridLayoutAlignment {
    UpperLeft,
    UpperCenter,
    UpperRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    LowerLeft,
    LowerCenter,
    LowerRight
};

struct UIAnchorData {
    UIAnchor anchor = UIAnchor::MiddleCenter;
    glm::vec2 offset = glm::vec2(0.0f);
    glm::vec2 size = glm::vec2(100.0f);
};

struct GridLayoutData {
    int columns = 1;
    int rows = 0;
    glm::vec2 cell_size = glm::vec2(100.0f);
    glm::vec2 spacing = glm::vec2(10.0f);
    GridLayoutAlignment alignment = GridLayoutAlignment::UpperLeft;
};

struct CanvasScalerData {
    glm::vec2 reference_resolution = glm::vec2(1920.0f, 1080.0f);
    float scale_factor = 1.0f;
    bool match_width_or_height = true;
};

class UILayoutSystem {
public:
    float CalculateScaleFactor(
        const glm::vec2& screen_size,
        const CanvasScalerData& scaler
    ) const;

    glm::vec2 CalculateAnchorPosition(
        const UIAnchorData& anchor,
        const glm::vec2& screen_size,
        float scale_factor
    ) const;

    void UpdateGridLayout(
        entt::registry& registry,
        entt::entity parent,
        const GridLayoutData& grid,
        const glm::vec2& parent_pos,
        float scale_factor
    ) const;

    void UpdateBoxLayout(
        entt::registry& registry,
        entt::entity parent,
        float scale_factor
    ) const;

    void UpdateContentSizeFitter(
        entt::registry& registry,
        entt::entity entity
    ) const;

    void Update(entt::registry& registry, const glm::vec2& screen_size);
};

} // namespace gameplay2d
} // namespace dse

#endif // DSE_UI_LAYOUT_H
