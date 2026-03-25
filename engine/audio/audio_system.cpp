#include "audio_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"
#include <iostream>
#include <algorithm>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>
#ifdef PlaySound
#undef PlaySound
#endif

namespace dse {
namespace gameplay2d {

AudioSystem::AudioSystem() {
}

AudioSystem::~AudioSystem() {
    Shutdown();
}

bool AudioSystem::Initialize() {
    if (is_initialized) {
        return true;
    }

    ma_engine* engine = new ma_engine;
    ma_result result = ma_engine_init(nullptr, engine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine." << std::endl;
        delete engine;
        return false;
    }

    ma_engine_ptr = engine;
    SetMasterVolume(master_volume_);
    is_initialized = true;
    return true;
}

void AudioSystem::Update(entt::registry& registry, float dt) {
    (void)dt;
    if (!is_initialized || !ma_engine_ptr) {
        return;
    }

    for (auto it = entity_sounds_.begin(); it != entity_sounds_.end();) {
        Entity entity = static_cast<Entity>(it->first);
        if (!registry.valid(entity) || !registry.all_of<AudioSourceComponent>(entity)) {
            DestroySound(it->second);
            it = entity_sounds_.erase(it);
        } else {
            ++it;
        }
    }

    auto view = registry.view<AudioSourceComponent>();
    for (auto entity : view) {
        auto& audio = view.get<AudioSourceComponent>(entity);
        const std::uint32_t key = static_cast<std::uint32_t>(entity);
        auto sound_it = entity_sounds_.find(key);
        ma_sound* sound = sound_it != entity_sounds_.end() ? static_cast<ma_sound*>(sound_it->second) : nullptr;

        if (!sound && audio.clip && (audio.play_on_awake || audio.is_playing)) {
            ma_engine* engine = static_cast<ma_engine*>(ma_engine_ptr);
            ma_sound* new_sound = new ma_sound;
            ma_result result = ma_sound_init_from_file(engine, audio.clip->GetPath().c_str(), 0, nullptr, nullptr, new_sound);
            if (result == MA_SUCCESS) {
                ma_sound_set_looping(new_sound, audio.loop ? MA_TRUE : MA_FALSE);
                ma_sound_set_volume(new_sound, audio.volume * sfx_volume_);
                ma_sound_start(new_sound);
                entity_sounds_[key] = new_sound;
                sound = new_sound;
            } else {
                delete new_sound;
            }
        }

        if (!sound) {
            audio.is_playing = false;
            continue;
        }

        ma_sound_set_looping(sound, audio.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(sound, audio.volume * sfx_volume_);
        const bool should_play = audio.is_playing || audio.play_on_awake;
        const bool now_playing_before = ma_sound_is_playing(sound) == MA_TRUE;

        if (should_play && !now_playing_before) {
            ma_sound_start(sound);
        } else if (!should_play && now_playing_before) {
            ma_sound_stop(sound);
        }

        audio.play_on_awake = false;
        const bool now_playing = ma_sound_is_playing(sound) == MA_TRUE;

        if (!audio.loop && !now_playing) {
            DestroySound(sound);
            entity_sounds_.erase(key);
            audio.runtime_handle = 0;
            audio.is_playing = false;
            continue;
        }

        if (now_playing) {
            audio.runtime_handle = static_cast<unsigned int>(key);
        }
        audio.is_playing = now_playing;
    }

    CleanupFinishedSfx();
}

void AudioSystem::Shutdown() {
    StopBgm();
    StopAllSfx();
    for (auto& pair : entity_sounds_) {
        DestroySound(pair.second);
    }
    entity_sounds_.clear();
    if (!is_initialized || !ma_engine_ptr) {
        return;
    }
    ma_engine* engine = static_cast<ma_engine*>(ma_engine_ptr);
    ma_engine_uninit(engine);
    delete engine;
    ma_engine_ptr = nullptr;
    is_initialized = false;
}

void AudioSystem::PlaySound(const std::string& filepath, float volume) {
    PlaySfx(filepath, volume, false);
}

void AudioSystem::PlaySfx(const std::string& filepath, float volume, bool loop) {
    if (!is_initialized || !ma_engine_ptr || filepath.empty()) {
        return;
    }
    ma_engine* engine = static_cast<ma_engine*>(ma_engine_ptr);
    ma_sound* sound = new ma_sound;
    ma_result result = ma_sound_init_from_file(engine, filepath.c_str(), 0, nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        delete sound;
        return;
    }
    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(sound, volume * sfx_volume_);
    ma_sound_start(sound);
    active_sfx_.push_back(sound);
}

bool AudioSystem::PlayBgm(const std::string& filepath, float volume, bool loop) {
    if (!is_initialized || !ma_engine_ptr || filepath.empty()) {
        return false;
    }
    StopBgm();
    ma_engine* engine = static_cast<ma_engine*>(ma_engine_ptr);
    ma_sound* sound = new ma_sound;
    ma_result result = ma_sound_init_from_file(engine, filepath.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        delete sound;
        return false;
    }
    bgm_sound_ptr_ = sound;
    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(sound, volume * bgm_volume_);
    ma_sound_start(sound);
    return true;
}

void AudioSystem::PauseBgm() {
    if (!bgm_sound_ptr_) {
        return;
    }
    ma_sound_stop(static_cast<ma_sound*>(bgm_sound_ptr_));
}

void AudioSystem::ResumeBgm() {
    if (!bgm_sound_ptr_) {
        return;
    }
    ma_sound_start(static_cast<ma_sound*>(bgm_sound_ptr_));
}

void AudioSystem::StopBgm() {
    if (!bgm_sound_ptr_) {
        return;
    }
    DestroySound(bgm_sound_ptr_);
    bgm_sound_ptr_ = nullptr;
}

void AudioSystem::StopAllSfx() {
    for (auto* sound_ptr : active_sfx_) {
        DestroySound(sound_ptr);
    }
    active_sfx_.clear();
}

void AudioSystem::SetMasterVolume(float volume) {
    master_volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (is_initialized && ma_engine_ptr) {
        ma_engine_set_volume(static_cast<ma_engine*>(ma_engine_ptr), master_volume_);
    }
}

void AudioSystem::SetBgmVolume(float volume) {
    bgm_volume_ = std::clamp(volume, 0.0f, 1.0f);
    ApplyAllVolumes();
}

void AudioSystem::SetSfxVolume(float volume) {
    sfx_volume_ = std::clamp(volume, 0.0f, 1.0f);
    ApplyAllVolumes();
}

void AudioSystem::DestroySound(void* sound_ptr) {
    if (!sound_ptr) {
        return;
    }
    ma_sound* sound = static_cast<ma_sound*>(sound_ptr);
    ma_sound_stop(sound);
    ma_sound_uninit(sound);
    delete sound;
}

void AudioSystem::ApplyAllVolumes() {
    if (bgm_sound_ptr_) {
        ma_sound_set_volume(static_cast<ma_sound*>(bgm_sound_ptr_), bgm_volume_);
    }
    for (auto* sound_ptr : active_sfx_) {
        ma_sound_set_volume(static_cast<ma_sound*>(sound_ptr), sfx_volume_);
    }
    for (auto& pair : entity_sounds_) {
        ma_sound_set_volume(static_cast<ma_sound*>(pair.second), sfx_volume_);
    }
}

void AudioSystem::CleanupFinishedSfx() {
    for (auto it = active_sfx_.begin(); it != active_sfx_.end();) {
        ma_sound* sound = static_cast<ma_sound*>(*it);
        if (ma_sound_is_playing(sound) == MA_TRUE) {
            ++it;
            continue;
        }
        DestroySound(sound);
        it = active_sfx_.erase(it);
    }
}

} // namespace gameplay2d
} // namespace dse
