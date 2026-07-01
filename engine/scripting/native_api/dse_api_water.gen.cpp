/**
 * @file dse_api_water.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：WaterComponent）
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

/* ---- WaterComponent ---- */
extern "C" int dse_water_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_water_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_water_get_water_level(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->water_level : 0.0f;
}
extern "C" void dse_water_set_water_level(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->water_level = v;
    }
}
extern "C" void dse_water_get_deep_color(uint32_t e, float* x, float* y, float* z, float* w) {
    if (const auto* c = GCC<dse::WaterComponent>(e)) { *x = c->deep_color.x; *y = c->deep_color.y; *z = c->deep_color.z; *w = c->deep_color.w; }
}
extern "C" void dse_water_set_deep_color(uint32_t e, float x, float y, float z, float w) {
    if (auto* c = GC<dse::WaterComponent>(e)) c->deep_color = glm::vec4(x, y, z, w);
}
extern "C" void dse_water_get_shallow_color(uint32_t e, float* x, float* y, float* z, float* w) {
    if (const auto* c = GCC<dse::WaterComponent>(e)) { *x = c->shallow_color.x; *y = c->shallow_color.y; *z = c->shallow_color.z; *w = c->shallow_color.w; }
}
extern "C" void dse_water_set_shallow_color(uint32_t e, float x, float y, float z, float w) {
    if (auto* c = GC<dse::WaterComponent>(e)) c->shallow_color = glm::vec4(x, y, z, w);
}
extern "C" float dse_water_get_max_depth(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->max_depth : 0.0f;
}
extern "C" void dse_water_set_max_depth(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->max_depth = v;
    }
}
extern "C" float dse_water_get_transparency(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->transparency : 0.0f;
}
extern "C" void dse_water_set_transparency(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->transparency = v;
    }
}
extern "C" float dse_water_get_wave_amplitude(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->wave_amplitude : 0.0f;
}
extern "C" void dse_water_set_wave_amplitude(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->wave_amplitude = v;
    }
}
extern "C" float dse_water_get_wave_frequency(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->wave_frequency : 0.0f;
}
extern "C" void dse_water_set_wave_frequency(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->wave_frequency = v;
    }
}
extern "C" float dse_water_get_wave_speed(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->wave_speed : 0.0f;
}
extern "C" void dse_water_set_wave_speed(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->wave_speed = v;
    }
}
extern "C" void dse_water_get_wave_direction(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::WaterComponent>(e)) { *x = c->wave_direction.x; *y = c->wave_direction.y; *z = c->wave_direction.z; }
}
extern "C" void dse_water_set_wave_direction(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->wave_direction = glm::vec3(x, y, z);
    }
}
extern "C" float dse_water_get_refraction_strength(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->refraction_strength : 0.0f;
}
extern "C" void dse_water_set_refraction_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->refraction_strength = v;
    }
}
extern "C" float dse_water_get_reflection_strength(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->reflection_strength : 0.0f;
}
extern "C" void dse_water_set_reflection_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->reflection_strength = v;
    }
}
extern "C" float dse_water_get_specular_power(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->specular_power : 0.0f;
}
extern "C" void dse_water_set_specular_power(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->specular_power = v;
    }
}
extern "C" float dse_water_get_caustic_intensity(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->caustic_intensity : 0.0f;
}
extern "C" void dse_water_set_caustic_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->caustic_intensity = v;
    }
}
extern "C" float dse_water_get_caustic_scale(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->caustic_scale : 0.0f;
}
extern "C" void dse_water_set_caustic_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->caustic_scale = v;
    }
}
extern "C" float dse_water_get_foam_intensity(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->foam_intensity : 0.0f;
}
extern "C" void dse_water_set_foam_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->foam_intensity = v;
    }
}
extern "C" float dse_water_get_foam_depth_threshold(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->foam_depth_threshold : 0.0f;
}
extern "C" void dse_water_set_foam_depth_threshold(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->foam_depth_threshold = v;
    }
}
extern "C" float dse_water_get_underwater_fog_density(uint32_t e) {
    const auto* c = GCC<dse::WaterComponent>(e);
    return c ? c->underwater_fog_density : 0.0f;
}
extern "C" void dse_water_set_underwater_fog_density(uint32_t e, float v) {
    if (auto* c = GC<dse::WaterComponent>(e)) {
        c->underwater_fog_density = v;
    }
}
extern "C" void dse_water_get_underwater_fog_color(uint32_t e, float* x, float* y, float* z, float* w) {
    if (const auto* c = GCC<dse::WaterComponent>(e)) { *x = c->underwater_fog_color.x; *y = c->underwater_fog_color.y; *z = c->underwater_fog_color.z; *w = c->underwater_fog_color.w; }
}
extern "C" void dse_water_set_underwater_fog_color(uint32_t e, float x, float y, float z, float w) {
    if (auto* c = GC<dse::WaterComponent>(e)) c->underwater_fog_color = glm::vec4(x, y, z, w);
}
