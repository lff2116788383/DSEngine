/**
 * @file impostor_baker.h
 * @brief Impostor Atlas 烘焙工具 — 从 mesh 多角度渲染生成 billboard atlas
 *
 * 离线/编辑器工具：对给定 mesh 从 N×M 个视角渲染到正交投影 FBO，
 * 拼接为单张 atlas 纹理（albedo RGBA + normal RGB），导出为 .dimpostor 格式。
 *
 * 使用方式：
 * 1. 编辑器中选中实体 → 右键 "Generate Impostor Atlas"
 * 2. 或 AssetBuilder CLI: --impostor <mesh.dmesh> <out.dimpostor> [--frames 12x3]
 */

#ifndef DSE_RENDER_IMPOSTOR_BAKER_H
#define DSE_RENDER_IMPOSTOR_BAKER_H

#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace dse {
namespace render {

class RhiDevice;
class CommandBuffer;

/// 烘焙参数
struct ImpostorBakeConfig {
    int frames_x = 12;          ///< 水平帧数
    int frames_y = 3;           ///< 垂直帧数
    int frame_resolution = 256; ///< 每帧分辨率（像素）
    bool bake_normals = true;   ///< 同时烘焙法线 atlas
    bool hemi_only = true;      ///< 仅半球（地面物体）vs 全球
    float padding = 2.0f;       ///< 帧间像素 padding（减少采样渗透）
};

/// 烘焙结果（CPU 侧像素数据）
struct ImpostorBakeResult {
    int atlas_width = 0;
    int atlas_height = 0;
    std::vector<uint8_t> albedo_rgba;  ///< RGBA8 像素数据
    std::vector<uint8_t> normal_rgb;   ///< RGB8 法线数据（可选）
    bool success = false;
};

/// .dimpostor 文件头
struct DimpostorHeader {
    char magic[8] = {'D','I','M','P','O','S','T','R'};
    uint32_t version = 1;
    uint32_t atlas_width = 0;
    uint32_t atlas_height = 0;
    uint32_t frames_x = 0;
    uint32_t frames_y = 0;
    uint32_t frame_resolution = 0;
    uint32_t flags = 0;            ///< bit0: has_normals
    uint32_t albedo_offset = 0;    ///< albedo 数据偏移（字节）
    uint32_t albedo_size = 0;      ///< albedo 数据大小
    uint32_t normal_offset = 0;    ///< normal 数据偏移
    uint32_t normal_size = 0;      ///< normal 数据大小
    float bounds_radius = 0.0f;    ///< mesh 包围球半径
    uint32_t reserved[4] = {};
};

/// Impostor Atlas 烘焙器
class ImpostorBaker {
public:
    ImpostorBaker() = default;
    ~ImpostorBaker() = default;

    /// 烘焙 atlas（需要 GPU 上下文可用）。
    /// vertices/indices 为原始 mesh 数据，bounds_min/max 为 AABB。
    ImpostorBakeResult Bake(RhiDevice& device,
                            const float* vertices, int vertex_count, int vertex_stride_floats,
                            const uint32_t* indices, int index_count,
                            const glm::vec3& bounds_min, const glm::vec3& bounds_max,
                            const ImpostorBakeConfig& config);

    /// 将烘焙结果保存为 .dimpostor 文件
    static bool SaveToFile(const std::string& path,
                           const ImpostorBakeResult& result,
                           const ImpostorBakeConfig& config,
                           float bounds_radius);

    /// 从 .dimpostor 文件加载 atlas 到 GPU
    static bool LoadFromFile(const std::string& path,
                             RhiDevice& device,
                             unsigned int& out_albedo_tex,
                             unsigned int& out_normal_tex,
                             int& out_frames_x, int& out_frames_y,
                             float& out_bounds_radius);

private:
    /// 计算第 (fx, fy) 帧的相机视角矩阵
    static glm::mat4 ComputeViewForFrame(int fx, int fy, int frames_x, int frames_y,
                                         bool hemi_only,
                                         const glm::vec3& center, float radius);
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_IMPOSTOR_BAKER_H
