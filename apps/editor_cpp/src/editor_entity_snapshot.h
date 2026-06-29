#pragma once

#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/script.h"

#include <optional>
#include <string>

namespace dse::editor {

/// Captures and restores the full component state of a single entity for Undo/Redo.
/// Runtime state (body pointers, GPU handles) is reset on restore.
struct EntitySnapshot {
    // Core
    std::optional<EditorNameComponent> name;
    std::optional<TransformComponent> transform;
    std::optional<ScriptComponent> script;

    // 2D
    std::optional<SpriteRendererComponent> sprite_renderer;
    std::optional<UIRendererComponent> ui_renderer;
    std::optional<UILabelComponent> ui_label;
    std::optional<UIAnchorComponent> ui_anchor;
    std::optional<UIGridLayoutComponent> ui_grid_layout;
    std::optional<UICanvasScalerComponent> ui_canvas_scaler;
    std::optional<UIAnimationComponent> ui_animation;
    std::optional<UIRichTextComponent> ui_rich_text;
    std::optional<RigidBody2DComponent> rigidbody_2d;
    std::optional<ParticleEmitterComponent> particle_emitter;

    // 3D
    std::optional<dse::Camera3DComponent> camera_3d;
    std::optional<dse::DirectionalLight3DComponent> directional_light;
    std::optional<dse::PointLightComponent> point_light;
    std::optional<dse::SpotLightComponent> spot_light;
    std::optional<dse::MeshRendererComponent> mesh_renderer;
    std::optional<dse::Animator3DComponent> animator_3d;
    std::optional<dse::FreeCameraControllerComponent> free_camera;
    std::optional<dse::TerrainComponent> terrain;
    std::optional<dse::SubSceneComponent> sub_scene;
    std::optional<dse::PostProcessComponent> post_process;

    // 3D Physics
    std::optional<dse::RigidBody3DComponent> rigidbody_3d;
    std::optional<dse::BoxCollider3DComponent> box_collider_3d;
    std::optional<dse::SphereCollider3DComponent> sphere_collider_3d;
    std::optional<dse::CapsuleCollider3DComponent> capsule_collider_3d;
    std::optional<dse::MeshCollider3DComponent> mesh_collider_3d;

    // Audio
    std::optional<AudioSourceComponent> audio_source;
    std::optional<AudioListenerComponent> audio_listener;

    // 3D Particles
    std::optional<dse::ParticleSystem3DComponent> particle_system_3d;

    // Hierarchy
    std::optional<ParentComponent> parent;
    std::optional<SiblingIndexComponent> sibling_index;

    /// Capture all components from entity
    static EntitySnapshot Capture(entt::registry& reg, entt::entity entity) {
        EntitySnapshot snap;
        auto capture = [&](auto& field, auto type_tag) {
            using C = std::decay_t<decltype(type_tag)>;
            if (reg.all_of<C>(entity)) field = reg.get<C>(entity);
        };
        capture(snap.name, EditorNameComponent{});
        capture(snap.transform, TransformComponent{});
        capture(snap.script, ScriptComponent{});
        capture(snap.sprite_renderer, SpriteRendererComponent{});
        capture(snap.ui_renderer, UIRendererComponent{});
        capture(snap.ui_label, UILabelComponent{});
        capture(snap.ui_anchor, UIAnchorComponent{});
        capture(snap.ui_grid_layout, UIGridLayoutComponent{});
        capture(snap.ui_canvas_scaler, UICanvasScalerComponent{});
        capture(snap.ui_animation, UIAnimationComponent{});
        capture(snap.ui_rich_text, UIRichTextComponent{});
        capture(snap.rigidbody_2d, RigidBody2DComponent{});
        capture(snap.particle_emitter, ParticleEmitterComponent{});
        capture(snap.camera_3d, dse::Camera3DComponent{});
        capture(snap.directional_light, dse::DirectionalLight3DComponent{});
        capture(snap.point_light, dse::PointLightComponent{});
        capture(snap.spot_light, dse::SpotLightComponent{});
        capture(snap.mesh_renderer, dse::MeshRendererComponent{});
        capture(snap.animator_3d, dse::Animator3DComponent{});
        capture(snap.free_camera, dse::FreeCameraControllerComponent{});
        capture(snap.terrain, dse::TerrainComponent{});
        capture(snap.sub_scene, dse::SubSceneComponent{});
        capture(snap.post_process, dse::PostProcessComponent{});
        capture(snap.rigidbody_3d, dse::RigidBody3DComponent{});
        capture(snap.box_collider_3d, dse::BoxCollider3DComponent{});
        capture(snap.sphere_collider_3d, dse::SphereCollider3DComponent{});
        capture(snap.capsule_collider_3d, dse::CapsuleCollider3DComponent{});
        capture(snap.mesh_collider_3d, dse::MeshCollider3DComponent{});
        capture(snap.audio_source, AudioSourceComponent{});
        capture(snap.audio_listener, AudioListenerComponent{});
        capture(snap.particle_system_3d, dse::ParticleSystem3DComponent{});
        capture(snap.parent, ParentComponent{});
        capture(snap.sibling_index, SiblingIndexComponent{});
        return snap;
    }

    /// Restore entity with captured components, resetting runtime state
    entt::entity Restore(World& world, entt::registry& reg) const {
        auto entity = world.CreateEntity();
        auto restore = [&](const auto& field, auto reset_fn) {
            using Opt = std::decay_t<decltype(field)>;
            using C = typename Opt::value_type;
            if (field.has_value()) {
                auto comp = field.value();
                reset_fn(comp);
                reg.emplace<C>(entity, std::move(comp));
            }
        };
        auto noop = [](auto&) {};

        restore(name, noop);
        restore(transform, [](TransformComponent& t) { t.dirty = true; });
        restore(script, noop);
        restore(sprite_renderer, noop);
        restore(ui_renderer, [](UIRendererComponent& u) {
            u.is_hovered = false; u.is_pressed = false;
            u.runtime_model = glm::mat4(1.0f);
        });
        restore(ui_label, [](UILabelComponent& l) {
            l.runtime_glyph_entities.clear(); l.dirty = true;
        });
        restore(ui_anchor, noop);
        restore(ui_grid_layout, noop);
        restore(ui_canvas_scaler, noop);
        restore(ui_animation, noop);
        restore(ui_rich_text, [](UIRichTextComponent& r) { r.dirty = true; });
        restore(rigidbody_2d, [](RigidBody2DComponent& r) { r.runtime_body = nullptr; });
        restore(particle_emitter, [](ParticleEmitterComponent& p) {
            p.particles.clear(); p.emit_accumulator = 0.0f; p.pending_burst = 0;
        });
        restore(camera_3d, noop);
        restore(directional_light, noop);
        restore(point_light, noop);
        restore(spot_light, noop);
        restore(mesh_renderer, noop);
        restore(animator_3d, noop);
        restore(free_camera, noop);
        restore(terrain, noop);
        restore(sub_scene, noop);
        restore(post_process, noop);
        restore(rigidbody_3d, [](dse::RigidBody3DComponent& r) { r.runtime_body = nullptr; });
        restore(box_collider_3d, [](dse::BoxCollider3DComponent& c) { c.runtime_shape = nullptr; });
        restore(sphere_collider_3d, [](dse::SphereCollider3DComponent& c) { c.runtime_shape = nullptr; });
        restore(capsule_collider_3d, [](dse::CapsuleCollider3DComponent& c) { c.runtime_shape = nullptr; });
        restore(mesh_collider_3d, [](dse::MeshCollider3DComponent& c) { c.runtime_shape = nullptr; });
        restore(audio_source, [](AudioSourceComponent& a) {
            a.runtime_handle = 0; a.is_playing = false; a.restart_requested = false;
        });
        restore(audio_listener, noop);
        restore(particle_system_3d, [](dse::ParticleSystem3DComponent& p) {
            p.particles.clear(); p.emission_accumulator = 0.0f;
            p.active_particle_count = 0; p.instance_vbo = 0;
            p.texture_handle = 0; p.initialized = false;
        });
        restore(parent, noop);
        restore(sibling_index, noop);
        return entity;
    }

    /// Re-apply, onto an EXISTING entity, only those captured components that are
    /// currently missing. Used to undo a single component removal: the removed type
    /// is the only one absent, so it is restored with its captured data while every
    /// other component (possibly edited since) is left untouched.
    void RestoreMissingInPlace(entt::registry& reg, entt::entity entity) const {
        auto restore = [&](const auto& field, auto reset_fn) {
            using Opt = std::decay_t<decltype(field)>;
            using C = typename Opt::value_type;
            if (field.has_value() && !reg.all_of<C>(entity)) {
                auto comp = field.value();
                reset_fn(comp);
                reg.emplace<C>(entity, std::move(comp));
            }
        };
        auto noop = [](auto&) {};

        restore(name, noop);
        restore(transform, [](TransformComponent& t) { t.dirty = true; });
        restore(script, noop);
        restore(sprite_renderer, noop);
        restore(ui_renderer, [](UIRendererComponent& u) {
            u.is_hovered = false; u.is_pressed = false;
            u.runtime_model = glm::mat4(1.0f);
        });
        restore(ui_label, [](UILabelComponent& l) {
            l.runtime_glyph_entities.clear(); l.dirty = true;
        });
        restore(ui_anchor, noop);
        restore(ui_grid_layout, noop);
        restore(ui_canvas_scaler, noop);
        restore(ui_animation, noop);
        restore(ui_rich_text, [](UIRichTextComponent& r) { r.dirty = true; });
        restore(rigidbody_2d, [](RigidBody2DComponent& r) { r.runtime_body = nullptr; });
        restore(particle_emitter, [](ParticleEmitterComponent& p) {
            p.particles.clear(); p.emit_accumulator = 0.0f; p.pending_burst = 0;
        });
        restore(camera_3d, noop);
        restore(directional_light, noop);
        restore(point_light, noop);
        restore(spot_light, noop);
        restore(mesh_renderer, noop);
        restore(animator_3d, noop);
        restore(free_camera, noop);
        restore(terrain, noop);
        restore(sub_scene, noop);
        restore(post_process, noop);
        restore(rigidbody_3d, [](dse::RigidBody3DComponent& r) { r.runtime_body = nullptr; });
        restore(box_collider_3d, [](dse::BoxCollider3DComponent& c) { c.runtime_shape = nullptr; });
        restore(sphere_collider_3d, [](dse::SphereCollider3DComponent& c) { c.runtime_shape = nullptr; });
        restore(capsule_collider_3d, [](dse::CapsuleCollider3DComponent& c) { c.runtime_shape = nullptr; });
        restore(mesh_collider_3d, [](dse::MeshCollider3DComponent& c) { c.runtime_shape = nullptr; });
        restore(audio_source, [](AudioSourceComponent& a) {
            a.runtime_handle = 0; a.is_playing = false; a.restart_requested = false;
        });
        restore(audio_listener, noop);
        restore(particle_system_3d, [](dse::ParticleSystem3DComponent& p) {
            p.particles.clear(); p.emission_accumulator = 0.0f;
            p.active_particle_count = 0; p.instance_vbo = 0;
            p.texture_handle = 0; p.initialized = false;
        });
        restore(parent, noop);
        restore(sibling_index, noop);
    }
};

} // namespace dse::editor
