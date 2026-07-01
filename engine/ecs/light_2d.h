/**
 * @file light_2d.h
 * @brief 2D 灯光与阴影组件
 */

#ifndef DSE_ECS_LIGHT_2D_H
#define DSE_ECS_LIGHT_2D_H

#include <glm/glm.hpp>
#include <memory>

class TextureAsset;

/**
 * @enum Light2DType
 * @brief 2D 灯光类型
 */
enum class Light2DType {
    Point = 0,       ///< 点光源 (全方位)
    Spot = 1,        ///< 聚光灯 (锥形)
    Directional = 2  ///< 方向光 (全局平行光)
};

/**
 * @enum Shadow2DMode
 * @brief 2D 阴影模式
 */
enum class Shadow2DMode {
    None = 0,        ///< 不投射阴影
    Hard = 1,        ///< 硬阴影 (ray-march)
    Soft = 2         ///< 软阴影 (PCF 模糊)
};

/**
 * @struct Light2DComponent
 * @brief 2D 灯光组件，驱动 2D 场景光照
 */
struct Light2DComponent {
    Light2DType type = Light2DType::Point;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f); ///< 灯光颜色 (RGB)
    float intensity = 1.0f;                          ///< 强度倍数
    float range = 5.0f;                              ///< 影响半径 (Point/Spot)
    float falloff = 2.0f;                            ///< 衰减指数 (越大越集中)
    float spot_angle = 45.0f;                        ///< 聚光锥角度 (度, Spot only)
    float spot_outer_angle = 60.0f;                  ///< 聚光外锥角度 (度, Spot only)
    float direction_angle = 0.0f;                    ///< 方向角度 (度, Spot/Directional)
    Shadow2DMode shadow_mode = Shadow2DMode::None;
    float shadow_strength = 0.8f;                    ///< 阴影强度 [0,1]
    int shadow_ray_count = 64;                       ///< 阴影光线数 (ray-march 采样数)
    std::shared_ptr<TextureAsset> cookie_texture;    ///< 可选的 cookie 纹理 (光照形状遮罩)
    unsigned int cookie_handle = 0;
    bool enabled = true;
    int layer_mask = -1;                             ///< 影响的图层掩码 (-1=全部)
};

/**
 * @struct NormalMap2DComponent
 * @brief 2D 法线贴图组件，让 sprite 响应 2D 灯光
 */
struct NormalMap2DComponent {
    std::shared_ptr<TextureAsset> normal_texture;    ///< 法线贴图
    unsigned int normal_handle = 0;                  ///< RHI 纹理句柄
    float normal_strength = 1.0f;                    ///< 法线贴图强度
};

/**
 * @struct Ambient2DComponent
 * @brief 2D 场景全局环境光设置 (单例组件)
 */
struct Ambient2DComponent {
    glm::vec3 color = glm::vec3(0.2f, 0.2f, 0.3f); ///< 环境光颜色
    float intensity = 0.5f;                          ///< 环境光强度
};

#endif // DSE_ECS_LIGHT_2D_H
