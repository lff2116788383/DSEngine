/**
 * @file line_renderer_2d.h
 * @brief 2D 折线/曲线渲染组件
 */

#ifndef DSE_ECS_LINE_RENDERER_2D_H
#define DSE_ECS_LINE_RENDERER_2D_H

#include <glm/glm.hpp>
#include <memory>
#include <vector>

class TextureAsset;

/**
 * @enum LineCapMode
 * @brief 线段端点样式
 */
enum class LineCapMode {
    None = 0,    ///< 无端帽 (方形截断)
    Round = 1    ///< 圆形端帽
};

/**
 * @enum LineJoinMode
 * @brief 线段拐角样式
 */
enum class LineJoinMode {
    Miter = 0,   ///< 尖角
    Bevel = 1,   ///< 斜切
    Round = 2    ///< 圆角
};

/**
 * @struct LineRenderer2DComponent
 * @brief 2D 折线渲染组件 (激光、路径、轨迹线、UI 连线)
 */
struct LineRenderer2DComponent {
    std::vector<glm::vec2> points;               ///< 折线顶点 (局部坐标)
    std::vector<glm::vec4> colors;               ///< 每顶点颜色 (可选，空时用 start/end_color)
    std::shared_ptr<TextureAsset> texture;       ///< 可选纹理 (沿折线 UV 平铺)
    unsigned int texture_handle = 0;

    float width = 0.1f;                          ///< 线宽
    float start_width = -1.0f;                   ///< 起始宽度 (<0 时用 width)
    float end_width = -1.0f;                     ///< 末端宽度 (<0 时用 width)
    glm::vec4 start_color = glm::vec4(1.0f);    ///< 起始颜色 (无 per-vertex 时)
    glm::vec4 end_color = glm::vec4(1.0f);      ///< 末端颜色
    LineCapMode cap = LineCapMode::None;
    LineJoinMode join = LineJoinMode::Miter;
    float miter_limit = 4.0f;                    ///< Miter 尖角限制
    bool closed = false;                         ///< 是否闭合 (首尾相连)
    bool use_world_space = false;                ///< 点坐标是否为世界空间
    int sorting_layer = 0;
    int order_in_layer = 0;
    bool visible = true;
};

#endif // DSE_ECS_LINE_RENDERER_2D_H
