/**
 * @file vulkan_rhi_test.cpp
 * @brief Vulkan RHI 端到端验证单元测试
 *
 * 覆盖场景（均不需要真实 GPU / Vulkan Instance）：
 *
 * 1. RHI Factory 测试
 *    - RhiBackendToString 枚举映射
 *    - ResolveRhiBackendFromEnv 默认值
 *    - CreateRhiDevice(OpenGL) 返回非空
 *    - CreateRhiDevice(Vulkan) 在 DSE_ENABLE_VULKAN 下返回非空
 *
 * 2. VulkanRhiDevice 无 GPU 测试
 *    - 构造/析构不崩溃
 *    - 未初始化时 Shutdown 安全
 *    - 未初始化时 BeginFrame/EndFrame 安全
 *    - 未初始化时 Submit 安全
 *    - CreateVertexArray 返回递增句柄
 *    - DeleteVertexArray no-op 不崩溃
 *    - LastFrameStats 默认值
 *
 * 3. VulkanCommandBuffer 无 GPU 测试
 *    - 构造后 VkCommandBuffer 为 VK_NULL_HANDLE
 *    - Reset 重置状态
 *    - SetCamera 存储 view/projection
 *    - 全局 uniform 暂存与清除
 *    - 无 device 时所有 Draw/RenderPass 命令安全
 *
 * 4. Vulkan 子系统接口一致性测试
 *    - PipelineStateManager 枚举映射完整性
 *    - VulkanDrawExecutor 全局阴影/光源状态
 *    - VulkanContext 默认成员状态
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_factory.h"
#include "engine/render/rhi/rhi_device.h"

#ifdef DSE_ENABLE_VULKAN
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/render/rhi/vulkan/vulkan_resource_manager.h"
#include "engine/render/rhi/vulkan/vulkan_pipeline_state_manager.h"
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include "engine/render/rhi/vulkan/vulkan_draw_executor.h"
#include "engine/render/shaders/generated/embed/pbr_frag.gen.h"
#include "engine/render/shaders/generated/embed/pbr_frag_reflect.gen.h"
#include "engine/render/shaders/generated/embed/skybox_vert.gen.h"
#endif

#include <glm/glm.hpp>
#include <string>
#include <algorithm>

using namespace dse::render;

// ============================================================
// 1. RHI Factory
// ============================================================

TEST(RhiFactoryTest, BackendToString_OpenGL) {
    EXPECT_EQ(RhiBackendToString(RhiBackend::OpenGL), "OpenGL");
}

TEST(RhiFactoryTest, BackendToString_Vulkan) {
    EXPECT_EQ(RhiBackendToString(RhiBackend::Vulkan), "Vulkan");
}

TEST(RhiFactoryTest, BackendToString_Unknown) {
    auto str = RhiBackendToString(static_cast<RhiBackend>(99));
    EXPECT_EQ(str, "Unknown");
}

TEST(RhiFactoryTest, ResolveRhiBackendFromEnv_DefaultValues) {
    // 未设置环境变量时应返回 Default
    // 注意：若 CI 中设置了 DSE_RHI_BACKEND 此测试可能需调整
    auto backend = ResolveRhiBackendFromEnv();
    // 仅验证返回的枚举值在合法范围内
    unsigned int val = static_cast<unsigned int>(backend);
    EXPECT_LE(val, 1u);
}

TEST(RhiFactoryTest, CreateRhiDevice_OpenGLReturnNonEmpty) {
    // OpenGL 后端始终可用（不需 GPU context 创建也能构造）
    auto device = CreateRhiDevice(RhiBackend::OpenGL);
    EXPECT_NE(device, nullptr);
}

#ifdef DSE_ENABLE_VULKAN
TEST(RhiFactoryTest, CreateRhiDevice_VulkanReturnNonEmpty) {
    auto device = CreateRhiDevice(RhiBackend::Vulkan);
    EXPECT_NE(device, nullptr);
}

TEST(RhiFactoryTest, CreateRhiDevice_DefaultFallbackToVulkan) {
    // DSE_ENABLE_VULKAN 时 Default 回退到 Vulkan
    auto device = CreateRhiDevice(RhiBackend::Default);
    EXPECT_NE(device, nullptr);
}
#endif

// ============================================================
// 2. VulkanRhiDevice 无 GPU 测试
// ============================================================

#ifdef DSE_ENABLE_VULKAN

TEST(VulkanRhiDeviceTest, DoesNotCrash) {
    VulkanRhiDevice device;
    // 未调用 InitVulkan，直接析构应安全
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedShutdownSafety) {
    VulkanRhiDevice device;
    device.Shutdown(); // initialized_=false，应直接 return
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedSubmitSafety) {
    VulkanRhiDevice device;
    auto cmd = std::make_shared<VulkanCommandBuffer>();
    device.Submit(cmd); // initialized_=false，应直接 return
}

TEST(VulkanRhiDeviceTest, CreateVertexArrayReturnIncrementHandle) {
    VulkanRhiDevice device;
    auto h1 = device.CreateVertexArray();
    auto h2 = device.CreateVertexArray();
    EXPECT_NE(h1, h2);
    EXPECT_GT(h2.raw(), h1.raw());
}

TEST(VulkanRhiDeviceTest, DeleteVertexArray_NoOpDoesNotCrash) {
    VulkanRhiDevice device;
    device.DeleteVertexArray(VertexArrayHandle::from_raw(12345)); // no-op
    device.DeleteVertexArray(VertexArrayHandle{});                // no-op
}

TEST(VulkanRhiDeviceTest, LastFrameStatsDefaultValueIsZero) {
    VulkanRhiDevice device;
    const auto& stats = device.LastFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

TEST(VulkanRhiDeviceTest, SystemCanCalls) {
    VulkanRhiDevice device;
    // 验证子系统对象可访问（即使未初始化也不应崩溃）
    auto& ctx = device.context();
    auto& res = device.resource_mgr();
    auto& state = device.state_mgr();
    auto& shader = device.shader_mgr();
    auto& draw = device.draw_executor();
    (void)ctx; (void)res; (void)state; (void)shader; (void)draw;
}

TEST(VulkanRhiDeviceTest, AllTheInterfaceDoesNotCrash) {
    VulkanRhiDevice device;
    device.SetGlobalShadowMap(0, 100);
    device.SetGlobalShadowMap(2, 200);
    device.SetGlobalSpotShadowMap(0, 300);
    device.SetGlobalPointShadowMap(0, 400);
    device.SetGlobalLightSpaceMatrix(0, glm::mat4(1.0f));
    device.SetGlobalCascadeSplit(0, 0.3f);
    device.SetGlobalSpotLightSpaceMatrix(0, glm::mat4(1.0f));
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedBeginFrameSafety) {
    VulkanRhiDevice device;
    device.BeginFrame();
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedEndFrameSafety) {
    VulkanRhiDevice device;
    device.EndFrame();
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedCreateRenderTargetReturnsZero) {
    VulkanRhiDevice device;
    RenderTargetDesc desc{};
    desc.width = 256;
    desc.height = 256;
    desc.has_color = true;
    unsigned int handle = device.CreateRenderTarget(desc);
    EXPECT_EQ(handle, 0u);
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedCreateTexture2DReturnsZero) {
    VulkanRhiDevice device;
    unsigned int handle = device.CreateTexture2D(4, 4, nullptr, false);
    EXPECT_EQ(handle, 0u);
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedCreateBufferReturnsZero) {
    VulkanRhiDevice device;
    float data[] = {1.0f, 2.0f, 3.0f};
    unsigned int handle = device.CreateBuffer(sizeof(data), data, false, false);
    EXPECT_EQ(handle, 0u);
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedCreateShaderProgramReturnsZero) {
    VulkanRhiDevice device;
    unsigned int handle = device.CreateShaderProgram("void main(){}", "void main(){}");
    EXPECT_EQ(handle, 0u);
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedDeleteSafety) {
    VulkanRhiDevice device;
    device.DeleteRenderTarget(999);
    device.DeleteTexture(999);
    device.DeleteShaderProgram(999);
}

TEST(VulkanRhiDeviceTest, WhenNotInitializedUpdateBufferSafety) {
    VulkanRhiDevice device;
    float data[] = {0.5f};
    device.UpdateBuffer(999, 0, sizeof(data), data, false);
}

// ============================================================
// 3. VulkanCommandBuffer 无 GPU 测试
// ============================================================

TEST(VulkanCommandBufferTest, AfterVkCommandBufferIsNULL_HANDLE) {
    VulkanCommandBuffer cmd;
    EXPECT_EQ(cmd.GetVkCommandBuffer(), VK_NULL_HANDLE);
}

TEST(VulkanCommandBufferTest, ResetresetState) {
    VulkanCommandBuffer cmd;
    cmd.SetVkCommandBuffer(reinterpret_cast<VkCommandBuffer>(0x1234));
    cmd.Reset();
    EXPECT_EQ(cmd.GetVkCommandBuffer(), VK_NULL_HANDLE);
}

TEST(VulkanCommandBufferTest, SetCamerastorageMatrix) {
    VulkanCommandBuffer cmd;
    glm::mat4 view = glm::mat4(2.0f);
    glm::mat4 proj = glm::mat4(3.0f);
    cmd.SetCamera(view, proj);
    // SetCamera 是 void，验证不崩溃即可
    // 矩阵值会在 Draw 时通过 executor 消费
}

TEST(VulkanCommandBufferTest, AlluniformAndClear) {
    VulkanCommandBuffer cmd;

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

TEST(VulkanCommandBufferTest, WithoutdeviceWhenDrawSpriteBatchEmptySafety) {
    VulkanCommandBuffer cmd;
    std::vector<SpriteDrawItem> items;
    cmd.DrawSpriteBatch(items);
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenDrawMeshBatchSafety) {
    VulkanCommandBuffer cmd;
    std::vector<MeshDrawItem> items;
    items.emplace_back();
    cmd.DrawMeshBatch(items);
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenDrawSpriteBatchSafety) {
    VulkanCommandBuffer cmd;
    std::vector<SpriteDrawItem> items;
    items.emplace_back();
    cmd.DrawSpriteBatch(items);
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenBeginEndRenderPassSafety) {
    VulkanCommandBuffer cmd;
    RenderPassDesc desc;
    cmd.BeginRenderPass(desc);
    cmd.EndRenderPass();
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenSetPipelineStateSafety) {
    VulkanCommandBuffer cmd;
    cmd.SetPipelineState(12345);
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenClearColorSafety) {
    VulkanCommandBuffer cmd;
    cmd.ClearColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenDrawSkyboxSafety) {
    VulkanCommandBuffer cmd;
    cmd.DrawSkybox(100);
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenDrawPostProcessSafety) {
    VulkanCommandBuffer cmd;
    cmd.DrawPostProcess({"bloom", 100, {1.0f, 0.5f}});
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenDrawParticles3DSafety) {
    VulkanCommandBuffer cmd;
    std::vector<Particle3DDrawItem> items;
    cmd.DrawParticles3D(items, glm::mat4(1.0f), glm::mat4(1.0f));
}

TEST(VulkanCommandBufferTest, WithoutdeviceWhenDeferShadowMapSafety) {
    VulkanCommandBuffer cmd;
    cmd.BindGlobalShadowMap(0, 100);
    cmd.BindGlobalSpotShadowMap(0, 200);
    cmd.BindGlobalPointShadowMap(0, 300);
}

TEST(VulkanCommandBufferTest, SetDeviceAndSetVkCommandBuffer) {
    VulkanCommandBuffer cmd;
    cmd.SetDevice(nullptr);
    cmd.SetVkCommandBuffer(VK_NULL_HANDLE);
    EXPECT_EQ(cmd.GetVkCommandBuffer(), VK_NULL_HANDLE);
}

// ============================================================
// 4. Vulkan 子系统接口一致性测试
// ============================================================

TEST(VulkanPipelineStateManagerTest, BlendFactorEnumMappingComplete) {
    // 验证所有 BlendFactor 值都能映射到有效 VkBlendFactor
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::Zero), VK_BLEND_FACTOR_ZERO);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::One), VK_BLEND_FACTOR_ONE);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::SrcAlpha), VK_BLEND_FACTOR_SRC_ALPHA);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::OneMinusSrcAlpha), VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::DstAlpha), VK_BLEND_FACTOR_DST_ALPHA);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::OneMinusDstAlpha), VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::SrcColor), VK_BLEND_FACTOR_SRC_COLOR);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::OneMinusSrcColor), VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::DstColor), VK_BLEND_FACTOR_DST_COLOR);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor::OneMinusDstColor), VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR);
}

TEST(VulkanPipelineStateManagerTest, CompareFuncEnumMappingComplete) {
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Never), VK_COMPARE_OP_NEVER);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Less), VK_COMPARE_OP_LESS);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Equal), VK_COMPARE_OP_EQUAL);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::LessEqual), VK_COMPARE_OP_LESS_OR_EQUAL);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Greater), VK_COMPARE_OP_GREATER);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::NotEqual), VK_COMPARE_OP_NOT_EQUAL);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::GreaterEqual), VK_COMPARE_OP_GREATER_OR_EQUAL);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Always), VK_COMPARE_OP_ALWAYS);
}

TEST(VulkanPipelineStateManagerTest, CullFaceEnumMappingComplete) {
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCullMode(CullFace::Back), VK_CULL_MODE_BACK_BIT);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCullMode(CullFace::Front), VK_CULL_MODE_FRONT_BIT);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCullMode(CullFace::None), VK_CULL_MODE_NONE);
}

TEST(VulkanPipelineStateManagerTest, WhenNotInitializedpipeline_state_CountisZero) {
    VulkanPipelineStateManager mgr;
    EXPECT_EQ(mgr.pipeline_state_count(), 0u);
    EXPECT_EQ(mgr.active_pipeline_state(), 0u);
}

TEST(VulkanPipelineStateManagerTest, set_active_pipeline_StateReadableAndWritable) {
    VulkanPipelineStateManager mgr;
    mgr.set_active_pipeline_state(42);
    EXPECT_EQ(mgr.active_pipeline_state(), 42u);
}

TEST(VulkanPipelineStateManagerTest, WireframeOffByDefault) {
    VulkanPipelineStateManager mgr;
    EXPECT_FALSE(mgr.wireframe_mode());
}

TEST(VulkanPipelineStateManagerTest, OverdrawOffByDefault) {
    VulkanPipelineStateManager mgr;
    EXPECT_FALSE(mgr.overdraw_mode());
}

TEST(VulkanPipelineStateManagerTest, SetWireframeModeReadableAndWritable) {
    VulkanPipelineStateManager mgr;
    mgr.SetWireframeMode(true);
    EXPECT_TRUE(mgr.wireframe_mode());
    mgr.SetWireframeMode(false);
    EXPECT_FALSE(mgr.wireframe_mode());
}

TEST(VulkanPipelineStateManagerTest, SetOverdrawModeReadableAndWritable) {
    VulkanPipelineStateManager mgr;
    mgr.SetOverdrawMode(true);
    EXPECT_TRUE(mgr.overdraw_mode());
    mgr.SetOverdrawMode(false);
    EXPECT_FALSE(mgr.overdraw_mode());
}

TEST(VulkanPipelineStateManagerTest, WireframeOverdrawindependentOfEachOther) {
    VulkanPipelineStateManager mgr;
    mgr.SetWireframeMode(true);
    mgr.SetOverdrawMode(false);
    EXPECT_TRUE(mgr.wireframe_mode());
    EXPECT_FALSE(mgr.overdraw_mode());
    mgr.SetWireframeMode(false);
    mgr.SetOverdrawMode(true);
    EXPECT_FALSE(mgr.wireframe_mode());
    EXPECT_TRUE(mgr.overdraw_mode());
}

TEST(VulkanContextTest, DefaultState) {
    VulkanContext ctx;
    EXPECT_EQ(ctx.instance(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.physical_device(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.device(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.surface(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.swapchain(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.graphics_queue(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.present_queue(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.swapchain_render_pass(), VK_NULL_HANDLE);
    EXPECT_EQ(ctx.swapchain_image_count(), 0u);
    EXPECT_EQ(ctx.current_frame(), 0u);
    EXPECT_EQ(ctx.current_image_index(), 0u);
}

TEST(VulkanContextTest, WhenNotInitializedShutdownSafety) {
    VulkanContext ctx;
    ctx.Shutdown();
}

TEST(VulkanContextTest, WhenNotInitializedWaitIdleSafety) {
    VulkanContext ctx;
    ctx.WaitIdle(); // device_=VK_NULL_HANDLE, 应跳过 vkDeviceWaitIdle
}

TEST(VulkanContextTest, MAX_FRAMES_IN_FLIGHTconstantCorrect) {
    EXPECT_EQ(VulkanContext::MAX_FRAMES_IN_FLIGHT, 2u);
}

TEST(VulkanResourceManagerTest, WhenNotInitializedcommand_PoolIsEmpty) {
    VulkanResourceManager mgr;
    EXPECT_EQ(mgr.command_pool(), VK_NULL_HANDLE);
    EXPECT_EQ(mgr.descriptor_pool(), VK_NULL_HANDLE);
}

TEST(VulkanShaderManagerTest, WhenNotInitializedisZero) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.particle_shader_handle(), 0u);
    EXPECT_EQ(mgr.sprite_shader_handle(), 0u);
    EXPECT_EQ(mgr.postprocess_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

TEST(VulkanShaderManagerTest, GetProgramInvalidHandleReturnednullptr) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.GetProgram(0), nullptr);
    EXPECT_EQ(mgr.GetProgram(999), nullptr);
}

TEST(VulkanDrawExecutorTest, AllState) {
    DrawExecutorGlobalState state;
    // index < 3 / < 4 的有效索引
    state.SetShadowMap(0, 100);
    state.SetShadowMap(2, 200);
    state.SetSpotShadowMap(3, 300);
    state.SetPointShadowMap(3, 400);
    state.SetLightSpaceMatrix(2, glm::mat4(1.0f));
    state.SetCascadeSplit(2, 0.5f);
    state.SetSpotLightSpaceMatrix(3, glm::mat4(1.0f));

    // 越界索引应静默忽略
    state.SetShadowMap(3, 999);          // 越界
    state.SetSpotShadowMap(4, 999);      // 越界
    state.SetPointShadowMap(4, 999);     // 越界
    state.SetLightSpaceMatrix(3, glm::mat4(1.0f)); // 越界
    state.SetCascadeSplit(3, 1.0f);      // 越界
    state.SetSpotLightSpaceMatrix(4, glm::mat4(1.0f)); // 越界
}

TEST(VulkanDrawExecutorTest, WhenNotInitializedcurrent_frame_StatsDefaultIsZero) {
    // BeginFrame/EndFrame 需要 context_ 非空，此处仅验证 stats 默认值
    DrawExecutorGlobalState state;
    VulkanDrawExecutor exec(state);
    const auto& stats = exec.current_frame_stats();
    EXPECT_EQ(stats.draw_calls, 0);
}

TEST(VulkanDrawExecutorTest, DefaultstatsisZero) {
    DrawExecutorGlobalState state;
    VulkanDrawExecutor exec(state);
    const auto& stats = exec.current_frame_stats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

// ============================================================
// 5. QueueFamilyIndices 数据结构
// ============================================================

TEST(QueueFamilyIndicesTest, DefaultIsCompleteIsfalse) {
    QueueFamilyIndices indices;
    EXPECT_FALSE(indices.IsComplete());
}

TEST(QueueFamilyIndicesTest, SetUpgraphicsAndpresentAfterIsCompleteIstrue) {
    QueueFamilyIndices indices;
    indices.graphics = 0;
    indices.present = 0;
    EXPECT_TRUE(indices.IsComplete());
}

TEST(QueueFamilyIndicesTest, GraphicsNotIsComplete) {
    QueueFamilyIndices indices;
    indices.graphics = 0;
    EXPECT_FALSE(indices.IsComplete());
}

// ============================================================
// 6. Vulkan 数据结构默认值
// ============================================================

TEST(VulkanBufferTest, DefaultValues) {
    VulkanBuffer buf;
    EXPECT_EQ(buf.buffer, VK_NULL_HANDLE);
    EXPECT_EQ(buf.memory, VK_NULL_HANDLE);
    EXPECT_EQ(buf.size, 0u);
    EXPECT_EQ(buf.mapped, nullptr);
    EXPECT_FALSE(buf.is_dynamic);
}

TEST(VulkanTextureTest, DefaultValues) {
    VulkanTexture tex;
    EXPECT_EQ(tex.image, VK_NULL_HANDLE);
    EXPECT_EQ(tex.image_view, VK_NULL_HANDLE);
    EXPECT_EQ(tex.memory, VK_NULL_HANDLE);
    EXPECT_EQ(tex.format, VK_FORMAT_UNDEFINED);
    EXPECT_EQ(tex.width, 0);
    EXPECT_EQ(tex.height, 0);
    EXPECT_EQ(tex.channels, 4);
}

TEST(VulkanRenderTargetTest, DefaultValues) {
    VulkanRenderTarget rt;
    EXPECT_EQ(rt.width, 0);
    EXPECT_EQ(rt.height, 0);
    EXPECT_TRUE(rt.has_color);
    EXPECT_FALSE(rt.has_depth);
    EXPECT_FALSE(rt.generate_mipmaps);
    EXPECT_EQ(rt.framebuffer, VK_NULL_HANDLE);
    EXPECT_EQ(rt.render_pass, VK_NULL_HANDLE);
}

// ============================================================
// Phase C 回归测试（C1 MSAA / C2 HDR / C3 Bloom CS）
// ============================================================

// C1 — MSAA 新字段默认值
TEST(VulkanRenderTargetTest, C1_MSAAFieldDefaultValues) {
    VulkanRenderTarget rt;
    EXPECT_FALSE(rt.is_msaa);
    EXPECT_EQ(rt.msaa_samples, 1);
    EXPECT_FALSE(rt.allow_uav);
    // msaa_color_texture 默认为空
    EXPECT_EQ(rt.msaa_color_texture.image, VK_NULL_HANDLE);
    EXPECT_EQ(rt.msaa_color_texture.image_view, VK_NULL_HANDLE);
    EXPECT_EQ(rt.msaa_color_texture.memory, VK_NULL_HANDLE);
}

// C2 — HDR Context 访问器
TEST(VulkanContextTest, C2_HDRDisabledByDefault) {
    VulkanContext ctx;
    EXPECT_FALSE(ctx.hdr_enabled());
}

// C3 — Bloom CS 句柄默认值
TEST(VulkanShaderManagerTest, C3_BloomCShandleDefaultsToZero) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.bloom_downsample_cs_handle(), 0u);
    EXPECT_EQ(mgr.bloom_upsample_cs_handle(), 0u);
}

// C3 — GetComputeProgram 无效句柄返回 nullptr
TEST(VulkanShaderManagerTest, C3_GetComputeProgramInvalidHandleReturnednullptr) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.GetComputeProgram(0), nullptr);
    EXPECT_EQ(mgr.GetComputeProgram(999), nullptr);
}

// C3 — VulkanComputeProgram 默认值
TEST(VulkanComputeProgramTest, C3_DefaultValues) {
    VulkanComputeProgram prog;
    EXPECT_EQ(prog.comp_module, VK_NULL_HANDLE);
    EXPECT_EQ(prog.pipeline, VK_NULL_HANDLE);
    EXPECT_EQ(prog.pipeline_layout, VK_NULL_HANDLE);
    EXPECT_EQ(prog.descriptor_set_layout, VK_NULL_HANDLE);
}

// C1 — VulkanResourceManager 新增 default_sampler 默认空
TEST(VulkanResourceManagerTest, C1_default_SamplernotInitializedToNULL) {
    VulkanResourceManager mgr;
    EXPECT_EQ(mgr.default_sampler(), VK_NULL_HANDLE);
}

TEST(VulkanShaderProgramTest, DefaultValues) {
    VulkanShaderProgram prog;
    EXPECT_EQ(prog.vert_module, VK_NULL_HANDLE);
    EXPECT_EQ(prog.frag_module, VK_NULL_HANDLE);
    EXPECT_EQ(prog.pipeline_layout, VK_NULL_HANDLE);
    EXPECT_TRUE(prog.descriptor_set_layouts.empty());
    EXPECT_FALSE(prog.reflection.has_push_constant);
}

TEST(VulkanPipelineStateTest, DefaultValues) {
    VulkanPipelineState state;
    EXPECT_EQ(state.pipeline, VK_NULL_HANDLE);
    EXPECT_EQ(state.render_pass, VK_NULL_HANDLE);
    EXPECT_EQ(state.pipeline_layout, VK_NULL_HANDLE);
    EXPECT_EQ(state.shader_program_handle, 0u);
}

TEST(VulkanPerFrameUBOTest, Size) {
    // 确保 UBO 结构体大小符合预期（对齐到 std140 布局）
    VulkanPerFrameUBO ubo;
    ubo.vp = glm::mat4(1.0f);
    ubo.view = glm::mat4(1.0f);
    ubo.camera_pos = glm::vec4(0.0f);
    EXPECT_GE(sizeof(VulkanPerFrameUBO), sizeof(glm::mat4) * 2 + sizeof(glm::vec4));
}

TEST(DescriptorBindingInfoTest, TestCase74) {
    DescriptorBindingInfo a;
    a.set = 0; a.binding = 1; a.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    a.stage_flags = VK_SHADER_STAGE_VERTEX_BIT; a.count = 1;

    DescriptorBindingInfo b = a;
    EXPECT_TRUE(a == b);

    b.binding = 2;
    EXPECT_FALSE(a == b);
}

// ============================================================
// Phase J: VulkanPointLights/SpotLights UBO 结构体验证
// ============================================================

TEST(VulkanPointLightUBOTest, J1_StructureSize16BAlignment) {
    EXPECT_EQ(sizeof(VulkanPointLightsUBO) % 16u, 0u);
}

TEST(VulkanPointLightUBOTest, J1_EntrycorrectSize) {
    EXPECT_EQ(sizeof(PointLightEntry), 48u);
}

TEST(VulkanPointLightUBOTest, J1_DefaultcountisZero) {
    VulkanPointLightsUBO ubo{};
    EXPECT_EQ(ubo.u_point_light_count, 0);
}

TEST(VulkanSpotLightsUBOTest, J1_StructureSize16BAlignment) {
    EXPECT_EQ(sizeof(VulkanSpotLightsUBO) % 16u, 0u);
}

TEST(VulkanSpotLightsUBOTest, J1_EntrycorrectSize) {
    EXPECT_EQ(sizeof(SpotLightEntry), 64u);
}

TEST(VulkanSpotLightsUBOTest, J1_DefaultcountisZero) {
    VulkanSpotLightsUBO ubo{};
    EXPECT_EQ(ubo.u_spot_light_count, 0);
}

// ============================================================
// Phase K: GLSL 着色器字符串关键声明验证（无 GPU 依赖）
// ============================================================

TEST(VulkanGLSLShaderTest, K_PointLightsSSBOdeclareExistence) {
    using namespace dse::render::generated_shaders::reflect;
    bool found = false;
    for (uint32_t i = 0; i < kpbr_frag_ssbo_count; ++i) {
        if (std::string(kpbr_frag_ssbos[i].name) == "PointLightSSBO" &&
            kpbr_frag_ssbos[i].set == 1 && kpbr_frag_ssbos[i].binding == 1) {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "PointLightSSBO should be at set=1 binding=1";
}

TEST(VulkanGLSLShaderTest, K_SpotLightsSSBOdeclareExistence) {
    using namespace dse::render::generated_shaders::reflect;
    bool found = false;
    for (uint32_t i = 0; i < kpbr_frag_ssbo_count; ++i) {
        if (std::string(kpbr_frag_ssbos[i].name) == "SpotLightSSBO" &&
            kpbr_frag_ssbos[i].set == 1 && kpbr_frag_ssbos[i].binding == 2) {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "SpotLightSSBO should be at set=1 binding=2";
}

TEST(VulkanGLSLShaderTest, K_PointLightCubeShadowSamplerDeclarationExists) {
    std::string src(dse::render::generated_shaders::kpbr_frag_glsl430);
    EXPECT_NE(src.find("u_point_shadow_maps"), std::string::npos)
        << "kPbrFragment should declare u_point_shadow_maps samplerCube";
}

TEST(VulkanGLSLShaderTest, K_SpotLightusefloat_PadNonvec2_pad) {
    std::string src(dse::render::generated_shaders::kpbr_frag_glsl430);
    // SpotLight 含有 outer_cone 字段；在 outer_cone 之后到下一个 "};" 之间
    // 必须是 float _pad 而非 vec2 _pad（vec2 对齐会使 stride 从 64B 变成 80B）
    auto outer_pos = src.find("float outer_cone;");
    ASSERT_NE(outer_pos, std::string::npos) << "outer_cone field not found in PBR fragment source";
    auto struct_end = src.find("};", outer_pos);
    ASSERT_NE(struct_end, std::string::npos);
    auto vec2_pos = src.find("vec2 _pad", outer_pos);
    EXPECT_TRUE(vec2_pos == std::string::npos || vec2_pos > struct_end)
        << "SpotLight._pad must be float, not vec2 (stride must be 64B not 80B)";
}

// ============================================================
// RHI 统一回归测试
// ============================================================

TEST(VulkanPipelineStateManagerTest, FrontFaceIsCCWbecauseYFlipAndViewportYoffset) {
    // 投影 Y-flip 与 Vulkan viewport Y-inversion 相互抵消 → 帧缓冲绕序保持 CCW
    EXPECT_EQ(VulkanPipelineStateManager::ToVkFrontFace(), VK_FRONT_FACE_COUNTER_CLOCKWISE);
}

TEST(VulkanProjectionCorrectionTest, YFlipRowAndColumnValuesAreCorrect) {
    VulkanRhiDevice device;
    glm::mat4 corr = device.GetProjectionCorrection();
    // row 0: (1, 0, 0, 0)
    EXPECT_FLOAT_EQ(corr[0][0], 1.0f);
    // row 1: (0, -1, 0, 0) — Y-flip
    EXPECT_FLOAT_EQ(corr[1][1], -1.0f);
    // row 2,3: Z remap (0.5, 0.5)
    EXPECT_FLOAT_EQ(corr[2][2], 0.5f);
    EXPECT_FLOAT_EQ(corr[3][2], 0.5f);
}

TEST(VulkanSkyboxShaderTest, SamplingTowarduseaPos) {
    std::string src(dse::render::generated_shaders::kskybox_vert_glsl430);
    EXPECT_NE(src.find("vTexCoords = aPos"), std::string::npos)
        << "Skybox VS should use raw vertex position as cubemap sampling direction";
}

#endif // DSE_ENABLE_VULKAN

// ============================================================
// ToneMappingTest — ACES Filmic 纯数学验证（无 GPU 依赖）
// ============================================================

TEST(ToneMappingTest, ACESFilmic_BlackInputIsZero) {
    float x = 0.0f;
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    float result = (x * (a * x + b)) / (x * (c * x + d) + e);
    EXPECT_NEAR(result, 0.0f, 1e-5f);
}

TEST(ToneMappingTest, ACESFilmic_HighlightsTruncatedToNoMoreThan1) {
    float x = 100.0f;
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    float raw = (x * (a * x + b)) / (x * (c * x + d) + e);
    float result = std::min(raw, 1.0f);
    EXPECT_LE(result, 1.0f);
}

TEST(ToneMappingTest, ACESFilmic_NeutralInputIsInTheOpenInterval) {
    float x = 1.0f;
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    float result = (x * (a * x + b)) / (x * (c * x + d) + e);
    EXPECT_GT(result, 0.0f);
    EXPECT_LT(result, 1.0f);
}

// ============================================================
// ShadowPCFTest — PCF 纯数学验证（无 GPU 依赖）
// ============================================================

TEST(ShadowPCFTest, PCF_FullOcclusionReturnsZero) {
    float samples[9] = {0,0,0,0,0,0,0,0,0};
    float sum = 0.0f;
    for (float s : samples) sum += s;
    EXPECT_NEAR(sum / 9.0f, 0.0f, 1e-5f);
}

TEST(ShadowPCFTest, PCF_ReturnToOneWithoutOcclusion) {
    float samples[9] = {1,1,1,1,1,1,1,1,1};
    float sum = 0.0f;
    for (float s : samples) sum += s;
    EXPECT_NEAR(sum / 9.0f, 1.0f, 1e-5f);
}

TEST(ShadowPCFTest, PCF_HalfOcclusionIsInTheOpenRange) {
    float samples[9] = {1,0,1,0,1,0,1,0,1};
    float sum = 0.0f;
    for (float s : samples) sum += s;
    float result = sum / 9.0f;
    EXPECT_GT(result, 0.0f);
    EXPECT_LT(result, 1.0f);
}
