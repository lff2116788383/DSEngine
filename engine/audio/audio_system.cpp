/**
 * @file audio_system.cpp
 * @brief 音频系统管理，封装底层音频库，提供音效和背景音乐的播放控制
 */

#include "audio_system.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <glm/gtc/quaternion.hpp>

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

namespace {
AssetManager& RequireAssetManager(AssetManager* asset_manager) {
    if (asset_manager != nullptr) {
        return *asset_manager;
    }
    throw std::runtime_error("AudioSystem requires an injected AssetManager");
}
}

static AssetManager* g_audio_asset_manager = nullptr;

static ma_result CustomVFS_Open(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile) {
    if ((openMode & MA_OPEN_MODE_READ) == 0) return MA_ERROR;
    (void)pVFS;
    std::vector<uint8_t> data;
    if (!RequireAssetManager(g_audio_asset_manager).LoadFileToMemory(pFilePath, data)) {
        return MA_DOES_NOT_EXIST;
    }
    auto* f = new CustomVFSFile();
    f->data = std::move(data);
    f->cursor = 0;
    *pFile = (ma_vfs_file)f;
    return MA_SUCCESS;
}

static ma_result CustomVFS_OpenW(ma_vfs* pVFS, const wchar_t* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile) {
    (void)pVFS;
    (void)pFilePath;
    (void)openMode;
    (void)pFile;
    return MA_NOT_IMPLEMENTED;
}

static ma_result CustomVFS_Close(ma_vfs* pVFS, ma_vfs_file file) {
    (void)pVFS;
    auto* f = static_cast<CustomVFSFile*>(file);
    delete f;
    return MA_SUCCESS;
}

static ma_result CustomVFS_Read(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead) {
    (void)pVFS;
    auto* f = static_cast<CustomVFSFile*>(file);
    const size_t remaining = f->data.size() - f->cursor;
    const size_t to_read = sizeInBytes < remaining ? sizeInBytes : remaining;
    if (to_read > 0) {
        memcpy(pDst, f->data.data() + f->cursor, to_read);
        f->cursor += to_read;
    }
    if (pBytesRead) *pBytesRead = to_read;
    return MA_SUCCESS;
}

static ma_result CustomVFS_Write(ma_vfs* pVFS, ma_vfs_file file, const void* pSrc, size_t sizeInBytes, size_t* pBytesWritten) {
    (void)pVFS;
    (void)file;
    (void)pSrc;
    (void)sizeInBytes;
    (void)pBytesWritten;
    return MA_NOT_IMPLEMENTED;
}

static ma_result CustomVFS_Seek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin) {
    (void)pVFS;
    auto* f = static_cast<CustomVFSFile*>(file);
    if (origin == ma_seek_origin_start) {
        f->cursor = static_cast<size_t>(offset);
    } else if (origin == ma_seek_origin_current) {
        f->cursor += static_cast<size_t>(offset);
    } else if (origin == ma_seek_origin_end) {
        f->cursor = f->data.size() + static_cast<size_t>(offset);
    }
    if (f->cursor > f->data.size()) f->cursor = f->data.size();
    return MA_SUCCESS;
}

static ma_result CustomVFS_Tell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor) {
    (void)pVFS;
    auto* f = static_cast<CustomVFSFile*>(file);
    if (pCursor) *pCursor = static_cast<ma_int64>(f->cursor);
    return MA_SUCCESS;
}

static ma_result CustomVFS_Info(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo) {
    (void)pVFS;
    auto* f = static_cast<CustomVFSFile*>(file);
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

struct AudioSystem::EngineHandle {
    ma_engine value{};
    bool initialized = false;

    ~EngineHandle() {
        if (initialized) {
            ma_engine_uninit(&value);
        }
    }

    ma_engine* get() {
        return initialized ? &value : nullptr;
    }

    const ma_engine* get() const {
        return initialized ? &value : nullptr;
    }
};

struct AudioSystem::ResourceManagerHandle {
    ma_resource_manager value{};
    bool initialized = false;

    ~ResourceManagerHandle() {
        if (initialized) {
            ma_resource_manager_uninit(&value);
        }
    }

    ma_resource_manager* get() {
        return initialized ? &value : nullptr;
    }
};

struct AudioSystem::SoundHandle {
    ma_sound value{};
    bool initialized = false;

    ~SoundHandle() {
        if (initialized) {
            ma_sound_stop(&value);
            ma_sound_uninit(&value);
        }
    }

    ma_sound* get() {
        return initialized ? &value : nullptr;
    }

    const ma_sound* get() const {
        return initialized ? &value : nullptr;
    }
};

AudioSystem::AudioSystem() {
}

AudioSystem::~AudioSystem() {
    Shutdown();
}

bool AudioSystem::Initialize(AssetManager* asset_manager) {
    if (is_initialized) {
        return true;
    }

    asset_manager_ = asset_manager;
    if (!asset_manager_) {
        throw std::runtime_error("AudioSystem::Initialize requires an injected AssetManager");
    }

    g_audio_asset_manager = asset_manager_;

    auto resource_manager = std::make_unique<ResourceManagerHandle>();
    ma_resource_manager_config rm_config = ma_resource_manager_config_init();
    rm_config.pVFS = &g_custom_vfs_callbacks;
    if (ma_resource_manager_init(&rm_config, &resource_manager->value) == MA_SUCCESS) {
        resource_manager->initialized = true;
    }

    auto engine = std::make_unique<EngineHandle>();
    ma_engine_config config = ma_engine_config_init();
    if (resource_manager->initialized) {
        config.pResourceManager = resource_manager->get();
    }

    const ma_result result = ma_engine_init(&config, &engine->value);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine." << std::endl;
        g_audio_asset_manager = nullptr;
        asset_manager_ = nullptr;
        return false;
    }

    engine->initialized = true;
    resource_manager_handle_ = std::move(resource_manager);
    engine_handle_ = std::move(engine);
    SetMasterVolume(master_volume_);
    is_initialized = true;
    return true;
}

namespace {
ma_attenuation_model MapAttenuationModel(AudioAttenuationModel model) {
    switch (model) {
        case AudioAttenuationModel::Linear:      return ma_attenuation_model_linear;
        case AudioAttenuationModel::Exponential: return ma_attenuation_model_exponential;
        case AudioAttenuationModel::Inverse:
        default:                                 return ma_attenuation_model_inverse;
    }
}
} // anonymous namespace

void AudioSystem::Update(entt::registry& registry, float dt) {
    (void)dt;
    if (!is_initialized || !engine_handle_ || !engine_handle_->get()) {
        return;
    }

    for (auto it = entity_sounds_.begin(); it != entity_sounds_.end();) {
        Entity entity = static_cast<Entity>(it->first);
        if (!registry.valid(entity) || !registry.all_of<AudioSourceComponent>(entity)) {
            it = entity_sounds_.erase(it);
        } else {
            ++it;
        }
    }

    // --- Phase 1: Listener position + orientation ---
    glm::vec3 listener_pos(0.0f);
    glm::vec3 listener_fwd(0.0f, 0.0f, -1.0f);
    glm::vec3 listener_up(0.0f, 1.0f, 0.0f);
    bool listener_found = false;
    unsigned int active_listener_index = 0;

    auto listener_view = registry.view<AudioListenerComponent, TransformComponent>();
    for (auto entity : listener_view) {
        const auto& listener = listener_view.get<AudioListenerComponent>(entity);
        if (!listener.enabled) {
            continue;
        }
        const auto& transform = listener_view.get<TransformComponent>(entity);
        listener_pos = transform.position;
        listener_fwd = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        listener_up  = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        active_listener_index = listener.listener_index;
        listener_found = true;
        break;
    }

    // Camera3DComponent fallback: auto-follow camera if no explicit listener
    if (!listener_found) {
        auto camera_view = registry.view<Camera3DComponent, TransformComponent>();
        for (auto entity : camera_view) {
            const auto& camera = camera_view.get<Camera3DComponent>(entity);
            if (!camera.enabled) continue;
            const auto& transform = camera_view.get<TransformComponent>(entity);
            listener_pos = transform.position;
            listener_fwd = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            listener_up  = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            active_listener_index = 0;
            listener_found = true;
            break;
        }
    }

    ma_engine_listener_set_position(engine_handle_->get(), active_listener_index,
        listener_pos.x, listener_pos.y, listener_pos.z);
    ma_engine_listener_set_direction(engine_handle_->get(), active_listener_index,
        listener_fwd.x, listener_fwd.y, listener_fwd.z);
    ma_engine_listener_set_world_up(engine_handle_->get(), active_listener_index,
        listener_up.x, listener_up.y, listener_up.z);

    // --- Process audio sources ---
    auto view = registry.view<AudioSourceComponent>();
    for (auto entity : view) {
        auto& audio = view.get<AudioSourceComponent>(entity);
        const std::uint32_t key = static_cast<std::uint32_t>(entity);
        auto sound_it = entity_sounds_.find(key);
        SoundHandle* sound_handle = sound_it != entity_sounds_.end() ? sound_it->second.get() : nullptr;
        ma_sound* sound = sound_handle ? sound_handle->get() : nullptr;

        if (!sound && audio.clip && (audio.play_on_awake || audio.is_playing)) {
            auto new_sound = std::make_unique<SoundHandle>();
            const ma_result result = ma_sound_init_from_file(
                engine_handle_->get(),
                audio.clip->GetPath().c_str(),
                0,
                nullptr,
                nullptr,
                &new_sound->value);
            if (result == MA_SUCCESS) {
                new_sound->initialized = true;
                ma_sound_set_looping(&new_sound->value, audio.loop ? MA_TRUE : MA_FALSE);
                ma_sound_set_pitch(&new_sound->value, audio.pitch);
                ma_sound_set_spatialization_enabled(&new_sound->value, audio.spatial_enabled ? MA_TRUE : MA_FALSE);
                ma_sound_set_min_distance(&new_sound->value, audio.min_distance);
                ma_sound_set_max_distance(&new_sound->value, audio.max_distance);
                ma_sound_set_rolloff(&new_sound->value, audio.rolloff);
                // Phase 2: attenuation model
                ma_sound_set_attenuation_model(&new_sound->value, MapAttenuationModel(audio.attenuation_model));
                // Phase 3: occlusion volume
                float effective_volume = audio.volume * sfx_volume_;
                if (audio.spatial_enabled && audio.occlusion_enabled && raycast_func_ && listener_found
                    && registry.all_of<TransformComponent>(entity)) {
                    const auto& src_transform = registry.get<TransformComponent>(entity);
                    const glm::vec3 dir = src_transform.position - listener_pos;
                    const float dist = glm::length(dir);
                    if (dist > 0.001f) {
                        auto ray_result = raycast_func_(listener_pos, dir / dist, dist);
                        if (ray_result.hit && ray_result.distance < dist - 0.01f) {
                            effective_volume *= audio.occlusion_factor;
                        }
                    }
                }
                ma_sound_set_volume(&new_sound->value, effective_volume);
                if (registry.all_of<TransformComponent>(entity)) {
                    const auto& transform = registry.get<TransformComponent>(entity);
                    ma_sound_set_position(&new_sound->value, transform.position.x, transform.position.y, transform.position.z);
                }
                ma_sound_start(&new_sound->value);
                sound = new_sound->get();
                sound_handle = new_sound.get();
                entity_sounds_[key] = std::move(new_sound);
                audio.restart_requested = false;
            }
        }

        if (!sound) {
            audio.runtime_handle = 0;
            audio.is_playing = false;
            audio.restart_requested = false;
            continue;
        }

        ma_sound_set_looping(sound, audio.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_pitch(sound, audio.pitch);
        ma_sound_set_spatialization_enabled(sound, audio.spatial_enabled ? MA_TRUE : MA_FALSE);
        ma_sound_set_min_distance(sound, audio.min_distance);
        ma_sound_set_max_distance(sound, audio.max_distance);
        ma_sound_set_rolloff(sound, audio.rolloff);
        // Phase 2: attenuation model
        ma_sound_set_attenuation_model(sound, MapAttenuationModel(audio.attenuation_model));
        // Phase 3: occlusion volume
        float effective_volume = audio.volume * sfx_volume_;
        if (audio.spatial_enabled && audio.occlusion_enabled && raycast_func_ && listener_found
            && registry.all_of<TransformComponent>(entity)) {
            const auto& src_transform = registry.get<TransformComponent>(entity);
            const glm::vec3 dir = src_transform.position - listener_pos;
            const float dist = glm::length(dir);
            if (dist > 0.001f) {
                auto ray_result = raycast_func_(listener_pos, dir / dist, dist);
                if (ray_result.hit && ray_result.distance < dist - 0.01f) {
                    effective_volume *= audio.occlusion_factor;
                }
            }
        }
        ma_sound_set_volume(sound, effective_volume);
        if (registry.all_of<TransformComponent>(entity)) {
            const auto& transform = registry.get<TransformComponent>(entity);
            ma_sound_set_position(sound, transform.position.x, transform.position.y, transform.position.z);
        }
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
            entity_sounds_.erase(key);
            audio.runtime_handle = 0;
            audio.is_playing = false;
            audio.restart_requested = false;
            continue;
        }

        if (now_playing) {
            audio.runtime_handle = static_cast<unsigned int>(key);
        } else {
            audio.runtime_handle = 0;
        }
        audio.is_playing = now_playing;
    }

    CleanupFinishedSfx();
}

void AudioSystem::Shutdown() {
    StopBgm();
    StopAllSfx();
    entity_sounds_.clear();
    active_sfx_per_clip_.clear();
    sfx_clip_lookup_.clear();
    sfx_last_trigger_ms_.clear();
    resource_manager_handle_.reset();
    engine_handle_.reset();
    g_audio_asset_manager = nullptr;
    asset_manager_ = nullptr;
    is_initialized = false;
}

void AudioSystem::PlaySound(const std::string& filepath, float volume) {
    PlaySfx(filepath, volume, false);
}

void AudioSystem::PlaySfx(const std::string& filepath, float volume, bool loop) {
    if (!is_initialized || !engine_handle_ || !engine_handle_->get() || filepath.empty()) {
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

    auto new_sound = std::make_unique<SoundHandle>();
    const ma_result result = ma_sound_init_from_file(
        engine_handle_->get(),
        filepath.c_str(),
        0,
        nullptr,
        nullptr,
        &new_sound->value);
    if (result != MA_SUCCESS) {
        return;
    }

    new_sound->initialized = true;
    ma_sound_set_looping(&new_sound->value, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&new_sound->value, volume * sfx_volume_);
    ma_sound_start(&new_sound->value);

    const SoundHandle* sound_key = new_sound.get();
    active_sfx_.push_back(std::move(new_sound));
    active_sfx_per_clip_[filepath] += 1;
    sfx_clip_lookup_[sound_key] = filepath;
    sfx_last_trigger_ms_[filepath] = now_ms;
}

bool AudioSystem::PlayBgm(const std::string& filepath, float volume, bool loop) {
    if (!is_initialized || !engine_handle_ || !engine_handle_->get() || filepath.empty()) {
        return false;
    }

    StopBgm();
    auto new_sound = std::make_unique<SoundHandle>();
    const ma_result result = ma_sound_init_from_file(
        engine_handle_->get(),
        filepath.c_str(),
        MA_SOUND_FLAG_STREAM,
        nullptr,
        nullptr,
        &new_sound->value);
    if (result != MA_SUCCESS) {
        return false;
    }

    new_sound->initialized = true;
    ma_sound_set_looping(&new_sound->value, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&new_sound->value, volume * bgm_volume_);
    ma_sound_start(&new_sound->value);
    bgm_sound_ = std::move(new_sound);
    return true;
}

void AudioSystem::PauseBgm() {
    if (!bgm_sound_ || !bgm_sound_->get()) {
        return;
    }
    ma_sound_stop(bgm_sound_->get());
}

void AudioSystem::ResumeBgm() {
    if (!bgm_sound_ || !bgm_sound_->get()) {
        return;
    }
    ma_sound_start(bgm_sound_->get());
}

void AudioSystem::StopBgm() {
    bgm_sound_.reset();
}

void AudioSystem::StopAllSfx() {
    active_sfx_.clear();
    active_sfx_per_clip_.clear();
    sfx_clip_lookup_.clear();
}

void AudioSystem::SetMasterVolume(float volume) {
    master_volume_ = std::clamp(volume, 0.0f, 1.0f);
    if (is_initialized && engine_handle_ && engine_handle_->get()) {
        ma_engine_set_volume(engine_handle_->get(), master_volume_);
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
    if (it == entity_sounds_.end() || !it->second || !it->second->get()) {
        return;
    }
    ma_sound_set_pitch(it->second->get(), pitch);
}

void AudioSystem::SetMaxConcurrentSfxPerClip(std::size_t max_instances) {
    max_concurrent_sfx_per_clip_ = max_instances == 0 ? 1 : max_instances;
}

void AudioSystem::SetSfxTriggerCooldownMs(std::uint32_t cooldown_ms) {
    sfx_trigger_cooldown_ms_ = cooldown_ms;
}

void AudioSystem::SetRaycastFunction(AudioRaycastFunc func) {
    raycast_func_ = std::move(func);
}

void AudioSystem::ApplyAllVolumes() {
    if (bgm_sound_ && bgm_sound_->get()) {
        ma_sound_set_volume(bgm_sound_->get(), bgm_volume_);
    }
    for (auto& sound_ptr : active_sfx_) {
        if (sound_ptr && sound_ptr->get()) {
            ma_sound_set_volume(sound_ptr->get(), sfx_volume_);
        }
    }
    for (auto& pair : entity_sounds_) {
        if (pair.second && pair.second->get()) {
            ma_sound_set_volume(pair.second->get(), sfx_volume_);
        }
    }
}

void AudioSystem::CleanupFinishedSfx() {
    for (auto it = active_sfx_.begin(); it != active_sfx_.end();) {
        if (!(*it) || !(*it)->get()) {
            it = active_sfx_.erase(it);
            continue;
        }

        const SoundHandle* sound_key = it->get();
        ma_sound* sound = (*it)->get();
        if (ma_sound_is_playing(sound) == MA_TRUE) {
            ++it;
            continue;
        }

        auto clip_it = sfx_clip_lookup_.find(sound_key);
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
        it = active_sfx_.erase(it);
    }
}

} // namespace gameplay2d
} // namespace dse
