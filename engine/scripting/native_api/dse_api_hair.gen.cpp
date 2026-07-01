/**
 * @file dse_api_hair.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：HairComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_render.h"
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

/* ---- HairComponent ---- */
extern "C" int dse_hair_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_hair_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" void dse_hair_set_hair_asset_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->hair_asset_path = v ? v : "";
    }
}
extern "C" int dse_hair_get_hair_asset_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::HairComponent>(e);
    if (!c || c->hair_asset_path.empty()) return 0;
    std::strncpy(buf, c->hair_asset_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" float dse_hair_get_damping(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return c ? c->damping : 0.0f;
}
extern "C" void dse_hair_set_damping(uint32_t e, float v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->damping = v;
    }
}
extern "C" float dse_hair_get_stiffness_local(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return c ? c->stiffness_local : 0.0f;
}
extern "C" void dse_hair_set_stiffness_local(uint32_t e, float v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->stiffness_local = v;
    }
}
extern "C" float dse_hair_get_stiffness_global(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return c ? c->stiffness_global : 0.0f;
}
extern "C" void dse_hair_set_stiffness_global(uint32_t e, float v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->stiffness_global = v;
    }
}
extern "C" float dse_hair_get_gravity(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return c ? c->gravity : 0.0f;
}
extern "C" void dse_hair_set_gravity(uint32_t e, float v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->gravity = v;
    }
}
extern "C" void dse_hair_get_wind(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::HairComponent>(e)) { *x = c->wind.x; *y = c->wind.y; *z = c->wind.z; }
}
extern "C" void dse_hair_set_wind(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->wind = glm::vec3(x, y, z);
    }
}
extern "C" float dse_hair_get_wind_turbulence(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return c ? c->wind_turbulence : 0.0f;
}
extern "C" void dse_hair_set_wind_turbulence(uint32_t e, float v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->wind_turbulence = v;
    }
}
extern "C" void dse_hair_get_root_color(uint32_t e, float* x, float* y, float* z, float* w) {
    if (const auto* c = GCC<dse::HairComponent>(e)) { *x = c->root_color.x; *y = c->root_color.y; *z = c->root_color.z; *w = c->root_color.w; }
}
extern "C" void dse_hair_set_root_color(uint32_t e, float x, float y, float z, float w) {
    if (auto* c = GC<dse::HairComponent>(e)) c->root_color = glm::vec4(x, y, z, w);
}
extern "C" void dse_hair_get_tip_color(uint32_t e, float* x, float* y, float* z, float* w) {
    if (const auto* c = GCC<dse::HairComponent>(e)) { *x = c->tip_color.x; *y = c->tip_color.y; *z = c->tip_color.z; *w = c->tip_color.w; }
}
extern "C" void dse_hair_set_tip_color(uint32_t e, float x, float y, float z, float w) {
    if (auto* c = GC<dse::HairComponent>(e)) c->tip_color = glm::vec4(x, y, z, w);
}
extern "C" float dse_hair_get_fiber_radius(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return c ? c->fiber_radius : 0.0f;
}
extern "C" void dse_hair_set_fiber_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->fiber_radius = v;
    }
}
extern "C" float dse_hair_get_opacity(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return c ? c->opacity : 0.0f;
}
extern "C" void dse_hair_set_opacity(uint32_t e, float v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->opacity = v;
    }
}
extern "C" int dse_hair_get_cast_shadow(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_hair_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->cast_shadow = (v != 0);
    }
}
extern "C" int dse_hair_get_receive_shadow(uint32_t e) {
    const auto* c = GCC<dse::HairComponent>(e);
    return (c && c->receive_shadow) ? 1 : 0;
}
extern "C" void dse_hair_set_receive_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::HairComponent>(e)) {
        c->receive_shadow = (v != 0);
    }
}
