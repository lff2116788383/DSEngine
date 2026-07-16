/**
 * @file dse_api_services.h
 * @brief DSEngine Native C ABI — Services (Audio/Localization/UI/Navigation) module
 *
 * Split from dse_api.h for maintainability. Include dse_api.h for all modules.
 */

#ifndef DSE_API_SERVICES_H
#define DSE_API_SERVICES_H

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

// Audio — SFX 随机化 / 混音总线 / 快照 / 源状态
DSE_CAPI void dse_audio_play_sfx_random(const char* path, float volume,
                                        float pitch_min, float pitch_max);
DSE_CAPI int  dse_audio_bus_set_volume(const char* name, float volume);
DSE_CAPI int  dse_audio_bus_set_muted(const char* name, int muted);
DSE_CAPI int  dse_audio_bus_create(const char* name, const char* parent, float volume);
DSE_CAPI int  dse_audio_bus_remove(const char* name);
DSE_CAPI int  dse_audio_bus_add_effect(const char* bus_name, int type, float cutoff_hz, float q,
                                       float delay_time_ms, float feedback, float wet_mix,
                                       float room_size, float damping);
DSE_CAPI int  dse_audio_bus_remove_effect(const char* bus_name, int index);
// 名称以 '\n' 分隔写入 out（null 结尾，按 cap 截断），返回写入长度。
DSE_CAPI int  dse_audio_bus_get_names(char* out, int cap);
DSE_CAPI int  dse_audio_snapshot_save(const char* name);
DSE_CAPI int  dse_audio_snapshot_load(const char* name);
DSE_CAPI int  dse_audio_snapshot_list(char* out, int cap);
// 返回 1=组件存在；out_flags[3]=has_clip/is_playing/spatial；
// out_params[5]=min_distance/max_distance/rolloff/volume/pitch；
// out_runtime_handle/out_clip_size 可为 NULL；clip 路径写入 out_path。
DSE_CAPI int  dse_audio_source_get_state(uint32_t e, int* out_flags, float* out_params,
                                         long long* out_runtime_handle, long long* out_clip_size,
                                         char* out_path, int path_cap);
DSE_CAPI int  dse_audio_source_get_state_ex(uint32_t e,
                                            int* out_clip_loaded, int* out_is_playing, int* out_spatial,
                                            float* out_min_dist, float* out_max_dist, float* out_rolloff,
                                            float* out_volume, float* out_pitch,
                                            double* out_runtime_handle, double* out_clip_bytes,
                                            char* out_path, int path_cap);

// ============================================================
// Navigation（NavMeshSystem via ServiceLocator + NavMeshAgentComponent）
// ============================================================
// DSE_ENABLE_NAVMESH 关闭时所有函数为安全空实现（返回 0）。
// find_path：路径点写入 out_xyz（最多 max_points 点×3 float），返回实际点数；
// out_xyz=null 时仅返回点数。

DSE_CAPI int  dse_nav_is_ready(void);
DSE_CAPI int  dse_nav_load(const char* path);
DSE_CAPI int  dse_nav_save(const char* path);
DSE_CAPI int  dse_nav_find_nearest(float x, float y, float z, float* out_xyz);
DSE_CAPI int  dse_nav_raycast(float sx, float sy, float sz,
                              float ex, float ey, float ez, float* out_hit_xyz);
DSE_CAPI int  dse_nav_find_path(float sx, float sy, float sz,
                                float ex, float ey, float ez,
                                float* out_xyz, int max_points);
// bake：verts=nverts×3 float，tris=ntris×3 int；config 浮点 NaN=使用默认值。
DSE_CAPI int  dse_nav_bake(const float* verts, int nverts, const int* tris, int ntris,
                           float cell_size, float cell_height,
                           float agent_height, float agent_radius,
                           float agent_max_climb, float agent_max_slope);

// NavMeshAgent（ECS）。set_agent: 浮点 NaN=保持当前/默认值。
DSE_CAPI void dse_nav_agent_set(uint32_t e, float speed, float acceleration,
                                float stopping_dist, float radius, float height);
DSE_CAPI void dse_nav_agent_set_destination(uint32_t e, float x, float y, float z);
DSE_CAPI void dse_nav_agent_get_destination(uint32_t e, float* out_xyz);
DSE_CAPI int  dse_nav_agent_has_path(uint32_t e);
DSE_CAPI int  dse_nav_agent_arrived(uint32_t e);
// get：out_params={speed,acceleration,stopping_dist,radius,height,dest_x,dest_y,dest_z}（float[8]）；
// out_flags={has_path,path_pending,arrived,current_waypoint}（int[4]）。返回 1=存在组件。
DSE_CAPI int  dse_nav_agent_get(uint32_t e, float* out_params, int* out_flags);

// ============================================================
// Localization（LocalizationManager via ServiceLocator）
// ============================================================
// 字符串输出走 out 缓冲（null 结尾，按 cap 截断），返回写入长度（不含 null）。

DSE_CAPI int  dse_l10n_load(const char* path, const char* locale);
DSE_CAPI int  dse_l10n_load_string(const char* json, const char* locale);
DSE_CAPI void dse_l10n_set_locale(const char* locale);
DSE_CAPI int  dse_l10n_get_locale(char* out, int cap);
DSE_CAPI int  dse_l10n_get(const char* key, char* out, int cap);
DSE_CAPI int  dse_l10n_has_key(const char* key);
DSE_CAPI int  dse_l10n_get_locales(char* out, int cap);  // null-separated, returns count
// Lua 绑定薄包装
DSE_CAPI int         dse_compat_l10n_load(const char* locale, const char* json);
DSE_CAPI const char* dse_compat_l10n_get_locale(void);
DSE_CAPI const char* dse_compat_l10n_get(const char* key, const char* fallback);

// ============================================================
// Scene / Prefab 序列化
// ============================================================
// load/save：路径按字面使用（不做 data root 拼接）。返回 1=成功/0=失败。
// instantiate_prefab：use_pos!=0 时用 (x,y,z) 覆盖预制体内置 Transform；
// 返回新实体 id，失败返回 UINT32_MAX 语义的无效 id（dse_entity_valid 判 0）。

DSE_CAPI int      dse_scene_load(const char* path);
DSE_CAPI int      dse_scene_save(const char* path);
DSE_CAPI int      dse_scene_save_prefab(uint32_t e, const char* path);
DSE_CAPI uint32_t dse_scene_instantiate_prefab(const char* path, float x, float y, float z,
                                               int use_pos);

// ============================================================
// SubScene / SceneManager（异步加载 / 卸载 / 查询 / 场景过渡）
// ============================================================
// 路径相对 data root（内部拼接）。transition mode：0=instant 1=additive 2=fade。
// transition state：0=idle 1=fading_out 2=loading 3=fading_in。

DSE_CAPI int   dse_scene_load_sub(const char* path, int* out_entity_count);  // 同步加载
DSE_CAPI int   dse_scene_load_sub_async(const char* path);
DSE_CAPI void  dse_scene_unload_sub(const char* path);
DSE_CAPI void  dse_scene_unload_all_subs(void);
DSE_CAPI int   dse_scene_is_sub_loaded(const char* path);
// 已加载子场景完整路径以 '\n' 分隔写入 out，返回条数。
DSE_CAPI int   dse_scene_get_loaded_subs(char* out, int cap);
DSE_CAPI int   dse_scene_get_sub_count(void);
DSE_CAPI int   dse_scene_get_pending_count(void);
DSE_CAPI void  dse_scene_transition_to(const char* path, int mode, float fade_duration);
DSE_CAPI int   dse_scene_get_transition_state(void);
DSE_CAPI float dse_scene_get_fade_progress(void);
DSE_CAPI int   dse_scene_get_active(char* out, int cap);

// ============================================================
// UUIDComponent（跨场景稳定引用）
// ============================================================
// uuid 均为 16 位十六进制字符串。get：无组件或 uuid==0 返回 0。
// set：uuid_str=null 时自动生成，最终字符串写入 out。resolve：失败返回 0xFFFFFFFF。

DSE_CAPI int      dse_uuid_get(uint32_t e, char* out, int cap);
DSE_CAPI int      dse_uuid_set(uint32_t e, const char* uuid_str, char* out, int cap);
DSE_CAPI uint32_t dse_uuid_resolve(const char* uuid_str);

// ============================================================

// UI — 扩展控件（纯 ECS 组件操作）
// ============================================================

DSE_CAPI void  dse_ui_add_label(uint32_t e, const char* text, uint32_t font_tex_handle,
                                 float r, float g, float b, float a,
                                 float glyph_w, float glyph_h, float spacing,
                                 int atlas_cols, int atlas_rows, int ascii_start,
                                 float offset_x, float offset_y);
DSE_CAPI void  dse_ui_set_label_number(uint32_t e, long long number);
DSE_CAPI void  dse_ui_set_label_layout(uint32_t e, float max_width, int align,
                                        int overflow, int max_lines, float line_spacing);
DSE_CAPI void  dse_ui_add_mask(uint32_t e, float w, float h, float ox, float oy, int block_outside);
DSE_CAPI void  dse_ui_add_rich_text(uint32_t e, const char* text,
                                     float r, float g, float b, float a, int shadow, int outline);
DSE_CAPI void  dse_ui_set_rich_text(uint32_t e, const char* text);
DSE_CAPI void  dse_ui_set_button_scale(uint32_t e, float hover, float pressed, float lerp_speed);
DSE_CAPI void  dse_ui_set_uv(uint32_t e, float u, float v, float w, float h);
DSE_CAPI void  dse_ui_set_nine_slice(uint32_t e, int enabled, float l, float b, float r, float t,
                                      float sw, float sh);
DSE_CAPI void  dse_ui_add_anchor(uint32_t e, int anchor_type, float ox, float oy);
DSE_CAPI void  dse_ui_set_anchor_type(uint32_t e, int anchor_type);
DSE_CAPI void  dse_ui_set_anchor_offset(uint32_t e, float ox, float oy);
DSE_CAPI void  dse_ui_add_grid_layout(uint32_t e, int columns, float cw, float ch,
                                       float sx, float sy);
DSE_CAPI void  dse_ui_set_grid_layout(uint32_t e, int columns, int rows,
                                       float cw, float ch, float sx, float sy, int alignment);
DSE_CAPI void  dse_ui_add_canvas_scaler(uint32_t e, float ref_w, float ref_h, int match);
DSE_CAPI void  dse_ui_set_canvas_scaler(uint32_t e, float ref_w, float ref_h,
                                         float scale_factor, int match_wh, float match, int pixel_snap);
DSE_CAPI void  dse_ui_add_box_layout(uint32_t e, int vertical, float spacing,
                                      float pad_x, float pad_y, int align_main, int align_cross, int reverse);
DSE_CAPI void  dse_ui_set_box_layout(uint32_t e, int vertical, float spacing,
                                      float pad_x, float pad_y, int align_main, int align_cross, int reverse);
DSE_CAPI void  dse_ui_add_content_size_fitter(uint32_t e, int fit_w, int fit_h,
                                                float min_w, float min_h, float max_w, float max_h);
DSE_CAPI void  dse_ui_add_animation(uint32_t e, float duration, int easing,
                                     int loop, int ping_pong, float delay);
DSE_CAPI void  dse_ui_animate_position(uint32_t e, float tx, float ty);
DSE_CAPI void  dse_ui_animate_scale(uint32_t e, float sx, float sy);
DSE_CAPI void  dse_ui_animate_alpha(uint32_t e, float alpha);
DSE_CAPI void  dse_ui_animate_color(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_stop_animation(uint32_t e);
DSE_CAPI void  dse_ui_set_text_input_placeholder(uint32_t e, const char* placeholder);
DSE_CAPI void  dse_ui_add_scroll_view(uint32_t e, float cw, float ch, int horizontal, int vertical);
DSE_CAPI void  dse_ui_set_scroll_offset(uint32_t e, float x, float y);
DSE_CAPI void  dse_ui_get_scroll_offset(uint32_t e, float* out_x, float* out_y);
DSE_CAPI void  dse_ui_set_scroll_content_size(uint32_t e, float w, float h);
DSE_CAPI void  dse_ui_set_slider_colors(uint32_t e, float tr, float tg, float tb, float ta,
                                         float fr, float fg, float fb, float fa,
                                         float hr, float hg, float hb, float ha);
DSE_CAPI void  dse_ui_set_slider_handle_size(uint32_t e, float size);
DSE_CAPI void  dse_ui_set_slider_vertical(uint32_t e, int vertical);
DSE_CAPI void  dse_ui_set_slider_range(uint32_t e, float min_val, float max_val);
DSE_CAPI void  dse_ui_add_dropdown(uint32_t e, float item_height, int max_visible);
DSE_CAPI void  dse_ui_dropdown_add_option(uint32_t e, const char* text, const char* value);
DSE_CAPI void  dse_ui_dropdown_clear_options(uint32_t e);
DSE_CAPI void  dse_ui_set_dropdown_index(uint32_t e, int index);
DSE_CAPI int   dse_ui_get_dropdown_index(uint32_t e);
DSE_CAPI int   dse_ui_get_dropdown_value(uint32_t e, char* out, int cap);
DSE_CAPI void  dse_ui_set_dropdown_open(uint32_t e, int open);
DSE_CAPI void  dse_ui_add_filled_image(uint32_t e, float fill_amount, int method, int origin, int clockwise);
DSE_CAPI void  dse_ui_set_fill_amount(uint32_t e, float amount);
DSE_CAPI float dse_ui_get_fill_amount(uint32_t e);
DSE_CAPI void  dse_ui_set_fill_method(uint32_t e, int method, int origin, int clockwise);
DSE_CAPI void  dse_ui_add_focus_navigable(uint32_t e, int tab_index);
DSE_CAPI void  dse_ui_set_focus_nav(uint32_t e, uint32_t up, uint32_t down, uint32_t left, uint32_t right);
DSE_CAPI int   dse_ui_is_focused(uint32_t e);
DSE_CAPI void  dse_ui_set_focus_tint(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_add_event_propagation(uint32_t e, int bubbles_click, int bubbles_hover);
DSE_CAPI void  dse_ui_stop_propagation(uint32_t e);
DSE_CAPI void  dse_ui_add_visual_effect(uint32_t e);
DSE_CAPI void  dse_ui_set_corner_radius(uint32_t e, float radius);
DSE_CAPI void  dse_ui_set_gradient(uint32_t e, float sr, float sg, float sb, float sa,
                                    float er, float eg, float eb, float ea, int direction);
DSE_CAPI void  dse_ui_set_blur(uint32_t e, float radius, float intensity);
DSE_CAPI void  dse_ui_add_virtual_scroll(uint32_t e, int total_items, float item_height);
DSE_CAPI void  dse_ui_set_virtual_scroll_count(uint32_t e, int count);
DSE_CAPI void  dse_ui_get_virtual_scroll_range(uint32_t e, int* out_start, int* out_end);
DSE_CAPI void  dse_ui_destroy_virtual_scroll(uint32_t e);

// ============================================================

// Audio LOD
DSE_CAPI void dse_audio_lod_init(float full_distance, int max_active_sources);
DSE_CAPI uint32_t dse_audio_lod_register_source(const char* path,
                                                float x, float y, float z,
                                                float max_distance, float priority);
DSE_CAPI void dse_audio_lod_tick(float lx, float ly, float lz, float dt);
// stats：out={full,reduced,virtual,culled}（int[4]）。返回 1=系统已初始化。
DSE_CAPI int  dse_audio_lod_get_stats(int* out_stats);
DSE_CAPI int  dse_audio_lod_is_audible(uint32_t source_id);
DSE_CAPI void dse_audio_lod_shutdown(void);

// 释放所有 P2-P5 大世界系统实例。
DSE_CAPI void dse_open_world_p2p5_shutdown(void);

// ============================================================



#ifdef __cplusplus
}
#endif

#endif // DSE_API_SERVICES_H
