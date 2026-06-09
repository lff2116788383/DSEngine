/**
 * @file dse_api_animator3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：Animator3DComponent）
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

/* ---- Animator3DComponent ---- */
extern "C" int dse_animator3d_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::Animator3DComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_animator3d_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::Animator3DComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" void dse_animator3d_set_danim_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::Animator3DComponent>(e)) {
        c->danim_path = v ? v : "";
    }
}
extern "C" int dse_animator3d_get_danim_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::Animator3DComponent>(e);
    if (!c || c->danim_path.empty()) return 0;
    std::strncpy(buf, c->danim_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" void dse_animator3d_set_dskel_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::Animator3DComponent>(e)) {
        c->dskel_path = v ? v : "";
    }
}
extern "C" int dse_animator3d_get_dskel_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::Animator3DComponent>(e);
    if (!c || c->dskel_path.empty()) return 0;
    std::strncpy(buf, c->dskel_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" float dse_animator3d_get_speed(uint32_t e) {
    const auto* c = GCC<dse::Animator3DComponent>(e);
    return c ? c->speed : 1.0f;
}
extern "C" void dse_animator3d_set_speed(uint32_t e, float v) {
    if (auto* c = GC<dse::Animator3DComponent>(e)) {
        c->speed = v;
    }
}
extern "C" int dse_animator3d_get_loop(uint32_t e) {
    const auto* c = GCC<dse::Animator3DComponent>(e);
    return (c && c->loop) ? 1 : 0;
}
extern "C" void dse_animator3d_set_loop(uint32_t e, int v) {
    if (auto* c = GC<dse::Animator3DComponent>(e)) {
        c->loop = (v != 0);
    }
}
extern "C" int dse_animator3d_get_use_anim_tree(uint32_t e) {
    const auto* c = GCC<dse::Animator3DComponent>(e);
    return (c && c->use_anim_tree) ? 1 : 0;
}
extern "C" void dse_animator3d_set_use_anim_tree(uint32_t e, int v) {
    if (auto* c = GC<dse::Animator3DComponent>(e)) {
        c->use_anim_tree = (v != 0);
    }
}
extern "C" void dse_animator3d_set_blend_parameter(uint32_t e, const char* v) {
    if (auto* c = GC<dse::Animator3DComponent>(e)) {
        c->blend_parameter = v ? v : "";
    }
}
extern "C" int dse_animator3d_get_blend_parameter(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::Animator3DComponent>(e);
    if (!c || c->blend_parameter.empty()) return 0;
    std::strncpy(buf, c->blend_parameter.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" float dse_animator3d_get_blend_parameter_value(uint32_t e) {
    const auto* c = GCC<dse::Animator3DComponent>(e);
    return c ? c->blend_parameter_value : 0.0f;
}
extern "C" void dse_animator3d_set_blend_parameter_value(uint32_t e, float v) {
    if (auto* c = GC<dse::Animator3DComponent>(e)) {
        c->blend_parameter_value = v;
    }
}
