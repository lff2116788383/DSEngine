/**
 * @file dse_api.h
 * @brief DSEngine Native C ABI — Lua 与 C# 共享的底层引擎接口
 *
 * 纯 C 函数导出，消除 Lua / C# 两套绑定的重复逻辑。
 * C# 侧通过 Mono InternalCall 或 P/Invoke 调用。
 * Lua 侧 lua_binding_ecs_*.cpp 逐步迁移为调用本层函数。
 *
 * 本文件为聚合头文件，按模块拆分为：
 *   dse_api_core.h      — Entity/Transform/ECS/Input/App/Metrics
 *   dse_api_render.h    — Camera/Mesh/Light/PostProcess/Particles
 *   dse_api_physics.h   — Physics 3D/2D/Colliders/Joints
 *   dse_api_world.h     — Terrain/Water/Weather/Scene/Streaming/OpenWorld
 *   dse_api_services.h  — Audio/Localization/UI/Navigation
 *   dse_api_gameplay.h  — Gameplay3D/Animation
 *
 * 修改单个模块只需编辑对应的 dse_api_<module>.h。
 */

#ifndef DSE_API_H
#define DSE_API_H

#include <stdint.h>

#ifdef _WIN32
#  define DSE_CAPI __declspec(dllexport)
#else
#  define DSE_CAPI __attribute__((visibility("default")))
#endif

// API Version — must be visible before module includes
#define DSE_API_VERSION 10000u   /* v1.0.0 — MMNNPP format */

#ifdef __cplusplus
extern "C" {
#endif

// ── Module includes ──────────────────────────────────────────────
#include "dse_api_core.h"
#include "dse_api_render.h"
#include "dse_api_physics.h"
#include "dse_api_world.h"
#include "dse_api_services.h"
#include "dse_api_gameplay.h"


#ifdef __cplusplus
}
#endif

#endif // DSE_API_H
