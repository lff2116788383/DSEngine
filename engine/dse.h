/**
 * @file dse.h
 * @brief DSEngine 聚合头文件 — 一次引入所有公开 API。
 *
 * 用法：
 * @code
 * #include "engine/dse.h"
 * @endcode
 */

#ifndef DSE_H
#define DSE_H

// Version
#include "engine/dse_version.h"

// Core
#include "engine/core/service_locator.h"
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/job_system.h"
#include "engine/core/module.h"
#include "engine/core/object_pool.h"
#include "engine/core/memory/memory.h"
#include "engine/core/memory/pool_allocator.h"
#include "engine/core/dynamic_library.h"

// Base
#include "engine/base/debug.h"
#include "engine/base/time.h"
#include "engine/base/tween.h"
#include "engine/base/bezier.h"

// ECS
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/gameplay.h"
#include "engine/ecs/particle_2d.h"
#include "engine/ecs/physics_2d.h"
#include "engine/ecs/script.h"
#include "engine/ecs/tilemap.h"
#include "engine/ecs/ui.h"
#include "engine/ecs/uuid_component.h"

// Input
#include "engine/input/input.h"
#include "engine/input/action_mapping.h"
#include "engine/input/input_recorder.h"
#include "engine/input/key_code.h"

// Physics
#include "engine/physics/physics2d/physics2d_system.h"

// Render
#include "engine/render/render_graph.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

// Scene
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/transform_system.h"

// Assets
#include "engine/assets/asset_manager.h"

// Audio
#include "engine/audio/audio_system.h"

// Runtime
#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/runtime_context.h"
#include "engine/runtime/runtime_services.h"

// Platform
#include "engine/platform/screen.h"

// Profiler
#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/memory_profiler.h"
#include "engine/profiler/render_profiler.h"

#endif // DSE_H
