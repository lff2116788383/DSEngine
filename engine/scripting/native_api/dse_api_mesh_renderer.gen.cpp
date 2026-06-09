/**
 * @file dse_api_mesh_renderer.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：MeshRendererComponent）
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

/* ---- MeshRendererComponent ---- */
extern "C" void dse_mesh_renderer_get_color(uint32_t e, float* x, float* y, float* z, float* w) {
    if (const auto* c = GCC<dse::MeshRendererComponent>(e)) { *x = c->color.x; *y = c->color.y; *z = c->color.z; *w = c->color.w; }
}
extern "C" void dse_mesh_renderer_set_color(uint32_t e, float x, float y, float z, float w) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) c->color = glm::vec4(x, y, z, w);
}
extern "C" int dse_mesh_renderer_get_visible(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return (c && c->visible) ? 1 : 0;
}
extern "C" void dse_mesh_renderer_set_visible(uint32_t e, int v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->visible = (v != 0);
    }
}
extern "C" float dse_mesh_renderer_get_metallic(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return c ? c->metallic : 0.0f;
}
extern "C" void dse_mesh_renderer_set_metallic(uint32_t e, float v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->metallic = v;
    }
}
extern "C" float dse_mesh_renderer_get_roughness(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return c ? c->roughness : 0.5f;
}
extern "C" void dse_mesh_renderer_set_roughness(uint32_t e, float v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->roughness = v;
    }
}
extern "C" void dse_mesh_renderer_get_emissive(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::MeshRendererComponent>(e)) { *x = c->emissive.x; *y = c->emissive.y; *z = c->emissive.z; }
}
extern "C" void dse_mesh_renderer_set_emissive(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->emissive = glm::vec3(x, y, z);
    }
}
extern "C" int dse_mesh_renderer_get_receive_shadow(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return (c && c->receive_shadow) ? 1 : 0;
}
extern "C" void dse_mesh_renderer_set_receive_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->receive_shadow = (v != 0);
    }
}
extern "C" int dse_mesh_renderer_get_mesh_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    if (!c || c->mesh_path.empty()) return 0;
    std::strncpy(buf, c->mesh_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" void dse_mesh_renderer_set_shader_variant(uint32_t e, const char* v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->shader_variant = v ? v : "";
    }
}
extern "C" int dse_mesh_renderer_get_shader_variant(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    if (!c || c->shader_variant.empty()) return 0;
    std::strncpy(buf, c->shader_variant.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
