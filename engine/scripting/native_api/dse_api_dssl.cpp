/**
 * @file dse_api_dssl.cpp
 * @brief DSEngine C ABI - DSSL — 使用 DSSLMaterialLoader
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/render/material/dssl_material_loader.h"
#include "engine/render/material/dssl_material_instance.h"

using namespace dse_api_internal;


using dse::render::DSSLMaterialLoader;
using dse::render::DSSLShaderType;

static DSSLMaterialLoader* GetDSSL() {
    return dse::core::ServiceLocator::Instance().Get<DSSLMaterialLoader>();
}

extern "C" uint32_t dse_dssl_load_material(const char* path) {
    if (!path) return 0;
    auto* dssl = GetDSSL();
    if (!dssl) return 0;
    AssetManager* am = GAM();
    std::string full = am ? am->ResolveAssetPath(path) : std::string();
    if (full.empty()) full = path;
    auto inst = dssl->LoadFromFile(full, am);
    return inst ? inst->GetId() : 0;
}
extern "C" uint32_t dse_dssl_create_instance(const char* path) {
    if (!path) return 0;
    auto* dssl = GetDSSL();
    if (!dssl) return 0;
    AssetManager* am = GAM();
    std::string full = am ? am->ResolveAssetPath(path) : std::string();
    if (full.empty()) full = path;
    auto inst = dssl->CreateInstance(full, am);
    return inst ? inst->GetId() : 0;
}
extern "C" void dse_dssl_set_float(uint32_t instance, const char* name, float value) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (inst) inst->SetFloat(name, value);
}
extern "C" void dse_dssl_set_color(uint32_t instance, const char* name, float r, float g, float b, float a) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (inst) inst->SetVec4(name, glm::vec4(r, g, b, a));
}
extern "C" void dse_dssl_set_vec3(uint32_t instance, const char* name, float x, float y, float z) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (inst) inst->SetVec3(name, glm::vec3(x, y, z));
}
extern "C" void dse_dssl_set_texture(uint32_t instance, const char* name, const char* path) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (!inst || !path) return;
    AssetManager* am = GAM();
    if (!am) return;
    auto tex = am->LoadTexture(path);
    if (tex) inst->SetTexture(name, tex->GetHandle());
}
extern "C" void dse_dssl_set_texture_handle(uint32_t instance, const char* name, uint32_t handle) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    if (inst) inst->SetTexture(name, handle);
}
extern "C" void dse_dssl_apply_material(uint32_t e, uint32_t instance) {
    World* world = static_cast<World*>(dse_get_world_ptr());
    if (!world) return;
    auto entity = TE(e);
    auto* mesh = world->registry().try_get<dse::MeshRendererComponent>(entity);
    if (!mesh) return;
    auto* loader = GetDSSL();
    if (!loader) return;
    auto inst = loader->GetInstance(instance);
    if (!inst) return;

    mesh->material_instance_id = instance;
    mesh->material_data_source = dse::MeshRendererComponent::MaterialDataSource::MaterialInstance;

    switch (inst->GetShaderType()) {
        case DSSLShaderType::Surface:
            if (inst->GetRenderModes().lighting_model == "toon")
                mesh->shader_variant = "MESH_TOON";
            else if (inst->GetRenderModes().lighting_model == "watercolor")
                mesh->shader_variant = "MESH_WATERCOLOR";
            else
                mesh->shader_variant = "MESH_PBR";
            break;
        case DSSLShaderType::Unlit:   mesh->shader_variant = "MESH_UNLIT"; break;
        default:                      mesh->shader_variant = "MESH_PBR"; break;
    }

    mesh->color = inst->GetBaseColor();
    mesh->emissive = inst->GetEmissiveColor();
    mesh->metallic = inst->GetMetallic();
    mesh->roughness = inst->GetRoughness();
    mesh->ao = inst->GetAO();
    mesh->normal_strength = inst->GetNormalStrength();
    mesh->material_alpha_cutoff = inst->GetAlphaCutoff();
    mesh->material_alpha_test = inst->GetAlphaTest();
    mesh->material_double_sided = inst->GetDoubleSided();

    unsigned int albedo_tex = inst->GetAlbedoTexture();
    if (albedo_tex) mesh->albedo_texture_handle = albedo_tex;
    unsigned int normal_tex = inst->GetNormalTexture();
    if (normal_tex) mesh->normal_texture_handle = normal_tex;
    unsigned int mr_tex = inst->GetMetallicRoughnessTexture();
    if (mr_tex) mesh->metallic_roughness_texture_handle = mr_tex;
    unsigned int emissive_tex = inst->GetEmissiveTexture();
    if (emissive_tex) mesh->emissive_texture_handle = emissive_tex;
    unsigned int occlusion_tex = inst->GetOcclusionTexture();
    if (occlusion_tex) mesh->occlusion_texture_handle = occlusion_tex;

    mesh->receive_shadow = inst->GetRenderModes().shadows_enabled;

    if (inst->GetRenderModes().lighting_model == "toon") {
        glm::vec4 sc = inst->GetVec4("shadow_color", glm::vec4(0.15f, 0.1f, 0.18f, 1.0f));
        mesh->toon_shadow_color = glm::vec3(sc);
        mesh->toon_shadow_threshold = inst->GetFloat("shadow_threshold", 0.35f);
        mesh->toon_shadow_softness = inst->GetFloat("shadow_softness", 0.05f);
        mesh->toon_specular_size = inst->GetFloat("specular_size", 0.6f);
        mesh->toon_specular_strength = inst->GetFloat("specular_strength", 0.8f);
        mesh->toon_rim_strength = inst->GetFloat("rim_strength", 0.3f);
    }

    if (inst->GetRenderModes().lighting_model == "watercolor") {
        mesh->watercolor_paper_strength = inst->GetFloat("paper_strength", 0.3f);
        mesh->watercolor_edge_darkening = inst->GetFloat("edge_darkening", 0.4f);
        mesh->watercolor_color_bleed = inst->GetFloat("color_bleed", 0.2f);
        mesh->watercolor_pigment_density = inst->GetFloat("pigment_density", 1.0f);
    }
}
extern "C" float dse_dssl_get_float(uint32_t instance, const char* name) {
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    return inst ? inst->GetFloat(name) : 0.0f;
}
extern "C" void dse_dssl_get_color(uint32_t instance, const char* name, float* out_rgba) {
    if (!out_rgba) return;
    auto* dssl = GetDSSL();
    auto inst = dssl ? dssl->GetInstance(instance) : nullptr;
    glm::vec4 v(0.0f);
    if (inst) v = inst->GetVec4(name);
    out_rgba[0] = v.r; out_rgba[1] = v.g; out_rgba[2] = v.b; out_rgba[3] = v.a;
}

