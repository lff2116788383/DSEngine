/**
 * @file rhi_pixel_harness.cpp
 * @brief 见 rhi_pixel_harness.h。三后端 GPU 上下文样板的唯一实现处。
 */
#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"

#include <cmath>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace dse {
namespace test {

using ::RenderTargetReadback;
using dse::render::RhiDevice;

const unsigned char* PixelAt(const RenderTargetReadback& rb, int x, int y) {
    if (x < 0 || y < 0 || x >= rb.width || y >= rb.height) return nullptr;
    const size_t idx = (static_cast<size_t>(y) * rb.width + x) * 4;
    if (idx + 4 > rb.pixels.size()) return nullptr;
    return rb.pixels.data() + idx;
}

double ComputeRmse(const RenderTargetReadback& a, const RenderTargetReadback& b) {
    if (a.width != b.width || a.height != b.height) return -1.0;
    if (a.pixels.empty() || a.pixels.size() != b.pixels.size()) return -1.0;
    double acc = 0.0;
    for (size_t i = 0; i < a.pixels.size(); ++i) {
        const double d = static_cast<double>(a.pixels[i]) - static_cast<double>(b.pixels[i]);
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(a.pixels.size()));
}

}  // namespace test
}  // namespace dse

// ============================================================
// OpenGL
// ============================================================
#ifdef _WIN32

#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/render/rhi/opengl/gl_loader.h"

namespace dse {
namespace test {
namespace {

LRESULT CALLBACK HarnessWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    return DefWindowProcA(hwnd, msg, w, l);
}

// 注册一个共享窗口类（首次调用时注册一次）。
const char* EnsureWindowClass() {
    static const char* kClassName = "DSE_RhiPixelHarness";
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = HarnessWndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClassName;
        RegisterClassA(&wc);
        registered = true;
    }
    return kClassName;
}

HWND CreateHarnessWindow() {
    return CreateWindowExA(0, EnsureWindowClass(), "DSE_RhiPixelHarness", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
                           nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
}

struct GLContextGuard {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC hglrc = nullptr;
    ~GLContextGuard() {
        if (hglrc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(hglrc); }
        if (hdc && hwnd) { ReleaseDC(hwnd, hdc); }
        if (hwnd) { DestroyWindow(hwnd); }
    }
};

bool CreateGLContext(GLContextGuard& guard) {
    guard.hwnd = CreateHarnessWindow();
    if (!guard.hwnd) return false;
    guard.hdc = GetDC(guard.hwnd);
    if (!guard.hdc) return false;

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(guard.hdc, &pfd);
    if (pf == 0) return false;
    if (!SetPixelFormat(guard.hdc, pf, &pfd)) return false;
    guard.hglrc = wglCreateContext(guard.hdc);
    if (!guard.hglrc) return false;
    if (!wglMakeCurrent(guard.hdc, guard.hglrc)) return false;
    return true;
}

GLADapiproc GLGladLoad(const char* name) {
    PROC proc = wglGetProcAddress(name);
    if (proc == nullptr) {
        static HMODULE gl_module = LoadLibraryA("opengl32.dll");
        if (gl_module) proc = GetProcAddress(gl_module, name);
    }
    return reinterpret_cast<GLADapiproc>(proc);
}

}  // namespace

BackendResult RunOpenGL(const RenderFn& fn) {
    BackendResult result;
    GLContextGuard guard;
    if (!CreateGLContext(guard)) {
        result.skip_reason = "No GL window/driver";
        return result;
    }
    const int gl_version = gladLoadGL(GLGladLoad);
    if (gl_version == 0 || !GLAD_GL_VERSION_3_3) {
        result.skip_reason = "Requires OpenGL 3.3+ context";
        return result;
    }
    {
        dse::render::OpenGLRhiDevice device;
        result.readback = fn(device);
        device.Shutdown();
    }
    result.available = true;
    return result;
}

}  // namespace test
}  // namespace dse

#else  // !_WIN32

namespace dse { namespace test {
BackendResult RunOpenGL(const RenderFn&) { return {}; }
}}  // namespace dse::test

#endif  // _WIN32

// ============================================================
// Direct3D 11
// ============================================================
#if defined(_WIN32) && defined(DSE_ENABLE_D3D11)

#include "engine/render/rhi/dx11/dx11_rhi_device.h"

namespace dse {
namespace test {

BackendResult RunD3D11(const RenderFn& fn) {
    BackendResult result;
    HWND hwnd = CreateHarnessWindow();
    if (!hwnd) {
        result.skip_reason = "No Win32 window";
        return result;
    }
    {
        dse::render::DX11RhiDevice device;
        if (!device.InitD3D11(static_cast<void*>(hwnd), 320, 240, true)) {
            DestroyWindow(hwnd);
            result.skip_reason = "No D3D11 device/driver";
            return result;
        }
        result.readback = fn(device);
        device.Shutdown();
    }
    DestroyWindow(hwnd);
    result.available = true;
    return result;
}

}  // namespace test
}  // namespace dse

#else

namespace dse { namespace test {
BackendResult RunD3D11(const RenderFn&) { return {}; }
}}  // namespace dse::test

#endif  // _WIN32 && DSE_ENABLE_D3D11

// ============================================================
// Vulkan
// ============================================================
#if defined(_WIN32) && defined(DSE_ENABLE_VULKAN)

#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"

namespace dse {
namespace test {

BackendResult RunVulkan(const RenderFn& fn) {
    BackendResult result;
    HWND hwnd = CreateHarnessWindow();
    if (!hwnd) {
        result.skip_reason = "No Win32 window";
        return result;
    }
    {
        dse::render::VulkanRhiDevice device;
        if (!device.InitVulkan(static_cast<void*>(hwnd), 320, 240, true)) {
            DestroyWindow(hwnd);
            result.skip_reason = "No Vulkan device/driver";
            return result;
        }
        result.readback = fn(device);
        device.Shutdown();
    }
    DestroyWindow(hwnd);
    result.available = true;
    return result;
}

}  // namespace test
}  // namespace dse

#else

namespace dse { namespace test {
BackendResult RunVulkan(const RenderFn&) { return {}; }
}}  // namespace dse::test

#endif  // _WIN32 && DSE_ENABLE_VULKAN
