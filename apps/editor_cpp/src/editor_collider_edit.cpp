// 碰撞体编辑的纯数学核心（无 ImGui 依赖，可无头测试）。
// ImGui/ImGuizmo 视口交互见 editor_collider_edit_gizmo.cpp。

#include "editor_collider_edit.h"

#include <algorithm>

namespace dse::editor::collideredit {

glm::mat4 BuildBoxMatrix(const glm::vec3& entity_pos, const glm::vec3& entity_scale,
                         const glm::vec3& local_center, const glm::vec3& size) {
    const glm::vec3 world_center = entity_pos + local_center * entity_scale;
    const glm::vec3 world_size = size * entity_scale;
    glm::mat4 m(1.0f);
    m[0][0] = world_size.x;
    m[1][1] = world_size.y;
    m[2][2] = world_size.z;
    m[3][0] = world_center.x;
    m[3][1] = world_center.y;
    m[3][2] = world_center.z;
    return m;
}

void ExtractBox(const glm::mat4& m, const glm::vec3& entity_pos, const glm::vec3& entity_scale,
                glm::vec3& out_center, glm::vec3& out_size) {
    const glm::vec3 world_center(m[3][0], m[3][1], m[3][2]);
    const glm::vec3 world_size(glm::length(glm::vec3(m[0])),
                               glm::length(glm::vec3(m[1])),
                               glm::length(glm::vec3(m[2])));
    out_center = glm::vec3(SafeDiv(world_center.x - entity_pos.x, entity_scale.x),
                           SafeDiv(world_center.y - entity_pos.y, entity_scale.y),
                           SafeDiv(world_center.z - entity_pos.z, entity_scale.z));
    out_size = glm::vec3(std::max(SafeDiv(world_size.x, entity_scale.x), 0.0001f),
                         std::max(SafeDiv(world_size.y, entity_scale.y), 0.0001f),
                         std::max(SafeDiv(world_size.z, entity_scale.z), 0.0001f));
}

glm::mat4 BuildSphereMatrix(const glm::vec3& entity_pos, const glm::vec3& entity_scale,
                            const glm::vec3& local_center, float radius) {
    const float ref = std::max({entity_scale.x, entity_scale.y, entity_scale.z});
    const glm::vec3 world_center = entity_pos + local_center * entity_scale;
    const float diameter = radius * 2.0f * ref;
    glm::mat4 m(1.0f);
    m[0][0] = diameter;
    m[1][1] = diameter;
    m[2][2] = diameter;
    m[3][0] = world_center.x;
    m[3][1] = world_center.y;
    m[3][2] = world_center.z;
    return m;
}

void ExtractSphere(const glm::mat4& m, const glm::vec3& entity_pos, const glm::vec3& entity_scale,
                   glm::vec3& out_center, float& out_radius) {
    const float ref = std::max({entity_scale.x, entity_scale.y, entity_scale.z});
    const glm::vec3 world_center(m[3][0], m[3][1], m[3][2]);
    // 取三轴平均直径，抗各向异性缩放带来的轻微差异。
    const float diameter = (glm::length(glm::vec3(m[0])) +
                            glm::length(glm::vec3(m[1])) +
                            glm::length(glm::vec3(m[2]))) / 3.0f;
    out_center = glm::vec3(SafeDiv(world_center.x - entity_pos.x, entity_scale.x),
                           SafeDiv(world_center.y - entity_pos.y, entity_scale.y),
                           SafeDiv(world_center.z - entity_pos.z, entity_scale.z));
    out_radius = std::max(SafeDiv(diameter, 2.0f * ref), 0.0001f);
}

}  // namespace dse::editor::collideredit
