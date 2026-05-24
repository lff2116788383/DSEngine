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

/// Get the 8 oriented bounding box corners for an entity (in world space).
/// Uses local_to_world to properly account for rotation.
/// Returns false if the entity has no spatial component.
bool GetEntityOBBCorners(entt::registry& reg, entt::entity entity,
                         glm::vec3 out_corners[8]) {
    if (!reg.all_of<TransformComponent>(entity)) return false;
    auto& tf = reg.get<TransformComponent>(entity);

    glm::vec3 local_min(-0.5f);
    glm::vec3 local_max( 0.5f);

    // Try MeshRenderer for 3D bounds
    if (reg.all_of<dse::MeshRendererComponent>(entity)) {
        auto& mesh = reg.get<dse::MeshRendererComponent>(entity);
        if (mesh.local_bounds_valid) {
            local_min = mesh.local_bounds_min;
            local_max = mesh.local_bounds_max;
        }
    }
    // Try SpriteRenderer for 2D bounds
    else if (reg.all_of<SpriteRendererComponent>(entity)) {
        local_min = glm::vec3(-0.5f, -0.5f, -0.01f);
        local_max = glm::vec3( 0.5f,  0.5f,  0.01f);
    }
    // Try BoxCollider3D
    else if (reg.all_of<dse::BoxCollider3DComponent>(entity)) {
        auto& col = reg.get<dse::BoxCollider3DComponent>(entity);
        glm::vec3 half = col.size * 0.5f;
        local_min = -half;
        local_max =  half;
    }
    // Try SphereCollider3D
    else if (reg.all_of<dse::SphereCollider3DComponent>(entity)) {
        auto& col = reg.get<dse::SphereCollider3DComponent>(entity);
        local_min = glm::vec3(-col.radius);
        local_max = glm::vec3( col.radius);
    }
    // Default: small box
    else {
        if (glm::length(tf.scale) < 0.02f) {
            local_min = glm::vec3(-0.25f);
            local_max = glm::vec3( 0.25f);
        }
    }

    const glm::mat4& m = tf.local_to_world;
    for (int i = 0; i < 8; ++i) {
        glm::vec3 corner(
            (i & 1) ? local_max.x : local_min.x,
            (i & 2) ? local_max.y : local_min.y,
            (i & 4) ? local_max.z : local_min.z);
        out_corners[i] = glm::vec3(m * glm::vec4(corner, 1.0f));
    }
    return true;
}

void DrawWireOBB(ImDrawList* dl,
                  const glm::vec3 corners[8],
                  const glm::mat4& view,
                  const glm::mat4& proj,
                  const glm::vec2& win_pos,
                  const glm::vec2& panel_size,
                  ImU32 color,
                  float thickness) {
    // corners order: 0=(-,-,-) 1=(+,-,-) 2=(-,+,-) 3=(+,+,-)
    //                4=(-,-,+) 5=(+,-,+) 6=(-,+,+) 7=(+,+,+)
    ImVec2 p[8];
    for (int i = 0; i < 8; i++)
        p[i] = WorldToScreen(corners[i], view, proj, win_pos, panel_size);
    static const int edges[12][2] = {
        {0,1},{2,3},{4,5},{6,7},  // X edges
        {0,2},{1,3},{4,6},{5,7},  // Y edges
        {0,4},{1,5},{2,6},{3,7},  // Z edges
    };
    for (auto& e : edges) dl->AddLine(p[e[0]], p[e[1]], color, thickness);
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

        glm::vec3 corners[8];
        if (!GetEntityOBBCorners(ctx.registry, entity, corners)) continue;

        bool is_primary = (entity == selection.GetPrimary());
        ImU32 color = is_primary ? IM_COL32(255, 165, 0, 220) : IM_COL32(100, 180, 255, 180);
        float thickness = is_primary ? 2.0f : 1.5f;

        DrawWireOBB(dl, corners, view, proj,
                    window_pos, panel_size, color, thickness);

        // Label for primary selection
        if (is_primary && ctx.registry.all_of<EditorNameComponent>(entity)) {
            auto& name = ctx.registry.get<EditorNameComponent>(entity);
            // Compute top-center from OBB corners
            glm::vec3 center(0.0f);
            float max_y = -1e30f;
            for (int ci = 0; ci < 8; ++ci) {
                center += corners[ci];
                max_y = std::max(max_y, corners[ci].y);
            }
            center /= 8.0f;
            glm::vec3 label_pos = glm::vec3(center.x, max_y + 0.2f, center.z);
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
