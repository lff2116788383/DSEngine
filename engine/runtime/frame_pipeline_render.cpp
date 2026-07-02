/**
 * @file frame_pipeline_render.cpp
 * @brief FramePipeline render path â€” RunRenderInternal, BuildRenderGraph, ExecuteRenderGraph.
 */

#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/frame_pipeline_impl.h"
#include "engine/runtime/i_builtin_modules.h"
#include "engine/core/module.h"
#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"
#include "engine/core/job_system.h"
#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/builtin_passes.h"
#include "engine/render/hiz_types.h"
#include "engine/render/rhi/rhi_factory.h"
#include "engine/render/shaders/generated/embed/hi_z_copy_comp.gen.h"
#include "engine/render/shaders/generated/embed/hi_z_downsample_comp.gen.h"
#include "engine/render/shaders/generated/embed/hi_z_cull_comp.gen.h"
#include "engine/render/shaders/generated/embed/gpu_cull_comp.gen.h"
#include "engine/base/debug.h"
#include "engine/base/time.h"
#include "engine/platform/screen.h"
#include "engine/input/input.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/components_3d.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iostream>

void FramePipeline::RunRenderInternal() {
    dse::profiler::ScopedCPUProfile _profile_render(rs_->cpu_profiler_, "FramePipeline::Render");

    // Update phase: all ECS reads -> scene_view / thin snapshot / lights / cluster / render queues
    PrepareRenderFrame();

    // Render phase: consumes only the snapshot and pre-extracted data, never touches World
    ExecuteRenderFrame();

    CollectRuntimeStats();
}

void FramePipeline::PrepareGPUSceneAndQueues() {
    render_pass_context_.gpu_driven_scene_prepared = false;
    render_pass_context_.gpu_driven_active_this_frame = false;
    modules_impl_->ResetGPUSceneState();

    const bool allow_external_modules =
        gpu_driven_policy_ == GpuDrivenPolicy::WithModules ||
        gpu_driven_policy_ == GpuDrivenPolicy::Force;
    const bool gpu_scene_provider_available = modules_.empty() || allow_external_modules;
    const bool can_prepare_gpu_scene = render_resources_.gpu_driven_supported
        && gpu_driven_requested_ && gpu_scene_provider_available;
    if (can_prepare_gpu_scene && runtime_context_.world && !render_thread_active_.load()) {
        const int prepared = modules_impl_->PrepareGPUScene(*runtime_context_.world, render_pass_context_);
        render_pass_context_.gpu_driven_scene_prepared = prepared > 0;
        render_pass_context_.gpu_driven_active_this_frame =
            prepared > 0 && render_pass_context_.gpu_mega_vao && render_pass_context_.gpu_draw_cmd_ssbo;
        render_resources_.gpu_draw_cmd_ssbo = render_pass_context_.gpu_draw_cmd_ssbo;
        render_resources_.gpu_instance_ssbo = render_pass_context_.gpu_instance_ssbo;
        render_resources_.gpu_material_ssbo = render_pass_context_.gpu_material_ssbo;
        render_resources_.gpu_aabb_ssbo = render_pass_context_.gpu_aabb_ssbo;
        render_resources_.gpu_aabb_capacity = render_pass_context_.gpu_aabb_capacity;
    }
    if (gpu_driven_diag_) {
        DEBUG_LOG_INFO("[GpuDriven] requested={} supported={} provider={} prepared={} active={} draws={} instances={} modules={} builtin_gameplay3d={}",
                       render_pass_context_.gpu_driven_requested,
                       render_pass_context_.gpu_driven_supported,
                       gpu_scene_provider_available,
                       render_pass_context_.gpu_driven_scene_prepared,
                       render_pass_context_.gpu_driven_active_this_frame,
                       render_pass_context_.gpu_indirect_draw_count,
                       render_pass_context_.gpu_total_instances,
                       modules_.size(),
                       builtin_gameplay3d_enabled_);
    }

    // GPU Compute Skinning: å¸§å¼€å§‹ï¼ˆæ¸…ç©ºä¸Šä¸€å¸§è¯·æ±‚ï¼Œè¯»å›žä¸Šä¸€å¸§ç»“æžœï¼‰
    if (rs_->gpu_skinning_system_.IsAvailable()) {
        rs_->gpu_skinning_system_.BeginFrame();
    }

    BuildRenderSceneQueues();

    // ===== B-1ï¼ˆæ–¹æ¡ˆ Bï¼‰ï¼šweb è’™çš®å¯è§æ¿€æ´» â€” GPU compute + å¤ç”¨å¼‚æ­¥å›žè¯» â†’ ä¸–ç•Œçƒ˜ç„™é™æ€ç½‘æ ¼ =====
    // åœ¨ç¼º ForwardSkinnedShaded å†…å»ºç¨‹åºçš„åŽç«¯ï¼ˆWebGPU æ—  per-draw è’™çš®ï¼›WebGL2/GLES3.0 æ— æ³•ç¼–è¯‘
    // SSBO è’™çš® VSï¼‰ä¸Šï¼ŒDrawSkinnedShaded ä¸º no-opã€è’™çš®ç½‘æ ¼ä¸å¯è§ã€‚æ­¤å¤„æŠŠè’™çš®é¡¹å–‚ GPU compute
    // è’™çš®ç³»ç»Ÿï¼ˆWebGPU ä¸Š GPU çœŸåšè’™çš®ï¼‰ï¼Œæ¶ˆè´¹ã€Œä¸Šä¸€å¸§ã€å¼‚æ­¥å›žè¯»ç»“æžœï¼ˆgrass ä¹‹å¤–ç¬¬ 2 ä¸ªçœŸå®žå›žè¯»
    // æ¶ˆè´¹æ–¹ï¼‰ï¼Œæœªå°±ç»ª/WebGL2 åˆ™ CPU è’™çš®å›žé€€ï¼ˆåŒå…¬å¼ï¼Œç»“æžœä¸€è‡´ï¼‰ï¼Œçƒ˜ç„™ä¸ºå¯¹è±¡ç©ºé—´é¡¶ç‚¹çš„éžè’™çš®é¡¹ï¼Œ
    // èµ°çŽ°æœ‰ ForwardShaded è·¯å¾„ï¼ˆé›¶ç€è‰²å™¨/ç®¡çº¿æ”¹åŠ¨ï¼‰ã€‚æ¡Œé¢ï¼ˆç¨‹åºå¯ç”¨ï¼‰ä¸è§¦å‘ï¼Œè¡Œä¸ºä¸å˜ã€‚
    // æ³¨æ„ï¼šmesh_render_system æŠŠè’™çš®é¡¹ï¼ˆitem.skinned=trueï¼‰æ”¾è¿› cpu_meshes.opaque/transparent é˜Ÿåˆ—ï¼Œ
    // è€Œéžç‹¬ç«‹çš„ skinned é˜Ÿåˆ—ï¼ˆåŽè€…ä»Žæœªè¢«å¡«å……ï¼‰ã€‚æ•…æ­¤å¤„å°±åœ°æ‰«æè¿™ä¸¤ä¸ªé˜Ÿåˆ—é‡Œçš„è’™çš®é¡¹å¤„ç†ã€‚
    if (!skinning_bake_checked_) {
        skinning_bake_for_web_ = (runtime_context_.rhi_device->GetBuiltinProgram(
            BuiltinProgram::ForwardSkinnedShaded) == 0);
        skinning_bake_checked_ = true;
    }
    if (skinning_bake_for_web_) {
        const bool gpu_skin = rs_->gpu_skinning_system_.IsAvailable();
        // CPU è’™çš®å›žé€€ï¼šé€å¥é•œåƒ compute è’™çš®ï¼ˆ4 æƒé‡ï¼Œç¬¬ 4 æƒé‡ = 1 - w0 - w1 - w2ï¼›
        // æ³•çº¿/åˆ‡çº¿ç”¨ mat3(skin)ï¼‰ï¼Œä¿è¯ä¸Ž GPU å›žè¯»è·¯å¾„æ•°å€¼ä¸€è‡´ â†’ æš–æœº/WebGL2 æ— ç ´å¸§ã€‚
        auto skin_like_compute = [](const BatchVertex& bv,
                                    const std::vector<glm::mat4>& bones,
                                    glm::vec3& out_pos, glm::vec3& out_nrm, glm::vec3& out_tan) {
            const float bw0 = bv.weights[0], bw1 = bv.weights[1], bw2 = bv.weights[2];
            const float bw3 = 1.0f - bw0 - bw1 - bw2;
            const int n = static_cast<int>(bones.size());
            auto B = [&](int i) -> glm::mat4 {
                return (i >= 0 && i < n) ? bones[i] : glm::mat4(1.0f);
            };
            const glm::mat4 sm = B(static_cast<int>(bv.joints[0])) * bw0
                               + B(static_cast<int>(bv.joints[1])) * bw1
                               + B(static_cast<int>(bv.joints[2])) * bw2
                               + B(static_cast<int>(bv.joints[3])) * bw3;
            const glm::mat3 nm = glm::mat3(sm);
            out_pos = glm::vec3(sm * glm::vec4(bv.pos, 1.0f));
            out_nrm = glm::normalize(nm * bv.normal);
            out_tan = glm::normalize(nm * bv.tangent);
        };
        auto bake_skinned_in_queue = [&](std::vector<MeshDrawItem>& queue) {
            for (auto& item : queue) {
                // ä»…å¤„ç†å•å®žä¾‹è’™çš®é¡¹ï¼ˆå®žä¾‹åŒ–è’™çš®çš„ web çƒ˜ç„™æš‚ä¸è¦†ç›–ï¼šæœ¬ demo ç”¨å•å®žä¾‹ï¼‰ã€‚
                if (!item.skinned || item.bone_matrices.empty()) continue;
                if (item.instance_transforms.size() > 1) continue;
                const BatchVertex* src =
                    item.shared_vertex_ptr ? item.shared_vertex_ptr : item.vertices.data();
                const uint32_t vcount = item.shared_vertex_ptr
                    ? item.shared_vertex_count
                    : static_cast<uint32_t>(item.vertices.size());
                if (vcount == 0) continue;

                // ç”Ÿäº§è€…ï¼šæœ¬å¸§è¯·æ±‚å–‚ GPU computeï¼ˆWebGPU çœŸè’™çš®ï¼›WebGL2 æ—  compute è·³è¿‡ï¼Œçº¯ CPUï¼‰ã€‚
                if (gpu_skin) {
                    dse::render::SkinningRequest req;
                    req.entity_id = item.entity_id;
                    req.vertex_count = vcount;
                    req.bone_matrices = item.bone_matrices;
                    req.src_vertex_data.resize(static_cast<size_t>(vcount) * 16);
                    for (uint32_t i = 0; i < vcount; ++i) {
                        const BatchVertex& bv = src[i];
                        float* d = req.src_vertex_data.data() + static_cast<size_t>(i) * 16;
                        d[0]  = bv.pos.x;     d[1]  = bv.pos.y;     d[2]  = bv.pos.z;     d[3]  = bv.weights[0];
                        d[4]  = bv.normal.x;  d[5]  = bv.normal.y;  d[6]  = bv.normal.z;  d[7]  = bv.weights[1];
                        d[8]  = bv.tangent.x; d[9]  = bv.tangent.y; d[10] = bv.tangent.z; d[11] = bv.weights[2];
                        d[12] = bv.joints[0]; d[13] = bv.joints[1]; d[14] = bv.joints[2]; d[15] = bv.joints[3];
                    }
                    rs_->gpu_skinning_system_.Submit(std::move(req));
                }

                // æ¶ˆè´¹ï¼šä¸Šä¸€å¸§ GPU å›žè¯»ï¼ˆä¸–ç•Œç©ºé—´â€”â€”éª¨éª¼çŸ©é˜µå·²é¢„ä¹˜ modelï¼‰ï¼›æœªå°±ç»ª/WebGL2 â†’ CPU è’™çš®å›žé€€ã€‚
                const dse::render::SkinnedOutput* out =
                    gpu_skin ? rs_->gpu_skinning_system_.GetSkinnedOutput(item.entity_id) : nullptr;
                const bool use_gpu = out && out->vertex_count == vcount;

                std::vector<BatchVertex> baked(vcount);
                for (uint32_t i = 0; i < vcount; ++i) {
                    BatchVertex ov = src[i];
                    if (use_gpu) {
                        ov.pos     = out->positions[i];
                        ov.normal  = out->normals[i];
                        ov.tangent = out->tangents[i];
                    } else {
                        skin_like_compute(src[i], item.bone_matrices, ov.pos, ov.normal, ov.tangent);
                    }
                    ov.weights = glm::vec4(0.0f);
                    ov.joints  = glm::vec4(0.0f);
                    baked[i] = ov;
                }

                // å°±åœ°è½¬ä¸ºéžè’™çš®é™æ€é¡¹ï¼šè’™çš®çŸ©é˜µæ˜¯ã€Œå¯¹è±¡ç©ºé—´ã€è’™çš®ï¼ˆbind-local â†’ å§¿æ€åŽçš„æ¨¡åž‹ç©ºé—´ï¼‰ï¼Œ
                // é¡¶ç‚¹ä»åœ¨æ¨¡åž‹ç©ºé—´ â†’ ä¿ç•™åŽŸ model çŸ©é˜µä¸å˜ï¼Œç”± ForwardShaded çš„ CPU ä¸–ç•Œçƒ˜ç„™
                // ï¼ˆBuildShadedWorldVertexBuffer ä¹˜ model + æ³•çº¿çŸ©é˜µï¼‰æŠŠå§¿æ€åŽçš„æ¨¡åž‹ç©ºé—´é¡¶ç‚¹å˜åˆ°ä¸–ç•Œï¼Œ
                // ä¸Žæ™®é€šé™æ€ç½‘æ ¼å®Œå…¨ä¸€è‡´ï¼›ä»…æ¸…æŽ‰è’™çš®æ ‡å¿—/éª¨éª¼æ•°æ®ï¼Œé¿å…èµ° DrawSkinnedShadedï¼ˆweb ä¸Š no-opï¼‰ã€‚
                if (item.indices.empty() && item.shared_index_ptr && item.shared_index_count) {
                    item.indices.assign(item.shared_index_ptr,
                                        item.shared_index_ptr + item.shared_index_count);
                }
                item.vertices = std::move(baked);
                item.shared_vertex_ptr = nullptr;
                item.shared_vertex_count = 0;
                item.shared_index_ptr = nullptr;
                item.shared_index_count = 0;
                item.skinned = false;
                item.bone_matrices.clear();
                item.instance_transforms.clear();
                item.bone_palette.clear();
                item.instance_bone_palette_idx.clear();
            }
        };
        bake_skinned_in_queue(rs_->render_scene_.cpu_meshes.opaque);
        bake_skinned_in_queue(rs_->render_scene_.cpu_meshes.transparent);
    }
}

void FramePipeline::CollectRuntimeStats() {
    if (!runtime_context_.world || !runtime_context_.rhi_device) return;
    stats_accumulator_ += Time::delta_time();
    if (stats_accumulator_ >= 1.0f) {
        stats_accumulator_ = 0.0f;
        const auto& stats = runtime_context_.rhi_device->LastFrameStats();
        size_t entity_count = runtime_context_.world->EntityCount();
        size_t physics_bodies = 0;
        auto physics_view = runtime_context_.world->registry().view<RigidBody2DComponent>();
        for (auto entity : physics_view) {
            (void)entity;
            ++physics_bodies;
        }
#if defined(DSE_ENABLE_3D) && (defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT))
        auto physics3d_view = runtime_context_.world->registry().view<dse::RigidBody3DComponent>();
        for (auto entity : physics3d_view) {
            (void)entity;
            ++physics_bodies;
        }
#endif
        size_t particle_emitters = 0;
        size_t active_particles = 0;
        auto particle2d_view = runtime_context_.world->registry().view<ParticleEmitterComponent>();
        for (auto emitter_entity : particle2d_view) {
            auto& emitter = particle2d_view.get<ParticleEmitterComponent>(emitter_entity);
            (void)emitter_entity;
            ++particle_emitters;
            active_particles += emitter.particles.size();
        }
        auto particle3d_view = runtime_context_.world->registry().view<dse::ParticleSystem3DComponent>();
        for (auto particle_entity : particle3d_view) {
            const auto& particle_system = particle3d_view.get<dse::ParticleSystem3DComponent>(particle_entity);
            (void)particle_entity;
            ++particle_emitters;
            const int active_particle_count = particle_system.active_particle_count;
            active_particles += static_cast<size_t>(active_particle_count > 0 ? active_particle_count : 0);
        }
        float avg_update_ms = update_samples_ > 0 ? update_time_accumulator_ms_ / static_cast<float>(update_samples_) : 0.0f;
        float avg_fixed_ms = fixed_samples_ > 0 ? fixed_time_accumulator_ms_ / static_cast<float>(fixed_samples_) : 0.0f;
        float avg_render_ms = render_samples_ > 0 ? render_time_accumulator_ms_ / static_cast<float>(render_samples_) : 0.0f;
        auto& asset_manager = RequireAssetManager(runtime_context_.asset_manager);
        std::size_t pending_callbacks = asset_manager.PendingMainThreadCallbacks();
        std::size_t pending_callbacks_hwm = asset_manager.PendingMainThreadCallbacksHighWatermark();
        DEBUG_LOG_INFO("Runtime stats: entities={}, sprites={}, meshes={}, draw_calls={}, material_switches={}, shadow_passes={}, max_batch_sprites={}, render_passes={}, physics_bodies={}, particle_emitters={}, active_particles={}, avg_update_ms={}, avg_fixed_ms={}, avg_render_ms={}, instanced_meshes={}, gpu_driven_requested={}, gpu_driven_supported={}, gpu_driven_prepared={}, gpu_driven_active={}, gpu_indirect_draws={}, gpu_instances={}, pending_upload_callbacks={}, pending_upload_callbacks_hwm={}, upload_budget={}",
                       entity_count,
                       stats.sprite_count,
                       stats.mesh_count,
                       stats.draw_calls,
                       stats.material_switches,
                       stats.shadow_passes,
                       stats.max_batch_sprites,
                       stats.render_passes,
                       physics_bodies,
                       particle_emitters,
                       active_particles,
                       avg_update_ms,
                       avg_fixed_ms,
                       avg_render_ms,
                       stats.instanced_mesh_count,
                       render_pass_context_.gpu_driven_requested,
                       render_pass_context_.gpu_driven_supported,
                       render_pass_context_.gpu_driven_scene_prepared,
                       render_pass_context_.gpu_driven_active_this_frame,
                       render_pass_context_.gpu_indirect_draw_count,
                       render_pass_context_.gpu_total_instances,
                       pending_callbacks,
                       pending_callbacks_hwm,
                       callback_budget_per_frame_);
        update_time_accumulator_ms_ = 0.0f;
        fixed_time_accumulator_ms_ = 0.0f;
        render_time_accumulator_ms_ = 0.0f;
        update_samples_ = 0;
        fixed_samples_ = 0;
        render_samples_ = 0;
    }
}

void FramePipeline::BuildRenderGraph() {
    dse::runtime::BuildFrameRenderGraph(*this);
}

void FramePipeline::BuildRenderSceneQueues() {
    rs_->render_scene_.Clear();
    render_pass_context_.render_scene = &rs_->render_scene_;
    if (!runtime_context_.world) return;

    World* world = runtime_context_.world;

    // 2D/3D 双路径的选择封装在 IBuiltinModules 实现内
    modules_impl_->BuildRenderQueues(*world, rs_->render_scene_, builtin_gameplay3d_enabled_);

    // åŠ¨æ€æ¨¡å—çš„æ¸²æŸ“è´¡çŒ®ç»Ÿä¸€é€šè¿‡ RegisterRenderPasses æ³¨å†Œåˆ° RenderGraphï¼Œ
    // ä¸å†ç»ç”± IModule çš„å›ºå®šé˜¶æ®µå›žè°ƒåŒ…è£…è¿› RenderScene å›žè°ƒæ¡¶ã€‚
    (void)world;
}

void FramePipeline::BuildRenderGraphInternal() {
    render_graph_dag_.Reset();
    registered_passes_.clear();

    // ---- å¡«å…… RenderPassContext ----
    render_pass_context_.world = runtime_context_.world;
    render_pass_context_.asset_manager = runtime_context_.asset_manager;
    render_pass_context_.rhi_device = runtime_context_.rhi_device.get();
    render_pass_context_.render_scene = &rs_->render_scene_;
    render_pass_context_.mesh_renderer = &rs_->cpu_mesh_renderer_;
    render_pass_context_.light_buffer = &rs_->light_buffer_;
    render_pass_context_.cluster_grid = &rs_->cluster_grid_;
    render_pass_context_.editor_mode = runtime_context_.editor_mode;

    // Pass å±‚å›¾å½¢ç®¡çº¿ï¼ˆB5-3bï¼‰ï¼šèšåˆä¸º (pso, program=0) PSO-only ç®¡çº¿å¥æŸ„â€”â€”å…¶åŽç»˜åˆ¶ç» GPU-driven è‡ªç»‘ program
    // æˆ–è¢«æ¸²æŸ“å™¨è‡ªå¸¦ (pso+program) è¦†ç›–ï¼Œæ•…æ­¤å¤„ä¸çƒ˜ programï¼ŒBindPipeline ä»…åº”ç”¨ PSO çŠ¶æ€ï¼Œä¿ç•™åŽŸ SetPipelineState è¯­ä¹‰ã€‚
    auto* rhi = runtime_context_.rhi_device.get();
    render_pass_context_.pipeline_states.sprite    = rhi->GetGraphicsPipeline(render_resources_.sprite_pipeline_state, 0);
    render_pass_context_.pipeline_states.mesh      = rhi->GetGraphicsPipeline(render_resources_.mesh_pipeline_state, 0);
    render_pass_context_.pipeline_states.prez      = rhi->GetGraphicsPipeline(render_resources_.prez_pipeline_state, 0);
    render_pass_context_.pipeline_states.shadow    = rhi->GetGraphicsPipeline(render_resources_.shadow_pipeline_state, 0);
    render_pass_context_.pipeline_states.composite = rhi->GetGraphicsPipeline(render_resources_.composite_pipeline_state, 0);
    render_pass_context_.pipeline_states.decal_blend = rhi->GetGraphicsPipeline(render_resources_.decal_blend_pipeline_state, 0);
    render_pass_context_.pipeline_states.wboit_accum = rhi->GetGraphicsPipeline(render_resources_.wboit_accum_pipeline_state, 0);
    render_pass_context_.pipeline_states.wboit_reveal = rhi->GetGraphicsPipeline(render_resources_.wboit_reveal_pipeline_state, 0);

    render_pass_context_.render_targets.main     = render_resources_.main_render_target;
    render_pass_context_.render_targets.scene    = render_resources_.scene_render_target;
    render_pass_context_.render_targets.ui       = render_resources_.ui_render_target;
    render_pass_context_.render_targets.prez     = render_resources_.prez_render_target;
    for (int i = 0; i < CSM_CASCADES; ++i) {
        render_pass_context_.render_targets.shadow[i] = render_resources_.shadow_render_target[i];
    }
    render_pass_context_.render_targets.shadow_atlas = render_resources_.shadow_atlas_render_target;
    for (int i = 0; i < 4; ++i) {
        render_pass_context_.render_targets.spot_shadow[i]  = render_resources_.spot_shadow_render_target[i];
        render_pass_context_.render_targets.point_shadow[i] = render_resources_.point_shadow_render_target[i];
    }
    render_pass_context_.render_targets.bloom_extract = render_resources_.pp_bloom_extract_rt;
    render_pass_context_.render_targets.bloom_mips    = render_resources_.pp_bloom_mip_rts;
    render_pass_context_.render_targets.ssao      = render_resources_.pp_ssao_rt;
    render_pass_context_.render_targets.ssao_blur = render_resources_.pp_ssao_blur_rt;
    render_pass_context_.render_targets.contact_shadow = render_resources_.pp_contact_shadow_rt;
    render_pass_context_.render_targets.fxaa      = render_resources_.pp_fxaa_rt;
    render_pass_context_.render_targets.taa       = render_resources_.pp_taa_rt;
    render_pass_context_.render_targets.dof       = render_resources_.pp_dof_rt;
    render_pass_context_.render_targets.ssr       = render_resources_.pp_ssr_rt;
    render_pass_context_.render_targets.motion_vector = render_resources_.pp_motion_vector_rt;
    render_pass_context_.render_targets.outline = render_resources_.pp_outline_rt;
    render_pass_context_.render_targets.fog    = render_resources_.pp_fog_rt;
    render_pass_context_.render_targets.cloud  = render_resources_.pp_cloud_rt;
    render_pass_context_.render_targets.wboit_accum = render_resources_.wboit_accum_rt;
    render_pass_context_.render_targets.wboit_reveal = render_resources_.wboit_reveal_rt;
    render_pass_context_.render_targets.sss_temp = render_resources_.pp_sss_temp_rt;
    if (render_resources_.rsm_render_target != 0) {
        render_pass_context_.rsm_render_target = render_resources_.rsm_render_target;
        render_pass_context_.rsm_targets.position = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.rsm_render_target, 0);
        render_pass_context_.rsm_targets.normal   = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.rsm_render_target, 1);
        render_pass_context_.rsm_targets.flux     = runtime_context_.rhi_device->GetRenderTargetColorTexture(render_resources_.rsm_render_target, 2);
        render_pass_context_.rsm_targets.width = 512;
        render_pass_context_.rsm_targets.height = 512;
    }
    render_pass_context_.render_targets.lum_temp  = render_resources_.pp_lum_temp_rt;
    render_pass_context_.render_targets.lum_adapted[0] = render_resources_.pp_lum_adapted_rt[0];
    render_pass_context_.render_targets.lum_adapted[1] = render_resources_.pp_lum_adapted_rt[1];
    render_pass_context_.render_targets.hiz_texture = render_resources_.hiz_texture;
    render_pass_context_.hiz_visibility_ssbo = render_resources_.hiz_visibility_ssbo;
    render_pass_context_.hiz_aabb_ssbo = render_resources_.hiz_aabb_ssbo;
    render_pass_context_.hiz_aabb_capacity = render_resources_.hiz_ssbo_capacity;
    render_pass_context_.hiz_culling_enabled = false;
    render_pass_context_.hiz_object_count = 0;
    render_pass_context_.hiz_copy_shader = render_resources_.hiz_copy_shader;
    render_pass_context_.hiz_downsample_shader = render_resources_.hiz_downsample_shader;
    render_pass_context_.hiz_cull_shader = render_resources_.hiz_cull_shader;

    // GPU Driven é˜èˆµâ‚¬?
    render_pass_context_.gpu_driven_enabled = render_resources_.gpu_driven_supported;
    render_pass_context_.gpu_driven_supported = render_resources_.gpu_driven_supported;
    render_pass_context_.gpu_driven_requested = gpu_driven_requested_;
    render_pass_context_.gpu_driven_scene_prepared = false;
    render_pass_context_.gpu_driven_active_this_frame = false;
    render_pass_context_.gpu_indirect_buffer = render_resources_.gpu_indirect_buffer;
    render_pass_context_.gpu_instance_ssbo = render_resources_.gpu_instance_ssbo;
    render_pass_context_.gpu_material_ssbo = render_resources_.gpu_material_ssbo;
    render_pass_context_.gpu_draw_cmd_ssbo = render_resources_.gpu_draw_cmd_ssbo;
    render_pass_context_.gpu_aabb_ssbo = render_resources_.gpu_aabb_ssbo;
    render_pass_context_.gpu_aabb_capacity = render_resources_.gpu_aabb_capacity;
    render_pass_context_.gpu_visible_indices_ssbo = render_resources_.gpu_visible_indices_ssbo;
    render_pass_context_.gpu_atomic_counter_ssbo = render_resources_.gpu_atomic_counter_ssbo;
    render_pass_context_.gpu_mega_vao = render_resources_.gpu_mega_vao;
    render_pass_context_.gpu_cull_shader = render_resources_.gpu_cull_shader;
    render_pass_context_.gpu_indirect_draw_count = 0;
    render_pass_context_.gpu_total_instances = 0;
    render_pass_context_.fxaa_active = false;
    render_pass_context_.taa_active = false;
    render_pass_context_.auto_exposure_active = false;
    render_pass_context_.pipeline_features.bloom =
        IsProfilePassEnabled(rs_->render_pipeline_profile_, "bloom");
    render_pass_context_.pipeline_features.ssao =
        IsProfilePassEnabled(rs_->render_pipeline_profile_, "ssao");
    render_pass_context_.pipeline_features.contact_shadow =
        IsProfilePassEnabled(rs_->render_pipeline_profile_, "contact_shadow");
    render_pass_context_.pipeline_features.auto_exposure =
        IsProfilePassEnabled(rs_->render_pipeline_profile_, "auto_exposure");
    render_pass_context_.pipeline_features.fxaa =
        IsProfilePassEnabled(rs_->render_pipeline_profile_, "fxaa");
    render_pass_context_.pipeline_features.taa =
        IsProfilePassEnabled(rs_->render_pipeline_profile_, "taa");
    render_pass_context_.pipeline_features.ui =
        IsProfilePassEnabled(rs_->render_pipeline_profile_, "ui");
    render_pass_context_.pipeline_features.gpu_cull =
        IsProfilePassEnabled(rs_->render_pipeline_profile_, "gpu_cull");
    render_pass_context_.pipeline_features.shadows = rs_->render_pipeline_profile_.settings.shadows &&
        (IsProfilePassEnabled(rs_->render_pipeline_profile_, "csm_shadow") ||
         IsProfilePassEnabled(rs_->render_pipeline_profile_, "spot_shadow") ||
         IsProfilePassEnabled(rs_->render_pipeline_profile_, "point_shadow"));
    render_pass_context_.pipeline_overrides.bloom_intensity = PipelineValueToFloat(
        dse::render::FindRenderPipelinePassParam(
            rs_->render_pipeline_profile_, dse::render::BuiltinRenderPipelineRegistry(), "bloom", "intensity"),
        -1.0f);
    render_pass_context_.pipeline_overrides.bloom_threshold = PipelineValueToFloat(
        dse::render::FindRenderPipelinePassParam(
            rs_->render_pipeline_profile_, dse::render::BuiltinRenderPipelineRegistry(), "bloom", "threshold"),
        -1.0f);

    render_pass_context_.modules.clear();
    for (auto& mod : modules_) {
        if (mod.instance) {
            render_pass_context_.modules.push_back({mod.instance});
        }
    }
    render_pass_context_.render_2d_scene = [this](World& world, CommandBuffer& cmd, const dse::render::FrameContext& frame) {
        modules_impl_->RenderScene2D(world, cmd, frame);
    };
    render_pass_context_.render_2d_ui = [this](World& world, CommandBuffer& cmd, int w, int h, const glm::mat4& clip) {
        modules_impl_->RenderUI2D(world, cmd, w, h, clip);
    };
    render_pass_context_.render_meshes = [this](World& world, CommandBuffer& cmd, const dse::render::FrameContext& frame) {
        modules_impl_->RenderMeshes(world, cmd, *render_pass_context_.rhi_device, rs_->cpu_mesh_renderer_, frame);
    };

    // ---- æ¾¹ç‰ˆæ§‘æ¾¶æ ­å„´æˆæ’³åš­ ----
    auto main_color  = render_graph_dag_.DeclareResource("main_color");
    auto scene_color = render_graph_dag_.DeclareResource("scene_color");
    auto taa_color   = render_graph_dag_.DeclareResource("taa_color");
    auto dof_color   = render_graph_dag_.DeclareResource("dof_color");
    auto ssr_color   = render_graph_dag_.DeclareResource("ssr_color");
    auto mb_color    = render_graph_dag_.DeclareResource("motion_blur_color");
    auto outline_color = render_graph_dag_.DeclareResource("outline_color");
    render_graph_dag_.MarkOutput(main_color);
    render_graph_dag_.MarkOutput(scene_color);
    render_graph_dag_.MarkOutput(taa_color);
    render_graph_dag_.MarkOutput(dof_color);
    render_graph_dag_.MarkOutput(ssr_color);
    render_graph_dag_.MarkOutput(mb_color);
    render_graph_dag_.MarkOutput(outline_color);

    taa_pass_ = nullptr;
    const auto& registry = dse::render::BuiltinRenderPipelineRegistry();
    dse::render::RenderPipelineValidationContext prune_ctx{};
    prune_ctx.editor_mode = runtime_context_.editor_mode;
    prune_ctx.hiz_available = render_resources_.hiz_texture != 0;
    prune_ctx.gpu_driven_supported = render_resources_.gpu_driven_supported;
    if (const dse::render::RhiDevice* dev = runtime_context_.rhi_device.get()) {
        prune_ctx.compute_supported = dev->SupportsCompute();
        prune_ctx.ssbo_supported = dev->SupportsSSBO();
        prune_ctx.max_color_attachments = dev->GetMaxColorAttachments();
    }
    for (const auto& pass_config : rs_->render_pipeline_profile_.passes) {
        if (!pass_config.enabled) continue;
        const std::string pass_name = registry.ResolveName(pass_config.name);
        const dse::render::RenderPassMetadata* metadata = registry.FindMetadata(pass_name);
        if (!metadata) {
            DEBUG_LOG_WARN("Render pipeline skipped unknown pass '{}'", pass_config.name);
            continue;
        }
        if (const char* prune_reason = dse::render::RenderPassCapabilityPruneReason(*metadata, prune_ctx)) {
            DEBUG_LOG_INFO("Render pipeline pruned pass '{}' ({})", pass_name, prune_reason);
            continue;
        }
        if (!rs_->render_pipeline_profile_.settings.shadows &&
            (pass_name == "csm_shadow" || pass_name == "spot_shadow" || pass_name == "point_shadow")) {
            continue;
        }
        auto pass = registry.Create(pass_name, render_pass_context_);
        if (!pass) {
            DEBUG_LOG_WARN("Render pipeline failed to create pass '{}'", pass_name);
            continue;
        }
        if (pass_name == "taa") {
            taa_pass_ = static_cast<dse::render::TAAPass*>(pass.get());
        }
        registered_passes_.push_back(std::move(pass));
    }

    // ---- å¦¯â€³æ½¡é”ã„¦â‚¬ä½¹æ•žéå²ƒåšœç€¹æ°«ç®Ÿ Pass ----
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->RegisterRenderPasses(render_graph_dag_, render_pass_context_, registered_passes_);
        }
    }
    if (builtin_gameplay3d_enabled_) {
        modules_impl_->RegisterGameplay3DPasses(render_graph_dag_, render_pass_context_, registered_passes_);
    }

    // ---- éŽµâ‚¬éˆ?Pass é¦?RenderGraph æ¶“å©‚ï¼é„åºç··ç’§?----
    for (auto& pass : registered_passes_) {
        pass->Setup(render_graph_dag_);
    }

    // ç¼‚æ ¬ç˜§ DAGé”›å Ÿå«‡éŽµæˆžå¸“æ´?+ éƒçŠµæ•¤ Pass é“æ—ˆæ«Žé”›?
    if (!render_graph_dag_.Compile()) {
        DEBUG_LOG_ERROR("RenderGraph ç¼‚æ ¬ç˜§æ¾¶è¾«è§¦é”›æ°­î—…å¨´å¬ªåŸŒå¯°î†å¹†æ¸šæ¿Šç¦†");
    }
}


void FramePipeline::ExecuteRenderGraph(CommandBuffer& cmd_buffer) {
    dse::runtime::ExecuteFrameRenderGraph(*this, cmd_buffer);
}

/// æ£°å‹­å„¹ builtin Pass é¦?Execute() æ¶“î… æ•¤é’æ‰®æ®‘éŽµâ‚¬éˆ?ECS ç¼å‹ªæ¬¢å§¹çŠ®â‚¬?
/// é‚æ¿î–ƒ Pass é‘»ãƒ¤å¨‡é¢ã„¦æŸŠç¼å‹ªæ¬¢ç»«è¯²ç€·é”›å±½ç¹€æ¤¤è¯²æ¹ªå§ã‚…î˜©ç›ãƒ¥åŽ–ç€µç‘°ç°² view ç’‹å†ªæ•¤éŠ†?
/// Debug å¦¯â€³ç´¡æ¶“?ExecuteRenderGraphInternal æµ¼æ°¬æ¹ªéªžæƒ°î”‘éŽµÑ†î”‘éšåº¢æŸ‡ç‘·â‚¬å§¹çŠ³æšŸé–²å¿”æ¹­æ¾§ç‚ºæš±é”›?
/// æµ ãƒ¦î—…å¨´å¬®ä»å©•å¿•æ®‘ç¼å‹ªæ¬¢ç»«è¯²ç€·éŠ†?
static void WarmUpRenderECSPools(entt::registry& reg) {
    // --- builtin Pass é©å­˜å¸´æµ£è·¨æ•¤ ---
    (void)reg.view<TransformComponent>();
    (void)reg.view<CameraComponent>();
    (void)reg.view<dse::Camera3DComponent>();
    (void)reg.view<dse::DirectionalLight3DComponent>();
    (void)reg.view<TransformComponent, dse::SpotLightComponent>();
    (void)reg.view<TransformComponent, dse::PointLightComponent>();
    (void)reg.view<dse::PostProcessComponent>();
    (void)reg.view<dse::SkyboxComponent>();
    (void)reg.view<dse::DecalComponent, TransformComponent>();
    (void)reg.view<dse::WaterComponent>();
    // --- æ¨¡å—æ¸²æŸ“ï¼ˆBuildRenderQueues / RenderPassContext é’©å­ï¼‰é—´æŽ¥ä½¿ç”¨ ---
    (void)reg.view<TransformComponent, dse::MeshRendererComponent>();
    (void)reg.view<dse::SkyLightComponent>();
    (void)reg.view<dse::TerrainComponent, TransformComponent>();
    (void)reg.view<dse::GrassComponent, TransformComponent>();
    (void)reg.view<dse::HairComponent, TransformComponent>();
    (void)reg.view<dse::ParticleSystem3DComponent>();
    (void)reg.view<dse::FluidEmitterComponent>();
    (void)reg.view<dse::GIProbeVolumeComponent>();
}

void FramePipeline::ExecuteRenderGraphInternal(CommandBuffer& cmd_buffer) {
    static int diag_pass_frame = 0;
    const char* pass_diag = std::getenv("DSE_PASS_DIAG");
    const bool pass_diag_enabled = pass_diag && pass_diag[0] != '\0' && pass_diag[0] != '0';
    const unsigned int scene_rt = render_resources_.scene_render_target;
    if (pass_diag_enabled && diag_pass_frame < 4 && scene_rt != 0 && runtime_context_.rhi_device) {
        dse::render::RhiDevice* rhi = runtime_context_.rhi_device.get();
        render_graph_dag_.ExecuteWithCallback(cmd_buffer, [&](const std::string& pass_name) {
            auto rb = rhi->ReadRenderTargetColorRgba8WithSize(scene_rt);
            if (rb.width > 0 && rb.height > 0) {
                int cx = rb.width / 2, cy = rb.height / 2;
                int bx = rb.width / 4, by = rb.height / 4;
                std::size_t ci = (static_cast<std::size_t>(cy) * rb.width + cx) * 4;
                std::size_t bi = (static_cast<std::size_t>(by) * rb.width + bx) * 4;
                int black_count = 0;
                for (std::size_t i = 0; i + 3 < rb.pixels.size(); i += 4) {
                    if (static_cast<int>(rb.pixels[i]) + static_cast<int>(rb.pixels[i+1]) + static_cast<int>(rb.pixels[i+2]) <= 8)
                        ++black_count;
                }
                DEBUG_LOG_INFO("[PassDiag] frame={} after={} black={} center=({},{},{}) bbox_tl=({},{},{})",
                    diag_pass_frame, pass_name, black_count,
                    static_cast<int>(rb.pixels[ci]), static_cast<int>(rb.pixels[ci+1]), static_cast<int>(rb.pixels[ci+2]),
                    static_cast<int>(rb.pixels[bi]), static_cast<int>(rb.pixels[bi+1]), static_cast<int>(rb.pixels[bi+2]));
            }
        });
        ++diag_pass_frame;
    } else {
        render_graph_dag_.Execute(cmd_buffer);
    }
}

