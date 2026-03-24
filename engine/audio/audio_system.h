#ifndef DSE_PHASE1_AUDIO_SYSTEM_H
#define DSE_PHASE1_AUDIO_SYSTEM_H

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace dse {
namespace phase1 {

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

private:
    void DestroySound(void* sound_ptr);
    void ApplyAllVolumes();
    void CleanupFinishedSfx();

    void* ma_engine_ptr = nullptr;
    void* bgm_sound_ptr_ = nullptr;
    std::vector<void*> active_sfx_;
    std::unordered_map<std::uint32_t, void*> entity_sounds_;
    float master_volume_ = 1.0f;
    float bgm_volume_ = 1.0f;
    float sfx_volume_ = 1.0f;
    bool is_initialized = false;
};

} // namespace phase1
} // namespace dse

#endif // DSE_PHASE1_AUDIO_SYSTEM_H
