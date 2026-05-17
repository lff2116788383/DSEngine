#ifndef DSE_SCENE_SPATIAL_SCENE_H
#define DSE_SCENE_SPATIAL_SCENE_H

#include "engine/scene/octree.h"
#include "engine/ecs/world.h"
#include <vector>
#include <unordered_set>

namespace dse {
namespace scene {

/// 渲染分类（供 VisibleSet 分层输出）
enum class RenderCategory : uint8_t {
    Opaque = 0,
    AlphaTest = 1,
    Transparent = 2,
    Count
};

/// 分层可见集 — FrustumCullingSystem 的输出，供渲染 Pass 直接消费
struct VisibleSet {
    std::vector<entt::entity> opaque;
    std::vector<entt::entity> transparent;
    std::vector<entt::entity> shadow_casters;

    void Clear() {
        opaque.clear();
        transparent.clear();
        shadow_casters.clear();
    }

    size_t TotalCount() const {
        return opaque.size() + transparent.size() + shadow_casters.size();
    }
};

/// 空间场景 — 动静态 Octree 分离，替代每帧全量重建 Octree 的做法
class SpatialScene {
public:
    SpatialScene() = default;
    ~SpatialScene() = default;

    /// 构建静态 Octree（场景加载时调用一次）
    void BuildStatic(World& world);

    /// 标记实体为静态（不参与每帧遍历）
    void MarkStatic(entt::entity e);

    /// 标记实体为动态（每帧重新计算包围盒）
    void MarkDynamic(entt::entity e);

    /// 每帧更新：仅处理动态物体的包围盒变化
    void UpdateDynamicBounds(World& world);

    /// 执行视锥剔除，输出到 VisibleSet 和实体的 visible 标记
    void CullFrustum(const glm::mat4& view_proj, World& world, VisibleSet& out_visible);

    /// 静态 Octree 是否已构建
    bool IsBuilt() const { return built_; }

    /// 强制重建（场景结构剧变时调用）
    void Invalidate() { built_ = false; }

    size_t GetStaticCount() const { return static_entities_.size(); }
    size_t GetDynamicCount() const { return dynamic_entities_.size(); }

private:
    /// 计算实体的世界空间 AABB
    static AABB ComputeWorldAABB(World& world, entt::entity e);

    std::unique_ptr<Octree> static_tree_;
    std::vector<OctreeData> dynamic_objects_;
    std::unordered_set<entt::entity> static_entities_;
    std::unordered_set<entt::entity> dynamic_entities_;
    bool built_ = false;
};

} // namespace scene
} // namespace dse

#endif // DSE_SCENE_SPATIAL_SCENE_H
