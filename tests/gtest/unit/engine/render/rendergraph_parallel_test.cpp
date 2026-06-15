/**
 * @file rendergraph_parallel_test.cpp
 * @brief RenderGraph 执行测试
 *
 * 测试策略：
 * - 编译后拓扑顺序正确性
 * - Execute 串行执行所有 Pass
 * - 依赖链保证顺序正确
 */

#include <gtest/gtest.h>
#include "engine/render/render_graph.h"
#include "engine/render/rhi/opengl/gl_command_buffer.h"
#include <atomic>

using namespace dse::render;

class RenderGraphExecuteTest : public ::testing::Test {
protected:
    RenderGraph graph_;
};

// 测试 渲染图执行：空执行不崩溃
TEST_F(RenderGraphExecuteTest, EmptyExecuteDoesNotCrash) {
    graph_.Compile();
    OpenGLCommandBuffer cmd;
    graph_.Execute(cmd);
}

// 测试 渲染图执行：单一通道执行
TEST_F(RenderGraphExecuteTest, SinglePassExecute) {
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
    graph_.Execute(cmd);

    EXPECT_EQ(counter.load(), 1);
}

// 测试 渲染图执行：两个独立通道全部Executed
TEST_F(RenderGraphExecuteTest, TwoIndependentPassAllExecuted) {
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
    graph_.Execute(cmd);

    EXPECT_EQ(counter.load(), 2);
}

// 测试 渲染图执行：链执行
TEST_F(RenderGraphExecuteTest, ChainExecute) {
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
    graph_.Execute(cmd);

    EXPECT_LT(shadow_order.load(), forward_order.load());
}

// 测试 渲染图执行：全部执行
TEST_F(RenderGraphExecuteTest, AllExecute) {
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
    graph_.Execute(cmd);

    EXPECT_EQ(counter.load(), 4);
}
