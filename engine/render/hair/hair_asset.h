/**
 * @file hair_asset.h
 * @brief TressFX 风格毛发资产数据结构
 *
 * 定义 strand-based 毛发的 CPU 数据表示：
 * - HairStrandVertex: 逐顶点位置 + 切线
 * - HairStrand: 一根发丝的顶点序列
 * - HairAsset: 整个毛发资产（guide strands + 参数）
 *
 * 依赖方向: engine/ 层，不依赖 modules/ 或 apps/
 */

#ifndef DSE_RENDER_HAIR_ASSET_H
#define DSE_RENDER_HAIR_ASSET_H

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace dse {
namespace render {

/// 毛发顶点（逐控制点）
struct HairStrandVertex {
    glm::vec4 position;   ///< xyz=世界坐标, w=rest_length (到上一顶点的静止长度)
    glm::vec4 tangent;    ///< xyz=切线方向, w=thickness (0..1 从根到梢)
};

/// 单根发丝描述
struct HairStrand {
    uint32_t vertex_offset = 0;  ///< 在全局 vertex 数组中的起始索引
    uint32_t vertex_count  = 0;  ///< 本发丝顶点数
};

/// 毛发资产（CPU 端数据，可序列化）
struct HairAsset {
    std::string name;

    /// 所有 guide strands 的顶点（紧密排列）
    std::vector<HairStrandVertex> vertices;

    /// guide strands 描述
    std::vector<HairStrand> strands;

    /// 每根发丝的固定顶点数（TressFX 约定所有发丝等长）
    uint32_t vertices_per_strand = 16;

    /// guide strand 数量
    uint32_t num_guide_strands() const { return static_cast<uint32_t>(strands.size()); }

    /// 总顶点数
    uint32_t num_vertices() const { return static_cast<uint32_t>(vertices.size()); }

    /// 每根 guide strand 展开的 follower strand 数量
    uint32_t num_follow_per_guide = 4;

    /// follower strand 最大偏移距离
    float follow_root_offset_range = 1.5f;

    /// 发丝根部固定到头皮的骨骼索引（可选，-1 表示无骨骼绑定）
    std::vector<int32_t> root_bone_indices;

    /// 发丝根部的重心坐标（相对于三角面片，用于头皮绑定）
    std::vector<glm::vec3> root_barycentric;

    /// 发丝根部三角面片索引
    std::vector<uint32_t> root_triangle_indices;

    /// 验证数据完整性
    bool IsValid() const {
        if (strands.empty() || vertices.empty()) return false;
        if (vertices_per_strand == 0) return false;
        return vertices.size() == strands.size() * vertices_per_strand;
    }
};

/// 从 .tfx (TressFX binary) 或 .dhair (DSEngine 自定义) 加载毛发资产
/// @param file_path 文件路径
/// @param[out] out_asset 输出资产
/// @return true 加载成功
bool LoadHairAsset(const std::string& file_path, HairAsset& out_asset);

/// 程序化生成测试用毛发资产（球面均匀分布）
/// @param num_guide_strands guide strand 数量
/// @param verts_per_strand  每根发丝顶点数
/// @param hair_length       发丝长度
/// @param sphere_radius     球面半径
/// @param[out] out_asset    输出资产
void GenerateTestHairAsset(uint32_t num_guide_strands,
                            uint32_t verts_per_strand,
                            float hair_length,
                            float sphere_radius,
                            HairAsset& out_asset);

} // namespace render
} // namespace dse

#endif // DSE_RENDER_HAIR_ASSET_H
