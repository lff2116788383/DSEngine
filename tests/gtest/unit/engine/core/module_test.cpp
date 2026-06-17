/**
 * @file module_test.cpp
 * @brief IModule 接口单元测试
 *
 * 覆盖场景：
 * - MockModule 生命周期回调（OnInit/OnUpdate/OnFixedUpdate/OnShutdown）
 * - 渲染扩展点 RegisterRenderPasses：默认不注册；模块注册的 Pass 被收集且可 Setup
 * - GetName 返回值正确
 * - 多模块独立生命周期
 * - 多态销毁
 */

#include <gtest/gtest.h>
#include "engine/core/module.h"
#include "engine/ecs/world.h"
#include "engine/render/render_graph.h"
#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include <memory>
#include <string>
#include <vector>

using namespace dse::core;

/// 测试用 Mock 模块
class MockModule : public IModule {
public:
    explicit MockModule(const std::string& name) : name_(name) {}

    const char* GetName() const override { return name_.c_str(); }

    bool OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) override {
        init_called = true;
        init_world = &world;
        init_rhi = rhi_device;
        init_asset_mgr = asset_manager;
        return init_return_value;
    }

    void OnUpdate(World& world, float delta_time) override {
        update_called = true;
        update_count++;
        last_delta = delta_time;
    }

    void OnFixedUpdate(World& world, float fixed_delta_time) override {
        fixed_update_called = true;
        fixed_update_count++;
        last_fixed_delta = fixed_delta_time;
    }

    void OnShutdown(World& world) override {
        shutdown_called = true;
    }

    // 状态记录
    bool init_called = false;
    bool init_return_value = true;
    World* init_world = nullptr;
    RhiDevice* init_rhi = nullptr;
    AssetManager* init_asset_mgr = nullptr;

    bool update_called = false;
    int update_count = 0;
    float last_delta = 0.0f;

    bool fixed_update_called = false;
    int fixed_update_count = 0;
    float last_fixed_delta = 0.0f;

    bool shutdown_called = false;

private:
    std::string name_;
};

// ============================================================
// 生命周期
// ============================================================

// 测试 模块：开启Initis被调用且接收参数
TEST(ModuleTest, OnInitisCalledAndReceivesParameters) {
    MockModule mod("TestModule");
    World world;

    EXPECT_TRUE(mod.OnInit(world, nullptr, nullptr));
    EXPECT_TRUE(mod.init_called);
    EXPECT_EQ(mod.init_world, &world);
    EXPECT_EQ(mod.init_rhi, nullptr);
    EXPECT_EQ(mod.init_asset_mgr, nullptr);
}

// 测试 模块：开启Updateis被调用且Passeddelta时间
TEST(ModuleTest, OnUpdateisCalledAndPasseddelta_time) {
    MockModule mod("TestModule");
    World world;
    mod.OnInit(world, nullptr, nullptr);

    mod.OnUpdate(world, 0.016f);
    EXPECT_TRUE(mod.update_called);
    EXPECT_EQ(mod.update_count, 1);
    EXPECT_FLOAT_EQ(mod.last_delta, 0.016f);
}

// 测试 模块：开启固定Updateis被调用且Passedfixed增量
TEST(ModuleTest, OnFixedUpdateisCalledAndPassedfixed_delta) {
    MockModule mod("TestModule");
    World world;

    mod.OnFixedUpdate(world, 0.02f);
    EXPECT_TRUE(mod.fixed_update_called);
    EXPECT_EQ(mod.fixed_update_count, 1);
    EXPECT_FLOAT_EQ(mod.last_fixed_delta, 0.02f);
}

// 测试 模块：开启Shutdowncalled
TEST(ModuleTest, OnShutdowncalled) {
    MockModule mod("TestModule");
    World world;
    mod.OnInit(world, nullptr, nullptr);

    mod.OnShutdown(world);
    EXPECT_TRUE(mod.shutdown_called);
}

// 测试 模块：获取名称返回模块名称
TEST(ModuleTest, GetNameReturnModuleName) {
    MockModule mod("Gameplay2D");
    EXPECT_STREQ(mod.GetName(), "Gameplay2D");
}

// ============================================================
// 渲染扩展点：RegisterRenderPasses
// ============================================================

namespace {

/// 测试用 Mock 渲染 Pass，记录是否被 Setup
class MockRenderPass : public dse::render::IRenderPass {
public:
    void Setup(dse::render::RenderGraph& graph) override {
        setup_called = true;
        graph.AddPass(GetName());
    }
    void Execute(dse::render::CommandBuffer& /*cmd_buffer*/) override { execute_called = true; }
    const char* GetName() const override { return "MockRenderPass"; }

    bool setup_called = false;
    bool execute_called = false;
};

/// 仅通过 RegisterRenderPasses 参与渲染的模块
class RenderRegisteringModule : public IModule {
public:
    const char* GetName() const override { return "RenderRegistering"; }
    bool OnInit(World&, RhiDevice*, AssetManager*) override { return true; }
    void OnUpdate(World&, float) override {}
    void OnFixedUpdate(World&, float) override {}
    void OnShutdown(World&) override {}

    void RegisterRenderPasses(
        dse::render::RenderGraph& graph,
        dse::render::RenderPassContext& ctx,
        std::vector<std::unique_ptr<dse::render::IRenderPass>>& out_passes) override {
        (void)graph;
        (void)ctx;
        out_passes.push_back(std::make_unique<MockRenderPass>());
    }
};

} // namespace

// 测试 模块：基类 RegisterRenderPasses 默认不注册任何 Pass
TEST(ModuleTest, RegisterRenderPassesDefaultRegistersNothing) {
    MockModule mod("TestModule");
    dse::render::RenderGraph graph;
    dse::render::RenderPassContext ctx;
    std::vector<std::unique_ptr<dse::render::IRenderPass>> passes;

    mod.RegisterRenderPasses(graph, ctx, passes);
    EXPECT_TRUE(passes.empty());
}

// 测试 模块：模块通过 RegisterRenderPasses 注册的 Pass 被收集且可 Setup
TEST(ModuleTest, RegisterRenderPassesCollectsAndSetsUpPasses) {
    RenderRegisteringModule mod;
    dse::render::RenderGraph graph;
    dse::render::RenderPassContext ctx;
    std::vector<std::unique_ptr<dse::render::IRenderPass>> passes;

    mod.RegisterRenderPasses(graph, ctx, passes);
    ASSERT_EQ(passes.size(), 1u);
    EXPECT_STREQ(passes[0]->GetName(), "MockRenderPass");

    // 模拟 FramePipeline 对收集到的 Pass 调用 Setup
    for (auto& pass : passes) {
        pass->Setup(graph);
    }
    auto* mock = dynamic_cast<MockRenderPass*>(passes[0].get());
    ASSERT_NE(mock, nullptr);
    EXPECT_TRUE(mock->setup_called);
    EXPECT_TRUE(graph.Compile());
}

// ============================================================
// 多模块场景
// ============================================================

// 测试 模块：Multimodule独立生命周期
TEST(ModuleTest, MultimoduleIndependentLifecycle) {
    auto mod_a = std::make_unique<MockModule>("ModuleA");
    auto mod_b = std::make_unique<MockModule>("ModuleB");
    World world;

    mod_a->OnInit(world, nullptr, nullptr);
    mod_b->OnInit(world, nullptr, nullptr);

    mod_a->OnUpdate(world, 0.016f);
    mod_b->OnFixedUpdate(world, 0.02f);

    EXPECT_TRUE(mod_a->update_called);
    EXPECT_FALSE(mod_a->fixed_update_called);
    EXPECT_FALSE(mod_b->update_called);
    EXPECT_TRUE(mod_b->fixed_update_called);

    mod_a->OnShutdown(world);
    mod_b->OnShutdown(world);
    EXPECT_TRUE(mod_a->shutdown_called);
    EXPECT_TRUE(mod_b->shutdown_called);
}

// 测试 模块：多次数更新调用正确
TEST(ModuleTest, MultiTimesUpdateCallsCorrect) {
    MockModule mod("Counter");
    World world;

    for (int i = 0; i < 10; ++i) {
        mod.OnUpdate(world, static_cast<float>(i) * 0.01f);
    }
    EXPECT_EQ(mod.update_count, 10);

    for (int i = 0; i < 5; ++i) {
        mod.OnFixedUpdate(world, 0.02f);
    }
    EXPECT_EQ(mod.fixed_update_count, 5);
}

// ============================================================
// 多态使用
// ============================================================

// 测试 模块：通道调用多
TEST(ModuleTest, PassCallsMulti) {
    std::unique_ptr<IModule> mod = std::make_unique<MockModule>("PolyModule");
    World world;

    EXPECT_STREQ(mod->GetName(), "PolyModule");
    EXPECT_TRUE(mod->OnInit(world, nullptr, nullptr));
    EXPECT_NO_THROW(mod->OnUpdate(world, 0.016f));
    EXPECT_NO_THROW(mod->OnFixedUpdate(world, 0.02f));
    EXPECT_NO_THROW(mod->OnShutdown(world));
}
