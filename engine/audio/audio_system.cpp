/**
 * @file audio_system.cpp
 * @brief 音频系统管理，封装底层音频库，提供音效和背景音乐的播放控制
 */

#include "audio_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"
#include <iostream>
#include <algorithm>
#include <chrono>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>
#ifdef PlaySound
#undef PlaySound
#endif

namespace dse {
namespace gameplay2d {

struct CustomVFSFile {
    std::vector<uint8_t> data;
    size_t cursor = 0;
};

static ma_result CustomVFS_Open(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile) {
    if ((openMode & MA_OPEN_MODE_READ) == 0) return MA_ERROR;
    std::vector<uint8_t> data;
    if (!AssetManager::Instance().LoadFileToMemory(pFilePath, data)) {
        return MA_DOES_NOT_EXIST;
    }
    CustomVFSFile* f = new CustomVFSFile();
    f->data = std::move(data);
    f->cursor = 0;
    *pFile = (ma_vfs_file)f;
    return MA_SUCCESS;
}

static ma_result CustomVFS_OpenW(ma_vfs* pVFS, const wchar_t* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile) {
    return MA_NOT_IMPLEMENTED;
}

static ma_result CustomVFS_Close(ma_vfs* pVFS, ma_vfs_file file) {
    CustomVFSFile* f = (CustomVFSFile*)file;
    delete f;
    return MA_SUCCESS;
}

static ma_result CustomVFS_Read(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead) {
    CustomVFSFile* f = (CustomVFSFile*)file;
    size_t remaining = f->data.size() - f->cursor;
    size_t to_read = sizeInBytes < remaining ? sizeInBytes : remaining;
    if (to_read > 0) {
        memcpy(pDst, f->data.data() + f->cursor, to_read);
        f->cursor += to_read;
    }
    if (pBytesRead) *pBytesRead = to_read;
    return MA_SUCCESS;
}

static ma_result CustomVFS_Write(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten) {
    return MA_NOT_IMPLEMENTED;
}

static ma_result CustomVFS_Seek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin) {
    CustomVFSFile* f = (CustomVFSFile*)file;
    if (origin == ma_seek_origin_start) {
        f->cursor = offset;
    } else if (origin == ma_seek_origin_current) {
        f->cursor += offset;
    } else if (origin == ma_seek_origin_end) {
        f->cursor = f->data.size() + offset;
    }
    if (f->cursor > f->data.size()) f->cursor = f->data.size();
    return MA_SUCCESS;
}

static ma_result CustomVFS_Tell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor) {
    CustomVFSFile* f = (CustomVFSFile*)file;
    if (pCursor) *pCursor = f->cursor;
    return MA_SUCCESS;
}

static ma_result CustomVFS_Info(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo) {
    CustomVFSFile* f = (CustomVFSFile*)file;
    if (pInfo) {
        pInfo->sizeInBytes = f->data.size();
    }
    return MA_SUCCESS;
}

static ma_vfs_callbacks g_custom_vfs_callbacks = {
    CustomVFS_Open,
    CustomVFS_OpenW,
    CustomVFS_Close,
    CustomVFS_Read,
    CustomVFS_Write,
    CustomVFS_Seek,
    CustomVFS_Tell,
    CustomVFS_Info
};

AudioSystem::AudioSystem() {
}

AudioSystem::~AudioSystem() {
    Shutdown();
}

bool AudioSystem::Initialize() {
    if (is_initialized) {
        return true;
    }

    auto engine = std::make_unique<ma_engine>();
    ma_engine_config config = ma_engine_config_init();
    
    ma_resource_manager_config rmConfig = ma_resource_manager_config_init();
    rmConfig.pVFS = &g_custom_vfs_callbacks;
    
    ma_resource_manager* pResourceManager = new ma_resource_manager();
    ma_result rmResult = ma_resource_manager_init(&rmConfig, pResourceManager);
    if (rmResult == MA_SUCCESS) {
        config.pResourceManager = pResourceManager;
        ma_resource_manager_ptr = pResourceManager;
    }

    ma_result result = ma_engine_init(&config, engine.get());
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine." << std::endl;
        return false;
    }

    ma_engine_ptr = engine.release();
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
            auto new_sound = std::make_unique<ma_sound>();
            ma_result result = ma_sound_init_from_file(engine, audio.clip->GetPath().c_str(), 0, nullptr, nullptr, new_sound.get());
            if (result == MA_SUCCESS) {
                ma_sound_set_looping(new_sound.get(), audio.loop ? MA_TRUE : MA_FALSE);
                ma_sound_set_volume(new_sound.get(), audio.volume * sfx_volume_);
                ma_sound_set_pitch(new_sound.get(), audio.pitch);
                ma_sound_start(new_sound.get());
                entity_sounds_[key] = new_sound.get();
                sound = new_sound.release();
                audio.restart_requested = false;
            }
        }

        if (!sound) {
            audio.is_playing = false;
            audio.restart_requested = false;
            continue;
        }

        ma_sound_set_looping(sound, audio.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(sound, audio.volume * sfx_volume_);
        ma_sound_set_pitch(sound, audio.pitch);
        if (audio.restart_requested) {
            ma_sound_stop(sound);
            ma_sound_seek_to_pcm_frame(sound, 0);
            if (audio.is_playing || audio.play_on_awake) {
                ma_sound_start(sound);
            }
            audio.restart_requested = false;
        }
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
            audio.restart_requested = false;
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
    active_sfx_per_clip_.clear();
    sfx_clip_lookup_.clear();
    sfx_last_trigger_ms_.clear();
    if (!is_initialized || !ma_engine_ptr) {
        return;
    }
    ma_engine* engine = static_cast<ma_engine*>(ma_engine_ptr);
    ma_engine_uninit(engine);
    std::unique_ptr<ma_engine> engine_deleter(engine);
    
    if (ma_resource_manager_ptr) {
        ma_resource_manager* rm = static_cast<ma_resource_manager*>(ma_resource_manager_ptr);
        ma_resource_manager_uninit(rm);
        delete rm;
        ma_resource_manager_ptr = nullptr;
    }
    
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
    const auto now_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    auto last_it = sfx_last_trigger_ms_.find(filepath);
    if (last_it != sfx_last_trigger_ms_.end()) {
        std::uint64_t elapsed = now_ms >= last_it->second ? now_ms - last_it->second : 0;
        if (elapsed < sfx_trigger_cooldown_ms_) {
            return;
        }
    }
    auto active_it = active_sfx_per_clip_.find(filepath);
    if (active_it != active_sfx_per_clip_.end() && active_it->second >= max_concurrent_sfx_per_clip_) {
        return;
    }
    ma_engine* engine = static_cast<ma_engine*>(ma_engine_ptr);
    auto new_sound = std::make_unique<ma_sound>();
    ma_result result = ma_sound_init_from_file(engine, filepath.c_str(), 0, nullptr, nullptr, new_sound.get());
    if (result != MA_SUCCESS) {
        return;
    }
    ma_sound* sound = new_sound.release();
    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(sound, volume * sfx_volume_);
    ma_sound_start(sound);
    active_sfx_.push_back(sound);
    active_sfx_per_clip_[filepath] += 1;
    sfx_clip_lookup_[sound] = filepath;
    sfx_last_trigger_ms_[filepath] = now_ms;
}

bool AudioSystem::PlayBgm(const std::string& filepath, float volume, bool loop) {
    if (!is_initialized || !ma_engine_ptr || filepath.empty()) {
        return false;
    }
    StopBgm();
    ma_engine* engine = static_cast<ma_engine*>(ma_engine_ptr);
    auto new_sound = std::make_unique<ma_sound>();
    ma_result result = ma_sound_init_from_file(engine, filepath.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, new_sound.get());
    if (result != MA_SUCCESS) {
        return false;
    }
    ma_sound* sound = new_sound.release();
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
    active_sfx_per_clip_.clear();
    sfx_clip_lookup_.clear();
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

void AudioSystem::SetEntityPitch(std::uint32_t entity, float pitch) {
    if (pitch <= 0.01f) {
        pitch = 0.01f;
    }
    auto it = entity_sounds_.find(entity);
    if (it == entity_sounds_.end()) {
        return;
    }
    ma_sound_set_pitch(static_cast<ma_sound*>(it->second), pitch);
}

void AudioSystem::SetMaxConcurrentSfxPerClip(std::size_t max_instances) {
    max_concurrent_sfx_per_clip_ = max_instances == 0 ? 1 : max_instances;
}

void AudioSystem::SetSfxTriggerCooldownMs(std::uint32_t cooldown_ms) {
    sfx_trigger_cooldown_ms_ = cooldown_ms;
}

void AudioSystem::DestroySound(void* sound_ptr) {
    if (!sound_ptr) {
        return;
    }
    ma_sound* sound = static_cast<ma_sound*>(sound_ptr);
    ma_sound_stop(sound);
    ma_sound_uninit(sound);
    std::unique_ptr<ma_sound> sound_deleter(sound);
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
        auto clip_it = sfx_clip_lookup_.find(sound);
        if (clip_it != sfx_clip_lookup_.end()) {
            auto count_it = active_sfx_per_clip_.find(clip_it->second);
            if (count_it != active_sfx_per_clip_.end() && count_it->second > 0) {
                count_it->second -= 1;
                if (count_it->second == 0) {
                    active_sfx_per_clip_.erase(count_it);
                }
            }
            sfx_clip_lookup_.erase(clip_it);
        }
        DestroySound(sound);
        it = active_sfx_.erase(it);
    }
}

} // namespace gameplay2d
} // namespace dse
