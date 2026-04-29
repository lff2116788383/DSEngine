/**
 * @file audio.h
 * @brief 音频源组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_AUDIO_H
#define DSE_ECS_COMPONENTS_2D_AUDIO_H

#include <memory>
#include <algorithm>

class AudioClipAsset;

/**
 * @struct AudioSourceComponent
 * @brief 音频源组件，挂载在实体上用于播放 3D/2D 空间音效
 */
struct AudioSourceComponent {
    std::shared_ptr<AudioClipAsset> clip;                ///< 引用的音频片段资产
    bool play_on_awake = true;                           ///< 是否在组件创建时自动播放
    bool loop = false;                                   ///< 是否循环播放
    float volume = 1.0f;                                 ///< 音量大小 (0.0 - 1.0)
    float pitch = 1.0f;                                  ///< 音高倍数 (1.0 为原始音高)
    bool is_playing = false;                             ///< 当前是否正在播放
    bool restart_requested = false;                      ///< 是否请求重新开始播放

    bool spatial_enabled = false;                        ///< 是否启用 3D 空间化
    float min_distance = 1.0f;                           ///< 3D 衰减起始距离
    float max_distance = 20.0f;                          ///< 3D 衰减最大距离
    float rolloff = 1.0f;                                ///< 3D 距离衰减曲线系数
    
    // Internal handle to audio engine (e.g., miniaudio)
    unsigned int runtime_handle = 0;                     ///< 引擎底层的音频句柄
};

/**
 * @struct AudioListenerComponent
 * @brief 3D 音频监听器组件，位置由实体 TransformComponent 同步到底层音频引擎
 */
struct AudioListenerComponent {
    bool enabled = true;                                 ///< 是否作为 listener 参与 3D 音频
    unsigned int listener_index = 0;                     ///< miniaudio listener 索引，当前固定使用 0
};

#endif // DSE_ECS_COMPONENTS_2D_AUDIO_H
