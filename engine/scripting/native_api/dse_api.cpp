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
#include "engine/platform/screen.h"
#include "engine/ecs/floating_origin_system.h"

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
    // Extended context (via dse_native_api_init_ext)
    int   (*get_max_batch_sprites_fn)(void)      = nullptr;
    int   (*get_sprite_count_fn)(void)           = nullptr;
    int   (*get_gpu_driven_active_fn)(void)      = nullptr;
    int   (*get_gpu_indirect_draw_count_fn)(void) = nullptr;
    int   (*get_gpu_total_instances_fn)(void)    = nullptr;
    void* floating_origin = nullptr;
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
// API Version
// ============================================================

extern "C" DSE_CAPI uint32_t dse_api_version(void) {
    return DSE_API_VERSION;
}

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

extern "C" void* dse_get_asset_manager_ptr(void) { return g_ctx.asset_manager; }

extern "C" void* dse_get_audio_system_ptr(void) { return g_ctx.audio_system; }

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
    return tex ? tex->GetHandle().raw() : 0;
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
// Extended Context Setup
// ============================================================

extern "C" void dse_native_api_init_ext(
    int   (*get_max_batch_sprites_fn)(void),
    int   (*get_sprite_count_fn)(void),
    int   (*get_gpu_driven_active_fn)(void),
    int   (*get_gpu_indirect_draw_count_fn)(void),
    int   (*get_gpu_total_instances_fn)(void),
    void* floating_origin)
{
    g_ctx.get_max_batch_sprites_fn       = get_max_batch_sprites_fn;
    g_ctx.get_sprite_count_fn           = get_sprite_count_fn;
    g_ctx.get_gpu_driven_active_fn      = get_gpu_driven_active_fn;
    g_ctx.get_gpu_indirect_draw_count_fn = get_gpu_indirect_draw_count_fn;
    g_ctx.get_gpu_total_instances_fn    = get_gpu_total_instances_fn;
    g_ctx.floating_origin               = floating_origin;
}

extern "C" void* dse_get_floating_origin_ptr(void) { return g_ctx.floating_origin; }

// ============================================================
// Input 扩展
// ============================================================

extern "C" float dse_input_get_screen_width(void) {
    return static_cast<float>(Screen::width());
}

extern "C" float dse_input_get_screen_height(void) {
    return static_cast<float>(Screen::height());
}

extern "C" int dse_input_is_gamepad_connected(int gamepad_id) {
    return Input::IsGamepadConnected(gamepad_id) ? 1 : 0;
}

extern "C" void dse_input_set_gamepad_dead_zone(float zone) {
    Input::SetGamepadDeadZone(zone);
}

extern "C" float dse_input_get_gamepad_dead_zone(void) {
    return Input::GetGamepadDeadZone();
}

extern "C" float dse_input_get_mouse_scroll_dx(void) {
    return 0.0f;  // GLFW only reports vertical scroll
}

extern "C" float dse_input_get_mouse_scroll_dy(void) {
    return Input::mouseScroll();
}

extern "C" int dse_input_get_mouse_middle(void) {
    return Input::GetMouseButton(2) ? 1 : 0;
}

extern "C" int dse_input_get_mouse_middle_down(void) {
    return Input::GetMouseButtonDown(2) ? 1 : 0;
}

extern "C" int dse_input_get_mouse_left_double_click(void) {
    return Input::GetDoubleClick(0) ? 1 : 0;
}

extern "C" int dse_input_get_mouse_left_long_press(float duration) {
    return Input::GetLongPress(0, duration) ? 1 : 0;
}

extern "C" float dse_input_get_mouse_swipe_dx(void) {
    return Input::GetSwipeDelta().x;
}

extern "C" float dse_input_get_mouse_swipe_dy(void) {
    return Input::GetSwipeDelta().y;
}

extern "C" int dse_input_get_device_shake(void) {
    return Input::IsDeviceShaking() ? 1 : 0;
}

extern "C" int dse_input_get_touch_count(void) {
    return 0;  // Desktop: no touch
}

extern "C" int dse_input_get_touch(int index, float* out_x, float* out_y, int* out_phase) {
    if (out_x) *out_x = 0.0f;
    if (out_y) *out_y = 0.0f;
    if (out_phase) *out_phase = 0;
    return 0;  // Desktop: no touch
}

// ============================================================
// App / Time 扩展
// ============================================================

extern "C" float dse_app_get_time_since_startup(void) {
    return Time::TimeSinceStartup();
}

extern "C" void dse_app_set_time_scale(float scale) {
    Time::set_time_scale(scale);
}

extern "C" float dse_app_get_time_scale(void) {
    return Time::time_scale();
}

extern "C" float dse_app_get_fps(void) {
    float dt = Time::delta_time();
    return (dt > 0.0f) ? (1.0f / dt) : 0.0f;
}

extern "C" float dse_app_get_frame_time_ms(void) {
    return Time::delta_time() * 1000.0f;
}

// ============================================================
// Metrics 扩展
// ============================================================

extern "C" int dse_metrics_get_max_batch_sprites(void) {
    return g_ctx.get_max_batch_sprites_fn ? g_ctx.get_max_batch_sprites_fn() : 0;
}

extern "C" int dse_metrics_get_sprite_count(void) {
    return g_ctx.get_sprite_count_fn ? g_ctx.get_sprite_count_fn() : 0;
}

extern "C" int dse_metrics_get_gpu_driven_active(void) {
    return g_ctx.get_gpu_driven_active_fn ? g_ctx.get_gpu_driven_active_fn() : 0;
}

extern "C" int dse_metrics_get_gpu_indirect_draw_count(void) {
    return g_ctx.get_gpu_indirect_draw_count_fn ? g_ctx.get_gpu_indirect_draw_count_fn() : 0;
}

extern "C" int dse_metrics_get_gpu_total_instances(void) {
    return g_ctx.get_gpu_total_instances_fn ? g_ctx.get_gpu_total_instances_fn() : 0;
}

extern "C" float dse_metrics_get_fps(void) {
    float dt = Time::delta_time();
    return (dt > 0.0f) ? (1.0f / dt) : 0.0f;
}

extern "C" float dse_metrics_get_frame_time_ms(void) {
    return Time::delta_time() * 1000.0f;
}

// ============================================================
// Floating Origin
// ============================================================

static dse::FloatingOriginSystem* GetFO() {
    return static_cast<dse::FloatingOriginSystem*>(g_ctx.floating_origin);
}

extern "C" void dse_origin_get_accumulated(float* out_x, float* out_y, float* out_z) {
    auto* fo = GetFO();
    if (!fo) { if (out_x) *out_x = 0; if (out_y) *out_y = 0; if (out_z) *out_z = 0; return; }
    const auto& acc = fo->accumulated_origin();
    if (out_x) *out_x = static_cast<float>(acc.x);
    if (out_y) *out_y = static_cast<float>(acc.y);
    if (out_z) *out_z = static_cast<float>(acc.z);
}

extern "C" void dse_origin_to_absolute(float lx, float ly, float lz, float* out_x, float* out_y, float* out_z) {
    auto* fo = GetFO();
    if (!fo) { if (out_x) *out_x = lx; if (out_y) *out_y = ly; if (out_z) *out_z = lz; return; }
    glm::dvec3 abs = fo->ToAbsolute(glm::vec3(lx, ly, lz));
    if (out_x) *out_x = static_cast<float>(abs.x);
    if (out_y) *out_y = static_cast<float>(abs.y);
    if (out_z) *out_z = static_cast<float>(abs.z);
}

extern "C" void dse_origin_to_local(float ax, float ay, float az, float* out_x, float* out_y, float* out_z) {
    auto* fo = GetFO();
    if (!fo) { if (out_x) *out_x = ax; if (out_y) *out_y = ay; if (out_z) *out_z = az; return; }
    glm::vec3 loc = fo->ToLocal(glm::dvec3(ax, ay, az));
    if (out_x) *out_x = loc.x;
    if (out_y) *out_y = loc.y;
    if (out_z) *out_z = loc.z;
}

extern "C" void dse_origin_set_rebase_threshold(float threshold) {
    auto* fo = GetFO();
    if (fo) fo->set_rebase_threshold(threshold);
}

extern "C" float dse_origin_get_rebase_threshold(void) {
    auto* fo = GetFO();
    return fo ? fo->rebase_threshold() : 5000.0f;
}
