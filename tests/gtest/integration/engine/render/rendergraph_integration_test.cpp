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
#include "engine/render/render_snapshot.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include <vector>
#include <string>
#include <algorithm>

using namespace dse::render;

// ============================================================
// GMock 版 CommandBuffer，支持 EXPECT_CALL 验证渲染命令调用
// ============================================================

class MockCommandBuffer : public CommandBuffer {
public:
    MOCK_METHOD(void, BeginRenderPass, (const RenderPassDesc&), (override));
    MOCK_METHOD(void, EndRenderPass, (), (override));
    MOCK_METHOD(void, BindPipeline, (unsigned int), (override));
    MOCK_METHOD(void, SetCamera, (const glm::mat4&, const glm::mat4&), (override));
    MOCK_METHOD(void, ClearColor, (const glm::vec4&), (override));
    MOCK_METHOD(void, SetGlobalMat4, (const std::string&, const glm::mat4&), (override));
    MOCK_METHOD(void, DispatchComputePass, (const ComputeDispatch&), (override));
    MOCK_METHOD(void, SetViewport, (int, int, int, int), (override));
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

// 测试 渲染图集成：单一通道资源之后执行
TEST_F(RenderGraphIntegrationTest, SinglePassAssetAfterExecute) {
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

// 测试 渲染图集成：链写入B读取
TEST_F(RenderGraphIntegrationTest, ChainAWriteBRead) {
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

// 测试 渲染图集成：三通道链依赖Topology为Sorted正确
TEST_F(RenderGraphIntegrationTest, ThreePassTheChainDependencyTopologyIsSortedCorrectly) {
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

// 测试 渲染图集成：无按读取通道按
TEST_F(RenderGraphIntegrationTest, WithoutByReadPassBy) {
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

// 测试 渲染图集成：标记Passnot应
TEST_F(RenderGraphIntegrationTest, MarkPassnotBe) {
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

// 测试 渲染图集成：正确
TEST_F(RenderGraphIntegrationTest, Correct) {
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

// 测试 渲染图集成：能够应重建之后重置DAG
TEST_F(RenderGraphIntegrationTest, CanBeRebuiltAfterResetDAG) {
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

// 测试 渲染图集成：空渲染Graphpass
TEST_F(RenderGraphIntegrationTest, EmptyRenderGraphpass) {
    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 0u);

    graph.Execute(cmd_buffer);
    SUCCEED();
}

// 测试 渲染图集成：带标记输出无Passpass
TEST_F(RenderGraphIntegrationTest, WithMarkOutputWithoutPasspass) {
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
    // 记录高层 Pass 迁移后经 PostProcessRenderer 向后端索取的 gen-PP 效果名，
    // 用于断言 CompositePass 的路由（bloom 启用→bloom_composite / ui_overlay 等）。
    mutable std::vector<std::string> requested_pp_effects;
    unsigned int GetGenPPShaderProgram(const std::string& effect_name) override {
        requested_pp_effects.push_back(effect_name);
        return 1;  // 非 0：表示该效果已迁到 PostProcessRenderer（受支持）
    }
    // bloom mip 链走 CommandBuffer 级 compute 原语（Option A）：返回非 0 句柄启用 compute
    // 路径（downsample=99 / upsample=88），供 BloomPass 测试区分两种调度。
    unsigned int GetBloomComputeShader(bool upsample) const override { return upsample ? 88u : 99u; }
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
    VertexArrayHandle CreateVertexArray() override { return {}; }
    void DeleteVertexArray(VertexArrayHandle) override {}
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override { return nullptr; }
    void Submit(std::shared_ptr<CommandBuffer>) override {}
    void EndFrame() override {}
    const RenderStats& LastFrameStats() const override { return stats_; }
};

// ============================================================
// BloomPassTest
// ============================================================

class BloomPassTest : public ::testing::Test {
protected:
    World world;
    StubRhiDevice rhi_dev;
    dse::render::RenderThinSnapshot snap;
    dse::render::RenderPassContext ctx;

    void SetUp() override {
        snap.Reset();
        ctx.world       = &world;
        ctx.rhi_device  = &rhi_dev;
        ctx.snapshot    = &snap;
        ctx.render_targets.scene         = 1;
        ctx.render_targets.bloom_extract = 2;
        ctx.render_targets.bloom_mips    = {3, 4, 5, 6, 7};
    }

    void SetBloomConfig(bool enabled, float threshold = 1.0f, float intensity = 0.5f) {
        snap.post_process.valid = true;
        snap.post_process.enabled = true;
        snap.post_process.bloom_enabled = enabled;
        snap.post_process.bloom_threshold = threshold;
        snap.post_process.bloom_intensity = intensity;
    }
};

// 测试 泛光通道：泛光禁用时不发起 compute 调度（提前返回，无渲染）
TEST_F(BloomPassTest, BloomPass_Disabled_DoesNotDispatch) {
    SetBloomConfig(false);

    ::testing::NiceMock<MockCommandBuffer> mock;
    EXPECT_CALL(mock, DispatchComputePass(::testing::_)).Times(0);
    EXPECT_CALL(mock, BeginRenderPass(::testing::_)).Times(0);

    dse::render::BloomPass pass(ctx);
    pass.Execute(mock);
}

// 测试 泛光通道：泛光启用 → compute 后端经 DispatchComputePass 跑 down/upsample mip 链。
// stub 设备 GetBloomComputeShader 返回 99(降)/88(升)，据 ComputeDispatch::shader 区分两段。
TEST_F(BloomPassTest, BloomPass_Enabled_DispatchesDownsampleAndUpsample) {
    SetBloomConfig(true, 0.8f);

    ::testing::NiceMock<MockCommandBuffer> mock;
    // 兜底：允许任意 compute 调度
    EXPECT_CALL(mock, DispatchComputePass(::testing::_))
        .Times(::testing::AnyNumber());
    // 降采样 mip 链（shader=99）
    EXPECT_CALL(mock, DispatchComputePass(::testing::Field(&ComputeDispatch::shader, 99u)))
        .Times(::testing::AtLeast(1));
    // 升采样 mip 链（shader=88）
    EXPECT_CALL(mock, DispatchComputePass(::testing::Field(&ComputeDispatch::shader, 88u)))
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
    dse::render::RenderThinSnapshot snap;
    dse::render::RenderPassContext ctx;

    void SetUp() override {
        snap.Reset();
        ctx.world       = &world;
        ctx.rhi_device  = &rhi_dev;
        ctx.snapshot    = &snap;
        ctx.render_targets.scene = 1;
        ctx.render_targets.ui    = 2;
        ctx.render_targets.main  = 3;
        ctx.render_targets.bloom_mips = {4};
    }

    void SetBloomConfig(bool enabled, float intensity = 0.5f, float exposure = 1.0f) {
        snap.post_process.valid = true;
        snap.post_process.enabled = true;
        snap.post_process.bloom_enabled = enabled;
        snap.post_process.bloom_intensity = intensity;
        snap.post_process.exposure = exposure;
    }
};

// CompositePass 已全迁到 PostProcessRenderer：路由经 device.GetGenPPShaderProgram(effect)
// 体现（stub 记录效果名）。bloom 禁用 → 走 tonemapping，不取 bloom_composite；ui_overlay 恒取。
TEST_F(CompositePassTest, CompositePass_BloomDisabled_UsesTonemapping) {
    SetBloomConfig(false);

    ::testing::NiceMock<MockCommandBuffer> mock;
    dse::render::CompositePass pass(ctx);
    pass.Execute(mock);

    const auto& fx = rhi_dev.requested_pp_effects;
    EXPECT_EQ(std::count(fx.begin(), fx.end(), std::string("bloom_composite")), 0);
    EXPECT_EQ(std::count(fx.begin(), fx.end(), std::string("tonemapping")), 1);
    EXPECT_EQ(std::count(fx.begin(), fx.end(), std::string("ui_overlay")), 1);
}

// bloom 启用 → 取 bloom_composite（final composite），且 ui_overlay 恒取。
TEST_F(CompositePassTest, CompositePass_BloomEnabled_UsesBloomComposite) {
    SetBloomConfig(true, 0.5f, 1.0f);

    ::testing::NiceMock<MockCommandBuffer> mock;
    dse::render::CompositePass pass(ctx);
    pass.Execute(mock);

    const auto& fx = rhi_dev.requested_pp_effects;
    EXPECT_EQ(std::count(fx.begin(), fx.end(), std::string("bloom_composite")), 1);
    EXPECT_EQ(std::count(fx.begin(), fx.end(), std::string("ui_overlay")), 1);
}

// ============================================================
// Phase H3 — RenderGraph WAW 冲突检测
// ============================================================

// 测试 渲染图集成：WAW Conflict Compilation失败
TEST_F(RenderGraphIntegrationTest, WAWConflictCompilationFailed) {
    auto res = graph.DeclareResource("rt_color");
    graph.AddPass("PassA", {}, {{res, ResourceState::RenderTarget}},
                  [](CommandBuffer&) {});
    graph.AddPass("PassB", {}, {{res, ResourceState::RenderTarget}},
                  [](CommandBuffer&) {});
    graph.MarkOutput(res);
    EXPECT_FALSE(graph.Compile());
}

// ============================================================
// Phase RG — 自动 Barrier 生成
// ============================================================

// 测试 渲染图集成：自动屏障渲染目标到着色器读取
TEST_F(RenderGraphIntegrationTest, AutoBarrier_RenderTargetToShaderRead) {
    auto res = graph.DeclareResource("rt_color");
    auto output = graph.DeclareResource("final");

    auto write_pass = graph.AddPass("WritePass");
    graph.PassWriteWithState(write_pass, res, ResourceState::RenderTarget);
    graph.PassSetExecute(write_pass, [&](CommandBuffer&) { log.Record("WritePass"); });

    auto read_pass = graph.AddPass("ReadPass");
    graph.PassReadWithState(read_pass, res, ResourceState::ShaderRead);
    graph.PassWrite(read_pass, output);
    graph.PassSetExecute(read_pass, [&](CommandBuffer&) { log.Record("ReadPass"); });

    graph.MarkOutput(output);

    ASSERT_TRUE(graph.Compile());

    // 验证 ReadPass 的 pre_barriers 包含 RenderTarget→ShaderRead 转换
    bool found_barrier = false;
    for (uint32_t pass_id : graph.compiled_order()) {
        // 找到 ReadPass
        for (const auto& p : graph.passes()) {
            if (p.id == pass_id && p.name == "ReadPass") {
                ASSERT_FALSE(p.pre_barriers.empty());
                EXPECT_EQ(p.pre_barriers[0].from, ResourceState::RenderTarget);
                EXPECT_EQ(p.pre_barriers[0].to, ResourceState::ShaderRead);
                found_barrier = true;
            }
        }
    }
    EXPECT_TRUE(found_barrier);
}

// 测试 渲染图集成：自动屏障UAV到着色器读取
TEST_F(RenderGraphIntegrationTest, AutoBarrier_UAVToShaderRead) {
    auto res = graph.DeclareResource("compute_out");
    auto output = graph.DeclareResource("final");

    auto compute_pass = graph.AddPass("ComputePass");
    graph.PassWriteWithState(compute_pass, res, ResourceState::UnorderedAccess);
    graph.PassSetExecute(compute_pass, [&](CommandBuffer&) { log.Record("ComputePass"); });

    auto sample_pass = graph.AddPass("SamplePass");
    graph.PassReadWithState(sample_pass, res, ResourceState::ShaderRead);
    graph.PassWrite(sample_pass, output);
    graph.PassSetExecute(sample_pass, [&](CommandBuffer&) { log.Record("SamplePass"); });

    graph.MarkOutput(output);

    ASSERT_TRUE(graph.Compile());

    bool found_barrier = false;
    for (const auto& p : graph.passes()) {
        if (p.name == "SamplePass") {
            ASSERT_FALSE(p.pre_barriers.empty());
            EXPECT_EQ(p.pre_barriers[0].from, ResourceState::UnorderedAccess);
            EXPECT_EQ(p.pre_barriers[0].to, ResourceState::ShaderRead);
            found_barrier = true;
        }
    }
    EXPECT_TRUE(found_barrier);
}

// 测试 渲染图集成：自动屏障深度写入到着色器读取
TEST_F(RenderGraphIntegrationTest, AutoBarrier_DepthWriteToShaderRead) {
    auto depth = graph.DeclareResource("depth");
    auto color = graph.DeclareResource("color");

    auto depth_pass = graph.AddPass("DepthPass");
    graph.PassWriteWithState(depth_pass, depth, ResourceState::DepthWrite);
    graph.PassSetExecute(depth_pass, [&](CommandBuffer&) { log.Record("DepthPass"); });

    auto lighting_pass = graph.AddPass("LightingPass");
    graph.PassReadWithState(lighting_pass, depth, ResourceState::ShaderRead);
    graph.PassWrite(lighting_pass, color);
    graph.PassSetExecute(lighting_pass, [&](CommandBuffer&) { log.Record("LightingPass"); });

    graph.MarkOutput(color);
    ASSERT_TRUE(graph.Compile());

    bool found = false;
    for (const auto& p : graph.passes()) {
        if (p.name == "LightingPass") {
            ASSERT_FALSE(p.pre_barriers.empty());
            EXPECT_EQ(p.pre_barriers[0].from, ResourceState::DepthWrite);
            EXPECT_EQ(p.pre_barriers[0].to, ResourceState::ShaderRead);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

// 测试 渲染图集成：无状态不生成屏障
TEST_F(RenderGraphIntegrationTest, WithoutStateNotgenerateBarrier) {
    auto res = graph.DeclareResource("rt_color");

    auto pass1 = graph.AddPass("Pass1");
    graph.PassWriteWithState(pass1, res, ResourceState::ShaderRead);
    graph.PassSetExecute(pass1, [&](CommandBuffer&) { log.Record("Pass1"); });

    // pass2 也读 ShaderRead — 状态没变，不应产生 barrier
    auto pass2 = graph.AddPass("Pass2");
    graph.PassReadWithState(pass2, res, ResourceState::ShaderRead);
    graph.PassSetExecute(pass2, [&](CommandBuffer&) { log.Record("Pass2"); });

    graph.MarkOutput(res);
    ASSERT_TRUE(graph.Compile());

    for (const auto& p : graph.passes()) {
        if (p.name == "Pass2") {
            EXPECT_TRUE(p.pre_barriers.empty());
        }
    }
}

// ============================================================
// Phase RG — Transient RT 生命周期 & Alias 复用
// ============================================================

class TransientRTStub : public StubRhiDevice {
    unsigned int next_handle_ = 100;
public:
    std::vector<unsigned int> created_handles;
    std::vector<unsigned int> deleted_handles;

    unsigned int CreateRenderTarget(const RenderTargetDesc&) override {
        unsigned int h = next_handle_++;
        created_handles.push_back(h);
        return h;
    }

    void DeleteRenderTarget(unsigned int handle) override {
        deleted_handles.push_back(handle);
    }
};

// 测试 渲染图集成：Transient渲染目标编译Distribution且Resetrelease
TEST_F(RenderGraphIntegrationTest, TransientRT_CompileDistributionAndResetrelease) {
    TransientRTStub stub;
    graph.SetRhiDevice(&stub);

    RenderTargetDesc desc{};
    desc.width = 256;
    desc.height = 256;
    desc.has_color = true;
    desc.has_depth = false;

    auto t1 = graph.DeclareTransient("temp1", desc);
    auto output = graph.DeclareResource("final");

    auto pass1 = graph.AddPass("FillTemp");
    graph.PassWrite(pass1, t1);
    graph.PassSetExecute(pass1, [&](CommandBuffer&) { log.Record("FillTemp"); });

    auto pass2 = graph.AddPass("UseTemp");
    graph.PassRead(pass2, t1);
    graph.PassWrite(pass2, output);
    graph.PassSetExecute(pass2, [&](CommandBuffer&) { log.Record("UseTemp"); });

    graph.MarkOutput(output);
    ASSERT_TRUE(graph.Compile());

    // Transient 应被分配了物理 RT
    EXPECT_EQ(stub.created_handles.size(), 1u);
    EXPECT_NE(graph.GetResourceRT(t1), 0u);

    // Reset 应释放
    graph.Reset();
    EXPECT_EQ(stub.deleted_handles.size(), 1u);
    EXPECT_EQ(stub.deleted_handles[0], stub.created_handles[0]);
    graph.SetRhiDevice(nullptr);
}

// 测试 渲染图集成：Transient渲染目标非Overlapping资源Alias复用
TEST_F(RenderGraphIntegrationTest, TransientRT_NonOverlappingResourcesAliasReuse) {
    TransientRTStub stub;
    graph.SetRhiDevice(&stub);

    RenderTargetDesc desc{};
    desc.width = 512;
    desc.height = 512;
    desc.has_color = true;
    desc.has_depth = false;

    auto t1 = graph.DeclareTransient("temp1", desc);
    auto t2 = graph.DeclareTransient("temp2", desc);
    auto output = graph.DeclareResource("final");

    // Pass1 写 t1
    auto p1 = graph.AddPass("P1");
    graph.PassWrite(p1, t1);
    graph.PassSetExecute(p1, [&](CommandBuffer&) { log.Record("P1"); });

    // Pass2 读 t1、写 output（t1 最后使用在此）
    auto p2 = graph.AddPass("P2");
    graph.PassRead(p2, t1);
    graph.PassWrite(p2, output);
    graph.PassSetExecute(p2, [&](CommandBuffer&) { log.Record("P2"); });

    // Pass3 写 t2（t1 已不再使用，t2 可复用 t1 的物理 RT）
    auto p3 = graph.AddPass("P3");
    graph.PassRead(p3, output);
    graph.PassWrite(p3, t2);
    graph.PassSetExecute(p3, [&](CommandBuffer&) { log.Record("P3"); });

    graph.MarkOutput(t2);
    ASSERT_TRUE(graph.Compile());

    // t1 和 t2 应共享同一个物理 RT（alias 复用）
    EXPECT_EQ(stub.created_handles.size(), 1u);
    EXPECT_EQ(graph.GetResourceRT(t1), graph.GetResourceRT(t2));

    // 手动 Reset 避免 TearDown 时 stub 已析构
    graph.Reset();
    graph.SetRhiDevice(nullptr);
}

// ============================================================
// ISceneRenderer 注册 / 阶段分发（A2 Phase 2）
// ============================================================

namespace {
class RecordingSceneRenderer : public ISceneRenderer {
public:
    void RenderPreZ(CommandBuffer&, const RenderScenePassContext&) override { ++prez; }
    void RenderShadow(CommandBuffer&, const RenderScenePassContext&) override { ++shadow; }
    void RenderOpaque(CommandBuffer&, const RenderScenePassContext&) override { ++opaque; }
    void RenderTransparent(CommandBuffer&, const RenderScenePassContext&) override { ++transparent; }
    int prez = 0, shadow = 0, opaque = 0, transparent = 0;
};
} // namespace

TEST(SceneRendererTest, ExecuteSceneRenderersDispatchesRequestedStage) {
    RenderScene scene;
    RecordingSceneRenderer r;
    scene.scene_renderers.push_back(&r);

    MockCommandBuffer cmd;
    RenderScenePassContext ctx;

    ExecuteSceneRenderers(&scene, SceneRenderStage::Opaque, cmd, ctx);
    EXPECT_EQ(r.opaque, 1);
    EXPECT_EQ(r.prez, 0);
    EXPECT_EQ(r.shadow, 0);
    EXPECT_EQ(r.transparent, 0);

    ExecuteSceneRenderers(&scene, SceneRenderStage::PreZ, cmd, ctx);
    ExecuteSceneRenderers(&scene, SceneRenderStage::Shadow, cmd, ctx);
    ExecuteSceneRenderers(&scene, SceneRenderStage::Transparent, cmd, ctx);
    EXPECT_EQ(r.prez, 1);
    EXPECT_EQ(r.shadow, 1);
    EXPECT_EQ(r.transparent, 1);
    EXPECT_EQ(r.opaque, 1);
}

TEST(SceneRendererTest, ExecuteSceneRenderersToleratesNullSceneAndNullEntry) {
    MockCommandBuffer cmd;
    RenderScenePassContext ctx;

    // 空 scene 不崩溃
    ExecuteSceneRenderers(nullptr, SceneRenderStage::Opaque, cmd, ctx);

    // 含 nullptr 项的注册表被跳过
    RenderScene scene;
    scene.scene_renderers.push_back(nullptr);
    RecordingSceneRenderer r;
    scene.scene_renderers.push_back(&r);
    ExecuteSceneRenderers(&scene, SceneRenderStage::Opaque, cmd, ctx);
    EXPECT_EQ(r.opaque, 1);
}

TEST(SceneRendererTest, RenderSceneClearResetsRegistry) {
    RenderScene scene;
    RecordingSceneRenderer r;
    scene.scene_renderers.push_back(&r);
    EXPECT_EQ(scene.scene_renderers.size(), 1u);

    scene.Clear();
    EXPECT_TRUE(scene.scene_renderers.empty());
}

TEST(SceneRendererTest, DefaultSceneRendererStagesAreNoOps) {
    // 仅重写 Opaque 的渲染器：其余阶段走基类默认空实现，不应崩溃。
    class OpaqueOnly : public ISceneRenderer {
    public:
        void RenderOpaque(CommandBuffer&, const RenderScenePassContext&) override { ++opaque; }
        int opaque = 0;
    } r;
    RenderScene scene;
    scene.scene_renderers.push_back(&r);

    MockCommandBuffer cmd;
    RenderScenePassContext ctx;
    ExecuteSceneRenderers(&scene, SceneRenderStage::PreZ, cmd, ctx);
    ExecuteSceneRenderers(&scene, SceneRenderStage::Shadow, cmd, ctx);
    ExecuteSceneRenderers(&scene, SceneRenderStage::Transparent, cmd, ctx);
    EXPECT_EQ(r.opaque, 0);
    ExecuteSceneRenderers(&scene, SceneRenderStage::Opaque, cmd, ctx);
    EXPECT_EQ(r.opaque, 1);
}
