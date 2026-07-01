/**
 * @file audio_spatial_2d_system.cpp
 * @brief 2D 音频空间化系统实现 — 基于 2D 距离的音量/声像/多普勒计算
 */

#include "modules/gameplay_2d/audio/audio_spatial_2d_system.h"
#include "engine/ecs/audio_spatial_2d.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/transform.h"
#include <cmath>
#include <algorithm>

void AudioSpatial2DSystem::Update(World& world, float delta_time) {
    auto& reg = world.registry();

    // Find listener position
    glm::vec2 listener_pos = {0.0f, 0.0f};
    float global_volume = 1.0f;
    auto listener_view = reg.view<AudioListener2DComponent, TransformComponent>();
    for (auto e : listener_view) {
        auto& al = listener_view.get<AudioListener2DComponent>(e);
        if (!al.enabled) continue;
        auto& t = listener_view.get<TransformComponent>(e);
        listener_pos = glm::vec2(t.position.x, t.position.y);
        global_volume = al.global_volume;
        break;
    }

    // Update all spatial audio sources
    auto view = reg.view<AudioSpatial2DComponent, AudioSourceComponent, TransformComponent>();
    for (auto entity : view) {
        auto& spatial = view.get<AudioSpatial2DComponent>(entity);
        auto& source = view.get<AudioSourceComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        glm::vec2 src_pos = glm::vec2(transform.position.x, transform.position.y);
        float distance = glm::length(src_pos - listener_pos);

        // --- Distance Attenuation ---
        float attenuation = 1.0f;
        float norm_dist = std::clamp((distance - spatial.min_distance) /
            std::max(spatial.max_distance - spatial.min_distance, 0.001f), 0.0f, 1.0f);

        switch (spatial.attenuation) {
        case AudioAttenuation2DModel::Linear:
            attenuation = 1.0f - norm_dist;
            break;
        case AudioAttenuation2DModel::InverseDistance:
            if (distance > spatial.min_distance) {
                attenuation = spatial.min_distance /
                    (spatial.min_distance + spatial.rolloff * (distance - spatial.min_distance));
            }
            break;
        case AudioAttenuation2DModel::Exponential:
            attenuation = std::pow(distance / std::max(spatial.min_distance, 0.001f), -spatial.rolloff);
            attenuation = std::clamp(attenuation, 0.0f, 1.0f);
            break;
        case AudioAttenuation2DModel::Custom:
            attenuation = 1.0f - norm_dist; // Fallback to linear
            break;
        }

        if (distance > spatial.max_distance) {
            attenuation = 0.0f;
        }

        spatial.computed_volume = attenuation * global_volume;

        // --- Pan (Stereo positioning) ---
        if (spatial.enable_pan && distance > 0.001f) {
            glm::vec2 dir = src_pos - listener_pos;
            float pan = std::clamp(dir.x / std::max(spatial.max_distance, 1.0f), -1.0f, 1.0f);
            spatial.computed_pan = pan * spatial.pan_strength;
        } else {
            spatial.computed_pan = 0.0f;
        }

        // --- Doppler Effect ---
        if (spatial.enable_doppler) {
            // Simplified doppler: based on radial velocity approximation
            // In a full implementation, we'd track previous position
            spatial.computed_pitch = 1.0f; // Placeholder — needs velocity tracking
        } else {
            spatial.computed_pitch = 1.0f;
        }

        // --- Occlusion ---
        if (spatial.enable_occlusion) {
            // Would need physics raycast from listener to source
            // For now, no occlusion applied (full implementation needs Physics2DSystem integration)
        }

        // Apply computed values to AudioSourceComponent
        source.volume = source.volume * spatial.computed_volume;
        source.pitch = source.pitch * spatial.computed_pitch;
    }
}
