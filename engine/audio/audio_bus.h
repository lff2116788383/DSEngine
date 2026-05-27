/**
 * @file audio_bus.h
 * @brief 混音总线 + DSP 效果链系统
 *
 * 提供层级混音总线路由和每条总线上的 DSP 效果节点管理。
 * 基于 miniaudio node graph 实现，零额外依赖。
 */

#ifndef DSE_AUDIO_BUS_H
#define DSE_AUDIO_BUS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

struct ma_engine;

namespace dse {
namespace gameplay2d {

/// DSP 效果类型枚举
enum class DspEffectType : uint8_t {
    LowPass = 0,
    HighPass,
    BandPass,
    Delay,
    Reverb,
    Count
};

/// DSP 效果参数
struct DspEffectParams {
    DspEffectType type = DspEffectType::LowPass;
    float cutoff_hz = 1000.0f;       ///< 截止频率（LowPass/HighPass/BandPass）
    float q = 0.707f;                ///< 品质因数 Q
    float delay_time_ms = 250.0f;    ///< 延迟时间（Delay / Reverb 前反射）
    float feedback = 0.3f;           ///< 延迟反馈量（Delay / Reverb 衰减）
    float wet_mix = 0.5f;            ///< 湿信号混合比
    float room_size = 0.5f;          ///< Reverb 房间大小 [0,1]（映射到延迟长度）
    float damping = 0.5f;            ///< Reverb 高频衰减 [0,1]
    bool enabled = true;
};

/// 运行时 DSP 节点句柄（内部使用，持有 heap-allocated miniaudio 节点）
struct DspNodeHandle {
    DspEffectType type = DspEffectType::LowPass;
    void* node_ptr = nullptr;
};

/// 单条混音总线
struct AudioBus {
    std::string name;                 ///< 总线名称（如 "master", "music", "sfx", "voice"）
    float volume = 1.0f;             ///< 总线音量 [0,1]
    bool muted = false;
    std::string parent_name;          ///< 父总线名称（空 = 挂载到引擎端点）
    std::vector<DspEffectParams> effects; ///< 效果链（按插入顺序执行）

    // 运行时句柄（内部使用，实际为 ma_sound*）
    void* group_handle = nullptr;
    std::vector<DspNodeHandle> active_nodes; ///< 运行时 DSP 节点实例
};

/**
 * @class AudioBusManager
 * @brief 管理所有混音总线及其 DSP 效果链
 *
 * 默认预创建三条总线：master → (music, sfx, voice)
 */
class AudioBusManager {
public:
    AudioBusManager();
    ~AudioBusManager();

    /// 初始化总线系统（必须在 AudioSystem::Initialize 之后调用）
    bool Initialize(ma_engine* engine);

    /// 关闭并释放所有总线资源
    void Shutdown();

    /// 创建自定义总线（parent 为空则挂到 master）
    bool CreateBus(const std::string& name, const std::string& parent = "master", float volume = 1.0f);

    /// 删除自定义总线（不允许删除 master）
    bool RemoveBus(const std::string& name);

    /// 获取总线（返回 nullptr 表示不存在）
    AudioBus* GetBus(const std::string& name);
    const AudioBus* GetBus(const std::string& name) const;

    /// 设置总线音量
    void SetBusVolume(const std::string& name, float volume);

    /// 设置总线静音
    void SetBusMuted(const std::string& name, bool muted);

    /// 在指定总线末尾添加 DSP 效果
    bool AddEffect(const std::string& bus_name, const DspEffectParams& params);

    /// 移除指定总线上第 index 个效果（0-based）
    bool RemoveEffect(const std::string& bus_name, size_t index);

    /// 修改指定总线上第 index 个效果参数
    bool SetEffectParams(const std::string& bus_name, size_t index, const DspEffectParams& params);

    /// 获取总线上效果数量
    size_t GetEffectCount(const std::string& bus_name) const;

    /// 获取 miniaudio sound group 句柄（实际为 ma_sound*，用于路由）
    void* GetGroupHandle(const std::string& bus_name);

    /// 获取所有总线名称
    std::vector<std::string> GetBusNames() const;

    /// 保存当前所有总线状态为快照
    bool SaveSnapshot(const std::string& name);

    /// 加载快照（立即应用）
    bool LoadSnapshot(const std::string& name);

    /// 获取所有快照名称
    std::vector<std::string> GetSnapshotNames() const;

private:
    void ApplyBusVolume(AudioBus& bus);
    void RebuildEffectChain(AudioBus& bus);

    ma_engine* engine_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<AudioBus>> buses_;
    bool initialized_ = false;

    /// 音频快照存储
    struct BusSnapshot {
        float volume = 1.0f;
        bool muted = false;
        std::vector<DspEffectParams> effects;
    };
    struct AudioSnapshot {
        std::unordered_map<std::string, BusSnapshot> buses;
    };
    std::unordered_map<std::string, AudioSnapshot> snapshots_;
};

} // namespace gameplay2d
} // namespace dse

#endif // DSE_AUDIO_BUS_H
