/**
 * @file dse_api_sub_scene.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：SubSceneComponent）
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

/* ---- SubSceneComponent ---- */
extern "C" int dse_sub_scene_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::SubSceneComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_sub_scene_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::SubSceneComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" void dse_sub_scene_set_scene_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::SubSceneComponent>(e)) {
        c->scene_path = v ? v : "";
    }
}
extern "C" int dse_sub_scene_get_scene_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::SubSceneComponent>(e);
    if (!c || c->scene_path.empty()) return 0;
    std::strncpy(buf, c->scene_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
