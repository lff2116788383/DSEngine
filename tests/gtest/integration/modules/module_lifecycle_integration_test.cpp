/**
 * @file module_lifecycle_integration_test.cpp
 * @brief IModule 生命周期集成测试
 *
 * 验证场景：
 * - 自定义 IModule 子类的 OnInit/OnUpdate/OnFixedUpdate/OnShutdown 完整生命周期
 * - 模块与 World 的交互：模块内创建实体并访问组件
 * - 多模块并存时互不干扰
 * - 模块 OnShutdown 后资源正确释放
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include "engine/core/module.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include <vector>
#include <string>

using namespace dse::core;

// ============================================================
// 测试用模块实现
// ============================================================

/// 记录生命周期调用的测试模块
class TestLifecycleModule : public IModule {
public:
    std::vector<std::string> call_log;
    int init_count = 0;
    int update_count = 0;
    int fixed_update_count = 0;
    int shutdown_count = 0;
    Entity created_entity = entt::null;

    const char* GetName() const override { return "TestLifecycle"; }

    bool OnInit(World& world, RhiDevice*, AssetManager*) override {
        call_log.push_back("OnInit");
        ++init_count;
        // 在模块初始化时创建实体
        created_entity = world.CreateEntity();
        auto& transform = world.registry().emplace<TransformComponent>(created_entity);
        transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
        transform.dirty = true;
        return true;
    }

    void OnUpdate(World& world, float delta_time) override {
        call_log.push_back("OnUpdate");
        ++update_count;
        // 每帧移动实体
        if (world.registry().valid(created_entity) &&
            world.registry().all_of<TransformComponent>(created_entity)) {
            auto& transform = world.registry().get<TransformComponent>(created_entity);
            transform.position.x += delta_time;
            transform.dirty = true;
        }
    }

    void OnFixedUpdate(World& world, float fixed_delta_time) override {
        call_log.push_back("OnFixedUpdate");
        ++fixed_update_count;
        (void)world;
        (void)fixed_delta_time;
    }

    void OnShutdown(World& world) override {
        call_log.push_back("OnShutdown");
        ++shutdown_count;
        // 清理模块创建的实体
        if (world.registry().valid(created_entity)) {
            world.DestroyEntity(created_entity);
        }
        created_entity = entt::null;
    }
};

/// 第二个测试模块，验证多模块并存
class SecondTestModule : public IModule {
public:
    int init_count = 0;
    int shutdown_count = 0;

    const char* GetName() const override { return "SecondTest"; }

    bool OnInit(World& world, RhiDevice*, AssetManager*) override {
        ++init_count;
        (void)world;
        return true;
    }

    void OnUpdate(World& world, float delta_time) override {
        (void)world;
        (void)delta_time;
    }

    void OnFixedUpdate(World& world, float fixed_delta_time) override {
        (void)world;
        (void)fixed_delta_time;
    }

    void OnShutdown(World& world) override {
        ++shutdown_count;
        (void)world;
    }
};

// ============================================================
// 模块生命周期集成测试
// ============================================================

class ModuleLifecycleIntegrationTest : public ::testing::Test {
protected:
    World world;
    std::unique_ptr<TestLifecycleModule> module;

    void SetUp() override {
        module = std::make_unique<TestLifecycleModule>();
    }

    void TearDown() override {
        if (module && module->shutdown_count == 0) {
            module->OnShutdown(world);
        }
    }
};

TEST_F(ModuleLifecycleIntegrationTest, LifecycleByCalls) {
    // Init → Update → FixedUpdate → Shutdown
    ASSERT_TRUE(module->OnInit(world, nullptr, nullptr));
    module->OnUpdate(world, 0.016f);
    module->OnFixedUpdate(world, 0.02f);
    module->OnShutdown(world);

    ASSERT_EQ(module->call_log.size(), 4u);
    EXPECT_EQ(module->call_log[0], "OnInit");
    EXPECT_EQ(module->call_log[1], "OnUpdate");
    EXPECT_EQ(module->call_log[2], "OnFixedUpdate");
    EXPECT_EQ(module->call_log[3], "OnShutdown");
}

TEST_F(ModuleLifecycleIntegrationTest, ModuleInitializeWhenCreateEntity) {
    ASSERT_TRUE(module->OnInit(world, nullptr, nullptr));

    // 模块应创建了实体
    EXPECT_TRUE(world.registry().valid(module->created_entity));
    EXPECT_EQ(world.EntityCount(), 1u);

    // 实体应拥有 TransformComponent
    EXPECT_TRUE(world.registry().all_of<TransformComponent>(module->created_entity));
    auto& transform = world.registry().get<TransformComponent>(module->created_entity);
    EXPECT_FLOAT_EQ(transform.position.x, 1.0f);
    EXPECT_FLOAT_EQ(transform.position.y, 2.0f);
    EXPECT_FLOAT_EQ(transform.position.z, 3.0f);
}

TEST_F(ModuleLifecycleIntegrationTest, ModuleUpdatedriveEntity) {
    ASSERT_TRUE(module->OnInit(world, nullptr, nullptr));

    // 模拟 3 帧更新
    for (int i = 0; i < 3; ++i) {
        module->OnUpdate(world, 1.0f);
    }

    EXPECT_EQ(module->update_count, 3);

    auto& transform = world.registry().get<TransformComponent>(module->created_entity);
    // 初始 x=1.0，每帧 +=1.0，3 帧后 x=4.0
    EXPECT_FLOAT_EQ(transform.position.x, 4.0f);
}

TEST_F(ModuleLifecycleIntegrationTest, ModuleShutdownWhenCleanupCreateEntity) {
    ASSERT_TRUE(module->OnInit(world, nullptr, nullptr));
    EXPECT_EQ(world.EntityCount(), 1u);

    module->OnShutdown(world);
    EXPECT_EQ(world.EntityCount(), 0u);
    EXPECT_TRUE(module->created_entity == entt::null);
}

TEST_F(ModuleLifecycleIntegrationTest, MultiTimesInitAndShutdownDoesNotCrash) {
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(module->OnInit(world, nullptr, nullptr));
        module->OnUpdate(world, 0.016f);
        module->OnFixedUpdate(world, 0.02f);
        module->OnShutdown(world);
    }

    EXPECT_EQ(module->init_count, 3);
    EXPECT_EQ(module->shutdown_count, 3);
    EXPECT_EQ(world.EntityCount(), 0u);
}

TEST_F(ModuleLifecycleIntegrationTest, MultimoduleNot) {
    auto module2 = std::make_unique<SecondTestModule>();

    ASSERT_TRUE(module->OnInit(world, nullptr, nullptr));
    ASSERT_TRUE(module2->OnInit(world, nullptr, nullptr));

    module->OnUpdate(world, 0.016f);
    module2->OnUpdate(world, 0.016f);

    module->OnFixedUpdate(world, 0.02f);
    module2->OnFixedUpdate(world, 0.02f);

    EXPECT_EQ(module->init_count, 1);
    EXPECT_EQ(module2->init_count, 1);
    EXPECT_EQ(module->update_count, 1);
    EXPECT_EQ(module->shutdown_count, 0);
    EXPECT_EQ(module2->shutdown_count, 0);

    // 世界中应有模块1创建的1个实体
    EXPECT_EQ(world.EntityCount(), 1u);

    module->OnShutdown(world);
    module2->OnShutdown(world);

    EXPECT_EQ(world.EntityCount(), 0u);
}

TEST_F(ModuleLifecycleIntegrationTest, ModuleAcquireCorrect) {
    EXPECT_STREQ(module->GetName(), "TestLifecycle");

    auto module2 = std::make_unique<SecondTestModule>();
    EXPECT_STREQ(module2->GetName(), "SecondTest");
}
