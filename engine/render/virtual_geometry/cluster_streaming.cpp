/**
 * @file cluster_streaming.cpp
 * @brief Cluster streaming + VRAM budget management implementation
 */

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/cluster_streaming.h"
#include <algorithm>
#include <cassert>

namespace dse {
namespace render {
namespace vg {

void ClusterStreaming::Init(const VirtualGeometryConfig& config) {
    max_resident_ = config.max_resident_clusters;
    vram_budget_ = static_cast<uint64_t>(config.vram_budget_mb) * 1024 * 1024;
    vram_used_ = 0;
    current_frame_ = 0;
    next_vbo_offset_ = 0;
    next_ibo_offset_ = 0;
    slots_.reserve(max_resident_);
}

void ClusterStreaming::Shutdown() {
    slots_.clear();
    slot_map_.clear();
    pending_loads_.clear();
    requested_this_frame_.clear();
    vram_used_ = 0;
}

void ClusterStreaming::BeginFrame(uint64_t frame_number) {
    current_frame_ = frame_number;
    requested_this_frame_.clear();
    stats_.loads_this_frame = 0;
    stats_.evictions_this_frame = 0;
}

bool ClusterStreaming::RequestCluster(uint32_t mesh_id, uint32_t cluster_index) {
    SlotKey key{mesh_id, cluster_index};
    uint64_t combined = (uint64_t(mesh_id) << 32) | cluster_index;

    if (requested_this_frame_.count(combined)) {
        auto it = slot_map_.find(key);
        return it != slot_map_.end();
    }
    requested_this_frame_.insert(combined);

    auto it = slot_map_.find(key);
    if (it != slot_map_.end()) {
        slots_[it->second].last_used_frame = current_frame_;
        return true;
    }

    pending_loads_.push_back(key);
    return false;
}

void ClusterStreaming::ProcessLoads(
        const std::unordered_map<uint32_t, const VirtualGeometryMesh*>& meshes) {
    while (!pending_loads_.empty()) {
        SlotKey key = pending_loads_.front();
        pending_loads_.pop_front();

        if (slot_map_.count(key)) continue;

        auto mesh_it = meshes.find(key.mesh_id);
        if (mesh_it == meshes.end()) continue;
        const auto* mesh = mesh_it->second;
        if (key.cluster_index >= mesh->clusters.size()) continue;

        const auto& cluster = mesh->clusters[key.cluster_index];
        uint32_t vert_bytes = cluster.vertex_count * sizeof(float) * 8;  // pos+normal+uv
        uint32_t idx_bytes = cluster.triangle_count * 3 * sizeof(uint32_t);
        uint64_t total_bytes = vert_bytes + idx_bytes;

        while (vram_used_ + total_bytes > vram_budget_ && !slots_.empty()) {
            EvictLRU();
        }

        if (slots_.size() >= max_resident_) {
            EvictLRU();
        }

        ClusterSlot slot{};
        slot.mesh_id = key.mesh_id;
        slot.cluster_index = key.cluster_index;
        slot.mega_vbo_offset = AllocateVBOSlot(cluster.vertex_count);
        slot.mega_ibo_offset = AllocateIBOSlot(cluster.triangle_count * 3);
        slot.vertex_count = cluster.vertex_count;
        slot.index_count = cluster.triangle_count * 3;
        slot.last_used_frame = current_frame_;

        uint32_t slot_idx = static_cast<uint32_t>(slots_.size());
        slots_.push_back(slot);
        slot_map_[key] = slot_idx;
        vram_used_ += total_bytes;
        ++stats_.loads_this_frame;
    }

    stats_.resident_clusters = static_cast<uint32_t>(slots_.size());
    stats_.total_slots = max_resident_;
    stats_.vram_used_bytes = vram_used_;
    stats_.vram_budget_bytes = vram_budget_;
}

bool ClusterStreaming::GetClusterSlot(uint32_t mesh_id, uint32_t cluster_index,
                                      uint32_t& out_vbo_offset,
                                      uint32_t& out_ibo_offset) const {
    SlotKey key{mesh_id, cluster_index};
    auto it = slot_map_.find(key);
    if (it == slot_map_.end()) return false;
    const auto& slot = slots_[it->second];
    out_vbo_offset = slot.mega_vbo_offset;
    out_ibo_offset = slot.mega_ibo_offset;
    return true;
}

void ClusterStreaming::TouchCluster(uint32_t mesh_id, uint32_t cluster_index) {
    SlotKey key{mesh_id, cluster_index};
    auto it = slot_map_.find(key);
    if (it != slot_map_.end()) {
        slots_[it->second].last_used_frame = current_frame_;
    }
}

void ClusterStreaming::EvictLRU() {
    if (slots_.empty()) return;

    uint32_t oldest_idx = 0;
    uint64_t oldest_frame = slots_[0].last_used_frame;
    for (uint32_t i = 1; i < slots_.size(); ++i) {
        if (slots_[i].last_used_frame < oldest_frame) {
            oldest_frame = slots_[i].last_used_frame;
            oldest_idx = i;
        }
    }

    const auto& evicted = slots_[oldest_idx];
    uint64_t freed = evicted.vertex_count * sizeof(float) * 8 +
                     evicted.index_count * sizeof(uint32_t);
    vram_used_ = (vram_used_ > freed) ? vram_used_ - freed : 0;

    SlotKey evict_key{evicted.mesh_id, evicted.cluster_index};
    slot_map_.erase(evict_key);

    // Swap-and-pop
    if (oldest_idx != slots_.size() - 1) {
        SlotKey last_key{slots_.back().mesh_id, slots_.back().cluster_index};
        slots_[oldest_idx] = slots_.back();
        slot_map_[last_key] = oldest_idx;
    }
    slots_.pop_back();
    ++stats_.evictions_this_frame;
}

uint32_t ClusterStreaming::AllocateVBOSlot(uint32_t vertex_count) {
    uint32_t offset = next_vbo_offset_;
    next_vbo_offset_ += vertex_count * sizeof(float) * 8;
    return offset;
}

uint32_t ClusterStreaming::AllocateIBOSlot(uint32_t index_count) {
    uint32_t offset = next_ibo_offset_;
    next_ibo_offset_ += index_count * sizeof(uint32_t);
    return offset;
}

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
