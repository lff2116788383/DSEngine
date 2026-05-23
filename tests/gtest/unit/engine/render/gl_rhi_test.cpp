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

TEST(RhiFactoryGLTest, CreateRhiDevice_OpenGL返回非空) {
    auto device = CreateRhiDevice(RhiBackend::OpenGL);
    EXPECT_NE(device, nullptr);
}

// ============================================================
// 2. OpenGLRhiDevice 无 GPU 测试
// ============================================================

TEST(OpenGLRhiDeviceTest, 构造析构不崩溃) {
    OpenGLRhiDevice device;
}

TEST(OpenGLRhiDeviceTest, 未初始化时Shutdown安全) {
    OpenGLRhiDevice device;
    device.Shutdown();
}

TEST(OpenGLRhiDeviceTest, 未初始化时BeginFrame安全) {
    GTEST_SKIP() << "Requires GL context";
}

TEST(OpenGLRhiDeviceTest, 未初始化时EndFrame安全) {
    GTEST_SKIP() << "Requires GL context";
}

TEST(OpenGLRhiDeviceTest, 未初始化时Submit安全) {
    OpenGLRhiDevice device;
    auto cmd = std::make_shared<OpenGLCommandBuffer>();
    device.Submit(cmd);
}

TEST(OpenGLRhiDeviceTest, CreateVertexArray返回递增句柄) {
    GTEST_SKIP() << "Requires GL context";
}

TEST(OpenGLRhiDeviceTest, DeleteVertexArray_NoOp不崩溃) {
    GTEST_SKIP() << "Requires GL context";
}

TEST(OpenGLRhiDeviceTest, LastFrameStats默认值为零) {
    OpenGLRhiDevice device;
    const auto& stats = device.LastFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

TEST(OpenGLRhiDeviceTest, 子系统访问器可调用) {
    OpenGLRhiDevice device;
    auto& res    = device.resource_mgr();
    auto& state  = device.state_mgr();
    auto& shader = device.shader_mgr();
    auto& draw   = device.draw_executor();
    auto& ubo    = device.ubo_mgr();
    (void)res; (void)state; (void)shader; (void)draw; (void)ubo;
}

TEST(OpenGLRhiDeviceTest, 未初始化时CreateBuffer返回零) {
    OpenGLRhiDevice device;
    unsigned int handle = device.CreateBuffer(16, nullptr, false, false);
    EXPECT_EQ(handle, 0u);
}

TEST(OpenGLRhiDeviceTest, 未初始化时CreateTexture2D返回零) {
    OpenGLRhiDevice device;
    unsigned int handle = device.CreateTexture2D(4, 4, nullptr, false);
    EXPECT_EQ(handle, 0u);
}

TEST(OpenGLRhiDeviceTest, 未初始化时CreateRenderTarget返回零) {
    OpenGLRhiDevice device;
    RenderTargetDesc desc{};
    desc.width = 256;
    desc.height = 256;
    desc.has_color = true;
    unsigned int handle = device.CreateRenderTarget(desc);
    EXPECT_EQ(handle, 0u);
}

TEST(OpenGLRhiDeviceTest, 未初始化时UpdateBuffer安全) {
    OpenGLRhiDevice device;
    float data[] = {0.5f};
    device.UpdateBuffer(999, 0, sizeof(data), data, false);
}

TEST(OpenGLRhiDeviceTest, SetGlobalShadowMap越界静默忽略) {
    OpenGLRhiDevice device;
    device.SetGlobalShadowMap(0, 100);
    device.SetGlobalShadowMap(2, 200);
    device.SetGlobalShadowMap(3, 999);
    device.SetGlobalShadowMap(100, 999);
}

TEST(OpenGLRhiDeviceTest, 全局阴影光源接口不崩溃) {
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

TEST(OpenGLRhiDeviceTest, LightProbeSH接口不崩溃) {
    OpenGLRhiDevice device;
    glm::vec4 sh[9] = {};
    device.SetGlobalLightProbeSH(sh, true);
    device.SetGlobalLightProbeSH(sh, false);
}

TEST(OpenGLRhiDeviceTest, GBuffer接口不崩溃) {
    OpenGLRhiDevice device;
    device.SetGlobalGBufferTexture(0, 100);
    device.SetGlobalGBufferTexture(3, 200);
    device.SetGBufferRenderingMode(true);
    device.SetGBufferRenderingMode(false);
}

TEST(OpenGLRhiDeviceTest, NeedsTextureYFlip返回true) {
    OpenGLRhiDevice device;
    EXPECT_TRUE(device.NeedsTextureYFlip());
}

TEST(OpenGLRhiDeviceTest, NeedsReadbackYFlip返回true) {
    OpenGLRhiDevice device;
    EXPECT_TRUE(device.NeedsReadbackYFlip());
}

TEST(OpenGLRhiDeviceTest, GetProjectionCorrection为Identity) {
    OpenGLRhiDevice device;
    glm::mat4 corr = device.GetProjectionCorrection();
    EXPECT_FLOAT_EQ(corr[0][0], 1.0f);
    EXPECT_FLOAT_EQ(corr[1][1], 1.0f);
    EXPECT_FLOAT_EQ(corr[2][2], 1.0f);
    EXPECT_FLOAT_EQ(corr[3][3], 1.0f);
}

TEST(OpenGLRhiDeviceTest, GetShadowSampleCorrection为Identity) {
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

TEST(OpenGLCommandBufferTest, 构造不崩溃) {
    OpenGLCommandBuffer cmd;
}

TEST(OpenGLCommandBufferTest, 无device时BeginEndRenderPass安全) {
    OpenGLCommandBuffer cmd;
    RenderPassDesc desc{};
    cmd.BeginRenderPass(desc);
    cmd.EndRenderPass();
}

TEST(OpenGLCommandBufferTest, 无device时DrawMeshBatch安全) {
    OpenGLCommandBuffer cmd;
    std::vector<MeshDrawItem> items;
    items.emplace_back();
    cmd.DrawMeshBatch(items);
}

TEST(OpenGLCommandBufferTest, 无device时DrawSpriteBatch安全) {
    OpenGLCommandBuffer cmd;
    std::vector<SpriteDrawItem> items;
    items.emplace_back();
    cmd.DrawSpriteBatch(items);
}

TEST(OpenGLCommandBufferTest, 无device时DrawSpriteBatch空列表安全) {
    OpenGLCommandBuffer cmd;
    std::vector<SpriteDrawItem> items;
    cmd.DrawSpriteBatch(items);
}

TEST(OpenGLCommandBufferTest, 无device时DrawSkybox安全) {
    OpenGLCommandBuffer cmd;
    cmd.DrawSkybox(100);
}

TEST(OpenGLCommandBufferTest, 无device时DrawPostProcess安全) {
    OpenGLCommandBuffer cmd;
    cmd.DrawPostProcess({"bloom_downsample", 100, {1.0f, 0.5f}});
}

TEST(OpenGLCommandBufferTest, 无device时DrawParticles3D安全) {
    OpenGLCommandBuffer cmd;
    std::vector<Particle3DDrawItem> items;
    cmd.DrawParticles3D(items, glm::mat4(1.0f), glm::mat4(1.0f));
}

TEST(OpenGLCommandBufferTest, 无device时ClearColor安全) {
    OpenGLCommandBuffer cmd;
    cmd.ClearColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

TEST(OpenGLCommandBufferTest, 无device时SetPipelineState安全) {
    OpenGLCommandBuffer cmd;
    cmd.SetPipelineState(12345);
}

TEST(OpenGLCommandBufferTest, 无device时DeferShadowMap安全) {
    OpenGLCommandBuffer cmd;
    cmd.BindGlobalShadowMap(0, 100);
    cmd.BindGlobalSpotShadowMap(0, 200);
    cmd.BindGlobalPointShadowMap(0, 300);
}

TEST(OpenGLCommandBufferTest, SetCamera存储矩阵不崩溃) {
    OpenGLCommandBuffer cmd;
    cmd.SetCamera(glm::mat4(2.0f), glm::mat4(3.0f));
}

TEST(OpenGLCommandBufferTest, SetGlobalMat4录制命令) {
    OpenGLCommandBuffer cmd;
    cmd.SetGlobalMat4("u_view", glm::mat4(1.0f));
    cmd.SetGlobalMat4Array("u_bones", {glm::mat4(1.0f), glm::mat4(2.0f)});
    cmd.SetGlobalFloatArray("u_weights", {0.5f, 0.3f, 0.2f});
}

TEST(OpenGLCommandBufferTest, Reset重置状态) {
    OpenGLCommandBuffer cmd;
    cmd.SetCamera(glm::mat4(2.0f), glm::mat4(3.0f));
    cmd.DrawSkybox(100);
    cmd.Reset();
    // After Reset, view/projection are identity, pending uniforms cleared
    cmd.DrawSkybox(200);
}

// ============================================================
// 4. GLDrawExecutor 全局状态边界检查
// ============================================================

TEST(GLDrawExecutorTest, 全局状态接口边界检查) {
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

TEST(GLDrawExecutorTest, LightProbeSH接口不崩溃) {
    DrawExecutorGlobalState state;
    glm::vec4 sh[9] = {};
    for (int i = 0; i < 9; ++i) sh[i] = glm::vec4(static_cast<float>(i));
    state.SetLightProbeSH(sh, true);
    state.SetLightProbeSH(sh, false);
}

TEST(GLDrawExecutorTest, GBuffer接口不崩溃) {
    DrawExecutorGlobalState state;
    state.SetGBufferTexture(0, 100);
    state.SetGBufferTexture(3, 200);
    state.gbuffer_rendering_mode = true;
    state.gbuffer_rendering_mode = false;
}

TEST(GLDrawExecutorTest, 默认stats为零) {
    DrawExecutorGlobalState state;
    GLDrawExecutor exec(state);
    const auto& stats = exec.current_frame_stats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

TEST(GLDrawExecutorTest, BeginEndFrame不崩溃) {
    GTEST_SKIP() << "Requires GL context";
}

TEST(GLDrawExecutorTest, 默认句柄为零) {
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

TEST(GLDrawExecutorTest, ShutdownGeometryBuffers未初始化不崩溃) {
    DrawExecutorGlobalState state;
    GLDrawExecutor exec(state);
    exec.ShutdownGeometryBuffers();
}

// ============================================================
// 5. GLShaderManager 未初始化默认值
// ============================================================

TEST(GLShaderManagerTest, 未初始化时句柄为零) {
    GLShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.particle_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

TEST(GLShaderManagerTest, 默认PBR_locations纹理为负一) {
    GLShaderManager mgr;
    const auto& loc = mgr.pbr_locations();
    EXPECT_EQ(loc.texture, -1);
    EXPECT_EQ(loc.normal_map, -1);
    EXPECT_EQ(loc.metallic_roughness_map, -1);
    EXPECT_EQ(loc.emissive_map, -1);
    EXPECT_EQ(loc.model, -1);
}

TEST(GLShaderManagerTest, GenPP未知效果返回零) {
    GLShaderManager mgr;
    EXPECT_EQ(mgr.GetOrCreateGenPPShader("nonexistent"), 0u);
    EXPECT_EQ(mgr.GetOrCreateGenPPShader(""), 0u);
}

TEST(GLShaderManagerTest, Shutdown未初始化不崩溃) {
    GLShaderManager mgr;
    mgr.Shutdown();
}

TEST(GLShaderManagerTest, supports_ssbo默认为true) {
    GLShaderManager mgr;
    EXPECT_TRUE(mgr.supports_ssbo());
    mgr.set_supports_ssbo(false);
    EXPECT_FALSE(mgr.supports_ssbo());
}

// ============================================================
// 6. GLResourceManager 句柄分配与渲染目标存储
// ============================================================

TEST(GLResourceManagerTest, 句柄分配递增) {
    GLResourceManager mgr;
    unsigned int h1 = mgr.AllocateRenderTargetHandle();
    unsigned int h2 = mgr.AllocateRenderTargetHandle();
    EXPECT_GT(h2, h1);
}

TEST(GLResourceManagerTest, 纹理句柄分配递增) {
    GLResourceManager mgr;
    unsigned int h1 = mgr.AllocateTextureHandle();
    unsigned int h2 = mgr.AllocateTextureHandle();
    EXPECT_GT(h2, h1);
}

TEST(GLResourceManagerTest, 管线状态句柄分配递增) {
    GLResourceManager mgr;
    unsigned int h1 = mgr.AllocatePipelineStateHandle();
    unsigned int h2 = mgr.AllocatePipelineStateHandle();
    EXPECT_GT(h2, h1);
}

TEST(GLResourceManagerTest, 存储查询渲染目标) {
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

TEST(GLResourceManagerTest, 查询不存在的渲染目标返回nullptr) {
    GLResourceManager mgr;
    EXPECT_EQ(mgr.GetRenderTarget(99999), nullptr);
}

TEST(GLResourceManagerTest, 移除渲染目标) {
    GLResourceManager mgr;
    unsigned int handle = mgr.AllocateRenderTargetHandle();
    RenderTargetResource rt;
    rt.desc.width = 128;
    mgr.StoreRenderTarget(handle, rt);
    mgr.RemoveRenderTarget(handle);
    EXPECT_EQ(mgr.GetRenderTarget(handle), nullptr);
}

TEST(GLResourceManagerTest, 存储查询管线状态) {
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

TEST(GLResourceManagerTest, 资源账本初始为零) {
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

TEST(GLPipelineStateManagerTest, CreatePipelineState返回非零句柄) {
    GLPipelineStateManager mgr;
    PipelineStateDesc desc{};
    unsigned int h = mgr.CreatePipelineState(desc);
    EXPECT_NE(h, 0u);
}

TEST(GLPipelineStateManagerTest, 句柄递增) {
    GLPipelineStateManager mgr;
    PipelineStateDesc desc{};
    unsigned int h1 = mgr.CreatePipelineState(desc);
    unsigned int h2 = mgr.CreatePipelineState(desc);
    EXPECT_GT(h2, h1);
}

TEST(GLPipelineStateManagerTest, GetPipelineState查询) {
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

TEST(GLPipelineStateManagerTest, GetPipelineState不存在返回nullptr) {
    GLPipelineStateManager mgr;
    EXPECT_EQ(mgr.GetPipelineState(0), nullptr);
    EXPECT_EQ(mgr.GetPipelineState(99999), nullptr);
}

TEST(GLPipelineStateManagerTest, Shutdown清除所有状态) {
    GLPipelineStateManager mgr;
    PipelineStateDesc desc{};
    unsigned int h = mgr.CreatePipelineState(desc);
    mgr.Shutdown();
    EXPECT_EQ(mgr.GetPipelineState(h), nullptr);
    EXPECT_EQ(mgr.pipeline_state_count(), 0u);
}

TEST(GLPipelineStateManagerTest, ClearActiveState重置) {
    GLPipelineStateManager mgr;
    mgr.set_active_pipeline_state(12345);
    EXPECT_EQ(mgr.active_pipeline_state(), 12345u);
    mgr.ClearActiveState();
    EXPECT_EQ(mgr.active_pipeline_state(), 0u);
}

// ============================================================
// 8. UBOManager 未初始化默认值
// ============================================================

TEST(UBOManagerTest, 未初始化时buffer句柄为零) {
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

TEST(UBOManagerTest, 未初始化时Shutdown安全) {
    UBOManager mgr;
    mgr.Shutdown();
    EXPECT_FALSE(mgr.initialized());
}

// ============================================================
// 9. gl_enum_convert 枚举映射完整性
// ============================================================

TEST(GLEnumConvertTest, BlendFactor全枚举有映射) {
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

TEST(GLEnumConvertTest, CompareFunc全枚举有映射) {
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Never), GLConst::NEVER);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Less), GLConst::LESS);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Equal), GLConst::EQUAL);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::LessEqual), GLConst::LEQUAL);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Greater), GLConst::GREATER);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::NotEqual), GLConst::NOTEQUAL);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::GreaterEqual), GLConst::GEQUAL);
    EXPECT_EQ(ToGLCompareFunc(CompareFunc::Always), GLConst::ALWAYS);
}

TEST(GLEnumConvertTest, CullFace全枚举有映射) {
    EXPECT_EQ(ToGLCullFace(CullFace::None), 0u);
    EXPECT_EQ(ToGLCullFace(CullFace::Front), GLConst::FRONT);
    EXPECT_EQ(ToGLCullFace(CullFace::Back), GLConst::BACK);
    EXPECT_EQ(ToGLCullFace(CullFace::FrontAndBack), GLConst::FRONT_AND_BACK);
}

// ============================================================
// 10. DrawExecutorGlobalState 共享状态验证
// ============================================================

TEST(DrawExecutorGlobalStateTest, 默认值全零) {
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

TEST(DrawExecutorGlobalStateTest, Setter越界静默忽略) {
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

TEST(DrawExecutorGlobalStateTest, LightProbeSH设置读回) {
    DrawExecutorGlobalState state;
    glm::vec4 sh[9];
    for (int i = 0; i < 9; ++i) sh[i] = glm::vec4(static_cast<float>(i), 0.0f, 0.0f, 1.0f);
    state.SetLightProbeSH(sh, true);
    EXPECT_TRUE(state.light_probe_enabled);
    EXPECT_FLOAT_EQ(state.light_probe_sh[5].x, 5.0f);
    state.SetLightProbeSH(sh, false);
    EXPECT_FALSE(state.light_probe_enabled);
}

TEST(DrawExecutorGlobalStateTest, GBuffer纹理设置读回) {
    DrawExecutorGlobalState state;
    state.SetGBufferTexture(0, 100);
    state.SetGBufferTexture(3, 200);
    EXPECT_EQ(state.gbuffer_texture[0], 100u);
    EXPECT_EQ(state.gbuffer_texture[3], 200u);
    // 越界
    state.SetGBufferTexture(4, 999);
}

TEST(DrawExecutorGlobalStateTest, BeginEndFrame统计流转) {
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

TEST(RenderTargetDescGLTest, msaa_samples默认值为1) {
    RenderTargetDesc desc{};
    EXPECT_EQ(desc.msaa_samples, 1);
}

TEST(PipelineStateDescGLTest, 默认值) {
    PipelineStateDesc desc{};
    EXPECT_TRUE(desc.depth_test_enabled);
    EXPECT_TRUE(desc.depth_write_enabled);
    EXPECT_TRUE(desc.blend_enabled);
    EXPECT_EQ(desc.cull_face, CullFace::Back);
}

// ============================================================
// 12. CreateShaderProgram 未初始化安全性
// ============================================================

TEST(OpenGLRhiDeviceTest, 未初始化时CreateShaderProgram返回零) {
    OpenGLRhiDevice device;
    unsigned int handle = device.CreateShaderProgram("void main(){}", "void main(){}");
    EXPECT_EQ(handle, 0u);
}

// ============================================================
// 13. 编辑器场景视图模式 (Scene View Mode)
// ============================================================

TEST(OpenGLRhiDeviceTest, SetWireframeMode不崩溃) {
    OpenGLRhiDevice device;
    device.SetWireframeMode(true);
    device.SetWireframeMode(false);
}

TEST(OpenGLRhiDeviceTest, SetForceUnlit可读写) {
    OpenGLRhiDevice device;
    device.SetForceUnlit(true);
    EXPECT_TRUE(device.GetGlobalRenderState().force_unlit);
    device.SetForceUnlit(false);
    EXPECT_FALSE(device.GetGlobalRenderState().force_unlit);
}

TEST(OpenGLRhiDeviceTest, SetOverdrawMode不崩溃) {
    OpenGLRhiDevice device;
    device.SetOverdrawMode(true);
    device.SetOverdrawMode(false);
}
