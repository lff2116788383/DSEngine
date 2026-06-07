#ifndef DSE_COMPONENTS_3D_TERRAIN_TILE_H
#define DSE_COMPONENTS_3D_TERRAIN_TILE_H

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include "engine/render/rhi/rhi_handle.h"

namespace dse {

/// Individual terrain tile state
struct TerrainTileData {
    int tile_x = 0;
    int tile_z = 0;

    // Heightmap data for this tile
    std::vector<float> height_data;
    std::vector<float> splat_data;

    // GPU resources
    dse::render::VertexArrayHandle vao;
    dse::render::BufferHandle vbo;
    std::vector<dse::render::BufferHandle> lod_ebos;
    std::vector<unsigned int> lod_index_counts;
    unsigned int index_count = 0;

    int current_lod = 0;
    bool gpu_dirty = true;
    bool loaded = false;
};

/// Terrain Tile Manager Component - placed on an entity to enable tiled terrain
struct TerrainTileManagerComponent {
    bool enabled = true;

    // Tile configuration
    float tile_world_size = 64.0f;      ///< each tile covers this many world units
    int tile_resolution = 64;            ///< heightmap resolution per tile (vertices per edge)
    float max_height = 20.0f;            ///< maximum height for all tiles
    int max_lod_levels = 4;
    float lod_distance_factor = 50.0f;

    // Loading radius
    float load_radius = 200.0f;          ///< tiles within this radius are loaded
    float unload_radius = 250.0f;        ///< tiles beyond this radius are unloaded

    // Heightmap source pattern (format: "terrain/tile_{x}_{z}.raw")
    std::string heightmap_pattern = "terrain/tile_{x}_{z}.raw";

    // Splat textures (shared across all tiles)
    std::string splat_texture_paths[4];
    unsigned int splat_texture_handles[4] = {0, 0, 0, 0};
    glm::vec4 splat_tiling = glm::vec4(10.0f);

    // Base texture for tiles without splatmap
    std::string base_texture_path;
    unsigned int base_texture_handle = 0;

    // Runtime tile storage - maps (tile_x, tile_z) encoded as int64 to tile data
    std::unordered_map<int64_t, TerrainTileData> tiles;

    // Procedural height generation callback (fallback when no heightmap file exists)
    bool use_procedural = true;          ///< if true, generate flat tiles when no file found
    float procedural_base_height = 0.0f; ///< base height for procedural tiles

    // Statistics
    int loaded_tile_count = 0;
    int visible_tile_count = 0;
};

inline int64_t TerrainTileKey(int tx, int tz) {
    return (int64_t(uint32_t(tx)) << 32) | int64_t(uint32_t(tz));
}

} // namespace dse

#endif // DSE_COMPONENTS_3D_TERRAIN_TILE_H
