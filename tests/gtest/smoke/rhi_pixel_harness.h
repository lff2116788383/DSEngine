/**
 * @file rhi_pixel_harness.h
 * @brief 跨后端离屏像素冒烟「共享测试工具」。把 OpenGL / D3D11 / Vulkan 三后端的 GPU 上下文
 *        样板（建窗口/上下文、初始化 RhiDevice、无驱动优雅跳过、收尾销毁）抽出，
 *        让每个效果的像素 smoke 只需写「画什么（RenderFn）+ 校验什么（断言）」。
 *
 * 用法：
 *   auto fn = [](RhiDevice& d){ return RenderMyEffect(d); };  // 建 RT→画→回读
 *   auto r = dse::test::RunOpenGL(fn);
 *   if (!r.available) GTEST_SKIP() << r.skip_reason;
 *   VerifyMyEffect(r.readback, "OpenGL");
 *
 * 跨后端一致性（RMSE）：分别取三后端 readback，用 ComputeRmse 比较。
 * 注：软件渲染（llvmpipe/WARP/lavapipe）像素正确但 RMSE 不复现真机实测值
 *    （A1 RTX 3070 实测 GL vs DX11≈0.75、vs Vulkan≈2.1）。
 */
#pragma once

#include "engine/render/rhi/rhi_types.h"

#include <functional>

namespace dse {
namespace render {
class RhiDevice;
}  // namespace render
}  // namespace dse

namespace dse {
namespace test {

// 给定一个已初始化的 device：建离屏 RT → 渲染目标效果 → 回读像素并返回。
// 注：RenderTargetReadback 定义于全局命名空间（rhi_types.h 无 namespace）。
using RenderFn = std::function<::RenderTargetReadback(dse::render::RhiDevice&)>;

// 单后端执行结果。available=false 表示该后端无驱动/无法初始化，调用方应 GTEST_SKIP。
struct BackendResult {
    bool available = false;
    const char* skip_reason = "";
    ::RenderTargetReadback readback;
};

// 各后端：自建 GPU 上下文 + RhiDevice，调用 fn(device)，回读后销毁全部资源并返回 readback。
// 无对应驱动时返回 {available=false, skip_reason=...}（与既有 *_rhi_smoke_test 的优雅跳过同构）。
BackendResult RunOpenGL(const RenderFn& fn);
BackendResult RunD3D11(const RenderFn& fn);
BackendResult RunVulkan(const RenderFn& fn);

// 指向 (x,y) 处 RGBA8 texel 的指针；越界/空返回 nullptr。
const unsigned char* PixelAt(const ::RenderTargetReadback& rb, int x, int y);

// 两张同尺寸 readback 在全部 RGBA 字节上的 RMSE（0..255 标度）。尺寸不一致/空返回 -1。
double ComputeRmse(const ::RenderTargetReadback& a,
                   const ::RenderTargetReadback& b);

}  // namespace test
}  // namespace dse
