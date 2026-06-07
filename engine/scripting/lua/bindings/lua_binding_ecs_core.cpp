/**
 * @file lua_binding_ecs_core.cpp
 * @brief ECS Lua 绑定 — 实体创建、场景加载、实体查询、层级、脚本
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/script.h"
#include "engine/scene/scene.h"
#include "engine/scene/sub_scene.h"
#include "engine/assets/asset_manager.h"

#include <glm/glm.hpp>
extern "C" {
#include "depends/lua/lauxlib.h"
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
    helper::PushEntity(L, e);
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

int L_EcsLoadSubScene(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "world_unavailable");
        return 2;
    }
    const char* path = luaL_checkstring(L, 1);
    AssetManager& am = GetAssetManager();
    std::string resolved = am.GetDataRoot();
    if (!resolved.empty() && resolved.back() != '/' && resolved.back() != '\\') {
        resolved += '/';
    }
    resolved += path;
    scene::SubScene sub;
    const bool ok = sub.Load(*world, am, resolved);
    lua_pushboolean(L, ok ? 1 : 0);
    if (ok) {
        lua_pushinteger(L, static_cast<lua_Integer>(sub.EntityCount()));
    } else {
        lua_pushstring(L, "sub_scene_load_failed");
    }
    return 2;
}

int L_EcsDestroyEntity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    if (world->registry().valid(e)) {
        world->DestroyEntity(e);
    }
    return 0;
}

int L_EcsFindEntitiesByMeshPath(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_newtable(L);
        return 1;
    }

    const char* mesh_path = luaL_checkstring(L, 1);
    lua_newtable(L);
    int index = 1;
    auto view = world->registry().view<MeshRendererComponent>();
    for (auto entity : view) {
        const auto& mesh = view.get<MeshRendererComponent>(entity);
        if (mesh.mesh_path == mesh_path) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, index++);
        }
    }
    return 1;
}

int L_EcsAddTransform(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float x  = helper::CheckFloat(L, 2);
    float y  = helper::CheckFloat(L, 3);
    float z  = helper::OptFloat(L, 4, 0.0f);
    float sx = helper::OptFloat(L, 5, 1.0f);
    float sy = helper::OptFloat(L, 6, 1.0f);
    float sz = helper::OptFloat(L, 7, 1.0f);
    auto& transform = world->registry().emplace_or_replace<TransformComponent>(e);
    transform.position = glm::vec3(x, y, z);
    transform.scale    = glm::vec3(sx, sy, sz);
    transform.dirty    = true;
    return 0;
}

// ============================================================
// ParentComponent（层级）
// ============================================================

int L_EcsAddParent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    Entity parent_e = helper::CheckEntity(L, 2);
    auto& pc = world->registry().emplace_or_replace<ParentComponent>(e);
    pc.parent = parent_e;
    return 0;
}

int L_EcsSetParent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    Entity parent_e = helper::CheckEntity(L, 2);
    auto* pc = helper::TryGetComponent<ParentComponent>(*world, e);
    if (!pc) return 0;
    pc->parent = parent_e;
    return 0;
}

int L_EcsGetParent(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnil(L); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* pc = helper::TryGetComponentConst<ParentComponent>(*world, e);
    if (!pc || pc->parent == entt::null) {
        lua_pushnil(L);
        return 1;
    }
    helper::PushEntity(L, pc->parent);
    return 1;
}

int L_EcsClearParent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    if (world->registry().valid(e) && world->registry().all_of<ParentComponent>(e)) {
        world->registry().remove<ParentComponent>(e);
    }
    return 0;
}

// ============================================================
// ScriptComponent
// ============================================================

int L_EcsAddScript(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* script_path = luaL_checkstring(L, 2);
    auto& sc = world->registry().emplace_or_replace<ScriptComponent>(e);
    sc.script_path = script_path;
    sc.enabled = true;
    return 0;
}

int L_EcsSetScriptPath(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* path = luaL_checkstring(L, 2);
    auto* sc = helper::TryGetComponent<ScriptComponent>(*world, e);
    if (!sc) return 0;
    sc->script_path = path;
    return 0;
}

int L_EcsGetScriptPath(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnil(L); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* sc = helper::TryGetComponentConst<ScriptComponent>(*world, e);
    if (!sc) { lua_pushnil(L); return 1; }
    lua_pushstring(L, sc->script_path.c_str());
    return 1;
}

int L_EcsSetScriptEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool enabled = helper::CheckBool(L, 2);
    auto* sc = helper::TryGetComponent<ScriptComponent>(*world, e);
    if (!sc) return 0;
    sc->enabled = enabled;
    return 0;
}

int L_EcsGetScriptEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* sc = helper::TryGetComponentConst<ScriptComponent>(*world, e);
    if (!sc) { lua_pushboolean(L, 0); return 1; }
    helper::PushBool(L, sc->enabled);
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
        {"find_entities_by_mesh_path", L_EcsFindEntitiesByMeshPath},
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
