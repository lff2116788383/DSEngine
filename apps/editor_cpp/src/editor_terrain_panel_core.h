#pragma once

// 地形雕刻 / splat 绘制纯核心 —— 无 ImGui 依赖，可无头测试。
//
// 这里只放与渲染无关的纯数学/数据逻辑：
//   - 世界↔屏幕↔地形格点的坐标换算与射线-平面求交
//   - 高斯衰减权重
//   - 高度笔刷（Raise/Lower/Smooth/Flatten）对 height_data 的写入
//   - splat 权重图初始化与绘制（4 层归一化）
//
// ImGui 面板、笔刷叠加绘制、视口鼠标接线与 undo 录制留在 editor_terrain_panel.cpp。

#include <glm/glm.hpp>

#include "editor_terrain_panel.h"  // TerrainBrushMode / TerrainEditorState

struct TransformComponent;

namespace dse {
struct TerrainComponent;
}  // namespace dse

namespace dse::editor {

/// 世界坐标投影到屏幕像素。clip.w≈0 时返回远点哨兵 (-10000,-10000)。
glm::vec2 WorldToScreen(const glm::vec3& world_pos,
                        const glm::mat4& view,
                        const glm::mat4& proj,
                        const glm::vec2& window_pos,
                        const glm::vec2& panel_size);

/// 屏幕像素反投影成射线并与 Y=plane_y 水平面求交（地形在 XZ 平面）。
/// 射线近水平（|dir.y|<1e-6）时返回原点 (0,0,0)。
glm::vec3 ScreenToWorldOnTerrain(const glm::vec2& screen_pos,
                                 const glm::mat4& view,
                                 const glm::mat4& proj,
                                 const glm::vec2& window_pos,
                                 const glm::vec2& panel_size,
                                 float plane_y);

/// 世界坐标 → 地形格点浮点坐标（local 空间 / 分辨率映射）。
/// 返回值表示格点是否落在 [0,resolution) 范围内。
bool WorldToTerrainGrid(const glm::vec3& world_pos,
                        const TerrainComponent& terrain,
                        const TransformComponent& tf,
                        float& out_gx, float& out_gz);

/// 笔刷权重：dist>=radius 为 0；falloff=0 硬边(恒1)，falloff=1 高斯软边。
float GaussianFalloff(float dist, float radius, float falloff);

/// 按笔刷模式修改地形 height_data（就地）；clamp 到 [0,max_height]，置 is_dirty。
void ApplyBrush(TerrainComponent& terrain,
                const TransformComponent& tf,
                const glm::vec3& world_hit,
                const TerrainEditorState& state,
                float delta_time);

/// 确保 splat_data 大小为 resolution_x*resolution_z*4；新建时 layer0=1 其余=0。
void EnsureSplatData(TerrainComponent& terrain);

/// 在 active_splat_layer 上累加权重，其余层按比例衰减以保持和≈1；置 splat_dirty。
void ApplySplatBrush(TerrainComponent& terrain,
                     const TransformComponent& tf,
                     const glm::vec3& world_hit,
                     const TerrainEditorState& state,
                     float delta_time);

}  // namespace dse::editor
