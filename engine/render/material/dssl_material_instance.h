#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

class AssetManager;

namespace dse {
namespace render {

struct DSSLUniformInfo {
    std::string name;
    std::string type;       // float, vec2, vec3, vec4, int, sampler2D, etc.
    std::string default_value;
    bool is_sampler = false;
    std::vector<std::string> hints;
};

enum class DSSLShaderType {
    Surface = 0,
    Unlit,
    Particle,
    Sky,
    Postprocess,
    Canvas
};

class DSSLMaterialInstance {
public:
    DSSLMaterialInstance(unsigned int id, const std::string& dssl_path);
    ~DSSLMaterialInstance() = default;

    unsigned int GetId() const { return id_; }
    const std::string& GetDSSLPath() const { return dssl_path_; }
    DSSLShaderType GetShaderType() const { return shader_type_; }

    // 设置解析后的 uniform 信息
    void SetUniformInfos(const std::vector<DSSLUniformInfo>& infos) { uniform_infos_ = infos; }
    void SetShaderType(DSSLShaderType type) { shader_type_ = type; }

    // 获取所有 uniform 信息
    const std::vector<DSSLUniformInfo>& GetUniformInfos() const { return uniform_infos_; }

    // === Uniform 值设置/获取 API ===
    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);
    void SetInt(const std::string& name, int value);
    void SetTexture(const std::string& name, unsigned int texture_handle);

    float GetFloat(const std::string& name, float fallback = 0.0f) const;
    glm::vec2 GetVec2(const std::string& name, const glm::vec2& fallback = glm::vec2(0)) const;
    glm::vec3 GetVec3(const std::string& name, const glm::vec3& fallback = glm::vec3(0)) const;
    glm::vec4 GetVec4(const std::string& name, const glm::vec4& fallback = glm::vec4(0)) const;
    int GetInt(const std::string& name, int fallback = 0) const;
    unsigned int GetTexture(const std::string& name) const;

    // === 应用到 MeshRendererComponent 的便捷接口 ===
    // 将 DSSL uniform 值映射到引擎标准材质属性
    glm::vec4 GetBaseColor() const;
    glm::vec3 GetEmissiveColor() const;
    float GetMetallic() const;
    float GetRoughness() const;
    float GetAO() const;
    float GetNormalStrength() const;
    float GetAlphaCutoff() const;
    bool GetAlphaTest() const;
    bool GetDoubleSided() const;
    unsigned int GetAlbedoTexture() const;
    unsigned int GetNormalTexture() const;
    unsigned int GetMetallicRoughnessTexture() const;
    unsigned int GetEmissiveTexture() const;
    unsigned int GetOcclusionTexture() const;

    // 渲染模式
    struct RenderModes {
        std::string blend = "disabled";
        std::string cull = "back";
        bool shadows_enabled = true;
        bool alpha_test = false;
    };
    void SetRenderModes(const RenderModes& modes) { render_modes_ = modes; }
    const RenderModes& GetRenderModes() const { return render_modes_; }

private:
    unsigned int id_;
    std::string dssl_path_;
    DSSLShaderType shader_type_ = DSSLShaderType::Surface;
    std::vector<DSSLUniformInfo> uniform_infos_;
    RenderModes render_modes_;

    // 统一存储所有 uniform 的值
    std::unordered_map<std::string, float> floats_;
    std::unordered_map<std::string, glm::vec2> vec2s_;
    std::unordered_map<std::string, glm::vec3> vec3s_;
    std::unordered_map<std::string, glm::vec4> vec4s_;
    std::unordered_map<std::string, int> ints_;
    std::unordered_map<std::string, unsigned int> textures_;
};

} // namespace render
} // namespace dse
