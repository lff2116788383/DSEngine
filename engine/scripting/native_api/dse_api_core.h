/**
 * @file dse_api_core.h
 * @brief DSEngine Native C ABI — Core (Entity/Transform/ECS/Input/App/Metrics) module
 *
 * Split from dse_api.h for maintainability. Include dse_api.h for all modules.
 */

#ifndef DSE_API_CORE_H
#define DSE_API_CORE_H

#include <stdint.h>

#ifndef DSE_CAPI
#  ifdef _WIN32
#    define DSE_CAPI __declspec(dllexport)
#  else
#    define DSE_CAPI __attribute__((visibility("default")))
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// API Version — 用于运行时 ABI 兼容性检测
// ============================================================

/// 返回 C ABI 版本号。每次 dse_api.h / dse_api.gen.h 的公共接口
/// 发生破坏性变更时递增。脚本宿主（Lua/C#）在加载 DLL 后应首先
/// 调用此函数，与编译时 DSE_API_VERSION 比较，不一致则拒绝运行。
///   格式：MMNNPP（Major * 10000 + Minor * 100 + Patch）
DSE_CAPI uint32_t dse_api_version(void);

#ifndef DSE_API_VERSION
#define DSE_API_VERSION 10000u   /* v1.0.0 — MMNNPP format */
#endif

// ============================================================

// Context Setup — 引擎初始化时调用一次
// ============================================================

DSE_CAPI void dse_native_api_init(
    void* world,           // World*
    void* asset_manager,   // AssetManager*
    void* audio_system,    // AudioSystem* (可为 nullptr)
    void  (*quit_fn)(void),
    void  (*set_title_fn)(const char*),
    float (*get_fps_fn)(void),
    void  (*set_fps_fn)(float),
    int   (*get_draw_calls_fn)(void)   // 可为 nullptr
);

// 供生成代码（dse_api.gen.cpp）访问内部 World 指针
DSE_CAPI void* dse_get_world_ptr(void);

// ============================================================

// Entity
// ============================================================

DSE_CAPI uint32_t dse_entity_create(void);
DSE_CAPI void     dse_entity_destroy(uint32_t e);
DSE_CAPI int      dse_entity_valid(uint32_t e);

// ============================================================

// TransformComponent
// ============================================================

DSE_CAPI void dse_transform_add(uint32_t e,
    float x, float y, float z,
    float sx, float sy, float sz);

DSE_CAPI void dse_transform_get_position(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_transform_set_position(uint32_t e, float x, float y, float z);

DSE_CAPI void dse_transform_get_rotation(uint32_t e, float* x, float* y, float* z); // Euler degrees
DSE_CAPI void dse_transform_set_rotation(uint32_t e, float x, float y, float z);    // Euler degrees

DSE_CAPI void dse_transform_get_scale(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_transform_set_scale(uint32_t e, float x, float y, float z);

// ============================================================

// Input
// ============================================================

DSE_CAPI int   dse_input_get_key(int key_code);
DSE_CAPI int   dse_input_get_key_down(int key_code);
DSE_CAPI int   dse_input_get_key_up(int key_code);
DSE_CAPI int   dse_input_get_mouse_button(int button);
DSE_CAPI int   dse_input_get_mouse_button_down(int button);
DSE_CAPI int   dse_input_get_mouse_button_up(int button);
DSE_CAPI float dse_input_get_mouse_x(void);
DSE_CAPI float dse_input_get_mouse_y(void);
DSE_CAPI float dse_input_get_mouse_scroll(void);
DSE_CAPI float dse_input_get_gamepad_axis(int gamepad_id, int axis);

// ============================================================

// Assets
// ============================================================

DSE_CAPI uint32_t dse_assets_load_texture(const char* path);
DSE_CAPI void     dse_assets_set_data_root(const char* path);

// ============================================================

// App / System
// ============================================================

DSE_CAPI void  dse_app_quit(void);
DSE_CAPI void  dse_app_set_window_title(const char* title);
DSE_CAPI float dse_app_get_time(void);
DSE_CAPI float dse_app_get_delta_time(void);
DSE_CAPI void  dse_app_set_target_fps(float fps);
DSE_CAPI float dse_app_get_target_fps(void);

// ============================================================

// Metrics
// ============================================================

DSE_CAPI int dse_metrics_get_draw_calls(void);

// ============================================================
// Audio（全局 AudioSystem + ECS AudioSource，手写 dse_api_services.cpp）
// ============================================================
// 全局函数依赖 dse_native_api_init 注入的 AudioSystem*，为 null 时安全返回 0/无操作。
// bool 参数/返回值使用 int(0/1)。

DSE_CAPI int  dse_audio_play_bgm(const char* path, float volume, int loop);
DSE_CAPI void dse_audio_pause_bgm(void);
DSE_CAPI void dse_audio_resume_bgm(void);
DSE_CAPI void dse_audio_stop_bgm(void);
DSE_CAPI int  dse_audio_crossfade_bgm(const char* path, float fade_sec, float volume, int loop);
DSE_CAPI void dse_audio_play_sfx(const char* path, float volume, int loop);
DSE_CAPI void dse_audio_stop_all_sfx(void);
DSE_CAPI void dse_audio_fade_out_all_sfx(float duration_sec);
DSE_CAPI int  dse_audio_preload(const char* path);
DSE_CAPI void dse_audio_set_master_volume(float volume);
DSE_CAPI void dse_audio_set_bgm_volume(float volume);
DSE_CAPI void dse_audio_set_sfx_volume(float volume);

// ECS AudioSource / AudioListener（纯组件操作）。
DSE_CAPI void dse_audio_source_add(uint32_t e, const char* path, int play_on_awake,
                                   int loop, float volume);
DSE_CAPI void dse_audio_source_set_playing(uint32_t e, int playing);
DSE_CAPI void dse_audio_source_restart(uint32_t e);
DSE_CAPI void dse_audio_source_set_loop(uint32_t e, int loop);
DSE_CAPI void dse_audio_source_set_volume(uint32_t e, float volume);
DSE_CAPI void dse_audio_source_set_pitch(uint32_t e, float pitch);
DSE_CAPI void dse_audio_source_set_3d_mode(uint32_t e, int enabled);
DSE_CAPI void dse_audio_source_set_3d_distance(uint32_t e, float min_distance,
                                               float max_distance, float rolloff);
DSE_CAPI void dse_audio_source_set_bus(uint32_t e, const char* bus_name);
DSE_CAPI int  dse_audio_source_is_playing(uint32_t e);
DSE_CAPI void dse_audio_listener_add(uint32_t e, int enabled);


// ECS Core — 通用组件查询 / 层级 / 脚本 / AABB / 时间缩放
// ============================================================
// find_*：实体 id 写入 out（最多 cap 个），返回总数；组件名未知返回 -1。
// has_component：1/0；组件名未知返回 -1。
// queryable components：组件名以 '\n' 分隔写入 out，返回条数。

DSE_CAPI int  dse_ecs_find_entities_by_mesh_path(const char* mesh_path,
                                                 uint32_t* out, int cap);
DSE_CAPI int  dse_ecs_find_entities_with(const char* component, uint32_t* out, int cap);
DSE_CAPI int  dse_ecs_count_entities_with(const char* component);
DSE_CAPI int  dse_ecs_has_component(uint32_t e, const char* component);
DSE_CAPI int  dse_ecs_get_queryable_components(char* out, int cap);

// AABB：out_min_max={min_x,min_y,min_z,max_x,max_y,max_z}（float[6]）。返回 1=有 BoundingBox。
DSE_CAPI int  dse_ecs_get_world_aabb(uint32_t e, float* out_min_max);
DSE_CAPI int  dse_ecs_get_local_aabb(uint32_t e, float* out_min_max);

// TimeScaleComponent：get 无组件时返回 1.0。
DSE_CAPI void  dse_ecs_set_time_scale(uint32_t e, float scale);
DSE_CAPI float dse_ecs_get_time_scale(uint32_t e);

// Transform（position+scale 初始化，emplace_or_replace）
DSE_CAPI void dse_ecs_add_transform(uint32_t e, float x, float y, float z,
                                    float sx, float sy, float sz);

// ParentComponent。get_parent：无父级返回 0xFFFFFFFF。
DSE_CAPI void     dse_ecs_add_parent(uint32_t e, uint32_t parent);
DSE_CAPI void     dse_ecs_set_parent(uint32_t e, uint32_t parent);
DSE_CAPI uint32_t dse_ecs_get_parent(uint32_t e);
DSE_CAPI void     dse_ecs_clear_parent(uint32_t e);

// ScriptComponent。get_script_path/enabled：无组件返回 -1。
DSE_CAPI void dse_ecs_add_script(uint32_t e, const char* path);
DSE_CAPI void dse_ecs_set_script_path(uint32_t e, const char* path);
DSE_CAPI int  dse_ecs_get_script_path(uint32_t e, char* out, int cap);
DSE_CAPI void dse_ecs_set_script_enabled(uint32_t e, int enabled);
DSE_CAPI int  dse_ecs_get_script_enabled(uint32_t e);

// ============================================================
// UI（核心控件，纯 ECS 组件操作）
// ============================================================
// bool 参数/返回值使用 int(0/1)。

DSE_CAPI void  dse_ui_add_renderer(uint32_t e, uint32_t texture_handle,
                                   float r, float g, float b, float a,
                                   int order, float w, float h);
DSE_CAPI void  dse_ui_add_panel(uint32_t e, int blocks_input);
DSE_CAPI void  dse_ui_add_button(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_add_ttf_label(uint32_t e, const char* text, const char* font_id,
                                    float font_size, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_set_label_text(uint32_t e, const char* text);
DSE_CAPI void  dse_ui_set_label_font(uint32_t e, const char* font_id, float font_size);
DSE_CAPI void  dse_ui_set_position(uint32_t e, float x, float y);
DSE_CAPI void  dse_ui_set_size(uint32_t e, float w, float h);
DSE_CAPI void  dse_ui_set_anchor(uint32_t e, float ax, float ay);
DSE_CAPI void  dse_ui_set_color(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_set_visible(uint32_t e, int visible);
DSE_CAPI int   dse_ui_is_hovered(uint32_t e);
DSE_CAPI int   dse_ui_is_pressed(uint32_t e);

DSE_CAPI void  dse_ui_add_joystick(uint32_t e, float max_radius, int follow_pointer,
                                   int reset_on_release);
DSE_CAPI float dse_ui_get_joystick_x(uint32_t e);
DSE_CAPI float dse_ui_get_joystick_y(uint32_t e);

DSE_CAPI void  dse_ui_add_slider(uint32_t e, float min_value, float max_value,
                                 float value, int whole_numbers);
DSE_CAPI void  dse_ui_set_slider_value(uint32_t e, float value);
DSE_CAPI float dse_ui_get_slider_value(uint32_t e);

DSE_CAPI void  dse_ui_add_toggle(uint32_t e, int is_on, int group);
DSE_CAPI void  dse_ui_set_toggle(uint32_t e, int is_on);
DSE_CAPI int   dse_ui_get_toggle(uint32_t e);

DSE_CAPI void  dse_ui_add_progress_bar(uint32_t e, float value, float max_value);
DSE_CAPI void  dse_ui_set_progress(uint32_t e, float value);
DSE_CAPI float dse_ui_get_progress(uint32_t e);

DSE_CAPI void  dse_ui_add_text_input(uint32_t e, const char* placeholder,
                                     int max_length, int is_password);
DSE_CAPI void  dse_ui_set_text_input_text(uint32_t e, const char* text);
DSE_CAPI int   dse_ui_get_text_input_text(uint32_t e, char* out, int cap);
// Lua 绑定薄包装：返回字符串
DSE_CAPI const char* dse_compat_ui_get_text_input_text(uint32_t e);
DSE_CAPI void  dse_ui_set_text_input_focus(uint32_t e, int focused);

// UI 布局文件加载（UISerializer）。返回创建的实体数（写入 out_entities，最多 cap 个）。
DSE_CAPI int   dse_ui_load_from_file(const char* path, uint32_t* out_entities, int cap);
DSE_CAPI int   dse_ui_load_from_json(const char* json, uint32_t* out_entities, int cap);

// 供手写实现访问内部 AudioSystem 指针
DSE_CAPI void* dse_get_audio_system_ptr(void);

// ============================================================

// Extended Context Setup — 扩展上下文（metrics 回调 + floating origin）
// ============================================================

DSE_CAPI void dse_native_api_init_ext(
    int   (*get_max_batch_sprites_fn)(void),
    int   (*get_sprite_count_fn)(void),
    int   (*get_gpu_driven_active_fn)(void),
    int   (*get_gpu_indirect_draw_count_fn)(void),
    int   (*get_gpu_total_instances_fn)(void),
    void* floating_origin);

DSE_CAPI void* dse_get_floating_origin_ptr(void);

// ============================================================
// Input 扩展（Screen / Gamepad buttons / Touch / Mouse extras）
// ============================================================

DSE_CAPI float dse_input_get_screen_width(void);
DSE_CAPI float dse_input_get_screen_height(void);
DSE_CAPI int   dse_input_is_gamepad_connected(int gamepad_id);
DSE_CAPI void  dse_input_set_gamepad_dead_zone(float zone);
DSE_CAPI float dse_input_get_gamepad_dead_zone(void);
DSE_CAPI float dse_input_get_mouse_scroll_dx(void);
DSE_CAPI float dse_input_get_mouse_scroll_dy(void);
DSE_CAPI int   dse_input_get_mouse_middle(void);
DSE_CAPI int   dse_input_get_mouse_middle_down(void);
DSE_CAPI int   dse_input_get_mouse_left_double_click(void);
DSE_CAPI int   dse_input_get_mouse_left_long_press(float duration);
DSE_CAPI float dse_input_get_mouse_swipe_dx(void);
DSE_CAPI float dse_input_get_mouse_swipe_dy(void);
DSE_CAPI int   dse_input_get_device_shake(void);
DSE_CAPI int   dse_input_get_touch_count(void);
DSE_CAPI int   dse_input_get_touch(int index, float* out_x, float* out_y, int* out_phase);

// ============================================================
// App / Time 扩展
// ============================================================

DSE_CAPI float dse_app_get_time_since_startup(void);
DSE_CAPI void  dse_app_set_time_scale(float scale);
DSE_CAPI float dse_app_get_time_scale(void);
DSE_CAPI float dse_app_get_fps(void);
DSE_CAPI float dse_app_get_frame_time_ms(void);

// ============================================================
// Metrics 扩展
// ============================================================

DSE_CAPI int   dse_metrics_get_max_batch_sprites(void);
DSE_CAPI int   dse_metrics_get_sprite_count(void);
DSE_CAPI int   dse_metrics_get_gpu_driven_active(void);
DSE_CAPI int   dse_metrics_get_gpu_indirect_draw_count(void);
DSE_CAPI int   dse_metrics_get_gpu_total_instances(void);
DSE_CAPI float dse_metrics_get_fps(void);
DSE_CAPI float dse_metrics_get_frame_time_ms(void);

// ============================================================

// Floating Origin
// ============================================================

DSE_CAPI void  dse_origin_get_accumulated(float* out_x, float* out_y, float* out_z);
DSE_CAPI void  dse_origin_to_absolute(float lx, float ly, float lz, float* out_x, float* out_y, float* out_z);
DSE_CAPI void  dse_origin_to_local(float ax, float ay, float az, float* out_x, float* out_y, float* out_z);
DSE_CAPI void  dse_origin_set_rebase_threshold(float threshold);
DSE_CAPI float dse_origin_get_rebase_threshold(void);

// ============================================================
// Physics3D 扩展（Collision/Trigger events, SphereCast, BoxCast, Sleep/Wake, Kinematic, Mass, Damping）
// ============================================================
// collision_events: 每个事件 11 floats (type, entity_a, entity_b, px,py,pz, nx,ny,nz, impulse, pad)
//                   写入 out_buf（cap 个 float），返回实际事件数
DSE_CAPI int dse_physics3d_get_collision_count(void);
DSE_CAPI int dse_physics3d_get_collision_events(float* out_buf, int cap);
DSE_CAPI int dse_physics3d_get_trigger_count(void);
DSE_CAPI int dse_physics3d_get_trigger_events(uint32_t* out_entities, int* out_types, int cap);
DSE_CAPI int dse_physics3d_spherecast(float ox, float oy, float oz,
                                      float dx, float dy, float dz,
                                      float radius, float max_dist,
                                      uint32_t* out_entity, float* out_point,
                                      float* out_normal, float* out_distance);
DSE_CAPI int dse_physics3d_boxcast(float ox, float oy, float oz,
                                   float dx, float dy, float dz,
                                   float hx, float hy, float hz,
                                   float max_dist,
                                   uint32_t* out_entity, float* out_point,
                                   float* out_normal, float* out_distance);
DSE_CAPI void dse_rigidbody3d_set_kinematic(uint32_t e, int kinematic);
DSE_CAPI float dse_rigidbody3d_get_mass(uint32_t e);   // codegen 逐字段
DSE_CAPI void  dse_rigidbody3d_set_mass(uint32_t e, float mass);  // codegen 逐字段
DSE_CAPI void  dse_rigidbody3d_add_force_at_position(uint32_t e, float fx, float fy, float fz,
                                                     float px, float py, float pz);
DSE_CAPI void  dse_rigidbody3d_set_linear_damping(uint32_t e, float damping);
DSE_CAPI float dse_rigidbody3d_get_linear_damping(uint32_t e);
DSE_CAPI void  dse_rigidbody3d_set_angular_damping(uint32_t e, float damping);
DSE_CAPI float dse_rigidbody3d_get_angular_damping(uint32_t e);

// ============================================================
// Physics2D（Box2D 集成）
// ============================================================

DSE_CAPI void dse_physics2d_add_rigidbody(uint32_t e, int type, float gravity_scale, int fixed_rotation);
DSE_CAPI void dse_physics2d_set_rigidbody_velocity(uint32_t e, float vx, float vy);
DSE_CAPI void dse_physics2d_add_box_collider(uint32_t e, float w, float h,
                                              float density, float friction, float restitution);
DSE_CAPI void dse_physics2d_set_box_collider_trigger(uint32_t e, int is_trigger);
DSE_CAPI void dse_physics2d_add_circle_collider(uint32_t e, float radius,
                                                 float density, float friction, float restitution);
DSE_CAPI void dse_physics2d_set_circle_collider_trigger(uint32_t e, int is_trigger);
DSE_CAPI void dse_physics2d_add_polygon_collider(uint32_t e, const float* verts, int count,
                                                  float density, float friction, float restitution);
DSE_CAPI void dse_physics2d_set_polygon_collider_trigger(uint32_t e, int is_trigger);
DSE_CAPI void dse_physics2d_add_joint(uint32_t e, int type, uint32_t entity_a, uint32_t entity_b,
                                       float ax, float ay, float bx, float by, int collide_connected);
DSE_CAPI void dse_physics2d_set_joint_revolute(uint32_t e, int enable_limit, float lower_deg, float upper_deg,
                                                int enable_motor, float motor_speed, float max_torque);
DSE_CAPI void dse_physics2d_set_joint_distance(uint32_t e, float min_len, float max_len,
                                                float stiffness, float damping);
DSE_CAPI void dse_physics2d_set_joint_prismatic(uint32_t e, float axis_x, float axis_y,
                                                 int enable_limit, float lower, float upper,
                                                 int enable_motor, float motor_speed, float max_force);
DSE_CAPI void dse_physics2d_destroy_joint(uint32_t e);
DSE_CAPI int  dse_physics2d_raycast(float sx, float sy, float ex, float ey,
                                    uint32_t* out_entity, float* out_point, float* out_normal);
DSE_CAPI int  dse_physics2d_poll_collision_event(uint32_t e, uint32_t* out_other,
                                                  int* out_is_trigger, int* out_is_enter);
DSE_CAPI void dse_physics2d_add_tilemap(uint32_t e, int width, int height,
                                        float tile_size, uint32_t tex_handle);
DSE_CAPI void dse_physics2d_set_tile(uint32_t e, int x, int y, int tile_id);

// ============================================================
// 2D Systems（Parallax / Light2D / SpriteSheet / Atlas / Camera2D / Trail / LineRenderer / AudioSpatial2D）
// ============================================================

DSE_CAPI void  dse_parallax_add(uint32_t e);
DSE_CAPI int   dse_parallax_add_layer(uint32_t e, float scroll_x, float scroll_y);
DSE_CAPI void  dse_parallax_set_layer_scroll(uint32_t e, int layer, float sx, float sy);
DSE_CAPI void  dse_parallax_set_layer_auto_scroll(uint32_t e, int layer, float sx, float sy);
DSE_CAPI void  dse_parallax_set_layer_opacity(uint32_t e, int layer, float opacity);
DSE_CAPI int   dse_parallax_get_layer_count(uint32_t e);

DSE_CAPI void  dse_light2d_add(uint32_t e, int type);
DSE_CAPI void  dse_light2d_set_color(uint32_t e, float r, float g, float b);
DSE_CAPI void  dse_light2d_set_intensity(uint32_t e, float intensity);
DSE_CAPI void  dse_light2d_set_range(uint32_t e, float range);
DSE_CAPI void  dse_light2d_set_shadow(uint32_t e, int mode);
DSE_CAPI void  dse_light2d_set_ambient(uint32_t e, float r, float g, float b, float intensity);
DSE_CAPI void  dse_normal_map_2d_add(uint32_t e, float strength);

DSE_CAPI int   dse_sprite_sheet_load(const char* path);
DSE_CAPI int   dse_sprite_sheet_frame_count(int sheet);
DSE_CAPI void  dse_sprite_sheet_get_frame_uv(int sheet, int frame, float* out_uv);

DSE_CAPI int   dse_atlas_load(const char* path);
DSE_CAPI int   dse_atlas_entry_count(int atlas);
DSE_CAPI void  dse_atlas_get_entry_uv(int atlas, const char* name, float* out_uv);

DSE_CAPI void  dse_camera_controller_2d_add(uint32_t e);
DSE_CAPI void  dse_camera_2d_shake(uint32_t e, float trauma);
DSE_CAPI void  dse_camera_2d_set_zoom(uint32_t e, float zoom);
DSE_CAPI void  dse_camera_2d_set_bounds(uint32_t e, float min_x, float min_y, float max_x, float max_y);
DSE_CAPI void  dse_camera_2d_set_look_ahead(uint32_t e, float lax, float lay);

DSE_CAPI void  dse_trail_renderer_add(uint32_t e, float lifetime, float start_width, float end_width);
DSE_CAPI void  dse_trail_set_emitting(uint32_t e, int emitting);
DSE_CAPI void  dse_trail_set_colors(uint32_t e, float r1, float g1, float b1, float a1,
                                    float r2, float g2, float b2, float a2);
DSE_CAPI void  dse_trail_clear(uint32_t e);

DSE_CAPI void  dse_line_renderer_add(uint32_t e, float width);
DSE_CAPI void  dse_line_renderer_set_points(uint32_t e, const float* points, int count);
DSE_CAPI void  dse_line_renderer_set_width(uint32_t e, float width);
DSE_CAPI void  dse_line_renderer_set_color(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_line_renderer_set_closed(uint32_t e, int closed);

DSE_CAPI void  dse_audio_spatial_2d_add(uint32_t e, float min_dist, float max_dist);
DSE_CAPI void  dse_audio_spatial_2d_set_range(uint32_t e, float min_dist, float max_dist);
DSE_CAPI void  dse_audio_spatial_2d_set_attenuation(uint32_t e, int model, float rolloff);
DSE_CAPI void  dse_audio_listener_2d_add(uint32_t e, float global_volume);

// ============================================================

// HTTP — 异步完成队列模型
// ============================================================

// 发送请求，返回 request_id（0 = 失败）。headers_json 为 JSON 字符串 {"K":"V",...} 或 NULL。
DSE_CAPI uint32_t dse_http_send(const char* method, const char* url, const char* body,
                                const char* headers_json, int timeout_sec, int verify_peer,
                                const char* ca_file);
// 轮询已完成的请求 ID，返回数量。每帧调用。
DSE_CAPI int   dse_http_poll(uint32_t* out_ids, int max_ids);
// 获取指定请求的响应数据。成功返回 1。
DSE_CAPI int   dse_http_get_response(uint32_t request_id, int* out_status,
                                     char* out_body, int body_cap,
                                     char* out_error, int error_cap);
// 内部 pump（引擎 Tick 自动调用）
DSE_CAPI void  dse_http_update(void);
DSE_CAPI int   dse_http_available(void);

// ============================================================

// Video — 句柄式视频播放器
// ============================================================

DSE_CAPI uint32_t dse_video_create_player(void);
// 注入 RHI 设备指针（void* 避免头文件依赖）；播放前设置，未设置时视频纹理保持 stub
DSE_CAPI void  dse_video_set_rhi_device(void* rhi_device);
DSE_CAPI void  dse_video_destroy_player(uint32_t player);
// play: backend: 0=Auto, 1=FFmpeg, 2=PlMpeg
DSE_CAPI void  dse_video_play(uint32_t player, const char* path, int loop, float playback_rate,
                             int decode_audio, int prefetch_frames, int backend);
DSE_CAPI void  dse_video_pause(uint32_t player);
DSE_CAPI void  dse_video_resume(uint32_t player);
DSE_CAPI void  dse_video_stop(uint32_t player);
DSE_CAPI void  dse_video_seek(uint32_t player, float time);
DSE_CAPI void  dse_video_set_loop(uint32_t player, int loop);
DSE_CAPI void  dse_video_set_playback_rate(uint32_t player, float rate);
DSE_CAPI uint32_t dse_video_update(uint32_t player, float delta_time);  // 返回 texture_id
DSE_CAPI int   dse_video_get_state(uint32_t player);   // 0=Idle,1=Playing,2=Paused,3=Stopped,4=Ended,5=Error
DSE_CAPI float dse_video_get_time(uint32_t player);
DSE_CAPI float dse_video_get_duration(uint32_t player);
DSE_CAPI void  dse_video_get_info(uint32_t player, int* out_w, int* out_h, float* out_fps,
                                 float* out_duration, int* out_total_frames, int* out_has_audio,
                                 int* out_sample_rate, int* out_channels,
                                 char* out_codec, int codec_cap);
DSE_CAPI uint32_t dse_video_get_texture(uint32_t player);

// ============================================================
// DSSL（DSEngine Shader Language）
// ============================================================

DSE_CAPI uint32_t dse_dssl_load_material(const char* path);
DSE_CAPI uint32_t dse_dssl_create_instance(const char* path);
DSE_CAPI void  dse_dssl_set_float(uint32_t instance, const char* name, float value);
DSE_CAPI void  dse_dssl_set_color(uint32_t instance, const char* name, float r, float g, float b, float a);
DSE_CAPI void  dse_dssl_set_vec3(uint32_t instance, const char* name, float x, float y, float z);
DSE_CAPI void  dse_dssl_set_texture(uint32_t instance, const char* name, const char* path);
DSE_CAPI void  dse_dssl_set_texture_handle(uint32_t instance, const char* name, uint32_t handle);
DSE_CAPI void  dse_dssl_apply_material(uint32_t e, uint32_t instance);
DSE_CAPI float dse_dssl_get_float(uint32_t instance, const char* name);
DSE_CAPI void  dse_dssl_get_color(uint32_t instance, const char* name, float* out_rgba);

// ============================================================

// Meshlet — Cluster 渲染系统
// ============================================================

// build: positions = flat xyz triplets (count = pos_count * 3), indices = uint32 array
DSE_CAPI uint32_t dse_meshlet_build(const float* positions, int pos_count,
                                    const uint32_t* indices, int idx_count,
                                    uint32_t max_vertices, uint32_t max_triangles);
DSE_CAPI int   dse_meshlet_serialize(uint32_t handle, const char* path);
DSE_CAPI uint32_t dse_meshlet_deserialize(const char* path);
DSE_CAPI void  dse_meshlet_destroy(uint32_t handle);
DSE_CAPI void  dse_meshlet_get_info(uint32_t handle, int* out_meshlets, int* out_vertices,
                                    int* out_indices, int* out_meshlet_vertices);
DSE_CAPI uint32_t dse_meshlet_cull_create(void);
DSE_CAPI void  dse_meshlet_cull_destroy(uint32_t cull_handle);
DSE_CAPI uint32_t dse_meshlet_cull_register(uint32_t cull_handle, uint32_t meshlet_handle);
DSE_CAPI void  dse_meshlet_cull_unregister(uint32_t cull_handle, uint32_t reg_handle);
DSE_CAPI void  dse_meshlet_cull_begin_frame(uint32_t cull_handle);
DSE_CAPI void  dse_meshlet_cull_add_instance(uint32_t cull_handle, uint32_t reg_handle,
                                            const float* matrix16);
// prepare: vp_matrix = 16 floats, returns instance count
DSE_CAPI uint32_t dse_meshlet_cull_prepare(uint32_t cull_handle, const float* vp_matrix16,
                                           float cam_x, float cam_y, float cam_z);
// execute_cpu: vp_matrix = 16 floats, flags: bit0=frustum, bit1=occlusion, bit2=cone
DSE_CAPI uint32_t dse_meshlet_cull_execute_cpu(uint32_t cull_handle, const float* vp_matrix16,
                                                float cam_x, float cam_y, float cam_z,
                                                uint32_t flags);
DSE_CAPI void  dse_meshlet_cull_stats(uint32_t cull_handle, int* out_total, int* out_visible,
                                      int* out_meshes, int* out_instances);

// ============================================================
// World Systems（Spline / Ocean / Editor / VSM / EQS / Distribution）
// ============================================================


// Spline
DSE_CAPI int   dse_spline_init(void);
DSE_CAPI void  dse_spline_shutdown(void);
DSE_CAPI uint32_t dse_spline_create(const char* name);
DSE_CAPI void  dse_spline_destroy(uint32_t spline);
DSE_CAPI void  dse_spline_add_point(uint32_t spline, float x, float y, float z, float width);
DSE_CAPI void  dse_spline_set_point(uint32_t spline, int index, float x, float y, float z, float width);
DSE_CAPI void  dse_spline_remove_point(uint32_t spline, int index);
DSE_CAPI int   dse_spline_get_point_count(uint32_t spline);
DSE_CAPI float dse_spline_get_length(uint32_t spline);
DSE_CAPI void  dse_spline_evaluate(uint32_t spline, float t, float* out_xyz);
DSE_CAPI void  dse_spline_evaluate_distance(uint32_t spline, float dist, float* out_xyz);
DSE_CAPI float dse_spline_find_nearest(uint32_t spline, float x, float y, float z);
DSE_CAPI int   dse_spline_gen_road(uint32_t spline, float segment_length, int width_segments);
DSE_CAPI int   dse_spline_gen_river(uint32_t spline, float segment_length, float depth);


// Ocean
DSE_CAPI int   dse_ocean_init(int fft_resolution, float tile_size, float wind_speed, float choppiness);
DSE_CAPI void  dse_ocean_shutdown(void);
DSE_CAPI void  dse_ocean_update(float time, float cam_x, float cam_y, float cam_z);
DSE_CAPI float dse_ocean_get_height(float x, float z);
DSE_CAPI void  dse_ocean_get_normal(float x, float z, float* out_xyz);
DSE_CAPI float dse_ocean_get_foam(float x, float z);
DSE_CAPI void  dse_ocean_set_wind(float speed, float dx, float dz);
DSE_CAPI void  dse_ocean_set_choppiness(float choppiness);
DSE_CAPI void  dse_ocean_get_stats(int* out_total_tiles, int* out_visible_tiles,
                                   int* out_fft_res, float* out_max_height);
DSE_CAPI int   dse_ocean_get_lod_count(void);


// Editor helpers
DSE_CAPI int   dse_editor_init(void);
DSE_CAPI void  dse_editor_shutdown(void);
DSE_CAPI int   dse_editor_terrain_brush(int op, float x, float y, float z, float radius,
                                        float strength, float falloff);
DSE_CAPI void  dse_editor_brush_preview(float x, float y, float z, float radius,
                                        float* out_min_x, float* out_min_y,
                                        float* out_max_x, float* out_max_y);
DSE_CAPI int   dse_editor_place_foliage(float x, float y, float z, float radius,
                                        float density, const char* mesh_path);
DSE_CAPI int   dse_editor_erase_foliage(float x, float y, float z, float radius);
DSE_CAPI int   dse_editor_get_foliage_count(void);
DSE_CAPI int   dse_editor_begin_road(float width);
DSE_CAPI void  dse_editor_add_road_point(uint32_t session, float x, float y, float z);
DSE_CAPI void  dse_editor_end_road(uint32_t session);
DSE_CAPI void  dse_editor_update_partition_vis(float cam_x, float cam_y, float cam_z, float cell_size);
DSE_CAPI int   dse_editor_get_cell_count(void);
DSE_CAPI int   dse_editor_undo(void);
DSE_CAPI int   dse_editor_redo(void);

// VSM (Virtual Shadow Maps)
DSE_CAPI int   dse_vsm_init(uint32_t virtual_resolution, uint32_t page_size,
                            uint32_t pool_pages, uint32_t clipmap_levels);
DSE_CAPI void  dse_vsm_shutdown(void);
DSE_CAPI uint32_t dse_vsm_register_light(uint32_t light_id, int is_directional,
                                         float dx, float dy, float dz);
DSE_CAPI void  dse_vsm_unregister_light(uint32_t light_id);
DSE_CAPI void  dse_vsm_begin_frame(uint32_t frame, float cam_x, float cam_y, float cam_z);
DSE_CAPI void  dse_vsm_end_frame(void);
DSE_CAPI void  dse_vsm_invalidate(uint32_t light_id,
                                  float min_x, float min_y, float min_z,
                                  float max_x, float max_y, float max_z);
DSE_CAPI void  dse_vsm_mark_page_rendered(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id);
DSE_CAPI int   dse_vsm_get_pages_to_render(void);
DSE_CAPI int   dse_vsm_lookup_page(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id,
                                   uint32_t* out_px, uint32_t* out_py);
DSE_CAPI void  dse_vsm_get_stats(int* out_total, int* out_mapped, int* out_dirty,
                                 int* out_rendered, int* out_cache_hit, int* out_pool_usage);
DSE_CAPI int   dse_vsm_get_clipmap_levels(void);

// EQS (Environment Query System)
DSE_CAPI int   dse_eqs_init(void);
DSE_CAPI void  dse_eqs_shutdown(void);
DSE_CAPI uint32_t dse_eqs_create_template(const char* name);
DSE_CAPI void  dse_eqs_destroy_template(uint32_t tmpl);
DSE_CAPI void  dse_eqs_set_generator(uint32_t tmpl, int type, float radius, float spacing, int max_points);
DSE_CAPI void  dse_eqs_add_scorer(uint32_t tmpl, int type, float weight, int invert, float max_value);
DSE_CAPI void  dse_eqs_clear_scorers(uint32_t tmpl);
DSE_CAPI void  dse_eqs_set_combine_mode(uint32_t tmpl, int mode);
DSE_CAPI void  dse_eqs_set_max_results(uint32_t tmpl, uint32_t max_results);
// execute: out = {best_x,best_y,best_z,best_score,total_generated,valid_count,query_time_ms}
DSE_CAPI void  dse_eqs_execute(uint32_t tmpl, float x, float y, float z, float* out_result);
DSE_CAPI int   dse_eqs_get_template_count(void);
DSE_CAPI void  dse_eqs_execute_at(uint32_t tmpl, float px, float py, float pz,
                                  float cx, float cy, float cz, float* out_result);


// Distribution
DSE_CAPI int   dse_dist_init(float cell_size, int max_downloads, const char* cdn_url);
DSE_CAPI void  dse_dist_shutdown(void);
DSE_CAPI int   dse_dist_load_manifest(const char* path);
DSE_CAPI int   dse_dist_save_manifest(const char* path);
DSE_CAPI int   dse_dist_package_cell(int cx, int cz, int lod, const char* const* assets, int count);
DSE_CAPI void  dse_dist_request_download(const char* package);
DSE_CAPI void  dse_dist_cancel_download(const char* package);
DSE_CAPI void  dse_dist_update_priorities(float x, float y, float z);
DSE_CAPI void  dse_dist_tick(float dt);
DSE_CAPI int   dse_dist_is_installed(const char* package);
DSE_CAPI void  dse_dist_get_stats(int* out_total, int* out_installed, int* out_downloading,
                                  int* out_pending, double* out_downloaded_bytes, double* out_speed_bps);
DSE_CAPI int   dse_dist_get_missing(float x, float y, float z, float radius,
                                    char* out_buf, int buf_cap);
DSE_CAPI int   dse_dist_verify(const char* package);
DSE_CAPI uint64_t dse_dist_get_disk_usage(void);

// ============================================================

// Font — 字体服务
// ============================================================

DSE_CAPI int   dse_font_load(const char* font_id, const char* ttf_path);
DSE_CAPI int   dse_font_load_cjk(const char* font_id, const char* ttf_path);
DSE_CAPI void  dse_font_unload(const char* font_id);
DSE_CAPI int   dse_font_set_default(const char* font_id);
DSE_CAPI float dse_font_measure(const char* text, const char* font_id, float font_size);
DSE_CAPI float dse_font_line_height(const char* font_id, float font_size);
DSE_CAPI uint32_t dse_font_get_texture(const char* font_id);

// ============================================================

// Spine — 骨骼动画渲染组件
// ============================================================

DSE_CAPI void  dse_spine_add_renderer(uint32_t e, const char* skel_path, const char* atlas_path);
DSE_CAPI void  dse_spine_set_animation(uint32_t e, const char* anim_name, int loop);

// ============================================================

// Terrain / Water / Grass / Foliage / Environment — 组件创建与复合操作
// ============================================================

DSE_CAPI void  dse_terrain_add(uint32_t e, const char* heightmap_path,
                                float width, float depth, float max_height);
DSE_CAPI void  dse_terrain_set_params(uint32_t e, int res_x, int res_z,
                                       int max_lod, float lod_factor, int use_dynamic_lod);
DSE_CAPI void  dse_terrain_set_height(uint32_t e, int x, int z, float height);
DSE_CAPI int   dse_terrain_load_heightmap(uint32_t e, const char* path,
                                           int* out_w, int* out_h, int* out_ch,
                                           int* out_rx, int* out_rz);
DSE_CAPI int   dse_terrain_set_texture(uint32_t e, const char* path,
                                        uint32_t* out_handle, int* out_w, int* out_h);
DSE_CAPI void  dse_terrain_get_lod(uint32_t e, int* out_lod, int* out_rx, int* out_rz,
                                    int* out_max_lod, float* out_lod_factor);
DSE_CAPI float dse_terrain_sample_height(uint32_t e, float wx, float wz);
DSE_CAPI int   dse_terrain_set_splat_texture(uint32_t e, int layer, const char* path);

DSE_CAPI void  dse_water_add(uint32_t e);
DSE_CAPI void  dse_water_set(uint32_t e, int enabled, float water_level,
                              float dr, float dg, float db,
                              float sr, float sg, float sb,
                              float max_depth, float transparency,
                              float wave_amp, float wave_freq, float wave_speed,
                              float wdir_x, float wdir_y,
                              float refraction, float reflection, float spec_power,
                              float caustic_int, float caustic_scale,
                              float foam_int, float foam_threshold,
                              float ufog_density, float ufog_r, float ufog_g, float ufog_b);
DSE_CAPI int   dse_water_get(uint32_t e, int* out_enabled, float* out_water_level,
                              float* out_deep_rgb, float* out_shallow_rgb,
                              float* out_max_depth, float* out_transparency,
                              float* out_wave, float* out_wdir,
                              float* out_refraction, float* out_reflection, float* out_spec_power);

DSE_CAPI void  dse_grass_add(uint32_t e, float density, float spawn_radius,
                              float blade_height, float blade_width);
DSE_CAPI void  dse_grass_set_params(uint32_t e, float density, float spawn_radius,
                                     float blade_height, float blade_width,
                                     float blade_height_var, float chunk_size, int seed);
DSE_CAPI void  dse_grass_set_color(uint32_t e, float br, float bg, float bb,
                                    float tr, float tg, float tb);
DSE_CAPI void  dse_grass_set_wind(uint32_t e, float dx, float dy,
                                   float speed, float strength, float turbulence);
DSE_CAPI void  dse_grass_set_lod(uint32_t e, float near_dist, float far_dist,
                                  int cast_shadow, float shadow_dist);
DSE_CAPI void  dse_grass_set_enabled(uint32_t e, int enabled);
DSE_CAPI int   dse_grass_get_stats(uint32_t e);

DSE_CAPI void  dse_foliage_add(uint32_t e);
DSE_CAPI void  dse_foliage_set_wind_strength(uint32_t e, float v);   // codegen 逐字段
DSE_CAPI float dse_foliage_get_wind_strength(uint32_t e);   // codegen 逐字段
DSE_CAPI void  dse_foliage_set_stiffness(uint32_t e, float v);   // codegen 逐字段
DSE_CAPI float dse_foliage_get_stiffness(uint32_t e);   // codegen 逐字段
DSE_CAPI void  dse_foliage_set_enabled(uint32_t e, int v);   // codegen 逐字段
DSE_CAPI int   dse_foliage_get_enabled(uint32_t e);   // codegen 逐字段

DSE_CAPI void  dse_tree_add(uint32_t e, const char* mesh_path);
DSE_CAPI void  dse_terrain_tile_manager_add(uint32_t e);
DSE_CAPI void  dse_dynamic_obstacle_add(uint32_t e, int shape);
DSE_CAPI void  dse_navmesh_rebake_add(uint32_t e);

// ============================================================

// Terrain Deformation
DSE_CAPI void dse_terrain_deform_init(float max_depth, float max_height);
DSE_CAPI int  dse_terrain_deform_apply(int type, float x, float y, float z,
                                       float radius, float strength);
DSE_CAPI int  dse_terrain_deform_undo(void);
DSE_CAPI int  dse_terrain_deform_redo(void);
DSE_CAPI float dse_terrain_deform_sample_height(float x, float z);
DSE_CAPI void dse_terrain_deform_shutdown(void);


// GOAP
DSE_CAPI int   dse_ai_goap_create(void);
DSE_CAPI void  dse_ai_goap_destroy(int planner_id);
DSE_CAPI void  dse_ai_goap_action_begin(int planner_id, const char* name, float cost);
DSE_CAPI void  dse_ai_goap_action_precondition(int planner_id, const char* key, int value);
DSE_CAPI void  dse_ai_goap_action_effect(int planner_id, const char* key, int value);
DSE_CAPI void  dse_ai_goap_action_commit(int planner_id);
DSE_CAPI void  dse_ai_goap_state_clear(int planner_id);
// which: 0=current_state 1=goal。
DSE_CAPI void  dse_ai_goap_state_set(int planner_id, int which, const char* key, int value);
// 规划：动作名以 '\n' 分隔写入 out（null 结尾，按 cap 截断）。返回 -1=无解，否则写入长度。
DSE_CAPI int   dse_ai_goap_plan(int planner_id, char* out, int cap);

#ifdef __cplusplus
}
#endif

#endif // DSE_API_CORE_H
