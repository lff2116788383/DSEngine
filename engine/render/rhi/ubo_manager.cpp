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

    PointLightsUBO pl{};
    SpotLightsUBO sl{};
    SpotLightDataUBO sld{};
    BoneMatricesUBO bm{};
    MorphWeightsUBO mw{};
    point_lights_buffer_ = CreateUBO(sizeof(PointLightsUBO), &pl, UBOBindingPoint::PointLights);
    spot_lights_buffer_ = CreateUBO(sizeof(SpotLightsUBO), &sl, UBOBindingPoint::SpotLights);
    spot_light_data_buffer_ = CreateUBO(sizeof(SpotLightDataUBO), &sld, UBOBindingPoint::SpotLightData);
    bone_matrices_buffer_ = CreateUBO(sizeof(BoneMatricesUBO), &bm, UBOBindingPoint::BoneMatrices);
    morph_weights_buffer_ = CreateUBO(sizeof(MorphWeightsUBO), &mw, UBOBindingPoint::MorphWeights);

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
    unsigned int* extra[] = { &point_lights_buffer_, &spot_lights_buffer_,
                              &spot_light_data_buffer_, &bone_matrices_buffer_,
                              &morph_weights_buffer_ };
    for (auto* p : extra) {
        if (*p != 0) { glDeleteBuffers(1, p); *p = 0; }
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

void UBOManager::UploadPointLights(const PointLightsUBO& data) {
    if (point_lights_buffer_ != 0)
        UpdateUBO(point_lights_buffer_, sizeof(PointLightsUBO), &data);
}

void UBOManager::UploadSpotLights(const SpotLightsUBO& data) {
    if (spot_lights_buffer_ != 0)
        UpdateUBO(spot_lights_buffer_, sizeof(SpotLightsUBO), &data);
}

void UBOManager::UploadSpotLightData(const SpotLightDataUBO& data) {
    if (spot_light_data_buffer_ != 0)
        UpdateUBO(spot_light_data_buffer_, sizeof(SpotLightDataUBO), &data);
}

void UBOManager::UploadBoneMatrices(const BoneMatricesUBO& data) {
    if (bone_matrices_buffer_ != 0)
        UpdateUBO(bone_matrices_buffer_, sizeof(BoneMatricesUBO), &data);
}

void UBOManager::UploadMorphWeights(const MorphWeightsUBO& data) {
    if (morph_weights_buffer_ != 0)
        UpdateUBO(morph_weights_buffer_, sizeof(MorphWeightsUBO), &data);
}

void UBOManager::BindAll() const {
    Bind(UBOBindingPoint::PerFrame);
    Bind(UBOBindingPoint::PerScene);
    Bind(UBOBindingPoint::PerMaterial);
    Bind(UBOBindingPoint::PointLights);
    Bind(UBOBindingPoint::SpotLights);
    Bind(UBOBindingPoint::SpotLightData);
    Bind(UBOBindingPoint::BoneMatrices);
    Bind(UBOBindingPoint::MorphWeights);
}

void UBOManager::Bind(UBOBindingPoint binding) const {
    unsigned int buffer = 0;
    switch (binding) {
        case UBOBindingPoint::PerFrame:      buffer = per_frame_buffer_; break;
        case UBOBindingPoint::PerScene:      buffer = per_scene_buffer_; break;
        case UBOBindingPoint::PerMaterial:   buffer = per_material_buffer_; break;
        case UBOBindingPoint::PointLights:   buffer = point_lights_buffer_; break;
        case UBOBindingPoint::SpotLights:    buffer = spot_lights_buffer_; break;
        case UBOBindingPoint::SpotLightData: buffer = spot_light_data_buffer_; break;
        case UBOBindingPoint::BoneMatrices:  buffer = bone_matrices_buffer_; break;
        case UBOBindingPoint::MorphWeights:  buffer = morph_weights_buffer_; break;
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
