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

namespace dse::editor {

namespace {

/// Duplicate the currently selected entity (mirrors Hierarchy panel logic)
void DuplicateSelectedEntity(ShortcutContext& context) {
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

/// Delete the currently selected entity
void DeleteSelectedEntity(ShortcutContext& context) {
    if (context.selected_entity == entt::null || !context.registry.valid(context.selected_entity)) {
        return;
    }
    context.world.DestroyEntity(context.selected_entity);
    context.selected_entity = entt::null;
}

} // namespace

UndoRedoManager& GetUndoRedoManager() {
    static UndoRedoManager instance(200);
    return instance;
}

void ProcessShortcuts(ShortcutContext& context) {
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
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
            SaveScene(context.registry, "scene.json");
            SetCurrentScenePath("scene.json");
            EditorLog(LogLevel::Info, "Scene saved: scene.json");
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
            std::string path = SaveSceneFileDialog();
            if (!path.empty()) {
                SaveScene(context.registry, path);
                SetCurrentScenePath(path);
                EditorLog(LogLevel::Info, "Scene saved: " + path);
            }
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) {
            std::string path = OpenSceneFileDialog();
            if (!path.empty()) {
                LoadScene(context.registry, path);
                context.selected_entity = entt::null;
                SetCurrentScenePath(path);
                EditorLog(LogLevel::Info, "Scene loaded: " + path);
            }
        }
    }

    // --- Entity operations (blocked in Play mode) ---
    if (!context.read_only) {
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal)) {
            DuplicateSelectedEntity(context);
            EditorLog(LogLevel::Info, "Entity duplicated");
        }
        if (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteGlobal)) {
            DeleteSelectedEntity(context);
            EditorLog(LogLevel::Info, "Entity deleted");
        }
        if (ImGui::Shortcut(ImGuiKey_F2, ImGuiInputFlags_RouteGlobal)) {
            // F2 rename: handled by focusing the Inspector name field
            // For now we just select the entity — the Inspector's InputText will
            // be focused when we add the rename-in-place feature to Hierarchy.
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
