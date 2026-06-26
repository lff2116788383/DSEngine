#pragma once

// 瓦片地图编辑纯核心 —— 无 ImGui 依赖，可无头测试。
//
// 这里只放与渲染无关的网格/数据逻辑：
//   - 世界坐标 → 瓦片格子下标
//   - 洪水填充（flood fill）
//   - 自动瓦片（4 邻位 bitmask → 变体 id）单格 / 邻域解析
//   - Bresenham 直线格子序列
//
// ImGui 面板、网格叠加绘制、视口鼠标接线与 undo 录制留在 editor_tilemap_panel.cpp。

#include <glm/glm.hpp>
#include <utility>
#include <vector>

#include "editor_tilemap_panel.h"  // AutoTileRule

struct TilemapComponent;
struct TransformComponent;

namespace dse::editor {

/// 世界坐标 → 瓦片格子下标。地图以 transform.position 为中心、tile_size 为格距。
/// 返回值表示 (cx,cy) 是否落在 [0,width)×[0,height) 内。
bool WorldToTilemapCell(const glm::vec3& world_pos,
                        const TilemapComponent& tm,
                        const TransformComponent& tm_transform,
                        int& out_cx, int& out_cy);

/// 从 (cx,cy) 起对相同 id 的连通区域洪水填充为 fill_id（4 邻接，目标==fill 时直接返回）。
void FloodFillTiles(TilemapComponent& tm, int cx, int cy, int fill_id);

/// 依据 4 邻位（U=1 R=2 D=4 L=8）同类 bitmask 把单格解析为 rule.variant_tiles[mask]。
/// 仅对属于 auto-tile 集合（base 或某 variant）且非空的格生效。
void AutoTileResolve(TilemapComponent& tm, int cx, int cy, const AutoTileRule& rule);

/// 对 (cx,cy) 的 3×3 邻域逐格调用 AutoTileResolve。
void AutoTileResolveNeighbours(TilemapComponent& tm, int cx, int cy, const AutoTileRule& rule);

/// Bresenham 直线：返回从 (x0,y0) 到 (x1,y1) 的整数格子序列（含端点）。
std::vector<std::pair<int, int>> BresenhamLine(int x0, int y0, int x1, int y1);

}  // namespace dse::editor
