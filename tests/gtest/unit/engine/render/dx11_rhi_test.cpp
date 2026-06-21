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
#include "engine/render/shaders/generated/embed/pbr_frag.gen.h"
#include "engine/render/shaders/generated/embed/skybox_vert.gen.h"
#endif

#include <glm/glm.hpp>

using namespace dse::render;

// ============================================================
// RHI Factory — D3D11 创建
// ============================================================

#ifdef DSE_ENABLE_D3D11

// 测试 RHI工厂：后端到字符串D 3D 11
TEST(RhiFactoryTest, BackendToString_D3D11) {
    EXPECT_EQ(RhiBackendToString(RhiBackend::D3D11), "D3D11");
}

// 测试 RHI工厂：创建RHI设备D 3D 11返回非空
TEST(RhiFactoryTest, CreateRhiDevice_D3D11ReturnNonEmpty) {
    auto device = CreateRhiDevice(RhiBackend::D3D11);
    EXPECT_NE(device, nullptr);
}

// ============================================================
// DX11RhiDevice 无 GPU 测试
// ============================================================

// 测试 DX 11 RHI设备：不崩溃
TEST(DX11RhiDeviceTest, DoesNotCrash) {
    DX11RhiDevice device;
}

// 测试 DX 11 RHI设备：当不已初始化关闭安全
TEST(DX11RhiDeviceTest, WhenNotInitializedShutdownSafety) {
    DX11RhiDevice device;
    device.Shutdown();
}

// 测试 DX 11 RHI设备：当不已初始化开始帧安全
TEST(DX11RhiDeviceTest, WhenNotInitializedBeginFrameSafety) {
    DX11RhiDevice device;
    device.BeginFrame();
}

// 测试 DX 11 RHI设备：当不已初始化结束帧安全
TEST(DX11RhiDeviceTest, WhenNotInitializedEndFrameSafety) {
    DX11RhiDevice device;
    device.EndFrame();
}

// 测试 DX 11 RHI设备：当不已初始化提交安全
TEST(DX11RhiDeviceTest, WhenNotInitializedSubmitSafety) {
    DX11RhiDevice device;
    auto cmd = std::make_shared<DX11CommandBuffer>();
    device.Submit(cmd);
}

// 测试 DX 11 RHI设备：当不已初始化创建渲染目标返回零
TEST(DX11RhiDeviceTest, WhenNotInitializedCreateRenderTargetReturnsZero) {
    DX11RhiDevice device;
    RenderTargetDesc desc{};
    desc.width = 256;
    desc.height = 256;
    desc.has_color = true;
    unsigned int handle = device.CreateRenderTarget(desc);
    EXPECT_EQ(handle, 0u);
}

// 测试 DX 11 RHI设备：当不已初始化创建纹理2D返回零
TEST(DX11RhiDeviceTest, WhenNotInitializedCreateTexture2DReturnsZero) {
    DX11RhiDevice device;
    unsigned int handle = device.CreateTexture2D(4, 4, nullptr, false);
    EXPECT_EQ(handle, 0u);
}

// 测试 DX 11 RHI设备：当不已初始化创建缓冲区安全
TEST(DX11RhiDeviceTest, WhenNotInitializedCreateBufferSafety) {
    DX11RhiDevice device;
    unsigned int handle = device.CreateBuffer(16, nullptr, false, false);
    EXPECT_EQ(handle, 0u);
}

// 测试 DX 11 RHI设备：当不已初始化创建着色器程序返回零
TEST(DX11RhiDeviceTest, WhenNotInitializedCreateShaderProgramReturnsZero) {
    DX11RhiDevice device;
    unsigned int handle = device.CreateShaderProgram("void main(){}", "void main(){}");
    EXPECT_EQ(handle, 0u);
}

// 测试 DX 11 RHI设备：当不已初始化删除安全
TEST(DX11RhiDeviceTest, WhenNotInitializedDeleteSafety) {
    DX11RhiDevice device;
    device.DeleteRenderTarget(999);
    device.DeleteTexture(999);
    device.DeleteShaderProgram(999);
}

// 测试 DX 11 RHI设备：当不已初始化更新缓冲区安全
TEST(DX11RhiDeviceTest, WhenNotInitializedUpdateBufferSafety) {
    DX11RhiDevice device;
    float data[] = {0.5f};
    device.UpdateBuffer(999, 0, sizeof(data), data, false);
}

// 测试 DX 11 RHI设备：创建顶点数组返回Increment句柄
TEST(DX11RhiDeviceTest, CreateVertexArrayReturnIncrementHandle) {
    DX11RhiDevice device;
    auto h1 = device.CreateVertexArray();
    auto h2 = device.CreateVertexArray();
    EXPECT_NE(h1, h2);
    EXPECT_GT(h2.raw(), h1.raw());
}

// 测试 DX 11 RHI设备：删除顶点数组无操作不崩溃
TEST(DX11RhiDeviceTest, DeleteVertexArray_NoOpDoesNotCrash) {
    DX11RhiDevice device;
    device.DeleteVertexArray(VertexArrayHandle{99999});
    device.DeleteVertexArray(VertexArrayHandle{0});
}

// 测试 DX 11 RHI设备：最后帧统计默认值为零
TEST(DX11RhiDeviceTest, LastFrameStatsDefaultValueIsZero) {
    DX11RhiDevice device;
    const auto& stats = device.LastFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

// 测试 DX 11 RHI设备：系统能够调用
TEST(DX11RhiDeviceTest, SystemCanCalls) {
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

// 测试 DX 11 RHI设备：设置全局阴影映射交叉边框静默忽略
TEST(DX11RhiDeviceTest, SetGlobalShadowMapCrossBorderSilentlyIgnore) {
    DX11RhiDevice device;
    // 有效索引
    device.SetGlobalShadowMap(0, 100);
    device.SetGlobalShadowMap(2, 200);
    // 越界索引 >= 3 应静默忽略，不崩溃
    device.SetGlobalShadowMap(3, 999);
    device.SetGlobalShadowMap(100, 999);
}

// 测试 DX 11 RHI设备：全部接口不崩溃
TEST(DX11RhiDeviceTest, AllTheInterfaceDoesNotCrash) {
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

// 测试 DX 11命令缓冲区：不崩溃
TEST(DX11CommandBufferTest, DoesNotCrash) {
    DX11CommandBuffer cmd;
}

// 测试 DX 11命令缓冲区：无设备当开始结束渲染通道安全
TEST(DX11CommandBufferTest, WithoutdeviceWhenBeginEndRenderPassSafety) {
    DX11CommandBuffer cmd;
    RenderPassDesc desc{};
    cmd.BeginRenderPass(desc);
    cmd.EndRenderPass();
}

// 测试 DX 11命令缓冲区：无设备当绘制网格批次安全
TEST(DX11CommandBufferTest, WithoutdeviceWhenDrawMeshBatchSafety) {
    DX11CommandBuffer cmd;
    std::vector<MeshDrawItem> items;
    items.emplace_back();
    cmd.DrawMeshBatch(items);
}

// 测试 DX 11命令缓冲区：无设备当绘制后期处理安全
TEST(DX11CommandBufferTest, WithoutdeviceWhenDrawPostProcessSafety) {
    DX11CommandBuffer cmd;
    cmd.DrawPostProcess({"bloom_downsample", 100, {1.0f, 0.5f}});
}

// 测试 DX 11命令缓冲区：无设备当清空颜色安全
TEST(DX11CommandBufferTest, WithoutdeviceWhenClearColorSafety) {
    DX11CommandBuffer cmd;
    cmd.ClearColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

// 测试 DX 11命令缓冲区：无设备当设置管线状态安全
TEST(DX11CommandBufferTest, WithoutdeviceWhenSetPipelineStateSafety) {
    DX11CommandBuffer cmd;
    cmd.SetPipelineState(12345);
}

// 测试 DX 11命令缓冲区：无设备当延迟阴影映射安全
TEST(DX11CommandBufferTest, WithoutdeviceWhenDeferShadowMapSafety) {
    DX11CommandBuffer cmd;
    cmd.BindGlobalShadowMap(0, 100);
    cmd.BindGlobalSpotShadowMap(0, 200);
    cmd.BindGlobalPointShadowMap(0, 300);
}

// 测试 DX 11命令缓冲区：设置相机Storage矩阵不折叠
TEST(DX11CommandBufferTest, SetCameraStorageMatrixDoesNotCollapse) {
    DX11CommandBuffer cmd;
    cmd.SetCamera(glm::mat4(2.0f), glm::mat4(3.0f));
}

// 测试 DX 11命令缓冲区：Alluniform且清空
TEST(DX11CommandBufferTest, AlluniformAndClear) {
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

// 测试 DX 11命令缓冲区：Resetresetuniform状态
TEST(DX11CommandBufferTest, ResetresetuniformState) {
    DX11CommandBuffer cmd;
    cmd.SetGlobalMat4("test", glm::mat4(1.0f));
    cmd.Reset();
    EXPECT_TRUE(cmd.pending_mat4().empty());
}

// ============================================================
// DX11DrawExecutor 全局状态边界检查
// ============================================================

// 测试 DX 11绘制执行器：全部状态
TEST(DX11DrawExecutorTest, AllState) {
    DrawExecutorGlobalState state;
    // 有效索引
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

// 测试 DX 11绘制执行器：默认统计为零
TEST(DX11DrawExecutorTest, DefaultstatsisZero) {
    DrawExecutorGlobalState state;
    DX11DrawExecutor exec(state);
    const auto& stats = exec.current_frame_stats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
}

// ============================================================
// DX11ShaderManager 未初始化状态
// ============================================================

// 测试 DX 11着色器管理器：当不初始化为零
TEST(DX11ShaderManagerTest, WhenNotInitializedisZero) {
    DX11ShaderManager mgr;
    EXPECT_EQ(mgr.pbr_shader_handle(), 0u);
    EXPECT_EQ(mgr.skybox_shader_handle(), 0u);
    EXPECT_EQ(mgr.sprite_shader_handle(), 0u);
    EXPECT_EQ(mgr.postprocess_shader_handle(), 0u);
    EXPECT_EQ(mgr.shadow_shader_handle(), 0u);
    EXPECT_EQ(mgr.programs_created(), 0u);
    EXPECT_EQ(mgr.programs_destroyed(), 0u);
}

// 测试 DX 11着色器管理器：获取程序无效句柄返回空指针
TEST(DX11ShaderManagerTest, GetProgramInvalidHandleReturnednullptr) {
    DX11ShaderManager mgr;
    EXPECT_EQ(mgr.GetProgram(0), nullptr);
    EXPECT_EQ(mgr.GetProgram(999), nullptr);
}

// 测试 DX 11着色器管理器：获取输入布局无效句柄返回空指针
TEST(DX11ShaderManagerTest, GetInputLayoutInvalidHandleReturnednullptr) {
    DX11ShaderManager mgr;
    EXPECT_EQ(mgr.GetInputLayout(0), nullptr);
    EXPECT_EQ(mgr.GetInputLayout(999), nullptr);
}

// ============================================================
// DX11 数据结构默认值
// ============================================================

// 测试 DX 11纹理：默认值
TEST(DX11TextureTest, DefaultValues) {
    DX11Texture tex;
    EXPECT_EQ(tex.texture.Get(), nullptr);
    EXPECT_EQ(tex.srv.Get(), nullptr);
    EXPECT_EQ(tex.width, 0);
    EXPECT_EQ(tex.height, 0);
    EXPECT_FALSE(tex.is_cube);
}

// 测试 DX 11缓冲区：默认值
TEST(DX11BufferTest, DefaultValues) {
    DX11Buffer buf;
    EXPECT_EQ(buf.buffer.Get(), nullptr);
    EXPECT_EQ(buf.size, 0u);
    EXPECT_FALSE(buf.is_dynamic);
    EXPECT_FALSE(buf.is_index);
}

// 测试 DX 11渲染目标：默认值
TEST(DX11RenderTargetTest, DefaultValues) {
    DX11RenderTarget rt;
    EXPECT_EQ(rt.width, 0);
    EXPECT_EQ(rt.height, 0);
    EXPECT_TRUE(rt.has_color);
    EXPECT_FALSE(rt.has_depth);
    EXPECT_EQ(rt.color_texture_handle, 0u);
    EXPECT_EQ(rt.depth_texture_handle, 0u);
}

// 测试 DX 11上下文：当不Initializeddevice为空
TEST(DX11ContextTest, WhenNotInitializeddeviceIsEmpty) {
    DX11Context ctx;
    EXPECT_EQ(ctx.device(), nullptr);
    EXPECT_EQ(ctx.device_context(), nullptr);
    EXPECT_EQ(ctx.swapchain(), nullptr);
    EXPECT_EQ(ctx.backbuffer_rtv(), nullptr);
    EXPECT_EQ(ctx.backbuffer_dsv(), nullptr);
    EXPECT_FALSE(ctx.initialized());
}

// 测试 DX 11上下文：当不已初始化关闭安全
TEST(DX11ContextTest, WhenNotInitializedShutdownSafety) {
    DX11Context ctx;
    ctx.Shutdown();
}

// 测试 渲染目标描述符：MSAA Samples默认值为1
TEST(RenderTargetDescTest, msaa_SamplesTheDefaultValueIs1) {
    RenderTargetDesc desc{};
    EXPECT_EQ(desc.msaa_samples, 1);
}

// 测试 渲染目标描述符：allow Uav默认值为false
TEST(RenderTargetDescTest, allow_UavTheDefaultValueIsfalse) {
    RenderTargetDesc desc{};
    EXPECT_FALSE(desc.allow_uav);
}

// ============================================================
// ComparisonSamplerTest — shadow_sampler_ 配置验证（无 GPU）
// ============================================================

// 测试 比较采样器：PCF采样器过滤类型为正确
TEST(ComparisonSamplerTest, PCFSamplerFilterTypeIsCorrect) {
    // D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT = 0x94 (148)
    // = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT(0x14) | D3D11 comparison bit(0x80)
    EXPECT_EQ(static_cast<int>(D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT), 0x94);
}

// 测试 比较采样器：小于相等比较函数值为正确
TEST(ComparisonSamplerTest, LESS_EQUALTheComparisonFunctionValueIsCorrect) {
    EXPECT_EQ(static_cast<int>(D3D11_COMPARISON_LESS_EQUAL), 4);
}

// ============================================================
// Phase J: DX11 PointLight/SpotLight CB 结构体验证
// ============================================================

// 测试 点灯光回调：J 3点灯光C Bsize 16 B对齐
TEST(PointLightCBTest, J3_PointLightsCBsize16BAlignment) {
    EXPECT_EQ(sizeof(DX11PointLightsCB) % 16u, 0u);
}

// 测试 点灯光回调：J 3点灯光条目正确尺寸
TEST(PointLightCBTest, J3_PointLightEntrycorrectSize) {
    EXPECT_EQ(sizeof(PointLightEntry), 48u);
}

// 测试 点灯光回调：J 3默认计数为零
TEST(PointLightCBTest, J3_DefaultcountisZero) {
    DX11PointLightsCB cb{};
    EXPECT_EQ(cb.u_point_light_count, 0);
}

// 测试 聚光灯光回调：J 3聚光灯光C Bsize 16 B对齐
TEST(SpotLightCBTest, J3_SpotLightsCBsize16BAlignment) {
    EXPECT_EQ(sizeof(DX11SpotLightsCB) % 16u, 0u);
}

// 测试 聚光灯光回调：J 3聚光灯光条目正确尺寸
TEST(SpotLightCBTest, J3_SpotLightEntrycorrectSize) {
    EXPECT_EQ(sizeof(SpotLightEntry), 64u);
}

// 测试 聚光灯光回调：J 3默认计数为零
TEST(SpotLightCBTest, J3_DefaultcountisZero) {
    DX11SpotLightsCB cb{};
    EXPECT_EQ(cb.u_spot_light_count, 0);
}

// ============================================================
// Phase L: DX11 kPbrPS 聚光灯着色器字符串校验（无 GPU）
// ============================================================

// 测试 聚光灯光着色器：L Spotlight PBR环形存在
TEST(SpotLightShaderTest, L_SpotlightPBRCircularExistence) {
    std::string src(dse::render::generated_shaders::kpbr_frag_hlsl);
    EXPECT_NE(src.find("SpotLight"), std::string::npos)
        << "kPbrPS should contain SpotLight struct";
    EXPECT_NE(src.find("cl_spot_count"), std::string::npos)
        << "kPbrPS should contain spot light cluster loop";
}

// 测试 聚光灯光着色器：L Cone Angle Falloff Calculation存在
TEST(SpotLightShaderTest, L_ConeAngleFalloffCalculationExists) {
    std::string src(dse::render::generated_shaders::kpbr_frag_hlsl);
    EXPECT_NE(src.find("outer_cone"), std::string::npos)
        << "kPbrPS should compute cone attenuation using outer_cone";
    EXPECT_NE(src.find("inner_cone"), std::string::npos)
        << "kPbrPS should compute cone attenuation using inner_cone";
}

// 测试 聚光灯光着色器：L Spotlight阴影Mapt 12 declare存在
TEST(SpotLightShaderTest, L_SpotlightShadowMapt12declareExistence) {
    std::string src(dse::render::generated_shaders::kpbr_frag_hlsl);
    EXPECT_NE(src.find("register(t"), std::string::npos)
        << "kPbrPS should declare spot shadow map texture registers";
}

// 测试 聚光灯光着色器：L聚光矩阵C Bcorrect尺寸
TEST(SpotLightShaderTest, L_SpotMatricesCBcorrectSize) {
    EXPECT_EQ(sizeof(DX11SpotMatricesCB), 256u);
    EXPECT_EQ(sizeof(DX11SpotMatricesCB) % 16u, 0u);
}

// ============================================================
// RHI 统一回归测试
// ============================================================

// 测试 DX 11投影校正：Z重映射仅无Y翻转
TEST(DX11ProjectionCorrectionTest, ZRemapOnlyWithoutYFlip) {
    DX11RhiDevice device;
    glm::mat4 corr = device.GetProjectionCorrection();
    // row 0: (1, 0, 0, 0)
    EXPECT_FLOAT_EQ(corr[0][0], 1.0f);
    // row 1: (0, 1, 0, 0) — NO Y-flip (DX11 Y-up same as OpenGL)
    EXPECT_FLOAT_EQ(corr[1][1], 1.0f);
    // row 2,3: Z remap (0.5, 0.5)
    EXPECT_FLOAT_EQ(corr[2][2], 0.5f);
    EXPECT_FLOAT_EQ(corr[3][2], 0.5f);
}

// 测试 DX 11天空盒着色器：采样Towarduseinput Pos
TEST(DX11SkyboxShaderTest, SamplingTowarduseinputPos) {
    std::string src(dse::render::generated_shaders::kskybox_vert_hlsl);
    EXPECT_NE(src.find("vTexCoords = aPos"), std::string::npos)
        << "Skybox VS should use raw vertex position as cubemap sampling direction";
}

// ============================================================
// DX11PipelineStateManager 枚举映射完整性
// ============================================================

// 测试 DX 11管线状态管理器：混合因子枚举映射完成
TEST(DX11PipelineStateManagerTest, BlendFactorEnumMappingComplete) {
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11Blend(BlendFactor::Zero),             D3D11_BLEND_ZERO);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11Blend(BlendFactor::One),              D3D11_BLEND_ONE);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11Blend(BlendFactor::SrcAlpha),         D3D11_BLEND_SRC_ALPHA);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11Blend(BlendFactor::OneMinusSrcAlpha), D3D11_BLEND_INV_SRC_ALPHA);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11Blend(BlendFactor::DstAlpha),         D3D11_BLEND_DEST_ALPHA);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11Blend(BlendFactor::OneMinusDstAlpha), D3D11_BLEND_INV_DEST_ALPHA);
}

// 测试 DX 11管线状态管理器：比较函数枚举映射完成
TEST(DX11PipelineStateManagerTest, CompareFuncEnumMappingComplete) {
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc::Never),        D3D11_COMPARISON_NEVER);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc::Less),         D3D11_COMPARISON_LESS);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc::Equal),        D3D11_COMPARISON_EQUAL);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc::LessEqual),    D3D11_COMPARISON_LESS_EQUAL);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc::Greater),      D3D11_COMPARISON_GREATER);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc::NotEqual),     D3D11_COMPARISON_NOT_EQUAL);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc::GreaterEqual), D3D11_COMPARISON_GREATER_EQUAL);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc::Always),       D3D11_COMPARISON_ALWAYS);
}

// 测试 DX 11管线状态管理器：剔除面枚举映射完成
TEST(DX11PipelineStateManagerTest, CullFaceEnumMappingComplete) {
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11CullMode(CullFace::None),  D3D11_CULL_NONE);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11CullMode(CullFace::Front), D3D11_CULL_FRONT);
    EXPECT_EQ(DX11PipelineStateManager::ToD3D11CullMode(CullFace::Back),  D3D11_CULL_BACK);
}

// 测试 DX 11管线状态管理器：当不已初始化创建管线状态返回零
TEST(DX11PipelineStateManagerTest, WhenNotInitializedCreatePipelineStateReturnsZero) {
    DX11PipelineStateManager mgr;
    PipelineStateDesc desc{};
    unsigned int handle = mgr.CreatePipelineState(desc);
    EXPECT_EQ(handle, 0u);
}

// 测试 DX 11管线状态管理器：设置激活管线状态可读且可写
TEST(DX11PipelineStateManagerTest, set_active_pipeline_StateReadableAndWritable) {
    DX11PipelineStateManager mgr;
    mgr.set_active_pipeline_state(42);
    EXPECT_EQ(mgr.active_pipeline_state(), 42u);
    mgr.set_active_pipeline_state(0);
    EXPECT_EQ(mgr.active_pipeline_state(), 0u);
}

// ============================================================
// DX11 编辑器场景视图模式 (Scene View Mode)
// ============================================================

// 测试 DX 11 RHI设备：设置线框模式未初始化不崩溃
TEST(DX11RhiDeviceTest, SetWireframeModeUninitializedDoesNotCrash) {
    DX11RhiDevice device;
    device.SetWireframeMode(true);
    device.SetWireframeMode(false);
}

// 测试 DX 11 RHI设备：设置强制无光照可读且可写
TEST(DX11RhiDeviceTest, SetForceUnlitReadableAndWritable) {
    DX11RhiDevice device;
    device.SetForceUnlit(true);
    EXPECT_TRUE(device.GetGlobalRenderState().force_unlit);
    device.SetForceUnlit(false);
    EXPECT_FALSE(device.GetGlobalRenderState().force_unlit);
}

// 测试 DX 11 RHI设备：设置过度绘制模式未初始化不崩溃
TEST(DX11RhiDeviceTest, SetOverdrawModeUninitializedDoesNotCrash) {
    DX11RhiDevice device;
    device.SetOverdrawMode(true);
    device.SetOverdrawMode(false);
}

// ============================================================
// DX11ResourceManager 基本句柄分配
// ============================================================

// 测试 DX 11 RHI设备：创建顶点数组返回Incrementing非零句柄
TEST(DX11RhiDeviceTest, CreateVertexArrayReturnsAnIncrementingNonZeroHandle) {
    DX11RhiDevice device;
    auto h1 = device.CreateVertexArray();
    auto h2 = device.CreateVertexArray();
    EXPECT_NE(h1.raw(), 0u);
    EXPECT_NE(h2.raw(), 0u);
    EXPECT_NE(h1.raw(), h2.raw());
}

// ============================================================
// Compute Uniform — name→offset 映射（无 GPU 验证路径安全+不崩溃）
// ============================================================

// 测试 DX 11 RHI设备：计算Uniform未初始化不崩溃
TEST(DX11RhiDeviceTest, ComputeUniformUninitializedDoesNotCrash) {
    DX11RhiDevice device;
    device.SetComputeUniformInt  (1, "u_count", 42);
    device.SetComputeUniformFloat(1, "u_value", 3.14f);
    device.SetComputeUniformVec2i(1, "u_off",  10, 20);
    device.SetComputeUniformVec3 (1, "u_pos",  1.f, 2.f, 3.f);
    device.SetComputeUniformVec4 (1, "u_color", 0.f, 0.f, 1.f, 1.f);
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    device.SetComputeUniformMat4 (1, "u_mvp",  identity);
    device.ClearComputeParams();
}

// 测试 DX 11 RHI设备：计算Uniform参数带相同名称执行不崩溃
TEST(DX11RhiDeviceTest, ComputeUniformParametersWithTheSameNameDoNotCrash) {
    DX11RhiDevice device;
    device.SetComputeUniformInt(1, "u_count", 42);
    device.SetComputeUniformInt(1, "u_count", 99);  // 同名重写
    device.ClearComputeParams();
}

#endif // DSE_ENABLE_D3D11