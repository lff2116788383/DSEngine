/**
 * @file rendergraph_parallel_test.cpp
 * @brief RenderGraph ExecuteParallel 多线程测试
 *
 * 测试策略：
 * - 编译后波次划分正确性
 * - ExecuteParallel 配合 JobSystem 执行不崩溃
 * - 所有 Pass 均被执行
 * - 依赖链保证顺序正确
 */

#include <gtest/gtest.h>
#include "engine/render/render_graph.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/core/job_system.h"
#include <atomic>

using namespace dse::render;
using namespace dse::core;

class RenderGraphParallelTest : public ::testing::Test {
protected:
    RenderGraph graph_;
    JobSystem js_;

    void SetUp() override {
        js_.Init();
    }
    void TearDown() override {
        js_.Shutdown();
    }
};

TEST_F(RenderGraphParallelTest, 空图ExecuteParallel不崩溃) {
    graph_.Compile();
    OpenGLCommandBuffer cmd;
    graph_.ExecuteParallel(cmd, js_);
}

TEST_F(RenderGraphParallelTest, 单Pass并行执行) {
    auto color = graph_.DeclareResource("color");
    graph_.MarkOutput(color);

    std::atomic<int> counter{0};
    graph_.AddPass("OnlyPass",
                   {},
                   {{color, ResourceState::RenderTarget}},
                   [&counter](CommandBuffer& cmd) { counter.fetch_add(1); });

    ASSERT_TRUE(graph_.Compile());
    EXPECT_EQ(graph_.compiled_pass_count(), 1u);

    OpenGLCommandBuffer cmd;
    graph_.ExecuteParallel(cmd, js_);

    EXPECT_EQ(counter.load(), 1);
}

TEST_F(RenderGraphParallelTest, 两独立Pass同波次) {
    auto color_a = graph_.DeclareResource("color_a");
    auto color_b = graph_.DeclareResource("color_b");
    graph_.MarkOutput(color_a);
    graph_.MarkOutput(color_b);

    std::atomic<int> counter{0};
    graph_.AddPass("PassA", {}, {{color_a, ResourceState::RenderTarget}},
                   [&counter](CommandBuffer&) { counter.fetch_add(1); });
    graph_.AddPass("PassB", {}, {{color_b, ResourceState::RenderTarget}},
                   [&counter](CommandBuffer&) { counter.fetch_add(1); });

    ASSERT_TRUE(graph_.Compile());
    EXPECT_EQ(graph_.compiled_pass_count(), 2u);

    OpenGLCommandBuffer cmd;
    graph_.ExecuteParallel(cmd, js_);

    EXPECT_EQ(counter.load(), 2);
}

TEST_F(RenderGraphParallelTest, 依赖链顺序执行) {
    auto depth = graph_.DeclareResource("depth");
    auto color = graph_.DeclareResource("color");
    graph_.MarkOutput(color);

    std::atomic<int> order{0};
    std::atomic<int> shadow_order{-1};
    std::atomic<int> forward_order{-1};

    graph_.AddPass("ShadowMap", {},
                   {{depth, ResourceState::RenderTarget}},
                   [&order, &shadow_order](CommandBuffer&) {
                       shadow_order.store(order.fetch_add(1));
                   });
    graph_.AddPass("Forward",
                   {{depth, ResourceState::ShaderRead}},
                   {{color, ResourceState::RenderTarget}},
                   [&order, &forward_order](CommandBuffer&) {
                       forward_order.store(order.fetch_add(1));
                   });

    ASSERT_TRUE(graph_.Compile());

    OpenGLCommandBuffer cmd;
    graph_.ExecuteParallel(cmd, js_);

    EXPECT_LT(shadow_order.load(), forward_order.load());
}

TEST_F(RenderGraphParallelTest, 菱形依赖全部执行) {
    auto depth = graph_.DeclareResource("depth");
    auto color_a = graph_.DeclareResource("color_a");
    auto color_b = graph_.DeclareResource("color_b");
    auto final_color = graph_.DeclareResource("final");
    graph_.MarkOutput(final_color);

    std::atomic<int> counter{0};

    graph_.AddPass("Shadow", {},
                   {{depth, ResourceState::RenderTarget}},
                   [&counter](CommandBuffer&) { counter.fetch_add(1); });
    graph_.AddPass("BranchA",
                   {{depth, ResourceState::ShaderRead}},
                   {{color_a, ResourceState::RenderTarget}},
                   [&counter](CommandBuffer&) { counter.fetch_add(1); });
    graph_.AddPass("BranchB",
                   {{depth, ResourceState::ShaderRead}},
                   {{color_b, ResourceState::RenderTarget}},
                   [&counter](CommandBuffer&) { counter.fetch_add(1); });
    graph_.AddPass("Composite",
                   {{color_a, ResourceState::ShaderRead},
                    {color_b, ResourceState::ShaderRead}},
                   {{final_color, ResourceState::RenderTarget}},
                   [&counter](CommandBuffer&) { counter.fetch_add(1); });

    ASSERT_TRUE(graph_.Compile());

    OpenGLCommandBuffer cmd;
    graph_.ExecuteParallel(cmd, js_);

    EXPECT_EQ(counter.load(), 4);
}
