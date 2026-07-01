/**
 * @file dse_api_impostor.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：ImpostorComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_impostor.h"
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

/* ---- ImpostorComponent ---- */
extern "C" int dse_impostor_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_impostor_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" void dse_impostor_set_atlas_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->atlas_path = v ? v : "";
    }
}
extern "C" int dse_impostor_get_atlas_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::ImpostorComponent>(e);
    if (!c || c->atlas_path.empty()) return 0;
    std::strncpy(buf, c->atlas_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" int dse_impostor_get_frames_x(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return c ? static_cast<int>(c->frames_x) : 0;
}
extern "C" void dse_impostor_set_frames_x(uint32_t e, int v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->frames_x = v;
    }
}
extern "C" int dse_impostor_get_frames_y(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return c ? static_cast<int>(c->frames_y) : 0;
}
extern "C" void dse_impostor_set_frames_y(uint32_t e, int v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->frames_y = v;
    }
}
extern "C" float dse_impostor_get_transition_distance(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return c ? c->transition_distance : 0.0f;
}
extern "C" void dse_impostor_set_transition_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->transition_distance = v;
    }
}
extern "C" float dse_impostor_get_fade_range(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return c ? c->fade_range : 0.0f;
}
extern "C" void dse_impostor_set_fade_range(uint32_t e, float v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->fade_range = v;
    }
}
extern "C" float dse_impostor_get_cull_distance(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return c ? c->cull_distance : 0.0f;
}
extern "C" void dse_impostor_set_cull_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->cull_distance = v;
    }
}
extern "C" float dse_impostor_get_impostor_size(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return c ? c->impostor_size : 0.0f;
}
extern "C" void dse_impostor_set_impostor_size(uint32_t e, float v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->impostor_size = v;
    }
}
extern "C" void dse_impostor_get_pivot_offset(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::ImpostorComponent>(e)) { *x = c->pivot_offset.x; *y = c->pivot_offset.y; *z = c->pivot_offset.z; }
}
extern "C" void dse_impostor_set_pivot_offset(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->pivot_offset = glm::vec3(x, y, z);
    }
}
extern "C" int dse_impostor_get_cast_shadow(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_impostor_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->cast_shadow = (v != 0);
    }
}
extern "C" int dse_impostor_get_use_frame_interpolation(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return (c && c->use_frame_interpolation) ? 1 : 0;
}
extern "C" void dse_impostor_set_use_frame_interpolation(uint32_t e, int v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->use_frame_interpolation = (v != 0);
    }
}
extern "C" float dse_impostor_get_normal_strength(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return c ? c->normal_strength : 0.0f;
}
extern "C" void dse_impostor_set_normal_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->normal_strength = v;
    }
}
extern "C" int dse_impostor_get_auto_from_lod_group(uint32_t e) {
    const auto* c = GCC<dse::ImpostorComponent>(e);
    return (c && c->auto_from_lod_group) ? 1 : 0;
}
extern "C" void dse_impostor_set_auto_from_lod_group(uint32_t e, int v) {
    if (auto* c = GC<dse::ImpostorComponent>(e)) {
        c->auto_from_lod_group = (v != 0);
    }
}
