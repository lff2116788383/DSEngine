#include "frustum_culling_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <cmath>
#include <algorithm>

namespace dse {
namespace gameplay3d {

struct Plane {
    glm::vec3 normal = {0.f, 1.f, 0.f};
    float distance = 0.f;

    Plane() = default;
    Plane(const glm::vec4& p) : normal(p.x, p.y, p.z), distance(p.w) {}

    void Normalize() {
        float length = glm::length(normal);
        normal /= length;
        distance /= length;
    }
};

static std::array<Plane, 6> ExtractFrustumPlanes(const glm::mat4& view_proj) {
    std::array<Plane, 6> planes;
    
    // Left
    planes[0] = Plane(glm::vec4(view_proj[0][3] + view_proj[0][0], view_proj[1][3] + view_proj[1][0], view_proj[2][3] + view_proj[2][0], view_proj[3][3] + view_proj[3][0]));
    // Right
    planes[1] = Plane(glm::vec4(view_proj[0][3] - view_proj[0][0], view_proj[1][3] - view_proj[1][0], view_proj[2][3] - view_proj[2][0], view_proj[3][3] - view_proj[3][0]));
    // Bottom
    planes[2] = Plane(glm::vec4(view_proj[0][3] + view_proj[0][1], view_proj[1][3] + view_proj[1][1], view_proj[2][3] + view_proj[2][1], view_proj[3][3] + view_proj[3][1]));
    // Top
    planes[3] = Plane(glm::vec4(view_proj[0][3] - view_proj[0][1], view_proj[1][3] - view_proj[1][1], view_proj[2][3] - view_proj[2][1], view_proj[3][3] - view_proj[3][1]));
    // Near
    planes[4] = Plane(glm::vec4(view_proj[0][3] + view_proj[0][2], view_proj[1][3] + view_proj[1][2], view_proj[2][3] + view_proj[2][2], view_proj[3][3] + view_proj[3][2]));
    // Far
    planes[5] = Plane(glm::vec4(view_proj[0][3] - view_proj[0][2], view_proj[1][3] - view_proj[1][2], view_proj[2][3] - view_proj[2][2], view_proj[3][3] - view_proj[3][2]));

    for (auto& plane : planes) {
        plane.Normalize();
    }
    return planes;
}

static bool IsAABBVisible(const std::array<Plane, 6>& planes, const glm::vec3& center, const glm::vec3& extents) {
    for (int i = 0; i < 6; ++i) {
        const auto& plane = planes[i];
        float r = extents.x * std::abs(plane.normal.x) + 
                  extents.y * std::abs(plane.normal.y) + 
                  extents.z * std::abs(plane.normal.z);
        float d = glm::dot(plane.normal, center) + plane.distance;
        
        if (d < -r) {
            return false; // completely outside
        }
    }
    return true;
}

static void QueryOctree(const scene::Octree* node, const std::array<Plane, 6>& planes, World& world) {
    if (!node) return;

    const auto& bounds = node->GetBounds();
    glm::vec3 center = (bounds.min_extents + bounds.max_extents) * 0.5f;
    glm::vec3 extents = (bounds.max_extents - bounds.min_extents) * 0.5f;

    if (!IsAABBVisible(planes, center, extents)) {
        return; // Node is outside frustum, skip its children
    }

    // Process elements in this node
    for (const auto& data : node->GetElements()) {
        glm::vec3 obj_center = (data.bounds.min_extents + data.bounds.max_extents) * 0.5f;
        glm::vec3 obj_extents = (data.bounds.max_extents - data.bounds.min_extents) * 0.5f;
        
        bool is_visible = IsAABBVisible(planes, obj_center, obj_extents);
        
        if (world.registry().all_of<MeshRendererComponent>(data.entity)) {
            world.registry().get<MeshRendererComponent>(data.entity).visible = is_visible;
        }
        if (world.registry().all_of<TerrainComponent>(data.entity)) {
            world.registry().get<TerrainComponent>(data.entity).visible = is_visible;
        }
    }

    // Process children
    if (node->IsDivided()) {
        for (int i = 0; i < 8; ++i) {
            QueryOctree(node->GetChild(i).get(), planes, world);
        }
    }
}

void FrustumCullingSystem::Update(World& world) {
    // 1. Find main camera
    auto camera_view = world.registry().view<Camera3DComponent>();
    entt::entity main_camera = entt::null;
    int max_priority = -9999;
    
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<Camera3DComponent>((entt::entity)entity);
        if (cam.enabled && cam.priority > max_priority) {
            max_priority = cam.priority;
            main_camera = (entt::entity)entity;
        }
    }

    if (main_camera == entt::null) {
        return; // No valid camera
    }

    auto& cam = camera_view.get<Camera3DComponent>((entt::entity)main_camera);
    glm::mat4 projection = glm::perspective(glm::radians(cam.fov), cam.aspect_ratio, cam.near_clip, cam.far_clip);
    glm::mat4 view = glm::mat4(1.0f);
    if (world.registry().all_of<TransformComponent>((entt::entity)main_camera)) {
        auto& transform = world.registry().get<TransformComponent>((entt::entity)main_camera);
        glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        view = glm::lookAt(transform.position, transform.position + front, up);
    }
    
    glm::mat4 view_proj = projection * view;
    std::array<Plane, 6> planes = ExtractFrustumPlanes(view_proj);

    // Rebuild Octree (simple dynamic approach)
    glm::vec3 min_bounds(std::numeric_limits<float>::max());
    glm::vec3 max_bounds(std::numeric_limits<float>::lowest());
    
    auto renderable_view = world.registry().view<TransformComponent, BoundingBoxComponent>();
    
    // Calculate global bounds
    for (auto entity : renderable_view) {
        auto& transform = renderable_view.get<TransformComponent>(entity);
        auto& bbox = renderable_view.get<BoundingBoxComponent>(entity);
        glm::vec3 global_center = glm::vec3(transform.local_to_world * glm::vec4(bbox.center(), 1.0f));
        glm::mat3 m(transform.local_to_world);
        glm::vec3 global_extents(
            std::abs(m[0][0]) * bbox.extents().x + std::abs(m[1][0]) * bbox.extents().y + std::abs(m[2][0]) * bbox.extents().z,
            std::abs(m[0][1]) * bbox.extents().x + std::abs(m[1][1]) * bbox.extents().y + std::abs(m[2][1]) * bbox.extents().z,
            std::abs(m[0][2]) * bbox.extents().x + std::abs(m[1][2]) * bbox.extents().y + std::abs(m[2][2]) * bbox.extents().z
        );
        min_bounds = glm::min(min_bounds, global_center - global_extents);
        max_bounds = glm::max(max_bounds, global_center + global_extents);
    }
    
    // Add small padding
    min_bounds -= glm::vec3(1.0f);
    max_bounds += glm::vec3(1.0f);
    
    octree_ = std::make_unique<scene::Octree>(scene::AABB{min_bounds, max_bounds});
    
    for (auto entity : renderable_view) {
        auto& transform = renderable_view.get<TransformComponent>(entity);
        auto& bbox = renderable_view.get<BoundingBoxComponent>(entity);
        glm::vec3 global_center = glm::vec3(transform.local_to_world * glm::vec4(bbox.center(), 1.0f));
        glm::mat3 m(transform.local_to_world);
        glm::vec3 global_extents(
            std::abs(m[0][0]) * bbox.extents().x + std::abs(m[1][0]) * bbox.extents().y + std::abs(m[2][0]) * bbox.extents().z,
            std::abs(m[0][1]) * bbox.extents().x + std::abs(m[1][1]) * bbox.extents().y + std::abs(m[2][1]) * bbox.extents().z,
            std::abs(m[0][2]) * bbox.extents().x + std::abs(m[1][2]) * bbox.extents().y + std::abs(m[2][2]) * bbox.extents().z
        );
        
        // Default to false
        if (world.registry().all_of<MeshRendererComponent>(entity)) {
            world.registry().get<MeshRendererComponent>(entity).visible = false;
        }
        if (world.registry().all_of<TerrainComponent>(entity)) {
            world.registry().get<TerrainComponent>(entity).visible = false;
        }
        
        octree_->Insert({entity, scene::AABB{global_center - global_extents, global_center + global_extents}});
    }

    // 2. Visibility test using Octree
    QueryOctree(octree_.get(), planes, world);
}

} // namespace gameplay3d
} // namespace dse
