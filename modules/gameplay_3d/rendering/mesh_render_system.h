/**
 * @file mesh_render_system.h
 * @brief 3D 静态网格渲染系统，负责提取 MeshRendererComponent 并提交给 RHI
 */

#ifndef DSE_MESH_RENDER_SYSTEM_H
#define DSE_MESH_RENDER_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
class AssetManager;

namespace dse {
namespace gameplay3d {

/**
 * @class MeshRenderSystem
 * @brief 3D 渲染系统，负责遍历场景中的 3D 网格并提交渲染
 */
class MeshRenderSystem {
public:
    MeshRenderSystem() = default;
    ~MeshRenderSystem() = default;

    /**
     * @brief 提取 ECS 中的 MeshRendererComponent 并提交到 CommandBuffer
     * @param world 实体世界
     * @param cmd_buffer 渲染命令缓冲
     */
    void Render(World& world, CommandBuffer& cmd_buffer);
    void SetAssetManager(AssetManager* asset_manager);
private:
    AssetManager* asset_manager_ = nullptr;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_MESH_RENDER_SYSTEM_H
