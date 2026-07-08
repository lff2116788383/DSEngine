/**
 * @file dse_api_world.h
 * @brief DSEngine Native C ABI — World (Terrain/Water/Weather/Scene/Streaming/OpenWorld) module
 *
 * Split from dse_api.h for maintainability. Include dse_api.h for all modules.
 */

#ifndef DSE_API_WORLD_H
#define DSE_API_WORLD_H

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

// Streaming
// ============================================================

DSE_CAPI uint32_t dse_streaming_create_zone(const char* name, float x, float y, float z,
                                       float load_r, float unload_r);
DSE_CAPI void  dse_streaming_destroy_zone(uint32_t zone);
DSE_CAPI void  dse_streaming_add_asset(uint32_t zone, const char* path, const char* type_str);
DSE_CAPI void  dse_streaming_add_assets(uint32_t zone, const char* const* paths, int count,
                                        const char* type_str);
DSE_CAPI void  dse_streaming_set_zone_center(uint32_t zone, float x, float y, float z);
DSE_CAPI void  dse_streaming_force_load(uint32_t zone);
DSE_CAPI void  dse_streaming_force_unload(uint32_t zone);
DSE_CAPI int   dse_streaming_get_zone_state(uint32_t zone);
DSE_CAPI float dse_streaming_get_zone_progress(uint32_t zone);
DSE_CAPI void  dse_streaming_set_budget(int max_loads_per_frame, int max_concurrent);
DSE_CAPI int   dse_streaming_get_active_loads(void);
DSE_CAPI int   dse_streaming_get_zone_count(void);

// ============================================================

// Open World P2-P5（Mesh Streaming / Physics LOD / Terrain Deform / Audio LOD）
// ============================================================
// 系统实例由本层管理（单例）。浮点参数 NaN=使用默认值，整型参数 <0=使用默认值。


// Mesh Streaming
DSE_CAPI void dse_mesh_streaming_init(float hysteresis, int load_budget_per_frame);
DSE_CAPI uint32_t dse_mesh_streaming_register_mesh(const char* name,
                                                   float x, float y, float z, float radius);
DSE_CAPI void dse_mesh_streaming_add_lod(uint32_t mesh_id, uint32_t level, const char* path,
                                         float distance, uint32_t triangle_count);
DSE_CAPI void dse_mesh_streaming_tick(float cam_x, float cam_y, float cam_z, float dt);
DSE_CAPI int  dse_mesh_streaming_get_current_lod(uint32_t mesh_id);
DSE_CAPI int  dse_mesh_streaming_get_mesh_count(void);
DSE_CAPI void dse_mesh_streaming_shutdown(void);


// Cutscene — 过场/导演系统（实例句柄由本层管理）
// ============================================================

// 相机轨道应用回调。
typedef void (*dse_cutscene_camera_fn)(float px, float py, float pz,
                                       float lx, float ly, float lz,
                                       float fov, void* user_data);
// 事件轨道触发回调。
typedef void (*dse_cutscene_event_fn)(const char* event_name, const char* payload,
                                      void* user_data);
// 序列播放完成回调。
typedef void (*dse_cutscene_finish_fn)(const char* seq_name, void* user_data);

DSE_CAPI int   dse_cutscene_create(void);
DSE_CAPI void  dse_cutscene_destroy(int player_id);
DSE_CAPI void  dse_cutscene_shutdown(void);

DSE_CAPI void  dse_cutscene_add_sequence(int player_id, const char* name, float duration);
DSE_CAPI void  dse_cutscene_remove_sequence(int player_id, const char* name);

DSE_CAPI void  dse_cutscene_add_camera_keyframe(int player_id, const char* seq_name, float time,
                                                float px, float py, float pz,
                                                float lx, float ly, float lz, float fov);
// interp: 0=Linear 1=Step 2=CubicBezier。
DSE_CAPI void  dse_cutscene_add_property_keyframe(int player_id, const char* seq_name,
                                                  const char* track_name, float time,
                                                  float value, int interp);
DSE_CAPI void  dse_cutscene_add_event(int player_id, const char* seq_name, float time,
                                      const char* event_name, const char* payload);
DSE_CAPI void  dse_cutscene_add_audio_cue(int player_id, const char* seq_name, float time,
                                          const char* path, float volume, int loop);

DSE_CAPI void  dse_cutscene_play(int player_id, const char* seq_name);
DSE_CAPI void  dse_cutscene_pause(int player_id);
DSE_CAPI void  dse_cutscene_resume(int player_id);
DSE_CAPI void  dse_cutscene_stop(int player_id);
DSE_CAPI void  dse_cutscene_seek(int player_id, float time);
DSE_CAPI float dse_cutscene_get_time(int player_id);
// 返回 0=stopped 1=playing 2=paused。
DSE_CAPI int   dse_cutscene_get_state(int player_id);
DSE_CAPI void  dse_cutscene_set_play_rate(int player_id, float rate);
DSE_CAPI void  dse_cutscene_update(int player_id, float dt);

DSE_CAPI void  dse_cutscene_set_camera_callback(int player_id, const char* seq_name,
                                                dse_cutscene_camera_fn fn, void* user_data);
DSE_CAPI void  dse_cutscene_set_event_callback(int player_id, const char* seq_name,
                                               dse_cutscene_event_fn fn, void* user_data);
DSE_CAPI void  dse_cutscene_set_finish_callback(int player_id,
                                                dse_cutscene_finish_fn fn, void* user_data);

// ============================================================
// AI（行为树 + GOAP 规划器）
// ============================================================

// 条件回调：返回 0=失败 1=成功。
typedef int (*dse_ai_condition_fn)(void* user_data);
// 动作回调：返回 0=failure 1=success 2=running。
typedef int (*dse_ai_action_fn)(float dt, void* user_data);
// user_data 释放回调（节点销毁时调用，可为 NULL）。
typedef void (*dse_ai_destroy_fn)(void* user_data);

DSE_CAPI int   dse_ai_tree_create(const char* name);
DSE_CAPI void  dse_ai_tree_destroy(int tree_id);
// 返回 0=failure 1=success 2=running。
DSE_CAPI int   dse_ai_tree_tick(int tree_id, float dt);
DSE_CAPI void  dse_ai_tree_reset(int tree_id);
DSE_CAPI void  dse_ai_shutdown(void);

// 黑板
DSE_CAPI void  dse_ai_bb_set_bool(int tree_id, const char* key, int v);
DSE_CAPI void  dse_ai_bb_set_int(int tree_id, const char* key, int v);
DSE_CAPI void  dse_ai_bb_set_float(int tree_id, const char* key, float v);
DSE_CAPI void  dse_ai_bb_set_string(int tree_id, const char* key, const char* v);
DSE_CAPI void  dse_ai_bb_set_vec3(int tree_id, const char* key, float x, float y, float z);
DSE_CAPI int   dse_ai_bb_get_bool(int tree_id, const char* key);
DSE_CAPI int   dse_ai_bb_get_int(int tree_id, const char* key);
DSE_CAPI float dse_ai_bb_get_float(int tree_id, const char* key);
DSE_CAPI int   dse_ai_bb_get_string(int tree_id, const char* key, char* out, int cap);
DSE_CAPI int   dse_ai_bb_get_vec3(int tree_id, const char* key, float* out_xyz);

// 树构建（栈式）
DSE_CAPI void  dse_ai_begin_sequence(int tree_id, const char* name);
DSE_CAPI void  dse_ai_begin_selector(int tree_id, const char* name);
// require_one: 0=RequireAll 1=RequireOne。
DSE_CAPI void  dse_ai_begin_parallel(int tree_id, int require_one, const char* name);
DSE_CAPI void  dse_ai_end_composite(int tree_id);
DSE_CAPI void  dse_ai_add_condition(int tree_id, const char* name, dse_ai_condition_fn fn,
                                    void* user_data, dse_ai_destroy_fn destroy);
DSE_CAPI void  dse_ai_add_action(int tree_id, const char* name, dse_ai_action_fn fn,
                                 void* user_data, dse_ai_destroy_fn destroy);
DSE_CAPI void  dse_ai_add_inverter(int tree_id, const char* name);
DSE_CAPI void  dse_ai_add_succeeder(int tree_id, const char* name);
// max_repeats<0 = 无限。
DSE_CAPI void  dse_ai_add_repeater(int tree_id, const char* name, int max_repeats);




#ifdef __cplusplus
}
#endif

#endif // DSE_API_WORLD_H
