/**
 * @file platform_app_test.cpp
 * @brief Platform 抽象层单元测试
 */

#include "gtest/gtest.h"
#include "engine/platform/platform_app.h"

using namespace dse::platform;

// ============================================================
// 生命周期测试
// ============================================================

TEST(PlatformAppTest, 无图形模式Init成功) {
    auto app = CreateDefaultPlatformApp();
    ASSERT_NE(app, nullptr);

    WindowConfig config;
    config.width = 400;
    config.height = 300;
    config.title = "Test Window";
    config.no_graphics_api = true;  // 不创建 GL context，避免 GPU 依赖

    bool success = app->Init(config);
    EXPECT_TRUE(success) << "无图形模式应成功初始化";

    if (success) {
        app->Shutdown();
    }
}

TEST(PlatformAppTest, 重复Shutdown不崩溃) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (app->Init(config)) {
        app->Shutdown();
        app->Shutdown();  // 重复调用应安全
        SUCCEED();
    } else {
        // Init 失败时 Shutdown 也应安全
        app->Shutdown();
        SUCCEED();
    }
}

TEST(PlatformAppTest, Init后默认ShouldClose返回false) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    EXPECT_FALSE(app->ShouldClose()) << "Init 后窗口应未请求关闭";

    app->Shutdown();
}

// ============================================================
// 窗口信息测试
// ============================================================

TEST(PlatformAppTest, GetFramebufferSize返回配置尺寸) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.width = 640;
    config.height = 480;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    int w, h;
    app->GetFramebufferSize(w, h);
    EXPECT_EQ(w, 640);
    EXPECT_EQ(h, 480);

    app->Shutdown();
}

TEST(PlatformAppTest, SetWindowTitle修改标题) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    app->SetWindowTitle("New Title");
    // 无法直接验证标题，但不崩溃即通过
    SUCCEED();

    app->Shutdown();
}

TEST(PlatformAppTest, RequestClose使ShouldClose返回true) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    EXPECT_FALSE(app->ShouldClose());
    app->RequestClose();
    EXPECT_TRUE(app->ShouldClose());

    app->Shutdown();
}

// ============================================================
// 平台桥接测试
// ============================================================

TEST(PlatformAppTest, 无图形模式HasGLContext返回false) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    EXPECT_FALSE(app->HasGLContext());

    app->Shutdown();
}

TEST(PlatformAppTest, GetNativeWindowHandle返回非空) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    void* handle = app->GetNativeWindowHandle();
    EXPECT_NE(handle, nullptr);

    app->Shutdown();
}

// ============================================================
// 输入回调测试
// ============================================================

TEST(PlatformAppTest, 设置输入回调不崩溃) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    static int key_called = 0, mouse_called = 0, scroll_called = 0, cursor_called = 0;
    key_called = mouse_called = scroll_called = cursor_called = 0;

    app->SetInputCallbacks(
        [](int, int) { key_called++; },
        [](int, int) { mouse_called++; },
        [](float) { scroll_called++; },
        [](float, float) { cursor_called++; }
    );

    // 实际触发回调需要事件泵送，这里只验证设置不崩溃
    SUCCEED();

    app->Shutdown();
}

// ============================================================
// 时间函数测试
// ============================================================

TEST(PlatformAppTest, GetTime返回非负值) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    double time = app->GetTime();
    EXPECT_GE(time, 0.0);

    app->Shutdown();
}

// ============================================================
// 主循环驱动测试
// ============================================================

TEST(PlatformAppTest, PollEvents不崩溃) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    app->PollEvents();  // 空事件队列
    SUCCEED();

    app->Shutdown();
}

TEST(PlatformAppTest, SwapBuffers无图形模式不崩溃) {
    auto app = CreateDefaultPlatformApp();
    WindowConfig config;
    config.no_graphics_api = true;

    if (!app->Init(config)) {
        GTEST_SKIP() << "Init 失败，跳过测试";
    }

    app->SwapBuffers();  // 无 GL context 应为 no-op
    SUCCEED();

    app->Shutdown();
}
