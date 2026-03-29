#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

// Helper component for editor to display names in Hierarchy
struct EditorNameComponent {
    std::string name;
};


void SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Deep tech dark theme based on Editor_UI_Architecture.md
    colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.66f, 1.00f, 1.00f); // Tech Blue
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 0.66f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.00f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.00f, 0.66f, 1.00f, 0.80f); // Tech Blue hover
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.66f, 1.00f, 0.40f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.00f, 0.66f, 1.00f, 0.80f);
    colors[ImGuiCol_Separator]              = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.00f, 0.66f, 1.00f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.00f, 0.66f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.66f, 1.00f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 0.66f, 1.00f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.00f, 0.66f, 1.00f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.00f, 0.66f, 1.00f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.54f, 0.17f, 0.89f, 0.70f); // Neon Purple for docking preview
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
    
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
}

enum class EditorState {
    Edit,
    Play
};

static EditorState g_editor_state = EditorState::Edit;
static std::unique_ptr<entt::registry> g_backup_registry;

void CopyRegistry(entt::registry& dst, entt::registry& src) {
    dst.clear();
    for (auto entity : src.storage<entt::entity>()) {
        auto new_ent = dst.create(entity);
        
        if (src.all_of<EditorNameComponent>(entity)) dst.emplace<EditorNameComponent>(new_ent, src.get<EditorNameComponent>(entity));
        if (src.all_of<TransformComponent>(entity)) dst.emplace<TransformComponent>(new_ent, src.get<TransformComponent>(entity));
        if (src.all_of<SpriteRendererComponent>(entity)) dst.emplace<SpriteRendererComponent>(new_ent, src.get<SpriteRendererComponent>(entity));
        if (src.all_of<RigidBody2DComponent>(entity)) dst.emplace<RigidBody2DComponent>(new_ent, src.get<RigidBody2DComponent>(entity));
    }
}

void SaveScene(entt::registry& registry, const std::string& filepath) {
    rapidjson::Document doc;
    doc.SetArray();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    for (auto entity : registry.storage<entt::entity>()) {
        rapidjson::Value ent_obj(rapidjson::kObjectType);
        ent_obj.AddMember("id", static_cast<uint32_t>(entity), allocator);

        if (registry.all_of<EditorNameComponent>(entity)) {
            rapidjson::Value name_val;
            name_val.SetString(registry.get<EditorNameComponent>(entity).name.c_str(), allocator);
            ent_obj.AddMember("name", name_val, allocator);
        }

        if (registry.all_of<TransformComponent>(entity)) {
            auto& t = registry.get<TransformComponent>(entity);
            rapidjson::Value t_obj(rapidjson::kObjectType);
            
            rapidjson::Value pos(rapidjson::kArrayType);
            pos.PushBack(t.position.x, allocator).PushBack(t.position.y, allocator).PushBack(t.position.z, allocator);
            t_obj.AddMember("position", pos, allocator);
            
            rapidjson::Value rot(rapidjson::kArrayType);
            rot.PushBack(t.rotation.x, allocator).PushBack(t.rotation.y, allocator).PushBack(t.rotation.z, allocator).PushBack(t.rotation.w, allocator);
            t_obj.AddMember("rotation", rot, allocator);
            
            rapidjson::Value scale(rapidjson::kArrayType);
            scale.PushBack(t.scale.x, allocator).PushBack(t.scale.y, allocator).PushBack(t.scale.z, allocator);
            t_obj.AddMember("scale", scale, allocator);
            
            ent_obj.AddMember("transform", t_obj, allocator);
        }
        
        if (registry.all_of<SpriteRendererComponent>(entity)) {
            auto& s = registry.get<SpriteRendererComponent>(entity);
            rapidjson::Value s_obj(rapidjson::kObjectType);
            
            rapidjson::Value path_val;
            path_val.SetString(s.shader_variant.c_str(), allocator);
            s_obj.AddMember("path", path_val, allocator);
            
            rapidjson::Value color(rapidjson::kArrayType);
            color.PushBack(s.color.r, allocator).PushBack(s.color.g, allocator).PushBack(s.color.b, allocator).PushBack(s.color.a, allocator);
            s_obj.AddMember("color", color, allocator);
            
            ent_obj.AddMember("sprite", s_obj, allocator);
        }

        doc.PushBack(ent_obj, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs(filepath);
    if (ofs.is_open()) {
        ofs << buffer.GetString();
    }
}

void LoadScene(entt::registry& registry, const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    doc.Parse(content.c_str());

    if (!doc.IsArray()) return;

    registry.clear();
    for (auto& v : doc.GetArray()) {
        auto entity = registry.create();
        
        if (v.HasMember("name") && v["name"].IsString()) {
            registry.emplace<EditorNameComponent>(entity, v["name"].GetString());
        }

        if (v.HasMember("transform") && v["transform"].IsObject()) {
            auto& t_obj = v["transform"];
            auto& t = registry.emplace<TransformComponent>(entity);
            if (t_obj.HasMember("position") && t_obj["position"].IsArray()) {
                auto pos = t_obj["position"].GetArray();
                if (pos.Size() >= 3) t.position = glm::vec3(pos[0].GetFloat(), pos[1].GetFloat(), pos[2].GetFloat());
            }
            if (t_obj.HasMember("rotation") && t_obj["rotation"].IsArray()) {
                auto rot = t_obj["rotation"].GetArray();
                if (rot.Size() >= 4) t.rotation = glm::quat(rot[3].GetFloat(), rot[0].GetFloat(), rot[1].GetFloat(), rot[2].GetFloat());
            }
            if (t_obj.HasMember("scale") && t_obj["scale"].IsArray()) {
                auto scale = t_obj["scale"].GetArray();
                if (scale.Size() >= 3) t.scale = glm::vec3(scale[0].GetFloat(), scale[1].GetFloat(), scale[2].GetFloat());
            }
        }
        
        if (v.HasMember("sprite") && v["sprite"].IsObject()) {
            auto& s_obj = v["sprite"];
            auto& s = registry.emplace<SpriteRendererComponent>(entity);
            if (s_obj.HasMember("path") && s_obj["path"].IsString()) {
                s.shader_variant = s_obj["path"].GetString();
            }
            if (s_obj.HasMember("color") && s_obj["color"].IsArray()) {
                auto color = s_obj["color"].GetArray();
                if (color.Size() >= 4) s.color = glm::vec4(color[0].GetFloat(), color[1].GetFloat(), color[2].GetFloat(), color[3].GetFloat());
            }
        }
    }
}

void DrawEditorUI(dse::runtime::EngineInstance& engine, unsigned int scene_texture, unsigned int game_texture) {
    static entt::entity selected_entity = entt::null;
    
    static bool inspector_active = true;
    static bool inspector_static = false;
    static bool sprite_flip_x = false;
    static bool sprite_flip_y = false;
    static bool collider_is_trigger = false;
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        static bool first_time = true;
        if (first_time) {
            first_time = false;

            ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            // Split the dockspace into nodes -- Unity Style
            auto dock_id_main = dockspace_id;
            
            // 1. Split top for toolbar
            auto dock_id_top = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up, 0.05f, nullptr, &dock_id_main);
            
            // 2. Split bottom for Project/Console/Animation
            auto dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.30f, nullptr, &dock_id_main);
            
            // 3. Split left for Hierarchy
            auto dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, nullptr, &dock_id_main);
            
            // 4. Split right for Inspector
            auto dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, nullptr, &dock_id_main);

            // We now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow("Toolbar", dock_id_top);
            ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Project", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Animation", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Tile Palette", dock_id_bottom); // New 2D panel
            ImGui::DockBuilderDockWindow("Scene", dock_id_main);
            ImGui::DockBuilderDockWindow("Game", dock_id_main);
            
            // Set some nodes to hide their tab bar if needed (e.g., Toolbar)
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id_top);
            if (node) {
                node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeY;
            }
            
            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                World::Instance().registry().clear();
            }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
                LoadScene(World::Instance().registry(), "scene.json");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                SaveScene(World::Instance().registry(), "scene.json");
            }
            if (ImGui::MenuItem("Save As...")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Toolbar Panel
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4));
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);
    float avail_width = ImGui::GetContentRegionAvail().x;
    
    // Tools (Left)
    static int current_gizmo_operation = 0; // 0: Translate
    ImGui::SetCursorPosX(10);
    
    ImGui::PushStyleColor(ImGuiCol_Button, current_gizmo_operation == -1 ? ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("[H]", ImVec2(32, 24))) { current_gizmo_operation = -1; } // Hand
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, current_gizmo_operation == 0 ? ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("[M]", ImVec2(32, 24))) { current_gizmo_operation = 0; } // Move
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, current_gizmo_operation == 1 ? ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("[R]", ImVec2(32, 24))) { current_gizmo_operation = 1; } // Rotate
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, current_gizmo_operation == 2 ? ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("[S]", ImVec2(32, 24))) { current_gizmo_operation = 2; } // Scale
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    // 2D/3D Toggle
    ImGui::SetCursorPosX(10 + 4 * 36 + 20); // Spacing after transform tools
    static bool is2D = false;
    if (is2D) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
    }
    if (ImGui::Button("2D", ImVec2(32, 24))) { is2D = !is2D; }
    if (is2D) {
        ImGui::PopStyleColor();
    }

    // Play Controls (Center)
    ImGui::SameLine();
    ImGui::SetCursorPosX((avail_width / 2.0f) - 60);
    
    if (g_editor_state == EditorState::Edit) {
        if (ImGui::Button(">", ImVec2(32, 24))) { // Play
            g_editor_state = EditorState::Play;
            g_backup_registry = std::make_unique<entt::registry>();
            CopyRegistry(*g_backup_registry, World::Instance().registry());
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("[]", ImVec2(32, 24))) { // Stop
            g_editor_state = EditorState::Edit;
            CopyRegistry(World::Instance().registry(), *g_backup_registry);
            g_backup_registry.reset();
            selected_entity = entt::null;
        }
        ImGui::PopStyleColor();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("||", ImVec2(32, 24))) {} // Pause
    ImGui::SameLine();
    if (ImGui::Button(">|", ImVec2(32, 24))) {} // Step

    // Account/Settings (Right)
    ImGui::SameLine();
    ImGui::SetCursorPosX(avail_width - 130);
    ImGui::Button("Collab", ImVec2(60, 24));
    ImGui::SameLine();
    ImGui::Button("Layers", ImVec2(60, 24));

    ImGui::End();
    ImGui::PopStyleVar();

    // Panels (Unity-style layout)
    ImGui::Begin("Hierarchy");
    World& world = World::Instance();
    auto& registry = world.registry();
    
    if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto entity : registry.storage<entt::entity>()) {
            std::string entity_name = "Entity " + std::to_string(static_cast<uint32_t>(entity));
            if (registry.all_of<EditorNameComponent>(entity)) {
                entity_name = registry.get<EditorNameComponent>(entity).name;
            }
            
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (selected_entity == entity) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            
            ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", entity_name.c_str());
            if (ImGui::IsItemClicked()) {
                selected_entity = entity;
            }
        }
        ImGui::TreePop();
    }
    
    // Right click menu for Hierarchy
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Create Empty Entity")) {
            auto new_ent = world.CreateEntity();
            registry.emplace<EditorNameComponent>(new_ent, "New Entity");
            selected_entity = new_ent;
        }
        if (selected_entity != entt::null && ImGui::MenuItem("Delete Entity")) {
            world.DestroyEntity(selected_entity);
            selected_entity = entt::null;
        }
        ImGui::EndPopup();
    }
    
    ImGui::End();

    ImGui::Begin("Inspector");
    
    if (selected_entity != entt::null && registry.valid(selected_entity)) {
        // Header component
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::AlignTextToFramePadding();
        ImGui::Checkbox("##Active", &inspector_active); ImGui::SameLine();
        
        char nameBuf[64] = "";
        if (registry.all_of<EditorNameComponent>(selected_entity)) {
            strncpy(nameBuf, registry.get<EditorNameComponent>(selected_entity).name.c_str(), sizeof(nameBuf) - 1);
        } else {
            strncpy(nameBuf, "Entity", sizeof(nameBuf) - 1);
        }
        
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 70);
        if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) {
            if (registry.all_of<EditorNameComponent>(selected_entity)) {
                registry.get<EditorNameComponent>(selected_entity).name = nameBuf;
            } else {
                registry.emplace<EditorNameComponent>(selected_entity, nameBuf);
            }
        }
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::Checkbox("Static", &inspector_static);
        ImGui::Separator();

        // Helper macro for left-aligned labels in Inspector
        #define INSPECTOR_PROPERTY(label, code) \
            ImGui::AlignTextToFramePadding(); \
            ImGui::Text(label); \
            ImGui::NextColumn(); \
            ImGui::SetNextItemWidth(-1); \
            code; \
            ImGui::NextColumn();

        if (registry.all_of<TransformComponent>(selected_entity)) {
            auto& transform = registry.get<TransformComponent>(selected_entity);
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "transform_cols", false);
                ImGui::SetColumnWidth(0, 80.0f);
                
                float pos[3] = {transform.position.x, transform.position.y, transform.position.z};
                INSPECTOR_PROPERTY("Position", if (ImGui::DragFloat3("##pos", pos, 0.1f)) {
                    transform.position = glm::vec3(pos[0], pos[1], pos[2]);
                });
                
                // transform.rotation is a glm::quat, we might want to convert to Euler angles for editing, but for now we will just use a dummy logic or simple representation.
                glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
                float rot[3] = {euler.x, euler.y, euler.z};
                if (is2D) {
                    INSPECTOR_PROPERTY("Rotation", if (ImGui::DragFloat("##rotZ", &rot[2], 0.1f)) {
                        euler.z = rot[2];
                        transform.rotation = glm::quat(glm::radians(euler));
                    });
                } else {
                    INSPECTOR_PROPERTY("Rotation", if (ImGui::DragFloat3("##rot", rot, 0.1f)) {
                        euler = glm::vec3(rot[0], rot[1], rot[2]);
                        transform.rotation = glm::quat(glm::radians(euler));
                    });
                }
                
                float scale[3] = {transform.scale.x, transform.scale.y, transform.scale.z};
                INSPECTOR_PROPERTY("Scale", if (ImGui::DragFloat3("##scale", scale, 0.1f)) {
                    transform.scale = glm::vec3(scale[0], scale[1], scale[2]);
                });
                
                ImGui::Columns(1);
            }
        }
        
        if (registry.all_of<SpriteRendererComponent>(selected_entity)) {
            auto& sprite = registry.get<SpriteRendererComponent>(selected_entity);
            if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "spriterenderer_cols", false);
                ImGui::SetColumnWidth(0, 80.0f);
                
                // Asset drop target
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Sprite");
                ImGui::NextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::Button(sprite.shader_variant.empty() ? "None (Texture)" : "Texture Set", ImVec2(-1, 0));
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                        const char* path = (const char*)payload->Data;
                        sprite.shader_variant = path; // Mocking setting texture path
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::NextColumn();
                
                float color[4] = {sprite.color.r, sprite.color.g, sprite.color.b, sprite.color.a};
                INSPECTOR_PROPERTY("Color", if (ImGui::ColorEdit4("##color", color)) {
                    sprite.color = glm::vec4(color[0], color[1], color[2], color[3]);
                });
                
                ImGui::Columns(1);
            }
        }

        if (registry.all_of<RigidBody2DComponent>(selected_entity)) {
            auto& rb2d = registry.get<RigidBody2DComponent>(selected_entity);
            if (ImGui::CollapsingHeader("RigidBody 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "rb2d_cols", false);
                ImGui::SetColumnWidth(0, 80.0f);
                
                const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
                int currentType = static_cast<int>(rb2d.type);
                INSPECTOR_PROPERTY("Body Type", if (ImGui::Combo("##type", &currentType, bodyTypes, IM_ARRAYSIZE(bodyTypes))) {
                    rb2d.type = static_cast<RigidBody2DType>(currentType);
                });
                
                float vel[2] = {rb2d.velocity.x, rb2d.velocity.y};
                INSPECTOR_PROPERTY("Velocity", if (ImGui::DragFloat2("##vel", vel, 0.1f)) {
                    rb2d.velocity = glm::vec2(vel[0], vel[1]);
                });
                
                INSPECTOR_PROPERTY("Gravity", ImGui::DragFloat("##grav", &rb2d.gravity_scale, 0.1f));
                
                ImGui::Columns(1);
            }
        }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 60);
    if (ImGui::Button("Add Component", ImVec2(120, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    
    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (ImGui::MenuItem("Transform")) {
            if (!registry.all_of<TransformComponent>(selected_entity)) {
                registry.emplace<TransformComponent>(selected_entity);
            }
        }
        if (ImGui::MenuItem("Name")) {
            if (!registry.all_of<EditorNameComponent>(selected_entity)) {
                registry.emplace<EditorNameComponent>(selected_entity, "New Component");
            }
        }
        if (ImGui::MenuItem("Sprite Renderer")) {
            if (!registry.all_of<SpriteRendererComponent>(selected_entity)) {
                registry.emplace<SpriteRendererComponent>(selected_entity);
            }
        }
        if (ImGui::MenuItem("RigidBody 2D")) {
            if (!registry.all_of<RigidBody2DComponent>(selected_entity)) {
                registry.emplace<RigidBody2DComponent>(selected_entity);
            }
        }
        // More components can be added here
        ImGui::EndPopup();
    }
    
    ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("No Entity Selected");
    }
    ImGui::End();

    ImGui::Begin("Project");
    ImGui::Text("Assets");
    ImGui::Separator();
    
    // Simple file browser
    // Since the executable runs from the bin/ directory or project root, we need to find the correct data path
    static std::filesystem::path base_data_path = []() {
        std::filesystem::path p = std::filesystem::current_path();
        // Check if we are in bin directory, if so go up one level
        if (p.filename() == "bin" || p.filename() == "build_vs2022") {
            p = p.parent_path();
        }
        
        std::filesystem::path target_path = p / "samples" / "lua" / "data";
        
        // If data folder doesn't exist, try creating it or fallback to root
        if (!std::filesystem::exists(target_path)) {
            try {
                std::filesystem::create_directories(target_path);
            } catch (...) {
                return p;
            }
        }
        return target_path;
    }();
    
    static std::filesystem::path current_path = base_data_path;
    
    if (ImGui::BeginPopupContextWindow("ProjectContextMenu")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) {
                std::string new_folder = (current_path / "NewFolder").string();
                std::filesystem::create_directory(new_folder);
            }
            if (ImGui::MenuItem("Lua Script")) {
                std::string new_script = (current_path / "NewScript.lua").string();
                std::ofstream ofs(new_script);
                if (ofs.is_open()) {
                    ofs << "-- New Lua Script\n";
                    ofs.close();
                }
            }
            if (ImGui::MenuItem("Material")) {
                std::string new_mat = (current_path / "NewMaterial.mat").string();
                std::ofstream ofs(new_mat);
                if (ofs.is_open()) {
                    ofs << "{\n  \"shader\": \"default\",\n  \"color\": [1,1,1,1]\n}\n";
                    ofs.close();
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    
    if (!std::filesystem::exists(current_path)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Data path not found: %s", current_path.string().c_str());
    } else {
        if (current_path != base_data_path) {
            if (ImGui::Button("<- Back")) {
                current_path = current_path.parent_path();
            }
            ImGui::Separator();
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
            const auto& path = entry.path();
            auto filename = path.filename().string();
            
            if (entry.is_directory()) {
                if (ImGui::Selectable((std::string("[DIR] ") + filename).c_str())) {
                    current_path /= path.filename();
                }
            } else {
                ImGui::Selectable((std::string("[FILE] ") + filename).c_str());
                
                // Support Drag and Drop
                if (ImGui::BeginDragDropSource()) {
                    std::string relative_path = std::filesystem::relative(path, base_data_path).string();
                    ImGui::SetDragDropPayload("ASSET_PATH", relative_path.c_str(), relative_path.size() + 1);
                    ImGui::Text("%s", filename.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
    ImGui::End();

    ImGui::Begin("Console");
    ImGui::Text("[Info] Engine initialized successfully.");
    ImGui::Text("[Info] Loaded default scene.");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Warning] Missing texture 'skybox_diffuse'.");
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Error] Failed to load shader 'standard_pbr.glsl'.");
    ImGui::End();
    
    ImGui::Begin("Animation");
    ImGui::Text("No animated object selected.");
    ImGui::End();

    ImGui::Begin("Tile Palette");
    if (!is2D) {
        ImGui::TextDisabled("Tile Palette is only available in 2D mode.");
    } else {
        ImGui::Button("Active Brush", ImVec2(120, 24)); ImGui::SameLine();
        ImGui::Button("Paint", ImVec2(60, 24)); ImGui::SameLine();
        ImGui::Button("Erase", ImVec2(60, 24));
        
        ImGui::Separator();
        ImGui::Text("Select a tilemap to start painting.");
        
        // Mock tile palette grid
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 6; x++) {
                draw_list->AddRectFilled(
                    ImVec2(p.x + x * 32, p.y + y * 32), 
                    ImVec2(p.x + x * 32 + 30, p.y + y * 32 + 30), 
                    IM_COL32(80 + (x+y)*10, 100 + x*10, 120 + y*10, 255)
                );
            }
        }
    }
    ImGui::End();

    // The Scene Viewport Panel (Where engine editor rendering would happen)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene");
    ImVec2 scenePanelSize = ImGui::GetContentRegionAvail();
    
    // Process Picking
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 window_pos = ImGui::GetWindowPos();
        // Just a mock for picking logic - would normally use real world coordinates from engine camera
        float x = (mouse_pos.x - window_pos.x) - scenePanelSize.x / 2.0f;
        float y = (mouse_pos.y - window_pos.y) - scenePanelSize.y / 2.0f;
        
        // Simple distance check
        float min_dist = 10000.0f;
        entt::entity picked = entt::null;
        for (auto entity : registry.storage<entt::entity>()) {
            if (registry.all_of<TransformComponent>(entity)) {
                auto& t = registry.get<TransformComponent>(entity);
                // Mock coordinate mapping
                float dx = (t.position.x * 50.0f) - x;
                float dy = (-t.position.y * 50.0f) - y;
                float dist = dx*dx + dy*dy;
                if (dist < 2500.0f && dist < min_dist) { // 50 pixel radius
                    min_dist = dist;
                    picked = entity;
                }
            }
        }
        if (picked != entt::null) {
            selected_entity = picked;
        }
    }

    if (scene_texture != 0) {
        // Render engine FBO texture, note UVs are flipped vertically because OpenGL textures are bottom-up
        ImGui::Image((ImTextureID)(intptr_t)scene_texture, scenePanelSize, ImVec2(0, 1), ImVec2(1, 0));
        
    } else {
        // Fallback placeholder
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + scenePanelSize.x, p_min.y + scenePanelSize.y);
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(40, 40, 40, 255));
        for (float i = 0; i < scenePanelSize.x; i += 50) {
            draw_list->AddLine(ImVec2(p_min.x + i, p_min.y), ImVec2(p_min.x + i, p_max.y), IM_COL32(60, 60, 60, 255));
        }
        for (float i = 0; i < scenePanelSize.y; i += 50) {
            draw_list->AddLine(ImVec2(p_min.x, p_min.y + i), ImVec2(p_max.x, p_min.y + i), IM_COL32(60, 60, 60, 255));
        }
        draw_list->AddText(ImVec2(p_min.x + scenePanelSize.x/2 - 50, p_min.y + scenePanelSize.y/2), IM_COL32(200, 200, 200, 255), "Scene View");
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // The Game Viewport Panel (Independent Game Camera view)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Game");
    ImVec2 gamePanelSize = ImGui::GetContentRegionAvail();
    if (game_texture != 0) {
        ImGui::Image((ImTextureID)(intptr_t)game_texture, gamePanelSize, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + gamePanelSize.x, p_min.y + gamePanelSize.y);
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(20, 20, 20, 255));
        draw_list->AddText(ImVec2(p_min.x + gamePanelSize.x/2 - 40, p_min.y + gamePanelSize.y/2), IM_COL32(150, 150, 150, 255), "Game View");
    }
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::End(); // End DockSpace window
}

int main() {
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DSEngine Editor", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window in Editor." << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "Editor window created successfully." << std::endl;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGL(glfwGetProcAddress)) {
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    
    // Redirect imgui.ini to bin folder so it doesn't clutter the project root
    io.IniFilename = "bin/editor_layout.ini";

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    SetupImGuiStyle();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize DSEngine
    dse::runtime::EngineRunConfig engine_config;
    engine_config.window_width = 1280;
    engine_config.window_height = 720;
    engine_config.window_title = "DSEngine Editor";
    engine_config.business_mode = BusinessMode::Lua;
    engine_config.enable_editor = true;
    engine_config.startup_lua_script_path = "samples/lua/main.lua";
    
    // We need to set DSE_DATA_ROOT before initializing the engine
    if (std::getenv("DSE_DATA_ROOT") == nullptr) {
#if defined(_WIN32)
        _putenv_s("DSE_DATA_ROOT", "samples/lua/data"); // Default or maybe we can hardcode for editor?
#else
        setenv("DSE_DATA_ROOT", "samples/lua/data", 1);
#endif
    }

    dse::runtime::EngineInstance engine_instance(engine_config);
    if (!engine_instance.Init()) {
        std::cerr << "Failed to initialize DSEngine in Editor." << std::endl;
        return -1;
    }

    std::cout << "Engine initialized successfully. Entering main loop..." << std::endl;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Tick Engine
        engine_instance.Tick();

        unsigned int scene_texture = engine_instance.pipeline()->GetSceneTextureId();
        unsigned int game_texture = engine_instance.pipeline()->GetMainTextureId();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw Editor UI
        DrawEditorUI(engine_instance, scene_texture, game_texture);

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    engine_instance.Shutdown();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
