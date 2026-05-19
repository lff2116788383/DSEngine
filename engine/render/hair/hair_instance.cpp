/**
 * @file hair_instance.cpp
 * @brief 毛发实例 GPU 资源管理实现
 */

#include "engine/render/hair/hair_instance.h"
#include "engine/render/hair/hair_compute_shaders.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"

namespace dse {
namespace render {

bool HairInstance::CreateGPUResources(RhiDevice* rhi, const HairAsset& hair_asset) {
    if (!rhi || !hair_asset.IsValid()) return false;

    asset = &hair_asset;
    total_vertex_count = hair_asset.num_vertices();
    active_strand_count = hair_asset.num_guide_strands();

    const size_t vert_bytes = total_vertex_count * sizeof(glm::vec4);
    const size_t strand_bytes = hair_asset.num_guide_strands() * sizeof(uint32_t) * 2;

    // 位置 SSBO × 3 (current / prev / rest)
    GpuBufferDesc desc;
    desc.usage = GpuBufferUsage::kStorage;
    desc.is_dynamic = true;

    desc.size = vert_bytes;
    desc.debug_name = "hair_position";
    position_ssbo      = rhi->CreateGpuBuffer(desc, nullptr);
    desc.debug_name = "hair_position_prev";
    position_prev_ssbo = rhi->CreateGpuBuffer(desc, nullptr);
    desc.debug_name = "hair_position_rest";
    position_rest_ssbo = rhi->CreateGpuBuffer(desc, nullptr);

    // 切线 SSBO
    desc.debug_name = "hair_tangent";
    tangent_ssbo = rhi->CreateGpuBuffer(desc, nullptr);

    // Strand info SSBO: uvec2(offset, count) per strand
    desc.size = strand_bytes;
    desc.debug_name = "hair_strand_info";
    strand_info_ssbo = rhi->CreateGpuBuffer(desc, nullptr);

    if (!position_ssbo || !position_prev_ssbo || !position_rest_ssbo ||
        !tangent_ssbo || !strand_info_ssbo) {
        DEBUG_LOG_ERROR("[HairInstance] Failed to allocate GPU SSBOs");
        DestroyGPUResources(rhi);
        return false;
    }

    // 上传 strand info
    std::vector<uint32_t> strand_data(hair_asset.num_guide_strands() * 2);
    for (uint32_t i = 0; i < hair_asset.num_guide_strands(); ++i) {
        strand_data[i * 2 + 0] = hair_asset.strands[i].vertex_offset;
        strand_data[i * 2 + 1] = hair_asset.strands[i].vertex_count;
    }
    rhi->UpdateGpuBuffer(strand_info_ssbo, 0, strand_bytes, strand_data.data());

    // CPU 侧 per-strand 绘制参数
    draw_firsts_.resize(hair_asset.num_guide_strands());
    draw_counts_.resize(hair_asset.num_guide_strands());
    for (uint32_t i = 0; i < hair_asset.num_guide_strands(); ++i) {
        draw_firsts_[i] = static_cast<int>(hair_asset.strands[i].vertex_offset);
        draw_counts_[i] = static_cast<int>(hair_asset.strands[i].vertex_count);
    }

    gpu_resources_valid = true;

    DEBUG_LOG_INFO("[HairInstance] GPU resources created: {} verts, {} strands, {} bytes total",
                   total_vertex_count, active_strand_count, vert_bytes * 4 + strand_bytes);
    return true;
}

void HairInstance::DestroyGPUResources(RhiDevice* rhi) {
    if (!rhi) return;

    if (position_ssbo)      { rhi->DeleteGpuBuffer(position_ssbo);      position_ssbo = {}; }
    if (position_prev_ssbo) { rhi->DeleteGpuBuffer(position_prev_ssbo); position_prev_ssbo = {}; }
    if (position_rest_ssbo) { rhi->DeleteGpuBuffer(position_rest_ssbo); position_rest_ssbo = {}; }
    if (tangent_ssbo)       { rhi->DeleteGpuBuffer(tangent_ssbo);       tangent_ssbo = {}; }
    if (strand_info_ssbo)   { rhi->DeleteGpuBuffer(strand_info_ssbo);   strand_info_ssbo = {}; }

    gpu_resources_valid = false;
    asset = nullptr;
}

void HairInstance::UploadInitialPositions(RhiDevice* rhi, const HairAsset& hair_asset) {
    if (!rhi || !gpu_resources_valid) return;

    // 提取 position (vec4) 和 tangent (vec4) 数据
    std::vector<glm::vec4> positions(total_vertex_count);
    std::vector<glm::vec4> tangents(total_vertex_count);

    for (uint32_t i = 0; i < total_vertex_count; ++i) {
        positions[i] = hair_asset.vertices[i].position;
        tangents[i]  = hair_asset.vertices[i].tangent;
    }

    const size_t pos_bytes = total_vertex_count * sizeof(glm::vec4);
    rhi->UpdateGpuBuffer(position_ssbo,      0, pos_bytes, positions.data());
    rhi->UpdateGpuBuffer(position_prev_ssbo, 0, pos_bytes, positions.data());
    rhi->UpdateGpuBuffer(position_rest_ssbo, 0, pos_bytes, positions.data());
    rhi->UpdateGpuBuffer(tangent_ssbo,       0, pos_bytes, tangents.data());
}

void HairInstance::UpdateLOD(float camera_distance) {
    if (!asset) {
        current_lod = 3;
        active_strand_count = 0;
        return;
    }

    uint32_t total = asset->num_guide_strands();

    if (camera_distance > lod_params.cull_distance) {
        current_lod = 3;
        active_strand_count = 0;
    } else if (camera_distance > lod_params.lod2_distance) {
        current_lod = 2;
        active_strand_count = static_cast<uint32_t>(total * lod_params.lod2_strand_ratio);
    } else if (camera_distance > lod_params.lod1_distance) {
        current_lod = 1;
        active_strand_count = static_cast<uint32_t>(total * lod_params.lod1_strand_ratio);
    } else {
        current_lod = 0;
        active_strand_count = total;
    }

    if (active_strand_count == 0 && current_lod < 3) {
        active_strand_count = 1;
    }
}

void HairInstance::DestroyComputeShaders(RhiDevice* rhi) {
    if (!rhi) return;
    if (cs_integrate_)   { rhi->DeleteComputeShader(cs_integrate_);   cs_integrate_   = 0; }
    if (cs_length_)      { rhi->DeleteComputeShader(cs_length_);      cs_length_      = 0; }
    if (cs_local_shape_) { rhi->DeleteComputeShader(cs_local_shape_); cs_local_shape_ = 0; }
    if (cs_tangent_)     { rhi->DeleteComputeShader(cs_tangent_);     cs_tangent_     = 0; }
}

void HairInstance::Simulate(RhiDevice* rhi, float dt, float time) {
    if (!rhi || !gpu_resources_valid || current_lod >= 3) return;
    if (!rhi->SupportsCompute()) return;

    // --- 懒加载 shader ---
    if (cs_integrate_ == 0) {
        cs_integrate_ = rhi->CreateComputeShaderEx(
            kHairIntegrateSource, kHairIntegrateSourceVK, kHairIntegrateSourceHLSL,
            4, 0, 0, 48);
        if (cs_integrate_ == 0) { DEBUG_LOG_ERROR("[Hair] Failed to compile integrate CS"); return; }
    }
    if (cs_length_ == 0) {
        cs_length_ = rhi->CreateComputeShaderEx(
            kHairLengthConstraintSource, kHairLengthConstraintSourceVK, kHairLengthConstraintSourceHLSL,
            3, 0, 0, 4);
        if (cs_length_ == 0) { DEBUG_LOG_ERROR("[Hair] Failed to compile length CS"); return; }
    }
    if (cs_local_shape_ == 0) {
        cs_local_shape_ = rhi->CreateComputeShaderEx(
            kHairLocalShapeSource, kHairLocalShapeSourceVK, kHairLocalShapeSourceHLSL,
            3, 0, 0, 12);
        if (cs_local_shape_ == 0) { DEBUG_LOG_ERROR("[Hair] Failed to compile local shape CS"); return; }
    }
    if (cs_tangent_ == 0) {
        cs_tangent_ = rhi->CreateComputeShaderEx(
            kHairUpdateTangentSource, kHairUpdateTangentSourceVK, kHairUpdateTangentSourceHLSL,
            3, 0, 0, 12);
        if (cs_tangent_ == 0) { DEBUG_LOG_ERROR("[Hair] Failed to compile tangent CS"); return; }
    }

    const int nv = static_cast<int>(total_vertex_count);
    const int ns = static_cast<int>(active_strand_count);
    const int vps = (ns > 0) ? (nv / ns) : nv;

    rhi->BeginComputePass();

    // --- Pass 1: Verlet Integration ---
    // All backends: binding 0=pos_cur, 1=pos_prev, 2=pos_rest, 3=strand_info
    rhi->BindGpuBuffer(position_ssbo,      0);
    rhi->BindGpuBuffer(position_prev_ssbo, 1);
    rhi->BindGpuBuffer(position_rest_ssbo, 2);
    rhi->BindGpuBuffer(strand_info_ssbo,   3);
    rhi->SetComputeUniformInt(cs_integrate_,   "u_num_vertices", nv);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_dt",           dt);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_damping",      sim_params.damping);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_gx",           sim_params.gravity_dir.x);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_gy",           sim_params.gravity_dir.y);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_gz",           sim_params.gravity_dir.z);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_gw",           sim_params.gravity_magnitude);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_wx",           sim_params.wind.x);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_wy",           sim_params.wind.y);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_wz",           sim_params.wind.z);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_ww",           sim_params.wind_turbulence);
    rhi->SetComputeUniformFloat(cs_integrate_, "u_time",         time);
    rhi->DispatchCompute(cs_integrate_, static_cast<unsigned int>((nv + 63) / 64), 1, 1);
    rhi->ComputeMemoryBarrier();

    // --- Pass 2 & 3: Constraints (多次迭代) ---
    // All backends: binding 0=pos_cur, 1=pos_rest, 2=strand_info
    rhi->BindGpuBuffer(position_ssbo,      0);
    rhi->BindGpuBuffer(position_rest_ssbo, 1);
    rhi->BindGpuBuffer(strand_info_ssbo,   2);
    for (int i = 0; i < sim_params.length_constraint_iterations; ++i) {
        rhi->SetComputeUniformInt(cs_length_, "u_num_strands", ns);
        rhi->DispatchCompute(cs_length_, static_cast<unsigned int>((ns + 63) / 64), 1, 1);
        rhi->ComputeMemoryBarrier();
    }
    for (int i = 0; i < sim_params.local_constraint_iterations; ++i) {
        rhi->SetComputeUniformInt(cs_local_shape_,   "u_num_strands",    ns);
        rhi->SetComputeUniformFloat(cs_local_shape_, "u_stiffness_local",  sim_params.stiffness_local);
        rhi->SetComputeUniformFloat(cs_local_shape_, "u_stiffness_global", sim_params.stiffness_global);
        rhi->DispatchCompute(cs_local_shape_, static_cast<unsigned int>((ns + 63) / 64), 1, 1);
        rhi->ComputeMemoryBarrier();
    }

    // --- Pass 4: Tangent Update ---
    // All backends: binding 0=pos_cur, 1=tangent, 2=strand_info
    rhi->BindGpuBuffer(position_ssbo,  0);
    rhi->BindGpuBuffer(tangent_ssbo,   1);
    rhi->BindGpuBuffer(strand_info_ssbo, 2);
    rhi->SetComputeUniformInt(cs_tangent_, "u_num_vertices",  nv);
    rhi->SetComputeUniformInt(cs_tangent_, "u_num_strands",   ns);
    rhi->SetComputeUniformInt(cs_tangent_, "u_verts_per_strand", vps);
    rhi->DispatchCompute(cs_tangent_, static_cast<unsigned int>((nv + 63) / 64), 1, 1);
    rhi->ComputeMemoryBarrier();

    rhi->EndComputePass();
}

} // namespace render
} // namespace dse
