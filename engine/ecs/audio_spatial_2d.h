/**
 * @file audio_spatial_2d.h
 * @brief 2D 音频空间化组件 (距离衰减、声障遮挡)
 */

#ifndef DSE_ECS_AUDIO_SPATIAL_2D_H
#define DSE_ECS_AUDIO_SPATIAL_2D_H

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <string>

using Entity = entt::entity;

/**
 * @enum AudioAttenuation2DModel
 * @brief 2D 距离衰减模型
 */
enum class AudioAttenuation2DModel {
    Linear = 0,        ///< 线性衰减 (简单，适合小地图)
    InverseDistance,    ///< 反比例衰减 (物理真实)
    Exponential,       ///< 指数衰减 (快速衰减)
    Custom             ///< 自定义衰减曲线
};

/**
 * @struct AudioSpatial2DComponent
 * @brief 2D 空间化音频组件，基于 2D 距离驱动音量/声像
 */
struct AudioSpatial2DComponent {
    float min_distance = 1.0f;       ///< 无衰减区域半径
    float max_distance = 20.0f;      ///< 完全静音距离
    float rolloff = 1.0f;            ///< 衰减因子
    AudioAttenuation2DModel attenuation = AudioAttenuation2DModel::InverseDistance;

    // 声像 (Pan)
    bool enable_pan = true;          ///< 是否启用左右声像
    float pan_strength = 1.0f;       ///< 声像强度 (0=始终中置, 1=完全基于位置)

    // 遮挡
    bool enable_occlusion = false;   ///< 是否启用物理遮挡
    float occlusion_factor = 0.3f;   ///< 被遮挡时的音量乘数
    int occlusion_layer_mask = -1;   ///< 遮挡检测的物理层掩码

    // 多普勒
    bool enable_doppler = false;     ///< 是否启用多普勒效果
    float doppler_factor = 1.0f;     ///< 多普勒强度

    // 运行时状态
    float computed_volume = 1.0f;    ///< 计算后的最终音量 [0,1]
    float computed_pan = 0.0f;       ///< 计算后的声像 [-1,1] (左右)
    float computed_pitch = 1.0f;     ///< 计算后的音高 (多普勒)
};

/**
 * @struct AudioListener2DComponent
 * @brief 2D 音频监听器 (通常挂在相机实体上)
 */
struct AudioListener2DComponent {
    bool enabled = true;
    float global_volume = 1.0f;      ///< 全局音量倍数
};

#endif // DSE_ECS_AUDIO_SPATIAL_2D_H
