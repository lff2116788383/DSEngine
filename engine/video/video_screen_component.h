/**
 * @file video_screen_component.h
 * @brief ECS 组件：3D 世界内视频屏幕
 */

#ifndef DSE_VIDEO_SCREEN_COMPONENT_H
#define DSE_VIDEO_SCREEN_COMPONENT_H

#include "engine/video/video_types.h"
#include <string>
#include <cstdint>

namespace dse {
namespace video {

struct VideoScreenComponent {
    bool enabled = true;
    std::string video_path;
    bool auto_play = true;
    bool loop = true;
    float playback_rate = 1.0f;
    DecoderBackend backend = DecoderBackend::Auto;

    // Runtime state (managed by VideoSystem)
    uint32_t player_handle = 0;
    uint32_t current_texture = 0;
};

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_SCREEN_COMPONENT_H
