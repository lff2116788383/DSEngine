#include <gtest/gtest.h>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "engine/render/rhi/rhi_device.h"
#define private public
#include "engine/runtime/frame_pipeline.h"
#undef private
#include "engine/runtime/runtime_render_shell.h"

using namespace dse::render;
using namespace dse::runtime;

namespace {
class RuntimeRenderShellCommandBuffer : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc&) override {}
    void EndRenderPass() override {}
    void BindPipeline(GraphicsPipelineHandle) override {}
    void BindVertexBuffer(uint32_t, BufferHandle, uint32_t, const std::vector<VertexAttr>&,
                          VertexInputRate) override {}
    void PushConstants(ShaderStage, uint32_t, const void*, uint32_t) override {}
    void Draw(uint32_t, uint32_t) override {}
    void BindIndexBuffer(BufferHandle, IndexType) override {}
    void BindTexture(uint32_t, TextureHandle, TextureDim) override {}
    void BindUniformBuffer(uint32_t, BufferHandle, uint32_t, uint32_t) override {}
    void BindStorageBuffer(uint32_t, BufferHandle, uint32_t, uint32_t) override {}
    void DrawIndexed(uint32_t, uint32_t, int32_t) override {}
    void DispatchComputePass(const ComputeDispatch&) override {}
    void DrawIndexedInstanced(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {}
    void DrawIndexedIndirect(BufferHandle, uint32_t) override {}
    void ClearColor(const glm::vec4&) override {}
    void SetViewport(int, int, int, int) override {}
    void BindGlobalShadowMap(unsigned int, TextureHandle) override {}
    void BindGlobalSpotShadowMap(unsigned int, TextureHandle) override {}
    void BindGlobalPointShadowMap(unsigned int, TextureHandle) override {}
};

class RuntimeRenderShellRhiDevice : public RhiDevice {
public:
    std::shared_ptr<RuntimeRenderShellCommandBuffer> command_buffer = std::make_shared<RuntimeRenderShellCommandBuffer>();
    std::shared_ptr<CommandBuffer> submitted_command_buffer;
    RenderStats stats{};
    int begin_frame_count = 0;
    int submit_count = 0;
    int end_frame_count = 0;

    RhiBackend GetBackend() const override { return RhiBackend::OpenGL; }
    void Shutdown() override {}
    void BeginFrame() override { ++begin_frame_count; }
    RenderTargetHandle CreateRenderTarget(const RenderTargetDesc&) override { return {}; }
    TextureHandle GetRenderTargetColorTexture(RenderTargetHandle render_target_handle) const override {
        return TextureHandle::from_raw(render_target_handle.raw() + 500u);
    }
    TextureHandle GetRenderTargetDepthTexture(RenderTargetHandle render_target_handle) const override {
        return TextureHandle::from_raw(render_target_handle.raw() + 1000u);
    }
    std::vector<unsigned char> ReadRenderTargetColorRgba8(RenderTargetHandle) const override { return {}; }
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(RenderTargetHandle) const override { return {}; }
    TextureHandle CreateTexture2D(int, int, const unsigned char*, bool) override { return {}; }
    TextureHandle CreateTextureCube(int, int, const unsigned char* const[6], bool) override { return {}; }
    TextureHandle CreateTexture3D(int, int, int, const unsigned char*, bool) override { return {}; }
    void DeleteTexture(TextureHandle) override {}
    ShaderHandle CreateShaderProgram(const std::string&, const std::string&) override { return {}; }
    void DeleteShaderProgram(ShaderHandle) override {}
    dse::render::PipelineHandle CreatePipelineState(const PipelineStateDesc&) override { return {}; }
    BufferHandle CreateBuffer(size_t, const void*, bool, bool) override { return {}; }
    void UpdateBuffer(BufferHandle, size_t, size_t, const void*, bool) override {}
    void DeleteBuffer(BufferHandle) override {}
    VertexArrayHandle CreateVertexArray() override { return {}; }
    void DeleteVertexArray(VertexArrayHandle) override {}
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override { return command_buffer; }
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override { submitted_command_buffer = std::move(cmd_buffer); ++submit_count; }
    void EndFrame() override { ++end_frame_count; }
    const RenderStats& LastFrameStats() const override { return stats; }
};

RuntimeRenderShellRhiDevice* InstallRhi(FramePipeline& pipeline) {
    auto device = std::make_unique<RuntimeRenderShellRhiDevice>();
    auto* raw = device.get();
    pipeline.runtime_context_.rhi_device = std::move(device);
    return raw;
}
}

// 测试 运行时渲染外壳单元：开始帧且创建命令Bufferentrust RHI设备
TEST(RuntimeRenderShellUnitTest, BeginFrameAndCreateCommandBufferentrustRhiDevice) {
    FramePipeline pipeline;
    auto* device = InstallRhi(pipeline);

    BeginRuntimeRenderFrame(pipeline);
    auto cmd = CreateRuntimeRenderCommandBuffer(pipeline);

    EXPECT_EQ(device->begin_frame_count, 1);
    EXPECT_EQ(cmd, device->command_buffer);
}

// 测试 运行时渲染外壳单元：绑定阴影Maps写入全局状态从渲染目标深度映射
TEST(RuntimeRenderShellUnitTest, BindShadowMapsWritingGlobalStateFromRenderTargetDepthMap) {
    FramePipeline pipeline;
    auto* device = InstallRhi(pipeline);

    for (int i = 0; i < CSM_CASCADES; ++i) {
        pipeline.render_resources_.shadow_render_target[i] =
            RenderTargetHandle::from_raw(10u + static_cast<unsigned int>(i));
    }
    for (int i = 0; i < 4; ++i) {
        pipeline.render_resources_.spot_shadow_render_target[i] =
            RenderTargetHandle::from_raw(20u + static_cast<unsigned int>(i));
        pipeline.render_resources_.point_shadow_render_target[i] =
            RenderTargetHandle::from_raw(30u + static_cast<unsigned int>(i));
    }

    BindRuntimeShadowMaps(pipeline);

    const auto& state = device->GetGlobalRenderState();
    for (int i = 0; i < CSM_CASCADES; ++i) {
        EXPECT_EQ(state.shadow_map[i],
                  TextureHandle::from_raw(1010u + static_cast<unsigned int>(i)));
    }
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(state.spot_shadow_map[i],
                  TextureHandle::from_raw(1020u + static_cast<unsigned int>(i)));
        EXPECT_EQ(state.point_shadow_map[i],
                  TextureHandle::from_raw(1030u + static_cast<unsigned int>(i)));
    }
}

// 测试 运行时渲染外壳单元：提交且结束帧提交Commands于Sequence且结束帧
TEST(RuntimeRenderShellUnitTest, SubmitAndEndFrameSubmitCommandsInSequenceAndEndFrame) {
    FramePipeline pipeline;
    auto* device = InstallRhi(pipeline);
    auto cmd = CreateRuntimeRenderCommandBuffer(pipeline);

    SubmitAndEndRuntimeRenderFrame(pipeline, cmd);

    EXPECT_EQ(device->submit_count, 1);
    EXPECT_EQ(device->submitted_command_buffer, cmd);
    EXPECT_EQ(device->end_frame_count, 1);
}

// 测试 运行时渲染外壳单元：Finalize Framesynchronous RHI统计帧管线
TEST(RuntimeRenderShellUnitTest, FinalizeFramesynchronousRhiStatisticsFramePipeline) {
    FramePipeline pipeline;
    auto* device = InstallRhi(pipeline);
    device->stats.draw_calls = 7;
    device->stats.material_switches = 3;
    device->stats.max_batch_sprites = 11;
    device->stats.sprite_count = 29;

    FinalizeRuntimeRenderFrame(pipeline);

    EXPECT_EQ(pipeline.LastDrawCalls(), 7);
    EXPECT_EQ(pipeline.LastMaterialSwitches(), 3);
    EXPECT_EQ(pipeline.LastMaxBatchSprites(), 11);
    EXPECT_EQ(pipeline.LastSpriteCount(), 29);
}
