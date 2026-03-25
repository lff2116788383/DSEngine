#ifndef DSE_AUDIO_SYSTEM_H
#define DSE_AUDIO_SYSTEM_H

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <deque>

namespace dse {
namespace gameplay2d {

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool Initialize();
    void Update(entt::registry& registry, float dt);
    void Shutdown();

    void PlaySound(const std::string& filepath, float volume = 1.0f);
    void PlaySfx(const std::string& filepath, float volume = 1.0f, bool loop = false);
    bool PlayBgm(const std::string& filepath, float volume = 1.0f, bool loop = true);
    void PauseBgm();
    void ResumeBgm();
    void StopBgm();
    void StopAllSfx();
    void SetMasterVolume(float volume);
    void SetBgmVolume(float volume);
    void SetSfxVolume(float volume);
    void SetEntityPitch(std::uint32_t entity, float pitch);
    void SetMaxConcurrentSfxPerClip(std::size_t max_instances);
    void SetSfxTriggerCooldownMs(std::uint32_t cooldown_ms);

private:
    void DestroySound(void* sound_ptr);
    void ApplyAllVolumes();
    void CleanupFinishedSfx();

    void* ma_engine_ptr = nullptr;
    void* bgm_sound_ptr_ = nullptr;
    std::vector<void*> active_sfx_;
    std::unordered_map<std::uint32_t, void*> entity_sounds_;
    std::unordered_map<std::string, std::size_t> active_sfx_per_clip_;
    std::unordered_map<void*, std::string> sfx_clip_lookup_;
    std::unordered_map<std::string, std::uint64_t> sfx_last_trigger_ms_;
    float master_volume_ = 1.0f;
    float bgm_volume_ = 1.0f;
    float sfx_volume_ = 1.0f;
    std::size_t max_concurrent_sfx_per_clip_ = 4;
    std::uint32_t sfx_trigger_cooldown_ms_ = 20;
    bool is_initialized = false;
};

} // namespace gameplay2d
} // namespace dse

#endif
