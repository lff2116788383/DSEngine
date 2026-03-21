#include "audio/audio_system.h"
#include "phase1/ecs/components_2d.h"
#include "utils/debug.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace audio {

void AudioSystem::Init() {
    if (ma_engine_init(NULL, &engine_) != MA_SUCCESS) {
        DEBUG_LOG_ERROR("Failed to initialize miniaudio engine.");
        return;
    }
    is_initialized_ = true;
    DEBUG_LOG_INFO("AudioSystem Initialized with miniaudio");
}

void AudioSystem::Update(Phase1World& world) {
    if (!is_initialized_) return;

    auto view = world.registry().view<AudioSourceComponent>();
    
    for (auto entity : view) {
        auto& audio_src = view.get<AudioSourceComponent>(entity);
        
        if (audio_src.play_on_awake && !audio_src.is_playing) {
            PlaySound(audio_src.clip_path, audio_src.loop, audio_src.volume);
            audio_src.is_playing = true;
        }
    }
}

void AudioSystem::PlaySound(const std::string& filepath, bool loop, float volume) {
    if (!is_initialized_ || filepath.empty()) return;
    DEBUG_LOG_INFO("AudioSystem: Playing sound {} (loop: {}, vol: {})", filepath, loop, volume);
    
    // For simple fire-and-forget sound playback:
    ma_engine_play_sound(&engine_, filepath.c_str(), NULL);
    
    // Note: To handle looping and volume dynamically per component, 
    // we would need to allocate ma_sound objects and store them in the AudioSourceComponent.
    // For the Phase 1 architecture, fire-and-forget is a good starting point.
}

void AudioSystem::StopAll() {
    DEBUG_LOG_INFO("AudioSystem: Stop All");
    if (is_initialized_) {
        // ma_engine_stop_group or stopping individual sounds requires managing sound instances.
        // As a fallback, we could re-init the engine, but typically you manage ma_sound instances.
    }
}

void AudioSystem::Shutdown() {
    if (is_initialized_) {
        ma_engine_uninit(&engine_);
        is_initialized_ = false;
    }
    DEBUG_LOG_INFO("AudioSystem Shutdown");
}

} // namespace audio
