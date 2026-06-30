/**
 * @file eqs_system.h
 * @brief 环境查询系统 (EQS) — 查询模板/生成器/评分器 + AI集成
 *
 * 功能：
 * - 查询模板（Query Template）：可配置的空间查询定义
 * - 生成器（Generator）：Grid/Ring/Cone/NavMesh/Random 等点生成策略
 * - 评分器（Scorer）：Distance/Visibility/Dot/Custom 等评分函数
 * - 多条件组合：加权和/乘法/最小值组合模式
 * - AI集成：与行为树/GOAP配合选择最佳位置（掩体/巡逻点/目标点）
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace ai {

/// 生成器类型
enum class GeneratorType : uint8_t {
    Grid = 0,        ///< 网格均匀生成
    Ring = 1,        ///< 环形等距生成
    Cone = 2,        ///< 锥形区域生成
    NavMesh = 3,     ///< NavMesh 表面采样
    Random = 4,      ///< 随机散布
    PathPoints = 5   ///< 沿路径点生成
};

/// 评分器类型
enum class ScorerType : uint8_t {
    Distance = 0,    ///< 距离评分（近=高 或 远=高）
    Visibility = 1,  ///< 可见性评分（被遮挡=高=掩体）
    DotProduct = 2,  ///< 方向点积评分
    Height = 3,      ///< 高度评分
    Reachable = 4,   ///< 可达性（navmesh路径检查）
    Custom = 5       ///< 自定义 Lua 回调评分
};

/// 评分组合模式
enum class CombineMode : uint8_t {
    WeightedSum = 0, ///< 加权求和
    Multiply = 1,    ///< 乘法（任一为0则排除）
    Minimum = 2      ///< 取最小值
};

/// 生成器配置
struct GeneratorConfig {
    GeneratorType type = GeneratorType::Grid;
    glm::vec3 center{0.0f};      ///< 生成中心（可为 querier 位置）
    float radius = 20.0f;        ///< 生成半径
    float spacing = 2.0f;        ///< 点间距（Grid/Ring）
    float inner_radius = 5.0f;   ///< 内径（Ring/Cone 排除区域）
    float cone_angle = 90.0f;    ///< 锥形角度（度）
    glm::vec3 direction{1, 0, 0};///< 方向（Cone/Dot）
    int max_points = 200;        ///< 最大生成点数
    float height_offset = 0.5f;  ///< 高度偏移（离地高度）
};

/// 评分器配置
struct ScorerConfig {
    ScorerType type = ScorerType::Distance;
    float weight = 1.0f;         ///< 权重
    bool invert = false;         ///< 反转分数（1-score）
    glm::vec3 reference_point{0.0f}; ///< 参考点（Distance/Dot）
    glm::vec3 reference_dir{1, 0, 0};///< 参考方向（Dot）
    float min_value = 0.0f;      ///< 最小有效值（低于此值排除）
    float max_value = 1000.0f;   ///< 归一化最大距离
};

/// 查询候选点
struct QueryCandidate {
    glm::vec3 position;
    float score = 0.0f;          ///< 综合评分 [0,1]
    bool valid = true;           ///< 是否通过所有硬约束
    std::vector<float> individual_scores; ///< 各评分器单独分数
};

/// 查询结果
struct QueryResult {
    std::vector<QueryCandidate> candidates;
    glm::vec3 best_position{0.0f};
    float best_score = 0.0f;
    uint32_t total_generated = 0;
    uint32_t valid_count = 0;
    float query_time_ms = 0.0f;
};

/// 查询模板
struct QueryTemplate {
    std::string name;
    GeneratorConfig generator;
    std::vector<ScorerConfig> scorers;
    CombineMode combine_mode = CombineMode::WeightedSum;
    bool sort_descending = true;    ///< 结果按分数降序
    uint32_t max_results = 10;      ///< 返回前 N 个结果
};

/// 自定义评分回调
using CustomScorerFunc = std::function<float(const glm::vec3& candidate, const glm::vec3& querier)>;

/// 可见性检测回调
using VisibilityFunc = std::function<bool(const glm::vec3& from, const glm::vec3& to)>;

/// 高度采样回调
using HeightSampleFunc = std::function<float(float x, float z)>;

/// 环境查询系统
class DSE_EXPORT EQSSystem {
public:
    EQSSystem() = default;
    ~EQSSystem() = default;

    void Init();
    void Shutdown();

    // === 模板管理 ===

    /// 创建查询模板，返回 template_id
    uint32_t CreateTemplate(const std::string& name);

    /// 删除模板
    void DestroyTemplate(uint32_t template_id);

    /// 设置模板的生成器
    void SetGenerator(uint32_t template_id, const GeneratorConfig& config);

    /// 添加评分器到模板
    void AddScorer(uint32_t template_id, const ScorerConfig& config);

    /// 清除模板的所有评分器
    void ClearScorers(uint32_t template_id);

    /// 设置组合模式
    void SetCombineMode(uint32_t template_id, CombineMode mode);

    /// 设置最大返回数
    void SetMaxResults(uint32_t template_id, uint32_t max_results);

    /// 获取模板数量
    uint32_t GetTemplateCount() const { return static_cast<uint32_t>(templates_.size()); }

    // === 执行查询 ===

    /// 执行查询：使用指定模板，从 querier_pos 出发
    QueryResult Execute(uint32_t template_id, const glm::vec3& querier_pos) const;

    /// 执行查询（覆盖中心点）
    QueryResult ExecuteAt(uint32_t template_id, const glm::vec3& querier_pos,
                          const glm::vec3& custom_center) const;

    // === 回调设置 ===

    /// 设置可见性检测回调（用于 Visibility 评分器）
    void SetVisibilityFunc(VisibilityFunc func) { visibility_func_ = std::move(func); }

    /// 设置高度采样回调
    void SetHeightSampleFunc(HeightSampleFunc func) { height_func_ = std::move(func); }

    /// 注册自定义评分回调
    uint32_t RegisterCustomScorer(const std::string& name, CustomScorerFunc func);

    /// 设置自定义评分器关联
    void SetCustomScorerForTemplate(uint32_t template_id, uint32_t scorer_index, uint32_t custom_id);

private:
    std::vector<QueryCandidate> GeneratePoints(const GeneratorConfig& config,
                                                const glm::vec3& querier_pos) const;
    float ScoreCandidate(const QueryCandidate& candidate, const ScorerConfig& scorer,
                         const glm::vec3& querier_pos) const;

    std::vector<QueryTemplate> templates_;
    std::vector<uint32_t> template_ids_;
    uint32_t next_id_ = 0;

    VisibilityFunc visibility_func_;
    HeightSampleFunc height_func_;
    std::vector<std::pair<std::string, CustomScorerFunc>> custom_scorers_;

    bool initialized_ = false;
};

} // namespace ai
} // namespace dse
