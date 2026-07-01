/**
 * @file frame_pipeline_impl.h
 * @brief FramePipeline::RenderState definition (Pimpl).
 *        Include only in .cpp files that need access to the heavy render subsystem members.
 */

#ifndef DSE_FRAME_PIPELINE_IMPL_H
#define DSE_FRAME_PIPELINE_IMPL_H

#include "engine/runtime/frame_pipeline.h"

#include "engine/scene/transform_system.h"
#include "engine/render/mesh_renderer.h"
#include "engine/render/render_scene_view.h"
#include "engine/render/pipeline/render_pipeline_profile.h"
#include "engine/render/render_snapshot.h"
#include "engine/render/light_buffer.h"
#include "engine/render/cluster_grid.h"
#include "engine/render/light_probe_system.h"
#include "engine/render/reflection_probe_system.h"
#include "engine/render/gi/ddgi_system.h"
#include "engine/render/skinning/gpu_skinning.h"
#include "engine/assets/streaming_manager.h"
#include "engine/ecs/floating_origin_system.h"
#include "engine/core/event_bus.h"
#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/render_profiler.h"
#include "engine/profiler/memory_profiler.h"

// Shared helpers used by frame_pipeline*.cpp split files
class AssetManager;
struct RenderTargetReadback;

void LogReadbackStats(const char* label, const RenderTargetReadback& readback);
void LogDefaultFramebufferStats();
AssetManager& RequireAssetManager(AssetManager* asset_manager);
bool IsProfilePassEnabled(const dse::render::RenderPipelineProfile& profile, const std::string& name);
float PipelineValueToFloat(const dse::render::PipelineValue* value, float fallback);

struct FramePipeline::RenderState {
    dse::render::RenderPipelineProfile render_pipeline_profile_;
    dse::render::RenderScene render_scene_;
    TransformSystem transform_system_;
    dse::render::RenderSceneView scene_view_;
    dse::render::LightBuffer light_buffer_;
    dse::render::ClusterGrid cluster_grid_;
    dse::render::LightProbeSystem light_probe_system_;
    dse::render::ReflectionProbeSystem reflection_probe_system_;
    dse::render::gi::DDGISystem ddgi_system_;
    dse::render::GPUSkinningSystem gpu_skinning_system_;
    dse::FloatingOriginSystem floating_origin_system_;
    dse::core::SubscriptionHandle origin_rebase_handle_;
    dse::streaming::StreamingManager streaming_manager_;
    dse::render::MeshRenderer cpu_mesh_renderer_;
    dse::render::RenderThinSnapshot snapshot_pool_[2];
    dse::profiler::CPUProfiler cpu_profiler_;
    dse::profiler::RenderProfiler render_profiler_;
    dse::profiler::MemoryProfiler memory_profiler_;
};

#endif
