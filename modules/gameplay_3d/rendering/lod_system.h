#ifndef DSE_LOD_SYSTEM_H
#define DSE_LOD_SYSTEM_H

#include "engine/ecs/world.h"

class AssetManager;

namespace dse {
namespace gameplay3d {

/**
 * @class LODSystem
 * @brief 屏幕空间投影大小驱动的 Mesh LOD 系统
 *
 * 公式：screen_size = (proj_scale² × bbox_radius²) / max(1, dist²) × global_scale
 * 选第一个 screen_size > threshold 的级别；无匹配时降至最低细节级别。
 *
 * 设计原则（路径驱动，无独立数据缓存）：
 *  - LOD 切换时修改 MeshRendererComponent::mesh_path 并清空 temp_vertices
 *  - MeshRenderSystem::EnsureMeshPathDataLoaded 完成实际解析（避免重复实现）
 *  - LODLevelConfig::loaded=true 后调用 AssetManager::LoadDmesh 预热缓存，
 *    保证切换帧无磁盘 I/O
 *  - hysteresis 死区防止阈值边界的频繁切换抖动
 */
class LODSystem {
public:
    LODSystem() = default;
    ~LODSystem() = default;

    void SetAssetManager(AssetManager* asset_manager);

    /**
     * @brief 每帧更新 LOD 级别；应在 FrustumCullingSystem::Update 之后调用
     */
    void Update(World& world);

private:
    AssetManager* asset_manager_ = nullptr;
    unsigned int next_handle_ = 1;  ///< 稳定 LOD 级别 ID，仅用于标记，不关联数据
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_LOD_SYSTEM_H
