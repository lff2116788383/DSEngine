/**
 * @file trail_renderer_2d.h
 * @brief 2D 拖尾/丝带渲染组件
 */

#ifndef DSE_ECS_TRAIL_RENDERER_2D_H
#define DSE_ECS_TRAIL_RENDERER_2D_H

#include <glm/glm.hpp>
#include <memory>
#include <vector>

class TextureAsset;

/**
 * @struct TrailPoint
 * @brief 拖尾上的单个采样点
 */
struct TrailPoint {
    glm::vec2 position;      ///< 世界坐标
    float width;             ///< 该点处的宽度
    float life_remaining;    ///< 剩余生命 (秒)
    glm::vec4 color;         ///< 该点处的颜色
};

/**
 * @struct TrailRenderer2DComponent
 * @brief 2D 拖尾渲染组件 (刀光、残影、彗星尾巴)
 */
struct TrailRenderer2DComponent {
    std::vector<TrailPoint> points;              ///< 活跃的拖尾点
    std::shared_ptr<TextureAsset> texture;       ///< 拖尾纹理 (沿长度方向 UV 映射)
    unsigned int texture_handle = 0;

    float lifetime = 0.5f;                       ///< 每个采样点的生命周期 (秒)
    float min_vertex_distance = 0.1f;            ///< 新增采样点的最小距离阈值
    float start_width = 0.5f;                    ///< 起始宽度
    float end_width = 0.0f;                      ///< 末端宽度
    glm::vec4 start_color = glm::vec4(1.0f);    ///< 起始颜色
    glm::vec4 end_color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f); ///< 末端颜色 (默认渐隐)
    int max_points = 128;                        ///< 最大采样点数
    int sorting_layer = 0;
    int order_in_layer = 0;
    bool emitting = true;                        ///< 是否持续发射新点
    bool world_space = true;                     ///< 点坐标是否在世界空间 (false=跟随实体)
};

#endif // DSE_ECS_TRAIL_RENDERER_2D_H
