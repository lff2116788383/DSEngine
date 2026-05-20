#include "editor_multi_viewport.h"
#include "editor_icons.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace dse::editor {

MultiViewportState& GetMultiViewportState() {
    static MultiViewportState state = []() {
        MultiViewportState s;
        s.cameras[0] = {"Perspective", {0, 8, -15}, {0, 0, 0}, 60.0f, true, false, 10.0f, 0};
        s.cameras[1] = {"Top",         {0, 20, 0},  {0, 0, 0}, 60.0f, false, true, 20.0f, 0};
        s.cameras[2] = {"Front",       {0, 5, -20}, {0, 5, 0}, 60.0f, false, true, 15.0f, 0};
        s.cameras[3] = {"Right",       {20, 5, 0},  {0, 5, 0}, 60.0f, false, true, 15.0f, 0};
        return s;
    }();
    return state;
}

void GetViewportCameraMatrices(int camera_index,
                                float aspect,
                                glm::mat4& out_view,
                                glm::mat4& out_proj) {
    auto& mvs = GetMultiViewportState();
    auto& cam = mvs.cameras[camera_index % 4];

    out_view = glm::lookAt(cam.position, cam.target, glm::vec3(0, 1, 0));

    if (cam.ortho) {
        float half_h = cam.ortho_size * 0.5f;
        float half_w = half_h * aspect;
        out_proj = glm::ortho(-half_w, half_w, -half_h, half_h, 0.1f, 1000.0f);
    } else {
        out_proj = glm::perspective(glm::radians(cam.fov), aspect, 0.1f, 1000.0f);
    }
}

void DrawMultiViewportConfigPanel() {
    ImGui::Begin("Multi-Viewport");

    auto& mvs = GetMultiViewportState();

    ImGui::Checkbox("Enable Multi-Viewport", &mvs.enabled);

    if (!mvs.enabled) {
        ImGui::TextDisabled("Multi-Viewport is disabled. Enable to split the scene view.");
        ImGui::End();
        return;
    }

    // Layout selection
    const char* layouts[] = { "Single", "Side-by-Side", "Top-Bottom", "Quad" };
    ImGui::Combo("Layout", &mvs.layout, layouts, IM_ARRAYSIZE(layouts));

    int cam_count = 1;
    if (mvs.layout == 1 || mvs.layout == 2) cam_count = 2;
    else if (mvs.layout == 3) cam_count = 4;

    ImGui::Separator();

    for (int i = 0; i < cam_count; i++) {
        auto& cam = mvs.cameras[i];
        ImGui::PushID(i);

        bool open = ImGui::CollapsingHeader(cam.name.c_str(),
                                             (i == mvs.active_camera) ? ImGuiTreeNodeFlags_DefaultOpen : 0);

        if (open) {
            char name_buf[64];
            strncpy_s(name_buf, sizeof(name_buf), cam.name.c_str(), _TRUNCATE);
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                cam.name = name_buf;
            }

            ImGui::Checkbox("Orthographic", &cam.ortho);
            if (cam.ortho) {
                ImGui::DragFloat("Ortho Size", &cam.ortho_size, 0.5f, 1.0f, 200.0f);
            } else {
                ImGui::SliderFloat("FOV", &cam.fov, 10.0f, 120.0f, "%.0f");
            }

            ImGui::DragFloat3("Position", &cam.position.x, 0.1f);
            ImGui::DragFloat3("Target", &cam.target.x, 0.1f);

            const char* modes[] = { "Shaded", "Wireframe", "Unlit" };
            ImGui::Combo("Render Mode", &cam.render_mode, modes, IM_ARRAYSIZE(modes));

            // Preset buttons
            if (ImGui::SmallButton("Top")) {
                cam.position = {0, 20, 0.01f}; cam.target = {0, 0, 0}; cam.ortho = true; cam.name = "Top";
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Front")) {
                cam.position = {0, 5, -20}; cam.target = {0, 5, 0}; cam.ortho = true; cam.name = "Front";
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Right")) {
                cam.position = {20, 5, 0}; cam.target = {0, 5, 0}; cam.ortho = true; cam.name = "Right";
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Persp")) {
                cam.position = {0, 8, -15}; cam.target = {0, 0, 0}; cam.ortho = false; cam.name = "Perspective";
            }
        }

        ImGui::PopID();
    }

    // Preview layout
    ImGui::Separator();
    ImGui::Text("Layout Preview:");
    ImVec2 preview_pos = ImGui::GetCursorScreenPos();
    float pw = std::min(ImGui::GetContentRegionAvail().x, 200.0f);
    float ph = pw * 0.6f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(preview_pos, ImVec2(preview_pos.x + pw, preview_pos.y + ph),
                       IM_COL32(30, 30, 40, 255), 4.0f);

    if (mvs.layout == 0) {
        // Single viewport
        dl->AddRect(preview_pos, ImVec2(preview_pos.x + pw, preview_pos.y + ph),
                    IM_COL32(100, 200, 255, 200), 4.0f, 0, 2.0f);
        ImVec2 center(preview_pos.x + pw * 0.5f, preview_pos.y + ph * 0.5f);
        dl->AddText(ImVec2(center.x - 10, center.y - 6), IM_COL32(200, 200, 200, 255), "1");
    } else if (mvs.layout == 1) {
        // Side by side
        float half = pw * 0.5f;
        dl->AddRect(preview_pos, ImVec2(preview_pos.x + half - 1, preview_pos.y + ph),
                    IM_COL32(100, 200, 255, 200), 4.0f, 0, 2.0f);
        dl->AddRect(ImVec2(preview_pos.x + half + 1, preview_pos.y),
                    ImVec2(preview_pos.x + pw, preview_pos.y + ph),
                    IM_COL32(255, 200, 100, 200), 4.0f, 0, 2.0f);
    } else if (mvs.layout == 2) {
        // Top-bottom
        float half = ph * 0.5f;
        dl->AddRect(preview_pos, ImVec2(preview_pos.x + pw, preview_pos.y + half - 1),
                    IM_COL32(100, 200, 255, 200), 4.0f, 0, 2.0f);
        dl->AddRect(ImVec2(preview_pos.x, preview_pos.y + half + 1),
                    ImVec2(preview_pos.x + pw, preview_pos.y + ph),
                    IM_COL32(255, 200, 100, 200), 4.0f, 0, 2.0f);
    } else if (mvs.layout == 3) {
        // Quad
        float hw = pw * 0.5f, hh = ph * 0.5f;
        ImU32 colors[] = {
            IM_COL32(100, 200, 255, 200), IM_COL32(255, 200, 100, 200),
            IM_COL32(100, 255, 150, 200), IM_COL32(255, 150, 200, 200)
        };
        for (int q = 0; q < 4; q++) {
            float x0 = preview_pos.x + (q % 2) * hw + ((q % 2) ? 1 : 0);
            float y0 = preview_pos.y + (q / 2) * hh + ((q / 2) ? 1 : 0);
            float x1 = x0 + hw - ((q % 2) ? 0 : 1);
            float y1 = y0 + hh - ((q / 2) ? 0 : 1);
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), colors[q], 2.0f, 0, 2.0f);
            char lbl[8]; snprintf(lbl, sizeof(lbl), "%d", q + 1);
            dl->AddText(ImVec2((x0 + x1) * 0.5f - 3, (y0 + y1) * 0.5f - 6),
                        IM_COL32(200, 200, 200, 200), lbl);
        }
    }

    ImGui::Dummy(ImVec2(pw, ph));

    ImGui::End();
}

} // namespace dse::editor
