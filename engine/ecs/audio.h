/**
 * @file audio.h
 * @brief 音频源组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_AUDIO_H
#define DSE_ECS_COMPONENTS_2D_AUDIO_H

#include <memory>

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
    
    // Internal handle to audio engine (e.g., miniaudio)
    unsigned int runtime_handle = 0;                     ///< 引擎底层的音频句柄
};

#endif // DSE_ECS_COMPONENTS_2D_AUDIO_H
