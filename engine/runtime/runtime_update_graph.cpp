#include "engine/runtime/runtime_update_graph.h"

#include "engine/runtime/frame_pipeline.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"

#ifdef DSE_ENABLE_3D
#if defined(DSE_ENABLE_PHYSX)
#include "engine/physics/physics3d/physics3d_system.h"
#endif
#endif

#include <glm/vec2.hpp>

namespace dse::runtime {

void RunRuntimeUpdateGraph(::FramePipeline& pipeline, float delta_time) {
    auto& world = pipeline.world();

    // 2D 模块统一通过 IBuiltinModules 接口调度
    pipeline.modules_impl_->UpdateGameplay2D(world, delta_time);

    for (auto& mod : pipeline.modules_) {
        if (mod.instance) {
            mod.instance->OnUpdate(world, delta_time);
        }
    }
#ifdef DSE_ENABLE_3D
    if (pipeline.builtin_gameplay3d_enabled_) {
        pipeline.modules_impl_->UpdateGameplay3D(world, delta_time);
    }
#endif
}

void RunRuntimeFixedUpdateGraph(::FramePipeline& pipeline, float fixed_delta_time) {
    auto& world = pipeline.world();

    // 2D 模块统一通过 IBuiltinModules 接口调度
    pipeline.modules_impl_->FixedUpdateGameplay2D(world, fixed_delta_time);

    for (auto& mod : pipeline.modules_) {
        if (mod.instance) {
            mod.instance->OnFixedUpdate(world, fixed_delta_time);
        }
    }
#ifdef DSE_ENABLE_3D
    if (pipeline.builtin_gameplay3d_enabled_) {
        pipeline.modules_impl_->FixedUpdateGameplay3D(world, fixed_delta_time);
    }
#endif

#ifdef DSE_ENABLE_3D
#if defined(DSE_ENABLE_PHYSX)
    // PhysX 3D 物理固定步长更新
    pipeline.physics3d_system_.FixedUpdate(world, fixed_delta_time);
#endif
#endif
}

} // namespace dse::runtime
