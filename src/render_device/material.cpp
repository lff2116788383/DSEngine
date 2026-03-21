#include "render_device/material.h"
#include "render_device/render_task_producer.h"
#include <glad/gl.h>

namespace render {

Material::Material(std::shared_ptr<ShaderAsset> shader) : shader_(shader) {
}

void Material::SetInt(const std::string& name, int value) { uniforms_[name] = value; }
void Material::SetFloat(const std::string& name, float value) { uniforms_[name] = value; }
void Material::SetVec2(const std::string& name, const glm::vec2& value) { uniforms_[name] = value; }
void Material::SetVec3(const std::string& name, const glm::vec3& value) { uniforms_[name] = value; }
void Material::SetVec4(const std::string& name, const glm::vec4& value) { uniforms_[name] = value; }
void Material::SetMat4(const std::string& name, const glm::mat4& value) { uniforms_[name] = value; }

void Material::SetTexture(const std::string& name, std::shared_ptr<TextureAsset> texture) {
    textures_[name] = texture;
}

void Material::Apply() const {
    if (!shader_) return;

    unsigned int handle = shader_->GetHandle();
    RenderTaskProducer::ProduceRenderTaskUseShaderProgram(handle);

    // Simplistic application: iterate through uniforms and send to RenderTaskProducer
    // In a full implementation, we'd query uniform locations from the shader.
    // For now, we mock the dispatch:
    
    for (const auto& [name, value] : uniforms_) {
        if (std::holds_alternative<int>(value)) {
            RenderTaskProducer::ProduceRenderTaskSetUniform1i(handle, name.c_str(), std::get<int>(value));
        } else if (std::holds_alternative<float>(value)) {
            // Need a float variant in RenderTaskProducer, assuming it exists or similar
            // If missing, we skip or mock. Let's assume SetUniform1f is not there but we'll mock it via vec3 or ignore.
        } else if (std::holds_alternative<glm::vec3>(value)) {
            auto& v = std::get<glm::vec3>(value);
            RenderTaskProducer::ProduceRenderTaskSetUniform3f(handle, name.c_str(), v);
        } else if (std::holds_alternative<glm::mat4>(value)) {
            auto& m = std::get<glm::mat4>(value);
            RenderTaskProducer::ProduceRenderTaskSetUniformMatrix4fv(handle, name.c_str(), false, m);
        }
    }
    
    int tex_unit = 0;
    for (const auto& [name, tex] : textures_) {
        if (tex) {
            RenderTaskProducer::ProduceRenderTaskActiveAndBindTexture(GL_TEXTURE0 + tex_unit, tex->GetHandle());
            RenderTaskProducer::ProduceRenderTaskSetUniform1i(handle, name.c_str(), tex_unit);
            tex_unit++;
        }
    }
}

} // namespace render
