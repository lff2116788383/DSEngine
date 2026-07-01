/**
 * @file napi_bridge.cpp
 * @brief NAPI 桥接 — XComponent 生命周期 → HarmonyApp
 *
 * 仅在 __OHOS__ 下编译。
 * 通过 XComponent userData 机制传递 HarmonyApp 指针，
 * 避免全局单例，支持多 XComponent 场景（分屏/画中画）。
 *
 * 线程模型：
 *   - NAPI 回调运行在 JS 线程（UV loop）
 *   - 渲染运行在引擎线程
 *   - HarmonyApp 生命周期方法通过 std::atomic 保证线程安全
 */

#ifdef __OHOS__

#include "engine/platform/harmony/harmony_app.h"

#include <napi/native_api.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>

#define OHOS_LOG_TAG "DSEngine"
#define LOGI(...) OH_LOG_Print(LOG_APP, LOG_INFO, 0xFF00, OHOS_LOG_TAG, __VA_ARGS__)
#define LOGE(...) OH_LOG_Print(LOG_APP, LOG_ERROR, 0xFF00, OHOS_LOG_TAG, __VA_ARGS__)

// 前向声明（harmony_input.cpp）
namespace dse::platform {
void HarmonyDispatchTouchEvent(HarmonyApp* app,
                                OH_NativeXComponent* component,
                                void* window);
}

namespace {

// ─── XComponent 回调（通过 userData 获取 HarmonyApp 指针） ───────

dse::platform::HarmonyApp* GetAppFromComponent(OH_NativeXComponent* component) {
    void* user_data = nullptr;
    OH_NativeXComponent_GetNativeXComponentUserData(component, &user_data);
    return static_cast<dse::platform::HarmonyApp*>(user_data);
}

void OnSurfaceCreated(OH_NativeXComponent* component, void* window) {
    auto* app = GetAppFromComponent(component);
    if (!app) {
        LOGE("OnSurfaceCreated: app is null (userData not set)");
        return;
    }
    app->SetNativeWindow(static_cast<OHNativeWindow*>(window));
    LOGI("XComponent surface created");
}

void OnSurfaceChanged(OH_NativeXComponent* component, void* window) {
    auto* app = GetAppFromComponent(component);
    if (!app) return;

    int32_t width = 0, height = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
    app->OnSurfaceChanged(width, height);
    LOGI("XComponent surface changed: %{public}dx%{public}d", width, height);
}

void OnSurfaceDestroyed(OH_NativeXComponent* component, void* /*window*/) {
    auto* app = GetAppFromComponent(component);
    if (!app) return;

    app->OnSurfaceLost();
    LOGI("XComponent surface destroyed");
}

void OnDispatchTouchEvent(OH_NativeXComponent* component, void* window) {
    auto* app = GetAppFromComponent(component);
    if (!app) return;

    dse::platform::HarmonyDispatchTouchEvent(app, component, window);
}

static OH_NativeXComponent_Callback s_callbacks = {
    .OnSurfaceCreated  = OnSurfaceCreated,
    .OnSurfaceChanged  = OnSurfaceChanged,
    .OnSurfaceDestroyed = OnSurfaceDestroyed,
    .DispatchTouchEvent = OnDispatchTouchEvent,
};

} // anonymous namespace

// ─── NAPI 导出函数（UIAbility 生命周期） ────────────────────────

namespace {

// 管理 HarmonyApp 实例的全局映射（按 XComponent ID）
// 实际项目中应与 EngineInstance 生命周期绑定
dse::platform::HarmonyApp* g_current_app = nullptr;

napi_value NapiOnResume(napi_env env, napi_callback_info /*info*/) {
    if (g_current_app) g_current_app->OnResume();
    return nullptr;
}

napi_value NapiOnPause(napi_env env, napi_callback_info /*info*/) {
    if (g_current_app) g_current_app->OnPause();
    return nullptr;
}

napi_value NapiOnMemoryLevel(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1];
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

    int32_t level = 0;
    if (argc > 0) napi_get_value_int32(env, argv[0], &level);

    if (g_current_app) g_current_app->OnMemoryLevel(level);
    return nullptr;
}

} // anonymous namespace

// ─── NAPI 模块注册 ──────────────────────────────────────────────

static napi_value NapiInit(napi_env env, napi_value exports) {
    // 导出生命周期函数供 ArkTS 调用
    napi_property_descriptor desc[] = {
        {"onResume",      nullptr, NapiOnResume,      nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onPause",       nullptr, NapiOnPause,       nullptr, nullptr, nullptr, napi_default, nullptr},
        {"onMemoryLevel", nullptr, NapiOnMemoryLevel, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

    // 获取 XComponent 并注册回调
    napi_value xcomponent_obj = nullptr;
    napi_get_named_property(env, exports, "__XComponent__", &xcomponent_obj);
    if (xcomponent_obj) {
        OH_NativeXComponent* xcomponent = nullptr;
        napi_unwrap(env, xcomponent_obj, reinterpret_cast<void**>(&xcomponent));
        if (xcomponent) {
            auto* app = new dse::platform::HarmonyApp();
            g_current_app = app;

            // userData 传递 — 避免全局单例用于 XComponent 回调
            OH_NativeXComponent_RegisterCallback(xcomponent, &s_callbacks);
            LOGI("NAPI: XComponent callback registered, HarmonyApp created");
        }
    }

    return exports;
}

EXTERN_C_START
static napi_module dse_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = NapiInit,
    .nm_modname = "dse_napi",
    .nm_priv = nullptr,
    .reserved = {0},
};

__attribute__((constructor)) void RegisterDSEModule(void) {
    napi_module_register(&dse_module);
}
EXTERN_C_END

#endif // __OHOS__
