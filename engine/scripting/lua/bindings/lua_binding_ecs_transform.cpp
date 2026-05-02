/**
 * @file lua_binding_ecs_transform.cpp
 * @brief ECS Lua 绑定 — TransformComponent 位置/旋转
 *
 * 示范使用 DSE_LUA_COMPONENT_* 宏替代手写 getter/setter。
 * position 字段使用 VEC3_DIRTY 宏，setter 自动标记 dirty。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/world.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dse::runtime::lua_binding {
namespace {

// 使用宏生成 get_transform_position / set_transform_position（setter 自动标记 dirty）
DSE_LUA_COMPONENT_VEC3_DIRTY(TransformPosition, TransformComponent, position, dirty)

int L_EcsAddTransform(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float x = helper::CheckFloat(L, 2);
    float y = helper::CheckFloat(L, 3);
    float z = helper::OptFloat(L, 4, 0.0f);
    float sx = helper::OptFloat(L, 5, 1.0f);
    float sy = helper::OptFloat(L, 6, 1.0f);
    float sz = helper::OptFloat(L, 7, 1.0f);
    auto& transform = world->registry().emplace_or_replace<TransformComponent>(e);
    transform.position = glm::vec3(x, y, z);
    transform.scale = glm::vec3(sx, sy, sz);
    transform.dirty = true;
    return 0;
}

int L_EcsSetTransformRotation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* transform = helper::TryGetComponent<TransformComponent>(*world, e);
    if (!transform) return 0;
    float x = helper::CheckFloat(L, 2);
    float y = helper::CheckFloat(L, 3);
    float z = helper::CheckFloat(L, 4);
    glm::vec3 euler_angles(glm::radians(x), glm::radians(y), glm::radians(z));
    transform->rotation = glm::quat(euler_angles);
    transform->dirty = true;
    return 0;
}

} // namespace

void RegisterEcsTransformBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_transform",            L_EcsAddTransform},
        {"get_transform_position",   L_EcsGetTransformPosition},
        {"set_transform_position",   L_EcsSetTransformPosition},
        {"set_transform_rotation",   L_EcsSetTransformRotation},
    });
}

} // namespace dse::runtime::lua_binding
