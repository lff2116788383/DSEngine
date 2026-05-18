/**
 * @file hair_system.cpp
 * @brief HairSystem 实现 — HairComponent 驱动的毛发实例管理
 */

#include "modules/gameplay_3d/rendering/hair_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/render/hair/hair_compute_shaders.h"
#include "engine/base/debug.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace dse {
namespace gameplay3d {

void HairSystem::Init(RhiDevice* rhi_device) {
    rhi_ = rhi_device;
    InitComputeShaders();
    DEBUG_LOG_INFO("[HairSystem] Initialized, gpu_compute={}", gpu_compute_enabled_);
}

void HairSystem::Shutdown(::World& world) {
    ShutdownComputeResources();

    // 释放所有 GPU 资源
    for (auto& inst : instances_) {
        inst.DestroyGPUResources(rhi_);
    }
    instances_.clear();
    free_slots_.clear();
    asset_cache_.clear();

    // 重置所有 HairComponent 的运行时索引
    auto shutdown_view = world.registry().view<HairComponent>();
    for (auto entity : shutdown_view) {
        auto& hair = shutdown_view.get<HairComponent>(entity);
        hair.hair_instance_index_ = -1;
    }

    DEBUG_LOG_INFO("[HairSystem] Shutdown, released all hair resources");
}

void HairSystem::Update(::World& world, const glm::vec3& camera_pos, float dt) {
    if (!rhi_) return;
    accumulated_time_ += static_cast<double>(dt);

    auto hair_view = world.registry().view<HairComponent, TransformComponent>();

    for (auto entity : hair_view) {
        auto& hair = hair_view.get<HairComponent>(entity);
        auto& transform = hair_view.get<TransformComponent>(entity);

        if (!hair.enabled || hair.hair_asset_path.empty()) {
            // 禁用 → 释放已有实例，回收槽位
            if (hair.hair_instance_index_ >= 0 &&
                hair.hair_instance_index_ < static_cast<int>(instances_.size())) {
                instances_[hair.hair_instance_index_].DestroyGPUResources(rhi_);
                free_slots_.push_back(hair.hair_instance_index_);
            }
            hair.hair_instance_index_ = -1;
            continue;
        }

        // 如果还没分配 instance，创建一个
        if (hair.hair_instance_index_ < 0) {
            const render::HairAsset* asset = LoadOrGetAsset(hair.hair_asset_path);
            if (!asset || !asset->IsValid()) continue;

            render::HairInstance instance;
            if (!instance.CreateGPUResources(rhi_, *asset)) continue;
            instance.UploadInitialPositions(rhi_, *asset);

            if (!free_slots_.empty()) {
                int slot = free_slots_.back();
                free_slots_.pop_back();
                instances_[slot] = std::move(instance);
                hair.hair_instance_index_ = slot;
            } else {
                hair.hair_instance_index_ = static_cast<int>(instances_.size());
                instances_.push_back(std::move(instance));
            }
        }

        int idx = hair.hair_instance_index_;
        if (idx < 0 || idx >= static_cast<int>(instances_.size())) continue;
        auto& inst = instances_[idx];

        // 同步参数
        inst.sim_params.damping = hair.damping;
        inst.sim_params.stiffness_local = hair.stiffness_local;
        inst.sim_params.stiffness_global = hair.stiffness_global;
        inst.sim_params.gravity_magnitude = hair.gravity;
        inst.sim_params.wind = hair.wind;
        inst.sim_params.wind_turbulence = hair.wind_turbulence;

        inst.render_params.root_color = hair.root_color;
        inst.render_params.tip_color = hair.tip_color;
        inst.render_params.fiber_radius = hair.fiber_radius;
        inst.render_params.opacity = hair.opacity;
        inst.render_params.specular_power_primary = hair.specular_power_primary;
        inst.render_params.specular_power_secondary = hair.specular_power_secondary;
        inst.render_params.specular_strength_primary = hair.specular_strength_primary;
        inst.render_params.specular_strength_secondary = hair.specular_strength_secondary;
        inst.render_params.specular_color = hair.specular_color;
        inst.render_params.cast_shadow = hair.cast_shadow;
        inst.render_params.receive_shadow = hair.receive_shadow;

        inst.lod_params.lod0_distance = hair.lod0_distance;
        inst.lod_params.lod1_distance = hair.lod1_distance;
        inst.lod_params.lod2_distance = hair.lod2_distance;
        inst.lod_params.cull_distance = hair.cull_distance;

        // 世界变换
        inst.world_transform = transform.local_to_world;

        // LOD 更新
        glm::vec3 world_pos = glm::vec3(inst.world_transform[3]);
        float dist = glm::length(camera_pos - world_pos);
        inst.UpdateLOD(dist);
    }

    // GPU 物理模拟
    if (gpu_compute_enabled_ && !instances_.empty()) {
        SimulateCompute(dt);
    }
}

void HairSystem::Render(::World& world, CommandBuffer& cmd_buffer,
                         const glm::mat4& view, const glm::mat4& projection) {
    if (instances_.empty()) return;

    // 查找方向光
    glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
    glm::vec3 light_col(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
    {
        auto dl_view = world.registry().view<dse::DirectionalLight3DComponent>();
        for (auto e : dl_view) {
            auto& dl = dl_view.get<dse::DirectionalLight3DComponent>(e);
            if (!dl.enabled) continue;
            light_dir = dl.direction;
            light_col = dl.color;
            light_intensity = dl.intensity;
            ambient_intensity = dl.ambient_intensity;
            break;
        }
    }

    std::vector<HairDrawItem> items;
    for (auto& inst : instances_) {
        if (!inst.gpu_resources_valid || inst.current_lod >= 3) continue;
        if (inst.active_strand_count == 0 || inst.total_vertex_count == 0) continue;

        HairDrawItem item;
        item.position_ssbo     = inst.position_ssbo;
        item.tangent_ssbo      = inst.tangent_ssbo;
        item.total_vertex_count = inst.total_vertex_count;
        item.strand_count      = inst.active_strand_count;
        item.strand_firsts     = inst.draw_firsts_.data();
        item.strand_counts     = inst.draw_counts_.data();
        item.world_transform   = inst.world_transform;

        item.root_color  = inst.render_params.root_color;
        item.tip_color   = inst.render_params.tip_color;
        item.fiber_radius = inst.render_params.fiber_radius;
        item.opacity      = inst.render_params.opacity;
        item.specular_primary   = inst.render_params.specular_power_primary;
        item.specular_secondary = inst.render_params.specular_power_secondary;
        item.specular_strength_primary   = inst.render_params.specular_strength_primary;
        item.specular_strength_secondary = inst.render_params.specular_strength_secondary;
        item.specular_color     = inst.render_params.specular_color;

        item.light_direction  = light_dir;
        item.light_color      = light_col;
        item.light_intensity  = light_intensity;
        item.ambient_intensity = ambient_intensity;

        items.push_back(item);
    }

    if (!items.empty()) {
        cmd_buffer.DrawHairStrands(items, view, projection);
    }
}

const render::HairAsset* HairSystem::GetCachedAsset(const std::string& path) const {
    auto it = asset_cache_.find(path);
    return it != asset_cache_.end() ? &it->second : nullptr;
}

const render::HairAsset* HairSystem::LoadOrGetAsset(const std::string& path) {
    auto it = asset_cache_.find(path);
    if (it != asset_cache_.end()) return &it->second;

    render::HairAsset asset;

    // 程序化生成: "procedural:strands:verts:length:radius"
    if (path.rfind("procedural:", 0) == 0) {
        uint32_t strands = 256, verts = 16;
        float length = 5.0f, radius = 2.0f;

        // 简单解析
        const char* p = path.c_str() + 11;
        strands = static_cast<uint32_t>(std::atoi(p));
        const char* sep1 = std::strchr(p, ':');
        if (sep1) { verts = static_cast<uint32_t>(std::atoi(sep1 + 1)); }
        const char* sep2 = sep1 ? std::strchr(sep1 + 1, ':') : nullptr;
        if (sep2) { length = static_cast<float>(std::atof(sep2 + 1)); }
        const char* sep3 = sep2 ? std::strchr(sep2 + 1, ':') : nullptr;
        if (sep3) { radius = static_cast<float>(std::atof(sep3 + 1)); }

        render::GenerateTestHairAsset(strands, verts, length, radius, asset);
    } else {
        if (!render::LoadHairAsset(path, asset)) {
            DEBUG_LOG_ERROR("[HairSystem] Failed to load hair asset: {}", path);
            return nullptr;
        }
    }

    asset_cache_[path] = std::move(asset);
    return &asset_cache_[path];
}

// ============================================================
// Compute Shader 初始化 / 清理
// ============================================================

void HairSystem::InitComputeShaders() {
    if (!rhi_ || !rhi_->SupportsSSBOCompute()) {
        gpu_compute_enabled_ = false;
        return;
    }
    cs_integrate_      = rhi_->CreateComputeShader(render::kHairIntegrateSource);
    cs_length_         = rhi_->CreateComputeShader(render::kHairLengthConstraintSource);
    cs_local_shape_    = rhi_->CreateComputeShader(render::kHairLocalShapeSource);
    cs_update_tangent_ = rhi_->CreateComputeShader(render::kHairUpdateTangentSource);

    gpu_compute_enabled_ = (cs_integrate_ != 0 && cs_length_ != 0 &&
                            cs_local_shape_ != 0 && cs_update_tangent_ != 0);
    if (!gpu_compute_enabled_) {
        DEBUG_LOG_WARN("[HairSystem] Some compute shaders failed to compile, "
                       "integrate={} length={} local_shape={} tangent={}",
                       cs_integrate_, cs_length_, cs_local_shape_, cs_update_tangent_);
        ShutdownComputeResources();
    }
}

void HairSystem::ShutdownComputeResources() {
    if (!rhi_) return;
    auto del = [this](unsigned int& h) {
        if (h != 0) { rhi_->DeleteComputeShader(h); h = 0; }
    };
    del(cs_integrate_);
    del(cs_length_);
    del(cs_local_shape_);
    del(cs_update_tangent_);
    gpu_compute_enabled_ = false;
}

// ============================================================
// GPU Compute 模拟
// ============================================================

void HairSystem::SimulateCompute(float dt) {
    if (!gpu_compute_enabled_ || !rhi_) return;

    const float sim_time = static_cast<float>(std::fmod(accumulated_time_, 10000.0));

    rhi_->BeginComputePass();

    for (auto& inst : instances_) {
        if (!inst.gpu_resources_valid || inst.current_lod >= 3) continue;
        if (inst.active_strand_count == 0 || inst.total_vertex_count == 0) continue;

        const uint32_t num_verts   = inst.total_vertex_count;
        const uint32_t num_strands = inst.active_strand_count;
        const auto& sim = inst.sim_params;

        // 绑定 SSBO
        rhi_->BindSSBO(inst.position_ssbo,      0);
        rhi_->BindSSBO(inst.position_prev_ssbo, 1);
        rhi_->BindSSBO(inst.position_rest_ssbo, 2);
        rhi_->BindSSBO(inst.tangent_ssbo,       3);
        rhi_->BindSSBO(inst.strand_info_ssbo,   4);

        // --- Pass 1: Integration ---
        rhi_->SetComputeUniformInt(cs_integrate_,   "u_num_vertices", static_cast<int>(num_verts));
        rhi_->SetComputeUniformFloat(cs_integrate_, "u_dt",           dt);
        rhi_->SetComputeUniformFloat(cs_integrate_, "u_damping",      sim.damping);
        rhi_->SetComputeUniformVec4(cs_integrate_,  "u_gravity",
            sim.gravity_dir.x, sim.gravity_dir.y, sim.gravity_dir.z, sim.gravity_magnitude);
        rhi_->SetComputeUniformVec4(cs_integrate_,  "u_wind",
            sim.wind.x, sim.wind.y, sim.wind.z, sim.wind_turbulence);
        rhi_->SetComputeUniformFloat(cs_integrate_, "u_time", sim_time);

        unsigned int groups_v = (num_verts + 63) / 64;
        rhi_->DispatchCompute(cs_integrate_, groups_v, 1, 1);
        rhi_->ComputeMemoryBarrier();

        unsigned int groups_s = (num_strands + 63) / 64;

        // --- Pass 2 & 3: Constraints (iterated) ---
        for (int iter = 0; iter < sim.local_constraint_iterations; ++iter) {
            // Local shape
            rhi_->SetComputeUniformInt(cs_local_shape_,   "u_num_strands",     static_cast<int>(num_strands));
            rhi_->SetComputeUniformFloat(cs_local_shape_, "u_stiffness_local", sim.stiffness_local);
            rhi_->SetComputeUniformFloat(cs_local_shape_, "u_stiffness_global",sim.stiffness_global);
            rhi_->DispatchCompute(cs_local_shape_, groups_s, 1, 1);
            rhi_->ComputeMemoryBarrier();
        }

        for (int iter = 0; iter < sim.length_constraint_iterations; ++iter) {
            // Length constraint
            rhi_->SetComputeUniformInt(cs_length_, "u_num_strands", static_cast<int>(num_strands));
            rhi_->DispatchCompute(cs_length_, groups_s, 1, 1);
            rhi_->ComputeMemoryBarrier();
        }

        // --- Pass 4: Update tangents ---
        rhi_->SetComputeUniformInt(cs_update_tangent_, "u_num_vertices", static_cast<int>(num_verts));
        rhi_->SetComputeUniformInt(cs_update_tangent_, "u_num_strands",  static_cast<int>(num_strands));
        rhi_->SetComputeUniformInt(cs_update_tangent_, "u_verts_per_strand",
                                   inst.asset ? static_cast<int>(inst.asset->vertices_per_strand) : 16);
        rhi_->DispatchCompute(cs_update_tangent_, groups_v, 1, 1);
        rhi_->ComputeMemoryBarrier();
    }

    rhi_->EndComputePass();
}

} // namespace gameplay3d
} // namespace dse
