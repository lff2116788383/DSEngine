/**
 * @file forwarding_command_buffer.h
 * @brief 立即转发型命令缓冲基类
 *
 * DX11、Vulkan、OpenGL 三端的 CommandBuffer 均继承此类。
 * 提取的共享逻辑：
 * - SetCamera()：缓存 view/projection 矩阵
 * - BindGlobal*ShadowMap()：阴影贴图绑定直接委托 RhiDevice 基类指针
 * - pending uniform 暂存与清理
 */

#ifndef DSE_FORWARDING_COMMAND_BUFFER_H
#define DSE_FORWARDING_COMMAND_BUFFER_H

#include "engine/render/rhi/rhi_device.h"
#include <unordered_map>

namespace dse {
namespace render {

class ForwardingCommandBuffer : public CommandBuffer {
public:
    // --- 共享实现（三端完全一致） ---

    void SetCamera(const glm::mat4& view, const glm::mat4& projection) override {
        view_ = view;
        projection_ = projection;
    }

    void SetGlobalMat4(const std::string& name, const glm::mat4& value) override {
        pending_mat4_[name] = value;
    }

    void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) override {
        pending_mat4_array_[name] = values;
    }

    void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) override {
        pending_float_array_[name] = values;
    }

    void BindGlobalShadowMap(unsigned int index, unsigned int texture_handle) override {
        if (base_device_) base_device_->SetGlobalShadowMap(index, texture_handle);
    }

    void BindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override {
        if (base_device_) base_device_->SetGlobalSpotShadowMap(index, texture_handle);
    }

    void BindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override {
        if (base_device_) base_device_->SetGlobalPointShadowMap(index, texture_handle);
    }

    // --- pending uniform 访问器 ---

    const std::unordered_map<std::string, glm::mat4>& pending_mat4() const { return pending_mat4_; }
    const std::unordered_map<std::string, std::vector<glm::mat4>>& pending_mat4_array() const { return pending_mat4_array_; }
    const std::unordered_map<std::string, std::vector<float>>& pending_float_array() const { return pending_float_array_; }

    void ClearPendingUniforms() {
        pending_mat4_.clear();
        pending_mat4_array_.clear();
        pending_float_array_.clear();
    }

    void ResetBase() {
        view_ = glm::mat4(1.0f);
        projection_ = glm::mat4(1.0f);
        ClearPendingUniforms();
    }

protected:
    /// 将 pending 阴影/光源 mat4 array 派发到 Device 全局状态。
    /// 供子类在 DrawMeshBatch() 入口调用，统一派发时机。
    void DispatchPendingLightArrays() {
        if (!base_device_) return;
        {
            auto it = pending_mat4_array_.find("u_light_space_matrices");
            if (it != pending_mat4_array_.end()) {
                for (size_t i = 0; i < it->second.size() && i < 3; ++i)
                    base_device_->SetGlobalLightSpaceMatrix(static_cast<unsigned int>(i), it->second[i]);
            }
        }
        {
            auto it = pending_float_array_.find("u_cascade_splits");
            if (it != pending_float_array_.end()) {
                for (size_t i = 0; i < it->second.size() && i < 3; ++i)
                    base_device_->SetGlobalCascadeSplit(static_cast<unsigned int>(i), it->second[i]);
            }
        }
        {
            auto it = pending_mat4_array_.find("u_spot_light_space_matrices");
            if (it != pending_mat4_array_.end()) {
                for (size_t i = 0; i < it->second.size() && i < 4; ++i)
                    base_device_->SetGlobalSpotLightSpaceMatrix(static_cast<unsigned int>(i), it->second[i]);
            }
        }
    }

    RhiDevice* base_device_ = nullptr;
    glm::mat4 view_ = glm::mat4(1.0f);
    glm::mat4 projection_ = glm::mat4(1.0f);

    std::unordered_map<std::string, glm::mat4> pending_mat4_;
    std::unordered_map<std::string, std::vector<glm::mat4>> pending_mat4_array_;
    std::unordered_map<std::string, std::vector<float>> pending_float_array_;
};

} // namespace render
} // namespace dse

#endif // DSE_FORWARDING_COMMAND_BUFFER_H
