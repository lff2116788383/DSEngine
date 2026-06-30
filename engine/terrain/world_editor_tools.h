/**
 * @file world_editor_tools.h
 * @brief 编辑器世界编辑工具 — 地形笔刷/植被画刷/道路绘制/Partition可视化
 *
 * 功能：
 * - 地形笔刷：高度/平滑/侵蚀/Splat绘制（圆形/方形/自定义形状）
 * - 植被画刷：基于密度的散布放置 + 碰撞检测 + 实例管理
 * - 道路绘制工具：与SplineSystem联动的道路布线
 * - World Partition可视化：Cell边界/加载状态/LOD层级可视化
 * - Undo/Redo：所有操作支持完整撤销/重做
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace terrain {

/// 笔刷形状
enum class BrushShape : uint8_t {
    Circle = 0,
    Square = 1,
    Custom = 2     ///< 使用自定义 alpha mask
};

/// 地形笔刷操作类型
enum class TerrainBrushOp : uint8_t {
    RaiseHeight = 0,
    LowerHeight = 1,
    SmoothHeight = 2,
    FlattenHeight = 3,
    ErodeThermal = 4,
    ErodeHydraulic = 5,
    PaintSplat = 6,     ///< 绘制 splat 权重
    PaintHole = 7       ///< 标记地形孔洞
};

/// 植被放置模式
enum class FoliagePlaceMode : uint8_t {
    Single = 0,         ///< 单个放置
    Scatter = 1,        ///< 密度散布
    Erase = 2,          ///< 擦除范围内实例
    Align = 3           ///< 对齐到法线
};

/// 笔刷参数
struct BrushParams {
    glm::vec3 center{0.0f};         ///< 笔刷中心世界坐标
    float radius = 10.0f;           ///< 笔刷半径
    float strength = 0.5f;          ///< 强度 [0,1]
    float falloff = 0.5f;           ///< 衰减曲线 [0=硬边, 1=线性]
    BrushShape shape = BrushShape::Circle;
    float target_height = 0.0f;     ///< Flatten 目标高度
    uint32_t splat_layer = 0;       ///< PaintSplat 目标层
};

/// 植被画刷参数
struct FoliageBrushParams {
    glm::vec3 center{0.0f};
    float radius = 20.0f;
    float density = 0.5f;           ///< 每平方米实例数
    float min_scale = 0.8f;
    float max_scale = 1.2f;
    float random_rotation = 360.0f; ///< 随机旋转范围（度）
    float slope_limit = 45.0f;      ///< 最大放置坡度
    bool align_to_normal = true;
    std::string mesh_path;          ///< 使用的mesh资源路径
    FoliagePlaceMode mode = FoliagePlaceMode::Scatter;
};

/// 植被实例
struct FoliageInstance {
    glm::vec3 position;
    glm::vec3 rotation;     ///< 欧拉角
    float scale = 1.0f;
    std::string mesh_path;
    uint32_t instance_id = 0;
};

/// Cell可视化状态
struct CellVisualState {
    int cell_x = 0;
    int cell_y = 0;
    bool loaded = false;
    int lod_level = 0;
    float distance_to_camera = 0.0f;
    uint32_t entity_count = 0;
};

/// 编辑器操作记录（用于Undo/Redo）
struct EditorOperation {
    enum Type { TerrainEdit, FoliagePlace, FoliageErase, RoadDraw };
    Type type;
    std::vector<uint8_t> undo_data;
    std::vector<uint8_t> redo_data;
    std::string description;
};

/// 世界编辑器工具系统
class DSE_EXPORT WorldEditorTools {
public:
    WorldEditorTools() = default;
    ~WorldEditorTools() = default;

    void Init();
    void Shutdown();

    // === 地形笔刷 ===

    /// 应用地形笔刷（返回操作ID用于 undo）
    uint32_t ApplyTerrainBrush(TerrainBrushOp op, const BrushParams& params);

    /// 获取笔刷预览：返回受影响区域的 AABB (min_x, min_z, max_x, max_z)
    glm::vec4 GetBrushPreview(const BrushParams& params) const;

    /// 计算笔刷影响权重
    float CalculateBrushWeight(const glm::vec3& point, const BrushParams& params) const;

    // === 植被画刷 ===

    /// 执行植被散布放置
    uint32_t PlaceFoliage(const FoliageBrushParams& params);

    /// 擦除范围内植被
    uint32_t EraseFoliage(const glm::vec3& center, float radius);

    /// 获取指定范围内的植被实例
    std::vector<FoliageInstance> GetFoliageInRadius(const glm::vec3& center, float radius) const;

    /// 获取植被实例总数
    uint32_t GetFoliageCount() const { return static_cast<uint32_t>(foliage_instances_.size()); }

    // === 道路绘制 ===

    /// 开始道路绘制会话
    uint32_t BeginRoadDraw(float road_width);

    /// 添加道路控制点
    void AddRoadPoint(uint32_t session_id, const glm::vec3& point);

    /// 完成道路绘制
    void EndRoadDraw(uint32_t session_id);

    /// 取消道路绘制
    void CancelRoadDraw(uint32_t session_id);

    // === World Partition 可视化 ===

    /// 更新可视化数据
    void UpdatePartitionVisualization(const glm::vec3& camera_pos, float cell_size);

    /// 获取所有 Cell 的可视化状态
    const std::vector<CellVisualState>& GetCellStates() const { return cell_states_; }

    /// 获取可视化范围内的 Cell 数量
    uint32_t GetVisibleCellCount() const { return static_cast<uint32_t>(cell_states_.size()); }

    // === Undo / Redo ===

    bool Undo();
    bool Redo();
    uint32_t GetUndoCount() const { return static_cast<uint32_t>(undo_stack_.size()); }
    uint32_t GetRedoCount() const { return static_cast<uint32_t>(redo_stack_.size()); }

    /// 设置高度图写入回调
    using HeightWriteFunc = std::function<void(int gx, int gz, float height)>;
    void SetHeightWriteFunc(HeightWriteFunc func) { height_write_func_ = func; }

    /// 设置高度采样回调
    using HeightReadFunc = std::function<float(float x, float z)>;
    void SetHeightReadFunc(HeightReadFunc func) { height_read_func_ = func; }

private:
    void PushUndoOp(EditorOperation op);
    float SampleFalloff(float distance, float radius, float falloff) const;

    std::vector<FoliageInstance> foliage_instances_;
    std::vector<CellVisualState> cell_states_;
    std::vector<EditorOperation> undo_stack_;
    std::vector<EditorOperation> redo_stack_;

    HeightWriteFunc height_write_func_;
    HeightReadFunc height_read_func_;

    uint32_t next_instance_id_ = 1;
    uint32_t next_op_id_ = 1;
    uint32_t next_road_session_ = 1;
    bool initialized_ = false;
};

} // namespace terrain
} // namespace dse
