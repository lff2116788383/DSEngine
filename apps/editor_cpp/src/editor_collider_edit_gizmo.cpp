// 碰撞体编辑的 ImGui/ImGuizmo 视口交互（GPU/ImGui 路径，无头测试不覆盖）。
// 纯数学核心见 editor_collider_edit.cpp。

#include "editor_collider_edit.h"
#include "editor_context.h"

#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/physics_2d.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include <glm/gtc/type_ptr.hpp>

namespace dse::editor {

bool& GetColliderEditEnabled() {
    static bool enabled = false;
    return enabled;
}

namespace {

using collideredit::Kind;

Kind DetectKind(entt::registry& reg, entt::entity e) {
    if (e == entt::null || !reg.valid(e)) return Kind::None;
    if (reg.all_of<dse::BoxCollider3DComponent>(e)) return Kind::Box3D;
    if (reg.all_of<dse::SphereCollider3DComponent>(e)) return Kind::Sphere3D;
    if (reg.all_of<BoxCollider2DComponent>(e)) return Kind::Box2D;
    if (reg.all_of<CircleCollider2DComponent>(e)) return Kind::Circle2D;
    return Kind::None;
}

}  // namespace

bool DrawColliderEditGizmo(EditorContext& ctx,
                           const glm::vec2& window_pos,
                           const glm::vec2& panel_size,
                           const glm::mat4& view,
                           const glm::mat4& proj) {
    if (!GetColliderEditEnabled()) return false;

    entt::registry& reg = ctx.registry;
    const entt::entity e = ctx.selected_entity;
    const Kind kind = DetectKind(reg, e);
    if (kind == Kind::None) return false;
    if (!reg.all_of<TransformComponent>(e)) return false;

    auto& tf = reg.get<TransformComponent>(e);
    const glm::vec3 epos = tf.position;
    const glm::vec3 escale = tf.scale;

    // 当前碰撞体 → 世界矩阵
    glm::mat4 m(1.0f);
    bool is_sphere = false;
    switch (kind) {
        case Kind::Box3D: {
            auto& c = reg.get<dse::BoxCollider3DComponent>(e);
            m = collideredit::BuildBoxMatrix(epos, escale, c.center, c.size);
            break;
        }
        case Kind::Sphere3D: {
            auto& c = reg.get<dse::SphereCollider3DComponent>(e);
            m = collideredit::BuildSphereMatrix(epos, escale, c.center, c.radius);
            is_sphere = true;
            break;
        }
        case Kind::Box2D: {
            auto& c = reg.get<BoxCollider2DComponent>(e);
            m = collideredit::BuildBoxMatrix(epos, escale,
                    glm::vec3(c.offset.x, c.offset.y, 0.0f),
                    glm::vec3(c.size.x, c.size.y, 0.01f));
            break;
        }
        case Kind::Circle2D: {
            auto& c = reg.get<CircleCollider2DComponent>(e);
            m = collideredit::BuildSphereMatrix(epos, escale,
                    glm::vec3(c.offset.x, c.offset.y, 0.0f), c.radius);
            is_sphere = true;
            break;
        }
        default: return false;
    }

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(window_pos.x, window_pos.y, panel_size.x, panel_size.y);

    glm::mat4 view_m = view;
    glm::mat4 proj_m = proj;
    // 球/圆用均匀缩放控制半径，盒体用平移+缩放控制 center/size。
    const ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE |
        (is_sphere ? ImGuizmo::SCALEU : ImGuizmo::SCALE);

    const bool changed = ImGuizmo::Manipulate(glm::value_ptr(view_m), glm::value_ptr(proj_m),
                                              op, ImGuizmo::WORLD, glm::value_ptr(m));
    if (changed) {
        switch (kind) {
            case Kind::Box3D: {
                auto& c = reg.get<dse::BoxCollider3DComponent>(e);
                collideredit::ExtractBox(m, epos, escale, c.center, c.size);
                c.prev_size = glm::vec3(-1.0f);  // 触发后端形状重建
                break;
            }
            case Kind::Sphere3D: {
                auto& c = reg.get<dse::SphereCollider3DComponent>(e);
                collideredit::ExtractSphere(m, epos, escale, c.center, c.radius);
                c.prev_radius = -1.0f;
                break;
            }
            case Kind::Box2D: {
                auto& c = reg.get<BoxCollider2DComponent>(e);
                glm::vec3 center, size;
                collideredit::ExtractBox(m, epos, escale, center, size);
                c.offset = glm::vec2(center.x, center.y);
                c.size = glm::vec2(size.x, size.y);
                break;
            }
            case Kind::Circle2D: {
                auto& c = reg.get<CircleCollider2DComponent>(e);
                glm::vec3 center; float radius;
                collideredit::ExtractSphere(m, epos, escale, center, radius);
                c.offset = glm::vec2(center.x, center.y);
                c.radius = radius;
                break;
            }
            default: break;
        }
    }

    return ImGuizmo::IsUsing() || ImGuizmo::IsOver();
}

}  // namespace dse::editor
