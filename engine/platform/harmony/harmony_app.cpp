/**
 * @file harmony_app.cpp
 * @brief HarmonyApp 实现 — XComponent + EGL/Vulkan + 生命周期状态机
 *
 * 仅在 __OHOS__ 下编译（PC 构建中此文件被 CMake 过滤排除）。
 */

#ifdef __OHOS__

#include "engine/platform/harmony/harmony_app.h"
#include "engine/base/debug.h"

#include <hilog/log.h>

#ifdef DSE_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#define VK_USE_PLATFORM_OHOS_OPENHARMONY
#include <vulkan/vulkan_ohos.h>
#endif

#define OHOS_LOG_TAG "DSEngine"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, OHOS_LOG_TAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, OHOS_LOG_TAG, __VA_ARGS__)

namespace dse::platform {

HarmonyApp::~HarmonyApp() {
    Shutdown();
}

// ─── 注入 ───────────────────────────────────────────────────────

void HarmonyApp::SetNativeWindow(OHNativeWindow* window) {
    native_window_ = window;
}

void HarmonyApp::SetResourceManager(NativeResourceManager* mgr) {
    resource_mgr_ = mgr;
}

// ─── Init ───────────────────────────────────────────────────────

bool HarmonyApp::Init(const WindowConfig& config) {
    if (!native_window_) {
        LOGE("HarmonyApp::Init: OHNativeWindow not set");
        return false;
    }

    start_time_ = std::chrono::steady_clock::now();

    if (!config.no_graphics_api) {
        if (!InitEGL(config)) return false;
    }

    state_.store(AppState::Active, std::memory_order_release);
    LOGI("HarmonyApp initialized");
    return true;
}

bool HarmonyApp::InitEGL(const WindowConfig& /*config*/) {
    if (!dse::platform::InitEGL(egl_, native_window_)) {
        LOGE("EGL initialization failed");
        return false;
    }
    LOGI("EGL context created (OpenGL ES 3)");
    return true;
}

void HarmonyApp::DestroyEGL() {
    dse::platform::DestroyEGL(egl_);
}

// ─── 生命周期状态机 ────────────────────────────────────────────

void HarmonyApp::OnPause() {
    auto cur = state_.load(std::memory_order_acquire);
    if (cur == AppState::Active)
        state_.store(AppState::Paused, std::memory_order_release);
    else if (cur == AppState::SurfaceLost)
        state_.store(AppState::PausedNoSurface, std::memory_order_release);
    LOGI("HarmonyApp paused");
}

void HarmonyApp::OnResume() {
    auto cur = state_.load(std::memory_order_acquire);
    if (cur == AppState::Paused)
        state_.store(AppState::Active, std::memory_order_release);
    else if (cur == AppState::PausedNoSurface)
        state_.store(AppState::SurfaceLost, std::memory_order_release);
    LOGI("HarmonyApp resumed");
}

void HarmonyApp::OnSurfaceLost() {
    DestroyEGL();
    native_window_ = nullptr;
    auto cur = state_.load(std::memory_order_acquire);
    if (cur == AppState::Paused)
        state_.store(AppState::PausedNoSurface, std::memory_order_release);
    else
        state_.store(AppState::SurfaceLost, std::memory_order_release);
    LOGI("HarmonyApp surface lost");
}

void HarmonyApp::OnSurfaceRegained(OHNativeWindow* window) {
    native_window_ = window;
    dse::platform::InitEGL(egl_, native_window_);
    auto cur = state_.load(std::memory_order_acquire);
    if (cur == AppState::PausedNoSurface)
        state_.store(AppState::Paused, std::memory_order_release);
    else
        state_.store(AppState::Active, std::memory_order_release);
    LOGI("HarmonyApp surface regained");
}

void HarmonyApp::OnSurfaceChanged(int width, int height) {
    LOGI("HarmonyApp surface changed: %{public}dx%{public}d", width, height);
}

void HarmonyApp::OnMemoryLevel(int level) {
    LOGI("HarmonyApp memory level: %{public}d", level);
}

HarmonyApp::AppState HarmonyApp::GetAppState() const {
    return state_.load(std::memory_order_acquire);
}

bool HarmonyApp::IsRenderable() const {
    return state_.load(std::memory_order_acquire) == AppState::Active;
}

// ─── Shutdown ──────────────────────────────────────────────────

void HarmonyApp::Shutdown() {
    if (state_.load() == AppState::Destroyed) return;
    DestroyEGL();
    state_.store(AppState::Destroyed, std::memory_order_release);
    LOGI("HarmonyApp shutdown");
}

// ─── 主循环 ───────────────────────────────────────────────────

bool HarmonyApp::ShouldClose() const {
    return should_close_.load(std::memory_order_relaxed);
}

void HarmonyApp::PollEvents() {
    // XComponent 触屏事件通过回调直接注入，无需轮询
}

void HarmonyApp::SwapBuffers() {
    if (egl_.display != EGL_NO_DISPLAY && egl_.surface != EGL_NO_SURFACE) {
        eglSwapBuffers(egl_.display, egl_.surface);
    }
}

double HarmonyApp::GetTime() const {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start_time_).count();
}

// ─── 窗口信息 ─────────────────────────────────────────────────

void HarmonyApp::GetFramebufferSize(int& w, int& h) const {
    if (native_window_) {
        int32_t width = 0, height = 0;
        OH_NativeWindow_NativeWindowHandleOpt(native_window_,
            GET_BUFFER_GEOMETRY, &height, &width);
        w = width;
        h = height;
    } else {
        w = h = 0;
    }
}

void HarmonyApp::SetWindowTitle(const std::string& /*title*/) {
    // OHOS 无窗口标题栏，忽略
}

void HarmonyApp::RequestClose() {
    should_close_.store(true, std::memory_order_relaxed);
}

// ─── 平台桥接 ─────────────────────────────────────────────────

void* HarmonyApp::GetNativeWindowHandle() const {
    return static_cast<void*>(native_window_);
}

bool HarmonyApp::HasGLContext() const {
    return egl_.has_context;
}

bool HarmonyApp::LoadGLFunctions() {
    // GLES 3 函数在 OHOS 上直接可用（动态链接 libGLESv3）
    return true;
}

uint64_t HarmonyApp::CreateVulkanSurface(void* vk_instance) {
#ifdef DSE_ENABLE_VULKAN
    VkOHOSSurfaceCreateInfoOpenHarmony info{};
    info.sType = VK_STRUCTURE_TYPE_OHOS_SURFACE_CREATE_INFO_OPENHARMONY;
    info.window = native_window_;

    auto fn = reinterpret_cast<PFN_vkCreateOHOSSurfaceOpenHarmony>(
        vkGetInstanceProcAddr(static_cast<VkInstance>(vk_instance),
                              "vkCreateOHOSSurfaceOpenHarmony"));
    if (!fn) {
        LOGE("vkCreateOHOSSurfaceOpenHarmony not available");
        return 0;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (fn(static_cast<VkInstance>(vk_instance), &info, nullptr, &surface) != VK_SUCCESS) {
        LOGE("vkCreateOHOSSurfaceOpenHarmony failed");
        return 0;
    }
    return reinterpret_cast<uint64_t>(surface);
#else
    (void)vk_instance;
    return 0;
#endif
}

void HarmonyApp::SetInputCallbacks(KeyCallback key, MouseButtonCallback mouse,
                                    ScrollCallback scroll, CursorPosCallback cursor) {
    key_cb_        = key;
    mouse_btn_cb_  = mouse;
    scroll_cb_     = scroll;
    cursor_pos_cb_ = cursor;
}

void HarmonyApp::SetTouchCallback(TouchCallback touch) {
    touch_cb_ = touch;
}

bool HarmonyApp::AttachExternal(void* existing_window) {
    native_window_ = static_cast<OHNativeWindow*>(existing_window);
    return native_window_ != nullptr;
}

// ─── 工厂函数（OHOS 版） ────────────────────────────────────────

std::unique_ptr<PlatformApp> CreateDefaultPlatformApp() {
    return std::make_unique<HarmonyApp>();
}

} // namespace dse::platform

#endif // __OHOS__
