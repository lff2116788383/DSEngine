/**
 * @file rendergraph_integration_test.cpp
 * @brief RenderGraph 集成测试
 *
 * 验证场景：
 * - 资源声明与 Pass 读写依赖自动推断
 * - 拓扑排序正确性：依赖 Pass 先于消费 Pass 执行
 * - 无输出被读取的 Pass 自动剔除
 * - 外部输出标记 MarkOutput 保护 Pass 不被剔除
 * - 复杂 DAG 场景（菱形依赖）
 * - Reset 与重新构建
 *
 * 注意：CommandBuffer 是纯虚基类，集成测试使用 NullCommandBuffer 桩实现，
 *       不引入 OpenGL 渲染上下文依赖。
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "engine/render/render_graph.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/passes/builtin_passes.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include <vector>
#include <string>

using namespace dse::render;

// ============================================================
// GMock 版 CommandBuffer，支持 EXPECT_CALL 验证渲染命令调用
// ============================================================

class MockCommandBuffer : public CommandBuffer {
public:
    MOCK_METHOD(void, BeginRenderPass, (const RenderPassDesc&), (override));
    MOCK_METHOD(void, EndRenderPass, (), (override));
    MOCK_METHOD(void, SetPipelineState, (unsigned int), (override));
    MOCK_METHOD(void, SetCamera, (const glm::mat4&, const glm::mat4&), (override));
    MOCK_METHOD(void, DrawMeshBatch, (const std::vector<MeshDrawItem>&), (override));
    MOCK_METHOD(void, DrawSpriteBatch, (const std::vector<SpriteDrawItem>&), (override));
    MOCK_METHOD(void, ClearColor, (const glm::vec4&), (override));
    MOCK_METHOD(void, SetGlobalMat4, (const std::string&, const glm::mat4&), (override));
    MOCK_METHOD(void, SetGlobalMat4Array, (const std::string&, const std::vector<glm::mat4>&), (override));
    MOCK_METHOD(void, SetGlobalFloatArray, (const std::string&, const std::vector<float>&), (override));
    MOCK_METHOD(void, DrawSkybox, (unsigned int), (override));
    MOCK_METHOD(void, DrawPostProcess, (dse::render::PostProcessRequest request), (override));
    MOCK_METHOD(void, DrawParticles3D, (const std::vector<Particle3DDrawItem>&, const glm::mat4&, const glm::mat4&), (override));
    MOCK_METHOD(void, DrawHairStrands, (const std::vector<HairDrawItem>&, const glm::mat4&, const glm::mat4&), (override));
    MOCK_METHOD(void, BindGlobalShadowMap, (unsigned int, unsigned int), (override));
    MOCK_METHOD(void, BindGlobalSpotShadowMap, (unsigned int, unsigned int), (override));
    MOCK_METHOD(void, BindGlobalPointShadowMap, (unsigned int, unsigned int), (override));
};

// ============================================================
// 辅助：记录 Pass 执行顺序
// ============================================================

struct PassExecutionLog {
    std::vector<std::string> executed_passes;

    void Record(const std::string& name) {
        executed_passes.push_back(name);
    }

    void Clear() { executed_passes.clear(); }

    bool Contains(const std::string& name) const {
        return std::find(executed_passes.begin(), executed_passes.end(), name) != executed_passes.end();
    }

    size_t IndexOf(const std::string& name) const {
        auto it = std::find(executed_passes.begin(), executed_passes.end(), name);
        if (it == executed_passes.end()) return SIZE_MAX;
        return static_cast<size_t>(std::distance(executed_passes.begin(), it));
    }
};

// ============================================================
// 基础 DAG 构建
// ============================================================

class RenderGraphIntegrationTest : public ::testing::Test {
protected:
    RenderGraph graph;
    PassExecutionLog log;
    MockCommandBuffer cmd_buffer;

    void TearDown() override {
        graph.Reset();
    }
};

TEST_F(RenderGraphIntegrationTest, 单Pass读写资源后编译执行) {
    auto res = graph.DeclareResource("color_buffer");
    auto pass = graph.AddPass("ClearPass");
    graph.PassWrite(pass, res);
    graph.PassSetExecute(pass, [&](CommandBuffer&) { log.Record("ClearPass"); });
    graph.MarkOutput(res);

    ASSERT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 1u);

    graph.Execute(cmd_buffer);
    EXPECT_TRUE(log.Contains("ClearPass"));
}

TEST_F(RenderGraphIntegrationTest, 依赖链A写入B读取) {
    auto depth = graph.DeclareResource("depth_buffer");
    auto color = graph.DeclareResource("color_buffer");

    auto shadow = graph.AddPass("ShadowMap");
    graph.PassWrite(shadow, depth);
    graph.PassSetExecute(shadow, [&](CommandBuffer&) { log.Record("ShadowMap"); });

    auto forward = graph.AddPass("Forward");
    graph.PassRead(forward, depth);
    graph.PassWrite(forward, color);
    graph.PassSetExecute(forward, [&](CommandBuffer&) { log.Record("Forward"); });

    graph.MarkOutput(color);

    ASSERT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 2u);

    graph.Execute(cmd_buffer);

    EXPECT_LT(log.IndexOf("ShadowMap"), log.IndexOf("Forward"));
}

TEST_F(RenderGraphIntegrationTest, 三Pass链式依赖拓扑排序正确) {
    auto res_a = graph.DeclareResource("res_a");
    auto res_b = graph.DeclareResource("res_b");
    auto res_c = graph.DeclareResource("res_c");

    auto pass1 = graph.AddPass("Pass1");
    graph.PassWrite(pass1, res_a);
    graph.PassSetExecute(pass1, [&](CommandBuffer&) { log.Record("Pass1"); });

    auto pass2 = graph.AddPass("Pass2");
    graph.PassRead(pass2, res_a);
    graph.PassWrite(pass2, res_b);
    graph.PassSetExecute(pass2, [&](CommandBuffer&) { log.Record("Pass2"); });

    auto pass3 = graph.AddPass("Pass3");
    graph.PassRead(pass3, res_b);
    graph.PassWrite(pass3, res_c);
    graph.PassSetExecute(pass3, [&](CommandBuffer&) { log.Record("Pass3"); });

    graph.MarkOutput(res_c);

    ASSERT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 3u);

    graph.Execute(cmd_buffer);

    EXPECT_LT(log.IndexOf("Pass1"), log.IndexOf("Pass2"));
    EXPECT_LT(log.IndexOf("Pass2"), log.IndexOf("Pass3"));
}

// ============================================================
// Pass 剔除
// ============================================================

TEST_F(RenderGraphIntegrationTest, 无输出被读取的Pass被剔除) {
    auto used = graph.DeclareResource("used");
    auto unused = graph.DeclareResource("unused");

    auto main_pass = graph.AddPass("MainPass");
    graph.PassWrite(main_pass, used);
    graph.PassSetExecute(main_pass, [&](CommandBuffer&) { log.Record("MainPass"); });

    auto dead_pass = graph.AddPass("DeadPass");
    graph.PassWrite(dead_pass, unused);
    graph.PassSetExecute(dead_pass, [&](CommandBuffer&) { log.Record("DeadPass"); });

    graph.MarkOutput(used);

    ASSERT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 1u);
    EXPECT_EQ(graph.culled_pass_count(), 1u);

    graph.Execute(cmd_buffer);
    EXPECT_TRUE(log.Contains("MainPass"));
    EXPECT_FALSE(log.Contains("DeadPass"));
}

TEST_F(RenderGraphIntegrationTest, 标记输出保护Pass不被剔除) {
    auto res = graph.DeclareResource("final_output");

    auto pass = graph.AddPass("ProtectedPass");
    graph.PassWrite(pass, res);
    graph.PassSetExecute(pass, [&](CommandBuffer&) { log.Record("ProtectedPass"); });

    graph.MarkOutput(res);

    ASSERT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 1u);
    EXPECT_EQ(graph.culled_pass_count(), 0u);

    graph.Execute(cmd_buffer);
    EXPECT_TRUE(log.Contains("ProtectedPass"));
}

// ============================================================
// 复杂 DAG
// ============================================================

TEST_F(RenderGraphIntegrationTest, 菱形依赖拓扑排序正确) {
    auto res1 = graph.DeclareResource("res1");
    auto res2 = graph.DeclareResource("res2");
    auto res3 = graph.DeclareResource("res3");
    auto res4 = graph.DeclareResource("res4");

    auto pa = graph.AddPass("A");
    graph.PassWrite(pa, res1);
    graph.PassSetExecute(pa, [&](CommandBuffer&) { log.Record("A"); });

    auto pb = graph.AddPass("B");
    graph.PassRead(pb, res1);
    graph.PassWrite(pb, res2);
    graph.PassSetExecute(pb, [&](CommandBuffer&) { log.Record("B"); });

    auto pc = graph.AddPass("C");
    graph.PassRead(pc, res1);
    graph.PassWrite(pc, res3);
    graph.PassSetExecute(pc, [&](CommandBuffer&) { log.Record("C"); });

    auto pd = graph.AddPass("D");
    graph.PassRead(pd, res2);
    graph.PassRead(pd, res3);
    graph.PassWrite(pd, res4);
    graph.PassSetExecute(pd, [&](CommandBuffer&) { log.Record("D"); });

    graph.MarkOutput(res4);

    ASSERT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 4u);

    graph.Execute(cmd_buffer);

    EXPECT_LT(log.IndexOf("A"), log.IndexOf("B"));
    EXPECT_LT(log.IndexOf("A"), log.IndexOf("C"));
    EXPECT_LT(log.IndexOf("B"), log.IndexOf("D"));
    EXPECT_LT(log.IndexOf("C"), log.IndexOf("D"));
}

// ============================================================
// Compile/Execute 重复与 Reset
// ============================================================

TEST_F(RenderGraphIntegrationTest, 重置后可重新构建DAG) {
    auto res = graph.DeclareResource("first_res");
    auto pass = graph.AddPass("FirstPass");
    graph.PassWrite(pass, res);
    graph.MarkOutput(res);

    ASSERT_TRUE(graph.Compile());
    graph.Reset();

    EXPECT_FALSE(graph.is_compiled());
    EXPECT_EQ(graph.compiled_pass_count(), 0u);

    auto res2 = graph.DeclareResource("second_res");
    auto pass2 = graph.AddPass("SecondPass");
    graph.PassWrite(pass2, res2);
    graph.MarkOutput(res2);

    ASSERT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 1u);
}

// ============================================================
// 边界场景
// ============================================================

TEST_F(RenderGraphIntegrationTest, 空RenderGraph编译通过) {
    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 0u);

    graph.Execute(cmd_buffer);
    SUCCEED();
}

TEST_F(RenderGraphIntegrationTest, 只有MarkOutput无Pass编译通过) {
    auto res = graph.DeclareResource("orphan_res");
    graph.MarkOutput(res);

    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 0u);
}

// ============================================================
// Stub RhiDevice：无 GPU，GetRenderTargetColorTexture 固定返回 42
// ============================================================

class StubRhiDevice : public RhiDevice {
    RenderStats stats_{};
public:
    void Shutdown() override {}
    void BeginFrame() override {}
    unsigned int CreateRenderTarget(const RenderTargetDesc&) override { return 0; }
    unsigned int GetRenderTargetColorTexture(unsigned int) const override { return 42; }
    unsigned int GetRenderTargetDepthTexture(unsigned int) const override { return 0; }
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
    unsigned int CreateVertexArray() override { return 0; }
    void DeleteVertexArray(unsigned int) override {}
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override { return nullptr; }
    void Submit(std::shared_ptr<CommandBuffer>) override {}
    void EndFrame() override {}
    const RenderStats& LastFrameStats() const override { return stats_; }
    void SetGlobalShadowMap(unsigned int, unsigned int) override {}
    void SetGlobalSpotShadowMap(unsigned int, unsigned int) override {}
    void SetGlobalPointShadowMap(unsigned int, unsigned int) override {}
    void SetGlobalLightSpaceMatrix(unsigned int, const glm::mat4&) override {}
    void SetGlobalCascadeSplit(unsigned int, float) override {}
    void SetGlobalSpotLightSpaceMatrix(unsigned int, const glm::mat4&) override {}
    void SetGlobalLightProbeSH(const glm::vec4[9], bool) override {}
};

// ============================================================
// BloomPassTest
// ============================================================

class BloomPassTest : public ::testing::Test {
protected:
    World world;
    StubRhiDevice rhi_dev;
    dse::render::RenderPassContext ctx;

    void SetUp() override {
        ctx.world       = &world;
        ctx.rhi_device  = &rhi_dev;
        ctx.render_targets.scene         = 1;
        ctx.render_targets.bloom_extract = 2;
        ctx.render_targets.bloom_mips    = {3, 4, 5, 6, 7};
    }
};

TEST_F(BloomPassTest, BloomPass_Disabled_不调用DrawPostProcess) {
    auto e = world.registry().create();
    dse::PostProcessComponent pp;
    pp.enabled       = true;
    pp.bloom_enabled = false;
    world.registry().emplace<dse::PostProcessComponent>(e, pp);

    ::testing::NiceMock<MockCommandBuffer> mock;
    EXPECT_CALL(mock, DrawPostProcess(::testing::_)).Times(0);

    dse::render::BloomPass pass(ctx);
    pass.Execute(mock);
}

TEST_F(BloomPassTest, BloomPass_Enabled_调用downsample和upsample) {
    auto e = world.registry().create();
    dse::PostProcessComponent pp;
    pp.enabled         = true;
    pp.bloom_enabled   = true;
    pp.bloom_threshold = 0.8f;
    world.registry().emplace<dse::PostProcessComponent>(e, pp);

    ::testing::NiceMock<MockCommandBuffer> mock;
    // 兜底：允许 bloom_extract 等其他效果自由调用
    EXPECT_CALL(mock, DrawPostProcess(::testing::_))
        .Times(::testing::AnyNumber());
    // 具体验证
    EXPECT_CALL(mock, DrawPostProcess(::testing::Field(&dse::render::PostProcessRequest::effect_name, "bloom_downsample")))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(mock, DrawPostProcess(::testing::Field(&dse::render::PostProcessRequest::effect_name, "bloom_upsample")))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(mock, BeginRenderPass(::testing::_))
        .Times(::testing::AtLeast(6));

    dse::render::BloomPass pass(ctx);
    pass.Execute(mock);
}

// ============================================================
// CompositePassTest
// ============================================================

class CompositePassTest : public ::testing::Test {
protected:
    World world;
    StubRhiDevice rhi_dev;
    dse::render::RenderPassContext ctx;

    void SetUp() override {
        ctx.world       = &world;
        ctx.rhi_device  = &rhi_dev;
        ctx.render_targets.scene = 1;
        ctx.render_targets.ui    = 2;
        ctx.render_targets.main  = 3;
        ctx.render_targets.bloom_mips = {4};
    }
};

TEST_F(CompositePassTest, CompositePass_BloomDisabled_使用copy) {
    auto e = world.registry().create();
    dse::PostProcessComponent pp;
    pp.enabled       = true;
    pp.bloom_enabled = false;
    world.registry().emplace<dse::PostProcessComponent>(e, pp);

    ::testing::NiceMock<MockCommandBuffer> mock;
    EXPECT_CALL(mock, DrawPostProcess(::testing::Field(&dse::render::PostProcessRequest::effect_name, "tonemapping")))
        .Times(1);
    EXPECT_CALL(mock, DrawPostProcess(::testing::Field(&dse::render::PostProcessRequest::effect_name, "ui_overlay")))
        .Times(1);

    dse::render::CompositePass pass(ctx);
    pass.Execute(mock);
}

TEST_F(CompositePassTest, CompositePass_BloomEnabled_使用bloom_composite) {
    auto e = world.registry().create();
    dse::PostProcessComponent pp;
    pp.enabled         = true;
    pp.bloom_enabled   = true;
    pp.bloom_intensity = 0.5f;
    pp.exposure        = 1.0f;
    world.registry().emplace<dse::PostProcessComponent>(e, pp);

    ::testing::NiceMock<MockCommandBuffer> mock;
    EXPECT_CALL(mock, DrawPostProcess(::testing::Field(&dse::render::PostProcessRequest::effect_name, "bloom_composite")))
        .Times(1);
    EXPECT_CALL(mock, DrawPostProcess(::testing::Field(&dse::render::PostProcessRequest::effect_name, "ui_overlay")))
        .Times(1);

    dse::render::CompositePass pass(ctx);
    pass.Execute(mock);
}

// ============================================================
// Phase H3 — RenderGraph WAW 冲突检测
// ============================================================

TEST_F(RenderGraphIntegrationTest, WAW冲突编译失败) {
    auto res = graph.DeclareResource("rt_color");
    graph.AddPass("PassA", {}, {{res, ResourceState::RenderTarget}},
                  [](CommandBuffer&) {});
    graph.AddPass("PassB", {}, {{res, ResourceState::RenderTarget}},
                  [](CommandBuffer&) {});
    graph.MarkOutput(res);
    EXPECT_FALSE(graph.Compile());
}
