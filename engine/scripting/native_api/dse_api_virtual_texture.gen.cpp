/**
 * @file dse_api_virtual_texture.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：VirtualTextureComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/render/virtual_texture/virtual_texture.h"
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

/* ---- VirtualTextureComponent ---- */
extern "C" int dse_virtual_texture_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::vt::VirtualTextureComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_virtual_texture_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::vt::VirtualTextureComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" int dse_virtual_texture_get_vt_id(uint32_t e) {
    const auto* c = GCC<dse::vt::VirtualTextureComponent>(e);
    return c ? static_cast<int>(c->vt_id) : 0;
}
extern "C" void dse_virtual_texture_set_vt_id(uint32_t e, int v) {
    if (auto* c = GC<dse::vt::VirtualTextureComponent>(e)) {
        c->vt_id = v;
    }
}
extern "C" void dse_virtual_texture_set_tile_data_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::vt::VirtualTextureComponent>(e)) {
        c->tile_data_path = v ? v : "";
    }
}
extern "C" int dse_virtual_texture_get_tile_data_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::vt::VirtualTextureComponent>(e);
    if (!c || c->tile_data_path.empty()) return 0;
    std::strncpy(buf, c->tile_data_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" int dse_virtual_texture_get_virtual_width(uint32_t e) {
    const auto* c = GCC<dse::vt::VirtualTextureComponent>(e);
    return c ? static_cast<int>(c->virtual_width) : 0;
}
extern "C" void dse_virtual_texture_set_virtual_width(uint32_t e, int v) {
    if (auto* c = GC<dse::vt::VirtualTextureComponent>(e)) {
        c->virtual_width = v;
    }
}
extern "C" int dse_virtual_texture_get_virtual_height(uint32_t e) {
    const auto* c = GCC<dse::vt::VirtualTextureComponent>(e);
    return c ? static_cast<int>(c->virtual_height) : 0;
}
extern "C" void dse_virtual_texture_set_virtual_height(uint32_t e, int v) {
    if (auto* c = GC<dse::vt::VirtualTextureComponent>(e)) {
        c->virtual_height = v;
    }
}
extern "C" float dse_virtual_texture_get_mip_bias(uint32_t e) {
    const auto* c = GCC<dse::vt::VirtualTextureComponent>(e);
    return c ? c->mip_bias : 0.0f;
}
extern "C" void dse_virtual_texture_set_mip_bias(uint32_t e, float v) {
    if (auto* c = GC<dse::vt::VirtualTextureComponent>(e)) {
        c->mip_bias = v;
    }
}
