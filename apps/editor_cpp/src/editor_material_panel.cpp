#include "editor_material_panel.h"

#include "engine/ecs/components_3d.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstring>

namespace dse::editor {

void DrawMaterialPanel(entt::registry& registry, entt::entity selected_entity) {
    ImGui::Begin("Material");

    bool has_mesh = (selected_entity != entt::null &&
                     registry.valid(selected_entity) &&
                     registry.all_of<dse::MeshRendererComponent>(selected_entity));

    if (!has_mesh) {
        ImGui::TextDisabled("Select an entity with MeshRendererComponent to edit its material.");
        ImGui::End();
        return;
    }

    auto& mesh = registry.get<dse::MeshRendererComponent>(selected_entity);

    // Material preview sphere (approximate with ImDrawList)
    {
        ImVec2 preview_pos = ImGui::GetCursorScreenPos();
        const float sphere_radius = 32.0f;
        ImVec2 center(preview_pos.x + sphere_radius + 8, preview_pos.y + sphere_radius + 4);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Draw a shaded circle to approximate PBR sphere preview
        ImU32 base_color = ImGui::ColorConvertFloat4ToU32(
            ImVec4(mesh.color.r, mesh.color.g, mesh.color.b, mesh.color.a));
        dl->AddCircleFilled(center, sphere_radius, base_color, 48);

        // Specular highlight (approximation based on roughness)
        float highlight_size = sphere_radius * (1.0f - mesh.roughness) * 0.4f;
        if (highlight_size > 2.0f) {
            ImVec2 highlight_center(center.x - sphere_radius * 0.25f, center.y - sphere_radius * 0.25f);
            ImU32 highlight_color = IM_COL32(255, 255, 255, static_cast<int>((1.0f - mesh.roughness) * 180));
            dl->AddCircleFilled(highlight_center, highlight_size, highlight_color, 24);
        }

        // Metallic rim darkening
        dl->AddCircle(center, sphere_radius, IM_COL32(0, 0, 0, static_cast<int>(mesh.metallic * 120)), 48, 2.0f);

        ImGui::Dummy(ImVec2(sphere_radius * 2 + 16, sphere_radius * 2 + 8));
    }

    ImGui::Separator();
    ImGui::Text(MDI_ICON_PALETTE "  PBR Material Properties");
    ImGui::Spacing();

    // Albedo color
    float albedo[4] = { mesh.color.r, mesh.color.g, mesh.color.b, mesh.color.a };
    if (ImGui::ColorEdit4("Albedo", albedo)) {
        mesh.color = glm::vec4(albedo[0], albedo[1], albedo[2], albedo[3]);
    }

    // Metallic
    ImGui::SliderFloat("Metallic", &mesh.metallic, 0.0f, 1.0f, "%.2f");

    // Roughness
    ImGui::SliderFloat("Roughness", &mesh.roughness, 0.0f, 1.0f, "%.2f");

    // AO
    ImGui::SliderFloat("Ambient Occlusion", &mesh.ao, 0.0f, 1.0f, "%.2f");

    // Normal strength
    ImGui::SliderFloat("Normal Strength", &mesh.normal_strength, 0.0f, 2.0f, "%.2f");

    // Emissive
    float emissive[3] = { mesh.emissive.r, mesh.emissive.g, mesh.emissive.b };
    if (ImGui::ColorEdit3("Emissive", emissive)) {
        mesh.emissive = glm::vec3(emissive[0], emissive[1], emissive[2]);
    }

    ImGui::Separator();
    ImGui::Text(MDI_ICON_IMAGE "  Texture Slots");
    ImGui::Spacing();

    // Texture slots with drag-drop support
    auto DrawTextureSlot = [](const char* label, unsigned int& texture_handle) {
        ImGui::PushID(label);
        ImGui::Text("%s", label);
        ImGui::SameLine(140);

        char buf[64];
        if (texture_handle != 0) {
            snprintf(buf, sizeof(buf), "Texture #%u", texture_handle);
        } else {
            snprintf(buf, sizeof(buf), "(None)");
        }

        ImGui::Button(buf, ImVec2(ImGui::GetContentRegionAvail().x - 30, 0));

        // Accept drag-drop from Project panel
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                // In a real implementation, we'd load the texture here
                // For now, just indicate a texture was assigned
                (void)payload;
                texture_handle = 1; // placeholder handle
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        if (texture_handle != 0) {
            if (ImGui::SmallButton("X")) {
                texture_handle = 0;
            }
        } else {
            ImGui::TextDisabled("X");
        }

        ImGui::PopID();
    };

    DrawTextureSlot("Albedo Map", mesh.albedo_texture_handle);
    DrawTextureSlot("Normal Map", mesh.normal_texture_handle);
    DrawTextureSlot("Metallic/Roughness", mesh.metallic_roughness_texture_handle);
    DrawTextureSlot("Emissive Map", mesh.emissive_texture_handle);
    DrawTextureSlot("Occlusion Map", mesh.occlusion_texture_handle);

    ImGui::Separator();
    ImGui::Text(MDI_ICON_COG "  Render Options");
    ImGui::Spacing();

    // Shader variant
    const char* shader_variants[] = { "MESH_UNLIT", "MESH_LIT", "MESH_PBR" };
    int current_variant = 0;
    if (mesh.shader_variant == "MESH_LIT") current_variant = 1;
    else if (mesh.shader_variant == "MESH_PBR") current_variant = 2;
    if (ImGui::Combo("Shader", &current_variant, shader_variants, 3)) {
        mesh.shader_variant = shader_variants[current_variant];
    }

    ImGui::Checkbox("Alpha Test", &mesh.material_alpha_test);
    if (mesh.material_alpha_test) {
        ImGui::SliderFloat("Alpha Cutoff", &mesh.material_alpha_cutoff, 0.0f, 1.0f);
    }
    ImGui::Checkbox("Double Sided", &mesh.material_double_sided);
    ImGui::Checkbox("Receive Shadow", &mesh.receive_shadow);
    ImGui::Checkbox("Depth Test", &mesh.depth_test_enabled);
    ImGui::Checkbox("Depth Write", &mesh.depth_write_enabled);
    ImGui::Checkbox("Visible", &mesh.visible);

    ImGui::End();
}

} // namespace dse::editor
