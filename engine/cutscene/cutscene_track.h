/**
 * @file cutscene_track.h
 * @brief 过场轨道系统 —— Director Track 抽象与具体轨道类型
 *
 * 轨道类型：
 * - CameraTrack：摄像机移动/旋转关键帧
 * - PropertyTrack：任意属性浮点动画
 * - EventTrack：离散事件触发（回调/Lua 函数）
 * - AudioTrack：音频播放控制
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace cutscene {

/// 关键帧插值方式
enum class InterpMode : uint8_t {
    Linear = 0,
    Step,
    CubicBezier,
};

/// 通用关键帧模板
template <typename T>
struct Keyframe {
    float time = 0.0f;           ///< 时间点（秒）
    T value{};                   ///< 值
    InterpMode interp = InterpMode::Linear;
};

/// 轨道类型枚举
enum class TrackType : uint8_t {
    Camera = 0,
    Property,
    Event,
    Audio,
};

/// 轨道基类
class DSE_EXPORT CutsceneTrack {
public:
    explicit CutsceneTrack(const std::string& name = "", TrackType type = TrackType::Property)
        : name_(name), type_(type) {}
    virtual ~CutsceneTrack() = default;

    /// 每帧评估
    virtual void Evaluate(float time) = 0;

    /// 重置状态
    virtual void Reset() {}

    const std::string& GetName() const { return name_; }
    TrackType GetType() const { return type_; }

protected:
    std::string name_;
    TrackType type_;
};

// ============================================================
// Camera Track
// ============================================================

struct CameraKeyframe {
    float time = 0.0f;
    glm::vec3 position{0.0f};
    glm::vec3 look_at{0.0f, 0.0f, -1.0f};
    float fov = 60.0f;
    InterpMode interp = InterpMode::Linear;
};

/// 摄像机轨道回调（传入插值后的 position, look_at, fov）
using CameraApplyFunc = std::function<void(const glm::vec3& pos, const glm::vec3& look_at, float fov)>;

class DSE_EXPORT CameraTrack : public CutsceneTrack {
public:
    explicit CameraTrack(const std::string& name = "CameraTrack")
        : CutsceneTrack(name, TrackType::Camera) {}

    void AddKeyframe(const CameraKeyframe& kf) { keyframes_.push_back(kf); }
    void SetApplyCallback(CameraApplyFunc func) { apply_func_ = std::move(func); }
    void Evaluate(float time) override;
    void Reset() override {}

    const std::vector<CameraKeyframe>& GetKeyframes() const { return keyframes_; }

private:
    std::vector<CameraKeyframe> keyframes_;
    CameraApplyFunc apply_func_;
};

// ============================================================
// Property Track (float)
// ============================================================

using PropertyApplyFunc = std::function<void(float value)>;

class DSE_EXPORT PropertyTrack : public CutsceneTrack {
public:
    explicit PropertyTrack(const std::string& name = "PropertyTrack")
        : CutsceneTrack(name, TrackType::Property) {}

    void AddKeyframe(float time, float value, InterpMode interp = InterpMode::Linear);
    void SetApplyCallback(PropertyApplyFunc func) { apply_func_ = std::move(func); }
    void Evaluate(float time) override;

    const std::vector<Keyframe<float>>& GetKeyframes() const { return keyframes_; }

private:
    std::vector<Keyframe<float>> keyframes_;
    PropertyApplyFunc apply_func_;
};

// ============================================================
// Event Track
// ============================================================

struct CutsceneEvent {
    float time = 0.0f;
    std::string event_name;
    std::string payload;         ///< 可选参数
};

using EventFireFunc = std::function<void(const std::string& name, const std::string& payload)>;

class DSE_EXPORT EventTrack : public CutsceneTrack {
public:
    explicit EventTrack(const std::string& name = "EventTrack")
        : CutsceneTrack(name, TrackType::Event) {}

    void AddEvent(float time, const std::string& event_name, const std::string& payload = "");
    void SetFireCallback(EventFireFunc func) { fire_func_ = std::move(func); }
    void Evaluate(float time) override;
    void Reset() override;

    const std::vector<CutsceneEvent>& GetEvents() const { return events_; }

private:
    std::vector<CutsceneEvent> events_;
    EventFireFunc fire_func_;
    float last_time_ = -1.0f;
};

// ============================================================
// Audio Track
// ============================================================

struct AudioCue {
    float time = 0.0f;
    std::string audio_path;
    float volume = 1.0f;
    bool loop = false;
};

using AudioPlayFunc = std::function<void(const std::string& path, float volume, bool loop)>;

class DSE_EXPORT AudioTrack : public CutsceneTrack {
public:
    explicit AudioTrack(const std::string& name = "AudioTrack")
        : CutsceneTrack(name, TrackType::Audio) {}

    void AddCue(float time, const std::string& path, float volume = 1.0f, bool loop = false);
    void SetPlayCallback(AudioPlayFunc func) { play_func_ = std::move(func); }
    void Evaluate(float time) override;
    void Reset() override;

    const std::vector<AudioCue>& GetCues() const { return cues_; }

private:
    std::vector<AudioCue> cues_;
    AudioPlayFunc play_func_;
    float last_time_ = -1.0f;
};

} // namespace cutscene
} // namespace dse
