/**
 * @file egl_helper.h
 * @brief 共享 EGL 初始化/销毁工具 — Android 与 OHOS 复用
 *
 * 仅在 Android 或 OHOS 构建中编译（两者都使用 EGL + GLES3）。
 * 桌面端（GLFW）和 iOS（MoltenVK）不使用此文件。
 */

#pragma once
#if defined(__ANDROID__) || defined(__OHOS__)

#include <EGL/egl.h>

namespace dse::platform {

/// EGL 上下文状态（display + context + surface）
struct EGLState {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    bool has_context = false;
};

/**
 * @brief 在指定原生窗口上创建 EGL context（GLES 3）
 * @param state         EGL 状态输出
 * @param native_window 平台原生窗口句柄（ANativeWindow* 或 OHNativeWindow*）
 * @param es_version    GLES 主版本号（默认 3）
 * @param depth_bits    深度缓冲位数（默认 24）
 * @return true 成功
 */
bool InitEGL(EGLState& state, void* native_window,
             int es_version = 3, int depth_bits = 24);

/**
 * @brief 销毁 EGL context 并重置状态
 */
void DestroyEGL(EGLState& state);

} // namespace dse::platform

#endif // __ANDROID__ || __OHOS__
