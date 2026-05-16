/**
 * @file hair_instance.cpp
 * @brief 毛发实例 GPU 资源管理实现
 */

#include "engine/render/hair/hair_instance.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"

namespace dse {
namespace render {

bool HairInstance::CreateGPUResources(::RhiDevice* rhi, const HairAsset& hair_asset) {
    if (!rhi || !hair_asset.IsValid()) return false;

    asset = &hair_asset;
    total_vertex_count = hair_asset.num_vertices();
    active_strand_count = hair_asset.num_guide_strands();

    const size_t vert_bytes = total_vertex_count * sizeof(glm::vec4);
    const size_t strand_bytes = hair_asset.num_guide_strands() * sizeof(uint32_t) * 2;

    // 位置 SSBO × 3 (current / prev / rest)
    position_ssbo      = rhi->CreateSSBO(vert_bytes, nullptr);
    position_prev_ssbo = rhi->CreateSSBO(vert_bytes, nullptr);
    position_rest_ssbo = rhi->CreateSSBO(vert_bytes, nullptr);

    // 切线 SSBO
    tangent_ssbo = rhi->CreateSSBO(vert_bytes, nullptr);

    // Strand info SSBO: uvec2(offset, count) per strand
    strand_info_ssbo = rhi->CreateSSBO(strand_bytes, nullptr);

    if (position_ssbo == 0 || position_prev_ssbo == 0 || position_rest_ssbo == 0 ||
        tangent_ssbo == 0 || strand_info_ssbo == 0) {
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
    rhi->UpdateSSBO(strand_info_ssbo, 0, strand_bytes, strand_data.data());

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

void HairInstance::DestroyGPUResources(::RhiDevice* rhi) {
    if (!rhi) return;

    if (position_ssbo)      { rhi->DeleteSSBO(position_ssbo);      position_ssbo = 0; }
    if (position_prev_ssbo) { rhi->DeleteSSBO(position_prev_ssbo); position_prev_ssbo = 0; }
    if (position_rest_ssbo) { rhi->DeleteSSBO(position_rest_ssbo); position_rest_ssbo = 0; }
    if (tangent_ssbo)       { rhi->DeleteSSBO(tangent_ssbo);       tangent_ssbo = 0; }
    if (strand_info_ssbo)   { rhi->DeleteSSBO(strand_info_ssbo);   strand_info_ssbo = 0; }

    gpu_resources_valid = false;
    asset = nullptr;
}

void HairInstance::UploadInitialPositions(::RhiDevice* rhi, const HairAsset& hair_asset) {
    if (!rhi || !gpu_resources_valid) return;

    // 提取 position (vec4) 和 tangent (vec4) 数据
    std::vector<glm::vec4> positions(total_vertex_count);
    std::vector<glm::vec4> tangents(total_vertex_count);

    for (uint32_t i = 0; i < total_vertex_count; ++i) {
        positions[i] = hair_asset.vertices[i].position;
        tangents[i]  = hair_asset.vertices[i].tangent;
    }

    const size_t pos_bytes = total_vertex_count * sizeof(glm::vec4);
    rhi->UpdateSSBO(position_ssbo,      0, pos_bytes, positions.data());
    rhi->UpdateSSBO(position_prev_ssbo, 0, pos_bytes, positions.data());
    rhi->UpdateSSBO(position_rest_ssbo, 0, pos_bytes, positions.data());
    rhi->UpdateSSBO(tangent_ssbo,       0, pos_bytes, tangents.data());
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

} // namespace render
} // namespace dse
