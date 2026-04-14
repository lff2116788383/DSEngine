/**
 * @file lua_binding_ecs.cpp
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scene/scene.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/world.h"
#ifdef DSE_ENABLE_PHYSX
#include "engine/physics/physics3d/physics3d_system.h"
#endif
#include <limits>
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include "engine/assets/asset_manager.h"
#include <rapidjson/document.h>

namespace dse::runtime::lua_binding {
namespace {

int L_EcsAddParticleSystem3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int max_particles = static_cast<int>(luaL_optinteger(L, 2, 1000));
    float emission_rate = static_cast<float>(luaL_optnumber(L, 3, 100.0));
    
    auto& ps = world->registry().emplace_or_replace<ParticleSystem3DComponent>(e);
    ps.max_particles = max_particles;
    ps.emission_rate = emission_rate;
    return 0;
}

int L_EcsGetTransformPosition(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<TransformComponent>(e)) {
        auto& transform = world->registry().get<TransformComponent>(e);
        lua_pushnumber(L, transform.position.x);
        lua_pushnumber(L, transform.position.y);
        lua_pushnumber(L, transform.position.z);
        return 3;
    }
    return 0;
}

int L_EcsSetTransformPosition(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    if (world->registry().valid(e) && world->registry().all_of<TransformComponent>(e)) {
        auto& transform = world->registry().get<TransformComponent>(e);
        transform.position = glm::vec3(x, y, z);
        transform.dirty = true;
    }
    return 0;
}

int L_EcsAddPointLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float intensity = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float radius = static_cast<float>(luaL_optnumber(L, 6, 10.0));
    auto& light = world->registry().emplace_or_replace<PointLightComponent>(e);
    light.enabled = true;
    light.color = glm::vec3(r, g, b);
    light.intensity = intensity;
    light.radius = radius;
    return 0;
}

int L_EcsAddSpotLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float dir_x = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float dir_y = static_cast<float>(luaL_optnumber(L, 3, -1.0));
    float dir_z = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float r = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    float intensity = static_cast<float>(luaL_optnumber(L, 8, 1.0));
    float radius = static_cast<float>(luaL_optnumber(L, 9, 20.0));
    float inner_angle = static_cast<float>(luaL_optnumber(L, 10, 12.5));
    float outer_angle = static_cast<float>(luaL_optnumber(L, 11, 17.5));
    auto& light = world->registry().emplace_or_replace<SpotLightComponent>(e);
    light.enabled = true;
    light.direction = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
    light.color = glm::vec3(r, g, b);
    light.intensity = intensity;
    light.radius = radius;
    light.inner_cone_angle = inner_angle;
    light.outer_cone_angle = outer_angle;
    return 0;
}

int L_EcsAddTerrain(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* heightmap_path = luaL_optstring(L, 2, "");
    float width = static_cast<float>(luaL_optnumber(L, 3, 100.0));
    float depth = static_cast<float>(luaL_optnumber(L, 4, 100.0));
    float max_height = static_cast<float>(luaL_optnumber(L, 5, 20.0));
    auto& terrain = world->registry().emplace_or_replace<TerrainComponent>(e);
    terrain.enabled = true;
    terrain.heightmap_path = heightmap_path;
    terrain.width = width;
    terrain.depth = depth;
    terrain.max_height = max_height;
    terrain.is_dirty = true;
    return 0;
}

int L_EcsAddSteering(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float max_vel = static_cast<float>(luaL_optnumber(L, 2, 5.0));
    float max_force = static_cast<float>(luaL_optnumber(L, 3, 10.0));
    float mass = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    auto& steering = world->registry().emplace_or_replace<SteeringComponent>(e);
    steering.enabled = true;
    steering.max_velocity = max_vel;
    steering.max_force = max_force;
    steering.mass = mass;
    return 0;
}

int L_EcsSetSteeringTarget(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* behavior = luaL_checkstring(L, 2);
    float tx = static_cast<float>(luaL_checknumber(L, 3));
    float ty = static_cast<float>(luaL_checknumber(L, 4));
    float tz = static_cast<float>(luaL_checknumber(L, 5));
    if (world->registry().valid(e) && world->registry().all_of<SteeringComponent>(e)) {
        auto& steering = world->registry().get<SteeringComponent>(e);
        std::string b = behavior;
        if (b == "seek") {
            steering.seek_enabled = true;
            steering.seek_target = glm::vec3(tx, ty, tz);
        } else if (b == "flee") {
            steering.flee_enabled = true;
            steering.flee_target = glm::vec3(tx, ty, tz);
        } else if (b == "arrive") {
            steering.arrive_enabled = true;
            steering.arrive_target = glm::vec3(tx, ty, tz);
        }
    }
    return 0;
}

int L_EcsCreateEntity(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    Entity e = world->CreateEntity();
    lua_pushinteger(L, static_cast<lua_Integer>(static_cast<std::uint32_t>(e)));
    return 1;
}

int L_EcsAddTransform(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float sx = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float sy = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    float sz = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    auto& transform = world->registry().emplace_or_replace<TransformComponent>(e);
    transform.position = glm::vec3(x, y, z);
    transform.scale = glm::vec3(sx, sy, sz);
    transform.dirty = true;
    return 0;
}

int L_EcsAddCamera(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float ortho_size = static_cast<float>(luaL_optnumber(L, 2, 10.0));
    int priority = static_cast<int>(luaL_optinteger(L, 3, 0));
    auto& camera = world->registry().emplace_or_replace<CameraComponent>(e);
    camera.enabled = true;
    camera.priority = priority;
    camera.orthographic = true;
    camera.orthographic_size = ortho_size;
    return 0;
}

int L_EcsAddCamera3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float fov = static_cast<float>(luaL_optnumber(L, 2, 60.0));
    int priority = static_cast<int>(luaL_optinteger(L, 3, 0));
    auto& camera = world->registry().emplace_or_replace<Camera3DComponent>(e);
    camera.enabled = true;
    camera.priority = priority;
    camera.fov = fov;
    camera.near_clip = 0.1f;
    camera.far_clip = 1000.0f;
    return 0;
}

int L_EcsSetCameraPriority(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int priority = static_cast<int>(luaL_checkinteger(L, 2));
    if (world->registry().valid(e) && world->registry().all_of<Camera3DComponent>(e)) {
        auto& camera = world->registry().get<Camera3DComponent>(e);
        camera.priority = priority;
    }
    if (world->registry().valid(e) && world->registry().all_of<CameraComponent>(e)) {
        auto& camera = world->registry().get<CameraComponent>(e);
        camera.priority = priority;
    }
    return 0;
}

int L_EcsSetCameraEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    bool enabled = lua_toboolean(L, 2) != 0;
    if (world->registry().valid(e) && world->registry().all_of<Camera3DComponent>(e)) {
        auto& camera = world->registry().get<Camera3DComponent>(e);
        camera.enabled = enabled;
    }
    if (world->registry().valid(e) && world->registry().all_of<CameraComponent>(e)) {
        auto& camera = world->registry().get<CameraComponent>(e);
        camera.enabled = enabled;
    }
    return 0;
}

int L_EcsSetCameraFollow(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity camera_entity = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    Entity target_entity = LuaEntityFromInteger(luaL_checkinteger(L, 2));
    float damping = static_cast<float>(luaL_optnumber(L, 3, 0.12));
    float dead_zone_x = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float dead_zone_y = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float offset_x = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float offset_y = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    if (world->registry().valid(camera_entity)) {
        auto& follow = world->registry().emplace_or_replace<CameraFollowComponent>(camera_entity);
        follow.target = target_entity;
        follow.damping = damping;
        follow.dead_zone = glm::vec2(dead_zone_x, dead_zone_y);
        follow.offset = glm::vec3(offset_x, offset_y, 0.0f);
        follow.enabled = true;
    }
    return 0;
}

int L_EcsAddSprite(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float a = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    int order = static_cast<int>(luaL_optinteger(L, 6, 0));
    unsigned int texture_handle = static_cast<unsigned int>(luaL_optinteger(L, 7, 0));
    auto& sprite = world->registry().emplace_or_replace<SpriteRendererComponent>(e);
    sprite.color = glm::vec4(r, g, b, a);
    sprite.order_in_layer = order;
    sprite.texture_handle = texture_handle;
    sprite.visible = true;
    return 0;
}

int L_EcsSetSpriteUvScroll(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float speed_x = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float speed_y = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    if (world->registry().valid(e) && world->registry().all_of<SpriteRendererComponent>(e)) {
        auto& sprite = world->registry().get<SpriteRendererComponent>(e);
        sprite.uv_scroll_speed = glm::vec2(speed_x, speed_y);
    }
    return 0;
}

int L_EcsSetSpriteUvOffset(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float offset_x = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float offset_y = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    if (world->registry().valid(e) && world->registry().all_of<SpriteRendererComponent>(e)) {
        auto& sprite = world->registry().get<SpriteRendererComponent>(e);
        sprite.uv_offset = glm::vec2(offset_x, offset_y);
    }
    return 0;
}

int L_EcsAddRigidBody(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int type = static_cast<int>(luaL_optinteger(L, 2, 2));
    float gravity_scale = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    int fixed_rotation = static_cast<int>(luaL_optinteger(L, 4, 0));
    auto& rb = world->registry().emplace_or_replace<RigidBody2DComponent>(e);
    if (type <= 0) {
        rb.type = RigidBody2DType::Static;
    } else if (type == 1) {
        rb.type = RigidBody2DType::Kinematic;
    } else {
        rb.type = RigidBody2DType::Dynamic;
    }
    rb.gravity_scale = gravity_scale;
    rb.fixed_rotation = fixed_rotation != 0;
    return 0;
}

int L_EcsAddBoxCollider(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float w = static_cast<float>(luaL_checknumber(L, 2));
    float h = static_cast<float>(luaL_checknumber(L, 3));
    float density = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float friction = static_cast<float>(luaL_optnumber(L, 5, 0.3));
    float restitution = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    auto& collider = world->registry().emplace_or_replace<BoxCollider2DComponent>(e);
    collider.size = glm::vec2(w, h);
    collider.density = density;
    collider.friction = friction;
    collider.restitution = restitution;
    return 0;
}

int L_EcsAddTilemap(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int width = static_cast<int>(luaL_checkinteger(L, 2));
    int height = static_cast<int>(luaL_checkinteger(L, 3));
    float tile_size = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    unsigned int tex_handle = static_cast<unsigned int>(luaL_optinteger(L, 5, 0));
    auto& tilemap = world->registry().emplace_or_replace<TilemapComponent>(e);
    tilemap.width = width;
    tilemap.height = height;
    tilemap.tile_size = tile_size;
    tilemap.tileset_handle = tex_handle;
    tilemap.tiles.resize(width * height, -1);
    return 0;
}

int L_EcsSetTile(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int x = static_cast<int>(luaL_checkinteger(L, 2));
    int y = static_cast<int>(luaL_checkinteger(L, 3));
    int tile_id = static_cast<int>(luaL_checkinteger(L, 4));
    if (world->registry().valid(e) && world->registry().all_of<TilemapComponent>(e)) {
        auto& tilemap = world->registry().get<TilemapComponent>(e);
        if (x >= 0 && x < tilemap.width && y >= 0 && y < tilemap.height) {
            tilemap.tiles[y * tilemap.width + x] = tile_id;
            tilemap.dirty = true;
        }
    }
    return 0;
}

int L_EcsAddAnimator(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    world->registry().emplace_or_replace<AnimatorComponent>(e);
    return 0;
}

int L_EcsAddAnimationEvent(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* state_name = luaL_checkstring(L, 2);
    float normalized_time = static_cast<float>(luaL_checknumber(L, 3));
    const char* event_name = luaL_checkstring(L, 4);
    if (normalized_time < 0.0f) normalized_time = 0.0f;
    if (normalized_time > 1.0f) normalized_time = 1.0f;
    if (world->registry().valid(e) && world->registry().all_of<AnimatorComponent>(e)) {
        auto& animator = world->registry().get<AnimatorComponent>(e);
        auto it = animator.states.find(state_name);
        if (it != animator.states.end()) {
            it->second.events.emplace_back(normalized_time, std::string(event_name));
        }
    }
    return 0;
}

int L_EcsPlayAnimationSegment(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int start_frame = static_cast<int>(luaL_checkinteger(L, 2));
    int end_frame = static_cast<int>(luaL_checkinteger(L, 3));
    bool loop = lua_toboolean(L, 4);
    if (world->registry().valid(e) && world->registry().all_of<AnimatorComponent>(e)) {
        auto& animator = world->registry().get<AnimatorComponent>(e);
        animator.PlaySegment(start_frame, end_frame, loop);
    }
    return 0;
}

int L_EcsPopAnimationEvent(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushstring(L, "");
        return 1;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<AnimatorComponent>(e)) {
        auto& animator = world->registry().get<AnimatorComponent>(e);
        if (!animator.fired_events.empty()) {
            std::string event_name = animator.fired_events.front();
            animator.fired_events.erase(animator.fired_events.begin());
            lua_pushstring(L, event_name.c_str());
            return 1;
        }
    }
    lua_pushstring(L, "");
    return 1;
}

int L_EcsAddParticleEmitter(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    unsigned int texture_handle = static_cast<unsigned int>(luaL_optinteger(L, 2, 0));
    int max_particles = static_cast<int>(luaL_optinteger(L, 3, 100));
    float emit_rate = static_cast<float>(luaL_optnumber(L, 4, 10.0));
    auto& emitter = world->registry().emplace_or_replace<ParticleEmitterComponent>(e);
    emitter.texture_handle = texture_handle;
    emitter.max_particles = max_particles;
    emitter.emit_rate = emit_rate;
    return 0;
}

int L_EcsSetParticleDensity(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float density_scale = static_cast<float>(luaL_checknumber(L, 2));
    if (density_scale < 0.0f) {
        density_scale = 0.0f;
    }
    if (world->registry().valid(e) && world->registry().all_of<ParticleEmitterComponent>(e)) {
        auto& emitter = world->registry().get<ParticleEmitterComponent>(e);
        emitter.emit_rate_scale = density_scale;
    }
    return 0;
}

int L_EcsParticleBurst(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int burst_count = static_cast<int>(luaL_checkinteger(L, 2));
    if (burst_count < 0) {
        burst_count = 0;
    }
    if (world->registry().valid(e) && world->registry().all_of<ParticleEmitterComponent>(e)) {
        auto& emitter = world->registry().get<ParticleEmitterComponent>(e);
        emitter.pending_burst += burst_count;
    }
    return 0;
}

int L_EcsAddGameplayTuning(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    world->registry().emplace_or_replace<GameplayTuningComponent>(e);
    return 0;
}

int L_EcsSetGameplayTuning(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<GameplayTuningComponent>(e)) {
        auto& tuning = world->registry().get<GameplayTuningComponent>(e);
        tuning.leaf_min_distance = static_cast<float>(luaL_optnumber(L, 2, tuning.leaf_min_distance));
        tuning.leaf_move_left = static_cast<float>(luaL_optnumber(L, 3, tuning.leaf_move_left));
        tuning.leaf_move_right = static_cast<float>(luaL_optnumber(L, 4, tuning.leaf_move_right));
        tuning.jump_speed_scale = static_cast<float>(luaL_optnumber(L, 5, tuning.jump_speed_scale));
        tuning.jump_speed_max = static_cast<float>(luaL_optnumber(L, 6, tuning.jump_speed_max));
        tuning.camera_follow_damping = static_cast<float>(luaL_optnumber(L, 7, tuning.camera_follow_damping));
    }
    return 0;
}

int L_EcsAddAnimationState(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* state_name = luaL_checkstring(L, 2);
    float fps = static_cast<float>(luaL_checknumber(L, 3));
    bool loop = lua_toboolean(L, 4);
    if (world->registry().valid(e) && world->registry().all_of<AnimatorComponent>(e)) {
        auto& animator = world->registry().get<AnimatorComponent>(e);
        AnimationState state;
        state.name = state_name;
        state.frame_rate = fps;
        state.loop = loop;
        if (lua_istable(L, 5)) {
            int len = lua_rawlen(L, 5);
            for (int i = 1; i <= len; ++i) {
                lua_rawgeti(L, 5, i);
                unsigned int handle = static_cast<unsigned int>(lua_tointeger(L, -1));
                state.frame_handles.push_back(handle);
                lua_pop(L, 1);
            }
        }
        animator.states[state_name] = state;
    }
    return 0;
}

int L_EcsPlayAnimation(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* state_name = luaL_checkstring(L, 2);
    if (world->registry().valid(e) && world->registry().all_of<AnimatorComponent>(e)) {
        auto& animator = world->registry().get<AnimatorComponent>(e);
        animator.current_state = state_name;
        animator.current_time = 0.0f;
        animator.current_frame = 0;
        animator.playing = true;
    }
    return 0;
}
int L_EcsAddMeshRenderer(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float a = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    
    auto& mesh = world->registry().emplace_or_replace<MeshRendererComponent>(e);
    mesh.mesh_path.clear();
    mesh.color = glm::vec4(r, g, b, a);
    mesh.temp_vertices.clear();
    mesh.temp_indices.clear();
    
    if (lua_istable(L, 6)) {
        int v_len = lua_rawlen(L, 6);
        for (int i = 1; i <= v_len; ++i) {
            lua_rawgeti(L, 6, i);
            mesh.temp_vertices.push_back(static_cast<float>(luaL_checknumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    const std::size_t vertex_count = mesh.temp_vertices.size() / 3;
    if (lua_istable(L, 7)) {
        int i_len = lua_rawlen(L, 7);
        for (int i = 1; i <= i_len; ++i) {
            lua_rawgeti(L, 7, i);
            const lua_Integer raw_index = luaL_checkinteger(L, -1);
            if (raw_index >= 0 &&
                static_cast<std::size_t>(raw_index) < vertex_count &&
                raw_index <= static_cast<lua_Integer>(std::numeric_limits<unsigned short>::max())) {
                mesh.temp_indices.push_back(static_cast<unsigned short>(raw_index));
            }
            lua_pop(L, 1);
        }
    }
    
    return 0;
}

int L_EcsSetMeshPath(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* mesh_path = luaL_checkstring(L, 2);
    if (world->registry().valid(e) && world->registry().all_of<MeshRendererComponent>(e)) {
        auto& mesh = world->registry().get<MeshRendererComponent>(e);
        mesh.mesh_path = mesh_path;
        mesh.temp_vertices.clear();
        mesh.temp_indices.clear();
    }
    return 0;
}

int L_EcsSetMeshMaterial(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<MeshRendererComponent>(e)) {
        auto& mesh = world->registry().get<MeshRendererComponent>(e);
        
        // Check if second argument is a string (dmat path)
        if (lua_type(L, 2) == LUA_TSTRING) {
            std::string dmat_path = lua_tostring(L, 2);
            auto material = GetAssetManager().LoadMaterialInstanceFromDmat(dmat_path);
            if (material) {
                mesh.material_instance_id = material->GetId();
                mesh.material_data_source = MeshRendererComponent::MaterialDataSource::MaterialInstance;
                mesh.shader_variant = material->GetShaderVariant();
                mesh.color = material->GetBaseColor();
                mesh.emissive = material->GetEmissiveColor();
                mesh.albedo_texture_handle = material->GetTextureSlots().albedo;
                mesh.normal_texture_handle = material->GetTextureSlots().normal;
                mesh.metallic_roughness_texture_handle = material->GetTextureSlots().metallic_roughness;
                mesh.emissive_texture_handle = material->GetTextureSlots().emissive;
                mesh.occlusion_texture_handle = material->GetTextureSlots().occlusion;
                mesh.metallic = material->GetScalarOverrides().metallic;
                mesh.roughness = material->GetScalarOverrides().roughness;
                mesh.ao = material->GetScalarOverrides().ao;
                mesh.normal_strength = material->GetScalarOverrides().normal_strength;
                mesh.material_alpha_cutoff = material->GetScalarOverrides().alpha_cutoff;
                mesh.material_alpha_test = material->GetScalarOverrides().alpha_test;
                mesh.material_double_sided = material->GetRasterOverrides().double_sided;
            }
            return 0;
        }

        mesh.metallic = static_cast<float>(luaL_optnumber(L, 2, mesh.metallic));
        mesh.roughness = static_cast<float>(luaL_optnumber(L, 3, mesh.roughness));
        mesh.ao = static_cast<float>(luaL_optnumber(L, 4, mesh.ao));
        float er = static_cast<float>(luaL_optnumber(L, 5, mesh.emissive.r));
        float eg = static_cast<float>(luaL_optnumber(L, 6, mesh.emissive.g));
        float eb = static_cast<float>(luaL_optnumber(L, 7, mesh.emissive.b));
        mesh.emissive = glm::vec3(er, eg, eb);
        mesh.normal_strength = static_cast<float>(luaL_optnumber(L, 8, mesh.normal_strength));
        if (lua_gettop(L) >= 9) {
            mesh.receive_shadow = lua_toboolean(L, 9) != 0;
        }
    }
    return 0;
}

int L_EcsSetMeshShaderVariant(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* shader_variant = luaL_checkstring(L, 2);
    if (world->registry().valid(e) && world->registry().all_of<MeshRendererComponent>(e)) {
        auto& mesh = world->registry().get<MeshRendererComponent>(e);
        mesh.shader_variant = shader_variant;
    }
    return 0;
}

int L_EcsSetMeshMaterialScalar(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* param_name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    if (world->registry().valid(e) && world->registry().all_of<MeshRendererComponent>(e)) {
        auto& mesh = world->registry().get<MeshRendererComponent>(e);
        std::string name(param_name);
        if (name == "metallic") mesh.metallic = value;
        else if (name == "roughness") mesh.roughness = value;
        else if (name == "ao") mesh.ao = value;
        else if (name == "normal_strength") mesh.normal_strength = value;
        else if (name == "material_alpha_cutoff") mesh.material_alpha_cutoff = value;
    }
    return 0;
}

int L_EcsSetMeshEmissive(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    if (world->registry().valid(e) && world->registry().all_of<MeshRendererComponent>(e)) {
        auto& mesh = world->registry().get<MeshRendererComponent>(e);
        mesh.emissive = glm::vec3(r, g, b);
    }
    return 0;
}

int L_EcsAddDirectionalLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float dir_x = static_cast<float>(luaL_optnumber(L, 2, -0.4));
    float dir_y = static_cast<float>(luaL_optnumber(L, 3, -1.0));
    float dir_z = static_cast<float>(luaL_optnumber(L, 4, -0.3));
    float r = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    float intensity = static_cast<float>(luaL_optnumber(L, 8, 1.0));
    float ambient_intensity = static_cast<float>(luaL_optnumber(L, 9, 0.2));
    float shadow_strength = static_cast<float>(luaL_optnumber(L, 10, 0.35));
    auto& light = world->registry().emplace_or_replace<DirectionalLight3DComponent>(e);
    light.enabled = true;
    light.direction = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
    light.color = glm::vec3(r, g, b);
    light.intensity = intensity;
    light.ambient_intensity = ambient_intensity;
    light.shadow_strength = shadow_strength;
    return 0;
}

int L_EcsSetDirectionalLight3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<DirectionalLight3DComponent>(e)) {
        auto& light = world->registry().get<DirectionalLight3DComponent>(e);
        if (lua_gettop(L) >= 2) {
            light.enabled = lua_toboolean(L, 2) != 0;
        }
        float dir_x = static_cast<float>(luaL_optnumber(L, 3, light.direction.x));
        float dir_y = static_cast<float>(luaL_optnumber(L, 4, light.direction.y));
        float dir_z = static_cast<float>(luaL_optnumber(L, 5, light.direction.z));
        light.direction = glm::normalize(glm::vec3(dir_x, dir_y, dir_z));
        light.color.r = static_cast<float>(luaL_optnumber(L, 6, light.color.r));
        light.color.g = static_cast<float>(luaL_optnumber(L, 7, light.color.g));
        light.color.b = static_cast<float>(luaL_optnumber(L, 8, light.color.b));
        light.intensity = static_cast<float>(luaL_optnumber(L, 9, light.intensity));
        light.ambient_intensity = static_cast<float>(luaL_optnumber(L, 10, light.ambient_intensity));
        light.shadow_strength = static_cast<float>(luaL_optnumber(L, 11, light.shadow_strength));
    }
    return 0;
}

int L_EcsSetTransformRotation(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    
    if (world->registry().valid(e) && world->registry().all_of<TransformComponent>(e)) {
        auto& transform = world->registry().get<TransformComponent>(e);
        glm::vec3 euler_angles(glm::radians(x), glm::radians(y), glm::radians(z));
        transform.rotation = glm::quat(euler_angles);
        transform.dirty = true;
    }
    return 0;
}

int L_EcsAddAnimator3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* danim_path = luaL_optstring(L, 2, "");
    const char* dskel_path = luaL_optstring(L, 3, "");
    auto& animator = world->registry().emplace_or_replace<Animator3DComponent>(e);
    animator.danim_path = danim_path;
    animator.dskel_path = dskel_path;
    animator.enabled = true;
    animator.current_time = 0.0f;
    animator.speed = 1.0f;
    animator.loop = true;
    return 0;
}

int L_EcsSetAnimator3DState(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<Animator3DComponent>(e)) {
        auto& animator = world->registry().get<Animator3DComponent>(e);
        if (lua_gettop(L) >= 2 && lua_isstring(L, 2)) {
            const char* state_name = lua_tostring(L, 2);
            if (animator.state_machine) {
                // If using state machine, just set a trigger or force state
                // This is a simplified interface; usually we drive it by setting parameters.
                // For direct override:
                animator.current_state_name = state_name;
                animator.state_time = 0.0f;
                animator.is_transitioning = false;
            } else {
                // Legacy
                animator.danim_path = state_name;
            }
        }
        if (lua_gettop(L) >= 3) {
            animator.speed = static_cast<float>(luaL_checknumber(L, 3));
        }
        if (lua_gettop(L) >= 4) {
            animator.loop = lua_toboolean(L, 4) != 0;
        }
    }
    return 0;
}

int L_EcsInitAnimator3DStateMachine(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<Animator3DComponent>(e)) {
        auto& animator = world->registry().get<Animator3DComponent>(e);
        animator.state_machine = std::make_shared<gameplay3d::AnimationStateMachine>();
    }
    return 0;
}

int L_EcsAddAnimator3DState(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* state_name = luaL_checkstring(L, 2);
    const char* danim_path = luaL_checkstring(L, 3);
    bool loop = lua_toboolean(L, 4) != 0;
    float speed = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    
    if (world->registry().valid(e) && world->registry().all_of<Animator3DComponent>(e)) {
        auto& animator = world->registry().get<Animator3DComponent>(e);
        if (animator.state_machine) {
            gameplay3d::AnimState state;
            state.name = state_name;
            state.danim_path = danim_path;
            state.loop = loop;
            state.speed = speed;
            state.is_blend_tree = false;
            animator.state_machine->AddState(state);
        }
    }
    return 0;
}

int L_EcsAddAnimator3DTransition(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* from_state = luaL_checkstring(L, 2);
    const char* to_state = luaL_checkstring(L, 3);
    float transition_duration = static_cast<float>(luaL_optnumber(L, 4, 0.25));
    bool has_exit_time = lua_toboolean(L, 5) != 0;
    float exit_time = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    
    if (world->registry().valid(e) && world->registry().all_of<Animator3DComponent>(e)) {
        auto& animator = world->registry().get<Animator3DComponent>(e);
        if (animator.state_machine) {
            // Note: In a real implementation we would look up the from_state by reference,
            // but here we modify the copy in the map.
            // Since we need to modify it, let's add a helper method to StateMachine or modify it directly.
            // For now we'll do a hacky const_cast since we know it's local.
            auto& fsm = *animator.state_machine;
            auto states = const_cast<std::unordered_map<std::string, gameplay3d::AnimState>&>(fsm.GetStates());
            auto it = states.find(from_state);
            if (it != states.end()) {
                gameplay3d::AnimTransition trans;
                trans.target_state = to_state;
                trans.transition_duration = transition_duration;
                trans.has_exit_time = has_exit_time;
                trans.exit_time = exit_time;
                
                // Read conditions from table if provided (Arg 7)
                if (lua_istable(L, 7)) {
                    lua_pushnil(L);
                    while (lua_next(L, 7) != 0) {
                        // value is a condition table {param, mode, threshold/int_val}
                        if (lua_istable(L, -1)) {
                            gameplay3d::AnimTransitionCondition cond;
                            lua_rawgeti(L, -1, 1); cond.parameter_name = lua_tostring(L, -1); lua_pop(L, 1);
                            lua_rawgeti(L, -1, 2); int mode = lua_tointeger(L, -1); lua_pop(L, 1);
                            cond.mode = static_cast<gameplay3d::AnimConditionMode>(mode);
                            
                            lua_rawgeti(L, -1, 3);
                            if (lua_isnumber(L, -1)) {
                                cond.threshold = static_cast<float>(lua_tonumber(L, -1));
                                cond.int_value = static_cast<int>(lua_tointeger(L, -1));
                            }
                            lua_pop(L, 1);
                            
                            trans.conditions.push_back(cond);
                        }
                        lua_pop(L, 1);
                    }
                }
                
                it->second.transitions.push_back(trans);
            }
        }
    }
    return 0;
}

int L_EcsSetAnimator3DParamFloat(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* param_name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    
    if (world->registry().valid(e) && world->registry().all_of<Animator3DComponent>(e)) {
        auto& animator = world->registry().get<Animator3DComponent>(e);
        if (animator.state_machine) {
            // Auto add parameter if not exists
            if (animator.state_machine->GetParameters().find(param_name) == animator.state_machine->GetParameters().end()) {
                animator.state_machine->AddParameter(param_name, gameplay3d::AnimParamType::Float, 0.0f);
            }
            animator.state_machine->SetFloat(param_name, value);
        }
    }
    return 0;
}

int L_EcsSetAnimator3DParamTrigger(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* param_name = luaL_checkstring(L, 2);
    
    if (world->registry().valid(e) && world->registry().all_of<Animator3DComponent>(e)) {
        auto& animator = world->registry().get<Animator3DComponent>(e);
        if (animator.state_machine) {
            if (animator.state_machine->GetParameters().find(param_name) == animator.state_machine->GetParameters().end()) {
                animator.state_machine->AddTrigger(param_name);
            }
            animator.state_machine->SetTrigger(param_name);
        }
    }
    return 0;
}

int L_EcsAddSkybox(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* cubemap_path = luaL_optstring(L, 2, "");
    auto& skybox = world->registry().emplace_or_replace<SkyboxComponent>(e);
    skybox.cubemap_path = cubemap_path;
    skybox.enabled = true;
    return 0;
}

int L_EcsAddFreeCameraController(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    auto& controller = world->registry().emplace_or_replace<FreeCameraControllerComponent>(e);
    controller.enabled = true;
    controller.move_speed = static_cast<float>(luaL_optnumber(L, 2, 5.0));
    controller.mouse_sensitivity = static_cast<float>(luaL_optnumber(L, 3, 0.1));
    return 0;
}

int L_EcsAddRigidBody3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int type = static_cast<int>(luaL_optinteger(L, 2, 2)); // 0: Static, 1: Kinematic, 2: Dynamic
    float mass = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    auto& rb = world->registry().emplace_or_replace<RigidBody3DComponent>(e);
    rb.type = static_cast<RigidBody3DType>(type);
    rb.mass = mass;
    return 0;
}

int L_EcsAddBoxCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    auto& collider = world->registry().emplace_or_replace<BoxCollider3DComponent>(e);
    collider.size = glm::vec3(x, y, z);
    return 0;
}

int L_EcsAddSphereCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    auto& collider = world->registry().emplace_or_replace<SphereCollider3DComponent>(e);
    collider.radius = radius;
    return 0;
}

int L_EcsAddPostProcess(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    auto& pp = world->registry().emplace_or_replace<PostProcessComponent>(e);
    pp.enabled = true;
    pp.bloom_enabled = lua_toboolean(L, 2) != 0;
    pp.bloom_threshold = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    pp.bloom_intensity = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    return 0;
}

int L_Physics3DRaycast(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
#ifdef DSE_ENABLE_PHYSX
    // We assume Physics3DSystem is globally accessible or accessible via world context
    // For now, we will just return a dummy result as Physics3DSystem is not directly accessible here yet
    // In a real scenario, we'd fetch the system from the world or a global locator
    
    // Read origin
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    
    // Read direction
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    
    // Read max distance
    float max_dist = static_cast<float>(luaL_optnumber(L, 7, 1000.0));

    // TODO: Actually call physics3d::Physics3DSystem::Raycast
    // For now, mock the return
#else
    // PhysX disabled or unavailable in current build; keep Lua API callable with empty result.
#endif
    bool hit = false;
    
    lua_newtable(L);
    lua_pushboolean(L, hit);
    lua_setfield(L, -2, "hit");
    
    return 1;
}

int L_EcsLoadScene(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "world_unavailable");
        return 2;
    }

    const char* scene_path = luaL_checkstring(L, 1);
    scene::Scene scene_loader("lua_runtime_scene_loader");
    scene_loader.BindWorld(world);
    const bool ok = scene_loader.Deserialize(scene_path);
    scene_loader.UnbindWorld();

    lua_pushboolean(L, ok ? 1 : 0);
    if (ok) {
        lua_pushstring(L, "");
    } else {
        lua_pushstring(L, "scene_deserialize_failed");
    }
    return 2;
}

}

void RegisterEcsBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("create_entity", L_EcsCreateEntity);
    set_fn("add_transform", L_EcsAddTransform);
    set_fn("get_transform_position", L_EcsGetTransformPosition);
    set_fn("set_transform_position", L_EcsSetTransformPosition);
    set_fn("set_transform_rotation", L_EcsSetTransformRotation);
    set_fn("add_camera", L_EcsAddCamera);
    set_fn("add_camera_3d", L_EcsAddCamera3D);
    set_fn("set_camera_priority", L_EcsSetCameraPriority);
    set_fn("set_camera_enabled", L_EcsSetCameraEnabled);
    set_fn("set_camera_follow", L_EcsSetCameraFollow);
    set_fn("add_sprite", L_EcsAddSprite);
    set_fn("add_mesh_renderer", L_EcsAddMeshRenderer);
    set_fn("set_mesh_path", L_EcsSetMeshPath);
    set_fn("set_mesh_material", L_EcsSetMeshMaterial);
    set_fn("set_mesh_shader_variant", L_EcsSetMeshShaderVariant);
    set_fn("set_mesh_material_scalar", L_EcsSetMeshMaterialScalar);
    set_fn("set_mesh_emissive", L_EcsSetMeshEmissive);

    set_fn("add_directional_light_3d", L_EcsAddDirectionalLight3D);
    set_fn("set_directional_light_3d", L_EcsSetDirectionalLight3D);
    set_fn("add_point_light_3d", L_EcsAddPointLight3D);
    set_fn("add_spot_light_3d", L_EcsAddSpotLight3D);
    set_fn("add_animator_3d", L_EcsAddAnimator3D);
    set_fn("set_animator_3d_state", L_EcsSetAnimator3DState);
    set_fn("init_animator_3d_fsm", L_EcsInitAnimator3DStateMachine);
    set_fn("add_animator_3d_state", L_EcsAddAnimator3DState);
    set_fn("add_animator_3d_transition", L_EcsAddAnimator3DTransition);
    set_fn("set_animator_3d_param_float", L_EcsSetAnimator3DParamFloat);
    set_fn("set_animator_3d_param_trigger", L_EcsSetAnimator3DParamTrigger);
    set_fn("add_skybox", L_EcsAddSkybox);
    set_fn("add_terrain", L_EcsAddTerrain);
    set_fn("add_steering", L_EcsAddSteering);
    set_fn("set_steering_target", L_EcsSetSteeringTarget);
    set_fn("add_free_camera_controller", L_EcsAddFreeCameraController);
    set_fn("add_rigidbody_3d", L_EcsAddRigidBody3D);
    set_fn("add_box_collider_3d", L_EcsAddBoxCollider3D);
    set_fn("add_sphere_collider_3d", L_EcsAddSphereCollider3D);
    set_fn("add_particle_system_3d", L_EcsAddParticleSystem3D);
    set_fn("add_post_process", L_EcsAddPostProcess);
    set_fn("set_sprite_uv_scroll", L_EcsSetSpriteUvScroll);
    set_fn("set_sprite_uv_offset", L_EcsSetSpriteUvOffset);
    set_fn("add_rigid_body", L_EcsAddRigidBody);
    set_fn("add_box_collider", L_EcsAddBoxCollider);
    set_fn("add_tilemap", L_EcsAddTilemap);
    set_fn("set_tile", L_EcsSetTile);
    set_fn("add_animator", L_EcsAddAnimator);
    set_fn("add_animation_state", L_EcsAddAnimationState);
    set_fn("add_animation_event", L_EcsAddAnimationEvent);
    set_fn("play_animation", L_EcsPlayAnimation);
    set_fn("play_animation_segment", L_EcsPlayAnimationSegment);
    set_fn("pop_animation_event", L_EcsPopAnimationEvent);
    set_fn("add_particle_emitter", L_EcsAddParticleEmitter);
    set_fn("set_particle_density", L_EcsSetParticleDensity);
    set_fn("particle_burst", L_EcsParticleBurst);
    set_fn("add_gameplay_tuning", L_EcsAddGameplayTuning);
    set_fn("set_gameplay_tuning", L_EcsSetGameplayTuning);
    set_fn("physics_3d_raycast", L_Physics3DRaycast);
    set_fn("load_scene", L_EcsLoadScene);
}


}
