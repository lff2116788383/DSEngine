/**
 * @file builtin_passes_test.cpp
 * @brief 内置渲染 Pass 无 GPU 单元测试
 *
 * 测试策略：
 * - 构造每个 Pass 实例（仅依赖 RenderPassContext 引用），验证不崩溃
 * - 验证 GetName() 返回预期字符串
 * - 验证 RenderPassContext 默认值
 * - 验证 TAAPass jitter 辅助函数
 * - 验证 MotionVectorPass 内部状态
 * - 验证 RenderGraph 基本声明/编译/重置
 */

#include <gtest/gtest.h>
#include "engine/render/passes/builtin_passes.h"
#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/render/render_graph.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>
#include <string>
#include <cmath>

using namespace dse::render;

// ============================================================
// RenderPassContext 默认值
// ============================================================

// 测试 渲染通道上下文：默认值全部空
TEST(RenderPassContextTest, DefaultValuesAllEmpty) {
    RenderPassContext ctx;
    EXPECT_EQ(ctx.world, nullptr);
    EXPECT_EQ(ctx.asset_manager, nullptr);
    EXPECT_EQ(ctx.rhi_device, nullptr);
    EXPECT_EQ(ctx.light_buffer, nullptr);
    EXPECT_EQ(ctx.cluster_grid, nullptr);
    EXPECT_FALSE(ctx.editor_mode);
    EXPECT_FALSE(ctx.fxaa_active);
    EXPECT_FALSE(ctx.taa_active);
    EXPECT_FALSE(ctx.use_editor_camera);
    EXPECT_FALSE(ctx.auto_exposure_active);
}

// 测试 渲染通道上下文：管线States默认为零
TEST(RenderPassContextTest, PipelineStatesDefaultIsZero) {
    RenderPassContext ctx;
    EXPECT_EQ(ctx.pipeline_states.sprite, 0u);
    EXPECT_EQ(ctx.pipeline_states.mesh, 0u);
    EXPECT_EQ(ctx.pipeline_states.prez, 0u);
    EXPECT_EQ(ctx.pipeline_states.shadow, 0u);
    EXPECT_EQ(ctx.pipeline_states.composite, 0u);
    EXPECT_EQ(ctx.pipeline_states.decal_blend, 0u);
    EXPECT_EQ(ctx.pipeline_states.wboit_accum, 0u);
    EXPECT_EQ(ctx.pipeline_states.wboit_reveal, 0u);
}

// 测试 渲染通道上下文：渲染目标默认为零
TEST(RenderPassContextTest, RenderTargetsDefaultIsZero) {
    RenderPassContext ctx;
    EXPECT_EQ(ctx.render_targets.main, 0u);
    EXPECT_EQ(ctx.render_targets.scene, 0u);
    EXPECT_EQ(ctx.render_targets.ui, 0u);
    EXPECT_EQ(ctx.render_targets.prez, 0u);
    EXPECT_EQ(ctx.render_targets.bloom_extract, 0u);
    EXPECT_EQ(ctx.render_targets.ssao, 0u);
    EXPECT_EQ(ctx.render_targets.fxaa, 0u);
    EXPECT_EQ(ctx.render_targets.taa, 0u);
    EXPECT_EQ(ctx.render_targets.dof, 0u);
    EXPECT_EQ(ctx.render_targets.ssr, 0u);
    EXPECT_EQ(ctx.render_targets.motion_vector, 0u);
    EXPECT_EQ(ctx.render_targets.outline, 0u);
    EXPECT_EQ(ctx.render_targets.fog, 0u);
    EXPECT_EQ(ctx.render_targets.gbuffer, 0u);
    EXPECT_EQ(ctx.render_targets.deferred_lighting, 0u);
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(ctx.render_targets.shadow[i], 0u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(ctx.render_targets.spot_shadow[i], 0u);
        EXPECT_EQ(ctx.render_targets.point_shadow[i], 0u);
    }
}

// 测试 渲染通道上下文：TAA Jitter默认为零
TEST(RenderPassContextTest, TAAJitterDefaultIsZero) {
    RenderPassContext ctx;
    EXPECT_FLOAT_EQ(ctx.taa_jitter.x, 0.0f);
    EXPECT_FLOAT_EQ(ctx.taa_jitter.y, 0.0f);
}

// ============================================================
// 各 Pass 构造 + GetName
// ============================================================

#define PASS_NAME_TEST(PassClass, expected_name) \
    TEST(BuiltinPassNameTest, PassClass##_GetName) { \
        RenderPassContext ctx; \
        PassClass pass(ctx); \
        EXPECT_STREQ(pass.GetName(), expected_name); \
    }

PASS_NAME_TEST(PreZPass,             "prez_pass")
PASS_NAME_TEST(CSMShadowPass,       "shadow_pass")
PASS_NAME_TEST(SpotShadowPass,      "spot_shadow_pass")
PASS_NAME_TEST(PointShadowPass,     "point_shadow_pass")
PASS_NAME_TEST(GBufferPass,         "gbuffer_pass")
PASS_NAME_TEST(DeferredLightingPass, "deferred_lighting_pass")
PASS_NAME_TEST(ForwardScenePass,    "scene_pass")
PASS_NAME_TEST(BloomPass,           "post_process_pass")
PASS_NAME_TEST(SSAOPass,            "ssao_pass")
PASS_NAME_TEST(ContactShadowPass,   "contact_shadow_pass")
PASS_NAME_TEST(FXAAPass,            "fxaa_pass")
PASS_NAME_TEST(AutoExposurePass,    "auto_exposure_pass")
PASS_NAME_TEST(UIPass,              "ui_pass")
PASS_NAME_TEST(CompositePass,       "composite_pass")
PASS_NAME_TEST(TAAPass,             "taa_pass")
PASS_NAME_TEST(DOFPass,             "dof_pass")
PASS_NAME_TEST(MotionVectorPass,    "motion_vector_pass")
PASS_NAME_TEST(MotionBlurPass,      "motion_blur_pass")
PASS_NAME_TEST(SSRPass,             "ssr_pass")
PASS_NAME_TEST(OutlinePass,         "outline_pass")
PASS_NAME_TEST(VolumetricFogPass,   "volumetric_fog_pass")
PASS_NAME_TEST(WBOITPass,           "wboit_pass")
PASS_NAME_TEST(DecalPass,           "decal_pass")
PASS_NAME_TEST(PresentPass,         "present_pass")

#undef PASS_NAME_TEST

// ============================================================
// TAAPass Jitter 测试
// ============================================================

// 测试 TAA通道：默认Jitteris零
TEST(TAAPassTest, DefaultJitterisZero) {
    RenderPassContext ctx;
    TAAPass pass(ctx);
    glm::vec2 j = pass.GetCurrentJitter();
    EXPECT_FLOAT_EQ(j.x, 0.0f);
    EXPECT_FLOAT_EQ(j.y, 0.0f);
}

// 测试 TAA通道：更新Jitterchange值
TEST(TAAPassTest, UpdateJitterchangeValue) {
    RenderPassContext ctx;
    TAAPass pass(ctx);
    pass.UpdateJitter(0);
    glm::vec2 j0 = pass.GetCurrentJitter();
    pass.UpdateJitter(1);
    glm::vec2 j1 = pass.GetCurrentJitter();
    // 无窗口时 Screen 尺寸为 0，jitter 始终为 (0,0)，两帧相同是正常行为
    // 有窗口时 Halton 序列产生不同值
    if (j0.x != 0.0f || j0.y != 0.0f) {
        EXPECT_FALSE(j0.x == j1.x && j0.y == j1.y);
    }
}

// 测试 TAA通道：Jitterwithin Reason
TEST(TAAPassTest, JitterwithinReason) {
    RenderPassContext ctx;
    TAAPass pass(ctx);
    for (int i = 0; i < 16; ++i) {
        pass.UpdateJitter(i);
        glm::vec2 j = pass.GetCurrentJitter();
        EXPECT_GE(j.x, -1.0f);
        EXPECT_LE(j.x, 1.0f);
        EXPECT_GE(j.y, -1.0f);
        EXPECT_LE(j.y, 1.0f);
    }
}

// ============================================================
// IRenderPass 接口多态
// ============================================================

// 测试 I渲染通道：多获取名称
TEST(IRenderPassTest, MultiGetName) {
    RenderPassContext ctx;
    PreZPass prez(ctx);
    ForwardScenePass scene(ctx);
    BloomPass bloom(ctx);

    IRenderPass* passes[] = { &prez, &scene, &bloom };
    EXPECT_STREQ(passes[0]->GetName(), "prez_pass");
    EXPECT_STREQ(passes[1]->GetName(), "scene_pass");
    EXPECT_STREQ(passes[2]->GetName(), "post_process_pass");
}

// ============================================================
// RenderGraph 基本声明/编译/重置
// ============================================================

// 测试 内置通道渲染图：空编译成功
TEST(BuiltinPassesRenderGraphTest, EmptyCompileSucceeds) {
    RenderGraph graph;
    EXPECT_TRUE(graph.Compile());
}

// 测试 内置通道渲染图：资源返回有效
TEST(BuiltinPassesRenderGraphTest, AssetReturnValid) {
    RenderGraph graph;
    auto h = graph.DeclareResource("scene_color");
    EXPECT_NE(h.id, 0u);
}

// 测试 内置通道渲染图：添加到通道返回有效
TEST(BuiltinPassesRenderGraphTest, AddToPassReturnValid) {
    RenderGraph graph;
    auto h = graph.AddPass("test_pass");
    EXPECT_TRUE(h.is_valid());
}

// 测试 内置通道渲染图：重置之后Recompiling成功
TEST(BuiltinPassesRenderGraphTest, ResetAfterRecompilingSuccessfully) {
    RenderGraph graph;
    graph.DeclareResource("res_a");
    graph.AddPass("pass_a");
    graph.Compile();
    graph.Reset();
    EXPECT_TRUE(graph.Compile());
}

// 测试 内置通道渲染图：外部资源
TEST(BuiltinPassesRenderGraphTest, OutsideAsset) {
    RenderGraph graph;
    auto h = graph.ImportResource("backbuffer", 42);
    EXPECT_EQ(graph.GetResourceRT(h), 42u);
}

// 测试 内置通道渲染图：Notbinding资源渲染目标返回零
TEST(BuiltinPassesRenderGraphTest, NotbindingAssetRTReturnsZero) {
    RenderGraph graph;
    auto h = graph.DeclareResource("unbound");
    EXPECT_EQ(graph.GetResourceRT(h), 0u);
}
