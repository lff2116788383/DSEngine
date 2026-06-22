#ifndef DSE_RENDER_RENDER_SCENE_H
#define DSE_RENDER_RENDER_SCENE_H

#include <cstdint>
#include <functional>
#include <vector>
#include <glm/glm.hpp>
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/mesh_renderer.h"

class World;

namespace dse {
namespace render {

class CommandBuffer;
class ISceneRenderer;

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
    glm::vec3 camera_offset{0.0f};  ///< Camera-Relative: model matrix 平移减去此偏移
    int cascade_index = 0;
};

struct RenderScene {
    void Clear() {
        objects.clear();
        cpu_meshes.opaque.clear();
        cpu_meshes.skinned.clear();
        cpu_meshes.transparent.clear();
        cpu_meshes.static_cpu_fallback.clear();
        scene_renderers.clear();
    }

    /// Camera-Relative Rendering: 将所有 CPU mesh model matrix 的平移部分减去 camera_offset
    void ApplyCameraOffset(const glm::vec3& camera_offset) {
        if (camera_offset == glm::vec3(0.0f)) return;
        glm::vec4 offset4(camera_offset, 0.0f);
        auto offset_items = [&](std::vector<MeshDrawItem>& items) {
            for (auto& item : items) {
                item.model[3] -= offset4;
            }
        };
        offset_items(cpu_meshes.opaque);
        offset_items(cpu_meshes.skinned);
        offset_items(cpu_meshes.transparent);
        offset_items(cpu_meshes.static_cpu_fallback);
    }

    /// 阶段4-M4：取代 cmd.DrawMeshBatch ABI，逐队列经常驻 MeshRenderer::DrawBatch 分发。
    /// view/proj 取自 cmd（含投影修正，与原执行器同源）；depth-only/gbuffer/编辑器模式
    /// 由 device.GetGlobalRenderState() 透出，DrawBatch 内部据此路由（见 mesh_renderer.cpp）。
    void DrawOpaqueCpu(CommandBuffer& cmd, RhiDevice& device, MeshRenderer& renderer) const {
        const glm::mat4 view = cmd.GetViewMatrix();
        const glm::mat4 proj = cmd.GetProjectionMatrix();
        if (!cpu_meshes.opaque.empty()) renderer.DrawBatch(cmd, device, cpu_meshes.opaque, view, proj);
        if (!cpu_meshes.skinned.empty()) renderer.DrawBatch(cmd, device, cpu_meshes.skinned, view, proj);
        if (!cpu_meshes.static_cpu_fallback.empty()) renderer.DrawBatch(cmd, device, cpu_meshes.static_cpu_fallback, view, proj);
    }

    void DrawTransparent(CommandBuffer& cmd, int wboit_mode, RhiDevice& device, MeshRenderer& renderer) {
        for (auto& item : cpu_meshes.transparent) {
            item.wboit_mode = wboit_mode;
        }
        if (!cpu_meshes.transparent.empty())
            renderer.DrawBatch(cmd, device, cpu_meshes.transparent,
                               cmd.GetViewMatrix(), cmd.GetProjectionMatrix());
    }

    std::vector<RenderObjectRef> objects;
    CpuMeshQueue cpu_meshes;

    /// 模块注册的强类型场景贡献对象（逐帧由模块在 BuildRenderQueues 时注册）。
    /// 由内建 Pass 在各自渲染作用域内按阶段（PreZ/Shadow/Opaque/Transparent）调用。
    std::vector<ISceneRenderer*> scene_renderers;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_RENDER_SCENE_H
