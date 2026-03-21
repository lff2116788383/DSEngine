#ifndef DSE_RENDER_MATERIAL_H
#define DSE_RENDER_MATERIAL_H

#include "phase1/asset/asset_manager.h"
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <string>
#include <variant>

namespace render {

class Material {
public:
    using UniformValue = std::variant<int, float, glm::vec2, glm::vec3, glm::vec4, glm::mat4>;

    Material(std::shared_ptr<ShaderAsset> shader);

    void SetInt(const std::string& name, int value);
    void SetFloat(const std::string& name, float value);
    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);
    void SetMat4(const std::string& name, const glm::mat4& value);
    void SetTexture(const std::string& name, std::shared_ptr<TextureAsset> texture);

    void Apply() const;

    std::shared_ptr<ShaderAsset> GetShader() const { return shader_; }

private:
    std::shared_ptr<ShaderAsset> shader_;
    std::unordered_map<std::string, UniformValue> uniforms_;
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> textures_;
};

} // namespace render

#endif // DSE_RENDER_MATERIAL_H
