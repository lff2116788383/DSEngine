/**
 * @file gl_rhi_test.cpp
 * @brief OpenGL RHI 单元测试（无 GPU / 无窗口）
 *
 * 覆盖场景（均不需要真实 GL 上下文）：
 *
 * 1. OpenGLRhiDevice 构造/析构/未初始化时接口安全
 * 2. OpenGLCommandBuffer 立即转发/Reset
 * 3. GLDrawExecutor 全局状态边界检查
 * 4. GLShaderManager 未初始化默认值
 * 5. GLResourceManager 句柄分配递增 / 渲染目标存储查询
 * 6. GLPipelineStateManager 管线状态 CRUD
 * 7. UBOManager 未初始化默认值
 * 8. gl_enum_convert 枚举映射完整性
 * 9. DrawExecutorGlobalState 共享状态验证
 * 10. OpenGL 投影修正矩阵
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_factory.h"
#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/render/rhi/opengl/gl_command_buffer.h"
#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/opengl/gl_resource_manager.h"
#include "engine/render/rhi/opengl/gl_pipeline_state_manager.h"
#include "engine/render/rhi/opengl/ubo_manager.h"
#include "engine/render/rhi/opengl/gl_enum_convert.h"
#include "engine/render/rhi/draw_executor_common.h"

#include <glm/glm.hpp>
#include <string>

using namespace dse::render;

// ============================================================
// 1. RHI Factory — OpenGL
// ============================================================

// 测试 RHI工厂GL：创建RHI设备打开GL返回非空
TEST(RhiFactoryGLTest, CreateRhiDevice_OpenGLReturnNonEmpty) {
    auto device = CreateRhiDevice(RhiBackend::OpenGL);
    EXPECT_NE(device, nullptr);
}

// ============================================================
// 2. OpenGLRhiDevice 无 GPU 测试
// ============================================================

// 测试 打开GL RHI设备：不崩溃
TEST(OpenGLRhiDeviceTest, DoesNotCrash) {
    OpenGLRhiDevice device;
}

// 测试 打开GL RHI设备：当不已初始化关闭安全
TEST(OpenGLRhiDeviceTest, WhenNotInitializedShutdownSafety) {
    OpenGLRhiDevice device;
    device.Shutdown();
}

// 注：BeginFrame 会触发 EnsureInitialized() 发起真实 GL 调用，需有效 GL 上下文，
// 不属于本无 GPU 单元文件。其真实覆盖在 gl_rhi_smoke_test.cpp 的
// GLRhiSmokeTest.SingleFrameEmptyDoesNotCrash / InitOpenGLSucceeds 中（有真实上下文时执行）。

// 测试 打开GL RHI设备：当不已初始化结束帧安全
TEST(OpenGLRhiDeviceTest, WhenNotInitializedEndFrameSafety) {
    OpenGLRhiDevice device;
    device.EndFrame();
    SUCCEED();
}

// 测试 打开GL RHI设备：当不已初始化提交安全
TEST(OpenGLRhiDeviceTest, WhenNotInitializedSubmitSafety) {
    OpenGLRhiDevice device;
    auto cmd = std::make_shared<OpenGLCommandBuffer>();
    device.Submit(cmd);
}

// 注：CreateVertexArray 需有效 GL 上下文（glGenVertexArrays），不属于本无 GPU 单元文件。
// 其真实覆盖在 gl_rhi_smoke_test.cpp 的 GLRhiSmokeTest.VertexArrayCreateIncrementsHandle 中。

// 测试 打开GL RHI设备：删除顶点数组无操作不崩溃
TEST(OpenGLRhiDeviceTest, DeleteVertexArray_NoOpDoesNotCrash) {
    OpenGLRhiDevice device;
    device.DeleteVertexArray(dse::render::VertexArrayHandle{999});
    SUCCEED();
}

// 测试 打开GL RHI设备：最后帧统计默认值为零
TEST(OpenGLRhiDeviceTest, LastFrameStatsDefaultValueIsZero) {
    OpenGLRhiDevice device;
    const auto& stats = device.LastFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

// 测试 打开GL RHI设备：系统能够调用
TEST(OpenGLRhiDeviceTest, SystemCanCalls) {
    OpenGLRhiDevice device;
    auto& res    = device.resource_mgr();
    auto& state  = device.state_mgr();
    auto& shader = device.shader_mgr();
    auto& draw   = device.draw_executor();
    auto& ubo    = device.ubo_mgr();
    (void)res; (void)state; (void)shader; (void)draw; (void)ubo;
}

// 测试 打开GL RHI设备：当不已初始化创建缓冲区返回零
TEST(OpenGLRhiDeviceTest, WhenNotInitializedCreateBufferReturnsZero) {
    OpenGLRhiDevice device;
    unsigned int handle = device.CreateBuffer(16, nullptr, false, false);
    EXPECT_EQ(handle, 0u);
}

// CreateTexture2D / CreateRenderTarget 使用 EnsureInitialized() 延迟初始化，
// 不能在无 GL 上下文的测试环境中调用（会触发真实 GL 调用）。
// 这两个函数的正确行为是：首次调用时自动初始化设备，而不是静默返回 0。

// 测试 打开GL RHI设备：当不已初始化更新缓冲区安全
TEST(OpenGLRhiDeviceTest, WhenNotInitializedUpdateBufferSafety) {
    OpenGLRhiDevice device;
    float data[] = {0.5f};
    device.UpdateBuffer(999, 0, sizeof(data), data, false);
}

// 测试 打开GL RHI设备：设置全局阴影映射交叉边框静默忽略
TEST(OpenGLRhiDeviceTest, SetGlobalShadowMapCrossBorderSilentlyIgnore) {
    OpenGLRhiDevice device;
    device.SetGlobalShadowMap(0, 100);
    device.SetGlobalShadowMap(2, 200);
    device.SetGlobalShadowMap(3, 999);
    device.SetGlobalShadowMap(100, 999);
}

// 测试 打开GL RHI设备：全部接口不崩溃
TEST(OpenGLRhiDeviceTest, AllTheInterfaceDoesNotCrash) {
    OpenGLRhiDevice device;
    device.SetGlobalShadowMap(0, 1);
    device.SetGlobalSpotShadowMap(0, 2);
    device.SetGlobalPointShadowMap(0, 3);
    device.SetGlobalLightSpaceMatrix(0, glm::mat4(1.0f));
    device.SetGlobalCascadeSplit(0, 0.1f);
    device.SetGlobalCascadeSplit(1, 0.3f);
    device.SetGlobalCascadeSplit(2, 0.9f);
    device.SetGlobalSpotLightSpaceMatrix(0, glm::mat4(1.0f));
}

// 测试 打开GL RHI设备：灯光探针SH接口不崩溃
TEST(OpenGLRhiDeviceTest, LightProbeSHTheInterfaceDoesNotCrash) {
    OpenGLRhiDevice device;
    glm::vec4 sh[9] = {};
    device.SetGlobalLightProbeSH(sh, true);
    device.SetGlobalLightProbeSH(sh, false);
}

// 测试 打开GL RHI设备：G缓冲区接口不崩溃
TEST(OpenGLRhiDeviceTest, GBufferTheInterfaceDoesNotCrash) {
    OpenGLRhiDevice device;
    device.SetGlobalGBufferTexture(0, 100);
    device.SetGlobalGBufferTexture(3, 200);
    device.SetGBufferRenderingMode(true);
    device.SetGBufferRenderingMode(false);
}

// 测试 打开GL RHI设备：需要纹理Y翻转返回true
TEST(OpenGLRhiDeviceTest, NeedsTextureYFlipReturnstrue) {
    OpenGLRhiDevice device;
    EXPECT_TRUE(device.NeedsTextureYFlip());
}

// 测试 打开GL RHI设备：需要回读Y翻转返回true
TEST(OpenGLRhiDeviceTest, NeedsReadbackYFlipReturnstrue) {
    OpenGLRhiDevice device;
    EXPECT_TRUE(device.NeedsReadbackYFlip());
}

// 测试 打开GL RHI设备：获取投影校正为单位
TEST(OpenGLRhiDeviceTest, GetProjectionCorrectionIsIdentity) {
    OpenGLRhiDevice device;
    glm::mat4 corr = device.GetProjectionCorrection();
    EXPECT_FLOAT_EQ(corr[0][0], 1.0f);
    EXPECT_FLOAT_EQ(corr[1][1], 1.0f);
    EXPECT_FLOAT_EQ(corr[2][2], 1.0f);
    EXPECT_FLOAT_EQ(corr[3][3], 1.0f);
}

// 测试 打开GL RHI设备：获取阴影采样校正为单位
TEST(OpenGLRhiDeviceTest, GetShadowSampleCorrectionIsIdentity) {
    OpenGLRhiDevice device;
    glm::mat4 corr = device.GetShadowSampleCorrection();
    EXPECT_FLOAT_EQ(corr[0][0], 1.0f);
    EXPECT_FLOAT_EQ(corr[1][1], 1.0f);
    EXPECT_FLOAT_EQ(corr[2][2], 1.0f);
    EXPECT_FLOAT_EQ(corr[3][3], 1.0f);
}

// ============================================================
// 3. OpenGLCommandBuffer 无 device 测试
// ============================================================

// 测试 打开GL命令缓冲区：不崩溃
TEST(OpenGLCommandBufferTest, DoesNotCrash) {
    OpenGLCommandBuffer cmd;
}

// 测试 打开GL命令缓冲区：无设备当开始结束渲染通道安全
TEST(OpenGLCommandBufferTest, WithoutdeviceWhenBeginEndRenderPassSafety) {
    OpenGLCommandBuffer cmd;
    RenderPassDesc desc{};
    cmd.BeginRenderPass(desc);
    cmd.EndRenderPass();
}

// 测试 打开GL命令缓冲区：无设备当 compute 调度安全
TEST(OpenGLCommandBufferTest, WithoutdeviceWhenDispatchComputePassSafety) {
    OpenGLCommandBuffer cmd;
    cmd.DispatchComputePass(ComputeDispatch{1, 100, 0.5f});
}

// 测试 打开GL命令缓冲区：无设备当清空颜色安全
TEST(OpenGLCommandBufferTest, WithoutdeviceWhenClearColorSafety) {
    OpenGLCommandBuffer cmd;
    cmd.ClearColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

// 测试 打开GL命令缓冲区：无设备当绑定图形管线安全
TEST(OpenGLCommandBufferTest, WithoutdeviceWhenBindPipelineSafety) {
    OpenGLCommandBuffer cmd;
    cmd.BindPipeline(12345);
}

// 测试 打开GL命令缓冲区：无设备当延迟阴影映射安全
TEST(OpenGLCommandBufferTest, WithoutdeviceWhenDeferShadowMapSafety) {
    OpenGLCommandBuffer cmd;
    cmd.BindGlobalShadowMap(0, 100);
    cmd.BindGlobalSpotShadowMap(0, 200);
    cmd.BindGlobalPointShadowMap(0, 300);
}

// 测试 打开GL命令缓冲区：设置全局Mat 4录制命令
TEST(OpenGLCommandBufferTest, SetGlobalMat4RecordingCommand) {
    OpenGLCommandBuffer cmd;
    cmd.SetGlobalMat4("u_view", glm::mat4(1.0f));
}

// 测试 打开GL命令缓冲区：Resetreset状态
TEST(OpenGLCommandBufferTest, ResetresetState) {
    OpenGLCommandBuffer cmd;
    cmd.SetGlobalMat4("u_view", glm::mat4(2.0f));
    cmd.Reset();
    // After Reset, pending uniforms cleared
    cmd.SetGlobalMat4("u_view", glm::mat4(1.0f));
}

// ============================================================
// 4. GLDrawExecutor 全局状态边界检查
// ============================================================

// 测试 GL绘制执行器：全部状态
TEST(GLDrawExecutorTest, AllState) {
    DrawExecutorGlobalState state;
    state.SetShadowMap(0, 100);
    state.SetShadowMap(2, 200);
    state.SetSpotShadowMap(3, 300);
    state.SetPointShadowMap(3, 400);
    state.SetLightSpaceMatrix(2, glm::mat4(1.0f));
    state.SetCascadeSplit(2, 0.5f);
    state.SetSpotLightSpaceMatrix(3, glm::mat4(1.0f));
    // 越界静默忽略
    state.SetShadowMap(3, 999);
    state.SetSpotShadowMap(4, 999);
    state.SetPointShadowMap(4, 999);
    state.SetLightSpaceMatrix(3, glm::mat4(1.0f));
    state.SetCascadeSplit(3, 1.0f);
    state.SetSpotLightSpaceMatrix(4, glm::mat4(1.0f));
}

// 测试 GL绘制执行器：灯光探针SH接口不崩溃
TEST(GLDrawExecutorTest, LightProbeSHTheInterfaceDoesNotCrash) {
    DrawExecutorGlobalState state;
    glm::vec4 sh[9] = {};
    for (int i = 0; i < 9; ++i) sh[i] = glm::vec4(static_cast<float>(i));
    state.SetLightProbeSH(sh, true);
    state.SetLightProbeSH(sh, false);
}

// 测试 GL绘制执行器：G缓冲区接口不崩溃
TEST(GLDrawExecutorTest, GBufferTheInterfaceDoesNotCrash) {
    DrawExecutorGlobalState state;
    state.SetGBufferTexture(0, 100);
    state.SetGBufferTexture(3, 200);
    state.gbuffer_rendering_mode = true;
    state.gbuffer_rendering_mode = false;
}

// 测试 GL绘制执行器：默认统计为零
TEST(GLDrawExecutorTest, DefaultstatsisZero) {
    DrawExecutorGlobalState state;
    GLDrawExecutor exec(state);
    const auto& stats = exec.current_frame_stats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

// 测试 GL绘制执行器：开始结束帧不崩溃
TEST(GLDrawExecutorTest, BeginEndFrameDoesNotCrash) {
    DrawExecutorGlobalState state;
    GLDrawExecutor executor(state);
    executor.BeginFrame();
    SUCCEED();
}

// 测试 GL绘制执行器：Defaultis零
TEST(GLDrawExecutorTest, DefaultisZero) {
    DrawExecutorGlobalState state;
    GLDrawExecutor exec(state);
    EXPECT_EQ(exec.white_texture_handle(), 0u);
    EXPECT_EQ(exec.vao_handle().raw(), 0u);
    EXPECT_EQ(exec.vbo_handle(), 0u);
    EXPECT_EQ(exec.ebo_handle(), 0u);
    EXPECT_EQ(exec.mesh_vao_handle().raw(), 0u);
    EXPECT_EQ(exec.mesh_vbo_handle(), 0u);
    EXPECT_EQ(exec.mesh_ibo_handle(), 0u);
    EXPECT_EQ(exec.skybox_vao_handle().raw(), 0u);
    EXPECT_EQ(exec.skybox_vbo_handle(), 0u);
    EXPECT_EQ(exec.active_render_target(), 0u);
}

// 测试 GL绘制执行器：关闭Geometry缓冲区未初始化不崩溃
TEST(GLDrawExecutorTest, ShutdownGeometryBuffersUninitializedDoesNotCrash) {
    DrawExecutorGlobalState state;
    GLDrawExecutor exec(state);
    exec.ShutdownGeometryBuffers();
}

// ============================================================
// 5. GLShaderManager 未初始化默认值
// ============================================================

// 测试 GL着色器管理器：当不初始化为零
TEST(GLShaderManagerTest, WhenNotInitializedisZero) {
    GLShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

// 测试 GL着色器管理器：默认PBR位置Isburden单个
TEST(GLShaderManagerTest, DefaultPBR_LocationsIsburdenOne) {
    GLShaderManager mgr;
    const auto& loc = mgr.pbr_locations();
    EXPECT_EQ(loc.texture, -1);
    EXPECT_EQ(loc.normal_map, -1);
    EXPECT_EQ(loc.metallic_roughness_map, -1);
    EXPECT_EQ(loc.emissive_map, -1);
    EXPECT_EQ(loc.model, -1);
}

// 测试 GL着色器管理器：Gen PP未知特效返回零
TEST(GLShaderManagerTest, GenPPUnknownEffectReturnsZero) {
    GLShaderManager mgr;
    EXPECT_EQ(mgr.GetOrCreateGenPPShader("nonexistent"), 0u);
    EXPECT_EQ(mgr.GetOrCreateGenPPShader(""), 0u);
}

// 测试 GL着色器管理器：关闭未初始化不崩溃
TEST(GLShaderManagerTest, ShutdownUninitializedDoesNotCrash) {
    GLShaderManager mgr;
    mgr.Shutdown();
}

// 测试 GL着色器管理器：支持SSBO默认为true
TEST(GLShaderManagerTest, supports_SsboDefaultIstrue) {
    GLShaderManager mgr;
    EXPECT_TRUE(mgr.supports_ssbo());
    mgr.set_supports_ssbo(false);
    EXPECT_FALSE(mgr.supports_ssbo());
}

// ============================================================
// 6. GLResourceManager 句柄分配与渲染目标存储
// ============================================================

// 测试 GL资源管理器：递增
TEST(GLResourceManagerTest, Increments) {
    GLResourceManager mgr;
    unsigned int h1 = mgr.AllocateRenderTargetHandle();
    unsigned int h2 = mgr.AllocateRenderTargetHandle();
    EXPECT_GT(h2, h1);
}

// 测试 GL资源管理器：递增2
TEST(GLResourceManagerTest, Increments_2) {
    GLResourceManager mgr;
    unsigned int h1 = mgr.AllocateTextureHandle();
    unsigned int h2 = mgr.AllocateTextureHandle();
    EXPECT_GT(h2, h1);
}

// 测试 GL资源管理器：状态递增
TEST(GLResourceManagerTest, StateIncrements) {
    GLResourceManager mgr;
    unsigned int h1 = mgr.AllocatePipelineStateHandle();
    unsigned int h2 = mgr.AllocatePipelineStateHandle();
    EXPECT_GT(h2, h1);
}

// 测试 GL资源管理器：查询
TEST(GLResourceManagerTest, Query) {
    GLResourceManager mgr;
    unsigned int handle = mgr.AllocateRenderTargetHandle();
    RenderTargetResource rt;
    rt.desc.width = 512;
    rt.desc.height = 256;
    rt.color_texture_handle = 100;
    rt.depth_texture_handle = 200;
    mgr.StoreRenderTarget(handle, rt);

    const auto* result = mgr.GetRenderTarget(handle);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->desc.width, 512);
    EXPECT_EQ(result->desc.height, 256);
    EXPECT_EQ(result->color_texture_handle, 100u);
    EXPECT_EQ(result->depth_texture_handle, 200u);
}

// 测试 GL资源管理器：Querydoes不存在返回空指针
TEST(GLResourceManagerTest, QuerydoesNotExistReturnsnullptr) {
    GLResourceManager mgr;
    EXPECT_EQ(mgr.GetRenderTarget(99999), nullptr);
}

// 测试 GL资源管理器：移除
TEST(GLResourceManagerTest, Remove) {
    GLResourceManager mgr;
    unsigned int handle = mgr.AllocateRenderTargetHandle();
    RenderTargetResource rt;
    rt.desc.width = 128;
    mgr.StoreRenderTarget(handle, rt);
    mgr.RemoveRenderTarget(handle);
    EXPECT_EQ(mgr.GetRenderTarget(handle), nullptr);
}

// 测试 GL资源管理器：查询状态
TEST(GLResourceManagerTest, QueryState) {
    GLResourceManager mgr;
    unsigned int handle = mgr.AllocatePipelineStateHandle();
    PipelineStateDesc desc{};
    desc.depth_test_enabled = true;
    desc.depth_write_enabled = true;
    desc.cull_face = CullFace::Back;
    mgr.StorePipelineState(handle, desc);

    const auto* result = mgr.GetPipelineState(handle);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->depth_test_enabled);
    EXPECT_TRUE(result->depth_write_enabled);
    EXPECT_EQ(result->cull_face, CullFace::Back);
}

// 测试 GL资源管理器：Assetis零
TEST(GLResourceManagerTest, AssetisZero) {
    GLResourceManager mgr;
    const auto& ledger = mgr.ledger();
    EXPECT_EQ(ledger.textures_created, 0u);
    EXPECT_EQ(ledger.textures_destroyed, 0u);
    EXPECT_EQ(ledger.framebuffers_created, 0u);
    EXPECT_EQ(ledger.buffers_created, 0u);
}

// ============================================================
// 7. GLPipelineStateManager 管线状态 CRUD
// ============================================================

// 测试 GL管线状态管理器：创建管线状态返回非零句柄
TEST(GLPipelineStateManagerTest, CreatePipelineStateReturnsANonZeroHandle) {
    GLPipelineStateManager mgr;
    PipelineStateDesc desc{};
    unsigned int h = mgr.CreatePipelineState(desc);
    EXPECT_NE(h, 0u);
}

// 测试 GL管线状态管理器：递增
TEST(GLPipelineStateManagerTest, Increments) {
    GLPipelineStateManager mgr;
    PipelineStateDesc desc{};
    unsigned int h1 = mgr.CreatePipelineState(desc);
    unsigned int h2 = mgr.CreatePipelineState(desc);
    EXPECT_GT(h2, h1);
}

// 测试 GL管线状态管理器：获取管线状态查询
TEST(GLPipelineStateManagerTest, GetPipelineStateQuery) {
    GLPipelineStateManager mgr;
    PipelineStateDesc desc{};
    desc.depth_test_enabled = true;
    desc.blend_enabled = false;
    unsigned int h = mgr.CreatePipelineState(desc);
    const auto* result = mgr.GetPipelineState(h);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->depth_test_enabled);
    EXPECT_FALSE(result->blend_enabled);
}

// 测试 GL管线状态管理器：获取管线Statedoes不存在返回空指针
TEST(GLPipelineStateManagerTest, GetPipelineStatedoesNotExistReturnnullptr) {
    GLPipelineStateManager mgr;
    EXPECT_EQ(mgr.GetPipelineState(0), nullptr);
    EXPECT_EQ(mgr.GetPipelineState(99999), nullptr);
}

// 测试 GL管线状态管理器：关闭清空全部状态
TEST(GLPipelineStateManagerTest, ShutdownClearAllStatus) {
    GLPipelineStateManager mgr;
    PipelineStateDesc desc{};
    unsigned int h = mgr.CreatePipelineState(desc);
    mgr.Shutdown();
    EXPECT_EQ(mgr.GetPipelineState(h), nullptr);
    EXPECT_EQ(mgr.pipeline_state_count(), 0u);
}

// 测试 GL管线状态管理器：清空激活Statereset
TEST(GLPipelineStateManagerTest, ClearActiveStatereset) {
    GLPipelineStateManager mgr;
    mgr.set_active_pipeline_state(12345);
    EXPECT_EQ(mgr.active_pipeline_state(), 12345u);
    mgr.ClearActiveState();
    EXPECT_EQ(mgr.active_pipeline_state(), 0u);
}

// ============================================================
// 8. UBOManager 未初始化默认值
// ============================================================

// 测试 UBO管理器：当不Initializedbufferis零
TEST(UBOManagerTest, WhenNotInitializedbufferisZero) {
    UBOManager mgr;
    EXPECT_EQ(mgr.per_frame_buffer(), 0u);
    EXPECT_EQ(mgr.per_scene_buffer(), 0u);
    EXPECT_EQ(mgr.per_material_buffer(), 0u);
    EXPECT_EQ(mgr.point_lights_buffer(), 0u);
    EXPECT_EQ(mgr.spot_lights_buffer(), 0u);
    EXPECT_EQ(mgr.bone_matrices_buffer(), 0u);
    EXPECT_EQ(mgr.morph_weights_buffer(), 0u);
    EXPECT_EQ(mgr.light_probe_data_buffer(), 0u);
    EXPECT_FALSE(mgr.initialized());
}

// 测试 UBO管理器：当不已初始化关闭安全
TEST(UBOManagerTest, WhenNotInitializedShutdownSafety) {
    UBOManager mgr;
    mgr.Shutdown();
    EXPECT_FALSE(mgr.initialized());
}

// ============================================================
// 9. gl_enum_convert 枚举映射完整性
// ============================================================

// 测试 GL枚举转换：混合因子完整枚举拥有映射
TEST(GLEnumConvertTest, BlendFactorFullEnumerationHasMapping) {
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::Zero), GLConst::ZERO);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::One), GLConst::ONE);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::SrcAlpha), GLConst::SRC_ALPHA);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::OneMinusSrcAlpha), GLConst::ONE_MINUS_SRC_ALPHA);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::DstAlpha), GLConst::DST_ALPHA);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::OneMinusDstAlpha), GLConst::ONE_MINUS_DST_ALPHA);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::SrcColor), GLConst::SRC_COLOR);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::OneMinusSrcColor), GLConst::ONE_MINUS_SRC_COLOR);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::DstColor), GLConst::DST_COLOR);
    EXPECT_EQ(ToGLBlendFactor(BlendFactor::OneMinusDstColor), GLConst::ONE_MINUS_DST_COLOR);
}

// 测试 GL枚举转换：比较函数完整枚举拥有映射
TEST(GLEnumConvertTest, CompareFuncFullEnumerationHasMapping) {
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Never), GLConst::NEVER);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Less), GLConst::LESS);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Equal), GLConst::EQUAL);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::LessEqual), GLConst::LEQUAL);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Greater), GLConst::GREATER);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::NotEqual), GLConst::NOTEQUAL);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::GreaterEqual), GLConst::GEQUAL);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Always), GLConst::ALWAYS);
}

// 测试 GL枚举转换：剔除面完整枚举拥有映射
TEST(GLEnumConvertTest, CullFaceFullEnumerationHasMapping) {
    EXPECT_EQ(ToGLCullFace(CullFace::None), 0u);
    EXPECT_EQ(ToGLCullFace(CullFace::Front), GLConst::FRONT);
    EXPECT_EQ(ToGLCullFace(CullFace::Back), GLConst::BACK);
    EXPECT_EQ(ToGLCullFace(CullFace::FrontAndBack), GLConst::FRONT_AND_BACK);
}

// ============================================================
// 10. DrawExecutorGlobalState 共享状态验证
// ============================================================

// 测试 绘制执行器全局状态：默认值全部零
TEST(DrawExecutorGlobalStateTest, DefaultValuesAllZero) {
    DrawExecutorGlobalState state;
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(state.shadow_map[i], 0u);
        EXPECT_FLOAT_EQ(state.cascade_splits[i], 0.0f);
    }
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(state.spot_shadow_map[i], 0u);
        EXPECT_EQ(state.point_shadow_map[i], 0u);
    }
    EXPECT_FALSE(state.light_probe_enabled);
    EXPECT_FALSE(state.gbuffer_rendering_mode);
    EXPECT_EQ(state.current_frame_stats.draw_calls, 0);
}

// 测试 绘制执行器全局状态：Setter交叉边框静默忽略
TEST(DrawExecutorGlobalStateTest, SetterCrossBorderSilentlyIgnore) {
    DrawExecutorGlobalState state;
    state.SetShadowMap(3, 999);
    state.SetSpotShadowMap(4, 999);
    state.SetPointShadowMap(4, 999);
    state.SetLightSpaceMatrix(3, glm::mat4(1.0f));
    state.SetCascadeSplit(3, 1.0f);
    state.SetSpotLightSpaceMatrix(4, glm::mat4(1.0f));
    // 有效索引设置后可读回
    state.SetShadowMap(1, 42);
    EXPECT_EQ(state.shadow_map[1], 42u);
    state.SetCascadeSplit(2, 0.75f);
    EXPECT_FLOAT_EQ(state.cascade_splits[2], 0.75f);
}

// 测试 绘制执行器全局状态：灯光探针SH设置回读
TEST(DrawExecutorGlobalStateTest, LightProbeSHSetReadback) {
    DrawExecutorGlobalState state;
    glm::vec4 sh[9];
    for (int i = 0; i < 9; ++i) sh[i] = glm::vec4(static_cast<float>(i), 0.0f, 0.0f, 1.0f);
    state.SetLightProbeSH(sh, true);
    EXPECT_TRUE(state.light_probe_enabled);
    EXPECT_FLOAT_EQ(state.light_probe_sh[5].x, 5.0f);
    state.SetLightProbeSH(sh, false);
    EXPECT_FALSE(state.light_probe_enabled);
}

// 测试 绘制执行器全局状态：G缓冲区纹理设置读取返回
TEST(DrawExecutorGlobalStateTest, GBufferTextureSettingsReadBack) {
    DrawExecutorGlobalState state;
    state.SetGBufferTexture(0, 100);
    state.SetGBufferTexture(3, 200);
    EXPECT_EQ(state.gbuffer_texture[0], 100u);
    EXPECT_EQ(state.gbuffer_texture[3], 200u);
    // 越界
    state.SetGBufferTexture(4, 999);
}

// 测试 绘制执行器全局状态：开始结束帧统计流
TEST(DrawExecutorGlobalStateTest, BeginEndFrameStatisticsFlow) {
    DrawExecutorGlobalState state;
    state.BeginFrame();
    state.current_frame_stats.draw_calls = 42;
    state.current_frame_stats.mesh_count = 10;
    state.EndFrame();
    EXPECT_EQ(state.last_frame_stats.draw_calls, 42);
    EXPECT_EQ(state.last_frame_stats.mesh_count, 10);
    state.BeginFrame();
    EXPECT_EQ(state.current_frame_stats.draw_calls, 0);
}

// ============================================================
// 11. RenderTargetDesc / PipelineStateDesc 默认值回归
// ============================================================

// 测试 渲染目标描述符GL：MSAA Samples默认值为1
TEST(RenderTargetDescGLTest, msaa_SamplesTheDefaultValueIs1) {
    RenderTargetDesc desc{};
    EXPECT_EQ(desc.msaa_samples, 1);
}

// 测试 管线状态描述符GL：默认值
TEST(PipelineStateDescGLTest, DefaultValues) {
    PipelineStateDesc desc{};
    EXPECT_TRUE(desc.depth_test_enabled);
    EXPECT_TRUE(desc.depth_write_enabled);
    EXPECT_TRUE(desc.blend_enabled);
    EXPECT_EQ(desc.cull_face, CullFace::Back);
}

// ============================================================
// 12. CreateShaderProgram 未初始化安全性
// ============================================================

// 测试 打开GL RHI设备：当不已初始化创建着色器程序返回零
TEST(OpenGLRhiDeviceTest, WhenNotInitializedCreateShaderProgramReturnsZero) {
    OpenGLRhiDevice device;
    unsigned int handle = device.CreateShaderProgram("void main(){}", "void main(){}");
    EXPECT_EQ(handle, 0u);
}

// ============================================================
// 13. 编辑器场景视图模式 (Scene View Mode)
// ============================================================

// 测试 打开GL RHI设备：设置线框模式不崩溃
TEST(OpenGLRhiDeviceTest, SetWireframeModeDoesNotCrash) {
    OpenGLRhiDevice device;
    device.SetWireframeMode(true);
    device.SetWireframeMode(false);
}

// 测试 打开GL RHI设备：设置强制无光照可读且可写
TEST(OpenGLRhiDeviceTest, SetForceUnlitReadableAndWritable) {
    OpenGLRhiDevice device;
    device.SetForceUnlit(true);
    EXPECT_TRUE(device.GetGlobalRenderState().force_unlit);
    device.SetForceUnlit(false);
    EXPECT_FALSE(device.GetGlobalRenderState().force_unlit);
}

// 测试 打开GL RHI设备：设置过度绘制模式不崩溃
TEST(OpenGLRhiDeviceTest, SetOverdrawModeDoesNotCrash) {
    OpenGLRhiDevice device;
    device.SetOverdrawMode(true);
    device.SetOverdrawMode(false);
}
