/**
 * @file dse_api_spot_light.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：SpotLightComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>
#include <cstring>

using Entity = entt::entity;

namespace {
inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline bool V(uint32_t e) { World* w = GW(); return w && w->registry().valid(static_cast<Entity>(static_cast<entt::id_type>(e))); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
template<typename T> T* GC(uint32_t e) { World* w = GW(); if (!V(e)) return nullptr; return w->registry().try_get<T>(TE(e)); }
template<typename T> const T* GCC(uint32_t e) { return GC<T>(e); }
}

/* ---- SpotLightComponent ---- */
extern "C" void dse_spot_light_get_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::SpotLightComponent>(e)) { *x = c->color.x; *y = c->color.y; *z = c->color.z; }
}
extern "C" void dse_spot_light_set_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->color = glm::vec3(x, y, z);
    }
}
extern "C" float dse_spot_light_get_intensity(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->intensity : 1.0f;
}
extern "C" void dse_spot_light_set_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->intensity = v;
    }
}
extern "C" float dse_spot_light_get_radius(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->radius : 20.0f;
}
extern "C" void dse_spot_light_set_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->radius = v;
    }
}
extern "C" float dse_spot_light_get_inner_cone_angle(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->inner_cone_angle : 12.5f;
}
extern "C" void dse_spot_light_set_inner_cone_angle(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->inner_cone_angle = v;
    }
}
extern "C" float dse_spot_light_get_outer_cone_angle(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->outer_cone_angle : 17.5f;
}
extern "C" void dse_spot_light_set_outer_cone_angle(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->outer_cone_angle = v;
    }
}
extern "C" void dse_spot_light_get_direction(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::SpotLightComponent>(e)) { *x = c->direction.x; *y = c->direction.y; *z = c->direction.z; }
}
extern "C" void dse_spot_light_set_direction(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->direction = glm::vec3(x, y, z);
    }
}
extern "C" int dse_spot_light_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_spot_light_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" int dse_spot_light_get_cast_shadow(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_spot_light_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->cast_shadow = (v != 0);
    }
}
