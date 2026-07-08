/**
 * @file dse_api_gameplay.h
 * @brief DSEngine Native C ABI — Gameplay3D (Fracture/Cloth/Fluid/Vehicle/Animation) module
 *
 * Split from dse_api.h for maintainability. Include dse_api.h for all modules.
 */

#ifndef DSE_API_GAMEPLAY_H
#define DSE_API_GAMEPLAY_H

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

// Physics LOD
DSE_CAPI void dse_physics_lod_init(float full_distance, float reduced_distance,
                                   float simplified_distance);
DSE_CAPI void dse_physics_lod_register_body(uint32_t entity_id,
                                            float x, float y, float z, float radius);
// evaluate：激活 body 的 entity_id 写入 out_ids（最多 cap 个），返回激活总数。
// out_ids=null 时仅返回总数。
DSE_CAPI int  dse_physics_lod_evaluate(float cam_x, float cam_y, float cam_z,
                                       uint32_t frame, uint32_t* out_ids, int cap);
// stats：out={full,reduced,simplified,sleeping}（int[4]）。返回 1=系统已初始化。
DSE_CAPI int  dse_physics_lod_get_stats(int* out_stats);
DSE_CAPI void dse_physics_lod_wake(uint32_t entity_id);
DSE_CAPI void dse_physics_lod_sleep(uint32_t entity_id);
DSE_CAPI void dse_physics_lod_shutdown(void);




#ifdef __cplusplus
}
#endif

#endif // DSE_API_GAMEPLAY_H
