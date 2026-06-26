/**
 * @file forwarding_command_buffer.h
 * @brief 立即转发型命令缓冲基类
 *
 * DX11、Vulkan、OpenGL 三端的 CommandBuffer 均继承此类。
 * 提取的共享逻辑：
 * - BindGlobal*ShadowMap()：阴影贴图绑定直接委托 RhiDevice 基类指针
 */

#ifndef DSE_FORWARDING_COMMAND_BUFFER_H
#define DSE_FORWARDING_COMMAND_BUFFER_H

#include "engine/render/rhi/rhi_device.h"

namespace dse {
namespace render {

class ForwardingCommandBuffer : public CommandBuffer {
public:
    // --- 共享实现（三端完全一致） ---

    void BindGlobalShadowMap(unsigned int index, unsigned int texture_handle) override {
        if (base_device_) base_device_->SetGlobalShadowMap(index, texture_handle);
    }

    void BindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override {
        if (base_device_) base_device_->SetGlobalSpotShadowMap(index, texture_handle);
    }

    void BindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override {
        if (base_device_) base_device_->SetGlobalPointShadowMap(index, texture_handle);
    }

    void ResetBase() {}

protected:
    RhiDevice* base_device_ = nullptr;
};

} // namespace render
} // namespace dse

#endif // DSE_FORWARDING_COMMAND_BUFFER_H
