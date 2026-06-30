/**
 * @file world_partition.cpp
 * @brief 世界分区流式加载系统实现
 */

#include "engine/scene/world_partition.h"
#include "engine/scene/scene_manager.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include <entt/entt.hpp>
#include <algorithm>
#include <cmath>

namespace dse {

// ─── 坐标转换 ──────────────────────────────────────────────────────────────

CellCoord WorldPartitionSystem::WorldToCell(const glm::vec3& world_pos, float cell_size) const {
    return CellCoord{
        static_cast<int>(std::floor(world_pos.x / cell_size)),
        static_cast<int>(std::floor(world_pos.z / cell_size))
    };
}

glm::vec3 WorldPartitionSystem::CellToWorld(const CellCoord& coord, float cell_size) const {
    return glm::vec3(
        (static_cast<float>(coord.x) + 0.5f) * cell_size,
        0.0f,
        (static_cast<float>(coord.y) + 0.5f) * cell_size
    );
}

// ─── Init / Shutdown ────────────────────────────────────────────────────────

void WorldPartitionSystem::Init(::scene::SceneManager* scene_mgr) {
    scene_mgr_ = scene_mgr;
}

void WorldPartitionSystem::Shutdown() {
    cells_.clear();
    scene_mgr_ = nullptr;
}

// ─── 路径构建 ──────────────────────────────────────────────────────────────

std::string WorldPartitionSystem::BuildCellPath(
    const WorldPartitionConfigComponent& config, const CellCoord& coord) const {

    // 格式: {cells_directory}/cell_{cx}_{cy}.dscene
    std::string path = config.cells_directory;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path += '/';
    }
    path += "cell_" + std::to_string(coord.x) + "_" + std::to_string(coord.y) + ".dscene";
    return path;
}

// ─── Update ─────────────────────────────────────────────────────────────────

void WorldPartitionSystem::Update(::World& world) {
    if (!scene_mgr_) return;

    auto& registry = world.registry();

    // 查找 WorldPartitionConfig
    WorldPartitionConfigComponent config;
    bool has_config = false;
    {
        auto view = registry.view<WorldPartitionConfigComponent>();
        for (auto entity : view) {
            auto& cfg = view.get<WorldPartitionConfigComponent>(entity);
            if (cfg.enabled) {
                config = cfg;
                has_config = true;
                break;
            }
        }
    }
    if (!has_config) return;

    // 收集所有 StreamingOrigin 位置
    std::vector<glm::vec3> origins;
    {
        auto view = registry.view<StreamingOriginComponent, TransformComponent>();
        for (auto entity : view) {
            auto& so = view.get<StreamingOriginComponent>(entity);
            if (!so.enabled) continue;
            auto& tf = view.get<TransformComponent>(entity);
            origins.push_back(tf.position);
        }
    }
    if (origins.empty()) return;

    // 计算最终加载/卸载半径（取所有 origin 中最大的）
    float load_radius = 0.0f;
    float unload_radius = 0.0f;
    {
        auto view = registry.view<StreamingOriginComponent>();
        for (auto entity : view) {
            auto& so = view.get<StreamingOriginComponent>(entity);
            if (!so.enabled) continue;
            load_radius = std::max(load_radius, so.load_radius);
            unload_radius = std::max(unload_radius, so.unload_radius);
        }
    }

    float load_radius_sq = load_radius * load_radius;
    float unload_radius_sq = unload_radius * unload_radius;

    // 确定哪些 Cell 应该被加载（在任意 origin 加载半径内）
    std::unordered_set<CellCoord, CellCoordHash> desired_cells;
    for (const auto& origin : origins) {
        CellCoord center = WorldToCell(origin, config.cell_size);
        int radius_cells = static_cast<int>(std::ceil(load_radius / config.cell_size));

        for (int dy = -radius_cells; dy <= radius_cells; ++dy) {
            for (int dx = -radius_cells; dx <= radius_cells; ++dx) {
                CellCoord candidate{center.x + dx, center.y + dy};

                // 超出网格范围则跳过
                if (candidate.x < config.grid_min_x || candidate.x > config.grid_max_x) continue;
                if (candidate.y < config.grid_min_y || candidate.y > config.grid_max_y) continue;

                // 检查距离
                glm::vec3 cell_center = CellToWorld(candidate, config.cell_size);
                float min_dist_sq = std::numeric_limits<float>::max();
                for (const auto& o : origins) {
                    float dx2 = cell_center.x - o.x;
                    float dz2 = cell_center.z - o.z;
                    float d = dx2 * dx2 + dz2 * dz2;
                    min_dist_sq = std::min(min_dist_sq, d);
                }

                if (min_dist_sq <= load_radius_sq) {
                    desired_cells.insert(candidate);
                }
            }
        }
    }

    // 卸载超出 unload_radius 的 Cell
    std::vector<CellCoord> to_unload;
    for (auto& [coord, info] : cells_) {
        if (info.state != CellState::Loaded && info.state != CellState::Loading) continue;

        glm::vec3 cell_center = CellToWorld(coord, config.cell_size);
        float min_dist_sq = std::numeric_limits<float>::max();
        for (const auto& o : origins) {
            float dx2 = cell_center.x - o.x;
            float dz2 = cell_center.z - o.z;
            float d = dx2 * dx2 + dz2 * dz2;
            min_dist_sq = std::min(min_dist_sq, d);
        }

        if (min_dist_sq > unload_radius_sq) {
            to_unload.push_back(coord);
        }
    }

    for (const auto& coord : to_unload) {
        auto& info = cells_[coord];
        if (info.state == CellState::Loaded || info.state == CellState::Loading) {
            scene_mgr_->UnloadSubScene(info.scene_path);
            info.state = CellState::Unloaded;
        }
    }

    // 加载所需但尚未加载的 Cell（按距离排序，每帧限制加载数量）
    struct LoadCandidate {
        CellCoord coord;
        float distance_sq;
    };
    std::vector<LoadCandidate> to_load;

    for (const auto& coord : desired_cells) {
        auto it = cells_.find(coord);
        if (it != cells_.end() && it->second.state != CellState::Unloaded) continue;

        glm::vec3 cell_center = CellToWorld(coord, config.cell_size);
        float min_dist_sq = std::numeric_limits<float>::max();
        for (const auto& o : origins) {
            float dx2 = cell_center.x - o.x;
            float dz2 = cell_center.z - o.z;
            float d = dx2 * dx2 + dz2 * dz2;
            min_dist_sq = std::min(min_dist_sq, d);
        }
        to_load.push_back({coord, min_dist_sq});
    }

    // 按距离排序：最近的优先加载
    std::sort(to_load.begin(), to_load.end(),
        [](const LoadCandidate& a, const LoadCandidate& b) {
            return a.distance_sq < b.distance_sq;
        });

    uint32_t loads_this_frame = 0;
    for (const auto& candidate : to_load) {
        if (loads_this_frame >= config.max_loads_per_frame) break;

        std::string path = BuildCellPath(config, candidate.coord);

        CellInfo info;
        info.coord = candidate.coord;
        info.state = CellState::Loading;
        info.scene_path = path;
        info.distance_sq = candidate.distance_sq;
        cells_[candidate.coord] = info;

        scene_mgr_->LoadSubSceneAsync(path);
        ++loads_this_frame;
    }

    // 将 Loading 状态的 Cell 检查是否已完成加载
    for (auto& [coord, info] : cells_) {
        if (info.state == CellState::Loading) {
            if (scene_mgr_->IsSubSceneLoaded(info.scene_path)) {
                info.state = CellState::Loaded;
            }
        }
    }

    // 进度回调
    if (progress_cb_) {
        int loaded = 0;
        int total = static_cast<int>(desired_cells.size());
        for (const auto& coord : desired_cells) {
            auto it = cells_.find(coord);
            if (it != cells_.end() && it->second.state == CellState::Loaded) {
                ++loaded;
            }
        }
        progress_cb_(loaded, total);
    }
}

// ─── ForceLoad / ForceUnload ────────────────────────────────────────────────

void WorldPartitionSystem::ForceLoadCell(const CellCoord& coord) {
    if (!scene_mgr_) return;
    auto it = cells_.find(coord);
    if (it != cells_.end() && it->second.state != CellState::Unloaded) return;

    // 需要一个 config 来生成路径；使用默认值
    WorldPartitionConfigComponent default_cfg;
    std::string path = BuildCellPath(default_cfg, coord);

    CellInfo info;
    info.coord = coord;
    info.state = CellState::Loading;
    info.scene_path = path;
    cells_[coord] = info;

    scene_mgr_->LoadSubSceneAsync(path);
}

void WorldPartitionSystem::ForceUnloadCell(const CellCoord& coord) {
    if (!scene_mgr_) return;
    auto it = cells_.find(coord);
    if (it == cells_.end()) return;

    if (it->second.state == CellState::Loaded || it->second.state == CellState::Loading) {
        scene_mgr_->UnloadSubScene(it->second.scene_path);
        it->second.state = CellState::Unloaded;
    }
}

size_t WorldPartitionSystem::LoadedCellCount() const {
    size_t count = 0;
    for (const auto& [coord, info] : cells_) {
        if (info.state == CellState::Loaded) ++count;
    }
    return count;
}

} // namespace dse
