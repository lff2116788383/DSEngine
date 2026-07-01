/**
 * @file egl_helper.cpp
 * @brief 共享 EGL 初始化/销毁实现 — Android 与 OHOS 复用
 *
 * 仅在 Android 或 OHOS 构建中编译。
 */

#if defined(__ANDROID__) || defined(__OHOS__)

#include "engine/platform/shared/egl_helper.h"
#include "engine/base/debug.h"

#ifdef __ANDROID__
#include <android/native_window.h>
#endif

namespace dse::platform {

bool InitEGL(EGLState& state, void* native_window,
             int es_version, int depth_bits) {
    state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (state.display == EGL_NO_DISPLAY) {
        DEBUG_LOG_ERROR("eglGetDisplay failed");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(state.display, &major, &minor)) {
        DEBUG_LOG_ERROR("eglInitialize failed");
        return false;
    }

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_DEPTH_SIZE, depth_bits,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint num_cfg = 0;
    if (!eglChooseConfig(state.display, attribs, &cfg, 1, &num_cfg) || num_cfg == 0) {
        DEBUG_LOG_ERROR("eglChooseConfig failed");
        return false;
    }

#ifdef __ANDROID__
    // Android: 窗口格式与 EGL config 对齐
    EGLint format;
    eglGetConfigAttrib(state.display, cfg, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(
        static_cast<ANativeWindow*>(native_window), 0, 0, format);
#endif

    state.surface = eglCreateWindowSurface(
        state.display, cfg,
        static_cast<EGLNativeWindowType>(native_window), nullptr);
    if (state.surface == EGL_NO_SURFACE) {
        DEBUG_LOG_ERROR("eglCreateWindowSurface failed");
        return false;
    }

    const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, es_version, EGL_NONE };
    state.context = eglCreateContext(state.display, cfg, EGL_NO_CONTEXT, ctx_attribs);
    if (state.context == EGL_NO_CONTEXT) {
        DEBUG_LOG_ERROR("eglCreateContext failed");
        return false;
    }

    if (!eglMakeCurrent(state.display, state.surface, state.surface, state.context)) {
        DEBUG_LOG_ERROR("eglMakeCurrent failed");
        return false;
    }

    state.has_context = true;
    return true;
}

void DestroyEGL(EGLState& state) {
    if (state.display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (state.context != EGL_NO_CONTEXT) {
        eglDestroyContext(state.display, state.context);
        state.context = EGL_NO_CONTEXT;
    }
    if (state.surface != EGL_NO_SURFACE) {
        eglDestroySurface(state.display, state.surface);
        state.surface = EGL_NO_SURFACE;
    }
    eglTerminate(state.display);
    state.display = EGL_NO_DISPLAY;
    state.has_context = false;
}

} // namespace dse::platform

#endif // __ANDROID__ || __OHOS__
