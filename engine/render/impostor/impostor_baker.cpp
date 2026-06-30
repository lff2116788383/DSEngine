/**
 * @file impostor_baker.cpp
 * @brief ImpostorBaker 实现 — 多角度渲染 + atlas 拼接 + .dimpostor 读写
 */

#include "engine/render/impostor/impostor_baker.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>

namespace dse {
namespace render {

ImpostorBakeResult ImpostorBaker::Bake(RhiDevice& device,
                                       const float* vertices, int vertex_count, int vertex_stride_floats,
                                       const uint32_t* indices, int index_count,
                                       const glm::vec3& bounds_min, const glm::vec3& bounds_max,
                                       const ImpostorBakeConfig& config) {
    ImpostorBakeResult result;
    result.atlas_width = config.frames_x * config.frame_resolution;
    result.atlas_height = config.frames_y * config.frame_resolution;

    const glm::vec3 center = (bounds_min + bounds_max) * 0.5f;
    const float radius = glm::length(bounds_max - bounds_min) * 0.5f;

    // 分配 atlas 像素缓冲
    const int atlas_pixels = result.atlas_width * result.atlas_height;
    result.albedo_rgba.resize(atlas_pixels * 4, 0);
    if (config.bake_normals) {
        result.normal_rgb.resize(atlas_pixels * 3, 128);  // 默认 (0.5,0.5,1) 编码为 (128,128,255)
    }

    const int res = config.frame_resolution;

    // 创建临时 FBO（逐帧渲染）
    RenderTargetDesc rt_desc;
    rt_desc.width = res;
    rt_desc.height = res;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int bake_rt = device.CreateRenderTarget(rt_desc);
    if (bake_rt == 0) return result;

    for (int fy = 0; fy < config.frames_y; ++fy) {
        for (int fx = 0; fx < config.frames_x; ++fx) {
            glm::mat4 view_mat = ComputeViewForFrame(fx, fy, config.frames_x, config.frames_y,
                                                     config.hemi_only, center, radius);
            // 正交投影包围球
            float ortho_size = radius;
            glm::mat4 proj_mat = glm::ortho(-ortho_size, ortho_size,
                                            -ortho_size, ortho_size,
                                            0.01f, radius * 4.0f);

            // 渲染到 bake_rt（使用设备的 ImmediateDraw 或简化路径）
            // 注：这里使用 ReadRenderTargetColorRgba8WithSize 回读像素。
            // 具体渲染调用依赖设备能力 — 这里先准备正交 VP 和 mesh 数据，
            // 实际绘制通过一个临时 shader program + 顶点数据直接 DrawImmediate。
            ImmediateDrawDesc draw_desc;
            draw_desc.render_target = bake_rt;
            draw_desc.clear = true;
            draw_desc.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            draw_desc.depth_test = true;
            draw_desc.blend = false;
            draw_desc.viewport = glm::ivec4(0, 0, res, res);

            // 简化路径：使用设备内建 ForwardPbr 程序绘制白色 mesh
            unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbr);
            if (program == 0) continue;
            draw_desc.shader_program = program;

            // 设置 MVP uniform（投影到像素空间）
            glm::mat4 mvp = proj_mat * view_mat;
            draw_desc.uniforms_vec4.push_back({"u_mvp_row0", glm::vec4(mvp[0])});
            draw_desc.uniforms_vec4.push_back({"u_mvp_row1", glm::vec4(mvp[1])});
            draw_desc.uniforms_vec4.push_back({"u_mvp_row2", glm::vec4(mvp[2])});
            draw_desc.uniforms_vec4.push_back({"u_mvp_row3", glm::vec4(mvp[3])});

            // 提交顶点数据
            draw_desc.vertices = vertices;
            draw_desc.vertex_bytes = static_cast<size_t>(vertex_count * vertex_stride_floats * sizeof(float));
            draw_desc.vertex_count = index_count > 0 ? index_count : vertex_count;
            draw_desc.stride_bytes = vertex_stride_floats * static_cast<int>(sizeof(float));
            draw_desc.attribs = {
                {0, 3, 0},   // position
                {1, 4, 12},  // color
                {2, 2, 28},  // uv
                {3, 3, 36},  // normal
            };

            device.ImmediateDraw(draw_desc);

            // 回读像素
            RenderTargetReadback readback = device.ReadRenderTargetColorRgba8WithSize(bake_rt);
            int out_w = readback.width;
            int out_h = readback.height;
            auto& frame_pixels = readback.pixels;
            if (!frame_pixels.empty()) {
                // 复制到 atlas 对应位置
                int dst_x = fx * res;
                int dst_y = fy * res;
                for (int row = 0; row < res && row < out_h; ++row) {
                    int src_offset = row * out_w * 4;
                    int dst_offset = ((dst_y + row) * result.atlas_width + dst_x) * 4;
                    int copy_bytes = std::min(res, out_w) * 4;
                    if (src_offset + copy_bytes <= static_cast<int>(frame_pixels.size()) &&
                        dst_offset + copy_bytes <= static_cast<int>(result.albedo_rgba.size())) {
                        std::memcpy(&result.albedo_rgba[dst_offset],
                                    &frame_pixels[src_offset], copy_bytes);
                    }
                }
            }
        }
    }

    device.DeleteRenderTarget(bake_rt);
    result.success = true;
    return result;
}

bool ImpostorBaker::SaveToFile(const std::string& path,
                               const ImpostorBakeResult& result,
                               const ImpostorBakeConfig& config,
                               float bounds_radius) {
    if (!result.success) return false;

    DimpostorHeader header{};
    header.atlas_width = static_cast<uint32_t>(result.atlas_width);
    header.atlas_height = static_cast<uint32_t>(result.atlas_height);
    header.frames_x = static_cast<uint32_t>(config.frames_x);
    header.frames_y = static_cast<uint32_t>(config.frames_y);
    header.frame_resolution = static_cast<uint32_t>(config.frame_resolution);
    header.bounds_radius = bounds_radius;

    uint32_t data_offset = sizeof(DimpostorHeader);
    header.albedo_offset = data_offset;
    header.albedo_size = static_cast<uint32_t>(result.albedo_rgba.size());
    data_offset += header.albedo_size;

    if (!result.normal_rgb.empty()) {
        header.flags |= 1u;  // has_normals
        header.normal_offset = data_offset;
        header.normal_size = static_cast<uint32_t>(result.normal_rgb.size());
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(result.albedo_rgba.data()), result.albedo_rgba.size());
    if (!result.normal_rgb.empty()) {
        file.write(reinterpret_cast<const char*>(result.normal_rgb.data()), result.normal_rgb.size());
    }

    return file.good();
}

bool ImpostorBaker::LoadFromFile(const std::string& path,
                                 RhiDevice& device,
                                 unsigned int& out_albedo_tex,
                                 unsigned int& out_normal_tex,
                                 int& out_frames_x, int& out_frames_y,
                                 float& out_bounds_radius) {
    out_albedo_tex = out_normal_tex = 0;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    DimpostorHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file.good()) return false;

    // 验证 magic
    if (std::memcmp(header.magic, "DIMPOSTR", 8) != 0) return false;

    out_frames_x = static_cast<int>(header.frames_x);
    out_frames_y = static_cast<int>(header.frames_y);
    out_bounds_radius = header.bounds_radius;

    // 读取 albedo 数据
    std::vector<uint8_t> albedo_data(header.albedo_size);
    file.seekg(header.albedo_offset);
    file.read(reinterpret_cast<char*>(albedo_data.data()), header.albedo_size);
    if (!file.good()) return false;

    out_albedo_tex = device.CreateTexture2D(
        static_cast<int>(header.atlas_width),
        static_cast<int>(header.atlas_height),
        albedo_data.data(), true);

    // 读取 normal 数据（如果存在）
    if (header.flags & 1u) {
        std::vector<uint8_t> normal_data(header.normal_size);
        file.seekg(header.normal_offset);
        file.read(reinterpret_cast<char*>(normal_data.data()), header.normal_size);
        if (file.good()) {
            // RGB → RGBA（纹理创建需要 4 通道）
            int px_count = static_cast<int>(header.atlas_width * header.atlas_height);
            std::vector<uint8_t> normal_rgba(px_count * 4);
            for (int i = 0; i < px_count; ++i) {
                normal_rgba[i * 4 + 0] = normal_data[i * 3 + 0];
                normal_rgba[i * 4 + 1] = normal_data[i * 3 + 1];
                normal_rgba[i * 4 + 2] = normal_data[i * 3 + 2];
                normal_rgba[i * 4 + 3] = 255;
            }
            out_normal_tex = device.CreateTexture2D(
                static_cast<int>(header.atlas_width),
                static_cast<int>(header.atlas_height),
                normal_rgba.data(), true);
        }
    }

    return out_albedo_tex != 0;
}

glm::mat4 ImpostorBaker::ComputeViewForFrame(int fx, int fy, int frames_x, int frames_y,
                                              bool hemi_only,
                                              const glm::vec3& center, float radius) {
    // 水平角：均匀分布 [0, 2π)
    float azimuth = (static_cast<float>(fx) + 0.5f) / static_cast<float>(frames_x) * 2.0f * glm::pi<float>();

    // 垂直角：
    // hemi_only: [0, π/2] （地面到天顶）
    // full: [-π/2, π/2]
    float elevation;
    if (hemi_only) {
        elevation = (static_cast<float>(fy) + 0.5f) / static_cast<float>(frames_y) * glm::half_pi<float>();
    } else {
        elevation = ((static_cast<float>(fy) + 0.5f) / static_cast<float>(frames_y) - 0.5f) * glm::pi<float>();
    }

    // 球面坐标到相机位置
    float cos_el = std::cos(elevation);
    glm::vec3 eye_dir = glm::vec3(
        std::sin(azimuth) * cos_el,
        std::sin(elevation),
        std::cos(azimuth) * cos_el
    );
    glm::vec3 eye_pos = center + eye_dir * radius * 2.0f;

    glm::vec3 up = (std::abs(elevation) > glm::half_pi<float>() - 0.01f)
                   ? glm::vec3(0.0f, 0.0f, 1.0f)
                   : glm::vec3(0.0f, 1.0f, 0.0f);

    return glm::lookAt(eye_pos, center, up);
}

} // namespace render
} // namespace dse
