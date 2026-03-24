#include "engine/scripting/lua/lua_runtime.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/components_2d.h"
#include "engine/base/debug.h"
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <unordered_map>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace phase1::runtime {
namespace {
struct RuntimeState {
    struct ScriptInstance {
        int table_ref = LUA_NOREF;
        bool awake_called = false;
        std::string script_path;
        std::filesystem::file_time_type last_write_time = std::filesystem::file_time_type::min();
        bool has_last_write_time = false;
    };
    lua_State* state = nullptr;
    bool awake_called = false;
    std::string startup_script_path;
    std::string startup_script_override;
    LuaApiContext api_context;
    std::unordered_map<std::uint32_t, ScriptInstance> script_instances;
};

RuntimeState& State() {
    static RuntimeState state;
    return state;
}

Phase1AssetManager& GetAssetManager() {
    if (State().api_context.asset_manager) {
        return *State().api_context.asset_manager;
    }
    return Phase1AssetManager::Instance();
}

std::string ResolveStartupLuaScript() {
    if (!State().startup_script_override.empty() && std::filesystem::exists(State().startup_script_override)) {
        return State().startup_script_override;
    }
    if (const char* script_from_env = std::getenv("DSE_STARTUP_LUA")) {
        if (script_from_env[0] != '\0' && std::filesystem::exists(script_from_env)) {
            return script_from_env;
        }
    }
    return "";
}

void SetupLuaPackagePath(lua_State* L, const std::string& startup_script_path) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    std::string package_path = lua_tostring(L, -1);
    package_path.append(";./?.lua;./script/?.lua;./script/?/init.lua");
    package_path.append(";../?.lua;../script/?.lua;../script/?/init.lua");
    package_path.append(";../../?.lua;../../script/?.lua;../../script/?/init.lua");
    if (!startup_script_path.empty()) {
        std::filesystem::path startup_path(startup_script_path);
        std::filesystem::path startup_dir = startup_path.parent_path();
        if (!startup_dir.empty()) {
            package_path.append(";");
            package_path.append(startup_dir.generic_string());
            package_path.append("/?.lua");
            std::filesystem::path startup_root = startup_dir.parent_path();
            if (!startup_root.empty()) {
                package_path.append(";");
                package_path.append(startup_root.generic_string());
                package_path.append("/?.lua");
            }
        }
    }
    lua_pop(L, 1);
    lua_pushstring(L, package_path.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
}

Entity LuaEntityFromInteger(lua_Integer value) {
    return static_cast<Entity>(static_cast<std::uint32_t>(value));
}

Phase1World* GetWorld() {
    return State().api_context.world;
}

int LuaDSECreateEntity(lua_State* L) {
    Phase1World* world = GetWorld();
    if (!world) {
        lua_pushinteger(L, 0);
        return 1;
    }
    Entity e = world->CreateEntity();
    lua_pushinteger(L, static_cast<lua_Integer>(static_cast<std::uint32_t>(e)));
    return 1;
}

int LuaDSEAddTransform(lua_State* L) {
    Phase1World* world = GetWorld();
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

int LuaDSEAddCamera(lua_State* L) {
    Phase1World* world = GetWorld();
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

int LuaDSELoadTexture(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    auto texture = GetAssetManager().LoadTexture(path);
    lua_pushinteger(L, texture ? static_cast<lua_Integer>(texture->GetHandle()) : 0);
    return 1;
}

int LuaDSESetDataRoot(lua_State* L) {
    const char* data_root = luaL_checkstring(L, 1);
    auto& asset_manager = GetAssetManager();
    asset_manager.ConfigureDataRoot(data_root);
    DEBUG_LOG_INFO("Phase1 data root updated from lua: {}", asset_manager.GetDataRoot());
    return 0;
}

int LuaDSESetWindowTitle(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const auto& setter = State().api_context.set_window_title;
    if (setter) {
        setter(title);
    }
    return 0;
}

int LuaDSEAddSprite(lua_State* L) {
    Phase1World* world = GetWorld();
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

int LuaDSEAddRigidBody(lua_State* L) {
    Phase1World* world = GetWorld();
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

int LuaDSEAddBoxCollider(lua_State* L) {
    Phase1World* world = GetWorld();
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

int LuaDSEAddAudioSource(lua_State* L) {
    Phase1World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    bool play_on_awake = lua_toboolean(L, 3);
    bool loop = lua_toboolean(L, 4);
    float volume = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    auto& audio = world->registry().emplace_or_replace<AudioSourceComponent>(e);
    audio.clip = Phase1AssetManager::Instance().LoadAudioClip(path);
    audio.play_on_awake = play_on_awake;
    audio.loop = loop;
    audio.volume = volume;
    return 0;
}

int LuaDSEAddUIRenderer(lua_State* L) {
    Phase1World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    unsigned int tex_handle = static_cast<unsigned int>(luaL_optinteger(L, 2, 0));
    float r = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float a = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    int order = static_cast<int>(luaL_optinteger(L, 7, 0));
    auto& ui = world->registry().emplace_or_replace<UIRendererComponent>(e);
    ui.texture_handle = tex_handle;
    ui.color = glm::vec4(r, g, b, a);
    ui.order = order;
    return 0;
}

int LuaDSEAddUILabel(lua_State* L) {
    Phase1World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    unsigned int font_tex_handle = static_cast<unsigned int>(luaL_optinteger(L, 3, 0));
    float r = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    float a = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    float glyph_w = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float glyph_h = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    float spacing = static_cast<float>(luaL_optnumber(L, 10, 0.0));
    int atlas_cols = static_cast<int>(luaL_optinteger(L, 11, 0));
    int atlas_rows = static_cast<int>(luaL_optinteger(L, 12, 0));
    int ascii_start = static_cast<int>(luaL_optinteger(L, 13, 0));
    float offset_x = static_cast<float>(luaL_optnumber(L, 14, 0.0));
    float offset_y = static_cast<float>(luaL_optnumber(L, 15, 0.0));
    auto& label = world->registry().emplace_or_replace<UILabelComponent>(e);
    label.text = text;
    label.font_texture_handle = font_tex_handle;
    label.color = glm::vec4(r, g, b, a);
    if (glyph_w > 0.0f && glyph_h > 0.0f) {
        label.glyph_size = glm::vec2(glyph_w, glyph_h);
    }
    if (spacing != 0.0f) {
        label.spacing = spacing;
    }
    if (atlas_cols > 0) {
        label.atlas_cols = atlas_cols;
    }
    if (atlas_rows > 0) {
        label.atlas_rows = atlas_rows;
    }
    if (ascii_start > 0) {
        label.ascii_start = ascii_start;
    }
    if (offset_x != 0.0f || offset_y != 0.0f) {
        label.offset = glm::vec2(offset_x, offset_y);
    }
    label.dirty = true;
    return 0;
}

int LuaDSEAddTilemap(lua_State* L) {
    Phase1World* world = GetWorld();
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

int LuaDSESetTile(lua_State* L) {
    Phase1World* world = GetWorld();
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

int LuaDSEAddAnimator(lua_State* L) {
    Phase1World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    world->registry().emplace_or_replace<AnimatorComponent>(e);
    return 0;
}

int LuaDSEAddAnimationState(lua_State* L) {
    Phase1World* world = GetWorld();
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
        
        // table at index 5 for textures
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

int LuaDSEPlayAnimation(lua_State* L) {
    Phase1World* world = GetWorld();
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

int LuaDSEGetDrawCalls(lua_State* L) {
    const auto& fn = State().api_context.get_draw_calls;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}

int LuaDSEGetMaxBatchSprites(lua_State* L) {
    const auto& fn = State().api_context.get_max_batch_sprites;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}

int LuaDSEGetSpriteCount(lua_State* L) {
    const auto& fn = State().api_context.get_sprite_count;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}

void RegisterPhase1LuaApi(lua_State* L) {
    lua_register(L, "DSE_CreateEntity", LuaDSECreateEntity);
    lua_register(L, "DSE_AddTransform", LuaDSEAddTransform);
    lua_register(L, "DSE_AddCamera", LuaDSEAddCamera);
    lua_register(L, "DSE_SetDataRoot", LuaDSESetDataRoot);
    lua_register(L, "DSE_SetWindowTitle", LuaDSESetWindowTitle);
    lua_register(L, "DSE_LoadTexture", LuaDSELoadTexture);
    lua_register(L, "DSE_AddSprite", LuaDSEAddSprite);
    lua_register(L, "DSE_AddRigidBody", LuaDSEAddRigidBody);
    lua_register(L, "DSE_AddBoxCollider", LuaDSEAddBoxCollider);
    lua_register(L, "DSE_AddAudioSource", LuaDSEAddAudioSource);
    lua_register(L, "DSE_AddUIRenderer", LuaDSEAddUIRenderer);
    lua_register(L, "DSE_AddUILabel", LuaDSEAddUILabel);
    lua_register(L, "DSE_AddTilemap", LuaDSEAddTilemap);
    lua_register(L, "DSE_SetTile", LuaDSESetTile);
    lua_register(L, "DSE_AddAnimator", LuaDSEAddAnimator);
    lua_register(L, "DSE_AddAnimationState", LuaDSEAddAnimationState);
    lua_register(L, "DSE_PlayAnimation", LuaDSEPlayAnimation);
    lua_register(L, "DSE_GetDrawCalls", LuaDSEGetDrawCalls);
    lua_register(L, "DSE_GetMaxBatchSprites", LuaDSEGetMaxBatchSprites);
    lua_register(L, "DSE_GetSpriteCount", LuaDSEGetSpriteCount);
}

std::uint32_t EntityToKey(Entity entity) {
    return static_cast<std::uint32_t>(entity);
}

bool CallScriptTableMethod(lua_State* L, int table_ref, const char* method_name, int entity_id, float delta_time, bool pass_delta_time) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, table_ref);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    lua_getfield(L, -1, method_name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return false;
    }
    lua_pushvalue(L, -2);
    lua_pushinteger(L, entity_id);
    int args = 2;
    if (pass_delta_time) {
        lua_pushnumber(L, delta_time);
        args = 3;
    }
    if (lua_pcall(L, args, 0, 0) != LUA_OK) {
        DEBUG_LOG_ERROR("Phase1 lua method {} failed: {}", method_name, lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_pop(L, 1);
        return false;
    }
    lua_pop(L, 1);
    return true;
}

bool QueryScriptFileWriteTime(const std::string& script_path, std::filesystem::file_time_type& out_write_time) {
    std::error_code ec;
    if (!std::filesystem::exists(script_path, ec)) {
        return false;
    }
    out_write_time = std::filesystem::last_write_time(script_path, ec);
    return !ec;
}

int SerializeScriptState(lua_State* L, int table_ref, int entity_id) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, table_ref);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return LUA_NOREF;
    }
    lua_getfield(L, -1, "OnSerializeState");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return LUA_NOREF;
    }
    lua_pushvalue(L, -2);
    lua_pushinteger(L, entity_id);
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        DEBUG_LOG_ERROR("Phase1 lua method OnSerializeState failed: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_pop(L, 1);
        return LUA_NOREF;
    }
    int state_ref = LUA_NOREF;
    if (lua_istable(L, -1)) {
        state_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return state_ref;
}

void RestoreScriptState(lua_State* L, int table_ref, int entity_id, int state_ref) {
    if (state_ref == LUA_NOREF) {
        return;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, table_ref);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, state_ref);
        return;
    }
    lua_getfield(L, -1, "OnDeserializeState");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        luaL_unref(L, LUA_REGISTRYINDEX, state_ref);
        return;
    }
    lua_pushvalue(L, -2);
    lua_pushinteger(L, entity_id);
    lua_rawgeti(L, LUA_REGISTRYINDEX, state_ref);
    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        DEBUG_LOG_ERROR("Phase1 lua method OnDeserializeState failed: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    luaL_unref(L, LUA_REGISTRYINDEX, state_ref);
}

void DestroyScriptInstance(lua_State* L, int entity_id, RuntimeState::ScriptInstance& instance, bool call_destroy = true) {
    if (instance.table_ref == LUA_NOREF) {
        return;
    }
    if (call_destroy && instance.awake_called) {
        CallScriptTableMethod(L, instance.table_ref, "OnDestroy", entity_id, 0.0f, false);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, instance.table_ref);
    instance.table_ref = LUA_NOREF;
    instance.awake_called = false;
    instance.has_last_write_time = false;
    instance.last_write_time = std::filesystem::file_time_type::min();
}

bool EnsureScriptInstanceLoaded(lua_State* L, int entity_id, const std::string& script_path, RuntimeState::ScriptInstance& instance) {
    std::filesystem::file_time_type current_write_time = std::filesystem::file_time_type::min();
    const bool has_current_write_time = QueryScriptFileWriteTime(script_path, current_write_time);
    bool should_reload = instance.table_ref == LUA_NOREF || instance.script_path != script_path;
    if (!should_reload && has_current_write_time && instance.has_last_write_time && current_write_time != instance.last_write_time) {
        should_reload = true;
    }
    if (!should_reload) {
        return true;
    }
    int serialized_state_ref = LUA_NOREF;
    bool hot_reload = false;
    bool preserve_awake = false;
    if (instance.table_ref != LUA_NOREF) {
        hot_reload = instance.script_path == script_path;
        preserve_awake = instance.awake_called;
        if (hot_reload && preserve_awake) {
            serialized_state_ref = SerializeScriptState(L, instance.table_ref, entity_id);
        }
        DestroyScriptInstance(L, entity_id, instance, !hot_reload);
    }
    if (luaL_dofile(L, script_path.c_str()) != LUA_OK) {
        DEBUG_LOG_ERROR("Phase1 lua script load failed: {}, error: {}", script_path, lua_tostring(L, -1));
        lua_pop(L, 1);
        if (serialized_state_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, serialized_state_ref);
        }
        return false;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        DEBUG_LOG_ERROR("Phase1 lua script invalid return value, expected table: {}", script_path);
        if (serialized_state_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, serialized_state_ref);
        }
        return false;
    }
    instance.table_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    instance.script_path = script_path;
    instance.awake_called = preserve_awake;
    instance.has_last_write_time = has_current_write_time;
    instance.last_write_time = current_write_time;
    if (serialized_state_ref != LUA_NOREF) {
        RestoreScriptState(L, instance.table_ref, entity_id, serialized_state_ref);
    }
    return true;
}

void UpdateScriptComponents(float delta_time) {
    auto& state = State();
    if (!state.state || !state.api_context.world) {
        return;
    }
    auto& registry = state.api_context.world->registry();
    std::unordered_map<std::uint32_t, bool> active_keys;
    auto view = registry.view<ScriptComponent>();
    for (auto entity : view) {
        auto& script_component = view.get<ScriptComponent>(entity);
        std::uint32_t entity_key = EntityToKey(entity);
        active_keys[entity_key] = script_component.enabled;
        auto instance_it = state.script_instances.find(entity_key);
        if (!script_component.enabled || script_component.script_path.empty()) {
            if (instance_it != state.script_instances.end()) {
                DestroyScriptInstance(state.state, static_cast<int>(entity_key), instance_it->second);
            }
            continue;
        }
        if (instance_it == state.script_instances.end()) {
            instance_it = state.script_instances.emplace(entity_key, RuntimeState::ScriptInstance{}).first;
        }
        auto& instance = instance_it->second;
        if (!EnsureScriptInstanceLoaded(state.state, static_cast<int>(entity_key), script_component.script_path, instance)) {
            continue;
        }
        if (!instance.awake_called) {
            CallScriptTableMethod(state.state, instance.table_ref, "OnAwake", static_cast<int>(entity_key), 0.0f, false);
            instance.awake_called = true;
        }
        CallScriptTableMethod(state.state, instance.table_ref, "OnUpdate", static_cast<int>(entity_key), delta_time, true);
    }
    std::vector<std::uint32_t> stale_keys;
    for (const auto& pair : state.script_instances) {
        auto it = active_keys.find(pair.first);
        if (it == active_keys.end() || !it->second) {
            stale_keys.push_back(pair.first);
        }
    }
    for (std::uint32_t key : stale_keys) {
        auto it = state.script_instances.find(key);
        if (it == state.script_instances.end()) {
            continue;
        }
        DestroyScriptInstance(state.state, static_cast<int>(key), it->second);
        state.script_instances.erase(it);
    }
}
}

void ConfigureLuaApiContext(LuaApiContext context) {
    State().api_context = std::move(context);
}

void SetStartupLuaScriptPath(std::string script_path) {
    State().startup_script_override = std::move(script_path);
}

void ShutdownLuaRuntime() {
    auto& state = State();
    if (state.state) {
        for (auto& pair : state.script_instances) {
            DestroyScriptInstance(state.state, static_cast<int>(pair.first), pair.second);
        }
        state.script_instances.clear();
        lua_close(state.state);
        state.state = nullptr;
    }
    state.awake_called = false;
    state.startup_script_path.clear();
}

bool BootstrapLuaRuntime() {
    auto& state = State();
    ShutdownLuaRuntime();
    if (!state.api_context.world) {
        DEBUG_LOG_ERROR("Phase1 lua init failed: LuaApiContext.world is null");
        return false;
    }
    state.state = luaL_newstate();
    if (!state.state) {
        DEBUG_LOG_ERROR("Phase1 lua init failed: luaL_newstate returned nullptr");
        return false;
    }
    state.startup_script_path = ResolveStartupLuaScript();
    if (state.startup_script_path.empty()) {
        DEBUG_LOG_ERROR("Phase1 lua startup script not found, set by SetStartupLuaScriptPath or DSE_STARTUP_LUA");
        return false;
    }
    luaL_openlibs(state.state);
    SetupLuaPackagePath(state.state, state.startup_script_path);
    RegisterPhase1LuaApi(state.state);
    if (luaL_dofile(state.state, state.startup_script_path.c_str()) != LUA_OK) {
        DEBUG_LOG_ERROR("Phase1 lua startup load failed: {}", lua_tostring(state.state, -1));
        lua_pop(state.state, 1);
        return false;
    }
    lua_getglobal(state.state, "Awake");
    if (lua_isfunction(state.state, -1)) {
        if (lua_pcall(state.state, 0, 0, 0) != LUA_OK) {
            DEBUG_LOG_ERROR("Phase1 lua Awake failed: {}", lua_tostring(state.state, -1));
            lua_pop(state.state, 1);
            return false;
        }
        state.awake_called = true;
    } else {
        lua_pop(state.state, 1);
    }
    DEBUG_LOG_INFO("Phase1 lua startup script loaded: {}", state.startup_script_path);
    return true;
}

void TickLuaRuntime(float delta_time) {
    auto& state = State();
    if (!state.state) {
        return;
    }
    lua_getglobal(state.state, "Update");
    if (!lua_isfunction(state.state, -1)) {
        lua_pop(state.state, 1);
        return;
    }
    lua_pushnumber(state.state, delta_time);
    if (lua_pcall(state.state, 1, 0, 0) != LUA_OK) {
        DEBUG_LOG_ERROR("Phase1 lua Update failed: {}", lua_tostring(state.state, -1));
        lua_pop(state.state, 1);
    }
    UpdateScriptComponents(delta_time);
}

}
