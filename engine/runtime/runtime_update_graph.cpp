#include "engine/runtime/runtime_update_graph.h"

#include "engine/runtime/frame_pipeline.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"

#ifdef DSE_ENABLE_3D
#include "engine/physics/physics3d/physics3d_system.h"
#endif

#include <glm/vec2.hpp>

namespace dse::runtime {

void RunRuntimeUpdateGraph(::FramePipeline& pipeline, float delta_time) {
    auto& world = pipeline.world();

    // 2D 模块统一通过 IModule 接口调度
    pipeline.gameplay2d_module_.OnUpdate(world, delta_time);

    for (auto& mod : pipeline.modules_) {
        if (mod.instance) {
            mod.instance->OnUpdate(world, delta_time);
        }
    }
}

void RunRuntimeFixedUpdateGraph(::FramePipeline& pipeline, float fixed_delta_time) {
    auto& world = pipeline.world();

    // 2D 模块统一通过 IModule 接口调度
    pipeline.gameplay2d_module_.OnFixedUpdate(world, fixed_delta_time);

    for (auto& mod : pipeline.modules_) {
        if (mod.instance) {
            mod.instance->OnFixedUpdate(world, fixed_delta_time);
        }
    }

#ifdef DSE_ENABLE_3D
    // PhysX 3D 物理固定步长更新
    pipeline.physics3d_system_.FixedUpdate(world, fixed_delta_time);
#endif
}

} // namespace dse::runtime
