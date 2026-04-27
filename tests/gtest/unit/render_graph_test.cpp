/**
 * @file render_graph_test.cpp
 * @brief RenderGraph DAG 核心逻辑单元测试
 *
 * 覆盖场景：
 * - 资源声明与重复声明
 * - Pass 添加与读写声明
 * - 拓扑排序正确性（线性链 / 菱形依赖）
 * - 无输出 Pass 自动剔除
 * - MarkOutput 保护 Pass 不被剔除
 * - 循环依赖检测
 * - 未编译时回退执行
 * - Reset 清空状态
 * - culled_pass_count 查询
 * - 空图编译与执行
 */

#include <gtest/gtest.h>
#include "engine/render/render_graph.h"
#include "engine/render/rhi/rhi_device.h"
#include <vector>
#include <string>

using namespace dse::render;

/// 最小 mock：仅记录 Pass 执行顺序
class MockCommandBuffer : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc&) override {}
    void EndRenderPass() override {}
    void SetPipelineState(unsigned int) override {}
    void SetCamera(const glm::mat4&, const glm::mat4&) override {}
    void DrawBatch(const std::vector<DrawBatchItem>&) override {}
    void DrawMeshBatch(const std::vector<MeshDrawItem>&) override {}
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>&) override {}
    void ClearColor(const glm::vec4&) override {}
    void SetGlobalMat4(const std::string&, const glm::mat4&) override {}
    void SetGlobalMat4Array(const std::string&, const std::vector<glm::mat4>&) override {}
    void SetGlobalFloatArray(const std::string&, const std::vector<float>&) override {}
    void DrawSkybox(unsigned int) override {}
    void DrawPostProcess(unsigned int, const std::string&, const std::vector<float>&) override {}
    void DrawParticles3D(const std::vector<Particle3DDrawItem>&, const glm::mat4&, const glm::mat4&) override {}

    /// 记录已执行的 Pass 名称
    std::vector<std::string> executed_passes;
};

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

TEST(RenderGraphTest, 线性依赖拓扑排序) {
    // A → B → C
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

    // 验证执行顺序：A 在 B 前，B 在 C 前
    MockCommandBuffer cmd;
    std::vector<std::string> order;
    graph.PassSetExecute(pass_a, [&](CommandBuffer&) { order.push_back("A"); });
    graph.PassSetExecute(pass_b, [&](CommandBuffer&) { order.push_back("B"); });
    graph.PassSetExecute(pass_c, [&](CommandBuffer&) { order.push_back("C"); });
    graph.Execute(cmd);

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "A");
    EXPECT_EQ(order[1], "B");
    EXPECT_EQ(order[2], "C");
}

TEST(RenderGraphTest, 菱形依赖拓扑排序) {
    //     A
    //    / \
    //   B   C
    //    \ /
    //     D
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

    MockCommandBuffer cmd;
    std::vector<std::string> order;
    graph.PassSetExecute(pass_a, [&](CommandBuffer&) { order.push_back("A"); });
    graph.PassSetExecute(pass_b, [&](CommandBuffer&) { order.push_back("B"); });
    graph.PassSetExecute(pass_c, [&](CommandBuffer&) { order.push_back("C"); });
    graph.PassSetExecute(pass_d, [&](CommandBuffer&) { order.push_back("D"); });
    graph.Execute(cmd);

    ASSERT_EQ(order.size(), 4u);
    // A 一定在最前，D 一定在最后
    EXPECT_EQ(order[0], "A");
    EXPECT_EQ(order[3], "D");
    // B 和 C 在 A 之后、D 之前（顺序不确定但都在中间）
    EXPECT_TRUE(std::find(order.begin() + 1, order.end() - 1, "B") != order.end() - 1);
    EXPECT_TRUE(std::find(order.begin() + 1, order.end() - 1, "C") != order.end() - 1);
}

TEST(RenderGraphTest, 无依赖Pass按添加顺序执行) {
    RenderGraph graph;
    auto pass_a = graph.AddPass("A");
    auto pass_b = graph.AddPass("B");

    EXPECT_TRUE(graph.Compile());
    EXPECT_EQ(graph.compiled_pass_count(), 2u);

    MockCommandBuffer cmd;
    std::vector<std::string> order;
    graph.PassSetExecute(pass_a, [&](CommandBuffer&) { order.push_back("A"); });
    graph.PassSetExecute(pass_b, [&](CommandBuffer&) { order.push_back("B"); });
    graph.Execute(cmd);

    ASSERT_EQ(order.size(), 2u);
    // 无依赖时按添加顺序
    EXPECT_EQ(order[0], "A");
    EXPECT_EQ(order[1], "B");
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

TEST(RenderGraphTest, MarkOutput保护依赖链不被剔除) {
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

TEST(RenderGraphTest, 无MarkOutput时保留所有Pass) {
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

TEST(RenderGraphTest, 未编译时按添加顺序执行) {
    RenderGraph graph;
    auto pass_a = graph.AddPass("A");
    auto pass_b = graph.AddPass("B");

    MockCommandBuffer cmd;
    std::vector<std::string> order;
    graph.PassSetExecute(pass_a, [&](CommandBuffer&) { order.push_back("A"); });
    graph.PassSetExecute(pass_b, [&](CommandBuffer&) { order.push_back("B"); });
    // 不调用 Compile，直接 Execute
    graph.Execute(cmd);

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], "A");
    EXPECT_EQ(order[1], "B");
}

TEST(RenderGraphTest, 空PassExecute不崩溃) {
    RenderGraph graph;
    auto pass = graph.AddPass("Empty");
    // 不设置 execute
    EXPECT_TRUE(graph.Compile());

    MockCommandBuffer cmd;
    EXPECT_NO_THROW(graph.Execute(cmd));
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

TEST(RenderGraphTest, 空图执行不崩溃) {
    RenderGraph graph;
    graph.Compile();
    MockCommandBuffer cmd;
    EXPECT_NO_THROW(graph.Execute(cmd));
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
