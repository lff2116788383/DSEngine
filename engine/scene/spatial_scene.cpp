#include "engine/scene/spatial_scene.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/transform.h"
#include <cmath>
#include <array>
#include <limits>
#include <algorithm>

namespace dse {
namespace scene {

// ---- 内部 frustum 剔除工具 ----

struct FrustumPlane {
    glm::vec3 normal = {0.f, 1.f, 0.f};
    float distance = 0.f;
};

static std::array<FrustumPlane, 6> ExtractPlanes(const glm::mat4& vp) {
    std::array<FrustumPlane, 6> planes;
    auto extract = [&](int idx, float sx, float sy, float sz, float sw) {
        float len = std::sqrt(sx * sx + sy * sy + sz * sz);
        planes[idx].normal = glm::vec3(sx, sy, sz) / len;
        planes[idx].distance = sw / len;
    };
    // Left
    extract(0, vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right
    extract(1, vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom
    extract(2, vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top
    extract(3, vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near
    extract(4, vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far
    extract(5, vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
    return planes;
}

static bool IsAABBInFrustum(const std::array<FrustumPlane, 6>& planes,
                            const glm::vec3& center, const glm::vec3& extents) {
    for (int i = 0; i < 6; ++i) {
        float r = extents.x * std::abs(planes[i].normal.x) +
                  extents.y * std::abs(planes[i].normal.y) +
                  extents.z * std::abs(planes[i].normal.z);
        float d = glm::dot(planes[i].normal, center) + planes[i].distance;
        if (d < -r) return false;
    }
    return true;
}

// ---- 可见实体分类（公共逻辑，消除 DRY 重复）----

static void ClassifyVisibleEntity(entt::entity entity, World& world, VisibleSet& out) {
    if (world.registry().all_of<MeshRendererComponent>(entity)) {
        auto& mr = world.registry().get<MeshRendererComponent>(entity);
        mr.visible = true;
        bool is_transparent = mr.color.a < 1.0f ||
            mr.shader_variant.find("TRANSPARENT") != std::string::npos;
        if (is_transparent) {
            out.transparent.push_back(entity);
        } else {
            out.opaque.push_back(entity);
            // 不透明物体默认投射阴影
            out.shadow_casters.push_back(entity);
        }
    }
    if (world.registry().all_of<TerrainComponent>(entity)) {
        world.registry().get<TerrainComponent>(entity).visible = true;
        out.opaque.push_back(entity);
    }
}

// ---- 递归 Octree 剔除 ----

static void QueryOctreeVisible(const Octree* node,
                               const std::array<FrustumPlane, 6>& planes,
                               World& world,
                               VisibleSet& out) {
    if (!node) return;

    const auto& bounds = node->GetBounds();
    glm::vec3 center = (bounds.min_extents + bounds.max_extents) * 0.5f;
    glm::vec3 extents = (bounds.max_extents - bounds.min_extents) * 0.5f;

    if (!IsAABBInFrustum(planes, center, extents)) return;

    for (const auto& data : node->GetElements()) {
        glm::vec3 obj_c = (data.bounds.min_extents + data.bounds.max_extents) * 0.5f;
        glm::vec3 obj_e = (data.bounds.max_extents - data.bounds.min_extents) * 0.5f;

        if (!IsAABBInFrustum(planes, obj_c, obj_e)) continue;
        ClassifyVisibleEntity(data.entity, world, out);
    }

    if (node->IsDivided()) {
        for (int i = 0; i < 8; ++i) {
            QueryOctreeVisible(node->GetChild(i).get(), planes, world, out);
        }
    }
}

// ---- SpatialScene 实现 ----

AABB SpatialScene::ComputeWorldAABB(World& world, entt::entity e) {
    auto& transform = world.registry().get<TransformComponent>(e);
    auto& bbox = world.registry().get<BoundingBoxComponent>(e);

    glm::vec3 global_center = glm::vec3(transform.local_to_world * glm::vec4(bbox.center(), 1.0f));
    glm::mat3 m(transform.local_to_world);
    glm::vec3 global_extents(
        std::abs(m[0][0]) * bbox.extents().x + std::abs(m[1][0]) * bbox.extents().y + std::abs(m[2][0]) * bbox.extents().z,
        std::abs(m[0][1]) * bbox.extents().x + std::abs(m[1][1]) * bbox.extents().y + std::abs(m[2][1]) * bbox.extents().z,
        std::abs(m[0][2]) * bbox.extents().x + std::abs(m[1][2]) * bbox.extents().y + std::abs(m[2][2]) * bbox.extents().z
    );
    return AABB{global_center - global_extents, global_center + global_extents};
}

void SpatialScene::BuildStatic(World& world) {
    static_entities_.clear();
    dynamic_entities_.clear();
    dynamic_objects_.clear();

    // 所有有 BoundingBox + Transform 的实体
    auto view = world.registry().view<TransformComponent, BoundingBoxComponent>();

    // 判定是否静态：有 RigidBody3D 且为 Dynamic/Kinematic → dynamic，否则 → static
    for (auto entity : view) {
        bool is_dynamic = false;
        if (world.registry().all_of<dse::RigidBody3DComponent>(entity)) {
            auto& rb = world.registry().get<dse::RigidBody3DComponent>(entity);
            if (rb.type != dse::RigidBody3DType::Static) {
                is_dynamic = true;
            }
        }
        // 布料等也视为动态
        if (world.registry().all_of<dse::ClothComponent>(entity)) {
            is_dynamic = true;
        }

        if (is_dynamic) {
            dynamic_entities_.insert(entity);
        } else {
            static_entities_.insert(entity);
        }
    }

    // 计算全局包围盒
    glm::vec3 min_b(std::numeric_limits<float>::max());
    glm::vec3 max_b(std::numeric_limits<float>::lowest());

    for (auto e : static_entities_) {
        AABB aabb = ComputeWorldAABB(world, e);
        min_b = glm::min(min_b, aabb.min_extents);
        max_b = glm::max(max_b, aabb.max_extents);
    }
    if (min_b.x > max_b.x) {
        // 空场景
        min_b = glm::vec3(-1.0f);
        max_b = glm::vec3(1.0f);
    }

    min_b -= glm::vec3(10.0f);
    max_b += glm::vec3(10.0f);

    // 构建静态 Octree
    static_tree_ = std::make_unique<Octree>(AABB{min_b, max_b}, 8, 6);
    for (auto e : static_entities_) {
        AABB aabb = ComputeWorldAABB(world, e);
        static_tree_->Insert({e, aabb});
    }

    built_ = true;
}

void SpatialScene::MarkStatic(entt::entity e) {
    dynamic_entities_.erase(e);
    static_entities_.insert(e);
    built_ = false; // 需要重建静态树
}

void SpatialScene::MarkDynamic(entt::entity e) {
    static_entities_.erase(e);
    dynamic_entities_.insert(e);
    built_ = false;
}

void SpatialScene::UpdateDynamicBounds(World& world) {
    dynamic_objects_.clear();
    dynamic_objects_.reserve(dynamic_entities_.size());
    for (auto e : dynamic_entities_) {
        if (!world.registry().valid(e)) continue;
        if (!world.registry().all_of<TransformComponent, BoundingBoxComponent>(e)) continue;
        dynamic_objects_.push_back({e, ComputeWorldAABB(world, e)});
    }
}

void SpatialScene::CullFrustum(const glm::mat4& view_proj, World& world, VisibleSet& out_visible) {
    out_visible.Clear();

    auto planes = ExtractPlanes(view_proj);

    // 先将所有可渲染实体设为不可见
    auto mr_view = world.registry().view<MeshRendererComponent>();
    for (auto e : mr_view) {
        mr_view.get<MeshRendererComponent>(e).visible = false;
    }
    auto terrain_view = world.registry().view<TerrainComponent>();
    for (auto e : terrain_view) {
        terrain_view.get<TerrainComponent>(e).visible = false;
    }

    // 1. 查询静态 Octree
    if (static_tree_) {
        QueryOctreeVisible(static_tree_.get(), planes, world, out_visible);
    }

    // 2. 遍历动态物体（flat list，无需树查询）
    for (const auto& data : dynamic_objects_) {
        glm::vec3 obj_c = (data.bounds.min_extents + data.bounds.max_extents) * 0.5f;
        glm::vec3 obj_e = (data.bounds.max_extents - data.bounds.min_extents) * 0.5f;

        if (!IsAABBInFrustum(planes, obj_c, obj_e)) continue;
        ClassifyVisibleEntity(data.entity, world, out_visible);
    }
}

} // namespace scene
} // namespace dse
