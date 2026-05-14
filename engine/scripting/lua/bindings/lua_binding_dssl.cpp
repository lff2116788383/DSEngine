#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/render/material/dssl_material_loader.h"
#include "engine/render/material/dssl_material_instance.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/components_3d.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

using namespace dse::render;
using namespace dse::runtime::lua_binding;

// ============================================================================
// dssl.load_material(path) → material_id
// ============================================================================
static int L_DsslLoadMaterial(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::string resolved_path = path;

    // 尝试解析 data root 相对路径
    auto& asset_mgr = GetAssetManager();
    std::string full_path = asset_mgr.ResolveAssetPath(resolved_path);
    if (full_path.empty()) full_path = resolved_path;

    auto inst = DSSLMaterialLoader::Instance().LoadFromFile(full_path, &asset_mgr);
    if (!inst) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(inst->GetId()));
    return 1;
}

// ============================================================================
// dssl.create_instance(path) → material_id
// ============================================================================
static int L_DsslCreateInstance(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::string resolved_path = path;
    auto& asset_mgr = GetAssetManager();
    std::string full_path = asset_mgr.ResolveAssetPath(resolved_path);
    if (full_path.empty()) full_path = resolved_path;

    auto inst = DSSLMaterialLoader::Instance().CreateInstance(full_path, &asset_mgr);
    if (!inst) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(inst->GetId()));
    return 1;
}

// ============================================================================
// dssl.set_float(material_id, name, value)
// ============================================================================
static int L_DsslSetFloat(lua_State* L) {
    unsigned int id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    auto inst = DSSLMaterialLoader::Instance().GetInstance(id);
    if (inst) inst->SetFloat(name, value);
    return 0;
}

// ============================================================================
// dssl.set_color(material_id, name, r, g, b[, a])
// ============================================================================
static int L_DsslSetColor(lua_State* L) {
    unsigned int id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float r = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    float b = static_cast<float>(luaL_checknumber(L, 5));
    float a = lua_gettop(L) >= 6 ? static_cast<float>(luaL_checknumber(L, 6)) : 1.0f;
    auto inst = DSSLMaterialLoader::Instance().GetInstance(id);
    if (inst) inst->SetVec4(name, glm::vec4(r, g, b, a));
    return 0;
}

// ============================================================================
// dssl.set_vec3(material_id, name, x, y, z)
// ============================================================================
static int L_DsslSetVec3(lua_State* L) {
    unsigned int id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    auto inst = DSSLMaterialLoader::Instance().GetInstance(id);
    if (inst) inst->SetVec3(name, glm::vec3(x, y, z));
    return 0;
}

// ============================================================================
// dssl.set_texture(material_id, name, texture_path)
// ============================================================================
static int L_DsslSetTexture(lua_State* L) {
    unsigned int id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    const char* tex_path = luaL_checkstring(L, 3);

    auto inst = DSSLMaterialLoader::Instance().GetInstance(id);
    if (!inst) return 0;

    auto& asset_mgr = GetAssetManager();
    auto tex = asset_mgr.LoadTexture(tex_path);
    if (tex) {
        inst->SetTexture(name, tex->GetHandle());
    }
    return 0;
}

// ============================================================================
// dssl.set_texture_handle(material_id, name, texture_handle)
// ============================================================================
static int L_DsslSetTextureHandle(lua_State* L) {
    unsigned int id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    unsigned int handle = static_cast<unsigned int>(luaL_checkinteger(L, 3));

    auto inst = DSSLMaterialLoader::Instance().GetInstance(id);
    if (inst) inst->SetTexture(name, handle);
    return 0;
}

// ============================================================================
// dssl.apply_material(entity, material_id)
// 将 DSSL 材质实例应用到 MeshRendererComponent
// ============================================================================
static int L_DsslApplyMaterial(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;

    Entity e = helper::CheckEntity(L, 1);
    unsigned int mat_id = static_cast<unsigned int>(luaL_checkinteger(L, 2));

    auto* mesh = helper::TryGetComponent<dse::MeshRendererComponent>(*world, e);
    if (!mesh) return 0;

    auto inst = DSSLMaterialLoader::Instance().GetInstance(mat_id);
    if (!inst) return 0;

    // 映射 DSSL 材质到 MeshRendererComponent 字段
    mesh->material_instance_id = mat_id;
    mesh->material_data_source = dse::MeshRendererComponent::MaterialDataSource::MaterialInstance;

    // 设置 shader variant
    switch (inst->GetShaderType()) {
        case DSSLShaderType::Surface:
            if (inst->GetRenderModes().lighting_model == "toon")
                mesh->shader_variant = "MESH_TOON";
            else
                mesh->shader_variant = "MESH_PBR";
            break;
        case DSSLShaderType::Unlit:   mesh->shader_variant = "MESH_UNLIT"; break;
        default:                      mesh->shader_variant = "MESH_PBR"; break;
    }

    // 基础颜色
    mesh->color = inst->GetBaseColor();
    mesh->emissive = inst->GetEmissiveColor();

    // 标量
    mesh->metallic = inst->GetMetallic();
    mesh->roughness = inst->GetRoughness();
    mesh->ao = inst->GetAO();
    mesh->normal_strength = inst->GetNormalStrength();
    mesh->material_alpha_cutoff = inst->GetAlphaCutoff();
    mesh->material_alpha_test = inst->GetAlphaTest();
    mesh->material_double_sided = inst->GetDoubleSided();

    // 纹理
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

    // 阴影
    mesh->receive_shadow = inst->GetRenderModes().shadows_enabled;

    // Toon 参数
    if (inst->GetRenderModes().lighting_model == "toon") {
        glm::vec4 sc = inst->GetVec4("shadow_color", glm::vec4(0.15f, 0.1f, 0.18f, 1.0f));
        mesh->toon_shadow_color = glm::vec3(sc);
        mesh->toon_shadow_threshold = inst->GetFloat("shadow_threshold", 0.35f);
        mesh->toon_shadow_softness = inst->GetFloat("shadow_softness", 0.05f);
        mesh->toon_specular_size = inst->GetFloat("specular_size", 0.6f);
        mesh->toon_specular_strength = inst->GetFloat("specular_strength", 0.8f);
        mesh->toon_rim_strength = inst->GetFloat("rim_strength", 0.3f);
    }

    return 0;
}

// ============================================================================
// dssl.get_float(material_id, name) → number
// ============================================================================
static int L_DsslGetFloat(lua_State* L) {
    unsigned int id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    auto inst = DSSLMaterialLoader::Instance().GetInstance(id);
    if (!inst) { lua_pushnumber(L, 0.0); return 1; }
    lua_pushnumber(L, inst->GetFloat(name));
    return 1;
}

// ============================================================================
// dssl.get_color(material_id, name) → r, g, b, a
// ============================================================================
static int L_DsslGetColor(lua_State* L) {
    unsigned int id = static_cast<unsigned int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    auto inst = DSSLMaterialLoader::Instance().GetInstance(id);
    glm::vec4 v(0.0f);
    if (inst) v = inst->GetVec4(name);
    lua_pushnumber(L, v.r);
    lua_pushnumber(L, v.g);
    lua_pushnumber(L, v.b);
    lua_pushnumber(L, v.a);
    return 4;
}

// ============================================================================
// 注册
// ============================================================================

namespace dse::runtime::lua_binding {

void RegisterDSSLBindings(lua_State* L) {
    lua_newtable(L);

    static const luaL_Reg funcs[] = {
        {"load_material",      L_DsslLoadMaterial},
        {"create_instance",    L_DsslCreateInstance},
        {"set_float",          L_DsslSetFloat},
        {"set_color",          L_DsslSetColor},
        {"set_vec3",           L_DsslSetVec3},
        {"set_texture",        L_DsslSetTexture},
        {"set_texture_handle", L_DsslSetTextureHandle},
        {"apply_material",     L_DsslApplyMaterial},
        {"get_float",          L_DsslGetFloat},
        {"get_color",          L_DsslGetColor},
        {nullptr, nullptr}
    };

    luaL_setfuncs(L, funcs, 0);
    lua_setglobal(L, "dssl");
}

} // namespace dse::runtime::lua_binding
