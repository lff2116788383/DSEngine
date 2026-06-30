/**
 * @file world_state_persistence.h
 * @brief 持久化世界状态系统
 *
 * 按 Cell 粒度保存/加载玩家对世界的修改（破坏、建造、物体移动等）：
 * - 每个 Cell 对应一个 .dcell_state 增量文件
 * - 仅存储与原始场景的差异（delta），最小化磁盘占用
 * - 支持撤销到原始状态（reset cell）
 * - 与 WorldPartitionSystem 集成：加载 Cell 时自动合并修改
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "engine/core/dse_export.h"

namespace dse {

/// 实体修改类型
enum class EntityModType : uint8_t {
    Spawned = 0,    ///< 玩家新建的实体
    Destroyed,      ///< 被删除的原始实体
    Modified,       ///< 组件字段被修改
};

/// 单个实体修改记录
struct EntityModRecord {
    uint64_t entity_id = 0;         ///< 稳定 entity ID（场景内唯一）
    EntityModType type = EntityModType::Modified;
    std::string component_name;     ///< 修改的组件名（Modified 时有效）
    std::string field_name;         ///< 修改的字段名
    std::vector<uint8_t> new_value; ///< 序列化后的新值（spawn 时为完整组件数据）
};

/// Cell 修改集合
struct CellStateData {
    int cell_x = 0;
    int cell_y = 0;
    uint32_t version = 1;           ///< 格式版本
    uint64_t timestamp = 0;         ///< 最后修改时间戳（unix ms）
    std::vector<EntityModRecord> modifications;
};

/// Cell 坐标 key
struct CellKey {
    int x = 0;
    int y = 0;
    bool operator==(const CellKey& o) const { return x == o.x && y == o.y; }
};

struct CellKeyHash {
    size_t operator()(const CellKey& k) const {
        size_t h = std::hash<int>()(k.x);
        h ^= std::hash<int>()(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

/// 世界状态持久化系统
class DSE_EXPORT WorldStatePersistence {
public:
    WorldStatePersistence() = default;
    ~WorldStatePersistence() = default;

    /// 初始化，设置存档目录
    void Init(const std::string& save_directory);

    /// 记录实体修改
    void RecordModification(int cell_x, int cell_y, const EntityModRecord& record);

    /// 记录实体新建
    void RecordSpawn(int cell_x, int cell_y, uint64_t entity_id,
                     const std::vector<uint8_t>& serialized_data);

    /// 记录实体删除
    void RecordDestruction(int cell_x, int cell_y, uint64_t entity_id);

    /// 获取某 Cell 的所有修改（用于加载 Cell 后合并）
    const CellStateData* GetCellState(int cell_x, int cell_y) const;

    /// 保存某 Cell 状态到磁盘
    bool SaveCell(int cell_x, int cell_y);

    /// 从磁盘加载某 Cell 状态（Cell 加载时自动调用）
    bool LoadCell(int cell_x, int cell_y);

    /// 重置某 Cell 到原始状态（删除增量文件）
    void ResetCell(int cell_x, int cell_y);

    /// 保存所有 dirty cell 到磁盘
    void SaveAll();

    /// 获取有修改的 Cell 数量
    size_t DirtyCellCount() const { return dirty_cells_.size(); }

    /// 获取总修改记录数
    size_t TotalModificationCount() const;

    void Shutdown();

private:
    std::string BuildCellStatePath(int cell_x, int cell_y) const;

    std::string save_directory_;
    std::unordered_map<CellKey, CellStateData, CellKeyHash> cell_states_;
    std::unordered_map<CellKey, bool, CellKeyHash> dirty_cells_;
    bool initialized_ = false;
};

} // namespace dse
