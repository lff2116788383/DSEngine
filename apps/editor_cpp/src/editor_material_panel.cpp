#include "editor_material_panel.h"

#include "engine/ecs/components_3d.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_undo.h"
#include "editor_shortcuts.h"
#include "engine/runtime/engine_app.h"
#include "editor_scene_tabs.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace dse::editor {

void DrawMaterialPanel(EditorContext& ctx) {
    auto& registry = ctx.registry;
    auto selected_entity = ctx.selected_entity;
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

    // ─── Real-time PBR Preview Sphere (software-rendered per-pixel) ─────
    {
        static float s_preview_rot_y = 0.0f;
        static float s_preview_rot_x = 0.3f;
        static bool s_auto_rotate = true;
        static float s_light_dir[3] = {0.5f, 0.7f, 0.5f};
        static float s_env_intensity = 0.3f;

        ImGui::Text(MDI_ICON_SPHERE "  Material Preview");
        if (s_auto_rotate) s_preview_rot_y += ImGui::GetIO().DeltaTime * 0.4f;
        ImGui::Checkbox("Auto Rotate##mat", &s_auto_rotate);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("##rot_y_mat", &s_preview_rot_y, -3.14159f, 3.14159f, "Y:%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("##env_int", &s_env_intensity, 0.0f, 1.0f, "Env:%.2f");

        ImVec2 preview_pos = ImGui::GetCursorScreenPos();
        const float sphere_radius = 56.0f;
        ImVec2 center(preview_pos.x + sphere_radius + 8, preview_pos.y + sphere_radius + 4);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Material parameters
        float base_r = mesh.color.r, base_g = mesh.color.g, base_b = mesh.color.b;
        float metallic_val = mesh.metallic;
        float roughness_val = mesh.roughness;
        float emissive_r = mesh.emissive.r;
        float emissive_g = mesh.emissive.g;
        float emissive_b = mesh.emissive.b;

        // Normalized light direction
        float lx = s_light_dir[0], ly = s_light_dir[1], lz = s_light_dir[2];
        float llen = std::sqrt(lx*lx + ly*ly + lz*lz);
        if (llen > 0.001f) { lx /= llen; ly /= llen; lz /= llen; }

        // Render sphere pixel-by-pixel (low-res for performance)
        const int res = 28;
        float pixel_size = sphere_radius * 2.0f / static_cast<float>(res);
        float cos_ry = std::cos(s_preview_rot_y), sin_ry = std::sin(s_preview_rot_y);
        float cos_rx = std::cos(s_preview_rot_x), sin_rx = std::sin(s_preview_rot_x);

        for (int py = 0; py < res; py++) {
            for (int px = 0; px < res; px++) {
                float u = (static_cast<float>(px) + 0.5f) / res * 2.0f - 1.0f;
                float v = (static_cast<float>(py) + 0.5f) / res * 2.0f - 1.0f;
                float r2 = u * u + v * v;
                if (r2 > 1.0f) continue;

                float nz_local = std::sqrt(1.0f - r2);
                float nx_local = u, ny_local = -v;

                // Rotate normal
                float nx1 = nx_local * cos_ry + nz_local * sin_ry;
                float nz1 = -nx_local * sin_ry + nz_local * cos_ry;
                float ny1 = ny_local * cos_rx + nz1 * sin_rx;
                float nz2 = -ny_local * sin_rx + nz1 * cos_rx;
                (void)nz2;

                // Lambertian diffuse
                float ndotl = std::max(0.0f, nx1 * lx + ny1 * ly + nz1 * lz);

                // View direction (camera at z=inf, so V = (0,0,1) in local space)
                float vx = 0, vy = 0, vz = 1.0f;
                // Halfway vector
                float hx = lx + vx, hy = ly + vy, hz = lz + vz;
                float hlen = std::sqrt(hx*hx + hy*hy + hz*hz);
                if (hlen > 0.001f) { hx /= hlen; hy /= hlen; hz /= hlen; }
                float ndoth = std::max(0.0f, nx_local * hx + ny_local * hy + nz_local * hz);

                // Specular (Blinn-Phong approximation of GGX)
                float alpha = roughness_val * roughness_val;
                float spec_power = 2.0f / (alpha * alpha + 0.001f) - 2.0f;
                spec_power = std::max(1.0f, std::min(2048.0f, spec_power));
                float spec = std::pow(ndoth, spec_power) * (spec_power + 2.0f) / 8.0f;

                // Fresnel (Schlick)
                float f0_val = 0.04f + metallic_val * 0.96f;
                float fresnel = f0_val + (1.0f - f0_val) * std::pow(1.0f - std::max(0.0f, ndoth), 5.0f);

                // Combine: metallic uses base color as specular color
                float diff_r = base_r * (1.0f - metallic_val) * ndotl;
                float diff_g = base_g * (1.0f - metallic_val) * ndotl;
                float diff_b = base_b * (1.0f - metallic_val) * ndotl;

                float spec_r = (metallic_val * base_r + (1.0f - metallic_val) * f0_val) * spec * fresnel;
                float spec_g = (metallic_val * base_g + (1.0f - metallic_val) * f0_val) * spec * fresnel;
                float spec_b = (metallic_val * base_b + (1.0f - metallic_val) * f0_val) * spec * fresnel;

                // Ambient / environment
                float amb_r = base_r * s_env_intensity * (0.5f + 0.5f * ny1);
                float amb_g = base_g * s_env_intensity * (0.5f + 0.5f * ny1);
                float amb_b = base_b * s_env_intensity * (0.5f + 0.5f * ny1);

                float final_r = diff_r + spec_r + amb_r + emissive_r;
                float final_g = diff_g + spec_g + amb_g + emissive_g;
                float final_b = diff_b + spec_b + amb_b + emissive_b;

                // Tone mapping + gamma
                final_r = final_r / (final_r + 1.0f);
                final_g = final_g / (final_g + 1.0f);
                final_b = final_b / (final_b + 1.0f);
                final_r = std::pow(final_r, 1.0f / 2.2f);
                final_g = std::pow(final_g, 1.0f / 2.2f);
                final_b = std::pow(final_b, 1.0f / 2.2f);

                int ir = std::min(255, static_cast<int>(final_r * 255));
                int ig = std::min(255, static_cast<int>(final_g * 255));
                int ib = std::min(255, static_cast<int>(final_b * 255));

                ImVec2 pmin(center.x + (u - 1.0f / res) * sphere_radius,
                            center.y + (v - 1.0f / res) * sphere_radius);
                ImVec2 pmax(pmin.x + pixel_size, pmin.y + pixel_size);
                dl->AddRectFilled(pmin, pmax, IM_COL32(ir, ig, ib, 255));
            }
        }

        ImGui::Dummy(ImVec2(sphere_radius * 2 + 16, sphere_radius * 2 + 8));
    }

    ImGui::Separator();
    ImGui::Text(MDI_ICON_PALETTE "  PBR Material Properties");
    ImGui::Spacing();

    auto& tab_mgr = SceneTabManager::Get();

    auto& undo_mgr = GetUndoRedoManager();
    const auto entity = selected_entity;

    // Helper: float undo with merge (for slider drags)
    auto UndoFloat = [&](const char* desc, float& field, float old_val) {
        float new_val = field;
        auto* reg = &registry;
        undo_mgr.Execute(std::make_unique<PropertyChangeCommand<float>>(
            desc, old_val, new_val,
            [reg, entity, offset = (size_t)((char*)&field - (char*)&mesh)](const float& v) {
                if (reg->valid(entity) && reg->all_of<dse::MeshRendererComponent>(entity)) {
                    *reinterpret_cast<float*>(reinterpret_cast<char*>(&reg->get<dse::MeshRendererComponent>(entity)) + offset) = v;
                }
            }
        ), true);
        tab_mgr.MarkDirty();
    };

    // Albedo color
    float albedo[4] = { mesh.color.r, mesh.color.g, mesh.color.b, mesh.color.a };
    if (ImGui::ColorEdit4("Albedo", albedo)) {
        glm::vec4 old_color = mesh.color;
        mesh.color = glm::vec4(albedo[0], albedo[1], albedo[2], albedo[3]);
        glm::vec4 new_color = mesh.color;
        auto* reg = &registry;
        undo_mgr.Execute(std::make_unique<PropertyChangeCommand<glm::vec4>>(
            "Material Albedo", old_color, new_color,
            [reg, entity](const glm::vec4& v) {
                if (reg->valid(entity) && reg->all_of<dse::MeshRendererComponent>(entity))
                    reg->get<dse::MeshRendererComponent>(entity).color = v;
            }
        ), true);
        tab_mgr.MarkDirty();
    }

    // Metallic
    { float old_v = mesh.metallic;
    if (ImGui::SliderFloat("Metallic", &mesh.metallic, 0.0f, 1.0f, "%.2f")) UndoFloat("Material Metallic", mesh.metallic, old_v); }

    // Roughness
    { float old_v = mesh.roughness;
    if (ImGui::SliderFloat("Roughness", &mesh.roughness, 0.0f, 1.0f, "%.2f")) UndoFloat("Material Roughness", mesh.roughness, old_v); }

    // AO
    { float old_v = mesh.ao;
    if (ImGui::SliderFloat("Ambient Occlusion", &mesh.ao, 0.0f, 1.0f, "%.2f")) UndoFloat("Material AO", mesh.ao, old_v); }

    // Normal strength
    { float old_v = mesh.normal_strength;
    if (ImGui::SliderFloat("Normal Strength", &mesh.normal_strength, 0.0f, 2.0f, "%.2f")) UndoFloat("Material Normal Strength", mesh.normal_strength, old_v); }

    // Emissive
    float emissive[3] = { mesh.emissive.r, mesh.emissive.g, mesh.emissive.b };
    if (ImGui::ColorEdit3("Emissive", emissive)) {
        glm::vec3 old_em = mesh.emissive;
        mesh.emissive = glm::vec3(emissive[0], emissive[1], emissive[2]);
        glm::vec3 new_em = mesh.emissive;
        auto* reg = &registry;
        undo_mgr.Execute(std::make_unique<PropertyChangeCommand<glm::vec3>>(
            "Material Emissive", old_em, new_em,
            [reg, entity](const glm::vec3& v) {
                if (reg->valid(entity) && reg->all_of<dse::MeshRendererComponent>(entity))
                    reg->get<dse::MeshRendererComponent>(entity).emissive = v;
            }
        ), true);
        tab_mgr.MarkDirty();
    }

    ImGui::Separator();
    ImGui::Text(MDI_ICON_IMAGE "  Texture Slots");
    ImGui::Spacing();

    // handle → display-name cache (session-scoped, no serialization needed)
    static std::unordered_map<unsigned int, std::string> s_handle_names;

    AssetManager* asset_mgr = ctx.engine.asset_manager();

    // Texture slots with drag-drop support
    auto DrawTextureSlot = [&](const char* label, unsigned int& texture_handle) {
        ImGui::PushID(label);
        ImGui::Text("%s", label);
        ImGui::SameLine(140);

        char buf[128];
        if (texture_handle != 0) {
            auto it = s_handle_names.find(texture_handle);
            if (it == s_handle_names.end() && asset_mgr) {
                std::string path = asset_mgr->FindTexturePathByHandle(texture_handle);
                if (!path.empty())
                    s_handle_names[texture_handle] = std::filesystem::path(path).filename().string();
                it = s_handle_names.find(texture_handle);
            }
            if (it != s_handle_names.end()) {
                snprintf(buf, sizeof(buf), "%s", it->second.c_str());
            } else {
                snprintf(buf, sizeof(buf), "Texture #%u", texture_handle);
            }
        } else {
            snprintf(buf, sizeof(buf), "(None)");
        }

        ImGui::Button(buf, ImVec2(ImGui::GetContentRegionAvail().x - 30, 0));
        if (ImGui::IsItemHovered() && texture_handle != 0) {
            auto it = s_handle_names.find(texture_handle);
            if (it != s_handle_names.end())
                ImGui::SetTooltip("%s", it->second.c_str());
        }

        // Accept drag-drop of image assets from Project panel
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string rel_path(static_cast<const char*>(payload->Data));
                const auto ext = std::filesystem::path(rel_path).extension().string();
                const bool is_image = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                                       ext == ".tga" || ext == ".bmp" || ext == ".hdr" ||
                                       ext == ".ktx");
                if (is_image && asset_mgr) {
                    auto tex = asset_mgr->LoadTexture(rel_path);
                    if (tex) {
                        unsigned int old_handle = texture_handle;
                        unsigned int new_handle = tex->GetHandle();
                        texture_handle = new_handle;
                        s_handle_names[new_handle] =
                            std::filesystem::path(rel_path).filename().string();
                        auto* reg = &registry;
                        size_t offset = (size_t)((char*)&texture_handle - (char*)&mesh);
                        undo_mgr.Execute(std::make_unique<PropertyChangeCommand<unsigned int>>(
                            "Material Texture", old_handle, new_handle,
                            [reg, entity, offset](const unsigned int& v) {
                                if (reg->valid(entity) && reg->all_of<dse::MeshRendererComponent>(entity))
                                    *reinterpret_cast<unsigned int*>(reinterpret_cast<char*>(&reg->get<dse::MeshRendererComponent>(entity)) + offset) = v;
                            }), false);
                        tab_mgr.MarkDirty();
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        if (texture_handle != 0) {
            if (ImGui::SmallButton("X")) {
                unsigned int old_handle = texture_handle;
                s_handle_names.erase(texture_handle);
                texture_handle = 0;
                auto* reg = &registry;
                size_t offset = (size_t)((char*)&texture_handle - (char*)&mesh);
                undo_mgr.Execute(std::make_unique<PropertyChangeCommand<unsigned int>>(
                    "Clear Material Texture", old_handle, 0u,
                    [reg, entity, offset](const unsigned int& v) {
                        if (reg->valid(entity) && reg->all_of<dse::MeshRendererComponent>(entity))
                            *reinterpret_cast<unsigned int*>(reinterpret_cast<char*>(&reg->get<dse::MeshRendererComponent>(entity)) + offset) = v;
                    }), false);
                tab_mgr.MarkDirty();
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
        std::string old_variant = mesh.shader_variant;
        mesh.shader_variant = shader_variants[current_variant];
        std::string new_variant = mesh.shader_variant;
        auto* reg = &registry;
        undo_mgr.Execute(std::make_unique<PropertyChangeCommand<std::string>>(
            "Material Shader", old_variant, new_variant,
            [reg, entity](const std::string& v) {
                if (reg->valid(entity) && reg->all_of<dse::MeshRendererComponent>(entity))
                    reg->get<dse::MeshRendererComponent>(entity).shader_variant = v;
            }), false);
        tab_mgr.MarkDirty();
    }

    auto UndoBool = [&](const char* desc, bool& field, bool old_val) {
        bool new_val = field;
        auto* reg = &registry;
        undo_mgr.Execute(std::make_unique<PropertyChangeCommand<bool>>(
            desc, old_val, new_val,
            [reg, entity, offset = (size_t)((char*)&field - (char*)&mesh)](const bool& v) {
                if (reg->valid(entity) && reg->all_of<dse::MeshRendererComponent>(entity))
                    *reinterpret_cast<bool*>(reinterpret_cast<char*>(&reg->get<dse::MeshRendererComponent>(entity)) + offset) = v;
            }), false);
        tab_mgr.MarkDirty();
    };

    { bool old_v = mesh.material_alpha_test;
    if (ImGui::Checkbox("Alpha Test", &mesh.material_alpha_test)) UndoBool("Material Alpha Test", mesh.material_alpha_test, old_v); }
    if (mesh.material_alpha_test) {
        { float old_v = mesh.material_alpha_cutoff;
        if (ImGui::SliderFloat("Alpha Cutoff", &mesh.material_alpha_cutoff, 0.0f, 1.0f)) UndoFloat("Material Alpha Cutoff", mesh.material_alpha_cutoff, old_v); }
    }
    { bool old_v = mesh.material_double_sided;
    if (ImGui::Checkbox("Double Sided", &mesh.material_double_sided)) UndoBool("Material Double Sided", mesh.material_double_sided, old_v); }
    { bool old_v = mesh.receive_shadow;
    if (ImGui::Checkbox("Receive Shadow", &mesh.receive_shadow)) UndoBool("Material Receive Shadow", mesh.receive_shadow, old_v); }
    { bool old_v = mesh.depth_test_enabled;
    if (ImGui::Checkbox("Depth Test", &mesh.depth_test_enabled)) UndoBool("Material Depth Test", mesh.depth_test_enabled, old_v); }
    { bool old_v = mesh.depth_write_enabled;
    if (ImGui::Checkbox("Depth Write", &mesh.depth_write_enabled)) UndoBool("Material Depth Write", mesh.depth_write_enabled, old_v); }
    { bool old_v = mesh.visible;
    if (ImGui::Checkbox("Visible", &mesh.visible)) UndoBool("Material Visible", mesh.visible, old_v); }

    ImGui::End();
}

} // namespace dse::editor
