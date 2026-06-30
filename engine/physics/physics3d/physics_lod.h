/**
 * @file physics_lod.h
 * @brief 物理 LOD / 休眠系统
 *
 * 基于距离的物理模拟优化：
 * - 远距离刚体自动休眠（完全停止模拟）
 * - 中距离刚体降低模拟频率（每 N 帧更新一次）
 * - 简化碰撞体替换（远处用 AABB/Sphere 替代复杂凸包）
 * - 与 AI LOD 调度互补，物理侧独立管理
 *
 * 层级划分（可配置）：
 * - Full:     完整模拟频率 + 精确碰撞体
 * - Reduced:  降频模拟（1/2 或 1/4）+ 精确碰撞体
 * - Simplified: 降频模拟 + 简化碰撞体
 * - Sleep:    完全休眠，仅保留位置/旋转快照
 */

#ifndef DSE_PHYSICS_LOD_H
#define DSE_PHYSICS_LOD_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace dse {
namespace physics3d {

/// 物理 LOD 层级
enum class PhysicsLODLevel : uint8_t {
    Full = 0,       ///< 完整模拟
    Reduced,        ///< 降频模拟
    Simplified,     ///< 降频 + 简化碰撞体
    Sleep,          ///< 完全休眠
};

/// 单个物理体的 LOD 状态
struct PhysicsLODEntry {
    uint32_t entity_id = 0;
    glm::vec3 world_position = glm::vec3(0.0f);
    float bounding_radius = 1.0f;
    PhysicsLODLevel current_level = PhysicsLODLevel::Full;
    PhysicsLODLevel desired_level = PhysicsLODLevel::Full;
    uint32_t sim_frequency_divider = 1;    ///< 模拟频率除数（1=每帧, 2=隔帧, 4=每4帧）
    uint32_t frame_counter = 0;            ///< 当前帧计数（用于降频）
    bool collider_simplified = false;      ///< 碰撞体是否已简化
    bool is_sleeping = false;              ///< 是否在休眠状态
    float velocity_magnitude = 0.0f;       ///< 最近的速度大小（用于唤醒判断）

    // 休眠前的快照
    glm::vec3 sleep_position = glm::vec3(0.0f);
    glm::vec3 sleep_rotation_euler = glm::vec3(0.0f);
};

/// 物理 LOD 系统配置
struct PhysicsLODConfig {
    float full_distance = 50.0f;        ///< Full 层级最大距离
    float reduced_distance = 150.0f;    ///< Reduced 层级最大距离
    float simplified_distance = 300.0f; ///< Simplified 层级最大距离
    // 超出 simplified_distance → Sleep

    uint32_t reduced_frequency = 2;     ///< Reduced 层的模拟频率除数
    uint32_t simplified_frequency = 4;  ///< Simplified 层的模拟频率除数

    float sleep_velocity_threshold = 0.05f;  ///< 低于此速度允许休眠（m/s）
    float wake_velocity_threshold = 0.1f;    ///< 超过此速度强制唤醒
    float hysteresis_factor = 1.1f;          ///< 层级切换滞回系数

    bool enable_collider_simplification = true; ///< 是否启用碰撞体简化
    bool enable_frequency_reduction = true;     ///< 是否启用降频模拟
};

/**
 * @class PhysicsLODSystem
 * @brief 管理物理体的 LOD 状态，决定每帧哪些物体需要模拟
 */
class PhysicsLODSystem {
public:
    PhysicsLODSystem() = default;
    ~PhysicsLODSystem() = default;

    void Init(const PhysicsLODConfig& config = {});
    void Shutdown();

    /// 注册物理体
    uint32_t RegisterBody(uint32_t entity_id, const glm::vec3& position, float radius);

    /// 移除物理体
    void UnregisterBody(uint32_t entity_id);

    /// 更新物理体位置和速度
    void UpdateBodyState(uint32_t entity_id, const glm::vec3& position, float velocity_mag);

    /// 每帧评估：根据摄像机距离更新所有物理体的 LOD 状态
    /// @return 本帧需要模拟的物理体 entity_id 列表
    std::vector<uint32_t> Evaluate(const glm::vec3& camera_position, uint32_t frame_number);

    /// 强制唤醒某个物理体
    void WakeBody(uint32_t entity_id);

    /// 强制休眠某个物理体
    void SleepBody(uint32_t entity_id);

    // ========== 查询 ==========

    PhysicsLODLevel GetBodyLevel(uint32_t entity_id) const;
    bool IsBodySleeping(uint32_t entity_id) const;
    bool ShouldSimulateThisFrame(uint32_t entity_id, uint32_t frame_number) const;
    uint32_t GetRegisteredBodyCount() const;
    uint32_t GetSleepingBodyCount() const;
    uint32_t GetActiveBodyCount() const;

    /// 统计各层级物理体数量
    struct LevelStats {
        uint32_t full = 0;
        uint32_t reduced = 0;
        uint32_t simplified = 0;
        uint32_t sleeping = 0;
    };
    LevelStats GetLevelStats() const;

    /// Floating Origin 偏移
    void RebaseOrigin(const glm::vec3& offset);

    const PhysicsLODConfig& GetConfig() const { return config_; }

private:
    PhysicsLODLevel EvaluateLevel(float distance, float velocity) const;

    PhysicsLODConfig config_;
    std::unordered_map<uint32_t, PhysicsLODEntry> bodies_;
    bool initialized_ = false;
};

} // namespace physics3d
} // namespace dse

#endif // DSE_PHYSICS_LOD_H
