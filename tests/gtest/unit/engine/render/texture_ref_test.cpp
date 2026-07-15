/**
 * @file texture_ref_test.cpp
 * @brief TextureRef / TextureRefRegistry 引用计数与生命周期单元测试（无需 GPU）。
 *
 * 覆盖场景：
 * - 未托管句柄不进入计数（Find 返回空，RefCount 恒为 0）
 * - 拷贝 +1 / 析构 -1 / 移动不重复计数
 * - 拷贝赋值、从句柄隐式赋值会释放旧引用、保留新引用
 * - 注销后仍持有的 TextureRef 不悬挂（控制块 shared 存活），析构安全
 * - 句柄 id 复用：旧 TextureRef 指向旧控制块，新计数互不干扰
 * - 隐式转换 / 比较运算符语义
 */

#include <gtest/gtest.h>

#include "engine/render/rhi/texture_ref.h"

using dse::render::TextureHandle;
using dse::render::TextureRef;
using dse::render::TextureRefRegistry;

namespace {

// 每个测试使用独立 id 段，避免进程级单例注册表跨用例串扰。
TextureHandle H(uint32_t id) { return TextureHandle::from_raw(id); }

}  // namespace

// 未 Register 的句柄：TextureRef 不计数，不被视为托管。
TEST(TextureRefTest, UnmanagedHandleIsNotCounted) {
    auto& reg = TextureRefRegistry::Instance();
    const uint32_t id = 91001;
    EXPECT_FALSE(reg.IsManaged(id));

    TextureRef ref(H(id));
    EXPECT_EQ(ref.raw(), id);
    EXPECT_TRUE(static_cast<bool>(ref));
    EXPECT_EQ(reg.RefCount(id), 0);  // 未托管 → 恒为 0
    EXPECT_FALSE(reg.IsManaged(id));
}

// 空句柄：bool 为 false，raw 为 0，不计数。
TEST(TextureRefTest, DefaultIsEmpty) {
    TextureRef ref;
    EXPECT_FALSE(static_cast<bool>(ref));
    EXPECT_EQ(ref.raw(), 0u);
    EXPECT_EQ(ref.handle(), TextureHandle{});
}

// Register 后：构造 +1，拷贝 +1，析构各 -1。
TEST(TextureRefTest, CopyIncrementsAndDestroyDecrements) {
    auto& reg = TextureRefRegistry::Instance();
    const uint32_t id = 91002;
    reg.Register(id);
    EXPECT_EQ(reg.RefCount(id), 0);

    {
        TextureRef a(H(id));
        EXPECT_EQ(reg.RefCount(id), 1);
        {
            TextureRef b = a;  // 拷贝
            EXPECT_EQ(reg.RefCount(id), 2);
            TextureRef c(a);   // 再拷贝
            EXPECT_EQ(reg.RefCount(id), 3);
        }
        EXPECT_EQ(reg.RefCount(id), 1);  // b、c 析构
    }
    EXPECT_EQ(reg.RefCount(id), 0);  // a 析构
    reg.Unregister(id);
}

// 移动不重复计数：移动后源清空、计数不变。
TEST(TextureRefTest, MoveTransfersWithoutDoubleCount) {
    auto& reg = TextureRefRegistry::Instance();
    const uint32_t id = 91003;
    reg.Register(id);

    TextureRef a(H(id));
    EXPECT_EQ(reg.RefCount(id), 1);

    TextureRef b(std::move(a));
    EXPECT_EQ(reg.RefCount(id), 1);       // 计数不变
    EXPECT_FALSE(static_cast<bool>(a));   // 源被清空
    EXPECT_EQ(b.raw(), id);

    TextureRef c;
    c = std::move(b);                     // 移动赋值
    EXPECT_EQ(reg.RefCount(id), 1);
    EXPECT_FALSE(static_cast<bool>(b));
    EXPECT_EQ(c.raw(), id);

    reg.Unregister(id);
}

// 拷贝赋值：释放旧引用、保留新引用。
TEST(TextureRefTest, CopyAssignReleasesOldRef) {
    auto& reg = TextureRefRegistry::Instance();
    const uint32_t id1 = 91004;
    const uint32_t id2 = 91005;
    reg.Register(id1);
    reg.Register(id2);

    TextureRef a(H(id1));
    TextureRef b(H(id2));
    EXPECT_EQ(reg.RefCount(id1), 1);
    EXPECT_EQ(reg.RefCount(id2), 1);

    a = b;  // a 释放 id1、改持 id2
    EXPECT_EQ(reg.RefCount(id1), 0);
    EXPECT_EQ(reg.RefCount(id2), 2);

    reg.Unregister(id1);
    reg.Unregister(id2);
}

// 从裸句柄隐式赋值：同样释放旧引用、按新句柄登记。
TEST(TextureRefTest, AssignFromHandleReleasesOldRef) {
    auto& reg = TextureRefRegistry::Instance();
    const uint32_t id1 = 91006;
    const uint32_t id2 = 91007;
    reg.Register(id1);
    reg.Register(id2);

    TextureRef a(H(id1));
    EXPECT_EQ(reg.RefCount(id1), 1);

    a = H(id2);  // 隐式转换构造 + 移动赋值
    EXPECT_EQ(reg.RefCount(id1), 0);
    EXPECT_EQ(reg.RefCount(id2), 1);

    a = TextureHandle{};  // 清空
    EXPECT_EQ(reg.RefCount(id2), 0);
    EXPECT_FALSE(static_cast<bool>(a));

    reg.Unregister(id1);
    reg.Unregister(id2);
}

// 注销后仍持有的 TextureRef 不悬挂：控制块由 shared_ptr 存活至最后一个引用析构。
TEST(TextureRefTest, UnregisterWhileReferencedIsSafe) {
    auto& reg = TextureRefRegistry::Instance();
    const uint32_t id = 91008;
    reg.Register(id);

    TextureRef a(H(id));
    EXPECT_EQ(reg.RefCount(id), 1);

    reg.Unregister(id);                 // 表中移除，但 a 仍持控制块
    EXPECT_EQ(reg.RefCount(id), 0);     // 已注销 → 查询返回 0
    EXPECT_FALSE(reg.IsManaged(id));

    // a 的拷贝/析构必须安全（不得访问已释放内存）。
    {
        TextureRef b = a;
        EXPECT_EQ(b.raw(), id);
    }
    // 到此若控制块已悬挂，析构 b / a 会崩溃；能正常结束即验证安全。
    SUCCEED();
}

// 句柄 id 复用：注销并重新登记同一 id 会得到新控制块，旧 TextureRef 只影响旧块。
TEST(TextureRefTest, HandleIdReuseUsesFreshControlBlock) {
    auto& reg = TextureRefRegistry::Instance();
    const uint32_t id = 91009;
    reg.Register(id);

    TextureRef stale(H(id));            // 绑定到第一代控制块
    EXPECT_EQ(reg.RefCount(id), 1);

    reg.Unregister(id);
    reg.Register(id);                   // 新一代控制块，计数从 0 起
    EXPECT_EQ(reg.RefCount(id), 0);

    TextureRef fresh(H(id));            // 绑定到新一代控制块
    EXPECT_EQ(reg.RefCount(id), 1);     // stale 不计入新块

    reg.Unregister(id);
}

// 隐式转换与比较运算符。
TEST(TextureRefTest, ConversionAndComparison) {
    const uint32_t id = 91010;
    TextureRef a(H(id));
    TextureRef b(H(id));
    TextureRef c(H(id + 1));

    // 隐式转换为 TextureHandle
    TextureHandle h = a;
    EXPECT_EQ(h.raw(), id);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);

    // 与裸句柄比较（成员与自由函数两向）
    EXPECT_TRUE(a == H(id));
    EXPECT_TRUE(H(id) == a);
    EXPECT_TRUE(a != H(id + 5));
    EXPECT_TRUE(H(id + 5) != a);
}
