#include "engine/runtime/runtime_update_graph.h"

#include "engine/runtime/frame_pipeline.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"

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
}

} // namespace dse::runtime
