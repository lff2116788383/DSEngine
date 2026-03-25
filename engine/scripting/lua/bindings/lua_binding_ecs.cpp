#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/ecs/components_2d.h"
extern "C" {
#include <lauxlib.h>
}

namespace dse::runtime::lua_binding {
namespace {
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
    auto& camera = world->registry().emplace_or_replace<CameraComponent>(e);
    camera.orthographic = true;
    camera.orthographic_size = ortho_size;
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
}

void RegisterEcsBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("create_entity", L_EcsCreateEntity);
    set_fn("add_transform", L_EcsAddTransform);
    set_fn("add_camera", L_EcsAddCamera);
    set_fn("set_camera_follow", L_EcsSetCameraFollow);
    set_fn("add_sprite", L_EcsAddSprite);
    set_fn("set_sprite_uv_scroll", L_EcsSetSpriteUvScroll);
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
}

}
