/**
 * @file lua_runtime.cpp
 * @brief 时间管理系统，提供高精度计时器、增量时间(Delta Time)计算
 */

#include "engine/scripting/lua/lua_runtime.h"
#include "engine/scripting/lua/lua_debugger.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_registry.h"
#if defined(DSE_ENABLE_HTTP) || defined(DSE_NET_ENABLED)
#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#endif
#include "engine/ecs/script.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
#include <algorithm>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <unordered_map>
extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
#include "depends/lua/lualib.h"
}

namespace dse::runtime {
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
    bool shutting_down = false;
    std::string startup_script_path;
    std::string startup_script_override;
    LuaApiContext api_context;
    std::unordered_map<std::uint32_t, ScriptInstance> script_instances;
    size_t lua_memory_usage = 0;
};

RuntimeState& State() {
    static RuntimeState state;
    return state;
}

static void* LuaMemoryAllocator(void* ud, void* ptr, size_t osize, size_t nsize) {
    (void)ud;
    auto& state = State();

    if (nsize == 0) {
        if (ptr) {
            state.lua_memory_usage = (osize <= state.lua_memory_usage)
                ? (state.lua_memory_usage - osize)
                : 0;
        }
        std::free(ptr);
        return nullptr;
    }

    void* new_ptr = std::realloc(ptr, nsize);
    if (!new_ptr) {
        return nullptr;
    }

    if (!ptr) {
        state.lua_memory_usage += nsize;
    } else if (nsize >= osize) {
        state.lua_memory_usage += (nsize - osize);
    } else {
        const size_t shrink = osize - nsize;
        state.lua_memory_usage = (shrink <= state.lua_memory_usage)
            ? (state.lua_memory_usage - shrink)
            : 0;
    }

    return new_ptr;
}

lua_State* CreateLuaState(lua_State* (*new_state)(lua_Alloc), lua_Alloc allocator) {
    lua_State* L = new_state(allocator);
    if (L) {
        lua_setallocf(L, allocator, nullptr);
    }
    return L;
}

lua_State* CreateLuaState(lua_State* (*new_state)(lua_Alloc, void*), lua_Alloc allocator) {
    return new_state(allocator, nullptr);
}

lua_State* CreateLuaState(lua_State* (*new_state)(lua_Alloc, void*, unsigned int), lua_Alloc allocator) {
    return new_state(allocator, nullptr, 0);
}

lua_State* CreateLuaState() {
    return CreateLuaState(&lua_newstate, LuaMemoryAllocator);
}

std::string ResolveStartupLuaScript() {
    // 磁盘优先：override / 环境变量指向的真实文件。
    if (!State().startup_script_override.empty() && std::filesystem::exists(State().startup_script_override)) {
        return State().startup_script_override;
    }
    if (const char* script_from_env = std::getenv("DSE_STARTUP_LUA")) {
        if (script_from_env[0] != '\0' && std::filesystem::exists(script_from_env)) {
            return script_from_env;
        }
    }
    // 磁盘不存在：保留逻辑路径（如 "scripts/main.lua"），交由已挂载的 .bun/.dpak VFS 加载。
    // 端到端加密发行版中入口脚本只存在于资源包内，磁盘上没有明文。
    if (!State().startup_script_override.empty()) {
        return State().startup_script_override;
    }
    if (const char* script_from_env = std::getenv("DSE_STARTUP_LUA")) {
        if (script_from_env[0] != '\0') {
            return script_from_env;
        }
    }
    return "";
}

// 加载并执行启动脚本：磁盘优先，其次从 AssetManager 的 VFS（已挂载的 .bun/.dpak）读取。
// 失败时错误信息保证压栈，供调用方 lua_tostring(L, -1) 读取。
bool LoadAndRunStartupScript(lua_State* L, const std::string& path) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && !ec) {
        return luaL_dofile(L, path.c_str()) == LUA_OK;
    }
    AssetManager* am = State().api_context.asset_manager;
    if (!am) {
        lua_pushfstring(L, "startup script '%s' not on disk and no asset manager for VFS lookup", path.c_str());
        return false;
    }
    std::vector<uint8_t> data;
    if (!am->LoadFileToMemory(path, data)) {
        lua_pushfstring(L, "startup script '%s' not found on disk or in mounted bundle/pak", path.c_str());
        return false;
    }
    const std::string chunkname = "@" + path;
    if (luaL_loadbuffer(L, reinterpret_cast<const char*>(data.data()), data.size(), chunkname.c_str()) != LUA_OK) {
        return false; // load 错误已压栈
    }
    return lua_pcall(L, 0, LUA_MULTRET, 0) == LUA_OK; // run 错误（若有）已压栈
}

// package.searchers 的自定义 searcher：从 VFS 解析 require 的模块（支持加密 Lua）。
int VfsLuaSearcher(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    AssetManager* am = State().api_context.asset_manager;
    if (!am) {
        lua_pushstring(L, "\n\t[VFS] no asset manager");
        return 1;
    }
    std::string mod = name;
    std::replace(mod.begin(), mod.end(), '.', '/');
    const std::string candidates[] = {
        mod + ".lua",
        "scripts/" + mod + ".lua",
        "script/" + mod + ".lua",
        mod + "/init.lua",
        "scripts/" + mod + "/init.lua",
    };
    for (const auto& cand : candidates) {
        std::vector<uint8_t> data;
        if (am->LoadFileToMemory(cand, data)) {
            const std::string chunkname = "@" + cand;
            if (luaL_loadbuffer(L, reinterpret_cast<const char*>(data.data()), data.size(), chunkname.c_str()) != LUA_OK) {
                return lua_error(L);
            }
            lua_pushstring(L, cand.c_str()); // loader 的第二返回值（传给 loader 的额外数据）
            return 2;
        }
    }
    lua_pushfstring(L, "\n\tno VFS module '%s'", name);
    return 1;
}

// 把 VFS searcher 追加到 package.searchers 末尾（不影响磁盘 package.path 行为）。
void RegisterVfsLuaSearcher(lua_State* L) {
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_getfield(L, -1, "searchers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return;
    }
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, -1));
    lua_pushcfunction(L, VfsLuaSearcher);
    lua_rawseti(L, -2, count + 1);
    lua_pop(L, 2);
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
        DEBUG_LOG_ERROR("Lua method {} failed: {}", method_name, lua_tostring(L, -1));
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
        DEBUG_LOG_ERROR("Lua method OnSerializeState failed: {}", lua_tostring(L, -1));
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
        DEBUG_LOG_ERROR("Lua method OnDeserializeState failed: {}", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    luaL_unref(L, LUA_REGISTRYINDEX, state_ref);
}

void DestroyScriptInstance(lua_State* L, int entity_id, RuntimeState::ScriptInstance& instance, bool call_destroy = true) {
    if (instance.table_ref == LUA_NOREF) {
        DEBUG_LOG_INFO("[LuaRuntime] DestroyScriptInstance skipped entity={} ref=LUA_NOREF", entity_id);
        return;
    }

    auto& runtime_state = State();
    DEBUG_LOG_INFO("[LuaRuntime] DestroyScriptInstance begin entity={} ref={} awake_called={} call_destroy={} shutting_down={} state={}",
                   entity_id,
                   instance.table_ref,
                   instance.awake_called ? 1 : 0,
                   call_destroy ? 1 : 0,
                   runtime_state.shutting_down ? 1 : 0,
                   static_cast<void*>(L));
    if (call_destroy && instance.awake_called && !runtime_state.shutting_down) {
        DEBUG_LOG_INFO("[LuaRuntime] invoking OnDestroy entity={} ref={}", entity_id, instance.table_ref);
        CallScriptTableMethod(L, instance.table_ref, "OnDestroy", entity_id, 0.0f, false);
    }

    DEBUG_LOG_INFO("[LuaRuntime] luaL_unref entity={} ref={}", entity_id, instance.table_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, instance.table_ref);
    instance.table_ref = LUA_NOREF;
    instance.awake_called = false;
    instance.script_path.clear();
    instance.has_last_write_time = false;
    instance.last_write_time = std::filesystem::file_time_type::min();
    DEBUG_LOG_INFO("[LuaRuntime] DestroyScriptInstance end entity={}", entity_id);
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
        DEBUG_LOG_ERROR("Lua script load failed: {}, error: {}", script_path, lua_tostring(L, -1));
        lua_pop(L, 1);
        if (serialized_state_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, serialized_state_ref);
        }
        return false;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        DEBUG_LOG_ERROR("Lua script invalid return value, expected table: {}", script_path);
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
    if (hot_reload) {
        DEBUG_LOG_INFO("[LuaRuntime] Hot-reloaded script: {} (entity={})", script_path, entity_id);
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

void ConfigureLuaApiContext(const LuaApiContext& context) {
    State().api_context = context;
    lua_binding::ConfigureBindingContext(State().api_context);
}

void SetStartupLuaScriptPath(const std::string& script_path) {
    State().startup_script_override = script_path;
}

void ShutdownLuaRuntime() {
    auto& state = State();
    DEBUG_LOG_INFO("[LuaRuntime] Shutdown begin state={} script_instances={} lua_memory_usage={} awake_called={}",
                   static_cast<void*>(state.state),
                   state.script_instances.size(),
                   state.lua_memory_usage,
                   state.awake_called ? 1 : 0);
    state.shutting_down = true;
    if (state.state) {
        for (auto& pair : state.script_instances) {
            DestroyScriptInstance(state.state, static_cast<int>(pair.first), pair.second, false);
        }
        state.script_instances.clear();
        // 必须在 lua_close 之前 detach：Detach() 会对 lua_State 调用 lua_sethook，
        // 若先 close 再 detach 则是对已释放的 lua_State 解引用（偶发访问越界）。
        dse::scripting::LuaDebugger::Instance().Detach();
        DEBUG_LOG_INFO("[LuaRuntime] lua_close state={}", static_cast<void*>(state.state));
        lua_close(state.state);
        state.state = nullptr;
        DEBUG_LOG_INFO("[LuaRuntime] lua_close complete");
    } else {
        dse::scripting::LuaDebugger::Instance().Detach();
    }
    state.lua_memory_usage = 0;
    state.awake_called = false;
    state.startup_script_path.clear();
    state.shutting_down = false;
    DEBUG_LOG_INFO("[LuaRuntime] Shutdown end");
}

bool BootstrapLuaRuntime() {
    auto& state = State();
    ShutdownLuaRuntime();
    if (!state.api_context.world) {
        DEBUG_LOG_ERROR("Lua init failed: LuaApiContext.world is null");
        return false;
    }
    DEBUG_LOG_INFO("[LuaRuntime] lua_newstate begin allocator={} seed=0", reinterpret_cast<const void*>(&LuaMemoryAllocator));
    state.state = CreateLuaState();
    if (!state.state) {
        DEBUG_LOG_ERROR("Lua init failed: lua_newstate returned nullptr");
        return false;
    }
    state.startup_script_path = ResolveStartupLuaScript();
    DEBUG_LOG_INFO("Lua bootstrap: startup script resolved to {}", state.startup_script_path.empty() ? "<empty>" : state.startup_script_path);
    if (state.startup_script_path.empty()) {
        DEBUG_LOG_ERROR("Lua startup script not found, set by SetStartupLuaScriptPath or DSE_STARTUP_LUA");
        lua_close(state.state);
        state.state = nullptr;
        state.lua_memory_usage = 0;
        return false;
    }
    luaL_requiref(state.state, LUA_GNAME, luaopen_base, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_LOADLIBNAME, luaopen_package, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_COLIBNAME, luaopen_coroutine, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_IOLIBNAME, luaopen_io, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_OSLIBNAME, luaopen_os, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(state.state, 1);
    luaL_requiref(state.state, LUA_DBLIBNAME, luaopen_debug, 1);
    lua_pop(state.state, 1);
    DEBUG_LOG_INFO("Lua bootstrap: package path setup begin");
    SetupLuaPackagePath(state.state, state.startup_script_path);
    // 注册 VFS searcher：让 require 也能解析加密 .bun/.dpak 内的 Lua 模块。
    RegisterVfsLuaSearcher(state.state);
    DEBUG_LOG_INFO("Lua bootstrap: register API begin");
    lua_binding::RegisterPhase1LuaApi(state.state);
    DEBUG_LOG_INFO("Lua bootstrap: loading startup script begin");
    if (!LoadAndRunStartupScript(state.state, state.startup_script_path)) {
        DEBUG_LOG_ERROR("Lua startup load failed: {}", lua_tostring(state.state, -1));
        lua_pop(state.state, 1);
        lua_close(state.state);
        state.state = nullptr;
        state.lua_memory_usage = 0;
        return false;
    }
    DEBUG_LOG_INFO("Lua bootstrap: startup script load OK");
    lua_getglobal(state.state, "Awake");
    if (lua_isfunction(state.state, -1)) {
        DEBUG_LOG_INFO("Lua bootstrap: Awake begin");
        if (lua_pcall(state.state, 0, 0, 0) != LUA_OK) {
            DEBUG_LOG_ERROR("Lua Awake failed: {}", lua_tostring(state.state, -1));
            lua_pop(state.state, 1);
            lua_close(state.state);
            state.state = nullptr;
            state.lua_memory_usage = 0;
            return false;
        }
        state.awake_called = true;
        DEBUG_LOG_INFO("Lua bootstrap: Awake OK");
    } else {
        lua_pop(state.state, 1);
    }
    DEBUG_LOG_INFO("Lua startup script loaded: {}", state.startup_script_path);
    dse::scripting::LuaDebugger::Instance().Attach(state.state);
    return true;
}

void TickLuaRuntime(float delta_time) {
    auto& state = State();
    if (!state.state) {
        return;
    }
#ifdef DSE_ENABLE_HTTP
    // 触发本帧已完成的异步 HTTP 回调（在脚本 Update 之前，使脚本可立即消费结果）
    lua_binding::PumpHttp(state.state);
#endif
#ifdef DSE_NET_ENABLED
    // 派发本帧网络事件（连接/断开/收消息）回调，在脚本 Update 之前。
    lua_binding::PumpNet(state.state);
#endif
    lua_getglobal(state.state, "Update");
    if (!lua_isfunction(state.state, -1)) {
        lua_pop(state.state, 1);
    } else {
        lua_pushnumber(state.state, delta_time);
        if (lua_pcall(state.state, 1, 0, 0) != LUA_OK) {
            DEBUG_LOG_ERROR("Lua Update failed: {}", lua_tostring(state.state, -1));
            lua_pop(state.state, 1);
        }
    }
    UpdateScriptComponents(delta_time);
}

void BroadcastLuaEvent(const char* event_name) {
    auto& state = State();
    if (!state.state) return;
    for (auto& pair : state.script_instances) {
        CallScriptTableMethod(state.state, pair.second.table_ref, event_name, static_cast<int>(pair.first), 0.0f, false);
    }
}

size_t GetLuaMemoryUsage() {
    return State().lua_memory_usage;
}

int PumpLuaScriptHotReloads() {
    auto& state = State();
    if (!state.state || !state.api_context.world) return 0;
    auto& registry = state.api_context.world->registry();
    int reloaded = 0;
    auto view = registry.view<ScriptComponent>();
    for (auto entity : view) {
        auto& sc = view.get<ScriptComponent>(entity);
        if (!sc.enabled || sc.script_path.empty()) continue;
        std::uint32_t key = EntityToKey(entity);
        auto it = state.script_instances.find(key);
        if (it == state.script_instances.end()) continue;
        auto& instance = it->second;
        std::filesystem::file_time_type wt;
        if (!QueryScriptFileWriteTime(instance.script_path, wt)) continue;
        if (!instance.has_last_write_time || wt == instance.last_write_time) continue;
        // File changed — reload
        if (EnsureScriptInstanceLoaded(state.state, static_cast<int>(key), sc.script_path, instance)) {
            ++reloaded;
        }
    }
    return reloaded;
}

bool ExecuteLuaString(const char* code, std::string* out_result) {
    auto& state = State();
    if (!state.state) {
        if (out_result) *out_result = "[error] Lua VM not initialized";
        return false;
    }
    lua_State* L = state.state;
    int top = lua_gettop(L);

    // Try "return <code>" first to evaluate expressions
    std::string expr_code = std::string("return ") + code;
    int load_result = luaL_loadstring(L, expr_code.c_str());
    if (load_result != 0) {
        lua_pop(L, 1);
        // Fall back to raw statement
        load_result = luaL_loadstring(L, code);
        if (load_result != 0) {
            if (out_result) *out_result = lua_tostring(L, -1);
            lua_settop(L, top);
            return false;
        }
    }

    int call_result = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (call_result != 0) {
        if (out_result) *out_result = lua_tostring(L, -1);
        lua_settop(L, top);
        return false;
    }

    // Collect return values
    int nresults = lua_gettop(L) - top;
    if (out_result) {
        out_result->clear();
        for (int i = 1; i <= nresults; ++i) {
            if (i > 1) out_result->append("\t");
            int idx = top + i;
            if (lua_isstring(L, idx)) {
                out_result->append(lua_tostring(L, idx));
            } else if (lua_isnumber(L, idx)) {
                out_result->append(std::to_string(lua_tonumber(L, idx)));
            } else if (lua_isboolean(L, idx)) {
                out_result->append(lua_toboolean(L, idx) ? "true" : "false");
            } else if (lua_isnil(L, idx)) {
                out_result->append("nil");
            } else {
                out_result->append(luaL_typename(L, idx));
                out_result->append(": ");
                char addr[32];
                std::snprintf(addr, sizeof(addr), "%p", lua_topointer(L, idx));
                out_result->append(addr);
            }
        }
    }
    lua_settop(L, top);
    return true;
}

}
