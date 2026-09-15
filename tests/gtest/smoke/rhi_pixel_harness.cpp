/**
 * @file rhi_pixel_harness.cpp
 * @brief 见 rhi_pixel_harness.h。三后端 GPU 上下文样板的唯一实现处。
 *
 * 设备复用（P1-1）：
 * 原先每次 Run* 都新建窗口 + 建设备 + 用完销毁。实测单次固定开销（min-of-15，no-op 渲染）
 * 为 OpenGL 18~44 ms / D3D11 137~170 ms / Vulkan 294~322 ms；而 338 个用例的 smoke 电池
 * 要建立上千次设备，561 s 的耗时几乎全是这个固定开销（受负载放大后中位数更高）。
 * 现在每个后端只建一次长驻会话（窗口 + 上下文 + 设备），跨用例复用；会话创建失败会缓存
 * 原因，后续调用直接返回 skip，不再反复重试。
 *
 * 两个环境变量：
 *   DSE_SMOKE_FRESH_DEVICE=1  退回「每次新建/销毁」的旧行为。复用换速度、代价是隔离性；
 *                             需要二分定位跨用例状态污染时打开它做对照。
 *   DSE_SMOKE_GPU_DEBUG=1     打开后端调试/校验层（Vulkan validation / D3D11 debug layer）。
 *                             默认关闭：它们是调试手段且显著拖慢每次调用（实测设备建立耗时
 *                             上升 1.1~2.4 倍），需要抓 RHI 用法错误时单独跑一次即可。
 */
#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"

#include <cmath>
#include <cstdlib>
#include <memory>

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

#ifdef _WIN32

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

bool EnvFlag(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && v[0] == '1';
}

bool GpuDebugLayers() {
    static const bool on = EnvFlag("DSE_SMOKE_GPU_DEBUG");
    return on;
}

bool FreshDevicePerCall() {
    static const bool on = EnvFlag("DSE_SMOKE_FRESH_DEVICE");
    return on;
}

// 「本用例申请一次性设备」标志（见头文件）。由 RequestFreshDeviceForThisTest 置位，
// 被 Run* 消费一次后自动复位，因此不会影响后续用例。
thread_local bool tls_fresh_device_request = false;

bool ConsumeFreshDeviceRequest() {
    const bool requested = tls_fresh_device_request;
    tls_fresh_device_request = false;
    return requested;
}

}  // namespace

void RequestFreshDeviceForThisTest() { tls_fresh_device_request = true; }

}  // namespace test
}  // namespace dse

// ============================================================
// OpenGL
// ============================================================
#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/render/rhi/opengl/gl_loader.h"

namespace dse {
namespace test {
namespace {

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

// 长驻 GL 会话：成员声明顺序即析构逆序  device 先析构（删除 GL 对象），
// 之后 guard 才释放 HGLRC/HDC/窗口。顺序反了会在上下文已销毁后删 GL 对象。
struct GLSession {
    GLContextGuard guard;
    std::unique_ptr<dse::render::OpenGLRhiDevice> device;
    bool initialized = false;
    bool available = false;
    const char* reason = "No GL window/driver";

    ~GLSession() {
        if (device) device->Shutdown();
        device.reset();
    }

    GLSession() = default;
    GLSession(const GLSession&) = delete;
    GLSession& operator=(const GLSession&) = delete;

    // 每次使用前重新绑定本会话的上下文。GL 的 current context 是线程全局状态，
    // 用例里存在自建上下文的 fixture（如 GLRhiSmokeTest：SetUp 建、TearDown 销毁），
    // 它们会把本线程的 current context 换掉/清空。长驻会话若不重绑，后续所有 GL 调用
    // 都会落在失效上下文上（表现为 shader 创建失败、readback 尺寸为 0）。
    // wglMakeCurrent 是微秒级调用，相对每次重建上下文可忽略。
    void MakeCurrent() const {
        if (guard.hdc && guard.hglrc) {
            wglMakeCurrent(guard.hdc, guard.hglrc);
        }
    }

    void Init() {
        if (!CreateGLContext(guard)) {
            reason = "No GL window/driver";
            return;
        }
        const int gl_version = gladLoadGL(GLGladLoad);
        if (gl_version == 0 || !GLAD_GL_VERSION_3_3) {
            reason = "Requires OpenGL 3.3+ context";
            return;
        }
        device = std::make_unique<dse::render::OpenGLRhiDevice>();
        available = true;
    }
};

GLSession& SharedGLSession() {
    static GLSession session;
    if (!session.initialized) {
        session.initialized = true;
        session.Init();
    }
    return session;
}

}  // namespace

BackendResult RunOpenGL(const RenderFn& fn) {
    BackendResult result;
    if (FreshDevicePerCall() || ConsumeFreshDeviceRequest()) {
        GLSession tmp;
        tmp.Init();
        if (!tmp.available) {
            result.skip_reason = tmp.reason;
            return result;
        }
        result.readback = fn(*tmp.device);
        result.available = true;
        return result;
    }

    GLSession& session = SharedGLSession();
    if (!session.available) {
        result.skip_reason = session.reason;
        return result;
    }
    session.MakeCurrent();
    result.readback = fn(*session.device);
    // 用例可能把工作留在 GPU 队列里（未 WaitIdle 就返回）。复用时下一个用例会在这份
    // 「仍有在飞工作 + 已提交资源删除」的设备上开始，Vulkan 侧表现为下一个 pass 静默
    // 不执行、回读全 0。每次调用后静默设备，让下一个用例从干净状态起步。
    // GL/DX11 是立即转发型，WaitIdle 为基类空实现，无额外开销。
    session.device->WaitIdle();
    result.available = true;
    return result;
}

}  // namespace test
}  // namespace dse

// ============================================================
// Direct3D 11
// ============================================================
#if defined(DSE_ENABLE_D3D11)

#include "engine/render/rhi/dx11/dx11_rhi_device.h"

namespace dse {
namespace test {
namespace {

struct D3D11Session {
    HWND hwnd = nullptr;
    std::unique_ptr<dse::render::DX11RhiDevice> device;
    bool initialized = false;
    bool available = false;
    const char* reason = "No Win32 window";

    ~D3D11Session() {
        if (device) device->Shutdown();
        device.reset();
        if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
    }

    D3D11Session() = default;
    D3D11Session(const D3D11Session&) = delete;
    D3D11Session& operator=(const D3D11Session&) = delete;

    void Init() {
        hwnd = CreateHarnessWindow();
        if (!hwnd) {
            reason = "No Win32 window";
            return;
        }
        device = std::make_unique<dse::render::DX11RhiDevice>();
        if (!device->InitD3D11(static_cast<void*>(hwnd), 320, 240, GpuDebugLayers())) {
            device.reset();
            DestroyWindow(hwnd);
            hwnd = nullptr;
            reason = "No D3D11 device/driver";
            return;
        }
        available = true;
    }
};

D3D11Session& SharedD3D11Session() {
    static D3D11Session session;
    if (!session.initialized) {
        session.initialized = true;
        session.Init();
    }
    return session;
}

}  // namespace

BackendResult RunD3D11(const RenderFn& fn) {
    BackendResult result;
    if (FreshDevicePerCall() || ConsumeFreshDeviceRequest()) {
        D3D11Session tmp;
        tmp.Init();
        if (!tmp.available) {
            result.skip_reason = tmp.reason;
            return result;
        }
        result.readback = fn(*tmp.device);
        result.available = true;
        return result;
    }

    D3D11Session& session = SharedD3D11Session();
    if (!session.available) {
        result.skip_reason = session.reason;
        return result;
    }
    result.readback = fn(*session.device);
    // 用例可能把工作留在 GPU 队列里（未 WaitIdle 就返回）。复用时下一个用例会在这份
    // 「仍有在飞工作 + 已提交资源删除」的设备上开始，Vulkan 侧表现为下一个 pass 静默
    // 不执行、回读全 0。每次调用后静默设备，让下一个用例从干净状态起步。
    // GL/DX11 是立即转发型，WaitIdle 为基类空实现，无额外开销。
    session.device->WaitIdle();
    result.available = true;
    return result;
}

}  // namespace test
}  // namespace dse

#endif  // DSE_ENABLE_D3D11

// ============================================================
// Vulkan
// ============================================================
#if defined(DSE_ENABLE_VULKAN)

#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"

namespace dse {
namespace test {
namespace {

struct VulkanSession {
    HWND hwnd = nullptr;
    std::unique_ptr<dse::render::VulkanRhiDevice> device;
    bool initialized = false;
    bool available = false;
    const char* reason = "No Win32 window";

    ~VulkanSession() {
        if (device) device->Shutdown();
        device.reset();
        if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
    }

    VulkanSession() = default;
    VulkanSession(const VulkanSession&) = delete;
    VulkanSession& operator=(const VulkanSession&) = delete;

    void Init() {
        hwnd = CreateHarnessWindow();
        if (!hwnd) {
            reason = "No Win32 window";
            return;
        }
        device = std::make_unique<dse::render::VulkanRhiDevice>();
        if (!device->InitVulkan(static_cast<void*>(hwnd), 320, 240, GpuDebugLayers())) {
            device.reset();
            DestroyWindow(hwnd);
            hwnd = nullptr;
            reason = "No Vulkan device/driver";
            return;
        }
        available = true;
    }
};

VulkanSession& SharedVulkanSession() {
    static VulkanSession session;
    if (!session.initialized) {
        session.initialized = true;
        session.Init();
    }
    return session;
}

}  // namespace

BackendResult RunVulkan(const RenderFn& fn) {
    BackendResult result;
    if (FreshDevicePerCall() || ConsumeFreshDeviceRequest()) {
        VulkanSession tmp;
        tmp.Init();
        if (!tmp.available) {
            result.skip_reason = tmp.reason;
            return result;
        }
        result.readback = fn(*tmp.device);
        result.available = true;
        return result;
    }

    VulkanSession& session = SharedVulkanSession();
    if (!session.available) {
        result.skip_reason = session.reason;
        return result;
    }
    result.readback = fn(*session.device);
    // 用例可能把工作留在 GPU 队列里（未 WaitIdle 就返回）。复用时下一个用例会在这份
    // 「仍有在飞工作 + 已提交资源删除」的设备上开始，Vulkan 侧表现为下一个 pass 静默
    // 不执行、回读全 0。每次调用后静默设备，让下一个用例从干净状态起步。
    // GL/DX11 是立即转发型，WaitIdle 为基类空实现，无额外开销。
    session.device->WaitIdle();
    result.available = true;
    return result;
}

}  // namespace test
}  // namespace dse

#endif  // DSE_ENABLE_VULKAN

#else  // !_WIN32

namespace dse { namespace test {
BackendResult RunOpenGL(const RenderFn&) { return {}; }
BackendResult RunD3D11(const RenderFn&) { return {}; }
BackendResult RunVulkan(const RenderFn&) { return {}; }
void RequestFreshDeviceForThisTest() {}
}}  // namespace dse::test

#endif  // _WIN32
