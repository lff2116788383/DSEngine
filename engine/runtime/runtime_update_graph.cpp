#include "engine/runtime/runtime_update_graph.h"

#include "engine/runtime/frame_pipeline.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"

#include <glm/vec2.hpp>

namespace dse::runtime {

void RunRuntimeUpdateGraph(::FramePipeline& pipeline, float delta_time) {
    auto& world = pipeline.world();

    pipeline.tilemap_system_.Update(world.registry());
    pipeline.animation_system_.Update(world, delta_time);
    pipeline.particle_system_.Update(world, delta_time, &pipeline.physics2d_system_);
    pipeline.spine_system_.Update(world.registry(), delta_time);
    pipeline.transform_system_.Update(world);
    pipeline.ui_logic_system_.Update(world.registry(),
                                     delta_time,
                                     glm::vec2(Screen::width(), Screen::height()),
                                     Input::mousePosition(),
                                     Input::GetMouseButton(0));
    pipeline.camera_system_.Update(world, Screen::aspect_ratio());
    pipeline.audio_system_.Update(world.registry(), delta_time);

    for (auto& mod : pipeline.modules_) {
        if (mod.instance) {
            mod.instance->OnUpdate(world, delta_time);
        }
    }
}

void RunRuntimeFixedUpdateGraph(::FramePipeline& pipeline, float fixed_delta_time) {
    auto& world = pipeline.world();
    pipeline.physics2d_system_.FixedUpdate(world, fixed_delta_time);

    for (auto& mod : pipeline.modules_) {
        if (mod.instance) {
            mod.instance->OnFixedUpdate(world, fixed_delta_time);
        }
    }
}

} // namespace dse::runtime
