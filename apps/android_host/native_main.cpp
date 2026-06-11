/**
 * @file native_main.cpp
 * @brief DSEngine Android 原生宿主入口（NativeActivity + native_app_glue）
 *
 * 仅 __ANDROID__ 下编译。通过 native_app_glue 的 android_main 接管
 * ANativeActivity 生命周期，把系统的 ANativeWindow / AAssetManager 喂给
 * 引擎的 PlatformApp(AndroidApp)，初始化 EGL/GLES 并跑一个最小帧循环。
 *
 * 这是验证「引擎可被 NativeActivity 加载并在设备上运行」的最小宿主，
 * 不引入任何 Java 代码（hasCode=false），由本 .so 导出的
 * ANativeActivity_onCreate(由 glue 提供) 作为入口。
 */

#ifdef __ANDROID__

#include <android_native_app_glue.h>
#include <android/log.h>

#include <GLES3/gl3.h>

#include "engine/platform/android/android_app.h"

#define DSE_HOST_TAG "DSEngineHost"
#define HLOGI(...) __android_log_print(ANDROID_LOG_INFO,  DSE_HOST_TAG, __VA_ARGS__)
#define HLOGE(...) __android_log_print(ANDROID_LOG_ERROR, DSE_HOST_TAG, __VA_ARGS__)

using dse::platform::AndroidApp;
using dse::platform::WindowConfig;

namespace {

struct HostState {
    AndroidApp* app = nullptr;
    bool        rendering = false;
    int         frame = 0;
};

void DestroyApp(HostState* hs) {
    if (hs->app) {
        hs->app->Shutdown();
        delete hs->app;
        hs->app = nullptr;
    }
    hs->rendering = false;
}

void HandleCmd(struct android_app* a, int32_t cmd) {
    auto* hs = static_cast<HostState*>(a->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (a->window && !hs->app) {
                hs->app = new AndroidApp();
                hs->app->SetNativeWindow(a->window);
                if (a->activity) hs->app->SetAssetManager(a->activity->assetManager);
                WindowConfig cfg;
                cfg.title = "DSEngine Android Host";
                if (hs->app->Init(cfg)) {
                    hs->app->LoadGLFunctions();
                    hs->rendering = true;
                    HLOGI("AndroidApp initialized; EGL/GLES ready");
                } else {
                    HLOGE("AndroidApp::Init failed");
                    DestroyApp(hs);
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            HLOGI("APP_CMD_TERM_WINDOW; tearing down");
            DestroyApp(hs);
            break;
        default:
            break;
    }
}

void DrawFrame(HostState* hs) {
    if (!hs->rendering || !hs->app) return;
    int w = 0, h = 0;
    hs->app->GetFramebufferSize(w, h);
    glViewport(0, 0, w, h);
    // 简单呼吸色，证明 GLES 上下文与 swap 链路工作
    const float t = static_cast<float>(hs->frame % 240) / 240.0f;
    glClearColor(0.1f, 0.2f + 0.3f * t, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    hs->app->SwapBuffers();
    ++hs->frame;
}

} // namespace

extern "C" void android_main(struct android_app* state) {
    HostState hs;
    state->userData = &hs;
    state->onAppCmd = HandleCmd;

    HLOGI("android_main entered");

    while (true) {
        int events = 0;
        struct android_poll_source* source = nullptr;
        // 渲染时不阻塞(timeout 0)，空闲时阻塞等待事件(timeout -1)
        const int timeout = hs.rendering ? 0 : -1;
        while (ALooper_pollOnce(timeout, nullptr, &events,
                               reinterpret_cast<void**>(&source)) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) {
                HLOGI("destroyRequested; exiting android_main");
                DestroyApp(&hs);
                return;
            }
            if (!hs.rendering) continue;  // 仍在排空事件队列
        }
        DrawFrame(&hs);
    }
}

#endif // __ANDROID__
