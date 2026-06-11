#include "modules/gameplay_2d/ui/ui_layout.h"

#include <algorithm>
#include <cmath>

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
        // 在宽/高比之间按 match 权重线性插值（match=0.5 等价旧"宽高平均"，保持向后兼容）。
        const float m = std::min(std::max(scaler.match, 0.0f), 1.0f);
        return ((1.0f - m) * width_ratio + m * height_ratio) * scaler.scale_factor;
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

void UILayoutSystem::UpdateBoxLayout(
    entt::registry& registry,
    entt::entity parent,
    float scale_factor
) const {
    if (!registry.all_of<UIBoxLayoutComponent>(parent)) return;
    const auto& box = registry.get<UIBoxLayoutComponent>(parent);

    // 收集子实体
    std::vector<entt::entity> children;
    auto parent_view = registry.view<ParentComponent>();
    for (auto entity : parent_view) {
        const auto& pc = parent_view.get<ParentComponent>(entity);
        if (pc.parent == parent && registry.all_of<UIRendererComponent>(entity)) {
            auto& child_ui = registry.get<UIRendererComponent>(entity);
            if (child_ui.visible) {
                children.push_back(entity);
            }
        }
    }

    std::sort(children.begin(), children.end(), [](entt::entity a, entt::entity b) {
        return static_cast<uint32_t>(a) < static_cast<uint32_t>(b);
    });

    if (box.reverse) {
        std::reverse(children.begin(), children.end());
    }

    if (children.empty()) return;

    const int main_axis = box.vertical ? 1 : 0;
    const int cross_axis = box.vertical ? 0 : 1;

    glm::vec2 parent_pos(0.0f);
    glm::vec2 parent_size(0.0f);
    if (registry.all_of<UIRendererComponent>(parent)) {
        auto& parent_ui = registry.get<UIRendererComponent>(parent);
        parent_pos = parent_ui.position;
        parent_size = parent_ui.size;
    }

    const float scaled_spacing = box.spacing * scale_factor;
    const glm::vec2 scaled_padding = box.padding * scale_factor;

    // 计算子元素在主轴方向的总大小
    float total_main_size = 0.0f;
    for (size_t i = 0; i < children.size(); ++i) {
        auto& child_ui = registry.get<UIRendererComponent>(children[i]);
        float child_main = child_ui.size[main_axis] * child_ui.scale;
        total_main_size += child_main;
        if (i > 0) total_main_size += scaled_spacing;
    }

    // 主轴可用空间和起始偏移
    float available_main = parent_size[main_axis] - scaled_padding[main_axis == 0 ? 0 : 1] * 2.0f;
    float main_offset = scaled_padding[main_axis == 0 ? 0 : 1];

    switch (box.align_main) {
        case 0: break;  // 起始
        case 1: main_offset += (available_main - total_main_size) * 0.5f; break; // 居中
        case 2: main_offset += available_main - total_main_size; break;          // 尾部
        default: break; // 两端均布在下面处理
    }

    float even_gap = 0.0f;
    if (box.align_main == 3 && children.size() > 1) {
        float items_total = 0.0f;
        for (auto child : children) {
            auto& child_ui = registry.get<UIRendererComponent>(child);
            items_total += child_ui.size[main_axis] * child_ui.scale;
        }
        even_gap = (available_main - items_total) / static_cast<float>(children.size() - 1);
    }

    float cursor = main_offset;
    for (auto child : children) {
        auto& child_ui = registry.get<UIRendererComponent>(child);
        float child_main = child_ui.size[main_axis] * child_ui.scale;
        float child_cross = child_ui.size[cross_axis] * child_ui.scale;

        // 交叉轴对齐
        float cross_offset = scaled_padding[cross_axis == 0 ? 0 : 1];
        float available_cross = parent_size[cross_axis] - scaled_padding[cross_axis == 0 ? 0 : 1] * 2.0f;

        switch (box.align_cross) {
            case 0: break; // 起始
            case 1: cross_offset += (available_cross - child_cross) * 0.5f; break; // 居中
            case 2: cross_offset += available_cross - child_cross; break;          // 尾部
            case 3: // 拉伸
                child_ui.size[cross_axis] = available_cross / child_ui.scale;
                break;
        }

        glm::vec2 pos = parent_pos;
        pos[main_axis] += cursor;
        pos[cross_axis] += cross_offset;
        child_ui.position = pos;

        cursor += child_main + (box.align_main == 3 ? even_gap : scaled_spacing);
    }
}

void UILayoutSystem::UpdateContentSizeFitter(
    entt::registry& registry,
    entt::entity entity
) const {
    if (!registry.all_of<UIContentSizeFitterComponent, UIRendererComponent>(entity)) return;
    const auto& fitter = registry.get<UIContentSizeFitterComponent>(entity);
    auto& ui = registry.get<UIRendererComponent>(entity);

    if (fitter.fit_width == 0 && fitter.fit_height == 0) return;

    // 收集子实体包围盒
    float max_x = 0.0f;
    float max_y = 0.0f;
    auto parent_view = registry.view<ParentComponent, UIRendererComponent>();
    for (auto child : parent_view) {
        const auto& pc = parent_view.get<ParentComponent>(child);
        if (pc.parent != entity) continue;
        const auto& child_ui = parent_view.get<UIRendererComponent>(child);
        if (!child_ui.visible) continue;

        float right = (child_ui.position.x - ui.position.x) + child_ui.size.x * child_ui.scale;
        float bottom = (child_ui.position.y - ui.position.y) + child_ui.size.y * child_ui.scale;
        max_x = std::fmax(max_x, right);
        max_y = std::fmax(max_y, bottom);
    }

    // 也考虑 BoxLayout 的 padding
    if (registry.all_of<UIBoxLayoutComponent>(entity)) {
        const auto& box = registry.get<UIBoxLayoutComponent>(entity);
        max_x += box.padding.x;
        max_y += box.padding.y;
    }

    glm::vec2 new_size = ui.size;
    if (fitter.fit_width > 0) {
        new_size.x = max_x;
        if (fitter.min_size.x > 0.0f) new_size.x = std::fmax(new_size.x, fitter.min_size.x);
        if (fitter.max_size.x > 0.0f) new_size.x = std::fmin(new_size.x, fitter.max_size.x);
    }
    if (fitter.fit_height > 0) {
        new_size.y = max_y;
        if (fitter.min_size.y > 0.0f) new_size.y = std::fmax(new_size.y, fitter.min_size.y);
        if (fitter.max_size.y > 0.0f) new_size.y = std::fmin(new_size.y, fitter.max_size.y);
    }
    ui.size = new_size;
}

void UILayoutSystem::Update(entt::registry& registry, const glm::vec2& screen_size) {
    float global_scale = 1.0f;
    bool pixel_snap = false;

    auto scaler_view = registry.view<UICanvasScalerComponent>();
    for (auto entity : scaler_view) {
        const auto& scaler_comp = scaler_view.get<UICanvasScalerComponent>(entity);
        CanvasScalerData scaler_data{};
        scaler_data.reference_resolution = scaler_comp.reference_resolution;
        scaler_data.scale_factor = scaler_comp.scale_factor;
        scaler_data.match_width_or_height = scaler_comp.match_width_or_height;
        scaler_data.match = scaler_comp.match;
        scaler_data.pixel_snap = scaler_comp.pixel_snap;
        global_scale = CalculateScaleFactor(screen_size, scaler_data);
        pixel_snap = scaler_comp.pixel_snap;
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

        glm::vec2 pos = CalculateAnchorPosition(anchor_data, screen_size, global_scale);
        renderer.position = pixel_snap ? SnapToPixel(pos) : pos;
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

    // Box Layout (HBox / VBox)
    auto box_view = registry.view<UIBoxLayoutComponent>();
    for (auto entity : box_view) {
        UpdateBoxLayout(registry, entity, global_scale);
    }

    // Content Size Fitter (在 BoxLayout 之后，读取子元素排列结果)
    auto fitter_view = registry.view<UIContentSizeFitterComponent>();
    for (auto entity : fitter_view) {
        UpdateContentSizeFitter(registry, entity);
    }
}

} // namespace gameplay2d
} // namespace dse
