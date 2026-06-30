/**
 * @file ai_lod_scheduler.cpp
 * @brief AI LOD 分层调度实现
 */

#include "engine/ai/ai_lod_scheduler.h"
#include <cmath>

namespace dse {
namespace ai {

void AILodScheduler::Init(const AILodConfig& config) {
    config_ = config;
    initialized_ = true;
    global_frame_ = 0;
}

void AILodScheduler::Register(uint32_t entity_id, float importance) {
    AILodState state;
    state.entity_id = entity_id;
    state.importance = importance;
    state.current_level = AILodLevel::Near;
    state.frame_counter = 0;
    state.force_active = false;
    states_[entity_id] = state;
}

void AILodScheduler::Unregister(uint32_t entity_id) {
    states_.erase(entity_id);
}

void AILodScheduler::SetForceActive(uint32_t entity_id, bool active) {
    auto it = states_.find(entity_id);
    if (it != states_.end()) {
        it->second.force_active = active;
    }
}

AILodUpdateResult AILodScheduler::Update(const glm::vec3& viewer_pos,
                                          const std::unordered_map<uint32_t, glm::vec3>& entity_positions) {
    if (!initialized_) return {};

    ++global_frame_;

    AILodUpdateResult result;

    for (auto& [id, state] : states_) {
        // 计算距离
        float distance = 0.0f;
        auto pos_it = entity_positions.find(id);
        if (pos_it != entity_positions.end()) {
            distance = glm::length(pos_it->second - viewer_pos);
        }

        // 计算目标层级
        AILodLevel target = ComputeLevel(distance, state.current_level, state.importance);

        // 强制激活覆盖
        if (state.force_active) {
            target = AILodLevel::Near;
        }

        state.current_level = target;

        // 统计
        switch (target) {
            case AILodLevel::Near: ++result.near_count; break;
            case AILodLevel::Medium: ++result.medium_count; break;
            case AILodLevel::Far: ++result.far_count; break;
            case AILodLevel::Dormant: ++result.dormant_count; break;
        }

        // 更新帧计数器，判断本帧是否 tick
        ++state.frame_counter;
        bool should_tick = false;
        switch (target) {
            case AILodLevel::Near:
                should_tick = true;
                state.frame_counter = 0;
                break;
            case AILodLevel::Medium:
                if (state.frame_counter >= config_.medium_skip_frames) {
                    should_tick = true;
                    state.frame_counter = 0;
                }
                break;
            case AILodLevel::Far:
                if (state.frame_counter >= config_.far_skip_frames) {
                    should_tick = true;
                    state.frame_counter = 0;
                }
                break;
            case AILodLevel::Dormant:
                should_tick = false;
                break;
        }

        if (should_tick) ++result.active_this_frame;
    }

    return result;
}

bool AILodScheduler::ShouldTick(uint32_t entity_id) const {
    auto it = states_.find(entity_id);
    if (it == states_.end()) return false;

    const auto& state = it->second;
    if (state.force_active) return true;

    switch (state.current_level) {
        case AILodLevel::Near: return true;
        case AILodLevel::Medium: return state.frame_counter == 0;
        case AILodLevel::Far: return state.frame_counter == 0;
        case AILodLevel::Dormant: return false;
    }
    return false;
}

AILodLevel AILodScheduler::GetLevel(uint32_t entity_id) const {
    auto it = states_.find(entity_id);
    if (it != states_.end()) return it->second.current_level;
    return AILodLevel::Dormant;
}

AILodLevel AILodScheduler::ComputeLevel(float distance, AILodLevel current, float importance) const {
    // 高重要性实体永远 Near
    if (importance > 10.0f) return AILodLevel::Near;

    // 缩放距离（重要性高则有效距离更近）
    float effective_dist = distance / std::max(importance, 0.1f);

    // 应用 hysteresis：从高频→低频需要更远，从低频→高频需要更近
    float near_threshold = config_.near_distance;
    float medium_threshold = config_.medium_distance;
    float far_threshold = config_.far_distance;

    // 滞后调整
    if (current == AILodLevel::Near) {
        near_threshold *= (1.0f + config_.hysteresis);
    } else if (current == AILodLevel::Medium) {
        near_threshold *= (1.0f - config_.hysteresis);
        medium_threshold *= (1.0f + config_.hysteresis);
    } else if (current == AILodLevel::Far) {
        medium_threshold *= (1.0f - config_.hysteresis);
        far_threshold *= (1.0f + config_.hysteresis);
    }

    if (effective_dist <= near_threshold) return AILodLevel::Near;
    if (effective_dist <= medium_threshold) return AILodLevel::Medium;
    if (effective_dist <= far_threshold) return AILodLevel::Far;
    return AILodLevel::Dormant;
}

void AILodScheduler::Shutdown() {
    states_.clear();
    initialized_ = false;
}

} // namespace ai
} // namespace dse
