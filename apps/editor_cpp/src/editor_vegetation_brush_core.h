#pragma once

// 植被刷纯核心 —— 无 ImGui 依赖，可无头测试。
//
// 这里只放与渲染无关的纯数学/数据逻辑：
//   - 植被密度遮罩的初始化（覆盖世界范围 + 分辨率 + 初值）
//   - 在遮罩上施加笔刷（Plant 向 1 / Clear 向 0，高斯衰减，钳到 [0,1]）
//
// ImGui 面板、笔刷叠加绘制、视口鼠标接线与 undo 录制留在 editor_vegetation_panel.cpp。

#include <glm/glm.hpp>

namespace dse {
struct VegetationDensityMask;
}  // namespace dse

namespace dse::editor {

/// 确保遮罩按给定世界范围/分辨率初始化。
/// 当遮罩无效或范围/分辨率发生变化时，重建为 init_value 填充（默认满密度 1.0）。
/// 已匹配则保持现有 weights 不变（保护已绘制内容）。
void EnsureVegetationMask(dse::VegetationDensityMask& mask,
                          const glm::vec2& world_min,
                          const glm::vec2& world_size,
                          int res_x, int res_z,
                          float init_value = 1.0f);

/// 在世界坐标 world_hit 处施加笔刷：plant=true 向 1 靠拢，否则向 0 靠拢。
/// 使用高斯衰减权重，逐格 clamp 到 [0,1]。遮罩无效时为 no-op。
void ApplyVegetationBrush(dse::VegetationDensityMask& mask,
                          const glm::vec3& world_hit,
                          float radius, float strength, float falloff,
                          bool plant, float delta_time);

}  // namespace dse::editor
