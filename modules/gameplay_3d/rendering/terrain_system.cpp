#include "modules/gameplay_3d/rendering/terrain_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"
#include <iostream>

namespace dse {
namespace gameplay3d {

void TerrainSystem::RebuildTerrain(TerrainComponent& terrain) {
    if (terrain.resolution_x < 2 || terrain.resolution_z < 2) return;
    
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    // Create base vertices
    float dx = terrain.width / (terrain.resolution_x - 1);
    float dz = terrain.depth / (terrain.resolution_z - 1);
    float start_x = -terrain.width / 2.0f;
    float start_z = -terrain.depth / 2.0f;
    
    // We assume resolution_x == resolution_z and it's a power of 2 plus 1 (e.g., 65x65) for perfect LOD,
    // but for simplicity we will handle general cases by adjusting step size.
    
    if (terrain.height_data.empty()) {
        terrain.height_data.resize(terrain.resolution_x * terrain.resolution_z, 0.0f);
    }
    
    // Build LOD indices
    terrain.lod_index_counts.clear();
    terrain.lod_index_counts.resize(terrain.max_lod_levels, 0);
    // In a real RHI we would create multiple EBOs, but here we just store the CPU side index buffers
    // Actually, since we are doing client memory drawing right now, we will just compute the indices on the fly per frame based on current_lod!
    
    terrain.is_dirty = false;
}

void TerrainSystem::Render(World& world, CommandBuffer& cmd_buffer) {
    auto view = world.registry().view<TerrainComponent, TransformComponent>();
    auto camera_view = world.registry().view<Camera3DComponent, TransformComponent>();
    
    glm::vec3 camera_pos(0.0f);
    if (camera_view.begin() != camera_view.end()) {
        auto& cam_transform = camera_view.get<TransformComponent>((entt::entity)*camera_view.begin());
        camera_pos = glm::vec3(cam_transform.local_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    }
    
    std::vector<MeshDrawItem> items;
    items.reserve(view.size_hint());
    
    for (auto entity : view) {
        auto& terrain = view.get<TerrainComponent>((entt::entity)entity);
        auto& transform = view.get<TransformComponent>((entt::entity)entity);
        
        if (!terrain.enabled) continue;
        
        if (terrain.is_dirty) {
            RebuildTerrain(terrain);
            if (!world.registry().all_of<BoundingBoxComponent>((entt::entity)entity)) {
                world.registry().emplace<BoundingBoxComponent>((entt::entity)entity);
            }
            auto& bbox = world.registry().get<BoundingBoxComponent>((entt::entity)entity);
            bbox.min_extents = glm::vec3(-terrain.width / 2.0f, 0.0f, -terrain.depth / 2.0f);
            bbox.max_extents = glm::vec3(terrain.width / 2.0f, terrain.max_height, terrain.depth / 2.0f);
        }
        
        // Frustum Culling Support for Terrain
        if (!terrain.visible) continue;
        
        // --- Dynamic LOD Calculation (inspired by VSLodTerrainGeometry) ---
        if (terrain.use_dynamic_lod) {
            glm::vec3 terrain_center = glm::vec3(transform.local_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            float dist = glm::length(camera_pos - terrain_center);
            
            // Distance-based LOD selection
            int desired_lod = static_cast<int>(dist / terrain.lod_distance_factor);
            terrain.current_lod = std::clamp(desired_lod, 0, terrain.max_lod_levels - 1);
        } else {
            terrain.current_lod = 0;
        }
        
        int step = 1 << terrain.current_lod; // step = 2^lod (1, 2, 4, 8...)
        
        MeshDrawItem item;
        item.model = transform.local_to_world;
        item.texture_handle = terrain.texture_handle;
        
        float dx = terrain.width / (terrain.resolution_x - 1);
        float dz = terrain.depth / (terrain.resolution_z - 1);
        float start_x = -terrain.width / 2.0f;
        float start_z = -terrain.depth / 2.0f;
        
        // Client side array filling (with LOD step)
        item.vertices.reserve(terrain.resolution_x * terrain.resolution_z);
        for (int z = 0; z < terrain.resolution_z; ++z) {
            for (int x = 0; x < terrain.resolution_x; ++x) {
                BatchVertex v;
                v.pos = glm::vec3(start_x + x * dx, terrain.height_data[z * terrain.resolution_x + x], start_z + z * dz);
                v.color = glm::vec4(1.0f);
                v.uv = glm::vec2(static_cast<float>(x) / (terrain.resolution_x - 1) * 10.0f, static_cast<float>(z) / (terrain.resolution_z - 1) * 10.0f);
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                item.vertices.push_back(v);
            }
        }
        
        // Generate indices with LOD step
        item.indices.reserve((terrain.resolution_x - 1) * (terrain.resolution_z - 1) * 6 / (step * step));
        for (int z = 0; z < terrain.resolution_z - 1 - step; z += step) {
            for (int x = 0; x < terrain.resolution_x - 1 - step; x += step) {
                int top_left = z * terrain.resolution_x + x;
                int top_right = z * terrain.resolution_x + (x + step);
                int bottom_left = (z + step) * terrain.resolution_x + x;
                int bottom_right = (z + step) * terrain.resolution_x + (x + step);
                
                // Add two triangles for this quad patch
                item.indices.push_back(top_left);
                item.indices.push_back(bottom_left);
                item.indices.push_back(top_right);
                
                item.indices.push_back(top_right);
                item.indices.push_back(bottom_left);
                item.indices.push_back(bottom_right);
            }
        }
        
        // Setup PBR materials for terrain
        item.lighting_enabled = true;
        item.material_albedo = glm::vec3(0.5f, 0.7f, 0.3f); // Greenish
        item.material_metallic = 0.0f;
        item.material_roughness = 0.9f;
        item.receive_shadow = true;
        
        // Find lights
        auto light_view = world.registry().view<DirectionalLight3DComponent>();
        if (light_view.begin() != light_view.end()) {
            auto& light = light_view.get<DirectionalLight3DComponent>((entt::entity)*light_view.begin());
            if (light.enabled) {
                item.light_direction = light.direction;
                item.light_color = light.color;
                item.light_intensity = light.intensity;
                item.ambient_intensity = light.ambient_intensity;
                item.shadow_strength = light.shadow_strength;
            }
        }
        
        // We don't override VAO since we populate client memory
        // item.vao_override = terrain.vao;
        // item.index_count_override = terrain.index_count;
        
        items.push_back(item);
    }
    
    if (!items.empty()) {
        cmd_buffer.DrawMeshBatch(items);
    }
}

} // namespace gameplay3d
} // namespace dse