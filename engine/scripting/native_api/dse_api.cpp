/**
 * @file dse_api.cpp
 * @brief DSEngine Native C ABI — 实现
 *
 * 通过 dse_native_api_init() 注入 World / AssetManager 等指针后，
 * 所有函数均可安全调用。各函数内置空指针保护，不会崩溃引擎。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/input/input.h"
#include "engine/base/time.h"
#include "engine/base/debug.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstring>

// ============================================================
// 内部 Context
// ============================================================

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
// TransformComponent
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

extern "C" void dse_transform_get_position(uint32_t e, float* x, float* y, float* z) {
    if (const auto* t = GetCompConst<TransformComponent>(e)) {
        *x = t->position.x; *y = t->position.y; *z = t->position.z;
    }
}

extern "C" void dse_transform_set_position(uint32_t e, float x, float y, float z) {
    if (auto* t = GetComp<TransformComponent>(e)) {
        t->position = glm::vec3(x, y, z);
        t->dirty = true;
    }
}

extern "C" void dse_transform_get_rotation(uint32_t e, float* x, float* y, float* z) {
    if (const auto* t = GetCompConst<TransformComponent>(e)) {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(t->rotation));
        *x = euler.x; *y = euler.y; *z = euler.z;
    }
}

extern "C" void dse_transform_set_rotation(uint32_t e, float x, float y, float z) {
    if (auto* t = GetComp<TransformComponent>(e)) {
        t->rotation = glm::quat(glm::vec3(glm::radians(x), glm::radians(y), glm::radians(z)));
        t->dirty = true;
    }
}

extern "C" void dse_transform_get_scale(uint32_t e, float* x, float* y, float* z) {
    if (const auto* t = GetCompConst<TransformComponent>(e)) {
        *x = t->scale.x; *y = t->scale.y; *z = t->scale.z;
    }
}

extern "C" void dse_transform_set_scale(uint32_t e, float x, float y, float z) {
    if (auto* t = GetComp<TransformComponent>(e)) {
        t->scale = glm::vec3(x, y, z);
        t->dirty = true;
    }
}

// ============================================================
// Camera3DComponent
// ============================================================

extern "C" void dse_camera3d_add(uint32_t e, float fov, float near_clip, float far_clip) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    auto& c = w->registry().emplace_or_replace<dse::Camera3DComponent>(ToEntity(e));
    c.fov       = fov;
    c.near_clip = near_clip;
    c.far_clip  = far_clip;
}

extern "C" float dse_camera3d_get_fov(uint32_t e) {
    const auto* c = GetCompConst<dse::Camera3DComponent>(e);
    return c ? c->fov : 60.0f;
}

extern "C" void dse_camera3d_set_fov(uint32_t e, float v) {
    if (auto* c = GetComp<dse::Camera3DComponent>(e)) c->fov = v;
}

extern "C" float dse_camera3d_get_near_clip(uint32_t e) {
    const auto* c = GetCompConst<dse::Camera3DComponent>(e);
    return c ? c->near_clip : 0.1f;
}

extern "C" void dse_camera3d_set_near_clip(uint32_t e, float v) {
    if (auto* c = GetComp<dse::Camera3DComponent>(e)) c->near_clip = v;
}

extern "C" float dse_camera3d_get_far_clip(uint32_t e) {
    const auto* c = GetCompConst<dse::Camera3DComponent>(e);
    return c ? c->far_clip : 1000.0f;
}

extern "C" void dse_camera3d_set_far_clip(uint32_t e, float v) {
    if (auto* c = GetComp<dse::Camera3DComponent>(e)) c->far_clip = v;
}

extern "C" int dse_camera3d_get_enabled(uint32_t e) {
    const auto* c = GetCompConst<dse::Camera3DComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}

extern "C" void dse_camera3d_set_enabled(uint32_t e, int v) {
    if (auto* c = GetComp<dse::Camera3DComponent>(e)) c->enabled = (v != 0);
}

extern "C" int dse_camera3d_get_priority(uint32_t e) {
    const auto* c = GetCompConst<dse::Camera3DComponent>(e);
    return c ? c->priority : 0;
}

extern "C" void dse_camera3d_set_priority(uint32_t e, int v) {
    if (auto* c = GetComp<dse::Camera3DComponent>(e)) c->priority = v;
}

// ============================================================
// MeshRendererComponent
// ============================================================

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

extern "C" void dse_mesh_renderer_get_color(uint32_t e, float* r, float* g, float* b, float* a) {
    if (const auto* m = GetCompConst<dse::MeshRendererComponent>(e)) {
        *r = m->color.r; *g = m->color.g; *b = m->color.b; *a = m->color.a;
    }
}

extern "C" void dse_mesh_renderer_set_color(uint32_t e, float r, float g, float b, float a) {
    if (auto* m = GetComp<dse::MeshRendererComponent>(e))
        m->color = glm::vec4(r, g, b, a);
}

extern "C" int dse_mesh_renderer_get_visible(uint32_t e) {
    const auto* m = GetCompConst<dse::MeshRendererComponent>(e);
    return (m && m->visible) ? 1 : 0;
}

extern "C" void dse_mesh_renderer_set_visible(uint32_t e, int v) {
    if (auto* m = GetComp<dse::MeshRendererComponent>(e)) m->visible = (v != 0);
}

extern "C" float dse_mesh_renderer_get_metallic(uint32_t e) {
    const auto* m = GetCompConst<dse::MeshRendererComponent>(e);
    return m ? m->metallic : 0.0f;
}

extern "C" void dse_mesh_renderer_set_metallic(uint32_t e, float v) {
    if (auto* m = GetComp<dse::MeshRendererComponent>(e)) m->metallic = v;
}

extern "C" float dse_mesh_renderer_get_roughness(uint32_t e) {
    const auto* m = GetCompConst<dse::MeshRendererComponent>(e);
    return m ? m->roughness : 0.5f;
}

extern "C" void dse_mesh_renderer_set_roughness(uint32_t e, float v) {
    if (auto* m = GetComp<dse::MeshRendererComponent>(e)) m->roughness = v;
}

extern "C" void dse_mesh_renderer_get_emissive(uint32_t e, float* r, float* g, float* b) {
    if (const auto* m = GetCompConst<dse::MeshRendererComponent>(e)) {
        *r = m->emissive.r; *g = m->emissive.g; *b = m->emissive.b;
    }
}

extern "C" void dse_mesh_renderer_set_emissive(uint32_t e, float r, float g, float b) {
    if (auto* m = GetComp<dse::MeshRendererComponent>(e))
        m->emissive = glm::vec3(r, g, b);
}

extern "C" int dse_mesh_renderer_get_receive_shadow(uint32_t e) {
    auto* m = GetComp<dse::MeshRendererComponent>(e);
    return m ? (int)m->receive_shadow : 1;
}
extern "C" void dse_mesh_renderer_set_receive_shadow(uint32_t e, int v) {
    if (auto* m = GetComp<dse::MeshRendererComponent>(e))
        m->receive_shadow = (v != 0);
}

// ============================================================
// DirectionalLight3DComponent
// ============================================================

extern "C" void dse_dir_light_add(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->registry().emplace_or_replace<dse::DirectionalLight3DComponent>(ToEntity(e));
}

extern "C" void dse_dir_light_get_direction(uint32_t e, float* x, float* y, float* z) {
    if (const auto* l = GetCompConst<dse::DirectionalLight3DComponent>(e)) {
        *x = l->direction.x; *y = l->direction.y; *z = l->direction.z;
    }
}

extern "C" void dse_dir_light_set_direction(uint32_t e, float x, float y, float z) {
    if (auto* l = GetComp<dse::DirectionalLight3DComponent>(e))
        l->direction = glm::vec3(x, y, z);
}

extern "C" void dse_dir_light_get_color(uint32_t e, float* r, float* g, float* b) {
    if (const auto* l = GetCompConst<dse::DirectionalLight3DComponent>(e)) {
        *r = l->color.r; *g = l->color.g; *b = l->color.b;
    }
}

extern "C" void dse_dir_light_set_color(uint32_t e, float r, float g, float b) {
    if (auto* l = GetComp<dse::DirectionalLight3DComponent>(e))
        l->color = glm::vec3(r, g, b);
}

extern "C" float dse_dir_light_get_intensity(uint32_t e) {
    const auto* l = GetCompConst<dse::DirectionalLight3DComponent>(e);
    return l ? l->intensity : 1.0f;
}

extern "C" void dse_dir_light_set_intensity(uint32_t e, float v) {
    if (auto* l = GetComp<dse::DirectionalLight3DComponent>(e)) l->intensity = v;
}

extern "C" float dse_dir_light_get_ambient_intensity(uint32_t e) {
    const auto* l = GetCompConst<dse::DirectionalLight3DComponent>(e);
    return l ? l->ambient_intensity : 0.2f;
}

extern "C" void dse_dir_light_set_ambient_intensity(uint32_t e, float v) {
    if (auto* l = GetComp<dse::DirectionalLight3DComponent>(e)) l->ambient_intensity = v;
}

extern "C" int dse_dir_light_get_cast_shadow(uint32_t e) {
    const auto* l = GetCompConst<dse::DirectionalLight3DComponent>(e);
    return (l && l->cast_shadow) ? 1 : 0;
}

extern "C" void dse_dir_light_set_cast_shadow(uint32_t e, int v) {
    if (auto* l = GetComp<dse::DirectionalLight3DComponent>(e)) l->cast_shadow = (v != 0);
}

extern "C" float dse_dir_light_get_shadow_strength(uint32_t e) {
    const auto* l = GetCompConst<dse::DirectionalLight3DComponent>(e);
    return l ? l->shadow_strength : 0.35f;
}

extern "C" void dse_dir_light_set_shadow_strength(uint32_t e, float v) {
    if (auto* l = GetComp<dse::DirectionalLight3DComponent>(e)) l->shadow_strength = v;
}

// ============================================================
// PointLightComponent
// ============================================================

extern "C" void dse_point_light_add(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->registry().emplace_or_replace<dse::PointLightComponent>(ToEntity(e));
}

extern "C" void dse_point_light_get_color(uint32_t e, float* r, float* g, float* b) {
    if (const auto* l = GetCompConst<dse::PointLightComponent>(e)) {
        *r = l->color.r; *g = l->color.g; *b = l->color.b;
    }
}

extern "C" void dse_point_light_set_color(uint32_t e, float r, float g, float b) {
    if (auto* l = GetComp<dse::PointLightComponent>(e)) l->color = glm::vec3(r, g, b);
}

extern "C" float dse_point_light_get_intensity(uint32_t e) {
    const auto* l = GetCompConst<dse::PointLightComponent>(e);
    return l ? l->intensity : 1.0f;
}

extern "C" void dse_point_light_set_intensity(uint32_t e, float v) {
    if (auto* l = GetComp<dse::PointLightComponent>(e)) l->intensity = v;
}

extern "C" float dse_point_light_get_radius(uint32_t e) {
    const auto* l = GetCompConst<dse::PointLightComponent>(e);
    return l ? l->radius : 10.0f;
}

extern "C" void dse_point_light_set_radius(uint32_t e, float v) {
    if (auto* l = GetComp<dse::PointLightComponent>(e)) l->radius = v;
}

extern "C" int dse_point_light_get_enabled(uint32_t e) {
    const auto* l = GetCompConst<dse::PointLightComponent>(e);
    return (l && l->enabled) ? 1 : 0;
}

extern "C" void dse_point_light_set_enabled(uint32_t e, int v) {
    if (auto* l = GetComp<dse::PointLightComponent>(e)) l->enabled = (v != 0);
}

// ============================================================
// SpotLightComponent
// ============================================================

extern "C" void dse_spot_light_add(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->registry().emplace_or_replace<dse::SpotLightComponent>(ToEntity(e));
}

extern "C" void dse_spot_light_get_color(uint32_t e, float* r, float* g, float* b) {
    if (const auto* l = GetCompConst<dse::SpotLightComponent>(e)) {
        *r = l->color.r; *g = l->color.g; *b = l->color.b;
    }
}

extern "C" void dse_spot_light_set_color(uint32_t e, float r, float g, float b) {
    if (auto* l = GetComp<dse::SpotLightComponent>(e)) l->color = glm::vec3(r, g, b);
}

extern "C" float dse_spot_light_get_intensity(uint32_t e) {
    const auto* l = GetCompConst<dse::SpotLightComponent>(e);
    return l ? l->intensity : 1.0f;
}

extern "C" void dse_spot_light_set_intensity(uint32_t e, float v) {
    if (auto* l = GetComp<dse::SpotLightComponent>(e)) l->intensity = v;
}

extern "C" float dse_spot_light_get_radius(uint32_t e) {
    const auto* l = GetCompConst<dse::SpotLightComponent>(e);
    return l ? l->radius : 20.0f;
}

extern "C" void dse_spot_light_set_radius(uint32_t e, float v) {
    if (auto* l = GetComp<dse::SpotLightComponent>(e)) l->radius = v;
}

extern "C" void dse_spot_light_get_direction(uint32_t e, float* x, float* y, float* z) {
    if (const auto* l = GetCompConst<dse::SpotLightComponent>(e)) {
        *x = l->direction.x; *y = l->direction.y; *z = l->direction.z;
    }
}

extern "C" void dse_spot_light_set_direction(uint32_t e, float x, float y, float z) {
    if (auto* l = GetComp<dse::SpotLightComponent>(e)) l->direction = glm::vec3(x, y, z);
}

extern "C" float dse_spot_light_get_inner_cone_angle(uint32_t e) {
    const auto* l = GetCompConst<dse::SpotLightComponent>(e);
    return l ? l->inner_cone_angle : 12.5f;
}

extern "C" void dse_spot_light_set_inner_cone_angle(uint32_t e, float v) {
    if (auto* l = GetComp<dse::SpotLightComponent>(e)) l->inner_cone_angle = v;
}

extern "C" float dse_spot_light_get_outer_cone_angle(uint32_t e) {
    const auto* l = GetCompConst<dse::SpotLightComponent>(e);
    return l ? l->outer_cone_angle : 17.5f;
}

extern "C" void dse_spot_light_set_outer_cone_angle(uint32_t e, float v) {
    if (auto* l = GetComp<dse::SpotLightComponent>(e)) l->outer_cone_angle = v;
}

extern "C" int dse_spot_light_get_enabled(uint32_t e) {
    const auto* l = GetCompConst<dse::SpotLightComponent>(e);
    return (l && l->enabled) ? 1 : 0;
}

extern "C" void dse_spot_light_set_enabled(uint32_t e, int v) {
    if (auto* l = GetComp<dse::SpotLightComponent>(e)) l->enabled = (v != 0);
}

extern "C" int dse_spot_light_get_cast_shadow(uint32_t e) {
    const auto* l = GetCompConst<dse::SpotLightComponent>(e);
    return (l && l->cast_shadow) ? 1 : 0;
}

extern "C" void dse_spot_light_set_cast_shadow(uint32_t e, int v) {
    if (auto* l = GetComp<dse::SpotLightComponent>(e)) l->cast_shadow = (v != 0);
}

// ============================================================
// SkyLightComponent
// ============================================================

extern "C" void dse_sky_light_add(uint32_t e) {
    World* w = GetWorld();
    if (!ValidEntity(w, e)) return;
    w->registry().emplace_or_replace<dse::SkyLightComponent>(ToEntity(e));
}

extern "C" void dse_sky_light_get_up_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* l = GetCompConst<dse::SkyLightComponent>(e)) {
        *x = l->up_color.r; *y = l->up_color.g; *z = l->up_color.b;
    }
}

extern "C" void dse_sky_light_set_up_color(uint32_t e, float x, float y, float z) {
    if (auto* l = GetComp<dse::SkyLightComponent>(e)) l->up_color = glm::vec3(x, y, z);
}

extern "C" void dse_sky_light_get_down_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* l = GetCompConst<dse::SkyLightComponent>(e)) {
        *x = l->down_color.r; *y = l->down_color.g; *z = l->down_color.b;
    }
}

extern "C" void dse_sky_light_set_down_color(uint32_t e, float x, float y, float z) {
    if (auto* l = GetComp<dse::SkyLightComponent>(e)) l->down_color = glm::vec3(x, y, z);
}

extern "C" float dse_sky_light_get_intensity(uint32_t e) {
    const auto* l = GetCompConst<dse::SkyLightComponent>(e);
    return l ? l->intensity : 1.0f;
}

extern "C" void dse_sky_light_set_intensity(uint32_t e, float v) {
    if (auto* l = GetComp<dse::SkyLightComponent>(e)) l->intensity = v;
}

extern "C" int dse_sky_light_get_enabled(uint32_t e) {
    const auto* l = GetCompConst<dse::SkyLightComponent>(e);
    return (l && l->enabled) ? 1 : 0;
}

extern "C" void dse_sky_light_set_enabled(uint32_t e, int v) {
    if (auto* l = GetComp<dse::SkyLightComponent>(e)) l->enabled = (v != 0);
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

// ============================================================
// TreeComponent
// ============================================================

extern "C" int dse_tree_get_enabled(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_tree_set_enabled(uint32_t e, int v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->enabled = (v != 0);
}
extern "C" float dse_tree_get_density(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->density : 0.02f;
}
extern "C" void dse_tree_set_density(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->density = v;
}
extern "C" float dse_tree_get_spawn_radius(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->spawn_radius : 120.0f;
}
extern "C" void dse_tree_set_spawn_radius(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->spawn_radius = v;
}
extern "C" float dse_tree_get_chunk_size(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->chunk_size : 32.0f;
}
extern "C" void dse_tree_set_chunk_size(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->chunk_size = v;
}
extern "C" float dse_tree_get_min_scale(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->min_scale : 0.8f;
}
extern "C" void dse_tree_set_min_scale(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->min_scale = v;
}
extern "C" float dse_tree_get_max_scale(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->max_scale : 1.3f;
}
extern "C" void dse_tree_set_max_scale(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->max_scale = v;
}
extern "C" float dse_tree_get_lod1_distance(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->lod1_distance : 60.0f;
}
extern "C" void dse_tree_set_lod1_distance(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->lod1_distance = v;
}
extern "C" float dse_tree_get_cull_distance(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->cull_distance : 200.0f;
}
extern "C" void dse_tree_set_cull_distance(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->cull_distance = v;
}
extern "C" float dse_tree_get_wind_strength(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->wind_strength : 0.3f;
}
extern "C" void dse_tree_set_wind_strength(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->wind_strength = v;
}
extern "C" float dse_tree_get_wind_speed(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->wind_speed : 1.0f;
}
extern "C" void dse_tree_set_wind_speed(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->wind_speed = v;
}
extern "C" int dse_tree_get_cast_shadow(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_tree_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->cast_shadow = (v != 0);
}
extern "C" float dse_tree_get_shadow_distance(uint32_t e) {
    const auto* c = GetCompConst<dse::TreeComponent>(e);
    return c ? c->shadow_distance : 80.0f;
}
extern "C" void dse_tree_set_shadow_distance(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TreeComponent>(e)) c->shadow_distance = v;
}

// ============================================================
// TerrainTileManagerComponent
// ============================================================

extern "C" int dse_terrain_tile_get_enabled(uint32_t e) {
    const auto* c = GetCompConst<dse::TerrainTileManagerComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_terrain_tile_set_enabled(uint32_t e, int v) {
    if (auto* c = GetComp<dse::TerrainTileManagerComponent>(e)) c->enabled = (v != 0);
}
extern "C" float dse_terrain_tile_get_tile_world_size(uint32_t e) {
    const auto* c = GetCompConst<dse::TerrainTileManagerComponent>(e);
    return c ? c->tile_world_size : 64.0f;
}
extern "C" void dse_terrain_tile_set_tile_world_size(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TerrainTileManagerComponent>(e)) c->tile_world_size = v;
}
extern "C" int dse_terrain_tile_get_tile_resolution(uint32_t e) {
    const auto* c = GetCompConst<dse::TerrainTileManagerComponent>(e);
    return c ? c->tile_resolution : 64;
}
extern "C" void dse_terrain_tile_set_tile_resolution(uint32_t e, int v) {
    if (auto* c = GetComp<dse::TerrainTileManagerComponent>(e)) c->tile_resolution = v;
}
extern "C" float dse_terrain_tile_get_max_height(uint32_t e) {
    const auto* c = GetCompConst<dse::TerrainTileManagerComponent>(e);
    return c ? c->max_height : 20.0f;
}
extern "C" void dse_terrain_tile_set_max_height(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TerrainTileManagerComponent>(e)) c->max_height = v;
}
extern "C" float dse_terrain_tile_get_load_radius(uint32_t e) {
    const auto* c = GetCompConst<dse::TerrainTileManagerComponent>(e);
    return c ? c->load_radius : 200.0f;
}
extern "C" void dse_terrain_tile_set_load_radius(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TerrainTileManagerComponent>(e)) c->load_radius = v;
}
extern "C" float dse_terrain_tile_get_unload_radius(uint32_t e) {
    const auto* c = GetCompConst<dse::TerrainTileManagerComponent>(e);
    return c ? c->unload_radius : 250.0f;
}
extern "C" void dse_terrain_tile_set_unload_radius(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TerrainTileManagerComponent>(e)) c->unload_radius = v;
}
extern "C" int dse_terrain_tile_get_use_procedural(uint32_t e) {
    const auto* c = GetCompConst<dse::TerrainTileManagerComponent>(e);
    return (c && c->use_procedural) ? 1 : 0;
}
extern "C" void dse_terrain_tile_set_use_procedural(uint32_t e, int v) {
    if (auto* c = GetComp<dse::TerrainTileManagerComponent>(e)) c->use_procedural = (v != 0);
}
extern "C" float dse_terrain_tile_get_procedural_base_height(uint32_t e) {
    const auto* c = GetCompConst<dse::TerrainTileManagerComponent>(e);
    return c ? c->procedural_base_height : 0.0f;
}
extern "C" void dse_terrain_tile_set_procedural_base_height(uint32_t e, float v) {
    if (auto* c = GetComp<dse::TerrainTileManagerComponent>(e)) c->procedural_base_height = v;
}

// ============================================================
// DynamicObstacleComponent
// ============================================================

extern "C" int dse_dyn_obstacle_get_enabled(uint32_t e) {
    const auto* c = GetCompConst<dse::DynamicObstacleComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_dyn_obstacle_set_enabled(uint32_t e, int v) {
    if (auto* c = GetComp<dse::DynamicObstacleComponent>(e)) c->enabled = (v != 0);
}
extern "C" void dse_dyn_obstacle_get_box_extents(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GetCompConst<dse::DynamicObstacleComponent>(e)) {
        *x = c->box_extents.x; *y = c->box_extents.y; *z = c->box_extents.z;
    }
}
extern "C" void dse_dyn_obstacle_set_box_extents(uint32_t e, float x, float y, float z) {
    if (auto* c = GetComp<dse::DynamicObstacleComponent>(e))
        c->box_extents = glm::vec3(x, y, z);
}
extern "C" float dse_dyn_obstacle_get_cylinder_radius(uint32_t e) {
    const auto* c = GetCompConst<dse::DynamicObstacleComponent>(e);
    return c ? c->cylinder_radius : 1.0f;
}
extern "C" void dse_dyn_obstacle_set_cylinder_radius(uint32_t e, float v) {
    if (auto* c = GetComp<dse::DynamicObstacleComponent>(e)) c->cylinder_radius = v;
}
extern "C" float dse_dyn_obstacle_get_cylinder_height(uint32_t e) {
    const auto* c = GetCompConst<dse::DynamicObstacleComponent>(e);
    return c ? c->cylinder_height : 2.0f;
}
extern "C" void dse_dyn_obstacle_set_cylinder_height(uint32_t e, float v) {
    if (auto* c = GetComp<dse::DynamicObstacleComponent>(e)) c->cylinder_height = v;
}
