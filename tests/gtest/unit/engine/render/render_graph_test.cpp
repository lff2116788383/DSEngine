/**
 * @file render_graph_test.cpp
 * @brief RenderGraph DAG 纯数据逻辑单元测试
 *
 * 仅覆盖 Compile 阶段的 DAG 构建与验证逻辑，不涉及 Execute 阶段。
 * Compile→Execute 端到端验证见 integration/rendergraph_integration_test.cpp。
 *
 * 覆盖场景：
 * - 资源声明与重复声明
 * - Pass 添加与读写声明
 * - 编译后 Pass 数量（线性/菱形/无依赖）
 * - 无输出 Pass 自动剔除
 * - MarkOutput 保护 Pass 不被剔除
 * - 循环依赖检测
 * - 未编译状态检查
 * - Reset 清空状态
 * - culled_pass_count 查询
 * - 空图编译
 */

#include <gtest/gtest.h>
#include "engine/render/render_graph.h"
#include "engine/render/rhi/rhi_device.h"
#include <vector>
#include <string>

using namespace dse::render;

// ============================================================
// 资源声明
// ============================================================

TEST(RenderGraphTest, 声明资源返回有效句柄) {
    RenderGraph graph;
    auto handle = graph.DeclareResource("scene_color");
    EXPECT_TRUE(handle.is_valid());
    EXPECT_NE(handle.id, 0u);
}

TEST(RenderGraphTest, 重复声明同名资源返回相同句柄) {
    RenderGraph graph;
    auto h1 = graph.DeclareResource("depth");
    auto h2 = graph.DeclareResource("depth");
    EXPECT_EQ(h1.id, h2.id);
}

TEST(RenderGraphTest, 不同名资源返回不同句柄) {
    RenderGraph graph;
    auto h1 = graph.DeclareResource("color");
    auto h2 = graph.DeclareResource("depth");
    EXPECT_NE(h1.id, h2.id);
}

// ============================================================
// Pass 添加与读写
// ============================================================

TEST(RenderGraphTest, 添加Pass返回有效句柄) {
    RenderGraph graph;
    auto pass = graph.AddPass("Forward");
    EXPECT_TRUE(pass.is_valid());
    EXPECT_NE(pass.id, 0u);
}

TEST(RenderGraphTest, 多个Pass句柄递增) {
    RenderGraph graph;
    auto p1 = graph.AddPass("Pass1");
    auto p2 = graph.AddPass("Pass2");
    EXPECT_NE(p1.id, p2.id);
}

TEST(RenderGraphTest, 无效句柄PassRead不崩溃) {
    RenderGraph graph;
    auto res = graph.DeclareResource("color");
    RenderPassHandle invalid_pass{0};
    EXPECT_NO_THROW(graph.PassRead(invalid_pass, res));
}

TEST(RenderGraphTest, 无效句柄PassWrite不崩溃) {
    RenderGraph graph;
    auto res = graph.DeclareResource("color");
    RenderPassHandle invalid_pass{0};
    EXPECT_NO_THROW(graph.PassWrite(invalid_pass, res));
}

TEST(RenderGraphTest, 无效句柄PassSetExecute不崩溃) {
    RenderGraph graph;
    auto pass = graph.AddPass("P");
    RenderPassHandle invalid{0};
    EXPECT_NO_THROW(graph.PassSetExecute(invalid, nullptr));
}

// ============================================================
// 拓扑排序
// ============================================================

/// 线性依赖：A→B→C，编译后 Pass 数正确即可
/// 执行顺序验证见 integration/rendergraph_integration_test.cpp
TEST(RenderGraphTest, 线性依赖编译后Pass数正确) {
    RenderGraph graph;
    auto res_a = graph.DeclareResource("res_a");
    auto res_b = graph.DeclareResource("res_b");

    auto pass_a = graph.AddPass("A");
    graph.PassWrite(pass_a, res_a);

    auto pass_b = graph.AddPass("B");
    graph.PassRead(pass_b, res_a);
    graph.PassWrite(pass_b, res_b);

    auto pass_c = graph.AddPass("C");
    graph.PassRead(pass_c, res_b);

    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 3u);
}

/// 菱形依赖编译后 Pass 数正确
TEST(RenderGraphTest, 菱形依赖编译后Pass数正确) {
    RenderGraph graph;
    auto res_a = graph.DeclareResource("res_a");

    auto pass_a = graph.AddPass("A");
    graph.PassWrite(pass_a, res_a);

    auto pass_b = graph.AddPass("B");
    graph.PassRead(pass_b, res_a);

    auto pass_c = graph.AddPass("C");
    graph.PassRead(pass_c, res_a);

    auto res_b = graph.DeclareResource("res_b");
    auto res_c = graph.DeclareResource("res_c");
    graph.PassWrite(pass_b, res_b);
    graph.PassWrite(pass_c, res_c);

    auto pass_d = graph.AddPass("D");
    graph.PassRead(pass_d, res_b);
    graph.PassRead(pass_d, res_c);

    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 4u);
}

/// 无依赖 Pass 编译后数量正确
TEST(RenderGraphTest, 无依赖Pass编译后数量正确) {
    RenderGraph graph;
    graph.AddPass("A");
    graph.AddPass("B");

    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 2u);
}

// ============================================================
// 自动剔除
// ============================================================

TEST(RenderGraphTest, 无输出Pass被自动剔除) {
    RenderGraph graph;
    auto res_a = graph.DeclareResource("res_a");
    auto res_b = graph.DeclareResource("res_b");
    auto res_output = graph.DeclareResource("res_output");

    auto pass_a = graph.AddPass("A");
    graph.PassWrite(pass_a, res_a);

    auto pass_b = graph.AddPass("B");
    graph.PassRead(pass_b, res_a);
    graph.PassWrite(pass_b, res_b);

    auto pass_output = graph.AddPass("Output");
    graph.PassWrite(pass_output, res_output);

    // 仅标记 res_output 为外部输出
    graph.MarkOutput(res_output);

    EXPECT_TRUE(graph.Compile());
    // A 和 B 的输出不被任何需要保留的 Pass 读取，应被剔除
    EXPECT_EQ(graph.compiled_pass_count(), 1u);
    EXPECT_EQ(graph.culled_pass_count(), 2u);
}

TEST(RenderGraphTest, 标记输出保护依赖链不被剔除) {
    RenderGraph graph;
    auto res_a = graph.DeclareResource("res_a");
    auto res_b = graph.DeclareResource("res_b");

    auto pass_a = graph.AddPass("A");
    graph.PassWrite(pass_a, res_a);

    auto pass_b = graph.AddPass("B");
    graph.PassRead(pass_b, res_a);
    graph.PassWrite(pass_b, res_b);

    // 标记最终输出
    graph.MarkOutput(res_b);

    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 2u);
    EXPECT_EQ(graph.culled_pass_count(), 0u);
}

TEST(RenderGraphTest, 无标记输出时保留所有Pass) {
    RenderGraph graph;
    auto res_a = graph.DeclareResource("res_a");

    auto pass_a = graph.AddPass("A");
    graph.PassWrite(pass_a, res_a);

    auto pass_b = graph.AddPass("B");
    graph.PassRead(pass_b, res_a);

    // 不调用 MarkOutput → 兼容模式，保留所有 Pass
    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 2u);
    EXPECT_EQ(graph.culled_pass_count(), 0u);
}

// ============================================================
// 循环依赖检测
// ============================================================

TEST(RenderGraphTest, 循环依赖编译失败) {
    // A 写 res_a，B 读 res_a 写 res_b，A 读 res_b → 构成环
    RenderGraph graph;
    auto res_a = graph.DeclareResource("res_a");
    auto res_b = graph.DeclareResource("res_b");

    auto pass_a = graph.AddPass("A");
    graph.PassWrite(pass_a, res_a);
    graph.PassRead(pass_a, res_b);

    auto pass_b = graph.AddPass("B");
    graph.PassRead(pass_b, res_a);
    graph.PassWrite(pass_b, res_b);

    EXPECT_FALSE(graph.Compile());
    EXPECT_FALSE(graph.is_compiled());
}

// ============================================================
// 执行与回退
// ============================================================

/// 未编译时 is_compiled 返回 false
TEST(RenderGraphTest, 未编译时状态正确) {
    RenderGraph graph;
    graph.AddPass("A");
    graph.AddPass("B");
    EXPECT_FALSE(graph.is_compiled());
}

/// 编译后空 Pass Execute 不崩溃（仅验证 Compile 阶段）
TEST(RenderGraphTest, 空Pass编译不崩溃) {
    RenderGraph graph;
    auto pass = graph.AddPass("Empty");
    // 不设置 execute
    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 1u);
}

// ============================================================
// 查询与重置
// ============================================================

TEST(RenderGraphTest, 空图编译成功) {
    RenderGraph graph;
    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 0u);
    EXPECT_TRUE(graph.is_compiled());
}

/// 空图重置不崩溃
TEST(RenderGraphTest, 空图重置不崩溃) {
    RenderGraph graph;
    graph.Compile();
    EXPECT_NO_THROW(graph.Reset());
}

TEST(RenderGraphTest, Reset清空所有状态) {
    RenderGraph graph;
    graph.DeclareResource("res_a");
    graph.AddPass("A");
    graph.Compile();

    graph.Reset();

    EXPECT_FALSE(graph.is_compiled());
    EXPECT_EQ(graph.compiled_pass_count(), 0u);
    EXPECT_EQ(graph.culled_pass_count(), 0u);
}

TEST(RenderGraphTest, culled_pass_count统计正确) {
    RenderGraph graph;
    auto res_kept = graph.DeclareResource("res_kept");
    auto res_unused = graph.DeclareResource("res_unused");

    auto pass_kept = graph.AddPass("Kept");
    graph.PassWrite(pass_kept, res_kept);

    auto pass_unused = graph.AddPass("Unused");
    graph.PassWrite(pass_unused, res_unused);

    graph.MarkOutput(res_kept);
    graph.Compile();

    EXPECT_EQ(graph.compiled_pass_count(), 1u);
    EXPECT_EQ(graph.culled_pass_count(), 1u);
}

TEST(RenderGraphTest, Compile后添加新Pass标记未编译) {
    RenderGraph graph;
    graph.AddPass("A");
    graph.Compile();
    EXPECT_TRUE(graph.is_compiled());

    graph.AddPass("B");
    EXPECT_FALSE(graph.is_compiled());
}

TEST(RenderGraphTest, PassRead去重) {
    RenderGraph graph;
    auto res = graph.DeclareResource("res");
    auto pass = graph.AddPass("P");
    graph.PassRead(pass, res);
    graph.PassRead(pass, res);  // 重复读取

    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 1u);
}

TEST(RenderGraphTest, PassWrite去重) {
    RenderGraph graph;
    auto res = graph.DeclareResource("res");
    auto pass = graph.AddPass("P");
    graph.PassWrite(pass, res);
    graph.PassWrite(pass, res);  // 重复写入

    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 1u);
}

TEST(RenderGraphTest, GpuTiming默认启用) {
    RenderGraph graph;
    EXPECT_TRUE(graph.IsGpuTimingEnabled());
}

TEST(RenderGraphTest, GpuTiming可禁用) {
    RenderGraph graph;
    graph.SetGpuTimingEnabled(false);
    EXPECT_FALSE(graph.IsGpuTimingEnabled());
    graph.SetGpuTimingEnabled(true);
    EXPECT_TRUE(graph.IsGpuTimingEnabled());
}
