#include "editor_tilemap_panel_core.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/tilemap.h"

#include <cmath>
#include <cstdlib>
#include <queue>

namespace dse::editor {

bool WorldToTilemapCell(const glm::vec3& world_pos,
                        const TilemapComponent& tm,
                        const TransformComponent& tm_transform,
                        int& out_cx, int& out_cy) {
    float map_w = static_cast<float>(tm.width) * tm.tile_size;
    float map_h = static_cast<float>(tm.height) * tm.tile_size;
    float origin_x = tm_transform.position.x - map_w * 0.5f;
    float origin_y = tm_transform.position.y - map_h * 0.5f;

    float lx = world_pos.x - origin_x;
    float ly = world_pos.y - origin_y;

    out_cx = static_cast<int>(std::floor(lx / tm.tile_size));
    out_cy = static_cast<int>(std::floor(ly / tm.tile_size));

    return out_cx >= 0 && out_cx < tm.width && out_cy >= 0 && out_cy < tm.height;
}

void FloodFillTiles(TilemapComponent& tm, int cx, int cy, int fill_id) {
    if (cx < 0 || cx >= tm.width || cy < 0 || cy >= tm.height) return;
    int target_id = tm.tiles[cy * tm.width + cx];
    if (target_id == fill_id) return;

    std::queue<std::pair<int,int>> q;
    q.push({cx, cy});
    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop();
        if (x < 0 || x >= tm.width || y < 0 || y >= tm.height) continue;
        int idx = y * tm.width + x;
        if (tm.tiles[idx] != target_id) continue;
        tm.tiles[idx] = fill_id;
        q.push({x-1, y}); q.push({x+1, y});
        q.push({x, y-1}); q.push({x, y+1});
    }
}

void AutoTileResolve(TilemapComponent& tm, int cx, int cy, const AutoTileRule& rule) {
    if (!rule.enabled) return;
    int idx = cy * tm.width + cx;
    // Only resolve tiles belonging to the auto-tile set
    if (tm.tiles[idx] == 0) return;
    int base = rule.base_tile_id;
    bool belongs = false;
    for (int m = 0; m < 16; m++) {
        if (tm.tiles[idx] == rule.variant_tiles[m] || tm.tiles[idx] == base) { belongs = true; break; }
    }
    if (!belongs) return;

    auto is_same = [&](int x, int y) -> bool {
        if (x < 0 || x >= tm.width || y < 0 || y >= tm.height) return false;
        int tid = tm.tiles[y * tm.width + x];
        if (tid == base) return true;
        for (int m = 0; m < 16; m++) { if (tid == rule.variant_tiles[m]) return true; }
        return false;
    };
    int mask = 0;
    if (is_same(cx, cy - 1)) mask |= 1; // Up
    if (is_same(cx + 1, cy)) mask |= 2; // Right
    if (is_same(cx, cy + 1)) mask |= 4; // Down
    if (is_same(cx - 1, cy)) mask |= 8; // Left
    int resolved = rule.variant_tiles[mask];
    if (resolved > 0) tm.tiles[idx] = resolved;
}

void AutoTileResolveNeighbours(TilemapComponent& tm, int cx, int cy, const AutoTileRule& rule) {
    if (!rule.enabled) return;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = cx + dx, ny = cy + dy;
            if (nx >= 0 && nx < tm.width && ny >= 0 && ny < tm.height) {
                AutoTileResolve(tm, nx, ny, rule);
            }
        }
    }
}

std::vector<std::pair<int,int>> BresenhamLine(int x0, int y0, int x1, int y1) {
    std::vector<std::pair<int,int>> pts;
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        pts.push_back({x0, y0});
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
    return pts;
}

}  // namespace dse::editor
