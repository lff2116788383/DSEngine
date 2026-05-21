#include "editor_selection_outline.h"
#include "editor_context.h"
#include "editor_selection.h"
#include "editor_shared_components.h"
#include "editor_overlay_utils.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

using namespace dse::editor::overlay;

namespace dse::editor {

namespace {

/// Get the axis-aligned bounding box for an entity (in world space).
/// Returns false if the entity has no spatial component.
bool GetEntityAABB(entt::registry& reg, entt::entity entity,
                   glm::vec3& out_min, glm::vec3& out_max) {
    if (!reg.all_of<TransformComponent>(entity)) return false;
    auto& tf = reg.get<TransformComponent>(entity);

    glm::vec3 half_size(0.5f);

    // Try MeshRenderer for 3D bounds
    if (reg.all_of<dse::MeshRendererComponent>(entity)) {
        // Use unit cube scaled by transform.scale as default bounding box
        half_size = tf.scale * 0.5f;
    }
    // Try SpriteRenderer for 2D bounds
    else if (reg.all_of<SpriteRendererComponent>(entity)) {
        half_size = glm::vec3(tf.scale.x * 0.5f, tf.scale.y * 0.5f, 0.01f);
    }
    // Try BoxCollider3D
    else if (reg.all_of<dse::BoxCollider3DComponent>(entity)) {
        auto& col = reg.get<dse::BoxCollider3DComponent>(entity);
        half_size = col.size * 0.5f * tf.scale;
    }
    // Try SphereCollider3D
    else if (reg.all_of<dse::SphereCollider3DComponent>(entity)) {
        auto& col = reg.get<dse::SphereCollider3DComponent>(entity);
        float max_scale = std::max({tf.scale.x, tf.scale.y, tf.scale.z});
        float r = col.radius * max_scale;
        half_size = glm::vec3(r);
    }
    // Default: small box at position
    else {
        half_size = tf.scale * 0.5f;
        if (glm::length(half_size) < 0.01f) {
            half_size = glm::vec3(0.25f);
        }
    }

    out_min = tf.position - half_size;
    out_max = tf.position + half_size;
    return true;
}

void DrawWireframeAABB(ImDrawList* dl,
                       const glm::vec3& aabb_min,
                       const glm::vec3& aabb_max,
                       const glm::mat4& view,
                       const glm::mat4& proj,
                       const glm::vec2& win_pos,
                       const glm::vec2& panel_size,
                       ImU32 color,
                       float thickness) {
    glm::vec3 center = (aabb_min + aabb_max) * 0.5f;
    glm::vec3 half = (aabb_max - aabb_min) * 0.5f;
    DrawWireBox(dl, center, half, view, proj, win_pos, panel_size, color, thickness);
}

} // namespace

void DrawSelectionOutlines(EditorContext& ctx,
                           const glm::vec2& window_pos,
                           const glm::vec2& panel_size,
                           const glm::mat4& view,
                           const glm::mat4& proj) {
    auto& selection = SelectionManager::Get();
    if (selection.IsEmpty()) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (auto entity : selection.GetAll()) {
        if (!ctx.registry.valid(entity)) continue;

        glm::vec3 aabb_min, aabb_max;
        if (!GetEntityAABB(ctx.registry, entity, aabb_min, aabb_max)) continue;

        bool is_primary = (entity == selection.GetPrimary());
        ImU32 color = is_primary ? IM_COL32(255, 165, 0, 220) : IM_COL32(100, 180, 255, 180);
        float thickness = is_primary ? 2.0f : 1.5f;

        DrawWireframeAABB(dl, aabb_min, aabb_max, view, proj,
                          window_pos, panel_size, color, thickness);

        // Label for primary selection
        if (is_primary && ctx.registry.all_of<EditorNameComponent>(entity)) {
            auto& name = ctx.registry.get<EditorNameComponent>(entity);
            glm::vec3 label_pos = glm::vec3((aabb_min.x + aabb_max.x) * 0.5f,
                                             aabb_max.y + 0.2f,
                                             (aabb_min.z + aabb_max.z) * 0.5f);
            ImVec2 sp = WorldToScreen(label_pos, view, proj, window_pos, panel_size);
            if (sp.x > -5000.0f) {
                ImVec2 text_size = ImGui::CalcTextSize(name.name.c_str());
                ImVec2 bg_min(sp.x - text_size.x * 0.5f - 4.0f, sp.y - text_size.y - 2.0f);
                ImVec2 bg_max(sp.x + text_size.x * 0.5f + 4.0f, sp.y + 2.0f);
                dl->AddRectFilled(bg_min, bg_max, IM_COL32(0, 0, 0, 160), 3.0f);
                dl->AddText(ImVec2(sp.x - text_size.x * 0.5f, sp.y - text_size.y), IM_COL32(255, 165, 0, 255), name.name.c_str());
            }
        }
    }
}

} // namespace dse::editor
