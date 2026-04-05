/**
 * @file audio_system.h
 * @brief 音频系统，负责管理和播放 BGM、SFX，以及 ECS 实体的音频组件。
 */

#ifndef DSE_AUDIO_SYSTEM_H
#define DSE_AUDIO_SYSTEM_H

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <deque>

class AssetManager;

namespace dse {
namespace gameplay2d {

/**
 * @class AudioSystem
 * @brief 全局音频管理系统，封装 miniaudio，提供基础的音频播放控制。
 * 
 * @warning 该类的初始化和销毁必须在引擎生命周期内正确调用。
 */
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    /**
     * @brief 初始化音频引擎
     * @param asset_manager 已注入的资源管理器，不能为空
     * @return 成功返回 true，否则返回 false
     */
    bool Initialize(AssetManager* asset_manager);

    /**
     * @brief 更新 ECS 实体的音频组件状态
     * @param registry ECS 注册表
     * @param dt 增量时间
     */
    void Update(entt::registry& registry, float dt);

    /**
     * @brief 关闭音频引擎并释放所有资源
     */
    void Shutdown();

    /**
     * @brief 播放单次音效
     * @param filepath 音频文件路径
     * @param volume 音量 (0.0 - 1.0)
     * 
     * @example
     * AudioSystem::Instance().PlaySound("data/sound/hit.wav", 0.8f);
     */
    void PlaySound(const std::string& filepath, float volume = 1.0f);
    /**
     * @brief 播放指定的音效 (SFX)
     * @param filepath 音效文件路径
     * @param volume 音量大小 (0.0 - 1.0)，将乘以全局 sfx_volume_
     * @param loop 是否循环播放
     * @example
     * // audio_system.PlaySfx("assets/jump.wav", 0.8f, false);
     */
    void PlaySfx(const std::string& filepath, float volume = 1.0f, bool loop = false);

    /**
     * @brief 播放背景音乐 (BGM)，会替换当前正在播放的 BGM
     * @param filepath 音乐文件路径
     * @param volume 音量大小 (0.0 - 1.0)，将乘以全局 bgm_volume_
     * @param loop 是否循环播放，默认 true
     * @return 成功播放返回 true
     * @example
     * // audio_system.PlayBgm("assets/bgm_level1.ogg");
     */
    bool PlayBgm(const std::string& filepath, float volume = 1.0f, bool loop = true);

    /**
     * @brief 暂停当前播放的背景音乐
     */
    void PauseBgm();

    /**
     * @brief 恢复播放被暂停的背景音乐
     */
    void ResumeBgm();

    /**
     * @brief 停止并销毁当前的背景音乐
     */
    void StopBgm();

    /**
     * @brief 停止所有正在播放的短音效
     */
    void StopAllSfx();

    /**
     * @brief 设置主音量（影响所有声音）
     * @param volume 主音量大小 (0.0 - 1.0)
     */
    void SetMasterVolume(float volume);

    /**
     * @brief 设置背景音乐的独立音量
     * @param volume BGM音量大小 (0.0 - 1.0)
     */
    void SetBgmVolume(float volume);

    /**
     * @brief 设置短音效的独立音量
     * @param volume SFX音量大小 (0.0 - 1.0)
     */
    void SetSfxVolume(float volume);

    /**
     * @brief 设置挂载在 ECS 实体上的 AudioSource 的音高
     * @param entity 目标实体 ID
     * @param pitch 音高倍率 (1.0 为正常)
     */
    void SetEntityPitch(std::uint32_t entity, float pitch);

    /**
     * @brief 限制同一个音效文件同时播放的最大实例数，防止声音叠加过载
     * @param max_instances 允许的最大并发数
     */
    void SetMaxConcurrentSfxPerClip(std::size_t max_instances);

    /**
     * @brief 设置同名音效连续触发的冷却时间，防止频繁触发导致爆音
     * @param cooldown_ms 冷却时间（毫秒）
     */
    void SetSfxTriggerCooldownMs(std::uint32_t cooldown_ms);

private:
    /**
     * @brief 内部方法：安全销毁 miniaudio 的 ma_sound 实例
     * @param sound_ptr 转换为 void* 的 ma_sound 指针
     */
    void DestroySound(void* sound_ptr);

    /**
     * @brief 内部方法：应用当前所有的音量设置到对应的播放实例上
     */
    void ApplyAllVolumes();

    /**
     * @brief 内部方法：清理已经播放完毕的音效实例，释放内存
     */
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
    void* ma_resource_manager_ptr = nullptr;
    AssetManager* asset_manager_ = nullptr;
};

} // namespace gameplay2d
} // namespace dse

#endif
