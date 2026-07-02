#include "engine/runtime/runtime_update_graph.h"

#include "engine/runtime/frame_pipeline_impl.h"
#include "engine/runtime/i_builtin_modules.h"
#include "engine/core/module.h"
#include "engine/core/service_locator.h"
#include "engine/core/event_bus.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"

#ifdef DSE_ENABLE_3D
  #include "engine/physics/physics3d/i_physics3d_system.h"
#endif

#include <glm/vec2.hpp>

namespace dse::runtime {

void RunRuntimeUpdateGraph(::FramePipeline& pipeline, const dse::FrameUpdateContext& frame) {
    auto& world = pipeline.world();

    // 2D 模块统一通过 IBuiltinModules 接口调度
    pipeline.modules_impl_->UpdateGameplay2D(world, frame);

    for (auto& mod : pipeline.modules_) {
        if (mod.instance) {
            // 外部插件 IModule::OnUpdate 保持 float 签名，传入缩放后 dt
            mod.instance->OnUpdate(world, frame.time.scaled_dt);
        }
    }
    if (pipeline.builtin_gameplay3d_enabled_) {
        pipeline.modules_impl_->UpdateGameplay3D(world, frame);
    }
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
    if (pipeline.builtin_gameplay3d_enabled_) {
        pipeline.modules_impl_->FixedUpdateGameplay3D(world, fixed_delta_time);
    }

#ifdef DSE_ENABLE_3D
    // Floating Origin: 物理更新前检查是否需要 rebase
    {
        dse::physics3d::IPhysics3DSystem* phys = pipeline.physics3d_system_.get();
        auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>();
        pipeline.rs_->floating_origin_system_.Tick(world, phys, event_bus);
    }

    if (pipeline.physics3d_system_) {
        pipeline.physics3d_system_->FixedUpdate(world, fixed_delta_time);
    }
#endif
}

} // namespace dse::runtime
