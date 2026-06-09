/**
 * @file dse_api.cpp
 * @brief DSEngine Native C ABI — 手写实现（非 Codegen 部分）
 *
 * 组件字段 get/set 由 dse_api.gen.cpp 生成。
 * 本文件保留：Context / Entity / 组件 add 辅助 / Input / Assets / App / Metrics。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/input/input.h"
#include "engine/base/time.h"

#include <glm/glm.hpp>
#include <algorithm>

namespace {

struct NativeApiContext {
    World*        world         = nullptr;
    AssetManager* asset_manager = nullptr;
    void*         audio_system  = nullptr;
    void  (*quit_fn)(void)             = nullptr;
    void  (*set_title_fn)(const char*) = nullptr;
    float (*get_fps_fn)(void)          = nullptr;
    void  (*set_fps_fn)(float)         = nullptr;
    int   (*get_draw_calls_fn)(void)   = nullptr;
};

static NativeApiContext g_ctx;

inline World* GetWorld() { return g_ctx.world; }

inline Entity ToEntity(uint32_t id) {
    return static_cast<Entity>(static_cast<entt::id_type>(id));
}

inline bool ValidEntity(World* w, uint32_t id) {
    return w && w->registry().valid(ToEntity(id));
}

template <typename T>
inline T* GetComp(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return nullptr;
    return w->registry().try_get<T>(ToEntity(e));
}

template <typename T>
inline const T* GetCompConst(uint32_t e) { return GetComp<T>(e); }

} // namespace

// ============================================================
// Context Setup
// ============================================================

extern "C" void dse_native_api_init(
    void* world,
    void* asset_manager,
    void* audio_system,
    void  (*quit_fn)(void),
    void  (*set_title_fn)(const char*),
    float (*get_fps_fn)(void),
    void  (*set_fps_fn)(float),
    int   (*get_draw_calls_fn)(void))
{
    g_ctx.world             = static_cast<World*>(world);
    g_ctx.asset_manager     = static_cast<AssetManager*>(asset_manager);
    g_ctx.audio_system      = audio_system;
    g_ctx.quit_fn           = quit_fn;
    g_ctx.set_title_fn      = set_title_fn;
    g_ctx.get_fps_fn        = get_fps_fn;
    g_ctx.set_fps_fn        = set_fps_fn;
    g_ctx.get_draw_calls_fn = get_draw_calls_fn;
}

extern "C" void* dse_get_world_ptr(void) { return g_ctx.world; }

// ============================================================
// Entity
// ============================================================

extern "C" uint32_t dse_entity_create(void) {
    World* w = GetWorld();
    if (!w) return static_cast<uint32_t>(entt::null);
    Entity e = w->CreateEntity();
    return static_cast<uint32_t>(static_cast<entt::id_type>(e));
}

extern "C" void dse_entity_destroy(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->DestroyEntity(ToEntity(e));
}

extern "C" int dse_entity_valid(uint32_t e) {
    World* w = GetWorld();
    return (w && w->registry().valid(ToEntity(e))) ? 1 : 0;
}

// ============================================================
// Component add helpers (not in binding_defs)
// ============================================================

extern "C" void dse_transform_add(uint32_t e,
    float x, float y, float z,
    float sx, float sy, float sz)
{
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    auto& t = w->registry().emplace_or_replace<TransformComponent>(ToEntity(e));
    t.position = glm::vec3(x, y, z);
    t.scale    = glm::vec3(sx, sy, sz);
    t.dirty    = true;
}

extern "C" void dse_camera3d_add(uint32_t e, float fov, float near_clip, float far_clip) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    auto& c = w->registry().emplace_or_replace<dse::Camera3DComponent>(ToEntity(e));
    c.fov       = fov;
    c.near_clip = near_clip;
    c.far_clip  = far_clip;
}

extern "C" void dse_mesh_renderer_add(uint32_t e, const char* mesh_path) {
    World* w = GetWorld();
    if (!ValidEntity(w, e) || !mesh_path) return;
    auto& m = w->registry().emplace_or_replace<dse::MeshRendererComponent>(ToEntity(e));
    m.mesh_path = mesh_path;
}

extern "C" void dse_mesh_renderer_set_mesh_path(uint32_t e, const char* mesh_path) {
    if (!mesh_path) return;
    if (auto* m = GetComp<dse::MeshRendererComponent>(e)) {
        m->mesh_path = mesh_path;
        // 切换到文件网格时清空过程网格缓存，否则 MeshRenderSystem 见 temp_* 非空会跳过加载新 mesh_path
        m->temp_vertices.clear();
        m->temp_indices.clear();
        m->temp_uvs.clear();
        m->temp_normals.clear();
        m->temp_tangents.clear();
    }
}

extern "C" void dse_dir_light_add(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->registry().emplace_or_replace<dse::DirectionalLight3DComponent>(ToEntity(e));
}

extern "C" void dse_point_light_add(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->registry().emplace_or_replace<dse::PointLightComponent>(ToEntity(e));
}

extern "C" void dse_spot_light_add(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->registry().emplace_or_replace<dse::SpotLightComponent>(ToEntity(e));
}

extern "C" void dse_sky_light_add(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->registry().emplace_or_replace<dse::SkyLightComponent>(ToEntity(e));
}

// ============================================================
// S1.8 Tier C：DirectionalLight 复合阴影参数
// 封装 cascade 级联约束（split[i] ≥ split[i-1]+0.1）+ shadow_strength/lambda 的 clamp。
// 供 Lua set_directional_light_shadow 薄包装委托；调用方传入已与现值合并的参数。
// 该钳制逻辑与原手写 Lua setter 完全一致。
// ============================================================
extern "C" void dse_dir_light_set_shadow_params(uint32_t e, int cast_shadow, float shadow_strength,
                                                float c0, float c1, float c2, float lambda) {
    auto* light = GetComp<dse::DirectionalLight3DComponent>(e);
    if (!light) return;
    light->cast_shadow = (cast_shadow != 0);
    light->shadow_strength = std::clamp(shadow_strength, 0.0f, 1.0f);
    light->cascade_splits[0] = std::max(0.1f, c0);
    light->cascade_splits[1] = std::max(light->cascade_splits[0] + 0.1f, c1);
    light->cascade_splits[2] = std::max(light->cascade_splits[1] + 0.1f, c2);
    light->cascade_split_lambda = std::clamp(lambda, 0.0f, 1.0f);
}

// ============================================================
// Input
// ============================================================

extern "C" int dse_input_get_key(int key_code) {
    return Input::GetKey(static_cast<unsigned short>(key_code)) ? 1 : 0;
}

extern "C" int dse_input_get_key_down(int key_code) {
    return Input::GetKeyDown(static_cast<unsigned short>(key_code)) ? 1 : 0;
}

extern "C" int dse_input_get_key_up(int key_code) {
    return Input::GetKeyUp(static_cast<unsigned short>(key_code)) ? 1 : 0;
}

extern "C" int dse_input_get_mouse_button(int button) {
    return Input::GetMouseButton(static_cast<unsigned short>(button)) ? 1 : 0;
}

extern "C" int dse_input_get_mouse_button_down(int button) {
    return Input::GetMouseButtonDown(static_cast<unsigned short>(button)) ? 1 : 0;
}

extern "C" int dse_input_get_mouse_button_up(int button) {
    return Input::GetMouseButtonUp(static_cast<unsigned short>(button)) ? 1 : 0;
}

extern "C" float dse_input_get_mouse_x(void) {
    return Input::mousePosition().x;
}

extern "C" float dse_input_get_mouse_y(void) {
    return Input::mousePosition().y;
}

extern "C" float dse_input_get_mouse_scroll(void) {
    return Input::mouseScroll();
}

extern "C" float dse_input_get_gamepad_axis(int gamepad_id, int axis) {
    return Input::GetGamepadAxis(gamepad_id, axis);
}

// ============================================================
// Assets
// ============================================================

extern "C" uint32_t dse_assets_load_texture(const char* path) {
    if (!g_ctx.asset_manager || !path) return 0;
    auto tex = g_ctx.asset_manager->LoadTexture(path);
    return tex ? tex->GetHandle() : 0;
}

extern "C" void dse_assets_set_data_root(const char* path) {
    if (!g_ctx.asset_manager || !path) return;
    g_ctx.asset_manager->ConfigureDataRoot(path);
}

// ============================================================
// App / System
// ============================================================

extern "C" void dse_app_quit(void) {
    if (g_ctx.quit_fn) g_ctx.quit_fn();
}

extern "C" void dse_app_set_window_title(const char* title) {
    if (g_ctx.set_title_fn && title) g_ctx.set_title_fn(title);
}

extern "C" float dse_app_get_time(void) {
    return Time::TimeSinceStartup();
}

extern "C" float dse_app_get_delta_time(void) {
    return Time::delta_time();
}

extern "C" void dse_app_set_target_fps(float fps) {
    if (g_ctx.set_fps_fn) g_ctx.set_fps_fn(fps);
}

extern "C" float dse_app_get_target_fps(void) {
    return g_ctx.get_fps_fn ? g_ctx.get_fps_fn() : 60.0f;
}

// ============================================================
// Metrics
// ============================================================

extern "C" int dse_metrics_get_draw_calls(void) {
    return g_ctx.get_draw_calls_fn ? g_ctx.get_draw_calls_fn() : 0;
}
