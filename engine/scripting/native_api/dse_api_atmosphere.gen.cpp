/**
 * @file dse_api_atmosphere.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：AtmosphereComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_sky.h"
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

/* ---- AtmosphereComponent ---- */
extern "C" int dse_atmosphere_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_atmosphere_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_atmosphere_get_planet_radius(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->planet_radius : 0.0f;
}
extern "C" void dse_atmosphere_set_planet_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->planet_radius = v;
    }
}
extern "C" float dse_atmosphere_get_atmosphere_height(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->atmosphere_height : 0.0f;
}
extern "C" void dse_atmosphere_set_atmosphere_height(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->atmosphere_height = v;
    }
}
extern "C" void dse_atmosphere_get_rayleigh_coeff(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::AtmosphereComponent>(e)) { *x = c->rayleigh_coeff.x; *y = c->rayleigh_coeff.y; *z = c->rayleigh_coeff.z; }
}
extern "C" void dse_atmosphere_set_rayleigh_coeff(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->rayleigh_coeff = glm::vec3(x, y, z);
    }
}
extern "C" float dse_atmosphere_get_rayleigh_scale_height(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->rayleigh_scale_height : 0.0f;
}
extern "C" void dse_atmosphere_set_rayleigh_scale_height(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->rayleigh_scale_height = v;
    }
}
extern "C" float dse_atmosphere_get_mie_coeff(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->mie_coeff : 0.0f;
}
extern "C" void dse_atmosphere_set_mie_coeff(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->mie_coeff = v;
    }
}
extern "C" float dse_atmosphere_get_mie_scale_height(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->mie_scale_height : 0.0f;
}
extern "C" void dse_atmosphere_set_mie_scale_height(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->mie_scale_height = v;
    }
}
extern "C" float dse_atmosphere_get_mie_g(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->mie_g : 0.0f;
}
extern "C" void dse_atmosphere_set_mie_g(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->mie_g = v;
    }
}
extern "C" void dse_atmosphere_get_mie_albedo(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::AtmosphereComponent>(e)) { *x = c->mie_albedo.x; *y = c->mie_albedo.y; *z = c->mie_albedo.z; }
}
extern "C" void dse_atmosphere_set_mie_albedo(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->mie_albedo = glm::vec3(x, y, z);
    }
}
extern "C" void dse_atmosphere_get_ozone_coeff(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::AtmosphereComponent>(e)) { *x = c->ozone_coeff.x; *y = c->ozone_coeff.y; *z = c->ozone_coeff.z; }
}
extern "C" void dse_atmosphere_set_ozone_coeff(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->ozone_coeff = glm::vec3(x, y, z);
    }
}
extern "C" float dse_atmosphere_get_ozone_center_h(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->ozone_center_h : 0.0f;
}
extern "C" void dse_atmosphere_set_ozone_center_h(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->ozone_center_h = v;
    }
}
extern "C" float dse_atmosphere_get_ozone_width(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->ozone_width : 0.0f;
}
extern "C" void dse_atmosphere_set_ozone_width(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->ozone_width = v;
    }
}
extern "C" float dse_atmosphere_get_sun_intensity(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->sun_intensity : 0.0f;
}
extern "C" void dse_atmosphere_set_sun_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->sun_intensity = v;
    }
}
extern "C" float dse_atmosphere_get_sun_disk_angle(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return c ? c->sun_disk_angle : 0.0f;
}
extern "C" void dse_atmosphere_set_sun_disk_angle(uint32_t e, float v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->sun_disk_angle = v;
    }
}
extern "C" int dse_atmosphere_get_aerial_perspective_enabled(uint32_t e) {
    const auto* c = GCC<dse::AtmosphereComponent>(e);
    return (c && c->aerial_perspective_enabled) ? 1 : 0;
}
extern "C" void dse_atmosphere_set_aerial_perspective_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::AtmosphereComponent>(e)) {
        c->aerial_perspective_enabled = (v != 0);
    }
}
