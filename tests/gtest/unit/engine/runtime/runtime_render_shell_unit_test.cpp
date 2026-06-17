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
    void SetPipelineState(unsigned int) override {}
    void SetCamera(const glm::mat4&, const glm::mat4&) override {}
    void DrawMeshBatch(const std::vector<MeshDrawItem>&) override {}
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>&) override {}
    void ClearColor(const glm::vec4&) override {}
    void SetGlobalMat4(const std::string&, const glm::mat4&) override {}
    void SetGlobalMat4Array(const std::string&, const std::vector<glm::mat4>&) override {}
    void SetGlobalFloatArray(const std::string&, const std::vector<float>&) override {}
    void DrawPostProcess(PostProcessRequest) override {}
    void DrawParticles3D(const std::vector<Particle3DDrawItem>&, const glm::mat4&, const glm::mat4&) override {}
    void DrawHairStrands(const std::vector<HairDrawItem>&, const glm::mat4&, const glm::mat4&) override {}
    void SetViewport(int, int, int, int) override {}
    void BindGlobalShadowMap(unsigned int, unsigned int) override {}
    void BindGlobalSpotShadowMap(unsigned int, unsigned int) override {}
    void BindGlobalPointShadowMap(unsigned int, unsigned int) override {}
};

class RuntimeRenderShellRhiDevice : public RhiDevice {
public:
    std::shared_ptr<RuntimeRenderShellCommandBuffer> command_buffer = std::make_shared<RuntimeRenderShellCommandBuffer>();
    std::shared_ptr<CommandBuffer> submitted_command_buffer;
    RenderStats stats{};
    int begin_frame_count = 0;
    int submit_count = 0;
    int end_frame_count = 0;

    void Shutdown() override {}
    void BeginFrame() override { ++begin_frame_count; }
    unsigned int CreateRenderTarget(const RenderTargetDesc&) override { return 0; }
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override { return render_target_handle + 500u; }
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override { return render_target_handle + 1000u; }
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int) const override { return {}; }
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int) const override { return {}; }
    unsigned int CreateTexture2D(int, int, const unsigned char*, bool) override { return 0; }
    unsigned int CreateTextureCube(int, int, const unsigned char* const[6], bool) override { return 0; }
    unsigned int CreateTexture3D(int, int, int, const unsigned char*, bool) override { return 0; }
    void DeleteTexture(unsigned int) override {}
    unsigned int CreateShaderProgram(const std::string&, const std::string&) override { return 0; }
    void DeleteShaderProgram(unsigned int) override {}
    unsigned int CreatePipelineState(const PipelineStateDesc&) override { return 0; }
    unsigned int CreateBuffer(size_t, const void*, bool, bool) override { return 0; }
    void UpdateBuffer(unsigned int, size_t, size_t, const void*, bool) override {}
    void DeleteBuffer(unsigned int) override {}
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
        pipeline.render_resources_.shadow_render_target[i] = 10u + static_cast<unsigned int>(i);
    }
    for (int i = 0; i < 4; ++i) {
        pipeline.render_resources_.spot_shadow_render_target[i] = 20u + static_cast<unsigned int>(i);
        pipeline.render_resources_.point_shadow_render_target[i] = 30u + static_cast<unsigned int>(i);
    }

    BindRuntimeShadowMaps(pipeline);

    const auto& state = device->GetGlobalRenderState();
    for (int i = 0; i < CSM_CASCADES; ++i) {
        EXPECT_EQ(state.shadow_map[i], 1010u + static_cast<unsigned int>(i));
    }
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(state.spot_shadow_map[i], 1020u + static_cast<unsigned int>(i));
        EXPECT_EQ(state.point_shadow_map[i], 1030u + static_cast<unsigned int>(i));
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
