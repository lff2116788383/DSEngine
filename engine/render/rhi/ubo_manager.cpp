/**
 * @file ubo_manager.cpp
 * @brief UBO 管理器实现
 */

#include "engine/render/rhi/ubo_manager.h"
#include "engine/base/debug.h"
#include <glad/gl.h>

namespace dse {
namespace render {

void UBOManager::Init() {
    if (initialized_) return;

    // 使用空数据创建 UBO，后续通过 Upload 填充
    PerFrameUBO frame_data{};
    PerSceneUBO scene_data{};
    PerMaterialUBO material_data{};

    per_frame_buffer_ = CreateUBO(sizeof(PerFrameUBO), &frame_data, UBOBindingPoint::PerFrame);
    per_scene_buffer_ = CreateUBO(sizeof(PerSceneUBO), &scene_data, UBOBindingPoint::PerScene);
    per_material_buffer_ = CreateUBO(sizeof(PerMaterialUBO), &material_data, UBOBindingPoint::PerMaterial);

    initialized_ = true;
}

void UBOManager::Shutdown() {
    if (!initialized_) return;

    if (per_frame_buffer_ != 0) {
        glDeleteBuffers(1, &per_frame_buffer_);
        per_frame_buffer_ = 0;
    }
    if (per_scene_buffer_ != 0) {
        glDeleteBuffers(1, &per_scene_buffer_);
        per_scene_buffer_ = 0;
    }
    if (per_material_buffer_ != 0) {
        glDeleteBuffers(1, &per_material_buffer_);
        per_material_buffer_ = 0;
    }

    initialized_ = false;
}

void UBOManager::UploadPerFrame(const PerFrameUBO& data) {
    if (per_frame_buffer_ != 0) {
        UpdateUBO(per_frame_buffer_, sizeof(PerFrameUBO), &data);
    }
}

void UBOManager::UploadPerScene(const PerSceneUBO& data) {
    if (per_scene_buffer_ != 0) {
        UpdateUBO(per_scene_buffer_, sizeof(PerSceneUBO), &data);
    }
}

void UBOManager::UploadPerMaterial(const PerMaterialUBO& data) {
    if (per_material_buffer_ != 0) {
        UpdateUBO(per_material_buffer_, sizeof(PerMaterialUBO), &data);
    }
}

void UBOManager::BindAll() const {
    Bind(UBOBindingPoint::PerFrame);
    Bind(UBOBindingPoint::PerScene);
    Bind(UBOBindingPoint::PerMaterial);
}

void UBOManager::Bind(UBOBindingPoint binding) const {
    unsigned int buffer = 0;
    switch (binding) {
        case UBOBindingPoint::PerFrame:   buffer = per_frame_buffer_; break;
        case UBOBindingPoint::PerScene:   buffer = per_scene_buffer_; break;
        case UBOBindingPoint::PerMaterial: buffer = per_material_buffer_; break;
        default: return;
    }
    if (buffer != 0) {
        glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(binding), buffer);
    }
}

unsigned int UBOManager::CreateUBO(size_t size, const void* data, UBOBindingPoint binding) {
    unsigned int buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, buffer);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // 绑定到对应的 binding point
    glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(binding), buffer);

    return buffer;
}

void UBOManager::UpdateUBO(unsigned int buffer, size_t size, const void* data) {
    glBindBuffer(GL_UNIFORM_BUFFER, buffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(size), data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

} // namespace render
} // namespace dse
