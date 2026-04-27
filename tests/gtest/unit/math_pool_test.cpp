/**
 * @file math_pool_test.cpp
 * @brief 数学工具与内存池/对象池的单元测试
 *
 * 覆盖场景：
 * - BezierCurve2D：二次/三次贝塞尔曲线插值
 * - Tween：缓动函数评估、线性插值
 * - MemoryPool：分配/回收/扩容/线程安全
 * - ObjectPool：Acquire/Release/自定义工厂/预分配
 */

#include <gtest/gtest.h>
#include "engine/base/bezier.h"
#include "engine/base/tween.h"
#include "engine/core/memory_pool.h"
#include "engine/core/object_pool.h"
#include <glm/glm.hpp>
#include <cmath>
#include <thread>
#include <vector>
#include <atomic>

// ============================================================
// BezierCurve2D 测试
// ============================================================

TEST(BezierCurve2DTest, 二次贝塞尔起点) {
    glm::vec2 p0(0.0f, 0.0f), p1(1.0f, 2.0f), p2(3.0f, 0.0f);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateQuadratic(0.0f, p0, p1, p2);
    EXPECT_NEAR(result.x, 0.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, 二次贝塞尔终点) {
    glm::vec2 p0(0.0f, 0.0f), p1(1.0f, 2.0f), p2(3.0f, 0.0f);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateQuadratic(1.0f, p0, p1, p2);
    EXPECT_NEAR(result.x, 3.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, 二次贝塞尔中点) {
    glm::vec2 p0(0.0f, 0.0f), p1(1.0f, 2.0f), p2(3.0f, 0.0f);
    // t=0.5: (1-0.5)^2*p0 + 2*(1-0.5)*0.5*p1 + 0.5^2*p2 = 0.25*p0 + 0.5*p1 + 0.25*p2
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateQuadratic(0.5f, p0, p1, p2);
    EXPECT_NEAR(result.x, 0.25f * 0.0f + 0.5f * 1.0f + 0.25f * 3.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.25f * 0.0f + 0.5f * 2.0f + 0.25f * 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, 二次贝塞尔线性退化) {
    // 控制点在起点终点的连线上，应退化为线性插值
    glm::vec2 p0(0.0f, 0.0f), p1(2.0f, 2.0f), p2(4.0f, 4.0f);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateQuadratic(0.5f, p0, p1, p2);
    EXPECT_NEAR(result.x, 2.0f, 1e-5f);
    EXPECT_NEAR(result.y, 2.0f, 1e-5f);
}

TEST(BezierCurve2DTest, 三次贝塞尔起点) {
    glm::vec2 p0(0, 0), p1(1, 3), p2(4, 3), p3(5, 0);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateCubic(0.0f, p0, p1, p2, p3);
    EXPECT_NEAR(result.x, 0.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, 三次贝塞尔终点) {
    glm::vec2 p0(0, 0), p1(1, 3), p2(4, 3), p3(5, 0);
    glm::vec2 result = dse::math::BezierCurve2D::EvaluateCubic(1.0f, p0, p1, p2, p3);
    EXPECT_NEAR(result.x, 5.0f, 1e-5f);
    EXPECT_NEAR(result.y, 0.0f, 1e-5f);
}

TEST(BezierCurve2DTest, 三次贝塞尔对称性) {
    // 反转控制点顺序并在 t→1-t 时应得到相同结果
    glm::vec2 p0(0, 0), p1(1, 3), p2(4, 3), p3(5, 0);
    float t = 0.3f;
    glm::vec2 forward = dse::math::BezierCurve2D::EvaluateCubic(t, p0, p1, p2, p3);
    glm::vec2 backward = dse::math::BezierCurve2D::EvaluateCubic(1.0f - t, p3, p2, p1, p0);
    EXPECT_NEAR(forward.x, backward.x, 1e-4f);
    EXPECT_NEAR(forward.y, backward.y, 1e-4f);
}

// ============================================================
// Tween 测试
// ============================================================

TEST(TweenTest, Linear缓动恒等) {
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, 0.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, 0.5f), 0.5f, 1e-5f);
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, 1.0f), 1.0f, 1e-5f);
}

TEST(TweenTest, EaseInQuad值正确) {
    float t = 0.5f;
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::EaseInQuad, t), t * t, 1e-5f);
}

TEST(TweenTest, EaseOutQuad值正确) {
    float t = 0.5f;
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::EaseOutQuad, t), t * (2.0f - t), 1e-5f);
}

TEST(TweenTest, EaseInOutQuad前半段等价于EaseIn) {
    float t = 0.3f;
    float io_val = dse::utils::Tween::Evaluate(dse::utils::EaseType::EaseInOutQuad, t);
    float expected = 2.0f * t * t;
    EXPECT_NEAR(io_val, expected, 1e-5f);
}

TEST(TweenTest, EaseInOutQuad后半段等价于EaseOut变形) {
    float t = 0.7f;
    float io_val = dse::utils::Tween::Evaluate(dse::utils::EaseType::EaseInOutQuad, t);
    float expected = -1.0f + (4.0f - 2.0f * t) * t;
    EXPECT_NEAR(io_val, expected, 1e-5f);
}

TEST(TweenTest, 超出范围的t被Clamp) {
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, -1.0f), 0.0f, 1e-5f);
    EXPECT_NEAR(dse::utils::Tween::Evaluate(dse::utils::EaseType::Linear, 2.0f), 1.0f, 1e-5f);
}

TEST(TweenTest, Lerp线性插值正确) {
    EXPECT_NEAR(dse::utils::Tween::Lerp(0.0f, 100.0f, 0.5f), 50.0f, 1e-5f);
    EXPECT_NEAR(dse::utils::Tween::Lerp(-10.0f, 10.0f, 0.5f), 0.0f, 1e-5f);
}

TEST(TweenTest, Lerp带缓动插值正确) {
    float result = dse::utils::Tween::Lerp(0.0f, 100.0f, 0.5f, dse::utils::EaseType::EaseInQuad);
    float eased_t = 0.5f * 0.5f; // EaseInQuad(0.5) = 0.25
    EXPECT_NEAR(result, 100.0f * eased_t, 1e-3f);
}

// ============================================================
// MemoryPool 测试
// ============================================================

TEST(MemoryPoolTest, 分配并初始化对象) {
    dse::core::MemoryPool<int> pool(10);
    int* ptr = pool.Allocate();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 0); // int 默认初始化为 0
}

TEST(MemoryPoolTest, 回收后可再次分配) {
    dse::core::MemoryPool<int> pool(2);
    int* ptr1 = pool.Allocate();
    *ptr1 = 42;
    pool.Free(ptr1);
    int* ptr2 = pool.Allocate();
    // 回收后的内存应可再次分配（值被 placement new 重置为默认值）
    EXPECT_EQ(*ptr2, 0);
}

TEST(MemoryPoolTest, 空池自动扩容) {
    dse::core::MemoryPool<int> pool(1);
    std::vector<int*> ptrs;
    // 分配超过初始容量
    for (int i = 0; i < 10; ++i) {
        ptrs.push_back(pool.Allocate());
    }
    for (int i = 0; i < 10; ++i) {
        ASSERT_NE(ptrs[i], nullptr);
    }
}

TEST(MemoryPoolTest, Free空指针不崩溃) {
    dse::core::MemoryPool<int> pool(1);
    pool.Free(nullptr);
    SUCCEED();
}

TEST(MemoryPoolTest, 多线程并发分配回收) {
    dse::core::MemoryPool<int> pool(100);
    std::atomic<int> alloc_count{0};
    std::atomic<int> free_count{0};

    const int thread_count = 4;
    const int ops_per_thread = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&pool, &alloc_count, &free_count, ops_per_thread]() {
            std::vector<int*> local_ptrs;
            for (int i = 0; i < ops_per_thread; ++i) {
                int* ptr = pool.Allocate();
                *ptr = i;
                local_ptrs.push_back(ptr);
                alloc_count.fetch_add(1);
            }
            for (auto* ptr : local_ptrs) {
                pool.Free(ptr);
                free_count.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(alloc_count.load(), thread_count * ops_per_thread);
    EXPECT_EQ(free_count.load(), thread_count * ops_per_thread);
}

// ============================================================
// ObjectPool 测试
// ============================================================

TEST(ObjectPoolTest, Acquire从预分配池获取) {
    dse::core::ObjectPool<int> pool(5);
    EXPECT_EQ(pool.AvailableCount(), 5u);
    int val = pool.Acquire();
    EXPECT_EQ(pool.AvailableCount(), 4u);
}

TEST(ObjectPoolTest, Release归还后可用) {
    dse::core::ObjectPool<int> pool(1);
    pool.Acquire();
    EXPECT_EQ(pool.AvailableCount(), 0u);
    pool.Release(0);
    EXPECT_EQ(pool.AvailableCount(), 1u);
}

TEST(ObjectPoolTest, 空池Acquire动态创建) {
    dse::core::ObjectPool<int> pool(0);
    EXPECT_EQ(pool.AvailableCount(), 0u);
    int val = pool.Acquire();
    EXPECT_EQ(val, 0); // 默认构造的 int 值为 0
}

TEST(ObjectPoolTest, 自定义工厂函数) {
    int counter = 100;
    dse::core::ObjectPool<int> pool(3, [&counter]() { return counter++; });
    EXPECT_EQ(pool.AvailableCount(), 3u);

    int val1 = pool.Acquire();
    int val2 = pool.Acquire();
    // 工厂函数在 Reserve 时调用，返回值应从 100 开始
    EXPECT_GE(val1, 100);
    EXPECT_GE(val2, 100);
}

TEST(ObjectPoolTest, Reserve扩容) {
    dse::core::ObjectPool<int> pool(0);
    pool.Reserve(10);
    EXPECT_EQ(pool.AvailableCount(), 10u);
}

TEST(ObjectPoolTest, 循环AcquireRelease不泄漏) {
    dse::core::ObjectPool<int> pool(5);
    for (int i = 0; i < 100; ++i) {
        int val = pool.Acquire();
        pool.Release(val);
    }
    EXPECT_EQ(pool.AvailableCount(), 5u);
}

TEST(ObjectPoolTest, 复杂类型对象池) {
    struct Vec3 {
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };
    dse::core::ObjectPool<Vec3> pool(2);
    Vec3 v = pool.Acquire();
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}
