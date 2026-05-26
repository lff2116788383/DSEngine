#ifndef DSE_RENDER_RENDER_SCENE_H
#define DSE_RENDER_RENDER_SCENE_H

#include <cstdint>
#include <functional>
#include <vector>
#include <glm/glm.hpp>
#include "engine/render/rhi/rhi_device.h"

class World;

namespace dse {
namespace render {

class CommandBuffer;

struct RenderObjectId {
    uint64_t value = 0;

    explicit operator bool() const { return value != 0; }
    bool operator==(const RenderObjectId& rhs) const { return value == rhs.value; }
    bool operator!=(const RenderObjectId& rhs) const { return value != rhs.value; }
};

enum class RenderQueueKind {
    OpaqueGpuDriven,
    OpaqueCpu,
    Skinned,
    Transparent,
    Terrain,
    Foliage,
    Hair,
    Particle,
    Debug,
};

struct RenderObjectRef {
    RenderObjectId id;
    RenderQueueKind queue = RenderQueueKind::OpaqueCpu;
    glm::vec3 bounds_min = glm::vec3(0.0f);
    glm::vec3 bounds_max = glm::vec3(0.0f);
};

struct CpuMeshQueue {
    std::vector<MeshDrawItem> opaque;
    std::vector<MeshDrawItem> skinned;
    std::vector<MeshDrawItem> transparent;
    std::vector<MeshDrawItem> static_cpu_fallback;
};

struct RenderScenePassContext {
    World* world = nullptr;
    const glm::mat4* view = nullptr;
    const glm::mat4* projection = nullptr;
    const glm::mat4* clip_correction = nullptr;
    int cascade_index = 0;
    int wboit_mode = 0;
};

using RenderQueueCallback = std::function<void(CommandBuffer&, const RenderScenePassContext&)>;

struct RenderScene {
    void Clear() {
        objects.clear();
        cpu_meshes.opaque.clear();
        cpu_meshes.skinned.clear();
        cpu_meshes.transparent.clear();
        cpu_meshes.static_cpu_fallback.clear();
        prez_callbacks.clear();
        shadow_callbacks.clear();
        opaque_callbacks.clear();
        transparent_callbacks.clear();
        terrain_callbacks.clear();
        foliage_callbacks.clear();
        hair_callbacks.clear();
        particle_callbacks.clear();
        debug_callbacks.clear();
    }

    void DrawOpaqueCpu(CommandBuffer& cmd) const {
        if (!cpu_meshes.opaque.empty()) cmd.DrawMeshBatch(cpu_meshes.opaque);
        if (!cpu_meshes.skinned.empty()) cmd.DrawMeshBatch(cpu_meshes.skinned);
        if (!cpu_meshes.static_cpu_fallback.empty()) cmd.DrawMeshBatch(cpu_meshes.static_cpu_fallback);
    }

    void DrawTransparent(CommandBuffer& cmd, int wboit_mode) {
        for (auto& item : cpu_meshes.transparent) {
            item.wboit_mode = wboit_mode;
        }
        if (!cpu_meshes.transparent.empty()) cmd.DrawMeshBatch(cpu_meshes.transparent);
    }

    void ExecuteCallbacks(const std::vector<RenderQueueCallback>& callbacks,
                          CommandBuffer& cmd,
                          const RenderScenePassContext& pass_ctx) const {
        for (const auto& cb : callbacks) {
            if (cb) cb(cmd, pass_ctx);
        }
    }

    std::vector<RenderObjectRef> objects;
    CpuMeshQueue cpu_meshes;
    std::vector<RenderQueueCallback> prez_callbacks;
    std::vector<RenderQueueCallback> shadow_callbacks;
    std::vector<RenderQueueCallback> opaque_callbacks;
    std::vector<RenderQueueCallback> transparent_callbacks;
    std::vector<RenderQueueCallback> terrain_callbacks;
    std::vector<RenderQueueCallback> foliage_callbacks;
    std::vector<RenderQueueCallback> hair_callbacks;
    std::vector<RenderQueueCallback> particle_callbacks;
    std::vector<RenderQueueCallback> debug_callbacks;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_RENDER_SCENE_H
