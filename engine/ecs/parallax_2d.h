/**
 * @file parallax_2d.h
 * @brief 2D 视差滚动图层组件
 */

#ifndef DSE_ECS_PARALLAX_2D_H
#define DSE_ECS_PARALLAX_2D_H

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class TextureAsset;

/**
 * @struct ParallaxLayer
 * @brief 单个视差图层的数据
 */
struct ParallaxLayer {
    std::shared_ptr<TextureAsset> texture;       ///< 图层纹理
    unsigned int texture_handle = 0;             ///< RHI 纹理句柄
    std::string name;                            ///< 图层名称
    float scroll_factor_x = 1.0f;               ///< X 轴滚动因子 (0=不动, 1=与相机同步)
    float scroll_factor_y = 1.0f;               ///< Y 轴滚动因子
    float offset_x = 0.0f;                       ///< X 轴基础偏移
    float offset_y = 0.0f;                       ///< Y 轴基础偏移
    float auto_scroll_x = 0.0f;                  ///< X 轴自动滚动速度 (像素/秒)
    float auto_scroll_y = 0.0f;                  ///< Y 轴自动滚动速度 (像素/秒)
    float scale = 1.0f;                          ///< 图层缩放
    float opacity = 1.0f;                        ///< 图层透明度 [0,1]
    glm::vec4 tint = glm::vec4(1.0f);           ///< 染色
    int sorting_order = 0;                       ///< 渲染顺序 (小先画)
    bool repeat_x = true;                        ///< X 方向平铺
    bool repeat_y = false;                       ///< Y 方向平铺
    bool visible = true;                         ///< 是否可见
};

/**
 * @struct ParallaxComponent
 * @brief 视差滚动组件，管理多个图层的视差运动
 */
struct ParallaxComponent {
    std::vector<ParallaxLayer> layers;           ///< 所有视差图层
    bool enabled = true;                         ///< 是否启用视差系统
    glm::vec2 accumulated_scroll = {0.0f, 0.0f}; ///< 自动滚动累计量
};

#endif // DSE_ECS_PARALLAX_2D_H
