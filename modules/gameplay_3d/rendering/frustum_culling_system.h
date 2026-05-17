#ifndef DSE_FRUSTUM_CULLING_SYSTEM_H
#define DSE_FRUSTUM_CULLING_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/scene/spatial_scene.h"
#include <memory>

namespace dse::gameplay3d {

/**
 * @class FrustumCullingSystem
 * @brief 视锥体剔除系统 — 动静态 Octree 分离 + 分层可见集输出
 *
 * 静态物体的 Octree 仅在首次/Invalidate 后重建，每帧仅更新动态物体列表。
 * 输出 VisibleSet 供渲染 Pass 直接按分类消费（opaque/transparent/shadow_casters）。
 */
class FrustumCullingSystem {
public:
    FrustumCullingSystem() = default;
    ~FrustumCullingSystem() = default;

    /// 执行视锥体剔除更新（自动 lazy-build 静态 Octree）
    void Update(World& world);

    /// 获取上一次 Update 产生的可见集（供渲染 Pass 消费）
    const scene::VisibleSet& GetVisibleSet() const { return visible_set_; }

    /// 强制下一帧重建静态 Octree（场景结构变化时调用）
    void InvalidateStaticTree() { spatial_scene_.Invalidate(); }

    /// 获取 SpatialScene 引用（供外部查询统计）
    const scene::SpatialScene& GetSpatialScene() const { return spatial_scene_; }

private:
    scene::SpatialScene spatial_scene_;
    scene::VisibleSet visible_set_;
};

} // namespace dse::gameplay3d

#endif // DSE_FRUSTUM_CULLING_SYSTEM_H
