/**
 * @file editor_snapshot.h
 * @brief Registry 快照导出与对比，用于编辑器自动化测试验证
 */

#pragma once

#include <string>
#include <vector>
#include <entt/entt.hpp>

namespace dse::editor::test {

/**
 * @brief 导出 registry 中所有实体的可序列化组件为 JSON 字符串
 * @param registry 要导出的 registry
 * @return JSON 字符串快照
 *
 * 导出内容包括：EditorNameComponent, TransformComponent (position/rotation/scale)
 */
std::string ExportRegistrySnapshot(entt::registry& registry);

/**
 * @brief 对比两个 JSON 快照，返回差异列表
 * @param actual_json 实际快照 JSON
 * @param expected_json 期望快照 JSON
 * @param float_tolerance 浮点对比容差（默认 0.001）
 * @return 差异描述列表，空则表示一致
 */
std::vector<std::string> CompareSnapshot(const std::string& actual_json,
                                         const std::string& expected_json,
                                         float float_tolerance = 0.001f);

/**
 * @brief 获取 registry 中存活实体数（不含已回收的）
 * @param registry 要检查的 registry
 * @return 存活实体数量
 */
size_t CountAliveEntities(entt::registry& registry);

} // namespace dse::editor::test
