/**
 * @file three_rhi_soak_test.cpp
 * @brief P2-2：三 RHI（OpenGL / D3D11 / Vulkan）真机资源/内存/句柄 soak。
 *
 * 目的：在真实 GPU（RTX 3070）上，对每个后端在「同一设备生命周期内」反复执行
 *       「建离屏 RT → 渲染确定性场景 → 回读 → 销毁 RT / MeshRenderer」的整轮资源 churn
 *       （等价于反复 Play/渲染/Stop + 场景切换），并在热身后与结束时采样：
 *         - 进程工作集（CPU 常驻内存，psapi GetProcessMemoryInfo）
 *         - 进程内核句柄数（GetProcessHandleCount）
 *         - 本进程在本地显存段的占用（DXGI IDXGIAdapter3::QueryVideoMemoryInfo）
 *       断言三者在设定阈值内不随迭代线性增长 —— 即无每轮 GPU/CPU/句柄泄漏。
 *
 * 诚实边界：
 *  - 显存/句柄/工作集必须在真实 GPU 机读回才可信；软件回退（WARP/SwiftShader/lavapipe）
 *    的显存计数不代表真机，故显式断言 device.is_software==false 且 DXGI 选中的适配器非软件。
 *  - 后端切换由三个独立用例覆盖；设备 create/destroy 泄漏由 P0-4 三 RHI golden（每次
 *    Run* 都整建整销设备）已覆盖，本用例聚焦「设备存活期内反复资源 churn」的泄漏预算。
 *  - 迭代次数可用环境变量 DSE_SOAK_ITERS 调整（默认 96）。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <dxgi1_4.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "psapi.lib")
#endif

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;
constexpr int kWarmup = 8;

struct SoakSample {
    unsigned long long working_set = 0;
    unsigned long handle_count = 0;
    unsigned long long vram_bytes = 0;
};

struct SoakCapture {
    RenderDeviceInfo info;
    SoakSample baseline;
    SoakSample final_;
    std::string dxgi_adapter;
    bool dxgi_software = true;
    int iterations = 0;
    bool rendered_ok = false;
};

// RenderFn 内填充（与 golden 测试的 g_last_info 同构，绕过 harness 仅回传 readback 的限制）。
SoakCapture g_soak;

int SoakIters() {
    const char* v = std::getenv("DSE_SOAK_ITERS");
    if (v && v[0]) {
        const int n = std::atoi(v);
        if (n > 0) return n;
    }
    return 96;
}

#ifdef _WIN32
unsigned long long QueryWorkingSet() {
    PROCESS_MEMORY_COUNTERS c{};
    c.cb = sizeof(c);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof(c))) return c.WorkingSetSize;
    return 0;
}

unsigned long QueryHandleCount() {
    DWORD h = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &h)) return h;
    return 0;
}

// 选中当前进程实际使用的（非软件）适配器，返回其在本地显存段的进程占用字节。
unsigned long long QueryVramBytes(std::string& adapter_name, bool& software) {
    adapter_name.clear();
    software = true;
    IDXGIFactory4* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory))))
        return 0;
    unsigned long long best = 0;
    bool found_hw = false;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        const bool sw = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        IDXGIAdapter3* a3 = nullptr;
        if (SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),
                                              reinterpret_cast<void**>(&a3)))) {
            DXGI_QUERY_VIDEO_MEMORY_INFO vmi{};
            if (SUCCEEDED(a3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vmi))) {
                if (!sw && vmi.CurrentUsage >= best) {
                    best = vmi.CurrentUsage;
                    found_hw = true;
                    char buf[256] = {0};
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, buf,
                                        static_cast<int>(sizeof(buf)), nullptr, nullptr);
                    adapter_name = buf;
                }
            }
            a3->Release();
        }
        adapter->Release();
    }
    factory->Release();
    software = !found_hw;
    return best;
}
#else
unsigned long long QueryWorkingSet() { return 0; }
unsigned long QueryHandleCount() { return 0; }
unsigned long long QueryVramBytes(std::string&, bool&) { return 0; }
#endif

SoakSample Sample() {
    SoakSample s;
    s.working_set = QueryWorkingSet();
    s.handle_count = QueryHandleCount();
    std::string name;
    bool sw = true;
    s.vram_bytes = QueryVramBytes(name, sw);
    g_soak.dxgi_adapter = name;
    g_soak.dxgi_software = sw;
    return s;
}

void MakeTwoQuads(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    auto add_quad = [&](float x0, float x1, const glm::vec3& n) {
        const uint16_t base = static_cast<uint16_t>(verts.size());
        const float y0 = -0.7f, y1 = 0.7f;
        verts.push_back({{x0, y0, 0.0f}, col, {0.0f, 0.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x1, y0, 0.0f}, col, {1.0f, 0.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x1, y1, 0.0f}, col, {1.0f, 1.0f}, n, {0.0f, 1.0f, 0.0f}});
        verts.push_back({{x0, y1, 0.0f}, col, {0.0f, 1.0f}, n, {0.0f, 1.0f, 0.0f}});
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    };
    add_quad(-0.9f, -0.1f, glm::vec3(1.0f, 0.0f, 0.0f));
    add_quad(0.1f, 0.9f, glm::vec3(-1.0f, 0.0f, 0.0f));
}

// 单轮：整建 RT + MeshRenderer → 渲染 → 回读 → 整销。iter 用来轻微扰动光照，模拟场景切换。
RenderTargetReadback RenderOnce(RhiDevice& device, int iter) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};
    if (device.GetBuiltinProgram(BuiltinProgram::ForwardShaded) == 0) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    ShadedMaterial m;
    m.albedo = glm::vec3(0.8f);
    m.roughness = 0.5f;
    m.shading_mode = 0;

    DirectionalLight light;
    light.direction = glm::vec3(-1.0f, 0.0f, 0.0f);
    light.color = glm::vec3(1.0f);
    light.intensity = 2.0f + 0.01f * static_cast<float>(iter % 8);  // 场景切换扰动
    light.ambient = 0.05f;
    light.enabled = true;

    std::vector<ShadedPointLight> point_lights;
    ShadedPointLight pl;
    pl.color = glm::vec3(0.4f, 0.7f, 1.0f);
    pl.intensity = 3.0f;
    pl.position = glm::vec3(0.0f, 0.45f, 0.6f);
    pl.radius = 3.0f;
    point_lights.push_back(pl);

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeTwoQuads(verts, indices);

    const glm::mat4 I(1.0f);
    const glm::mat4 proj = device.GetProjectionCorrection();
    const glm::vec3 cam_pos(0.0f, 0.0f, 1.0f);

    MeshRenderer renderer;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.02f, 0.02f, 0.03f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        renderer.DrawShaded(*cmd, device, verts, indices, I, I, proj, cam_pos, m, light, point_lights);
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    renderer.Shutdown(device);
    device.DeleteRenderTarget(rt);
    return rb;
}

RenderTargetReadback SoakFn(RhiDevice& device) {
    g_soak = SoakCapture{};
    g_soak.info = device.GetDeviceInfo();
    const int iters = SoakIters();
    g_soak.iterations = iters;

    RenderTargetReadback last;
    for (int i = 0; i < kWarmup + iters; ++i) {
        last = RenderOnce(device, i);
        if (last.pixels.empty()) return {};  // ForwardShaded 不可用 → 让 CheckSoak 跳过
        if (i == kWarmup - 1) g_soak.baseline = Sample();
    }
    g_soak.final_ = Sample();
    g_soak.rendered_ok = !last.pixels.empty();
    return last;
}

double ToMiB(long long bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); }

void CheckSoak(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty())
        GTEST_SKIP() << "ForwardShaded builtin program unavailable (" << backend << ")";

    // 真机门禁：软件回退不能作为 P2-2 显存/句柄证据。
    EXPECT_FALSE(g_soak.info.is_software)
        << backend << ": 软件渲染回退不能作为真机 soak 证据（adapter="
        << g_soak.info.adapter_name << "）";
    EXPECT_FALSE(g_soak.dxgi_software)
        << backend << ": DXGI 未找到非软件适配器，显存计数不可信";

    const long long ws_delta =
        static_cast<long long>(g_soak.final_.working_set) -
        static_cast<long long>(g_soak.baseline.working_set);
    const long long handle_delta =
        static_cast<long long>(g_soak.final_.handle_count) -
        static_cast<long long>(g_soak.baseline.handle_count);
    const long long vram_delta =
        static_cast<long long>(g_soak.final_.vram_bytes) -
        static_cast<long long>(g_soak.baseline.vram_bytes);

    std::printf(
        "[P2-2] backend=%s adapter=\"%s\" software=%d dxgi=\"%s\" iters=%d\n"
        "       working_set: %.1f -> %.1f MiB (delta %.1f)\n"
        "       handles:     %lu -> %lu (delta %lld)\n"
        "       vram(local): %.1f -> %.1f MiB (delta %.1f)\n",
        backend, g_soak.info.adapter_name.c_str(), g_soak.info.is_software ? 1 : 0,
        g_soak.dxgi_adapter.c_str(), g_soak.iterations,
        ToMiB(static_cast<long long>(g_soak.baseline.working_set)),
        ToMiB(static_cast<long long>(g_soak.final_.working_set)), ToMiB(ws_delta),
        g_soak.baseline.handle_count, g_soak.final_.handle_count, handle_delta,
        ToMiB(static_cast<long long>(g_soak.baseline.vram_bytes)),
        ToMiB(static_cast<long long>(g_soak.final_.vram_bytes)), ToMiB(vram_delta));
    std::fflush(stdout);

    // 泄漏预算：热身后再跑 N 轮整建整销，三项指标不应随迭代线性增长。
    // 每轮 RT(256^2 RGBA+depth)≈0.75MiB，句柄若每轮泄漏则 ~N 线性增长；阈值远低于「每轮泄漏」量级。
    EXPECT_LE(handle_delta, 16)
        << backend << ": 内核句柄随迭代增长（每轮资源未释放？）iters=" << g_soak.iterations;
    EXPECT_LT(vram_delta, 24 * 1024 * 1024)
        << backend << ": 本地显存占用随迭代增长（GPU 资源泄漏？）iters=" << g_soak.iterations;
    EXPECT_LT(ws_delta, 48 * 1024 * 1024)
        << backend << ": 工作集随迭代增长（CPU 侧泄漏？）iters=" << g_soak.iterations;
}

}  // namespace

TEST(ThreeRhiSoak, OpenGL) { CheckSoak(dse::test::RunOpenGL(SoakFn), "opengl"); }
TEST(ThreeRhiSoak, D3D11) { CheckSoak(dse::test::RunD3D11(SoakFn), "d3d11"); }
TEST(ThreeRhiSoak, Vulkan) { CheckSoak(dse::test::RunVulkan(SoakFn), "vulkan"); }
