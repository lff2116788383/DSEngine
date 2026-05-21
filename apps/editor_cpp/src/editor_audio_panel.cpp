#include "editor_audio_panel.h"

#include "engine/ecs/audio.h"
#include "engine/ecs/components_2d.h"
#include "imgui.h"
#include "editor_icons.h"
#include "editor_undo.h"
#include "editor_shortcuts.h"
#include "editor_scene_tabs.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dse::editor {

namespace {

#define AUDIO_PROPERTY(label, code) \
    ImGui::AlignTextToFramePadding(); \
    ImGui::Text(label); \
    ImGui::NextColumn(); \
    ImGui::SetNextItemWidth(-1); \
    code; \
    ImGui::NextColumn();

} // namespace

void DrawAudioSection(entt::registry& registry, entt::entity selected_entity) {
    if (selected_entity == entt::null || !registry.valid(selected_entity)) return;

    // --- Audio Source Component ---
    if (registry.all_of<AudioSourceComponent>(selected_entity)) {
        auto& audio = registry.get<AudioSourceComponent>(selected_entity);
        if (ImGui::CollapsingHeader(MDI_ICON_PLAY "  Audio Source", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "audio_src_cols", false);
            ImGui::SetColumnWidth(0, 110.0f);

            // Play / Pause / Stop buttons
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Preview");
            ImGui::NextColumn();
            {
                float button_w = (ImGui::GetContentRegionAvail().x - 8.0f) / 3.0f;
                if (audio.is_playing) {
                    if (ImGui::Button(MDI_ICON_PAUSE " Pause", ImVec2(button_w, 0))) {
                        audio.is_playing = false;
                    }
                } else {
                    if (ImGui::Button(MDI_ICON_PLAY " Play", ImVec2(button_w, 0))) {
                        audio.is_playing = true;
                        audio.restart_requested = true;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(MDI_ICON_STOP " Stop", ImVec2(button_w, 0))) {
                    audio.is_playing = false;
                    audio.restart_requested = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Restart", ImVec2(button_w, 0))) {
                    audio.restart_requested = true;
                    audio.is_playing = true;
                }
            }
            ImGui::NextColumn();

            auto& undo_mgr = GetUndoRedoManager();
            auto& tab_mgr = SceneTabManager::Get();
            auto entity = selected_entity;
            auto* reg = &registry;

            auto UndoAudioFloat = [&](const char* desc, float& field, float old_val) {
                float new_val = field;
                undo_mgr.Execute(std::make_unique<PropertyChangeCommand<float>>(
                    desc, old_val, new_val,
                    [reg, entity, offset = (size_t)((char*)&field - (char*)&audio)](const float& v) {
                        if (reg->valid(entity) && reg->all_of<AudioSourceComponent>(entity))
                            *reinterpret_cast<float*>(reinterpret_cast<char*>(&reg->get<AudioSourceComponent>(entity)) + offset) = v;
                    }
                ), true);
                tab_mgr.MarkDirty();
            };
            auto UndoAudioBool = [&](const char* desc, bool& field, bool old_val) {
                bool new_val = field;
                undo_mgr.Execute(std::make_unique<PropertyChangeCommand<bool>>(
                    desc, old_val, new_val,
                    [reg, entity, offset = (size_t)((char*)&field - (char*)&audio)](const bool& v) {
                        if (reg->valid(entity) && reg->all_of<AudioSourceComponent>(entity))
                            *reinterpret_cast<bool*>(reinterpret_cast<char*>(&reg->get<AudioSourceComponent>(entity)) + offset) = v;
                    }
                ), false);
                tab_mgr.MarkDirty();
            };

            { bool old_v = audio.play_on_awake;
            AUDIO_PROPERTY("Play On Awake", if (ImGui::Checkbox("##play_on_awake", &audio.play_on_awake)) UndoAudioBool("Audio Play On Awake", audio.play_on_awake, old_v)); }
            { bool old_v = audio.loop;
            AUDIO_PROPERTY("Loop", if (ImGui::Checkbox("##audio_loop", &audio.loop)) UndoAudioBool("Audio Loop", audio.loop, old_v)); }
            { float old_v = audio.volume;
            AUDIO_PROPERTY("Volume", if (ImGui::SliderFloat("##audio_volume", &audio.volume, 0.0f, 1.0f, "%.2f")) UndoAudioFloat("Audio Volume", audio.volume, old_v)); }
            { float old_v = audio.pitch;
            AUDIO_PROPERTY("Pitch", if (ImGui::DragFloat("##audio_pitch", &audio.pitch, 0.01f, 0.1f, 3.0f, "%.2f")) UndoAudioFloat("Audio Pitch", audio.pitch, old_v)); }

            ImGui::Separator();
            ImGui::Text("3D Spatial Audio");
            ImGui::NextColumn(); ImGui::NextColumn();

            { bool old_v = audio.spatial_enabled;
            AUDIO_PROPERTY("Spatial", if (ImGui::Checkbox("##spatial_enabled", &audio.spatial_enabled)) UndoAudioBool("Audio Spatial", audio.spatial_enabled, old_v)); }

            if (audio.spatial_enabled) {
                { float old_v = audio.min_distance;
                AUDIO_PROPERTY("Min Distance", if (ImGui::DragFloat("##audio_min_dist", &audio.min_distance, 0.1f, 0.0f, audio.max_distance, "%.1f")) UndoAudioFloat("Audio Min Distance", audio.min_distance, old_v)); }
                { float old_v = audio.max_distance;
                AUDIO_PROPERTY("Max Distance", if (ImGui::DragFloat("##audio_max_dist", &audio.max_distance, 0.5f, audio.min_distance, 1000.0f, "%.1f")) UndoAudioFloat("Audio Max Distance", audio.max_distance, old_v)); }
                { float old_v = audio.rolloff;
                AUDIO_PROPERTY("Rolloff", if (ImGui::DragFloat("##audio_rolloff", &audio.rolloff, 0.05f, 0.0f, 10.0f, "%.2f")) UndoAudioFloat("Audio Rolloff", audio.rolloff, old_v)); }

                const char* attenuation_types[] = { "Inverse", "Linear", "Exponential" };
                int current_atten = static_cast<int>(audio.attenuation_model);
                AUDIO_PROPERTY("Attenuation", if (ImGui::Combo("##audio_atten", &current_atten, attenuation_types, IM_ARRAYSIZE(attenuation_types))) {
                    int old_atten = static_cast<int>(audio.attenuation_model);
                    audio.attenuation_model = static_cast<AudioAttenuationModel>(current_atten);
                    int new_atten = current_atten;
                    undo_mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
                        "Audio Attenuation", old_atten, new_atten,
                        [reg, entity](const int& v) {
                            if (reg->valid(entity) && reg->all_of<AudioSourceComponent>(entity))
                                reg->get<AudioSourceComponent>(entity).attenuation_model = static_cast<AudioAttenuationModel>(v);
                        }
                    ), false);
                    tab_mgr.MarkDirty();
                });

                ImGui::Separator();
                ImGui::Text("Occlusion");
                ImGui::NextColumn(); ImGui::NextColumn();
                { bool old_v = audio.occlusion_enabled;
                AUDIO_PROPERTY("Occlusion", if (ImGui::Checkbox("##audio_occlusion", &audio.occlusion_enabled)) UndoAudioBool("Audio Occlusion", audio.occlusion_enabled, old_v)); }
                if (audio.occlusion_enabled) {
                    { float old_v = audio.occlusion_factor;
                    AUDIO_PROPERTY("Occ Factor", if (ImGui::SliderFloat("##audio_occ_factor", &audio.occlusion_factor, 0.0f, 1.0f, "%.2f")) UndoAudioFloat("Audio Occlusion Factor", audio.occlusion_factor, old_v)); }
                }
            }

            ImGui::Columns(1);
        }
    }

    // --- Audio Listener Component ---
    if (registry.all_of<AudioListenerComponent>(selected_entity)) {
        auto& listener = registry.get<AudioListenerComponent>(selected_entity);
        if (ImGui::CollapsingHeader(MDI_ICON_EYE "  Audio Listener", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "audio_listener_cols", false);
            ImGui::SetColumnWidth(0, 110.0f);
            AUDIO_PROPERTY("Enabled", ImGui::Checkbox("##listener_enabled", &listener.enabled));
            ImGui::Columns(1);
        }
    }
}

void DrawAudioRangeOverlay(entt::registry& registry,
                           entt::entity selected_entity,
                           const glm::vec2& viewport_pos,
                           const glm::vec2& viewport_size,
                           const glm::mat4& view,
                           const glm::mat4& proj) {
    if (selected_entity == entt::null || !registry.valid(selected_entity)) return;
    if (!registry.all_of<AudioSourceComponent>(selected_entity)) return;
    if (!registry.all_of<TransformComponent>(selected_entity)) return;

    const auto& audio = registry.get<AudioSourceComponent>(selected_entity);
    if (!audio.spatial_enabled) return;

    const auto& transform = registry.get<TransformComponent>(selected_entity);
    const glm::vec3 center = transform.position;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const glm::mat4 vp = proj * view;

    // Project world point to screen
    auto project_to_screen = [&](const glm::vec3& world_pos) -> glm::vec2 {
        glm::vec4 clip = vp * glm::vec4(world_pos, 1.0f);
        if (clip.w <= 0.001f) return glm::vec2(-10000.0f);
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = viewport_pos.x + (ndc.x * 0.5f + 0.5f) * viewport_size.x;
        float sy = viewport_pos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_size.y;
        return glm::vec2(sx, sy);
    };

    // Draw a circle in 3D (approximated by segments projected to screen)
    auto draw_circle_3d = [&](float radius, ImU32 color, float thickness) {
        constexpr int segments = 48;
        glm::vec2 prev = project_to_screen(center + glm::vec3(radius, 0.0f, 0.0f));
        for (int i = 1; i <= segments; ++i) {
            float angle = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * static_cast<float>(M_PI);
            glm::vec3 world_pt = center + glm::vec3(radius * std::cos(angle), 0.0f, radius * std::sin(angle));
            glm::vec2 screen_pt = project_to_screen(world_pt);
            // Only draw if both points are on screen
            if (prev.x > -5000.0f && screen_pt.x > -5000.0f) {
                draw_list->AddLine(ImVec2(prev.x, prev.y), ImVec2(screen_pt.x, screen_pt.y), color, thickness);
            }
            prev = screen_pt;
        }
    };

    // Min distance: green circle
    draw_circle_3d(audio.min_distance, IM_COL32(50, 220, 50, 180), 2.0f);
    // Max distance: red circle
    draw_circle_3d(audio.max_distance, IM_COL32(220, 50, 50, 140), 1.5f);

    // Draw center point
    glm::vec2 center_screen = project_to_screen(center);
    if (center_screen.x > -5000.0f) {
        draw_list->AddCircleFilled(ImVec2(center_screen.x, center_screen.y), 4.0f, IM_COL32(255, 200, 0, 220));
    }

    // Labels
    glm::vec2 min_label = project_to_screen(center + glm::vec3(audio.min_distance, 0.0f, 0.0f));
    if (min_label.x > -5000.0f) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Min: %.1f", audio.min_distance);
        draw_list->AddText(ImVec2(min_label.x + 4, min_label.y - 14), IM_COL32(50, 220, 50, 220), buf);
    }
    glm::vec2 max_label = project_to_screen(center + glm::vec3(audio.max_distance, 0.0f, 0.0f));
    if (max_label.x > -5000.0f) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Max: %.1f", audio.max_distance);
        draw_list->AddText(ImVec2(max_label.x + 4, max_label.y - 14), IM_COL32(220, 50, 50, 220), buf);
    }
}

} // namespace dse::editor
