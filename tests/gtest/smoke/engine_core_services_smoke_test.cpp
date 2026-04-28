/**
 * @file engine_core_services_smoke_test.cpp
 * @brief 引擎核心服务冒烟测试
 *
 * 验证场景：
 * - 核心服务（EventBus、World、JobSystem、ServiceLocator）协作初始化与关闭
 * - World 创建实体后，EventBus 事件可正常发布/订阅
 * - JobSystem 提交任务后可等待完成
 * - 完整 Init → Use → Shutdown 链路不崩溃
 *
 * 注意：不依赖 GLFW/OpenGL，可在无窗口 CI 环境下运行。
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/service_locator.h"
#include "engine/core/job_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include <atomic>
#include <string>

using namespace dse::core;

// ============================================================
// 测试用事件
// ============================================================

struct SmokeTestEvent : public Event {
    std::string message;
    explicit SmokeTestEvent(std::string msg) : message(std::move(msg)) {}
    static constexpr EventId kEventId = MakeEventId("SmokeTestEvent");
};

// ============================================================
// 引擎核心服务冒烟测试
// ============================================================

class EngineCoreServicesSmokeTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().Reset<EventBus>();
        ServiceLocator::Instance().Reset<JobSystem>();
    }
};

TEST_F(EngineCoreServicesSmokeTest, 核心服务完整生命周期不崩溃) {
    // Init
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    auto job_system = std::make_shared<JobSystem>();
    job_system->Init();
    ServiceLocator::Instance().Register<JobSystem, JobSystem>(job_system);

    World world;

    // Use - 创建实体
    Entity e = world.CreateEntity();
    EXPECT_TRUE(world.IsAlive(e));
    auto& transform = world.registry().emplace<TransformComponent>(e);
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    transform.dirty = true;

    // Use - 发布/订阅事件
    bool event_received = false;
    bus->Subscribe<SmokeTestEvent>([&event_received](const SmokeTestEvent& ev) {
        if (ev.message == "smoke") event_received = true;
    });
    bus->Publish<SmokeTestEvent>("smoke");
    EXPECT_TRUE(event_received);

    // Use - 提交 Job
    std::atomic<int> counter{0};
    auto handle = job_system->Submit([&counter]() {
        counter.fetch_add(1);
    }, JobPriority::Normal);
    job_system->Wait(handle);
    EXPECT_EQ(counter.load(), 1);

    // Shutdown
    world.DestroyEntity(e);
    EXPECT_FALSE(world.IsAlive(e));

    job_system->Shutdown();
    ServiceLocator::Instance().Reset<JobSystem>();
    ServiceLocator::Instance().Reset<EventBus>();
}

TEST_F(EngineCoreServicesSmokeTest, EventBus与World协作冒烟) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    World world;

    std::vector<Entity> created_entities;
    bus->Subscribe<SmokeTestEvent>([&](const SmokeTestEvent& ev) {
        if (ev.message == "create_entity") {
            Entity e = world.CreateEntity();
            created_entities.push_back(e);
        }
    });

    // 模拟：业务逻辑通过事件驱动创建实体
    bus->Publish<SmokeTestEvent>("create_entity");
    bus->Publish<SmokeTestEvent>("create_entity");

    EXPECT_EQ(created_entities.size(), 2u);
    EXPECT_EQ(world.EntityCount(), 2u);

    for (Entity e : created_entities) {
        world.DestroyEntity(e);
    }
    EXPECT_EQ(world.EntityCount(), 0u);
}

TEST_F(EngineCoreServicesSmokeTest, JobSystem多任务并发冒烟) {
    auto job_system = std::make_shared<JobSystem>();
    job_system->Init();
    ServiceLocator::Instance().Register<JobSystem, JobSystem>(job_system);

    std::atomic<int> counter{0};
    constexpr int kTaskCount = 100;

    std::vector<JobHandle> handles;
    handles.reserve(kTaskCount);
    for (int i = 0; i < kTaskCount; ++i) {
        handles.push_back(job_system->Submit([&counter]() {
            counter.fetch_add(1);
        }, JobPriority::Normal));
    }

    for (auto& h : handles) {
        job_system->Wait(h);
    }

    EXPECT_EQ(counter.load(), kTaskCount);

    job_system->Shutdown();
}

TEST_F(EngineCoreServicesSmokeTest, ServiceLocator注册获取重置不崩溃) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    auto* retrieved = ServiceLocator::Instance().Get<EventBus>();
    ASSERT_NE(retrieved, nullptr);

    ServiceLocator::Instance().Reset<EventBus>();
    EXPECT_EQ(ServiceLocator::Instance().Get<EventBus>(), nullptr);
}

TEST_F(EngineCoreServicesSmokeTest, World大量实体创建销毁不崩溃) {
    World world;
    std::vector<Entity> entities;

    // 创建 1000 个实体
    for (int i = 0; i < 1000; ++i) {
        Entity e = world.CreateEntity();
        world.registry().emplace<TransformComponent>(e);
        entities.push_back(e);
    }
    EXPECT_EQ(world.EntityCount(), 1000u);

    // 销毁所有实体
    for (Entity e : entities) {
        world.DestroyEntity(e);
    }
    EXPECT_EQ(world.EntityCount(), 0u);
}

TEST_F(EngineCoreServicesSmokeTest, 多次InitShutdown交替不崩溃) {
    for (int i = 0; i < 3; ++i) {
        auto job_system = std::make_shared<JobSystem>();
        job_system->Init();
        ServiceLocator::Instance().Register<JobSystem, JobSystem>(job_system);

        std::atomic<int> val{0};
        auto h = job_system->Submit([&val]() { val.fetch_add(1); }, JobPriority::Normal);
        job_system->Wait(h);
        EXPECT_EQ(val.load(), 1);

        job_system->Shutdown();
        ServiceLocator::Instance().Reset<JobSystem>();
    }
}
