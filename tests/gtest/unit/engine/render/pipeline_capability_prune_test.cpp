/**
 * @file pipeline_capability_prune_test.cpp
 * @brief 能力声明式自动裁剪守护测试（WEB_3D_BACKEND.md §2 / A0）
 *
 * 守护目标：
 * - RenderPassCapabilityPruneReason 对每个能力位返回正确的裁剪原因。
 * - 内置 pass 的能力需求标注不被未来改动悄悄抹掉（gpu_cull / hiz_* 必须声明
 *   compute/ssbo），否则 WebGL2/GLES 档会错误地尝试运行 compute pass。
 * - 给定一组 device caps，default 管线裁剪后的 pass 集合符合预期，防止新 pass
 *   漏标能力位（设计 §2.3 验证守护）。
 */

#include <gtest/gtest.h>
#include "engine/render/pipeline/render_pipeline_profile.h"

#include <string>
#include <unordered_set>

using namespace dse::render;

namespace {

// 模拟一个全能力桌面设备（GL4.3+/Vulkan/D3D11）。
RenderPipelineValidationContext FullCapabilityContext() {
    RenderPipelineValidationContext ctx;
    ctx.editor_mode = false;
    ctx.hiz_available = true;
    ctx.gpu_driven_supported = true;
    ctx.compute_supported = true;
    ctx.ssbo_supported = true;
    ctx.max_color_attachments = 8;
    return ctx;
}

// 模拟 WebGL2(=GLES3.0) 能力档：无 compute / 无 SSBO / 无 GPU-driven / 无 Hi-Z。
RenderPipelineValidationContext WebGL2Context() {
    RenderPipelineValidationContext ctx;
    ctx.editor_mode = false;
    ctx.hiz_available = false;
    ctx.gpu_driven_supported = false;
    ctx.compute_supported = false;
    ctx.ssbo_supported = false;
    ctx.max_color_attachments = 4;  // WebGL2 通常 >=4 个颜色附件
    return ctx;
}

}  // namespace

// ============================================================
// 单能力位裁剪原因
// ============================================================

// 全能力设备：无需求的 pass 不被裁剪
TEST(PipelineCapabilityPruneTest, NoRequirementsKept) {
    RenderPassMetadata meta{"forward_scene", true};
    EXPECT_EQ(RenderPassCapabilityPruneReason(meta, FullCapabilityContext()), nullptr);
    EXPECT_EQ(RenderPassCapabilityPruneReason(meta, WebGL2Context()), nullptr);
}

// runtime_only pass 在编辑器模式被裁剪
TEST(PipelineCapabilityPruneTest, RuntimeOnlyPrunedInEditor) {
    RenderPassMetadata meta;
    meta.name = "present";
    meta.runtime_only = true;
    RenderPipelineValidationContext ctx = FullCapabilityContext();
    ctx.editor_mode = true;
    EXPECT_STREQ(RenderPassCapabilityPruneReason(meta, ctx), "editor");
    ctx.editor_mode = false;
    EXPECT_EQ(RenderPassCapabilityPruneReason(meta, ctx), nullptr);
}

// requires_hiz：无 Hi-Z 纹理时裁剪
TEST(PipelineCapabilityPruneTest, RequiresHiZ) {
    RenderPassMetadata meta;
    meta.name = "hiz_dependent";
    meta.requires_hiz = true;
    RenderPipelineValidationContext ctx = FullCapabilityContext();
    ctx.hiz_available = false;
    EXPECT_STREQ(RenderPassCapabilityPruneReason(meta, ctx), "no_hiz");
}

// requires_compute / requires_ssbo / requires_gpu_driven
TEST(PipelineCapabilityPruneTest, RequiresComputeSsboGpuDriven) {
    RenderPipelineValidationContext webgl2 = WebGL2Context();

    RenderPassMetadata compute_meta;
    compute_meta.name = "compute_pass";
    compute_meta.requires_compute = true;
    EXPECT_STREQ(RenderPassCapabilityPruneReason(compute_meta, webgl2), "no_compute");

    RenderPassMetadata ssbo_meta;
    ssbo_meta.name = "ssbo_pass";
    ssbo_meta.requires_ssbo = true;
    EXPECT_STREQ(RenderPassCapabilityPruneReason(ssbo_meta, webgl2), "no_ssbo");

    RenderPassMetadata gpu_meta;
    gpu_meta.name = "gpu_pass";
    gpu_meta.requires_gpu_driven = true;
    EXPECT_STREQ(RenderPassCapabilityPruneReason(gpu_meta, webgl2), "no_gpu_driven");
}

// requires_mrt：颜色附件不足时裁剪
TEST(PipelineCapabilityPruneTest, RequiresMrt) {
    RenderPassMetadata meta;
    meta.name = "deferred";
    meta.requires_mrt = true;
    RenderPipelineValidationContext ctx = FullCapabilityContext();
    ctx.max_color_attachments = 1;
    EXPECT_STREQ(RenderPassCapabilityPruneReason(meta, ctx), "no_mrt");
    ctx.max_color_attachments = 8;
    EXPECT_EQ(RenderPassCapabilityPruneReason(meta, ctx), nullptr);
}

// ============================================================
// 内置 pass 能力标注守护（防止未来改动抹掉标注）
// ============================================================

// gpu_cull 必须声明 compute + ssbo + gpu_driven
TEST(PipelineCapabilityPruneTest, GpuCullDeclaresCapabilities) {
    const auto& registry = BuiltinRenderPipelineRegistry();
    const RenderPassMetadata* meta = registry.FindMetadata("gpu_cull");
    ASSERT_NE(meta, nullptr);
    EXPECT_TRUE(meta->requires_gpu_driven);
    EXPECT_TRUE(meta->requires_compute);
    EXPECT_TRUE(meta->requires_ssbo);
}

// hiz_build / hiz_cull 必须声明 compute（且依赖 Hi-Z）
TEST(PipelineCapabilityPruneTest, HiZPassesDeclareCompute) {
    const auto& registry = BuiltinRenderPipelineRegistry();
    const RenderPassMetadata* build = registry.FindMetadata("hiz_build");
    const RenderPassMetadata* cull = registry.FindMetadata("hiz_cull");
    ASSERT_NE(build, nullptr);
    ASSERT_NE(cull, nullptr);
    EXPECT_TRUE(build->requires_hiz);
    EXPECT_TRUE(build->requires_compute);
    EXPECT_TRUE(cull->requires_hiz);
    EXPECT_TRUE(cull->requires_compute);
    EXPECT_TRUE(cull->requires_ssbo);
}

// ============================================================
// 端到端：default 管线在 WebGL2 能力档下的裁剪结果
// ============================================================

namespace {

std::unordered_set<std::string> SurvivingPasses(const RenderPipelineProfile& profile,
                                                const RenderPipelineValidationContext& ctx) {
    const auto& registry = BuiltinRenderPipelineRegistry();
    std::unordered_set<std::string> survivors;
    for (const auto& pass : profile.passes) {
        if (!pass.enabled) continue;
        const std::string canonical = registry.ResolveName(pass.name);
        const RenderPassMetadata* meta = registry.FindMetadata(canonical);
        if (!meta) continue;
        if (RenderPassCapabilityPruneReason(*meta, ctx) == nullptr)
            survivors.insert(canonical);
    }
    return survivors;
}

}  // namespace

// 全能力设备：default 管线的 compute/ssbo/gpu_driven pass 全部保留
TEST(PipelineCapabilityPruneTest, DefaultProfileKeepsHeavyPassesOnDesktop) {
    RenderPipelineProfile profile = MakeForwardPlusDefaultProfile();
    auto survivors = SurvivingPasses(profile, FullCapabilityContext());
    // 关键前向 pass 始终存在
    EXPECT_TRUE(survivors.count("pre_z"));
    EXPECT_TRUE(survivors.count("forward_scene"));
    EXPECT_TRUE(survivors.count("composite"));
    EXPECT_TRUE(survivors.count("present"));
}

// WebGL2 能力档：default 管线自动裁掉所有 compute/ssbo/gpu_driven pass，
// 但保留前向核心 pass —— 即"前向 + 片元能力子集"（设计 §2.3）
TEST(PipelineCapabilityPruneTest, DefaultProfilePrunesComputePassesOnWebGL2) {
    RenderPipelineProfile profile = MakeForwardPlusDefaultProfile();
    auto survivors = SurvivingPasses(profile, WebGL2Context());

    // compute/ssbo/gpu-driven pass 必须被裁剪
    EXPECT_FALSE(survivors.count("gpu_cull"));
    EXPECT_FALSE(survivors.count("hiz_build"));
    EXPECT_FALSE(survivors.count("hiz_cull"));

    // 前向核心 pass 必须存活
    EXPECT_TRUE(survivors.count("pre_z"));
    EXPECT_TRUE(survivors.count("forward_scene"));
    EXPECT_TRUE(survivors.count("composite"));
    EXPECT_TRUE(survivors.count("present"));
}
