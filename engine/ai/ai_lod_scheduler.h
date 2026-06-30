/**
 * @file ai_lod_scheduler.h
 * @brief AI LOD 分层调度系统
 *
 * 根据实体到观察者的距离自动调整 AI 更新频率：
 * - Near：每帧完整 tick（全精度 AI + 动画 + 物理）
 * - Medium：每 2-4 帧 tick（简化决策、降频动画）
 * - Far：每 8-16 帧 tick（休眠态，仅状态机推进）
 * - Dormant：超出范围完全休眠，不消耗 CPU
 *
 * 支持重要性权重（boss/quest NPC 永远 Near），可配置各层距离阈值。
 */

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace ai {

/// AI LOD 层级
enum class AILodLevel : uint8_t {
    Near = 0,       ///< 全频率（每帧）
    Medium = 1,     ///< 降频（每 N 帧）
    Far = 2,        ///< 低频（每 M 帧）
    Dormant = 3,    ///< 完全休眠
};

/// AI LOD 配置
struct AILodConfig {
    float near_distance = 30.0f;        ///< Near 层最大距离
    float medium_distance = 80.0f;      ///< Medium 层最大距离
    float far_distance = 200.0f;        ///< Far 层最大距离（超出则 Dormant）
    uint8_t medium_skip_frames = 2;     ///< Medium 层跳帧数（每 N+1 帧更新一次）
    uint8_t far_skip_frames = 8;        ///< Far 层跳帧数
    float hysteresis = 0.1f;            ///< 层级切换滞后比例（防抖）
};

/// 单个 AI 实体的 LOD 状态
struct AILodState {
    uint32_t entity_id = 0;
    AILodLevel current_level = AILodLevel::Near;
    uint8_t frame_counter = 0;          ///< 当前帧计数器
    float importance = 1.0f;            ///< 重要性权重（>1 = 永远 Near）
    bool force_active = false;          ///< 强制激活（剧情/Boss）
};

/// AI LOD 每帧更新结果
struct AILodUpdateResult {
    uint32_t near_count = 0;
    uint32_t medium_count = 0;
    uint32_t far_count = 0;
    uint32_t dormant_count = 0;
    uint32_t active_this_frame = 0;     ///< 本帧实际需要 tick 的实体数
};

/// AI LOD 调度系统
class DSE_EXPORT AILodScheduler {
public:
    AILodScheduler() = default;
    ~AILodScheduler() = default;

    /// 初始化
    void Init(const AILodConfig& config);

    /// 注册 AI 实体
    void Register(uint32_t entity_id, float importance = 1.0f);

    /// 注销 AI 实体
    void Unregister(uint32_t entity_id);

    /// 设置实体强制激活（Boss/剧情）
    void SetForceActive(uint32_t entity_id, bool active);

    /// 每帧更新：计算各实体 LOD 层级
    /// @param viewer_pos 观察者位置
    /// @param entity_positions 各实体位置（entity_id → world pos）
    AILodUpdateResult Update(const glm::vec3& viewer_pos,
                             const std::unordered_map<uint32_t, glm::vec3>& entity_positions);

    /// 查询某实体本帧是否应该 tick
    bool ShouldTick(uint32_t entity_id) const;

    /// 获取某实体当前 LOD 层级
    AILodLevel GetLevel(uint32_t entity_id) const;

    /// 获取配置
    const AILodConfig& GetConfig() const { return config_; }

    /// 获取已注册实体数
    size_t RegisteredCount() const { return states_.size(); }

    void Shutdown();

private:
    AILodLevel ComputeLevel(float distance, AILodLevel current, float importance) const;

    AILodConfig config_;
    std::unordered_map<uint32_t, AILodState> states_;
    uint32_t global_frame_ = 0;
    bool initialized_ = false;
};

} // namespace ai
} // namespace dse
