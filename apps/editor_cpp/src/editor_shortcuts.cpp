#include "editor_shortcuts.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "editor_scene_io.h"
#include "editor_shared_components.h"
#include "editor_toolbar.h"
#include "editor_scene_camera.h"
#include "editor_file_dialog.h"
#include "editor_console_panel.h"
#include "editor_shell.h"
#include "editor_selection.h"
#include "editor_scene_tabs.h"
#include "editor_settings.h"
#include "editor_hierarchy_panel.h"

namespace dse::editor {

void CreateEmptyEntity(EditorContext& context) {
    auto new_ent = context.world.CreateEntity();
    context.registry.emplace<EditorNameComponent>(new_ent, "New Entity");
    context.registry.emplace<TransformComponent>(new_ent);
    SelectionManager::Get().SetSingle(new_ent);
    context.selected_entity = new_ent;
    EditorLog(LogLevel::Info, "Created empty entity");
}

void DuplicateSelectedEntity(EditorContext& context) {
    if (context.selected_entity == entt::null || !context.registry.valid(context.selected_entity)) {
        return;
    }

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
    copy_runtime_reset_component(dse::ParticleSystem3DComponent{}, [](dse::ParticleSystem3DComponent& ps) {
        ps.particles.clear();
        ps.emission_accumulator = 0.0f;
        ps.active_particle_count = 0;
        ps.instance_vbo = 0;
        ps.texture_handle = 0;
        ps.initialized = false;
    });
    copy_component(dse::PostProcessComponent{});

    if (context.registry.all_of<TransformComponent>(new_ent)) {
        context.registry.get<TransformComponent>(new_ent).dirty = true;
    }
    context.selected_entity = new_ent;
}

void DeleteSelectedEntity(EditorContext& context) {
    if (context.selected_entity == entt::null || !context.registry.valid(context.selected_entity)) {
        return;
    }
    context.world.DestroyEntity(context.selected_entity);
    context.selected_entity = entt::null;
}

UndoRedoManager& GetUndoRedoManager() {
    static UndoRedoManager instance(200);
    return instance;
}

// ─── Clipboard (Copy/Cut/Paste) ────────────────────────────────────────────
static std::unique_ptr<entt::registry> s_clipboard_registry;
static bool s_clipboard_has_data = false;

void CopySelectedEntity(EditorContext& ctx) {
    if (ctx.selected_entity == entt::null || !ctx.registry.valid(ctx.selected_entity)) return;
    s_clipboard_registry = std::make_unique<entt::registry>();
    entt::entity src = ctx.selected_entity;
    entt::entity dst = s_clipboard_registry->create();

    auto copy_c = [&](auto tag) {
        using C = decltype(tag);
        if (ctx.registry.all_of<C>(src)) s_clipboard_registry->emplace<C>(dst, ctx.registry.get<C>(src));
    };
    copy_c(EditorNameComponent{});
    copy_c(TransformComponent{});
    copy_c(SpriteRendererComponent{});
    copy_c(UIRendererComponent{});
    copy_c(UILabelComponent{});
    copy_c(UIAnchorComponent{});
    copy_c(UIGridLayoutComponent{});
    copy_c(UICanvasScalerComponent{});
    copy_c(UIAnimationComponent{});
    copy_c(UIRichTextComponent{});
    copy_c(RigidBody2DComponent{});
    copy_c(ParticleEmitterComponent{});
    copy_c(dse::Camera3DComponent{});
    copy_c(dse::DirectionalLight3DComponent{});
    copy_c(dse::PointLightComponent{});
    copy_c(dse::MeshRendererComponent{});
    copy_c(dse::Animator3DComponent{});
    copy_c(dse::FreeCameraControllerComponent{});
    copy_c(dse::TerrainComponent{});
    copy_c(dse::RigidBody3DComponent{});
    copy_c(dse::BoxCollider3DComponent{});
    copy_c(dse::SphereCollider3DComponent{});
    copy_c(dse::ParticleSystem3DComponent{});
    copy_c(dse::PostProcessComponent{});

    s_clipboard_has_data = true;
    EditorLog(LogLevel::Info, "Entity copied to clipboard");
}

void CutSelectedEntity(EditorContext& ctx) {
    CopySelectedEntity(ctx);
    DeleteSelectedEntity(ctx);
    SelectionManager::Get().Clear();
    EditorLog(LogLevel::Info, "Entity cut to clipboard");
}

void PasteEntity(EditorContext& ctx) {
    if (!s_clipboard_has_data || !s_clipboard_registry) return;
    auto clipboard_view = s_clipboard_registry->view<EditorNameComponent>();
    for (auto src_ent : clipboard_view) {
        auto new_ent = ctx.world.CreateEntity();
        auto paste_c = [&](auto tag) {
            using C = decltype(tag);
            if (s_clipboard_registry->all_of<C>(src_ent))
                ctx.registry.emplace_or_replace<C>(new_ent, s_clipboard_registry->get<C>(src_ent));
        };
        paste_c(EditorNameComponent{});
        paste_c(TransformComponent{});
        paste_c(SpriteRendererComponent{});
        paste_c(UIRendererComponent{});
        paste_c(UILabelComponent{});
        paste_c(UIAnchorComponent{});
        paste_c(UIGridLayoutComponent{});
        paste_c(UICanvasScalerComponent{});
        paste_c(UIAnimationComponent{});
        paste_c(UIRichTextComponent{});
        paste_c(RigidBody2DComponent{});
        paste_c(ParticleEmitterComponent{});
        paste_c(dse::Camera3DComponent{});
        paste_c(dse::DirectionalLight3DComponent{});
        paste_c(dse::PointLightComponent{});
        paste_c(dse::MeshRendererComponent{});
        paste_c(dse::Animator3DComponent{});
        paste_c(dse::FreeCameraControllerComponent{});
        paste_c(dse::TerrainComponent{});
        paste_c(dse::RigidBody3DComponent{});
        paste_c(dse::BoxCollider3DComponent{});
        paste_c(dse::SphereCollider3DComponent{});
        paste_c(dse::ParticleSystem3DComponent{});
        paste_c(dse::PostProcessComponent{});

        if (ctx.registry.all_of<EditorNameComponent>(new_ent)) {
            ctx.registry.get<EditorNameComponent>(new_ent).name += " (Paste)";
        }
        if (ctx.registry.all_of<TransformComponent>(new_ent)) {
            ctx.registry.get<TransformComponent>(new_ent).dirty = true;
        }
        ctx.selected_entity = new_ent;
    }
    EditorLog(LogLevel::Info, "Entity pasted from clipboard");
}

bool HasEntityClipboard() {
    return s_clipboard_has_data && s_clipboard_registry != nullptr;
}

// ─── Entity Templates ──────────────────────────────────────────────────────
void CreateEntity3DCube(EditorContext& ctx) {
    auto ent = ctx.world.CreateEntity();
    ctx.registry.emplace<EditorNameComponent>(ent, "Cube");
    ctx.registry.emplace<TransformComponent>(ent);
    auto& mesh = ctx.registry.emplace<dse::MeshRendererComponent>(ent);
    mesh.mesh_path = "primitives/cube.dmesh";
    SelectionManager::Get().SetSingle(ent);
    ctx.selected_entity = ent;
    EditorLog(LogLevel::Info, "Created Cube entity");
}

void CreateEntity3DSphere(EditorContext& ctx) {
    auto ent = ctx.world.CreateEntity();
    ctx.registry.emplace<EditorNameComponent>(ent, "Sphere");
    ctx.registry.emplace<TransformComponent>(ent);
    auto& mesh = ctx.registry.emplace<dse::MeshRendererComponent>(ent);
    mesh.mesh_path = "primitives/sphere.dmesh";
    SelectionManager::Get().SetSingle(ent);
    ctx.selected_entity = ent;
    EditorLog(LogLevel::Info, "Created Sphere entity");
}

void CreateEntity3DPlane(EditorContext& ctx) {
    auto ent = ctx.world.CreateEntity();
    ctx.registry.emplace<EditorNameComponent>(ent, "Plane");
    auto& transform = ctx.registry.emplace<TransformComponent>(ent);
    transform.scale = glm::vec3(10.0f, 1.0f, 10.0f);
    auto& mesh = ctx.registry.emplace<dse::MeshRendererComponent>(ent);
    mesh.mesh_path = "primitives/plane.dmesh";
    SelectionManager::Get().SetSingle(ent);
    ctx.selected_entity = ent;
    EditorLog(LogLevel::Info, "Created Plane entity");
}

void CreateEntity3DCamera(EditorContext& ctx) {
    auto ent = ctx.world.CreateEntity();
    ctx.registry.emplace<EditorNameComponent>(ent, "Camera");
    auto& transform = ctx.registry.emplace<TransformComponent>(ent);
    transform.position = glm::vec3(0.0f, 2.0f, 5.0f);
    ctx.registry.emplace<dse::Camera3DComponent>(ent);
    SelectionManager::Get().SetSingle(ent);
    ctx.selected_entity = ent;
    EditorLog(LogLevel::Info, "Created Camera entity");
}

void CreateEntity3DDirectionalLight(EditorContext& ctx) {
    auto ent = ctx.world.CreateEntity();
    ctx.registry.emplace<EditorNameComponent>(ent, "Directional Light");
    ctx.registry.emplace<TransformComponent>(ent);
    ctx.registry.emplace<dse::DirectionalLight3DComponent>(ent);
    SelectionManager::Get().SetSingle(ent);
    ctx.selected_entity = ent;
    EditorLog(LogLevel::Info, "Created Directional Light entity");
}

void CreateEntity3DPointLight(EditorContext& ctx) {
    auto ent = ctx.world.CreateEntity();
    ctx.registry.emplace<EditorNameComponent>(ent, "Point Light");
    auto& transform = ctx.registry.emplace<TransformComponent>(ent);
    transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    ctx.registry.emplace<dse::PointLightComponent>(ent);
    SelectionManager::Get().SetSingle(ent);
    ctx.selected_entity = ent;
    EditorLog(LogLevel::Info, "Created Point Light entity");
}

void CreateEntity2DSprite(EditorContext& ctx) {
    auto ent = ctx.world.CreateEntity();
    ctx.registry.emplace<EditorNameComponent>(ent, "Sprite");
    ctx.registry.emplace<TransformComponent>(ent);
    ctx.registry.emplace<SpriteRendererComponent>(ent);
    SelectionManager::Get().SetSingle(ent);
    ctx.selected_entity = ent;
    EditorLog(LogLevel::Info, "Created Sprite entity");
}

void ProcessShortcuts(EditorContext& context) {
    // Don't process shortcuts when a text input is active
    if (ImGui::GetIO().WantTextInput) {
        return;
    }

    // --- Undo / Redo (always available, even in Play mode) ---
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) {
        GetUndoRedoManager().Undo();
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal)) {
        GetUndoRedoManager().Redo();
    }

    // --- Scene file operations (blocked in Play mode) ---
    if (!context.read_only) {
        auto& tab_mgr = SceneTabManager::Get();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal)) {
            tab_mgr.NewScene(context.registry);
            context.selected_entity = entt::null;
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
            const std::string& current_path = tab_mgr.GetActiveFilePath();
            if (current_path.empty()) {
                std::string path = SaveSceneFileDialog();
                if (!path.empty()) {
                    SaveScene(context.registry, path);
                    tab_mgr.SetCurrentPath(path);
                    tab_mgr.MarkClean();
                    EditorLog(LogLevel::Info, "Scene saved: " + path);
                }
            } else {
                SaveScene(context.registry, current_path);
                tab_mgr.MarkClean();
                EditorLog(LogLevel::Info, "Scene saved: " + current_path);
            }
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
            std::string path = SaveSceneFileDialog();
            if (!path.empty()) {
                SaveScene(context.registry, path);
                tab_mgr.SetCurrentPath(path);
                tab_mgr.MarkClean();
                EditorLog(LogLevel::Info, "Scene saved: " + path);
            }
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) {
            std::string path = OpenSceneFileDialog();
            if (!path.empty()) {
                tab_mgr.OpenScene(context.registry, path);
                context.selected_entity = entt::null;
                EditorSettings settings = LoadEditorSettings();
                AddRecentFile(settings, path);
                SaveEditorSettings(settings);
            }
        }
    }

    // --- Copy/Cut/Paste (blocked in Play mode) ---
    if (!context.read_only) {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_RouteGlobal)) {
            CopySelectedEntity(context);
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X, ImGuiInputFlags_RouteGlobal)) {
            CutSelectedEntity(context);
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_RouteGlobal)) {
            PasteEntity(context);
        }
    }

    // --- Entity operations (blocked in Play mode) ---
    if (!context.read_only) {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal)) {
            auto& sel = SelectionManager::Get();
            if (sel.IsMultiSelect()) {
                auto entities = sel.GetAll();
                for (auto ent : entities) {
                    context.selected_entity = ent;
                    DuplicateSelectedEntity(context);
                }
                EditorLog(LogLevel::Info, "Duplicated " + std::to_string(entities.size()) + " entities");
            } else {
                DuplicateSelectedEntity(context);
                EditorLog(LogLevel::Info, "Entity duplicated");
            }
        }
        if (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteGlobal)) {
            auto& sel = SelectionManager::Get();
            if (sel.IsMultiSelect()) {
                auto entities = sel.GetAll();
                for (auto ent : entities) {
                    if (context.registry.valid(ent)) {
                        context.world.DestroyEntity(ent);
                    }
                }
                sel.Clear();
                context.selected_entity = entt::null;
                EditorLog(LogLevel::Info, "Deleted " + std::to_string(entities.size()) + " entities");
            } else {
                DeleteSelectedEntity(context);
                sel.Clear();
                EditorLog(LogLevel::Info, "Entity deleted");
            }
        }
        if (ImGui::Shortcut(ImGuiKey_F2, ImGuiInputFlags_RouteGlobal)) {
            if (context.selected_entity != entt::null &&
                context.registry.valid(context.selected_entity)) {
                std::string name = "Entity";
                if (context.registry.all_of<EditorNameComponent>(context.selected_entity)) {
                    name = context.registry.get<EditorNameComponent>(context.selected_entity).name;
                }
                BeginHierarchyRename(context.selected_entity, name);
            }
        }
    }

    // F key → Focus selected entity (editor camera fly-to)
    if (ImGui::Shortcut(ImGuiKey_F, ImGuiInputFlags_RouteGlobal)) {
        if (context.selected_entity != entt::null &&
            context.registry.valid(context.selected_entity) &&
            context.registry.all_of<TransformComponent>(context.selected_entity)) {
            auto& transform = context.registry.get<TransformComponent>(context.selected_entity);
            FocusEditorCamera(GetEditorCamera(), transform.position);
        }
    }
}

} // namespace dse::editor
