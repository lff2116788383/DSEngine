/**
 * @file cluster_streaming.h
 * @brief Cluster data streaming + VRAM budget management
 *
 * Manages a fixed-size VRAM pool of resident clusters.  As the camera moves,
 * LOD selection requests clusters; the streamer loads them from disk (or
 * in-memory cache) and uploads to the mega buffer.  When the pool is full,
 * an LRU policy evicts the least-recently-used clusters.
 */

#ifndef DSE_CLUSTER_STREAMING_H
#define DSE_CLUSTER_STREAMING_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_types.h"
#include "engine/render/virtual_geometry/virtual_geometry_config.h"
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dse {
namespace render {
namespace vg {

struct ClusterSlot {
    uint32_t mesh_id;
    uint32_t cluster_index;
    uint32_t mega_vbo_offset;     ///< Byte offset in mega VBO
    uint32_t mega_ibo_offset;     ///< Byte offset in mega IBO
    uint32_t vertex_count;
    uint32_t index_count;
    uint64_t last_used_frame;
};

struct StreamingStats {
    uint32_t resident_clusters = 0;
    uint32_t total_slots = 0;
    uint32_t loads_this_frame = 0;
    uint32_t evictions_this_frame = 0;
    uint64_t vram_used_bytes = 0;
    uint64_t vram_budget_bytes = 0;
};

class ClusterStreaming {
public:
    ClusterStreaming() = default;
    ~ClusterStreaming() = default;

    void Init(const VirtualGeometryConfig& config);
    void Shutdown();

    /// Begin a new frame — age all slots, clear per-frame counters
    void BeginFrame(uint64_t frame_number);

    /// Request a cluster to be resident.  Returns true if already loaded.
    bool RequestCluster(uint32_t mesh_id, uint32_t cluster_index);

    /// Process pending loads (call after LOD selection, before rendering)
    void ProcessLoads(const std::unordered_map<uint32_t, const VirtualGeometryMesh*>& meshes);

    /// Get the mega buffer offset for a resident cluster
    /// Returns false if the cluster is not resident
    bool GetClusterSlot(uint32_t mesh_id, uint32_t cluster_index,
                        uint32_t& out_vbo_offset, uint32_t& out_ibo_offset) const;

    /// Mark a cluster as used this frame (prevents eviction)
    void TouchCluster(uint32_t mesh_id, uint32_t cluster_index);

    const StreamingStats& GetStats() const { return stats_; }

    /// Get all resident cluster data for GPU upload
    const std::vector<ClusterSlot>& GetResidentSlots() const { return slots_; }

private:
    struct SlotKey {
        uint32_t mesh_id;
        uint32_t cluster_index;
        bool operator==(const SlotKey& o) const {
            return mesh_id == o.mesh_id && cluster_index == o.cluster_index;
        }
    };
    struct SlotKeyHash {
        size_t operator()(const SlotKey& k) const {
            return std::hash<uint64_t>()(
                (uint64_t(k.mesh_id) << 32) | k.cluster_index);
        }
    };

    void EvictLRU();
    uint32_t AllocateVBOSlot(uint32_t vertex_count);
    uint32_t AllocateIBOSlot(uint32_t index_count);

    std::vector<ClusterSlot> slots_;
    std::unordered_map<SlotKey, uint32_t, SlotKeyHash> slot_map_;
    std::deque<SlotKey> pending_loads_;
    std::unordered_set<uint64_t> requested_this_frame_;

    uint32_t max_resident_ = 0;
    uint64_t vram_budget_ = 0;
    uint64_t vram_used_ = 0;
    uint64_t current_frame_ = 0;

    uint32_t next_vbo_offset_ = 0;
    uint32_t next_ibo_offset_ = 0;

    StreamingStats stats_;
};

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_CLUSTER_STREAMING_H
