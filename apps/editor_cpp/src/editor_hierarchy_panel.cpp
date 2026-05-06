#include "editor_hierarchy_panel.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_shortcuts.h"
#include "editor_console_panel.h"
#include "editor_selection.h"
#include "editor_prefab.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <string>
#include <cctype>
#include <cstring>
#include <vector>
#include <filesystem>

namespace dse::editor {

namespace {

// Hierarchy panel persistent state
static char s_search_filter[128] = "";
static entt::entity s_renaming_entity = entt::null;
static char s_rename_buf[64] = "";
static entt::entity s_last_clicked_entity = entt::null;

const char* GetEntityTypeIcon(entt::registry& registry, entt::entity entity) {
    if (registry.all_of<dse::Camera3DComponent>(entity) || registry.all_of<CameraComponent>(entity)) {
        return MDI_ICON_VIDEO;
    } else if (registry.all_of<dse::DirectionalLight3DComponent>(entity) ||
               registry.all_of<dse::PointLightComponent>(entity) ||
               registry.all_of<dse::SpotLightComponent>(entity)) {
        return MDI_ICON_LIGHTBULB;
    } else if (registry.all_of<dse::MeshRendererComponent>(entity)) {
        return MDI_ICON_SPHERE;
    } else if (registry.all_of<UIRendererComponent>(entity)) {
        return MDI_ICON_IMAGE;
    } else if (registry.all_of<dse::ParticleSystem3DComponent>(entity) ||
               registry.all_of<ParticleEmitterComponent>(entity)) {
        return MDI_ICON_CREATION;
    } else if (registry.all_of<dse::TerrainComponent>(entity)) {
        return MDI_ICON_TERRAIN;
    }
    return MDI_ICON_CUBE_OUTLINE;
}

bool MatchesSearchFilter(const std::string& name) {
    if (s_search_filter[0] == '\0') return true;
    // Case-insensitive substring match
    std::string lower_name = name;
    std::string lower_filter = s_search_filter;
    for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (auto& c : lower_filter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower_name.find(lower_filter) != std::string::npos;
}

std::vector<entt::entity> GetChildren(entt::registry& registry, entt::entity parent) {
    std::vector<entt::entity> children;
    for (auto entity : registry.storage<entt::entity>()) {
        if (!registry.valid(entity)) continue;
        if (registry.all_of<ParentComponent>(entity)) {
            if (registry.get<ParentComponent>(entity).parent == parent) {
                children.push_back(entity);
            }
        }
    }
    return children;
}

void DrawEntityNode(EditorHierarchyPanelContext& context, entt::entity entity) {
    if (!context.registry.valid(entity)) return;

    std::string entity_name = "Entity " + std::to_string(static_cast<uint32_t>(entity));
    if (context.registry.all_of<EditorNameComponent>(entity)) {
        entity_name = context.registry.get<EditorNameComponent>(entity).name;
    }

    if (!MatchesSearchFilter(entity_name)) return;

    // Prefab instance suffix
    bool is_prefab = IsPrefabInstance(context.registry, entity);
    std::string suffix = is_prefab ? " (Prefab)" : "";

    const char* type_icon = GetEntityTypeIcon(context.registry, entity);
    auto& selection = SelectionManager::Get();
    const bool is_selected = selection.Contains(entity);
    const bool is_renaming = (s_renaming_entity == entity);

    std::vector<entt::entity> children = GetChildren(context.registry, entity);
    bool has_children = !children.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Draw rounded highlight for selected entity
    if (is_selected) {
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        float item_width = ImGui::GetContentRegionAvail().x;
        float item_height = ImGui::GetTextLineHeightWithSpacing();
        ImU32 highlight_color = is_prefab ? IM_COL32(50, 180, 100, 80) : IM_COL32(71, 143, 255, 80);
        ImGui::GetWindowDrawList()->AddRectFilled(
            cursor_pos,
            ImVec2(cursor_pos.x + item_width, cursor_pos.y + item_height),
            highlight_color, 4.0f);
    }

    if (is_renaming) {
        ImGui::TextUnformatted(type_icon);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::SetKeyboardFocusHere();

        auto commit_rename = [&](entt::entity ent) {
            if (s_rename_buf[0] != '\0') {
                std::string old_name = entity_name;
                std::string new_name = s_rename_buf;
                if (context.registry.all_of<EditorNameComponent>(ent)) {
                    context.registry.get<EditorNameComponent>(ent).name = new_name;
                } else {
                    context.registry.emplace<EditorNameComponent>(ent, new_name);
                }
                auto& reg = context.registry;
                GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
                    "Rename Entity",
                    []{},
                    [&reg, ent, old_name]() {
                        if (reg.valid(ent) && reg.all_of<EditorNameComponent>(ent)) {
                            reg.get<EditorNameComponent>(ent).name = old_name;
                        }
                    }), false);
            }
            s_renaming_entity = entt::null;
        };

        if (ImGui::InputText("##rename", s_rename_buf, sizeof(s_rename_buf),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            commit_rename(entity);
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s_renaming_entity = entt::null;
        } else if (!ImGui::IsItemActive() && s_renaming_entity == entity &&
                   ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()) {
            commit_rename(entity);
        }
    } else {
        std::string display_name = std::string(type_icon) + "  " + entity_name + suffix;
        bool node_open = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", display_name.c_str());

        // Click handling
        if (ImGui::IsItemClicked()) {
            auto& sel = SelectionManager::Get();
            if (ImGui::GetIO().KeyCtrl) {
                sel.Toggle(entity);
                context.selected_entity = sel.GetPrimary();
            } else if (ImGui::GetIO().KeyShift && s_last_clicked_entity != entt::null) {
                sel.Clear();
                bool in_range = false;
                for (auto e : context.registry.storage<entt::entity>()) {
                    if (!context.registry.valid(e)) continue;
                    if (e == entity || e == s_last_clicked_entity) {
                        sel.Add(e);
                        if (in_range) break;
                        in_range = true;
                        continue;
                    }
                    if (in_range) sel.Add(e);
                }
                context.selected_entity = entity;
            } else {
                sel.SetSingle(entity);
                context.selected_entity = entity;
            }
            s_last_clicked_entity = entity;
        }

        // Double-click to rename
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !context.read_only) {
            s_renaming_entity = entity;
            std::strncpy(s_rename_buf, entity_name.c_str(), sizeof(s_rename_buf) - 1);
            s_rename_buf[sizeof(s_rename_buf) - 1] = '\0';
        }

        // Drag source
        if (!context.read_only && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entity, sizeof(entt::entity));
            ImGui::Text("%s", entity_name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target: reparent dragged entity under this one
        if (!context.read_only && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                entt::entity dragged = *static_cast<const entt::entity*>(payload->Data);
                if (dragged != entity && context.registry.valid(dragged)) {
                    entt::entity old_parent = entt::null;
                    if (context.registry.all_of<ParentComponent>(dragged)) {
                        old_parent = context.registry.get<ParentComponent>(dragged).parent;
                    }
                    // Set new parent
                    if (context.registry.all_of<ParentComponent>(dragged)) {
                        context.registry.get<ParentComponent>(dragged).parent = entity;
                    } else {
                        context.registry.emplace<ParentComponent>(dragged, entity);
                    }
                    // Undo support
                    auto& reg = context.registry;
                    GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
                        "Reparent Entity",
                        []{},
                        [&reg, dragged, old_parent]() {
                            if (!reg.valid(dragged)) return;
                            if (old_parent == entt::null) {
                                if (reg.all_of<ParentComponent>(dragged)) {
                                    reg.remove<ParentComponent>(dragged);
                                }
                            } else {
                                if (reg.all_of<ParentComponent>(dragged)) {
                                    reg.get<ParentComponent>(dragged).parent = old_parent;
                                } else {
                                    reg.emplace<ParentComponent>(dragged, old_parent);
                                }
                            }
                        }), false);
                    EditorLog(LogLevel::Info, "Reparented entity under " + entity_name);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Recurse into children
        if (has_children && node_open) {
            for (auto child : children) {
                DrawEntityNode(context, child);
            }
            ImGui::TreePop();
        }
    }
}

} // namespace

void DrawHierarchyPanel(EditorHierarchyPanelContext& context) {
    ImGui::Begin("Hierarchy");

    // Search bar
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##hierarchy_search", MDI_ICON_MAGNIFY "  Search entities...", s_search_filter, sizeof(s_search_filter));

    ImGui::Separator();

    bool hierarchy_clicked = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0);

    if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Drop target on Scene root: unparent entity or instantiate prefab
        if (!context.read_only && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string asset_path(static_cast<const char*>(payload->Data));
                if (asset_path.size() > 8 && asset_path.substr(asset_path.size() - 8) == ".dprefab") {
                    std::filesystem::path full_path = std::filesystem::current_path() / "samples" / "lua" / "data" / asset_path;
                    entt::entity new_ent = InstantiatePrefab(context.world, context.registry, full_path.string());
                    if (new_ent != entt::null) {
                        context.selected_entity = new_ent;
                        EditorLog(LogLevel::Info, "Instantiated prefab: " + asset_path);
                    }
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                entt::entity dragged = *static_cast<const entt::entity*>(payload->Data);
                if (context.registry.valid(dragged) && context.registry.all_of<ParentComponent>(dragged)) {
                    entt::entity old_parent = context.registry.get<ParentComponent>(dragged).parent;
                    context.registry.remove<ParentComponent>(dragged);
                    auto& reg = context.registry;
                    GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
                        "Unparent Entity",
                        []{},
                        [&reg, dragged, old_parent]() {
                            if (!reg.valid(dragged)) return;
                            if (reg.all_of<ParentComponent>(dragged)) {
                                reg.get<ParentComponent>(dragged).parent = old_parent;
                            } else {
                                reg.emplace<ParentComponent>(dragged, old_parent);
                            }
                        }), false);
                    EditorLog(LogLevel::Info, "Unparented entity");
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Draw root-level entities (those without a parent)
        for (auto entity : context.registry.storage<entt::entity>()) {
            if (!context.registry.valid(entity)) continue;
            if (context.registry.all_of<ParentComponent>(entity) &&
                context.registry.get<ParentComponent>(entity).parent != entt::null) {
                continue;
            }
            DrawEntityNode(context, entity);
        }
        ImGui::TreePop();
    }

    if (hierarchy_clicked) {
        context.selected_entity = entt::null;
        SelectionManager::Get().Clear();
    }

    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Create Empty Entity", nullptr, false, !context.read_only)) {
            auto new_ent = context.world.CreateEntity();
            context.registry.emplace<EditorNameComponent>(new_ent, "New Entity");
            context.registry.emplace<TransformComponent>(new_ent);
            context.selected_entity = new_ent;
            EditorLog(LogLevel::Info, "Created entity: New Entity");
            auto& world_ref = context.world;
            auto& sel_ref = context.selected_entity;
            GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
                "Create Empty Entity",
                []{},
                [&world_ref, &sel_ref, new_ent]() {
                    world_ref.DestroyEntity(new_ent);
                    if (sel_ref == new_ent) sel_ref = entt::null;
                }), false);
        }
        if (ImGui::MenuItem("Create UI Entity", nullptr, false, !context.read_only)) {
            auto new_ent = context.world.CreateEntity();
            context.registry.emplace<EditorNameComponent>(new_ent, "New UI Element");
            context.registry.emplace<TransformComponent>(new_ent);
            context.registry.emplace<UIRendererComponent>(new_ent);
            context.selected_entity = new_ent;
        }
        if (ImGui::BeginMenu("Create 3D Object", !context.read_only)) {
            if (ImGui::MenuItem("Camera 3D", nullptr, false, !context.read_only)) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Camera 3D");
                context.registry.emplace<TransformComponent>(new_ent, glm::vec3(0, 0, 5));
                context.registry.emplace<dse::Camera3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            if (ImGui::MenuItem("Directional Light", nullptr, false, !context.read_only)) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Directional Light");
                auto& t = context.registry.emplace<TransformComponent>(new_ent, glm::vec3(0, 5, 0));
                t.rotation = glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(-30.0f), 0.0f));
                context.registry.emplace<dse::DirectionalLight3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            if (ImGui::MenuItem("Cube", nullptr, false, !context.read_only)) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Cube");
                context.registry.emplace<TransformComponent>(new_ent);
                auto& mesh = context.registry.emplace<dse::MeshRendererComponent>(new_ent);
                mesh.mesh_path = "data/models/cube.dmesh";
                context.selected_entity = new_ent;
            }
            if (ImGui::MenuItem("Physics Box", nullptr, false, !context.read_only)) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Physics Box");
                context.registry.emplace<TransformComponent>(new_ent, glm::vec3(0, 5, 0));
                auto& mesh = context.registry.emplace<dse::MeshRendererComponent>(new_ent);
                mesh.mesh_path = "data/models/cube.dmesh";
                context.registry.emplace<dse::RigidBody3DComponent>(new_ent);
                context.registry.emplace<dse::BoxCollider3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            if (ImGui::MenuItem("Particle System 3D", nullptr, false, !context.read_only)) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Particle 3D");
                context.registry.emplace<TransformComponent>(new_ent);
                context.registry.emplace<dse::ParticleSystem3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            ImGui::EndMenu();
        }
        if (context.selected_entity != entt::null && ImGui::MenuItem("Save as Prefab", nullptr, false, !context.read_only)) {
            std::string prefab_name = "Entity";
            if (context.registry.all_of<EditorNameComponent>(context.selected_entity)) {
                prefab_name = context.registry.get<EditorNameComponent>(context.selected_entity).name;
            }
            std::filesystem::path prefab_dir = std::filesystem::current_path() / "samples" / "lua" / "data" / "prefabs";
            std::filesystem::create_directories(prefab_dir);
            std::string prefab_path = (prefab_dir / (prefab_name + ".dprefab")).string();
            if (SaveEntityAsPrefab(context.registry, context.selected_entity, prefab_path)) {
                EditorLog(LogLevel::Info, "Saved prefab: " + prefab_path);
            } else {
                EditorLog(LogLevel::Error, "Failed to save prefab");
            }
        }
        if (context.selected_entity != entt::null && ImGui::MenuItem("Delete Entity", nullptr, false, !context.read_only)) {
            entt::entity to_delete = context.selected_entity;
            std::string deleted_name = "Entity";
            if (context.registry.all_of<EditorNameComponent>(to_delete)) {
                deleted_name = context.registry.get<EditorNameComponent>(to_delete).name;
            }
            TransformComponent deleted_transform{};
            if (context.registry.all_of<TransformComponent>(to_delete)) {
                deleted_transform = context.registry.get<TransformComponent>(to_delete);
            }
            context.world.DestroyEntity(to_delete);
            context.selected_entity = entt::null;
            EditorLog(LogLevel::Info, "Deleted entity: " + deleted_name);
            auto& world_ref = context.world;
            auto& reg_ref = context.registry;
            auto& sel_ref = context.selected_entity;
            GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
                "Delete Entity: " + deleted_name,
                []{},
                [&world_ref, &reg_ref, &sel_ref, deleted_name, deleted_transform]() {
                    auto restored = world_ref.CreateEntity();
                    reg_ref.emplace<EditorNameComponent>(restored, deleted_name);
                    reg_ref.emplace<TransformComponent>(restored, deleted_transform);
                    sel_ref = restored;
                }), false);
        }
        if (context.selected_entity != entt::null && ImGui::MenuItem("Duplicate Entity", nullptr, false, !context.read_only)) {
            const entt::entity source = context.selected_entity;
            auto new_ent = context.world.CreateEntity();
            auto copy_component = [&](auto type_tag) {
                using Component = decltype(type_tag);
                if (context.registry.all_of<Component>(source) && !context.registry.all_of<Component>(new_ent)) {
                    context.registry.emplace<Component>(new_ent, context.registry.get<Component>(source));
                }
            };
            auto copy_runtime_reset_component = [&](auto type_tag, auto reset_runtime) {
                using Component = decltype(type_tag);
                if (context.registry.all_of<Component>(source) && !context.registry.all_of<Component>(new_ent)) {
                    auto component = context.registry.get<Component>(source);
                    reset_runtime(component);
                    context.registry.emplace<Component>(new_ent, std::move(component));
                }
            };

            copy_component(EditorNameComponent{});
            if (context.registry.all_of<EditorNameComponent>(new_ent)) {
                context.registry.get<EditorNameComponent>(new_ent).name += " (Copy)";
            }

            copy_component(TransformComponent{});
            copy_component(SpriteRendererComponent{});
            copy_runtime_reset_component(UIRendererComponent{}, [](UIRendererComponent& ui) {
                ui.is_hovered = false;
                ui.is_pressed = false;
                ui.runtime_model = glm::mat4(1.0f);
            });
            copy_runtime_reset_component(UILabelComponent{}, [](UILabelComponent& label) {
                label.runtime_glyph_entities.clear();
                label.dirty = true;
            });
            copy_component(UIAnchorComponent{});
            copy_component(UIGridLayoutComponent{});
            copy_component(UICanvasScalerComponent{});
            copy_component(UIAnimationComponent{});
            copy_runtime_reset_component(UIRichTextComponent{}, [](UIRichTextComponent& rich) {
                rich.dirty = true;
            });
            copy_runtime_reset_component(RigidBody2DComponent{}, [](RigidBody2DComponent& rigidbody) {
                rigidbody.runtime_body = nullptr;
            });
            copy_runtime_reset_component(ParticleEmitterComponent{}, [](ParticleEmitterComponent& emitter) {
                emitter.particles.clear();
                emitter.emit_accumulator = 0.0f;
                emitter.pending_burst = 0;
            });
            copy_component(dse::Camera3DComponent{});
            copy_component(dse::DirectionalLight3DComponent{});
            copy_component(dse::PointLightComponent{});
            copy_component(dse::MeshRendererComponent{});
            copy_component(dse::Animator3DComponent{});
            copy_component(dse::FreeCameraControllerComponent{});
            copy_component(dse::TerrainComponent{});
            copy_runtime_reset_component(dse::RigidBody3DComponent{}, [](dse::RigidBody3DComponent& rigidbody) {
                rigidbody.runtime_body = nullptr;
            });
            copy_runtime_reset_component(dse::BoxCollider3DComponent{}, [](dse::BoxCollider3DComponent& collider) {
                collider.runtime_shape = nullptr;
            });
            copy_runtime_reset_component(dse::SphereCollider3DComponent{}, [](dse::SphereCollider3DComponent& collider) {
                collider.runtime_shape = nullptr;
            });
            copy_runtime_reset_component(dse::ParticleSystem3DComponent{}, [](dse::ParticleSystem3DComponent& particle_system) {
                particle_system.particles.clear();
                particle_system.emission_accumulator = 0.0f;
                particle_system.active_particle_count = 0;
                particle_system.instance_vbo = 0;
                particle_system.texture_handle = 0;
                particle_system.initialized = false;
            });
            copy_component(dse::PostProcessComponent{});

            if (context.registry.all_of<TransformComponent>(new_ent)) {
                context.registry.get<TransformComponent>(new_ent).dirty = true;
            }
            context.selected_entity = new_ent;
            EditorLog(LogLevel::Info, "Duplicated entity");
            auto& world_ref = context.world;
            auto& sel_ref = context.selected_entity;
            GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
                "Duplicate Entity",
                []{},
                [&world_ref, &sel_ref, new_ent]() {
                    world_ref.DestroyEntity(new_ent);
                    if (sel_ref == new_ent) sel_ref = entt::null;
                }), false);
        }
        if (context.read_only) {
            ImGui::Separator();
            ImGui::TextDisabled("Play 模式下已禁用层级创建、删除、复制操作。");
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace dse::editor
