/**
 * @file lua_binding_ecs_rendering_terrain.cpp
 * @brief Terrain / Water / Grass / Foliage / Tree 等环境 Lua 绑定（S1.8 按域拆分自 lua_binding_ecs_rendering.cpp）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include "engine/ecs/components_3d_navmesh.h"
#include "engine/ecs/components_3d_foliage.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/lut_loader.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/platform/screen.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <limits>


namespace dse::runtime::lua_binding {
namespace {

// ============================================================
// Terrain
// ============================================================

void SampleTerrainHeightmap(TerrainComponent& terrain, const std::vector<unsigned char>& pixels,
                            int image_width, int image_height) {
    if (image_width <= 0 || image_height <= 0 || terrain.resolution_x < 2 || terrain.resolution_z < 2) {
        return;
    }

    terrain.height_data.assign(static_cast<std::size_t>(terrain.resolution_x * terrain.resolution_z), 0.0f);
    for (int z = 0; z < terrain.resolution_z; ++z) {
        const float v = terrain.resolution_z == 1 ? 0.0f : static_cast<float>(z) / static_cast<float>(terrain.resolution_z - 1);
        const int src_z = std::clamp(static_cast<int>(std::round(v * static_cast<float>(image_height - 1))), 0, image_height - 1);
        for (int x = 0; x < terrain.resolution_x; ++x) {
            const float u = terrain.resolution_x == 1 ? 0.0f : static_cast<float>(x) / static_cast<float>(terrain.resolution_x - 1);
            const int src_x = std::clamp(static_cast<int>(std::round(u * static_cast<float>(image_width - 1))), 0, image_width - 1);
            const std::size_t index = (static_cast<std::size_t>(src_z) * static_cast<std::size_t>(image_width) + static_cast<std::size_t>(src_x)) * 4u;
            if (index + 2 >= pixels.size()) continue;
            const float r = static_cast<float>(pixels[index + 0]) / 255.0f;
            const float g = static_cast<float>(pixels[index + 1]) / 255.0f;
            const float b = static_cast<float>(pixels[index + 2]) / 255.0f;
            const float luminance = r * 0.2126f + g * 0.7152f + b * 0.0722f;
            terrain.height_data[static_cast<std::size_t>(z * terrain.resolution_x + x)] = luminance * terrain.max_height;
        }
    }
    terrain.heightmap_width = image_width;
    terrain.heightmap_height = image_height;
}

bool LoadTerrainHeightmap(TerrainComponent& terrain, const std::string& heightmap_path) {
    if (heightmap_path.empty()) {
        terrain.heightmap_width = 0;
        terrain.heightmap_height = 0;
        terrain.heightmap_channels = 0;
        return false;
    }

    std::vector<unsigned char> pixels;
    int image_width = 0;
    int image_height = 0;
    int image_channels = 0;
    if (!GetAssetManager().LoadImageRgba(heightmap_path, pixels, image_width, image_height, image_channels)) {
        return false;
    }

    terrain.heightmap_path = heightmap_path;
    terrain.heightmap_channels = image_channels;
    SampleTerrainHeightmap(terrain, pixels, image_width, image_height);
    terrain.is_dirty = true;
    return true;
}

int L_EcsAddTerrain(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* heightmap_path = luaL_optstring(L, 2, "");
    float width = helper::OptFloat(L, 3, 100.0f);
    float depth = helper::OptFloat(L, 4, 100.0f);
    float max_height = helper::OptFloat(L, 5, 20.0f);
    auto& terrain = world->registry().emplace_or_replace<TerrainComponent>(e);
    terrain.enabled = true;
    terrain.heightmap_path = heightmap_path;
    terrain.width = width;
    terrain.depth = depth;
    terrain.max_height = max_height;
    if (heightmap_path[0] != '\0') {
        LoadTerrainHeightmap(terrain, heightmap_path);
    }
    terrain.is_dirty = true;
    return 0;
}

int L_EcsSetTerrainParams(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain) return 0;
    terrain->resolution_x = std::max(2, helper::OptInt(L, 2, terrain->resolution_x));
    terrain->resolution_z = std::max(2, helper::OptInt(L, 3, terrain->resolution_z));
    terrain->max_lod_levels = std::max(1, helper::OptInt(L, 4, terrain->max_lod_levels));
    terrain->lod_distance_factor = std::max(0.1f, helper::OptFloat(L, 5, terrain->lod_distance_factor));
    if (lua_gettop(L) >= 6) {
        terrain->use_dynamic_lod = helper::CheckBool(L, 6);
    }
    terrain->height_data.assign(static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z), 0.0f);
    if (!terrain->heightmap_path.empty()) {
        LoadTerrainHeightmap(*terrain, terrain->heightmap_path);
    }
    terrain->current_lod = std::clamp(terrain->current_lod, 0, terrain->max_lod_levels - 1);
    terrain->is_dirty = true;
    return 0;
}

int L_EcsSetTerrainHeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int x = helper::CheckInt(L, 2);
    int z = helper::CheckInt(L, 3);
    float height = helper::CheckFloat(L, 4);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain) return 0;
    if (terrain->resolution_x < 2 || terrain->resolution_z < 2) return 0;
    if (terrain->height_data.size() != static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z)) {
        terrain->height_data.assign(static_cast<std::size_t>(terrain->resolution_x * terrain->resolution_z), 0.0f);
    }
    if (x < 0 || z < 0 || x >= terrain->resolution_x || z >= terrain->resolution_z) return 0;
    terrain->height_data[static_cast<std::size_t>(z * terrain->resolution_x + x)] = height;
    terrain->is_dirty = true;
    return 0;
}

int L_EcsLoadTerrainHeightmap(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const char* heightmap_path = luaL_checkstring(L, 2);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const bool ok = LoadTerrainHeightmap(*terrain, heightmap_path);
    lua_pushboolean(L, ok ? 1 : 0);
    if (ok) {
        helper::PushInt(L, terrain->heightmap_width);
        helper::PushInt(L, terrain->heightmap_height);
        helper::PushInt(L, terrain->heightmap_channels);
        helper::PushInt(L, terrain->resolution_x);
        helper::PushInt(L, terrain->resolution_z);
        return 6;
    }
    return 1;
}

int L_EcsSetTerrainTexture(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const char* texture_path = luaL_checkstring(L, 2);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain) {
        lua_pushboolean(L, 0);
        return 1;
    }

    auto texture = GetAssetManager().LoadTexture(texture_path);
    if (!texture) {
        lua_pushboolean(L, 0);
        return 1;
    }

    terrain->texture_path = texture_path;
    terrain->texture_handle = texture->GetHandle();
    terrain->is_dirty = true;
    lua_pushboolean(L, 1);
    helper::PushInt(L, static_cast<int>(terrain->texture_handle));
    helper::PushInt(L, texture->GetWidth());
    helper::PushInt(L, texture->GetHeight());
    return 4;
}

int L_EcsGetTerrainLod(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* terrain = helper::TryGetComponentConst<TerrainComponent>(*world, e);
    if (!terrain) return 0;
    helper::PushInt(L, terrain->current_lod);
    helper::PushInt(L, terrain->resolution_x);
    helper::PushInt(L, terrain->resolution_z);
    helper::PushInt(L, terrain->max_lod_levels);
    helper::PushFloat(L, terrain->lod_distance_factor);
    return 5;
}

int L_EcsSampleTerrainHeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float wx = helper::CheckFloat(L, 2);
    float wz = helper::CheckFloat(L, 3);
    const auto* terrain = helper::TryGetComponentConst<TerrainComponent>(*world, e);
    const auto* transform = helper::TryGetComponentConst<TransformComponent>(*world, e);
    if (!terrain || !transform) {
        helper::PushFloat(L, 0.0f);
        return 1;
    }
    float h = dse::SampleTerrainHeight(*terrain, *transform, wx, wz);
    helper::PushFloat(L, h);
    return 1;
}

int L_EcsSetTerrainSplatTexture(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int layer = helper::CheckInt(L, 2);
    const char* path = luaL_checkstring(L, 3);
    auto* terrain = helper::TryGetComponent<TerrainComponent>(*world, e);
    if (!terrain || layer < 0 || layer > 3) {
        lua_pushboolean(L, 0);
        return 1;
    }
    auto tex = GetAssetManager().LoadTexture(path);
    if (!tex) { lua_pushboolean(L, 0); return 1; }
    terrain->splat_texture_paths[layer] = path;
    terrain->splat_texture_handles[layer] = tex->GetHandle();
    terrain->splat_dirty = true;
    lua_pushboolean(L, 1);
    return 1;
}


// ============================================================
// Water
// ============================================================

int L_EcsAddWater(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    if (!world->registry().all_of<dse::WaterComponent>(e))
        world->registry().emplace<dse::WaterComponent>(e);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsSetWater(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    auto* wc = helper::TryGetComponent<dse::WaterComponent>(*world, e);
    if (!wc) { lua_pushboolean(L, 0); return 1; }
    wc->enabled             = lua_isnoneornil(L, 2) ? wc->enabled : helper::CheckBool(L, 2);
    wc->water_level         = helper::OptFloat(L, 3, wc->water_level);
    wc->deep_color.r        = helper::OptFloat(L, 4, wc->deep_color.r);
    wc->deep_color.g        = helper::OptFloat(L, 5, wc->deep_color.g);
    wc->deep_color.b        = helper::OptFloat(L, 6, wc->deep_color.b);
    wc->shallow_color.r     = helper::OptFloat(L, 7, wc->shallow_color.r);
    wc->shallow_color.g     = helper::OptFloat(L, 8, wc->shallow_color.g);
    wc->shallow_color.b     = helper::OptFloat(L, 9, wc->shallow_color.b);
    wc->max_depth           = helper::OptFloat(L, 10, wc->max_depth);
    wc->transparency        = helper::OptFloat(L, 11, wc->transparency);
    wc->wave_amplitude      = helper::OptFloat(L, 12, wc->wave_amplitude);
    wc->wave_frequency      = helper::OptFloat(L, 13, wc->wave_frequency);
    wc->wave_speed          = helper::OptFloat(L, 14, wc->wave_speed);
    wc->wave_direction.x    = helper::OptFloat(L, 15, wc->wave_direction.x);
    wc->wave_direction.y    = helper::OptFloat(L, 16, wc->wave_direction.y);
    wc->refraction_strength = helper::OptFloat(L, 17, wc->refraction_strength);
    wc->reflection_strength = helper::OptFloat(L, 18, wc->reflection_strength);
    wc->specular_power      = helper::OptFloat(L, 19, wc->specular_power);
    wc->caustic_intensity   = helper::OptFloat(L, 20, wc->caustic_intensity);
    wc->caustic_scale       = helper::OptFloat(L, 21, wc->caustic_scale);
    wc->foam_intensity      = helper::OptFloat(L, 22, wc->foam_intensity);
    wc->foam_depth_threshold = helper::OptFloat(L, 23, wc->foam_depth_threshold);
    wc->underwater_fog_density = helper::OptFloat(L, 24, wc->underwater_fog_density);
    wc->underwater_fog_color.r = helper::OptFloat(L, 25, wc->underwater_fog_color.r);
    wc->underwater_fog_color.g = helper::OptFloat(L, 26, wc->underwater_fog_color.g);
    wc->underwater_fog_color.b = helper::OptFloat(L, 27, wc->underwater_fog_color.b);
    lua_pushboolean(L, 1);
    return 1;
}

int L_EcsGetWater(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* wc = helper::TryGetComponentConst<dse::WaterComponent>(*world, e);
    if (!wc) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, 1);
    helper::PushBool(L, wc->enabled);
    helper::PushFloat(L, wc->water_level);
    helper::PushFloat(L, wc->deep_color.r);
    helper::PushFloat(L, wc->deep_color.g);
    helper::PushFloat(L, wc->deep_color.b);
    helper::PushFloat(L, wc->shallow_color.r);
    helper::PushFloat(L, wc->shallow_color.g);
    helper::PushFloat(L, wc->shallow_color.b);
    helper::PushFloat(L, wc->max_depth);
    helper::PushFloat(L, wc->transparency);
    helper::PushFloat(L, wc->wave_amplitude);
    helper::PushFloat(L, wc->wave_frequency);
    helper::PushFloat(L, wc->wave_speed);
    helper::PushFloat(L, wc->wave_direction.x);
    helper::PushFloat(L, wc->wave_direction.y);
    helper::PushFloat(L, wc->refraction_strength);
    helper::PushFloat(L, wc->reflection_strength);
    helper::PushFloat(L, wc->specular_power);
    return 19;
}


// ============================================================
// Grass
// ============================================================

int L_EcsAddGrass(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& g = world->registry().emplace_or_replace<GrassComponent>(e);
    g.enabled = true;
    g.density       = helper::OptFloat(L, 2, 1.0f);
    g.spawn_radius  = helper::OptFloat(L, 3, 50.0f);
    g.blade_height  = helper::OptFloat(L, 4, 1.0f);
    g.blade_width   = helper::OptFloat(L, 5, 0.1f);
    return 0;
}

int L_EcsSetGrassParams(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    if (lua_gettop(L) >= 2) g->density       = helper::CheckFloat(L, 2);
    if (lua_gettop(L) >= 3) g->spawn_radius  = helper::CheckFloat(L, 3);
    if (lua_gettop(L) >= 4) g->blade_height  = helper::CheckFloat(L, 4);
    if (lua_gettop(L) >= 5) g->blade_width   = helper::CheckFloat(L, 5);
    if (lua_gettop(L) >= 6) g->blade_height_variation = helper::CheckFloat(L, 6);
    if (lua_gettop(L) >= 7) g->chunk_size    = helper::CheckFloat(L, 7);
    if (lua_gettop(L) >= 8) g->seed          = static_cast<unsigned int>(helper::CheckInt(L, 8));
    return 0;
}

int L_EcsSetGrassColor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    g->base_color = glm::vec3(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    if (lua_gettop(L) >= 7) {
        g->tip_color = glm::vec3(helper::CheckFloat(L, 5), helper::CheckFloat(L, 6), helper::CheckFloat(L, 7));
    }
    return 0;
}

int L_EcsSetGrassWind(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    g->wind_direction = glm::vec2(helper::CheckFloat(L, 2), helper::CheckFloat(L, 3));
    if (lua_gettop(L) >= 4) g->wind_speed      = helper::CheckFloat(L, 4);
    if (lua_gettop(L) >= 5) g->wind_strength    = helper::CheckFloat(L, 5);
    if (lua_gettop(L) >= 6) g->wind_turbulence  = helper::CheckFloat(L, 6);
    return 0;
}

int L_EcsSetGrassLod(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    g->lod_near = helper::CheckFloat(L, 2);
    g->lod_far  = helper::CheckFloat(L, 3);
    if (lua_gettop(L) >= 4) g->cast_shadow      = lua_toboolean(L, 4) != 0;
    if (lua_gettop(L) >= 5) g->shadow_distance   = helper::CheckFloat(L, 5);
    return 0;
}

int L_EcsSetGrassEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* g = helper::TryGetComponent<GrassComponent>(*world, e);
    if (!g) return 0;
    g->enabled = lua_toboolean(L, 2) != 0;
    return 0;
}

int L_EcsGetGrassStats(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* g = helper::TryGetComponentConst<GrassComponent>(*world, e);
    if (!g) return 0;
    helper::PushInt(L, g->cached_instance_count_);
    return 1;
}


// ============================================================
// TreeComponent — add_tree + mesh_path getter/setter
// ============================================================

int L_EcsAddTree(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* mesh_path = luaL_optstring(L, 2, "");
    auto& tree = world->registry().emplace_or_replace<dse::TreeComponent>(e);
    tree.enabled = true;
    tree.mesh_path = mesh_path;
    return 0;
}


// ============================================================
// TerrainTileManagerComponent — add
// ============================================================

int L_EcsAddTerrainTileManager(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& ttm = world->registry().emplace_or_replace<dse::TerrainTileManagerComponent>(e);
    ttm.enabled = true;
    return 0;
}


// ============================================================
// DynamicObstacleComponent — add
// ============================================================

int L_EcsAddDynamicObstacle(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& obs = world->registry().emplace_or_replace<dse::DynamicObstacleComponent>(e);
    obs.enabled = true;
    if (!lua_isnoneornil(L, 2)) {
        int shape_val = helper::CheckInt(L, 2);
        obs.shape = (shape_val == 1)
            ? dse::DynamicObstacleComponent::Shape::Cylinder
            : dse::DynamicObstacleComponent::Shape::Box;
    }
    return 0;
}


// ============================================================
// FoliageComponent
// ============================================================

int L_EcsAddFoliage(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& fc = world->registry().emplace_or_replace<dse::FoliageComponent>(e);
    fc.enabled = true;
    return 0;
}

int L_EcsSetFoliageWindStrength(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fc = helper::TryGetComponent<dse::FoliageComponent>(*world, e);
    if (!fc) return 0;
    fc->wind_strength = helper::CheckFloat(L, 2);
    return 0;
}

int L_EcsGetFoliageWindStrength(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* fc = helper::TryGetComponentConst<dse::FoliageComponent>(*world, e);
    if (!fc) { lua_pushnumber(L, 0.0); return 1; }
    helper::PushFloat(L, fc->wind_strength);
    return 1;
}

int L_EcsSetFoliageStiffness(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fc = helper::TryGetComponent<dse::FoliageComponent>(*world, e);
    if (!fc) return 0;
    fc->stiffness = helper::CheckFloat(L, 2);
    return 0;
}

int L_EcsGetFoliageStiffness(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* fc = helper::TryGetComponentConst<dse::FoliageComponent>(*world, e);
    if (!fc) { lua_pushnumber(L, 0.0); return 1; }
    helper::PushFloat(L, fc->stiffness);
    return 1;
}

int L_EcsSetFoliageEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* fc = helper::TryGetComponent<dse::FoliageComponent>(*world, e);
    if (!fc) return 0;
    fc->enabled = helper::CheckBool(L, 2);
    return 0;
}

int L_EcsGetFoliageEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* fc = helper::TryGetComponentConst<dse::FoliageComponent>(*world, e);
    if (!fc) { lua_pushboolean(L, 0); return 1; }
    helper::PushBool(L, fc->enabled);
    return 1;
}


// ============================================================
// NavMeshAutoRebakeComponent
// ============================================================

int L_EcsAddNavMeshAutoRebake(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto& nr = world->registry().emplace_or_replace<dse::NavMeshAutoRebakeComponent>(e);
    nr.enabled = true;
    return 0;
}


} // namespace

void RegisterEcsRenderingTerrainBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_terrain",               L_EcsAddTerrain},
        {"set_terrain_params",        L_EcsSetTerrainParams},
        {"set_terrain_height",        L_EcsSetTerrainHeight},
        {"load_terrain_heightmap",    L_EcsLoadTerrainHeightmap},
        {"set_terrain_texture",       L_EcsSetTerrainTexture},
        {"get_terrain_lod",           L_EcsGetTerrainLod},
        {"sample_terrain_height",     L_EcsSampleTerrainHeight},
        {"set_terrain_splat_texture", L_EcsSetTerrainSplatTexture},
        {"add_water",                 L_EcsAddWater},
        {"set_water",                 L_EcsSetWater},
        {"get_water",                 L_EcsGetWater},
        {"add_grass",                 L_EcsAddGrass},
        {"set_grass_params",          L_EcsSetGrassParams},
        {"set_grass_color",           L_EcsSetGrassColor},
        {"set_grass_wind",            L_EcsSetGrassWind},
        {"set_grass_lod",             L_EcsSetGrassLod},
        {"set_grass_enabled",         L_EcsSetGrassEnabled},
        {"get_grass_stats",           L_EcsGetGrassStats},
        {"add_tree",                  L_EcsAddTree},
        {"add_terrain_tile_manager",  L_EcsAddTerrainTileManager},
        {"add_dynamic_obstacle",      L_EcsAddDynamicObstacle},
        {"add_foliage",               L_EcsAddFoliage},
        {"set_foliage_wind_strength", L_EcsSetFoliageWindStrength},
        {"get_foliage_wind_strength", L_EcsGetFoliageWindStrength},
        {"set_foliage_stiffness",     L_EcsSetFoliageStiffness},
        {"get_foliage_stiffness",     L_EcsGetFoliageStiffness},
        {"set_foliage_enabled",       L_EcsSetFoliageEnabled},
        {"get_foliage_enabled",       L_EcsGetFoliageEnabled},
        {"add_navmesh_auto_rebake",   L_EcsAddNavMeshAutoRebake},
    });
}

} // namespace dse::runtime::lua_binding
