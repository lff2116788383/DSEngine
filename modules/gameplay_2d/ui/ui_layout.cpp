/**
 * @file ui_layout.cpp
 * @brief 高级 UI 布局系统实现
 */

#include "gameplay_2d/ui/ui_layout.h"
#include "engine/ecs/components_2d.h"
#include <algorithm>
#include <cmath>

namespace dse {
namespace gameplay2d {

// ============================================================================
// UILayoutSystem::CalculateScaleFactor
// ============================================================================
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

    const float width_ratio  = screen_size.x / scaler.reference_resolution.x;
    const float height_ratio = screen_size.y / scaler.reference_resolution.y;

    if (scaler.match_width_or_height) {
        return (width_ratio + height_ratio) * 0.5f * scaler.scale_factor;
    } else {
        return width_ratio * scaler.scale_factor;
    }
}

// ============================================================================
// UILayoutSystem::CalculateAnchorPosition
// ============================================================================
glm::vec2 UILayoutSystem::CalculateAnchorPosition(
    const UIAnchorData& anchor,
    const glm::vec2& screen_size,
    float scale_factor
) const {
    glm::vec2 base_pos(0.0f);

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

// ============================================================================
// UILayoutSystem::UpdateGridLayout
// ============================================================================
void UILayoutSystem::UpdateGridLayout(
    entt::registry& registry,
    entt::entity parent,
    const GridLayoutData& grid,
    const glm::vec2& parent_pos,
    float scale_factor
) const {
    // 收集所有 ParentComponent.parent == parent 的子实体
    std::vector<entt::entity> children;
    auto parent_view = registry.view<ParentComponent>();
    for (auto entity : parent_view) {
        const auto& pc = parent_view.get<ParentComponent>(entity);
        if (pc.parent == parent) {
            children.push_back(entity);
        }
    }

    // 按实体 ID 排序（近似创建顺序）
    std::sort(children.begin(), children.end(), [](entt::entity a, entt::entity b) {
        return static_cast<uint32_t>(a) < static_cast<uint32_t>(b);
    });

    const int columns = std::max(grid.columns, 1);
    const int total_items = static_cast<int>(children.size());
    const int actual_rows = (grid.rows > 0) ? grid.rows : ((total_items + columns - 1) / columns);

    // 计算网格总尺寸
    const float total_width  = static_cast<float>(columns) * grid.cell_size.x
                             + static_cast<float>(std::max(columns - 1, 0)) * grid.spacing.x;
    const float total_height = static_cast<float>(actual_rows) * grid.cell_size.y
                             + static_cast<float>(std::max(actual_rows - 1, 0)) * grid.spacing.y;

    // 根据 alignment 计算网格起始偏移（相对于 parent_pos）
    glm::vec2 grid_origin(0.0f);

    // 水平对齐
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

    // 垂直对齐
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

    // 应用缩放
    const glm::vec2 scaled_cell_size = grid.cell_size * scale_factor;
    const glm::vec2 scaled_spacing   = grid.spacing * scale_factor;
    const glm::vec2 scaled_origin    = grid_origin * scale_factor;

    // 遍历子实体，计算并更新位置
    for (int i = 0; i < total_items; ++i) {
        const int col = i % columns;
        const int row = i / columns;

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

        if (registry.all_of<UIRendererComponent>(children[i])) {
            auto& renderer = registry.get<UIRendererComponent>(children[i]);
            renderer.position = final_pos;
            renderer.size = scaled_cell_size;
        }
    }
}

// ============================================================================
// UILayoutSystem::Update
// ============================================================================
void UILayoutSystem::Update(entt::registry& registry, const glm::vec2& screen_size) {
    // 1. 查找 UICanvasScalerComponent，计算全局 scale_factor
    float global_scale = 1.0f;
    {
        auto scaler_view = registry.view<UICanvasScalerComponent>();
        for (auto entity : scaler_view) {
            const auto& scaler_comp = scaler_view.get<UICanvasScalerComponent>(entity);
            CanvasScalerData data;
            data.reference_resolution = scaler_comp.reference_resolution;
            data.scale_factor = scaler_comp.scale_factor;
            data.match_width_or_height = scaler_comp.match_width_or_height;
            global_scale = CalculateScaleFactor(screen_size, data);
            break;  // 使用第一个找到的 Canvas Scaler
        }
    }

    // 2. 遍历所有有 UIAnchorComponent 的实体，计算并更新其位置
    {
        auto anchor_view = registry.view<UIAnchorComponent, UIRendererComponent>();
        for (auto entity : anchor_view) {
            const auto& anchor_comp = anchor_view.get<UIAnchorComponent>(entity);
            auto& renderer = anchor_view.get<UIRendererComponent>(entity);

            UIAnchorData data;
            data.anchor = static_cast<UIAnchor>(anchor_comp.anchor);
            data.offset = anchor_comp.offset;
            data.size = renderer.size;

            renderer.position = CalculateAnchorPosition(data, screen_size, global_scale);
        }
    }

    // 3. 遍历所有有 UIGridLayoutComponent 的实体，调用 UpdateGridLayout
    {
        auto grid_view = registry.view<UIGridLayoutComponent>();
        for (auto entity : grid_view) {
            const auto& grid_comp = grid_view.get<UIGridLayoutComponent>(entity);

            glm::vec2 parent_pos(0.0f);
            if (registry.all_of<UIRendererComponent>(entity)) {
                parent_pos = registry.get<UIRendererComponent>(entity).position;
            }

            GridLayoutData data;
            data.columns = grid_comp.columns;
            data.rows = grid_comp.rows;
            data.cell_size = grid_comp.cell_size;
            data.spacing = grid_comp.spacing;
            data.alignment = static_cast<GridLayoutAlignment>(grid_comp.alignment);

            UpdateGridLayout(registry, entity, data, parent_pos, global_scale);
        }
    }
}

} // namespace gameplay2d
} // namespace dse
