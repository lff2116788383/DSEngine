#include "editor_physics_debug.h"
#include "editor_context.h"
#include "editor_icons.h"
#include "editor_overlay_utils.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>

using namespace dse::editor::overlay;

namespace dse::editor {

bool& GetPhysicsDebugEnabled() {
    static bool enabled = false;
    return enabled;
}

namespace {

void DrawWireCapsule(ImDrawList* dl,
                     const glm::vec3& center, float radius, float half_height,
                     const glm::mat4& view, const glm::mat4& proj,
                     const glm::vec2& wp, const glm::vec2& ps,
                     ImU32 color, float thick) {
    // Two circles at top and bottom of cylinder
    glm::vec3 top = center + glm::vec3(0, half_height, 0);
    glm::vec3 bot = center - glm::vec3(0, half_height, 0);
    DrawWireCircle(dl, top, radius, 1, view, proj, wp, ps, color, thick);
    DrawWireCircle(dl, bot, radius, 1, view, proj, wp, ps, color, thick);

    // 4 vertical lines
    for (int i = 0; i < 4; i++) {
        float a = static_cast<float>(i) / 4.0f * k2Pi;
        float cx = std::cos(a) * radius;
        float cz = std::sin(a) * radius;
        ImVec2 p1 = WorldToScreen(top + glm::vec3(cx, 0, cz), view, proj, wp, ps);
        ImVec2 p2 = WorldToScreen(bot + glm::vec3(cx, 0, cz), view, proj, wp, ps);
        dl->AddLine(p1, p2, color, thick);
    }

    // Half-sphere arcs at top and bottom
    constexpr int hsegs = 16;
    for (int arc = 0; arc < 2; arc++) { // XY and ZY arcs
        ImVec2 top_pts[hsegs + 1];
        ImVec2 bot_pts[hsegs + 1];
        for (int i = 0; i <= hsegs; i++) {
            float a = static_cast<float>(i) / static_cast<float>(hsegs) * kPi;
            float h = std::cos(a) * radius;
            float r = std::sin(a) * radius;
            glm::vec3 tp = top, bp = bot;
            tp.y += h;
            bp.y -= h;
            if (arc == 0) { tp.x += r; bp.x += r; }
            else { tp.z += r; bp.z += r; }
            top_pts[i] = WorldToScreen(tp, view, proj, wp, ps);
            bot_pts[i] = WorldToScreen(bp, view, proj, wp, ps);
        }
        dl->AddPolyline(top_pts, hsegs + 1, color, ImDrawFlags_None, thick);
        dl->AddPolyline(bot_pts, hsegs + 1, color, ImDrawFlags_None, thick);
    }
}

} // namespace

void DrawPhysicsDebugOverlay(EditorContext& ctx,
                              const glm::vec2& window_pos,
                              const glm::vec2& panel_size,
                              const glm::mat4& view,
                              const glm::mat4& proj) {
    if (!GetPhysicsDebugEnabled()) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& reg = ctx.registry;
    const ImU32 col_box    = IM_COL32(0, 255, 0, 140);
    const ImU32 col_sphere = IM_COL32(0, 200, 255, 140);
    const ImU32 col_caps   = IM_COL32(255, 200, 0, 140);
    const ImU32 col_mesh   = IM_COL32(255, 100, 255, 140);
    const ImU32 col_trigger= IM_COL32(255, 255, 0, 100);
    const float thick = 1.5f;

    // 3D Box Colliders
    {
        auto view3 = reg.view<TransformComponent, dse::BoxCollider3DComponent>();
        for (auto e : view3) {
            auto& tf = view3.get<TransformComponent>(e);
            auto& col = view3.get<dse::BoxCollider3DComponent>(e);
            glm::vec3 center = tf.position + col.center * tf.scale;
            glm::vec3 half = col.size * 0.5f * tf.scale;
            ImU32 c = col.is_trigger ? col_trigger : col_box;
            DrawWireBox(dl, center, half, view, proj, window_pos, panel_size, c, thick);
        }
    }

    // 3D Sphere Colliders
    {
        auto view3 = reg.view<TransformComponent, dse::SphereCollider3DComponent>();
        for (auto e : view3) {
            auto& tf = view3.get<TransformComponent>(e);
            auto& col = view3.get<dse::SphereCollider3DComponent>(e);
            glm::vec3 center = tf.position + col.center * tf.scale;
            float max_s = std::max({tf.scale.x, tf.scale.y, tf.scale.z});
            float r = col.radius * max_s;
            ImU32 c = col.is_trigger ? col_trigger : col_sphere;
            DrawWireSphere(dl, center, r, view, proj, window_pos, panel_size, c, thick);
        }
    }

    // 3D Capsule Colliders
    {
        auto view3 = reg.view<TransformComponent, dse::CapsuleCollider3DComponent>();
        for (auto e : view3) {
            auto& tf = view3.get<TransformComponent>(e);
            auto& col = view3.get<dse::CapsuleCollider3DComponent>(e);
            glm::vec3 center = tf.position + col.center * tf.scale;
            float axis_scale = (col.direction == 0) ? tf.scale.x
                             : (col.direction == 2) ? tf.scale.z
                             : tf.scale.y;
            float perp_scale = (col.direction == 0) ? std::max(tf.scale.y, tf.scale.z)
                             : (col.direction == 2) ? std::max(tf.scale.x, tf.scale.y)
                             : std::max(tf.scale.x, tf.scale.z);
            float r = col.radius * perp_scale;
            float hh = col.height * 0.5f * axis_scale;
            ImU32 c = col.is_trigger ? col_trigger : col_caps;
            DrawWireCapsule(dl, center, r, hh, view, proj, window_pos, panel_size, c, thick);
        }
    }

    // 3D Mesh Colliders (just draw a box based on transform for visualization)
    {
        auto view3 = reg.view<TransformComponent, dse::MeshCollider3DComponent>();
        for (auto e : view3) {
            auto& tf = view3.get<TransformComponent>(e);
            auto& col = view3.get<dse::MeshCollider3DComponent>(e);
            glm::vec3 half = tf.scale * 0.5f;
            ImU32 c = col.is_trigger ? col_trigger : col_mesh;
            DrawWireBox(dl, tf.position, half, view, proj, window_pos, panel_size, c, thick);
        }
    }

    // 2D Box Colliders
    {
        auto view2 = reg.view<TransformComponent, BoxCollider2DComponent>();
        for (auto e : view2) {
            auto& tf = view2.get<TransformComponent>(e);
            auto& col = view2.get<BoxCollider2DComponent>(e);
            glm::vec3 center = tf.position + glm::vec3(col.offset.x, col.offset.y, 0.0f);
            glm::vec3 half(col.size.x * 0.5f * tf.scale.x,
                          col.size.y * 0.5f * tf.scale.y,
                          0.01f);
            DrawWireBox(dl, center, half, view, proj, window_pos, panel_size, col_box, thick);
        }
    }

    // 2D Circle Colliders
    {
        auto view2 = reg.view<TransformComponent, CircleCollider2DComponent>();
        for (auto e : view2) {
            auto& tf = view2.get<TransformComponent>(e);
            auto& col = view2.get<CircleCollider2DComponent>(e);
            glm::vec3 center = tf.position + glm::vec3(col.offset.x, col.offset.y, 0.0f);
            float max_s = std::max(tf.scale.x, tf.scale.y);
            DrawWireSphere(dl, center, col.radius * max_s, view, proj,
                          window_pos, panel_size, col_sphere, thick);
        }
    }
}

} // namespace dse::editor
