/**
 * @file cutscene_player.h
 * @brief 过场播放器 —— Sequence 容器 + 触发器系统
 *
 * CutsceneSequence 包含多条轨道（Camera/Property/Event/Audio），
 * CutscenePlayer 管理多个 Sequence 的播放与触发调度。
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "engine/cutscene/cutscene_track.h"
#include "engine/core/dse_export.h"

namespace dse {
namespace cutscene {

/// 过场序列（Sequence）—— 一段完整过场的轨道集合
class DSE_EXPORT CutsceneSequence {
public:
    explicit CutsceneSequence(const std::string& name = "", float duration = 0.0f)
        : name_(name), duration_(duration) {}
    ~CutsceneSequence() = default;

    /// 添加轨道
    void AddTrack(std::shared_ptr<CutsceneTrack> track);

    /// 获取时长
    float GetDuration() const { return duration_; }
    void SetDuration(float d) { duration_ = d; }

    /// 获取名称
    const std::string& GetName() const { return name_; }

    /// 评估所有轨道
    void Evaluate(float time);

    /// 重置所有轨道
    void Reset();

    /// 获取轨道列表
    const std::vector<std::shared_ptr<CutsceneTrack>>& GetTracks() const { return tracks_; }

private:
    std::string name_;
    float duration_ = 0.0f;
    std::vector<std::shared_ptr<CutsceneTrack>> tracks_;
};

/// 播放状态
enum class PlayState : uint8_t {
    Stopped = 0,
    Playing,
    Paused,
};

/// 触发器条件
struct CutsceneTrigger {
    std::string sequence_name;                ///< 关联的 Sequence 名称
    std::function<bool()> condition;          ///< 触发条件（返回 true 时启动 Sequence）
    bool auto_remove = true;                  ///< 触发后是否移除
    bool fired = false;                       ///< 是否已触发
};

/// 完成回调
using CutsceneFinishCallback = std::function<void(const std::string& sequence_name)>;

/// 过场播放器
class DSE_EXPORT CutscenePlayer {
public:
    CutscenePlayer() = default;
    ~CutscenePlayer() = default;

    /// 注册 Sequence
    void AddSequence(std::shared_ptr<CutsceneSequence> seq);

    /// 移除 Sequence
    void RemoveSequence(const std::string& name);

    /// 获取 Sequence
    std::shared_ptr<CutsceneSequence> GetSequence(const std::string& name) const;

    /// 播放指定 Sequence
    void Play(const std::string& name);

    /// 暂停
    void Pause();

    /// 恢复
    void Resume();

    /// 停止
    void Stop();

    /// 跳转到时间点
    void Seek(float time);

    /// 获取当前播放时间
    float GetCurrentTime() const { return current_time_; }

    /// 获取播放状态
    PlayState GetState() const { return state_; }

    /// 获取当前正在播放的 Sequence 名称
    const std::string& GetCurrentSequenceName() const { return current_seq_name_; }

    /// 设置播放速率
    void SetPlayRate(float rate) { play_rate_ = rate; }
    float GetPlayRate() const { return play_rate_; }

    /// 设置完成回调
    void SetFinishCallback(CutsceneFinishCallback cb) { finish_callback_ = std::move(cb); }

    /// 添加触发器
    void AddTrigger(const CutsceneTrigger& trigger);

    /// 清空所有触发器
    void ClearTriggers();

    /// 每帧更新
    void Update(float dt);

private:
    void CheckTriggers();

    std::unordered_map<std::string, std::shared_ptr<CutsceneSequence>> sequences_;
    std::vector<CutsceneTrigger> triggers_;
    std::string current_seq_name_;
    float current_time_ = 0.0f;
    float play_rate_ = 1.0f;
    PlayState state_ = PlayState::Stopped;
    CutsceneFinishCallback finish_callback_;
};

} // namespace cutscene
} // namespace dse
