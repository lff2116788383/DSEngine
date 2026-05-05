/**
 * @file dx11_rhi_test.cpp
 * @brief D3D11 RHI 单元测试（无 GPU / 无窗口）
 *
 * 覆盖场景：
 * 1. DX11RhiDevice 构造/析构不崩溃
 * 2. 未初始化时所有接口返回安全值
 * 3. SetGlobalShadowMap 越界索引静默忽略
 * 4. DX11CommandBuffer 无 device 时各接口不崩溃
 * 5. DX11DrawExecutor 全局状态边界检查
 * 6. DX11ShaderManager 未初始化时句柄为零
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_factory.h"
#include "engine/render/rhi/rhi_device.h"

#ifdef DSE_ENABLE_D3D11
#include "engine/render/rhi/dx11/dx11_rhi_device.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/render/rhi/dx11/dx11_resource_manager.h"
#include "engine/render/rhi/dx11/dx11_pipeline_state_manager.h"
#include "engine/render/rhi/dx11/dx11_shader_manager.h"
#include "engine/render/rhi/dx11/dx11_draw_executor.h"
#endif

#include <glm/glm.hpp>

using namespace dse::render;

// ============================================================
// RHI Factory — D3D11 创建
// ============================================================

#ifdef DSE_ENABLE_D3D11

TEST(RhiFactoryTest, BackendToString_D3D11) {
    EXPECT_EQ(RhiBackendToString(RhiBackend::D3D11), "D3D11");
}

TEST(RhiFactoryTest, CreateRhiDevice_D3D11返回非空) {
    auto device = CreateRhiDevice(RhiBackend::D3D11);
    EXPECT_NE(device, nullptr);
}

// ============================================================
// DX11RhiDevice 无 GPU 测试
// ============================================================

TEST(DX11RhiDeviceTest, 构造析构不崩溃) {
    DX11RhiDevice device;
}

TEST(DX11RhiDeviceTest, 未初始化时Shutdown安全) {
    DX11RhiDevice device;
    device.Shutdown();
}

TEST(DX11RhiDeviceTest, 未初始化时BeginFrame安全) {
    DX11RhiDevice device;
    device.BeginFrame();
}

TEST(DX11RhiDeviceTest, 未初始化时EndFrame安全) {
    DX11RhiDevice device;
    device.EndFrame();
}

TEST(DX11RhiDeviceTest, 未初始化时Submit安全) {
    DX11RhiDevice device;
    auto cmd = std::make_shared<DX11CommandBuffer>();
    device.Submit(cmd);
}

TEST(DX11RhiDeviceTest, 未初始化时CreateRenderTarget返回零) {
    DX11RhiDevice device;
    RenderTargetDesc desc{};
    desc.width = 256;
    desc.height = 256;
    desc.has_color = true;
    unsigned int handle = device.CreateRenderTarget(desc);
    EXPECT_EQ(handle, 0u);
}

TEST(DX11RhiDeviceTest, 未初始化时CreateTexture2D返回零) {
    DX11RhiDevice device;
    unsigned int handle = device.CreateTexture2D(4, 4, nullptr, false);
    EXPECT_EQ(handle, 0u);
}

TEST(DX11RhiDeviceTest, CreateVertexArray返回递增句柄) {
    DX11RhiDevice device;
    unsigned int h1 = device.CreateVertexArray();
    unsigned int h2 = device.CreateVertexArray();
    EXPECT_NE(h1, h2);
    EXPECT_GT(h2, h1);
}

TEST(DX11RhiDeviceTest, DeleteVertexArray_NoOp不崩溃) {
    DX11RhiDevice device;
    device.DeleteVertexArray(99999);
    device.DeleteVertexArray(0);
}

TEST(DX11RhiDeviceTest, LastFrameStats默认值为零) {
    DX11RhiDevice device;
    const auto& stats = device.LastFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

TEST(DX11RhiDeviceTest, 子系统访问器可调用) {
    DX11RhiDevice device;
    auto& ctx    = device.context();
    auto& res    = device.resource_mgr();
    auto& state  = device.state_mgr();
    auto& shader = device.shader_mgr();
    auto& draw   = device.draw_executor();
    (void)ctx; (void)res; (void)state; (void)shader; (void)draw;
}

// ============================================================
// SetGlobalShadowMap 越界静默忽略
// ============================================================

TEST(DX11RhiDeviceTest, SetGlobalShadowMap越界静默忽略) {
    DX11RhiDevice device;
    // 有效索引
    device.SetGlobalShadowMap(0, 100);
    device.SetGlobalShadowMap(2, 200);
    // 越界索引 >= 3 应静默忽略，不崩溃
    device.SetGlobalShadowMap(3, 999);
    device.SetGlobalShadowMap(100, 999);
}

TEST(DX11RhiDeviceTest, 全局阴影光源接口不崩溃) {
    DX11RhiDevice device;
    device.SetGlobalShadowMap(0, 1);
    device.SetGlobalSpotShadowMap(0, 2);
    device.SetGlobalPointShadowMap(0, 3);
    device.SetGlobalLightSpaceMatrix(0, glm::mat4(1.0f));
    device.SetGlobalCascadeSplit(0, 0.1f);
    device.SetGlobalCascadeSplit(1, 0.3f);
    device.SetGlobalCascadeSplit(2, 0.9f);
    device.SetGlobalSpotLightSpaceMatrix(0, glm::mat4(1.0f));
}

// ============================================================
// DX11CommandBuffer 无 device 测试
// ============================================================

TEST(DX11CommandBufferTest, 构造不崩溃) {
    DX11CommandBuffer cmd;
}

TEST(DX11CommandBufferTest, 无device时BeginEndRenderPass安全) {
    DX11CommandBuffer cmd;
    RenderPassDesc desc{};
    cmd.BeginRenderPass(desc);
    cmd.EndRenderPass();
}

TEST(DX11CommandBufferTest, 无device时DrawMeshBatch安全) {
    DX11CommandBuffer cmd;
    std::vector<MeshDrawItem> items;
    items.emplace_back();
    cmd.DrawMeshBatch(items);
}

TEST(DX11CommandBufferTest, 无device时DrawSpriteBatch安全) {
    DX11CommandBuffer cmd;
    std::vector<SpriteDrawItem> items;
    items.emplace_back();
    cmd.DrawSpriteBatch(items);
}

TEST(DX11CommandBufferTest, 无device时DrawBatch安全) {
    DX11CommandBuffer cmd;
    std::vector<DrawBatchItem> items;
    cmd.DrawBatch(items);
}

TEST(DX11CommandBufferTest, 无device时DrawSkybox安全) {
    DX11CommandBuffer cmd;
    cmd.DrawSkybox(100);
}

TEST(DX11CommandBufferTest, 无device时DrawPostProcess安全) {
    DX11CommandBuffer cmd;
    cmd.DrawPostProcess(100, "bloom_downsample", {1.0f, 0.5f});
}

TEST(DX11CommandBufferTest, 无device时DrawParticles3D安全) {
    DX11CommandBuffer cmd;
    std::vector<Particle3DDrawItem> items;
    cmd.DrawParticles3D(items, glm::mat4(1.0f), glm::mat4(1.0f));
}

TEST(DX11CommandBufferTest, 无device时ClearColor安全) {
    DX11CommandBuffer cmd;
    cmd.ClearColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

TEST(DX11CommandBufferTest, 无device时SetPipelineState安全) {
    DX11CommandBuffer cmd;
    cmd.SetPipelineState(12345);
}

TEST(DX11CommandBufferTest, 无device时DeferShadowMap安全) {
    DX11CommandBuffer cmd;
    cmd.DeferSetGlobalShadowMap(0, 100);
    cmd.DeferSetGlobalSpotShadowMap(0, 200);
    cmd.DeferSetGlobalPointShadowMap(0, 300);
}

TEST(DX11CommandBufferTest, SetCamera存储矩阵不崩溃) {
    DX11CommandBuffer cmd;
    cmd.SetCamera(glm::mat4(2.0f), glm::mat4(3.0f));
}

TEST(DX11CommandBufferTest, 全局uniform暂存和清除) {
    DX11CommandBuffer cmd;
    cmd.SetGlobalMat4("u_view", glm::mat4(1.0f));
    cmd.SetGlobalMat4Array("u_bones", {glm::mat4(1.0f), glm::mat4(2.0f)});
    cmd.SetGlobalFloatArray("u_weights", {0.5f, 0.3f, 0.2f});

    EXPECT_EQ(cmd.pending_mat4().size(), 1u);
    EXPECT_EQ(cmd.pending_mat4_array().size(), 1u);
    EXPECT_EQ(cmd.pending_float_array().size(), 1u);

    cmd.ClearPendingUniforms();

    EXPECT_TRUE(cmd.pending_mat4().empty());
    EXPECT_TRUE(cmd.pending_mat4_array().empty());
    EXPECT_TRUE(cmd.pending_float_array().empty());
}

TEST(DX11CommandBufferTest, Reset重置uniform状态) {
    DX11CommandBuffer cmd;
    cmd.SetGlobalMat4("test", glm::mat4(1.0f));
    cmd.Reset();
    EXPECT_TRUE(cmd.pending_mat4().empty());
}

// ============================================================
// DX11DrawExecutor 全局状态边界检查
// ============================================================

TEST(DX11DrawExecutorTest, 全局状态接口边界检查) {
    DX11DrawExecutor exec;
    // 有效索引
    exec.SetGlobalShadowMap(0, 100);
    exec.SetGlobalShadowMap(2, 200);
    exec.SetGlobalSpotShadowMap(3, 300);
    exec.SetGlobalPointShadowMap(3, 400);
    exec.SetGlobalLightSpaceMatrix(2, glm::mat4(1.0f));
    exec.SetGlobalCascadeSplit(2, 0.5f);
    exec.SetGlobalSpotLightSpaceMatrix(3, glm::mat4(1.0f));
    // 越界静默忽略
    exec.SetGlobalShadowMap(3, 999);
    exec.SetGlobalSpotShadowMap(4, 999);
    exec.SetGlobalPointShadowMap(4, 999);
    exec.SetGlobalLightSpaceMatrix(3, glm::mat4(1.0f));
    exec.SetGlobalCascadeSplit(3, 1.0f);
    exec.SetGlobalSpotLightSpaceMatrix(4, glm::mat4(1.0f));
}

TEST(DX11DrawExecutorTest, 默认stats为零) {
    DX11DrawExecutor exec;
    const auto& stats = exec.current_frame_stats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

// ============================================================
// DX11ShaderManager 未初始化状态
// ============================================================

TEST(DX11ShaderManagerTest, 未初始化时句柄为零) {
    DX11ShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.particle_shader_handle(), 0u);
    EXPECT_EQ(mgr.sprite_shader_handle(), 0u);
    EXPECT_EQ(mgr.postprocess_shader_handle(), 0u);
    EXPECT_EQ(mgr.shadow_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

TEST(DX11ShaderManagerTest, GetProgram无效句柄返回nullptr) {
    DX11ShaderManager mgr;
    EXPECT_EQ(mgr.GetProgram(0), nullptr);
    EXPECT_EQ(mgr.GetProgram(999), nullptr);
}

TEST(DX11ShaderManagerTest, GetInputLayout无效句柄返回nullptr) {
    DX11ShaderManager mgr;
    EXPECT_EQ(mgr.GetInputLayout(0), nullptr);
    EXPECT_EQ(mgr.GetInputLayout(999), nullptr);
}

// ============================================================
// DX11 数据结构默认值
// ============================================================

TEST(DX11TextureTest, 默认值) {
    DX11Texture tex;
    EXPECT_EQ(tex.texture.Get(), nullptr);
    EXPECT_EQ(tex.srv.Get(), nullptr);
    EXPECT_EQ(tex.width, 0);
    EXPECT_EQ(tex.height, 0);
    EXPECT_FALSE(tex.is_cube);
}

TEST(DX11BufferTest, 默认值) {
    DX11Buffer buf;
    EXPECT_EQ(buf.buffer.Get(), nullptr);
    EXPECT_EQ(buf.size, 0u);
    EXPECT_FALSE(buf.is_dynamic);
    EXPECT_FALSE(buf.is_index);
}

TEST(DX11RenderTargetTest, 默认值) {
    DX11RenderTarget rt;
    EXPECT_EQ(rt.width, 0);
    EXPECT_EQ(rt.height, 0);
    EXPECT_TRUE(rt.has_color);
    EXPECT_FALSE(rt.has_depth);
    EXPECT_EQ(rt.color_texture_handle, 0u);
    EXPECT_EQ(rt.depth_texture_handle, 0u);
}

TEST(DX11ContextTest, 未初始化时device为空) {
    DX11Context ctx;
    EXPECT_EQ(ctx.device(), nullptr);
    EXPECT_EQ(ctx.device_context(), nullptr);
    EXPECT_EQ(ctx.swapchain(), nullptr);
    EXPECT_EQ(ctx.backbuffer_rtv(), nullptr);
    EXPECT_EQ(ctx.backbuffer_dsv(), nullptr);
    EXPECT_FALSE(ctx.initialized());
}

TEST(DX11ContextTest, 未初始化时Shutdown安全) {
    DX11Context ctx;
    ctx.Shutdown();
}

TEST(RenderTargetDescTest, msaa_samples默认值为1) {
    RenderTargetDesc desc{};
    EXPECT_EQ(desc.msaa_samples, 1);
}

TEST(RenderTargetDescTest, allow_uav默认值为false) {
    RenderTargetDesc desc{};
    EXPECT_FALSE(desc.allow_uav);
}

#endif // DSE_ENABLE_D3D11
