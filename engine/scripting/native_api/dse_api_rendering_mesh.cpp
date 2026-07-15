/**
 * @file dse_api_rendering_mesh.cpp
 * @brief DSEngine C ABI - Rendering Mesh 扩展
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/components_3d_render.h"

using namespace dse;
using namespace dse_api_internal;


extern "C" void dse_mesh_set_material(uint32_t e, const char* material_path) {
    World* world = GW();
    if (!world || !material_path) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) mr->material_path = material_path;
}

extern "C" void dse_mesh_set_depth_state(uint32_t e, int depth_test, int depth_write) {
    World* world = GW();
    if (!world) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (mr) { mr->depth_test_enabled = (depth_test != 0); mr->depth_write_enabled = (depth_write != 0); }
}

extern "C" void dse_mesh_set_material_scalar(uint32_t e, const char* param_name, float value) {
    World* world = GW();
    if (!world || !param_name) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (!mr) return;
    std::string name(param_name);
    if (name == "metallic") mr->metallic = value;
    else if (name == "roughness") mr->roughness = value;
    else if (name == "ao") mr->ao = value;
    else if (name == "normal_strength") mr->normal_strength = value;
    else if (name == "material_alpha_cutoff") mr->material_alpha_cutoff = value;
    else if (name == "sss_strength") mr->sss_strength = value;
    else if (name == "clear_coat") mr->clear_coat = value;
    else if (name == "clear_coat_roughness") mr->clear_coat_roughness = value;
    else if (name == "anisotropy") mr->anisotropy = value;
    else if (name == "pom_height_scale") mr->pom_height_scale = value;
    else return;
    mr->material_data_source = MeshRendererComponent::MaterialDataSource::ComponentFallback;
}

extern "C" void dse_mesh_set_texture_handle(uint32_t e, const char* slot, uint32_t texture_handle) {
    World* world = GW();
    if (!world || !slot) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (!mr) return;
    std::string name(slot);
    const dse::render::TextureHandle handle =
        dse::render::TextureHandle::from_raw(texture_handle);
    if (name == "albedo") mr->albedo_texture_handle = handle;
    else if (name == "normal") mr->normal_texture_handle = handle;
    else if (name == "metallic_roughness") mr->metallic_roughness_texture_handle = handle;
    else if (name == "emissive") mr->emissive_texture_handle = handle;
    else if (name == "occlusion") mr->occlusion_texture_handle = handle;
}

extern "C" void dse_mesh_set_emissive(uint32_t e, float r, float g, float b) {
    World* world = GW();
    if (!world) return;
    auto* mr = world->registry().try_get<MeshRendererComponent>(TE(e));
    if (!mr) return;
    mr->emissive = glm::vec3(r, g, b);
    mr->material_data_source = MeshRendererComponent::MaterialDataSource::ComponentFallback;
}
