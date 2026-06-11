/**
 * @file service_locator_test.cpp
 * @brief ServiceLocator 的单元测试
 *
 * 覆盖场景：
 * - 服务注册和获取
 * - 未注册服务返回 nullptr
 * - 服务替换
 * - 服务重置
 * - 全部重置
 * - 接口/实现分离注册
 * - 线程安全（基本验证）
 */

#include <gtest/gtest.h>
#include "engine/core/service_locator.h"
#include <string>
#include <thread>
#include <vector>
#include <atomic>

using namespace dse::core;

// ============================================================
// 测试用接口和实现
// ============================================================

class ITestService {
public:
    virtual ~ITestService() = default;
    virtual std::string Name() const = 0;
};

class ConcreteTestService : public ITestService {
public:
    explicit ConcreteTestService(std::string name) : name_(std::move(name)) {}
    std::string Name() const override { return name_; }
private:
    std::string name_;
};

class AnotherTestService {
public:
    explicit AnotherTestService(int value) : value_(value) {}
    int Value() const { return value_; }
private:
    int value_;
};

// ============================================================
// 测试夹具 - 确保每个测试后清理 ServiceLocator
// ============================================================

class ServiceLocatorTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().ResetAll();
    }
};

// ============================================================
// 注册与获取
// ============================================================

TEST_F(ServiceLocatorTest, RegisterAcquire) {
    auto service = std::make_shared<ConcreteTestService>("TestService");
    ServiceLocator::Instance().Register<ITestService, ConcreteTestService>(service);

    auto* retrieved = ServiceLocator::Instance().Get<ITestService>();
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->Name(), "TestService");
}

TEST_F(ServiceLocatorTest, NotregisterReturnsEmpty) {
    auto* retrieved = ServiceLocator::Instance().Get<ITestService>();
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(ServiceLocatorTest, Acquire) {
    auto service = std::make_shared<ConcreteTestService>("SharedTest");
    ServiceLocator::Instance().Register<ITestService, ConcreteTestService>(service);

    auto shared = ServiceLocator::Instance().GetShared<ITestService>();
    ASSERT_NE(shared, nullptr);
    EXPECT_EQ(shared->Name(), "SharedTest");
}

TEST_F(ServiceLocatorTest, NotregisterReturnsEmpty_2) {
    auto shared = ServiceLocator::Instance().GetShared<ITestService>();
    EXPECT_EQ(shared, nullptr);
}

// ============================================================
// Emplace 构造
// ============================================================

TEST_F(ServiceLocatorTest, EmplaceConstructTheServiceDirectly) {
    ServiceLocator::Instance().Emplace<AnotherTestService, AnotherTestService>(42);

    auto* service = ServiceLocator::Instance().Get<AnotherTestService>();
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->Value(), 42);
}

TEST_F(ServiceLocatorTest, EmplaceSeparationOfInterfaceAndImplementation) {
    ServiceLocator::Instance().Emplace<ITestService, ConcreteTestService>("EmplacedService");

    auto* service = ServiceLocator::Instance().Get<ITestService>();
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->Name(), "EmplacedService");
}

// ============================================================
// 服务替换
// ============================================================

TEST_F(ServiceLocatorTest, Register) {
    auto service1 = std::make_shared<ConcreteTestService>("V1");
    ServiceLocator::Instance().Register<ITestService, ConcreteTestService>(service1);
    EXPECT_EQ(ServiceLocator::Instance().Get<ITestService>()->Name(), "V1");

    auto service2 = std::make_shared<ConcreteTestService>("V2");
    ServiceLocator::Instance().Register<ITestService, ConcreteTestService>(service2);
    EXPECT_EQ(ServiceLocator::Instance().Get<ITestService>()->Name(), "V2");
}

// ============================================================
// 检查与重置
// ============================================================

TEST_F(ServiceLocatorTest, HasCheckIfTheServiceExists) {
    EXPECT_FALSE(ServiceLocator::Instance().Has<ITestService>());

    auto service = std::make_shared<ConcreteTestService>("CheckService");
    ServiceLocator::Instance().Register<ITestService, ConcreteTestService>(service);

    EXPECT_TRUE(ServiceLocator::Instance().Has<ITestService>());
}

TEST_F(ServiceLocatorTest, ResetRemoveASingleService) {
    auto service = std::make_shared<ConcreteTestService>("ToReset");
    ServiceLocator::Instance().Register<ITestService, ConcreteTestService>(service);
    EXPECT_TRUE(ServiceLocator::Instance().Has<ITestService>());

    ServiceLocator::Instance().Reset<ITestService>();
    EXPECT_FALSE(ServiceLocator::Instance().Has<ITestService>());
}

TEST_F(ServiceLocatorTest, ResetAllClearAllServices) {
    ServiceLocator::Instance().Emplace<ITestService, ConcreteTestService>("S1");
    ServiceLocator::Instance().Emplace<AnotherTestService, AnotherTestService>(99);

    EXPECT_TRUE(ServiceLocator::Instance().Has<ITestService>());
    EXPECT_TRUE(ServiceLocator::Instance().Has<AnotherTestService>());

    ServiceLocator::Instance().ResetAll();

    EXPECT_FALSE(ServiceLocator::Instance().Has<ITestService>());
    EXPECT_FALSE(ServiceLocator::Instance().Has<AnotherTestService>());
}

// ============================================================
// 多类型共存
// ============================================================

TEST_F(ServiceLocatorTest, MultiType) {
    ServiceLocator::Instance().Emplace<ITestService, ConcreteTestService>("MultiService");
    ServiceLocator::Instance().Emplace<AnotherTestService, AnotherTestService>(77);

    auto* test_svc = ServiceLocator::Instance().Get<ITestService>();
    auto* another_svc = ServiceLocator::Instance().Get<AnotherTestService>();

    ASSERT_NE(test_svc, nullptr);
    ASSERT_NE(another_svc, nullptr);
    EXPECT_EQ(test_svc->Name(), "MultiService");
    EXPECT_EQ(another_svc->Value(), 77);
}

// ============================================================
// 线程安全基本验证
// ============================================================

TEST_F(ServiceLocatorTest, RegisterAndAcquireDoesNotCrash) {
    constexpr int kThreadCount = 8;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&success_count, i]() {
            // 每个线程注册不同类型的服务
            auto service = std::make_shared<AnotherTestService>(i);
            ServiceLocator::Instance().Register<AnotherTestService, AnotherTestService>(service);
            auto* retrieved = ServiceLocator::Instance().Get<AnotherTestService>();
            if (retrieved != nullptr) {
                success_count.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 只要没崩溃就算通过
    EXPECT_GT(success_count.load(), 0);
}
