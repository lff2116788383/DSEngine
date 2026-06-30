/**
 * @file physics_lod.cpp
 * @brief 物理 LOD / 休眠系统实现
 */

#include "engine/physics/physics3d/physics_lod.h"
#include <algorithm>
#include <cmath>

namespace dse {
namespace physics3d {

void PhysicsLODSystem::Init(const PhysicsLODConfig& config) {
    config_ = config;
    initialized_ = true;
}

void PhysicsLODSystem::Shutdown() {
    bodies_.clear();
    initialized_ = false;
}

uint32_t PhysicsLODSystem::RegisterBody(uint32_t entity_id, const glm::vec3& position, float radius) {
    PhysicsLODEntry entry;
    entry.entity_id = entity_id;
    entry.world_position = position;
    entry.bounding_radius = radius;
    entry.current_level = PhysicsLODLevel::Full;
    entry.desired_level = PhysicsLODLevel::Full;
    entry.sim_frequency_divider = 1;
    bodies_[entity_id] = entry;
    return entity_id;
}

void PhysicsLODSystem::UnregisterBody(uint32_t entity_id) {
    bodies_.erase(entity_id);
}

void PhysicsLODSystem::UpdateBodyState(uint32_t entity_id, const glm::vec3& position, float velocity_mag) {
    auto it = bodies_.find(entity_id);
    if (it == bodies_.end()) return;
    it->second.world_position = position;
    it->second.velocity_magnitude = velocity_mag;
}

std::vector<uint32_t> PhysicsLODSystem::Evaluate(const glm::vec3& camera_position, uint32_t frame_number) {
    std::vector<uint32_t> active_bodies;
    active_bodies.reserve(bodies_.size());

    for (auto& [id, entry] : bodies_) {
        float distance = glm::length(camera_position - entry.world_position);
        PhysicsLODLevel desired = EvaluateLevel(distance, entry.velocity_magnitude);

        // Apply hysteresis: harder to move to higher quality
        if (desired < entry.current_level) {
            float threshold = 0.0f;
            switch (desired) {
                case PhysicsLODLevel::Full:
                    threshold = config_.full_distance / config_.hysteresis_factor;
                    break;
                case PhysicsLODLevel::Reduced:
                    threshold = config_.reduced_distance / config_.hysteresis_factor;
                    break;
                case PhysicsLODLevel::Simplified:
                    threshold = config_.simplified_distance / config_.hysteresis_factor;
                    break;
                default: break;
            }
            if (distance > threshold) {
                desired = entry.current_level;
            }
        }

        entry.desired_level = desired;
        entry.current_level = desired;

        // Update state based on level
        switch (desired) {
            case PhysicsLODLevel::Full:
                entry.sim_frequency_divider = 1;
                entry.collider_simplified = false;
                entry.is_sleeping = false;
                break;
            case PhysicsLODLevel::Reduced:
                entry.sim_frequency_divider = config_.reduced_frequency;
                entry.collider_simplified = false;
                entry.is_sleeping = false;
                break;
            case PhysicsLODLevel::Simplified:
                entry.sim_frequency_divider = config_.simplified_frequency;
                entry.collider_simplified = config_.enable_collider_simplification;
                entry.is_sleeping = false;
                break;
            case PhysicsLODLevel::Sleep:
                entry.is_sleeping = true;
                entry.sleep_position = entry.world_position;
                break;
        }

        entry.frame_counter = frame_number;

        // Determine if this body should simulate this frame
        if (!entry.is_sleeping) {
            if (!config_.enable_frequency_reduction || entry.sim_frequency_divider <= 1) {
                active_bodies.push_back(id);
            } else {
                if ((frame_number % entry.sim_frequency_divider) == 0) {
                    active_bodies.push_back(id);
                }
            }
        }
    }

    return active_bodies;
}

void PhysicsLODSystem::WakeBody(uint32_t entity_id) {
    auto it = bodies_.find(entity_id);
    if (it == bodies_.end()) return;
    it->second.is_sleeping = false;
    it->second.current_level = PhysicsLODLevel::Full;
    it->second.sim_frequency_divider = 1;
}

void PhysicsLODSystem::SleepBody(uint32_t entity_id) {
    auto it = bodies_.find(entity_id);
    if (it == bodies_.end()) return;
    it->second.is_sleeping = true;
    it->second.current_level = PhysicsLODLevel::Sleep;
    it->second.sleep_position = it->second.world_position;
}

PhysicsLODLevel PhysicsLODSystem::EvaluateLevel(float distance, float velocity) const {
    // High velocity bodies should never sleep
    if (velocity > config_.wake_velocity_threshold) {
        if (distance <= config_.full_distance) return PhysicsLODLevel::Full;
        if (distance <= config_.reduced_distance) return PhysicsLODLevel::Reduced;
        return PhysicsLODLevel::Simplified; // Don't sleep fast-moving objects
    }

    if (distance <= config_.full_distance) return PhysicsLODLevel::Full;
    if (distance <= config_.reduced_distance) return PhysicsLODLevel::Reduced;
    if (distance <= config_.simplified_distance) return PhysicsLODLevel::Simplified;

    // Only sleep if velocity is low enough
    if (velocity <= config_.sleep_velocity_threshold) {
        return PhysicsLODLevel::Sleep;
    }
    return PhysicsLODLevel::Simplified;
}

PhysicsLODLevel PhysicsLODSystem::GetBodyLevel(uint32_t entity_id) const {
    auto it = bodies_.find(entity_id);
    return (it != bodies_.end()) ? it->second.current_level : PhysicsLODLevel::Full;
}

bool PhysicsLODSystem::IsBodySleeping(uint32_t entity_id) const {
    auto it = bodies_.find(entity_id);
    return (it != bodies_.end()) ? it->second.is_sleeping : false;
}

bool PhysicsLODSystem::ShouldSimulateThisFrame(uint32_t entity_id, uint32_t frame_number) const {
    auto it = bodies_.find(entity_id);
    if (it == bodies_.end()) return false;
    if (it->second.is_sleeping) return false;
    if (it->second.sim_frequency_divider <= 1) return true;
    return (frame_number % it->second.sim_frequency_divider) == 0;
}

uint32_t PhysicsLODSystem::GetRegisteredBodyCount() const {
    return static_cast<uint32_t>(bodies_.size());
}

uint32_t PhysicsLODSystem::GetSleepingBodyCount() const {
    uint32_t count = 0;
    for (const auto& [id, entry] : bodies_) {
        if (entry.is_sleeping) ++count;
    }
    return count;
}

uint32_t PhysicsLODSystem::GetActiveBodyCount() const {
    uint32_t count = 0;
    for (const auto& [id, entry] : bodies_) {
        if (!entry.is_sleeping) ++count;
    }
    return count;
}

PhysicsLODSystem::LevelStats PhysicsLODSystem::GetLevelStats() const {
    LevelStats stats{};
    for (const auto& [id, entry] : bodies_) {
        switch (entry.current_level) {
            case PhysicsLODLevel::Full: ++stats.full; break;
            case PhysicsLODLevel::Reduced: ++stats.reduced; break;
            case PhysicsLODLevel::Simplified: ++stats.simplified; break;
            case PhysicsLODLevel::Sleep: ++stats.sleeping; break;
        }
    }
    return stats;
}

void PhysicsLODSystem::RebaseOrigin(const glm::vec3& offset) {
    for (auto& [id, entry] : bodies_) {
        entry.world_position -= offset;
        entry.sleep_position -= offset;
    }
}

} // namespace physics3d
} // namespace dse
