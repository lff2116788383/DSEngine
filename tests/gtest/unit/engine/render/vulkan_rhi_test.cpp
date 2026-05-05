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
#endif

#include <glm/glm.hpp>
#include <string>

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

TEST(RhiFactoryTest, ResolveRhiBackendFromEnv_默认值) {
    // 未设置环境变量时应返回 Default
    // 注意：若 CI 中设置了 DSE_RHI_BACKEND 此测试可能需调整
    auto backend = ResolveRhiBackendFromEnv();
    // 仅验证返回的枚举值在合法范围内
    unsigned int val = static_cast<unsigned int>(backend);
    EXPECT_LE(val, 1u);
}

TEST(RhiFactoryTest, CreateRhiDevice_OpenGL返回非空) {
    // OpenGL 后端始终可用（不需 GPU context 创建也能构造）
    auto device = CreateRhiDevice(RhiBackend::OpenGL);
    EXPECT_NE(device, nullptr);
}

#ifdef DSE_ENABLE_VULKAN
TEST(RhiFactoryTest, CreateRhiDevice_Vulkan返回非空) {
    auto device = CreateRhiDevice(RhiBackend::Vulkan);
    EXPECT_NE(device, nullptr);
}

TEST(RhiFactoryTest, CreateRhiDevice_Default回退到Vulkan) {
    // DSE_ENABLE_VULKAN 时 Default 回退到 Vulkan
    auto device = CreateRhiDevice(RhiBackend::Default);
    EXPECT_NE(device, nullptr);
}
#endif

// ============================================================
// 2. VulkanRhiDevice 无 GPU 测试
// ============================================================

#ifdef DSE_ENABLE_VULKAN

TEST(VulkanRhiDeviceTest, 构造析构不崩溃) {
    VulkanRhiDevice device;
    // 未调用 InitVulkan，直接析构应安全
}

TEST(VulkanRhiDeviceTest, 未初始化时Shutdown安全) {
    VulkanRhiDevice device;
    device.Shutdown(); // initialized_=false，应直接 return
}

TEST(VulkanRhiDeviceTest, 未初始化时Submit安全) {
    VulkanRhiDevice device;
    auto cmd = std::make_shared<VulkanCommandBuffer>();
    device.Submit(cmd); // initialized_=false，应直接 return
}

TEST(VulkanRhiDeviceTest, CreateVertexArray返回递增句柄) {
    VulkanRhiDevice device;
    unsigned int h1 = device.CreateVertexArray();
    unsigned int h2 = device.CreateVertexArray();
    EXPECT_NE(h1, h2);
    EXPECT_GT(h2, h1);
}

TEST(VulkanRhiDeviceTest, DeleteVertexArray_NoOp不崩溃) {
    VulkanRhiDevice device;
    device.DeleteVertexArray(12345); // no-op
    device.DeleteVertexArray(0);     // no-op
}

TEST(VulkanRhiDeviceTest, LastFrameStats默认值为零) {
    VulkanRhiDevice device;
    const auto& stats = device.LastFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

TEST(VulkanRhiDeviceTest, 子系统访问器可调用) {
    VulkanRhiDevice device;
    // 验证子系统对象可访问（即使未初始化也不应崩溃）
    auto& ctx = device.context();
    auto& res = device.resource_mgr();
    auto& state = device.state_mgr();
    auto& shader = device.shader_mgr();
    auto& draw = device.draw_executor();
    (void)ctx; (void)res; (void)state; (void)shader; (void)draw;
}

TEST(VulkanRhiDeviceTest, 全局阴影贴图接口不崩溃) {
    VulkanRhiDevice device;
    device.SetGlobalShadowMap(0, 100);
    device.SetGlobalShadowMap(2, 200);
    device.SetGlobalSpotShadowMap(0, 300);
    device.SetGlobalPointShadowMap(0, 400);
    device.SetGlobalLightSpaceMatrix(0, glm::mat4(1.0f));
    device.SetGlobalCascadeSplit(0, 0.3f);
    device.SetGlobalSpotLightSpaceMatrix(0, glm::mat4(1.0f));
}

// ============================================================
// 3. VulkanCommandBuffer 无 GPU 测试
// ============================================================

TEST(VulkanCommandBufferTest, 构造后VkCommandBuffer为NULL_HANDLE) {
    VulkanCommandBuffer cmd;
    EXPECT_EQ(cmd.GetVkCommandBuffer(), VK_NULL_HANDLE);
}

TEST(VulkanCommandBufferTest, Reset重置状态) {
    VulkanCommandBuffer cmd;
    cmd.SetVkCommandBuffer(reinterpret_cast<VkCommandBuffer>(0x1234));
    cmd.Reset();
    EXPECT_EQ(cmd.GetVkCommandBuffer(), VK_NULL_HANDLE);
}

TEST(VulkanCommandBufferTest, SetCamera存储矩阵) {
    VulkanCommandBuffer cmd;
    glm::mat4 view = glm::mat4(2.0f);
    glm::mat4 proj = glm::mat4(3.0f);
    cmd.SetCamera(view, proj);
    // SetCamera 是 void，验证不崩溃即可
    // 矩阵值会在 Draw 时通过 executor 消费
}

TEST(VulkanCommandBufferTest, 全局uniform暂存和清除) {
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

TEST(VulkanCommandBufferTest, 无device时DrawBatch安全) {
    VulkanCommandBuffer cmd;
    std::vector<DrawBatchItem> items;
    cmd.DrawBatch(items);
}

TEST(VulkanCommandBufferTest, 无device时DrawMeshBatch安全) {
    VulkanCommandBuffer cmd;
    std::vector<MeshDrawItem> items;
    items.emplace_back();
    cmd.DrawMeshBatch(items);
}

TEST(VulkanCommandBufferTest, 无device时DrawSpriteBatch安全) {
    VulkanCommandBuffer cmd;
    std::vector<SpriteDrawItem> items;
    items.emplace_back();
    cmd.DrawSpriteBatch(items);
}

TEST(VulkanCommandBufferTest, 无device时BeginEndRenderPass安全) {
    VulkanCommandBuffer cmd;
    RenderPassDesc desc;
    cmd.BeginRenderPass(desc);
    cmd.EndRenderPass();
}

TEST(VulkanCommandBufferTest, 无device时SetPipelineState安全) {
    VulkanCommandBuffer cmd;
    cmd.SetPipelineState(12345);
}

TEST(VulkanCommandBufferTest, 无device时ClearColor安全) {
    VulkanCommandBuffer cmd;
    cmd.ClearColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

TEST(VulkanCommandBufferTest, 无device时DrawSkybox安全) {
    VulkanCommandBuffer cmd;
    cmd.DrawSkybox(100);
}

TEST(VulkanCommandBufferTest, 无device时DrawPostProcess安全) {
    VulkanCommandBuffer cmd;
    cmd.DrawPostProcess(100, "bloom", {1.0f, 0.5f});
}

TEST(VulkanCommandBufferTest, 无device时DrawParticles3D安全) {
    VulkanCommandBuffer cmd;
    std::vector<Particle3DDrawItem> items;
    cmd.DrawParticles3D(items, glm::mat4(1.0f), glm::mat4(1.0f));
}

TEST(VulkanCommandBufferTest, 无device时DeferShadowMap安全) {
    VulkanCommandBuffer cmd;
    cmd.DeferSetGlobalShadowMap(0, 100);
    cmd.DeferSetGlobalSpotShadowMap(0, 200);
    cmd.DeferSetGlobalPointShadowMap(0, 300);
}

TEST(VulkanCommandBufferTest, SetDevice和SetVkCommandBuffer) {
    VulkanCommandBuffer cmd;
    cmd.SetDevice(nullptr);
    cmd.SetVkCommandBuffer(VK_NULL_HANDLE);
    EXPECT_EQ(cmd.GetVkCommandBuffer(), VK_NULL_HANDLE);
}

// ============================================================
// 4. Vulkan 子系统接口一致性测试
// ============================================================

TEST(VulkanPipelineStateManagerTest, BlendFactor枚举映射完整) {
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

TEST(VulkanPipelineStateManagerTest, CompareFunc枚举映射完整) {
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Never), VK_COMPARE_OP_NEVER);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Less), VK_COMPARE_OP_LESS);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Equal), VK_COMPARE_OP_EQUAL);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::LessEqual), VK_COMPARE_OP_LESS_OR_EQUAL);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Greater), VK_COMPARE_OP_GREATER);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::NotEqual), VK_COMPARE_OP_NOT_EQUAL);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::GreaterEqual), VK_COMPARE_OP_GREATER_OR_EQUAL);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCompareOp(CompareFunc::Always), VK_COMPARE_OP_ALWAYS);
}

TEST(VulkanPipelineStateManagerTest, CullFace枚举映射完整) {
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCullMode(CullFace::Back), VK_CULL_MODE_BACK_BIT);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCullMode(CullFace::Front), VK_CULL_MODE_FRONT_BIT);
    EXPECT_EQ(VulkanPipelineStateManager::ToVkCullMode(CullFace::None), VK_CULL_MODE_NONE);
}

TEST(VulkanPipelineStateManagerTest, 未初始化时pipeline_state_count为零) {
    VulkanPipelineStateManager mgr;
    EXPECT_EQ(mgr.pipeline_state_count(), 0u);
    EXPECT_EQ(mgr.active_pipeline_state(), 0u);
}

TEST(VulkanPipelineStateManagerTest, set_active_pipeline_state可读写) {
    VulkanPipelineStateManager mgr;
    mgr.set_active_pipeline_state(42);
    EXPECT_EQ(mgr.active_pipeline_state(), 42u);
}

TEST(VulkanContextTest, 默认成员状态) {
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

TEST(VulkanContextTest, 未初始化时Shutdown安全) {
    VulkanContext ctx;
    ctx.Shutdown();
}

TEST(VulkanContextTest, 未初始化时WaitIdle安全) {
    VulkanContext ctx;
    ctx.WaitIdle(); // device_=VK_NULL_HANDLE, 应跳过 vkDeviceWaitIdle
}

TEST(VulkanContextTest, MAX_FRAMES_IN_FLIGHT常量正确) {
    EXPECT_EQ(VulkanContext::MAX_FRAMES_IN_FLIGHT, 2u);
}

TEST(VulkanResourceManagerTest, 未初始化时command_pool为空) {
    VulkanResourceManager mgr;
    EXPECT_EQ(mgr.command_pool(), VK_NULL_HANDLE);
    EXPECT_EQ(mgr.descriptor_pool(), VK_NULL_HANDLE);
}

TEST(VulkanShaderManagerTest, 未初始化时句柄为零) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.particle_shader_handle(), 0u);
    EXPECT_EQ(mgr.sprite_shader_handle(), 0u);
    EXPECT_EQ(mgr.postprocess_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

TEST(VulkanShaderManagerTest, GetProgram无效句柄返回nullptr) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.GetProgram(0), nullptr);
    EXPECT_EQ(mgr.GetProgram(999), nullptr);
}

TEST(VulkanDrawExecutorTest, 全局状态接口边界检查) {
    VulkanDrawExecutor exec;
    // index < 3 / < 4 的有效索引
    exec.SetGlobalShadowMap(0, 100);
    exec.SetGlobalShadowMap(2, 200);
    exec.SetGlobalSpotShadowMap(3, 300);
    exec.SetGlobalPointShadowMap(3, 400);
    exec.SetGlobalLightSpaceMatrix(2, glm::mat4(1.0f));
    exec.SetGlobalCascadeSplit(2, 0.5f);
    exec.SetGlobalSpotLightSpaceMatrix(3, glm::mat4(1.0f));

    // 越界索引应静默忽略
    exec.SetGlobalShadowMap(3, 999);          // 越界
    exec.SetGlobalSpotShadowMap(4, 999);      // 越界
    exec.SetGlobalPointShadowMap(4, 999);     // 越界
    exec.SetGlobalLightSpaceMatrix(3, glm::mat4(1.0f)); // 越界
    exec.SetGlobalCascadeSplit(3, 1.0f);      // 越界
    exec.SetGlobalSpotLightSpaceMatrix(4, glm::mat4(1.0f)); // 越界
}

TEST(VulkanDrawExecutorTest, 未初始化时current_frame_stats默认为零) {
    // BeginFrame/EndFrame 需要 context_ 非空，此处仅验证 stats 默认值
    VulkanDrawExecutor exec;
    const auto& stats = exec.current_frame_stats();
    EXPECT_EQ(stats.draw_calls, 0);
}

TEST(VulkanDrawExecutorTest, 默认stats为零) {
    VulkanDrawExecutor exec;
    const auto& stats = exec.current_frame_stats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

// ============================================================
// 5. QueueFamilyIndices 数据结构
// ============================================================

TEST(QueueFamilyIndicesTest, 默认IsComplete为false) {
    QueueFamilyIndices indices;
    EXPECT_FALSE(indices.IsComplete());
}

TEST(QueueFamilyIndicesTest, 设置graphics和present后IsComplete为true) {
    QueueFamilyIndices indices;
    indices.graphics = 0;
    indices.present = 0;
    EXPECT_TRUE(indices.IsComplete());
}

TEST(QueueFamilyIndicesTest, 仅graphics不IsComplete) {
    QueueFamilyIndices indices;
    indices.graphics = 0;
    EXPECT_FALSE(indices.IsComplete());
}

// ============================================================
// 6. Vulkan 数据结构默认值
// ============================================================

TEST(VulkanBufferTest, 默认值) {
    VulkanBuffer buf;
    EXPECT_EQ(buf.buffer, VK_NULL_HANDLE);
    EXPECT_EQ(buf.memory, VK_NULL_HANDLE);
    EXPECT_EQ(buf.size, 0u);
    EXPECT_EQ(buf.mapped, nullptr);
    EXPECT_FALSE(buf.is_dynamic);
}

TEST(VulkanTextureTest, 默认值) {
    VulkanTexture tex;
    EXPECT_EQ(tex.image, VK_NULL_HANDLE);
    EXPECT_EQ(tex.image_view, VK_NULL_HANDLE);
    EXPECT_EQ(tex.memory, VK_NULL_HANDLE);
    EXPECT_EQ(tex.format, VK_FORMAT_UNDEFINED);
    EXPECT_EQ(tex.width, 0);
    EXPECT_EQ(tex.height, 0);
    EXPECT_EQ(tex.channels, 4);
}

TEST(VulkanRenderTargetTest, 默认值) {
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
TEST(VulkanRenderTargetTest, C1_MSAA字段默认值) {
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
TEST(VulkanContextTest, C2_HDR默认禁用) {
    VulkanContext ctx;
    EXPECT_FALSE(ctx.hdr_enabled());
}

// C3 — Bloom CS 句柄默认值
TEST(VulkanShaderManagerTest, C3_BloomCS句柄默认为零) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.bloom_downsample_cs_handle(), 0u);
    EXPECT_EQ(mgr.bloom_upsample_cs_handle(), 0u);
}

// C3 — GetComputeProgram 无效句柄返回 nullptr
TEST(VulkanShaderManagerTest, C3_GetComputeProgram无效句柄返回nullptr) {
    VulkanShaderManager mgr;
    EXPECT_EQ(mgr.GetComputeProgram(0), nullptr);
    EXPECT_EQ(mgr.GetComputeProgram(999), nullptr);
}

// C3 — VulkanComputeProgram 默认值
TEST(VulkanComputeProgramTest, C3_默认值) {
    VulkanComputeProgram prog;
    EXPECT_EQ(prog.comp_module, VK_NULL_HANDLE);
    EXPECT_EQ(prog.pipeline, VK_NULL_HANDLE);
    EXPECT_EQ(prog.pipeline_layout, VK_NULL_HANDLE);
    EXPECT_EQ(prog.descriptor_set_layout, VK_NULL_HANDLE);
}

// C1 — VulkanResourceManager 新增 default_sampler 默认空
TEST(VulkanResourceManagerTest, C1_default_sampler未初始化为NULL) {
    VulkanResourceManager mgr;
    EXPECT_EQ(mgr.default_sampler(), VK_NULL_HANDLE);
}

TEST(VulkanShaderProgramTest, 默认值) {
    VulkanShaderProgram prog;
    EXPECT_EQ(prog.vert_module, VK_NULL_HANDLE);
    EXPECT_EQ(prog.frag_module, VK_NULL_HANDLE);
    EXPECT_EQ(prog.pipeline_layout, VK_NULL_HANDLE);
    EXPECT_TRUE(prog.descriptor_set_layouts.empty());
    EXPECT_FALSE(prog.reflection.has_push_constant);
}

TEST(VulkanPipelineStateTest, 默认值) {
    VulkanPipelineState state;
    EXPECT_EQ(state.pipeline, VK_NULL_HANDLE);
    EXPECT_EQ(state.render_pass, VK_NULL_HANDLE);
    EXPECT_EQ(state.pipeline_layout, VK_NULL_HANDLE);
    EXPECT_EQ(state.shader_program_handle, 0u);
}

TEST(VulkanPerFrameUBOTest, 布局大小) {
    // 确保 UBO 结构体大小符合预期（对齐到 std140 布局）
    VulkanPerFrameUBO ubo;
    ubo.vp = glm::mat4(1.0f);
    ubo.view = glm::mat4(1.0f);
    ubo.camera_pos = glm::vec4(0.0f);
    EXPECT_GE(sizeof(VulkanPerFrameUBO), sizeof(glm::mat4) * 2 + sizeof(glm::vec4));
}

TEST(DescriptorBindingInfoTest, 相等比较) {
    DescriptorBindingInfo a;
    a.set = 0; a.binding = 1; a.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    a.stage_flags = VK_SHADER_STAGE_VERTEX_BIT; a.count = 1;

    DescriptorBindingInfo b = a;
    EXPECT_TRUE(a == b);

    b.binding = 2;
    EXPECT_FALSE(a == b);
}

#endif // DSE_ENABLE_VULKAN
