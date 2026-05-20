#include "editor_lighting_gizmos.h"
#include "editor_context.h"
#include "editor_icons.h"
#include "editor_overlay_utils.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>

using namespace dse::editor::overlay;

namespace dse::editor {

bool& GetLightingGizmosEnabled() {
    static bool enabled = true;
    return enabled;
}

namespace {

void DrawDirectionalLightArrow(ImDrawList* dl,
                                const glm::vec3& pos, const glm::vec3& dir,
                                float length,
                                const glm::mat4& view, const glm::mat4& proj,
                                const glm::vec2& wp, const glm::vec2& ps,
                                ImU32 color) {
    glm::vec3 end = pos + glm::normalize(dir) * length;
    ImVec2 sp = WorldToScreen(pos, view, proj, wp, ps);
    ImVec2 ep = WorldToScreen(end, view, proj, wp, ps);
    dl->AddLine(sp, ep, color, 2.0f);
    // Arrowhead
    float dx = ep.x - sp.x, dy = ep.y - sp.y;
    float len = std::sqrt(dx*dx + dy*dy);
    if (len > 0.01f) {
        dx /= len; dy /= len;
        float ax = -dx * 8 - dy * 4;
        float ay = -dy * 8 + dx * 4;
        float bx = -dx * 8 + dy * 4;
        float by = -dy * 8 - dx * 4;
        dl->AddTriangleFilled(ep, ImVec2(ep.x + ax, ep.y + ay), ImVec2(ep.x + bx, ep.y + by), color);
    }
}

} // namespace

void DrawLightingGizmos(EditorContext& ctx,
                         const glm::vec2& window_pos,
                         const glm::vec2& panel_size,
                         const glm::mat4& view,
                         const glm::mat4& proj) {
    if (!GetLightingGizmosEnabled()) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto& reg = ctx.registry;

    // Point Lights - sphere gizmo showing range
    {
        auto v = reg.view<TransformComponent, dse::PointLightComponent>();
        for (auto e : v) {
            auto& tf = v.get<TransformComponent>(e);
            auto& light = v.get<dse::PointLightComponent>(e);
            ImU32 col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(light.color.r, light.color.g, light.color.b, 0.5f));
            DrawWireSphere(dl, tf.position, light.radius, view, proj,
                          window_pos, panel_size, col, 1.0f);
            // Center marker
            ImVec2 sp = WorldToScreen(tf.position, view, proj, window_pos, panel_size);
            dl->AddCircleFilled(sp, 4.0f, col);
            DrawLabel3D(dl, MDI_ICON_LIGHTBULB " Point", tf.position + glm::vec3(0, 0.5f, 0),
                       view, proj, window_pos, panel_size, col);
        }
    }

    // Spot Lights - cone gizmo
    {
        auto v = reg.view<TransformComponent, dse::SpotLightComponent>();
        for (auto e : v) {
            auto& tf = v.get<TransformComponent>(e);
            auto& light = v.get<dse::SpotLightComponent>(e);
            ImU32 col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(light.color.r, light.color.g, light.color.b, 0.5f));
            ImVec2 sp = WorldToScreen(tf.position, view, proj, window_pos, panel_size);
            dl->AddCircleFilled(sp, 4.0f, col);

            // Cone visualization using 4 lines
            float cone_len = light.radius * 0.5f;
            float cone_radius = cone_len * std::tan(light.outer_cone_angle * kPi / 180.0f);
            glm::vec3 dir = glm::normalize(light.direction);
            glm::vec3 cone_tip = tf.position;
            glm::vec3 cone_center = cone_tip + dir * cone_len;

            // Find perpendicular axes
            glm::vec3 up(0, 1, 0);
            if (std::abs(glm::dot(dir, up)) > 0.99f) up = glm::vec3(1, 0, 0);
            glm::vec3 right = glm::normalize(glm::cross(dir, up));
            glm::vec3 up2 = glm::cross(right, dir);

            for (int i = 0; i < 8; i++) {
                float a = static_cast<float>(i) / 8.0f * k2Pi;
                glm::vec3 rim = cone_center + (right * std::cos(a) + up2 * std::sin(a)) * cone_radius;
                ImVec2 rp = WorldToScreen(rim, view, proj, window_pos, panel_size);
                dl->AddLine(sp, rp, col, 1.0f);
            }
            DrawWireCircle(dl, cone_center, cone_radius, 1, view, proj, window_pos, panel_size, col, 1.0f);
            DrawLabel3D(dl, MDI_ICON_SPOTLIGHT_BEAM " Spot", tf.position + glm::vec3(0, 0.5f, 0),
                       view, proj, window_pos, panel_size, col);
        }
    }

    // Directional Lights - arrow
    {
        auto v = reg.view<TransformComponent, dse::DirectionalLight3DComponent>();
        for (auto e : v) {
            auto& tf = v.get<TransformComponent>(e);
            auto& light = v.get<dse::DirectionalLight3DComponent>(e);
            ImU32 col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(light.color.r, light.color.g, light.color.b, 0.7f));
            DrawDirectionalLightArrow(dl, tf.position, light.direction, 3.0f,
                                      view, proj, window_pos, panel_size, col);
            DrawLabel3D(dl, MDI_ICON_WHITE_BALANCE_SUNNY " Dir", tf.position + glm::vec3(0, 0.5f, 0),
                       view, proj, window_pos, panel_size, col);
        }
    }

    // Light Probes - sphere gizmo with SH preview
    {
        auto v = reg.view<TransformComponent, dse::LightProbeComponent>();
        for (auto e : v) {
            auto& tf = v.get<TransformComponent>(e);
            auto& probe = v.get<dse::LightProbeComponent>(e);
            if (!probe.enabled) continue;
            ImU32 col = probe.needs_rebake
                ? IM_COL32(255, 150, 50, 120)
                : IM_COL32(100, 255, 150, 120);
            DrawWireSphere(dl, tf.position, probe.influence_radius, view, proj,
                          window_pos, panel_size, col, 1.0f);
            // Center icon
            ImVec2 sp = WorldToScreen(tf.position, view, proj, window_pos, panel_size);
            dl->AddCircleFilled(sp, 6.0f, col);
            dl->AddCircle(sp, 6.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
            DrawLabel3D(dl, probe.needs_rebake ? "LP (stale)" : "LP",
                       tf.position + glm::vec3(0, 0.3f, 0),
                       view, proj, window_pos, panel_size, col);
        }
    }

    // Reflection Probes - box or sphere gizmo
    {
        auto v = reg.view<TransformComponent, dse::ReflectionProbeComponent>();
        for (auto e : v) {
            auto& tf = v.get<TransformComponent>(e);
            auto& probe = v.get<dse::ReflectionProbeComponent>(e);
            if (!probe.enabled) continue;
            ImU32 col = probe.needs_rebake
                ? IM_COL32(255, 180, 80, 120)
                : IM_COL32(80, 180, 255, 120);

            if (probe.use_box_projection) {
                glm::vec3 half(probe.box_size_x * 0.5f, probe.box_size_y * 0.5f, probe.box_size_z * 0.5f);
                DrawWireBox(dl, tf.position, half, view, proj, window_pos, panel_size, col, 1.5f);
            } else {
                DrawWireSphere(dl, tf.position, probe.influence_radius, view, proj,
                              window_pos, panel_size, col, 1.5f);
            }
            ImVec2 sp = WorldToScreen(tf.position, view, proj, window_pos, panel_size);
            dl->AddRectFilled(ImVec2(sp.x - 5, sp.y - 5), ImVec2(sp.x + 5, sp.y + 5), col, 2.0f);
            DrawLabel3D(dl, probe.needs_rebake ? "RP (stale)" : "RP",
                       tf.position + glm::vec3(0, 0.3f, 0),
                       view, proj, window_pos, panel_size, col);
        }
    }
}

} // namespace dse::editor
