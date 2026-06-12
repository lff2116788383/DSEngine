/**
 * @file dynamic_library_test.cpp
 * @brief DynamicLibrary 动态库加载器单元测试
 *
 * 覆盖场景：
 * - 默认构造 IsLoaded 为 false
 * - 加载不存在的库返回 false
 * - Unload 空库不崩溃
 * - 移动语义
 * - GetSymbol 在未加载库上返回 nullptr
 * - 加载系统库并获取已知符号（Windows: kernel32.dll）
 */

#include <gtest/gtest.h>
#include "engine/core/dynamic_library.h"

using namespace dse::core;

// ============================================================
// 构造与基本状态
// ============================================================

TEST(DynamicLibraryTest, DefaultWhenNotLoad) {
    DynamicLibrary lib;
    EXPECT_FALSE(lib.IsLoaded());
}

TEST(DynamicLibraryTest, UnloadEmptyLibraryDoesNotCrash) {
    DynamicLibrary lib;
    EXPECT_NO_THROW(lib.Unload());
}

TEST(DynamicLibraryTest, GetSymbolReturnWhenNotLoadednullptr) {
    DynamicLibrary lib;
    EXPECT_EQ(lib.GetSymbol("SomeSymbol"), nullptr);
}

// ============================================================
// 加载不存在的库
// ============================================================

TEST(DynamicLibraryTest, LoaddoesNotExistReturnsfalse) {
    DynamicLibrary lib;
    bool result = lib.Load("nonexistent_library_that_does_not_exist_12345");
    EXPECT_FALSE(result);
    EXPECT_FALSE(lib.IsLoaded());
}

// ============================================================
// 移动语义
// ============================================================

TEST(DynamicLibraryTest, With) {
    DynamicLibrary lib1;
    DynamicLibrary lib2(std::move(lib1));
    // 移动后 lib1 不应持有资源
    EXPECT_FALSE(lib1.IsLoaded());
    EXPECT_FALSE(lib2.IsLoaded()); // lib1 本来就没加载
}

TEST(DynamicLibraryTest, With_2) {
    DynamicLibrary lib1;
    DynamicLibrary lib2;
    lib2 = std::move(lib1);
    EXPECT_FALSE(lib1.IsLoaded());
    EXPECT_FALSE(lib2.IsLoaded());
}

// ============================================================
// 加载系统库（Windows 特有）
// ============================================================

#if defined(_WIN32)

TEST(DynamicLibraryTest, LoadSystemkernel32) {
    DynamicLibrary lib;
    bool result = lib.Load("kernel32.dll");
    EXPECT_TRUE(result);
    EXPECT_TRUE(lib.IsLoaded());

    // GetProcAddress 是 kernel32 导出的函数
    void* sym = lib.GetSymbol("GetProcAddress");
    EXPECT_NE(sym, nullptr);

    // 不存在的符号返回 nullptr
    void* bad_sym = lib.GetSymbol("ThisSymbolDoesNotExist_12345");
    EXPECT_EQ(bad_sym, nullptr);

    lib.Unload();
    EXPECT_FALSE(lib.IsLoaded());
}

TEST(DynamicLibraryTest, LoadAfterUnloadCanLoad) {
    DynamicLibrary lib;
    ASSERT_TRUE(lib.Load("kernel32.dll"));
    EXPECT_TRUE(lib.IsLoaded());

    lib.Unload();
    EXPECT_FALSE(lib.IsLoaded());

    ASSERT_TRUE(lib.Load("kernel32.dll"));
    EXPECT_TRUE(lib.IsLoaded());

    lib.Unload();
}

TEST(DynamicLibraryTest, AlreadyLoad) {
    DynamicLibrary lib1;
    ASSERT_TRUE(lib1.Load("kernel32.dll"));
    EXPECT_TRUE(lib1.IsLoaded());

    DynamicLibrary lib2(std::move(lib1));
    EXPECT_FALSE(lib1.IsLoaded());
    EXPECT_TRUE(lib2.IsLoaded());

    // lib2 应能获取符号
    EXPECT_NE(lib2.GetSymbol("GetProcAddress"), nullptr);

    lib2.Unload();
}

#endif // _WIN32
