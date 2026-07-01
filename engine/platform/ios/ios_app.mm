/**
 * @file ios_app.mm
 * @brief UIKitApp 实现 — UIApplicationDelegate + MTKView + MoltenVK surface
 *
 * iOS 平台的窗口/视图管理与 Vulkan surface 创建。
 * 引擎帧循环由上层（EngineInstance / FramePipeline）驱动，
 * MTKView 仅提供 CAMetalLayer，不使用其 draw 回调。
 */

#ifdef DSE_ENABLE_APPLE_PLATFORM

#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "engine/platform/ios/ios_app.h"
#include "engine/base/debug.h"

#ifdef DSE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_IOS_MVK
#include <vulkan/vulkan_ios.h>
#endif

// Forward declaration from ios_input.mm
@class DSETouchView;
extern void DSETouchView_SetCallback(UIView* view, dse::platform::PlatformApp::TouchCallback cb);

// =============================================================================
// DSEViewController — 持有 MTKView，管理视图生命周期
// =============================================================================

@interface DSEViewController : UIViewController
@property (nonatomic, strong) MTKView* metalView;
@property (nonatomic, strong) DSETouchView* touchView;
@end

@implementation DSEViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    // 创建 Metal 设备
    id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
    if (!metalDevice) {
        NSLog(@"[DSEngine] Metal is not supported on this device");
        return;
    }

    // 创建 MTKView，其 layer 默认为 CAMetalLayer
    self.metalView = [[MTKView alloc] initWithFrame:self.view.bounds device:metalDevice];
    self.metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                     UIViewAutoresizingFlexibleHeight;
    // 引擎自驱帧循环，不使用 MTKView 内置 draw 回调
    self.metalView.paused = YES;
    self.metalView.enableSetNeedsDisplay = NO;

    // CAMetalLayer 配置
    CAMetalLayer* metalLayer = (CAMetalLayer*)self.metalView.layer;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.framebufferOnly = YES;

    [self.view addSubview:self.metalView];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    // MTKView 通过 autoresizingMask 自动调整
    // 触摸视图同步调整
    if (self.touchView) {
        self.touchView.frame = self.view.bounds;
    }
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    return UIRectEdgeAll;
}

- (BOOL)prefersHomeIndicatorAutoHidden {
    return YES;
}

@end

// =============================================================================
// DSEAppDelegate — UIApplicationDelegate 实现
// =============================================================================

@interface DSEAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow* window;
@end

@implementation DSEAppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
    // 进入后台前暂停（可选：通知引擎暂停帧循环）
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
    // 从后台恢复
}

- (void)applicationWillTerminate:(UIApplication*)application {
    // 引擎关闭通知
}

@end

// =============================================================================
// UIKitApp 实现
// =============================================================================

namespace dse::platform {

UIKitApp::~UIKitApp() {
    Shutdown();
}

bool UIKitApp::Init(const WindowConfig& config) {
    if (initialized_) return true;

    @autoreleasepool {
        // 创建 UIWindow
        UIWindow* window = [[UIWindow alloc] initWithFrame:
            [[UIScreen mainScreen] bounds]];

        // 创建 DSEViewController
        DSEViewController* vc = [[DSEViewController alloc] init];
        window.rootViewController = vc;
        [window makeKeyAndVisible];

        // 保留引用（ARC bridge）
        ui_window_ = (__bridge_retained void*)window;
        view_controller_ = (__bridge void*)vc;

        // viewDidLoad 可能延迟执行，确保视图已加载
        [vc loadViewIfNeeded];
        metal_view_ = (__bridge void*)vc.metalView;

        if (!metal_view_) {
            DEBUG_LOG_ERROR("[UIKitApp] Failed to create MTKView (Metal not supported?)");
            return false;
        }
    }

    should_close_ = false;
    initialized_ = true;
    DEBUG_LOG_INFO("[UIKitApp] Initialized on iOS");
    return true;
}

void UIKitApp::Shutdown() {
    if (!initialized_) return;

    @autoreleasepool {
        touch_cb_ = nullptr;
        metal_view_ = nullptr;
        view_controller_ = nullptr;

        // 释放 UIWindow 的 retained 引用
        if (ui_window_) {
            CFRelease(ui_window_);
            ui_window_ = nullptr;
        }
    }

    should_close_ = false;
    initialized_ = false;
    DEBUG_LOG_INFO("[UIKitApp] Shutdown complete");
}

bool UIKitApp::ShouldClose() const {
    return should_close_;
}

void UIKitApp::PollEvents() {
    @autoreleasepool {
        // iOS 事件由 UIKit runloop 处理
        // 在引擎帧循环中手动处理一次 runloop iteration
        // 以确保触屏/系统事件被及时处理
        SInt32 result;
        do {
            result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, TRUE);
        } while (result == kCFRunLoopRunHandledSource);
    }
}

void UIKitApp::SwapBuffers() {
    // MoltenVK/Vulkan swapchain 处理 present，无需额外操作
}

double UIKitApp::GetTime() const {
    return CACurrentMediaTime();
}

void UIKitApp::GetFramebufferSize(int& w, int& h) const {
    if (!metal_view_) {
        w = 0;
        h = 0;
        return;
    }

    @autoreleasepool {
        MTKView* view = (__bridge MTKView*)metal_view_;
        CGSize drawable_size = view.drawableSize;
        w = static_cast<int>(drawable_size.width);
        h = static_cast<int>(drawable_size.height);
    }
}

void UIKitApp::SetWindowTitle(const std::string& /*title*/) {
    // iOS 全屏应用无标题栏，no-op
}

void UIKitApp::RequestClose() {
    should_close_ = true;
}

void UIKitApp::Show() {
    // iOS 全屏应用在 Init 后即可见，no-op
}

void UIKitApp::MakeContextCurrent() {
    // Vulkan 模式无 GL context，no-op
}

void UIKitApp::ReleaseContext() {
    // Vulkan 模式无 GL context，no-op
}

void* UIKitApp::GetNativeWindowHandle() const {
    if (!metal_view_) return nullptr;

    @autoreleasepool {
        MTKView* view = (__bridge MTKView*)metal_view_;
        return (__bridge void*)view.layer; // CAMetalLayer*
    }
}

bool UIKitApp::HasGLContext() const {
    return false; // iOS 使用 Vulkan (MoltenVK)，无 GL context
}

bool UIKitApp::LoadGLFunctions() {
    return false; // iOS 使用 Vulkan (MoltenVK)，不加载 GL
}

uint64_t UIKitApp::CreateVulkanSurface(void* vk_instance) {
#ifdef DSE_ENABLE_VULKAN
    if (!metal_view_) {
        DEBUG_LOG_ERROR("[UIKitApp] Cannot create Vulkan surface: no MTKView");
        return 0;
    }

    @autoreleasepool {
        MTKView* view = (__bridge MTKView*)metal_view_;

        VkIOSSurfaceCreateInfoMVK create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
        create_info.pNext = nullptr;
        create_info.flags = 0;
        create_info.pView = (__bridge void*)view.layer; // CAMetalLayer*

        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkResult result = vkCreateIOSSurfaceMVK(
            static_cast<VkInstance>(vk_instance),
            &create_info, nullptr, &surface);

        if (result != VK_SUCCESS) {
            DEBUG_LOG_ERROR("[UIKitApp] vkCreateIOSSurfaceMVK failed: {}",
                            static_cast<int>(result));
            return 0;
        }

        DEBUG_LOG_INFO("[UIKitApp] Vulkan iOS surface created successfully");
        return reinterpret_cast<uint64_t>(surface);
    }
#else
    (void)vk_instance;
    DEBUG_LOG_ERROR("[UIKitApp] Vulkan not enabled");
    return 0;
#endif
}

void UIKitApp::SetInputCallbacks(KeyCallback /*key_cb*/,
                                  MouseButtonCallback /*mouse_btn_cb*/,
                                  ScrollCallback /*scroll_cb*/,
                                  CursorPosCallback /*cursor_pos_cb*/) {
    // iOS 无键盘/鼠标硬件输入（外接键盘支持可后续扩展）
    // 触屏输入通过 SetTouchCallback 设置
}

void UIKitApp::SetTouchCallback(TouchCallback cb) {
    touch_cb_ = cb;

    @autoreleasepool {
        DSEViewController* vc = (__bridge DSEViewController*)view_controller_;
        if (vc && vc.touchView) {
            DSETouchView_SetCallback(vc.touchView, cb);
        } else if (vc && cb) {
            // 首次设置回调时创建 DSETouchView 并添加到视图层级
            DSETouchView* touch_view = [[DSETouchView alloc]
                initWithFrame:vc.view.bounds];
            touch_view.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                          UIViewAutoresizingFlexibleHeight;
            touch_view.multipleTouchEnabled = YES;
            touch_view.backgroundColor = [UIColor clearColor];
            touch_view.userInteractionEnabled = YES;
            [vc.view addSubview:touch_view];
            // MTKView 在下面，touch view 在上面接收触屏事件
            [vc.view bringSubviewToFront:touch_view];
            vc.touchView = touch_view;
            DSETouchView_SetCallback(touch_view, cb);
        }
    }
}

bool UIKitApp::AttachExternal(void* existing_window) {
    if (!existing_window) return false;

    @autoreleasepool {
        UIWindow* window = (__bridge UIWindow*)existing_window;
        ui_window_ = (__bridge_retained void*)window;

        UIViewController* vc = window.rootViewController;
        if ([vc isKindOfClass:[DSEViewController class]]) {
            DSEViewController* dse_vc = (DSEViewController*)vc;
            view_controller_ = (__bridge void*)dse_vc;
            metal_view_ = (__bridge void*)dse_vc.metalView;
        }

        initialized_ = true;
    }

    return true;
}

// --- 工厂函数 ---
std::unique_ptr<PlatformApp> CreateDefaultPlatformApp() {
    return std::make_unique<UIKitApp>();
}

} // namespace dse::platform

// =============================================================================
// iOS Main Entry Point
// UIApplicationMain 启动 UIKit runloop，之后引擎通过 DSEAppDelegate 接管
// =============================================================================

#if !defined(DSE_IOS_NO_MAIN)
// 仅在未定义 DSE_IOS_NO_MAIN 时提供默认 main 入口
// 测试或嵌入场景可定义 DSE_IOS_NO_MAIN 避免 main 冲突
namespace {

// 外部声明的引擎入口（由 apps/runtime 或 apps/standalone 提供）
extern "C" int dse_ios_main(int argc, char* argv[]);

} // namespace
#endif // DSE_IOS_NO_MAIN

#endif // DSE_ENABLE_APPLE_PLATFORM
