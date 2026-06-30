/**
 * @file terrain_deformation.h
 * @brief 运行时地形形变系统
 *
 * 支持实时修改高度图：
 * - 挖洞/爆炸坑（球形/圆柱形凹陷）
 * - 玩家建造（高度抬升）
 * - 任意 brush 形变（自定义高度偏移图）
 * - 形变历史记录 + 撤销
 * - 与 GeometryClipmap 集成：修改后标记 dirty 触发 GPU 重新上传
 * - 持久化：形变数据序列化/反序列化
 */

#ifndef DSE_TERRAIN_DEFORMATION_H
#define DSE_TERRAIN_DEFORMATION_H

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace dse {
namespace terrain {

class GeometryClipmapSystem;

/// 形变操作类型
enum class DeformationType : uint8_t {
    Raise = 0,      ///< 抬升（建造）
    Lower,          ///< 降低（挖洞）
    Flatten,        ///< 压平到指定高度
    Smooth,         ///< 平滑（周围平均）
    Noise,          ///< 噪声叠加
    Stamp,          ///< 印章式形变（自定义 heightmap 贴花）
};

/// 形变 brush 形状
enum class BrushShape : uint8_t {
    Circle = 0,     ///< 圆形（球形衰减）
    Square,         ///< 方形
    Custom,         ///< 自定义 alpha 图
};

/// 单次形变操作描述
struct DeformationOp {
    DeformationType type = DeformationType::Lower;
    BrushShape shape = BrushShape::Circle;
    glm::vec3 center = glm::vec3(0.0f);  ///< 世界坐标中心
    float radius = 5.0f;                  ///< brush 半径（世界单位）
    float strength = 1.0f;                ///< 形变强度 [0,1]
    float target_height = 0.0f;           ///< Flatten 模式的目标高度
    float falloff = 1.0f;                 ///< 边缘衰减指数（1=线性, 2=二次）
    std::vector<float> custom_brush;      ///< Custom brush 高度偏移图（可选）
    uint32_t custom_brush_size = 0;       ///< Custom brush 边长
};

/// 形变记录条目（用于撤销/持久化）
struct DeformationRecord {
    uint32_t id = 0;
    DeformationOp op;
    float timestamp = 0.0f;
    // 原始高度备份（用于撤销）
    std::vector<float> original_heights;
    int grid_x = 0, grid_z = 0;          ///< 影响区域在 heightmap 中的起始坐标
    int grid_width = 0, grid_height_count = 0;
};

/// 形变系统配置
struct TerrainDeformConfig {
    float max_deformation_depth = 50.0f;  ///< 最大允许凹陷深度
    float max_deformation_height = 50.0f; ///< 最大允许抬升高度
    uint32_t max_history = 256;           ///< 最大历史记录数
    float min_height = -1000.0f;          ///< 高度下限
    float max_height = 1000.0f;           ///< 高度上限
    bool enable_history = true;           ///< 是否记录历史
};

/// 形变回调：通知外部系统高度图已变更
using DeformationCallback = std::function<void(const glm::vec3& center, float radius)>;

/**
 * @class TerrainDeformationSystem
 * @brief 管理地形实时形变
 */
class TerrainDeformationSystem {
public:
    TerrainDeformationSystem() = default;
    ~TerrainDeformationSystem() = default;

    void Init(const TerrainDeformConfig& config = {});
    void Shutdown();

    /// 关联 GeometryClipmap（形变后自动标记 dirty）
    void SetClipmapSystem(GeometryClipmapSystem* clipmap) { clipmap_ = clipmap; }

    /// 设置高度图数据指针（直接修改此数据）
    /// @param data 高度图数据指针
    /// @param width 高度图宽度
    /// @param height 高度图高度（行数）
    /// @param cell_size 每格世界大小
    /// @param origin 高度图左下角世界坐标
    void SetHeightmap(float* data, uint32_t width, uint32_t height,
                      float cell_size, const glm::vec3& origin);

    /// 执行一次形变操作
    /// @return 操作 ID（用于撤销），失败返回 0
    uint32_t ApplyDeformation(const DeformationOp& op);

    /// 撤销最近一次形变
    bool Undo();

    /// 重做上一次撤销的形变
    bool Redo();

    /// 清空所有形变历史（不恢复高度）
    void ClearHistory();

    /// 获取指定世界坐标的当前高度
    float SampleHeight(float world_x, float world_z) const;

    /// 注册形变回调
    void SetDeformationCallback(DeformationCallback cb) { callback_ = std::move(cb); }

    // ========== 持久化 ==========

    /// 序列化所有形变记录到二进制
    bool Serialize(const std::string& path) const;

    /// 反序列化并重新应用所有形变
    bool Deserialize(const std::string& path);

    // ========== 查询 ==========

    uint32_t GetHistoryCount() const { return static_cast<uint32_t>(history_.size()); }
    uint32_t GetUndoCount() const { return static_cast<uint32_t>(undo_stack_.size()); }
    const TerrainDeformConfig& GetConfig() const { return config_; }
    bool HasHeightmap() const { return heightmap_ != nullptr; }
    uint32_t GetHeightmapWidth() const { return hm_width_; }
    uint32_t GetHeightmapHeight() const { return hm_height_; }

private:
    void ApplyOpToHeightmap(const DeformationOp& op, DeformationRecord& record);
    float ComputeBrushWeight(const DeformationOp& op, float dist) const;
    void WorldToGrid(float wx, float wz, int& gx, int& gz) const;
    void NotifyChange(const glm::vec3& center, float radius);

    TerrainDeformConfig config_;
    float* heightmap_ = nullptr;
    uint32_t hm_width_ = 0;
    uint32_t hm_height_ = 0;
    float hm_cell_size_ = 1.0f;
    glm::vec3 hm_origin_ = glm::vec3(0.0f);

    GeometryClipmapSystem* clipmap_ = nullptr;
    DeformationCallback callback_;

    std::vector<DeformationRecord> history_;
    std::vector<DeformationRecord> undo_stack_;
    uint32_t next_record_id_ = 1;
    bool initialized_ = false;
};

} // namespace terrain
} // namespace dse

#endif // DSE_TERRAIN_DEFORMATION_H
