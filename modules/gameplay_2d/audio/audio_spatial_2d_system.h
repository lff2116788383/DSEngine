/**
 * @file audio_spatial_2d_system.h
 * @brief 2D 音频空间化系统
 */

#ifndef DSE_AUDIO_SPATIAL_2D_SYSTEM_H
#define DSE_AUDIO_SPATIAL_2D_SYSTEM_H

#include "engine/ecs/world.h"

class AudioSpatial2DSystem {
public:
    void Update(World& world, float delta_time);
};

#endif // DSE_AUDIO_SPATIAL_2D_SYSTEM_H
