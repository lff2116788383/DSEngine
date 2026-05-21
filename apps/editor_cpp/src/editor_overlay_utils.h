#pragma once

#include "imgui.h"
#include <glm/glm.hpp>
#include <cmath>

namespace dse::editor::overlay {

constexpr float kPi  = 3.14159265358979323846f;
constexpr float k2Pi = kPi * 2.0f;

inline ImVec2 WorldToScreen(const glm::vec3& wp,
                             const glm::mat4& view,
                             const glm::mat4& proj,
                             const glm::vec2& win_pos,
                             const glm::vec2& panel_size) {
    glm::vec4 clip = proj * view * glm::vec4(wp, 1.0f);
    if (std::abs(clip.w) < 1e-6f) return ImVec2(-10000.0f, -10000.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return ImVec2(win_pos.x + (ndc.x + 1.0f) * 0.5f * panel_size.x,
                  win_pos.y + (1.0f - ndc.y) * 0.5f * panel_size.y);
}

inline void DrawWireCircle(ImDrawList* dl,
                            const glm::vec3& center, float radius,
                            int axis,
                            const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec2& wp, const glm::vec2& ps,
                            ImU32 color, float thick) {
    constexpr int segs = 32;
    ImVec2 pts[segs];
    for (int i = 0; i < segs; i++) {
        float a = static_cast<float>(i) / static_cast<float>(segs) * k2Pi;
        float ca = std::cos(a) * radius;
        float sa = std::sin(a) * radius;
        glm::vec3 p = center;
        if (axis == 0) { p.x += ca; p.y += sa; }
        else if (axis == 1) { p.x += ca; p.z += sa; }
        else { p.y += ca; p.z += sa; }
        pts[i] = WorldToScreen(p, view, proj, wp, ps);
    }
    dl->AddPolyline(pts, segs, color, ImDrawFlags_Closed, thick);
}

inline void DrawWireSphere(ImDrawList* dl,
                            const glm::vec3& center, float radius,
                            const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec2& wp, const glm::vec2& ps,
                            ImU32 color, float thick) {
    DrawWireCircle(dl, center, radius, 0, view, proj, wp, ps, color, thick);
    DrawWireCircle(dl, center, radius, 1, view, proj, wp, ps, color, thick);
    DrawWireCircle(dl, center, radius, 2, view, proj, wp, ps, color, thick);
}

inline void DrawWireBox(ImDrawList* dl,
                         const glm::vec3& center, const glm::vec3& half,
                         const glm::mat4& view, const glm::mat4& proj,
                         const glm::vec2& wp, const glm::vec2& ps,
                         ImU32 color, float thick) {
    glm::vec3 c[8] = {
        center + glm::vec3(-half.x, -half.y, -half.z),
        center + glm::vec3( half.x, -half.y, -half.z),
        center + glm::vec3( half.x,  half.y, -half.z),
        center + glm::vec3(-half.x,  half.y, -half.z),
        center + glm::vec3(-half.x, -half.y,  half.z),
        center + glm::vec3( half.x, -half.y,  half.z),
        center + glm::vec3( half.x,  half.y,  half.z),
        center + glm::vec3(-half.x,  half.y,  half.z),
    };
    ImVec2 p[8];
    for (int i = 0; i < 8; i++) p[i] = WorldToScreen(c[i], view, proj, wp, ps);
    static const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (auto& e : edges) dl->AddLine(p[e[0]], p[e[1]], color, thick);
}

inline void DrawLabel3D(ImDrawList* dl, const char* text,
                         const glm::vec3& pos,
                         const glm::mat4& view, const glm::mat4& proj,
                         const glm::vec2& wp, const glm::vec2& ps,
                         ImU32 color) {
    ImVec2 sp = WorldToScreen(pos, view, proj, wp, ps);
    if (sp.x < -5000) return;
    ImVec2 text_size = ImGui::CalcTextSize(text);
    ImVec2 bg_min(sp.x - text_size.x * 0.5f - 3.0f, sp.y - text_size.y - 1.0f);
    ImVec2 bg_max(sp.x + text_size.x * 0.5f + 3.0f, sp.y + 2.0f);
    dl->AddRectFilled(bg_min, bg_max, IM_COL32(0, 0, 0, 140), 2.0f);
    dl->AddText(ImVec2(sp.x - text_size.x * 0.5f, sp.y - text_size.y), color, text);
}

} // namespace dse::editor::overlay
