#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <entt/entt.hpp>

struct EditorContext;

namespace dse::editor {

/// 植被刷作用目标：草地 (GrassComponent) 或 树木 (TreeComponent)。
enum class VegetationTarget {
    Grass,
    Tree
};

/// 植被刷模式：种植（密度→1）或清除（密度→0）。
enum class VegetationBrushMode {
    Plant,
    Clear
};

/// 植被刷编辑器状态（单例，跨帧保留）。
struct VegetationEditorState {
    VegetationTarget target = VegetationTarget::Grass;
    VegetationBrushMode brush_mode = VegetationBrushMode::Plant;

    float brush_radius = 8.0f;
    float brush_strength = 0.6f;
    float brush_falloff = 0.5f;     // 0 = 硬边，1 = 高斯软边

    // 遮罩网格参数（首次绘制时按此初始化）
    int   mask_resolution = 128;    // 每边格点数
    float mask_world_size = 200.0f; // 遮罩覆盖的世界 XZ 边长（以激活实体为中心）

    entt::entity active_entity = entt::null;
    bool editing_active = false;
    bool painting = false;

    // 笔刷命中（每帧由 overlay 更新）
    glm::vec3 last_brush_hit{0.0f};
    bool last_brush_hit_valid = false;

    // Undo：笔触开始时的遮罩快照
    std::vector<float> mask_snapshot;
};

VegetationEditorState& GetVegetationEditorState();

/// 绘制 Vegetation Brush 面板（目标/模式/参数）。
void DrawVegetationEditorPanel(EditorContext& ctx);

/// 在 Scene 视口绘制笔刷圆圈叠加。
void DrawVegetationBrushOverlay(entt::registry& registry,
                                const glm::vec2& window_pos,
                                const glm::vec2& panel_size,
                                const glm::mat4& view,
                                const glm::mat4& proj);

/// 处理 Scene 视口中的鼠标植被绘制。返回 true 表示消费了绘制动作。
bool HandleVegetationViewportPaint(entt::registry& registry,
                                   const glm::vec2& window_pos,
                                   const glm::vec2& panel_size,
                                   const glm::mat4& view,
                                   const glm::mat4& proj,
                                   float delta_time);

}  // namespace dse::editor
