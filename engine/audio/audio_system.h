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
#include <memory>
#include <functional>
#include <random>
#include <glm/glm.hpp>
#include "audio_bus.h"

class AssetManager;

namespace dse {
namespace gameplay2d {

/**
 * @struct AudioRaycastResult
 * @brief 音频遮挡射线检测结果
 */
struct AudioRaycastResult {
    bool hit = false;
    float distance = 0.0f;
};

/// 射线检测回调类型：(origin, direction, max_distance) -> result
using AudioRaycastFunc = std::function<AudioRaycastResult(const glm::vec3&, const glm::vec3&, float)>;

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
     * @brief 播放随机化音效（pitch 随机抖动）
     * @param filepath 音效文件路径
     * @param volume 音量
     * @param pitch_min 最小 pitch 倍率
     * @param pitch_max 最大 pitch 倍率
     */
    void PlaySfxRandomized(const std::string& filepath, float volume = 1.0f,
                           float pitch_min = 0.9f, float pitch_max = 1.1f);

    /**
     * @brief 预加载音频文件到内存缓存（避免首次播放延迟）
     * @param filepath 音频文件路径
     * @return 成功返回 true
     */
    bool PreloadAudio(const std::string& filepath);

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
     * @brief 带淡入淡出的 BGM 切换
     * @param filepath 新 BGM 文件路径
     * @param fade_sec 淡入淡出时长（秒）
     * @param volume 目标音量
     * @param loop 是否循环
     * @return 成功返回 true
     */
    bool CrossfadeBgm(const std::string& filepath, float fade_sec = 1.0f,
                      float volume = 1.0f, bool loop = true);

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

    /**
     * @brief 注入射线检测回调用于音频遮挡检测
     * @param func 射线检测函数（置空以禁用遮挡）
     */
    void SetRaycastFunction(AudioRaycastFunc func);

    /// 获取混音总线管理器（DSP 效果链 + 总线路由）
    AudioBusManager& GetBusManager() { return bus_manager_; }
    const AudioBusManager& GetBusManager() const { return bus_manager_; }

private:
    struct EngineHandle;
    struct ResourceManagerHandle;
    struct SoundHandle;

    /**
     * @brief 内部方法：应用当前所有的音量设置到对应的播放实例上
     */
    void ApplyAllVolumes();

    /**
     * @brief 内部方法：清理已经播放完毕的音效实例，释放内存
     */
    void CleanupFinishedSfx();

    std::unique_ptr<EngineHandle> engine_handle_;
    std::unique_ptr<ResourceManagerHandle> resource_manager_handle_;
    std::unique_ptr<SoundHandle> bgm_sound_;
    std::vector<std::unique_ptr<SoundHandle>> active_sfx_;
    std::unordered_map<std::uint32_t, std::unique_ptr<SoundHandle>> entity_sounds_;
    std::unordered_map<std::string, std::size_t> active_sfx_per_clip_;
    std::unordered_map<const SoundHandle*, std::string> sfx_clip_lookup_;
    std::unordered_map<std::string, std::uint64_t> sfx_last_trigger_ms_;
    float master_volume_ = 1.0f;
    float bgm_volume_ = 1.0f;
    float sfx_volume_ = 1.0f;
    
    std::size_t max_concurrent_sfx_per_clip_ = 4;
    std::uint32_t sfx_trigger_cooldown_ms_ = 20;
    bool is_initialized = false;
    AssetManager* asset_manager_ = nullptr;
    AudioRaycastFunc raycast_func_;
    AudioBusManager bus_manager_;

    // Crossfade 状态
    std::unique_ptr<SoundHandle> prev_bgm_sound_;
    float crossfade_elapsed_ = 0.0f;
    float crossfade_duration_ = 0.0f;
    float crossfade_target_volume_ = 1.0f;
    bool crossfading_ = false;

    // 音频随机化
    std::mt19937 rng_{std::random_device{}()};

    // 预加载缓存
    std::unordered_map<std::string, std::vector<uint8_t>> preload_cache_;
};

} // namespace gameplay2d
} // namespace dse

#endif
