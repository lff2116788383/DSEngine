/**
 * @file lua_binding_ecs.cpp
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include <limits>
extern "C" {
#include <lauxlib.h>
}

#include "engine/assets/asset_manager.h"
#include <rapidjson/document.h>

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
            // We will just read the file using AssetManager here directly as it's simple JSON
            std::vector<uint8_t> file_data;
            if (AssetManager::Instance().LoadFileToMemory(dmat_path, file_data)) {
                std::string text(reinterpret_cast<const char*>(file_data.data()), file_data.size());
                rapidjson::Document doc;
                doc.Parse(text.c_str());
                if (!doc.HasParseError() && doc.HasMember("materials") && doc["materials"].IsArray() && doc["materials"].Size() > 0) {
                    const auto& mat = doc["materials"][0];
                    if (mat.HasMember("base_color") && mat["base_color"].IsArray()) {
                        mesh.color = glm::vec4(
                            mat["base_color"][0].GetFloat(),
                            mat["base_color"][1].GetFloat(),
                            mat["base_color"][2].GetFloat(),
                            mat["base_color"][3].GetFloat()
                        );
                    }
                    if (mat.HasMember("emissive") && mat["emissive"].IsArray()) {
                        mesh.emissive = glm::vec3(
                            mat["emissive"][0].GetFloat(),
                            mat["emissive"][1].GetFloat(),
                            mat["emissive"][2].GetFloat()
                        );
                    }
                    if (mat.HasMember("metallic")) mesh.metallic = mat["metallic"].GetFloat();
                    if (mat.HasMember("roughness")) mesh.roughness = mat["roughness"].GetFloat();
                }
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
        if (lua_gettop(L) >= 2) {
            if (lua_isstring(L, 2)) {
                animator.danim_path = lua_tostring(L, 2);
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

}

void RegisterEcsBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("create_entity", L_EcsCreateEntity);
    set_fn("add_transform", L_EcsAddTransform);
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
    set_fn("add_directional_light_3d", L_EcsAddDirectionalLight3D);
    set_fn("set_directional_light_3d", L_EcsSetDirectionalLight3D);
    set_fn("add_animator_3d", L_EcsAddAnimator3D);
    set_fn("set_animator_3d_state", L_EcsSetAnimator3DState);
    set_fn("add_skybox", L_EcsAddSkybox);
    set_fn("add_free_camera_controller", L_EcsAddFreeCameraController);
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
}

}
