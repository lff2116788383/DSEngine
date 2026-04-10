#include "modules/gameplay_2d/ui/ui_layout.h"

#include <algorithm>

namespace dse {
namespace gameplay2d {

namespace {

UIAnchor ResolveAnchorType(int anchor_value) {
    switch (anchor_value) {
        case 0: return UIAnchor::TopLeft;
        case 1: return UIAnchor::TopCenter;
        case 2: return UIAnchor::TopRight;
        case 3: return UIAnchor::MiddleLeft;
        case 4: return UIAnchor::BottomCenter;
        case 5: return UIAnchor::MiddleCenter;
        case 6: return UIAnchor::MiddleRight;
        case 7: return UIAnchor::BottomLeft;
        case 8: return UIAnchor::BottomRight;
        case 9: return UIAnchor::Stretch;
        default: return UIAnchor::BottomCenter;
    }
}

} // namespace

float UILayoutSystem::CalculateScaleFactor(
    const glm::vec2& screen_size,
    const CanvasScalerData& scaler
) const {
    if (scaler.reference_resolution.x <= 0.0f || scaler.reference_resolution.y <= 0.0f) {
        return scaler.scale_factor;
    }

    if (screen_size.x <= 0.0f || screen_size.y <= 0.0f) {
        return scaler.scale_factor;
    }

    const float width_ratio = screen_size.x / scaler.reference_resolution.x;
    const float height_ratio = screen_size.y / scaler.reference_resolution.y;

    if (scaler.match_width_or_height) {
        return (width_ratio + height_ratio) * 0.5f * scaler.scale_factor;
    }

    return width_ratio * scaler.scale_factor;
}

glm::vec2 UILayoutSystem::CalculateAnchorPosition(
    const UIAnchorData& anchor,
    const glm::vec2& screen_size,
    float scale_factor
) const {
    glm::vec2 base_pos(0.0f, 0.0f);

    switch (anchor.anchor) {
        case UIAnchor::TopLeft:
            base_pos = glm::vec2(0.0f, 0.0f);
            break;
        case UIAnchor::TopCenter:
            base_pos = glm::vec2(screen_size.x * 0.5f, 0.0f);
            break;
        case UIAnchor::TopRight:
            base_pos = glm::vec2(screen_size.x, 0.0f);
            break;
        case UIAnchor::MiddleLeft:
            base_pos = glm::vec2(0.0f, screen_size.y * 0.5f);
            break;
        case UIAnchor::MiddleCenter:
            base_pos = glm::vec2(screen_size.x * 0.5f, screen_size.y * 0.5f);
            break;
        case UIAnchor::MiddleRight:
            base_pos = glm::vec2(screen_size.x, screen_size.y * 0.5f);
            break;
        case UIAnchor::BottomLeft:
            base_pos = glm::vec2(0.0f, screen_size.y);
            break;
        case UIAnchor::BottomCenter:
            base_pos = glm::vec2(screen_size.x * 0.5f, screen_size.y);
            break;
        case UIAnchor::BottomRight:
            base_pos = glm::vec2(screen_size.x, screen_size.y);
            break;
        case UIAnchor::Stretch:
            base_pos = glm::vec2(screen_size.x * 0.5f, screen_size.y * 0.5f);
            break;
    }

    return base_pos + anchor.offset * scale_factor;
}

void UILayoutSystem::UpdateGridLayout(
    entt::registry& registry,
    entt::entity parent,
    const GridLayoutData& grid,
    const glm::vec2& parent_pos,
    float scale_factor
) const {
    std::vector<entt::entity> children;
    auto parent_view = registry.view<ParentComponent>();
    for (auto entity : parent_view) {
        const auto& pc = parent_view.get<ParentComponent>(entity);
        if (pc.parent == parent) {
            children.push_back(entity);
        }
    }

    std::sort(children.begin(), children.end(), [](entt::entity lhs, entt::entity rhs) {
        return static_cast<uint32_t>(lhs) < static_cast<uint32_t>(rhs);
    });

    const int columns = std::max(grid.columns, 1);
    const int total_items = static_cast<int>(children.size());
    const int actual_rows = grid.rows > 0 ? grid.rows : (total_items + columns - 1) / columns;

    const glm::vec2 scaled_cell_size = grid.cell_size * scale_factor;
    const glm::vec2 scaled_spacing = grid.spacing * scale_factor;

    const float total_width = static_cast<float>(columns) * scaled_cell_size.x
        + static_cast<float>(std::max(columns - 1, 0)) * scaled_spacing.x;
    const float total_height = static_cast<float>(actual_rows) * scaled_cell_size.y
        + static_cast<float>(std::max(actual_rows - 1, 0)) * scaled_spacing.y;

    glm::vec2 grid_origin(0.0f, 0.0f);

    switch (grid.alignment) {
        case GridLayoutAlignment::UpperLeft:
        case GridLayoutAlignment::MiddleLeft:
        case GridLayoutAlignment::LowerLeft:
            grid_origin.x = 0.0f;
            break;
        case GridLayoutAlignment::UpperCenter:
        case GridLayoutAlignment::MiddleCenter:
        case GridLayoutAlignment::LowerCenter:
            grid_origin.x = -total_width * 0.5f;
            break;
        case GridLayoutAlignment::UpperRight:
        case GridLayoutAlignment::MiddleRight:
        case GridLayoutAlignment::LowerRight:
            grid_origin.x = -total_width;
            break;
    }

    switch (grid.alignment) {
        case GridLayoutAlignment::UpperLeft:
        case GridLayoutAlignment::UpperCenter:
        case GridLayoutAlignment::UpperRight:
            grid_origin.y = 0.0f;
            break;
        case GridLayoutAlignment::MiddleLeft:
        case GridLayoutAlignment::MiddleCenter:
        case GridLayoutAlignment::MiddleRight:
            grid_origin.y = -total_height * 0.5f;
            break;
        case GridLayoutAlignment::LowerLeft:
        case GridLayoutAlignment::LowerCenter:
        case GridLayoutAlignment::LowerRight:
            grid_origin.y = -total_height;
            break;
    }

    const glm::vec2 scaled_origin = grid_origin;

    for (int index = 0; index < total_items; ++index) {
        const int col = index % columns;
        const int row = index / columns;

        if (grid.rows > 0 && row >= grid.rows) {
            break;
        }

        const float cell_x = scaled_origin.x
            + static_cast<float>(col) * (scaled_cell_size.x + scaled_spacing.x)
            + scaled_cell_size.x * 0.5f;
        const float cell_y = scaled_origin.y
            + static_cast<float>(row) * (scaled_cell_size.y + scaled_spacing.y)
            + scaled_cell_size.y * 0.5f;

        const glm::vec2 final_pos = parent_pos + glm::vec2(cell_x, cell_y);
        const entt::entity child = children[static_cast<size_t>(index)];

        if (registry.all_of<UIRendererComponent>(child)) {
            auto& renderer = registry.get<UIRendererComponent>(child);
            renderer.position = final_pos;
            renderer.size = scaled_cell_size;
            renderer.scale = scale_factor;
        }
    }
}

void UILayoutSystem::Update(entt::registry& registry, const glm::vec2& screen_size) {
    float global_scale = 1.0f;

    auto scaler_view = registry.view<UICanvasScalerComponent>();
    for (auto entity : scaler_view) {
        const auto& scaler_comp = scaler_view.get<UICanvasScalerComponent>(entity);
        CanvasScalerData scaler_data{};
        scaler_data.reference_resolution = scaler_comp.reference_resolution;
        scaler_data.scale_factor = scaler_comp.scale_factor;
        scaler_data.match_width_or_height = scaler_comp.match_width_or_height;
        global_scale = CalculateScaleFactor(screen_size, scaler_data);
        break;
    }

    auto anchor_view = registry.view<UIAnchorComponent, UIRendererComponent>();
    for (auto entity : anchor_view) {
        const auto& anchor_comp = anchor_view.get<UIAnchorComponent>(entity);
        auto& renderer = anchor_view.get<UIRendererComponent>(entity);

        UIAnchorData anchor_data{};
        anchor_data.anchor = ResolveAnchorType(anchor_comp.anchor);
        anchor_data.offset = anchor_comp.offset;
        anchor_data.size = renderer.size;

        renderer.position = CalculateAnchorPosition(anchor_data, screen_size, global_scale);
    }

    auto grid_view = registry.view<UIGridLayoutComponent>();
    for (auto entity : grid_view) {
        const auto& grid_comp = grid_view.get<UIGridLayoutComponent>(entity);

        glm::vec2 parent_pos(0.0f, 0.0f);
        if (registry.all_of<UIRendererComponent>(entity)) {
            parent_pos = registry.get<UIRendererComponent>(entity).position;
        }

        GridLayoutData grid_data{};
        grid_data.columns = grid_comp.columns;
        grid_data.rows = grid_comp.rows;
        grid_data.cell_size = grid_comp.cell_size;
        grid_data.spacing = grid_comp.spacing;
        grid_data.alignment = static_cast<GridLayoutAlignment>(grid_comp.alignment);

        UpdateGridLayout(registry, entity, grid_data, parent_pos, global_scale);
    }
}

} // namespace gameplay2d
} // namespace dse
