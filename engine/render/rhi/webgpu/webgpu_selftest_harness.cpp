/**
 * @file webgpu_selftest_harness.cpp
 * @brief WebGPU 后端自检 harness 实现（编译期门控 DSE_WEBGPU_SELFTEST，默认关闭）。
 *
 * 这些自检为 bring-up 期诊断代码（21 项：compute/GPU-cull/蒙皮/storage-image/Hi-Z/
 * morph/DDGI/hair/bloom/grass/multi-draw-indirect/mega-VAO/GPU-driven-PBR/CSM/延迟/HDR/
 * IBL/WBOIT 等），离屏隔离、不碰 demo backbuffer/golden。从 WebGPURhiDevice 外置至本
 * friend harness，使生产设备类只剩真实 RHI 实现；默认不编入出货构建。
 */
#include "engine/render/rhi/webgpu/webgpu_selftest_harness.h"

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU) && defined(DSE_WEBGPU_SELFTEST)

#include "engine/render/rhi/webgpu/webgpu_rhi_device.h"
#include "engine/base/debug.h"
#include "engine/render/rhi/draw_executor_common.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/hair/hair_compute_shaders.h"

#include <glm/glm.hpp>

#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dse {
namespace render {

namespace {
constexpr uint64_t AlignUp4(uint64_t n) { return (n + 3u) & ~static_cast<uint64_t>(3u); }  // 镜像 device 私有助手

// --- B3a compute 自检：异步回读校验上下文 ---
// 自检 compute 着色器把 outbuf[i] = i*2 + base（i<N），并把 indirect DrawCmd 写成
// {count=36, instance=1, first=0, base_vertex=0, base_instance=0}。两路 copy 到 MapRead
// 缓冲后各发起一次 wgpuBufferMapAsync，回调里逐元素校验。pending 计数归零后汇总并释放缓冲。
constexpr uint32_t kCtN = 64;       ///< 输出 SSBO 元素数（= 1 个 workgroup × 64 线程）
constexpr uint32_t kCtBase = 100u;  ///< 输出值偏置（验证 UBO 参数确实进入 compute）
constexpr uint32_t kCtDrawWords = 5;///< DrawCmd 字数（count/instance/first/base_vertex/base_instance）

struct ComputeSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
    WGPUBuffer rb_draw = nullptr;
    int pending = 2;
    bool out_ok = false;
    bool draw_ok = false;
};

void FinalizeComputeSelfTest(ComputeSelfTestCtx* ctx) {
    if (--ctx->pending > 0) return;
    if (ctx->out_ok && ctx->draw_ok) {
        DEBUG_LOG_INFO("WebGPU[B3a] compute 自检 PASS：SSBO 输出（n={}）与 indirect DrawCmd 均符合预期", kCtN);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检 FAIL：out_ok={} draw_ok={}", ctx->out_ok, ctx->draw_ok);
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    if (ctx->rb_draw) wgpuBufferRelease(ctx->rb_draw);
    delete ctx;
}

void OnComputeSelfTestOutMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<ComputeSelfTestCtx*>(userdata);
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint32_t* p = static_cast<const uint32_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kCtN * sizeof(uint32_t)));
        if (p) {
            bool ok = true;
            for (uint32_t i = 0; i < kCtN; ++i) {
                if (p[i] != i * 2u + kCtBase) { ok = false; break; }
            }
            ctx->out_ok = ok;
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检：输出 SSBO 回读映射失败 status={}", static_cast<int>(status));
    }
    FinalizeComputeSelfTest(ctx);
}

void OnComputeSelfTestDrawMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<ComputeSelfTestCtx*>(userdata);
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint32_t* p = static_cast<const uint32_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_draw, 0, kCtDrawWords * sizeof(uint32_t)));
        if (p) {
            ctx->draw_ok = (p[0] == 36u && p[1] == 1u && p[2] == 0u && p[3] == 0u && p[4] == 0u);
        }
        wgpuBufferUnmap(ctx->rb_draw);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检：indirect 回读映射失败 status={}", static_cast<int>(status));
    }
    FinalizeComputeSelfTest(ctx);
}

// --- B3b-2 GPU-driven 剔除自检：异步回读校验上下文 ---
// 4 个实例（AABB 已知），WGSL 视锥剔除写 per-instance indirect draw command 的 instance_count
// （视锥内=1、外=0；预期 [1,0,1,0]）。然后用真 DrawIndexedIndirect 把 4 个不同颜色/象限的 quad
// 渲到 64×64 离屏 RT（被剔实例 instance_count=0 → 硬件不绘制 → 该象限保持黑）。两路回读：
//   (1) draw commands SSBO → 校验 instance_count 模式 == 预期剔除结果；
//   (2) RT 像素 → 校验可见象限有对应颜色、被剔象限为黑。
// 证明真链路 compute(视锥剔除)→SSBO(indirect cmd)→DrawIndexedIndirect→像素 端到端正确。
constexpr uint32_t kGcInstances = 4;     ///< 实例数
constexpr uint32_t kGcDrawWords = 5;     ///< 每条 indirect command 字数
constexpr uint32_t kGcRtSize = 64;       ///< 离屏 RT 边长（64×4=256B/行，满足 copyTextureToBuffer 256 对齐）
constexpr uint32_t kGcRtRowBytes = kGcRtSize * 4;
constexpr uint32_t kGcRtBytes = kGcRtRowBytes * kGcRtSize;

struct GpuCullSelfTestCtx {
    WGPUBuffer rb_draw = nullptr;
    WGPUBuffer rb_pixels = nullptr;
    // 提交后才能安全释放的瞬态渲染资源（被命令缓冲引用至执行完成）。
    WGPUTexture rt_tex = nullptr;
    WGPUTextureView rt_view = nullptr;
    WGPURenderPipeline pipeline = nullptr;
    WGPUShaderModule module = nullptr;
    WGPUBuffer vbo = nullptr;
    WGPUBuffer ibo = nullptr;
    int pending = 2;
    bool draw_ok = false;
    bool pixels_ok = false;
};

void FinalizeGpuCullSelfTest(GpuCullSelfTestCtx* ctx) {
    if (--ctx->pending > 0) return;
    if (ctx->draw_ok && ctx->pixels_ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-2] GPU-driven 剔除自检 PASS：视锥剔除 instance_count 模式 [1,0,1,0] "
                       "+ DrawIndexedIndirect 离屏像素（可见象限有色、被剔象限为黑）均符合预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-2] GPU-driven 剔除自检 FAIL：draw_ok={} pixels_ok={}",
                        ctx->draw_ok, ctx->pixels_ok);
    }
    if (ctx->rb_draw)   wgpuBufferRelease(ctx->rb_draw);
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    if (ctx->rt_view)   wgpuTextureViewRelease(ctx->rt_view);
    if (ctx->rt_tex)    wgpuTextureRelease(ctx->rt_tex);
    if (ctx->pipeline)  wgpuRenderPipelineRelease(ctx->pipeline);
    if (ctx->module)    wgpuShaderModuleRelease(ctx->module);
    if (ctx->vbo)       wgpuBufferRelease(ctx->vbo);
    if (ctx->ibo)       wgpuBufferRelease(ctx->ibo);
    delete ctx;
}

void OnGpuCullDrawMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<GpuCullSelfTestCtx*>(userdata);
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint32_t* p = static_cast<const uint32_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_draw, 0, kGcInstances * kGcDrawWords * sizeof(uint32_t)));
        if (p) {
            // 每条 command 第 2 字（offset 1）= instance_count；预期 [1,0,1,0]。
            const uint32_t expected[kGcInstances] = {1u, 0u, 1u, 0u};
            bool ok = true;
            for (uint32_t i = 0; i < kGcInstances; ++i) {
                if (p[i * kGcDrawWords + 1] != expected[i]) { ok = false; break; }
            }
            ctx->draw_ok = ok;
        }
        wgpuBufferUnmap(ctx->rb_draw);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-2] 剔除自检：draw commands 回读映射失败 status={}", static_cast<int>(status));
    }
    FinalizeGpuCullSelfTest(ctx);
}

void OnGpuCullPixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<GpuCullSelfTestCtx*>(userdata);
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* px = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kGcRtBytes));
        if (px) {
            auto at = [&](uint32_t x, uint32_t y) -> const uint8_t* {
                return px + (y * kGcRtRowBytes + x * 4);
            };
            // 4 象限中心像素（RT 顶左原点）：inst0 TL=(16,16) 红、inst1 TR=(48,16) 绿(被剔→黑)、
            //   inst2 BL=(16,48) 蓝、inst3 BR=(48,48) 黄(被剔→黑)。
            const uint8_t* tl = at(16, 16);
            const uint8_t* tr = at(48, 16);
            const uint8_t* bl = at(16, 48);
            const uint8_t* br = at(48, 48);
            auto bright = [](const uint8_t* c, int ch) { return c[ch] > 100; };
            auto dark   = [](const uint8_t* c) { return c[0] < 40 && c[1] < 40 && c[2] < 40; };
            const bool ok = bright(tl, 0) &&  // inst0 可见：红
                            dark(tr) &&        // inst1 被剔：黑
                            bright(bl, 2) &&  // inst2 可见：蓝
                            dark(br);          // inst3 被剔：黑
            ctx->pixels_ok = ok;
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[B3b-2] 剔除自检像素：TL({},{},{}) TR({},{},{}) BL({},{},{}) BR({},{},{})",
                                tl[0], tl[1], tl[2], tr[0], tr[1], tr[2],
                                bl[0], bl[1], bl[2], br[0], br[1], br[2]);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-2] 剔除自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    FinalizeGpuCullSelfTest(ctx);
}

// --- B3b-3 GPU 蒙皮自检：异步回读校验上下文 ---
// 手译自 engine/render/shaders/src/skinning.comp 的 WGSL 蒙皮 compute（骨骼矩阵调色板 + morph
// 混合），把绑定空间的 quad（4 顶点，全 100% 权重于 bone0=平移(0.4,0.4)）变形写入 dst SSBO。
// dst SSBO 随即作顶点缓冲被真绘制消费渲到 64×64 离屏 RT。两路回读：
//   (1) dst SSBO → 逐顶点校验蒙皮后坐标 == 绑定坐标 + (0.4,0.4)（骨骼平移），法线保持 (0,0,1)；
//   (2) RT 像素 → 校验位移后的红色 quad 落在预期屏幕区域（中心有色、远角为黑）。
// 证明真链路 compute(蒙皮)→SSBO(变形顶点)→draw(顶点拉取)→像素 端到端正确。
constexpr uint32_t kSkVertices = 4;      ///< 蒙皮网格顶点数（一个 quad）
constexpr uint32_t kSkInstances = 1;     ///< 蒙皮实例数
constexpr uint32_t kSkBones = 2;         ///< 骨骼数（bone0 平移 / bone1 单位，仅 bone0 被引用）
constexpr uint32_t kSkSrcFloats = 16;    ///< 每源顶点 float 数（4×vec4：pos_bw0/norm_bw1/tan_bw2/joints_bw3）
constexpr uint32_t kSkDstFloats = 12;    ///< 每蒙皮后顶点 float 数（3×vec4：pos/normal/tangent）
constexpr uint32_t kSkDstStride = kSkDstFloats * sizeof(float);  ///< 蒙皮后顶点步幅（作顶点缓冲）
constexpr float kSkBoneTx = 0.4f;        ///< bone0 平移 x（验证可预测）
constexpr float kSkBoneTy = 0.4f;        ///< bone0 平移 y
constexpr float kSkHalf = 0.3f;          ///< quad 半边（绑定空间，居中原点）
constexpr uint32_t kSkRtSize = 64;       ///< 离屏 RT 边长（64×4=256B/行，满足 256 对齐）
constexpr uint32_t kSkRtRowBytes = kSkRtSize * 4;
constexpr uint32_t kSkRtBytes = kSkRtRowBytes * kSkRtSize;

struct SkinningSelfTestCtx {
    WGPUBuffer rb_dst = nullptr;
    WGPUBuffer rb_pixels = nullptr;
    // 提交后才能安全释放的瞬态渲染资源（被命令缓冲引用至执行完成）。
    WGPUTexture rt_tex = nullptr;
    WGPUTextureView rt_view = nullptr;
    WGPURenderPipeline pipeline = nullptr;
    WGPUShaderModule module = nullptr;
    WGPUBuffer ibo = nullptr;
    int pending = 2;
    bool dst_ok = false;
    bool pixels_ok = false;
};

void FinalizeSkinningSelfTest(SkinningSelfTestCtx* ctx) {
    if (--ctx->pending > 0) return;
    if (ctx->dst_ok && ctx->pixels_ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-3] GPU 蒙皮自检 PASS：骨骼调色板变形顶点（dst SSBO 逐顶点坐标"
                       "==绑定+平移、法线保持）+ 变形 quad 离屏像素（位移区域有色、远角为黑）均符合预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-3] GPU 蒙皮自检 FAIL：dst_ok={} pixels_ok={}",
                        ctx->dst_ok, ctx->pixels_ok);
    }
    if (ctx->rb_dst)    wgpuBufferRelease(ctx->rb_dst);
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    if (ctx->rt_view)   wgpuTextureViewRelease(ctx->rt_view);
    if (ctx->rt_tex)    wgpuTextureRelease(ctx->rt_tex);
    if (ctx->pipeline)  wgpuRenderPipelineRelease(ctx->pipeline);
    if (ctx->module)    wgpuShaderModuleRelease(ctx->module);
    if (ctx->ibo)       wgpuBufferRelease(ctx->ibo);
    delete ctx;
}

void OnSkinningDstMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<SkinningSelfTestCtx*>(userdata);
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const float* p = static_cast<const float*>(
            wgpuBufferGetConstMappedRange(ctx->rb_dst, 0, kSkVertices * kSkDstFloats * sizeof(float)));
        if (p) {
            // 绑定空间 quad 顶点（与 RecordSkinningSelfTest 一致），蒙皮后应平移 (kSkBoneTx, kSkBoneTy)。
            const float bx[kSkVertices] = {-kSkHalf,  kSkHalf, kSkHalf, -kSkHalf};
            const float by[kSkVertices] = {-kSkHalf, -kSkHalf, kSkHalf,  kSkHalf};
            bool ok = true;
            for (uint32_t i = 0; i < kSkVertices && ok; ++i) {
                const float* v = p + i * kSkDstFloats;  // pos.xyz @0..2、normal.xyz @4..6
                const float ex = bx[i] + kSkBoneTx, ey = by[i] + kSkBoneTy;
                if (std::fabs(v[0] - ex) > 0.01f || std::fabs(v[1] - ey) > 0.01f ||
                    std::fabs(v[2] - 0.0f) > 0.01f) { ok = false; break; }
                // 平移矩阵 3×3 为单位 → 法线保持 (0,0,1)。
                if (std::fabs(v[4]) > 0.01f || std::fabs(v[5]) > 0.01f ||
                    std::fabs(v[6] - 1.0f) > 0.01f) { ok = false; break; }
            }
            ctx->dst_ok = ok;
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[B3b-3] 蒙皮自检 dst 顶点：v0=({},{},{}) v2=({},{},{})",
                                p[0], p[1], p[2], p[2 * kSkDstFloats], p[2 * kSkDstFloats + 1],
                                p[2 * kSkDstFloats + 2]);
            }
        }
        wgpuBufferUnmap(ctx->rb_dst);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-3] 蒙皮自检：dst 顶点回读映射失败 status={}", static_cast<int>(status));
    }
    FinalizeSkinningSelfTest(ctx);
}

void OnSkinningPixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<SkinningSelfTestCtx*>(userdata);
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* px = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kSkRtBytes));
        if (px) {
            auto at = [&](uint32_t x, uint32_t y) -> const uint8_t* {
                return px + (y * kSkRtRowBytes + x * 4);
            };
            // 蒙皮后 quad NDC x∈[0.1,0.7] y∈[0.1,0.7]（绑定 ±0.3 + 平移 0.4）。
            // RT 顶左原点、NDC y 向上：中心 NDC(0.4,0.4) → 像素约 (44,19)。远角 (6,6)/(6,57)/(57,57) 应为黑。
            const uint8_t* in  = at(44, 19);
            const uint8_t* c0  = at(6, 6);
            const uint8_t* c1  = at(6, 57);
            const uint8_t* c2  = at(57, 57);
            auto red  = [](const uint8_t* c) { return c[0] > 100 && c[1] < 60 && c[2] < 60; };
            auto dark = [](const uint8_t* c) { return c[0] < 40 && c[1] < 40 && c[2] < 40; };
            const bool ok = red(in) && dark(c0) && dark(c1) && dark(c2);
            ctx->pixels_ok = ok;
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[B3b-3] 蒙皮自检像素：in({},{},{}) c0({},{},{}) c1({},{},{}) c2({},{},{})",
                                in[0], in[1], in[2], c0[0], c0[1], c0[2],
                                c1[0], c1[1], c1[2], c2[0], c2[1], c2[2]);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-3] 蒙皮自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    FinalizeSkinningSelfTest(ctx);
}

// --- B3b-4 storage-image 写 compute 自检：异步回读校验上下文 ---
// compute 经 `texture_storage_2d<rgba8unorm, write>` 把已知渐变（r=x/(N-1)、g=y/(N-1)、b=0.25）
// 逐像素 textureStore 进 storage 纹理，随后 copy 纹理→回读缓冲，逐像素校验。验证 compute 写
// storage image 端到端原语（Hi-Z 金字塔下采样 / bloom 的前置能力）。单路回读。
constexpr uint32_t kSiDim = 64;                 ///< storage 纹理边长（64×4=256B/行，满足 copy 256 对齐）
constexpr uint32_t kSiRowBytes = kSiDim * 4;
constexpr uint32_t kSiBytes = kSiRowBytes * kSiDim;

struct StorageImageSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnStorageImagePixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<StorageImageSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* px = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kSiBytes));
        if (px) {
            auto at = [&](uint32_t x, uint32_t y) -> const uint8_t* {
                return px + (y * kSiRowBytes + x * 4);
            };
            // 渐变：r=x/(N-1)*255、g=y/(N-1)*255、b=0.25*255≈64、a=255。校验四角 + 中心。
            const uint8_t* c00 = at(0, 0);                       // r≈0   g≈0
            const uint8_t* cx1 = at(kSiDim - 1, 0);              // r≈255 g≈0
            const uint8_t* cy1 = at(0, kSiDim - 1);              // r≈0   g≈255
            const uint8_t* cmid = at(kSiDim / 2, kSiDim / 2);    // r≈g≈~129
            const bool blue_a = c00[2] > 48 && c00[2] < 80 && c00[3] > 240;
            ok = c00[0] < 12 && c00[1] < 12 && blue_a &&
                 cx1[0] > 240 && cx1[1] < 12 &&
                 cy1[0] < 12 && cy1[1] > 240 &&
                 cmid[0] > 110 && cmid[0] < 150 && cmid[1] > 110 && cmid[1] < 150;
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[B3b-4] storage-image 自检像素：c00({},{},{},{}) "
                                "cx1({},{},{}) cy1({},{},{}) cmid({},{})",
                                c00[0], c00[1], c00[2], c00[3], cx1[0], cx1[1], cx1[2],
                                cy1[0], cy1[1], cy1[2], cmid[0], cmid[1]);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-4] storage-image 自检：像素回读映射失败 status={}",
                        static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-4] storage-image 写自检 PASS：compute textureStore 渐变"
                       "（r=x、g=y、b=0.25）写入 rgba8unorm storage 纹理 → 回读四角/中心像素均符合预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-4] storage-image 写自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- B3b-5 Hi-Z 下采样核心 compute 自检：异步回读校验上下文 ---
// 两趟 r32float compute：①生成趟把已知渐变 src[x,y]=f32(x)+f32(y)*100 写入 src 纹理；②下采样趟用
// textureLoad 读 src + 取 2×2 max 写 dst（边长减半）。回读 dst（4×4 r32float），逐像素校验
// dst[X,Y]==max(2×2)==(2X+1)+(2Y+1)*100（每块右下角即最大）。验证 compute 采样读 + r32float storage
// 写的读后写链路（Hi-Z 金字塔逐级下采样核心原语）。单路回读。
constexpr uint32_t kHzSrcDim = 8;                  ///< src 纹理边长
constexpr uint32_t kHzDstDim = 4;                  ///< dst 纹理边长（src/2）
constexpr uint32_t kHzDstRowBytes = 256;           ///< 4×4B=16B/行 → 填充至 copy 256 对齐
constexpr uint32_t kHzDstBytes = kHzDstRowBytes * kHzDstDim;

struct HiZSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnHiZPixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<HiZSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const float* px = static_cast<const float*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kHzDstBytes));
        if (px) {
            ok = true;
            for (uint32_t y = 0; y < kHzDstDim && ok; ++y) {
                for (uint32_t x = 0; x < kHzDstDim; ++x) {
                    const float got = px[y * (kHzDstRowBytes / 4) + x];
                    const float exp = static_cast<float>(2u * x + 1u) +
                                      static_cast<float>(2u * y + 1u) * 100.0f;
                    if (std::abs(got - exp) > 0.01f) {
                        DEBUG_LOG_ERROR("WebGPU[B3b-5] Hi-Z 下采样自检像素失配：dst({},{}) got={} exp={}",
                                        x, y, got, exp);
                        ok = false;
                        break;
                    }
                }
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-5] Hi-Z 下采样自检：像素回读映射失败 status={}",
                        static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-5] Hi-Z 下采样核心自检 PASS：compute textureLoad 读 r32float 采样纹理 "
                       "+ 2×2 max → r32float storage 写，回读 dst 全 16 像素均 == CPU 预期 max");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-5] Hi-Z 下采样核心自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- B3b-6 Hi-Z storage-image 金字塔 compute 自检：异步回读校验上下文 ---
// 单张 R32Float mip 链纹理（mip0=8×8 → mip1=4×4 → mip2=2×2 → mip3=1×1）。①生成趟 textureStore
// 渐变 mip0[x,y]=f32(x)+f32(y)*100；②逐级下采样趟 textureLoad 读 mip[k-1] 单 mip 视图 + 取 2×2 max
// 写 mip[k] 单 mip storage 视图。各级 mip copy 到单一回读缓冲的 256 对齐分段后单路回读，逐级逐像素
// 校验 == CPU 预期递归 max。因渐变沿 x/y 单调增，第 k 级 [x,y] 的 max 即所覆 2^k×2^k 块右下角：
// col=(x+1)*2^k-1，row=(y+1)*2^k-1 → 值 = col + row*100。验证 per-mip 视图绑定 + 多级金字塔构建。
constexpr uint32_t kHzpBaseDim = 8;       ///< mip0 边长
constexpr uint32_t kHzpMips = 4;          ///< mip 级数（8,4,2,1）
constexpr uint32_t kHzpRowBytes = 256;    ///< 每级 copy 行字节（最大 8×4B=32B → 填充至 256 对齐）
// 各级 mip 在回读缓冲内的 256 对齐分段偏移：mip k 占 kHzpRowBytes × dim_k 字节。
constexpr uint32_t kHzpMipOffset(uint32_t k) {
    uint32_t off = 0;
    for (uint32_t i = 0; i < k; ++i) off += kHzpRowBytes * (kHzpBaseDim >> i);
    return off;
}
constexpr uint32_t kHzpTotalBytes = kHzpMipOffset(kHzpMips);

struct HiZPyramidSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnHiZPyramidPixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<HiZPyramidSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const float* px = static_cast<const float*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kHzpTotalBytes));
        if (px) {
            ok = true;
            for (uint32_t k = 0; k < kHzpMips && ok; ++k) {
                const uint32_t dim = kHzpBaseDim >> k;
                const uint32_t step = 1u << k;
                const uint32_t base = kHzpMipOffset(k) / 4u;  // float 索引
                for (uint32_t y = 0; y < dim && ok; ++y) {
                    for (uint32_t x = 0; x < dim; ++x) {
                        const float got = px[base + y * (kHzpRowBytes / 4) + x];
                        const float col = static_cast<float>((x + 1u) * step - 1u);
                        const float row = static_cast<float>((y + 1u) * step - 1u);
                        const float exp = col + row * 100.0f;
                        if (std::abs(got - exp) > 0.01f) {
                            DEBUG_LOG_ERROR("WebGPU[B3b-6] Hi-Z 金字塔自检像素失配：mip{} ({},{}) got={} exp={}",
                                            k, x, y, got, exp);
                            ok = false;
                            break;
                        }
                    }
                }
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-6] Hi-Z 金字塔自检：像素回读映射失败 status={}",
                        static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-7] Hi-Z storage-image 金字塔自检 PASS：句柄 SetComputeTextureImageMip "
                       "（按 mip 级单层视图绑定）+ {} 级逐级 2×2 max 下采样，回读各级 mip 全像素均 == "
                       "CPU 预期递归 max（mip0..{}）",
                       kHzpMips, kHzpMips - 1);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-6] Hi-Z storage-image 金字塔自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- B3b-8 命名 uniform + compute 采样器绑定 自检（常量 / ctx / 回读校验回调）---
//   采样纹理：kCbTexDim×kCbTexDim rgba8unorm 已知渐变 texel(x,y)=(x*40,y*60,(x+y)*20,255)；
//   命名 uniform：a_int=12345 / b_float=3.5 / c_coord=(2,1) / d_vec4=(1,2,3,4) / e_mat=单位阵。
//   compute 取 c_coord 处 texel + 单位阵×d_vec4 写 9×u32 结果 SSBO。
constexpr uint32_t kCbTexDim   = 4;
constexpr uint32_t kCbOutCount = 9;                 // u32：[0]a_int [1]bits(b_float) [2..4]texel rgb [5..8]bits(mat*vec)
constexpr uint32_t kCbOutBytes = kCbOutCount * 4u;
constexpr int      kCbAInt     = 12345;
constexpr float    kCbBFloat   = 3.5f;
constexpr int      kCbCX = 2, kCbCY = 1;            // c_coord：采样纹理坐标
inline uint8_t CbTexR(uint32_t x) { return static_cast<uint8_t>(x * 40u); }
inline uint8_t CbTexG(uint32_t y) { return static_cast<uint8_t>(y * 60u); }
inline uint8_t CbTexB(uint32_t x, uint32_t y) { return static_cast<uint8_t>((x + y) * 20u); }

struct ComputeBindSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
};

void OnComputeBindMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<ComputeBindSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint32_t* o = static_cast<const uint32_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kCbOutBytes));
        if (o) {
            auto bits2f = [](uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; };
            const float b_float = bits2f(o[1]);
            const float vx = bits2f(o[5]), vy = bits2f(o[6]), vz = bits2f(o[7]), vw = bits2f(o[8]);
            const bool int_ok   = (static_cast<int>(o[0]) == kCbAInt);
            const bool float_ok = (std::abs(b_float - kCbBFloat) < 1e-4f);
            const bool tex_ok   = (o[2] == CbTexR(kCbCX)) && (o[3] == CbTexG(kCbCY)) &&
                                  (o[4] == CbTexB(kCbCX, kCbCY));
            const bool mat_ok   = (std::abs(vx - 1.0f) < 1e-4f) && (std::abs(vy - 2.0f) < 1e-4f) &&
                                  (std::abs(vz - 3.0f) < 1e-4f) && (std::abs(vw - 4.0f) < 1e-4f);
            ok = int_ok && float_ok && tex_ok && mat_ok;
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[B3b-8] 命名 uniform/采样自检失配：int={}(exp {}) float={}(exp {}) "
                                "texel=({},{},{})(exp {},{},{}) mat*vec=({},{},{},{})(exp 1,2,3,4)",
                                static_cast<int>(o[0]), kCbAInt, b_float, kCbBFloat,
                                o[2], o[3], o[4], CbTexR(kCbCX), CbTexG(kCbCY), CbTexB(kCbCX, kCbCY),
                                vx, vy, vz, vw);
            }
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-8] 命名 uniform/采样自检：结果回读映射失败 status={}",
                        static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-8] 命名 uniform + compute 采样器绑定自检 PASS：SetComputeUniform* "
                       "（i32/f32/vec2i/vec4/mat4 命名块经 group1 保留 binding）+ SetComputeTextureSampler "
                       "（句柄绑定 group2，textureLoad 采样）均经 compute 读出且 == CPU 预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-8] 命名 uniform + compute 采样器绑定自检 FAIL");
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    delete ctx;
}

// --- B3b-9 Hi-Z 遮挡剔除真链路自检（常量 / ctx / 回读校验回调）---
//   引擎 HiZCullPass 真实 compute（AABB 投影 8 角 → NDC/UV → off-screen 拒绝 → mip 选择 →
//   多 tab Hi-Z max 深度遮挡判定）经手译 WGSL 接入。自检布置：单 mip r32float Hi-Z 纹理恒值 0.5、
//   单位 VP（NDC==世界坐标）、4 个已知 AABB（前/后/出屏/中），dispatch 后回读 4×u32 可见性 ==
//   CPU 预期 [可见,遮挡,出屏,可见]=[1,0,0,1]。
constexpr uint32_t kHcObjCount = 4;
constexpr uint32_t kHcVisBytes = kHcObjCount * 4u;
constexpr uint32_t kHcHizDim   = 8;        // Hi-Z 纹理边长（单 mip）
constexpr float    kHcHizDepth = 0.5f;     // Hi-Z 恒值深度
const uint32_t     kHcExpected[kHcObjCount] = {1u, 0u, 0u, 1u};

struct HiZCullSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
};

void OnHiZCullMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<HiZCullSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint32_t* v = static_cast<const uint32_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kHcVisBytes));
        if (v) {
            ok = true;
            for (uint32_t i = 0; i < kHcObjCount; ++i) if (v[i] != kHcExpected[i]) ok = false;
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[B3b-9] Hi-Z 剔除自检失配：vis=({},{},{},{}) exp=(1,0,0,1)",
                                v[0], v[1], v[2], v[3]);
            }
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-9] Hi-Z 剔除自检：结果回读映射失败 status={}",
                        static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-9] Hi-Z 遮挡剔除自检 PASS：引擎 HiZCullPass 真 compute 逻辑"
                       "（AABB 8 角投影 + NDC/UV + off-screen 拒绝 + mip 选择 + 多 tab Hi-Z max 深度"
                       "判定）经手译 WGSL 经 SetComputeUniform*/SetComputeTextureSampler/SSBO 跑出可见性 == CPU 预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-9] Hi-Z 遮挡剔除自检 FAIL");
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    delete ctx;
}

// --- Task 4 Subtask 4 Hi-Z 遮挡剔除真资源链路自检（常量 / ctx / 回读校验回调）---
//   经 CreateHiZTexture 真资源建 8×8 R32Float 完整 mip 链（4 级）→ 引擎 HiZBuildPass 真实绑定面
//   SetComputeTextureImageMip 写 mip0（texel(0,0)=0.9 占位遮挡深度、其余 0.1）+ 逐级 2×2 max 下采样
//   （0.9 经 3 级金字塔传到 1×1 顶 mip）→ HiZCullPass 手译 WGSL 经 SetComputeTextureSampler
//   (GetHiZGpuTexture) + GetHiZMipCount 采样金字塔判遮挡。3 个 AABB：①近物 z[-0.05,0]（test_depth≈
//   0.47 < 顶 mip 0.9 → 可见）；②远物 z[0.85,0.9]（test_depth≈0.92 > 0.9 → 被金字塔遮挡剔除）；
//   ③出屏。回读 3×u32 可见性 == [1,0,0]。近物可见恰证下采样把 0.9 传至顶 mip（否则误剔→失配可检）。
constexpr uint32_t kT44HizDim   = 8;
constexpr uint32_t kT44Mips     = 4;          // 8,4,2,1
constexpr uint32_t kT44ObjCount = 3;
constexpr uint32_t kT44VisBytes = kT44ObjCount * 4u;
const uint32_t     kT44Expected[kT44ObjCount] = {1u, 0u, 0u};

struct GpuDrivenHiZCullSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
};

void OnT44HiZCullMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<GpuDrivenHiZCullSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint32_t* v = static_cast<const uint32_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kT44VisBytes));
        if (v) {
            ok = true;
            for (uint32_t i = 0; i < kT44ObjCount; ++i) if (v[i] != kT44Expected[i]) ok = false;
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T4-4] Hi-Z 剔除自检失配：vis=({},{},{}) exp=(1,0,0)",
                                v[0], v[1], v[2]);
            }
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T4-4] Hi-Z 剔除自检：结果回读映射失败 status={}",
                        static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T4-4] GPU-driven Hi-Z 遮挡剔除自检 PASS：CreateHiZTexture 真资源建 "
                       "R32Float mip 链 + SetComputeTextureImageMip 写 mip0 + 逐级 2×2 max 下采样建金字塔 "
                       "+ HiZCullPass WGSL 经 GetHiZGpuTexture/GetHiZMipCount 采样判遮挡（近物可见/远物剔除）"
                       "符合预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T4-4] GPU-driven Hi-Z 遮挡剔除自检 FAIL");
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    delete ctx;
}

// --- B3b-10 形变目标（morph target）真链路自检（常量 / ctx / 回读校验回调）---
//   引擎 MorphTargetSystem 真实 compute（base 顶点 + Σ weight·delta → normalize 法线 → 写形变顶点）
//   经手译 WGSL 接入（morph_target_system.cpp::kMorphTargetCompWGSL）。自检布置：4 顶点、2 形变目标，
//   weights=[0.5,1.0]，dispatch 后回读 4×DeformedVertex（每 48B）逐顶点校验 pos==base+Σw·Δpos、
//   法线归一化、w==1、tangent 透传。证明该消费方着色器 WebGPU 可用。离屏隔离，不翻能力位。
constexpr uint32_t kMfVtxCount = 4;
constexpr uint32_t kMfTgtCount = 2;
constexpr uint32_t kMfOutBytes = kMfVtxCount * 48u;   // DeformedVertex = 3×vec4 = 48B
// CPU 预期形变后位置：base_i + 0.5·(1,0,0) + 1.0·(0,2,0) = base_i + (0.5, 2.0, 0)
const float kMfExpectedPos[kMfVtxCount][3] = {
    {1.5f, 2.0f, 0.0f}, {0.5f, 3.0f, 0.0f}, {0.5f, 2.0f, 1.0f}, {1.5f, 3.0f, 1.0f},
};

struct MorphSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
};

void OnMorphMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<MorphSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const float* v = static_cast<const float*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kMfOutBytes));
        if (v) {
            ok = true;
            for (uint32_t i = 0; i < kMfVtxCount && ok; ++i) {
                const float* p = v + i * 12u;            // pos.xyzw(0..3) normal.xyzw(4..7) tan(8..11)
                for (int c = 0; c < 3; ++c) if (std::abs(p[c] - kMfExpectedPos[i][c]) > 0.01f) ok = false;
                if (std::abs(p[3] - 1.0f) > 0.01f) ok = false;                 // position.w==1
                if (std::abs(p[6] - 1.0f) > 0.01f) ok = false;                 // normal==(0,0,1)
                if (!ok) {
                    DEBUG_LOG_ERROR("WebGPU[B3b-10] morph 自检顶点{} 失配：pos=({},{},{},{}) nrm.z={} "
                                    "exp_pos=({},{},{})", i, p[0], p[1], p[2], p[3], p[6],
                                    kMfExpectedPos[i][0], kMfExpectedPos[i][1], kMfExpectedPos[i][2]);
                }
            }
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-10] morph 自检：结果回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-10] 形变目标自检 PASS：引擎 MorphTargetSystem 真 compute 逻辑"
                       "（base + Σ weight·delta → normalize 法线 → 写形变顶点）经手译 WGSL 经"
                       " SetComputeUniformInt + 4×SSBO 跑出形变顶点 == CPU 预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-10] 形变目标自检 FAIL");
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    delete ctx;
}

// --- B3b-11 DDGI 探针更新（probe update）核心真链路自检（常量 / ctx / 回读校验回调）---
//   引擎 DDGISystem 真实 compute（ddgi_system.cpp::kDDGIUpdateComputeSource）核心：probe SSBO 读探针位
//   → 每 texel 经 octahedral 解码方向 → 从 RSM（位置/法线/通量，句柄采样 textureLoad）随机采样 VPL 累积
//   间接辐照度（cos·cos·平方衰减加权）→ 归一化 ×RSM 面积因子×0.01 → 写 irradiance/visibility storage image。
//   自检布置：1 探针、grid(1,1,1)、irradiance/visibility texels=4、RSM 2×2（4 样本，全同 VPL）、hysteresis=0
//   （绕开 temporal imageLoad 需 read-write storage 的限制）。除写 storage image 外另写一份 float SSBO（每
//   texel irr.rgb + total_weight）精校验：全同样本下权重在归一化中抵消 → 命中 texel 的 irr == flux×0.01，
//   其余（octahedral z≤0 不接收）== 0。证明该消费方核心 compute 逻辑 WebGPU 可用。离屏隔离，不翻能力位。
//   注：DDGI 真正翻转能力位前另需消费方两处适配——(1) storage image 与 RSM sampler 在 group2 的绑定槽错开
//   （现状同号 0/1/2 在 WebGPU 撞槽）；(2) temporal 混合的 imageLoad 需 ping-pong 或 read-write storage。
constexpr uint32_t kDgIrrTexels  = 4;                          // 每探针 irradiance texel 边长
constexpr uint32_t kDgVisTexels  = 4;
constexpr uint32_t kDgRsmDim     = 2;                          // RSM 2×2
constexpr uint32_t kDgTexelCount = kDgIrrTexels * kDgIrrTexels; // 16 texel
constexpr uint32_t kDgDbgBytes   = kDgTexelCount * 16u;        // 每 texel vec4<f32>（irr.rgb + total_weight）
// 自检 RSM 单像素值（rgba8unorm 字节）：通量 (200,100,50)/255。命中 texel 的预期 irr = 通量×0.01。
const float kDgFluxDequant[3] = {200.0f / 255.0f, 100.0f / 255.0f, 50.0f / 255.0f};

struct DDGISelfTestCtx {
    WGPUBuffer rb_out = nullptr;
};

void OnDDGIMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<DDGISelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const float* v = static_cast<const float*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kDgDbgBytes));
        if (v) {
            ok = true;
            int receiving = 0;
            for (uint32_t ty = 0; ty < kDgIrrTexels && ok; ++ty) {
                for (uint32_t tx = 0; tx < kDgIrrTexels && ok; ++tx) {
                    // octahedral 解码前的 n.z = 1-|fx|-|fy|；>0 即该方向接收来自探针方向的 VPL 贡献。
                    const float fx = (static_cast<float>(tx) + 0.5f) / float(kDgIrrTexels) * 2.0f - 1.0f;
                    const float fy = (static_cast<float>(ty) + 0.5f) / float(kDgIrrTexels) * 2.0f - 1.0f;
                    const bool receives = (1.0f - std::abs(fx) - std::abs(fy)) > 1e-5f;
                    const float* p = v + (ty * kDgIrrTexels + tx) * 4u;  // irr.rgb(0..2) total_weight(3)
                    if (receives) ++receiving;
                    for (int c = 0; c < 3; ++c) {
                        const float exp = receives ? kDgFluxDequant[c] * 0.01f : 0.0f;
                        if (std::abs(p[c] - exp) > 5e-4f) ok = false;
                    }
                    if (!ok) {
                        DEBUG_LOG_ERROR("WebGPU[B3b-11] DDGI 自检 texel({},{}) 失配：irr=({},{},{}) w={} "
                                        "receives={}", tx, ty, p[0], p[1], p[2], p[3],
                                        static_cast<int>(receives));
                    }
                }
            }
            if (ok && receiving != 4) {  // 内 2×2 共 4 个 texel 应接收 VPL
                ok = false;
                DEBUG_LOG_ERROR("WebGPU[B3b-11] DDGI 自检接收 texel 数 {} != 4", receiving);
            }
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-11] DDGI 自检：结果回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-11] DDGI 探针更新自检 PASS：引擎 DDGISystem 核心 compute 逻辑"
                       "（probe SSBO + octahedral 方向 + RSM 句柄采样 VPL 累积间接辐照度 → 归一化×0.01 →"
                       " 写 storage image）经手译 WGSL 经 14 命名 uniform + 3×RSM 采样 + 2×storage image 跑出"
                       " irr == CPU 预期（命中 texel == flux×0.01，余 0）");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-11] DDGI 探针更新自检 FAIL");
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    delete ctx;
}

// --- B3b-12 头发物理（hair）全 4 趟真链路自检（常量 / CPU 复算参考 / ctx / 回读校验回调）---
//   引擎 HairInstance::Simulate 真实 compute（hair_compute_shaders.h::kHair*SourceWGSL，本自检直接取
//   引擎真源 4 个 WGSL，非另写镜像）。按 hair_system.cpp::SimulateCompute 同序在同一 compute pass 内跑
//   全 4 趟（趟间 WebGPU 自动按 storage 资源依赖串行化）：
//     ①integrate（4×SSBO b0..3 + 12 uniform）根顶点固定 / velocity·(1-damping)+重力·dt²+风力·dt²；
//     ②local_shape（cur/rest/strand b0..2 + 3 uniform）向 rest+root_offset / rest 双 mix 拉回；
//     ③length（cur/rest/strand b0..2 + 1 uniform）逐段长度约束（i==0 仅移子，余对半修正）；
//     ④tangent（cur/tangent/strand b0..2 + 3 uniform）相邻差分写归一化切线、保留 .w 厚度。
//   自检布置：1 strand / 4 顶点（v0 根 + v1..v3）、dt=1、damping=0.2、重力(0,-1,0)·2、风=0（绕开
//   hash11 风扰动对校验的影响，仍保留 hash 路径执行）、st_local=0.5/st_global=0.3、v1 给非零初速度验阻尼。
//   末趟后回读 pos_cur + tangent 共 8×vec4，逐分量校验 == C++ 同序复算（HairRefChain，逐句镜像 4 趟）。
//   证明该消费方全 4 趟 compute 链路 WebGPU 可用。离屏隔离、不翻能力位。
constexpr uint32_t kHrStrands       = 1;
constexpr uint32_t kHrVerts         = 4;                    // 1 strand × 4 顶点
constexpr int      kHrVertsPerStr   = 4;
constexpr uint32_t kHrPosBytes      = kHrVerts * 16u;       // 每缓冲 4×vec4<f32>
constexpr uint32_t kHrRbBytes       = kHrPosBytes * 2u;     // pos_cur + tangent
constexpr float    kHrDt            = 1.0f;
constexpr float    kHrDamping       = 0.2f;
constexpr float    kHrGw            = 2.0f;                 // 重力幅值（方向 (0,-1,0)）
constexpr float    kHrStLocal       = 0.5f;
constexpr float    kHrStGlobal      = 0.3f;
// 初始缓冲（xyzw）。pos_*.w：根 w=0（判根）、非根 w=1；tangent.w=厚度（4 趟后须保留）。
const float kHrInitCur[kHrVerts * 4] = {
    0.0f, 4.0f, 0.0f, 0.0f,   // v0 根
    0.0f, 3.0f, 0.0f, 1.0f,
    0.0f, 2.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
};
const float kHrInitPrev[kHrVerts * 4] = {
    0.0f, 4.0f, 0.0f, 0.0f,
    0.0f, 3.1f, 0.0f, 1.0f,   // v1 非零初速度（prev.y=3.1 → vel.y=-0.1）验阻尼
    0.0f, 2.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
};
const float kHrInitRest[kHrVerts * 4] = {
    0.0f, 4.0f, 0.0f, 0.0f,   // 段静止长度均 1（相邻 y 差 1）
    0.0f, 3.0f, 0.0f, 1.0f,
    0.0f, 2.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
};
const float kHrInitTan[kHrVerts * 4] = {
    0.0f, 0.0f, 0.0f, 1.00f,  // 仅 .w 厚度须被保留
    0.0f, 0.0f, 0.0f, 0.75f,
    0.0f, 0.0f, 0.0f, 0.50f,
    0.0f, 0.0f, 0.0f, 0.25f,
};

// C++ 同序复算全 4 趟（逐句镜像 kHair*SourceWGSL）：out_cur/out_tan 各 4×vec4。
inline void HairRefChain(float out_cur[kHrVerts * 4], float out_tan[kHrVerts * 4]) {
    glm::vec3 cur[kHrVerts], prev[kHrVerts], rest[kHrVerts];
    float cur_w[kHrVerts], tan_w[kHrVerts];
    for (uint32_t i = 0; i < kHrVerts; ++i) {
        cur[i]  = glm::vec3(kHrInitCur[i*4], kHrInitCur[i*4+1], kHrInitCur[i*4+2]);
        prev[i] = glm::vec3(kHrInitPrev[i*4], kHrInitPrev[i*4+1], kHrInitPrev[i*4+2]);
        rest[i] = glm::vec3(kHrInitRest[i*4], kHrInitRest[i*4+1], kHrInitRest[i*4+2]);
        cur_w[i] = kHrInitCur[i*4+3];
        tan_w[i] = kHrInitTan[i*4+3];
    }
    const glm::vec3 grav = glm::vec3(0.0f, -1.0f, 0.0f) * kHrGw * kHrDt * kHrDt;  // 风=0
    // ① integrate
    for (uint32_t i = 0; i < kHrVerts; ++i) {
        if (kHrInitRest[i*4+3] < 0.001f) continue;  // 根：cur 不动（pos_prev←cur 与 cur 输出无关）
        glm::vec3 vel = (cur[i] - prev[i]) * (1.0f - kHrDamping);
        cur[i] = cur[i] + vel + grav;
    }
    // ② local_shape（strand 0，root_offset 双 mix）
    {
        const uint32_t off = 0u, cnt = kHrVerts;
        glm::vec3 root_off = cur[off] - rest[off];
        for (uint32_t i = 1u; i < cnt; ++i) {
            glm::vec3 n = cur[off+i];
            n = glm::mix(n, rest[off+i] + root_off, kHrStLocal * 0.1f);
            n = glm::mix(n, rest[off+i],            kHrStGlobal * 0.02f);
            cur[off+i] = n;
        }
    }
    // ③ length（strand 0，i==0 仅移子，余对半修正）
    {
        const uint32_t off = 0u, cnt = kHrVerts;
        for (uint32_t i = 0u; i + 1u < cnt; ++i) {
            glm::vec3 p0 = cur[off+i], p1 = cur[off+i+1u];
            float rl = glm::length(rest[off+i+1u] - rest[off+i]);
            glm::vec3 d = p1 - p0;
            float cl = glm::length(d);
            if (cl < 1e-7f) continue;
            glm::vec3 tgt = p0 + d / cl * rl;
            if (i == 0u) {
                cur[off+i+1u] = tgt;
            } else {
                glm::vec3 c = (tgt - p1) * 0.5f;
                cur[off+i]    = p0 - c;
                cur[off+i+1u] = p1 + c;
            }
        }
    }
    // ④ tangent（相邻差分，保留 .w）
    glm::vec3 tan[kHrVerts];
    for (uint32_t vid = 0; vid < kHrVerts; ++vid) {
        uint32_t li = vid % static_cast<uint32_t>(kHrVertsPerStr);
        uint32_t cnt = kHrVerts;
        if (li < cnt - 1u)      tan[vid] = glm::normalize(cur[vid+1u] - cur[vid]);
        else if (li > 0u)       tan[vid] = glm::normalize(cur[vid] - cur[vid-1u]);
        else                    tan[vid] = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    for (uint32_t i = 0; i < kHrVerts; ++i) {
        out_cur[i*4]   = cur[i].x; out_cur[i*4+1] = cur[i].y; out_cur[i*4+2] = cur[i].z; out_cur[i*4+3] = cur_w[i];
        out_tan[i*4]   = tan[i].x; out_tan[i*4+1] = tan[i].y; out_tan[i*4+2] = tan[i].z; out_tan[i*4+3] = tan_w[i];
    }
}

struct HairSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
};

void OnHairMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<HairSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const float* v = static_cast<const float*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kHrRbBytes));
        if (v) {
            float exp_cur[kHrVerts * 4], exp_tan[kHrVerts * 4];
            HairRefChain(exp_cur, exp_tan);
            ok = true;
            const float* c = v;                  // pos_cur 区
            for (uint32_t i = 0; i < kHrVerts && ok; ++i) {
                for (int e = 0; e < 4; ++e) {
                    if (std::abs(c[i*4+e] - exp_cur[i*4+e]) > 1e-4f) {
                        ok = false;
                        DEBUG_LOG_ERROR("WebGPU[B3b-12] hair 自检 pos_cur[{}] 失配：({},{},{},{}) 期望"
                                        "({},{},{},{})", i, c[i*4], c[i*4+1], c[i*4+2], c[i*4+3],
                                        exp_cur[i*4], exp_cur[i*4+1], exp_cur[i*4+2], exp_cur[i*4+3]);
                    }
                }
            }
            const float* t = v + kHrVerts * 4u;  // tangent 区
            for (uint32_t i = 0; i < kHrVerts && ok; ++i) {
                for (int e = 0; e < 4; ++e) {
                    if (std::abs(t[i*4+e] - exp_tan[i*4+e]) > 1e-4f) {
                        ok = false;
                        DEBUG_LOG_ERROR("WebGPU[B3b-12] hair 自检 tangent[{}] 失配：({},{},{},{}) 期望"
                                        "({},{},{},{})", i, t[i*4], t[i*4+1], t[i*4+2], t[i*4+3],
                                        exp_tan[i*4], exp_tan[i*4+1], exp_tan[i*4+2], exp_tan[i*4+3]);
                    }
                }
            }
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-12] hair 自检：结果回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-12] 头发物理自检 PASS：引擎 HairInstance 全 4 趟真 compute 链路"
                       "（integrate→local_shape→length→tangent）经手译 WGSL 同序跑出 pos_cur/tangent == CPU 复算");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-12] 头发物理自检 FAIL");
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    delete ctx;
}

// --- B3b-14 草地风场（grass wind）compute 正确性真链路自检（常量 / CPU 复算参考 / ctx / 回读校验回调）---
//   grass_system.cpp 的风场 WGSL 在 modules 层（engine 层不能 include），故 RecordGrassSelfTest 内
//   **内联一份镜像**（与 kGrassWindComputeSourceWGSL 逐句一致）。布置：4 个已知实例（pos/yaw/w/h/
//   wind_phase/fade）+ 已知 uniform（wind_dir/speed/strength/turbulence/time/count）→ 每实例算 h*fade
//   折叠 hf 的列主序风偏 mat4 → 回读逐元素校验 == C++ 同公式复算（GrassRefMatrix，镜像 CPU BuildWindMatrix）。
constexpr uint32_t kGrInstances = 4;
constexpr uint32_t kGrInBytes   = kGrInstances * 32u;       // 每实例 2×vec4（pos_yaw + wh_phase_fade）
constexpr uint32_t kGrOutBytes  = kGrInstances * 64u;       // 每实例 mat4（列主序，16 float）
// 已知实例：每行 8 float = pos.x,pos.y,pos.z,yaw, w,h,wind_phase,fade。
const float kGrInst[kGrInstances * 8] = {
     1.0f, 0.0f,  2.0f,  0.3f,   0.5f, 2.0f, 0.7f, 0.8f,
     3.0f, 0.0f,  4.0f,  1.1f,   0.6f, 1.5f, 1.3f, 1.0f,
    -2.0f, 1.0f,  0.0f, -0.5f,   0.4f, 2.5f, 2.1f, 0.6f,
     0.0f, 0.0f, -3.0f,  2.0f,   0.7f, 1.0f, 0.2f, 0.9f,
};
// 已知 uniform。
constexpr float kGrWindDirX = 0.6f, kGrWindDirY = 0.8f;
constexpr float kGrWindSpeed = 1.5f, kGrWindStrength = 0.3f, kGrWindTurb = 0.1f, kGrTime = 2.5f;

// C++ 同公式复算单实例风矩阵（逐句镜像 grass WGSL / CPU BuildWindMatrix）：out 16 float 列主序。
inline void GrassRefMatrix(const float* inst, float out[16]) {
    const float px = inst[0], py = inst[1], pz = inst[2], yaw = inst[3];
    const float w = inst[4], h = inst[5], wind_phase = inst[6], fade = inst[7];
    const float phase = wind_phase + kGrTime * kGrWindSpeed;
    const float bend = std::sin(phase) * kGrWindStrength;
    const float turb = std::sin(phase * 3.7f + wind_phase * 2.3f) * kGrWindTurb;
    float total_bend = bend + turb;
    total_bend = std::max(-0.436f, std::min(0.436f, total_bend));
    const float rx = -total_bend * kGrWindDirY;
    const float rz =  total_bend * kGrWindDirX;
    const float cx = std::cos(rx), sx = std::sin(rx);
    const float cz = std::cos(rz), sz = std::sin(rz);
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float a00 = cz * cy, a01 = -sz, a02 = cz * sy;
    const float a10 = sz * cy, a11 =  cz, a12 = sz * sy;
    const float a20 = -sy,     a21 = 0.0f, a22 = cy;
    const float r00 = a00,                 r01 = a01,                 r02 = a02;
    const float r10 = cx * a10 - sx * a20, r11 = cx * a11 - sx * a21, r12 = cx * a12 - sx * a22;
    const float r20 = sx * a10 + cx * a20, r21 = sx * a11 + cx * a21, r22 = sx * a12 + cx * a22;
    const float hf = h * fade;
    out[0]  = r00 * w;  out[1]  = r10 * w;  out[2]  = r20 * w;  out[3]  = 0.0f;  // col0
    out[4]  = r01 * hf; out[5]  = r11 * hf; out[6]  = r21 * hf; out[7]  = 0.0f;  // col1
    out[8]  = r02 * w;  out[9]  = r12 * w;  out[10] = r22 * w;  out[11] = 0.0f;  // col2
    out[12] = px;       out[13] = py;       out[14] = pz;       out[15] = 1.0f;  // col3
}

struct GrassSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
};

void OnGrassMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<GrassSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const float* v = static_cast<const float*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kGrOutBytes));
        if (v) {
            ok = true;
            for (uint32_t i = 0; i < kGrInstances && ok; ++i) {
                float exp[16];
                GrassRefMatrix(&kGrInst[i * 8], exp);
                for (int e = 0; e < 16 && ok; ++e) {
                    if (std::abs(v[i * 16 + e] - exp[e]) > 1e-4f) {
                        ok = false;
                        DEBUG_LOG_ERROR("WebGPU[B3b-14] grass 自检 inst{} elem{} 失配：{} 期望 {}",
                                        i, e, v[i * 16 + e], exp[e]);
                    }
                }
            }
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-14] grass 自检：结果回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-14] grass 风场自检 PASS：grass_system 风场真 compute 逻辑"
                       "（每实例 h*fade 折叠风偏 → 列主序 mat4）经内联镜像 WGSL 经 6 命名 uniform + 2×SSBO"
                       " 跑出风矩阵 == CPU BuildWindMatrix 复算");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-14] grass 风场自检 FAIL");
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    delete ctx;
}

// --- B3b-13 bloom 双滤波（dual-filter）compute 真链路自检（常量 / 半精解码 / ctx / 回读校验回调）---
//   引擎 BloomRenderer 真实 compute（bloom_downsample.comp / bloom_upsample.comp，GLSL 450）核心：
//   ①下采样 13-tap 加权滤波（Call of Duty: AW 演讲的双线性 box）；②上采样 3×3 tent 滤波 + 按权累加。
//   两核心 src 经采样纹理（此处 textureLoad 整数 tap，等价采样路径已 B3b-8 验证），结果写 rgba16f
//   storage（bloom 真实输出格式）。布置：先经一支 gen compute 把已知公式渐变写进 src8（8×8）/ usrc4 /
//   ubase4（4×4）rgba16f；②下采样 src8（中心 c=(2·dst+1) ±{1,2} 整数 tap，13-tap 权重）→ down4；
//   ③上采样 usrc4（3×3 tent）+ ubase4 按 blend=0.5 累加 → up4。回读 down4 + up4 各 4×4 rgba16f，逐
//   texel 逐通道半精解码后校验 == CPU 同公式预期（容差含 rgba16f 存储半精舍入）。证明 bloom 双滤波核心
//   compute（多 tap 加权 + rgba16f storage 写 + 滤波链）WebGPU 可用。离屏隔离、不翻能力位、不碰 demo。
//   注：bloom 上采样真正翻能力位前另需消费方适配——bloom_upsample.comp 用 imageLoad(u_dst) 读回自身
//   累加（read-write rgba16f storage 在核心 WebGPU 不支持），需 ping-pong 双缓冲；此自检以独立 base 采样
//   纹理替代 in-place imageLoad 验证 tent + 累加数学（与 DDGI temporal 同类前置项）。
constexpr uint32_t kBlSrcDim   = 8;
constexpr uint32_t kBlDownDim  = 4;                         // src/2
constexpr uint32_t kBlUpDim    = 4;
constexpr uint32_t kBlRowBytes = 256;                       // rgba16f 4×8B=32B/行 → 填充至 copy 256 对齐
constexpr uint32_t kBlDownOff  = 0;
constexpr uint32_t kBlUpOff    = kBlRowBytes * kBlDownDim;   // 1024
constexpr uint32_t kBlTotalBytes = kBlUpOff + kBlRowBytes * kBlUpDim;  // 2048
constexpr float    kBlBlend    = 0.5f;
constexpr float    kBlTol      = 0.05f;                     // 含 rgba16f 半精存储舍入

// 已知公式渐变（gen compute 与 CPU 预期共用，避免 CPU 侧 float→half 编码）。
inline void BlSrcTexel(int x, int y, float* o)    { o[0] = float(x + y * 8); o[1] = float(x) * 0.5f; o[2] = float(y) * 0.25f; }
inline void BlUpSrcTexel(int x, int y, float* o)  { o[0] = float(x + y * 4); o[1] = float(x) * 0.5f; o[2] = float(y) * 0.25f; }
inline void BlUpBaseTexel(int x, int y, float* o) { o[0] = float(x + y * 4) * 0.25f; o[1] = 1.0f; o[2] = 0.5f; }

// IEEE 754 half(binary16) → float，用于解码 rgba16f 回读缓冲。
inline float BlHalfToFloat(uint16_t h) {
    const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t man = h & 0x3FFu;
    uint32_t f;
    if (exp == 0u) {
        if (man == 0u) {
            f = sign;
        } else {
            exp = 127u - 15u + 1u;
            while ((man & 0x400u) == 0u) { man <<= 1; --exp; }
            man &= 0x3FFu;
            f = sign | (exp << 23) | (man << 13);
        }
    } else if (exp == 0x1Fu) {
        f = sign | 0x7F800000u | (man << 13);
    } else {
        f = sign | ((exp - 15u + 127u) << 23) | (man << 13);
    }
    float r;
    std::memcpy(&r, &f, sizeof(r));
    return r;
}

struct BloomSelfTestCtx {
    WGPUBuffer rb_out = nullptr;
};

void OnBloomMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<BloomSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_out, 0, kBlTotalBytes));
        if (base) {
            ok = true;
            auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
            auto rd = [&](uint32_t off, int x, int y, float* o) {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + off + y * kBlRowBytes + x * 8u);
                o[0] = BlHalfToFloat(p[0]); o[1] = BlHalfToFloat(p[1]); o[2] = BlHalfToFloat(p[2]);
            };
            // ① 下采样 13-tap 校验（src8 8×8 → down4 4×4）。
            const int sm = static_cast<int>(kBlSrcDim) - 1;
            auto sl = [&](int x, int y, float* o) { BlSrcTexel(clampi(x, 0, sm), clampi(y, 0, sm), o); };
            for (uint32_t dy = 0; dy < kBlDownDim && ok; ++dy) {
                for (uint32_t dx = 0; dx < kBlDownDim; ++dx) {
                    const int cx = static_cast<int>(dx) * 2 + 1;
                    const int cy = static_cast<int>(dy) * 2 + 1;
                    float a[3], b[3], c[3], d[3], e[3], f[3], g[3], hh[3], i[3], j[3], k[3], l[3], mm[3];
                    sl(cx - 2, cy + 2, a); sl(cx,     cy + 2, b); sl(cx + 2, cy + 2, c);
                    sl(cx - 2, cy,     d); sl(cx,     cy,     e); sl(cx + 2, cy,     f);
                    sl(cx - 2, cy - 2, g); sl(cx,     cy - 2, hh); sl(cx + 2, cy - 2, i);
                    sl(cx - 1, cy + 1, j); sl(cx + 1, cy + 1, k);
                    sl(cx - 1, cy - 1, l); sl(cx + 1, cy - 1, mm);
                    float got[3]; rd(kBlDownOff, static_cast<int>(dx), static_cast<int>(dy), got);
                    for (int ch = 0; ch < 3; ++ch) {
                        const float exp = e[ch] * 0.125f
                            + (a[ch] + c[ch] + g[ch] + i[ch]) * 0.03125f
                            + (b[ch] + d[ch] + f[ch] + hh[ch]) * 0.0625f
                            + (j[ch] + k[ch] + l[ch] + mm[ch]) * 0.125f;
                        if (std::abs(got[ch] - exp) > kBlTol) {
                            ok = false;
                            DEBUG_LOG_ERROR("WebGPU[B3b-13] bloom 下采样 down4[{},{}].{} 失配：{} 期望 {}",
                                            dx, dy, ch, got[ch], exp);
                        }
                    }
                }
            }
            // ② 上采样 3×3 tent + 累加校验（usrc4 4×4 + ubase4 → up4 4×4）。
            const int um = static_cast<int>(kBlUpDim) - 1;
            auto ul = [&](int x, int y, float* o) { BlUpSrcTexel(clampi(x, 0, um), clampi(y, 0, um), o); };
            for (uint32_t y = 0; y < kBlUpDim && ok; ++y) {
                for (uint32_t x = 0; x < kBlUpDim; ++x) {
                    const int cx = static_cast<int>(x);
                    const int cy = static_cast<int>(y);
                    float a[3], b[3], c[3], d[3], e[3], f[3], g[3], hh[3], i[3];
                    ul(cx - 1, cy + 1, a); ul(cx,     cy + 1, b); ul(cx + 1, cy + 1, c);
                    ul(cx - 1, cy,     d); ul(cx,     cy,     e); ul(cx + 1, cy,     f);
                    ul(cx - 1, cy - 1, g); ul(cx,     cy - 1, hh); ul(cx + 1, cy - 1, i);
                    float bs[3]; BlUpBaseTexel(cx, cy, bs);
                    float got[3]; rd(kBlUpOff, cx, cy, got);
                    for (int ch = 0; ch < 3; ++ch) {
                        const float up = (e[ch] * 4.0f + (b[ch] + d[ch] + f[ch] + hh[ch]) * 2.0f
                                          + (a[ch] + c[ch] + g[ch] + i[ch])) * (1.0f / 16.0f);
                        const float exp = bs[ch] + up * kBlBlend;
                        if (std::abs(got[ch] - exp) > kBlTol) {
                            ok = false;
                            DEBUG_LOG_ERROR("WebGPU[B3b-13] bloom 上采样 up4[{},{}].{} 失配：{} 期望 {}",
                                            x, y, ch, got[ch], exp);
                        }
                    }
                }
            }
        }
        wgpuBufferUnmap(ctx->rb_out);
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-13] bloom 自检：结果回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[B3b-13] bloom 双滤波自检 PASS：引擎 BloomRenderer 真 compute"
                       "（下采样 13-tap 加权 + 上采样 3×3 tent 累加 → rgba16f storage 写）经手译 WGSL"
                       " 跑出 down4/up4 == CPU 同公式预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[B3b-13] bloom 双滤波自检 FAIL");
    }
    if (ctx->rb_out) wgpuBufferRelease(ctx->rb_out);
    delete ctx;
}


// --- Task 4 Subtask 1：MultiDrawIndexedIndirect 离屏自检常量 / 上下文 / 回读回调 ---
// 离屏 RT 用引擎 CreateRenderTarget（场景色统一 RGBA16Float，8B/texel）；64×8=512B/行满足
// copyTextureToBuffer 的 256 字节对齐。预置 indirect cmds instance_count=[1,0,1,0]，校验
// 「可见象限（红/蓝）有色、被剔象限为黑」——即被测 MultiDrawIndexedIndirect 按 byte_offset+i*stride
// 循环 DrawIndexedIndirect、且 instance_count=0 条目硬件不绘制。
constexpr uint32_t kT41Instances = 4;     ///< 象限/draw 条数
constexpr uint32_t kT41DrawWords = 5;     ///< 每条 indirect command 字数
constexpr uint32_t kT41RtSize    = 64;    ///< 离屏 RT 边长
constexpr uint32_t kT41RtRowBytes = kT41RtSize * 8u;   ///< RGBA16Float 8B/texel
constexpr uint32_t kT41RtBytes    = kT41RtRowBytes * kT41RtSize;

struct MultiDrawIndirectSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnT41PixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<MultiDrawIndirectSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kT41RtBytes));
        if (base) {
            // rgba16f：每 texel 8 字节（4×half）。取 4 象限中心像素（RT 顶左原点）。
            auto rd = [&](uint32_t x, uint32_t y, float* o) {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + y * kT41RtRowBytes + x * 8u);
                o[0] = BlHalfToFloat(p[0]); o[1] = BlHalfToFloat(p[1]); o[2] = BlHalfToFloat(p[2]);
            };
            float tl[3], tr[3], bl[3], br[3];
            rd(16, 16, tl); rd(48, 16, tr); rd(16, 48, bl); rd(48, 48, br);
            auto bright = [](const float* c, int ch) { return c[ch] > 0.4f; };
            auto dark   = [](const float* c) { return c[0] < 0.15f && c[1] < 0.15f && c[2] < 0.15f; };
            ok = bright(tl, 0) &&  // inst0 可见：红
                 dark(tr)      &&  // inst1 被剔：黑
                 bright(bl, 2) &&  // inst2 可见：蓝
                 dark(br);         // inst3 被剔：黑
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T4-1] MDI 自检像素：TL({},{},{}) TR({},{},{}) "
                                "BL({},{},{}) BR({},{},{})",
                                tl[0], tl[1], tl[2], tr[0], tr[1], tr[2],
                                bl[0], bl[1], bl[2], br[0], br[1], br[2]);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T4-1] MDI 自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T4-1] MultiDrawIndexedIndirect 自检 PASS：引擎-facing 循环 DrawIndexedIndirect "
                       "（byte_offset+i*stride）+ instance_count=[1,0,1,0] 离屏像素（红/蓝象限有色、被剔象限为黑）符合预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T4-1] MultiDrawIndexedIndirect 自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- Task 4 Subtask 2：Mega VAO 离屏自检常量 / 上下文 / 回读回调 ---
// 经 CreateMegaVAO/UpdateMegaVBO/UpdateMegaIBO/BindMegaVAO 把 4 象限 BatchVertex(92B) 几何渲到 64×64 RT，
// 4 象限各一种颜色（红/绿/蓝/黄）全部可见，回读校验各象限颜色就位——即 92B 布局 pos@0/color@12 解析正确、
// BindMegaVAO 正确设置引擎 draw state。RT 同为 RGBA16Float（8B/texel，行 512B 满足 256 对齐）。
constexpr uint32_t kT42Quads    = 4;      ///< 象限数
constexpr uint32_t kT42RtSize   = 64;     ///< 离屏 RT 边长
constexpr uint32_t kT42RtRowBytes = kT42RtSize * 8u;  ///< RGBA16Float 8B/texel
constexpr uint32_t kT42RtBytes    = kT42RtRowBytes * kT42RtSize;

struct MegaVaoSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnT42PixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<MegaVaoSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kT42RtBytes));
        if (base) {
            auto rd = [&](uint32_t x, uint32_t y, float* o) {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + y * kT42RtRowBytes + x * 8u);
                o[0] = BlHalfToFloat(p[0]); o[1] = BlHalfToFloat(p[1]); o[2] = BlHalfToFloat(p[2]);
            };
            float tl[3], tr[3], bl[3], br[3];
            rd(16, 16, tl); rd(48, 16, tr); rd(16, 48, bl); rd(48, 48, br);
            auto hi = [](float v) { return v > 0.4f; };
            auto lo = [](float v) { return v < 0.15f; };
            ok = (hi(tl[0]) && lo(tl[1]) && lo(tl[2])) &&  // TL 红
                 (lo(tr[0]) && hi(tr[1]) && lo(tr[2])) &&  // TR 绿
                 (lo(bl[0]) && lo(bl[1]) && hi(bl[2])) &&  // BL 蓝
                 (hi(br[0]) && hi(br[1]) && lo(br[2]));     // BR 黄
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T4-2] MegaVAO 自检像素：TL({},{},{}) TR({},{},{}) "
                                "BL({},{},{}) BR({},{},{})",
                                tl[0], tl[1], tl[2], tr[0], tr[1], tr[2],
                                bl[0], bl[1], bl[2], br[0], br[1], br[2]);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T4-2] MegaVAO 自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T4-2] Mega VAO 自检 PASS：CreateMegaVAO/UpdateMegaVBO/IBO + BindMegaVAO "
                       "（BatchVertex 92B 布局 pos@0/color@12）+ 索引绘制 4 象限离屏像素（红/绿/蓝/黄各就位）符合预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T4-2] Mega VAO 自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- Task 4 Subtask 3：GPU-driven PBR 离屏自检常量 / 上下文 / 回读回调 ---
// 经引擎-facing SetupGPUDrivenPBRShader（绑 PerFrame/PerScene UBO）+ 实例 SSBO(b5,2 个 model 平移到左/右半)
// + 材质 SSBO(b9,红/绿 albedo) + BindMegaVAO(92B) + BindGPUDrivenTextures(白 albedo) + MultiDrawIndexedIndirect
// (1 条 cmd、instanceCount=2、instance_index 取 0/1)把两实例渲到 64×64 RT，回读校验左半红、右半绿——即
// 手译 PBR WGSL 经实例 SSBO 取 model、经材质 SSBO 取 albedo、纹理采样链路全部正确。RT 同为 RGBA16Float。
constexpr uint32_t kT43RtSize     = 64;
constexpr uint32_t kT43RtRowBytes = kT43RtSize * 8u;   ///< RGBA16Float 8B/texel
constexpr uint32_t kT43RtBytes    = kT43RtRowBytes * kT43RtSize;

struct GpuDrivenPBRSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnT43PixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<GpuDrivenPBRSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kT43RtBytes));
        if (base) {
            auto rd = [&](uint32_t x, uint32_t y, float* o) {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + y * kT43RtRowBytes + x * 8u);
                o[0] = BlHalfToFloat(p[0]); o[1] = BlHalfToFloat(p[1]); o[2] = BlHalfToFloat(p[2]);
            };
            float l[3], r[3];
            rd(16, 32, l); rd(48, 32, r);   // 左半中心 / 右半中心
            auto hi = [](float v) { return v > 0.4f; };
            auto lo = [](float v) { return v < 0.15f; };
            ok = (hi(l[0]) && lo(l[1]) && lo(l[2])) &&   // 实例0：红 albedo（左半）
                 (lo(r[0]) && hi(r[1]) && lo(r[2]));      // 实例1：绿 albedo（右半）
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T4-3] GPU-driven PBR 自检像素：L({},{},{}) R({},{},{})",
                                l[0], l[1], l[2], r[0], r[1], r[2]);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T4-3] GPU-driven PBR 自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T4-3] GPU-driven PBR 自检 PASS：SetupGPUDrivenPBRShader(PerFrame/PerScene UBO) "
                       "+ 实例 SSBO(b5) 取 model + 材质 SSBO(b9) 取 albedo + 白纹理桶 + MultiDrawIndexedIndirect "
                       "(instanceCount=2) 离屏像素（左半红/右半绿）符合预期");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T4-3] GPU-driven PBR 自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- Task 5 Subtask 1（T5-1）：CSM 方向光阴影深度图采样离屏自检 ---
// 验证 WebGPU 能把一张 Depth32 shadow atlas（由「阴影深度趟」写）在「随后的前向趟」作为
// texture_depth_2d 经 textureLoad（3×3 PCF）采样比较——即 CSM 真实场景「阴影 pass 写 atlas → 前向
// pass 采样 atlas」的跨 pass 读写（恰是旧注释担心的 Dawn 屏障冲突点，本自检证明分趟即无冲突）。
//   ①阴影趟：占据 NDC 中心 [-0.5,0.5]² 的遮挡 quad（z=0.3）渲入 32×32 atlas（其余清深=1.0）。
//   ②前向趟：全屏 quad 采样 atlas（receiverDepth=0.6 在遮挡体之后）→ 中心遮挡（0.6>0.3 受阴影→暗）、
//     四角无遮挡（0.6<1.0 受光→亮）→ 渲到 64×64 RGBA16Float RT → copy 回读校验 中心暗、四角亮。
constexpr uint32_t kT51AtlasDim   = 32;                       ///< shadow atlas 边长（Depth32）
constexpr uint32_t kT51RtSize     = 64;                       ///< 离屏 color RT 边长
constexpr uint32_t kT51RtRowBytes = kT51RtSize * 8u;          ///< RGBA16Float 8B/texel（512B 满足 256 对齐）
constexpr uint32_t kT51RtBytes    = kT51RtRowBytes * kT51RtSize;

struct CSMShadowSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnT51PixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<CSMShadowSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kT51RtBytes));
        if (base) {
            auto rd = [&](uint32_t x, uint32_t y) -> float {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + y * kT51RtRowBytes + x * 8u);
                return BlHalfToFloat(p[0]);
            };
            const float center = rd(32, 32);          // 中心：受遮挡 → 暗
            const float corner = (rd(4, 4) + rd(59, 4) + rd(4, 59) + rd(59, 59)) * 0.25f;  // 四角：受光 → 亮
            ok = (center < 0.3f) && (corner > 0.7f);
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T5-1] CSM shadow 自检像素：center={} corner_avg={}", center, corner);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-1] CSM shadow 自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T5-1] CSM shadow 自检 PASS：Depth32 atlas 跨 pass 经 texture_depth_2d + "
                       "textureLoad(3×3 PCF) 采样比较正确——中心受遮挡为暗、四角受光为亮（证明 CSM 阴影深度图 "
                       "采样能力，逻辑同 forward_shaded.frag 的 DirectionalShadow）");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-1] CSM shadow 自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- Task 5 Subtask 2 离屏自检常量 / 上下文 / 回读回调（延迟着色：MRT gbuffer + 延迟光照）---
// 验证 WebGPU 能在一趟里把几何渲入「多渲染目标（MRT）gbuffer」（albedo/normal/position 3 个颜色附件），
// 再在「随后的全屏光照趟」把这 3 张 gbuffer 纹理一并采样做延迟光照——即延迟管线「几何趟写 gbuffer →
// 光照趟采样 gbuffer」的核心能力，逻辑同 deferred_lighting.frag（albedo·NdotL + ambient，法线长度<阈视为空像素）。
//   ①几何趟：占据 NDC 中心 [-0.6,0.6]² 的 quad 渲入 64×64×3 gbuffer（albedo=(0.8,0.2,0.2)、normal 编码(0,0,1)、
//     position=0；其余清 0 → normal 长度 0 视为空像素）。
//   ②光照趟：全屏 quad 按 @builtin(position) 整数坐标 textureLoad 3 张 gbuffer → 中心几何受光为红、四角空像素为黑
//     → 渲到 64×64 RGBA16Float RT → copy 回读校验 中心红（r 高、g/b 低）、四角黑。
constexpr uint32_t kT52RtSize     = 64;                       ///< gbuffer / 离屏 color RT 边长
constexpr uint32_t kT52RtRowBytes = kT52RtSize * 8u;          ///< RGBA16Float 8B/texel（512B 满足 256 对齐）
constexpr uint32_t kT52RtBytes    = kT52RtRowBytes * kT52RtSize;

struct DeferredSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnT52PixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<DeferredSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kT52RtBytes));
        if (base) {
            auto rd = [&](uint32_t x, uint32_t y, uint32_t ch) -> float {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + y * kT52RtRowBytes + x * 8u);
                return BlHalfToFloat(p[ch]);
            };
            const float cr = rd(32, 32, 0), cg = rd(32, 32, 1), cb = rd(32, 32, 2);  // 中心：受光红
            const float corner = (rd(4, 4, 0) + rd(59, 4, 0) + rd(4, 59, 0) + rd(59, 59, 0)) * 0.25f;  // 四角：空像素黑
            ok = (cr > 0.6f) && (cg < 0.4f) && (cb < 0.4f) && (corner < 0.1f);
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T5-2] 延迟着色自检像素：center=({},{},{}) corner_r_avg={}", cr, cg, cb, corner);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-2] 延迟着色自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T5-2] 延迟着色自检 PASS：MRT gbuffer（albedo/normal/position 3 附件一趟写）+ 光照趟"
                       " textureLoad 3 张 gbuffer 做延迟光照正确——中心几何受光为红、四角空像素为黑（证明 MRT gbuffer "
                       "+ 延迟光照采样能力，逻辑同 deferred_lighting.frag）");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-2] 延迟着色自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// ============================================================================
// Task 5 Subtask 3/4/5 离屏自检共享 C++ 复算数学（与各自 WGSL 同公式，回调内复算期望值）。
// ============================================================================

// ACES filmic 色调映射（与 tonemapping.frag / T5-3 tonemap WGSL 同系数）。
inline float T5AcesFilmic(float x) {
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    float v = (x * (a * x + b)) / (x * (c * x + d) + e);
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

// Van der Corput 位反演（与 T5-4 BRDF LUT WGSL RadicalInverse_VdC 同位运算）。
inline float T5RadicalInverseVdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

// GGX split-sum BRDF 积分（N=(0,0,1)、256 采样，与 T5-4 IntegrateBRDF WGSL 逐行同算法）。
inline void T5IntegrateBRDF(float NdotV, float roughness, float& outA, float& outB) {
    const float PI = 3.14159265358979323846f;
    const float Vx = std::sqrt(1.0f - NdotV * NdotV), Vy = 0.0f, Vz = NdotV;  // V（N=(0,0,1)）
    const uint32_t kN = 256u;
    float A = 0.0f, B = 0.0f;
    for (uint32_t i = 0; i < kN; ++i) {
        const float xi_x = static_cast<float>(i) / static_cast<float>(kN);
        const float xi_y = T5RadicalInverseVdC(i);
        // ImportanceSampleGGX（N=(0,0,1) → 切空间 H 即世界 H）。
        const float a = roughness * roughness;
        const float phi = 2.0f * PI * xi_x;
        const float cosTheta = std::sqrt((1.0f - xi_y) / (1.0f + (a * a - 1.0f) * xi_y));
        const float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
        const float Hx = std::cos(phi) * sinTheta, Hy = std::sin(phi) * sinTheta, Hz = cosTheta;
        const float VdotH = Vx * Hx + Vy * Hy + Vz * Hz;
        const float Lz = 2.0f * VdotH * Hz - Vz;  // L = 2*dot(V,H)*H - V，取 .z 即 NdotL
        const float NdotL = Lz > 0.0f ? Lz : 0.0f;
        const float NdotH = Hz > 0.0f ? Hz : 0.0f;
        const float vh = VdotH > 0.0f ? VdotH : 0.0f;
        if (NdotL > 0.0f) {
            const float k = (roughness * roughness) / 2.0f;  // IBL 几何项 k
            const float gV = NdotV / (NdotV * (1.0f - k) + k);
            const float gL = NdotL / (NdotL * (1.0f - k) + k);
            const float G_Vis = (gV * gL * vh) / (NdotH * NdotV);
            const float Fc = std::pow(1.0f - vh, 5.0f);
            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    outA = A / static_cast<float>(kN);
    outB = B / static_cast<float>(kN);
}

// --- Task 5 Subtask 3（T5-3）：HDR auto-exposure 亮度归约 + ACES tonemap 离屏自检 ---
// 链路：①渲已知 HDR 值(4,2,1) 到 8×8 RGBA16Float 场景 RT；②亮度归约趟 textureLoad 8×8 采样点算平均
// log 亮度（dot(rgb,(0.2126,0.7152,0.0722))，逻辑同 lum_compute.frag）写 1×1 lum RT；③lum_adapt 趟
// avgLum=exp(avgLogLum)→targetExposure=0.18/avgLum→clamp 写 1×1 exposure RT（逻辑同 lum_adapt.frag）；
// ④tonemap 趟 ACES(hdr*exposure)+gamma(1/2.2) 渲到 64×64 RGBA16Float RT（逻辑同 tonemapping.frag）→
// copy 回读，在回调里以同公式 C++ 复算期望逐通道比对（容差 0.04、且 r>g>b、非全白非全黑）。
constexpr uint32_t kT53SceneDim   = 8;                        ///< 场景 RT 边长（归约趟 8×8 采样整张）
constexpr uint32_t kT53RtSize     = 64;                       ///< 最终 tonemap color RT 边长
constexpr uint32_t kT53RtRowBytes = kT53RtSize * 8u;          ///< RGBA16Float 8B/texel（512B 满足 256 对齐）
constexpr uint32_t kT53RtBytes    = kT53RtRowBytes * kT53RtSize;
constexpr float    kT53HdrR = 4.0f, kT53HdrG = 2.0f, kT53HdrB = 1.0f;  ///< 已知 HDR 场景色

struct HDRSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnT53PixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<HDRSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kT53RtBytes));
        if (base) {
            auto rd = [&](uint32_t x, uint32_t y, uint32_t ch) -> float {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + y * kT53RtRowBytes + x * 8u);
                return BlHalfToFloat(p[ch]);
            };
            // C++ 同公式复算期望（场景色恒定 → avgLogLum=log(lum)）。
            const float lum = kT53HdrR * 0.2126f + kT53HdrG * 0.7152f + kT53HdrB * 0.0722f;
            const float avgLum = std::exp(std::log(lum > 0.0001f ? lum : 0.0001f));
            float exposure = 0.18f / (avgLum > 0.001f ? avgLum : 0.001f);
            if (exposure < 0.01f) exposure = 0.01f;
            if (exposure > 10.0f) exposure = 10.0f;
            const float er = std::pow(T5AcesFilmic(kT53HdrR * exposure), 1.0f / 2.2f);
            const float eg = std::pow(T5AcesFilmic(kT53HdrG * exposure), 1.0f / 2.2f);
            const float eb = std::pow(T5AcesFilmic(kT53HdrB * exposure), 1.0f / 2.2f);
            const float cr = rd(32, 32, 0), cg = rd(32, 32, 1), cb = rd(32, 32, 2);
            const float tol = 0.04f;
            ok = std::fabs(cr - er) < tol && std::fabs(cg - eg) < tol && std::fabs(cb - eb) < tol &&
                 (cr > cg) && (cg > cb) &&                              // (4,2,1) tonemap 后保序
                 (cr < 0.999f) && (cb > 0.001f);                       // 非全白非全黑
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T5-3] HDR tonemap 自检像素：got=({},{},{}) exp=({},{},{}) exposure={}",
                                cr, cg, cb, er, eg, eb, exposure);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-3] HDR tonemap 自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T5-3] HDR tonemap 自检 PASS：HDR 场景 RT → 亮度归约(log 平均)→ lum_adapt(0.18/avgLum "
                       "曝光)→ ACES(hdr*exposure)+gamma 四趟链路正确（逻辑同 lum_compute/lum_adapt/tonemapping.frag），"
                       "回读像素与 C++ 同公式复算逐通道吻合、r>g>b 保序、非全白非全黑");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-3] HDR tonemap 自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- Task 5 Subtask 4（T5-4）：IBL（BRDF LUT + irradiance + prefilter env + PBR 环境项）离屏自检 ---
// 链路：①BRDF LUT 趟 GGX split-sum 积分（256 采样 Hammersley/ImportanceSampleGGX/Smith，uv.x=NdotV、
// uv.y=roughness）渲到 64×64 RGBA16Float LUT RT；②irradiance 趟把常量辐照度色渲到 1×1 RT（常量环境的
// 半球辐照积分即常量，离屏可控）；③prefilter 趟把常量预滤波镜面色渲到 1×1 RT；④PBR 环境项趟绑 LUT/irr/
// pref 三纹理，固定材质(albedo=(0.8,0.2,0.2)/metallic=0/roughness)按 split-sum 合成 ambient = kD*irr*albedo
// + pref*(F*brdf.x+brdf.y)（逻辑同 LearnOpenGL IBL）渲到 64×64 RT → copy 回读，回调里 C++ 同算法复算
// IntegrateBRDF + 合成逐通道比对（容差 0.04、可见 IBL 贡献、非全黑非饱和）。
constexpr uint32_t kT54LutDim     = 64;                       ///< BRDF LUT RT 边长
constexpr uint32_t kT54RtSize     = 64;                       ///< 最终 color RT 边长
constexpr uint32_t kT54RtRowBytes = kT54RtSize * 8u;          ///< RGBA16Float 8B/texel（512B 满足 256 对齐）
constexpr uint32_t kT54RtBytes    = kT54RtRowBytes * kT54RtSize;
constexpr uint32_t kT54BrdfCx     = 63u, kT54BrdfCy = 31u;    ///< PBR 趟采样 LUT 的 texel（NdotV≈1、roughness≈0.5）
constexpr float    kT54AlbedoR = 0.8f, kT54AlbedoG = 0.2f, kT54AlbedoB = 0.2f;  ///< 材质反照率
constexpr float    kT54IrrR = 0.4f, kT54IrrG = 0.5f, kT54IrrB = 0.6f;          ///< 常量辐照度
constexpr float    kT54PrefR = 0.5f, kT54PrefG = 0.5f, kT54PrefB = 0.5f;       ///< 常量预滤波镜面

struct IBLSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnT54PixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<IBLSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kT54RtBytes));
        if (base) {
            auto rd = [&](uint32_t x, uint32_t y, uint32_t ch) -> float {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + y * kT54RtRowBytes + x * 8u);
                return BlHalfToFloat(p[ch]);
            };
            // C++ 同算法复算：LUT 在采样 texel 的 uv 处积分；PBR split-sum 合成。
            const float lut_u = (static_cast<float>(kT54BrdfCx) + 0.5f) / static_cast<float>(kT54LutDim);
            const float lut_v = (static_cast<float>(kT54BrdfCy) + 0.5f) / static_cast<float>(kT54LutDim);
            float A = 0.0f, B = 0.0f;
            T5IntegrateBRDF(lut_u, lut_v, A, B);
            const float F0 = 0.04f, metallic = 0.0f;
            const float NdotV = 1.0f;                                  // V=N=(0,0,1)
            const float F = F0 + (((1.0f - lut_v) > F0 ? (1.0f - lut_v) : F0) - F0) * std::pow(1.0f - NdotV, 5.0f);
            const float kS = F, kD = (1.0f - kS) * (1.0f - metallic);
            const float dr = kT54IrrR * kT54AlbedoR, dg = kT54IrrG * kT54AlbedoG, db = kT54IrrB * kT54AlbedoB;
            const float spec = (F * A + B);                            // 灰度 F0 → 三通道同
            const float er = kD * dr + kT54PrefR * spec;
            const float eg = kD * dg + kT54PrefG * spec;
            const float eb = kD * db + kT54PrefB * spec;
            const float cr = rd(32, 32, 0), cg = rd(32, 32, 1), cb = rd(32, 32, 2);
            const float tol = 0.04f;
            ok = std::fabs(cr - er) < tol && std::fabs(cg - eg) < tol && std::fabs(cb - eb) < tol &&
                 (cr > 0.1f) && (cr > cg) &&                           // 可见 IBL、反照率红主导
                 (cr < 1.0f) && (cg < 1.0f) && (cb < 1.0f);            // 非饱和
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T5-4] IBL 自检像素：got=({},{},{}) exp=({},{},{}) brdf=({},{})",
                                cr, cg, cb, er, eg, eb, A, B);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-4] IBL 自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T5-4] IBL 自检 PASS：BRDF LUT（GGX split-sum 256 采样）+ irradiance/prefilter "
                       "纹理 + PBR 环境项 split-sum 合成（kD*irr*albedo + pref*(F*brdf.x+brdf.y)）四趟链路正确，"
                       "回读像素与 C++ 同算法复算逐通道吻合、可见 IBL 贡献、非全黑非饱和");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-4] IBL 自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

// --- Task 5 Subtask 5（T5-5）：WBOIT（accum/reveal MRT + resolve）离屏自检 ---
// 链路：①几何趟把两层半透明片元（layer0 红 α0.5 z0.2、layer1 蓝 α0.5 z0.6）按 WBOIT 权重在 shader 内
// 解析累加，写 2 附件 MRT（accum=Σ(c*a)*w 与 Σa*w 合成 RGBA16Float、reveal=Π(1-a) 存 .r）；②resolve 趟
// 绑 accum/reveal 两纹理，avgColor=accum.rgb/max(accum.a,eps)、finalAlpha=1-reveal、over 黑背景合成渲到
// 64×64 RGBA16Float RT → copy 回读，回调里 C++ 同公式复算逐通道比对（容差 0.04、红蓝双层均有贡献）。
constexpr uint32_t kT55RtSize     = 64;                       ///< accum/reveal/最终 color RT 边长
constexpr uint32_t kT55RtRowBytes = kT55RtSize * 8u;          ///< RGBA16Float 8B/texel（512B 满足 256 对齐）
constexpr uint32_t kT55RtBytes    = kT55RtRowBytes * kT55RtSize;
// 两层片元（与 T5-5 几何趟 WGSL 同值）。
constexpr float kT55C0r = 1.0f, kT55C0g = 0.0f, kT55C0b = 0.0f, kT55A0 = 0.5f, kT55Z0 = 0.2f;
constexpr float kT55C1r = 0.0f, kT55C1g = 0.0f, kT55C1b = 1.0f, kT55A1 = 0.5f, kT55Z1 = 0.6f;

struct WBOITSelfTestCtx {
    WGPUBuffer rb_pixels = nullptr;
};

void OnT55PixelsMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<WBOITSelfTestCtx*>(userdata);
    bool ok = false;
    if (status == WGPUBufferMapAsyncStatus_Success) {
        const uint8_t* base = static_cast<const uint8_t*>(
            wgpuBufferGetConstMappedRange(ctx->rb_pixels, 0, kT55RtBytes));
        if (base) {
            auto rd = [&](uint32_t x, uint32_t y, uint32_t ch) -> float {
                const uint16_t* p = reinterpret_cast<const uint16_t*>(base + y * kT55RtRowBytes + x * 8u);
                return BlHalfToFloat(p[ch]);
            };
            // C++ 同公式复算：weightFn(z)=max(0.01,3*(1-z))，w=alpha*weightFn(z)。
            auto wfn = [](float z) -> float { const float v = 3.0f * (1.0f - z); return v > 0.01f ? v : 0.01f; };
            const float w0 = kT55A0 * wfn(kT55Z0), w1 = kT55A1 * wfn(kT55Z1);
            const float ar = (kT55C0r * kT55A0) * w0 + (kT55C1r * kT55A1) * w1;
            const float ag = (kT55C0g * kT55A0) * w0 + (kT55C1g * kT55A1) * w1;
            const float ab = (kT55C0b * kT55A0) * w0 + (kT55C1b * kT55A1) * w1;
            const float aa = kT55A0 * w0 + kT55A1 * w1;
            const float reveal = (1.0f - kT55A0) * (1.0f - kT55A1);
            const float denom = aa > 1e-5f ? aa : 1e-5f;
            const float finalAlpha = 1.0f - reveal;
            const float er = (ar / denom) * finalAlpha;                // over 黑背景
            const float eg = (ag / denom) * finalAlpha;
            const float eb = (ab / denom) * finalAlpha;
            const float cr = rd(32, 32, 0), cg = rd(32, 32, 1), cb = rd(32, 32, 2);
            const float tol = 0.04f;
            ok = std::fabs(cr - er) < tol && std::fabs(cg - eg) < tol && std::fabs(cb - eb) < tol &&
                 (cr > 0.1f) && (cb > 0.1f) &&                         // 红蓝双层均有贡献（确证 OIT 混合）
                 (cg < 0.1f);                                          // 无绿层
            if (!ok) {
                DEBUG_LOG_ERROR("WebGPU[T5-5] WBOIT 自检像素：got=({},{},{}) exp=({},{},{}) reveal={}",
                                cr, cg, cb, er, eg, eb, reveal);
            }
        }
        wgpuBufferUnmap(ctx->rb_pixels);
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-5] WBOIT 自检：像素回读映射失败 status={}", static_cast<int>(status));
    }
    if (ok) {
        DEBUG_LOG_INFO("WebGPU[T5-5] WBOIT 自检 PASS：accum/reveal 2 附件 MRT（Σ(c*a)*w 与 Π(1-a)）+ resolve "
                       "（accum.rgb/max(accum.a,eps)、1-reveal 合成）链路正确，回读像素与 C++ 同公式复算逐通道吻合、"
                       "红蓝双层均有贡献（确证 OIT 混合非单层覆盖）");
    } else {
        DEBUG_LOG_ERROR("WebGPU[T5-5] WBOIT 自检 FAIL");
    }
    if (ctx->rb_pixels) wgpuBufferRelease(ctx->rb_pixels);
    delete ctx;
}

} // namespace

bool WebGpuSelfTestHarness::RecordComputeSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // 自检 compute：每线程写 outbuf[i]=i*2+base；0 号线程额外把 indirect DrawCmd 写为定值。
    // 同时演练 group1=UBO 参数、group3=两个 read_write storage（普通 SSBO + storage|indirect）。
    static const char* kComputeSelfTestWGSL = R"WGSL(// dse-wgsl
struct Params { n : u32, base : u32, pad0 : u32, pad1 : u32, };
struct DrawCmd { count : u32, instance_count : u32, first_index : u32, base_vertex : u32, base_instance : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(3) @binding(0) var<storage, read_write> outbuf : array<u32>;
@group(3) @binding(1) var<storage, read_write> draw : DrawCmd;
@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let i = gid.x;
  if (i < params.n) { outbuf[i] = i * 2u + params.base; }
  if (i == 0u) {
    draw.count = 36u;
    draw.instance_count = 1u;
    draw.first_index = 0u;
    draw.base_vertex = 0u;
    draw.base_instance = 0u;
  }
}
)WGSL";

    // B3b：经引擎-facing 入口 CreateComputeShaderEx 创建（gl/vk/hlsl 空、仅 WGSL 槽），
    // 验证整条多源 compute 通路（引擎各 compute 特性即按此签名调用）。
    if (!ct_shader_) ct_shader_ = dev_->CreateComputeShaderEx("", "", "", 1, 0, 0, 0, kComputeSelfTestWGSL);
    if (!ct_params_) {
        const uint32_t params[4] = {kCtN, kCtBase, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        ct_params_ = dev_->CreateGpuBuffer(d, params).raw();
    }
    if (!ct_out_) {
        GpuBufferDesc d; d.size = kCtN * sizeof(uint32_t); d.usage = GpuBufferUsage::kStorage;
        ct_out_ = dev_->CreateGpuBuffer(d, nullptr).raw();
    }
    if (!ct_draw_) {
        GpuBufferDesc d; d.size = kCtDrawWords * sizeof(uint32_t);
        d.usage = GpuBufferUsage::kStorage | GpuBufferUsage::kIndirect;
        ct_draw_ = dev_->CreateGpuBuffer(d, nullptr).raw();
    }
    if (!ct_shader_ || !ct_params_ || !ct_out_ || !ct_draw_) {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检资源创建失败，跳过");
        return false;
    }

    // 经与引擎相同的命令录制状态绑定资源（group1 b0 UBO；group3 b0/b1 SSBO）。
    dev_->ResetDrawState();
    dev_->CmdBindUniformBuffer(0, ct_params_, 0, sizeof(uint32_t) * 4);
    dev_->CmdBindStorageBuffer(0, ct_out_, 0, kCtN * sizeof(uint32_t));
    dev_->CmdBindStorageBuffer(1, ct_draw_, 0, kCtDrawWords * sizeof(uint32_t));

    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(ct_shader_, (kCtN + 63u) / 64u, 1, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // 回读缓冲（MapRead|CopyDst）+ storage→回读 拷贝（在本帧 frame_encoder_ 上录制，随帧提交）。
    auto make_rb = [&](uint64_t bytes) -> WGPUBuffer {
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bd.size = AlignUp4(bytes);
        return wgpuDeviceCreateBuffer(dev_->device_, &bd);
    };
    ct_rb_out_ = make_rb(kCtN * sizeof(uint32_t));
    ct_rb_draw_ = make_rb(kCtDrawWords * sizeof(uint32_t));
    const WebGPURhiDevice::BufferEntry* be_out = dev_->FindBuffer(ct_out_);
    const WebGPURhiDevice::BufferEntry* be_draw = dev_->FindBuffer(ct_draw_);
    if (!ct_rb_out_ || !ct_rb_draw_ || !be_out || !be_draw) {
        if (ct_rb_out_) { wgpuBufferRelease(ct_rb_out_); ct_rb_out_ = nullptr; }
        if (ct_rb_draw_) { wgpuBufferRelease(ct_rb_draw_); ct_rb_draw_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_out->buffer, 0, ct_rb_out_, 0,
                                         kCtN * sizeof(uint32_t));
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_draw->buffer, 0, ct_rb_draw_, 0,
                                         kCtDrawWords * sizeof(uint32_t));
    return true;
}

void WebGpuSelfTestHarness::KickComputeSelfTestReadback() {
    if (!ct_rb_out_ || !ct_rb_draw_) return;
    // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    auto* ctx = new ComputeSelfTestCtx();
    ctx->rb_out = ct_rb_out_;
    ctx->rb_draw = ct_rb_draw_;
    ct_rb_out_ = nullptr;
    ct_rb_draw_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kCtN * sizeof(uint32_t),
                       OnComputeSelfTestOutMapped, ctx);
    wgpuBufferMapAsync(ctx->rb_draw, WGPUMapMode_Read, 0, kCtDrawWords * sizeof(uint32_t),
                       OnComputeSelfTestDrawMapped, ctx);
}

// B3b-2：GPU-driven 剔除真链路自检——视锥剔除 compute（手译自 builtin_passes.cpp kGPUCullShaderSource
// 的 frustum 部分，Hi-Z 遮挡因需 storage-image 金字塔留后续）写 per-instance indirect draw command，
// 再用真 DrawIndexedIndirect 渲到离屏 RT，回读 SSBO + 像素双重校验。详见 GpuCullSelfTestCtx。
bool WebGpuSelfTestHarness::RecordGpuCullSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // --- WGSL 视锥剔除 compute（group1 b0 = 6 视锥面 + 实例数；group3 b0 = AABB；group3 b1 = draw cmds）---
    static const char* kCullWGSL = R"WGSL(// dse-wgsl
struct AABB { lo : vec4<f32>, hi : vec4<f32>, };
struct DrawCmd { count : u32, instance_count : u32, first_index : u32, base_vertex : u32, base_instance : u32, };
struct CullParams { planes : array<vec4<f32>, 6>, object_count : u32, pad0 : u32, pad1 : u32, pad2 : u32, };
@group(1) @binding(0) var<uniform> params : CullParams;
@group(3) @binding(0) var<storage, read_write> aabbs : array<AABB>;
@group(3) @binding(1) var<storage, read_write> draws : array<DrawCmd>;
fn frustum_in(lo : vec3<f32>, hi : vec3<f32>) -> bool {
  for (var i = 0u; i < 6u; i = i + 1u) {
    let p = params.planes[i];
    let pv = vec3<f32>(select(lo.x, hi.x, p.x >= 0.0),
                       select(lo.y, hi.y, p.y >= 0.0),
                       select(lo.z, hi.z, p.z >= 0.0));
    if (dot(p.xyz, pv) + p.w < 0.0) { return false; }
  }
  return true;
}
@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let i = gid.x;
  if (i >= params.object_count) { return; }
  if (frustum_in(aabbs[i].lo.xyz, aabbs[i].hi.xyz)) { draws[i].instance_count = 1u; }
  else { draws[i].instance_count = 0u; }
}
)WGSL";

    if (!gc_cull_shader_)
        gc_cull_shader_ = dev_->CreateComputeShaderEx("", "", "", 2, 0, 0, 0, kCullWGSL);

    // 剔除参数：保留区域 x∈[-1,1] y∈[-1,1] z∈[-10,10]（6 视锥面），实例数 4。
    if (!gc_params_ubo_) {
        struct CullParams { float planes[6][4]; uint32_t object_count; uint32_t pad[3]; } p{};
        const float planes[6][4] = {
            { 1, 0, 0, 1}, {-1, 0, 0, 1}, {0,  1, 0, 1},
            { 0,-1, 0, 1}, { 0, 0, 1,10}, {0,  0,-1,10}};
        std::memcpy(p.planes, planes, sizeof(planes));
        p.object_count = kGcInstances;
        GpuBufferDesc d; d.size = sizeof(p); d.usage = GpuBufferUsage::kUniform;
        gc_params_ubo_ = dev_->CreateGpuBuffer(d, &p).raw();
    }
    // 4 实例 AABB：inst0 原点(视锥内) / inst1 x=5(出界) / inst2 z=5(界内) / inst3 y=5(出界)。
    if (!gc_aabb_ssbo_) {
        const float aabbs[kGcInstances][8] = {
            {-0.4f,-0.4f,-0.4f,0, 0.4f,0.4f,0.4f,0},   // inst0 → 可见
            { 4.6f,-0.4f,-0.4f,0, 5.4f,0.4f,0.4f,0},   // inst1 → 剔除(x>1)
            {-0.4f,-0.4f, 4.6f,0, 0.4f,0.4f,5.4f,0},   // inst2 → 可见
            {-0.4f, 4.6f,-0.4f,0, 0.4f,5.4f,0.4f,0}};  // inst3 → 剔除(y>1)
        GpuBufferDesc d; d.size = sizeof(aabbs); d.usage = GpuBufferUsage::kStorage;
        gc_aabb_ssbo_ = dev_->CreateGpuBuffer(d, aabbs).raw();
    }
    // per-instance indirect draw commands（预置 count=6/first_index=i*6；instance_count 由剔除写）。
    if (!gc_draw_ssbo_) {
        uint32_t cmds[kGcInstances * kGcDrawWords];
        for (uint32_t i = 0; i < kGcInstances; ++i) {
            cmds[i * kGcDrawWords + 0] = 6u;       // index_count
            cmds[i * kGcDrawWords + 1] = 1u;       // instance_count（剔除覆写）
            cmds[i * kGcDrawWords + 2] = i * 6u;   // first_index
            cmds[i * kGcDrawWords + 3] = 0u;       // base_vertex
            cmds[i * kGcDrawWords + 4] = 0u;       // base_instance
        }
        GpuBufferDesc d; d.size = sizeof(cmds);
        d.usage = GpuBufferUsage::kStorage | GpuBufferUsage::kIndirect;
        gc_draw_ssbo_ = dev_->CreateGpuBuffer(d, cmds).raw();
    }

    // --- 离屏 RT（64×64 RGBA8）+ 渲染管线 + 4 象限不同色 quad 顶点/索引缓冲 ---
    if (!gc_rt_tex_) {
        WGPUTextureDescriptor td{};
        td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        td.dimension = WGPUTextureDimension_2D;
        td.size = {kGcRtSize, kGcRtSize, 1};
        td.format = WGPUTextureFormat_RGBA8Unorm;
        td.mipLevelCount = 1; td.sampleCount = 1;
        gc_rt_tex_ = wgpuDeviceCreateTexture(dev_->device_, &td);
        if (gc_rt_tex_) gc_rt_view_ = wgpuTextureCreateView(gc_rt_tex_, nullptr);
    }
    if (!gc_render_module_) {
        static const char* kRenderWGSL = R"WGSL(
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) color : vec3<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) c : vec3<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.color = c;
  return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> { return vec4<f32>(i.color, 1.0); }
)WGSL";
        gc_render_module_ = dev_->CompileWGSL(kRenderWGSL, "dse-gpu-cull-selftest-render");
    }
    if (!gc_pipeline_ && gc_render_module_) {
        WGPUVertexAttribute attrs[2]{};
        attrs[0].format = WGPUVertexFormat_Float32x2; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
        attrs[1].format = WGPUVertexFormat_Float32x3; attrs[1].offset = 8;  attrs[1].shaderLocation = 1;
        WGPUVertexBufferLayout vbl{};
        vbl.arrayStride = 5 * sizeof(float);
        vbl.stepMode = WGPUVertexStepMode_Vertex;
        vbl.attributeCount = 2; vbl.attributes = attrs;
        WGPUColorTargetState color{};
        color.format = WGPUTextureFormat_RGBA8Unorm;
        color.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fs{};
        fs.module = gc_render_module_; fs.entryPoint = "fs_main";
        fs.targetCount = 1; fs.targets = &color;
        WGPURenderPipelineDescriptor rpd{};
        rpd.layout = nullptr;  // 无绑定资源 → auto layout
        rpd.vertex.module = gc_render_module_; rpd.vertex.entryPoint = "vs_main";
        rpd.vertex.bufferCount = 1; rpd.vertex.buffers = &vbl;
        rpd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        rpd.primitive.cullMode = WGPUCullMode_None;
        rpd.primitive.frontFace = WGPUFrontFace_CCW;
        rpd.multisample.count = 1; rpd.multisample.mask = 0xFFFFFFFF;
        rpd.fragment = &fs;
        gc_pipeline_ = wgpuDeviceCreateRenderPipeline(dev_->device_, &rpd);
    }
    if (!gc_vbo_) {
        // 4 个象限 quad（中心 ±0.5），半边 0.35，各一种颜色。
        const float h = 0.35f;
        const float cx[kGcInstances] = {-0.5f, 0.5f, -0.5f, 0.5f};
        const float cy[kGcInstances] = { 0.5f, 0.5f, -0.5f, -0.5f};
        const float col[kGcInstances][3] = {{1,0,0},{0,1,0},{0,0,1},{1,1,0}};
        float verts[kGcInstances * 4 * 5];
        size_t v = 0;
        for (uint32_t i = 0; i < kGcInstances; ++i) {
            const float qx[4] = {cx[i]-h, cx[i]+h, cx[i]+h, cx[i]-h};
            const float qy[4] = {cy[i]-h, cy[i]-h, cy[i]+h, cy[i]+h};
            for (int k = 0; k < 4; ++k) {
                verts[v++] = qx[k]; verts[v++] = qy[k];
                verts[v++] = col[i][0]; verts[v++] = col[i][1]; verts[v++] = col[i][2];
            }
        }
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
        bd.size = sizeof(verts);
        gc_vbo_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
        if (gc_vbo_) wgpuQueueWriteBuffer(dev_->queue_, gc_vbo_, 0, verts, sizeof(verts));
    }
    if (!gc_ibo_) {
        uint32_t idx[kGcInstances * 6];
        for (uint32_t i = 0; i < kGcInstances; ++i) {
            const uint32_t b = i * 4;
            idx[i*6+0]=b+0; idx[i*6+1]=b+1; idx[i*6+2]=b+2;
            idx[i*6+3]=b+0; idx[i*6+4]=b+2; idx[i*6+5]=b+3;
        }
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
        bd.size = sizeof(idx);
        gc_ibo_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
        if (gc_ibo_) wgpuQueueWriteBuffer(dev_->queue_, gc_ibo_, 0, idx, sizeof(idx));
    }

    if (!gc_cull_shader_ || !gc_params_ubo_ || !gc_aabb_ssbo_ || !gc_draw_ssbo_ ||
        !gc_rt_view_ || !gc_pipeline_ || !gc_vbo_ || !gc_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-2] GPU-driven 剔除自检资源创建失败，跳过");
        return false;
    }

    // --- 录制 1：视锥剔除 compute（写 draw cmds instance_count）---
    dev_->ResetDrawState();
    dev_->CmdBindUniformBuffer(0, gc_params_ubo_, 0, 112);
    dev_->CmdBindStorageBuffer(0, gc_aabb_ssbo_, 0, kGcInstances * 32);
    dev_->CmdBindStorageBuffer(1, gc_draw_ssbo_, 0, kGcInstances * kGcDrawWords * sizeof(uint32_t));
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(gc_cull_shader_, 1, 1, 1);  // 4 实例 < 64 线程 → 1 workgroup
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // --- 录制 2：真 DrawIndexedIndirect 渲到离屏 RT（被剔实例 instance_count=0 → 不绘制）---
    const WebGPURhiDevice::BufferEntry* be_draw = dev_->FindBuffer(gc_draw_ssbo_);
    if (!be_draw || !be_draw->buffer) return false;
    WGPURenderPassColorAttachment att{};
    att.view = gc_rt_view_;
    att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};
    WGPURenderPassDescriptor pd{};
    pd.colorAttachmentCount = 1; pd.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(dev_->frame_encoder_, &pd);
    wgpuRenderPassEncoderSetPipeline(pass, gc_pipeline_);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, gc_vbo_, 0, kGcInstances * 4 * 5 * sizeof(float));
    wgpuRenderPassEncoderSetIndexBuffer(pass, gc_ibo_, WGPUIndexFormat_Uint32, 0,
                                        kGcInstances * 6 * sizeof(uint32_t));
    for (uint32_t i = 0; i < kGcInstances; ++i)
        wgpuRenderPassEncoderDrawIndexedIndirect(pass, be_draw->buffer,
                                                 i * kGcDrawWords * sizeof(uint32_t));
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    // --- copy draw cmds + RT 像素到回读缓冲（随帧提交）---
    auto make_rb = [&](uint64_t bytes) -> WGPUBuffer {
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bd.size = AlignUp4(bytes);
        return wgpuDeviceCreateBuffer(dev_->device_, &bd);
    };
    gc_rb_draw_ = make_rb(kGcInstances * kGcDrawWords * sizeof(uint32_t));
    gc_rb_pixels_ = make_rb(kGcRtBytes);
    if (!gc_rb_draw_ || !gc_rb_pixels_) {
        if (gc_rb_draw_) { wgpuBufferRelease(gc_rb_draw_); gc_rb_draw_ = nullptr; }
        if (gc_rb_pixels_) { wgpuBufferRelease(gc_rb_pixels_); gc_rb_pixels_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_draw->buffer, 0, gc_rb_draw_, 0,
                                         kGcInstances * kGcDrawWords * sizeof(uint32_t));
    WGPUImageCopyTexture src{};
    src.texture = gc_rt_tex_;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = gc_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kGcRtRowBytes;
    dst.layout.rowsPerImage = kGcRtSize;
    WGPUExtent3D ext{kGcRtSize, kGcRtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickGpuCullSelfTestReadback() {
    if (!gc_rb_draw_ || !gc_rb_pixels_) return;
    // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    auto* ctx = new GpuCullSelfTestCtx();
    ctx->rb_draw = gc_rb_draw_;
    ctx->rb_pixels = gc_rb_pixels_;
    ctx->rt_tex = gc_rt_tex_;       gc_rt_tex_ = nullptr;
    ctx->rt_view = gc_rt_view_;     gc_rt_view_ = nullptr;
    ctx->pipeline = gc_pipeline_;   gc_pipeline_ = nullptr;
    ctx->module = gc_render_module_; gc_render_module_ = nullptr;
    ctx->vbo = gc_vbo_;             gc_vbo_ = nullptr;
    ctx->ibo = gc_ibo_;             gc_ibo_ = nullptr;
    gc_rb_draw_ = nullptr;
    gc_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_draw, WGPUMapMode_Read, 0,
                       kGcInstances * kGcDrawWords * sizeof(uint32_t), OnGpuCullDrawMapped, ctx);
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kGcRtBytes, OnGpuCullPixelsMapped, ctx);
}

// B3b-3：GPU 蒙皮真链路自检——手译自 builtin skinning.comp 的 WGSL 蒙皮 compute（骨骼矩阵
// 调色板 + morph 混合，逐位对齐源 GLSL 的 SrcVertex/DstVertex/InstanceInfo 打包与算法）写蒙皮后
// 顶点 SSBO，该 SSBO 直接作顶点缓冲被真绘制消费渲到离屏 RT，回读 dst SSBO + 像素双重校验。
bool WebGpuSelfTestHarness::RecordSkinningSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // --- WGSL 蒙皮 compute（group1 b0 = 参数；group3 b0..b4 = src/dst/bone/morph/inst SSBO）---
    // 注：本后端 compute BGL 统一以 read_write storage 建组，故所有 storage 均声明 read_write。
    static const char* kSkinningWGSL = R"WGSL(// dse-wgsl
struct SrcVertex { pos_bw0 : vec4<f32>, norm_bw1 : vec4<f32>, tan_bw2 : vec4<f32>, joints_bw3 : vec4<f32>, };
struct DstVertex { pos : vec4<f32>, normal : vec4<f32>, tangent : vec4<f32>, };
struct InstanceInfo {
  vertex_start : u32, vertex_count : u32, bone_offset : u32, morph_target_count : u32,
  morph_weights : vec4<f32>,
  morph_delta_offset : u32, pad0 : u32, pad1 : u32, pad2 : u32,
};
struct Params { total_vertices : u32, instance_count : u32, pad0 : u32, pad1 : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(3) @binding(0) var<storage, read_write> src_vertices : array<SrcVertex>;
@group(3) @binding(1) var<storage, read_write> dst_vertices : array<DstVertex>;
@group(3) @binding(2) var<storage, read_write> bone_matrices : array<mat4x4<f32>>;
@group(3) @binding(3) var<storage, read_write> morph_deltas : array<vec4<f32>>;
@group(3) @binding(4) var<storage, read_write> instances : array<InstanceInfo>;
fn find_instance(gid : u32) -> u32 {
  var lo : u32 = 0u; var hi : u32 = params.instance_count;
  while (lo < hi) {
    let mid = (lo + hi) / 2u;
    if (instances[mid].vertex_start + instances[mid].vertex_count <= gid) { lo = mid + 1u; }
    else { hi = mid; }
  }
  return lo;
}
@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid3 : vec3<u32>) {
  let gid = gid3.x;
  if (gid >= params.total_vertices) { return; }
  let inst_id = find_instance(gid);
  if (inst_id >= params.instance_count) { return; }
  let inst = instances[inst_id];
  let local_vid = gid - inst.vertex_start;
  let s = src_vertices[gid];
  var pos = s.pos_bw0.xyz;
  let normal = s.norm_bw1.xyz;
  let tangent = s.tan_bw2.xyz;
  if (inst.morph_target_count > 0u) {
    let w = array<f32, 4>(inst.morph_weights.x, inst.morph_weights.y,
                          inst.morph_weights.z, inst.morph_weights.w);
    let mbase = inst.morph_delta_offset;
    for (var m = 0u; m < inst.morph_target_count && m < 4u; m = m + 1u) {
      if (abs(w[m]) > 0.001) {
        pos = pos + morph_deltas[mbase + m * inst.vertex_count + local_vid].xyz * w[m];
      }
    }
  }
  let bw0 = s.pos_bw0.w; let bw1 = s.norm_bw1.w; let bw2 = s.tan_bw2.w;
  let bw3 = 1.0 - bw0 - bw1 - bw2;
  let bi0 = u32(s.joints_bw3.x); let bi1 = u32(s.joints_bw3.y);
  let bi2 = u32(s.joints_bw3.z); let bi3 = u32(s.joints_bw3.w);
  let bb = inst.bone_offset;
  let sm = bone_matrices[bb + bi0] * bw0 + bone_matrices[bb + bi1] * bw1
         + bone_matrices[bb + bi2] * bw2 + bone_matrices[bb + bi3] * bw3;
  let nm = mat3x3<f32>(sm[0].xyz, sm[1].xyz, sm[2].xyz);
  dst_vertices[gid].pos     = vec4<f32>((sm * vec4<f32>(pos, 1.0)).xyz, 1.0);
  dst_vertices[gid].normal  = vec4<f32>(normalize(nm * normal), 0.0);
  dst_vertices[gid].tangent = vec4<f32>(normalize(nm * tangent), 0.0);
}
)WGSL";

    if (!sk_shader_)
        sk_shader_ = dev_->CreateComputeShaderEx("", "", "", 5, 0, 0, 0, kSkinningWGSL);

    // 参数 UBO：总顶点数 / 实例数。
    if (!sk_params_ubo_) {
        const uint32_t params[4] = {kSkVertices, kSkInstances, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        sk_params_ubo_ = dev_->CreateGpuBuffer(d, params).raw();
    }
    // 源顶点：居中原点 quad（半边 kSkHalf），全 100% 权重于 bone0（pos.w=1、其余权重 0、关节全 0），
    //   法线 (0,0,1)、切线 (1,0,0)。打包逐位对齐 skinning.comp 的 SrcVertex。
    if (!sk_src_ssbo_) {
        const float bx[kSkVertices] = {-kSkHalf,  kSkHalf, kSkHalf, -kSkHalf};
        const float by[kSkVertices] = {-kSkHalf, -kSkHalf, kSkHalf,  kSkHalf};
        float src[kSkVertices * kSkSrcFloats];
        for (uint32_t i = 0; i < kSkVertices; ++i) {
            float* v = src + i * kSkSrcFloats;
            v[0]=bx[i]; v[1]=by[i]; v[2]=0.0f; v[3]=1.0f;   // pos_bw0：位置 + bone_weight[0]=1
            v[4]=0.0f;  v[5]=0.0f;  v[6]=1.0f; v[7]=0.0f;   // norm_bw1：法线 + bone_weight[1]=0
            v[8]=1.0f;  v[9]=0.0f;  v[10]=0.0f;v[11]=0.0f;  // tan_bw2：切线 + bone_weight[2]=0
            v[12]=0.0f; v[13]=0.0f; v[14]=0.0f;v[15]=0.0f;  // joints_bw3：4 关节索引（全 0）
        }
        GpuBufferDesc d; d.size = sizeof(src); d.usage = GpuBufferUsage::kStorage;
        sk_src_ssbo_ = dev_->CreateGpuBuffer(d, src).raw();
    }
    // 蒙皮后顶点：compute 写入（storage）+ 作绘制顶点缓冲（vertex）。
    if (!sk_dst_ssbo_) {
        GpuBufferDesc d; d.size = kSkVertices * kSkDstFloats * sizeof(float);
        d.usage = GpuBufferUsage::kStorage | GpuBufferUsage::kVertex;
        sk_dst_ssbo_ = dev_->CreateGpuBuffer(d, nullptr).raw();
    }
    // 骨骼矩阵调色板（列主序）：bone0 = 平移(kSkBoneTx,kSkBoneTy,0)，bone1 = 单位（未引用）。
    if (!sk_bone_ssbo_) {
        float bones[kSkBones * 16] = {
            1,0,0,0, 0,1,0,0, 0,0,1,0, kSkBoneTx,kSkBoneTy,0,1,   // bone0：平移
            1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};                  // bone1：单位
        GpuBufferDesc d; d.size = sizeof(bones); d.usage = GpuBufferUsage::kStorage;
        sk_bone_ssbo_ = dev_->CreateGpuBuffer(d, bones).raw();
    }
    // morph delta（占位，本自检 morph_target_count=0 不访问；仍需存在并绑定以匹配着色器声明）。
    if (!sk_morph_ssbo_) {
        float zeros[kSkVertices * 4] = {0};
        GpuBufferDesc d; d.size = sizeof(zeros); d.usage = GpuBufferUsage::kStorage;
        sk_morph_ssbo_ = dev_->CreateGpuBuffer(d, zeros).raw();
    }
    // 实例信息：单实例覆盖全部顶点，bone_offset=0，无 morph。
    if (!sk_inst_ssbo_) {
        struct InstanceInfo {
            uint32_t vertex_start, vertex_count, bone_offset, morph_target_count;
            float morph_weights[4];
            uint32_t morph_delta_offset, pad0, pad1, pad2;
        } inst{};
        inst.vertex_start = 0; inst.vertex_count = kSkVertices; inst.bone_offset = 0;
        inst.morph_target_count = 0; inst.morph_delta_offset = 0;
        GpuBufferDesc d; d.size = sizeof(inst); d.usage = GpuBufferUsage::kStorage;
        sk_inst_ssbo_ = dev_->CreateGpuBuffer(d, &inst).raw();
    }

    // --- 离屏 RT（64×64 RGBA8）+ 渲染管线（顶点拉取蒙皮后顶点 + 固定红色）+ 索引缓冲 ---
    if (!sk_rt_tex_) {
        WGPUTextureDescriptor td{};
        td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        td.dimension = WGPUTextureDimension_2D;
        td.size = {kSkRtSize, kSkRtSize, 1};
        td.format = WGPUTextureFormat_RGBA8Unorm;
        td.mipLevelCount = 1; td.sampleCount = 1;
        sk_rt_tex_ = wgpuDeviceCreateTexture(dev_->device_, &td);
        if (sk_rt_tex_) sk_rt_view_ = wgpuTextureCreateView(sk_rt_tex_, nullptr);
    }
    if (!sk_render_module_) {
        static const char* kRenderWGSL = R"WGSL(
@vertex fn vs_main(@location(0) p : vec2<f32>) -> @builtin(position) vec4<f32> {
  return vec4<f32>(p, 0.0, 1.0);
}
@fragment fn fs_main() -> @location(0) vec4<f32> { return vec4<f32>(1.0, 0.0, 0.0, 1.0); }
)WGSL";
        sk_render_module_ = dev_->CompileWGSL(kRenderWGSL, "dse-skinning-selftest-render");
    }
    if (!sk_pipeline_ && sk_render_module_) {
        // 顶点缓冲 = 蒙皮后 DstVertex（步幅 48B），属性 0 = pos.xy（offset 0，float32x2）。
        WGPUVertexAttribute attr{};
        attr.format = WGPUVertexFormat_Float32x2; attr.offset = 0; attr.shaderLocation = 0;
        WGPUVertexBufferLayout vbl{};
        vbl.arrayStride = kSkDstStride;
        vbl.stepMode = WGPUVertexStepMode_Vertex;
        vbl.attributeCount = 1; vbl.attributes = &attr;
        WGPUColorTargetState color{};
        color.format = WGPUTextureFormat_RGBA8Unorm;
        color.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fs{};
        fs.module = sk_render_module_; fs.entryPoint = "fs_main";
        fs.targetCount = 1; fs.targets = &color;
        WGPURenderPipelineDescriptor rpd{};
        rpd.layout = nullptr;  // 无绑定资源 → auto layout
        rpd.vertex.module = sk_render_module_; rpd.vertex.entryPoint = "vs_main";
        rpd.vertex.bufferCount = 1; rpd.vertex.buffers = &vbl;
        rpd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        rpd.primitive.cullMode = WGPUCullMode_None;
        rpd.primitive.frontFace = WGPUFrontFace_CCW;
        rpd.multisample.count = 1; rpd.multisample.mask = 0xFFFFFFFF;
        rpd.fragment = &fs;
        sk_pipeline_ = wgpuDeviceCreateRenderPipeline(dev_->device_, &rpd);
    }
    if (!sk_ibo_) {
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
        bd.size = sizeof(idx);
        sk_ibo_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
        if (sk_ibo_) wgpuQueueWriteBuffer(dev_->queue_, sk_ibo_, 0, idx, sizeof(idx));
    }

    if (!sk_shader_ || !sk_params_ubo_ || !sk_src_ssbo_ || !sk_dst_ssbo_ || !sk_bone_ssbo_ ||
        !sk_morph_ssbo_ || !sk_inst_ssbo_ || !sk_rt_view_ || !sk_pipeline_ || !sk_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-3] GPU 蒙皮自检资源创建失败，跳过");
        return false;
    }

    // --- 录制 1：蒙皮 compute（写 dst 蒙皮后顶点）---
    dev_->ResetDrawState();
    dev_->CmdBindUniformBuffer(0, sk_params_ubo_, 0, sizeof(uint32_t) * 4);
    dev_->CmdBindStorageBuffer(0, sk_src_ssbo_,  0, kSkVertices * kSkSrcFloats * sizeof(float));
    dev_->CmdBindStorageBuffer(1, sk_dst_ssbo_,  0, kSkVertices * kSkDstFloats * sizeof(float));
    dev_->CmdBindStorageBuffer(2, sk_bone_ssbo_, 0, kSkBones * 16 * sizeof(float));
    dev_->CmdBindStorageBuffer(3, sk_morph_ssbo_,0, kSkVertices * 4 * sizeof(float));
    dev_->CmdBindStorageBuffer(4, sk_inst_ssbo_, 0, 48);
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(sk_shader_, (kSkVertices + 63u) / 64u, 1, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // --- 录制 2：真绘制（顶点缓冲 = 蒙皮后 dst SSBO）渲到离屏 RT ---
    const WebGPURhiDevice::BufferEntry* be_dst = dev_->FindBuffer(sk_dst_ssbo_);
    if (!be_dst || !be_dst->buffer) return false;
    WGPURenderPassColorAttachment att{};
    att.view = sk_rt_view_;
    att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};
    WGPURenderPassDescriptor pd{};
    pd.colorAttachmentCount = 1; pd.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(dev_->frame_encoder_, &pd);
    wgpuRenderPassEncoderSetPipeline(pass, sk_pipeline_);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, be_dst->buffer, 0,
                                         kSkVertices * kSkDstFloats * sizeof(float));
    wgpuRenderPassEncoderSetIndexBuffer(pass, sk_ibo_, WGPUIndexFormat_Uint32, 0, 6 * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(pass, 6, 1, 0, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    // --- copy 蒙皮后顶点 SSBO + RT 像素到回读缓冲（随帧提交）---
    auto make_rb = [&](uint64_t bytes) -> WGPUBuffer {
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bd.size = AlignUp4(bytes);
        return wgpuDeviceCreateBuffer(dev_->device_, &bd);
    };
    sk_rb_dst_ = make_rb(kSkVertices * kSkDstFloats * sizeof(float));
    sk_rb_pixels_ = make_rb(kSkRtBytes);
    if (!sk_rb_dst_ || !sk_rb_pixels_) {
        if (sk_rb_dst_)    { wgpuBufferRelease(sk_rb_dst_);    sk_rb_dst_ = nullptr; }
        if (sk_rb_pixels_) { wgpuBufferRelease(sk_rb_pixels_); sk_rb_pixels_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_dst->buffer, 0, sk_rb_dst_, 0,
                                         kSkVertices * kSkDstFloats * sizeof(float));
    WGPUImageCopyTexture src{};
    src.texture = sk_rt_tex_;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = sk_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kSkRtRowBytes;
    dst.layout.rowsPerImage = kSkRtSize;
    WGPUExtent3D ext{kSkRtSize, kSkRtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickSkinningSelfTestReadback() {
    if (!sk_rb_dst_ || !sk_rb_pixels_) return;
    // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    auto* ctx = new SkinningSelfTestCtx();
    ctx->rb_dst = sk_rb_dst_;
    ctx->rb_pixels = sk_rb_pixels_;
    ctx->rt_tex = sk_rt_tex_;         sk_rt_tex_ = nullptr;
    ctx->rt_view = sk_rt_view_;       sk_rt_view_ = nullptr;
    ctx->pipeline = sk_pipeline_;     sk_pipeline_ = nullptr;
    ctx->module = sk_render_module_;  sk_render_module_ = nullptr;
    ctx->ibo = sk_ibo_;               sk_ibo_ = nullptr;
    sk_rb_dst_ = nullptr;
    sk_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_dst, WGPUMapMode_Read, 0,
                       kSkVertices * kSkDstFloats * sizeof(float), OnSkinningDstMapped, ctx);
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kSkRtBytes, OnSkinningPixelsMapped, ctx);
}

// B3b-4：storage-image 写 compute 真链路自检——compute 经 texture_storage_2d<rgba8unorm, write>
// 把已知渐变逐像素 textureStore 进 storage 纹理（经 CreateComputeWriteTexture2D + SetComputeTextureImage
// group2 绑定），随后 copy 纹理→回读缓冲，逐像素校验。详见 StorageImageSelfTestCtx / OnStorageImagePixelsMapped。
bool WebGpuSelfTestHarness::RecordStorageImageSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // group1 b0 = UBO（dim）；group2 b0 = storage image（write）。
    static const char* kStorageImageWGSL = R"WGSL(// dse-wgsl
struct Params { dim : u32, pad0 : u32, pad1 : u32, pad2 : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(2) @binding(0) var dst_img : texture_storage_2d<rgba8unorm, write>;
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (gid.x >= params.dim || gid.y >= params.dim) { return; }
  let denom = f32(params.dim - 1u);
  let r = f32(gid.x) / denom;
  let g = f32(gid.y) / denom;
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(r, g, 0.25, 1.0));
}
)WGSL";

    if (!si_shader_) si_shader_ = dev_->CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kStorageImageWGSL);
    if (!si_params_ubo_) {
        const uint32_t params[4] = {kSiDim, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        si_params_ubo_ = dev_->CreateGpuBuffer(d, params).raw();
    }
    if (!si_image_) si_image_ = dev_->CreateComputeWriteTexture2D(static_cast<int>(kSiDim), static_cast<int>(kSiDim));
    if (!si_shader_ || !si_params_ubo_ || !si_image_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-4] storage-image 自检资源创建失败，跳过");
        return false;
    }

    // 经与引擎相同的命令录制状态绑定资源（group1 b0 UBO；group2 b0 storage image）。
    dev_->ResetDrawState();
    dev_->CmdBindUniformBuffer(0, si_params_ubo_, 0, sizeof(uint32_t) * 4);
    dev_->SetComputeTextureImage(0, si_image_, /*read_only=*/false);

    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(si_shader_, (kSiDim + 7u) / 8u, (kSiDim + 7u) / 8u, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // copy storage 纹理 → 回读缓冲（MapRead|CopyDst；随帧提交）。
    const WebGPURhiDevice::TextureEntry* te = dev_->FindTexture(si_image_);
    if (!te || !te->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kSiBytes);
    si_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!si_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = te->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = si_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kSiRowBytes;
    dst.layout.rowsPerImage = kSiDim;
    WGPUExtent3D ext{kSiDim, kSiDim, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickStorageImageSelfTestReadback() {
    if (!si_rb_pixels_) return;
    // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    auto* ctx = new StorageImageSelfTestCtx();
    ctx->rb_pixels = si_rb_pixels_;
    si_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kSiBytes, OnStorageImagePixelsMapped, ctx);
}

// B3b-5：Hi-Z 下采样核心 compute 真链路自检——两趟 r32float compute：①生成趟经
// texture_storage_2d<r32float, write> 写已知渐变 src[x,y]=f32(x)+f32(y)*100 到 src 纹理；②下采样趟用
// textureLoad 读 src（采样纹理，unfilterable-float）+ 取 2×2 max 写 dst（边长减半）→ copy dst→回读缓冲，
// 逐像素校验 == CPU 预期 max。验证 compute 采样读 + r32float storage 写的读后写链路（Hi-Z 金字塔逐级
// 下采样核心原语）。详见 HiZSelfTestCtx / OnHiZPixelsMapped。
bool WebGpuSelfTestHarness::RecordHiZDownsampleSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // ①生成趟：group1 b0 = UBO（dim）；group2 b0 = storage image（r32float 写）。
    static const char* kHzGenWGSL = R"WGSL(// dse-wgsl
struct Params { dim : u32, pad0 : u32, pad1 : u32, pad2 : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(2) @binding(0) var dst_img : texture_storage_2d<r32float, write>;
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (gid.x >= params.dim || gid.y >= params.dim) { return; }
  let v = f32(gid.x) + f32(gid.y) * 100.0;
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(v, 0.0, 0.0, 0.0));
}
)WGSL";

    // ②下采样趟：group1 b0 = UBO（src_dim/dst_dim）；group2 b0 = 采样纹理（src 读，textureLoad）；
    //   group2 b1 = storage image（dst r32float 写）。
    static const char* kHzDownWGSL = R"WGSL(// dse-wgsl
struct Params { src_dim : u32, dst_dim : u32, pad0 : u32, pad1 : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var dst_img : texture_storage_2d<r32float, write>;
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (gid.x >= params.dst_dim || gid.y >= params.dst_dim) { return; }
  let m = i32(params.src_dim) - 1;
  let sx = i32(gid.x * 2u);
  let sy = i32(gid.y * 2u);
  let d00 = textureLoad(src_tex, vec2<i32>(sx, sy), 0).r;
  let d10 = textureLoad(src_tex, vec2<i32>(min(sx + 1, m), sy), 0).r;
  let d01 = textureLoad(src_tex, vec2<i32>(sx, min(sy + 1, m)), 0).r;
  let d11 = textureLoad(src_tex, vec2<i32>(min(sx + 1, m), min(sy + 1, m)), 0).r;
  let mx = max(max(d00, d10), max(d01, d11));
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(mx, 0.0, 0.0, 0.0));
}
)WGSL";

    if (!hz_gen_shader_)  hz_gen_shader_  = dev_->CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kHzGenWGSL);
    if (!hz_down_shader_) hz_down_shader_ = dev_->CreateComputeShaderEx("", "", "", 0, 1, 1, 0, kHzDownWGSL);
    if (!hz_gen_ubo_) {
        const uint32_t params[4] = {kHzSrcDim, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        hz_gen_ubo_ = dev_->CreateGpuBuffer(d, params).raw();
    }
    if (!hz_down_ubo_) {
        const uint32_t params[4] = {kHzSrcDim, kHzDstDim, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        hz_down_ubo_ = dev_->CreateGpuBuffer(d, params).raw();
    }
    // src：生成趟 storage 写 + 下采样趟采样读；dst：下采样趟 storage 写 + copy 源。r32float 为
    // WebGPU 保证支持 storage 写的格式之一；作采样纹理时为 unfilterable-float（仅 textureLoad）。
    if (!hz_src_tex_) {
        hz_src_tex_ = dev_->CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kHzSrcDim, kHzSrcDim, 1,
            WGPUTextureFormat_R32Float,
            WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            1, 1, {nullptr}, TextureSamplerDesc::FromLinearFlag(false));
    }
    if (!hz_dst_tex_) {
        hz_dst_tex_ = dev_->CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kHzDstDim, kHzDstDim, 1,
            WGPUTextureFormat_R32Float,
            WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst,
            1, 1, {nullptr}, TextureSamplerDesc::FromLinearFlag(false));
    }
    if (!hz_gen_shader_ || !hz_down_shader_ || !hz_gen_ubo_ || !hz_down_ubo_ || !hz_src_tex_ || !hz_dst_tex_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-5] Hi-Z 下采样自检资源创建失败，跳过");
        return false;
    }

    // ①生成趟。
    dev_->ResetDrawState();
    dev_->CmdBindUniformBuffer(0, hz_gen_ubo_, 0, sizeof(uint32_t) * 4);
    dev_->SetComputeTextureImage(0, hz_src_tex_, /*read_only=*/false);
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(hz_gen_shader_, (kHzSrcDim + 7u) / 8u, (kHzSrcDim + 7u) / 8u, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // ②下采样趟（独立 compute pass：pass 间自动屏障保证 src 写对下采样趟可见）。
    dev_->CmdBindUniformBuffer(0, hz_down_ubo_, 0, sizeof(uint32_t) * 4);
    dev_->SetComputeTextureImage(0, hz_src_tex_, /*read_only=*/true);
    dev_->SetComputeTextureImage(1, hz_dst_tex_, /*read_only=*/false);
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(hz_down_shader_, (kHzDstDim + 7u) / 8u, (kHzDstDim + 7u) / 8u, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // copy dst storage 纹理 → 回读缓冲（MapRead|CopyDst；随帧提交）。
    const WebGPURhiDevice::TextureEntry* te = dev_->FindTexture(hz_dst_tex_);
    if (!te || !te->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kHzDstBytes;
    hz_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!hz_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = te->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = hz_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kHzDstRowBytes;
    dst.layout.rowsPerImage = kHzDstDim;
    WGPUExtent3D ext{kHzDstDim, kHzDstDim, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickHiZDownsampleSelfTestReadback() {
    if (!hz_rb_pixels_) return;
    auto* ctx = new HiZSelfTestCtx();
    ctx->rb_pixels = hz_rb_pixels_;
    hz_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kHzDstBytes, OnHiZPixelsMapped, ctx);
}

// B3b-6：Hi-Z storage-image 金字塔 compute 真链路自检——单张 R32Float mip 链纹理（mip0=8×8 ..
// mip3=1×1）。①生成趟经 texture_storage_2d<r32float, write> 写已知渐变到 mip0 单 mip 视图；②逐级
// 下采样趟用 textureLoad 读 mip[k-1] 单 mip 采样视图 + 取 2×2 max 写 mip[k] 单 mip storage 视图
// （per-mip 显式视图绑定，绕开默认全 mip 视图）。各级 mip copy 到回读缓冲 256 对齐分段后逐级校验
// == CPU 预期递归 max。验证 per-mip 视图绑定 + 多级 storage 金字塔构建（GPU-driven Hi-Z 遮挡剔除
// 金字塔核心原语，Task 4 前置）。详见 HiZPyramidSelfTestCtx / OnHiZPyramidPixelsMapped。
bool WebGpuSelfTestHarness::RecordHiZPyramidSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // 生成趟 / 下采样趟 WGSL 与 B3b-5 同形（group2 b0 storage 写；b0 采样读 + b1 storage 写），
    // 仅此处用显式单 mip 视图绑定到 group2 槽，实现金字塔逐级 in-place 下采样。
    static const char* kHzpGenWGSL = R"WGSL(// dse-wgsl
struct Params { dim : u32, pad0 : u32, pad1 : u32, pad2 : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(2) @binding(0) var dst_img : texture_storage_2d<r32float, write>;
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (gid.x >= params.dim || gid.y >= params.dim) { return; }
  let v = f32(gid.x) + f32(gid.y) * 100.0;
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(v, 0.0, 0.0, 0.0));
}
)WGSL";
    static const char* kHzpDownWGSL = R"WGSL(// dse-wgsl
struct Params { src_dim : u32, dst_dim : u32, pad0 : u32, pad1 : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var dst_img : texture_storage_2d<r32float, write>;
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (gid.x >= params.dst_dim || gid.y >= params.dst_dim) { return; }
  let m = i32(params.src_dim) - 1;
  let sx = i32(gid.x * 2u);
  let sy = i32(gid.y * 2u);
  let d00 = textureLoad(src_tex, vec2<i32>(sx, sy), 0).r;
  let d10 = textureLoad(src_tex, vec2<i32>(min(sx + 1, m), sy), 0).r;
  let d01 = textureLoad(src_tex, vec2<i32>(sx, min(sy + 1, m)), 0).r;
  let d11 = textureLoad(src_tex, vec2<i32>(min(sx + 1, m), min(sy + 1, m)), 0).r;
  let mx = max(max(d00, d10), max(d01, d11));
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(mx, 0.0, 0.0, 0.0));
}
)WGSL";

    if (!hzp_gen_shader_)  hzp_gen_shader_  = dev_->CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kHzpGenWGSL);
    if (!hzp_down_shader_) hzp_down_shader_ = dev_->CreateComputeShaderEx("", "", "", 0, 1, 1, 0, kHzpDownWGSL);
    if (!hzp_gen_ubo_) {
        const uint32_t params[4] = {kHzpBaseDim, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        hzp_gen_ubo_ = dev_->CreateGpuBuffer(d, params).raw();
    }
    if (hzp_down_ubos_.empty()) {
        for (uint32_t k = 1; k < kHzpMips; ++k) {
            const uint32_t src_dim = kHzpBaseDim >> (k - 1);
            const uint32_t dst_dim = kHzpBaseDim >> k;
            const uint32_t params[4] = {src_dim, dst_dim, 0u, 0u};
            GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
            hzp_down_ubos_.push_back(dev_->CreateGpuBuffer(d, params).raw());
        }
    }
    // 单张 R32Float mip 链纹理：生成趟写 mip0 storage + 逐级下采样写 mip[k] storage + 读 mip[k-1] 采样
    // + copy 各级 mip 源。usage 覆盖 storage 写 / 采样读 / copy 源。
    if (!hzp_tex_) {
        hzp_tex_ = dev_->CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kHzpBaseDim, kHzpBaseDim, 1,
            WGPUTextureFormat_R32Float,
            WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc,
            kHzpMips, 1, {nullptr}, TextureSamplerDesc::FromLinearFlag(false));
    }
    const WebGPURhiDevice::TextureEntry* pte = dev_->FindTexture(hzp_tex_);
    if (!hzp_gen_shader_ || !hzp_down_shader_ || !hzp_gen_ubo_ ||
        hzp_down_ubos_.size() != kHzpMips - 1 || !hzp_tex_ || !pte || !pte->texture) {
        DEBUG_LOG_ERROR("WebGPU[B3b-6] Hi-Z 金字塔自检资源创建失败，跳过");
        return false;
    }

    // B3b-7：改走引擎 Hi-Z build 真实绑定面 SetComputeTextureImageMip（句柄 + mip 级 + read_only +
    // r32f），其内部对 (句柄,mip) 缓存单层单 mip 视图并经 SetComputeImageViewExplicit 路由，
    // 故此自检同时验证「句柄→单 mip 视图」整条通路（不再手建 hzp_mip_views_）。

    // ①生成趟：写 mip0（8×8）渐变。
    dev_->ResetDrawState();
    dev_->CmdBindUniformBuffer(0, hzp_gen_ubo_, 0, sizeof(uint32_t) * 4);
    dev_->SetComputeTextureImageMip(0, hzp_tex_, 0, /*read_only=*/false, /*r32f=*/true);
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(hzp_gen_shader_, (kHzpBaseDim + 7u) / 8u, (kHzpBaseDim + 7u) / 8u, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // ②逐级下采样趟：读 mip[k-1] 采样视图 + 2×2 max 写 mip[k] storage 视图（独立 pass，pass 间屏障）。
    for (uint32_t k = 1; k < kHzpMips; ++k) {
        const uint32_t dst_dim = kHzpBaseDim >> k;
        dev_->CmdBindUniformBuffer(0, hzp_down_ubos_[k - 1], 0, sizeof(uint32_t) * 4);
        dev_->SetComputeTextureImageMip(0, hzp_tex_, static_cast<int>(k - 1), /*read_only=*/true,  /*r32f=*/true);
        dev_->SetComputeTextureImageMip(1, hzp_tex_, static_cast<int>(k),     /*read_only=*/false, /*r32f=*/true);
        dev_->BeginComputePass();
        if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
        dev_->DispatchCompute(hzp_down_shader_, (dst_dim + 7u) / 8u, (dst_dim + 7u) / 8u, 1);
        dev_->EndComputePass();
        dev_->ResetDrawState();
    }

    // copy 各级 mip → 回读缓冲（256 对齐分段；随帧提交）。
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kHzpTotalBytes;
    hzp_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!hzp_rb_pixels_) return false;
    for (uint32_t k = 0; k < kHzpMips; ++k) {
        const uint32_t dim = kHzpBaseDim >> k;
        WGPUImageCopyTexture src{};
        src.texture = pte->texture;
        src.mipLevel = k;
        src.aspect = WGPUTextureAspect_All;
        WGPUImageCopyBuffer dst{};
        dst.buffer = hzp_rb_pixels_;
        dst.layout.offset = kHzpMipOffset(k);
        dst.layout.bytesPerRow = kHzpRowBytes;
        dst.layout.rowsPerImage = dim;
        WGPUExtent3D ext{dim, dim, 1};
        wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    }
    return true;
}

void WebGpuSelfTestHarness::KickHiZPyramidSelfTestReadback() {
    if (!hzp_rb_pixels_) return;
    // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    auto* ctx = new HiZPyramidSelfTestCtx();
    ctx->rb_pixels = hzp_rb_pixels_;
    hzp_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kHzpTotalBytes, OnHiZPyramidPixelsMapped, ctx);
}

// B3b-8：命名 uniform + compute 采样器绑定 真链路自检。手写 WGSL compute 经 SetComputeUniform*
// （命名 i32/f32/vec2i/vec4/mat4 → group1 保留 binding 命名块，各成员 @align(16)）读入参数、经
// SetComputeTextureSampler（句柄 → group2 b0，textureLoad）采样已知渐变纹理，结果写 group3 SSBO，
// 再 copy SSBO→回读缓冲逐元素校验。覆盖引擎 Hi-Z/GPU-driven 剔除真实 compute API 面（命名 uniform
// 块布局 + 句柄采样绑定）。详见 ComputeBindSelfTestCtx / OnComputeBindMapped。
bool WebGpuSelfTestHarness::RecordComputeBindSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // 命名 uniform 块：成员声明序 + @align(16) 须与 SetComputeUniform* 调用序的 16B 对齐定位一致
    // （a_int@0 / b_float@16 / c_coord@32 / d_vec4@48 / e_mat@64，块 128B）。
    static const char* kBindSelfTestWGSL = R"WGSL(// dse-wgsl
struct NamedBlock {
  @align(16) a_int   : i32,
  @align(16) b_float : f32,
  @align(16) c_coord : vec2<i32>,
  @align(16) d_vec4  : vec4<f32>,
  @align(16) e_mat   : mat4x4<f32>,
};
@group(1) @binding(8) var<uniform> u : NamedBlock;
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(3) @binding(0) var<storage, read_write> outbuf : array<u32>;
@compute @workgroup_size(1)
fn cs_main() {
  outbuf[0] = u32(u.a_int);
  outbuf[1] = bitcast<u32>(u.b_float);
  let texel = textureLoad(src_tex, u.c_coord, 0);
  outbuf[2] = u32(round(texel.r * 255.0));
  outbuf[3] = u32(round(texel.g * 255.0));
  outbuf[4] = u32(round(texel.b * 255.0));
  let v = u.e_mat * u.d_vec4;
  outbuf[5] = bitcast<u32>(v.x);
  outbuf[6] = bitcast<u32>(v.y);
  outbuf[7] = bitcast<u32>(v.z);
  outbuf[8] = bitcast<u32>(v.w);
}
)WGSL";

    if (!cb_shader_) cb_shader_ = dev_->CreateComputeShaderEx("", "", "", 1, 0, 1, 0, kBindSelfTestWGSL);
    if (!cb_tex_) {
        // 已知渐变 rgba8unorm：texel(x,y)=(x*40, y*60, (x+y)*20, 255)。
        std::vector<uint8_t> texdata(kCbTexDim * kCbTexDim * 4u);
        for (uint32_t y = 0; y < kCbTexDim; ++y) {
            for (uint32_t x = 0; x < kCbTexDim; ++x) {
                uint8_t* p = &texdata[(y * kCbTexDim + x) * 4u];
                p[0] = CbTexR(x); p[1] = CbTexG(y); p[2] = CbTexB(x, y); p[3] = 255;
            }
        }
        cb_tex_ = dev_->CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kCbTexDim, kCbTexDim, 1,
            WGPUTextureFormat_RGBA8Unorm,
            WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            1, 1, {texdata.data()}, TextureSamplerDesc::FromLinearFlag(false));
    }
    if (!cb_out_) {
        GpuBufferDesc d; d.size = kCbOutBytes; d.usage = GpuBufferUsage::kStorage;
        cb_out_ = dev_->CreateGpuBuffer(d, nullptr).raw();
    }
    const WebGPURhiDevice::TextureEntry* cte = dev_->FindTexture(cb_tex_);
    if (!cb_shader_ || !cb_tex_ || !cte || !cte->texture || !cb_out_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-8] 命名 uniform/采样自检资源创建失败，跳过");
        return false;
    }

    // 经引擎真实 compute API 面绑定：命名 uniform（调用序定位）+ 句柄采样器 + 结果 SSBO。
    const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    dev_->ResetDrawState();
    dev_->SetComputeUniformInt(cb_shader_, "a_int", kCbAInt);
    dev_->SetComputeUniformFloat(cb_shader_, "b_float", kCbBFloat);
    dev_->SetComputeUniformVec2i(cb_shader_, "c_coord", kCbCX, kCbCY);
    dev_->SetComputeUniformVec4(cb_shader_, "d_vec4", 1.0f, 2.0f, 3.0f, 4.0f);
    dev_->SetComputeUniformMat4(cb_shader_, "e_mat", kIdentity);
    dev_->SetComputeTextureSampler(0, cb_tex_);
    dev_->CmdBindStorageBuffer(0, cb_out_, 0, kCbOutBytes);

    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(cb_shader_, 1, 1, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kCbOutBytes;
    cb_rb_out_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    const WebGPURhiDevice::BufferEntry* be_out = dev_->FindBuffer(cb_out_);
    if (!cb_rb_out_ || !be_out || !be_out->buffer) {
        if (cb_rb_out_) { wgpuBufferRelease(cb_rb_out_); cb_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_out->buffer, 0, cb_rb_out_, 0, kCbOutBytes);
    return true;
}

void WebGpuSelfTestHarness::KickComputeBindSelfTestReadback() {
    if (!cb_rb_out_) return;
    auto* ctx = new ComputeBindSelfTestCtx();
    ctx->rb_out = cb_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    cb_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kCbOutBytes, OnComputeBindMapped, ctx);
}

// B3b-9：Hi-Z 遮挡剔除真链路自检。手译引擎 HiZCullPass compute（builtin_passes.cpp
// kHiZCullShaderSourceVK）为 WGSL：AABB SSBO（group3 b0）→ 8 角经命名 uniform u_view_projection
// 投影 → NDC/UV → off-screen 拒绝 → 由屏幕像素跨度选 mip → 5 tab Hi-Z 采样取 max 与 AABB 最近深度
// 比较 → 写可见性 SSBO（group3 b1）。Hi-Z 纹理经 SetComputeTextureSampler（group2 b0）句柄绑定，
// textureLod 改为 textureLoad-at-mip（点采样 max 金字塔对遮挡判定保守正确；多级采样已由 B3b-6/7 证）。
// 经引擎真实 compute API 面（命名 uniform + 句柄采样 + 双 SSBO）跑通，证明该消费方着色器 WebGPU 可用。
bool WebGpuSelfTestHarness::RecordHiZCullSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    static const char* kHiZCullWGSL = R"WGSL(// dse-wgsl
struct AABB { min_point : vec4<f32>, max_point : vec4<f32>, };
@group(3) @binding(0) var<storage, read_write> aabbs : array<AABB>;
@group(3) @binding(1) var<storage, read_write> visibility : array<u32>;
@group(2) @binding(0) var u_hiz_texture : texture_2d<f32>;
struct PC {
  @align(16) u_view_projection : mat4x4<f32>,
  @align(16) u_screen_size     : vec2<f32>,
  @align(16) u_mip_count       : i32,
  @align(16) u_object_count    : i32,
};
@group(1) @binding(8) var<uniform> pc : PC;

fn sample_hiz(uv : vec2<f32>, mip : i32) -> f32 {
  // 基础（mip0）尺寸用无 level 形式取（textureDimensions 的运行期 level 形式在部分
  // D3D12/Tint 组合下会触发设备移除）；按 mip 右移得该级尺寸，coord 钳制后点采样。
  let base = vec2<f32>(textureDimensions(u_hiz_texture));
  let dim = max(base / f32(1 << u32(mip)), vec2<f32>(1.0));
  let coord = vec2<i32>(clamp(uv * dim, vec2<f32>(0.0), dim - vec2<f32>(1.0)));
  return textureLoad(u_hiz_texture, coord, mip).r;
}

@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let idx = gid.x;
  if (i32(idx) >= pc.u_object_count) { return; }
  let aabb_min = aabbs[idx].min_point.xyz;
  let aabb_max = aabbs[idx].max_point.xyz;
  var ndc_min = vec2<f32>(1.0);
  var ndc_max = vec2<f32>(-1.0);
  var nearest_z = 1.0;
  for (var i = 0; i < 8; i = i + 1) {
    let corner = vec3<f32>(
      select(aabb_min.x, aabb_max.x, (i & 1) != 0),
      select(aabb_min.y, aabb_max.y, (i & 2) != 0),
      select(aabb_min.z, aabb_max.z, (i & 4) != 0));
    let clip = pc.u_view_projection * vec4<f32>(corner, 1.0);
    if (clip.w <= 0.0) { visibility[idx] = 1u; return; }
    let ndc = clip.xyz / clip.w;
    ndc_min = min(ndc_min, ndc.xy);
    ndc_max = max(ndc_max, ndc.xy);
    nearest_z = min(nearest_z, ndc.z);
  }
  var uv_min = ndc_min * 0.5 + 0.5;
  var uv_max = ndc_max * 0.5 + 0.5;
  uv_min = clamp(uv_min, vec2<f32>(0.0), vec2<f32>(1.0));
  uv_max = clamp(uv_max, vec2<f32>(0.0), vec2<f32>(1.0));
  if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
    visibility[idx] = 0u; return;
  }
  let size_pixels = (uv_max - uv_min) * pc.u_screen_size;
  let max_dim = max(size_pixels.x, size_pixels.y);
  var mip_level = select(0.0, ceil(log2(max(max_dim, 1.0))), max_dim > 1.0);
  mip_level = clamp(mip_level, 0.0, f32(pc.u_mip_count - 1));
  let mip = i32(mip_level);
  let test_depth = nearest_z * 0.5 + 0.5 - 0.005;
  let uv_center = (uv_min + uv_max) * 0.5;
  let h0 = sample_hiz(uv_center, mip);
  let h1 = sample_hiz(uv_min, mip);
  let h2 = sample_hiz(uv_max, mip);
  let h3 = sample_hiz(vec2<f32>(uv_max.x, uv_min.y), mip);
  let h4 = sample_hiz(vec2<f32>(uv_min.x, uv_max.y), mip);
  let max_hiz = max(max(h0, h1), max(max(h2, h3), h4));
  visibility[idx] = select(1u, 0u, test_depth > max_hiz);
}
)WGSL";

    if (!hc_shader_) hc_shader_ = dev_->CreateComputeShaderEx("", "", "", 2, 0, 1, 80, kHiZCullWGSL);
    if (!hc_aabb_) {
        // 4 个 AABB（min.xyz/max.xyz，各 vec4 占位 w=0）：单位 VP 下 NDC==世界坐标。
        //  ①前景可见 z[-0.8,-0.6]；②背后遮挡 z[0.6,0.8]；③出屏 xy[1.5,2.0]；④中景可见 z[-0.2,0]。
        const float aabbs[kHcObjCount * 8] = {
            -0.5f, -0.5f, -0.8f, 0.0f,   0.5f, 0.5f, -0.6f, 0.0f,
            -0.5f, -0.5f,  0.6f, 0.0f,   0.5f, 0.5f,  0.8f, 0.0f,
             1.5f,  1.5f, -0.5f, 0.0f,   2.0f, 2.0f, -0.4f, 0.0f,
            -0.2f, -0.2f, -0.2f, 0.0f,   0.0f, 0.0f,  0.0f, 0.0f,
        };
        GpuBufferDesc d; d.size = sizeof(aabbs); d.usage = GpuBufferUsage::kStorage;
        hc_aabb_ = dev_->CreateGpuBuffer(d, aabbs).raw();
    }
    if (!hc_vis_) {
        GpuBufferDesc d; d.size = kHcVisBytes; d.usage = GpuBufferUsage::kStorage;
        hc_vis_ = dev_->CreateGpuBuffer(d, nullptr).raw();
    }
    if (!hc_hiz_tex_) {
        hc_hiz_tex_ = dev_->CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kHcHizDim, kHcHizDim, 1,
            WGPUTextureFormat_R32Float,
            WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            1, 1, {nullptr}, TextureSamplerDesc::FromLinearFlag(false));
        const WebGPURhiDevice::TextureEntry* hte = dev_->FindTexture(hc_hiz_tex_);
        if (hte && hte->texture) {
            // 恒值 0.5 填充（queue write 无 256 行对齐约束）。
            std::vector<float> hiz(kHcHizDim * kHcHizDim, kHcHizDepth);
            WGPUImageCopyTexture dst{};
            dst.texture = hte->texture; dst.mipLevel = 0; dst.aspect = WGPUTextureAspect_All;
            WGPUTextureDataLayout layout{};
            layout.offset = 0; layout.bytesPerRow = kHcHizDim * 4u; layout.rowsPerImage = kHcHizDim;
            WGPUExtent3D ext{kHcHizDim, kHcHizDim, 1};
            wgpuQueueWriteTexture(dev_->queue_, &dst, hiz.data(), hiz.size() * 4u, &layout, &ext);
        }
    }
    const WebGPURhiDevice::TextureEntry* hte = dev_->FindTexture(hc_hiz_tex_);
    if (!hc_shader_ || !hc_aabb_ || !hc_vis_ || !hc_hiz_tex_ || !hte || !hte->texture) {
        DEBUG_LOG_ERROR("WebGPU[B3b-9] Hi-Z 剔除自检资源创建失败，跳过");
        return false;
    }

    // 经引擎 HiZCullPass 真实绑定面：双 SSBO（slot0 AABB / slot1 可见性）+ 句柄采样 Hi-Z + 命名 uniform。
    const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    dev_->ResetDrawState();
    dev_->CmdBindStorageBuffer(0, hc_aabb_, 0, kHcObjCount * 8u * 4u);
    dev_->CmdBindStorageBuffer(1, hc_vis_, 0, kHcVisBytes);
    dev_->SetComputeTextureSampler(0, hc_hiz_tex_);
    dev_->SetComputeUniformMat4(hc_shader_, "u_view_projection", kIdentity);
    dev_->SetComputeUniformVec2f(hc_shader_, "u_screen_size", 256.0f, 256.0f);
    dev_->SetComputeUniformInt(hc_shader_, "u_mip_count", 1);
    dev_->SetComputeUniformInt(hc_shader_, "u_object_count", static_cast<int>(kHcObjCount));

    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(hc_shader_, (kHcObjCount + 63u) / 64u, 1, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kHcVisBytes;
    hc_rb_out_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    const WebGPURhiDevice::BufferEntry* be_vis = dev_->FindBuffer(hc_vis_);
    if (!hc_rb_out_ || !be_vis || !be_vis->buffer) {
        if (hc_rb_out_) { wgpuBufferRelease(hc_rb_out_); hc_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_vis->buffer, 0, hc_rb_out_, 0, kHcVisBytes);
    return true;
}

void WebGpuSelfTestHarness::KickHiZCullSelfTestReadback() {
    if (!hc_rb_out_) return;
    auto* ctx = new HiZCullSelfTestCtx();
    ctx->rb_out = hc_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    hc_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kHcVisBytes, OnHiZCullMapped, ctx);
}

// Task 4 Subtask 4：GPU-driven Hi-Z 遮挡剔除真资源链路自检。区别于 B3b-9（手建单 mip 恒值 Hi-Z 纹理
// 仅验剔除 compute 逻辑），本自检走 CreateHiZTexture 真资源（R32Float 完整 mip 链）+ 引擎 HiZBuildPass
// 真实绑定面（SetComputeTextureImageMip 写 mip0 占位深度 + 逐级 2×2 max 下采样建金字塔）+ HiZCullPass
// 经 GetHiZGpuTexture/GetHiZMipCount 采样金字塔判遮挡，端到端串起资源 API 与既有原语。详见
// GpuDrivenHiZCullSelfTestCtx / OnT44HiZCullMapped。离屏隔离、不翻 SupportsIndirectDraw()、不碰 demo 帧。
bool WebGpuSelfTestHarness::RecordGpuDrivenHiZCullSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // 生成趟：写 mip0 占位遮挡深度——texel(0,0)=0.9（最近遮挡）、其余 0.1。group2 b0 storage 写。
    static const char* kT44GenWGSL = R"WGSL(// dse-wgsl
struct Params { dim : u32, pad0 : u32, pad1 : u32, pad2 : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(2) @binding(0) var dst_img : texture_storage_2d<r32float, write>;
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (gid.x >= params.dim || gid.y >= params.dim) { return; }
  let occ = (gid.x == 0u) && (gid.y == 0u);
  let v = select(0.1, 0.9, occ);
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(v, 0.0, 0.0, 0.0));
}
)WGSL";
    // 下采样趟：读 mip[k-1] 采样视图 + 2×2 max 写 mip[k] storage 视图（与 B3b-6 同形）。
    static const char* kT44DownWGSL = R"WGSL(// dse-wgsl
struct Params { src_dim : u32, dst_dim : u32, pad0 : u32, pad1 : u32, };
@group(1) @binding(0) var<uniform> params : Params;
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var dst_img : texture_storage_2d<r32float, write>;
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (gid.x >= params.dst_dim || gid.y >= params.dst_dim) { return; }
  let m = i32(params.src_dim) - 1;
  let sx = i32(gid.x * 2u);
  let sy = i32(gid.y * 2u);
  let d00 = textureLoad(src_tex, vec2<i32>(sx, sy), 0).r;
  let d10 = textureLoad(src_tex, vec2<i32>(min(sx + 1, m), sy), 0).r;
  let d01 = textureLoad(src_tex, vec2<i32>(sx, min(sy + 1, m)), 0).r;
  let d11 = textureLoad(src_tex, vec2<i32>(min(sx + 1, m), min(sy + 1, m)), 0).r;
  let mx = max(max(d00, d10), max(d01, d11));
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(mx, 0.0, 0.0, 0.0));
}
)WGSL";
    // 剔除趟：HiZCullPass 手译 WGSL（与 B3b-9 同源），采样金字塔（textureLoad-at-mip）判遮挡。
    static const char* kT44CullWGSL = R"WGSL(// dse-wgsl
struct AABB { min_point : vec4<f32>, max_point : vec4<f32>, };
@group(3) @binding(0) var<storage, read_write> aabbs : array<AABB>;
@group(3) @binding(1) var<storage, read_write> visibility : array<u32>;
@group(2) @binding(0) var u_hiz_texture : texture_2d<f32>;
struct PC {
  @align(16) u_view_projection : mat4x4<f32>,
  @align(16) u_screen_size     : vec2<f32>,
  @align(16) u_mip_count       : i32,
  @align(16) u_object_count    : i32,
};
@group(1) @binding(8) var<uniform> pc : PC;

fn sample_hiz(uv : vec2<f32>, mip : i32) -> f32 {
  let base = vec2<f32>(textureDimensions(u_hiz_texture));
  let dim = max(base / f32(1 << u32(mip)), vec2<f32>(1.0));
  let coord = vec2<i32>(clamp(uv * dim, vec2<f32>(0.0), dim - vec2<f32>(1.0)));
  return textureLoad(u_hiz_texture, coord, mip).r;
}

@compute @workgroup_size(64)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let idx = gid.x;
  if (i32(idx) >= pc.u_object_count) { return; }
  let aabb_min = aabbs[idx].min_point.xyz;
  let aabb_max = aabbs[idx].max_point.xyz;
  var ndc_min = vec2<f32>(1.0);
  var ndc_max = vec2<f32>(-1.0);
  var nearest_z = 1.0;
  for (var i = 0; i < 8; i = i + 1) {
    let corner = vec3<f32>(
      select(aabb_min.x, aabb_max.x, (i & 1) != 0),
      select(aabb_min.y, aabb_max.y, (i & 2) != 0),
      select(aabb_min.z, aabb_max.z, (i & 4) != 0));
    let clip = pc.u_view_projection * vec4<f32>(corner, 1.0);
    if (clip.w <= 0.0) { visibility[idx] = 1u; return; }
    let ndc = clip.xyz / clip.w;
    ndc_min = min(ndc_min, ndc.xy);
    ndc_max = max(ndc_max, ndc.xy);
    nearest_z = min(nearest_z, ndc.z);
  }
  var uv_min = ndc_min * 0.5 + 0.5;
  var uv_max = ndc_max * 0.5 + 0.5;
  uv_min = clamp(uv_min, vec2<f32>(0.0), vec2<f32>(1.0));
  uv_max = clamp(uv_max, vec2<f32>(0.0), vec2<f32>(1.0));
  if (uv_max.x <= 0.0 || uv_min.x >= 1.0 || uv_max.y <= 0.0 || uv_min.y >= 1.0) {
    visibility[idx] = 0u; return;
  }
  let size_pixels = (uv_max - uv_min) * pc.u_screen_size;
  let max_dim = max(size_pixels.x, size_pixels.y);
  var mip_level = select(0.0, ceil(log2(max(max_dim, 1.0))), max_dim > 1.0);
  mip_level = clamp(mip_level, 0.0, f32(pc.u_mip_count - 1));
  let mip = i32(mip_level);
  let test_depth = nearest_z * 0.5 + 0.5 - 0.005;
  let uv_center = (uv_min + uv_max) * 0.5;
  let h0 = sample_hiz(uv_center, mip);
  let h1 = sample_hiz(uv_min, mip);
  let h2 = sample_hiz(uv_max, mip);
  let h3 = sample_hiz(vec2<f32>(uv_max.x, uv_min.y), mip);
  let h4 = sample_hiz(vec2<f32>(uv_min.x, uv_max.y), mip);
  let max_hiz = max(max(h0, h1), max(max(h2, h3), h4));
  visibility[idx] = select(1u, 0u, test_depth > max_hiz);
}
)WGSL";

    if (!t44_gen_shader_)  t44_gen_shader_  = dev_->CreateComputeShaderEx("", "", "", 0, 1, 0, 0,  kT44GenWGSL);
    if (!t44_down_shader_) t44_down_shader_ = dev_->CreateComputeShaderEx("", "", "", 0, 1, 1, 0,  kT44DownWGSL);
    if (!t44_cull_shader_) t44_cull_shader_ = dev_->CreateComputeShaderEx("", "", "", 2, 0, 1, 80, kT44CullWGSL);
    if (!t44_gen_ubo_.raw()) {
        const uint32_t params[4] = {kT44HizDim, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        t44_gen_ubo_ = dev_->CreateGpuBuffer(d, params);
    }
    if (t44_down_ubos_.empty()) {
        for (uint32_t k = 1; k < kT44Mips; ++k) {
            const uint32_t src_dim = kT44HizDim >> (k - 1);
            const uint32_t dst_dim = kT44HizDim >> k;
            const uint32_t params[4] = {src_dim, dst_dim, 0u, 0u};
            GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
            t44_down_ubos_.push_back(dev_->CreateGpuBuffer(d, params).raw());
        }
    }
    if (!t44_aabb_.raw()) {
        // 3 个 AABB（min.xyzw/max.xyzw，w 占位 0）：单位 VP 下 NDC==世界坐标。
        //  ①近物可见 z[-0.05,0]；②远物被金字塔遮挡 z[0.85,0.9]；③出屏 xy[1.5,2.0]。
        const float aabbs[kT44ObjCount * 8] = {
            -1.0f, -1.0f, -0.05f, 0.0f,   1.0f, 1.0f, 0.00f, 0.0f,
            -1.0f, -1.0f,  0.85f, 0.0f,   1.0f, 1.0f, 0.90f, 0.0f,
             1.5f,  1.5f, -0.50f, 0.0f,   2.0f, 2.0f, -0.40f, 0.0f,
        };
        GpuBufferDesc d; d.size = sizeof(aabbs); d.usage = GpuBufferUsage::kStorage;
        t44_aabb_ = dev_->CreateGpuBuffer(d, aabbs);
    }
    if (!t44_vis_.raw()) {
        GpuBufferDesc d; d.size = kT44VisBytes; d.usage = GpuBufferUsage::kStorage;
        t44_vis_ = dev_->CreateGpuBuffer(d, nullptr);
    }
    if (!t44_hiz_handle_) t44_hiz_handle_ = dev_->CreateHiZTexture(kT44HizDim, kT44HizDim);

    const unsigned int hiz_tex = dev_->GetHiZGpuTexture(t44_hiz_handle_);
    const int mip_count = dev_->GetHiZMipCount(t44_hiz_handle_);
    const WebGPURhiDevice::TextureEntry* pte = dev_->FindTexture(hiz_tex);
    if (!t44_gen_shader_ || !t44_down_shader_ || !t44_cull_shader_ || !t44_gen_ubo_.raw() ||
        t44_down_ubos_.size() != kT44Mips - 1 || !t44_aabb_.raw() || !t44_vis_.raw() ||
        !t44_hiz_handle_ || !hiz_tex || !pte || !pte->texture ||
        mip_count != static_cast<int>(kT44Mips)) {
        DEBUG_LOG_ERROR("WebGPU[T4-4] Hi-Z 剔除自检资源创建失败，跳过（mip_count={}）", mip_count);
        return false;
    }

    // ①生成趟：写 mip0（8×8）占位遮挡深度。经引擎 HiZBuildPass 真实绑定面 SetComputeTextureImageMip。
    dev_->ResetDrawState();
    dev_->CmdBindUniformBuffer(0, t44_gen_ubo_.raw(), 0, sizeof(uint32_t) * 4);
    dev_->SetComputeTextureImageMip(0, hiz_tex, 0, /*read_only=*/false, /*r32f=*/true);
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(t44_gen_shader_, (kT44HizDim + 7u) / 8u, (kT44HizDim + 7u) / 8u, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // ②逐级下采样趟：读 mip[k-1] + 2×2 max 写 mip[k]（独立 pass，pass 间屏障保证可见性）。
    for (uint32_t k = 1; k < kT44Mips; ++k) {
        const uint32_t dst_dim = kT44HizDim >> k;
        dev_->CmdBindUniformBuffer(0, t44_down_ubos_[k - 1], 0, sizeof(uint32_t) * 4);
        dev_->SetComputeTextureImageMip(0, hiz_tex, static_cast<int>(k - 1), /*read_only=*/true,  /*r32f=*/true);
        dev_->SetComputeTextureImageMip(1, hiz_tex, static_cast<int>(k),     /*read_only=*/false, /*r32f=*/true);
        dev_->BeginComputePass();
        if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
        dev_->DispatchCompute(t44_down_shader_, (dst_dim + 7u) / 8u, (dst_dim + 7u) / 8u, 1);
        dev_->EndComputePass();
        dev_->ResetDrawState();
    }

    // ③剔除趟：经 HiZCullPass 真实绑定面——双 SSBO + 句柄采样金字塔（GetHiZGpuTexture）+ 命名 uniform。
    const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    dev_->ResetDrawState();
    dev_->CmdBindStorageBuffer(0, t44_aabb_.raw(), 0, kT44ObjCount * 8u * 4u);
    dev_->CmdBindStorageBuffer(1, t44_vis_.raw(), 0, kT44VisBytes);
    dev_->SetComputeTextureSampler(0, hiz_tex);
    dev_->SetComputeUniformMat4(t44_cull_shader_, "u_view_projection", kIdentity);
    dev_->SetComputeUniformVec2f(t44_cull_shader_, "u_screen_size", static_cast<float>(kT44HizDim),
                           static_cast<float>(kT44HizDim));
    dev_->SetComputeUniformInt(t44_cull_shader_, "u_mip_count", mip_count);
    dev_->SetComputeUniformInt(t44_cull_shader_, "u_object_count", static_cast<int>(kT44ObjCount));

    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(t44_cull_shader_, (kT44ObjCount + 63u) / 64u, 1, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kT44VisBytes;
    t44_rb_out_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    const WebGPURhiDevice::BufferEntry* be_vis = dev_->FindBuffer(t44_vis_.raw());
    if (!t44_rb_out_ || !be_vis || !be_vis->buffer) {
        if (t44_rb_out_) { wgpuBufferRelease(t44_rb_out_); t44_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_vis->buffer, 0, t44_rb_out_, 0, kT44VisBytes);
    return true;
}

void WebGpuSelfTestHarness::KickGpuDrivenHiZCullSelfTestReadback() {
    if (!t44_rb_out_) return;
    auto* ctx = new GpuDrivenHiZCullSelfTestCtx();
    ctx->rb_out = t44_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t44_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kT44VisBytes, OnT44HiZCullMapped, ctx);
}

// B3b-10：形变目标真链路自检。手译引擎 MorphTargetSystem compute（morph_target_system.cpp
// kMorphTargetCompWGSL，与上方 GLSL 450 逐句对应）：base 顶点 + Σ weight·delta（按目标）→
// normalize 法线 → 写形变顶点。自检布置：4 顶点、2 目标、weights=[0.5,1.0]、delta 目标0 全 (1,0,0)、
// 目标1 全 (0,2,0)（法线 delta 为 0，输出法线 == normalize(base.nrm)=(0,0,1)）。经引擎真实 compute API 面
//（命名 uniform 顶点/目标数 + 4×SSBO）跑通，dispatch 后回读形变顶点逐顶点校验 == CPU 预期。
bool WebGpuSelfTestHarness::RecordMorphSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // 与 morph_target_system.cpp::kMorphTargetCompWGSL 一致（离屏自检内联副本）。
    static const char* kMorphWGSL = R"WGSL(// dse-wgsl
struct BaseVertex { position : vec4<f32>, normal : vec4<f32>, tangent : vec4<f32>, };
@group(3) @binding(0) var<storage, read_write> base_vertices : array<BaseVertex>;
struct MorphDelta { delta_pos : vec4<f32>, delta_normal : vec4<f32>, };
@group(3) @binding(1) var<storage, read_write> morph_deltas : array<MorphDelta>;
@group(3) @binding(2) var<storage, read_write> morph_weights : array<f32>;
struct DeformedVertex { position : vec4<f32>, normal : vec4<f32>, tangent : vec4<f32>, };
@group(3) @binding(3) var<storage, read_write> deformed_vertices : array<DeformedVertex>;
struct PC { @align(16) u_vertex_count : i32, @align(16) u_target_count : i32, };
@group(1) @binding(8) var<uniform> pc : PC;
@compute @workgroup_size(256)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let vid = gid.x;
  if (i32(vid) >= pc.u_vertex_count) { return; }
  var pos = base_vertices[vid].position.xyz;
  var nrm = base_vertices[vid].normal.xyz;
  let tan = base_vertices[vid].tangent;
  for (var t = 0; t < pc.u_target_count; t = t + 1) {
    let w = morph_weights[t];
    if (abs(w) < 0.0001) { continue; }
    let delta_idx = u32(t) * u32(pc.u_vertex_count) + vid;
    pos = pos + morph_deltas[delta_idx].delta_pos.xyz * w;
    nrm = nrm + morph_deltas[delta_idx].delta_normal.xyz * w;
  }
  nrm = normalize(nrm);
  deformed_vertices[vid].position = vec4<f32>(pos, 1.0);
  deformed_vertices[vid].normal = vec4<f32>(nrm, 0.0);
  deformed_vertices[vid].tangent = tan;
}
)WGSL";

    if (!mf_shader_) mf_shader_ = dev_->CreateComputeShaderEx("", "", "", 4, 0, 0, 8, kMorphWGSL);
    if (!mf_base_) {
        // 4 个 BaseVertex（pos.xyzw / normal.xyzw / tangent.xyzw）：法线统一 (0,0,1)，切线 (1,0,0,1)。
        const float base[kMfVtxCount * 12] = {
            1,0,0,1,  0,0,1,0,  1,0,0,1,
            0,1,0,1,  0,0,1,0,  1,0,0,1,
            0,0,1,1,  0,0,1,0,  1,0,0,1,
            1,1,1,1,  0,0,1,0,  1,0,0,1,
        };
        GpuBufferDesc d; d.size = sizeof(base); d.usage = GpuBufferUsage::kStorage;
        mf_base_ = dev_->CreateGpuBuffer(d, base).raw();
    }
    if (!mf_delta_) {
        // delta 排布 [target][vertex]：目标0 各顶点 Δpos=(1,0,0)、目标1 各顶点 Δpos=(0,2,0)，Δnrm=0。
        const float deltas[kMfTgtCount * kMfVtxCount * 8] = {
            1,0,0,0, 0,0,0,0,  1,0,0,0, 0,0,0,0,  1,0,0,0, 0,0,0,0,  1,0,0,0, 0,0,0,0,
            0,2,0,0, 0,0,0,0,  0,2,0,0, 0,0,0,0,  0,2,0,0, 0,0,0,0,  0,2,0,0, 0,0,0,0,
        };
        GpuBufferDesc d; d.size = sizeof(deltas); d.usage = GpuBufferUsage::kStorage;
        mf_delta_ = dev_->CreateGpuBuffer(d, deltas).raw();
    }
    if (!mf_weight_) {
        const float weights[kMfTgtCount] = {0.5f, 1.0f};
        GpuBufferDesc d; d.size = sizeof(weights); d.usage = GpuBufferUsage::kStorage;
        mf_weight_ = dev_->CreateGpuBuffer(d, weights).raw();
    }
    if (!mf_out_) {
        GpuBufferDesc d; d.size = kMfOutBytes; d.usage = GpuBufferUsage::kStorage;
        mf_out_ = dev_->CreateGpuBuffer(d, nullptr).raw();
    }
    if (!mf_shader_ || !mf_base_ || !mf_delta_ || !mf_weight_ || !mf_out_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-10] morph 自检资源创建失败，跳过");
        return false;
    }

    // 经引擎 MorphTargetSystem 真实绑定面：4×SSBO（slot0..3）+ 命名 uniform（同消费方调用序/名）。
    // 用 BindGpuBuffer（消费方 Dispatch 实际所调 API）而非 CmdBindStorageBuffer，验证真消费方绑定通路。
    dev_->ResetDrawState();
    dev_->BindGpuBuffer(BufferHandle{mf_base_}, 0);
    dev_->BindGpuBuffer(BufferHandle{mf_delta_}, 1);
    dev_->BindGpuBuffer(BufferHandle{mf_weight_}, 2);
    dev_->BindGpuBuffer(BufferHandle{mf_out_}, 3, true);
    dev_->SetComputeUniformInt(mf_shader_, "_20.u_vertex_count", static_cast<int>(kMfVtxCount));
    dev_->SetComputeUniformInt(mf_shader_, "_20.u_target_count", static_cast<int>(kMfTgtCount));

    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(mf_shader_, (kMfVtxCount + 255u) / 256u, 1, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kMfOutBytes;
    mf_rb_out_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    const WebGPURhiDevice::BufferEntry* be_out = dev_->FindBuffer(mf_out_);
    if (!mf_rb_out_ || !be_out || !be_out->buffer) {
        if (mf_rb_out_) { wgpuBufferRelease(mf_rb_out_); mf_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_out->buffer, 0, mf_rb_out_, 0, kMfOutBytes);
    return true;
}

void WebGpuSelfTestHarness::KickMorphSelfTestReadback() {
    if (!mf_rb_out_) return;
    auto* ctx = new MorphSelfTestCtx();
    ctx->rb_out = mf_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    mf_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kMfOutBytes, OnMorphMapped, ctx);
}

// B3b-11：DDGI 探针更新核心真链路自检。手译引擎 DDGISystem probe-update compute（ddgi_system.cpp
// kDDGIUpdateComputeSource）核心为 WGSL —— 与 GLSL 430 逐句对应：probe SSBO 读探针位 → 每 texel
// octahedral 解码方向 → 从 RSM（位置/法线/通量句柄采样，textureLoad 替 texture()，整数 texel 中心采样
// 等价点采样）随机采样 VPL 累积间接辐照度（cos·cos·平方衰减加权）→ 归一化×RSM 面积因子×0.01 → 写
// irradiance/visibility storage image。自检 hysteresis=0 绕开 temporal imageLoad（核心 WebGPU storage 仅写）。
// 除写 storage image 外另写 float SSBO（每 texel irr.rgb+总权重）精校验。
bool WebGpuSelfTestHarness::RecordDDGISelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    static const char* kDDGIWGSL = R"WGSL(// dse-wgsl
@group(3) @binding(0) var<storage, read_write> probe_states : array<vec4<f32>>;
@group(3) @binding(1) var<storage, read_write> debug_out : array<vec4<f32>>;
@group(2) @binding(0) var u_irradiance_atlas : texture_storage_2d<rgba8unorm, write>;
@group(2) @binding(1) var u_visibility_atlas : texture_storage_2d<rgba8unorm, write>;
@group(2) @binding(2) var u_rsm_position : texture_2d<f32>;
@group(2) @binding(3) var u_rsm_normal : texture_2d<f32>;
@group(2) @binding(4) var u_rsm_flux : texture_2d<f32>;
struct PC {
  @align(16) u_probe_count : i32,
  @align(16) u_probe_start : i32,
  @align(16) u_probes_to_update : i32,
  @align(16) u_irradiance_texels : i32,
  @align(16) u_visibility_texels : i32,
  @align(16) u_rsm_width : i32,
  @align(16) u_rsm_height : i32,
  @align(16) u_frame_index : i32,
  @align(16) u_hysteresis : f32,
  @align(16) u_grid_resolution : vec3<i32>,
  @align(16) u_grid_origin : vec3<f32>,
  @align(16) u_grid_spacing : vec3<f32>,
  @align(16) u_light_dir : vec3<f32>,
  @align(16) u_light_color : vec3<f32>,
};
@group(1) @binding(8) var<uniform> pc : PC;

fn oct_decode(uv : vec2<f32>) -> vec3<f32> {
  let f = uv * 2.0 - 1.0;
  var n = vec3<f32>(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
  if (n.z < 0.0) {
    let xy = (vec2<f32>(1.0) - abs(vec2<f32>(n.y, n.x))) * sign(vec2<f32>(n.x, n.y));
    n.x = xy.x; n.y = xy.y;
  }
  return normalize(n);
}

fn hash(seed_in : u32) -> f32 {
  var seed = seed_in;
  seed = seed * 747796405u + 2891336453u;
  seed = ((seed >> 16u) ^ seed) * 0x45d9f3bu;
  seed = ((seed >> 16u) ^ seed) * 0x45d9f3bu;
  seed = (seed >> 16u) ^ seed;
  return f32(seed) / 4294967295.0;
}

@compute @workgroup_size(8, 8, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let texel_x = i32(gid.x);
  let probe_local = i32(gid.y);
  if (texel_x >= pc.u_irradiance_texels) { return; }
  if (probe_local >= pc.u_probes_to_update) { return; }
  let probe_index = (pc.u_probe_start + probe_local) % pc.u_probe_count;
  let probe_data = probe_states[probe_index];
  let probe_pos = probe_data.xyz;
  if (probe_data.w < 0.5) { return; }

  let grid_x = probe_index % pc.u_grid_resolution.x;
  let grid_y = (probe_index / pc.u_grid_resolution.x) % pc.u_grid_resolution.y;
  let grid_z = probe_index / (pc.u_grid_resolution.x * pc.u_grid_resolution.y);
  let atlas_col = grid_x + grid_z * pc.u_grid_resolution.x;
  let atlas_row = grid_y;

  for (var texel_y = 0; texel_y < pc.u_irradiance_texels; texel_y = texel_y + 1) {
    let texel_uv = (vec2<f32>(f32(texel_x), f32(texel_y)) + 0.5) / f32(pc.u_irradiance_texels);
    let texel_dir = oct_decode(texel_uv);

    var irradiance = vec3<f32>(0.0);
    var total_weight = 0.0;
    var visibility_depth = 0.0;
    var visibility_depth2 = 0.0;

    let rng_seed = u32(probe_index * 1031 + pc.u_frame_index * 7919 + texel_x * 113 + texel_y * 37);
    let num_samples = min(64, pc.u_rsm_width * pc.u_rsm_height);

    for (var s = 0; s < num_samples; s = s + 1) {
      let r1 = hash(rng_seed + u32(s * 2));
      let r2 = hash(rng_seed + u32(s * 2 + 1));
      var rsm_coord = vec2<i32>(i32(r1 * f32(pc.u_rsm_width)), i32(r2 * f32(pc.u_rsm_height)));
      rsm_coord = clamp(rsm_coord, vec2<i32>(0), vec2<i32>(pc.u_rsm_width - 1, pc.u_rsm_height - 1));

      let vpl_pos = textureLoad(u_rsm_position, rsm_coord, 0).xyz;
      let vpl_normal = normalize(textureLoad(u_rsm_normal, rsm_coord, 0).xyz * 2.0 - 1.0);
      let vpl_flux = textureLoad(u_rsm_flux, rsm_coord, 0).rgb;

      if (dot(vpl_flux, vpl_flux) < 1e-6) { continue; }
      let to_probe = probe_pos - vpl_pos;
      let dist = length(to_probe);
      if (dist < 0.01) { continue; }
      let dir_to_probe = to_probe / dist;
      let vpl_cos = max(0.0, dot(vpl_normal, dir_to_probe));
      if (vpl_cos < 1e-4) { continue; }
      let receive_cos = max(0.0, dot(texel_dir, -dir_to_probe));
      if (receive_cos < 1e-4) { continue; }
      let attenuation = 1.0 / (dist * dist + 1.0);
      let weight = vpl_cos * receive_cos * attenuation;
      irradiance = irradiance + vpl_flux * weight;
      total_weight = total_weight + weight;
      visibility_depth = visibility_depth + dist * weight;
      visibility_depth2 = visibility_depth2 + dist * dist * weight;
    }

    if (total_weight > 1e-6) {
      irradiance = irradiance / total_weight;
      visibility_depth = visibility_depth / total_weight;
      visibility_depth2 = visibility_depth2 / total_weight;
    }
    let rsm_area_factor = f32(pc.u_rsm_width * pc.u_rsm_height) / f32(num_samples);
    irradiance = irradiance * (rsm_area_factor * 0.01);

    debug_out[texel_y * pc.u_irradiance_texels + texel_x] = vec4<f32>(irradiance, total_weight);

    let atlas_texel = vec2<i32>(atlas_col * pc.u_irradiance_texels + texel_x,
                                atlas_row * pc.u_irradiance_texels + texel_y);
    // hysteresis=0 → mix(irradiance, prev, 0) == irradiance（绕开 temporal imageLoad；核心 WebGPU storage 仅写）。
    textureStore(u_irradiance_atlas, atlas_texel, vec4<f32>(irradiance, 1.0));

    if (texel_x >= pc.u_visibility_texels || texel_y >= pc.u_visibility_texels) { continue; }
    let vis_texel = vec2<i32>(atlas_col * pc.u_visibility_texels + texel_x,
                              atlas_row * pc.u_visibility_texels + texel_y);
    let new_vis = vec2<f32>(visibility_depth, visibility_depth2);
    textureStore(u_visibility_atlas, vis_texel, vec4<f32>(new_vis, 0.0, 0.0));
  }
}
)WGSL";

    if (!dg_shader_) dg_shader_ = dev_->CreateComputeShaderEx("", "", "", 2, 2, 3, 224, kDDGIWGSL);
    if (!dg_probe_) {
        // 1 探针：位置 xy 对齐 RSM VPL（128/255），z=-2（探针在 VPL 背面 z 负向）、w=1 激活。
        const float vx = 128.0f / 255.0f;
        const float probe[4] = {vx, vx, -2.0f, 1.0f};
        GpuBufferDesc d; d.size = sizeof(probe); d.usage = GpuBufferUsage::kStorage;
        dg_probe_ = dev_->CreateGpuBuffer(d, probe).raw();
    }
    if (!dg_dbg_) {
        GpuBufferDesc d; d.size = kDgDbgBytes; d.usage = GpuBufferUsage::kStorage;
        dg_dbg_ = dev_->CreateGpuBuffer(d, nullptr).raw();
    }
    if (!dg_irr_tex_) dg_irr_tex_ = dev_->CreateComputeWriteTexture2D(static_cast<int>(kDgIrrTexels),
                                                                static_cast<int>(kDgIrrTexels));
    if (!dg_vis_tex_) dg_vis_tex_ = dev_->CreateComputeWriteTexture2D(static_cast<int>(kDgVisTexels),
                                                                static_cast<int>(kDgVisTexels));
    auto make_rsm = [&](uint8_t r, uint8_t g, uint8_t b) -> unsigned int {
        std::vector<uint8_t> px(kDgRsmDim * kDgRsmDim * 4u);
        for (uint32_t i = 0; i < kDgRsmDim * kDgRsmDim; ++i) {
            px[i * 4 + 0] = r; px[i * 4 + 1] = g; px[i * 4 + 2] = b; px[i * 4 + 3] = 255;
        }
        return dev_->CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kDgRsmDim, kDgRsmDim, 1,
            WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            1, 1, {px.data()}, TextureSamplerDesc::FromLinearFlag(false));
    };
    // RSM 位置 (0.5,0.5,0)；法线 (0,0,-1)（128,128,0 → ×2-1 → 归一化(0,0,-1)）；通量 (200,100,50)/255。
    if (!dg_rsm_pos_)  dg_rsm_pos_  = make_rsm(128, 128, 0);
    if (!dg_rsm_nrm_)  dg_rsm_nrm_  = make_rsm(128, 128, 0);
    if (!dg_rsm_flux_) dg_rsm_flux_ = make_rsm(200, 100, 50);

    if (!dg_shader_ || !dg_probe_ || !dg_dbg_ || !dg_irr_tex_ || !dg_vis_tex_ ||
        !dg_rsm_pos_ || !dg_rsm_nrm_ || !dg_rsm_flux_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-11] DDGI 自检资源创建失败，跳过");
        return false;
    }

    // 经引擎 DDGISystem::UpdateProbes 真实绑定面（命名 uniform 同名/同调用序）：storage image 0/1、
    // RSM sampler 2/3/4（与消费方现状 0/1/2 错开以避开 WebGPU group2 撞槽）、probe/debug SSBO 0/1。
    dev_->ResetDrawState();
    dev_->SetComputeTextureImage(0, dg_irr_tex_, /*read_only=*/false);
    dev_->SetComputeTextureImage(1, dg_vis_tex_, /*read_only=*/false);
    dev_->SetComputeTextureSampler(2, dg_rsm_pos_);
    dev_->SetComputeTextureSampler(3, dg_rsm_nrm_);
    dev_->SetComputeTextureSampler(4, dg_rsm_flux_);
    dev_->CmdBindStorageBuffer(0, dg_probe_, 0, 16u);
    dev_->CmdBindStorageBuffer(1, dg_dbg_, 0, kDgDbgBytes);
    dev_->SetComputeUniformInt(dg_shader_, "u_probe_count", 1);
    dev_->SetComputeUniformInt(dg_shader_, "u_probe_start", 0);
    dev_->SetComputeUniformInt(dg_shader_, "u_probes_to_update", 1);
    dev_->SetComputeUniformInt(dg_shader_, "u_irradiance_texels", static_cast<int>(kDgIrrTexels));
    dev_->SetComputeUniformInt(dg_shader_, "u_visibility_texels", static_cast<int>(kDgVisTexels));
    dev_->SetComputeUniformInt(dg_shader_, "u_rsm_width", static_cast<int>(kDgRsmDim));
    dev_->SetComputeUniformInt(dg_shader_, "u_rsm_height", static_cast<int>(kDgRsmDim));
    dev_->SetComputeUniformInt(dg_shader_, "u_frame_index", 0);
    dev_->SetComputeUniformFloat(dg_shader_, "u_hysteresis", 0.0f);
    dev_->SetComputeUniformIVec3(dg_shader_, "u_grid_resolution", 1, 1, 1);
    dev_->SetComputeUniformVec3(dg_shader_, "u_grid_origin", 0.0f, 0.0f, 0.0f);
    dev_->SetComputeUniformVec3(dg_shader_, "u_grid_spacing", 1.0f, 1.0f, 1.0f);
    dev_->SetComputeUniformVec3(dg_shader_, "u_light_dir", 0.0f, 0.0f, 1.0f);
    dev_->SetComputeUniformVec3(dg_shader_, "u_light_color", 1.0f, 1.0f, 1.0f);

    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(dg_shader_, (kDgIrrTexels + 7u) / 8u, 1, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kDgDbgBytes;
    dg_rb_out_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    const WebGPURhiDevice::BufferEntry* be_dbg = dev_->FindBuffer(dg_dbg_);
    if (!dg_rb_out_ || !be_dbg || !be_dbg->buffer) {
        if (dg_rb_out_) { wgpuBufferRelease(dg_rb_out_); dg_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_dbg->buffer, 0, dg_rb_out_, 0, kDgDbgBytes);
    return true;
}

void WebGpuSelfTestHarness::KickDDGISelfTestReadback() {
    if (!dg_rb_out_) return;
    auto* ctx = new DDGISelfTestCtx();
    ctx->rb_out = dg_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    dg_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kDgDbgBytes, OnDDGIMapped, ctx);
}

bool WebGpuSelfTestHarness::RecordHairSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // 4 个着色器直接取引擎真源（hair_compute_shaders.h::kHair*SourceWGSL），与消费方
    // hair_system.cpp::InitComputeShaders 同 push_constant_bytes（48/4/12/12）。
    if (!hr_shader_)  hr_shader_  = dev_->CreateComputeShaderEx("", "", "", 4, 0, 0, 48, render::kHairIntegrateSourceWGSL);
    if (!hr_local_)   hr_local_   = dev_->CreateComputeShaderEx("", "", "", 3, 0, 0, 12, render::kHairLocalShapeSourceWGSL);
    if (!hr_length_)  hr_length_  = dev_->CreateComputeShaderEx("", "", "", 3, 0, 0, 4,  render::kHairLengthConstraintSourceWGSL);
    if (!hr_tangent_) hr_tangent_ = dev_->CreateComputeShaderEx("", "", "", 3, 0, 0, 12, render::kHairUpdateTangentSourceWGSL);

    if (!hr_cur_) {
        GpuBufferDesc d; d.size = sizeof(kHrInitCur); d.usage = GpuBufferUsage::kStorage;
        hr_cur_ = dev_->CreateGpuBuffer(d, kHrInitCur).raw();
    }
    if (!hr_prev_) {
        GpuBufferDesc d; d.size = sizeof(kHrInitPrev); d.usage = GpuBufferUsage::kStorage;
        hr_prev_ = dev_->CreateGpuBuffer(d, kHrInitPrev).raw();
    }
    if (!hr_rest_) {
        GpuBufferDesc d; d.size = sizeof(kHrInitRest); d.usage = GpuBufferUsage::kStorage;
        hr_rest_ = dev_->CreateGpuBuffer(d, kHrInitRest).raw();
    }
    if (!hr_tan_) {
        GpuBufferDesc d; d.size = sizeof(kHrInitTan); d.usage = GpuBufferUsage::kStorage;
        hr_tan_ = dev_->CreateGpuBuffer(d, kHrInitTan).raw();
    }
    if (!hr_strand_) {
        const uint32_t si[2] = {0u, kHrVerts};  // strand 0：offset=0, count=4
        GpuBufferDesc d; d.size = sizeof(si); d.usage = GpuBufferUsage::kStorage;
        hr_strand_ = dev_->CreateGpuBuffer(d, si).raw();
    }

    if (!hr_shader_ || !hr_local_ || !hr_length_ || !hr_tangent_ ||
        !hr_cur_ || !hr_prev_ || !hr_rest_ || !hr_tan_ || !hr_strand_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-12] hair 自检资源创建失败，跳过");
        return false;
    }

    // 全 4 趟同序跑在同一 compute pass 内（趟间 WebGPU 自动按 storage 资源依赖串行化），
    // 与 hair_system.cpp::SimulateCompute 的绑定/uniform/dispatch 顺序逐一对齐。
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    const unsigned int groups_v = (kHrVerts + 63u) / 64u;
    const unsigned int groups_s = (kHrStrands + 63u) / 64u;

    // ① integrate：b0=cur b1=prev b2=rest b3=strand + 12 uniform
    dev_->ResetDrawState();
    dev_->CmdBindStorageBuffer(0, hr_cur_,  0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(1, hr_prev_, 0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(2, hr_rest_, 0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(3, hr_strand_, 0, 8u);
    dev_->SetComputeUniformInt(hr_shader_,   "u_num_vertices", static_cast<int>(kHrVerts));
    dev_->SetComputeUniformFloat(hr_shader_, "u_dt",           kHrDt);
    dev_->SetComputeUniformFloat(hr_shader_, "u_damping",      kHrDamping);
    dev_->SetComputeUniformFloat(hr_shader_, "u_gx",           0.0f);
    dev_->SetComputeUniformFloat(hr_shader_, "u_gy",          -1.0f);
    dev_->SetComputeUniformFloat(hr_shader_, "u_gz",           0.0f);
    dev_->SetComputeUniformFloat(hr_shader_, "u_gw",           kHrGw);
    dev_->SetComputeUniformFloat(hr_shader_, "u_wx",           0.0f);
    dev_->SetComputeUniformFloat(hr_shader_, "u_wy",           0.0f);
    dev_->SetComputeUniformFloat(hr_shader_, "u_wz",           0.0f);
    dev_->SetComputeUniformFloat(hr_shader_, "u_ww",           0.0f);
    dev_->SetComputeUniformFloat(hr_shader_, "u_time",         0.0f);
    dev_->DispatchCompute(hr_shader_, groups_v, 1, 1);
    dev_->ComputeMemoryBarrier();

    // ② local_shape：b0=cur b1=rest b2=strand + 3 uniform
    dev_->ResetDrawState();
    dev_->CmdBindStorageBuffer(0, hr_cur_,  0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(1, hr_rest_, 0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(2, hr_strand_, 0, 8u);
    dev_->SetComputeUniformInt(hr_local_,   "u_num_strands",      static_cast<int>(kHrStrands));
    dev_->SetComputeUniformFloat(hr_local_, "u_stiffness_local",  kHrStLocal);
    dev_->SetComputeUniformFloat(hr_local_, "u_stiffness_global", kHrStGlobal);
    dev_->DispatchCompute(hr_local_, groups_s, 1, 1);
    dev_->ComputeMemoryBarrier();

    // ③ length：b0=cur b1=rest b2=strand + 1 uniform
    dev_->ResetDrawState();
    dev_->CmdBindStorageBuffer(0, hr_cur_,  0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(1, hr_rest_, 0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(2, hr_strand_, 0, 8u);
    dev_->SetComputeUniformInt(hr_length_, "u_num_strands", static_cast<int>(kHrStrands));
    dev_->DispatchCompute(hr_length_, groups_s, 1, 1);
    dev_->ComputeMemoryBarrier();

    // ④ tangent：b0=cur b1=tangent b2=strand + 3 uniform
    dev_->ResetDrawState();
    dev_->CmdBindStorageBuffer(0, hr_cur_, 0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(1, hr_tan_, 0, kHrPosBytes);
    dev_->CmdBindStorageBuffer(2, hr_strand_, 0, 8u);
    dev_->SetComputeUniformInt(hr_tangent_, "u_num_vertices",     static_cast<int>(kHrVerts));
    dev_->SetComputeUniformInt(hr_tangent_, "u_num_strands",      static_cast<int>(kHrStrands));
    dev_->SetComputeUniformInt(hr_tangent_, "u_verts_per_strand", kHrVertsPerStr);
    dev_->DispatchCompute(hr_tangent_, groups_v, 1, 1);

    dev_->EndComputePass();
    dev_->ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kHrRbBytes;
    hr_rb_out_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    const WebGPURhiDevice::BufferEntry* be_cur = dev_->FindBuffer(hr_cur_);
    const WebGPURhiDevice::BufferEntry* be_tan = dev_->FindBuffer(hr_tan_);
    if (!hr_rb_out_ || !be_cur || !be_cur->buffer || !be_tan || !be_tan->buffer) {
        if (hr_rb_out_) { wgpuBufferRelease(hr_rb_out_); hr_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_cur->buffer, 0, hr_rb_out_, 0,           kHrPosBytes);
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_tan->buffer, 0, hr_rb_out_, kHrPosBytes, kHrPosBytes);
    return true;
}

void WebGpuSelfTestHarness::KickHairSelfTestReadback() {
    if (!hr_rb_out_) return;
    auto* ctx = new HairSelfTestCtx();
    ctx->rb_out = hr_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    hr_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kHrRbBytes, OnHairMapped, ctx);
}

// B3b-14 grass 风场 compute 正确性自检：内联镜像 grass_system.cpp 风场 WGSL（engine 层不能 include
//   modules 层头）→ 造已知实例 → BeginComputePass/DispatchCompute/EndComputePass → copy 输出 mat4 回读校验。
bool WebGpuSelfTestHarness::RecordGrassSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // 内联镜像（与 grass_system.cpp::kGrassWindComputeSourceWGSL 逐句一致）。
    static const char* kGrassWGSL = R"WGSL(// dse-wgsl
struct GrassInstance {
  pos_yaw : vec4<f32>,
  wh_phase_fade : vec4<f32>,
};
@group(3) @binding(0) var<storage, read_write> instances : array<GrassInstance>;
@group(3) @binding(1) var<storage, read_write> matrices : array<mat4x4<f32>>;
struct PC {
  @align(16) u_wind_dir : vec2<f32>,
  @align(16) u_wind_speed : f32,
  @align(16) u_wind_strength : f32,
  @align(16) u_wind_turbulence : f32,
  @align(16) u_time : f32,
  @align(16) u_instance_count : i32,
};
@group(1) @binding(8) var<uniform> pc : PC;
@compute @workgroup_size(64, 1, 1)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let idx = gid.x;
  if (i32(idx) >= pc.u_instance_count) { return; }
  let inst = instances[idx];
  let pos = inst.pos_yaw.xyz;
  let yaw = inst.pos_yaw.w;
  let w = inst.wh_phase_fade.x;
  let h = inst.wh_phase_fade.y;
  let wind_phase = inst.wh_phase_fade.z;
  let fade = inst.wh_phase_fade.w;
  let phase = wind_phase + pc.u_time * pc.u_wind_speed;
  let bend = sin(phase) * pc.u_wind_strength;
  let turb = sin(phase * 3.7 + wind_phase * 2.3) * pc.u_wind_turbulence;
  let total_bend = clamp(bend + turb, -0.436, 0.436);
  let rx = -total_bend * pc.u_wind_dir.y;
  let rz =  total_bend * pc.u_wind_dir.x;
  let cx = cos(rx); let sx = sin(rx);
  let cz = cos(rz); let sz = sin(rz);
  let cy = cos(yaw); let sy = sin(yaw);
  let a00 = cz * cy; let a01 = -sz; let a02 = cz * sy;
  let a10 = sz * cy; let a11 = cz;  let a12 = sz * sy;
  let a20 = -sy;     let a21 = 0.0; let a22 = cy;
  let r00 = a00;                  let r01 = a01;                  let r02 = a02;
  let r10 = cx * a10 - sx * a20;  let r11 = cx * a11 - sx * a21;  let r12 = cx * a12 - sx * a22;
  let r20 = sx * a10 + cx * a20;  let r21 = sx * a11 + cx * a21;  let r22 = sx * a12 + cx * a22;
  let hf = h * fade;
  matrices[idx] = mat4x4<f32>(
    vec4<f32>(r00 * w, r10 * w, r20 * w, 0.0),
    vec4<f32>(r01 * hf, r11 * hf, r21 * hf, 0.0),
    vec4<f32>(r02 * w, r12 * w, r22 * w, 0.0),
    vec4<f32>(pos, 1.0));
}
)WGSL";

    if (!gr_shader_) gr_shader_ = dev_->CreateComputeShaderEx("", "", "", 2, 0, 0, 28, kGrassWGSL);
    if (!gr_in_) {
        GpuBufferDesc d; d.size = kGrInBytes; d.usage = GpuBufferUsage::kStorage;
        gr_in_ = dev_->CreateGpuBuffer(d, kGrInst).raw();
    }
    if (!gr_out_) {
        GpuBufferDesc d; d.size = kGrOutBytes; d.usage = GpuBufferUsage::kStorage;
        gr_out_ = dev_->CreateGpuBuffer(d, nullptr).raw();
    }
    if (!gr_shader_ || !gr_in_ || !gr_out_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-14] grass 自检资源创建失败，跳过");
        return false;
    }

    dev_->ResetDrawState();
    dev_->CmdBindStorageBuffer(0, gr_in_,  0, kGrInBytes);
    dev_->CmdBindStorageBuffer(1, gr_out_, 0, kGrOutBytes);
    dev_->SetComputeUniformVec2f(gr_shader_, "u_wind_dir",        kGrWindDirX, kGrWindDirY);
    dev_->SetComputeUniformFloat(gr_shader_, "u_wind_speed",      kGrWindSpeed);
    dev_->SetComputeUniformFloat(gr_shader_, "u_wind_strength",   kGrWindStrength);
    dev_->SetComputeUniformFloat(gr_shader_, "u_wind_turbulence", kGrWindTurb);
    dev_->SetComputeUniformFloat(gr_shader_, "u_time",            kGrTime);
    dev_->SetComputeUniformInt(gr_shader_,   "u_instance_count",  static_cast<int>(kGrInstances));

    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(gr_shader_, (kGrInstances + 63u) / 64u, 1, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kGrOutBytes;
    gr_rb_out_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    const WebGPURhiDevice::BufferEntry* be_out = dev_->FindBuffer(gr_out_);
    if (!gr_rb_out_ || !be_out || !be_out->buffer) {
        if (gr_rb_out_) { wgpuBufferRelease(gr_rb_out_); gr_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(dev_->frame_encoder_, be_out->buffer, 0, gr_rb_out_, 0, kGrOutBytes);
    return true;
}

void WebGpuSelfTestHarness::KickGrassSelfTestReadback() {
    if (!gr_rb_out_) return;
    auto* ctx = new GrassSelfTestCtx();
    ctx->rb_out = gr_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    gr_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kGrOutBytes, OnGrassMapped, ctx);
}

// B3b-13 bloom 双滤波 compute 真链路自检：手译 bloom_downsample.comp / bloom_upsample.comp 核心为 WGSL，
//   经 gen compute 造已知 rgba16f 渐变 → 下采样 13-tap → 上采样 3×3 tent + base 累加 → copy 回读校验。
bool WebGpuSelfTestHarness::RecordBloomSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // gen：按 u_kind 选公式写 rgba16f storage（避免 CPU float→half 编码，与 CPU 预期共用公式）。
    static const char* kBloomGenWGSL = R"WGSL(// dse-wgsl
struct GP {
  @align(16) u_dim : i32,
  @align(16) u_kind : i32,
};
@group(1) @binding(8) var<uniform> gp : GP;
@group(2) @binding(0) var dst_img : texture_storage_2d<rgba16float, write>;
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (i32(gid.x) >= gp.u_dim || i32(gid.y) >= gp.u_dim) { return; }
  let x = f32(gid.x);
  let y = f32(gid.y);
  var c : vec3<f32>;
  if (gp.u_kind == 0) {
    c = vec3<f32>(x + y * 8.0, x * 0.5, y * 0.25);
  } else if (gp.u_kind == 1) {
    c = vec3<f32>(x + y * 4.0, x * 0.5, y * 0.25);
  } else {
    c = vec3<f32>((x + y * 4.0) * 0.25, 1.0, 0.5);
  }
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(c, 1.0));
}
)WGSL";

    // 下采样：bloom_downsample.comp 13-tap 加权（此处整数 tap，等价采样路径；中心 c=(2·dst+1)）。
    static const char* kBloomDownWGSL = R"WGSL(// dse-wgsl
struct DP {
  @align(16) u_src_dim : i32,
  @align(16) u_dst_dim : i32,
};
@group(1) @binding(8) var<uniform> dp : DP;
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var dst_img : texture_storage_2d<rgba16float, write>;
fn ld(p : vec2<i32>, m : i32) -> vec3<f32> {
  let c = clamp(p, vec2<i32>(0, 0), vec2<i32>(m, m));
  return textureLoad(src_tex, c, 0).rgb;
}
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (i32(gid.x) >= dp.u_dst_dim || i32(gid.y) >= dp.u_dst_dim) { return; }
  let m = dp.u_src_dim - 1;
  let cx = i32(gid.x) * 2 + 1;
  let cy = i32(gid.y) * 2 + 1;
  let a = ld(vec2<i32>(cx - 2, cy + 2), m);
  let b = ld(vec2<i32>(cx,     cy + 2), m);
  let c = ld(vec2<i32>(cx + 2, cy + 2), m);
  let d = ld(vec2<i32>(cx - 2, cy),     m);
  let e = ld(vec2<i32>(cx,     cy),     m);
  let f = ld(vec2<i32>(cx + 2, cy),     m);
  let g = ld(vec2<i32>(cx - 2, cy - 2), m);
  let h = ld(vec2<i32>(cx,     cy - 2), m);
  let i = ld(vec2<i32>(cx + 2, cy - 2), m);
  let j = ld(vec2<i32>(cx - 1, cy + 1), m);
  let k = ld(vec2<i32>(cx + 1, cy + 1), m);
  let l = ld(vec2<i32>(cx - 1, cy - 1), m);
  let mm = ld(vec2<i32>(cx + 1, cy - 1), m);
  let result = e * 0.125
    + (a + c + g + i) * 0.03125
    + (b + d + f + h) * 0.0625
    + (j + k + l + mm) * 0.125;
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(result, 1.0));
}
)WGSL";

    // 上采样：bloom_upsample.comp 3×3 tent + 按 blend 累加。消费方原 imageLoad(u_dst) 读回自身（in-place
    //   read-write rgba16f storage 核心 WebGPU 不支持），此自检以独立 base 采样纹理替代验证 tent + 累加数学。
    static const char* kBloomUpWGSL = R"WGSL(// dse-wgsl
struct UPp {
  @align(16) u_dim : i32,
  @align(16) u_blend : f32,
};
@group(1) @binding(8) var<uniform> up : UPp;
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var base_tex : texture_2d<f32>;
@group(2) @binding(2) var dst_img : texture_storage_2d<rgba16float, write>;
fn lu(p : vec2<i32>, m : i32) -> vec3<f32> {
  let c = clamp(p, vec2<i32>(0, 0), vec2<i32>(m, m));
  return textureLoad(src_tex, c, 0).rgb;
}
@compute @workgroup_size(8, 8)
fn cs_main(@builtin(global_invocation_id) gid : vec3<u32>) {
  if (i32(gid.x) >= up.u_dim || i32(gid.y) >= up.u_dim) { return; }
  let m = up.u_dim - 1;
  let cx = i32(gid.x);
  let cy = i32(gid.y);
  let a = lu(vec2<i32>(cx - 1, cy + 1), m);
  let b = lu(vec2<i32>(cx,     cy + 1), m);
  let c = lu(vec2<i32>(cx + 1, cy + 1), m);
  let d = lu(vec2<i32>(cx - 1, cy),     m);
  let e = lu(vec2<i32>(cx,     cy),     m);
  let f = lu(vec2<i32>(cx + 1, cy),     m);
  let g = lu(vec2<i32>(cx - 1, cy - 1), m);
  let h = lu(vec2<i32>(cx,     cy - 1), m);
  let i = lu(vec2<i32>(cx + 1, cy - 1), m);
  let ups = (e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i)) * (1.0 / 16.0);
  let base = textureLoad(base_tex, vec2<i32>(cx, cy), 0).rgb;
  let result = base + ups * up.u_blend;
  textureStore(dst_img, vec2<i32>(i32(gid.x), i32(gid.y)), vec4<f32>(result, 1.0));
}
)WGSL";

    if (!bl_gen_shader_)  bl_gen_shader_  = dev_->CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kBloomGenWGSL);
    if (!bl_down_shader_) bl_down_shader_ = dev_->CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kBloomDownWGSL);
    if (!bl_up_shader_)   bl_up_shader_   = dev_->CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kBloomUpWGSL);

    auto make_tex = [&](uint32_t dim, WGPUTextureUsageFlags usage) -> unsigned int {
        return dev_->CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, dim, dim, 1,
                                 WGPUTextureFormat_RGBA16Float, usage, 1, 1, {nullptr},
                                 TextureSamplerDesc::FromLinearFlag(false));
    };
    if (!bl_src8_)   bl_src8_   = make_tex(kBlSrcDim,  WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding);
    if (!bl_down4_)  bl_down4_  = make_tex(kBlDownDim, WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc);
    if (!bl_usrc4_)  bl_usrc4_  = make_tex(kBlUpDim,   WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding);
    if (!bl_ubase4_) bl_ubase4_ = make_tex(kBlUpDim,   WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding);
    if (!bl_up4_)    bl_up4_    = make_tex(kBlUpDim,   WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc);

    if (!bl_gen_shader_ || !bl_down_shader_ || !bl_up_shader_ ||
        !bl_src8_ || !bl_down4_ || !bl_usrc4_ || !bl_ubase4_ || !bl_up4_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-13] bloom 自检资源创建失败，跳过");
        return false;
    }

    auto gen = [&](unsigned int tex, uint32_t dim, int kind) -> bool {
        dev_->ResetDrawState();
        dev_->SetComputeUniformInt(bl_gen_shader_, "u_dim",  static_cast<int>(dim));
        dev_->SetComputeUniformInt(bl_gen_shader_, "u_kind", kind);
        dev_->SetComputeTextureImage(0, tex, /*read_only=*/false);
        dev_->BeginComputePass();
        if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
        dev_->DispatchCompute(bl_gen_shader_, (dim + 7u) / 8u, (dim + 7u) / 8u, 1);
        dev_->EndComputePass();
        dev_->ResetDrawState();
        return true;
    };
    // ① 三趟生成（src8 / usrc4 / ubase4），各独立 compute pass（pass 间自动屏障）。
    if (!gen(bl_src8_,   kBlSrcDim, 0)) return false;
    if (!gen(bl_usrc4_,  kBlUpDim,  1)) return false;
    if (!gen(bl_ubase4_, kBlUpDim,  2)) return false;

    // ② 下采样 src8（8×8）→ down4（4×4）。
    dev_->ResetDrawState();
    dev_->SetComputeUniformInt(bl_down_shader_, "u_src_dim", static_cast<int>(kBlSrcDim));
    dev_->SetComputeUniformInt(bl_down_shader_, "u_dst_dim", static_cast<int>(kBlDownDim));
    dev_->SetComputeTextureImage(0, bl_src8_,  /*read_only=*/true);
    dev_->SetComputeTextureImage(1, bl_down4_, /*read_only=*/false);
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(bl_down_shader_, (kBlDownDim + 7u) / 8u, (kBlDownDim + 7u) / 8u, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // ③ 上采样 usrc4（4×4）+ ubase4 按 blend 累加 → up4（4×4）。
    dev_->SetComputeUniformInt(bl_up_shader_,   "u_dim",   static_cast<int>(kBlUpDim));
    dev_->SetComputeUniformFloat(bl_up_shader_, "u_blend", kBlBlend);
    dev_->SetComputeTextureImage(0, bl_usrc4_,  /*read_only=*/true);
    dev_->SetComputeTextureImage(1, bl_ubase4_, /*read_only=*/true);
    dev_->SetComputeTextureImage(2, bl_up4_,    /*read_only=*/false);
    dev_->BeginComputePass();
    if (!dev_->cur_compute_pass_) { dev_->ResetDrawState(); return false; }
    dev_->DispatchCompute(bl_up_shader_, (kBlUpDim + 7u) / 8u, (kBlUpDim + 7u) / 8u, 1);
    dev_->EndComputePass();
    dev_->ResetDrawState();

    // copy down4 + up4 storage 纹理 → 回读缓冲（各占 256 对齐分段）。
    const WebGPURhiDevice::TextureEntry* te_down = dev_->FindTexture(bl_down4_);
    const WebGPURhiDevice::TextureEntry* te_up   = dev_->FindTexture(bl_up4_);
    if (!te_down || !te_down->texture || !te_up || !te_up->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kBlTotalBytes;
    bl_rb_out_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!bl_rb_out_) return false;
    auto copy_tex = [&](const WebGPURhiDevice::TextureEntry* te, uint32_t dim, uint32_t off) {
        WGPUImageCopyTexture src{};
        src.texture = te->texture; src.mipLevel = 0; src.aspect = WGPUTextureAspect_All;
        WGPUImageCopyBuffer dst{};
        dst.buffer = bl_rb_out_;
        dst.layout.offset = off;
        dst.layout.bytesPerRow = kBlRowBytes;
        dst.layout.rowsPerImage = dim;
        WGPUExtent3D ext{dim, dim, 1};
        wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    };
    copy_tex(te_down, kBlDownDim, kBlDownOff);
    copy_tex(te_up,   kBlUpDim,   kBlUpOff);
    return true;
}

void WebGpuSelfTestHarness::KickBloomSelfTestReadback() {
    if (!bl_rb_out_) return;
    auto* ctx = new BloomSelfTestCtx();
    ctx->rb_out = bl_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    bl_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kBlTotalBytes, OnBloomMapped, ctx);
}

// --- Task 4 Subtask 1：MultiDrawIndexedIndirect 离屏自检 ---
// 经引擎-facing API（CmdBeginRenderPass + CmdBindPipeline + CmdBindVertexBuffer + CmdBindIndexBuffer +
// MultiDrawIndexedIndirect）把 4 象限 quad 渲到 64×64 离屏 RT，预置 indirect cmds instance_count=[1,0,1,0]
// 模拟「可见/被剔」，随帧 copyTextureToBuffer，提交后异步回读半精解码校验可见象限有色、被剔象限为黑。
// 不经裸 wgpu 绘制，专为验证 MultiDrawIndexedIndirect 覆写（按 byte_offset+i*stride 循环）。
bool WebGpuSelfTestHarness::RecordMultiDrawIndirectSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // 离屏 RT（引擎 CreateRenderTarget：RGBA16Float 颜色 + CopySrc，无深度）。
    if (!t41_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT41RtSize);
        d.height = static_cast<int>(kT41RtSize);
        d.has_color = true;
        d.has_depth = false;
        t41_rt_ = dev_->CreateRenderTarget(d);
    }
    // 内建 WGSL 程序：pos.xy@loc0 + color.rgb@loc1 → 输出纯色。
    if (!t41_program_) {
        static const char* kWGSL = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) color : vec3<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) c : vec3<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.color = c; return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> { return vec4<f32>(i.color, 1.0); }
)WGSL";
        t41_program_ = dev_->CreateShaderProgram(kWGSL, "");
    }
    if (!t41_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t41_pso_ = dev_->CreatePipelineState(d);
    }
    // 4 象限 quad（中心 ±0.5，半边 0.35），各一种颜色（pos.xy + color.rgb，stride 20）。
    if (!t41_vbo_) {
        const float h = 0.35f;
        const float cx[kT41Instances] = {-0.5f, 0.5f, -0.5f, 0.5f};
        const float cy[kT41Instances] = { 0.5f, 0.5f, -0.5f, -0.5f};
        const float col[kT41Instances][3] = {{1,0,0},{0,1,0},{0,0,1},{1,1,0}};
        float verts[kT41Instances * 4 * 5];
        size_t v = 0;
        for (uint32_t i = 0; i < kT41Instances; ++i) {
            const float qx[4] = {cx[i]-h, cx[i]+h, cx[i]+h, cx[i]-h};
            const float qy[4] = {cy[i]-h, cy[i]-h, cy[i]+h, cy[i]+h};
            for (int k = 0; k < 4; ++k) {
                verts[v++] = qx[k]; verts[v++] = qy[k];
                verts[v++] = col[i][0]; verts[v++] = col[i][1]; verts[v++] = col[i][2];
            }
        }
        t41_vbo_ = dev_->CreateBuffer(sizeof(verts), verts, false, false);
    }
    if (!t41_ibo_) {
        uint32_t idx[kT41Instances * 6];
        for (uint32_t i = 0; i < kT41Instances; ++i) {
            const uint32_t b = i * 4;
            idx[i*6+0]=b+0; idx[i*6+1]=b+1; idx[i*6+2]=b+2;
            idx[i*6+3]=b+0; idx[i*6+4]=b+2; idx[i*6+5]=b+3;
        }
        t41_ibo_ = dev_->CreateBuffer(sizeof(idx), idx, false, true);
    }
    // indirect 缓冲：预置 4 条 [indexCount=6, instanceCount, firstIndex=i*6, baseVertex=0, firstInstance=0]，
    // instance_count=[1,0,1,0] 直接模拟剔除结果（本子项专测 MDI 覆写，不含 compute 剔除）。
    if (!t41_indirect_) {
        uint32_t cmds[kT41Instances * kT41DrawWords];
        const uint32_t inst_counts[kT41Instances] = {1u, 0u, 1u, 0u};
        for (uint32_t i = 0; i < kT41Instances; ++i) {
            cmds[i*kT41DrawWords+0] = 6u;            // indexCount
            cmds[i*kT41DrawWords+1] = inst_counts[i];// instanceCount
            cmds[i*kT41DrawWords+2] = i * 6u;        // firstIndex
            cmds[i*kT41DrawWords+3] = 0u;            // baseVertex
            cmds[i*kT41DrawWords+4] = 0u;            // firstInstance
        }
        GpuBufferDesc d;
        d.size = sizeof(cmds);
        d.usage = GpuBufferUsage::kIndirect | GpuBufferUsage::kStorage;
        t41_indirect_ = dev_->CreateGpuBuffer(d, cmds).raw();
    }
    if (!t41_rt_ || !t41_program_ || !t41_pso_ || !t41_vbo_ || !t41_ibo_ || !t41_indirect_) {
        DEBUG_LOG_ERROR("WebGPU[T4-1] MultiDrawIndexedIndirect 自检资源创建失败，跳过");
        return false;
    }

    // 引擎-facing 录制：开离屏 RT pass → 绑管线/顶点/索引 → MultiDrawIndexedIndirect（被测）。
    RenderPassDesc rp;
    rp.render_target = t41_rt_;
    rp.clear_color_enabled = true;
    rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    dev_->CmdBeginRenderPass(rp);
    if (!dev_->cur_pass_) return false;
    dev_->CmdSetViewport(0, 0, static_cast<int>(kT41RtSize), static_cast<int>(kT41RtSize));
    const unsigned int pipe = dev_->GetGraphicsPipeline(t41_pso_, t41_program_);
    dev_->CmdBindPipeline(pipe);
    const std::vector<VertexAttr> attrs = {
        VertexAttr{0, 2, 0},  // pos.xy
        VertexAttr{1, 3, 8},  // color.rgb
    };
    dev_->CmdBindVertexBuffer(0, t41_vbo_, 20, attrs, VertexInputRate::PerVertex);
    dev_->CmdBindIndexBuffer(t41_ibo_, IndexType::UInt32);
    dev_->MultiDrawIndexedIndirect(t41_indirect_, static_cast<int>(kT41Instances),
                             kT41DrawWords * sizeof(uint32_t), 0);
    dev_->CmdEndRenderPass();

    // copy 离屏 RT 颜色纹理 → 回读缓冲（随帧提交）。
    const WebGPURhiDevice::RenderTargetEntry* rt = dev_->FindRenderTarget(t41_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const WebGPURhiDevice::TextureEntry* color = dev_->FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT41RtBytes);
    t41_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!t41_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = color->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = t41_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kT41RtRowBytes;
    dst.layout.rowsPerImage = kT41RtSize;
    WGPUExtent3D ext{kT41RtSize, kT41RtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickMultiDrawIndirectSelfTestReadback() {
    if (!t41_rb_pixels_) return;
    auto* ctx = new MultiDrawIndirectSelfTestCtx();
    ctx->rb_pixels = t41_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t41_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT41RtBytes, OnT41PixelsMapped, ctx);
}

// --- Task 4 Subtask 2：Mega VAO 离屏自检 ---
// 经引擎-facing Mega VAO API（CreateMegaVAO → UpdateMegaVBO/IBO 上传 4 象限 BatchVertex(92B) 几何 →
// BindMegaVAO 设 92B draw state → CmdDrawIndexed）把 4 象限 quad 渲到 64×64 离屏 RT，4 象限各一种颜色
// 全部可见，随帧 copyTextureToBuffer，提交后异步回读半精解码校验各象限颜色就位——验证 BatchVertex 92B
// 布局（pos@0/color@12）解析与 BindMegaVAO 设状态正确。
bool WebGpuSelfTestHarness::RecordMegaVaoSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    if (!t42_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT42RtSize);
        d.height = static_cast<int>(kT42RtSize);
        d.has_color = true;
        d.has_depth = false;
        t42_rt_ = dev_->CreateRenderTarget(d);
    }
    // WGSL 程序：完整声明 BatchVertex 7 个 @location（与 BindMegaVAO 设的 92B 布局逐一对应），
    //   顶点取 pos.xy/color.rgb 输出纯色（其余属性声明但未用，验证 92B 布局可被正确解析）。
    if (!t42_program_) {
        static const char* kWGSL = R"WGSL(// dse-wgsl
struct VsIn {
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
  @location(5) weights : vec4<f32>,
  @location(6) joints : vec4<f32>,
};
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) color : vec3<f32>, };
@vertex fn vs_main(i : VsIn) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(i.pos.xy, 0.0, 1.0); o.color = i.color.rgb; return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> { return vec4<f32>(i.color, 1.0); }
)WGSL";
        t42_program_ = dev_->CreateShaderProgram(kWGSL, "");
    }
    if (!t42_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t42_pso_ = dev_->CreatePipelineState(d);
    }
    // 经被测 Mega VAO API 建缓冲并上传 4 象限 BatchVertex(92B) 几何（pos/color，余属性默认）。
    if (!t42_vao_) {
        const float h = 0.35f;
        const float cx[kT42Quads] = {-0.5f, 0.5f, -0.5f, 0.5f};
        const float cy[kT42Quads] = { 0.5f, 0.5f, -0.5f, -0.5f};
        const float col[kT42Quads][3] = {{1,0,0},{0,1,0},{0,0,1},{1,1,0}};
        std::vector<BatchVertex> verts(kT42Quads * 4);
        size_t vi = 0;
        for (uint32_t i = 0; i < kT42Quads; ++i) {
            const float qx[4] = {cx[i]-h, cx[i]+h, cx[i]+h, cx[i]-h};
            const float qy[4] = {cy[i]-h, cy[i]-h, cy[i]+h, cy[i]+h};
            for (int k = 0; k < 4; ++k) {
                BatchVertex& bv = verts[vi++];
                bv.pos = glm::vec3(qx[k], qy[k], 0.0f);
                bv.color = glm::vec4(col[i][0], col[i][1], col[i][2], 1.0f);
                bv.uv = glm::vec2(0.0f);
            }
        }
        std::vector<uint32_t> idx(kT42Quads * 6);
        for (uint32_t i = 0; i < kT42Quads; ++i) {
            const uint32_t b = i * 4;
            idx[i*6+0]=b+0; idx[i*6+1]=b+1; idx[i*6+2]=b+2;
            idx[i*6+3]=b+0; idx[i*6+4]=b+2; idx[i*6+5]=b+3;
        }
        const size_t vbytes = verts.size() * sizeof(BatchVertex);
        const size_t ibytes = idx.size() * sizeof(uint32_t);
        t42_vao_ = dev_->CreateMegaVAO(vbytes, ibytes, t42_vbo_, t42_ibo_);
        dev_->UpdateMegaVBO(t42_vbo_, 0, vbytes, verts.data());
        dev_->UpdateMegaIBO(t42_ibo_, 0, ibytes, idx.data());
    }
    if (!t42_rt_ || !t42_program_ || !t42_pso_ || !t42_vao_ || !t42_vbo_ || !t42_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[T4-2] Mega VAO 自检资源创建失败，跳过");
        return false;
    }

    RenderPassDesc rp;
    rp.render_target = t42_rt_;
    rp.clear_color_enabled = true;
    rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    dev_->CmdBeginRenderPass(rp);
    if (!dev_->cur_pass_) return false;
    dev_->CmdSetViewport(0, 0, static_cast<int>(kT42RtSize), static_cast<int>(kT42RtSize));
    const unsigned int pipe = dev_->GetGraphicsPipeline(t42_pso_, t42_program_);
    dev_->CmdBindPipeline(pipe);
    dev_->BindMegaVAO(t42_vao_);  // 被测：据记录的 VBO/IBO 设 BatchVertex 92B 引擎 draw state。
    dev_->CmdDrawIndexed(kT42Quads * 6, 0, 0);
    dev_->CmdEndRenderPass();

    const WebGPURhiDevice::RenderTargetEntry* rt = dev_->FindRenderTarget(t42_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const WebGPURhiDevice::TextureEntry* color = dev_->FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT42RtBytes);
    t42_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!t42_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = color->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = t42_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kT42RtRowBytes;
    dst.layout.rowsPerImage = kT42RtSize;
    WGPUExtent3D ext{kT42RtSize, kT42RtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickMegaVaoSelfTestReadback() {
    if (!t42_rb_pixels_) return;
    auto* ctx = new MegaVaoSelfTestCtx();
    ctx->rb_pixels = t42_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t42_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT42RtBytes, OnT42PixelsMapped, ctx);
}

// --- Task 4 Subtask 3：GPU-driven PBR 离屏自检 ---
// 经引擎-facing GPU-driven 消费方调用序（与 ForwardScenePass 同序：SetupGPUDrivenPBRShader → BindGpuBuffer
// 实例/材质 SSBO → BindMegaVAO → BindGPUDrivenTextures → MultiDrawIndexedIndirect）把两个实例（model 分别
// 平移到左/右半，材质分别红/绿 albedo）渲到 64×64 离屏 RT（颜色 RGBA16Float + 深度），随帧 copyTextureToBuffer，
// 提交后异步回读半精解码校验左半红、右半绿——验证手译 PBR WGSL 经实例 SSBO(b5) 取 model、材质 SSBO(b9) 取
// albedo、albedo 纹理采样链路全部正确。离屏隔离、不翻 SupportsIndirectDraw()、不碰 demo 帧。
bool WebGpuSelfTestHarness::RecordGpuDrivenPBRSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;
    if (!dev_->EnsureGpuDrivenPBRShader()) {
        DEBUG_LOG_ERROR("WebGPU[T4-3] GPU-driven PBR 程序/资源未就绪，跳过自检");
        return false;
    }
    // 离屏 RT（颜色 RGBA16Float + 深度，CopySrc）。带深度以匹配 PBR PSO 的 depth test/write 状态。
    if (!t43_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT43RtSize);
        d.height = static_cast<int>(kT43RtSize);
        d.has_color = true;
        d.has_depth = true;
        t43_rt_ = dev_->CreateRenderTarget(d);
    }
    // 单 quad（中心原点、半边 0.4、法线 +Z、UV 全 0）经被测 Mega VAO API 上传 BatchVertex(92B)。
    if (!t43_vao_) {
        const float hh = 0.4f;
        const float qx[4] = {-hh, hh, hh, -hh};
        const float qy[4] = {-hh, -hh, hh, hh};
        std::vector<BatchVertex> verts(4);
        for (int k = 0; k < 4; ++k) {
            BatchVertex& bv = verts[k];
            bv.pos = glm::vec3(qx[k], qy[k], 0.0f);
            bv.color = glm::vec4(1.0f);
            bv.uv = glm::vec2(0.0f);
            bv.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        const size_t vbytes = verts.size() * sizeof(BatchVertex);
        const size_t ibytes = sizeof(idx);
        t43_vao_ = dev_->CreateMegaVAO(vbytes, ibytes, t43_vbo_, t43_ibo_);
        dev_->UpdateMegaVBO(t43_vbo_, 0, vbytes, verts.data());
        dev_->UpdateMegaIBO(t43_ibo_, 0, ibytes, idx);
    }
    // 实例 SSBO（b5）：2 个 GPUInstanceData，model 分别平移到左/右半，material_id = 0/1。
    if (!t43_inst_ssbo_) {
        GPUInstanceData inst[2]{};
        // 列主序 mat4：第 3 列为平移。左实例平移 -0.5x、右实例 +0.5x。
        inst[0].model = glm::mat4(1.0f); inst[0].model[3] = glm::vec4(-0.5f, 0.0f, 0.0f, 1.0f);
        inst[0].material_id = 0;
        inst[1].model = glm::mat4(1.0f); inst[1].model[3] = glm::vec4( 0.5f, 0.0f, 0.0f, 1.0f);
        inst[1].material_id = 1;
        GpuBufferDesc d; d.size = sizeof(inst); d.usage = GpuBufferUsage::kStorage;
        t43_inst_ssbo_ = dev_->CreateGpuBuffer(d, inst);
    }
    // 材质 SSBO（b9）：material0 红 albedo、material1 绿 albedo（metallic=0、roughness=0.9、ao=1）。
    if (!t43_mat_ssbo_) {
        GPUMaterialData mat[2]{};
        mat[0].albedo = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        mat[0].roughness_ao = glm::vec4(0.9f, 1.0f, 1.0f, 0.0f);
        mat[1].albedo = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        mat[1].roughness_ao = glm::vec4(0.9f, 1.0f, 1.0f, 0.0f);
        GpuBufferDesc d; d.size = sizeof(mat); d.usage = GpuBufferUsage::kStorage;
        t43_mat_ssbo_ = dev_->CreateGpuBuffer(d, mat);
    }
    // indirect：单条 [indexCount=6, instanceCount=2, firstIndex=0, baseVertex=0, firstInstance=0]，
    // instance_index 取 0/1 → 两实例分别读 instances[0]/[1] 的 model 与材质。
    if (!t43_indirect_) {
        const uint32_t cmd[5] = {6u, 2u, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(cmd);
        d.usage = GpuBufferUsage::kIndirect | GpuBufferUsage::kStorage;
        t43_indirect_ = dev_->CreateGpuBuffer(d, cmd);
    }
    // 白 albedo 纹理（验证纹理采样链路；base = material.albedo × white = material.albedo）。
    if (!t43_albedo_tex_) {
        const unsigned char white[4] = {255, 255, 255, 255};
        t43_albedo_tex_ = dev_->CreateTexture2D(1, 1, white, /*linear_filter=*/true);
    }
    if (!t43_rt_ || !t43_vao_ || !t43_inst_ssbo_ || !t43_mat_ssbo_ || !t43_indirect_ || !t43_albedo_tex_) {
        DEBUG_LOG_ERROR("WebGPU[T4-3] GPU-driven PBR 自检资源创建失败，跳过");
        return false;
    }

    // 引擎-facing 录制（与 ForwardScenePass GPU-driven 路径同序）。vp 用单位矩阵（NDC 直通）：
    //   实例 model 平移 ±0.5、quad 半边 0.4 → 左实例覆 x∈[-0.9,-0.1]、右实例覆 x∈[0.1,0.9]，各占一半。
    //   光行进方向 (0,0,-1) → L=+Z 朝相机、法线 +Z → ndl=1 受光；相机置 +Z。
    RenderPassDesc rp;
    rp.render_target = t43_rt_;
    rp.clear_color_enabled = true;
    rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    dev_->CmdBeginRenderPass(rp);
    if (!dev_->cur_pass_) return false;
    dev_->CmdSetViewport(0, 0, static_cast<int>(kT43RtSize), static_cast<int>(kT43RtSize));

    const glm::mat4 ident(1.0f);
    dev_->SetupGPUDrivenPBRShader(/*view=*/ident, /*proj=*/ident,
                            /*camera_pos=*/glm::vec3(0.0f, 0.0f, 1.0f),
                            /*light_dir=*/glm::vec3(0.0f, 0.0f, -1.0f),
                            /*light_color=*/glm::vec3(1.0f, 1.0f, 1.0f),
                            /*light_intensity=*/1.0f, /*ambient_intensity=*/0.3f,
                            /*shadow_strength=*/0.0f);
    dev_->BindGpuBuffer(t43_inst_ssbo_, gpu_driven::kSSBOBindingInstances);  // b5
    dev_->BindGpuBuffer(t43_mat_ssbo_,  gpu_driven::kSSBOBindingMaterials);  // b9
    dev_->BindMegaVAO(t43_vao_);
    dev_->BindGPUDrivenTextures(t43_albedo_tex_, 0, 0, 0, 0);
    dev_->MultiDrawIndexedIndirect(t43_indirect_.raw(), 1, 5 * sizeof(uint32_t), 0);
    dev_->CmdEndRenderPass();
    dev_->UnbindVAO();

    const WebGPURhiDevice::RenderTargetEntry* rt = dev_->FindRenderTarget(t43_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const WebGPURhiDevice::TextureEntry* color = dev_->FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT43RtBytes);
    t43_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!t43_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = color->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = t43_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kT43RtRowBytes;
    dst.layout.rowsPerImage = kT43RtSize;
    WGPUExtent3D ext{kT43RtSize, kT43RtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickGpuDrivenPBRSelfTestReadback() {
    if (!t43_rb_pixels_) return;
    auto* ctx = new GpuDrivenPBRSelfTestCtx();
    ctx->rb_pixels = t43_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t43_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT43RtBytes, OnT43PixelsMapped, ctx);
}

// Task 5 Subtask 1：CSM 方向光阴影深度图采样真链路自检。①阴影深度趟把中心遮挡 quad(z=0.3) 渲入
// 32×32 Depth32 atlas（其余清深=1.0）；②前向趟把 atlas（GetRenderTargetDepthTexture）绑到 group2 slot11
// 作 texture_depth_2d，全屏 quad 经 textureLoad 3×3 PCF 采样比较（receiverDepth=0.6）→ 中心受遮挡为暗、
// 四角受光为亮 → copy 颜色 RT 回读校验。证明「阴影 pass 写 atlas → 前向 pass 采样」跨 pass 深度图采样
// 能力（旧注释担心的 Dawn 屏障冲突在分趟下不存在），逻辑同 forward_shaded.frag 的 DirectionalShadow。
bool WebGpuSelfTestHarness::RecordCSMShadowSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // shadow atlas RT（颜色 RGBA16Float 占位 + Depth32 深度附件，深度可作 texture_depth_2d 采样）。
    if (!t51_shadow_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT51AtlasDim);
        d.height = static_cast<int>(kT51AtlasDim);
        d.has_color = true;
        d.has_depth = true;
        t51_shadow_rt_ = dev_->CreateRenderTarget(d);
    }
    // 离屏 color RT（RGBA16Float + CopySrc）。
    if (!t51_color_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT51RtSize);
        d.height = static_cast<int>(kT51RtSize);
        d.has_color = true;
        d.has_depth = false;
        t51_color_rt_ = dev_->CreateRenderTarget(d);
    }
    // 遮挡程序（写深度，pos.xyz@loc0）+ PSO（depth test/write on）。
    if (!t51_occ_program_) {
        static const char* kOccWGSL = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec3<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 1.0); return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> { return vec4<f32>(0.0, 0.0, 0.0, 1.0); }
)WGSL";
        t51_occ_program_ = dev_->CreateShaderProgram(kOccWGSL, "");
    }
    if (!t51_occ_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = true;
        d.depth_write_enabled = true;
        d.depth_func = CompareFunc::Less;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t51_occ_pso_ = dev_->CreatePipelineState(d);
    }
    // 前向接收程序（采样 atlas，pos.xy@loc0 + uv@loc1）+ PSO（无深度/无剔除/blend off）。
    if (!t51_recv_program_) {
        static const char* kRecvWGSL = R"WGSL(// dse-wgsl
@group(2) @binding(22) var shadow_atlas : texture_depth_2d;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = uv; return o;
}
fn SampleShadowPCF(uv : vec2<f32>, ref_depth : f32) -> f32 {
  let dims = vec2<i32>(textureDimensions(shadow_atlas, 0));
  let base = vec2<i32>(uv * vec2<f32>(dims));
  var lit = 0.0;
  for (var x = -1; x <= 1; x = x + 1) {
    for (var y = -1; y <= 1; y = y + 1) {
      let c = clamp(base + vec2<i32>(x, y), vec2<i32>(0, 0), dims - vec2<i32>(1, 1));
      let d = textureLoad(shadow_atlas, c, 0);
      lit = lit + select(0.0, 1.0, ref_depth <= d);
    }
  }
  return lit / 9.0;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let receiver_depth = 0.6;
  let bias = 0.01;
  let lit = SampleShadowPCF(i.uv, receiver_depth - bias);
  let shadow = clamp(1.0 - lit, 0.0, 1.0);
  let c = mix(1.0, 0.1, shadow);
  return vec4<f32>(c, c, c, 1.0);
}
)WGSL";
        t51_recv_program_ = dev_->CreateShaderProgram(kRecvWGSL, "");
    }
    if (!t51_recv_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t51_recv_pso_ = dev_->CreatePipelineState(d);
    }
    // 遮挡 quad：NDC 中心 [-0.5,0.5]²、z=0.3（pos.xyz，stride 12）。
    if (!t51_occ_vbo_) {
        const float occ[4 * 3] = {
            -0.5f, -0.5f, 0.3f,   0.5f, -0.5f, 0.3f,
             0.5f,  0.5f, 0.3f,  -0.5f,  0.5f, 0.3f,
        };
        t51_occ_vbo_ = dev_->CreateBuffer(sizeof(occ), occ, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t51_occ_ibo_ = dev_->CreateBuffer(sizeof(idx), idx, false, true);
    }
    // 全屏 quad：NDC [-1,1]²、uv [0,1]²（pos.xy + uv，stride 16）。
    if (!t51_recv_vbo_) {
        const float fsq[4 * 4] = {
            -1.0f, -1.0f, 0.0f, 0.0f,   1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,  -1.0f,  1.0f, 0.0f, 1.0f,
        };
        t51_recv_vbo_ = dev_->CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t51_recv_ibo_ = dev_->CreateBuffer(sizeof(idx), idx, false, true);
    }
    if (!t51_shadow_rt_ || !t51_color_rt_ || !t51_occ_program_ || !t51_occ_pso_ ||
        !t51_recv_program_ || !t51_recv_pso_ || !t51_occ_vbo_ || !t51_occ_ibo_ ||
        !t51_recv_vbo_ || !t51_recv_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[T5-1] CSM shadow 自检资源创建失败，跳过");
        return false;
    }

    // ①阴影深度趟：把中心遮挡 quad 渲入 shadow atlas（清深=1.0、占用区写 0.3）。
    {
        RenderPassDesc rp;
        rp.render_target = t51_shadow_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        dev_->CmdBeginRenderPass(rp);
        if (!dev_->cur_pass_) return false;
        dev_->CmdSetViewport(0, 0, static_cast<int>(kT51AtlasDim), static_cast<int>(kT51AtlasDim));
        dev_->CmdBindPipeline(dev_->GetGraphicsPipeline(t51_occ_pso_, t51_occ_program_));
        const std::vector<VertexAttr> occ_attrs = { VertexAttr{0, 3, 0} };  // pos.xyz
        dev_->CmdBindVertexBuffer(0, t51_occ_vbo_, 12, occ_attrs, VertexInputRate::PerVertex);
        dev_->CmdBindIndexBuffer(t51_occ_ibo_, IndexType::UInt32);
        dev_->CmdDrawIndexed(6, 0, 0);
        dev_->CmdEndRenderPass();
    }

    // ②前向采样趟：把 shadow atlas 深度纹理绑到 slot11，全屏 quad 经 textureLoad PCF 采样判阴影。
    const unsigned int atlas_depth = dev_->GetRenderTargetDepthTexture(t51_shadow_rt_);
    if (!atlas_depth) {
        DEBUG_LOG_ERROR("WebGPU[T5-1] 取 shadow atlas 深度纹理失败，跳过");
        return false;
    }
    {
        RenderPassDesc rp;
        rp.render_target = t51_color_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        dev_->CmdBeginRenderPass(rp);
        if (!dev_->cur_pass_) return false;
        dev_->CmdSetViewport(0, 0, static_cast<int>(kT51RtSize), static_cast<int>(kT51RtSize));
        dev_->CmdBindPipeline(dev_->GetGraphicsPipeline(t51_recv_pso_, t51_recv_program_));
        const std::vector<VertexAttr> recv_attrs = { VertexAttr{0, 2, 0}, VertexAttr{1, 2, 8} };  // pos.xy + uv
        dev_->CmdBindVertexBuffer(0, t51_recv_vbo_, 16, recv_attrs, VertexInputRate::PerVertex);
        dev_->CmdBindIndexBuffer(t51_recv_ibo_, IndexType::UInt32);
        dev_->CmdBindTexture(11u, atlas_depth, TextureDim::Tex2D);  // → group2 binding22（texture_depth_2d）
        dev_->CmdDrawIndexed(6, 0, 0);
        dev_->CmdEndRenderPass();
    }

    // copy color RT → 回读缓冲（随帧提交）。
    const WebGPURhiDevice::RenderTargetEntry* rt = dev_->FindRenderTarget(t51_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const WebGPURhiDevice::TextureEntry* color = dev_->FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT51RtBytes);
    t51_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!t51_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = color->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = t51_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kT51RtRowBytes;
    dst.layout.rowsPerImage = kT51RtSize;
    WGPUExtent3D ext{kT51RtSize, kT51RtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickCSMShadowSelfTestReadback() {
    if (!t51_rb_pixels_) return;
    auto* ctx = new CSMShadowSelfTestCtx();
    ctx->rb_pixels = t51_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t51_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT51RtBytes, OnT51PixelsMapped, ctx);
}

// Task 5 Subtask 2：延迟着色真链路自检。①几何趟把中心 quad 渲入 64×64×3 MRT gbuffer（albedo/normal/
// position 一趟三附件）；②全屏光照趟把 3 张 gbuffer 纹理（GetRenderTargetColorTexture 0/1/2）绑到 group2
// slot0/1/2，按 @builtin(position) 整数坐标 textureLoad 做延迟光照（NdotL·albedo + ambient，法线长度<阈
// 视为空像素）→ 中心几何受光为红、四角空像素为黑 → copy 颜色 RT 回读校验。证明「几何趟写 MRT gbuffer →
// 光照趟采样 gbuffer」延迟着色能力，逻辑同 deferred_lighting.frag。
bool WebGpuSelfTestHarness::RecordDeferredSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // gbuffer RT（3 个 RGBA16Float 颜色附件 albedo/normal/position，无深度；附件均 TextureBinding 可采样）。
    if (!t52_gbuffer_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT52RtSize);
        d.height = static_cast<int>(kT52RtSize);
        d.has_color = true;
        d.has_depth = false;
        d.color_attachment_count = 3;
        t52_gbuffer_rt_ = dev_->CreateRenderTarget(d);
    }
    // 离屏 color RT（RGBA16Float + CopySrc）。
    if (!t52_color_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT52RtSize);
        d.height = static_cast<int>(kT52RtSize);
        d.has_color = true;
        d.has_depth = false;
        t52_color_rt_ = dev_->CreateRenderTarget(d);
    }
    // 几何程序（写 MRT 3 附件，pos.xy@loc0）+ PSO（无深度/无剔除/blend off）。
    if (!t52_geom_program_) {
        static const char* kGeomWGSL = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); return o;
}
struct GBuf {
  @location(0) albedo : vec4<f32>,
  @location(1) normal : vec4<f32>,
  @location(2) position : vec4<f32>,
};
@fragment fn fs_main(i : VsOut) -> GBuf {
  var o : GBuf;
  o.albedo   = vec4<f32>(0.8, 0.2, 0.2, 1.0);
  o.normal   = vec4<f32>(0.5, 0.5, 1.0, 1.0);  // 编码法线(0,0,1)：*0.5+0.5
  o.position = vec4<f32>(0.0, 0.0, 0.0, 1.0);
  return o;
}
)WGSL";
        t52_geom_program_ = dev_->CreateShaderProgram(kGeomWGSL, "");
    }
    if (!t52_geom_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t52_geom_pso_ = dev_->CreatePipelineState(d);
    }
    // 延迟光照程序（textureLoad 3 张 gbuffer，pos.xy@loc0）+ PSO（无深度/无剔除/blend off）。
    if (!t52_light_program_) {
        static const char* kLightWGSL = R"WGSL(// dse-wgsl
@group(2) @binding(0) var g_albedo   : texture_2d<f32>;
@group(2) @binding(2) var g_normal   : texture_2d<f32>;
@group(2) @binding(4) var g_position : texture_2d<f32>;
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); return o;
}
@fragment fn fs_main(@builtin(position) frag : vec4<f32>) -> @location(0) vec4<f32> {
  let c = vec2<i32>(i32(frag.x), i32(frag.y));
  let albedo = textureLoad(g_albedo, c, 0).rgb;
  let nraw   = textureLoad(g_normal, c, 0).rgb;
  let nv     = nraw * 2.0 - vec3<f32>(1.0, 1.0, 1.0);
  if (length(nv) < 0.01) { return vec4<f32>(0.0, 0.0, 0.0, 1.0); }  // 空像素（gbuffer 清 0）→ 黑
  let N = normalize(nv);
  let L = normalize(-vec3<f32>(0.0, 0.0, -1.0));  // 方向光朝 -z → 受光方向 +z
  let ndl = max(dot(N, L), 0.0);
  let light_color = vec3<f32>(1.0, 1.0, 1.0);
  let diffuse = albedo * light_color * 1.0 * ndl;
  let ambient = albedo * 0.2;
  return vec4<f32>(diffuse + ambient, 1.0);
}
)WGSL";
        t52_light_program_ = dev_->CreateShaderProgram(kLightWGSL, "");
    }
    if (!t52_light_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t52_light_pso_ = dev_->CreatePipelineState(d);
    }
    // 几何 quad：NDC 中心 [-0.6,0.6]²（pos.xy，stride 8）。
    if (!t52_geom_vbo_) {
        const float geo[4 * 2] = {
            -0.6f, -0.6f,   0.6f, -0.6f,
             0.6f,  0.6f,  -0.6f,  0.6f,
        };
        t52_geom_vbo_ = dev_->CreateBuffer(sizeof(geo), geo, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t52_geom_ibo_ = dev_->CreateBuffer(sizeof(idx), idx, false, true);
    }
    // 全屏 quad：NDC [-1,1]²（pos.xy，stride 8；光照趟按 @builtin(position) 取坐标，无需 uv）。
    if (!t52_light_vbo_) {
        const float fsq[4 * 2] = {
            -1.0f, -1.0f,   1.0f, -1.0f,
             1.0f,  1.0f,  -1.0f,  1.0f,
        };
        t52_light_vbo_ = dev_->CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t52_light_ibo_ = dev_->CreateBuffer(sizeof(idx), idx, false, true);
    }
    if (!t52_gbuffer_rt_ || !t52_color_rt_ || !t52_geom_program_ || !t52_geom_pso_ ||
        !t52_light_program_ || !t52_light_pso_ || !t52_geom_vbo_ || !t52_geom_ibo_ ||
        !t52_light_vbo_ || !t52_light_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[T5-2] 延迟着色自检资源创建失败，跳过");
        return false;
    }

    // ①几何趟：把中心 quad 渲入 MRT gbuffer（3 附件均清 0 → 空区 normal 长度 0；占用区写 albedo/normal/position）。
    {
        RenderPassDesc rp;
        rp.render_target = t52_gbuffer_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        dev_->CmdBeginRenderPass(rp);
        if (!dev_->cur_pass_) return false;
        dev_->CmdSetViewport(0, 0, static_cast<int>(kT52RtSize), static_cast<int>(kT52RtSize));
        dev_->CmdBindPipeline(dev_->GetGraphicsPipeline(t52_geom_pso_, t52_geom_program_));
        const std::vector<VertexAttr> geo_attrs = { VertexAttr{0, 2, 0} };  // pos.xy
        dev_->CmdBindVertexBuffer(0, t52_geom_vbo_, 8, geo_attrs, VertexInputRate::PerVertex);
        dev_->CmdBindIndexBuffer(t52_geom_ibo_, IndexType::UInt32);
        dev_->CmdDrawIndexed(6, 0, 0);
        dev_->CmdEndRenderPass();
    }

    // ②光照趟：把 3 张 gbuffer 颜色纹理绑到 slot0/1/2，全屏 quad 经 textureLoad 做延迟光照。
    const unsigned int g_albedo   = dev_->GetRenderTargetColorTexture(t52_gbuffer_rt_, 0);
    const unsigned int g_normal   = dev_->GetRenderTargetColorTexture(t52_gbuffer_rt_, 1);
    const unsigned int g_position = dev_->GetRenderTargetColorTexture(t52_gbuffer_rt_, 2);
    if (!g_albedo || !g_normal || !g_position) {
        DEBUG_LOG_ERROR("WebGPU[T5-2] 取 gbuffer 颜色纹理失败，跳过");
        return false;
    }
    {
        RenderPassDesc rp;
        rp.render_target = t52_color_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        dev_->CmdBeginRenderPass(rp);
        if (!dev_->cur_pass_) return false;
        dev_->CmdSetViewport(0, 0, static_cast<int>(kT52RtSize), static_cast<int>(kT52RtSize));
        dev_->CmdBindPipeline(dev_->GetGraphicsPipeline(t52_light_pso_, t52_light_program_));
        const std::vector<VertexAttr> light_attrs = { VertexAttr{0, 2, 0} };  // pos.xy
        dev_->CmdBindVertexBuffer(0, t52_light_vbo_, 8, light_attrs, VertexInputRate::PerVertex);
        dev_->CmdBindIndexBuffer(t52_light_ibo_, IndexType::UInt32);
        dev_->CmdBindTexture(0u, g_albedo,   TextureDim::Tex2D);  // → group2 binding0
        dev_->CmdBindTexture(1u, g_normal,   TextureDim::Tex2D);  // → group2 binding2
        dev_->CmdBindTexture(2u, g_position, TextureDim::Tex2D);  // → group2 binding4
        dev_->CmdDrawIndexed(6, 0, 0);
        dev_->CmdEndRenderPass();
    }

    // copy color RT → 回读缓冲（随帧提交）。
    const WebGPURhiDevice::RenderTargetEntry* rt = dev_->FindRenderTarget(t52_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const WebGPURhiDevice::TextureEntry* color = dev_->FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT52RtBytes);
    t52_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!t52_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = color->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = t52_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kT52RtRowBytes;
    dst.layout.rowsPerImage = kT52RtSize;
    WGPUExtent3D ext{kT52RtSize, kT52RtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickDeferredSelfTestReadback() {
    if (!t52_rb_pixels_) return;
    auto* ctx = new DeferredSelfTestCtx();
    ctx->rb_pixels = t52_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t52_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT52RtBytes, OnT52PixelsMapped, ctx);
}

// ============================================================================
// Task 5 Subtask 3（T5-3）：HDR auto-exposure 亮度归约 + ACES tonemap parity 离屏自检。
// ============================================================================
bool WebGpuSelfTestHarness::RecordHDRSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // RT：HDR 场景（8×8）、平均 log 亮度（1×1）、自动曝光（1×1）、tonemap color（64×64，CopySrc）。
    auto make_rt = [&](unsigned int& rt, uint32_t dim) {
        if (rt) return;
        RenderTargetDesc d;
        d.width = static_cast<int>(dim);
        d.height = static_cast<int>(dim);
        d.has_color = true;
        d.has_depth = false;
        rt = dev_->CreateRenderTarget(d);
    };
    make_rt(t53_scene_rt_, kT53SceneDim);
    make_rt(t53_lum_rt_, 1);
    make_rt(t53_exposure_rt_, 1);
    make_rt(t53_color_rt_, kT53RtSize);

    // 共享 PSO（无深度/无剔除/blend off）。
    if (!t53_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t53_pso_ = dev_->CreatePipelineState(d);
    }
    // ①HDR 场景程序：输出常量 (4,2,1)。
    if (!t53_scene_program_) {
        static const char* kSceneWGSL = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return vec4<f32>(4.0, 2.0, 1.0, 1.0);
}
)WGSL";
        t53_scene_program_ = dev_->CreateShaderProgram(kSceneWGSL, "");
    }
    // ②亮度归约程序：textureLoad 8×8 采样点算平均 log 亮度（逻辑同 lum_compute.frag）。
    if (!t53_reduce_program_) {
        static const char* kReduceWGSL = R"WGSL(// dse-wgsl
@group(2) @binding(0) var scene_tex : texture_2d<f32>;
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let dims = vec2<i32>(textureDimensions(scene_tex, 0));
  var logSum = 0.0;
  for (var x = 0; x < 8; x = x + 1) {
    for (var y = 0; y < 8; y = y + 1) {
      let uv = (vec2<f32>(f32(x), f32(y)) + vec2<f32>(0.5, 0.5)) / 8.0;
      let c = vec2<i32>(uv * vec2<f32>(dims));
      let rgb = textureLoad(scene_tex, c, 0).rgb;
      let lum = dot(rgb, vec3<f32>(0.2126, 0.7152, 0.0722));
      logSum = logSum + log(max(lum, 0.0001));
    }
  }
  let avgLogLum = logSum / 64.0;
  return vec4<f32>(avgLogLum, 0.0, 0.0, 1.0);
}
)WGSL";
        t53_reduce_program_ = dev_->CreateShaderProgram(kReduceWGSL, "");
    }
    // ③lum_adapt 程序：avgLum=exp(avgLogLum)→0.18/avgLum 曝光→clamp（逻辑同 lum_adapt.frag，补偿=0）。
    if (!t53_adapt_program_) {
        static const char* kAdaptWGSL = R"WGSL(// dse-wgsl
@group(2) @binding(0) var lum_tex : texture_2d<f32>;
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let avgLogLum = textureLoad(lum_tex, vec2<i32>(0, 0), 0).r;
  let avgLum = exp(avgLogLum);
  var targetExposure = 0.18 / max(avgLum, 0.001);
  targetExposure = clamp(targetExposure, 0.01, 10.0);
  return vec4<f32>(targetExposure, 0.0, 0.0, 1.0);
}
)WGSL";
        t53_adapt_program_ = dev_->CreateShaderProgram(kAdaptWGSL, "");
    }
    // ④tonemap 程序：ACES(hdr*exposure)+gamma(1/2.2)（逻辑同 tonemapping.frag）。
    if (!t53_tonemap_program_) {
        static const char* kTonemapWGSL = R"WGSL(// dse-wgsl
@group(2) @binding(0) var scene_tex : texture_2d<f32>;
@group(2) @binding(2) var exposure_tex : texture_2d<f32>;
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); return o;
}
fn AcesFilmic(x : vec3<f32>) -> vec3<f32> {
  let a = 2.51; let b = 0.03; let c = 2.43; let d = 0.59; let e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3<f32>(0.0), vec3<f32>(1.0));
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let hdr = textureLoad(scene_tex, vec2<i32>(0, 0), 0).rgb;
  let exposure = textureLoad(exposure_tex, vec2<i32>(0, 0), 0).r;
  var result = AcesFilmic(hdr * exposure);
  result = pow(result, vec3<f32>(1.0 / 2.2));
  return vec4<f32>(result, 1.0);
}
)WGSL";
        t53_tonemap_program_ = dev_->CreateShaderProgram(kTonemapWGSL, "");
    }
    // 全屏 quad（pos.xy，stride 8；各趟按 textureLoad 固定坐标取值，无需 uv）。
    if (!t53_quad_vbo_) {
        const float fsq[4 * 2] = {
            -1.0f, -1.0f,   1.0f, -1.0f,
             1.0f,  1.0f,  -1.0f,  1.0f,
        };
        t53_quad_vbo_ = dev_->CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t53_quad_ibo_ = dev_->CreateBuffer(sizeof(idx), idx, false, true);
    }
    if (!t53_scene_rt_ || !t53_lum_rt_ || !t53_exposure_rt_ || !t53_color_rt_ || !t53_pso_ ||
        !t53_scene_program_ || !t53_reduce_program_ || !t53_adapt_program_ || !t53_tonemap_program_ ||
        !t53_quad_vbo_ || !t53_quad_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[T5-3] HDR 自检资源创建失败，跳过");
        return false;
    }

    const std::vector<VertexAttr> attrs = { VertexAttr{0, 2, 0} };  // pos.xy
    auto draw_quad = [&](unsigned int rt, uint32_t dim, unsigned int program,
                         const glm::vec4& clear) -> bool {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color_enabled = true;
        rp.clear_color = clear;
        dev_->CmdBeginRenderPass(rp);
        if (!dev_->cur_pass_) return false;
        dev_->CmdSetViewport(0, 0, static_cast<int>(dim), static_cast<int>(dim));
        dev_->CmdBindPipeline(dev_->GetGraphicsPipeline(t53_pso_, program));
        dev_->CmdBindVertexBuffer(0, t53_quad_vbo_, 8, attrs, VertexInputRate::PerVertex);
        dev_->CmdBindIndexBuffer(t53_quad_ibo_, IndexType::UInt32);
        return true;
    };

    // ①场景趟：渲已知 HDR (4,2,1) 到 8×8 场景 RT。
    if (!draw_quad(t53_scene_rt_, kT53SceneDim, t53_scene_program_, glm::vec4(0.0f))) return false;
    dev_->CmdDrawIndexed(6, 0, 0);
    dev_->CmdEndRenderPass();

    // ②归约趟：绑场景纹理 slot0，算平均 log 亮度写 1×1 lum RT。
    const unsigned int scene_tex = dev_->GetRenderTargetColorTexture(t53_scene_rt_, 0);
    if (!scene_tex) { DEBUG_LOG_ERROR("WebGPU[T5-3] 取场景纹理失败，跳过"); return false; }
    if (!draw_quad(t53_lum_rt_, 1, t53_reduce_program_, glm::vec4(0.0f))) return false;
    dev_->CmdBindTexture(0u, scene_tex, TextureDim::Tex2D);  // → group2 binding0
    dev_->CmdDrawIndexed(6, 0, 0);
    dev_->CmdEndRenderPass();

    // ③lum_adapt 趟：绑 lum 纹理 slot0，算曝光写 1×1 exposure RT。
    const unsigned int lum_tex = dev_->GetRenderTargetColorTexture(t53_lum_rt_, 0);
    if (!lum_tex) { DEBUG_LOG_ERROR("WebGPU[T5-3] 取亮度纹理失败，跳过"); return false; }
    if (!draw_quad(t53_exposure_rt_, 1, t53_adapt_program_, glm::vec4(0.0f))) return false;
    dev_->CmdBindTexture(0u, lum_tex, TextureDim::Tex2D);  // → group2 binding0
    dev_->CmdDrawIndexed(6, 0, 0);
    dev_->CmdEndRenderPass();

    // ④tonemap 趟：绑场景纹理 slot0 + 曝光纹理 slot1，ACES+gamma 渲到 64×64 color RT。
    const unsigned int exposure_tex = dev_->GetRenderTargetColorTexture(t53_exposure_rt_, 0);
    if (!exposure_tex) { DEBUG_LOG_ERROR("WebGPU[T5-3] 取曝光纹理失败，跳过"); return false; }
    if (!draw_quad(t53_color_rt_, kT53RtSize, t53_tonemap_program_, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f))) return false;
    dev_->CmdBindTexture(0u, scene_tex, TextureDim::Tex2D);     // → group2 binding0
    dev_->CmdBindTexture(1u, exposure_tex, TextureDim::Tex2D);  // → group2 binding2
    dev_->CmdDrawIndexed(6, 0, 0);
    dev_->CmdEndRenderPass();

    // copy color RT → 回读缓冲（随帧提交）。
    const WebGPURhiDevice::RenderTargetEntry* rt = dev_->FindRenderTarget(t53_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const WebGPURhiDevice::TextureEntry* color = dev_->FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT53RtBytes);
    t53_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!t53_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = color->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = t53_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kT53RtRowBytes;
    dst.layout.rowsPerImage = kT53RtSize;
    WGPUExtent3D ext{kT53RtSize, kT53RtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickHDRSelfTestReadback() {
    if (!t53_rb_pixels_) return;
    auto* ctx = new HDRSelfTestCtx();
    ctx->rb_pixels = t53_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t53_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT53RtBytes, OnT53PixelsMapped, ctx);
}

// ============================================================================
// Task 5 Subtask 4（T5-4）：IBL（BRDF LUT + irradiance + prefilter env + PBR 环境项）离屏自检。
// ============================================================================
bool WebGpuSelfTestHarness::RecordIBLSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // RT：BRDF LUT（64×64）、irradiance（1×1）、prefilter（1×1）、PBR color（64×64，CopySrc）。
    auto make_rt = [&](unsigned int& rt, uint32_t dim) {
        if (rt) return;
        RenderTargetDesc d;
        d.width = static_cast<int>(dim);
        d.height = static_cast<int>(dim);
        d.has_color = true;
        d.has_depth = false;
        rt = dev_->CreateRenderTarget(d);
    };
    make_rt(t54_brdf_rt_, kT54LutDim);
    make_rt(t54_irr_rt_, 1);
    make_rt(t54_pref_rt_, 1);
    make_rt(t54_color_rt_, kT54RtSize);

    if (!t54_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t54_pso_ = dev_->CreatePipelineState(d);
    }
    // ①BRDF LUT 程序：GGX split-sum 积分（256 采样 Hammersley/ImportanceSampleGGX/Smith，uv.x=NdotV、
    //   uv.y=roughness）。与 C++ T5IntegrateBRDF 逐行同算法。
    if (!t54_brdf_program_) {
        static const char* kBrdfWGSL = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = uv; return o;
}
const PI : f32 = 3.14159265358979;
fn RadicalInverse_VdC(bitsIn : u32) -> f32 {
  var bits = bitsIn;
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return f32(bits) * 2.3283064365386963e-10;
}
fn Hammersley(i : u32, n : u32) -> vec2<f32> {
  return vec2<f32>(f32(i) / f32(n), RadicalInverse_VdC(i));
}
fn ImportanceSampleGGX(Xi : vec2<f32>, roughness : f32) -> vec3<f32> {
  let a = roughness * roughness;
  let phi = 2.0 * PI * Xi.x;
  let cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
  let sinTheta = sqrt(1.0 - cosTheta * cosTheta);
  return vec3<f32>(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}
fn GeometrySmith(NdotV : f32, NdotL : f32, roughness : f32) -> f32 {
  let k = (roughness * roughness) / 2.0;
  let gV = NdotV / (NdotV * (1.0 - k) + k);
  let gL = NdotL / (NdotL * (1.0 - k) + k);
  return gV * gL;
}
fn IntegrateBRDF(NdotV : f32, roughness : f32) -> vec2<f32> {
  let V = vec3<f32>(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
  var A = 0.0;
  var B = 0.0;
  let N = 256u;
  for (var i = 0u; i < N; i = i + 1u) {
    let Xi = Hammersley(i, N);
    let H = ImportanceSampleGGX(Xi, roughness);
    let VdotH = dot(V, H);
    let L = 2.0 * VdotH * H - V;
    let NdotL = max(L.z, 0.0);
    let NdotH = max(H.z, 0.0);
    let vh = max(VdotH, 0.0);
    if (NdotL > 0.0) {
      let G = GeometrySmith(NdotV, NdotL, roughness);
      let G_Vis = (G * vh) / (NdotH * NdotV);
      let Fc = pow(1.0 - vh, 5.0);
      A = A + (1.0 - Fc) * G_Vis;
      B = B + Fc * G_Vis;
    }
  }
  return vec2<f32>(A / f32(N), B / f32(N));
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let res = IntegrateBRDF(max(i.uv.x, 0.001), i.uv.y);
  return vec4<f32>(res.x, res.y, 0.0, 1.0);
}
)WGSL";
        t54_brdf_program_ = dev_->CreateShaderProgram(kBrdfWGSL, "");
    }
    // ②irradiance 程序：输出常量辐照度（常量环境半球辐照积分即常量）。
    if (!t54_irr_program_) {
        static const char* kIrrWGSL = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = uv; return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return vec4<f32>(0.4, 0.5, 0.6, 1.0);
}
)WGSL";
        t54_irr_program_ = dev_->CreateShaderProgram(kIrrWGSL, "");
    }
    // ③prefilter 程序：输出常量预滤波镜面色。
    if (!t54_pref_program_) {
        static const char* kPrefWGSL = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = uv; return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return vec4<f32>(0.5, 0.5, 0.5, 1.0);
}
)WGSL";
        t54_pref_program_ = dev_->CreateShaderProgram(kPrefWGSL, "");
    }
    // ④PBR 环境项程序：绑 LUT/irr/pref 三纹理，split-sum 合成 ambient = kD*irr*albedo + pref*(F*brdf.x+brdf.y)。
    if (!t54_pbr_program_) {
        static const char* kPbrWGSL = R"WGSL(// dse-wgsl
@group(2) @binding(0) var brdf_tex : texture_2d<f32>;
@group(2) @binding(2) var irr_tex  : texture_2d<f32>;
@group(2) @binding(4) var pref_tex : texture_2d<f32>;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); o.uv = uv; return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let albedo = vec3<f32>(0.8, 0.2, 0.2);
  let metallic = 0.0;
  let roughness = 0.4921875;  // = (31.5)/64，与采样的 LUT texel 行对应
  let NdotV = 1.0;
  let F0 = vec3<f32>(0.04, 0.04, 0.04);
  let Fr = F0 + (max(vec3<f32>(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
  let kS = Fr;
  let kD = (vec3<f32>(1.0, 1.0, 1.0) - kS) * (1.0 - metallic);
  let irradiance = textureLoad(irr_tex, vec2<i32>(0, 0), 0).rgb;
  let diffuse = irradiance * albedo;
  let prefiltered = textureLoad(pref_tex, vec2<i32>(0, 0), 0).rgb;
  let brdf = textureLoad(brdf_tex, vec2<i32>(63, 31), 0).rg;
  let specular = prefiltered * (Fr * brdf.x + vec3<f32>(brdf.y, brdf.y, brdf.y));
  let ambient = kD * diffuse + specular;
  return vec4<f32>(ambient, 1.0);
}
)WGSL";
        t54_pbr_program_ = dev_->CreateShaderProgram(kPbrWGSL, "");
    }
    // 全屏 quad（pos.xy + uv，stride 16；BRDF LUT 趟用 uv，其余趟接收但不读）。
    if (!t54_quad_vbo_) {
        const float fsq[4 * 4] = {
            -1.0f, -1.0f, 0.0f, 0.0f,   1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,  -1.0f,  1.0f, 0.0f, 1.0f,
        };
        t54_quad_vbo_ = dev_->CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t54_quad_ibo_ = dev_->CreateBuffer(sizeof(idx), idx, false, true);
    }
    if (!t54_brdf_rt_ || !t54_irr_rt_ || !t54_pref_rt_ || !t54_color_rt_ || !t54_pso_ ||
        !t54_brdf_program_ || !t54_irr_program_ || !t54_pref_program_ || !t54_pbr_program_ ||
        !t54_quad_vbo_ || !t54_quad_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[T5-4] IBL 自检资源创建失败，跳过");
        return false;
    }

    const std::vector<VertexAttr> attrs = { VertexAttr{0, 2, 0}, VertexAttr{1, 2, 8} };  // pos.xy + uv
    auto begin_quad = [&](unsigned int rt, uint32_t dim, unsigned int program) -> bool {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        dev_->CmdBeginRenderPass(rp);
        if (!dev_->cur_pass_) return false;
        dev_->CmdSetViewport(0, 0, static_cast<int>(dim), static_cast<int>(dim));
        dev_->CmdBindPipeline(dev_->GetGraphicsPipeline(t54_pso_, program));
        dev_->CmdBindVertexBuffer(0, t54_quad_vbo_, 16, attrs, VertexInputRate::PerVertex);
        dev_->CmdBindIndexBuffer(t54_quad_ibo_, IndexType::UInt32);
        return true;
    };

    // ①BRDF LUT 趟。
    if (!begin_quad(t54_brdf_rt_, kT54LutDim, t54_brdf_program_)) return false;
    dev_->CmdDrawIndexed(6, 0, 0);
    dev_->CmdEndRenderPass();
    // ②irradiance 趟。
    if (!begin_quad(t54_irr_rt_, 1, t54_irr_program_)) return false;
    dev_->CmdDrawIndexed(6, 0, 0);
    dev_->CmdEndRenderPass();
    // ③prefilter 趟。
    if (!begin_quad(t54_pref_rt_, 1, t54_pref_program_)) return false;
    dev_->CmdDrawIndexed(6, 0, 0);
    dev_->CmdEndRenderPass();

    // ④PBR 环境项趟：绑 LUT/irr/pref 三纹理，split-sum 合成渲到 64×64 color RT。
    const unsigned int brdf_tex = dev_->GetRenderTargetColorTexture(t54_brdf_rt_, 0);
    const unsigned int irr_tex  = dev_->GetRenderTargetColorTexture(t54_irr_rt_, 0);
    const unsigned int pref_tex = dev_->GetRenderTargetColorTexture(t54_pref_rt_, 0);
    if (!brdf_tex || !irr_tex || !pref_tex) {
        DEBUG_LOG_ERROR("WebGPU[T5-4] 取 LUT/irr/pref 纹理失败，跳过");
        return false;
    }
    if (!begin_quad(t54_color_rt_, kT54RtSize, t54_pbr_program_)) return false;
    dev_->CmdBindTexture(0u, brdf_tex, TextureDim::Tex2D);  // → group2 binding0
    dev_->CmdBindTexture(1u, irr_tex,  TextureDim::Tex2D);  // → group2 binding2
    dev_->CmdBindTexture(2u, pref_tex, TextureDim::Tex2D);  // → group2 binding4
    dev_->CmdDrawIndexed(6, 0, 0);
    dev_->CmdEndRenderPass();

    // copy color RT → 回读缓冲（随帧提交）。
    const WebGPURhiDevice::RenderTargetEntry* rt = dev_->FindRenderTarget(t54_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const WebGPURhiDevice::TextureEntry* color = dev_->FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT54RtBytes);
    t54_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!t54_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = color->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = t54_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kT54RtRowBytes;
    dst.layout.rowsPerImage = kT54RtSize;
    WGPUExtent3D ext{kT54RtSize, kT54RtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickIBLSelfTestReadback() {
    if (!t54_rb_pixels_) return;
    auto* ctx = new IBLSelfTestCtx();
    ctx->rb_pixels = t54_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t54_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT54RtBytes, OnT54PixelsMapped, ctx);
}

// ============================================================================
// Task 5 Subtask 5（T5-5）：WBOIT（accum/reveal MRT + resolve）离屏自检。
// ============================================================================
bool WebGpuSelfTestHarness::RecordWBOITSelfTest() {
    if (!dev_->device_ || !dev_->frame_encoder_ || dev_->cur_pass_ || dev_->cur_compute_pass_) return false;

    // accum/reveal MRT（2 个 RGBA16Float 颜色附件，均 TextureBinding 可采样）。
    if (!t55_mrt_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT55RtSize);
        d.height = static_cast<int>(kT55RtSize);
        d.has_color = true;
        d.has_depth = false;
        d.color_attachment_count = 2;
        t55_mrt_rt_ = dev_->CreateRenderTarget(d);
    }
    // 离屏 color RT（RGBA16Float + CopySrc）。
    if (!t55_color_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT55RtSize);
        d.height = static_cast<int>(kT55RtSize);
        d.has_color = true;
        d.has_depth = false;
        t55_color_rt_ = dev_->CreateRenderTarget(d);
    }
    // 几何程序：两层半透明片元按 WBOIT 权重 shader 内解析累加，写 accum/reveal 2 附件 MRT。
    if (!t55_geom_program_) {
        static const char* kGeomWGSL = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); return o;
}
struct MRT {
  @location(0) accum : vec4<f32>,
  @location(1) reveal : vec4<f32>,
};
fn weightFn(z : f32) -> f32 {
  return max(0.01, 3.0 * (1.0 - z));
}
@fragment fn fs_main(i : VsOut) -> MRT {
  let c0 = vec3<f32>(1.0, 0.0, 0.0); let a0 = 0.5; let z0 = 0.2;  // layer0 红
  let c1 = vec3<f32>(0.0, 0.0, 1.0); let a1 = 0.5; let z1 = 0.6;  // layer1 蓝
  let w0 = a0 * weightFn(z0);
  let w1 = a1 * weightFn(z1);
  var accum = vec4<f32>(0.0, 0.0, 0.0, 0.0);
  accum = accum + vec4<f32>(c0 * a0, a0) * w0;
  accum = accum + vec4<f32>(c1 * a1, a1) * w1;
  let reveal = (1.0 - a0) * (1.0 - a1);
  var o : MRT;
  o.accum = accum;
  o.reveal = vec4<f32>(reveal, 0.0, 0.0, 1.0);
  return o;
}
)WGSL";
        t55_geom_program_ = dev_->CreateShaderProgram(kGeomWGSL, "");
    }
    if (!t55_geom_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t55_geom_pso_ = dev_->CreatePipelineState(d);
    }
    // resolve 程序：绑 accum/reveal 两纹理，avgColor=accum.rgb/max(accum.a,eps)、1-reveal 合成 over 黑背景。
    if (!t55_resolve_program_) {
        static const char* kResolveWGSL = R"WGSL(// dse-wgsl
@group(2) @binding(0) var accum_tex  : texture_2d<f32>;
@group(2) @binding(2) var reveal_tex : texture_2d<f32>;
struct VsOut { @builtin(position) pos : vec4<f32>, };
@vertex fn vs_main(@location(0) p : vec2<f32>) -> VsOut {
  var o : VsOut; o.pos = vec4<f32>(p, 0.0, 1.0); return o;
}
@fragment fn fs_main(@builtin(position) frag : vec4<f32>) -> @location(0) vec4<f32> {
  let c = vec2<i32>(i32(frag.x), i32(frag.y));
  let accum = textureLoad(accum_tex, c, 0);
  let reveal = textureLoad(reveal_tex, c, 0).r;
  let avgColor = accum.rgb / max(accum.a, 1e-5);
  let finalAlpha = 1.0 - reveal;
  let outColor = avgColor * finalAlpha;  // over 黑背景
  return vec4<f32>(outColor, 1.0);
}
)WGSL";
        t55_resolve_program_ = dev_->CreateShaderProgram(kResolveWGSL, "");
    }
    if (!t55_resolve_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t55_resolve_pso_ = dev_->CreatePipelineState(d);
    }
    // 全屏 quad（pos.xy，stride 8）。
    if (!t55_quad_vbo_) {
        const float fsq[4 * 2] = {
            -1.0f, -1.0f,   1.0f, -1.0f,
             1.0f,  1.0f,  -1.0f,  1.0f,
        };
        t55_quad_vbo_ = dev_->CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t55_quad_ibo_ = dev_->CreateBuffer(sizeof(idx), idx, false, true);
    }
    if (!t55_mrt_rt_ || !t55_color_rt_ || !t55_geom_program_ || !t55_geom_pso_ ||
        !t55_resolve_program_ || !t55_resolve_pso_ || !t55_quad_vbo_ || !t55_quad_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[T5-5] WBOIT 自检资源创建失败，跳过");
        return false;
    }

    const std::vector<VertexAttr> attrs = { VertexAttr{0, 2, 0} };  // pos.xy

    // ①几何趟：全屏 quad 把两层 WBOIT 累加结果写 accum/reveal 2 附件 MRT。
    {
        RenderPassDesc rp;
        rp.render_target = t55_mrt_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        dev_->CmdBeginRenderPass(rp);
        if (!dev_->cur_pass_) return false;
        dev_->CmdSetViewport(0, 0, static_cast<int>(kT55RtSize), static_cast<int>(kT55RtSize));
        dev_->CmdBindPipeline(dev_->GetGraphicsPipeline(t55_geom_pso_, t55_geom_program_));
        dev_->CmdBindVertexBuffer(0, t55_quad_vbo_, 8, attrs, VertexInputRate::PerVertex);
        dev_->CmdBindIndexBuffer(t55_quad_ibo_, IndexType::UInt32);
        dev_->CmdDrawIndexed(6, 0, 0);
        dev_->CmdEndRenderPass();
    }

    // ②resolve 趟：绑 accum slot0 + reveal slot1，合成渲到 64×64 color RT。
    const unsigned int accum_tex  = dev_->GetRenderTargetColorTexture(t55_mrt_rt_, 0);
    const unsigned int reveal_tex = dev_->GetRenderTargetColorTexture(t55_mrt_rt_, 1);
    if (!accum_tex || !reveal_tex) {
        DEBUG_LOG_ERROR("WebGPU[T5-5] 取 accum/reveal 纹理失败，跳过");
        return false;
    }
    {
        RenderPassDesc rp;
        rp.render_target = t55_color_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        dev_->CmdBeginRenderPass(rp);
        if (!dev_->cur_pass_) return false;
        dev_->CmdSetViewport(0, 0, static_cast<int>(kT55RtSize), static_cast<int>(kT55RtSize));
        dev_->CmdBindPipeline(dev_->GetGraphicsPipeline(t55_resolve_pso_, t55_resolve_program_));
        dev_->CmdBindVertexBuffer(0, t55_quad_vbo_, 8, attrs, VertexInputRate::PerVertex);
        dev_->CmdBindIndexBuffer(t55_quad_ibo_, IndexType::UInt32);
        dev_->CmdBindTexture(0u, accum_tex,  TextureDim::Tex2D);  // → group2 binding0
        dev_->CmdBindTexture(1u, reveal_tex, TextureDim::Tex2D);  // → group2 binding2
        dev_->CmdDrawIndexed(6, 0, 0);
        dev_->CmdEndRenderPass();
    }

    // copy color RT → 回读缓冲（随帧提交）。
    const WebGPURhiDevice::RenderTargetEntry* rt = dev_->FindRenderTarget(t55_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const WebGPURhiDevice::TextureEntry* color = dev_->FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT55RtBytes);
    t55_rb_pixels_ = wgpuDeviceCreateBuffer(dev_->device_, &bd);
    if (!t55_rb_pixels_) return false;
    WGPUImageCopyTexture src{};
    src.texture = color->texture;
    src.mipLevel = 0;
    src.aspect = WGPUTextureAspect_All;
    WGPUImageCopyBuffer dst{};
    dst.buffer = t55_rb_pixels_;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = kT55RtRowBytes;
    dst.layout.rowsPerImage = kT55RtSize;
    WGPUExtent3D ext{kT55RtSize, kT55RtSize, 1};
    wgpuCommandEncoderCopyTextureToBuffer(dev_->frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGpuSelfTestHarness::KickWBOITSelfTestReadback() {
    if (!t55_rb_pixels_) return;
    auto* ctx = new WBOITSelfTestCtx();
    ctx->rb_pixels = t55_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t55_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT55RtBytes, OnT55PixelsMapped, ctx);
}

void WebGpuSelfTestHarness::EnsureSelfTestResources() {
    if (selftest_init_) return;
    selftest_init_ = true;  // 仅尝试一次（失败则后续 EndFrame 走 clear 兜底）

    static const char* kSelfTestWGSL = R"WGSL(// dse-wgsl
struct VsOut {
  @builtin(position) pos : vec4<f32>,
  @location(0) uv : vec2<f32>,
};

@vertex
fn vs_main(@location(0) in_pos : vec2<f32>, @location(1) in_uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(in_pos, 0.0, 1.0);
  o.uv = in_uv;
  return o;
}

struct Tint { color : vec4<f32>, };
@group(1) @binding(0) var<uniform> u_tint : Tint;
@group(2) @binding(0) var u_tex : texture_2d<f32>;
@group(2) @binding(1) var u_samp : sampler;

@fragment
fn fs_main(in : VsOut) -> @location(0) vec4<f32> {
  let tex = textureSample(u_tex, u_samp, in.uv);
  let grad = vec3<f32>(in.uv.x, in.uv.y, 1.0 - in.uv.x);
  return vec4<f32>(grad * tex.rgb * u_tint.color.rgb, 1.0);
}
)WGSL";
    selftest_program_ = dev_->CreateShaderProgram(kSelfTestWGSL, "");

    PipelineStateDesc d;
    d.blend_enabled = false;
    d.depth_test_enabled = false;
    d.depth_write_enabled = false;
    d.culling_enabled = false;
    d.cull_face = CullFace::None;
    d.topology = PrimitiveTopology::TriangleList;
    selftest_pso_ = dev_->CreatePipelineState(d);

    // 全屏 quad（两三角形，pos.xy + uv.xy，stride 16）。
    const float quad[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };
    selftest_vbo_ = dev_->CreateBuffer(sizeof(quad), quad, false, false);

    const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    selftest_ubo_ = dev_->CreateBuffer(sizeof(tint), tint, false, false);

    // 8×8 彩色棋盘（多色，保证 distinctColors 充足）。
    unsigned char checker[8 * 8 * 4];
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const int i = (y * 8 + x) * 4;
            const bool c = ((x + y) & 1) != 0;
            checker[i + 0] = static_cast<unsigned char>(x * 32);
            checker[i + 1] = static_cast<unsigned char>(y * 32);
            checker[i + 2] = c ? 255 : 40;
            checker[i + 3] = 255;
        }
    }
    selftest_tex_ = dev_->CreateTexture2D(8, 8, checker, false);
}

void WebGpuSelfTestHarness::RunBringUpSelfTest() {
    EnsureSelfTestResources();
    if (!selftest_program_ || !selftest_pso_ || !selftest_vbo_ || !selftest_ubo_ || !selftest_tex_) return;

    RenderPassDesc rp;
    rp.render_target = 0;
    rp.clear_color_enabled = true;
    rp.clear_color = glm::vec4(0.05f, 0.05f, 0.08f, 1.0f);
    dev_->CmdBeginRenderPass(rp);
    if (!dev_->cur_pass_) return;
    dev_->CmdSetViewport(0, 0, dev_->width_, dev_->height_);

    const unsigned int pipe = dev_->GetGraphicsPipeline(selftest_pso_, selftest_program_);
    dev_->CmdBindPipeline(pipe);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0, 2, 0},  // pos.xy
        VertexAttr{1, 2, 8},  // uv.xy
    };
    dev_->CmdBindVertexBuffer(0, selftest_vbo_, 16, attrs, VertexInputRate::PerVertex);
    dev_->CmdBindUniformBuffer(0, selftest_ubo_, 0, 16);
    dev_->CmdBindTexture(0, selftest_tex_, TextureDim::Tex2D);
    dev_->CmdDraw(6, 0);
    dev_->CmdEndRenderPass();
}

void WebGpuSelfTestHarness::RecordPending() {
    // 每会话各一次：在 device 当帧 frame_encoder_ 上录制（须在无 render/compute pass 时）。
    if (!compute_selftest_done_) { compute_selftest_done_ = true; ct_recorded_ = RecordComputeSelfTest(); }
    if (!gpu_cull_selftest_done_) { gpu_cull_selftest_done_ = true; gc_recorded_ = RecordGpuCullSelfTest(); }
    if (!skinning_selftest_done_) { skinning_selftest_done_ = true; sk_recorded_ = RecordSkinningSelfTest(); }
    if (!storage_image_selftest_done_) { storage_image_selftest_done_ = true; si_recorded_ = RecordStorageImageSelfTest(); }
    if (!hiz_selftest_done_) { hiz_selftest_done_ = true; hz_recorded_ = RecordHiZDownsampleSelfTest(); }
    if (!hizpyr_selftest_done_) { hizpyr_selftest_done_ = true; hzp_recorded_ = RecordHiZPyramidSelfTest(); }
    if (!compute_bind_selftest_done_) { compute_bind_selftest_done_ = true; cb_recorded_ = RecordComputeBindSelfTest(); }
    if (!hizcull_selftest_done_) { hizcull_selftest_done_ = true; hc_recorded_ = RecordHiZCullSelfTest(); }
    if (!morph_selftest_done_) { morph_selftest_done_ = true; mf_recorded_ = RecordMorphSelfTest(); }
    if (!ddgi_selftest_done_) { ddgi_selftest_done_ = true; dg_recorded_ = RecordDDGISelfTest(); }
    if (!hair_selftest_done_) { hair_selftest_done_ = true; hr_recorded_ = RecordHairSelfTest(); }
    if (!bloom_selftest_done_) { bloom_selftest_done_ = true; bl_recorded_ = RecordBloomSelfTest(); }
    if (!grass_selftest_done_) { grass_selftest_done_ = true; gr_recorded_ = RecordGrassSelfTest(); }
    if (!t41_mdi_selftest_done_) { t41_mdi_selftest_done_ = true; t41_recorded_ = RecordMultiDrawIndirectSelfTest(); }
    if (!t42_mega_selftest_done_) { t42_mega_selftest_done_ = true; t42_recorded_ = RecordMegaVaoSelfTest(); }
    if (!t43_pbr_selftest_done_) { t43_pbr_selftest_done_ = true; t43_recorded_ = RecordGpuDrivenPBRSelfTest(); }
    if (!t44_hiz_selftest_done_) { t44_hiz_selftest_done_ = true; t44_recorded_ = RecordGpuDrivenHiZCullSelfTest(); }
    if (!t51_csm_selftest_done_) { t51_csm_selftest_done_ = true; t51_recorded_ = RecordCSMShadowSelfTest(); }
    if (!t52_deferred_selftest_done_) { t52_deferred_selftest_done_ = true; t52_recorded_ = RecordDeferredSelfTest(); }
    if (!t53_hdr_selftest_done_) { t53_hdr_selftest_done_ = true; t53_recorded_ = RecordHDRSelfTest(); }
    if (!t54_ibl_selftest_done_) { t54_ibl_selftest_done_ = true; t54_recorded_ = RecordIBLSelfTest(); }
    if (!t55_wboit_selftest_done_) { t55_wboit_selftest_done_ = true; t55_recorded_ = RecordWBOITSelfTest(); }
}

void WebGpuSelfTestHarness::KickPendingReadbacks() {
    // 帧提交后对本帧已录制的自检发起异步 map 回读校验。
    if (ct_recorded_) KickComputeSelfTestReadback();
    if (gc_recorded_) KickGpuCullSelfTestReadback();
    if (sk_recorded_) KickSkinningSelfTestReadback();
    if (si_recorded_) KickStorageImageSelfTestReadback();
    if (hz_recorded_) KickHiZDownsampleSelfTestReadback();
    if (hzp_recorded_) KickHiZPyramidSelfTestReadback();
    if (cb_recorded_) KickComputeBindSelfTestReadback();
    if (hc_recorded_) KickHiZCullSelfTestReadback();
    if (mf_recorded_) KickMorphSelfTestReadback();
    if (dg_recorded_) KickDDGISelfTestReadback();
    if (hr_recorded_) KickHairSelfTestReadback();
    if (bl_recorded_) KickBloomSelfTestReadback();
    if (gr_recorded_) KickGrassSelfTestReadback();
    if (t41_recorded_) KickMultiDrawIndirectSelfTestReadback();
    if (t42_recorded_) KickMegaVaoSelfTestReadback();
    if (t43_recorded_) KickGpuDrivenPBRSelfTestReadback();
    if (t44_recorded_) KickGpuDrivenHiZCullSelfTestReadback();
    if (t51_recorded_) KickCSMShadowSelfTestReadback();
    if (t52_recorded_) KickDeferredSelfTestReadback();
    if (t53_recorded_) KickHDRSelfTestReadback();
    if (t54_recorded_) KickIBLSelfTestReadback();
    if (t55_recorded_) KickWBOITSelfTestReadback();
}

void WebGpuSelfTestHarness::RunBringUp() { RunBringUpSelfTest(); }

WebGpuSelfTestHarness::~WebGpuSelfTestHarness() {
    // 释放自检回读缓冲（多数在 kick 时所有权已转移给 ctx 回调 → 此处为 null）。
    if (ct_rb_out_)  { wgpuBufferRelease(ct_rb_out_);  ct_rb_out_ = nullptr; }
    if (ct_rb_draw_) { wgpuBufferRelease(ct_rb_draw_); ct_rb_draw_ = nullptr; }
    // B3b-2 GPU-driven 剔除自检瞬态资源（readback 已 kick 时所有权已转移给 ctx → 这里均为 null）。
    if (gc_rb_draw_)      { wgpuBufferRelease(gc_rb_draw_);          gc_rb_draw_ = nullptr; }
    if (gc_rb_pixels_)    { wgpuBufferRelease(gc_rb_pixels_);        gc_rb_pixels_ = nullptr; }
    if (gc_rt_view_)      { wgpuTextureViewRelease(gc_rt_view_);     gc_rt_view_ = nullptr; }
    if (gc_rt_tex_)       { wgpuTextureRelease(gc_rt_tex_);          gc_rt_tex_ = nullptr; }
    if (gc_pipeline_)     { wgpuRenderPipelineRelease(gc_pipeline_); gc_pipeline_ = nullptr; }
    if (gc_render_module_){ wgpuShaderModuleRelease(gc_render_module_); gc_render_module_ = nullptr; }
    if (gc_vbo_)          { wgpuBufferRelease(gc_vbo_);              gc_vbo_ = nullptr; }
    if (gc_ibo_)          { wgpuBufferRelease(gc_ibo_);              gc_ibo_ = nullptr; }
    // B3b-3 GPU 蒙皮自检瞬态资源（readback 已 kick 时所有权已转移给 ctx → 这里均为 null）。
    if (sk_rb_dst_)       { wgpuBufferRelease(sk_rb_dst_);           sk_rb_dst_ = nullptr; }
    if (sk_rb_pixels_)    { wgpuBufferRelease(sk_rb_pixels_);        sk_rb_pixels_ = nullptr; }
    if (sk_rt_view_)      { wgpuTextureViewRelease(sk_rt_view_);     sk_rt_view_ = nullptr; }
    if (sk_rt_tex_)       { wgpuTextureRelease(sk_rt_tex_);          sk_rt_tex_ = nullptr; }
    if (sk_pipeline_)     { wgpuRenderPipelineRelease(sk_pipeline_); sk_pipeline_ = nullptr; }
    if (sk_render_module_){ wgpuShaderModuleRelease(sk_render_module_); sk_render_module_ = nullptr; }
    if (sk_ibo_)          { wgpuBufferRelease(sk_ibo_);              sk_ibo_ = nullptr; }
    // B3b-4 storage-image 自检瞬态回读缓冲（readback 已 kick 时所有权已转移给 ctx → 这里为 null）。
    if (si_rb_pixels_)    { wgpuBufferRelease(si_rb_pixels_);        si_rb_pixels_ = nullptr; }
    // B3b-5 Hi-Z 下采样自检瞬态回读缓冲（同上：kick 后所有权转移给 ctx → 这里为 null）。
    if (hz_rb_pixels_)    { wgpuBufferRelease(hz_rb_pixels_);        hz_rb_pixels_ = nullptr; }
    // B3b-6 Hi-Z 金字塔自检回读缓冲（kick 后回读缓冲所有权转移给 ctx → null）。
    if (hzp_rb_pixels_)   { wgpuBufferRelease(hzp_rb_pixels_);       hzp_rb_pixels_ = nullptr; }
    // B3b-8 命名 uniform/采样自检回读缓冲（kick 后所有权转移给 ctx → null）。
    if (cb_rb_out_)       { wgpuBufferRelease(cb_rb_out_);           cb_rb_out_ = nullptr; }
    // B3b-9 Hi-Z 剔除自检回读缓冲（kick 后所有权转移给 ctx → null）。
    if (hc_rb_out_)       { wgpuBufferRelease(hc_rb_out_);           hc_rb_out_ = nullptr; }
    // B3b-10 morph 自检回读缓冲（kick 后所有权转移给 ctx → null）。
    if (mf_rb_out_)       { wgpuBufferRelease(mf_rb_out_);           mf_rb_out_ = nullptr; }
    // B3b-11 DDGI 自检回读缓冲（kick 后所有权转移给 ctx → null）。
    if (dg_rb_out_)       { wgpuBufferRelease(dg_rb_out_);           dg_rb_out_ = nullptr; }
    // B3b-12 hair 自检回读缓冲（kick 后所有权转移给 ctx → null）。
    if (hr_rb_out_)       { wgpuBufferRelease(hr_rb_out_);           hr_rb_out_ = nullptr; }
    // B3b-13 bloom 自检回读缓冲（kick 后所有权转移给 ctx → null）。
    if (bl_rb_out_)       { wgpuBufferRelease(bl_rb_out_);           bl_rb_out_ = nullptr; }
    // B3b-14 grass 自检回读缓冲（kick 后所有权转移给 ctx → null）。
    if (gr_rb_out_)       { wgpuBufferRelease(gr_rb_out_);           gr_rb_out_ = nullptr; }
    // Task 4 Subtask 1 MDI 自检像素回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t41_rb_pixels_)   { wgpuBufferRelease(t41_rb_pixels_);       t41_rb_pixels_ = nullptr; }
    // Task 4 Subtask 2 Mega VAO 自检像素回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t42_rb_pixels_)   { wgpuBufferRelease(t42_rb_pixels_);       t42_rb_pixels_ = nullptr; }
    // Task 4 Subtask 3 GPU-driven PBR 自检像素回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t43_rb_pixels_)   { wgpuBufferRelease(t43_rb_pixels_);       t43_rb_pixels_ = nullptr; }
    // Task 4 Subtask 4 GPU-driven Hi-Z 剔除自检可见性回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t44_rb_out_)      { wgpuBufferRelease(t44_rb_out_);          t44_rb_out_ = nullptr; }
    // Task 5 Subtask 1 CSM 阴影自检像素回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t51_rb_pixels_)   { wgpuBufferRelease(t51_rb_pixels_);       t51_rb_pixels_ = nullptr; }
    // Task 5 Subtask 2 延迟着色自检像素回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t52_rb_pixels_)   { wgpuBufferRelease(t52_rb_pixels_);       t52_rb_pixels_ = nullptr; }
    // Task 5 Subtask 3 HDR tonemap 自检像素回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t53_rb_pixels_)   { wgpuBufferRelease(t53_rb_pixels_);       t53_rb_pixels_ = nullptr; }
    // Task 5 Subtask 4 IBL 自检像素回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t54_rb_pixels_)   { wgpuBufferRelease(t54_rb_pixels_);       t54_rb_pixels_ = nullptr; }
    // Task 5 Subtask 5 WBOIT 自检像素回读缓冲（kick 后所有权转移给 ctx → null）。
    if (t55_rb_pixels_)   { wgpuBufferRelease(t55_rb_pixels_);       t55_rb_pixels_ = nullptr; }
}

} // namespace render
} // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU && DSE_WEBGPU_SELFTEST
