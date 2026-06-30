/**
 * @file audio_lod.h
 * @brief 大世界音频 LOD 系统
 *
 * 基于距离的音频优化：
 * - 远距离音源自动降低采样率/位深
 * - 超远距离音源完全静音（虚拟化）
 * - 基于优先级的音源数量上限（同时发声数限制）
 * - 音频数据流式加载（远处不加载完整音频）
 * - 距离衰减模型（线性/对数/自定义曲线）
 *
 * 层级划分：
 * - Full:       完整采样率 + 完整音频数据
 * - Reduced:    降采样（22050Hz）+ 单声道降混
 * - Virtual:    仅更新位置/优先级，不输出音频
 * - Culled:     完全忽略，不做任何计算
 */

#ifndef DSE_AUDIO_LOD_H
#define DSE_AUDIO_LOD_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace dse {
namespace audio {

/// 音频 LOD 层级
enum class AudioLODLevel : uint8_t {
    Full = 0,       ///< 完整质量
    Reduced,        ///< 降采样 + 单声道
    Virtual,        ///< 虚拟化（不发声但保持状态）
    Culled,         ///< 完全剔除
};

/// 距离衰减模型
enum class AttenuationModel : uint8_t {
    Linear = 0,     ///< 线性衰减
    Logarithmic,    ///< 对数衰减（更自然）
    InverseSquare,  ///< 平方反比衰减
    Custom,         ///< 自定义曲线
};

/// 单个音频源的 LOD 状态
struct AudioSourceLOD {
    uint32_t source_id = 0;
    std::string asset_path;
    glm::vec3 world_position = glm::vec3(0.0f);
    float max_distance = 100.0f;        ///< 最大可听距离
    float reference_distance = 1.0f;    ///< 参考距离（衰减开始）
    float base_volume = 1.0f;           ///< 基础音量 [0,1]
    float priority = 0.0f;              ///< 优先级（越高越不容易被虚拟化）
    bool looping = false;

    AudioLODLevel current_level = AudioLODLevel::Full;
    float current_volume = 1.0f;        ///< 经过距离衰减后的有效音量
    float current_distance = 0.0f;
    AttenuationModel attenuation = AttenuationModel::Logarithmic;

    bool is_streaming = false;          ///< 是否使用流式加载
    bool data_loaded = false;           ///< 音频数据是否已加载
};

/// 音频 LOD 配置
struct AudioLODConfig {
    float full_distance = 30.0f;        ///< Full 层级最大距离
    float reduced_distance = 80.0f;     ///< Reduced 层级最大距离
    float virtual_distance = 200.0f;    ///< Virtual 层级最大距离
    // 超出 virtual_distance → Culled

    uint32_t max_active_sources = 32;   ///< 最大同时活跃（发声）源数
    uint32_t max_full_sources = 16;     ///< 最大 Full 质量源数
    float min_audible_volume = 0.01f;   ///< 低于此音量视为静音

    uint32_t reduced_sample_rate = 22050;  ///< Reduced 模式采样率
    bool force_mono_reduced = true;     ///< Reduced 模式是否强制单声道

    float hysteresis_factor = 1.15f;    ///< 层级切换滞回系数
    float update_interval = 0.1f;       ///< LOD 评估间隔（秒）
};

/**
 * @class AudioLODSystem
 * @brief 管理音频源的 LOD 状态和距离衰减
 */
class AudioLODSystem {
public:
    AudioLODSystem() = default;
    ~AudioLODSystem() = default;

    void Init(const AudioLODConfig& config = {});
    void Shutdown();

    /// 注册音频源
    uint32_t RegisterSource(const std::string& asset_path, const glm::vec3& position,
                            float max_distance = 100.0f, float priority = 0.0f);

    /// 移除音频源
    void UnregisterSource(uint32_t source_id);

    /// 更新音源位置
    void UpdateSourcePosition(uint32_t source_id, const glm::vec3& position);

    /// 更新音源属性
    void SetSourceVolume(uint32_t source_id, float volume);
    void SetSourcePriority(uint32_t source_id, float priority);
    void SetSourceAttenuation(uint32_t source_id, AttenuationModel model);
    void SetSourceMaxDistance(uint32_t source_id, float distance);

    /// 每帧 Tick：评估所有音源 LOD + 距离衰减
    void Tick(const glm::vec3& listener_position, const glm::vec3& listener_forward, float delta_time);

    // ========== 查询 ==========

    AudioLODLevel GetSourceLevel(uint32_t source_id) const;
    float GetSourceEffectiveVolume(uint32_t source_id) const;
    bool IsSourceAudible(uint32_t source_id) const;
    uint32_t GetRegisteredSourceCount() const;
    uint32_t GetActiveSourceCount() const;   ///< Full + Reduced
    uint32_t GetVirtualSourceCount() const;
    uint32_t GetCulledSourceCount() const;

    /// 统计
    struct LODStats {
        uint32_t full = 0;
        uint32_t reduced = 0;
        uint32_t virtual_count = 0;
        uint32_t culled = 0;
    };
    LODStats GetStats() const;

    /// Floating Origin 偏移
    void RebaseOrigin(const glm::vec3& offset);

    const AudioLODConfig& GetConfig() const { return config_; }

private:
    float ComputeAttenuation(const AudioSourceLOD& source, float distance) const;
    AudioLODLevel EvaluateLevel(float distance, float effective_volume) const;
    void EnforceActiveLimits();

    AudioLODConfig config_;
    std::unordered_map<uint32_t, AudioSourceLOD> sources_;
    uint32_t next_source_id_ = 1;
    float time_since_update_ = 0.0f;
    glm::vec3 last_listener_pos_ = glm::vec3(0.0f);
    bool initialized_ = false;
};

} // namespace audio
} // namespace dse

#endif // DSE_AUDIO_LOD_H
