/**
 * @file sprite.h
 * @brief 2D 精灵与材质渲染组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_SPRITE_H
#define DSE_ECS_COMPONENTS_2D_SPRITE_H

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <entt/entt.hpp>

class TextureAsset;
using Entity = entt::entity;

/**
 * @enum SpriteBlendMode
 * @brief 渲染混合模式枚举
 */
enum class SpriteBlendMode {
    Alpha = 0,    ///< 传统的 Alpha 透明度混合
    Additive = 1, ///< 叠加混合（发光效果）
    Multiply = 2  ///< 正片叠底混合（阴影/加深效果）
};

/**
 * @struct MaterialInstanceComponent
 * @brief 材质实例组件，覆盖全局材质的属性
 */
struct MaterialInstanceComponent {
    unsigned int material_id = 0;
    std::string name;
    std::string shader_variant = "SPRITE_UNLIT";
    SpriteBlendMode blend_mode = SpriteBlendMode::Alpha;
    unsigned int texture_handle = 0;
    glm::vec4 tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 uv_rect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
};

/**
 * @struct SpriteRendererComponent
 * @brief 2D 精灵图渲染组件，负责在场景中绘制纹理切片
 */
struct SpriteRendererComponent {
    std::shared_ptr<TextureAsset> texture;               ///< 持有的纹理资产引用
    unsigned int texture_handle = 0;                     ///< RHI 层的纹理句柄
    unsigned int material_instance_id = 0;               ///< 绑定的材质实例 ID
    std::string shader_variant = "SPRITE_UNLIT";         ///< 使用的着色器变体
    SpriteBlendMode blend_mode = SpriteBlendMode::Alpha; ///< 混合模式
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); ///< 顶点颜色/染色
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);    ///< 纹理的采样区域 (x,y,w,h)
    glm::vec2 uv_offset = glm::vec2(0.0f, 0.0f);         ///< UV 滚动的当前偏移量
    glm::vec2 uv_scroll_speed = glm::vec2(0.0f, 0.0f);   ///< UV 滚动的速度 (x, y)
    int sorting_layer = 0;                               ///< 渲染层级(大类)
    int order_in_layer = 0;                              ///< 层级内的渲染顺序(小类)
    bool visible = true;                                 ///< 是否可见
};

/**
 * @struct SpineRendererComponent
 * @brief Spine 2D 骨骼动画渲染组件
 */
struct SpineRendererComponent {
    struct RuntimeHandle {
        virtual ~RuntimeHandle() = default;
    };

    std::string skeleton_data_path;                      ///< 骨骼数据路径 (.skel / .json)
    std::string atlas_path;                              ///< 图集路径 (.atlas)
    std::shared_ptr<RuntimeHandle> runtime;              ///< Spine runtime 统一句柄
    std::vector<std::shared_ptr<TextureAsset>> textures; ///< 持有的纹理资产
    int sorting_layer = 0;
    int order_in_layer = 0;
    bool visible = true;
    float time_scale = 1.0f;                             ///< 动画时间缩放
    std::string current_animation = "";                  ///< 当前播放的动画名
    bool loop = true;                                    ///< 是否循环
    bool dirty_animation = false;                        ///< 标记是否需要应用新动画
};

#endif // DSE_ECS_COMPONENTS_2D_SPRITE_H
