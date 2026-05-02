/**
 * @file lua_binding_ecs_core.cpp
 * @brief ECS Lua 绑定 — 实体创建、场景加载、实体查询
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/scene/scene.h"
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

} // namespace

void RegisterEcsCoreBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"create_entity",              L_EcsCreateEntity},
        {"load_scene",                 L_EcsLoadScene},
        {"find_entities_by_mesh_path", L_EcsFindEntitiesByMeshPath},
    });
}

} // namespace dse::runtime::lua_binding
