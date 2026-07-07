/**
 * @file lua_binding_ecs_core.cpp
 * @brief ECS Lua 绑定 — 实体创建、场景加载、实体查询、层级、脚本。薄包装委托至 C ABI。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

constexpr uint32_t kInvalidEntity = 0xFFFFFFFFu;

uint32_t EID(lua_State* L, int index) {
    return static_cast<uint32_t>(luaL_checkinteger(L, index));
}

// 将 '\n' 分隔的字符串拆分并逐条压入 Lua 数组表（表须已在栈顶）。
void PushLinesAsArray(lua_State* L, const std::string& joined, int count) {
    if (count <= 0 || joined.empty()) return;
    int idx = 1;
    size_t start = 0;
    while (start <= joined.size()) {
        size_t pos = joined.find('\n', start);
        std::string item = (pos == std::string::npos)
                               ? joined.substr(start)
                               : joined.substr(start, pos - start);
        lua_pushlstring(L, item.data(), item.size());
        lua_rawseti(L, -2, idx++);
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
}

int L_EcsCreateEntity(lua_State* L) {
    lua_pushinteger(L, dse_entity_create());
    return 1;
}

int L_EcsLoadScene(lua_State* L) {
    const bool ok = dse_scene_load(luaL_checkstring(L, 1)) != 0;
    lua_pushboolean(L, ok ? 1 : 0);
    lua_pushstring(L, ok ? "" : "scene_deserialize_failed");
    return 2;
}

int L_EcsLoadSubScene(lua_State* L) {
    int entity_count = 0;
    const bool ok = dse_scene_load_sub(luaL_checkstring(L, 1), &entity_count) != 0;
    lua_pushboolean(L, ok ? 1 : 0);
    if (ok) {
        lua_pushinteger(L, entity_count);
    } else {
        lua_pushstring(L, "sub_scene_load_failed");
    }
    return 2;
}

int L_EcsDestroyEntity(lua_State* L) {
    dse_entity_destroy(EID(L, 1));
    return 0;
}

int L_EcsFindEntitiesByMeshPath(lua_State* L) {
    const char* mesh_path = luaL_checkstring(L, 1);
    lua_newtable(L);
    int total = dse_ecs_find_entities_by_mesh_path(mesh_path, nullptr, 0);
    if (total <= 0) return 1;
    std::vector<uint32_t> ids(static_cast<size_t>(total));
    int n = dse_ecs_find_entities_by_mesh_path(mesh_path, ids.data(), total);
    if (n > total) n = total;
    for (int i = 0; i < n; ++i) {
        lua_pushinteger(L, ids[static_cast<size_t>(i)]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int L_EcsAddTransform(lua_State* L) {
    dse_ecs_add_transform(EID(L, 1),
                          helper::CheckFloat(L, 2), helper::CheckFloat(L, 3),
                          helper::OptFloat(L, 4, 0.0f),
                          helper::OptFloat(L, 5, 1.0f), helper::OptFloat(L, 6, 1.0f),
                          helper::OptFloat(L, 7, 1.0f));
    return 0;
}

// ============================================================
// ParentComponent（层级）
// ============================================================

int L_EcsAddParent(lua_State* L) {
    dse_ecs_add_parent(EID(L, 1), EID(L, 2));
    return 0;
}

int L_EcsSetParent(lua_State* L) {
    dse_ecs_set_parent(EID(L, 1), EID(L, 2));
    return 0;
}

int L_EcsGetParent(lua_State* L) {
    uint32_t parent = dse_ecs_get_parent(EID(L, 1));
    if (parent == kInvalidEntity) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, parent);
    return 1;
}

int L_EcsClearParent(lua_State* L) {
    dse_ecs_clear_parent(EID(L, 1));
    return 0;
}

// ============================================================
// ScriptComponent
// ============================================================

int L_EcsAddScript(lua_State* L) {
    dse_ecs_add_script(EID(L, 1), luaL_checkstring(L, 2));
    return 0;
}

int L_EcsSetScriptPath(lua_State* L) {
    dse_ecs_set_script_path(EID(L, 1), luaL_checkstring(L, 2));
    return 0;
}

int L_EcsGetScriptPath(lua_State* L) {
    char buf[512];
    int n = dse_ecs_get_script_path(EID(L, 1), buf, sizeof(buf));
    if (n < 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, buf, static_cast<size_t>(n));
    return 1;
}

int L_EcsSetScriptEnabled(lua_State* L) {
    dse_ecs_set_script_enabled(EID(L, 1), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

int L_EcsGetScriptEnabled(lua_State* L) {
    int enabled = dse_ecs_get_script_enabled(EID(L, 1));
    lua_pushboolean(L, enabled == 1 ? 1 : 0);
    return 1;
}

// ============================================================
// SceneManager（异步加载 / 卸载 / 查询 / 场景过渡）
// ============================================================

// ecs.load_sub_scene_async(path)
int L_EcsLoadSubSceneAsync(lua_State* L) {
    lua_pushboolean(L, dse_scene_load_sub_async(luaL_checkstring(L, 1)));
    return 1;
}

// ecs.unload_sub_scene(path)
int L_EcsUnloadSubScene(lua_State* L) {
    dse_scene_unload_sub(luaL_checkstring(L, 1));
    return 0;
}

// ecs.unload_all_sub_scenes()
int L_EcsUnloadAllSubScenes(lua_State* L) {
    (void)L;
    dse_scene_unload_all_subs();
    return 0;
}

// ecs.is_sub_scene_loaded(path) -> bool
int L_EcsIsSubSceneLoaded(lua_State* L) {
    lua_pushboolean(L, dse_scene_is_sub_loaded(luaL_checkstring(L, 1)));
    return 1;
}

// ecs.get_loaded_sub_scenes() -> table（完整解析后的路径）
int L_EcsGetLoadedSubScenes(lua_State* L) {
    lua_newtable(L);
    std::vector<char> buf(16384);
    int count = dse_scene_get_loaded_subs(buf.data(), static_cast<int>(buf.size()));
    PushLinesAsArray(L, std::string(buf.data()), count);
    return 1;
}

// ecs.get_sub_scene_count() -> int
int L_EcsGetSubSceneCount(lua_State* L) {
    lua_pushinteger(L, dse_scene_get_sub_count());
    return 1;
}

// ecs.get_pending_scene_count() -> int
int L_EcsGetPendingSceneCount(lua_State* L) {
    lua_pushinteger(L, dse_scene_get_pending_count());
    return 1;
}

// ecs.transition_to(path, [mode="fade"], [fade_duration=0.5])
//   mode: "instant" | "additive" | "fade"，或整数 0/1/2
int L_EcsTransitionTo(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int mode = 2;  // fade
    if (lua_isnumber(L, 2)) {
        int m = static_cast<int>(lua_tointeger(L, 2));
        mode = (m == 0 || m == 1) ? m : 2;
    } else if (lua_isstring(L, 2)) {
        const char* s = lua_tostring(L, 2);
        if (std::strcmp(s, "instant") == 0) mode = 0;
        else if (std::strcmp(s, "additive") == 0) mode = 1;
    }
    float fade = static_cast<float>(luaL_optnumber(L, 3, 0.5));
    dse_scene_transition_to(path, mode, fade);
    return 0;
}

// ecs.get_transition_state() -> string ("idle"|"fading_out"|"loading"|"fading_in")
int L_EcsGetTransitionState(lua_State* L) {
    const char* s = "idle";
    switch (dse_scene_get_transition_state()) {
        case 1: s = "fading_out"; break;
        case 2: s = "loading"; break;
        case 3: s = "fading_in"; break;
        default: s = "idle"; break;
    }
    lua_pushstring(L, s);
    return 1;
}

// ecs.get_fade_progress() -> number [0,1]
int L_EcsGetFadeProgress(lua_State* L) {
    lua_pushnumber(L, dse_scene_get_fade_progress());
    return 1;
}

// ecs.get_active_scene() -> string
int L_EcsGetActiveScene(lua_State* L) {
    char buf[1024];
    int n = dse_scene_get_active(buf, sizeof(buf));
    lua_pushlstring(L, buf, static_cast<size_t>(n < 0 ? 0 : n));
    return 1;
}

// ============================================================
// UUIDComponent（跨场景稳定引用）
// ============================================================

// ecs.get_uuid(e) -> string|nil（16 位十六进制）
int L_EcsGetUuid(lua_State* L) {
    char buf[64];
    if (!dse_uuid_get(EID(L, 1), buf, sizeof(buf))) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}

// ecs.set_uuid(e, [uuid_str]) -> string
//   省略 uuid_str 时自动生成；返回最终的 UUID 十六进制字符串
int L_EcsSetUuid(lua_State* L) {
    const char* uuid_str = lua_isstring(L, 2) ? lua_tostring(L, 2) : nullptr;
    char buf[64];
    if (!dse_uuid_set(EID(L, 1), uuid_str, buf, sizeof(buf))) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}

// ecs.resolve_uuid(uuid_str) -> entity|nil
//   仅能解析经由 SceneManager 加载的子场景中带 UUIDComponent 的实体
int L_EcsResolveUuid(lua_State* L) {
    const char* uuid_str;
    char numbuf[32];
    if (lua_isstring(L, 1)) {
        uuid_str = lua_tostring(L, 1);
    } else {
        // 整数形式转为十六进制字符串
        lua_Integer v = luaL_checkinteger(L, 1);
        std::snprintf(numbuf, sizeof(numbuf), "%016llx",
                      static_cast<unsigned long long>(v));
        uuid_str = numbuf;
    }
    uint32_t e = dse_uuid_resolve(uuid_str);
    if (e == kInvalidEntity) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, e);
    return 1;
}

// ============================================================
// 通用 ECS 组件查询（按组件名字符串）
// ============================================================

// ecs.find_entities_with(component_name) -> table（持有该组件的全部实体）
//   component_name 见文档 §5.1 支持列表；未知名抛出 Lua 错误。
int L_EcsFindEntitiesWith(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int total = dse_ecs_find_entities_with(name, nullptr, 0);
    if (total < 0) {
        return luaL_error(L, "find_entities_with: unknown component '%s'", name);
    }
    lua_newtable(L);
    if (total == 0) return 1;
    std::vector<uint32_t> ids(static_cast<size_t>(total));
    int n = dse_ecs_find_entities_with(name, ids.data(), total);
    if (n > total) n = total;
    for (int i = 0; i < n; ++i) {
        lua_pushinteger(L, ids[static_cast<size_t>(i)]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// ecs.count_entities_with(component_name) -> int
int L_EcsCountEntitiesWith(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int count = dse_ecs_count_entities_with(name);
    if (count < 0) {
        return luaL_error(L, "count_entities_with: unknown component '%s'", name);
    }
    lua_pushinteger(L, count);
    return 1;
}

// ecs.has_component(entity, component_name) -> bool
int L_EcsHasComponent(lua_State* L) {
    uint32_t e = EID(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int has = dse_ecs_has_component(e, name);
    if (has < 0) {
        return luaL_error(L, "has_component: unknown component '%s'", name);
    }
    lua_pushboolean(L, has);
    return 1;
}

// ecs.get_queryable_components() -> table（全部支持查询的组件名）
int L_EcsGetQueryableComponents(lua_State* L) {
    lua_newtable(L);
    std::vector<char> buf(8192);
    int count = dse_ecs_get_queryable_components(buf.data(), static_cast<int>(buf.size()));
    PushLinesAsArray(L, std::string(buf.data()), count);
    return 1;
}

// ============================================================
// 场景 / 预制体 保存（写盘，补齐此前只读的加载侧）
// ============================================================

// ecs.save_scene(path) -> bool, string
//   把当前 World 完整序列化到 path（路径按字面使用，不做 data root 拼接）。
int L_EcsSaveScene(lua_State* L) {
    const bool ok = dse_scene_save(luaL_checkstring(L, 1)) != 0;
    lua_pushboolean(L, ok ? 1 : 0);
    lua_pushstring(L, ok ? "" : "scene_serialize_failed");
    return 2;
}

// ecs.save_prefab(entity, path) -> bool
int L_EcsSavePrefab(lua_State* L) {
    lua_pushboolean(L, dse_scene_save_prefab(EID(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

// ecs.instantiate_prefab(path, [x, y, z]) -> entity|nil
//   省略坐标时使用预制体内置 Transform。
int L_EcsInstantiatePrefab(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint32_t e;
    if (lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
        e = dse_scene_instantiate_prefab(path,
                                         static_cast<float>(lua_tonumber(L, 2)),
                                         static_cast<float>(lua_tonumber(L, 3)),
                                         static_cast<float>(lua_tonumber(L, 4)), 1);
    } else {
        e = dse_scene_instantiate_prefab(path, 0.0f, 0.0f, 0.0f, 0);
    }
    if (!dse_entity_valid(e)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, e);
    return 1;
}

// ============================================================
// BoundingBoxComponent（只读 AABB 查询）
// ============================================================

// ecs.get_world_aabb(e) -> min_x,min_y,min_z, max_x,max_y,max_z | nil
//   返回实体的世界空间轴对齐包围盒（AABB）。无 BoundingBoxComponent 时返回 nil；
//   无 TransformComponent 时按单位变换（世界==模型）返回。
//   该组件由渲染/剔除系统更新，脚本读到的是上一帧结果。
int L_EcsGetWorldAabb(lua_State* L) {
    float mm[6] = {0};
    if (!dse_ecs_get_world_aabb(EID(L, 1), mm)) {
        lua_pushnil(L);
        return 1;
    }
    for (int i = 0; i < 6; ++i) lua_pushnumber(L, mm[i]);
    return 6;
}

// ecs.get_local_aabb(e) -> min_x,min_y,min_z, max_x,max_y,max_z | nil
//   返回 BoundingBoxComponent 原始的模型空间 AABB（不施加 Transform）。
int L_EcsGetLocalAabb(lua_State* L) {
    float mm[6] = {0};
    if (!dse_ecs_get_local_aabb(EID(L, 1), mm)) {
        lua_pushnil(L);
        return 1;
    }
    for (int i = 0; i < 6; ++i) lua_pushnumber(L, mm[i]);
    return 6;
}

// ============================================================
// TimeScaleComponent（逐实体 / 分层时间缩放）
// ============================================================

// ecs.set_time_scale(e, s)：设置实体局部时间缩放（无组件则自动添加），s 钳制为 >=0。
//   实体最终生效缩放 = 全局 time-scale × 局部 scale（对标 Unreal CustomTimeDilation）。
int L_EcsSetTimeScale(lua_State* L) {
    dse_ecs_set_time_scale(EID(L, 1), helper::CheckFloat(L, 2));
    return 0;
}

// ecs.get_time_scale(e) -> number：实体局部时间缩放；无 TimeScaleComponent 时返回 1.0。
int L_EcsGetTimeScale(lua_State* L) {
    lua_pushnumber(L, dse_ecs_get_time_scale(EID(L, 1)));
    return 1;
}

} // namespace

void RegisterEcsCoreBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"create_entity",              L_EcsCreateEntity},
        {"destroy_entity",             L_EcsDestroyEntity},
        {"load_scene",                 L_EcsLoadScene},
        {"load_sub_scene",             L_EcsLoadSubScene},
        // SceneManager（异步/卸载/查询/过渡）
        {"load_sub_scene_async",       L_EcsLoadSubSceneAsync},
        {"unload_sub_scene",           L_EcsUnloadSubScene},
        {"unload_all_sub_scenes",      L_EcsUnloadAllSubScenes},
        {"is_sub_scene_loaded",        L_EcsIsSubSceneLoaded},
        {"get_loaded_sub_scenes",      L_EcsGetLoadedSubScenes},
        {"get_sub_scene_count",        L_EcsGetSubSceneCount},
        {"get_pending_scene_count",    L_EcsGetPendingSceneCount},
        {"transition_to",              L_EcsTransitionTo},
        {"get_transition_state",       L_EcsGetTransitionState},
        {"get_fade_progress",          L_EcsGetFadeProgress},
        {"get_active_scene",           L_EcsGetActiveScene},
        // UUIDComponent（跨场景稳定引用）
        {"get_uuid",                   L_EcsGetUuid},
        {"set_uuid",                   L_EcsSetUuid},
        {"resolve_uuid",               L_EcsResolveUuid},
        {"find_entities_by_mesh_path", L_EcsFindEntitiesByMeshPath},
        // 通用组件查询
        {"find_entities_with",         L_EcsFindEntitiesWith},
        {"count_entities_with",        L_EcsCountEntitiesWith},
        {"has_component",              L_EcsHasComponent},
        {"get_queryable_components",   L_EcsGetQueryableComponents},
        // 场景 / 预制体保存（写盘）
        {"save_scene",                 L_EcsSaveScene},
        {"save_prefab",                L_EcsSavePrefab},
        {"instantiate_prefab",         L_EcsInstantiatePrefab},
        // BoundingBoxComponent（只读 AABB 查询）
        {"get_world_aabb",             L_EcsGetWorldAabb},
        {"get_local_aabb",             L_EcsGetLocalAabb},
        // TimeScaleComponent（逐实体 / 分层时间缩放）
        {"set_time_scale",             L_EcsSetTimeScale},
        {"get_time_scale",             L_EcsGetTimeScale},
        {"add_transform",              L_EcsAddTransform},
        // ParentComponent
        {"add_parent",                 L_EcsAddParent},
        {"set_parent",                 L_EcsSetParent},
        {"get_parent",                 L_EcsGetParent},
        {"clear_parent",               L_EcsClearParent},
        // ScriptComponent
        {"add_script",                 L_EcsAddScript},
        {"set_script_path",            L_EcsSetScriptPath},
        {"get_script_path",            L_EcsGetScriptPath},
        {"set_script_enabled",         L_EcsSetScriptEnabled},
        {"get_script_enabled",         L_EcsGetScriptEnabled},
    });
}

} // namespace dse::runtime::lua_binding
