/**
 * @file dse_api.cpp
 * @brief DSEngine Native C ABI — 手写实现（非 Codegen 部分）
 *
 * 组件字段 get/set 由 dse_api.gen.cpp 生成。
 * 本文件保留：Context / Entity / 组件 add 辅助 / Tree 字符串路径 / Input / Assets / App / Metrics。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/assets/asset_manager.h"
#include "engine/input/input.h"
#include "engine/base/time.h"

#include <glm/glm.hpp>
#include <cstring>

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

extern "C" void dse_mesh_renderer_set_mesh(uint32_t e, const char* mesh_path) {
    if (!mesh_path) return;
    if (auto* m = GetComp<dse::MeshRendererComponent>(e)) m->mesh_path = mesh_path;
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
// TreeComponent — string paths (Codegen 尚无 string 类型)
// ============================================================

namespace {

int CopyTreeString(uint32_t e, char* buf, int buf_size,
                   std::string dse::TreeComponent::* member) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    if (!c) return 0;
    const std::string& value = c->*member;
    if (value.empty()) return 0;
    std::strncpy(buf, value.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}

void SetTreeString(uint32_t e, const char* path,
                   std::string dse::TreeComponent::* member) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) {
        c->*member = path ? path : "";
    }
}

} // namespace

extern "C" void dse_tree_set_mesh_path(uint32_t e, const char* path) {
    SetTreeString(e, path, &dse::TreeComponent::mesh_path);
}
extern "C" int dse_tree_get_mesh_path(uint32_t e, char* buf, int buf_size) {
    return CopyTreeString(e, buf, buf_size, &dse::TreeComponent::mesh_path);
}
extern "C" void dse_tree_set_lod1_mesh_path(uint32_t e, const char* path) {
    SetTreeString(e, path, &dse::TreeComponent::lod1_mesh_path);
}
extern "C" int dse_tree_get_lod1_mesh_path(uint32_t e, char* buf, int buf_size) {
    return CopyTreeString(e, buf, buf_size, &dse::TreeComponent::lod1_mesh_path);
}
extern "C" void dse_tree_set_billboard_texture_path(uint32_t e, const char* path) {
    SetTreeString(e, path, &dse::TreeComponent::billboard_texture_path);
}
extern "C" int dse_tree_get_billboard_texture_path(uint32_t e, char* buf, int buf_size) {
    return CopyTreeString(e, buf, buf_size, &dse::TreeComponent::billboard_texture_path);
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
