/**
 * @file ubo_manager.h
 * @brief UBO 管理器 - 创建/更新/绑定 Uniform Buffer Object
 *
 * 职责：
 * 1. 管理 PerFrame / PerScene / PerMaterial 三类 UBO 的 GL 缓冲区生命周期
 * 2. 提供 Upload 接口将 CPU 端数据上传到 GPU
 * 3. 自动绑定 UBO 到对应的 binding point
 *
 * 使用模式：
 *   UBOManager ubo_mgr;
 *   ubo_mgr.Init();                                    // 创建所有 GL 缓冲区
 *   ubo_mgr.UploadPerFrame(per_frame_data);            // 每帧开始时
 *   ubo_mgr.UploadPerScene(per_scene_data);            // 每帧光照更新时
 *   ubo_mgr.UploadPerMaterial(per_material_data);      // 每材质切换时
 *   ubo_mgr.BindAll();                                 // 绑定到当前着色器程序
 *   ubo_mgr.Shutdown();                                // 清理 GL 资源
 */

#ifndef DSE_RENDER_UBO_MANAGER_H
#define DSE_RENDER_UBO_MANAGER_H

#include "engine/render/rhi/ubo_types.h"

namespace dse {
namespace render {

class UBOManager {
public:
    UBOManager() = default;
    ~UBOManager() = default;

    /// 创建所有 UBO 缓冲区（在 GL 上下文就绪后调用）
    void Init();

    /// 销毁所有 UBO 缓冲区
    void Shutdown();

    /// 上传 PerFrame 数据（每帧调用一次）
    void UploadPerFrame(const PerFrameUBO& data);

    /// 上传 PerScene 数据（每帧光照更新时调用）
    void UploadPerScene(const PerSceneUBO& data);

    /// 上传 PerMaterial 数据（每材质切换时调用）
    void UploadPerMaterial(const PerMaterialUBO& data);

    /// 上传 PointLights 数据（每次 DrawMeshBatch）
    void UploadPointLights(const PointLightsUBO& data);

    /// 上传 SpotLights 数据（每次 DrawMeshBatch）
    void UploadSpotLights(const SpotLightsUBO& data);

    /// 上传 SpotLightData 数据（聚光灯空间矩阵）
    void UploadSpotLightData(const SpotLightDataUBO& data);

    /// 上传 BoneMatrices 数据（每次 DrawMeshBatch）
    void UploadBoneMatrices(const BoneMatricesUBO& data);

    /// 上传 MorphWeights 数据（每次 DrawMeshBatch）
    void UploadMorphWeights(const MorphWeightsUBO& data);

    /// 上传 LightProbeData 数据（每帧 SH 球谐系数）
    void UploadLightProbeData(const LightProbeDataUBO& data);

    /// 绑定所有 UBO 到对应的 binding point
    void BindAll() const;

    /// 绑定指定 binding point 的 UBO
    void Bind(UBOBindingPoint binding) const;

    // --- 访问器 ---

    unsigned int per_frame_buffer() const { return per_frame_buffer_; }
    unsigned int per_scene_buffer() const { return per_scene_buffer_; }
    unsigned int per_material_buffer() const { return per_material_buffer_; }
    unsigned int point_lights_buffer() const { return point_lights_buffer_; }
    unsigned int spot_lights_buffer() const { return spot_lights_buffer_; }
    unsigned int spot_light_data_buffer() const { return spot_light_data_buffer_; }
    unsigned int bone_matrices_buffer() const { return bone_matrices_buffer_; }
    unsigned int morph_weights_buffer() const { return morph_weights_buffer_; }
    unsigned int light_probe_data_buffer() const { return light_probe_data_buffer_; }

    bool initialized() const { return initialized_; }

private:
    /// 创建单个 UBO 缓冲区
    unsigned int CreateUBO(size_t size, const void* data, UBOBindingPoint binding);

    /// 更新 UBO 缓冲区数据（使用 glBufferSubData）
    void UpdateUBO(unsigned int buffer, size_t size, const void* data);

    unsigned int per_frame_buffer_ = 0;
    unsigned int per_scene_buffer_ = 0;
    unsigned int per_material_buffer_ = 0;
    unsigned int point_lights_buffer_ = 0;
    unsigned int spot_lights_buffer_ = 0;
    unsigned int spot_light_data_buffer_ = 0;
    unsigned int bone_matrices_buffer_ = 0;
    unsigned int morph_weights_buffer_ = 0;
    unsigned int light_probe_data_buffer_ = 0;

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_UBO_MANAGER_H
