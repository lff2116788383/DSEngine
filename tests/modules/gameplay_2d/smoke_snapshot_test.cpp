#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include "modules/gameplay_2d/particle/particle_system.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "modules/gameplay_2d/ui/ui_system.h"

#include <cmath>
#include <sstream>
#include <string>

using dse::gameplay2d::TilemapSystem;
using dse::gameplay2d::UISystem;

namespace {

struct UiSmokeSnapshot {
    int clicks = 0;
    int enter_count = 0;
    int exit_count = 0;
    bool hovered = false;
    bool pressed = false;
    std::size_t glyph_count = 0;
    float scale = 0.0f;

    std::string ToDebugString() const {
        std::ostringstream oss;
        oss << "UiSmokeSnapshot{";
        oss << "clicks=" << clicks;
        oss << ", enter_count=" << enter_count;
        oss << ", exit_count=" << exit_count;
        oss << ", hovered=" << hovered;
        oss << ", pressed=" << pressed;
        oss << ", glyph_count=" << glyph_count;
        oss << ", scale=" << scale;
        oss << "}";
        return oss.str();
    }
};

struct Physics2DSmokeSnapshot {
    float start_y = 0.0f;
    float end_y = 0.0f;
    bool runtime_body_created = false;
    int collision_count = 0;
    bool raycast_hit = false;
    Entity hit_entity = entt::null;

    std::string ToDebugString() const {
        std::ostringstream oss;
        oss << "Physics2DSmokeSnapshot{";
        oss << "start_y=" << start_y;
        oss << ", end_y=" << end_y;
        oss << ", runtime_body_created=" << runtime_body_created;
        oss << ", collision_count=" << collision_count;
        oss << ", raycast_hit=" << raycast_hit;
        oss << ", hit_entity=" << static_cast<int>(hit_entity);
        oss << "}";
        return oss.str();
    }
};

struct TilemapSmokeSnapshot {
    bool dirty_after_update = true;
    std::size_t runtime_tile_count = 0;
    std::size_t collider_tile_count = 0;
    bool first_runtime_tile_valid = false;

    std::string ToDebugString() const {
        std::ostringstream oss;
        oss << "TilemapSmokeSnapshot{";
        oss << "dirty_after_update=" << dirty_after_update;
        oss << ", runtime_tile_count=" << runtime_tile_count;
        oss << ", collider_tile_count=" << collider_tile_count;
        oss << ", first_runtime_tile_valid=" << first_runtime_tile_valid;
        oss << "}";
        return oss.str();
    }
};

struct ParticleSmokeSnapshot {
    std::size_t particle_count = 0;
    bool first_particle_near_emitter = false;
    float first_particle_life = 0.0f;
    bool pending_burst_clamped = false;

    std::string ToDebugString() const {
        std::ostringstream oss;
        oss << "ParticleSmokeSnapshot{";
        oss << "particle_count=" << particle_count;
        oss << ", first_particle_near_emitter=" << first_particle_near_emitter;
        oss << ", first_particle_life=" << first_particle_life;
        oss << ", pending_burst_clamped=" << pending_burst_clamped;
        oss << "}";
        return oss.str();
    }
};

} // namespace

TEST_CASE("Smoke Snapshot - UI interaction and rich text remain deterministic", "[engine][smoke][snapshot][ui]") {
    World world;
    auto& reg = world.registry();

    auto button = world.CreateEntity();
    auto& ui = reg.emplace<UIRendererComponent>(button);
    ui.position = glm::vec2(160.0f, 120.0f);
    ui.size = glm::vec2(180.0f, 64.0f);
    ui.visible = true;
    ui.interactable = true;
    reg.emplace<UIButtonComponent>(button);

    int clicks = 0;
    int enters = 0;
    int exits = 0;
    ui.on_click = [&](Entity) { ++clicks; };
    ui.on_pointer_enter = [&](Entity) { ++enters; };
    ui.on_pointer_exit = [&](Entity) { ++exits; };

    auto label_entity = world.CreateEntity();
    auto& label_ui = reg.emplace<UIRendererComponent>(label_entity);
    label_ui.position = glm::vec2(100.0f, 60.0f);
    label_ui.size = glm::vec2(320.0f, 40.0f);
    label_ui.visible = true;
    auto& label = reg.emplace<UILabelComponent>(label_entity);
    label.text = "AB";
    label.dirty = true;
    auto& rich = reg.emplace<UIRichTextComponent>(label_entity);
    rich.text = "<color=#00ff00>A</color>B";
    rich.default_color = glm::vec4(1.0f);
    rich.dirty = true;

    UISystem system;
    const glm::vec2 screen(800.0f, 600.0f);
    system.Update(reg, 0.016f, screen, glm::vec2(160.0f, 120.0f), false);
    system.Update(reg, 0.016f, screen, glm::vec2(160.0f, 120.0f), true);
    system.Update(reg, 0.016f, screen, glm::vec2(160.0f, 120.0f), false);
    system.Update(reg, 0.016f, screen, glm::vec2(10.0f, 10.0f), false);

    UiSmokeSnapshot snapshot;
    snapshot.clicks = clicks;
    snapshot.enter_count = enters;
    snapshot.exit_count = exits;
    snapshot.hovered = ui.is_hovered;
    snapshot.pressed = ui.is_pressed;
    snapshot.glyph_count = label.runtime_glyph_entities.size();
    snapshot.scale = ui.scale;

    INFO(snapshot.ToDebugString());
    REQUIRE(snapshot.clicks == 1);
    REQUIRE(snapshot.enter_count == 1);
    REQUIRE(snapshot.exit_count == 1);
    REQUIRE_FALSE(snapshot.hovered);
    REQUIRE_FALSE(snapshot.pressed);
    REQUIRE(snapshot.glyph_count >= 2);
    REQUIRE(snapshot.scale >= 0.9f);
}

TEST_CASE("Smoke Snapshot - Physics2D gravity collision and raycast remain deterministic", "[engine][smoke][snapshot][physics2d]") {
    World world;
    Physics2DSystem physics_system;
    physics_system.Init(world);

    auto falling = world.CreateEntity();
    auto& falling_tf = world.registry().emplace<TransformComponent>(falling);
    falling_tf.position = glm::vec3(0.0f, 3.0f, 0.0f);
    auto& falling_rb = world.registry().emplace<RigidBody2DComponent>(falling);
    falling_rb.type = RigidBody2DType::Dynamic;
    auto& falling_box = world.registry().emplace<BoxCollider2DComponent>(falling);
    falling_box.size = glm::vec2(1.0f, 1.0f);

    auto floor = world.CreateEntity();
    auto& floor_tf = world.registry().emplace<TransformComponent>(floor);
    floor_tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& floor_rb = world.registry().emplace<RigidBody2DComponent>(floor);
    floor_rb.type = RigidBody2DType::Static;
    auto& floor_box = world.registry().emplace<BoxCollider2DComponent>(floor);
    floor_box.size = glm::vec2(6.0f, 1.0f);

    int collision_count = 0;
    falling_rb.on_collision_enter = [&](Entity other) {
        if (other == floor) {
            ++collision_count;
        }
    };

    const float start_y = falling_tf.position.y;
    for (int i = 0; i < 60; ++i) {
        physics_system.FixedUpdate(world, 1.0f / 60.0f);
    }

    Entity hit_entity = entt::null;
    glm::vec2 hit_point(0.0f);
    glm::vec2 hit_normal(0.0f);
    const bool hit = physics_system.Raycast(glm::vec2(0.0f, 5.0f), glm::vec2(0.0f, -2.0f), hit_entity, hit_point, hit_normal);

    Physics2DSmokeSnapshot snapshot;
    snapshot.start_y = start_y;
    snapshot.end_y = falling_tf.position.y;
    snapshot.runtime_body_created = (falling_rb.runtime_body != nullptr);
    snapshot.collision_count = collision_count;
    snapshot.raycast_hit = hit;
    snapshot.hit_entity = hit_entity;

    INFO(snapshot.ToDebugString());
    REQUIRE(snapshot.runtime_body_created);
    REQUIRE(snapshot.end_y < snapshot.start_y);
    REQUIRE(snapshot.collision_count >= 1);
    REQUIRE(snapshot.raycast_hit);
    REQUIRE((snapshot.hit_entity == floor || snapshot.hit_entity == falling));
}

TEST_CASE("Smoke Snapshot - Tilemap runtime tiles and colliders remain deterministic", "[engine][smoke][snapshot][tilemap]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(1.0f, 2.0f, 0.0f);
    tilemap.width = 3;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.tiles = {1, 0, 2};
    tilemap.generate_colliders = true;
    tilemap.collider_tile_min = 1;
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    TilemapSmokeSnapshot snapshot;
    snapshot.dirty_after_update = tilemap.dirty;
    snapshot.runtime_tile_count = tilemap.runtime_tile_entities.size();
    for (Entity runtime_tile : tilemap.runtime_tile_entities) {
        if (world.registry().all_of<RigidBody2DComponent, BoxCollider2DComponent>(runtime_tile)) {
            ++snapshot.collider_tile_count;
        }
    }
    snapshot.first_runtime_tile_valid = !tilemap.runtime_tile_entities.empty() && world.registry().valid(tilemap.runtime_tile_entities.front());

    INFO(snapshot.ToDebugString());
    REQUIRE_FALSE(snapshot.dirty_after_update);
    REQUIRE(snapshot.runtime_tile_count == 2);
    REQUIRE(snapshot.collider_tile_count == 2);
    REQUIRE(snapshot.first_runtime_tile_valid);
}

TEST_CASE("Smoke Snapshot - Particle emitter lifecycle remains deterministic", "[engine][smoke][snapshot][particle]") {
    World world;
    auto entity = world.CreateEntity();
    auto& emitter = world.registry().emplace<ParticleEmitterComponent>(entity);
    auto& transform = world.registry().emplace<TransformComponent>(entity);
    transform.position = glm::vec3(2.0f, 3.0f, 0.0f);
    emitter.emit_rate = 20.0f;
    emitter.emit_rate_scale = 1.0f;
    emitter.start_life_time = 1.5f;
    emitter.max_particles = 16;
    emitter.pending_burst = -3;
    emitter.emitting = true;

    ParticleSystem system;
    system.Update(world, 0.2f);

    ParticleSmokeSnapshot snapshot;
    snapshot.particle_count = emitter.particles.size();
    snapshot.pending_burst_clamped = (emitter.pending_burst == 0);
    if (!emitter.particles.empty()) {
        const auto& particle = emitter.particles.front();
        snapshot.first_particle_life = particle.life_time;
        snapshot.first_particle_near_emitter =
            std::fabs(particle.position.x - transform.position.x) < 2.0f &&
            std::fabs(particle.position.y - transform.position.y) < 2.0f;
    }

    INFO(snapshot.ToDebugString());
    REQUIRE(snapshot.particle_count > 0);
    REQUIRE(snapshot.particle_count <= 16);
    REQUIRE(snapshot.pending_burst_clamped);
    REQUIRE(snapshot.first_particle_life == Approx(1.5f));
    REQUIRE(snapshot.first_particle_near_emitter);
}
