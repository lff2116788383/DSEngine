/**
 * @file mesh_streaming.cpp
 * @brief 网格 LOD 流式加载系统实现
 */

#include "engine/render/mesh_streaming.h"
#include <algorithm>
#include <cmath>

namespace dse {
namespace render {

void MeshStreamingSystem::Init(const MeshStreamingConfig& config) {
    config_ = config;
    initialized_ = true;
}

void MeshStreamingSystem::Shutdown() {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    meshes_.clear();
    load_queue_.clear();
    initialized_ = false;
}

uint32_t MeshStreamingSystem::RegisterMesh(const std::string& name, const glm::vec3& position, float radius) {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    uint32_t id = next_mesh_id_++;
    StreamingMeshEntry entry;
    entry.id = id;
    entry.name = name;
    entry.world_position = position;
    entry.bounding_radius = radius;
    meshes_[id] = std::move(entry);
    return id;
}

void MeshStreamingSystem::AddLODLevel(uint32_t mesh_id, uint32_t level, const std::string& asset_path,
                                       float distance_threshold, uint32_t tri_count) {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    auto it = meshes_.find(mesh_id);
    if (it == meshes_.end()) return;

    MeshLODLevel lod;
    lod.level = level;
    lod.asset_path = asset_path;
    lod.distance_threshold = distance_threshold;
    lod.triangle_count = tri_count;
    lod.loaded = false;
    lod.loading = false;

    auto& lods = it->second.lods;
    // Insert sorted by level
    auto pos = std::lower_bound(lods.begin(), lods.end(), lod,
        [](const MeshLODLevel& a, const MeshLODLevel& b) { return a.level < b.level; });
    lods.insert(pos, lod);

    // Keep only the lowest quality LOD (highest level number) as force-resident
    if (config_.force_lowest_lod_resident) {
        for (auto& l : lods) l.loaded = false;
        lods.back().loaded = true;
        it->second.current_lod = lods.back().level;
        it->second.resident = true;
    }
}

void MeshStreamingSystem::UnregisterMesh(uint32_t mesh_id) {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    meshes_.erase(mesh_id);
    last_switch_time_.erase(mesh_id);
}

void MeshStreamingSystem::UpdatePosition(uint32_t mesh_id, const glm::vec3& position) {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    auto it = meshes_.find(mesh_id);
    if (it != meshes_.end()) {
        it->second.world_position = position;
    }
}

void MeshStreamingSystem::Tick(const glm::vec3& camera_position, float delta_time) {
    if (!initialized_) return;

    accumulated_time_ += delta_time;
    load_queue_.clear();

    std::lock_guard<std::mutex> lock(meshes_mutex_);

    for (auto& [id, entry] : meshes_) {
        if (entry.lods.empty()) continue;

        float distance = glm::length(camera_position - entry.world_position);
        entry.last_distance = distance;

        uint32_t desired = EvaluateDesiredLOD(entry, distance);
        entry.desired_lod = desired;

        if (desired != entry.current_lod) {
            // Check switch interval
            auto switch_it = last_switch_time_.find(id);
            if (switch_it != last_switch_time_.end()) {
                float elapsed = accumulated_time_ - switch_it->second;
                if (elapsed < config_.min_switch_interval) continue;
            }

            // Check if target LOD is already loaded
            bool target_loaded = false;
            for (const auto& lod : entry.lods) {
                if (lod.level == desired && lod.loaded) {
                    target_loaded = true;
                    break;
                }
            }

            if (target_loaded) {
                entry.current_lod = desired;
                last_switch_time_[id] = accumulated_time_;
            } else {
                // Enqueue load request
                bool already_loading = false;
                for (const auto& lod : entry.lods) {
                    if (lod.level == desired && lod.loading) {
                        already_loading = true;
                        break;
                    }
                }
                if (!already_loading) {
                    MeshLODRequest req;
                    req.mesh_id = id;
                    req.target_lod = desired;
                    req.priority = distance;
                    load_queue_.push_back(req);
                }
            }
        }
    }

    // Sort by priority (nearest first)
    std::sort(load_queue_.begin(), load_queue_.end(),
        [](const MeshLODRequest& a, const MeshLODRequest& b) {
            return a.priority < b.priority;
        });

    ProcessLoadQueue();
}

uint32_t MeshStreamingSystem::EvaluateDesiredLOD(const StreamingMeshEntry& entry, float distance) const {
    if (entry.lods.empty()) return 0;

    // Find the appropriate LOD based on distance
    // LODs are sorted by level (0=highest quality)
    // Each LOD has a distance_threshold: use this LOD when distance > threshold

    uint32_t best_lod = entry.lods[0].level; // Default to highest quality

    for (const auto& lod : entry.lods) {
        if (distance > lod.distance_threshold) {
            best_lod = lod.level;
        }
    }

    // Apply hysteresis when switching back to higher quality
    if (best_lod < entry.current_lod) {
        // Use the threshold of the current LOD as reference for hysteresis
        float current_threshold = 0.0f;
        for (const auto& lod : entry.lods) {
            if (lod.level == entry.current_lod) {
                current_threshold = lod.distance_threshold;
                break;
            }
        }
        float hysteresis_dist = current_threshold / config_.hysteresis_factor;
        if (distance > hysteresis_dist) {
            best_lod = entry.current_lod; // Stay at current LOD
        }
    }

    return best_lod;
}

void MeshStreamingSystem::ProcessLoadQueue() {
    int budget = config_.load_budget_per_frame;
    int current_active = active_loads_.load(std::memory_order_relaxed);

    for (const auto& req : load_queue_) {
        if (budget <= 0) break;
        if (current_active >= config_.max_concurrent_loads) break;

        auto it = meshes_.find(req.mesh_id);
        if (it == meshes_.end()) continue;

        for (auto& lod : it->second.lods) {
            if (lod.level == req.target_lod && !lod.loaded && !lod.loading) {
                lod.loading = true;
                active_loads_.fetch_add(1, std::memory_order_relaxed);
                ++current_active;
                --budget;

                // Simulate async load completion (in real engine, this would be async IO)
                SimulateLoadComplete(req.mesh_id, req.target_lod);
                break;
            }
        }
    }
}

void MeshStreamingSystem::SimulateLoadComplete(uint32_t mesh_id, uint32_t lod_level) {
    auto it = meshes_.find(mesh_id);
    if (it == meshes_.end()) return;

    for (auto& lod : it->second.lods) {
        if (lod.level == lod_level) {
            lod.loaded = true;
            lod.loading = false;
            active_loads_.fetch_sub(1, std::memory_order_relaxed);

            // Switch to the newly loaded LOD if it's still desired
            if (it->second.desired_lod == lod_level) {
                it->second.current_lod = lod_level;
                last_switch_time_[mesh_id] = accumulated_time_;
            }

            if (load_callback_) {
                load_callback_(mesh_id, lod_level, true);
            }
            break;
        }
    }
}

void MeshStreamingSystem::ForceLoadLOD(uint32_t mesh_id, uint32_t lod_level) {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    SimulateLoadComplete(mesh_id, lod_level);
}

void MeshStreamingSystem::ForceUnloadLOD(uint32_t mesh_id, uint32_t lod_level) {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    auto it = meshes_.find(mesh_id);
    if (it == meshes_.end()) return;

    for (auto& lod : it->second.lods) {
        if (lod.level == lod_level) {
            // Don't unload if it's the only loaded LOD
            if (config_.force_lowest_lod_resident && lod.level == it->second.lods.back().level) {
                return;
            }
            lod.loaded = false;
            lod.loading = false;

            // If current LOD was unloaded, fall back to lowest loaded LOD
            if (it->second.current_lod == lod_level) {
                for (auto rit = it->second.lods.rbegin(); rit != it->second.lods.rend(); ++rit) {
                    if (rit->loaded) {
                        it->second.current_lod = rit->level;
                        break;
                    }
                }
            }
            break;
        }
    }
}

uint32_t MeshStreamingSystem::GetCurrentLOD(uint32_t mesh_id) const {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    auto it = meshes_.find(mesh_id);
    return (it != meshes_.end()) ? it->second.current_lod : 0;
}

uint32_t MeshStreamingSystem::GetDesiredLOD(uint32_t mesh_id) const {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    auto it = meshes_.find(mesh_id);
    return (it != meshes_.end()) ? it->second.desired_lod : 0;
}

uint32_t MeshStreamingSystem::GetMeshCount() const {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    return static_cast<uint32_t>(meshes_.size());
}

uint32_t MeshStreamingSystem::GetLODCount(uint32_t mesh_id) const {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    auto it = meshes_.find(mesh_id);
    return (it != meshes_.end()) ? static_cast<uint32_t>(it->second.lods.size()) : 0;
}

uint64_t MeshStreamingSystem::GetLoadedTriangleCount() const {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    uint64_t total = 0;
    for (const auto& [id, entry] : meshes_) {
        for (const auto& lod : entry.lods) {
            if (lod.loaded) total += lod.triangle_count;
        }
    }
    return total;
}

void MeshStreamingSystem::RebaseOrigin(const glm::vec3& offset) {
    std::lock_guard<std::mutex> lock(meshes_mutex_);
    for (auto& [id, entry] : meshes_) {
        entry.world_position -= offset;
    }
}

} // namespace render
} // namespace dse
