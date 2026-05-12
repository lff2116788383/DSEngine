#include "dssl_material_instance.h"

namespace dse {
namespace render {

DSSLMaterialInstance::DSSLMaterialInstance(unsigned int id, const std::string& dssl_path)
    : id_(id), dssl_path_(dssl_path) {}

// === Uniform 设置 ===

void DSSLMaterialInstance::SetFloat(const std::string& name, float value) {
    floats_[name] = value;
}

void DSSLMaterialInstance::SetVec2(const std::string& name, const glm::vec2& value) {
    vec2s_[name] = value;
}

void DSSLMaterialInstance::SetVec3(const std::string& name, const glm::vec3& value) {
    vec3s_[name] = value;
}

void DSSLMaterialInstance::SetVec4(const std::string& name, const glm::vec4& value) {
    vec4s_[name] = value;
}

void DSSLMaterialInstance::SetInt(const std::string& name, int value) {
    ints_[name] = value;
}

void DSSLMaterialInstance::SetTexture(const std::string& name, unsigned int texture_handle) {
    textures_[name] = texture_handle;
}

// === Uniform 获取 ===

float DSSLMaterialInstance::GetFloat(const std::string& name, float fallback) const {
    auto it = floats_.find(name);
    return it != floats_.end() ? it->second : fallback;
}

glm::vec2 DSSLMaterialInstance::GetVec2(const std::string& name, const glm::vec2& fallback) const {
    auto it = vec2s_.find(name);
    return it != vec2s_.end() ? it->second : fallback;
}

glm::vec3 DSSLMaterialInstance::GetVec3(const std::string& name, const glm::vec3& fallback) const {
    auto it = vec3s_.find(name);
    return it != vec3s_.end() ? it->second : fallback;
}

glm::vec4 DSSLMaterialInstance::GetVec4(const std::string& name, const glm::vec4& fallback) const {
    auto it = vec4s_.find(name);
    return it != vec4s_.end() ? it->second : fallback;
}

int DSSLMaterialInstance::GetInt(const std::string& name, int fallback) const {
    auto it = ints_.find(name);
    return it != ints_.end() ? it->second : fallback;
}

unsigned int DSSLMaterialInstance::GetTexture(const std::string& name) const {
    auto it = textures_.find(name);
    return it != textures_.end() ? it->second : 0;
}

// === 映射到引擎标准材质属性 ===
// DSSL uniform 名称约定:
//   albedo_color / base_color → BaseColor
//   emission_color → EmissiveColor
//   metallic → Metallic
//   roughness → Roughness
//   ao → AO
//   normal_strength → NormalStrength
//   albedo_tex / base_texture → AlbedoTexture
//   normal_tex / normal_map → NormalTexture
//   orm_tex / metallic_roughness_tex → MetallicRoughnessTexture
//   emissive_tex / emission_tex → EmissiveTexture
//   occlusion_tex → OcclusionTexture

glm::vec4 DSSLMaterialInstance::GetBaseColor() const {
    // 优先 vec4 类型的 albedo_color
    auto it4 = vec4s_.find("albedo_color");
    if (it4 != vec4s_.end()) return it4->second;
    it4 = vec4s_.find("base_color");
    if (it4 != vec4s_.end()) return it4->second;
    // vec3 fallback
    auto it3 = vec3s_.find("albedo_color");
    if (it3 != vec3s_.end()) return glm::vec4(it3->second, 1.0f);
    it3 = vec3s_.find("base_color");
    if (it3 != vec3s_.end()) return glm::vec4(it3->second, 1.0f);
    return glm::vec4(1.0f);
}

glm::vec3 DSSLMaterialInstance::GetEmissiveColor() const {
    auto it3 = vec3s_.find("emission_color");
    if (it3 != vec3s_.end()) return it3->second;
    it3 = vec3s_.find("emissive_color");
    if (it3 != vec3s_.end()) return it3->second;
    auto it4 = vec4s_.find("emission_color");
    if (it4 != vec4s_.end()) return glm::vec3(it4->second);
    it4 = vec4s_.find("emissive_color");
    if (it4 != vec4s_.end()) return glm::vec3(it4->second);
    return glm::vec3(0.0f);
}

float DSSLMaterialInstance::GetMetallic() const {
    return GetFloat("metallic", 0.0f);
}

float DSSLMaterialInstance::GetRoughness() const {
    return GetFloat("roughness", 0.5f);
}

float DSSLMaterialInstance::GetAO() const {
    return GetFloat("ao", 1.0f);
}

float DSSLMaterialInstance::GetNormalStrength() const {
    return GetFloat("normal_strength", 1.0f);
}

float DSSLMaterialInstance::GetAlphaCutoff() const {
    return GetFloat("alpha_cutoff", 0.5f);
}

bool DSSLMaterialInstance::GetAlphaTest() const {
    return render_modes_.alpha_test;
}

bool DSSLMaterialInstance::GetDoubleSided() const {
    return render_modes_.cull == "disabled";
}

unsigned int DSSLMaterialInstance::GetAlbedoTexture() const {
    unsigned int h = GetTexture("albedo_tex");
    if (h) return h;
    h = GetTexture("base_texture");
    if (h) return h;
    return GetTexture("albedo_texture");
}

unsigned int DSSLMaterialInstance::GetNormalTexture() const {
    unsigned int h = GetTexture("normal_tex");
    if (h) return h;
    h = GetTexture("normal_map");
    if (h) return h;
    return GetTexture("normal_texture");
}

unsigned int DSSLMaterialInstance::GetMetallicRoughnessTexture() const {
    unsigned int h = GetTexture("orm_tex");
    if (h) return h;
    h = GetTexture("metallic_roughness_tex");
    if (h) return h;
    return GetTexture("metallic_roughness_texture");
}

unsigned int DSSLMaterialInstance::GetEmissiveTexture() const {
    unsigned int h = GetTexture("emissive_tex");
    if (h) return h;
    h = GetTexture("emission_tex");
    if (h) return h;
    return GetTexture("emissive_texture");
}

unsigned int DSSLMaterialInstance::GetOcclusionTexture() const {
    unsigned int h = GetTexture("occlusion_tex");
    if (h) return h;
    return GetTexture("occlusion_texture");
}

} // namespace render
} // namespace dse
