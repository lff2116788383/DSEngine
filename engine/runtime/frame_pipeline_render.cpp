/**
 * @file frame_pipeline_render.cpp
 * @brief FramePipeline render path â€” RunRenderInternal, BuildRenderGraph, ExecuteRenderGraph.
 */

#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/frame_pipeline_impl.h"
#include "engine/runtime/render_thread_manager.h"
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
    // GPU-Driven PBR shader 仅支持方向光+环境光；存在点光/聚光时回退 CPU 路径，
    // 保证 Clustered Forward+ 局部光照正确（Force 策略除外）。
    const bool local_lights_present = !rs_->light_buffer_.point_lights().empty()
        || !rs_->light_buffer_.spot_lights().empty();
    const bool can_prepare_gpu_scene = render_resources_.gpu_driven_supported
        && gpu_driven_requested_ && gpu_scene_provider_available
        && (!local_lights_present || gpu_driven_policy_ == GpuDrivenPolicy::Force);
    if (can_prepare_gpu_scene && runtime_context_.world && !render_thread_mgr_->IsActive()) {
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

    // GPU Compute Skinning: 帧开始（清空上一帧请求，读回上一帧结果）
    if (rs_->gpu_skinning_system_.IsAvailable()) {
        rs_->gpu_skinning_system_.BeginFrame();
    }

    BuildRenderSceneQueues();

    // ===== B-1（方案 B）：web 蒙皮可见激活 — GPU compute + 复用异步回读 → 世界烘焙静态网格 =====
    // 在缺 ForwardSkinnedShaded 内建程序的后端（WebGPU 无 per-draw 蒙皮；WebGL2/GLES3.0 无法编译
    // SSBO 蒙皮 VS）上，DrawSkinnedShaded 为 no-op、蒙皮网格不可见。此处把蒙皮项喂 GPU compute
    // 蒙皮系统（WebGPU 上 GPU 真做蒙皮），消费「上一帧」异步回读结果（grass 之外第 2 个真实回读
    // 消费方），未就绪/WebGL2 则 CPU 蒙皮回退（同公式，结果一致），烘焙为对象空间顶点的非蒙皮项，
    // 走现有 ForwardShaded 路径（零着色器/管线改动）。桌面（程序可用）不触发，行为不变。
    // 注意：mesh_render_system 把蒙皮项（item.skinned=true）放进 cpu_meshes.opaque/transparent 队列，
    // 而非独立的 skinned 队列（后者从未被填充）。故此处就地扫描这两个队列里的蒙皮项处理。
    if (!skinning_bake_checked_) {
        skinning_bake_for_web_ = !runtime_context_.rhi_device->GetBuiltinProgram(
            BuiltinProgram::ForwardSkinnedShaded);
        skinning_bake_checked_ = true;
    }
    if (skinning_bake_for_web_) {
        const bool gpu_skin = rs_->gpu_skinning_system_.IsAvailable();
        // CPU 蒙皮回退：逐句镜像 compute 蒙皮（4 权重，第 4 权重 = 1 - w0 - w1 - w2；
        // 法线/切线用 mat3(skin)），保证与 GPU 回读路径数值一致 → 暖机/WebGL2 无破帧。
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
                // 仅处理单实例蒙皮项（实例化蒙皮的 web 烘焙暂不覆盖：本 demo 用单实例）。
                if (!item.skinned || item.bone_matrices.empty()) continue;
                if (item.instance_transforms.size() > 1) continue;
                const BatchVertex* src =
                    item.shared_vertex_ptr ? item.shared_vertex_ptr : item.vertices.data();
                const uint32_t vcount = item.shared_vertex_ptr
                    ? item.shared_vertex_count
                    : static_cast<uint32_t>(item.vertices.size());
                if (vcount == 0) continue;

                // 生产者：本帧请求喂 GPU compute（WebGPU 真蒙皮；WebGL2 无 compute 跳过，纯 CPU）。
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

                // 消费：上一帧 GPU 回读（世界空间——骨骼矩阵已预乘 model）；未就绪/WebGL2 → CPU 蒙皮回退。
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

                // 就地转为非蒙皮静态项：蒙皮矩阵是「对象空间」蒙皮（bind-local → 姿态后的模型空间），
                // 顶点仍在模型空间 → 保留原 model 矩阵不变，由 ForwardShaded 的 CPU 世界烘焙
                // （BuildShadedWorldVertexBuffer 乘 model + 法线矩阵）把姿态后的模型空间顶点变到世界，
                // 与普通静态网格完全一致；仅清掉蒙皮标志/骨骼数据，避免走 DrawSkinnedShaded（web 上 no-op）。
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
    stats_.AccumulateStatsTimer(Time::delta_time());
    if (stats_.StatsWindowElapsed()) {
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
        float avg_update_ms = stats_.AvgUpdateMs();
        float avg_fixed_ms = stats_.AvgFixedMs();
        float avg_render_ms = stats_.AvgRenderMs();
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
        stats_.ResetAccumulators();
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

    // 编辑器模式：Edit 状态下不运行 Update 图（Gameplay3DModule::OnUpdate 不执行，
    // 无人调用 MarkBatchDirty），mesh 批次缓存会永久停留在启动时的空结果，
    // 导致编辑时创建/修改的实体不渲染。此处每帧标脏以强制重建。
    if (runtime_context_.editor_mode) {
        modules_impl_->MarkMeshBatchesDirty();
    }

    // 2D/3D 双路径的选择封装在 IBuiltinModules 实现内
    modules_impl_->BuildRenderQueues(*world, rs_->render_scene_, builtin_gameplay3d_enabled_);

    // 动态模块的渲染贡献统一通过 RegisterRenderPasses 注册到 RenderGraph，
    // 不再经由 IModule 的固定阶段回调包装进 RenderScene 回调桶。
    (void)world;
}

void FramePipeline::BuildRenderGraphInternal() {
    render_graph_dag_.Reset();
    registered_passes_.clear();

    // ---- 填充 RenderPassContext ----
    render_pass_context_.world = runtime_context_.world;
    render_pass_context_.asset_manager = runtime_context_.asset_manager;
    render_pass_context_.rhi_device = runtime_context_.rhi_device.get();
    render_pass_context_.render_scene = &rs_->render_scene_;
    render_pass_context_.mesh_renderer = &rs_->cpu_mesh_renderer_;
    render_pass_context_.light_buffer = &rs_->light_buffer_;
    render_pass_context_.cluster_grid = &rs_->cluster_grid_;
    render_pass_context_.editor_mode = runtime_context_.editor_mode;

    // Pass 层图形管线（B5-3b）：聚合为 (pso, program=0) PSO-only 管线句柄——其后绘制经 GPU-driven 自绑 program
    // 或被渲染器自带 (pso+program) 覆盖，故此处不烘 program，BindPipeline 仅应用 PSO 状态，保留原 SetPipelineState 语义。
    auto* rhi = runtime_context_.rhi_device.get();
    render_pass_context_.pipeline_states.sprite    = rhi->GetGraphicsPipeline(render_resources_.sprite_pipeline_state, {});
    render_pass_context_.pipeline_states.mesh      = rhi->GetGraphicsPipeline(render_resources_.mesh_pipeline_state, {});
    render_pass_context_.pipeline_states.prez      = rhi->GetGraphicsPipeline(render_resources_.prez_pipeline_state, {});
    render_pass_context_.pipeline_states.shadow    = rhi->GetGraphicsPipeline(render_resources_.shadow_pipeline_state, {});
    render_pass_context_.pipeline_states.composite = rhi->GetGraphicsPipeline(render_resources_.composite_pipeline_state, {});
    render_pass_context_.pipeline_states.decal_blend = rhi->GetGraphicsPipeline(render_resources_.decal_blend_pipeline_state, {});
    render_pass_context_.pipeline_states.wboit_accum = rhi->GetGraphicsPipeline(render_resources_.wboit_accum_pipeline_state, {});
    render_pass_context_.pipeline_states.wboit_reveal = rhi->GetGraphicsPipeline(render_resources_.wboit_reveal_pipeline_state, {});

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
    if (render_resources_.rsm_render_target) {
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

    // GPU Driven 鐘舵€?
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

    // ---- 澹版槑澶栭儴杈撳嚭 ----
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
    prune_ctx.hiz_available = static_cast<bool>(render_resources_.hiz_texture);
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

    // ---- 妯″潡鍔ㄦ€佹敞鍐岃嚜瀹氫箟 Pass ----
    for (auto& mod : modules_) {
        if (mod.instance) {
            mod.instance->RegisterRenderPasses(render_graph_dag_, render_pass_context_, registered_passes_);
        }
    }
    if (builtin_gameplay3d_enabled_) {
        modules_impl_->RegisterGameplay3DPasses(render_graph_dag_, render_pass_context_, registered_passes_);
    }

    // ---- 鎵€鏈?Pass 鍦?RenderGraph 涓婂０鏄庝緷璧?----
    for (auto& pass : registered_passes_) {
        pass->Setup(render_graph_dag_);
    }

    // 缂栬瘧 DAG锛堟嫇鎵戞帓搴?+ 鏃犵敤 Pass 鍓旈櫎锛?
    if (!render_graph_dag_.Compile()) {
        DEBUG_LOG_ERROR("RenderGraph 缂栬瘧澶辫触锛氭娴嬪埌寰幆渚濊禆");
    }
}


void FramePipeline::ExecuteRenderGraph(CommandBuffer& cmd_buffer) {
    dse::runtime::ExecuteFrameRenderGraph(*this, cmd_buffer);
}

/// 棰勭儹 builtin Pass 鍦?Execute() 涓敤鍒扮殑鎵€鏈?ECS 缁勪欢姹犮€?
/// 鏂板 Pass 鑻ヤ娇鐢ㄦ柊缁勪欢绫诲瀷锛屽繀椤诲湪姝ゅ琛ュ厖瀵瑰簲 view 璋冪敤銆?
/// Debug 妯″紡涓?ExecuteRenderGraphInternal 浼氬湪骞惰鎵ц鍚庢柇瑷€姹犳暟閲忔湭澧為暱锛?
/// 浠ユ娴嬮仐婕忕殑缁勪欢绫诲瀷銆?
static void WarmUpRenderECSPools(entt::registry& reg) {
    // --- builtin Pass 鐩存帴浣跨敤 ---
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
    // --- 模块渲染（BuildRenderQueues / RenderPassContext 钩子）间接使用 ---
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
    const auto scene_rt = render_resources_.scene_render_target;
    if (pass_diag_enabled && diag_pass_frame < 4 && scene_rt && runtime_context_.rhi_device) {
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
