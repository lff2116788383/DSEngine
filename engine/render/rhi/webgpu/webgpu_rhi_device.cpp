/**
 * @file webgpu_rhi_device.cpp
 * @brief WebGPU RHI 后端实现（B0 骨架）。详见头文件。
 */

#include "engine/render/rhi/webgpu/webgpu_rhi_device.h"

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/base/debug.h"
#include "engine/render/rhi/draw_executor_common.h"
#include "engine/render/rhi/gpu_scene_types.h"
// B3b-12 hair 全 4 趟自检直接取引擎真 compute 源（同 engine 层，无层级违规）。
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

/// B2 命令缓冲：把后端无关的录制接口逐调用转发到 WebGPURhiDevice 的设备级 Cmd*，
/// 由设备直接录制到本帧 frame_encoder_（立即转发，非缓存重放）。所有接口显式实现，
/// 不依赖基类静默默认，避免漏实现时无声吞掉绘制。ClearColor/SetGlobalMat4/三类 ShadowMap/
/// DispatchComputePass/DrawIndexedIndirect 转发到 B2 期保持 no-op 的 Cmd*（留 B3）。
class WebGPUCommandBuffer final : public CommandBuffer {
public:
    explicit WebGPUCommandBuffer(WebGPURhiDevice* device) : device_(device) {}

    void BeginRenderPass(const RenderPassDesc& render_pass) override { device_->CmdBeginRenderPass(render_pass); }
    void EndRenderPass() override { device_->CmdEndRenderPass(); }
    void ClearColor(const glm::vec4& color) override { device_->CmdClearColor(color); }
    void SetGlobalMat4(const std::string& name, const glm::mat4& value) override { device_->CmdSetGlobalMat4(name, value); }
    void SetViewport(int x, int y, int width, int height) override { device_->CmdSetViewport(x, y, width, height); }

    void BindGlobalShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalShadowMap(index, texture_handle); }
    void BindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalSpotShadowMap(index, texture_handle); }
    void BindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override { device_->CmdBindGlobalPointShadowMap(index, texture_handle); }

    void BindPipeline(unsigned int graphics_pipeline_handle) override { device_->CmdBindPipeline(graphics_pipeline_handle); }
    void BindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                          const std::vector<VertexAttr>& attrs,
                          VertexInputRate rate) override {
        device_->CmdBindVertexBuffer(slot, buffer_handle, stride, attrs, rate);
    }
    void PushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) override {
        device_->CmdPushConstants(stage, offset, data, size);
    }
    void Draw(uint32_t vertex_count, uint32_t first_vertex) override { device_->CmdDraw(vertex_count, first_vertex); }

    void BindIndexBuffer(unsigned int buffer_handle, IndexType type) override { device_->CmdBindIndexBuffer(buffer_handle, type); }
    void BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) override {
        device_->CmdBindTexture(slot, texture_handle, dim);
    }
    void BindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) override {
        device_->CmdBindUniformBuffer(slot, buffer_handle, offset, size);
    }
    void BindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size) override {
        device_->CmdBindStorageBuffer(slot, buffer_handle, offset, size);
    }
    void DrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) override {
        device_->CmdDrawIndexed(index_count, first_index, base_vertex);
    }
    void DispatchComputePass(const ComputeDispatch& dispatch) override { device_->CmdDispatchComputePass(dispatch); }
    void DrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                              uint32_t first_index, int32_t base_vertex,
                              uint32_t first_instance) override {
        device_->CmdDrawIndexedInstanced(index_count, instance_count, first_index, base_vertex, first_instance);
    }
    void DrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) override {
        device_->CmdDrawIndexedIndirect(indirect_buffer, byte_offset);
    }

private:
    WebGPURhiDevice* device_;
};

// --- B1 资源映射小工具 ---

constexpr uint64_t AlignUp4(uint64_t n) { return (n + 3u) & ~static_cast<uint64_t>(3u); }

/// 解析 WGSL 源中实际声明的 `@group(N) @binding(M)`，填入 out（key=(group<<16)|binding）。
/// 供 explicit pipeline-layout/BindGroup 过滤：仅纳入着色器真正使用的绑定，避免引擎多绑资源
/// 超 per-stage 上限 / 与着色器用量不符。render（vs/fs）与 compute 程序共用此解析。
void ParseWgslBindings(const std::string& src, std::set<uint32_t>& out) {
    for (size_t pos = src.find("@group("); pos != std::string::npos;
         pos = src.find("@group(", pos + 1)) {
        const size_t g0 = pos + 7;
        const size_t g1 = src.find(')', g0);
        if (g1 == std::string::npos) break;
        const size_t bpos = src.find("@binding(", g1);
        if (bpos == std::string::npos) break;
        // @binding 须紧随同一声明（其间只允许空白），否则视为不同声明。
        if (src.find_first_not_of(" \t\r\n", g1 + 1) != bpos) continue;
        const size_t b0 = bpos + 9;
        const size_t b1 = src.find(')', b0);
        if (b1 == std::string::npos) break;
        const uint32_t group = static_cast<uint32_t>(std::strtoul(src.c_str() + g0, nullptr, 10));
        const uint32_t binding = static_cast<uint32_t>(std::strtoul(src.c_str() + b0, nullptr, 10));
        out.insert((group << 16) | binding);
    }
}

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

// --- B2 PSO/顶点格式 → WebGPU 枚举映射 ---

WGPUVertexFormat ToVertexFormat(uint32_t components) {
    switch (components) {
        case 1:  return WGPUVertexFormat_Float32;
        case 2:  return WGPUVertexFormat_Float32x2;
        case 3:  return WGPUVertexFormat_Float32x3;
        default: return WGPUVertexFormat_Float32x4;
    }
}

WGPUTextureViewDimension ToViewDimension(TextureDim dim) {
    switch (dim) {
        case TextureDim::TexCube:    return WGPUTextureViewDimension_Cube;
        case TextureDim::Tex2DArray: return WGPUTextureViewDimension_2DArray;
        case TextureDim::Tex3D:      return WGPUTextureViewDimension_3D;
        case TextureDim::Tex2D:
        default:                     return WGPUTextureViewDimension_2D;
    }
}

WGPUPrimitiveTopology ToTopology(PrimitiveTopology t) {
    switch (t) {
        case PrimitiveTopology::LineStrip: return WGPUPrimitiveTopology_LineStrip;
        case PrimitiveTopology::LineList:  return WGPUPrimitiveTopology_LineList;
        case PrimitiveTopology::PointList: return WGPUPrimitiveTopology_PointList;
        case PrimitiveTopology::TriangleList:
        default:                           return WGPUPrimitiveTopology_TriangleList;
    }
}

WGPUCullMode ToCullMode(CullFace c) {
    switch (c) {
        case CullFace::Front: return WGPUCullMode_Front;
        case CullFace::Back:  return WGPUCullMode_Back;
        case CullFace::None:
        case CullFace::FrontAndBack:  // WebGPU 无 FrontAndBack；退化为 None（双面不剔除）
        default:              return WGPUCullMode_None;
    }
}

WGPUCompareFunction ToCompareFunc(CompareFunc f) {
    switch (f) {
        case CompareFunc::Never:        return WGPUCompareFunction_Never;
        case CompareFunc::Less:         return WGPUCompareFunction_Less;
        case CompareFunc::Equal:        return WGPUCompareFunction_Equal;
        case CompareFunc::LessEqual:    return WGPUCompareFunction_LessEqual;
        case CompareFunc::Greater:      return WGPUCompareFunction_Greater;
        case CompareFunc::NotEqual:     return WGPUCompareFunction_NotEqual;
        case CompareFunc::GreaterEqual: return WGPUCompareFunction_GreaterEqual;
        case CompareFunc::Always:
        default:                        return WGPUCompareFunction_Always;
    }
}

WGPUBlendFactor ToBlendFactor(BlendFactor f) {
    switch (f) {
        case BlendFactor::Zero:             return WGPUBlendFactor_Zero;
        case BlendFactor::One:              return WGPUBlendFactor_One;
        case BlendFactor::SrcAlpha:         return WGPUBlendFactor_SrcAlpha;
        case BlendFactor::OneMinusSrcAlpha: return WGPUBlendFactor_OneMinusSrcAlpha;
        case BlendFactor::DstAlpha:         return WGPUBlendFactor_DstAlpha;
        case BlendFactor::OneMinusDstAlpha: return WGPUBlendFactor_OneMinusDstAlpha;
        case BlendFactor::SrcColor:         return WGPUBlendFactor_Src;
        case BlendFactor::OneMinusSrcColor: return WGPUBlendFactor_OneMinusSrc;
        case BlendFactor::DstColor:         return WGPUBlendFactor_Dst;
        case BlendFactor::OneMinusDstColor: return WGPUBlendFactor_OneMinusDst;
        default:                            return WGPUBlendFactor_One;
    }
}

bool IsDepthFormat(WGPUTextureFormat f) {
    return f == WGPUTextureFormat_Depth32Float || f == WGPUTextureFormat_Depth24Plus ||
           f == WGPUTextureFormat_Depth24PlusStencil8 || f == WGPUTextureFormat_Depth16Unorm ||
           f == WGPUTextureFormat_Depth32FloatStencil8;
}

WGPUAddressMode ToAddressMode(TextureWrap w) {
    return w == TextureWrap::ClampToEdge ? WGPUAddressMode_ClampToEdge : WGPUAddressMode_Repeat;
}
WGPUFilterMode ToFilterMode(TextureFilter f) {
    return f == TextureFilter::Linear ? WGPUFilterMode_Linear : WGPUFilterMode_Nearest;
}

/// 全 mip 链层数（2D 维度，向下取整 log2(max(w,h))+1）。
uint32_t FullMipCount(uint32_t w, uint32_t h) {
    uint32_t m = (w > h ? w : h);
    uint32_t levels = 1;
    while (m > 1) { m >>= 1; ++levels; }
    return levels;
}

/// 向 mipLevel=0..1 的 2D 纹理写入一层 RGBA8 数据（origin.z 指定 cube 面 / 3D 切片）。
void WriteTextureLayerRGBA8(WGPUQueue queue, WGPUTexture tex, uint32_t mip_level,
                            uint32_t width, uint32_t height, uint32_t z,
                            const unsigned char* rgba8) {
    if (!rgba8) return;
    WGPUImageCopyTexture dst{};
    dst.texture = tex;
    dst.mipLevel = mip_level;
    dst.origin = WGPUOrigin3D{0, 0, z};
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout{};
    layout.offset = 0;
    layout.bytesPerRow = width * 4u;
    layout.rowsPerImage = height;
    WGPUExtent3D extent{width, height, 1u};
    wgpuQueueWriteTexture(queue, &dst, rgba8, static_cast<size_t>(width) * height * 4u, &layout, &extent);
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

WebGPURhiDevice::WebGPURhiDevice() = default;

WebGPURhiDevice::~WebGPURhiDevice() {
    Shutdown();
}

RenderDeviceInfo WebGPURhiDevice::GetDeviceInfo() const {
    RenderDeviceInfo info;
    info.adapter_name = "WebGPU";
    info.is_software = false;  // 实际软/硬由浏览器适配器决定；B5 经 adapter info 精确填充
    return info;
}

bool WebGPURhiDevice::AcquireDevice() {
    // 设备由 JS 侧（shell.html）经 navigator.gpu.requestAdapter().requestDevice()
    // 预创建并挂到 Module.preinitializedWebGPUDevice；此处同步取得其 C 句柄。
    device_ = emscripten_webgpu_get_device();
    if (!device_) {
        DEBUG_LOG_WARN("WebGPU: emscripten_webgpu_get_device() 返回空 —— JS 侧未预创建设备，"
                       "上层将回退 WebGL2(OpenGL) 后端");
        return false;
    }
    queue_ = wgpuDeviceGetQueue(device_);
    if (!queue_) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceGetQueue 失败");
        return false;
    }
    // 把 Dawn 未捕获错误（着色器编译 / 管线 / 绑定校验失败等）转出到日志，便于 bring-up 诊断。
    wgpuDeviceSetUncapturedErrorCallback(
        device_,
        [](WGPUErrorType type, char const* message, void*) {
            DEBUG_LOG_ERROR("WebGPU uncaptured error (type={}): {}",
                            static_cast<int>(type), message ? message : "(null)");
        },
        nullptr);

    // B4 能力探测：读取适配器实际 limits 填充 MRT 上限（供能力声明式裁剪 requires_mrt）。
    // 探测失败保持默认 8（WebGPU 规范保证的最低 maxColorAttachments）。
    WGPUSupportedLimits limits{};
    if (wgpuDeviceGetLimits(device_, &limits) && limits.limits.maxColorAttachments > 0) {
        max_color_attachments_ = static_cast<int>(limits.limits.maxColorAttachments);
    }
    // 一次性输出 WebGPU 能力矩阵（harness/浏览器控制台可见），明示当前裁剪路由依据：
    //   Task 3 起 supports_compute=true（B3b-2..13 逐消费方手译 WGSL 离屏自检全 PASS + 门控审计）；
    //   ssbo-compute 仍 false（无 SSBO 同步读回）。compute 消费方按各自能力门控接入（morph 已激活，
    //   skinning/grass 待 ssbo-compute、GPU-driven/Hi-Z 待 indirect-draw、DDGI/hair 无 WGSL 优雅回退）。
    DEBUG_LOG_INFO("WebGPU[B4] 能力矩阵：max_color_attachments={} supports_compute={} "
                   "supports_ssbo={} supports_ssbo_compute={}（Task 3：compute 已翻转，"
                   "消费方逐能力门控接入）",
                   max_color_attachments_, SupportsCompute(), SupportsSSBO(), SupportsSSBOCompute());
    return true;
}

bool WebGPURhiDevice::CreateSwapChain(int width, int height) {
    if (!device_) return false;
    if (!instance_) {
        instance_ = wgpuCreateInstance(nullptr);
    }
    if (!surface_) {
        // GLFW(Emscripten) 默认渲染到 Module.canvas（HTML 选择器 "#canvas"）。
        WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc{};
        canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        canvas_desc.selector = "#canvas";
        WGPUSurfaceDescriptor surf_desc{};
        surf_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&canvas_desc);
        surface_ = wgpuInstanceCreateSurface(instance_, &surf_desc);
        if (!surface_) {
            DEBUG_LOG_ERROR("WebGPU: wgpuInstanceCreateSurface 失败");
            return false;
        }
    }

    ReleaseSwapChain();

    WGPUSwapChainDescriptor sc_desc{};
    sc_desc.usage = WGPUTextureUsage_RenderAttachment;
    sc_desc.format = swapchain_format_;
    sc_desc.width = static_cast<uint32_t>(width > 0 ? width : 1);
    sc_desc.height = static_cast<uint32_t>(height > 0 ? height : 1);
    sc_desc.presentMode = WGPUPresentMode_Fifo;
    swapchain_ = wgpuDeviceCreateSwapChain(device_, surface_, &sc_desc);
    if (!swapchain_) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateSwapChain 失败 ({}x{})", width, height);
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

void WebGPURhiDevice::ReleaseSwapChain() {
    if (swapchain_) {
        wgpuSwapChainRelease(swapchain_);
        swapchain_ = nullptr;
    }
}

bool WebGPURhiDevice::InitDevice(void* window_handle, int width, int height) {
    (void)window_handle;
    if (initialized_) return true;
    if (!AcquireDevice()) return false;
    if (!CreateSwapChain(width, height)) return false;
    initialized_ = true;
    DEBUG_LOG_INFO("WebGPU 后端初始化成功 ({}x{}, 交换链格式=0x{:x})",
                   width, height, static_cast<unsigned int>(swapchain_format_));
    return true;
}

void WebGPURhiDevice::OnWindowResized(int width, int height) {
    if (!initialized_) return;
    if (width == width_ && height == height_) return;
    CreateSwapChain(width, height);
}

void WebGPURhiDevice::Shutdown() {
    // 先释放所有资源对象，再释放交换链/队列/设备。
    for (auto& [h, rt] : render_targets_) {
        (void)h;
        for (unsigned int th : rt.color_textures) {
            auto it = textures_.find(th);
            if (it != textures_.end()) { DestroyTextureEntry(it->second); textures_.erase(it); }
        }
        if (rt.depth_texture) {
            auto it = textures_.find(rt.depth_texture);
            if (it != textures_.end()) { DestroyTextureEntry(it->second); textures_.erase(it); }
        }
    }
    render_targets_.clear();
    for (auto& [h, e] : textures_) { (void)h; DestroyTextureEntry(e); }
    textures_.clear();
    for (auto& [h, e] : buffers_) { (void)h; if (e.buffer) wgpuBufferRelease(e.buffer); }
    buffers_.clear();
    for (auto& [h, e] : shaders_) { (void)h; if (e.module) wgpuShaderModuleRelease(e.module); }
    shaders_.clear();
    pipeline_states_.clear();

    // B2 录制缓存 / 池 / 瞬态：管线缓存（pipeline+layout+4×BGL）、push 缓冲池、本帧 BindGroup、临时面视图。
    for (auto& [key, e] : pipeline_cache_) {
        (void)key;
        if (e.pipeline) wgpuRenderPipelineRelease(e.pipeline);
        if (e.layout)   wgpuPipelineLayoutRelease(e.layout);
        for (WGPUBindGroupLayout bgl : e.bgls) if (bgl) wgpuBindGroupLayoutRelease(bgl);
    }
    pipeline_cache_.clear();
    // B3a compute：着色器 module、compute 管线缓存（pipeline+layout+4×BGL）、未消费的自检回读缓冲。
    for (auto& [h, e] : compute_shaders_) { (void)h; if (e.module) wgpuShaderModuleRelease(e.module); }
    compute_shaders_.clear();
    for (auto& [key, e] : compute_pipeline_cache_) {
        (void)key;
        if (e.pipeline) wgpuComputePipelineRelease(e.pipeline);
        if (e.layout)   wgpuPipelineLayoutRelease(e.layout);
        for (WGPUBindGroupLayout bgl : e.bgls) if (bgl) wgpuBindGroupLayoutRelease(bgl);
    }
    compute_pipeline_cache_.clear();
    if (cur_compute_pass_) { wgpuComputePassEncoderRelease(cur_compute_pass_); cur_compute_pass_ = nullptr; }
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
    // GPU→CPU 异步双缓冲延迟回读 staging（map 飞行中的所有权仍由本设备持有；这里随设备销毁一并释放）。
    for (int i = 0; i < 2; ++i) {
        if (async_rb_staging_[i]) { wgpuBufferRelease(async_rb_staging_[i]); async_rb_staging_[i] = nullptr; }
    }
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
    // B3b-7：SetComputeTextureImageMip 缓存的单 mip 视图统一释放（含金字塔自检各级 mip 视图）。
    for (auto& [k, v] : compute_mip_views_) if (v) wgpuTextureViewRelease(v);
    compute_mip_views_.clear();
    for (WGPUBuffer b : push_pool_) if (b) wgpuBufferRelease(b);
    push_pool_.clear();
    push_pool_used_ = 0;
    for (WGPUBindGroup bg : frame_bindgroups_) if (bg) wgpuBindGroupRelease(bg);
    frame_bindgroups_.clear();
    ReleasePassViews();

    ReleaseSwapChain();
    if (ubo_ring_) { wgpuBufferRelease(ubo_ring_); ubo_ring_ = nullptr; ubo_ring_size_ = 0; ubo_ring_cursor_ = 0; }
    if (geom_ring_) { wgpuBufferRelease(geom_ring_); geom_ring_ = nullptr; geom_ring_size_ = 0; geom_ring_cursor_ = 0; }
    if (surface_) { wgpuSurfaceRelease(surface_); surface_ = nullptr; }
    if (queue_)   { wgpuQueueRelease(queue_);     queue_ = nullptr; }
    if (device_)  { wgpuDeviceRelease(device_);   device_ = nullptr; }
    initialized_ = false;
}

void WebGPURhiDevice::WaitIdle() {
    // WebGPU 无显式 device idle 等待；提交后由浏览器调度。B3 起按需用 onSubmittedWorkDone。
}

bool WebGPURhiDevice::EnsureInitialized() {
    // Web 宿主（Emscripten）以空 native_window_handle 创建设备，FramePipeline 因此不调用
    // InitDevice（仅 D3D11/有窗口句柄时调用，A 阶段 GL 路径无需 swapchain）。这里据画布尺寸
    // 惰性完成 WebGPU 设备 + swapchain 初始化，不触碰 A 阶段回退逻辑。
    if (initialized_) return true;
    int cw = 0, ch = 0;
    emscripten_get_canvas_element_size("#canvas", &cw, &ch);
    if (cw <= 0) cw = 1280;
    if (ch <= 0) ch = 720;
    return InitDevice(nullptr, cw, ch);
}

uint64_t WebGPURhiDevice::AllocUboVersion(const void* data, uint64_t size) {
    if (!device_ || !data || size == 0) return UINT64_MAX;
    constexpr uint64_t kAlign = 256;  // WebGPU minUniformBufferOffsetAlignment 默认 256
    const uint64_t aligned_size = (size + 3) & ~uint64_t(3);
    const uint64_t off = (ubo_ring_cursor_ + kAlign - 1) & ~(kAlign - 1);
    // 环不足以容纳本次分配：本帧已录制的 BindGroup 仍引用现有环缓冲，不能中途重建；
    //   故仅当环尚未创建或在帧首（游标为 0）时按需扩容，运行中溢出则降级（返回失败，回退原存储）。
    if (off + aligned_size > ubo_ring_size_) {
        if (ubo_ring_cursor_ != 0) {
            DEBUG_LOG_WARN("WebGPU: UBO 版本环本帧溢出（需 {} > 容量 {}），该 UBO 回退原存储",
                           static_cast<unsigned long long>(off + aligned_size),
                           static_cast<unsigned long long>(ubo_ring_size_));
            return UINT64_MAX;
        }
        uint64_t new_size = ubo_ring_size_ ? ubo_ring_size_ : (1u << 22);  // 4MB 起步
        while (off + aligned_size > new_size) new_size *= 2;
        if (ubo_ring_) wgpuBufferRelease(ubo_ring_);
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        bd.size = new_size;
        ubo_ring_ = wgpuDeviceCreateBuffer(device_, &bd);
        ubo_ring_size_ = ubo_ring_ ? new_size : 0;
        if (!ubo_ring_) return UINT64_MAX;
    }
    if (aligned_size == size) {
        wgpuQueueWriteBuffer(queue_, ubo_ring_, off, data, size);
    } else {
        std::vector<uint8_t> padded(aligned_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, ubo_ring_, off, padded.data(), aligned_size);
    }
    ubo_ring_cursor_ = off + aligned_size;
    return off;
}

uint64_t WebGPURhiDevice::AllocGeomVersion(const void* data, uint64_t size) {
    if (!device_ || !data || size == 0) return UINT64_MAX;
    // 顶点缓冲偏移须 4 对齐；索引缓冲偏移须为索引元素字节数（2/4）的倍数。取 4 对齐两者均满足。
    constexpr uint64_t kAlign = 4;
    const uint64_t aligned_size = (size + 3) & ~uint64_t(3);
    const uint64_t off = (geom_ring_cursor_ + kAlign - 1) & ~(kAlign - 1);
    // 同 UBO 版本环：本帧已录制的绑定/索引绑定仍引用现有环缓冲，不能中途重建；
    //   故仅在环尚未创建或在帧首（游标为 0）按需扩容，运行中溢出则降级（返回失败，回退原存储）。
    if (off + aligned_size > geom_ring_size_) {
        if (geom_ring_cursor_ != 0) {
            DEBUG_LOG_WARN("WebGPU: 几何版本环本帧溢出（需 {} > 容量 {}），该顶点/索引回退原存储",
                           static_cast<unsigned long long>(off + aligned_size),
                           static_cast<unsigned long long>(geom_ring_size_));
            return UINT64_MAX;
        }
        uint64_t new_size = geom_ring_size_ ? geom_ring_size_ : (1u << 22);  // 4MB 起步
        while (off + aligned_size > new_size) new_size *= 2;
        if (geom_ring_) wgpuBufferRelease(geom_ring_);
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
        bd.size = new_size;
        geom_ring_ = wgpuDeviceCreateBuffer(device_, &bd);
        geom_ring_size_ = geom_ring_ ? new_size : 0;
        if (!geom_ring_) return UINT64_MAX;
    }
    if (aligned_size == size) {
        wgpuQueueWriteBuffer(queue_, geom_ring_, off, data, size);
    } else {
        std::vector<uint8_t> padded(aligned_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, geom_ring_, off, padded.data(), aligned_size);
    }
    geom_ring_cursor_ = off + aligned_size;
    return off;
}

void WebGPURhiDevice::EnsureShadowDepthFallback() {
    // 懒建 1×1 Depth32 回退纹理，并在无活动 pass 时一次性清深=1.0（恒无遮挡）。
    if (!device_) return;
    if (!shadow_fallback_rt_) {
        RenderTargetDesc d{};
        d.width = 1; d.height = 1;
        d.has_color = false;
        d.has_depth = true;
        shadow_fallback_rt_ = CreateRenderTarget(d);
        shadow_fallback_depth_tex_ = GetRenderTargetDepthTexture(shadow_fallback_rt_);
        shadow_fallback_cleared_ = false;
    }
    if (shadow_fallback_cleared_ || !frame_encoder_ || cur_pass_) return;
    const TextureEntry* fb = FindTexture(shadow_fallback_depth_tex_);
    if (!fb || !fb->view) return;
    WGPURenderPassDepthStencilAttachment ds{};
    ds.view = fb->view;
    ds.depthLoadOp = WGPULoadOp_Clear;
    ds.depthStoreOp = WGPUStoreOp_Store;
    ds.depthClearValue = 1.0f;           // 深度=1 → SampleCascadeShadow 恒判无遮挡
    ds.stencilLoadOp = WGPULoadOp_Undefined;
    ds.stencilStoreOp = WGPUStoreOp_Undefined;
    WGPURenderPassDescriptor pd{};
    pd.colorAttachmentCount = 0;
    pd.colorAttachments = nullptr;
    pd.depthStencilAttachment = &ds;
    WGPURenderPassEncoder p = wgpuCommandEncoderBeginRenderPass(frame_encoder_, &pd);
    if (p) {
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
        shadow_fallback_cleared_ = true;
    }
}

void WebGPURhiDevice::BeginFrame() {
    last_frame_stats_ = RenderStats{};
    if (!EnsureInitialized()) return;
    ubo_ring_cursor_ = 0;
    ubo_versions_.clear();
    geom_ring_cursor_ = 0;
    geom_versions_.clear();
    if (!swapchain_) return;
    backbuffer_view_ = wgpuSwapChainGetCurrentTextureView(swapchain_);
    if (!backbuffer_view_) return;
    frame_encoder_ = wgpuDeviceCreateCommandEncoder(device_, nullptr);
    // 每帧复位录制瞬态：push 池游标归零（缓冲跨帧复用）、自检触发标志、当前绘制绑定。
    backbuffer_drawn_ = false;
    push_pool_used_ = 0;
    ResetDrawState();
    // 5.1b：在任何前向 draw（采样 slot11 shadow atlas）之前，确保恒亮 Depth32 回退纹理就绪并已清深=1。
    EnsureShadowDepthFallback();
}

void WebGPURhiDevice::EndFrame() {
    if (!initialized_ || !backbuffer_view_ || !frame_encoder_) {
        if (backbuffer_view_) { wgpuTextureViewRelease(backbuffer_view_); backbuffer_view_ = nullptr; }
        if (frame_encoder_)   { wgpuCommandEncoderRelease(frame_encoder_); frame_encoder_ = nullptr; }
        return;
    }

    // 本帧若无任何真实绘制落到 backbuffer（B2 期引擎 GLSL 程序无 WGSL module，绘制在录制
    // 期被优雅跳过），跑 bring-up 自检：经 Cmd* 把渐变×棋盘纹理画上屏，验证整条录制链路。
    // 引擎 WGSL 内容（B2b+）就绪并真正上屏后，backbuffer_drawn_ 置真，自检自动不再触发。
    if (!backbuffer_drawn_) {
        RunBringUpSelfTest();
    }
    // 兜底：自检也未成形（资源创建失败）时，至少 clear 一次 backbuffer，避免呈现未定义内容。
    if (!backbuffer_drawn_) {
        WGPURenderPassColorAttachment color_att{};
        color_att.view = backbuffer_view_;
        color_att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        color_att.loadOp = WGPULoadOp_Clear;
        color_att.storeOp = WGPUStoreOp_Store;
        color_att.clearValue = WGPUColor{0.05, 0.05, 0.08, 1.0};
        WGPURenderPassDescriptor pass_desc{};
        pass_desc.colorAttachmentCount = 1;
        pass_desc.colorAttachments = &color_att;
        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(frame_encoder_, &pass_desc);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
        last_frame_stats_.render_passes += 1;
    }

    // B3a：每会话一次 compute 自检——在本帧 frame_encoder_ 上录制 dispatch + storage→回读拷贝，
    // 随帧提交后发起异步 map 校验。仅落地原语并自检，不翻转 SupportsCompute()，不影响渲染输出。
    bool ct_recorded = false;
    if (!compute_selftest_done_) {
        compute_selftest_done_ = true;
        ct_recorded = RecordComputeSelfTest();
    }

    // B3b-2：每会话一次 GPU-driven 剔除真链路自检（视锥剔除 compute → SSBO indirect cmd →
    // 真 DrawIndexedIndirect → 离屏 RT → 回读 SSBO+像素校验）。同样仅自检、不翻转能力位、不碰 demo 帧。
    bool gc_recorded = false;
    if (!gpu_cull_selftest_done_) {
        gpu_cull_selftest_done_ = true;
        gc_recorded = RecordGpuCullSelfTest();
    }

    // B3b-3：每会话一次 GPU 蒙皮真链路自检（蒙皮 compute → dst 顶点 SSBO → 该 SSBO 作顶点缓冲
    // 真绘制 → 离屏 RT → 回读 dst SSBO+像素校验）。同样仅自检、不翻转能力位、不碰 demo 帧。
    bool sk_recorded = false;
    if (!skinning_selftest_done_) {
        skinning_selftest_done_ = true;
        sk_recorded = RecordSkinningSelfTest();
    }

    // B3b-4：每会话一次 storage-image 写 compute 真链路自检（compute textureStore 写 storage 纹理 →
    // copy 纹理→回读缓冲 → 逐像素校验渐变）。同样仅自检、不翻转能力位、不碰 demo 帧。
    bool si_recorded = false;
    if (!storage_image_selftest_done_) {
        storage_image_selftest_done_ = true;
        si_recorded = RecordStorageImageSelfTest();
    }

    // B3b-5：每会话一次 Hi-Z 下采样核心自检（①生成趟 textureStore 渐变 → src；②下采样趟 textureLoad
    // 读 src + 2×2 max → dst → copy 回读校验）。同样仅自检、不翻转能力位、不碰 demo 帧。
    bool hz_recorded = false;
    if (!hiz_selftest_done_) {
        hiz_selftest_done_ = true;
        hz_recorded = RecordHiZDownsampleSelfTest();
    }

    // B3b-6：每会话一次 Hi-Z storage-image 金字塔自检（单张 R32Float mip 链，生成趟写 mip0 + 逐级
    // per-mip 视图下采样 2×2 max → 各级 mip → copy 回读逐级校验）。仅自检、不翻转能力位、不碰 demo 帧。
    bool hzp_recorded = false;
    if (!hizpyr_selftest_done_) {
        hizpyr_selftest_done_ = true;
        hzp_recorded = RecordHiZPyramidSelfTest();
    }

    // B3b-8：每会话一次 命名 uniform + compute 采样器绑定自检（SetComputeUniform* 命名块 +
    // SetComputeTextureSampler 句柄采样 → 结果 SSBO → copy 回读逐元素校验）。仅自检、不翻转能力位。
    bool cb_recorded = false;
    if (!compute_bind_selftest_done_) {
        compute_bind_selftest_done_ = true;
        cb_recorded = RecordComputeBindSelfTest();
    }

    // B3b-9：每会话一次 Hi-Z 遮挡剔除自检（手译 HiZCullPass compute → AABB 投影 + Hi-Z max 深度判定
    // → 可见性 SSBO → copy 回读逐元素校验）。仅自检、不翻转能力位、不碰 demo 帧。
    bool hc_recorded = false;
    if (!hizcull_selftest_done_) {
        hizcull_selftest_done_ = true;
        hc_recorded = RecordHiZCullSelfTest();
    }

    // B3b-10：每会话一次形变目标（morph）自检（手译 MorphTargetSystem compute → base + Σ weight·delta
    // → normalize 法线 → 写形变顶点 SSBO → copy 回读逐顶点校验）。仅自检、不翻转能力位、不碰 demo 帧。
    bool mf_recorded = false;
    if (!morph_selftest_done_) {
        morph_selftest_done_ = true;
        mf_recorded = RecordMorphSelfTest();
    }

    // B3b-11：每会话一次 DDGI 探针更新核心自检（手译 DDGISystem probe-update compute → probe SSBO +
    // octahedral 方向 + RSM 采样 VPL 累积 → 写 storage image + 调试 SSBO → copy 回读逐 texel 校验）。
    bool dg_recorded = false;
    if (!ddgi_selftest_done_) {
        ddgi_selftest_done_ = true;
        dg_recorded = RecordDDGISelfTest();
    }

    // B3b-12：每会话一次 hair 物理全 4 趟自检（取引擎真 WGSL，同序跑 integrate→local_shape→length→
    // tangent 于单 compute pass → copy 输出 pos_cur+tangent → 回读逐分量校验 == C++ 同序复算）。
    bool hr_recorded = false;
    if (!hair_selftest_done_) {
        hair_selftest_done_ = true;
        hr_recorded = RecordHairSelfTest();
    }

    // B3b-13：每会话一次 bloom 双滤波核心自检（手译 bloom_downsample/upsample compute → 下采样 13-tap +
    // 上采样 3×3 tent + base 累加 → copy down4/up4 回读半精解码逐 texel 逐通道校验）。
    bool bl_recorded = false;
    if (!bloom_selftest_done_) {
        bloom_selftest_done_ = true;
        bl_recorded = RecordBloomSelfTest();
    }

    // B3b-14：每会话一次 grass 风场 compute 正确性自检（内联镜像 grass_system.cpp 风场 WGSL → 造已知
    // 实例 → BeginComputePass/DispatchCompute/EndComputePass → copy 输出 mat4 → 回读 C++ 复算逐元素校验）。
    bool gr_recorded = false;
    if (!grass_selftest_done_) {
        grass_selftest_done_ = true;
        gr_recorded = RecordGrassSelfTest();
    }

    // Task 4 Subtask 1：每会话一次 MultiDrawIndexedIndirect 真链路自检（引擎-facing CmdBeginRenderPass +
    // CmdBind* + MultiDrawIndexedIndirect 循环 DrawIndexedIndirect 渲到离屏 RT → copy 回读校验
    // 「可见象限有色、被剔象限为黑」）。仅自检、不翻转 SupportsIndirectDraw()、不碰 demo 帧。
    bool t41_recorded = false;
    if (!t41_mdi_selftest_done_) {
        t41_mdi_selftest_done_ = true;
        t41_recorded = RecordMultiDrawIndirectSelfTest();
    }
    // Task 4 Subtask 2：每会话一次 Mega VAO 真链路自检（CreateMegaVAO/UpdateMegaVBO/IBO + BindMegaVAO
    // 设 BatchVertex 92B draw state + CmdDrawIndexed 渲到离屏 RT → copy 回读校验各象限颜色就位）。
    bool t42_recorded = false;
    if (!t42_mega_selftest_done_) {
        t42_mega_selftest_done_ = true;
        t42_recorded = RecordMegaVaoSelfTest();
    }
    // Task 4 Subtask 3：每会话一次 GPU-driven PBR 真链路自检（SetupGPUDrivenPBRShader + 实例/材质 SSBO +
    // BindMegaVAO + BindGPUDrivenTextures + MultiDrawIndexedIndirect 渲到离屏 RT → copy 回读校验左红右绿）。
    bool t43_recorded = false;
    if (!t43_pbr_selftest_done_) {
        t43_pbr_selftest_done_ = true;
        t43_recorded = RecordGpuDrivenPBRSelfTest();
    }
    // Task 4 Subtask 4：每会话一次 GPU-driven Hi-Z 遮挡剔除真资源自检（CreateHiZTexture 建 mip 链 +
    // SetComputeTextureImageMip 写 mip0 + 逐级 2×2 max 下采样 + HiZCullPass 采样判遮挡 → 回读校验近可见/远剔除）。
    bool t44_recorded = false;
    if (!t44_hiz_selftest_done_) {
        t44_hiz_selftest_done_ = true;
        t44_recorded = RecordGpuDrivenHiZCullSelfTest();
    }
    // Task 5 Subtask 1：每会话一次 CSM 方向光阴影深度图采样自检（阴影深度趟写 Depth32 atlas → 前向趟
    // 经 texture_depth_2d + textureLoad 3×3 PCF 采样 → 回读校验 中心受遮挡为暗、四角受光为亮）。
    bool t51_recorded = false;
    if (!t51_csm_selftest_done_) {
        t51_csm_selftest_done_ = true;
        t51_recorded = RecordCSMShadowSelfTest();
    }
    // Task 5 Subtask 2：每会话一次延迟着色自检（几何趟写 MRT gbuffer → 全屏光照趟 textureLoad 3 张
    // gbuffer 做延迟光照 → 回读校验 中心几何受光为红、四角空像素为黑）。
    bool t52_recorded = false;
    if (!t52_deferred_selftest_done_) {
        t52_deferred_selftest_done_ = true;
        t52_recorded = RecordDeferredSelfTest();
    }
    // Task 5 Subtask 3：每会话一次 HDR auto-exposure + ACES tonemap 自检（场景→亮度归约→lum_adapt→
    // tonemap 四趟 → 回读 C++ 同公式复算逐通道校验 r>g>b、非全白非全黑）。
    bool t53_recorded = false;
    if (!t53_hdr_selftest_done_) {
        t53_hdr_selftest_done_ = true;
        t53_recorded = RecordHDRSelfTest();
    }
    // Task 5 Subtask 4：每会话一次 IBL 自检（BRDF LUT 积分 + irradiance/prefilter 纹理 + PBR 环境项
    // split-sum 合成 → 回读 C++ 同算法复算逐通道校验 可见 IBL、非全黑非饱和）。
    bool t54_recorded = false;
    if (!t54_ibl_selftest_done_) {
        t54_ibl_selftest_done_ = true;
        t54_recorded = RecordIBLSelfTest();
    }
    // Task 5 Subtask 5：每会话一次 WBOIT 自检（两层半透明 accum/reveal MRT + resolve 合成 → 回读 C++
    // 同公式复算逐通道校验 红蓝双层均有贡献）。
    bool t55_recorded = false;
    if (!t55_wboit_selftest_done_) {
        t55_wboit_selftest_done_ = true;
        t55_recorded = RecordWBOITSelfTest();
    }

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(frame_encoder_, nullptr);
    wgpuQueueSubmit(queue_, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    if (ct_recorded) KickComputeSelfTestReadback();
    if (gc_recorded) KickGpuCullSelfTestReadback();
    if (sk_recorded) KickSkinningSelfTestReadback();
    if (si_recorded) KickStorageImageSelfTestReadback();
    if (hz_recorded) KickHiZDownsampleSelfTestReadback();
    if (hzp_recorded) KickHiZPyramidSelfTestReadback();
    if (cb_recorded) KickComputeBindSelfTestReadback();
    if (hc_recorded) KickHiZCullSelfTestReadback();
    if (mf_recorded) KickMorphSelfTestReadback();
    if (dg_recorded) KickDDGISelfTestReadback();
    if (hr_recorded) KickHairSelfTestReadback();
    if (bl_recorded) KickBloomSelfTestReadback();
    if (gr_recorded) KickGrassSelfTestReadback();
    KickDeferredReadback();  // GPU→CPU 异步双缓冲延迟回读：提交后对 pending staging 发 mapAsync
    if (t41_recorded) KickMultiDrawIndirectSelfTestReadback();
    if (t42_recorded) KickMegaVaoSelfTestReadback();
    if (t43_recorded) KickGpuDrivenPBRSelfTestReadback();
    if (t44_recorded) KickGpuDrivenHiZCullSelfTestReadback();
    if (t51_recorded) KickCSMShadowSelfTestReadback();
    if (t52_recorded) KickDeferredSelfTestReadback();
    if (t53_recorded) KickHDRSelfTestReadback();
    if (t54_recorded) KickIBLSelfTestReadback();
    if (t55_recorded) KickWBOITSelfTestReadback();

    wgpuCommandEncoderRelease(frame_encoder_);
    frame_encoder_ = nullptr;
    wgpuTextureViewRelease(backbuffer_view_);
    backbuffer_view_ = nullptr;

    // 提交后 GPU 已不再引用录制期创建的 BindGroup，统一释放（push 缓冲跨帧复用，不在此释放）。
    for (WGPUBindGroup bg : frame_bindgroups_) {
        if (bg) wgpuBindGroupRelease(bg);
    }
    frame_bindgroups_.clear();
}

void WebGPURhiDevice::PresentFrame() {
    // Emscripten 下浏览器在 rAF 回调结束后自动呈现 webgpu 画布上下文；wgpuSwapChainPresent
    // 在 Emscripten 胶水里直接 abort（"unsupported, use requestAnimationFrame"），故不可调用。
}

// ============================================================
// B1 资源映射：真实 WebGPU 缓冲 / 纹理 / 采样器 / 渲染目标
// ============================================================
//
// 句柄表（buffers_/textures_/render_targets_/pipeline_states_/shaders_）把后端无关的
// unsigned int 句柄映射到原生 WGPU 对象。命令缓冲录制（B2，依赖 WGSL 着色器与
// WGPURenderPipeline 组装）在此基础上经 FindBuffer/FindTexture 解析句柄发起绘制。

// --- 内部助手 ---

WGPUSampler WebGPURhiDevice::CreateSampler(const TextureSamplerDesc& desc, uint32_t mip_levels) const {
    WGPUSamplerDescriptor sd{};
    const WGPUAddressMode am = ToAddressMode(desc.wrap);
    sd.addressModeU = am; sd.addressModeV = am; sd.addressModeW = am;
    const WGPUFilterMode fm = ToFilterMode(desc.filter);
    sd.magFilter = fm; sd.minFilter = fm;
    sd.mipmapFilter = (desc.filter == TextureFilter::Linear) ? WGPUMipmapFilterMode_Linear
                                                             : WGPUMipmapFilterMode_Nearest;
    sd.lodMinClamp = 0.0f;
    sd.lodMaxClamp = (mip_levels > 1) ? static_cast<float>(mip_levels) : 32.0f;
    sd.maxAnisotropy = 1;
    sd.compare = WGPUCompareFunction_Undefined;  // 比较采样器（阴影 PCF）由 B2 按需另建
    return wgpuDeviceCreateSampler(device_, &sd);
}

void WebGPURhiDevice::DestroyTextureEntry(TextureEntry& e) {
    if (e.sampler) { wgpuSamplerRelease(e.sampler); e.sampler = nullptr; }
    if (e.view)    { wgpuTextureViewRelease(e.view); e.view = nullptr; }
    if (e.texture && e.owns_texture) { wgpuTextureRelease(e.texture); }
    e.texture = nullptr;
}

unsigned int WebGPURhiDevice::CreateTextureImpl(
        WGPUTextureDimension dim, WGPUTextureViewDimension view_dim,
        uint32_t width, uint32_t height, uint32_t depth_or_layers,
        WGPUTextureFormat format, WGPUTextureUsageFlags usage,
        uint32_t mip_levels, int msaa_samples,
        const std::vector<const unsigned char*>& layer_data,
        const TextureSamplerDesc& sampler) {
    if (!EnsureInitialized() || !device_) return 0;
    width = width > 0 ? width : 1;
    height = height > 0 ? height : 1;
    depth_or_layers = depth_or_layers > 0 ? depth_or_layers : 1;
    mip_levels = mip_levels > 0 ? mip_levels : 1;

    WGPUTextureDescriptor td{};
    td.usage = usage;
    td.dimension = dim;
    td.size = WGPUExtent3D{width, height, depth_or_layers};
    td.format = format;
    td.mipLevelCount = mip_levels;
    td.sampleCount = static_cast<uint32_t>(msaa_samples > 1 ? msaa_samples : 1);
    WGPUTexture tex = wgpuDeviceCreateTexture(device_, &td);
    if (!tex) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateTexture 失败 ({}x{}x{} fmt=0x{:x})",
                        width, height, depth_or_layers, static_cast<unsigned int>(format));
        return 0;
    }

    // mip0 上传（各层 RGBA8 紧打包；nullptr 层跳过，供 RT 附件 / 后续生成）。
    for (uint32_t z = 0; z < layer_data.size() && z < depth_or_layers; ++z) {
        WriteTextureLayerRGBA8(queue_, tex, 0, width, height, z, layer_data[z]);
    }

    WGPUTextureViewDescriptor vd{};
    vd.format = format;
    vd.dimension = view_dim;
    vd.baseMipLevel = 0;
    vd.mipLevelCount = mip_levels;
    vd.baseArrayLayer = 0;
    vd.arrayLayerCount = (dim == WGPUTextureDimension_3D) ? 1u : depth_or_layers;
    vd.aspect = WGPUTextureAspect_All;
    WGPUTextureView view = wgpuTextureCreateView(tex, &vd);

    TextureEntry e;
    e.texture = tex;
    e.view = view;
    e.sampler = CreateSampler(sampler, mip_levels);
    e.format = format;
    e.view_dim = view_dim;
    e.width = width; e.height = height;
    e.depth = (dim == WGPUTextureDimension_3D) ? depth_or_layers : 1;
    e.array_layers = (dim == WGPUTextureDimension_3D) ? 1 : depth_or_layers;
    e.mip_levels = mip_levels;
    e.msaa_samples = msaa_samples;
    e.owns_texture = true;

    const unsigned int h = NextHandle();
    textures_[h] = e;
    return h;
}

// --- 查表 ---

const WebGPURhiDevice::BufferEntry* WebGPURhiDevice::FindBuffer(unsigned int handle) const {
    auto it = buffers_.find(handle);
    return it != buffers_.end() ? &it->second : nullptr;
}
const WebGPURhiDevice::TextureEntry* WebGPURhiDevice::FindTexture(unsigned int handle) const {
    auto it = textures_.find(handle);
    return it != textures_.end() ? &it->second : nullptr;
}
const WebGPURhiDevice::RenderTargetEntry* WebGPURhiDevice::FindRenderTarget(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? &it->second : nullptr;
}
const PipelineStateDesc* WebGPURhiDevice::FindPipelineState(unsigned int handle) const {
    auto it = pipeline_states_.find(handle);
    return it != pipeline_states_.end() ? &it->second : nullptr;
}

// --- 渲染目标 ---

unsigned int WebGPURhiDevice::CreateRenderTarget(const RenderTargetDesc& desc) {
    if (!EnsureInitialized() || !device_) return 0;
    RenderTargetEntry rt;
    rt.width = desc.width > 0 ? desc.width : 1;
    rt.height = desc.height > 0 ? desc.height : 1;
    rt.is_cube = desc.cube_map;
    // 注：MSAA 解析（多重采样颜色 → 单采样可采样纹理）在 B2 渲染 pass 组装时落地；
    //     B1 先以单采样附件成形资源结构（多重采样纹理不可直接 TextureBinding 采样）。
    rt.msaa_samples = 1;

    const TextureSamplerDesc rt_sampler{TextureFilter::Linear, TextureWrap::ClampToEdge};

    if (desc.has_color) {
        WGPUTextureUsageFlags color_usage = WGPUTextureUsage_RenderAttachment |
                                            WGPUTextureUsage_TextureBinding |
                                            WGPUTextureUsage_CopySrc;
        if (desc.allow_uav) color_usage |= WGPUTextureUsage_StorageBinding;
        // 场景颜色用 HDR RGBA16F，与桌面/GL 后端 color=RGBA16F 一致。
        const WGPUTextureFormat color_fmt = WGPUTextureFormat_RGBA16Float;
        const uint32_t mips = desc.generate_mipmaps ? FullMipCount(rt.width, rt.height) : 1;
        const int n = desc.color_attachment_count > 0 ? desc.color_attachment_count : 1;
        for (int i = 0; i < n; ++i) {
            unsigned int th = desc.cube_map
                ? CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                                    rt.width, rt.height, 6, color_fmt, color_usage, mips, 1, {}, rt_sampler)
                : CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                                    rt.width, rt.height, 1, color_fmt, color_usage, mips, 1, {}, rt_sampler);
            if (th) rt.color_textures.push_back(th);
        }
    }

    if (desc.has_depth) {
        const WGPUTextureUsageFlags depth_usage = WGPUTextureUsage_RenderAttachment |
                                                  WGPUTextureUsage_TextureBinding;
        const WGPUTextureFormat depth_fmt = WGPUTextureFormat_Depth32Float;
        rt.depth_texture = desc.cube_map
            ? CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                                rt.width, rt.height, 6, depth_fmt, depth_usage, 1, 1, {}, rt_sampler)
            : CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                                rt.width, rt.height, 1, depth_fmt, depth_usage, 1, 1, {}, rt_sampler);
    }

    const unsigned int h = NextHandle();
    render_targets_[h] = std::move(rt);
    return h;
}

void WebGPURhiDevice::DeleteRenderTarget(unsigned int render_target_handle) {
    auto it = render_targets_.find(render_target_handle);
    if (it == render_targets_.end()) return;
    for (unsigned int th : it->second.color_textures) {
        auto te = textures_.find(th);
        if (te != textures_.end()) { DestroyTextureEntry(te->second); textures_.erase(te); }
    }
    if (it->second.depth_texture) {
        auto te = textures_.find(it->second.depth_texture);
        if (te != textures_.end()) { DestroyTextureEntry(te->second); textures_.erase(te); }
    }
    render_targets_.erase(it);
}

unsigned int WebGPURhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle) const {
    return GetRenderTargetColorTexture(render_target_handle, 0);
}

unsigned int WebGPURhiDevice::GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const {
    const RenderTargetEntry* rt = FindRenderTarget(render_target_handle);
    if (!rt || index < 0 || static_cast<size_t>(index) >= rt->color_textures.size()) return 0;
    return rt->color_textures[index];
}

unsigned int WebGPURhiDevice::GetRenderTargetDepthTexture(unsigned int render_target_handle) const {
    const RenderTargetEntry* rt = FindRenderTarget(render_target_handle);
    return rt ? rt->depth_texture : 0;
}

std::vector<unsigned char> WebGPURhiDevice::ReadRenderTargetColorRgba8(unsigned int render_target_handle) const {
    return ReadRenderTargetColorRgba8WithSize(render_target_handle).pixels;
}

RenderTargetReadback WebGPURhiDevice::ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const {
    // WebGPU 的 GPU→CPU 回读是异步的（texture→staging buffer copy + mapAsync）。在浏览器
    // 主线程同步返回需 ASYNCIFY，B1 不启用。回读供桌面编辑器/CI 像素校验用，Web 运行期渲染
    // 不依赖它；headless WebGPU 回读回归在 B5（Dawn 软件适配器 + onSubmittedWorkDone）落地。
    (void)render_target_handle;
    return {};
}

// --- 纹理 ---

unsigned int WebGPURhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) {
    return CreateTexture2D(width, height, rgba8_data, TextureSamplerDesc::FromLinearFlag(linear_filter));
}

unsigned int WebGPURhiDevice::CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                              const TextureSamplerDesc& sampler) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding |
                                        WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
    return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                             static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
                             WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, {rgba8_data}, sampler);
}

unsigned int WebGPURhiDevice::CreateComputeWriteTexture2D(int width, int height) {
    // B3b-4：可供 compute 写入的 storage image。usage 含 StorageBinding（compute textureStore）
    // + CopySrc（回读/拷贝至下一级金字塔）+ TextureBinding（随后采样）。rgba8unorm 为 WebGPU
    // storage 纹理保证支持的格式。无初始数据。
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_StorageBinding |
                                        WGPUTextureUsage_CopySrc |
                                        WGPUTextureUsage_CopyDst |
                                        WGPUTextureUsage_TextureBinding;
    return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
                             static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
                             WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, {nullptr},
                             TextureSamplerDesc::FromLinearFlag(false));
}

void WebGPURhiDevice::SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) {
    // read_only=false：可写 storage image（textureStore，WriteOnly 访问）→ cur_compute_images_。
    // read_only=true（B3b-5）：只读采样纹理（compute textureLoad，无 sampler）→ cur_compute_textures_。
    // 二者按声明的绑定槽分别收集，使同一 group2 可混合「采样读 src + storage 写 dst」（Hi-Z 下采样模式）。
    if (read_only) {
        if (texture_handle) cur_compute_textures_[binding] = texture_handle;
        else                cur_compute_textures_.erase(binding);
    } else {
        if (texture_handle) cur_compute_images_[binding] = texture_handle;
        else                cur_compute_images_.erase(binding);
    }
}

void WebGPURhiDevice::SetComputeImageViewExplicit(uint32_t binding, WGPUTextureView view,
                                                  WGPUTextureFormat format,
                                                  WGPUTextureViewDimension view_dim, bool read_only) {
    // B3b-6：直接绑定显式纹理视图（如 Hi-Z 金字塔的单 mip 视图），绕开「句柄→默认全 mip 视图」。
    // read_only=true→采样读（textureLoad）；false→storage 写（textureStore）。
    auto& m = read_only ? cur_compute_texture_views_ : cur_compute_image_views_;
    if (view) m[binding] = ComputeViewBind{view, format, view_dim};
    else      m.erase(binding);
}

void WebGPURhiDevice::InvalidateComputeMipViews(unsigned int texture_handle) {
    // 删纹理时清该句柄下全部缓存的单 mip 视图（key 高 32 位为句柄）。
    const uint64_t lo = static_cast<uint64_t>(texture_handle) << 16;
    const uint64_t hi = lo + 0xFFFF;
    for (auto it = compute_mip_views_.begin(); it != compute_mip_views_.end();) {
        if (it->first >= lo && it->first <= hi) {
            if (it->second) wgpuTextureViewRelease(it->second);
            it = compute_mip_views_.erase(it);
        } else {
            ++it;
        }
    }
}

void WebGPURhiDevice::SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                                int mip_level, bool read_only, bool r32f) {
    // B3b-7：引擎 Hi-Z build 真实绑定面——按句柄 + mip 级绑定单层单 mip 视图到 compute group2 槽。
    // 同一槽可在相邻 dispatch 间在「采样读 / storage 写」间切换（Hi-Z：先写 mip0，再逐级读 mip[k-1]
    // 写 mip[k]），且无 DispatchCompute 间自动 ResetDrawState，故先从「读/写」两映射均擦除该槽，
    // 再按本次 read_only 路由，避免上一次 dispatch 的陈旧绑定残留同槽。
    cur_compute_texture_views_.erase(binding);
    cur_compute_image_views_.erase(binding);
    if (!texture_handle) return;

    const TextureEntry* e = FindTexture(texture_handle);
    if (!e || !e->texture) return;
    if (mip_level < 0) mip_level = 0;
    if (static_cast<uint32_t>(mip_level) >= e->mip_levels) return;

    const WGPUTextureFormat fmt = r32f ? WGPUTextureFormat_R32Float : e->format;
    const uint64_t key = (static_cast<uint64_t>(texture_handle) << 16) |
                         static_cast<uint64_t>(static_cast<uint32_t>(mip_level));
    WGPUTextureView view = nullptr;
    auto cit = compute_mip_views_.find(key);
    if (cit != compute_mip_views_.end()) {
        view = cit->second;
    } else {
        WGPUTextureViewDescriptor vd{};
        vd.format          = fmt;
        vd.dimension       = WGPUTextureViewDimension_2D;
        vd.baseMipLevel    = static_cast<uint32_t>(mip_level);
        vd.mipLevelCount   = 1;
        vd.baseArrayLayer  = 0;
        vd.arrayLayerCount = 1;
        vd.aspect          = WGPUTextureAspect_All;
        view = wgpuTextureCreateView(e->texture, &vd);
        if (!view) return;
        compute_mip_views_[key] = view;
    }
    SetComputeImageViewExplicit(binding, view, fmt, WGPUTextureViewDimension_2D, read_only);
}

void WebGPURhiDevice::SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
    // B3b-8：引擎 compute 采样面（Hi-Z/GPU-driven 剔除读 Hi-Z/深度纹理）。WebGPU 路由到只读采样
    //   绑定（group2 binding=unit，texture_2d<f32>，手译 WGSL 用 textureLoad 取代 textureLod）。
    //   同槽先清显式视图 / storage image 绑定，避免上一次 dispatch 的陈旧绑定残留同槽。
    cur_compute_image_views_.erase(unit);
    cur_compute_texture_views_.erase(unit);
    cur_compute_images_.erase(unit);
    if (!texture_handle) { cur_compute_textures_.erase(unit); return; }
    cur_compute_textures_[unit] = texture_handle;
}

// B3b-8 命名 compute uniform：按调用序 16B 对齐定位（与 GL/DX11/Vulkan 同方案）。名字首见时分配下
//   一个 16B 对齐偏移并按 data_size 推进游标；整块经 UBO 版本环上传到 group1 保留 binding。
size_t WebGPURhiDevice::GetOrCreateComputeNamedOffset(const char* name, size_t data_size) {
    auto it = compute_named_offsets_.find(name);
    if (it != compute_named_offsets_.end()) return it->second;
    const size_t offset = (compute_named_next_ + 15) & ~size_t(15);
    compute_named_offsets_[name] = offset;
    compute_named_next_ = offset + data_size;
    return offset;
}

void WebGPURhiDevice::WriteComputeNamedStaging(size_t offset, const void* data, size_t size) {
    if (compute_named_staging_.size() < offset + size) compute_named_staging_.resize(offset + size, 0);
    std::memcpy(compute_named_staging_.data() + offset, data, size);
}

void WebGPURhiDevice::SetComputeUniformInt(unsigned int shader, const char* name, int value) {
    (void)shader;
    if (!name) return;
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(int)), &value, sizeof(int));
}

void WebGPURhiDevice::SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
    (void)shader;
    if (!name) return;
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(float)), &value, sizeof(float));
}

void WebGPURhiDevice::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
    (void)shader;
    if (!name) return;
    const int d[2]{x, y};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPURhiDevice::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
    (void)shader;
    if (!name) return;
    const float d[2]{x, y};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPURhiDevice::SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) {
    (void)shader;
    if (!name) return;
    const float d[3]{x, y, z};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPURhiDevice::SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) {
    (void)shader;
    if (!name) return;
    const int d[3]{x, y, z};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPURhiDevice::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
    (void)shader;
    if (!name) return;
    const float d[4]{x, y, z, w};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPURhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    (void)shader;
    if (!name || !data) return;
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, 64), data, 64);
}

unsigned int WebGPURhiDevice::CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    std::vector<const unsigned char*> faces(6, nullptr);
    if (rgba8_faces) for (int i = 0; i < 6; ++i) faces[i] = rgba8_faces[i];
    return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
                             static_cast<uint32_t>(width), static_cast<uint32_t>(height), 6,
                             WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, faces,
                             TextureSamplerDesc::FromLinearFlag(linear_filter));
}

unsigned int WebGPURhiDevice::CreateTextureCubeWithMips(const std::vector<CubeMipLevel>& mips, bool linear_filter) {
    if (mips.empty()) return 0;
    const uint32_t mip_count = static_cast<uint32_t>(mips.size());
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    // 先建带完整 mip 链的 cube（不在 Impl 内上传），再逐 mip 逐面写入。
    const unsigned int h = CreateTextureImpl(
        WGPUTextureDimension_2D, WGPUTextureViewDimension_Cube,
        static_cast<uint32_t>(mips[0].width), static_cast<uint32_t>(mips[0].height), 6,
        WGPUTextureFormat_RGBA8Unorm, usage, mip_count, 1, {},
        TextureSamplerDesc::FromLinearFlag(linear_filter));
    const TextureEntry* e = FindTexture(h);
    if (!e || !e->texture) return h;
    for (uint32_t m = 0; m < mip_count; ++m) {
        const uint32_t w = static_cast<uint32_t>(mips[m].width > 0 ? mips[m].width : 1);
        const uint32_t ht = static_cast<uint32_t>(mips[m].height > 0 ? mips[m].height : 1);
        for (uint32_t f = 0; f < 6; ++f) {
            WriteTextureLayerRGBA8(queue_, e->texture, m, w, ht, f, mips[m].faces[f]);
        }
    }
    return h;
}

unsigned int WebGPURhiDevice::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    const WGPUTextureUsageFlags usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    const unsigned int h = CreateTextureImpl(
        WGPUTextureDimension_3D, WGPUTextureViewDimension_3D,
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth),
        WGPUTextureFormat_RGBA8Unorm, usage, 1, 1, {},
        TextureSamplerDesc::FromLinearFlag(linear_filter));
    const TextureEntry* e = FindTexture(h);
    if (e && e->texture && rgba8_data && width > 0 && height > 0 && depth > 0) {
        // 3D 体一次性整卷上传（rowsPerImage=height，writeSize.depth=depth）。
        WGPUImageCopyTexture dst{};
        dst.texture = e->texture;
        dst.mipLevel = 0;
        dst.origin = WGPUOrigin3D{0, 0, 0};
        dst.aspect = WGPUTextureAspect_All;
        WGPUTextureDataLayout layout{};
        layout.offset = 0;
        layout.bytesPerRow = static_cast<uint32_t>(width) * 4u;
        layout.rowsPerImage = static_cast<uint32_t>(height);
        WGPUExtent3D extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                            static_cast<uint32_t>(depth)};
        wgpuQueueWriteTexture(queue_, &dst, rgba8_data,
                              static_cast<size_t>(width) * height * depth * 4u, &layout, &extent);
    }
    return h;
}

void WebGPURhiDevice::DeleteTexture(unsigned int texture_handle) {
    auto it = textures_.find(texture_handle);
    if (it == textures_.end()) return;
    InvalidateComputeMipViews(texture_handle);  // B3b-7：先释放该句柄缓存的单 mip 视图
    DestroyTextureEntry(it->second);
    textures_.erase(it);
}

// --- 着色器 / 管线状态 ---

unsigned int WebGPURhiDevice::CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) {
    // B2：以 sentinel 行 `// dse-wgsl`（允许前导空白）区分两类着色器：
    //   - WGSL（内建/自检程序）：vert_src 即整段 WGSL module（含 vs_main/fs_main），编译出 module。
    //   - 引擎 GLSL：无离线 GLSL→WGSL 工具，故不转译、module 留空，其绘制在录制期被优雅跳过。
    ShaderEntry e;
    e.vert_src = vert_src;
    e.frag_src = frag_src;

    const char* kSentinel = "// dse-wgsl";
    const size_t first = vert_src.find_first_not_of(" \t\r\n");
    const bool is_wgsl = first != std::string::npos &&
                         vert_src.compare(first, std::strlen(kSentinel), kSentinel) == 0;
    if (is_wgsl) {
        e.module = CompileWGSL(vert_src, "dse-wgsl-program");
        // 单一 module 同时承载 vs/fs 入口；无 fs_main 视为仅深度 pass（无片元阶段）。
        e.has_fragment = vert_src.find("fn fs_main") != std::string::npos ||
                         vert_src.find("fn " + e.fs_entry) != std::string::npos;
        // 解析 WGSL 实际声明的 `@group(N) @binding(M)`，供 explicit layout/BindGroup 过滤
        //（仅纳入着色器真正使用的绑定，避免引擎多绑资源超 per-stage 上限 / 与着色器用量不符）。
        ParseWgslBindings(vert_src, e.wgsl_bindings);
        if (!e.module) {
            DEBUG_LOG_ERROR("WebGPU: WGSL 着色器编译失败（module 为空），该程序绘制将被跳过");
        }
    }

    const unsigned int h = NextHandle();
    shaders_[h] = std::move(e);
    return h;
}

void WebGPURhiDevice::DeleteShaderProgram(unsigned int program_handle) {
    auto it = shaders_.find(program_handle);
    if (it == shaders_.end()) return;
    if (it->second.module) wgpuShaderModuleRelease(it->second.module);
    shaders_.erase(it);
}

unsigned int WebGPURhiDevice::CreatePipelineState(const PipelineStateDesc& desc) {
    // B1：登记 PSO 子状态（光栅/混合/深度/拓扑）。WGPURenderPipeline 在 B2 由
    // (pso, program, RT 颜色/深度格式, 顶点布局) 惰性组装并缓存（着色器就绪后）。
    const unsigned int h = NextHandle();
    pipeline_states_[h] = desc;
    return h;
}

// ============================================================
// B2b/B2c 内建 WGSL 程序（手写；经通用原语上屏）
// ============================================================
//
// 绑定约定（与 CollectGroupBindings 一致）：
//   group0：push 常量（binding0=VS / binding1=FS）  group1：UBO @binding=slot
//   group2：纹理 @binding=slot*2、采样器 @binding=slot*2+1  group3：SSBO @binding=slot
// 着色器只需声明其实际使用的绑定子集；BGL 可含更多条目（WebGPU 允许 layout ⊋ 着色器用量）。

namespace {

// 全屏 quad 直拷（copy/passthrough/fxaa 等）：源纹理在 slot0 → group2 binding0/1。
// 顶点来自 PostProcessRenderer 的 PPVertex：pos(vec2)@0、uv(vec2)@1（clip-space）。
const char* kWgslFullscreenBlit = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);  // GL 风格全屏 quad 在 WebGPU(top-left 纹理原点)需翻转 V
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return textureSample(src_tex, src_smp, i.uv);
}
)WGSL";

// 全屏合成（bloom_composite/tonemapping/ssao_apply）：采样 HDR 场景 → Reinhard tonemap + sRGB。
const char* kWgslComposite = R"WGSL(// dse-wgsl
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);  // GL 风格全屏 quad 在 WebGPU(top-left 纹理原点)需翻转 V
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let hdr = textureSample(src_tex, src_smp, i.uv).rgb;
  let mapped = hdr / (hdr + vec3<f32>(1.0));
  let srgb = pow(mapped, vec3<f32>(1.0 / 2.2));
  return vec4<f32>(srgb, 1.0);
}
)WGSL";

// ============================================================
// B-2 HDR-bloom 链（WebGPU 全屏 quad，镜像 engine/render/shaders/src/bloom_*.frag）
// ============================================================
// 渲染器（PostProcessRenderer/BloomRenderer）在无 compute 后端走全屏 quad：
//   bloom_extract → bloom_downsample×N → bloom_upsample×N → bloom_composite。
// 源纹理一律在 slot0 → group2 binding0/1；标量参数经 FS push（cmd.PushConstants Fragment）
//   → group0 binding1 的 uniform。各 PP 程序逐句镜像对应 .frag。V 翻转与 kWgslComposite 一致：
//   每趟均为「翻转 = 保向拷贝」，故整链方向一致、bloom 与场景在 composite 对齐。

// Bloom 亮度提取（镜像 bloom_extract.frag）：软膝阈值仅保留高光。参数 {threshold, knee}。
const char* kWgslBloomExtract = R"WGSL(// dse-wgsl
struct PC { p : vec4<f32> };  // p.x=threshold, p.y=knee
@group(0) @binding(1) var<uniform> pc : PC;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let color = textureSample(src_tex, src_smp, i.uv).rgb;
  let brightness = dot(color, vec3<f32>(0.2126, 0.7152, 0.0722));
  var soft = brightness - (pc.p.x - pc.p.y);
  soft = clamp(soft / (2.0 * pc.p.y + 0.0001), 0.0, 1.0);
  soft = soft * soft;
  let contribution = max(soft, step(pc.p.x, brightness));
  return vec4<f32>(color * contribution, 1.0);
}
)WGSL";

// Bloom 13-tap 降采样（镜像 bloom_downsample.frag）。参数 {srcResX, srcResY}。
const char* kWgslBloomDownsample = R"WGSL(// dse-wgsl
struct PC { p : vec4<f32> };  // p.x=srcResX, p.y=srcResY
@group(0) @binding(1) var<uniform> pc : PC;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
fn S(uv : vec2<f32>) -> vec3<f32> { return textureSample(src_tex, src_smp, uv).rgb; }
@fragment fn fs_main(inp : VsOut) -> @location(0) vec4<f32> {
  let x = 1.0 / pc.p.x;
  let y = 1.0 / pc.p.y;
  let uv = inp.uv;
  let a = S(vec2<f32>(uv.x - 2.0*x, uv.y + 2.0*y));
  let b = S(vec2<f32>(uv.x,         uv.y + 2.0*y));
  let c = S(vec2<f32>(uv.x + 2.0*x, uv.y + 2.0*y));
  let d = S(vec2<f32>(uv.x - 2.0*x, uv.y));
  let e = S(vec2<f32>(uv.x,         uv.y));
  let f = S(vec2<f32>(uv.x + 2.0*x, uv.y));
  let g = S(vec2<f32>(uv.x - 2.0*x, uv.y - 2.0*y));
  let h = S(vec2<f32>(uv.x,         uv.y - 2.0*y));
  let ii = S(vec2<f32>(uv.x + 2.0*x, uv.y - 2.0*y));
  let j = S(vec2<f32>(uv.x - x, uv.y + y));
  let k = S(vec2<f32>(uv.x + x, uv.y + y));
  let l = S(vec2<f32>(uv.x - x, uv.y - y));
  let m = S(vec2<f32>(uv.x + x, uv.y - y));
  var ds = e * 0.125;
  ds = ds + (a + c + g + ii) * 0.03125;
  ds = ds + (b + d + f + h) * 0.0625;
  ds = ds + (j + k + l + m) * 0.125;
  return vec4<f32>(ds, 1.0);
}
)WGSL";

// Bloom 3x3 帐篷升采样（镜像 bloom_upsample.frag）。参数 {filterRadius, blendWeight}；
// 由调用方（BloomRenderer::Upsample）开 alpha 混合累加。
const char* kWgslBloomUpsample = R"WGSL(// dse-wgsl
struct PC { p : vec4<f32> };  // p.x=filterRadius, p.y=blendWeight
@group(0) @binding(1) var<uniform> pc : PC;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);
  return o;
}
@group(2) @binding(0) var src_tex : texture_2d<f32>;
@group(2) @binding(1) var src_smp : sampler;
fn S(uv : vec2<f32>) -> vec3<f32> { return textureSample(src_tex, src_smp, uv).rgb; }
@fragment fn fs_main(inp : VsOut) -> @location(0) vec4<f32> {
  let x = pc.p.x;
  let y = pc.p.x;
  let uv = inp.uv;
  let a = S(vec2<f32>(uv.x - x, uv.y + y));
  let b = S(vec2<f32>(uv.x,     uv.y + y));
  let c = S(vec2<f32>(uv.x + x, uv.y + y));
  let d = S(vec2<f32>(uv.x - x, uv.y));
  let e = S(vec2<f32>(uv.x,     uv.y));
  let f = S(vec2<f32>(uv.x + x, uv.y));
  let g = S(vec2<f32>(uv.x - x, uv.y - y));
  let h = S(vec2<f32>(uv.x,     uv.y - y));
  let ii = S(vec2<f32>(uv.x + x, uv.y - y));
  var us = e * 4.0;
  us = us + (b + d + f + h) * 2.0;
  us = us + (a + c + g + ii);
  us = us * (1.0 / 16.0);
  return vec4<f32>(us * pc.p.y, pc.p.y);
}
)WGSL";

// Bloom 合成（镜像 bloom_composite_ssao_ae.frag 的 demo 子集）：场景 + bloom*intensity →
// ACES(色*exposure) → gamma → 可选 vignette → IGN dither。SSAO/AE/LUT/CS/film-grain 在 demo
// 全关，故不声明其纹理绑定（声明未绑定纹理会被 GetOrCreateRenderPipeline 判缺而跳过 draw）。
// 场景在 slot0 → group2 binding0/1；bloom 经 req.Tex(2) → slot1 → group2 binding2/3。
const char* kWgslBloomComposite = R"WGSL(// dse-wgsl
struct PC {
  p0 : vec4<f32>,  // exposure, bloomIntensity, bloomEnabled, ssaoEnabled
  p1 : vec4<f32>,  // autoExposureEnabled, lutEnabled, lutIntensity, csEnabled
  p2 : vec4<f32>,  // csStrength, vignetteEnabled, vignetteIntensity, vignetteRadius
  p3 : vec4<f32>,  // vignetteSoftness, filmGrainEnabled, filmGrainIntensity, filmGrainTime
};
@group(0) @binding(1) var<uniform> pc : PC;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) uv : vec2<f32> };
@vertex fn vs_main(@location(0) p : vec2<f32>, @location(1) uv : vec2<f32>) -> VsOut {
  var o : VsOut;
  o.pos = vec4<f32>(p, 0.0, 1.0);
  o.uv = vec2<f32>(uv.x, 1.0 - uv.y);
  return o;
}
@group(2) @binding(0) var scene_tex : texture_2d<f32>;
@group(2) @binding(1) var scene_smp : sampler;
@group(2) @binding(2) var bloom_tex : texture_2d<f32>;
@group(2) @binding(3) var bloom_smp : sampler;
fn AcesFilmic(v : vec3<f32>) -> vec3<f32> {
  let a = 2.51; let b = 0.03; let c = 2.43; let d = 0.59; let e = 0.14;
  return clamp((v * (a * v + b)) / (v * (c * v + d) + e), vec3<f32>(0.0), vec3<f32>(1.0));
}
@fragment fn fs_main(inp : VsOut) -> @location(0) vec4<f32> {
  var color = textureSample(scene_tex, scene_smp, inp.uv).rgb;
  if (pc.p0.z != 0.0) {
    let bloomColor = textureSample(bloom_tex, bloom_smp, inp.uv).rgb;
    color = color + bloomColor * pc.p0.y;
  }
  color = AcesFilmic(color * pc.p0.x);
  color = pow(color, vec3<f32>(1.0 / 2.2));
  if (pc.p2.y != 0.0) {
    let dist = length(inp.uv - vec2<f32>(0.5));
    let radius = clamp(pc.p2.w, 0.001, 1.5);
    let softness = max(pc.p3.x, 0.0001);
    let vig = 1.0 - smoothstep(radius, radius + softness, dist);
    color = color * mix(1.0, vig, clamp(pc.p2.z, 0.0, 1.0));
  }
  let ign = fract(52.9829189 * fract(0.06711056 * inp.pos.x + 0.00583715 * inp.pos.y));
  color = color + vec3<f32>((ign - 0.5) / 255.0);
  return vec4<f32>(color, 1.0);
}
)WGSL";

// 前向着色（ForwardPbr/ForwardShaded）：顶点已 CPU 预变换到世界空间（见 MeshRenderer），
// 仅需 PerFrame.vp 投影；方向光 Lambert + PerMaterial.albedo + albedo 纹理（slot0）。
// 进阶特征（CSM/SSS/clearcoat/clustered/DDGI/...）留 B2c+；BGL 含全部 8 UBO/20 纹理槽，
// 本着色器仅取其用到的子集（WebGPU 允许）。
const char* kWgslForward = R"WGSL(// dse-wgsl
struct PerFrame {
  vp : mat4x4<f32>,
  view : mat4x4<f32>,
  camera_pos : vec4<f32>,
  foliage_wind : vec4<f32>,
  foliage_push : vec4<f32>,
};
struct PerScene {
  light_dir_and_enabled : vec4<f32>,
  light_color_and_ambient : vec4<f32>,
  light_params : vec4<f32>,
};
struct PerMaterial {
  albedo : vec4<f32>,
  roughness_ao : vec4<f32>,
  emissive : vec4<f32>,
  flags : vec4<f32>,
};
@group(1) @binding(0) var<uniform> per_frame : PerFrame;
@group(1) @binding(1) var<uniform> per_scene : PerScene;
@group(1) @binding(2) var<uniform> per_material : PerMaterial;
@group(2) @binding(0) var albedo_tex : texture_2d<f32>;
@group(2) @binding(1) var albedo_smp : sampler;

struct VsOut {
  @builtin(position) clip : vec4<f32>,
  @location(0) nrm : vec3<f32>,
  @location(1) uv : vec2<f32>,
  @location(2) col : vec4<f32>,
};
@vertex fn vs_main(
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
) -> VsOut {
  var o : VsOut;
  o.clip = per_frame.vp * vec4<f32>(pos, 1.0);
  o.nrm = normal;
  o.uv = uv;
  o.col = color;
  return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let tex = textureSample(albedo_tex, albedo_smp, i.uv);
  let base = tex.rgb * per_material.albedo.rgb * i.col.rgb;
  let n = normalize(i.nrm);
  let l = normalize(per_scene.light_dir_and_enabled.xyz);
  let ndl = max(dot(n, l), 0.0);
  let enabled = per_scene.light_dir_and_enabled.w;
  let ambient = per_scene.light_color_and_ambient.w;
  let light_col = per_scene.light_color_and_ambient.rgb;
  let intensity = per_scene.light_params.x;
  let diffuse = light_col * (ndl * intensity * enabled);
  let lit = base * (vec3<f32>(ambient) + diffuse);
  let emissive = per_material.emissive.rgb;
  return vec4<f32>(lit + emissive, 1.0);
}
)WGSL";

// 进阶前向着色（ForwardShaded / DrawShaded）：移植 forward_shaded.frag 的特性子集——
//   shading_mode（0 PBR Cook-Torrance / 2 半兰伯特皮肤 / 3 半兰伯特静态 / 4 Toon / Unlit）、
//   SSS、clearcoat、clustered 点光（set3 PointLightUBO ≤64）、CSM 方向光阴影（set1 light_space_matrices
//   + shadow atlas，flat unit11 → group2 binding22/23）。UBO 逐字段对齐 mesh_renderer.cpp 的
//   FwdShadedMaterialUBO(160B)/FwdPerSceneUBO(560B)/PointLightsUBO std140 布局。
// 未纳入子集（白默认纹理/关闭 → 与引擎同结果，留后续）：法线贴图/POM、splatmap/积雪、MR/自发光/AO
//   贴图、DDGI/SH 间接光、聚光灯、anisotropy、watercolor(5)/faceSDF(6)。
// 顶点已 CPU 预变换到世界空间（位置/法线/切线，见 BuildShadedWorldVertexBuffer）。前向输出线性 HDR
//   （不在此 tonemap），由 composite（kWgslComposite）统一 Reinhard + sRGB。
const char* kWgslForwardShaded = R"WGSL(// dse-wgsl
const PI : f32 = 3.14159265359;
struct PerFrame {
  vp : mat4x4<f32>,
  view : mat4x4<f32>,
  camera_pos : vec4<f32>,
  foliage_wind : vec4<f32>,
  foliage_push : vec4<f32>,
};
struct PerScene {
  light_dir_and_enabled : vec4<f32>,
  light_color_and_ambient : vec4<f32>,
  light_params : vec4<f32>,             // x=intensity, y=shadow_strength, z=receive_shadow
  cascade_splits : vec4<f32>,
  light_space_matrices : array<mat4x4<f32>, 3>,
  shadow_atlas_regions : array<vec4<f32>, 3>,
  spot_light_space_matrices : array<mat4x4<f32>, 4>,
};
struct PerMaterial {
  albedo : vec4<f32>,        // xyz=base, w=metallic
  roughness_ao : vec4<f32>,  // x=rough, y=ao, z=normal_strength, w=alpha_cutoff
  emissive : vec4<f32>,      // xyz=emissive, w=alpha_test_on
  flags : vec4<f32>,         // x=normal_map, y=mr_map, z=emissive_map, w=occlusion_map
  mode_params : vec4<f32>,   // x=shading_mode, y=double_sided, z=anisotropy, w=pom
  sss : vec4<f32>,           // xyz=tint, w=strength
  clearcoat : vec4<f32>,     // x=coat, y=coat_roughness
  toon_shadow : vec4<f32>,   // xyz=shadow_color, w=threshold
  toon_params : vec4<f32>,   // x=softness, y=spec_size, z=spec_strength, w=rim
  watercolor : vec4<f32>,
};
struct PointLight {
  color : vec3<f32>, intensity : f32,
  position : vec3<f32>, radius : f32,
  cast_shadow : i32, shadow_index : i32, pad : vec2<f32>,
};
struct PointLights {
  count : i32, p0 : i32, p1 : i32, p2 : i32,
  lights : array<PointLight, 64>,
};
@group(1) @binding(0) var<uniform> per_frame : PerFrame;
@group(1) @binding(1) var<uniform> per_scene : PerScene;
@group(1) @binding(2) var<uniform> per_material : PerMaterial;
@group(1) @binding(3) var<uniform> point_lights : PointLights;
@group(2) @binding(0) var albedo_tex : texture_2d<f32>;
@group(2) @binding(1) var albedo_smp : sampler;
// 5.1b：CSM shadow atlas（flat unit11 → group2 binding22）以 texture_depth_2d 采样（textureLoad 手动
//   3×3 PCF，无需采样器）。消费方 mesh_renderer 在无 shadow map 时给 slot11 绑「白 RGBA8」回退（Float），
//   WebGPU 后端在 CollectGroupBindings 处把该回退替换为恒亮 Depth32 回退纹理（深度=1→无遮挡），使本
//   binding 的 sampleType 在所有 draw 上恒为 Depth、与此处声明一致（消除 BGL sampleType 冲突）。真实
//   遮挡逻辑见下方 DirectionalShadow / SampleCascadeShadow（移植 forward_shaded.frag，离屏自检 T5-1 已验证）。
@group(2) @binding(22) var shadow_atlas : texture_depth_2d;

fn DistributionGGX(N : vec3<f32>, H : vec3<f32>, roughness : f32) -> f32 {
  let a = roughness * roughness;
  let a2 = a * a;
  let NdotH = max(dot(N, H), 0.0);
  let denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
  return a2 / max(PI * denom * denom, 1e-7);
}
fn GeometrySchlickGGX(NdotV : f32, roughness : f32) -> f32 {
  let r = roughness + 1.0;
  let k = (r * r) / 8.0;
  return NdotV / (NdotV * (1.0 - k) + k);
}
fn GeometrySmith(N : vec3<f32>, V : vec3<f32>, L : vec3<f32>, roughness : f32) -> f32 {
  return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
       * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}
fn FresnelSchlick(cosTheta : f32, F0 : vec3<f32>) -> vec3<f32> {
  return F0 + (vec3<f32>(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// CSM 方向光阴影：移植 forward_shaded.frag 的 SampleCascadeShadow + DirectionalShadow（textureLoad 版
//   手动 3×3 PCF，逻辑同离屏自检 T5-1）。slot11 由后端保证恒为 Depth32（真 atlas 或恒亮回退纹理）。
fn SampleCascadeShadow(idx : i32, worldPos : vec3<f32>, bias : f32) -> f32 {
  let lp = per_scene.light_space_matrices[idx] * vec4<f32>(worldPos, 1.0);
  if (lp.w <= 1e-5) { return 0.0; }
  var pc = lp.xyz / lp.w;
  pc = pc * 0.5 + 0.5;
  if (pc.z > 1.0) { return 0.0; }
  if (pc.x < 0.0 || pc.x > 1.0 || pc.y < 0.0 || pc.y > 1.0) { return 0.0; }
  let region = per_scene.shadow_atlas_regions[idx];
  let uv = pc.xy * region.xy + region.zw;
  let dims = vec2<i32>(textureDimensions(shadow_atlas, 0));
  let base = vec2<i32>(uv * vec2<f32>(dims));
  var occ = 0.0;
  for (var x = -1; x <= 1; x = x + 1) {
    for (var y = -1; y <= 1; y = y + 1) {
      let c = clamp(base + vec2<i32>(x, y), vec2<i32>(0, 0), dims - vec2<i32>(1, 1));
      let d = textureLoad(shadow_atlas, c, 0);
      occ = occ + select(0.0, 1.0, (pc.z - bias) > d);
    }
  }
  return occ / 9.0;
}
fn DirectionalShadow(worldPos : vec3<f32>, N : vec3<f32>, L : vec3<f32>) -> f32 {
  if (per_scene.light_params.z < 0.5) { return 0.0; }   // receive_shadow 关
  let viewDepth = abs((per_frame.view * vec4<f32>(worldPos, 1.0)).z);
  var idx : i32 = 2;                                     // CSM_CASCADES - 1
  if (viewDepth < per_scene.cascade_splits.x) { idx = 0; }
  else if (viewDepth < per_scene.cascade_splits.y) { idx = 1; }
  let bias = max(0.0025 * (1.0 - dot(N, L)), 0.0004);
  let shadow = SampleCascadeShadow(idx, worldPos, bias);
  return clamp(shadow * per_scene.light_params.y, 0.0, 1.0);  // y = shadow_strength
}
fn PointLightsLo(N : vec3<f32>, V : vec3<f32>, world_pos : vec3<f32>,
                 surface_albedo : vec3<f32>, roughness : f32, metallic : f32, F0 : vec3<f32>) -> vec3<f32> {
  var sum = vec3<f32>(0.0);
  let n = point_lights.count;
  for (var i = 0; i < n; i = i + 1) {
    let pl = point_lights.lights[i];
    let d = pl.position - world_pos;
    let dist = length(d);
    let L = d / max(dist, 1e-4);
    let atten = clamp(1.0 - (dist * dist) / (pl.radius * pl.radius + 1e-4), 0.0, 1.0);
    let radiance = pl.color * pl.intensity * atten;
    let H = normalize(V + L);
    let NDF = DistributionGGX(N, H, roughness);
    let G = GeometrySmith(N, V, L, roughness);
    let F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    let spec = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
    sum = sum + (kD * surface_albedo / PI + spec) * radiance * max(dot(N, L), 0.0);
  }
  return sum;
}
fn SubsurfaceScattering(N : vec3<f32>, L : vec3<f32>, alb : vec3<f32>, sss_s : f32,
                        light_col : vec3<f32>, li : f32, tint : vec3<f32>) -> vec3<f32> {
  let wrap = 0.5 * sss_s;
  let wrapped = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
  let diff = wrapped - max(dot(N, L), 0.0);
  var sss_tint = tint;
  if (dot(tint, tint) <= 0.0) { sss_tint = vec3<f32>(1.0, 0.35, 0.2); }
  return alb * sss_tint * diff * light_col * li;
}

struct VsOut {
  @builtin(position) clip : vec4<f32>,
  @location(0) nrm : vec3<f32>,
  @location(1) uv : vec2<f32>,
  @location(2) col : vec4<f32>,
  @location(3) wpos : vec3<f32>,
};
@vertex fn vs_main(
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
) -> VsOut {
  var o : VsOut;
  o.clip = per_frame.vp * vec4<f32>(pos, 1.0);
  o.nrm = normal;
  o.uv = uv;
  o.col = color;
  o.wpos = pos;
  return o;
}
@fragment fn fs_main(i : VsOut, @builtin(front_facing) ff : bool) -> @location(0) vec4<f32> {
  let shading_mode = i32(per_material.mode_params.x + 0.5);
  let double_sided = per_material.mode_params.y > 0.5;
  var N = normalize(i.nrm);
  if (double_sided && !ff) { N = -N; }
  let V = normalize(per_frame.camera_pos.xyz - i.wpos);

  let texColor = textureSample(albedo_tex, albedo_smp, i.uv) * i.col;
  // alpha-test（emissive.w = 开关，roughness_ao.w = cutoff）。
  if (per_material.emissive.w > 0.5 && texColor.a < clamp(per_material.roughness_ao.w, 0.0, 1.0)) { discard; }

  let light_color = per_scene.light_color_and_ambient.xyz;
  let ambient = per_scene.light_color_and_ambient.w;
  let light_intensity = per_scene.light_params.x;
  let lighting_enabled = per_scene.light_dir_and_enabled.w > 0.5;
  let L = normalize(per_scene.light_dir_and_enabled.xyz);
  let shadow = DirectionalShadow(i.wpos, N, L);

  var color = vec3<f32>(0.0);
  var out_alpha = texColor.a;

  if (!lighting_enabled) {
    color = texColor.rgb * per_material.albedo.xyz;
  } else if (shading_mode == 2) {
    let hl = dot(N, L) * 0.5 + 0.5;
    let base_color = texColor.rgb * per_material.albedo.xyz;
    color = base_color * light_color * (hl * light_intensity * (1.0 - shadow) + ambient * 0.5);
    out_alpha = 1.0;
  } else if (shading_mode == 3) {
    let R = reflect(-L, N);
    let hl = dot(N, L) * 0.5 + 0.5;
    let diffuse = per_material.albedo.xyz * hl * light_color * light_intensity * (1.0 - shadow);
    let spec_power = max(per_material.roughness_ao.x, 1.0);
    let specular = vec3<f32>(per_material.albedo.w) * pow(max(dot(R, V), 0.0), spec_power);
    color = (diffuse + specular + per_material.emissive.xyz) * texColor.rgb;
  } else if (shading_mode == 4) {
    let H = normalize(L + V);
    let NdotL = dot(N, L) * 0.5 + 0.5;
    let soft = per_material.toon_params.x;
    let band1 = smoothstep(per_material.toon_shadow.w - soft, per_material.toon_shadow.w + soft, NdotL);
    let band2 = smoothstep(0.7 - soft, 0.7 + soft, NdotL);
    let cel = band1 * 0.7 + band2 * 0.3;
    let baseColor = texColor.rgb * per_material.albedo.xyz;
    let shadowColor = baseColor * per_material.toon_shadow.xyz;
    var diffuse = mix(shadowColor, baseColor * light_color, cel);
    diffuse = mix(shadowColor, diffuse, 1.0 - shadow);
    let spec = step(per_material.toon_params.y, max(dot(N, H), 0.0)) * per_material.toon_params.z;
    let rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * per_material.toon_params.w;
    color = diffuse + light_color * spec * (1.0 - shadow) + vec3<f32>(rim);
  } else {
    // 默认 PBR（Cook-Torrance）+ SSS / clearcoat / clustered 点光。
    let surface_albedo = pow(texColor.rgb * per_material.albedo.xyz, vec3<f32>(2.2));
    let metallic = clamp(per_material.albedo.w, 0.0, 1.0);
    let roughness = clamp(per_material.roughness_ao.x, 0.04, 1.0);
    let ao = max(per_material.roughness_ao.y, 0.0);
    let F0 = mix(vec3<f32>(0.04), surface_albedo, vec3<f32>(metallic));
    let H = normalize(V + L);
    let NDF = DistributionGGX(N, H, roughness);
    let G = GeometrySmith(N, V, L, roughness);
    let F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    let specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
    let NdotL = max(dot(N, L), 0.0);
    var Lo = (kD * surface_albedo / PI + specular) * light_color * light_intensity * NdotL;
    if (per_material.sss.w > 0.0) {
      Lo = Lo + SubsurfaceScattering(N, L, surface_albedo, per_material.sss.w,
                                     light_color, light_intensity, per_material.sss.xyz);
    }
    if (per_material.clearcoat.x > 0.0) {
      let cc_r = max(per_material.clearcoat.y, 0.04);
      let NDF_cc = DistributionGGX(N, H, cc_r);
      let G_cc = GeometrySmith(N, V, L, cc_r);
      let F_cc = FresnelSchlick(max(dot(H, V), 0.0), vec3<f32>(0.04));
      let spec_cc = (NDF_cc * G_cc * F_cc) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
      Lo = Lo + spec_cc * per_material.clearcoat.x * NdotL * light_color * light_intensity;
    }
    Lo = Lo * (1.0 - shadow);
    Lo = Lo + PointLightsLo(N, V, i.wpos, surface_albedo, roughness, metallic, F0);
    color = vec3<f32>(ambient) * surface_albedo * ao + Lo + per_material.emissive.xyz;
  }
  // 与 forward_shaded.frag 末尾一致：Reinhard tonemap + gamma（保证三后端 LDR 输出一致）。
  // 后续 composite（kWgslComposite）再做一次 Reinhard+sRGB，与 WebGL2 参考帧（同样双重处理）对齐。
  color = color / (color + vec3<f32>(1.0));
  color = pow(max(color, vec3<f32>(0.0)), vec3<f32>(1.0 / 2.2));
  return vec4<f32>(color, out_alpha);
}
)WGSL";

// 天空盒：vp 推送常量（VS push, group0 binding0）；cubemap 在 slot0 → group2 binding0/1。
// 顶点为 36 顶点立方体（vec3 pos）。z=w 使深度落远平面（配 LEQUAL 深度测试）。
const char* kWgslSkybox = R"WGSL(// dse-wgsl
struct VsPush { vp : mat4x4<f32> };
@group(0) @binding(0) var<uniform> vs_push : VsPush;
@group(2) @binding(0) var sky_tex : texture_cube<f32>;
@group(2) @binding(1) var sky_smp : sampler;
struct VsOut { @builtin(position) pos : vec4<f32>, @location(0) dir : vec3<f32> };
@vertex fn vs_main(@location(0) in_pos : vec3<f32>) -> VsOut {
  var o : VsOut;
  let p = vs_push.vp * vec4<f32>(in_pos, 1.0);
  o.pos = p.xyww;
  o.dir = in_pos;
  return o;
}
@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  return textureSample(sky_tex, sky_smp, normalize(i.dir));
}
)WGSL";

// 内建天空盒立方体（36 顶点，每顶点 vec3，逆时针外向；与桌面后端一致）。
const float kSkyboxCubeVerts[] = {
  -1,  1, -1,  -1, -1, -1,   1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
  -1, -1,  1,  -1, -1, -1,  -1,  1, -1,  -1,  1, -1,  -1,  1,  1,  -1, -1,  1,
   1, -1, -1,   1, -1,  1,   1,  1,  1,   1,  1,  1,   1,  1, -1,   1, -1, -1,
  -1, -1,  1,  -1,  1,  1,   1,  1,  1,   1,  1,  1,   1, -1,  1,  -1, -1,  1,
  -1,  1, -1,   1,  1, -1,   1,  1,  1,   1,  1,  1,  -1,  1,  1,  -1,  1, -1,
  -1, -1, -1,  -1, -1,  1,   1, -1, -1,   1, -1, -1,  -1, -1,  1,   1, -1,  1,
};

}  // namespace

unsigned int WebGPURhiDevice::GetOrCreateWgslProgram(const std::string& key, const std::string& wgsl) {
    auto it = wgsl_program_cache_.find(key);
    if (it != wgsl_program_cache_.end()) return it->second;
    const unsigned int h = CreateShaderProgram(wgsl, wgsl);
    wgsl_program_cache_[key] = h;
    DEBUG_LOG_INFO("WebGPU: 内建 WGSL 程序 '{}' -> handle {}", key, h);
    return h;
}

unsigned int WebGPURhiDevice::GetBuiltinProgram(BuiltinProgram program) {
    switch (program) {
        case BuiltinProgram::Skybox:
            return GetOrCreateWgslProgram("builtin.skybox", kWgslSkybox);
        // B2c：静态前向 PBR（最小前向 WGSL，64B PerMaterial）。
        case BuiltinProgram::ForwardPbr:
            return GetOrCreateWgslProgram("builtin.forward", kWgslForward);
        // B2c 进阶：高级 shading（shading_mode/SSS/clearcoat/点光/CSM，160B PerMaterial）。
        case BuiltinProgram::ForwardShaded:
            return GetOrCreateWgslProgram("builtin.forward.shaded", kWgslForwardShaded);
        default:
            // 蒙皮/实例化/morph/粒子/毛发/GBuffer 等需 SSBO 或专用布局，留后续阶段。
            return 0;
    }
}

unsigned int WebGPURhiDevice::GetGenPPShaderProgram(const std::string& effect_name) {
    // 合成族（HDR→LDR tonemap）与直拷族分流；未迁移效果返回 0（其 pass 优雅跳过）。
    // B-2：bloom 合成走 ACES + bloom/vignette（kWgslBloomComposite），独立于无 bloom 时的
    //   tonemapping/ssao_apply 直合成（kWgslComposite，Reinhard）。
    if (effect_name == "bloom_composite") {
        return GetOrCreateWgslProgram("genpp.bloom_composite", kWgslBloomComposite);
    }
    if (effect_name == "tonemapping" || effect_name == "ssao_apply") {
        return GetOrCreateWgslProgram("genpp.composite", kWgslComposite);
    }
    // B-2：bloom 链各趟（亮度提取 / 13-tap 降采样 / 3x3 帐篷升采样），全屏 quad WGSL。
    if (effect_name == "bloom_extract") {
        return GetOrCreateWgslProgram("genpp.bloom_extract", kWgslBloomExtract);
    }
    if (effect_name == "bloom_downsample") {
        return GetOrCreateWgslProgram("genpp.bloom_downsample", kWgslBloomDownsample);
    }
    if (effect_name == "bloom_upsample") {
        return GetOrCreateWgslProgram("genpp.bloom_upsample", kWgslBloomUpsample);
    }
    if (effect_name == "copy" || effect_name == "passthrough" || effect_name == "fxaa") {
        return GetOrCreateWgslProgram("genpp.blit", kWgslFullscreenBlit);
    }
    return 0;
}

unsigned int WebGPURhiDevice::GetSkyboxCubeVertexBuffer() {
    if (skybox_cube_vbo_) return skybox_cube_vbo_;
    skybox_cube_vbo_ = CreateBuffer(sizeof(kSkyboxCubeVerts), kSkyboxCubeVerts,
                                    /*is_dynamic=*/false, /*is_index=*/false);
    return skybox_cube_vbo_;
}

unsigned int WebGPURhiDevice::CreateBufferRaw(size_t logical_size, const void* data,
                                              WGPUBufferUsageFlags usage, bool is_index) {
    if (logical_size == 0 || !EnsureInitialized() || !device_) return 0;
    const uint64_t alloc = AlignUp4(logical_size);
    WGPUBufferDescriptor bd{};
    bd.usage = usage;
    bd.size = alloc;
    bd.mappedAtCreation = (data != nullptr);
    WGPUBuffer buf = wgpuDeviceCreateBuffer(device_, &bd);
    if (!buf) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateBuffer 失败 (size={})", static_cast<unsigned long long>(alloc));
        return 0;
    }
    if (data) {
        void* mapped = wgpuBufferGetMappedRange(buf, 0, alloc);
        if (mapped) std::memcpy(mapped, data, logical_size);
        wgpuBufferUnmap(buf);
    }

    BufferEntry e;
    e.buffer = buf;
    e.size = alloc;
    e.logical_size = logical_size;
    e.usage = usage;
    e.is_index = is_index;
    const unsigned int h = NextHandle();
    buffers_[h] = e;
    return h;
}

unsigned int WebGPURhiDevice::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    (void)is_dynamic;  // WebGPU 缓冲无静/动态区分；动态更新经 wgpuQueueWriteBuffer。
    WGPUBufferUsageFlags usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    if (is_index) {
        usage |= WGPUBufferUsage_Index;
    } else {
        // 非索引缓冲可能用作顶点流或 uniform，同时授予两种 usage（WebGPU 允许组合）。
        usage |= WGPUBufferUsage_Vertex | WGPUBufferUsage_Uniform;
    }
    return CreateBufferRaw(size, data, usage, is_index);
}

BufferHandle WebGPURhiDevice::CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) {
    if (desc.size == 0) return BufferHandle{};
    // 按 GpuBufferUsage 授予 WGPU usage 位（始终带 CopyDst|CopySrc 以支持更新/回读拷贝）。
    WGPUBufferUsageFlags usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    const bool is_index = has(desc.usage, GpuBufferUsage::kIndex);
    if (is_index)                                   usage |= WGPUBufferUsage_Index;
    if (has(desc.usage, GpuBufferUsage::kVertex))   usage |= WGPUBufferUsage_Vertex;
    if (has(desc.usage, GpuBufferUsage::kUniform))  usage |= WGPUBufferUsage_Uniform;
    if (has(desc.usage, GpuBufferUsage::kStorage))  usage |= WGPUBufferUsage_Storage;
    if (has(desc.usage, GpuBufferUsage::kIndirect)) usage |= WGPUBufferUsage_Indirect;
    // 无任何用途位（仅 CopyDst|CopySrc）：退化为顶点+uniform，兼容默认 GpuBufferDesc。
    if ((usage & ~static_cast<WGPUBufferUsageFlags>(WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc)) == 0)
        usage |= WGPUBufferUsage_Vertex | WGPUBufferUsage_Uniform;
    return BufferHandle{CreateBufferRaw(desc.size, initial_data, usage, is_index)};
}

void WebGPURhiDevice::UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) {
    auto it = buffers_.find(handle.raw());
    if (it == buffers_.end() || !data || size == 0) return;
    const BufferEntry& e = it->second;
    // 顶点/索引/uniform 缓冲必须经 UpdateBuffer：引擎前向路径每 draw 全量重写共享 vbo_/ibo_ 与
    //   逐材质 UBO（offset=0），需 geom/UBO 版本环避免 wgpuQueueWriteBuffer 合并丢失（见 UpdateBuffer）。
    // 仅 storage/indirect（B3a 新增、设备级生命周期、不逐 draw 重写）走直写，不入版本环。
    const bool storage_or_indirect =
        (e.usage & (WGPUBufferUsage_Storage | WGPUBufferUsage_Indirect)) != 0;
    if (!storage_or_indirect) {
        UpdateBuffer(handle.raw(), offset, size, data, e.is_index);
        return;
    }
    if (offset % 4 != 0) {
        DEBUG_LOG_WARN("WebGPU UpdateGpuBuffer: offset {} 非 4 对齐，跳过更新", static_cast<unsigned long long>(offset));
        return;
    }
    const uint64_t write_size = AlignUp4(size);
    if (offset + write_size > e.size) return;
    if (write_size == size) {
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, data, size);
    } else {
        std::vector<uint8_t> padded(write_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, padded.data(), write_size);
    }
}

void WebGPURhiDevice::BindGpuBuffer(BufferHandle handle, uint32_t binding_point) {
    // 路由到 group3 SSBO 槽位图（与 CmdBindStorageBuffer 一致）。size=0 → 整缓冲（见 CollectComputeGroupBindings）。
    cur_ssbos_[binding_point] = SsboBinding{handle.raw(), 0, 0};
}

void WebGPURhiDevice::BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) {
    (void)writable;  // WebGPU compute storage 统一 read_write，writable 无意义
    BindGpuBuffer(handle, binding_point);
}

void WebGPURhiDevice::DeleteGpuBuffer(BufferHandle handle) {
    DeleteBuffer(handle.raw());
}

// --- GPU→CPU 异步双缓冲延迟回读覆写（见头文件注释）---
//   BeginGpuReadback：在 frame_encoder_（须不在任何 pass 内）上把源缓冲 CopyBufferToBuffer 到
//   本帧写 staging[write]，记 pending、write^=1，返回「上一帧结果是否就绪」。EndFrame 提交后由
//   KickDeferredReadback 对 pending staging 发 mapAsync，回调拷进 async_rb_result_ 并置 ready。
bool WebGPURhiDevice::BeginGpuReadback(BufferHandle handle, size_t offset, size_t size) {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_ || size == 0) {
        return async_rb_ready_;  // 无法在本帧录制拷贝（pass 内/无 encoder）→ 仅返回既有就绪态
    }
    const BufferEntry* be = FindBuffer(handle.raw());
    if (!be || !be->buffer) return async_rb_ready_;

    const int w = async_rb_write_idx_;
    if (async_rb_mapped_[w]) {
        // 目标 staging 仍在上一轮 map 飞行中（罕见，map 慢于 2 帧）→ 本帧跳过拷贝，避免写入已映射缓冲
        return async_rb_ready_;
    }
    // 按需扩容 staging[w]（MapRead|CopyDst）。WGPU copy size 须 4 对齐（mat4 自然满足）。
    const size_t need = (size + 3u) & ~size_t(3);
    if (async_rb_capacity_[w] < need) {
        if (async_rb_staging_[w]) { wgpuBufferRelease(async_rb_staging_[w]); async_rb_staging_[w] = nullptr; }
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bd.size  = need;
        async_rb_staging_[w] = wgpuDeviceCreateBuffer(device_, &bd);
        async_rb_capacity_[w] = async_rb_staging_[w] ? need : 0;
    }
    if (!async_rb_staging_[w]) return async_rb_ready_;

    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be->buffer, offset, async_rb_staging_[w], 0, need);
    async_rb_has_pending_  = true;
    async_rb_pending_idx_  = w;
    async_rb_pending_size_ = size;
    async_rb_write_idx_    = 1 - w;
    return async_rb_ready_;
}

const void* WebGPURhiDevice::GetLastReadbackResult(size_t* out_size) const {
    if (out_size) *out_size = async_rb_ready_ ? async_rb_result_.size() : 0;
    return async_rb_ready_ ? async_rb_result_.data() : nullptr;
}

void WebGPURhiDevice::OnDeferredReadbackMapped(WGPUBufferMapAsyncStatus status, void* userdata) {
    auto* ctx = static_cast<DeferredReadbackCtx*>(userdata);
    WebGPURhiDevice* dev = ctx->dev;
    const int idx = ctx->idx;
    WGPUBuffer buf = dev->async_rb_staging_[idx];
    if (status == WGPUBufferMapAsyncStatus_Success && buf) {
        const void* m = wgpuBufferGetConstMappedRange(buf, 0, ctx->size);
        if (m) {
            dev->async_rb_result_.resize(ctx->size);
            std::memcpy(dev->async_rb_result_.data(), m, ctx->size);
            dev->async_rb_ready_ = true;
        }
        wgpuBufferUnmap(buf);
    }
    dev->async_rb_mapped_[idx] = false;
    delete ctx;
}

void WebGPURhiDevice::KickDeferredReadback() {
    if (!async_rb_has_pending_) return;
    async_rb_has_pending_ = false;
    const int p = async_rb_pending_idx_;
    if (!async_rb_staging_[p]) return;
    async_rb_mapped_[p] = true;
    auto* ctx = new DeferredReadbackCtx{this, p, async_rb_pending_size_};
    wgpuBufferMapAsync(async_rb_staging_[p], WGPUMapMode_Read, 0, async_rb_pending_size_,
                       OnDeferredReadbackMapped, ctx);
}

void WebGPURhiDevice::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) {
    (void)is_index;
    auto it = buffers_.find(handle);
    if (it == buffers_.end() || !data || size == 0) return;
    const BufferEntry& e = it->second;
    // wgpuQueueWriteBuffer 要求 offset 与 size 均为 4 的倍数。常规调用方已 4 对齐；
    // 否则把 size 向上取整到 4（缓冲分配已对齐，越界由下方 clamp 兜底）。
    if (offset % 4 != 0) {
        DEBUG_LOG_WARN("WebGPU UpdateBuffer: offset {} 非 4 对齐，跳过更新", static_cast<unsigned long long>(offset));
        return;
    }
    const uint64_t write_size = AlignUp4(size);
    if (offset + write_size > e.size) return;
    if (write_size == size) {
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, data, size);
    } else {
        std::vector<uint8_t> padded(write_size, 0);
        std::memcpy(padded.data(), data, size);
        wgpuQueueWriteBuffer(queue_, e.buffer, offset, padded.data(), write_size);
    }
    // 同时登记 UBO 版本切片：仅全量（offset=0）且 uniform 量级（≤64KB，WebGPU uniform 绑定上限）
    //   的更新参与——大顶点/存储缓冲不在此列。该缓冲若稍后以 UBO 绑定（group1），将改用此切片，
    //   使每 draw 的逐材质数据互不覆盖（见 CollectGroupBindings group1）。
    if (offset == 0 && size <= 65536) {
        const uint64_t voff = AllocUboVersion(data, size);
        if (voff != UINT64_MAX) ubo_versions_[handle] = {ubo_ring_, voff, size};
    }
    // 几何版本切片：引擎前向路径每 draw 把世界空间顶点/索引重写进共享 vbo_/ibo_（offset=0）。
    //   同 UBO 合并问题——故全量更新（offset=0）在 geom 环内分配独立切片，绑定时改用当帧最近版本，
    //   使各 draw 的顶点/索引互不覆盖（见 IssueDraw 的 SetVertexBuffer/SetIndexBuffer）。
    if (offset == 0) {
        const uint64_t goff = AllocGeomVersion(data, size);
        if (goff != UINT64_MAX) geom_versions_[handle] = {geom_ring_, goff, AlignUp4(size)};
    }
}

void WebGPURhiDevice::DeleteBuffer(unsigned int handle) {
    auto it = buffers_.find(handle);
    if (it == buffers_.end()) return;
    if (it->second.buffer) wgpuBufferRelease(it->second.buffer);
    buffers_.erase(it);
}

VertexArrayHandle WebGPURhiDevice::CreateVertexArray() {
    return VertexArrayHandle{NextHandle()};
}

void WebGPURhiDevice::DeleteVertexArray(VertexArrayHandle handle) {
    (void)handle;
}

std::shared_ptr<CommandBuffer> WebGPURhiDevice::CreateCommandBuffer() {
    return std::make_shared<WebGPUCommandBuffer>(this);
}

void WebGPURhiDevice::Submit(std::shared_ptr<CommandBuffer> cmd_buffer) {
    // B2：WebGPUCommandBuffer 录制即时落到本帧 frame_encoder_（经设备级 Cmd*），故 Submit
    // 无需重放；整帧命令在 EndFrame 一次 wgpuQueueSubmit 提交。
    (void)cmd_buffer;
}

const RenderStats& WebGPURhiDevice::LastFrameStats() const {
    return last_frame_stats_;
}

// ============================================================
// B2 命令录制引擎：设备级 Cmd* + 录制助手
// ============================================================
//
// WebGPUCommandBuffer 逐调用转发至此。录制立即落到本帧 frame_encoder_：BeginRenderPass 在其上
// 开 WGPURenderPassEncoder（cur_pass_），Bind*/PushConstants 累积当前绘制状态，Draw* 经
// GetOrCreateRenderPipeline 惰性组装 explicit-layout PSO 并发起；EndRenderPass 收尾。无 WGSL
// module 的程序（引擎 GLSL）在 IssueDraw 前优雅跳过，故 B2a 期引擎绘制不上屏、由 EndFrame 自检兜底。

// --- 录制助手 ---

void WebGPURhiDevice::ResetDrawState() {
    cur_pso_handle_ = 0;
    cur_program_ = 0;
    cur_vbs_.clear();
    cur_ib_handle_ = 0;
    cur_ib_format_ = WGPUIndexFormat_Uint16;
    cur_ubos_.clear();
    cur_texs_.clear();
    cur_ssbos_.clear();
    cur_compute_images_.clear();
    cur_compute_textures_.clear();
    cur_compute_image_views_.clear();
    cur_compute_texture_views_.clear();
    cur_vs_push_.clear();
    cur_fs_push_.clear();
}

void WebGPURhiDevice::ReleasePassViews() {
    for (WGPUTextureView v : cur_pass_views_) {
        if (v) wgpuTextureViewRelease(v);
    }
    cur_pass_views_.clear();
}

WGPUShaderModule WebGPURhiDevice::CompileWGSL(const std::string& code, const char* label) {
    if (!device_) return nullptr;
    WGPUShaderModuleWGSLDescriptor wgsl{};
    wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl.code = code.c_str();
    WGPUShaderModuleDescriptor sd{};
    sd.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgsl);
    sd.label = label;
    return wgpuDeviceCreateShaderModule(device_, &sd);
}

WGPUTextureView WebGPURhiDevice::MakeFaceView(const TextureEntry& e, int face) {
    WGPUTextureViewDescriptor vd{};
    vd.format = e.format;
    vd.dimension = WGPUTextureViewDimension_2D;
    vd.baseMipLevel = 0;
    vd.mipLevelCount = 1;
    vd.baseArrayLayer = static_cast<uint32_t>(face < 0 ? 0 : face);
    vd.arrayLayerCount = 1;
    vd.aspect = WGPUTextureAspect_All;
    return wgpuTextureCreateView(e.texture, &vd);
}

std::vector<WebGPURhiDevice::BindingInfo> WebGPURhiDevice::CollectGroupBindings(uint32_t group) {
    // 各组遍历顺序在 BGL（GetOrCreateRenderPipeline）与 BindGroup（BuildAndSetBindGroups）间共用，
    // 杜绝二者发散。group0=push（uniform 池）、group1=UBO、group2=texture+sampler、group3=SSBO。
    // std::map 保证按 binding 升序稳定遍历。
    std::vector<BindingInfo> out;
    const WGPUShaderStageFlags kVsFs = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    // 仅纳入当前 WGSL 程序实际声明的绑定：引擎可能绑定多于着色器所需的资源（如 ForwardShaded
    // 绑 8 UBO/20 纹理槽），全量纳入会超 per-stage 采样上限并使 layout 与着色器用量不符。
    const std::set<uint32_t>* used = nullptr;
    {
        auto sit = shaders_.find(cur_program_);
        if (sit != shaders_.end()) used = &sit->second.wgsl_bindings;
    }
    auto declared = [&](uint32_t binding) -> bool {
        return used && used->count((group << 16) | binding) != 0;
    };
    switch (group) {
        case 0: {
            // push 常量经 push 池 uniform 缓冲模拟：binding0=VS、binding1=FS。
            auto alloc_push = [&](const std::vector<uint8_t>& src) -> WGPUBuffer {
                constexpr uint32_t kPushBytes = 256;
                if (push_pool_used_ >= push_pool_.size()) {
                    WGPUBufferDescriptor bd{};
                    bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
                    bd.size = kPushBytes;
                    push_pool_.push_back(wgpuDeviceCreateBuffer(device_, &bd));
                }
                WGPUBuffer b = push_pool_[push_pool_used_++];
                std::vector<uint8_t> tmp(kPushBytes, 0);
                const size_t n = src.size() < kPushBytes ? src.size() : kPushBytes;
                if (n) std::memcpy(tmp.data(), src.data(), n);
                wgpuQueueWriteBuffer(queue_, b, 0, tmp.data(), kPushBytes);
                return b;
            };
            if (!cur_vs_push_.empty() && declared(0)) {
                BindingInfo b;
                b.binding = 0; b.kind = BindingInfo::Kind::Uniform; b.visibility = WGPUShaderStage_Vertex;
                b.buffer = alloc_push(cur_vs_push_); b.buf_offset = 0; b.buf_size = 256;
                out.push_back(b);
            }
            if (!cur_fs_push_.empty() && declared(1)) {
                BindingInfo b;
                b.binding = 1; b.kind = BindingInfo::Kind::Uniform; b.visibility = WGPUShaderStage_Fragment;
                b.buffer = alloc_push(cur_fs_push_); b.buf_offset = 0; b.buf_size = 256;
                out.push_back(b);
            }
            break;
        }
        case 1: {
            for (const auto& [slot, u] : cur_ubos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = FindBuffer(u.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Uniform; b.visibility = kVsFs;
                // 优先用当帧最近一次 UBO 版本切片（无范围偏移绑定时）：使逐 draw 材质数据互不覆盖。
                auto vit = (u.offset == 0) ? ubo_versions_.find(u.handle) : ubo_versions_.end();
                if (vit != ubo_versions_.end() && vit->second.buffer) {
                    b.buffer = vit->second.buffer;
                    b.buf_offset = vit->second.offset;
                    b.buf_size = vit->second.size;
                } else {
                    b.buffer = be->buffer; b.buf_offset = u.offset;
                    b.buf_size = u.size ? u.size : be->size;
                }
                out.push_back(b);
            }
            break;
        }
        case 2: {
            for (const auto& [slot, t] : cur_texs_) {
                // 纹理与采样器同声明（slot*2 / slot*2+1）：着色器声明纹理即纳入二者。
                if (!declared(slot * 2u)) continue;
                const TextureEntry* te = FindTexture(t.handle);
                if (!te || !te->view) continue;
                WGPUTextureView      use_view = te->view;
                WGPUSampler          use_smp  = te->sampler;
                WGPUTextureSampleType use_st   = IsDepthFormat(te->format)
                                                     ? WGPUTextureSampleType_Depth
                                                     : WGPUTextureSampleType_Float;
                TextureDim           use_dim  = t.dim;
                // 5.1b：slot11（CSM shadow atlas → binding22，前向 WGSL 声明 texture_depth_2d）须恒以
                //   Depth 采样。两种情形换用恒亮 Depth32 回退纹理：
                //   (a) 消费方在无 shadow map 时绑「白 RGBA8」回退（Float）→ 统一 binding sampleType 为 Depth；
                //   (b) 读写危险：当前 pass 把真 atlas 作可写附件（depth-only 阴影 atlas pass 仍复用 forward
                //       PSO，逐 draw 绑 slot11=atlas），WebGPU 不允许同一同步作用域内既可写附件又被采样 → 用
                //       只读回退纹理替代（阴影/prez 等 depth-only pass 本就不需要采样阴影，恒无遮挡安全）。
                if (slot == 11u) {
                    const bool not_depth = (use_st != WGPUTextureSampleType_Depth);
                    const bool rw_hazard =
                        std::find(cur_pass_attachment_texs_.begin(),
                                  cur_pass_attachment_texs_.end(), t.handle)
                        != cur_pass_attachment_texs_.end();
                    if (not_depth || rw_hazard) {
                        const TextureEntry* fb = shadow_fallback_depth_tex_
                                                     ? FindTexture(shadow_fallback_depth_tex_) : nullptr;
                        if (fb && fb->view) {
                            use_view = fb->view;
                            use_smp  = fb->sampler;
                            use_st   = WGPUTextureSampleType_Depth;
                            use_dim  = TextureDim::Tex2D;
                        }
                    }
                }
                BindingInfo tb;
                tb.binding = slot * 2u; tb.kind = BindingInfo::Kind::Texture;
                tb.visibility = WGPUShaderStage_Fragment;
                tb.view_dim = ToViewDimension(use_dim);
                tb.sample_type = use_st;
                tb.view = use_view;
                out.push_back(tb);
                BindingInfo sb;
                sb.binding = slot * 2u + 1u; sb.kind = BindingInfo::Kind::Sampler;
                sb.visibility = WGPUShaderStage_Fragment;
                sb.sampler = use_smp;
                out.push_back(sb);
            }
            break;
        }
        case 3: {
            for (const auto& [slot, s] : cur_ssbos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = FindBuffer(s.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Storage; b.visibility = kVsFs;
                b.buffer = be->buffer; b.buf_offset = s.offset;
                b.buf_size = s.size ? s.size : be->size;
                out.push_back(b);
            }
            break;
        }
        default: break;
    }
    return out;
}

const WebGPURhiDevice::PipelineCacheEntry* WebGPURhiDevice::GetOrCreateRenderPipeline() {
    // 无 WGSL module 的程序（引擎 GLSL，无离线转译）：返回 nullptr，调用方跳过该 draw。
    auto sit = shaders_.find(cur_program_);
    if (sit == shaders_.end() || !sit->second.module) return nullptr;
    const ShaderEntry& sh = sit->second;

    // 校验：WGSL 声明的每个绑定都须有当前已绑资源满足；否则 explicit BGL 会缺该绑定，
    // 使 pipeline 创建失败并污染整个命令缓冲。缺失即跳过该 draw（优雅降级）。
    if (!sh.wgsl_bindings.empty()) {
        std::set<uint32_t> present;
        for (uint32_t g = 0; g < 4; ++g) {
            for (const BindingInfo& b : CollectGroupBindings(g)) present.insert((g << 16) | b.binding);
        }
        for (uint32_t key : sh.wgsl_bindings) {
            if (present.count(key)) continue;
            if (logged_incomplete_programs_.insert(cur_program_).second) {
                DEBUG_LOG_WARN("WebGPU: 程序 {} 缺少所需绑定 group={} binding={}（资源未绑定），跳过该 draw",
                               cur_program_, key >> 16, key & 0xffffu);
            }
            return nullptr;
        }
    }

    const PipelineStateDesc fallback_pso;
    const PipelineStateDesc* pso = FindPipelineState(cur_pso_handle_);
    const PipelineStateDesc& ps = pso ? *pso : fallback_pso;

    // 缓存签名 = program + pso + 顶点布局 + 颜色/深度格式 + 采样数 + 绑定签名。
    std::string key;
    key.reserve(256);
    key += "p" + std::to_string(cur_program_) + "s" + std::to_string(cur_pso_handle_);
    for (size_t i = 0; i < cur_vbs_.size(); ++i) {
        const VbBinding& vb = cur_vbs_[i];
        if (!vb.set) continue;
        key += "|v" + std::to_string(i) + ":" + std::to_string(vb.stride) + ":" +
               std::to_string(static_cast<int>(vb.rate));
        for (const VertexAttr& a : vb.attrs) {
            key += "," + std::to_string(a.location) + "-" + std::to_string(a.components) +
                   "-" + std::to_string(a.offset);
        }
    }
    for (WGPUTextureFormat f : cur_color_formats_) key += "c" + std::to_string(static_cast<int>(f));
    key += "d" + std::to_string(static_cast<int>(cur_depth_format_));
    key += "m" + std::to_string(cur_sample_count_);
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectGroupBindings(g);
        key += "g" + std::to_string(g);
        for (const BindingInfo& b : bs) {
            key += ":" + std::to_string(b.binding) + "-" + std::to_string(static_cast<int>(b.kind)) +
                   "-" + std::to_string(static_cast<int>(b.visibility)) +
                   "-" + std::to_string(static_cast<int>(b.view_dim));
        }
    }

    auto cit = pipeline_cache_.find(key);
    if (cit != pipeline_cache_.end()) return &cit->second;

    PipelineCacheEntry entry{};

    // 4 组 explicit BGL（与 BindGroup 共用同序绑定签名）。
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectGroupBindings(g);
        std::vector<WGPUBindGroupLayoutEntry> bgle;
        bgle.reserve(bs.size());
        for (const BindingInfo& b : bs) {
            WGPUBindGroupLayoutEntry e{};
            e.binding = b.binding;
            e.visibility = b.visibility;
            switch (b.kind) {
                case BindingInfo::Kind::Uniform:
                    e.buffer.type = WGPUBufferBindingType_Uniform; break;
                case BindingInfo::Kind::Storage:
                    e.buffer.type = WGPUBufferBindingType_ReadOnlyStorage; break;
                case BindingInfo::Kind::Texture:
                    e.texture.sampleType = b.sample_type;
                    e.texture.viewDimension = b.view_dim;
                    e.texture.multisampled = false;
                    break;
                case BindingInfo::Kind::Sampler:
                    e.sampler.type = WGPUSamplerBindingType_Filtering; break;
                case BindingInfo::Kind::StorageTexture:
                    // render 路径不产生 storage image 绑定；列此分支仅为穷尽枚举（避免 -Wswitch）。
                    e.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
                    e.storageTexture.format = b.tex_format;
                    e.storageTexture.viewDimension = b.view_dim; break;
            }
            bgle.push_back(e);
        }
        WGPUBindGroupLayoutDescriptor bgld{};
        bgld.entryCount = bgle.size();
        bgld.entries = bgle.empty() ? nullptr : bgle.data();
        entry.bgls[g] = wgpuDeviceCreateBindGroupLayout(device_, &bgld);
    }

    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 4;
    pld.bindGroupLayouts = entry.bgls;
    entry.layout = wgpuDeviceCreatePipelineLayout(device_, &pld);

    // 顶点缓冲布局（每 set 的 slot 一条），attrs_store/vbls 预留容量以保证内部指针稳定。
    std::vector<std::vector<WGPUVertexAttribute>> attrs_store;
    std::vector<WGPUVertexBufferLayout> vbls;
    attrs_store.reserve(cur_vbs_.size());
    vbls.reserve(cur_vbs_.size());
    for (const VbBinding& vb : cur_vbs_) {
        if (!vb.set) continue;
        std::vector<WGPUVertexAttribute> va;
        va.reserve(vb.attrs.size());
        for (const VertexAttr& a : vb.attrs) {
            WGPUVertexAttribute at{};
            at.format = ToVertexFormat(a.components);
            at.offset = a.offset;
            at.shaderLocation = a.location;
            va.push_back(at);
        }
        attrs_store.push_back(std::move(va));
        WGPUVertexBufferLayout l{};
        l.arrayStride = vb.stride;
        l.stepMode = vb.rate == VertexInputRate::PerInstance ? WGPUVertexStepMode_Instance
                                                             : WGPUVertexStepMode_Vertex;
        l.attributeCount = attrs_store.back().size();
        l.attributes = attrs_store.back().data();
        vbls.push_back(l);
    }

    WGPUVertexState vs{};
    vs.module = sh.module;
    vs.entryPoint = sh.vs_entry.c_str();
    vs.bufferCount = vbls.size();
    vs.buffers = vbls.empty() ? nullptr : vbls.data();

    // 颜色目标（与 RT/backbuffer 附件格式一一对应）。
    std::vector<WGPUColorTargetState> targets;
    std::vector<WGPUBlendState> blends;
    targets.reserve(cur_color_formats_.size());
    blends.reserve(cur_color_formats_.size());
    for (WGPUTextureFormat f : cur_color_formats_) {
        WGPUColorTargetState t{};
        t.format = f;
        t.writeMask = WGPUColorWriteMask_All;
        if (ps.blend_enabled) {
            WGPUBlendState bs{};
            bs.color.operation = WGPUBlendOperation_Add;
            bs.color.srcFactor = ToBlendFactor(ps.blend_src);
            bs.color.dstFactor = ToBlendFactor(ps.blend_dst);
            bs.alpha.operation = WGPUBlendOperation_Add;
            bs.alpha.srcFactor = ToBlendFactor(ps.alpha_blend_src);
            bs.alpha.dstFactor = ToBlendFactor(ps.alpha_blend_dst);
            blends.push_back(bs);
            t.blend = &blends.back();
        }
        targets.push_back(t);
    }

    WGPUFragmentState fs{};
    fs.module = sh.module;
    fs.entryPoint = sh.fs_entry.c_str();
    fs.targetCount = targets.size();
    fs.targets = targets.empty() ? nullptr : targets.data();

    WGPURenderPipelineDescriptor rpd{};
    rpd.layout = entry.layout;
    rpd.vertex = vs;
    rpd.primitive.topology = ToTopology(ps.topology);
    rpd.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    rpd.primitive.frontFace = WGPUFrontFace_CCW;
    rpd.primitive.cullMode = ps.culling_enabled ? ToCullMode(ps.cull_face) : WGPUCullMode_None;

    WGPUDepthStencilState ds{};
    if (cur_depth_format_ != WGPUTextureFormat_Undefined) {
        ds.format = cur_depth_format_;
        ds.depthWriteEnabled = ps.depth_write_enabled;
        ds.depthCompare = ps.depth_test_enabled ? ToCompareFunc(ps.depth_func)
                                                : WGPUCompareFunction_Always;
        ds.stencilFront.compare = WGPUCompareFunction_Always;
        ds.stencilBack.compare = WGPUCompareFunction_Always;
        rpd.depthStencil = &ds;
    }

    rpd.multisample.count = cur_sample_count_ > 0 ? cur_sample_count_ : 1;
    rpd.multisample.mask = 0xFFFFFFFFu;
    rpd.multisample.alphaToCoverageEnabled = false;
    rpd.fragment = sh.has_fragment ? &fs : nullptr;

    entry.pipeline = wgpuDeviceCreateRenderPipeline(device_, &rpd);
    if (!entry.pipeline) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateRenderPipeline 失败 (program={})", cur_program_);
    }

    auto res = pipeline_cache_.emplace(std::move(key), entry);
    return &res.first->second;
}

void WebGPURhiDevice::BuildAndSetBindGroups(const PipelineCacheEntry& entry) {
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectGroupBindings(g);
        std::vector<WGPUBindGroupEntry> bge;
        bge.reserve(bs.size());
        for (const BindingInfo& b : bs) {
            WGPUBindGroupEntry e{};
            e.binding = b.binding;
            switch (b.kind) {
                case BindingInfo::Kind::Texture: e.textureView = b.view; break;
                case BindingInfo::Kind::Sampler: e.sampler = b.sampler; break;
                default:
                    e.buffer = b.buffer; e.offset = b.buf_offset; e.size = b.buf_size; break;
            }
            bge.push_back(e);
        }
        WGPUBindGroupDescriptor bgd{};
        bgd.layout = entry.bgls[g];
        bgd.entryCount = bge.size();
        bgd.entries = bge.empty() ? nullptr : bge.data();
        WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device_, &bgd);
        wgpuRenderPassEncoderSetBindGroup(cur_pass_, g, bg, 0, nullptr);
        frame_bindgroups_.push_back(bg);  // 提交后统一释放
    }
}

bool WebGPURhiDevice::BindPassDrawState(bool indexed, const PipelineCacheEntry*& pe_out) {
    const PipelineCacheEntry* pe = GetOrCreateRenderPipeline();
    if (!pe || !pe->pipeline) return false;  // 无 WGSL module / 组装失败 → 优雅跳过

    wgpuRenderPassEncoderSetPipeline(cur_pass_, pe->pipeline);
    for (size_t i = 0; i < cur_vbs_.size(); ++i) {
        const VbBinding& vb = cur_vbs_[i];
        if (!vb.set) continue;
        // 当帧若有该顶点缓冲的版本切片（每 draw 重写共享 vbo_），改绑版本切片避免合并覆盖。
        auto git = geom_versions_.find(vb.handle);
        if (git != geom_versions_.end() && git->second.buffer) {
            wgpuRenderPassEncoderSetVertexBuffer(cur_pass_, static_cast<uint32_t>(i),
                                                 git->second.buffer, git->second.offset, git->second.size);
            continue;
        }
        const BufferEntry* be = FindBuffer(vb.handle);
        if (!be || !be->buffer) continue;
        wgpuRenderPassEncoderSetVertexBuffer(cur_pass_, static_cast<uint32_t>(i), be->buffer, 0, be->size);
    }
    BuildAndSetBindGroups(*pe);

    if (indexed) {
        // 同顶点：优先当帧索引版本切片（每 draw 重写共享 ibo_），否则原索引缓冲。
        auto git = geom_versions_.find(cur_ib_handle_);
        if (git != geom_versions_.end() && git->second.buffer) {
            wgpuRenderPassEncoderSetIndexBuffer(cur_pass_, git->second.buffer, cur_ib_format_,
                                                git->second.offset, git->second.size);
        } else {
            const BufferEntry* ib = FindBuffer(cur_ib_handle_);
            if (!ib || !ib->buffer) return false;
            wgpuRenderPassEncoderSetIndexBuffer(cur_pass_, ib->buffer, cur_ib_format_, 0, ib->size);
        }
    }
    pe_out = pe;
    return true;
}

void WebGPURhiDevice::IssueDraw(bool indexed, uint32_t count, uint32_t instance_count,
                                uint32_t first, int32_t base_vertex, uint32_t first_instance) {
    if (!cur_pass_ || count == 0 || instance_count == 0) return;
    const PipelineCacheEntry* pe = nullptr;
    if (!BindPassDrawState(indexed, pe)) return;

    if (indexed) {
        wgpuRenderPassEncoderDrawIndexed(cur_pass_, count, instance_count, first,
                                         base_vertex, first_instance);
    } else {
        wgpuRenderPassEncoderDraw(cur_pass_, count, instance_count, first, first_instance);
    }

    if (cur_pass_is_backbuffer_) backbuffer_drawn_ = true;
    last_frame_stats_.draw_calls += 1;
}

// --- 设备级 Cmd*：render pass / viewport ---

void WebGPURhiDevice::CmdBeginRenderPass(const RenderPassDesc& desc) {
    if (!frame_encoder_) { cur_pass_ = nullptr; return; }
    if (cur_pass_) CmdEndRenderPass();  // 防御：上一 pass 未显式收尾
    ResetDrawState();
    ReleasePassViews();
    cur_color_formats_.clear();
    cur_pass_attachment_texs_.clear();
    cur_depth_format_ = WGPUTextureFormat_Undefined;
    cur_sample_count_ = 1;
    cur_pass_is_backbuffer_ = (desc.render_target == 0);
    cur_rt_width_ = 0;
    cur_rt_height_ = 0;

    const WGPULoadOp load = desc.clear_color_enabled ? WGPULoadOp_Clear : WGPULoadOp_Load;
    const WGPUColor clear = WGPUColor{desc.clear_color.r, desc.clear_color.g,
                                      desc.clear_color.b, desc.clear_color.a};

    std::vector<WGPURenderPassColorAttachment> color_atts;
    WGPURenderPassDepthStencilAttachment depth_att{};
    bool has_depth = false;

    if (desc.render_target == 0) {
        if (!backbuffer_view_) { cur_pass_ = nullptr; return; }
        cur_rt_width_ = static_cast<uint32_t>(width_ > 0 ? width_ : 1);
        cur_rt_height_ = static_cast<uint32_t>(height_ > 0 ? height_ : 1);
        WGPURenderPassColorAttachment ca{};
        ca.view = backbuffer_view_;
        ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        ca.loadOp = load;
        ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = clear;
        color_atts.push_back(ca);
        cur_color_formats_.push_back(swapchain_format_);
    } else {
        const RenderTargetEntry* rt = FindRenderTarget(desc.render_target);
        if (!rt) { cur_pass_ = nullptr; return; }
        cur_sample_count_ = static_cast<uint32_t>(rt->msaa_samples > 1 ? rt->msaa_samples : 1);
        cur_rt_width_ = rt->width > 0 ? rt->width : 1;
        cur_rt_height_ = rt->height > 0 ? rt->height : 1;
        for (unsigned int th : rt->color_textures) {
            const TextureEntry* te = FindTexture(th);
            if (!te) continue;
            cur_pass_attachment_texs_.push_back(th);
            WGPUTextureView v;
            if (rt->is_cube) {
                v = MakeFaceView(*te, desc.cube_face >= 0 ? desc.cube_face : 0);
                cur_pass_views_.push_back(v);
            } else {
                v = te->view;
            }
            WGPURenderPassColorAttachment ca{};
            ca.view = v;
            ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
            ca.loadOp = load;
            ca.storeOp = WGPUStoreOp_Store;
            ca.clearValue = clear;
            color_atts.push_back(ca);
            cur_color_formats_.push_back(te->format);
        }
        if (rt->depth_texture) {
            const TextureEntry* de = FindTexture(rt->depth_texture);
            if (de) {
                cur_pass_attachment_texs_.push_back(rt->depth_texture);
                WGPUTextureView v;
                if (rt->is_cube) {
                    v = MakeFaceView(*de, desc.cube_face >= 0 ? desc.cube_face : 0);
                    cur_pass_views_.push_back(v);
                } else {
                    v = de->view;
                }
                depth_att.view = v;
                depth_att.depthLoadOp = WGPULoadOp_Clear;  // B2 简化：深度恒清，B3 据 desc 细化
                depth_att.depthStoreOp = WGPUStoreOp_Store;
                depth_att.depthClearValue = 1.0f;
                depth_att.stencilLoadOp = WGPULoadOp_Undefined;
                depth_att.stencilStoreOp = WGPUStoreOp_Undefined;
                cur_depth_format_ = de->format;
                has_depth = true;
            }
        }
    }

    WGPURenderPassDescriptor pd{};
    pd.colorAttachmentCount = color_atts.size();
    pd.colorAttachments = color_atts.empty() ? nullptr : color_atts.data();  // depth-only：0 颜色附件
    pd.depthStencilAttachment = has_depth ? &depth_att : nullptr;

    cur_pass_ = wgpuCommandEncoderBeginRenderPass(frame_encoder_, &pd);
    last_frame_stats_.render_passes += 1;
}

void WebGPURhiDevice::CmdEndRenderPass() {
    if (cur_pass_) {
        wgpuRenderPassEncoderEnd(cur_pass_);
        wgpuRenderPassEncoderRelease(cur_pass_);
        cur_pass_ = nullptr;
    }
    ReleasePassViews();
}

void WebGPURhiDevice::CmdSetViewport(int x, int y, int width, int height) {
    if (!cur_pass_ || width <= 0 || height <= 0) return;
    // 裁剪到当前 pass 渲染目标范围：WebGPU 要求视口完全落在目标内，否则整个命令缓冲失效。
    // 引擎 GLSL pass 的绘制在 B2 期被跳过，但其 SetViewport 仍会录制，故须在此防御性裁剪。
    int rw = static_cast<int>(cur_rt_width_), rh = static_cast<int>(cur_rt_height_);
    if (rw <= 0 || rh <= 0) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= rw || y >= rh) return;
    if (x + width > rw) width = rw - x;
    if (y + height > rh) height = rh - y;
    if (width <= 0 || height <= 0) return;
    wgpuRenderPassEncoderSetViewport(cur_pass_, static_cast<float>(x), static_cast<float>(y),
                                     static_cast<float>(width), static_cast<float>(height),
                                     0.0f, 1.0f);
}

// --- 设备级 Cmd*：B2 暂保持 no-op（留 B3）---

void WebGPURhiDevice::CmdClearColor(const glm::vec4& color) { (void)color; }
void WebGPURhiDevice::CmdSetGlobalMat4(const std::string& name, const glm::mat4& value) { (void)name; (void)value; }
// B-3 CSM 可见激活：把 CSMShadowPass 渲出的 shadow atlas 深度纹理记入 global_render_state_，
// 供 mesh_renderer DrawShaded 取 grs.shadow_map[0] 绑 slot11（前向采样真 atlas）。前向 pass 中
// atlas 非附件且为 Depth32 → slot11 危险检测（~3959）放行真纹理；CSM atlas pass 自身复用前向 PSO
// 渲 caster 时 atlas 是可写附件 → 读写危险触发，换恒亮 Depth32 回退（安全，深度 pass 本不采样阴影）。
void WebGPURhiDevice::CmdBindGlobalShadowMap(unsigned int index, unsigned int texture_handle) {
    SetGlobalShadowMap(index, texture_handle);
}
void WebGPURhiDevice::CmdBindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) { (void)index; (void)texture_handle; }
void WebGPURhiDevice::CmdBindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) { (void)index; (void)texture_handle; }
// B3b-2：真 indirect 绘制——按当前已绑管线/顶点/索引缓冲，从 indirect buffer（须含 Indirect
// usage，由 CreateGpuBuffer(kIndirect) 授予）读取 [indexCount, instanceCount, firstIndex,
// baseVertex, firstInstance] 发 DrawIndexedIndirect。instance_count=0（被剔）时硬件不绘制。
void WebGPURhiDevice::CmdDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) {
    if (!cur_pass_) return;
    const BufferEntry* ind = FindBuffer(indirect_buffer);
    if (!ind || !ind->buffer) return;
    const PipelineCacheEntry* pe = nullptr;
    if (!BindPassDrawState(/*indexed=*/true, pe)) return;
    wgpuRenderPassEncoderDrawIndexedIndirect(cur_pass_, ind->buffer, byte_offset);
    if (cur_pass_is_backbuffer_) backbuffer_drawn_ = true;
    last_frame_stats_.draw_calls += 1;
}

// Task 4 Subtask 1：MultiDrawIndexedIndirect——WebGPU 无 glMultiDrawElementsIndirect，按已绑引擎
// draw state（管线/顶点/索引缓冲，经一次 BindPassDrawState 建立）循环逐条发
// wgpuRenderPassEncoderDrawIndexedIndirect，第 i 条从 byte_offset + i*stride 读取
// [indexCount, instanceCount, firstIndex, baseVertex, firstInstance]。instance_count=0（被剔）时硬件不绘制。
void WebGPURhiDevice::MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count,
                                               size_t stride, size_t byte_offset) {
    if (!cur_pass_ || draw_count <= 0) return;
    const BufferEntry* ind = FindBuffer(indirect_buffer);
    if (!ind || !ind->buffer) return;
    const PipelineCacheEntry* pe = nullptr;
    if (!BindPassDrawState(/*indexed=*/true, pe)) return;
    for (int i = 0; i < draw_count; ++i) {
        const uint64_t off = static_cast<uint64_t>(byte_offset) +
                             static_cast<uint64_t>(i) * static_cast<uint64_t>(stride);
        wgpuRenderPassEncoderDrawIndexedIndirect(cur_pass_, ind->buffer, off);
    }
    if (cur_pass_is_backbuffer_) backbuffer_drawn_ = true;
    last_frame_stats_.draw_calls += static_cast<uint32_t>(draw_count);
}

// Task 4 Subtask 2：Mega VAO——WebGPU 无 VAO 对象，仅创建并记录 VBO/IBO 句柄（BatchVertex 92B 布局
// 在 BindMegaVAO 时经引擎-facing CmdBindVertexBuffer 设置）。VBO 含 Vertex usage、IBO 含 Index usage，
// 经 CreateGpuBuffer 授予 WGPU usage 位（始终附带 CopyDst|CopySrc 以支持 UpdateMegaVBO/IBO 子区更新）。
VertexArrayHandle WebGPURhiDevice::CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                                 BufferHandle& out_vbo, BufferHandle& out_ibo) {
    if (vbo_size_bytes == 0 || ibo_size_bytes == 0) { out_vbo = {}; out_ibo = {}; return {}; }
    GpuBufferDesc vd; vd.size = vbo_size_bytes; vd.usage = GpuBufferUsage::kVertex;
    GpuBufferDesc id; id.size = ibo_size_bytes; id.usage = GpuBufferUsage::kIndex;
    BufferHandle vbo = CreateGpuBuffer(vd, nullptr);
    BufferHandle ibo = CreateGpuBuffer(id, nullptr);
    if (!vbo || !ibo) {
        if (vbo) DeleteGpuBuffer(vbo);
        if (ibo) DeleteGpuBuffer(ibo);
        out_vbo = {}; out_ibo = {};
        return {};
    }
    const unsigned int vao_id = next_mega_vao_id_++;
    mega_vaos_[vao_id] = MegaVaoEntry{vbo.raw(), ibo.raw()};
    out_vbo = vbo;
    out_ibo = ibo;
    return VertexArrayHandle{vao_id};
}

void WebGPURhiDevice::UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) {
    if (!vbo || size == 0 || !data) return;
    UpdateGpuBuffer(vbo, offset, size, data);
}

void WebGPURhiDevice::UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) {
    if (!ibo || size == 0 || !data) return;
    UpdateGpuBuffer(ibo, offset, size, data);
}

void WebGPURhiDevice::DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) {
    if (vao) mega_vaos_.erase(vao.raw());
    if (vbo) DeleteGpuBuffer(vbo);
    if (ibo) DeleteGpuBuffer(ibo);
}

// BindMegaVAO：据 VAO 句柄查 VBO/IBO，经引擎-facing CmdBindVertexBuffer（BatchVertex 92B 7 属性布局）+
// CmdBindIndexBuffer 设引擎 draw state，供后续（Multi）DrawIndexed(Indirect) 使用。
void WebGPURhiDevice::BindMegaVAO(VertexArrayHandle vao) {
    if (!vao) return;
    auto it = mega_vaos_.find(vao.raw());
    if (it == mega_vaos_.end()) return;
    // BatchVertex 92B 布局：pos(loc0,vec3,@0) color(loc1,vec4,@12) uv(loc2,vec2,@28)
    //   normal(loc3,vec3,@36) tangent(loc4,vec3,@48) weights(loc5,vec4,@60) joints(loc6,vec4,@76)。
    const std::vector<VertexAttr> attrs = {
        VertexAttr{0, 3, 0},
        VertexAttr{1, 4, 12},
        VertexAttr{2, 2, 28},
        VertexAttr{3, 3, 36},
        VertexAttr{4, 3, 48},
        VertexAttr{5, 4, 60},
        VertexAttr{6, 4, 76},
    };
    CmdBindVertexBuffer(0, it->second.vbo, 92, attrs, VertexInputRate::PerVertex);
    CmdBindIndexBuffer(it->second.ibo, IndexType::UInt32);
}

void WebGPURhiDevice::UnbindVAO() {
    // WebGPU 无 VAO 对象——清除已绑顶点/索引 draw state（等价 glBindVertexArray(0)）。
    cur_ib_handle_ = 0;
    if (!cur_vbs_.empty()) cur_vbs_[0].set = false;
}

// ============================================================
// Task 4 Subtask 3：GPU-driven PBR 着色器（手译 PBR WGSL）+ 纹理桶绑定 + 可用性查询
// ============================================================

// 手译 PBR WGSL：顶点经实例 SSBO(group3 b5) 取 model 变换 BatchVertex 几何，片元经材质 SSBO(group3 b9)
// 按实例 material_id 取 albedo/metallic/roughness、采样 albedo 纹理(group2 b0/b1)，Cook-Torrance 直接光
// + 环境项 + Reinhard/gamma。绑定约定：group1 b0=PerFrame、b1=PerScene；group3 b5=实例、b9=材质。
// 仅声明 albedo 纹理桶（其余 normal/MR/emissive/occlusion 桶由 BindGPUDrivenTextures 绑入但本着色器未声明，
// CollectGroupBindings 过滤未声明绑定）。
namespace {
const char* kGpuDrivenPBRWGSL = R"WGSL(// dse-wgsl
struct PerFrame {
  vp : mat4x4<f32>,
  view : mat4x4<f32>,
  camera_pos : vec4<f32>,
};
struct PerScene {
  light_dir : vec4<f32>,      // xyz=光行进方向, w=enabled
  light_color : vec4<f32>,    // rgb=颜色, w=ambient 强度
  light_params : vec4<f32>,   // x=intensity, y=shadow_strength, z=receive_shadow, w=0
};
struct Inst {
  model : mat4x4<f32>,
  material_id : u32,
  draw_cmd_id : u32,
  pad0 : u32,
  pad1 : u32,
};
struct Mat {
  albedo : vec4<f32>,         // rgb + metallic
  roughness_ao : vec4<f32>,   // roughness, ao, normal_strength, alpha_cutoff
  emissive : vec4<f32>,       // rgb + alpha_test
  flags : vec4<f32>,
  extra : vec4<f32>,
  extra2 : vec4<f32>,
  toon_shadow : vec4<f32>,
  toon_params : vec4<f32>,
};
@group(1) @binding(0) var<uniform> per_frame : PerFrame;
@group(1) @binding(1) var<uniform> per_scene : PerScene;
@group(3) @binding(5) var<storage, read> instances : array<Inst>;
@group(3) @binding(9) var<storage, read> materials : array<Mat>;
@group(2) @binding(0) var albedo_tex : texture_2d<f32>;
@group(2) @binding(1) var albedo_smp : sampler;

struct VsIn {
  @location(0) pos : vec3<f32>,
  @location(1) color : vec4<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) normal : vec3<f32>,
  @location(4) tangent : vec3<f32>,
  @location(5) weights : vec4<f32>,
  @location(6) joints : vec4<f32>,
};
struct VsOut {
  @builtin(position) clip : vec4<f32>,
  @location(0) world_pos : vec3<f32>,
  @location(1) world_normal : vec3<f32>,
  @location(2) uv : vec2<f32>,
  @location(3) @interpolate(flat) material_id : u32,
  @location(4) vcolor : vec4<f32>,
};

@vertex fn vs_main(i : VsIn, @builtin(instance_index) inst : u32) -> VsOut {
  let model = instances[inst].model;
  let wp = model * vec4<f32>(i.pos, 1.0);
  var o : VsOut;
  o.clip = per_frame.vp * wp;
  o.world_pos = wp.xyz;
  o.world_normal = normalize((model * vec4<f32>(i.normal, 0.0)).xyz);
  o.uv = i.uv;
  o.material_id = instances[inst].material_id;
  o.vcolor = i.color;
  return o;
}

const PI = 3.14159265359;
fn distributionGGX(ndh : f32, rough : f32) -> f32 {
  let a = rough * rough;
  let a2 = a * a;
  let d = ndh * ndh * (a2 - 1.0) + 1.0;
  return a2 / max(PI * d * d, 1e-4);
}
fn geometrySchlickGGX(ndv : f32, rough : f32) -> f32 {
  let r = rough + 1.0;
  let k = (r * r) / 8.0;
  return ndv / (ndv * (1.0 - k) + k);
}
fn fresnelSchlick(ct : f32, f0 : vec3<f32>) -> vec3<f32> {
  return f0 + (vec3<f32>(1.0) - f0) * pow(clamp(1.0 - ct, 0.0, 1.0), 5.0);
}

@fragment fn fs_main(i : VsOut) -> @location(0) vec4<f32> {
  let m = materials[i.material_id];
  let tex = textureSample(albedo_tex, albedo_smp, i.uv);
  // inline mesh 把实色烘进顶点色（材质 albedo=白）；file mesh 反之（顶点色=白）。两路均为 albedo×tex×vcolor。
  let base = m.albedo.rgb * tex.rgb * i.vcolor.rgb;
  let metallic = m.albedo.a;
  let rough = max(m.roughness_ao.x, 0.04);
  let ao = m.roughness_ao.y;

  let n = normalize(i.world_normal);
  let v = normalize(per_frame.camera_pos.xyz - i.world_pos);
  let l = normalize(-per_scene.light_dir.xyz);
  let h = normalize(v + l);
  let ndl = max(dot(n, l), 0.0);
  let ndv = max(dot(n, v), 0.0);
  let ndh = max(dot(n, h), 0.0);
  let hdv = max(dot(h, v), 0.0);

  let f0 = mix(vec3<f32>(0.04), base, metallic);
  let ndf = distributionGGX(ndh, rough);
  let g = geometrySchlickGGX(ndv, rough) * geometrySchlickGGX(ndl, rough);
  let f = fresnelSchlick(hdv, f0);
  let spec = (ndf * g * f) / max(4.0 * ndv * ndl, 1e-4);
  let kd = (vec3<f32>(1.0) - f) * (1.0 - metallic);
  let intensity = per_scene.light_params.x;
  let radiance = per_scene.light_color.rgb * intensity;
  let diffuse = kd * base / PI;
  var color = (diffuse + spec) * radiance * ndl;
  color += per_scene.light_color.a * base * ao;   // 环境项
  color += m.emissive.rgb;
  color = color / (color + vec3<f32>(1.0));        // Reinhard
  color = pow(color, vec3<f32>(1.0 / 2.2));        // gamma
  return vec4<f32>(color, 1.0);
}
)WGSL";
}  // namespace

// 惰性编译 GPU-driven PBR 程序 + 建 PSO（depth on、cull none、blend off）+ 默认白纹理 + PerFrame/PerScene UBO。
// 跨帧复用；编译失败置 failed 标记避免每帧重试。
bool WebGPURhiDevice::EnsureGpuDrivenPBRShader() {
    if (gpu_driven_pbr_failed_) return false;
    if (gpu_driven_pbr_program_ && gpu_driven_pbr_pso_ && white_texture_ &&
        gpu_driven_perframe_ubo_ && gpu_driven_perscene_ubo_) {
        return true;
    }
    if (!EnsureInitialized() || !device_) return false;
    if (!gpu_driven_pbr_program_) {
        gpu_driven_pbr_program_ = CreateShaderProgram(kGpuDrivenPBRWGSL, "");
        if (!gpu_driven_pbr_program_) {
            gpu_driven_pbr_failed_ = true;
            DEBUG_LOG_ERROR("WebGPU: GPU-driven PBR WGSL 编译失败，HasGPUDrivenPBRShader 将返回 false");
            return false;
        }
    }
    if (!gpu_driven_pbr_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = true;
        d.depth_write_enabled = true;
        d.depth_func = CompareFunc::Less;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        gpu_driven_pbr_pso_ = CreatePipelineState(d);
    }
    if (!white_texture_) {
        const unsigned char white[4] = {255, 255, 255, 255};
        white_texture_ = CreateTexture2D(1, 1, white, /*linear_filter=*/true);
    }
    if (!gpu_driven_perframe_ubo_) {
        GpuBufferDesc d; d.size = sizeof(PerFrameUBO); d.usage = GpuBufferUsage::kUniform;
        gpu_driven_perframe_ubo_ = CreateGpuBuffer(d, nullptr);
    }
    if (!gpu_driven_perscene_ubo_) {
        GpuBufferDesc d; d.size = sizeof(PerSceneUBO); d.usage = GpuBufferUsage::kUniform;
        gpu_driven_perscene_ubo_ = CreateGpuBuffer(d, nullptr);
    }
    return gpu_driven_pbr_program_ && gpu_driven_pbr_pso_ && white_texture_ &&
           gpu_driven_perframe_ubo_ && gpu_driven_perscene_ubo_;
}

bool WebGPURhiDevice::HasGPUDrivenPBRShader() const {
    // 惰性编译缓存查询（与 GL「init 时编译，运行时查句柄」语义对齐）。const 方法经 const_cast
    // 触发一次性惰性编译，后续直接返回缓存句柄状态。
    return const_cast<WebGPURhiDevice*>(this)->EnsureGpuDrivenPBRShader();
}

// 激活 GPU-driven PBR 程序 + PSO（设引擎 draw state，等价 glUseProgram + 深度/混合状态），上传 PerFrame
// (vp/view/camera_pos) + PerScene(light_dir/color/params) 并经引擎-facing CmdBindUniformBuffer 绑到 group1
// b0/b1。CSM 阴影矩阵/级联（PerScene 余字段）本子项离屏自检不涉及，置零。
void WebGPURhiDevice::SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                              const glm::vec3& camera_pos,
                                              const glm::vec3& light_dir, const glm::vec3& light_color,
                                              float light_intensity, float ambient_intensity,
                                              float shadow_strength) {
    if (!EnsureGpuDrivenPBRShader()) return;
    cur_program_    = gpu_driven_pbr_program_;
    cur_pso_handle_ = gpu_driven_pbr_pso_;

    PerFrameUBO per_frame{};
    per_frame.vp = proj * view;
    per_frame.view = view;
    per_frame.camera_pos = glm::vec4(camera_pos, 0.0f);
    UpdateGpuBuffer(gpu_driven_perframe_ubo_, 0, sizeof(per_frame), &per_frame);

    PerSceneUBO per_scene{};
    per_scene.light_dir_and_enabled   = glm::vec4(light_dir, 1.0f);
    per_scene.light_color_and_ambient = glm::vec4(light_color, ambient_intensity);
    const float receive_shadow = (shadow_strength > 0.0f) ? 1.0f : 0.0f;
    per_scene.light_params = glm::vec4(light_intensity, shadow_strength, receive_shadow, 0.0f);
    UpdateGpuBuffer(gpu_driven_perscene_ubo_, 0, sizeof(per_scene), &per_scene);

    CmdBindUniformBuffer(0, gpu_driven_perframe_ubo_.raw(), 0, sizeof(per_frame));
    CmdBindUniformBuffer(1, gpu_driven_perscene_ubo_.raw(), 0, sizeof(per_scene));
}

// 绑定 GPU-driven PBR 纹理桶：albedo→group2 slot0，余 normal/MR/emissive/occlusion→slot1..4（PBR WGSL 当前
// 仅声明 albedo，未声明者 CollectGroupBindings 过滤）。handle=0 → 默认白纹理回退（与 GL 一致）。
void WebGPURhiDevice::BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                                            unsigned int metallic_roughness,
                                            unsigned int emissive, unsigned int occlusion) {
    if (!white_texture_) EnsureGpuDrivenPBRShader();
    const unsigned int w = white_texture_;
    CmdBindTexture(0, albedo             ? albedo             : w, TextureDim::Tex2D);
    CmdBindTexture(1, normal             ? normal             : w, TextureDim::Tex2D);
    CmdBindTexture(2, metallic_roughness ? metallic_roughness : w, TextureDim::Tex2D);
    CmdBindTexture(3, emissive           ? emissive           : w, TextureDim::Tex2D);
    CmdBindTexture(4, occlusion          ? occlusion          : w, TextureDim::Tex2D);
}

// Subtask 4：Hi-Z 遮挡剔除资源——WebGPU 无 GL 的 glTexImage2D-per-mip，改经 CreateTextureImpl 建
//   R32Float 完整 mip 链纹理（usage=storage 写 + 采样读 + copy 源），nearest 过滤。hiz 句柄经独立
//   发号，登记 hiz_textures_[hiz]=引擎纹理句柄；GetHiZGpuTexture 返回该引擎句柄供消费方传给
//   SetComputeTextureImageMip（逐级下采样写）/ SetComputeTextureSampler（剔除采样读）。
unsigned int WebGPURhiDevice::CreateHiZTexture(int width, int height) {
    if (!EnsureInitialized() || !device_ || width <= 0 || height <= 0) return 0;

    int mip_count = 1;
    {
        int w = width, h = height;
        while (w > 1 || h > 1) {
            w = std::max(1, w / 2);
            h = std::max(1, h / 2);
            ++mip_count;
        }
    }

    const unsigned int tex = CreateTextureImpl(
        WGPUTextureDimension_2D, WGPUTextureViewDimension_2D,
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1,
        WGPUTextureFormat_R32Float,
        WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding |
            WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst,
        static_cast<uint32_t>(mip_count), 1, {nullptr},
        TextureSamplerDesc::FromLinearFlag(false));
    if (!tex) return 0;

    const unsigned int handle = NextHandle();
    hiz_textures_[handle] = tex;
    DEBUG_LOG_INFO("WebGPU Hi-Z texture created: handle={} tex={} {}x{} mips={}",
                   handle, tex, width, height, mip_count);
    return handle;
}

void WebGPURhiDevice::DeleteHiZTexture(unsigned int handle) {
    auto it = hiz_textures_.find(handle);
    if (it == hiz_textures_.end()) return;
    DeleteTexture(it->second);
    hiz_textures_.erase(it);
}

int WebGPURhiDevice::GetHiZMipCount(unsigned int handle) const {
    auto it = hiz_textures_.find(handle);
    if (it == hiz_textures_.end()) return 0;
    const TextureEntry* e = FindTexture(it->second);
    return e ? static_cast<int>(e->mip_levels) : 0;
}

unsigned int WebGPURhiDevice::GetHiZGpuTexture(unsigned int handle) const {
    auto it = hiz_textures_.find(handle);
    return it != hiz_textures_.end() ? it->second : 0;
}

void WebGPURhiDevice::CmdDispatchComputePass(const ComputeDispatch& dispatch) { (void)dispatch; }

// ============================================================
// B3a：Compute 基础设施实现（compute 管线 + SSBO + indirect 原语 + WGSL 自检）
// ============================================================

unsigned int WebGPURhiDevice::CreateComputeShader(const std::string& source) {
    if (!EnsureInitialized() || !device_) return 0;
    // 仅接受 WGSL（首非空行 `// dse-wgsl`）：引擎 GLSL/SPIR-V compute 无离线转译，返回 0 跳过。
    const char* kSentinel = "// dse-wgsl";
    const size_t first = source.find_first_not_of(" \t\r\n");
    const bool is_wgsl = first != std::string::npos &&
                         source.compare(first, std::strlen(kSentinel), kSentinel) == 0;
    if (!is_wgsl) {
        DEBUG_LOG_WARN("WebGPU: CreateComputeShader 收到非 WGSL 源（无 // dse-wgsl 标记），跳过（返回 0）");
        return 0;
    }
    ComputeShaderEntry e;
    e.module = CompileWGSL(source, "dse-wgsl-compute");
    if (!e.module) {
        DEBUG_LOG_ERROR("WebGPU: compute WGSL 编译失败（module 为空）");
        return 0;
    }
    // 入口名：默认 cs_main，允许 `fn main` 兜底。
    if (source.find("fn cs_main") == std::string::npos &&
        source.find("fn main") != std::string::npos) {
        e.entry = "main";
    }
    ParseWgslBindings(source, e.wgsl_bindings);
    const unsigned int h = NextHandle();
    compute_shaders_[h] = std::move(e);
    return h;
}

unsigned int WebGPURhiDevice::CreateComputeShaderEx(
    const std::string& /*gl_src*/, const std::string& /*vk_src*/, const std::string& /*hlsl_src*/,
    uint32_t /*ssbo_count*/, uint32_t /*storage_image_count*/, uint32_t /*sampler_count*/,
    uint32_t /*push_constant_bytes*/, const std::string& wgsl_src) {
    // B3b：WebGPU 仅消费手写 WGSL 源槽。空槽表示该 compute 特性尚未手译 WGSL（如 GPU-driven
    // 剔除 / HiZ / skinning / hair / grass）——返回 0，调用方按句柄 0 优雅回退到 CPU/无该特性。
    // 布局计数（ssbo/img/smp/pc）不需要：compute 管线 layout 由 WGSL @group/@binding 解析驱动。
    if (wgsl_src.empty()) return 0;
    return CreateComputeShader(wgsl_src);
}

void WebGPURhiDevice::DeleteComputeShader(unsigned int handle) {
    auto it = compute_shaders_.find(handle);
    if (it == compute_shaders_.end()) return;
    if (it->second.module) wgpuShaderModuleRelease(it->second.module);
    compute_shaders_.erase(it);
}

void WebGPURhiDevice::BeginComputePass() {
    // 不可与 render pass 嵌套；同一时刻仅一个 compute pass。
    if (!frame_encoder_ || cur_compute_pass_ || cur_pass_) return;
    cur_compute_pass_ = wgpuCommandEncoderBeginComputePass(frame_encoder_, nullptr);
}

void WebGPURhiDevice::EndComputePass() {
    if (!cur_compute_pass_) return;
    wgpuComputePassEncoderEnd(cur_compute_pass_);
    wgpuComputePassEncoderRelease(cur_compute_pass_);
    cur_compute_pass_ = nullptr;
}

std::vector<WebGPURhiDevice::BindingInfo>
WebGPURhiDevice::CollectComputeGroupBindings(uint32_t group, const ComputeShaderEntry& sh) {
    // 对齐 render 路径的 group 约定，compute 仅接 group1=UBO、group3=SSBO（可见性 Compute）；
    // group0（push）/group2（texture+sampler）在 B3a 暂不接入（无 compute 纹理用例）。
    std::vector<BindingInfo> out;
    const WGPUShaderStageFlags kCs = WGPUShaderStage_Compute;
    auto declared = [&](uint32_t binding) -> bool {
        return sh.wgsl_bindings.count((group << 16) | binding) != 0;
    };
    switch (group) {
        case 1: {
            for (const auto& [slot, u] : cur_ubos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = FindBuffer(u.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Uniform; b.visibility = kCs;
                b.buffer = be->buffer; b.buf_offset = u.offset;
                b.buf_size = u.size ? u.size : be->size;
                out.push_back(b);
            }
            // B3b-8：命名 uniform 块（SetComputeUniform* 累积）→ group1 保留 binding。仅当着色器
            //   声明该 binding 且本次 dispatch 有命名 uniform 切片时纳入。
            if (cur_compute_named_buffer_ && declared(kComputeNamedUboBinding)) {
                BindingInfo b;
                b.binding = kComputeNamedUboBinding; b.kind = BindingInfo::Kind::Uniform; b.visibility = kCs;
                b.buffer = cur_compute_named_buffer_;
                b.buf_offset = cur_compute_named_offset_;
                b.buf_size = cur_compute_named_size_;
                out.push_back(b);
            }
            break;
        }
        case 2: {
            // 采样纹理占用的槽集合：storage image 若与之同槽（GL sampler/image 分命名空间但 WebGPU 合一），
            //   挪到 slot + kComputeStorageBindingBase 错开（仅 Hi-Z copy 命中：深度采样 @0 + hiz_mip0 storage @0）。
            std::set<unsigned int> sampled_slots;
            for (const auto& [slot, h] : cur_compute_textures_) sampled_slots.insert(slot);
            for (const auto& [slot, vb] : cur_compute_texture_views_) sampled_slots.insert(slot);
            auto storage_binding = [&](unsigned int slot) -> uint32_t {
                return sampled_slots.count(slot)
                           ? slot + kComputeStorageBindingBase
                           : static_cast<uint32_t>(slot);
            };
            // B3b-5：compute 只读采样纹理（texture_2d<f32>/texture_depth_2d，textureLoad，无 sampler）。
            // r32float 为 unfilterable-float，深度格式为 depth sampleType，需相应声明。@binding=slot。
            for (const auto& [slot, tex_handle] : cur_compute_textures_) {
                if (!declared(slot)) continue;
                const TextureEntry* te = FindTexture(tex_handle);
                if (!te || !te->view) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Texture; b.visibility = kCs;
                b.view = te->view; b.view_dim = te->view_dim;
                b.sample_type = IsDepthFormat(te->format)
                                    ? WGPUTextureSampleType_Depth
                                    : (te->format == WGPUTextureFormat_R32Float
                                           ? WGPUTextureSampleType_UnfilterableFloat
                                           : WGPUTextureSampleType_Float);
                out.push_back(b);
            }
            // B3b-4：compute storage image（texture_storage_2d<...,write>）。@binding=slot（冲突时错开）。
            for (const auto& [slot, tex_handle] : cur_compute_images_) {
                const uint32_t bnd = storage_binding(slot);
                if (!declared(bnd)) continue;
                const TextureEntry* te = FindTexture(tex_handle);
                if (!te || !te->view) continue;
                BindingInfo b;
                b.binding = bnd; b.kind = BindingInfo::Kind::StorageTexture; b.visibility = kCs;
                b.view = te->view; b.tex_format = te->format; b.view_dim = te->view_dim;
                out.push_back(b);
            }
            // B3b-6：compute 显式视图绑定（per-mip Hi-Z 金字塔）。采样读视图（textureLoad，无 sampler）。
            for (const auto& [slot, vb] : cur_compute_texture_views_) {
                if (!declared(slot) || !vb.view) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Texture; b.visibility = kCs;
                b.view = vb.view; b.view_dim = vb.view_dim;
                b.sample_type = (vb.format == WGPUTextureFormat_R32Float)
                                    ? WGPUTextureSampleType_UnfilterableFloat
                                    : WGPUTextureSampleType_Float;
                out.push_back(b);
            }
            // B3b-6：storage 写显式视图（texture_storage_2d<...,write>，绑定单 mip 视图）。冲突时错开。
            for (const auto& [slot, vb] : cur_compute_image_views_) {
                const uint32_t bnd = storage_binding(slot);
                if (!declared(bnd) || !vb.view) continue;
                BindingInfo b;
                b.binding = bnd; b.kind = BindingInfo::Kind::StorageTexture; b.visibility = kCs;
                b.view = vb.view; b.tex_format = vb.format; b.view_dim = vb.view_dim;
                out.push_back(b);
            }
            break;
        }
        case 3: {
            for (const auto& [slot, s] : cur_ssbos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = FindBuffer(s.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Storage; b.visibility = kCs;
                b.buffer = be->buffer; b.buf_offset = s.offset;
                b.buf_size = s.size ? s.size : be->size;
                out.push_back(b);
            }
            break;
        }
        default: break;
    }
    return out;
}

const WebGPURhiDevice::ComputePipelineCacheEntry*
WebGPURhiDevice::GetOrCreateComputePipeline(unsigned int shader_handle) {
    auto sit = compute_shaders_.find(shader_handle);
    if (sit == compute_shaders_.end() || !sit->second.module) return nullptr;
    const ComputeShaderEntry& sh = sit->second;

    // 缓存签名 = shader + 绑定签名（binding 号 + 种类）。
    std::string key = "cs" + std::to_string(shader_handle);
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectComputeGroupBindings(g, sh);
        key += "g" + std::to_string(g);
        for (const BindingInfo& b : bs)
            key += ":" + std::to_string(b.binding) + "-" + std::to_string(static_cast<int>(b.kind));
    }
    auto cit = compute_pipeline_cache_.find(key);
    if (cit != compute_pipeline_cache_.end()) return &cit->second;

    ComputePipelineCacheEntry entry{};
    // 4 组 explicit BGL（与 BindGroup 共用同序绑定签名）；未用组建空 BGL（与 render 路径一致）。
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectComputeGroupBindings(g, sh);
        std::vector<WGPUBindGroupLayoutEntry> bgle;
        bgle.reserve(bs.size());
        for (const BindingInfo& b : bs) {
            WGPUBindGroupLayoutEntry e{};
            e.binding = b.binding;
            e.visibility = b.visibility;
            switch (b.kind) {
                case BindingInfo::Kind::Uniform:
                    e.buffer.type = WGPUBufferBindingType_Uniform; break;
                case BindingInfo::Kind::Storage:
                    // compute SSBO 默认 read_write（自检着色器写 outbuf/draw）；只读 storage 留待按声明细化。
                    e.buffer.type = WGPUBufferBindingType_Storage; break;
                case BindingInfo::Kind::Texture:
                    e.texture.sampleType = b.sample_type;
                    e.texture.viewDimension = b.view_dim;
                    e.texture.multisampled = false; break;
                case BindingInfo::Kind::Sampler:
                    e.sampler.type = WGPUSamplerBindingType_Filtering; break;
                case BindingInfo::Kind::StorageTexture:
                    // B3b-4：compute 写 storage image（rgba8unorm 仅 WriteOnly 访问）。
                    e.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
                    e.storageTexture.format = b.tex_format;
                    e.storageTexture.viewDimension = b.view_dim; break;
            }
            bgle.push_back(e);
        }
        WGPUBindGroupLayoutDescriptor bgld{};
        bgld.entryCount = bgle.size();
        bgld.entries = bgle.empty() ? nullptr : bgle.data();
        entry.bgls[g] = wgpuDeviceCreateBindGroupLayout(device_, &bgld);
    }

    WGPUPipelineLayoutDescriptor pld{};
    pld.bindGroupLayoutCount = 4;
    pld.bindGroupLayouts = entry.bgls;
    entry.layout = wgpuDeviceCreatePipelineLayout(device_, &pld);

    WGPUComputePipelineDescriptor cpd{};
    cpd.layout = entry.layout;
    cpd.compute.module = sh.module;
    cpd.compute.entryPoint = sh.entry.c_str();
    entry.pipeline = wgpuDeviceCreateComputePipeline(device_, &cpd);
    if (!entry.pipeline) {
        DEBUG_LOG_ERROR("WebGPU: wgpuDeviceCreateComputePipeline 失败 (shader={})", shader_handle);
    }

    auto res = compute_pipeline_cache_.emplace(std::move(key), entry);
    return &res.first->second;
}

void WebGPURhiDevice::DispatchCompute(unsigned int shader_handle,
                                      unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
    if (!cur_compute_pass_ || groups_x == 0 || groups_y == 0 || groups_z == 0) return;
    auto it = compute_shaders_.find(shader_handle);
    if (it == compute_shaders_.end() || !it->second.module) return;

    // B3b-8：命名 uniform 暂存（SetComputeUniform* 按调用序 16B 对齐累积）→ 经 UBO 版本环分配独立
    //   256 对齐切片并上传，供 group1 保留 binding 绑定。版本环切片避免同一帧多 dispatch 写同一缓冲
    //   被 wgpuQueueWriteBuffer 合并致仅见最后一次写入（与 UBO dynamic 版本同隐患）。须在管线/BindGroup
    //   组装前完成，使 CollectComputeGroupBindings 见到一致绑定签名。
    cur_compute_named_buffer_ = nullptr;
    cur_compute_named_offset_ = 0;
    cur_compute_named_size_   = 0;
    if (!compute_named_staging_.empty()) {
        // WGSL uniform struct 尺寸按 16B 向上取整；绑定字节数须 ≥ 该尺寸，否则校验失败
        //（如末成员 i32@96 → 暂存 100B，而 struct 取整为 112B）。补零至 16B 倍数后整块上传。
        compute_named_staging_.resize((compute_named_staging_.size() + 15u) & ~size_t(15), 0);
        const uint64_t off = AllocUboVersion(compute_named_staging_.data(), compute_named_staging_.size());
        if (off != UINT64_MAX) {
            cur_compute_named_buffer_ = ubo_ring_;
            cur_compute_named_offset_ = off;
            cur_compute_named_size_   = compute_named_staging_.size();
        }
    }

    const ComputePipelineCacheEntry* pe = GetOrCreateComputePipeline(shader_handle);
    if (!pe || !pe->pipeline) return;

    wgpuComputePassEncoderSetPipeline(cur_compute_pass_, pe->pipeline);
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectComputeGroupBindings(g, it->second);
        std::vector<WGPUBindGroupEntry> bge;
        bge.reserve(bs.size());
        for (const BindingInfo& b : bs) {
            WGPUBindGroupEntry e{};
            e.binding = b.binding;
            switch (b.kind) {
                case BindingInfo::Kind::Texture:        e.textureView = b.view; break;
                case BindingInfo::Kind::StorageTexture: e.textureView = b.view; break;
                case BindingInfo::Kind::Sampler:        e.sampler = b.sampler; break;
                default: e.buffer = b.buffer; e.offset = b.buf_offset; e.size = b.buf_size; break;
            }
            bge.push_back(e);
        }
        WGPUBindGroupDescriptor bgd{};
        bgd.layout = pe->bgls[g];
        bgd.entryCount = bge.size();
        bgd.entries = bge.empty() ? nullptr : bge.data();
        WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device_, &bgd);
        wgpuComputePassEncoderSetBindGroup(cur_compute_pass_, g, bg, 0, nullptr);
        frame_bindgroups_.push_back(bg);  // 提交后统一释放
    }
    wgpuComputePassEncoderDispatchWorkgroups(cur_compute_pass_, groups_x, groups_y, groups_z);

    // B3b-8：每次 dispatch 自带一组命名 uniform——上传后清空暂存与定位表（与 GL/DX11 同义）。
    compute_named_staging_.clear();
    compute_named_offsets_.clear();
    compute_named_next_ = 0;
}

bool WebGPURhiDevice::RecordComputeSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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
    if (!ct_shader_) ct_shader_ = CreateComputeShaderEx("", "", "", 1, 0, 0, 0, kComputeSelfTestWGSL);
    if (!ct_params_) {
        const uint32_t params[4] = {kCtN, kCtBase, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        ct_params_ = CreateGpuBuffer(d, params).raw();
    }
    if (!ct_out_) {
        GpuBufferDesc d; d.size = kCtN * sizeof(uint32_t); d.usage = GpuBufferUsage::kStorage;
        ct_out_ = CreateGpuBuffer(d, nullptr).raw();
    }
    if (!ct_draw_) {
        GpuBufferDesc d; d.size = kCtDrawWords * sizeof(uint32_t);
        d.usage = GpuBufferUsage::kStorage | GpuBufferUsage::kIndirect;
        ct_draw_ = CreateGpuBuffer(d, nullptr).raw();
    }
    if (!ct_shader_ || !ct_params_ || !ct_out_ || !ct_draw_) {
        DEBUG_LOG_ERROR("WebGPU[B3a] compute 自检资源创建失败，跳过");
        return false;
    }

    // 经与引擎相同的命令录制状态绑定资源（group1 b0 UBO；group3 b0/b1 SSBO）。
    ResetDrawState();
    CmdBindUniformBuffer(0, ct_params_, 0, sizeof(uint32_t) * 4);
    CmdBindStorageBuffer(0, ct_out_, 0, kCtN * sizeof(uint32_t));
    CmdBindStorageBuffer(1, ct_draw_, 0, kCtDrawWords * sizeof(uint32_t));

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(ct_shader_, (kCtN + 63u) / 64u, 1, 1);
    EndComputePass();
    ResetDrawState();

    // 回读缓冲（MapRead|CopyDst）+ storage→回读 拷贝（在本帧 frame_encoder_ 上录制，随帧提交）。
    auto make_rb = [&](uint64_t bytes) -> WGPUBuffer {
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bd.size = AlignUp4(bytes);
        return wgpuDeviceCreateBuffer(device_, &bd);
    };
    ct_rb_out_ = make_rb(kCtN * sizeof(uint32_t));
    ct_rb_draw_ = make_rb(kCtDrawWords * sizeof(uint32_t));
    const BufferEntry* be_out = FindBuffer(ct_out_);
    const BufferEntry* be_draw = FindBuffer(ct_draw_);
    if (!ct_rb_out_ || !ct_rb_draw_ || !be_out || !be_draw) {
        if (ct_rb_out_) { wgpuBufferRelease(ct_rb_out_); ct_rb_out_ = nullptr; }
        if (ct_rb_draw_) { wgpuBufferRelease(ct_rb_draw_); ct_rb_draw_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_out->buffer, 0, ct_rb_out_, 0,
                                         kCtN * sizeof(uint32_t));
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_draw->buffer, 0, ct_rb_draw_, 0,
                                         kCtDrawWords * sizeof(uint32_t));
    return true;
}

void WebGPURhiDevice::KickComputeSelfTestReadback() {
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
bool WebGPURhiDevice::RecordGpuCullSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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
        gc_cull_shader_ = CreateComputeShaderEx("", "", "", 2, 0, 0, 0, kCullWGSL);

    // 剔除参数：保留区域 x∈[-1,1] y∈[-1,1] z∈[-10,10]（6 视锥面），实例数 4。
    if (!gc_params_ubo_) {
        struct CullParams { float planes[6][4]; uint32_t object_count; uint32_t pad[3]; } p{};
        const float planes[6][4] = {
            { 1, 0, 0, 1}, {-1, 0, 0, 1}, {0,  1, 0, 1},
            { 0,-1, 0, 1}, { 0, 0, 1,10}, {0,  0,-1,10}};
        std::memcpy(p.planes, planes, sizeof(planes));
        p.object_count = kGcInstances;
        GpuBufferDesc d; d.size = sizeof(p); d.usage = GpuBufferUsage::kUniform;
        gc_params_ubo_ = CreateGpuBuffer(d, &p).raw();
    }
    // 4 实例 AABB：inst0 原点(视锥内) / inst1 x=5(出界) / inst2 z=5(界内) / inst3 y=5(出界)。
    if (!gc_aabb_ssbo_) {
        const float aabbs[kGcInstances][8] = {
            {-0.4f,-0.4f,-0.4f,0, 0.4f,0.4f,0.4f,0},   // inst0 → 可见
            { 4.6f,-0.4f,-0.4f,0, 5.4f,0.4f,0.4f,0},   // inst1 → 剔除(x>1)
            {-0.4f,-0.4f, 4.6f,0, 0.4f,0.4f,5.4f,0},   // inst2 → 可见
            {-0.4f, 4.6f,-0.4f,0, 0.4f,5.4f,0.4f,0}};  // inst3 → 剔除(y>1)
        GpuBufferDesc d; d.size = sizeof(aabbs); d.usage = GpuBufferUsage::kStorage;
        gc_aabb_ssbo_ = CreateGpuBuffer(d, aabbs).raw();
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
        gc_draw_ssbo_ = CreateGpuBuffer(d, cmds).raw();
    }

    // --- 离屏 RT（64×64 RGBA8）+ 渲染管线 + 4 象限不同色 quad 顶点/索引缓冲 ---
    if (!gc_rt_tex_) {
        WGPUTextureDescriptor td{};
        td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        td.dimension = WGPUTextureDimension_2D;
        td.size = {kGcRtSize, kGcRtSize, 1};
        td.format = WGPUTextureFormat_RGBA8Unorm;
        td.mipLevelCount = 1; td.sampleCount = 1;
        gc_rt_tex_ = wgpuDeviceCreateTexture(device_, &td);
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
        gc_render_module_ = CompileWGSL(kRenderWGSL, "dse-gpu-cull-selftest-render");
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
        gc_pipeline_ = wgpuDeviceCreateRenderPipeline(device_, &rpd);
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
        gc_vbo_ = wgpuDeviceCreateBuffer(device_, &bd);
        if (gc_vbo_) wgpuQueueWriteBuffer(queue_, gc_vbo_, 0, verts, sizeof(verts));
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
        gc_ibo_ = wgpuDeviceCreateBuffer(device_, &bd);
        if (gc_ibo_) wgpuQueueWriteBuffer(queue_, gc_ibo_, 0, idx, sizeof(idx));
    }

    if (!gc_cull_shader_ || !gc_params_ubo_ || !gc_aabb_ssbo_ || !gc_draw_ssbo_ ||
        !gc_rt_view_ || !gc_pipeline_ || !gc_vbo_ || !gc_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-2] GPU-driven 剔除自检资源创建失败，跳过");
        return false;
    }

    // --- 录制 1：视锥剔除 compute（写 draw cmds instance_count）---
    ResetDrawState();
    CmdBindUniformBuffer(0, gc_params_ubo_, 0, 112);
    CmdBindStorageBuffer(0, gc_aabb_ssbo_, 0, kGcInstances * 32);
    CmdBindStorageBuffer(1, gc_draw_ssbo_, 0, kGcInstances * kGcDrawWords * sizeof(uint32_t));
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(gc_cull_shader_, 1, 1, 1);  // 4 实例 < 64 线程 → 1 workgroup
    EndComputePass();
    ResetDrawState();

    // --- 录制 2：真 DrawIndexedIndirect 渲到离屏 RT（被剔实例 instance_count=0 → 不绘制）---
    const BufferEntry* be_draw = FindBuffer(gc_draw_ssbo_);
    if (!be_draw || !be_draw->buffer) return false;
    WGPURenderPassColorAttachment att{};
    att.view = gc_rt_view_;
    att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};
    WGPURenderPassDescriptor pd{};
    pd.colorAttachmentCount = 1; pd.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(frame_encoder_, &pd);
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
        return wgpuDeviceCreateBuffer(device_, &bd);
    };
    gc_rb_draw_ = make_rb(kGcInstances * kGcDrawWords * sizeof(uint32_t));
    gc_rb_pixels_ = make_rb(kGcRtBytes);
    if (!gc_rb_draw_ || !gc_rb_pixels_) {
        if (gc_rb_draw_) { wgpuBufferRelease(gc_rb_draw_); gc_rb_draw_ = nullptr; }
        if (gc_rb_pixels_) { wgpuBufferRelease(gc_rb_pixels_); gc_rb_pixels_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_draw->buffer, 0, gc_rb_draw_, 0,
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickGpuCullSelfTestReadback() {
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
bool WebGPURhiDevice::RecordSkinningSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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
        sk_shader_ = CreateComputeShaderEx("", "", "", 5, 0, 0, 0, kSkinningWGSL);

    // 参数 UBO：总顶点数 / 实例数。
    if (!sk_params_ubo_) {
        const uint32_t params[4] = {kSkVertices, kSkInstances, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        sk_params_ubo_ = CreateGpuBuffer(d, params).raw();
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
        sk_src_ssbo_ = CreateGpuBuffer(d, src).raw();
    }
    // 蒙皮后顶点：compute 写入（storage）+ 作绘制顶点缓冲（vertex）。
    if (!sk_dst_ssbo_) {
        GpuBufferDesc d; d.size = kSkVertices * kSkDstFloats * sizeof(float);
        d.usage = GpuBufferUsage::kStorage | GpuBufferUsage::kVertex;
        sk_dst_ssbo_ = CreateGpuBuffer(d, nullptr).raw();
    }
    // 骨骼矩阵调色板（列主序）：bone0 = 平移(kSkBoneTx,kSkBoneTy,0)，bone1 = 单位（未引用）。
    if (!sk_bone_ssbo_) {
        float bones[kSkBones * 16] = {
            1,0,0,0, 0,1,0,0, 0,0,1,0, kSkBoneTx,kSkBoneTy,0,1,   // bone0：平移
            1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};                  // bone1：单位
        GpuBufferDesc d; d.size = sizeof(bones); d.usage = GpuBufferUsage::kStorage;
        sk_bone_ssbo_ = CreateGpuBuffer(d, bones).raw();
    }
    // morph delta（占位，本自检 morph_target_count=0 不访问；仍需存在并绑定以匹配着色器声明）。
    if (!sk_morph_ssbo_) {
        float zeros[kSkVertices * 4] = {0};
        GpuBufferDesc d; d.size = sizeof(zeros); d.usage = GpuBufferUsage::kStorage;
        sk_morph_ssbo_ = CreateGpuBuffer(d, zeros).raw();
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
        sk_inst_ssbo_ = CreateGpuBuffer(d, &inst).raw();
    }

    // --- 离屏 RT（64×64 RGBA8）+ 渲染管线（顶点拉取蒙皮后顶点 + 固定红色）+ 索引缓冲 ---
    if (!sk_rt_tex_) {
        WGPUTextureDescriptor td{};
        td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
        td.dimension = WGPUTextureDimension_2D;
        td.size = {kSkRtSize, kSkRtSize, 1};
        td.format = WGPUTextureFormat_RGBA8Unorm;
        td.mipLevelCount = 1; td.sampleCount = 1;
        sk_rt_tex_ = wgpuDeviceCreateTexture(device_, &td);
        if (sk_rt_tex_) sk_rt_view_ = wgpuTextureCreateView(sk_rt_tex_, nullptr);
    }
    if (!sk_render_module_) {
        static const char* kRenderWGSL = R"WGSL(
@vertex fn vs_main(@location(0) p : vec2<f32>) -> @builtin(position) vec4<f32> {
  return vec4<f32>(p, 0.0, 1.0);
}
@fragment fn fs_main() -> @location(0) vec4<f32> { return vec4<f32>(1.0, 0.0, 0.0, 1.0); }
)WGSL";
        sk_render_module_ = CompileWGSL(kRenderWGSL, "dse-skinning-selftest-render");
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
        sk_pipeline_ = wgpuDeviceCreateRenderPipeline(device_, &rpd);
    }
    if (!sk_ibo_) {
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        WGPUBufferDescriptor bd{};
        bd.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
        bd.size = sizeof(idx);
        sk_ibo_ = wgpuDeviceCreateBuffer(device_, &bd);
        if (sk_ibo_) wgpuQueueWriteBuffer(queue_, sk_ibo_, 0, idx, sizeof(idx));
    }

    if (!sk_shader_ || !sk_params_ubo_ || !sk_src_ssbo_ || !sk_dst_ssbo_ || !sk_bone_ssbo_ ||
        !sk_morph_ssbo_ || !sk_inst_ssbo_ || !sk_rt_view_ || !sk_pipeline_ || !sk_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-3] GPU 蒙皮自检资源创建失败，跳过");
        return false;
    }

    // --- 录制 1：蒙皮 compute（写 dst 蒙皮后顶点）---
    ResetDrawState();
    CmdBindUniformBuffer(0, sk_params_ubo_, 0, sizeof(uint32_t) * 4);
    CmdBindStorageBuffer(0, sk_src_ssbo_,  0, kSkVertices * kSkSrcFloats * sizeof(float));
    CmdBindStorageBuffer(1, sk_dst_ssbo_,  0, kSkVertices * kSkDstFloats * sizeof(float));
    CmdBindStorageBuffer(2, sk_bone_ssbo_, 0, kSkBones * 16 * sizeof(float));
    CmdBindStorageBuffer(3, sk_morph_ssbo_,0, kSkVertices * 4 * sizeof(float));
    CmdBindStorageBuffer(4, sk_inst_ssbo_, 0, 48);
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(sk_shader_, (kSkVertices + 63u) / 64u, 1, 1);
    EndComputePass();
    ResetDrawState();

    // --- 录制 2：真绘制（顶点缓冲 = 蒙皮后 dst SSBO）渲到离屏 RT ---
    const BufferEntry* be_dst = FindBuffer(sk_dst_ssbo_);
    if (!be_dst || !be_dst->buffer) return false;
    WGPURenderPassColorAttachment att{};
    att.view = sk_rt_view_;
    att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};
    WGPURenderPassDescriptor pd{};
    pd.colorAttachmentCount = 1; pd.colorAttachments = &att;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(frame_encoder_, &pd);
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
        return wgpuDeviceCreateBuffer(device_, &bd);
    };
    sk_rb_dst_ = make_rb(kSkVertices * kSkDstFloats * sizeof(float));
    sk_rb_pixels_ = make_rb(kSkRtBytes);
    if (!sk_rb_dst_ || !sk_rb_pixels_) {
        if (sk_rb_dst_)    { wgpuBufferRelease(sk_rb_dst_);    sk_rb_dst_ = nullptr; }
        if (sk_rb_pixels_) { wgpuBufferRelease(sk_rb_pixels_); sk_rb_pixels_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_dst->buffer, 0, sk_rb_dst_, 0,
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickSkinningSelfTestReadback() {
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
bool WebGPURhiDevice::RecordStorageImageSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!si_shader_) si_shader_ = CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kStorageImageWGSL);
    if (!si_params_ubo_) {
        const uint32_t params[4] = {kSiDim, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        si_params_ubo_ = CreateGpuBuffer(d, params).raw();
    }
    if (!si_image_) si_image_ = CreateComputeWriteTexture2D(static_cast<int>(kSiDim), static_cast<int>(kSiDim));
    if (!si_shader_ || !si_params_ubo_ || !si_image_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-4] storage-image 自检资源创建失败，跳过");
        return false;
    }

    // 经与引擎相同的命令录制状态绑定资源（group1 b0 UBO；group2 b0 storage image）。
    ResetDrawState();
    CmdBindUniformBuffer(0, si_params_ubo_, 0, sizeof(uint32_t) * 4);
    SetComputeTextureImage(0, si_image_, /*read_only=*/false);

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(si_shader_, (kSiDim + 7u) / 8u, (kSiDim + 7u) / 8u, 1);
    EndComputePass();
    ResetDrawState();

    // copy storage 纹理 → 回读缓冲（MapRead|CopyDst；随帧提交）。
    const TextureEntry* te = FindTexture(si_image_);
    if (!te || !te->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kSiBytes);
    si_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickStorageImageSelfTestReadback() {
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
bool WebGPURhiDevice::RecordHiZDownsampleSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!hz_gen_shader_)  hz_gen_shader_  = CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kHzGenWGSL);
    if (!hz_down_shader_) hz_down_shader_ = CreateComputeShaderEx("", "", "", 0, 1, 1, 0, kHzDownWGSL);
    if (!hz_gen_ubo_) {
        const uint32_t params[4] = {kHzSrcDim, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        hz_gen_ubo_ = CreateGpuBuffer(d, params).raw();
    }
    if (!hz_down_ubo_) {
        const uint32_t params[4] = {kHzSrcDim, kHzDstDim, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        hz_down_ubo_ = CreateGpuBuffer(d, params).raw();
    }
    // src：生成趟 storage 写 + 下采样趟采样读；dst：下采样趟 storage 写 + copy 源。r32float 为
    // WebGPU 保证支持 storage 写的格式之一；作采样纹理时为 unfilterable-float（仅 textureLoad）。
    if (!hz_src_tex_) {
        hz_src_tex_ = CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kHzSrcDim, kHzSrcDim, 1,
            WGPUTextureFormat_R32Float,
            WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            1, 1, {nullptr}, TextureSamplerDesc::FromLinearFlag(false));
    }
    if (!hz_dst_tex_) {
        hz_dst_tex_ = CreateTextureImpl(
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
    ResetDrawState();
    CmdBindUniformBuffer(0, hz_gen_ubo_, 0, sizeof(uint32_t) * 4);
    SetComputeTextureImage(0, hz_src_tex_, /*read_only=*/false);
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(hz_gen_shader_, (kHzSrcDim + 7u) / 8u, (kHzSrcDim + 7u) / 8u, 1);
    EndComputePass();
    ResetDrawState();

    // ②下采样趟（独立 compute pass：pass 间自动屏障保证 src 写对下采样趟可见）。
    CmdBindUniformBuffer(0, hz_down_ubo_, 0, sizeof(uint32_t) * 4);
    SetComputeTextureImage(0, hz_src_tex_, /*read_only=*/true);
    SetComputeTextureImage(1, hz_dst_tex_, /*read_only=*/false);
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(hz_down_shader_, (kHzDstDim + 7u) / 8u, (kHzDstDim + 7u) / 8u, 1);
    EndComputePass();
    ResetDrawState();

    // copy dst storage 纹理 → 回读缓冲（MapRead|CopyDst；随帧提交）。
    const TextureEntry* te = FindTexture(hz_dst_tex_);
    if (!te || !te->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kHzDstBytes;
    hz_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickHiZDownsampleSelfTestReadback() {
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
bool WebGPURhiDevice::RecordHiZPyramidSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!hzp_gen_shader_)  hzp_gen_shader_  = CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kHzpGenWGSL);
    if (!hzp_down_shader_) hzp_down_shader_ = CreateComputeShaderEx("", "", "", 0, 1, 1, 0, kHzpDownWGSL);
    if (!hzp_gen_ubo_) {
        const uint32_t params[4] = {kHzpBaseDim, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        hzp_gen_ubo_ = CreateGpuBuffer(d, params).raw();
    }
    if (hzp_down_ubos_.empty()) {
        for (uint32_t k = 1; k < kHzpMips; ++k) {
            const uint32_t src_dim = kHzpBaseDim >> (k - 1);
            const uint32_t dst_dim = kHzpBaseDim >> k;
            const uint32_t params[4] = {src_dim, dst_dim, 0u, 0u};
            GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
            hzp_down_ubos_.push_back(CreateGpuBuffer(d, params).raw());
        }
    }
    // 单张 R32Float mip 链纹理：生成趟写 mip0 storage + 逐级下采样写 mip[k] storage + 读 mip[k-1] 采样
    // + copy 各级 mip 源。usage 覆盖 storage 写 / 采样读 / copy 源。
    if (!hzp_tex_) {
        hzp_tex_ = CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kHzpBaseDim, kHzpBaseDim, 1,
            WGPUTextureFormat_R32Float,
            WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc,
            kHzpMips, 1, {nullptr}, TextureSamplerDesc::FromLinearFlag(false));
    }
    const TextureEntry* pte = FindTexture(hzp_tex_);
    if (!hzp_gen_shader_ || !hzp_down_shader_ || !hzp_gen_ubo_ ||
        hzp_down_ubos_.size() != kHzpMips - 1 || !hzp_tex_ || !pte || !pte->texture) {
        DEBUG_LOG_ERROR("WebGPU[B3b-6] Hi-Z 金字塔自检资源创建失败，跳过");
        return false;
    }

    // B3b-7：改走引擎 Hi-Z build 真实绑定面 SetComputeTextureImageMip（句柄 + mip 级 + read_only +
    // r32f），其内部对 (句柄,mip) 缓存单层单 mip 视图并经 SetComputeImageViewExplicit 路由，
    // 故此自检同时验证「句柄→单 mip 视图」整条通路（不再手建 hzp_mip_views_）。

    // ①生成趟：写 mip0（8×8）渐变。
    ResetDrawState();
    CmdBindUniformBuffer(0, hzp_gen_ubo_, 0, sizeof(uint32_t) * 4);
    SetComputeTextureImageMip(0, hzp_tex_, 0, /*read_only=*/false, /*r32f=*/true);
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(hzp_gen_shader_, (kHzpBaseDim + 7u) / 8u, (kHzpBaseDim + 7u) / 8u, 1);
    EndComputePass();
    ResetDrawState();

    // ②逐级下采样趟：读 mip[k-1] 采样视图 + 2×2 max 写 mip[k] storage 视图（独立 pass，pass 间屏障）。
    for (uint32_t k = 1; k < kHzpMips; ++k) {
        const uint32_t dst_dim = kHzpBaseDim >> k;
        CmdBindUniformBuffer(0, hzp_down_ubos_[k - 1], 0, sizeof(uint32_t) * 4);
        SetComputeTextureImageMip(0, hzp_tex_, static_cast<int>(k - 1), /*read_only=*/true,  /*r32f=*/true);
        SetComputeTextureImageMip(1, hzp_tex_, static_cast<int>(k),     /*read_only=*/false, /*r32f=*/true);
        BeginComputePass();
        if (!cur_compute_pass_) { ResetDrawState(); return false; }
        DispatchCompute(hzp_down_shader_, (dst_dim + 7u) / 8u, (dst_dim + 7u) / 8u, 1);
        EndComputePass();
        ResetDrawState();
    }

    // copy 各级 mip → 回读缓冲（256 对齐分段；随帧提交）。
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kHzpTotalBytes;
    hzp_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
        wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    }
    return true;
}

void WebGPURhiDevice::KickHiZPyramidSelfTestReadback() {
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
bool WebGPURhiDevice::RecordComputeBindSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!cb_shader_) cb_shader_ = CreateComputeShaderEx("", "", "", 1, 0, 1, 0, kBindSelfTestWGSL);
    if (!cb_tex_) {
        // 已知渐变 rgba8unorm：texel(x,y)=(x*40, y*60, (x+y)*20, 255)。
        std::vector<uint8_t> texdata(kCbTexDim * kCbTexDim * 4u);
        for (uint32_t y = 0; y < kCbTexDim; ++y) {
            for (uint32_t x = 0; x < kCbTexDim; ++x) {
                uint8_t* p = &texdata[(y * kCbTexDim + x) * 4u];
                p[0] = CbTexR(x); p[1] = CbTexG(y); p[2] = CbTexB(x, y); p[3] = 255;
            }
        }
        cb_tex_ = CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kCbTexDim, kCbTexDim, 1,
            WGPUTextureFormat_RGBA8Unorm,
            WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            1, 1, {texdata.data()}, TextureSamplerDesc::FromLinearFlag(false));
    }
    if (!cb_out_) {
        GpuBufferDesc d; d.size = kCbOutBytes; d.usage = GpuBufferUsage::kStorage;
        cb_out_ = CreateGpuBuffer(d, nullptr).raw();
    }
    const TextureEntry* cte = FindTexture(cb_tex_);
    if (!cb_shader_ || !cb_tex_ || !cte || !cte->texture || !cb_out_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-8] 命名 uniform/采样自检资源创建失败，跳过");
        return false;
    }

    // 经引擎真实 compute API 面绑定：命名 uniform（调用序定位）+ 句柄采样器 + 结果 SSBO。
    const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    ResetDrawState();
    SetComputeUniformInt(cb_shader_, "a_int", kCbAInt);
    SetComputeUniformFloat(cb_shader_, "b_float", kCbBFloat);
    SetComputeUniformVec2i(cb_shader_, "c_coord", kCbCX, kCbCY);
    SetComputeUniformVec4(cb_shader_, "d_vec4", 1.0f, 2.0f, 3.0f, 4.0f);
    SetComputeUniformMat4(cb_shader_, "e_mat", kIdentity);
    SetComputeTextureSampler(0, cb_tex_);
    CmdBindStorageBuffer(0, cb_out_, 0, kCbOutBytes);

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(cb_shader_, 1, 1, 1);
    EndComputePass();
    ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kCbOutBytes;
    cb_rb_out_ = wgpuDeviceCreateBuffer(device_, &bd);
    const BufferEntry* be_out = FindBuffer(cb_out_);
    if (!cb_rb_out_ || !be_out || !be_out->buffer) {
        if (cb_rb_out_) { wgpuBufferRelease(cb_rb_out_); cb_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_out->buffer, 0, cb_rb_out_, 0, kCbOutBytes);
    return true;
}

void WebGPURhiDevice::KickComputeBindSelfTestReadback() {
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
bool WebGPURhiDevice::RecordHiZCullSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!hc_shader_) hc_shader_ = CreateComputeShaderEx("", "", "", 2, 0, 1, 80, kHiZCullWGSL);
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
        hc_aabb_ = CreateGpuBuffer(d, aabbs).raw();
    }
    if (!hc_vis_) {
        GpuBufferDesc d; d.size = kHcVisBytes; d.usage = GpuBufferUsage::kStorage;
        hc_vis_ = CreateGpuBuffer(d, nullptr).raw();
    }
    if (!hc_hiz_tex_) {
        hc_hiz_tex_ = CreateTextureImpl(
            WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, kHcHizDim, kHcHizDim, 1,
            WGPUTextureFormat_R32Float,
            WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
            1, 1, {nullptr}, TextureSamplerDesc::FromLinearFlag(false));
        const TextureEntry* hte = FindTexture(hc_hiz_tex_);
        if (hte && hte->texture) {
            // 恒值 0.5 填充（queue write 无 256 行对齐约束）。
            std::vector<float> hiz(kHcHizDim * kHcHizDim, kHcHizDepth);
            WGPUImageCopyTexture dst{};
            dst.texture = hte->texture; dst.mipLevel = 0; dst.aspect = WGPUTextureAspect_All;
            WGPUTextureDataLayout layout{};
            layout.offset = 0; layout.bytesPerRow = kHcHizDim * 4u; layout.rowsPerImage = kHcHizDim;
            WGPUExtent3D ext{kHcHizDim, kHcHizDim, 1};
            wgpuQueueWriteTexture(queue_, &dst, hiz.data(), hiz.size() * 4u, &layout, &ext);
        }
    }
    const TextureEntry* hte = FindTexture(hc_hiz_tex_);
    if (!hc_shader_ || !hc_aabb_ || !hc_vis_ || !hc_hiz_tex_ || !hte || !hte->texture) {
        DEBUG_LOG_ERROR("WebGPU[B3b-9] Hi-Z 剔除自检资源创建失败，跳过");
        return false;
    }

    // 经引擎 HiZCullPass 真实绑定面：双 SSBO（slot0 AABB / slot1 可见性）+ 句柄采样 Hi-Z + 命名 uniform。
    const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    ResetDrawState();
    CmdBindStorageBuffer(0, hc_aabb_, 0, kHcObjCount * 8u * 4u);
    CmdBindStorageBuffer(1, hc_vis_, 0, kHcVisBytes);
    SetComputeTextureSampler(0, hc_hiz_tex_);
    SetComputeUniformMat4(hc_shader_, "u_view_projection", kIdentity);
    SetComputeUniformVec2f(hc_shader_, "u_screen_size", 256.0f, 256.0f);
    SetComputeUniformInt(hc_shader_, "u_mip_count", 1);
    SetComputeUniformInt(hc_shader_, "u_object_count", static_cast<int>(kHcObjCount));

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(hc_shader_, (kHcObjCount + 63u) / 64u, 1, 1);
    EndComputePass();
    ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kHcVisBytes;
    hc_rb_out_ = wgpuDeviceCreateBuffer(device_, &bd);
    const BufferEntry* be_vis = FindBuffer(hc_vis_);
    if (!hc_rb_out_ || !be_vis || !be_vis->buffer) {
        if (hc_rb_out_) { wgpuBufferRelease(hc_rb_out_); hc_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_vis->buffer, 0, hc_rb_out_, 0, kHcVisBytes);
    return true;
}

void WebGPURhiDevice::KickHiZCullSelfTestReadback() {
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
bool WebGPURhiDevice::RecordGpuDrivenHiZCullSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!t44_gen_shader_)  t44_gen_shader_  = CreateComputeShaderEx("", "", "", 0, 1, 0, 0,  kT44GenWGSL);
    if (!t44_down_shader_) t44_down_shader_ = CreateComputeShaderEx("", "", "", 0, 1, 1, 0,  kT44DownWGSL);
    if (!t44_cull_shader_) t44_cull_shader_ = CreateComputeShaderEx("", "", "", 2, 0, 1, 80, kT44CullWGSL);
    if (!t44_gen_ubo_.raw()) {
        const uint32_t params[4] = {kT44HizDim, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
        t44_gen_ubo_ = CreateGpuBuffer(d, params);
    }
    if (t44_down_ubos_.empty()) {
        for (uint32_t k = 1; k < kT44Mips; ++k) {
            const uint32_t src_dim = kT44HizDim >> (k - 1);
            const uint32_t dst_dim = kT44HizDim >> k;
            const uint32_t params[4] = {src_dim, dst_dim, 0u, 0u};
            GpuBufferDesc d; d.size = sizeof(params); d.usage = GpuBufferUsage::kUniform;
            t44_down_ubos_.push_back(CreateGpuBuffer(d, params).raw());
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
        t44_aabb_ = CreateGpuBuffer(d, aabbs);
    }
    if (!t44_vis_.raw()) {
        GpuBufferDesc d; d.size = kT44VisBytes; d.usage = GpuBufferUsage::kStorage;
        t44_vis_ = CreateGpuBuffer(d, nullptr);
    }
    if (!t44_hiz_handle_) t44_hiz_handle_ = CreateHiZTexture(kT44HizDim, kT44HizDim);

    const unsigned int hiz_tex = GetHiZGpuTexture(t44_hiz_handle_);
    const int mip_count = GetHiZMipCount(t44_hiz_handle_);
    const TextureEntry* pte = FindTexture(hiz_tex);
    if (!t44_gen_shader_ || !t44_down_shader_ || !t44_cull_shader_ || !t44_gen_ubo_.raw() ||
        t44_down_ubos_.size() != kT44Mips - 1 || !t44_aabb_.raw() || !t44_vis_.raw() ||
        !t44_hiz_handle_ || !hiz_tex || !pte || !pte->texture ||
        mip_count != static_cast<int>(kT44Mips)) {
        DEBUG_LOG_ERROR("WebGPU[T4-4] Hi-Z 剔除自检资源创建失败，跳过（mip_count={}）", mip_count);
        return false;
    }

    // ①生成趟：写 mip0（8×8）占位遮挡深度。经引擎 HiZBuildPass 真实绑定面 SetComputeTextureImageMip。
    ResetDrawState();
    CmdBindUniformBuffer(0, t44_gen_ubo_.raw(), 0, sizeof(uint32_t) * 4);
    SetComputeTextureImageMip(0, hiz_tex, 0, /*read_only=*/false, /*r32f=*/true);
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(t44_gen_shader_, (kT44HizDim + 7u) / 8u, (kT44HizDim + 7u) / 8u, 1);
    EndComputePass();
    ResetDrawState();

    // ②逐级下采样趟：读 mip[k-1] + 2×2 max 写 mip[k]（独立 pass，pass 间屏障保证可见性）。
    for (uint32_t k = 1; k < kT44Mips; ++k) {
        const uint32_t dst_dim = kT44HizDim >> k;
        CmdBindUniformBuffer(0, t44_down_ubos_[k - 1], 0, sizeof(uint32_t) * 4);
        SetComputeTextureImageMip(0, hiz_tex, static_cast<int>(k - 1), /*read_only=*/true,  /*r32f=*/true);
        SetComputeTextureImageMip(1, hiz_tex, static_cast<int>(k),     /*read_only=*/false, /*r32f=*/true);
        BeginComputePass();
        if (!cur_compute_pass_) { ResetDrawState(); return false; }
        DispatchCompute(t44_down_shader_, (dst_dim + 7u) / 8u, (dst_dim + 7u) / 8u, 1);
        EndComputePass();
        ResetDrawState();
    }

    // ③剔除趟：经 HiZCullPass 真实绑定面——双 SSBO + 句柄采样金字塔（GetHiZGpuTexture）+ 命名 uniform。
    const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    ResetDrawState();
    CmdBindStorageBuffer(0, t44_aabb_.raw(), 0, kT44ObjCount * 8u * 4u);
    CmdBindStorageBuffer(1, t44_vis_.raw(), 0, kT44VisBytes);
    SetComputeTextureSampler(0, hiz_tex);
    SetComputeUniformMat4(t44_cull_shader_, "u_view_projection", kIdentity);
    SetComputeUniformVec2f(t44_cull_shader_, "u_screen_size", static_cast<float>(kT44HizDim),
                           static_cast<float>(kT44HizDim));
    SetComputeUniformInt(t44_cull_shader_, "u_mip_count", mip_count);
    SetComputeUniformInt(t44_cull_shader_, "u_object_count", static_cast<int>(kT44ObjCount));

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(t44_cull_shader_, (kT44ObjCount + 63u) / 64u, 1, 1);
    EndComputePass();
    ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kT44VisBytes;
    t44_rb_out_ = wgpuDeviceCreateBuffer(device_, &bd);
    const BufferEntry* be_vis = FindBuffer(t44_vis_.raw());
    if (!t44_rb_out_ || !be_vis || !be_vis->buffer) {
        if (t44_rb_out_) { wgpuBufferRelease(t44_rb_out_); t44_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_vis->buffer, 0, t44_rb_out_, 0, kT44VisBytes);
    return true;
}

void WebGPURhiDevice::KickGpuDrivenHiZCullSelfTestReadback() {
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
bool WebGPURhiDevice::RecordMorphSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!mf_shader_) mf_shader_ = CreateComputeShaderEx("", "", "", 4, 0, 0, 8, kMorphWGSL);
    if (!mf_base_) {
        // 4 个 BaseVertex（pos.xyzw / normal.xyzw / tangent.xyzw）：法线统一 (0,0,1)，切线 (1,0,0,1)。
        const float base[kMfVtxCount * 12] = {
            1,0,0,1,  0,0,1,0,  1,0,0,1,
            0,1,0,1,  0,0,1,0,  1,0,0,1,
            0,0,1,1,  0,0,1,0,  1,0,0,1,
            1,1,1,1,  0,0,1,0,  1,0,0,1,
        };
        GpuBufferDesc d; d.size = sizeof(base); d.usage = GpuBufferUsage::kStorage;
        mf_base_ = CreateGpuBuffer(d, base).raw();
    }
    if (!mf_delta_) {
        // delta 排布 [target][vertex]：目标0 各顶点 Δpos=(1,0,0)、目标1 各顶点 Δpos=(0,2,0)，Δnrm=0。
        const float deltas[kMfTgtCount * kMfVtxCount * 8] = {
            1,0,0,0, 0,0,0,0,  1,0,0,0, 0,0,0,0,  1,0,0,0, 0,0,0,0,  1,0,0,0, 0,0,0,0,
            0,2,0,0, 0,0,0,0,  0,2,0,0, 0,0,0,0,  0,2,0,0, 0,0,0,0,  0,2,0,0, 0,0,0,0,
        };
        GpuBufferDesc d; d.size = sizeof(deltas); d.usage = GpuBufferUsage::kStorage;
        mf_delta_ = CreateGpuBuffer(d, deltas).raw();
    }
    if (!mf_weight_) {
        const float weights[kMfTgtCount] = {0.5f, 1.0f};
        GpuBufferDesc d; d.size = sizeof(weights); d.usage = GpuBufferUsage::kStorage;
        mf_weight_ = CreateGpuBuffer(d, weights).raw();
    }
    if (!mf_out_) {
        GpuBufferDesc d; d.size = kMfOutBytes; d.usage = GpuBufferUsage::kStorage;
        mf_out_ = CreateGpuBuffer(d, nullptr).raw();
    }
    if (!mf_shader_ || !mf_base_ || !mf_delta_ || !mf_weight_ || !mf_out_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-10] morph 自检资源创建失败，跳过");
        return false;
    }

    // 经引擎 MorphTargetSystem 真实绑定面：4×SSBO（slot0..3）+ 命名 uniform（同消费方调用序/名）。
    // 用 BindGpuBuffer（消费方 Dispatch 实际所调 API）而非 CmdBindStorageBuffer，验证真消费方绑定通路。
    ResetDrawState();
    BindGpuBuffer(BufferHandle{mf_base_}, 0);
    BindGpuBuffer(BufferHandle{mf_delta_}, 1);
    BindGpuBuffer(BufferHandle{mf_weight_}, 2);
    BindGpuBuffer(BufferHandle{mf_out_}, 3, true);
    SetComputeUniformInt(mf_shader_, "_20.u_vertex_count", static_cast<int>(kMfVtxCount));
    SetComputeUniformInt(mf_shader_, "_20.u_target_count", static_cast<int>(kMfTgtCount));

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(mf_shader_, (kMfVtxCount + 255u) / 256u, 1, 1);
    EndComputePass();
    ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kMfOutBytes;
    mf_rb_out_ = wgpuDeviceCreateBuffer(device_, &bd);
    const BufferEntry* be_out = FindBuffer(mf_out_);
    if (!mf_rb_out_ || !be_out || !be_out->buffer) {
        if (mf_rb_out_) { wgpuBufferRelease(mf_rb_out_); mf_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_out->buffer, 0, mf_rb_out_, 0, kMfOutBytes);
    return true;
}

void WebGPURhiDevice::KickMorphSelfTestReadback() {
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
bool WebGPURhiDevice::RecordDDGISelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!dg_shader_) dg_shader_ = CreateComputeShaderEx("", "", "", 2, 2, 3, 224, kDDGIWGSL);
    if (!dg_probe_) {
        // 1 探针：位置 xy 对齐 RSM VPL（128/255），z=-2（探针在 VPL 背面 z 负向）、w=1 激活。
        const float vx = 128.0f / 255.0f;
        const float probe[4] = {vx, vx, -2.0f, 1.0f};
        GpuBufferDesc d; d.size = sizeof(probe); d.usage = GpuBufferUsage::kStorage;
        dg_probe_ = CreateGpuBuffer(d, probe).raw();
    }
    if (!dg_dbg_) {
        GpuBufferDesc d; d.size = kDgDbgBytes; d.usage = GpuBufferUsage::kStorage;
        dg_dbg_ = CreateGpuBuffer(d, nullptr).raw();
    }
    if (!dg_irr_tex_) dg_irr_tex_ = CreateComputeWriteTexture2D(static_cast<int>(kDgIrrTexels),
                                                                static_cast<int>(kDgIrrTexels));
    if (!dg_vis_tex_) dg_vis_tex_ = CreateComputeWriteTexture2D(static_cast<int>(kDgVisTexels),
                                                                static_cast<int>(kDgVisTexels));
    auto make_rsm = [&](uint8_t r, uint8_t g, uint8_t b) -> unsigned int {
        std::vector<uint8_t> px(kDgRsmDim * kDgRsmDim * 4u);
        for (uint32_t i = 0; i < kDgRsmDim * kDgRsmDim; ++i) {
            px[i * 4 + 0] = r; px[i * 4 + 1] = g; px[i * 4 + 2] = b; px[i * 4 + 3] = 255;
        }
        return CreateTextureImpl(
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
    ResetDrawState();
    SetComputeTextureImage(0, dg_irr_tex_, /*read_only=*/false);
    SetComputeTextureImage(1, dg_vis_tex_, /*read_only=*/false);
    SetComputeTextureSampler(2, dg_rsm_pos_);
    SetComputeTextureSampler(3, dg_rsm_nrm_);
    SetComputeTextureSampler(4, dg_rsm_flux_);
    CmdBindStorageBuffer(0, dg_probe_, 0, 16u);
    CmdBindStorageBuffer(1, dg_dbg_, 0, kDgDbgBytes);
    SetComputeUniformInt(dg_shader_, "u_probe_count", 1);
    SetComputeUniformInt(dg_shader_, "u_probe_start", 0);
    SetComputeUniformInt(dg_shader_, "u_probes_to_update", 1);
    SetComputeUniformInt(dg_shader_, "u_irradiance_texels", static_cast<int>(kDgIrrTexels));
    SetComputeUniformInt(dg_shader_, "u_visibility_texels", static_cast<int>(kDgVisTexels));
    SetComputeUniformInt(dg_shader_, "u_rsm_width", static_cast<int>(kDgRsmDim));
    SetComputeUniformInt(dg_shader_, "u_rsm_height", static_cast<int>(kDgRsmDim));
    SetComputeUniformInt(dg_shader_, "u_frame_index", 0);
    SetComputeUniformFloat(dg_shader_, "u_hysteresis", 0.0f);
    SetComputeUniformIVec3(dg_shader_, "u_grid_resolution", 1, 1, 1);
    SetComputeUniformVec3(dg_shader_, "u_grid_origin", 0.0f, 0.0f, 0.0f);
    SetComputeUniformVec3(dg_shader_, "u_grid_spacing", 1.0f, 1.0f, 1.0f);
    SetComputeUniformVec3(dg_shader_, "u_light_dir", 0.0f, 0.0f, 1.0f);
    SetComputeUniformVec3(dg_shader_, "u_light_color", 1.0f, 1.0f, 1.0f);

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(dg_shader_, (kDgIrrTexels + 7u) / 8u, 1, 1);
    EndComputePass();
    ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kDgDbgBytes;
    dg_rb_out_ = wgpuDeviceCreateBuffer(device_, &bd);
    const BufferEntry* be_dbg = FindBuffer(dg_dbg_);
    if (!dg_rb_out_ || !be_dbg || !be_dbg->buffer) {
        if (dg_rb_out_) { wgpuBufferRelease(dg_rb_out_); dg_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_dbg->buffer, 0, dg_rb_out_, 0, kDgDbgBytes);
    return true;
}

void WebGPURhiDevice::KickDDGISelfTestReadback() {
    if (!dg_rb_out_) return;
    auto* ctx = new DDGISelfTestCtx();
    ctx->rb_out = dg_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    dg_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kDgDbgBytes, OnDDGIMapped, ctx);
}

bool WebGPURhiDevice::RecordHairSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    // 4 个着色器直接取引擎真源（hair_compute_shaders.h::kHair*SourceWGSL），与消费方
    // hair_system.cpp::InitComputeShaders 同 push_constant_bytes（48/4/12/12）。
    if (!hr_shader_)  hr_shader_  = CreateComputeShaderEx("", "", "", 4, 0, 0, 48, render::kHairIntegrateSourceWGSL);
    if (!hr_local_)   hr_local_   = CreateComputeShaderEx("", "", "", 3, 0, 0, 12, render::kHairLocalShapeSourceWGSL);
    if (!hr_length_)  hr_length_  = CreateComputeShaderEx("", "", "", 3, 0, 0, 4,  render::kHairLengthConstraintSourceWGSL);
    if (!hr_tangent_) hr_tangent_ = CreateComputeShaderEx("", "", "", 3, 0, 0, 12, render::kHairUpdateTangentSourceWGSL);

    if (!hr_cur_) {
        GpuBufferDesc d; d.size = sizeof(kHrInitCur); d.usage = GpuBufferUsage::kStorage;
        hr_cur_ = CreateGpuBuffer(d, kHrInitCur).raw();
    }
    if (!hr_prev_) {
        GpuBufferDesc d; d.size = sizeof(kHrInitPrev); d.usage = GpuBufferUsage::kStorage;
        hr_prev_ = CreateGpuBuffer(d, kHrInitPrev).raw();
    }
    if (!hr_rest_) {
        GpuBufferDesc d; d.size = sizeof(kHrInitRest); d.usage = GpuBufferUsage::kStorage;
        hr_rest_ = CreateGpuBuffer(d, kHrInitRest).raw();
    }
    if (!hr_tan_) {
        GpuBufferDesc d; d.size = sizeof(kHrInitTan); d.usage = GpuBufferUsage::kStorage;
        hr_tan_ = CreateGpuBuffer(d, kHrInitTan).raw();
    }
    if (!hr_strand_) {
        const uint32_t si[2] = {0u, kHrVerts};  // strand 0：offset=0, count=4
        GpuBufferDesc d; d.size = sizeof(si); d.usage = GpuBufferUsage::kStorage;
        hr_strand_ = CreateGpuBuffer(d, si).raw();
    }

    if (!hr_shader_ || !hr_local_ || !hr_length_ || !hr_tangent_ ||
        !hr_cur_ || !hr_prev_ || !hr_rest_ || !hr_tan_ || !hr_strand_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-12] hair 自检资源创建失败，跳过");
        return false;
    }

    // 全 4 趟同序跑在同一 compute pass 内（趟间 WebGPU 自动按 storage 资源依赖串行化），
    // 与 hair_system.cpp::SimulateCompute 的绑定/uniform/dispatch 顺序逐一对齐。
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    const unsigned int groups_v = (kHrVerts + 63u) / 64u;
    const unsigned int groups_s = (kHrStrands + 63u) / 64u;

    // ① integrate：b0=cur b1=prev b2=rest b3=strand + 12 uniform
    ResetDrawState();
    CmdBindStorageBuffer(0, hr_cur_,  0, kHrPosBytes);
    CmdBindStorageBuffer(1, hr_prev_, 0, kHrPosBytes);
    CmdBindStorageBuffer(2, hr_rest_, 0, kHrPosBytes);
    CmdBindStorageBuffer(3, hr_strand_, 0, 8u);
    SetComputeUniformInt(hr_shader_,   "u_num_vertices", static_cast<int>(kHrVerts));
    SetComputeUniformFloat(hr_shader_, "u_dt",           kHrDt);
    SetComputeUniformFloat(hr_shader_, "u_damping",      kHrDamping);
    SetComputeUniformFloat(hr_shader_, "u_gx",           0.0f);
    SetComputeUniformFloat(hr_shader_, "u_gy",          -1.0f);
    SetComputeUniformFloat(hr_shader_, "u_gz",           0.0f);
    SetComputeUniformFloat(hr_shader_, "u_gw",           kHrGw);
    SetComputeUniformFloat(hr_shader_, "u_wx",           0.0f);
    SetComputeUniformFloat(hr_shader_, "u_wy",           0.0f);
    SetComputeUniformFloat(hr_shader_, "u_wz",           0.0f);
    SetComputeUniformFloat(hr_shader_, "u_ww",           0.0f);
    SetComputeUniformFloat(hr_shader_, "u_time",         0.0f);
    DispatchCompute(hr_shader_, groups_v, 1, 1);
    ComputeMemoryBarrier();

    // ② local_shape：b0=cur b1=rest b2=strand + 3 uniform
    ResetDrawState();
    CmdBindStorageBuffer(0, hr_cur_,  0, kHrPosBytes);
    CmdBindStorageBuffer(1, hr_rest_, 0, kHrPosBytes);
    CmdBindStorageBuffer(2, hr_strand_, 0, 8u);
    SetComputeUniformInt(hr_local_,   "u_num_strands",      static_cast<int>(kHrStrands));
    SetComputeUniformFloat(hr_local_, "u_stiffness_local",  kHrStLocal);
    SetComputeUniformFloat(hr_local_, "u_stiffness_global", kHrStGlobal);
    DispatchCompute(hr_local_, groups_s, 1, 1);
    ComputeMemoryBarrier();

    // ③ length：b0=cur b1=rest b2=strand + 1 uniform
    ResetDrawState();
    CmdBindStorageBuffer(0, hr_cur_,  0, kHrPosBytes);
    CmdBindStorageBuffer(1, hr_rest_, 0, kHrPosBytes);
    CmdBindStorageBuffer(2, hr_strand_, 0, 8u);
    SetComputeUniformInt(hr_length_, "u_num_strands", static_cast<int>(kHrStrands));
    DispatchCompute(hr_length_, groups_s, 1, 1);
    ComputeMemoryBarrier();

    // ④ tangent：b0=cur b1=tangent b2=strand + 3 uniform
    ResetDrawState();
    CmdBindStorageBuffer(0, hr_cur_, 0, kHrPosBytes);
    CmdBindStorageBuffer(1, hr_tan_, 0, kHrPosBytes);
    CmdBindStorageBuffer(2, hr_strand_, 0, 8u);
    SetComputeUniformInt(hr_tangent_, "u_num_vertices",     static_cast<int>(kHrVerts));
    SetComputeUniformInt(hr_tangent_, "u_num_strands",      static_cast<int>(kHrStrands));
    SetComputeUniformInt(hr_tangent_, "u_verts_per_strand", kHrVertsPerStr);
    DispatchCompute(hr_tangent_, groups_v, 1, 1);

    EndComputePass();
    ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kHrRbBytes;
    hr_rb_out_ = wgpuDeviceCreateBuffer(device_, &bd);
    const BufferEntry* be_cur = FindBuffer(hr_cur_);
    const BufferEntry* be_tan = FindBuffer(hr_tan_);
    if (!hr_rb_out_ || !be_cur || !be_cur->buffer || !be_tan || !be_tan->buffer) {
        if (hr_rb_out_) { wgpuBufferRelease(hr_rb_out_); hr_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_cur->buffer, 0, hr_rb_out_, 0,           kHrPosBytes);
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_tan->buffer, 0, hr_rb_out_, kHrPosBytes, kHrPosBytes);
    return true;
}

void WebGPURhiDevice::KickHairSelfTestReadback() {
    if (!hr_rb_out_) return;
    auto* ctx = new HairSelfTestCtx();
    ctx->rb_out = hr_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    hr_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kHrRbBytes, OnHairMapped, ctx);
}

// B3b-14 grass 风场 compute 正确性自检：内联镜像 grass_system.cpp 风场 WGSL（engine 层不能 include
//   modules 层头）→ 造已知实例 → BeginComputePass/DispatchCompute/EndComputePass → copy 输出 mat4 回读校验。
bool WebGPURhiDevice::RecordGrassSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!gr_shader_) gr_shader_ = CreateComputeShaderEx("", "", "", 2, 0, 0, 28, kGrassWGSL);
    if (!gr_in_) {
        GpuBufferDesc d; d.size = kGrInBytes; d.usage = GpuBufferUsage::kStorage;
        gr_in_ = CreateGpuBuffer(d, kGrInst).raw();
    }
    if (!gr_out_) {
        GpuBufferDesc d; d.size = kGrOutBytes; d.usage = GpuBufferUsage::kStorage;
        gr_out_ = CreateGpuBuffer(d, nullptr).raw();
    }
    if (!gr_shader_ || !gr_in_ || !gr_out_) {
        DEBUG_LOG_ERROR("WebGPU[B3b-14] grass 自检资源创建失败，跳过");
        return false;
    }

    ResetDrawState();
    CmdBindStorageBuffer(0, gr_in_,  0, kGrInBytes);
    CmdBindStorageBuffer(1, gr_out_, 0, kGrOutBytes);
    SetComputeUniformVec2f(gr_shader_, "u_wind_dir",        kGrWindDirX, kGrWindDirY);
    SetComputeUniformFloat(gr_shader_, "u_wind_speed",      kGrWindSpeed);
    SetComputeUniformFloat(gr_shader_, "u_wind_strength",   kGrWindStrength);
    SetComputeUniformFloat(gr_shader_, "u_wind_turbulence", kGrWindTurb);
    SetComputeUniformFloat(gr_shader_, "u_time",            kGrTime);
    SetComputeUniformInt(gr_shader_,   "u_instance_count",  static_cast<int>(kGrInstances));

    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(gr_shader_, (kGrInstances + 63u) / 64u, 1, 1);
    EndComputePass();
    ResetDrawState();

    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kGrOutBytes;
    gr_rb_out_ = wgpuDeviceCreateBuffer(device_, &bd);
    const BufferEntry* be_out = FindBuffer(gr_out_);
    if (!gr_rb_out_ || !be_out || !be_out->buffer) {
        if (gr_rb_out_) { wgpuBufferRelease(gr_rb_out_); gr_rb_out_ = nullptr; }
        return false;
    }
    wgpuCommandEncoderCopyBufferToBuffer(frame_encoder_, be_out->buffer, 0, gr_rb_out_, 0, kGrOutBytes);
    return true;
}

void WebGPURhiDevice::KickGrassSelfTestReadback() {
    if (!gr_rb_out_) return;
    auto* ctx = new GrassSelfTestCtx();
    ctx->rb_out = gr_rb_out_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    gr_rb_out_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_out, WGPUMapMode_Read, 0, kGrOutBytes, OnGrassMapped, ctx);
}

// B3b-13 bloom 双滤波 compute 真链路自检：手译 bloom_downsample.comp / bloom_upsample.comp 核心为 WGSL，
//   经 gen compute 造已知 rgba16f 渐变 → 下采样 13-tap → 上采样 3×3 tent + base 累加 → copy 回读校验。
bool WebGPURhiDevice::RecordBloomSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

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

    if (!bl_gen_shader_)  bl_gen_shader_  = CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kBloomGenWGSL);
    if (!bl_down_shader_) bl_down_shader_ = CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kBloomDownWGSL);
    if (!bl_up_shader_)   bl_up_shader_   = CreateComputeShaderEx("", "", "", 0, 1, 0, 0, kBloomUpWGSL);

    auto make_tex = [&](uint32_t dim, WGPUTextureUsageFlags usage) -> unsigned int {
        return CreateTextureImpl(WGPUTextureDimension_2D, WGPUTextureViewDimension_2D, dim, dim, 1,
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
        ResetDrawState();
        SetComputeUniformInt(bl_gen_shader_, "u_dim",  static_cast<int>(dim));
        SetComputeUniformInt(bl_gen_shader_, "u_kind", kind);
        SetComputeTextureImage(0, tex, /*read_only=*/false);
        BeginComputePass();
        if (!cur_compute_pass_) { ResetDrawState(); return false; }
        DispatchCompute(bl_gen_shader_, (dim + 7u) / 8u, (dim + 7u) / 8u, 1);
        EndComputePass();
        ResetDrawState();
        return true;
    };
    // ① 三趟生成（src8 / usrc4 / ubase4），各独立 compute pass（pass 间自动屏障）。
    if (!gen(bl_src8_,   kBlSrcDim, 0)) return false;
    if (!gen(bl_usrc4_,  kBlUpDim,  1)) return false;
    if (!gen(bl_ubase4_, kBlUpDim,  2)) return false;

    // ② 下采样 src8（8×8）→ down4（4×4）。
    ResetDrawState();
    SetComputeUniformInt(bl_down_shader_, "u_src_dim", static_cast<int>(kBlSrcDim));
    SetComputeUniformInt(bl_down_shader_, "u_dst_dim", static_cast<int>(kBlDownDim));
    SetComputeTextureImage(0, bl_src8_,  /*read_only=*/true);
    SetComputeTextureImage(1, bl_down4_, /*read_only=*/false);
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(bl_down_shader_, (kBlDownDim + 7u) / 8u, (kBlDownDim + 7u) / 8u, 1);
    EndComputePass();
    ResetDrawState();

    // ③ 上采样 usrc4（4×4）+ ubase4 按 blend 累加 → up4（4×4）。
    SetComputeUniformInt(bl_up_shader_,   "u_dim",   static_cast<int>(kBlUpDim));
    SetComputeUniformFloat(bl_up_shader_, "u_blend", kBlBlend);
    SetComputeTextureImage(0, bl_usrc4_,  /*read_only=*/true);
    SetComputeTextureImage(1, bl_ubase4_, /*read_only=*/true);
    SetComputeTextureImage(2, bl_up4_,    /*read_only=*/false);
    BeginComputePass();
    if (!cur_compute_pass_) { ResetDrawState(); return false; }
    DispatchCompute(bl_up_shader_, (kBlUpDim + 7u) / 8u, (kBlUpDim + 7u) / 8u, 1);
    EndComputePass();
    ResetDrawState();

    // copy down4 + up4 storage 纹理 → 回读缓冲（各占 256 对齐分段）。
    const TextureEntry* te_down = FindTexture(bl_down4_);
    const TextureEntry* te_up   = FindTexture(bl_up4_);
    if (!te_down || !te_down->texture || !te_up || !te_up->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = kBlTotalBytes;
    bl_rb_out_ = wgpuDeviceCreateBuffer(device_, &bd);
    if (!bl_rb_out_) return false;
    auto copy_tex = [&](const TextureEntry* te, uint32_t dim, uint32_t off) {
        WGPUImageCopyTexture src{};
        src.texture = te->texture; src.mipLevel = 0; src.aspect = WGPUTextureAspect_All;
        WGPUImageCopyBuffer dst{};
        dst.buffer = bl_rb_out_;
        dst.layout.offset = off;
        dst.layout.bytesPerRow = kBlRowBytes;
        dst.layout.rowsPerImage = dim;
        WGPUExtent3D ext{dim, dim, 1};
        wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    };
    copy_tex(te_down, kBlDownDim, kBlDownOff);
    copy_tex(te_up,   kBlUpDim,   kBlUpOff);
    return true;
}

void WebGPURhiDevice::KickBloomSelfTestReadback() {
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
bool WebGPURhiDevice::RecordMultiDrawIndirectSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    // 离屏 RT（引擎 CreateRenderTarget：RGBA16Float 颜色 + CopySrc，无深度）。
    if (!t41_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT41RtSize);
        d.height = static_cast<int>(kT41RtSize);
        d.has_color = true;
        d.has_depth = false;
        t41_rt_ = CreateRenderTarget(d);
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
        t41_program_ = CreateShaderProgram(kWGSL, "");
    }
    if (!t41_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t41_pso_ = CreatePipelineState(d);
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
        t41_vbo_ = CreateBuffer(sizeof(verts), verts, false, false);
    }
    if (!t41_ibo_) {
        uint32_t idx[kT41Instances * 6];
        for (uint32_t i = 0; i < kT41Instances; ++i) {
            const uint32_t b = i * 4;
            idx[i*6+0]=b+0; idx[i*6+1]=b+1; idx[i*6+2]=b+2;
            idx[i*6+3]=b+0; idx[i*6+4]=b+2; idx[i*6+5]=b+3;
        }
        t41_ibo_ = CreateBuffer(sizeof(idx), idx, false, true);
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
        t41_indirect_ = CreateGpuBuffer(d, cmds).raw();
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
    CmdBeginRenderPass(rp);
    if (!cur_pass_) return false;
    CmdSetViewport(0, 0, static_cast<int>(kT41RtSize), static_cast<int>(kT41RtSize));
    const unsigned int pipe = GetGraphicsPipeline(t41_pso_, t41_program_);
    CmdBindPipeline(pipe);
    const std::vector<VertexAttr> attrs = {
        VertexAttr{0, 2, 0},  // pos.xy
        VertexAttr{1, 3, 8},  // color.rgb
    };
    CmdBindVertexBuffer(0, t41_vbo_, 20, attrs, VertexInputRate::PerVertex);
    CmdBindIndexBuffer(t41_ibo_, IndexType::UInt32);
    MultiDrawIndexedIndirect(t41_indirect_, static_cast<int>(kT41Instances),
                             kT41DrawWords * sizeof(uint32_t), 0);
    CmdEndRenderPass();

    // copy 离屏 RT 颜色纹理 → 回读缓冲（随帧提交）。
    const RenderTargetEntry* rt = FindRenderTarget(t41_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const TextureEntry* color = FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT41RtBytes);
    t41_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickMultiDrawIndirectSelfTestReadback() {
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
bool WebGPURhiDevice::RecordMegaVaoSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    if (!t42_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT42RtSize);
        d.height = static_cast<int>(kT42RtSize);
        d.has_color = true;
        d.has_depth = false;
        t42_rt_ = CreateRenderTarget(d);
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
        t42_program_ = CreateShaderProgram(kWGSL, "");
    }
    if (!t42_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t42_pso_ = CreatePipelineState(d);
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
        t42_vao_ = CreateMegaVAO(vbytes, ibytes, t42_vbo_, t42_ibo_);
        UpdateMegaVBO(t42_vbo_, 0, vbytes, verts.data());
        UpdateMegaIBO(t42_ibo_, 0, ibytes, idx.data());
    }
    if (!t42_rt_ || !t42_program_ || !t42_pso_ || !t42_vao_ || !t42_vbo_ || !t42_ibo_) {
        DEBUG_LOG_ERROR("WebGPU[T4-2] Mega VAO 自检资源创建失败，跳过");
        return false;
    }

    RenderPassDesc rp;
    rp.render_target = t42_rt_;
    rp.clear_color_enabled = true;
    rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    CmdBeginRenderPass(rp);
    if (!cur_pass_) return false;
    CmdSetViewport(0, 0, static_cast<int>(kT42RtSize), static_cast<int>(kT42RtSize));
    const unsigned int pipe = GetGraphicsPipeline(t42_pso_, t42_program_);
    CmdBindPipeline(pipe);
    BindMegaVAO(t42_vao_);  // 被测：据记录的 VBO/IBO 设 BatchVertex 92B 引擎 draw state。
    CmdDrawIndexed(kT42Quads * 6, 0, 0);
    CmdEndRenderPass();

    const RenderTargetEntry* rt = FindRenderTarget(t42_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const TextureEntry* color = FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT42RtBytes);
    t42_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickMegaVaoSelfTestReadback() {
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
bool WebGPURhiDevice::RecordGpuDrivenPBRSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;
    if (!EnsureGpuDrivenPBRShader()) {
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
        t43_rt_ = CreateRenderTarget(d);
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
        t43_vao_ = CreateMegaVAO(vbytes, ibytes, t43_vbo_, t43_ibo_);
        UpdateMegaVBO(t43_vbo_, 0, vbytes, verts.data());
        UpdateMegaIBO(t43_ibo_, 0, ibytes, idx);
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
        t43_inst_ssbo_ = CreateGpuBuffer(d, inst);
    }
    // 材质 SSBO（b9）：material0 红 albedo、material1 绿 albedo（metallic=0、roughness=0.9、ao=1）。
    if (!t43_mat_ssbo_) {
        GPUMaterialData mat[2]{};
        mat[0].albedo = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        mat[0].roughness_ao = glm::vec4(0.9f, 1.0f, 1.0f, 0.0f);
        mat[1].albedo = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        mat[1].roughness_ao = glm::vec4(0.9f, 1.0f, 1.0f, 0.0f);
        GpuBufferDesc d; d.size = sizeof(mat); d.usage = GpuBufferUsage::kStorage;
        t43_mat_ssbo_ = CreateGpuBuffer(d, mat);
    }
    // indirect：单条 [indexCount=6, instanceCount=2, firstIndex=0, baseVertex=0, firstInstance=0]，
    // instance_index 取 0/1 → 两实例分别读 instances[0]/[1] 的 model 与材质。
    if (!t43_indirect_) {
        const uint32_t cmd[5] = {6u, 2u, 0u, 0u, 0u};
        GpuBufferDesc d; d.size = sizeof(cmd);
        d.usage = GpuBufferUsage::kIndirect | GpuBufferUsage::kStorage;
        t43_indirect_ = CreateGpuBuffer(d, cmd);
    }
    // 白 albedo 纹理（验证纹理采样链路；base = material.albedo × white = material.albedo）。
    if (!t43_albedo_tex_) {
        const unsigned char white[4] = {255, 255, 255, 255};
        t43_albedo_tex_ = CreateTexture2D(1, 1, white, /*linear_filter=*/true);
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
    CmdBeginRenderPass(rp);
    if (!cur_pass_) return false;
    CmdSetViewport(0, 0, static_cast<int>(kT43RtSize), static_cast<int>(kT43RtSize));

    const glm::mat4 ident(1.0f);
    SetupGPUDrivenPBRShader(/*view=*/ident, /*proj=*/ident,
                            /*camera_pos=*/glm::vec3(0.0f, 0.0f, 1.0f),
                            /*light_dir=*/glm::vec3(0.0f, 0.0f, -1.0f),
                            /*light_color=*/glm::vec3(1.0f, 1.0f, 1.0f),
                            /*light_intensity=*/1.0f, /*ambient_intensity=*/0.3f,
                            /*shadow_strength=*/0.0f);
    BindGpuBuffer(t43_inst_ssbo_, gpu_driven::kSSBOBindingInstances);  // b5
    BindGpuBuffer(t43_mat_ssbo_,  gpu_driven::kSSBOBindingMaterials);  // b9
    BindMegaVAO(t43_vao_);
    BindGPUDrivenTextures(t43_albedo_tex_, 0, 0, 0, 0);
    MultiDrawIndexedIndirect(t43_indirect_.raw(), 1, 5 * sizeof(uint32_t), 0);
    CmdEndRenderPass();
    UnbindVAO();

    const RenderTargetEntry* rt = FindRenderTarget(t43_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const TextureEntry* color = FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT43RtBytes);
    t43_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickGpuDrivenPBRSelfTestReadback() {
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
bool WebGPURhiDevice::RecordCSMShadowSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    // shadow atlas RT（颜色 RGBA16Float 占位 + Depth32 深度附件，深度可作 texture_depth_2d 采样）。
    if (!t51_shadow_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT51AtlasDim);
        d.height = static_cast<int>(kT51AtlasDim);
        d.has_color = true;
        d.has_depth = true;
        t51_shadow_rt_ = CreateRenderTarget(d);
    }
    // 离屏 color RT（RGBA16Float + CopySrc）。
    if (!t51_color_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT51RtSize);
        d.height = static_cast<int>(kT51RtSize);
        d.has_color = true;
        d.has_depth = false;
        t51_color_rt_ = CreateRenderTarget(d);
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
        t51_occ_program_ = CreateShaderProgram(kOccWGSL, "");
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
        t51_occ_pso_ = CreatePipelineState(d);
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
        t51_recv_program_ = CreateShaderProgram(kRecvWGSL, "");
    }
    if (!t51_recv_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t51_recv_pso_ = CreatePipelineState(d);
    }
    // 遮挡 quad：NDC 中心 [-0.5,0.5]²、z=0.3（pos.xyz，stride 12）。
    if (!t51_occ_vbo_) {
        const float occ[4 * 3] = {
            -0.5f, -0.5f, 0.3f,   0.5f, -0.5f, 0.3f,
             0.5f,  0.5f, 0.3f,  -0.5f,  0.5f, 0.3f,
        };
        t51_occ_vbo_ = CreateBuffer(sizeof(occ), occ, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t51_occ_ibo_ = CreateBuffer(sizeof(idx), idx, false, true);
    }
    // 全屏 quad：NDC [-1,1]²、uv [0,1]²（pos.xy + uv，stride 16）。
    if (!t51_recv_vbo_) {
        const float fsq[4 * 4] = {
            -1.0f, -1.0f, 0.0f, 0.0f,   1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,  -1.0f,  1.0f, 0.0f, 1.0f,
        };
        t51_recv_vbo_ = CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t51_recv_ibo_ = CreateBuffer(sizeof(idx), idx, false, true);
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
        CmdBeginRenderPass(rp);
        if (!cur_pass_) return false;
        CmdSetViewport(0, 0, static_cast<int>(kT51AtlasDim), static_cast<int>(kT51AtlasDim));
        CmdBindPipeline(GetGraphicsPipeline(t51_occ_pso_, t51_occ_program_));
        const std::vector<VertexAttr> occ_attrs = { VertexAttr{0, 3, 0} };  // pos.xyz
        CmdBindVertexBuffer(0, t51_occ_vbo_, 12, occ_attrs, VertexInputRate::PerVertex);
        CmdBindIndexBuffer(t51_occ_ibo_, IndexType::UInt32);
        CmdDrawIndexed(6, 0, 0);
        CmdEndRenderPass();
    }

    // ②前向采样趟：把 shadow atlas 深度纹理绑到 slot11，全屏 quad 经 textureLoad PCF 采样判阴影。
    const unsigned int atlas_depth = GetRenderTargetDepthTexture(t51_shadow_rt_);
    if (!atlas_depth) {
        DEBUG_LOG_ERROR("WebGPU[T5-1] 取 shadow atlas 深度纹理失败，跳过");
        return false;
    }
    {
        RenderPassDesc rp;
        rp.render_target = t51_color_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        CmdBeginRenderPass(rp);
        if (!cur_pass_) return false;
        CmdSetViewport(0, 0, static_cast<int>(kT51RtSize), static_cast<int>(kT51RtSize));
        CmdBindPipeline(GetGraphicsPipeline(t51_recv_pso_, t51_recv_program_));
        const std::vector<VertexAttr> recv_attrs = { VertexAttr{0, 2, 0}, VertexAttr{1, 2, 8} };  // pos.xy + uv
        CmdBindVertexBuffer(0, t51_recv_vbo_, 16, recv_attrs, VertexInputRate::PerVertex);
        CmdBindIndexBuffer(t51_recv_ibo_, IndexType::UInt32);
        CmdBindTexture(11u, atlas_depth, TextureDim::Tex2D);  // → group2 binding22（texture_depth_2d）
        CmdDrawIndexed(6, 0, 0);
        CmdEndRenderPass();
    }

    // copy color RT → 回读缓冲（随帧提交）。
    const RenderTargetEntry* rt = FindRenderTarget(t51_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const TextureEntry* color = FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT51RtBytes);
    t51_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickCSMShadowSelfTestReadback() {
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
bool WebGPURhiDevice::RecordDeferredSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    // gbuffer RT（3 个 RGBA16Float 颜色附件 albedo/normal/position，无深度；附件均 TextureBinding 可采样）。
    if (!t52_gbuffer_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT52RtSize);
        d.height = static_cast<int>(kT52RtSize);
        d.has_color = true;
        d.has_depth = false;
        d.color_attachment_count = 3;
        t52_gbuffer_rt_ = CreateRenderTarget(d);
    }
    // 离屏 color RT（RGBA16Float + CopySrc）。
    if (!t52_color_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT52RtSize);
        d.height = static_cast<int>(kT52RtSize);
        d.has_color = true;
        d.has_depth = false;
        t52_color_rt_ = CreateRenderTarget(d);
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
        t52_geom_program_ = CreateShaderProgram(kGeomWGSL, "");
    }
    if (!t52_geom_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t52_geom_pso_ = CreatePipelineState(d);
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
        t52_light_program_ = CreateShaderProgram(kLightWGSL, "");
    }
    if (!t52_light_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t52_light_pso_ = CreatePipelineState(d);
    }
    // 几何 quad：NDC 中心 [-0.6,0.6]²（pos.xy，stride 8）。
    if (!t52_geom_vbo_) {
        const float geo[4 * 2] = {
            -0.6f, -0.6f,   0.6f, -0.6f,
             0.6f,  0.6f,  -0.6f,  0.6f,
        };
        t52_geom_vbo_ = CreateBuffer(sizeof(geo), geo, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t52_geom_ibo_ = CreateBuffer(sizeof(idx), idx, false, true);
    }
    // 全屏 quad：NDC [-1,1]²（pos.xy，stride 8；光照趟按 @builtin(position) 取坐标，无需 uv）。
    if (!t52_light_vbo_) {
        const float fsq[4 * 2] = {
            -1.0f, -1.0f,   1.0f, -1.0f,
             1.0f,  1.0f,  -1.0f,  1.0f,
        };
        t52_light_vbo_ = CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t52_light_ibo_ = CreateBuffer(sizeof(idx), idx, false, true);
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
        CmdBeginRenderPass(rp);
        if (!cur_pass_) return false;
        CmdSetViewport(0, 0, static_cast<int>(kT52RtSize), static_cast<int>(kT52RtSize));
        CmdBindPipeline(GetGraphicsPipeline(t52_geom_pso_, t52_geom_program_));
        const std::vector<VertexAttr> geo_attrs = { VertexAttr{0, 2, 0} };  // pos.xy
        CmdBindVertexBuffer(0, t52_geom_vbo_, 8, geo_attrs, VertexInputRate::PerVertex);
        CmdBindIndexBuffer(t52_geom_ibo_, IndexType::UInt32);
        CmdDrawIndexed(6, 0, 0);
        CmdEndRenderPass();
    }

    // ②光照趟：把 3 张 gbuffer 颜色纹理绑到 slot0/1/2，全屏 quad 经 textureLoad 做延迟光照。
    const unsigned int g_albedo   = GetRenderTargetColorTexture(t52_gbuffer_rt_, 0);
    const unsigned int g_normal   = GetRenderTargetColorTexture(t52_gbuffer_rt_, 1);
    const unsigned int g_position = GetRenderTargetColorTexture(t52_gbuffer_rt_, 2);
    if (!g_albedo || !g_normal || !g_position) {
        DEBUG_LOG_ERROR("WebGPU[T5-2] 取 gbuffer 颜色纹理失败，跳过");
        return false;
    }
    {
        RenderPassDesc rp;
        rp.render_target = t52_color_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        CmdBeginRenderPass(rp);
        if (!cur_pass_) return false;
        CmdSetViewport(0, 0, static_cast<int>(kT52RtSize), static_cast<int>(kT52RtSize));
        CmdBindPipeline(GetGraphicsPipeline(t52_light_pso_, t52_light_program_));
        const std::vector<VertexAttr> light_attrs = { VertexAttr{0, 2, 0} };  // pos.xy
        CmdBindVertexBuffer(0, t52_light_vbo_, 8, light_attrs, VertexInputRate::PerVertex);
        CmdBindIndexBuffer(t52_light_ibo_, IndexType::UInt32);
        CmdBindTexture(0u, g_albedo,   TextureDim::Tex2D);  // → group2 binding0
        CmdBindTexture(1u, g_normal,   TextureDim::Tex2D);  // → group2 binding2
        CmdBindTexture(2u, g_position, TextureDim::Tex2D);  // → group2 binding4
        CmdDrawIndexed(6, 0, 0);
        CmdEndRenderPass();
    }

    // copy color RT → 回读缓冲（随帧提交）。
    const RenderTargetEntry* rt = FindRenderTarget(t52_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const TextureEntry* color = FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT52RtBytes);
    t52_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickDeferredSelfTestReadback() {
    if (!t52_rb_pixels_) return;
    auto* ctx = new DeferredSelfTestCtx();
    ctx->rb_pixels = t52_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t52_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT52RtBytes, OnT52PixelsMapped, ctx);
}

// ============================================================================
// Task 5 Subtask 3（T5-3）：HDR auto-exposure 亮度归约 + ACES tonemap parity 离屏自检。
// ============================================================================
bool WebGPURhiDevice::RecordHDRSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    // RT：HDR 场景（8×8）、平均 log 亮度（1×1）、自动曝光（1×1）、tonemap color（64×64，CopySrc）。
    auto make_rt = [&](unsigned int& rt, uint32_t dim) {
        if (rt) return;
        RenderTargetDesc d;
        d.width = static_cast<int>(dim);
        d.height = static_cast<int>(dim);
        d.has_color = true;
        d.has_depth = false;
        rt = CreateRenderTarget(d);
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
        t53_pso_ = CreatePipelineState(d);
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
        t53_scene_program_ = CreateShaderProgram(kSceneWGSL, "");
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
        t53_reduce_program_ = CreateShaderProgram(kReduceWGSL, "");
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
        t53_adapt_program_ = CreateShaderProgram(kAdaptWGSL, "");
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
        t53_tonemap_program_ = CreateShaderProgram(kTonemapWGSL, "");
    }
    // 全屏 quad（pos.xy，stride 8；各趟按 textureLoad 固定坐标取值，无需 uv）。
    if (!t53_quad_vbo_) {
        const float fsq[4 * 2] = {
            -1.0f, -1.0f,   1.0f, -1.0f,
             1.0f,  1.0f,  -1.0f,  1.0f,
        };
        t53_quad_vbo_ = CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t53_quad_ibo_ = CreateBuffer(sizeof(idx), idx, false, true);
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
        CmdBeginRenderPass(rp);
        if (!cur_pass_) return false;
        CmdSetViewport(0, 0, static_cast<int>(dim), static_cast<int>(dim));
        CmdBindPipeline(GetGraphicsPipeline(t53_pso_, program));
        CmdBindVertexBuffer(0, t53_quad_vbo_, 8, attrs, VertexInputRate::PerVertex);
        CmdBindIndexBuffer(t53_quad_ibo_, IndexType::UInt32);
        return true;
    };

    // ①场景趟：渲已知 HDR (4,2,1) 到 8×8 场景 RT。
    if (!draw_quad(t53_scene_rt_, kT53SceneDim, t53_scene_program_, glm::vec4(0.0f))) return false;
    CmdDrawIndexed(6, 0, 0);
    CmdEndRenderPass();

    // ②归约趟：绑场景纹理 slot0，算平均 log 亮度写 1×1 lum RT。
    const unsigned int scene_tex = GetRenderTargetColorTexture(t53_scene_rt_, 0);
    if (!scene_tex) { DEBUG_LOG_ERROR("WebGPU[T5-3] 取场景纹理失败，跳过"); return false; }
    if (!draw_quad(t53_lum_rt_, 1, t53_reduce_program_, glm::vec4(0.0f))) return false;
    CmdBindTexture(0u, scene_tex, TextureDim::Tex2D);  // → group2 binding0
    CmdDrawIndexed(6, 0, 0);
    CmdEndRenderPass();

    // ③lum_adapt 趟：绑 lum 纹理 slot0，算曝光写 1×1 exposure RT。
    const unsigned int lum_tex = GetRenderTargetColorTexture(t53_lum_rt_, 0);
    if (!lum_tex) { DEBUG_LOG_ERROR("WebGPU[T5-3] 取亮度纹理失败，跳过"); return false; }
    if (!draw_quad(t53_exposure_rt_, 1, t53_adapt_program_, glm::vec4(0.0f))) return false;
    CmdBindTexture(0u, lum_tex, TextureDim::Tex2D);  // → group2 binding0
    CmdDrawIndexed(6, 0, 0);
    CmdEndRenderPass();

    // ④tonemap 趟：绑场景纹理 slot0 + 曝光纹理 slot1，ACES+gamma 渲到 64×64 color RT。
    const unsigned int exposure_tex = GetRenderTargetColorTexture(t53_exposure_rt_, 0);
    if (!exposure_tex) { DEBUG_LOG_ERROR("WebGPU[T5-3] 取曝光纹理失败，跳过"); return false; }
    if (!draw_quad(t53_color_rt_, kT53RtSize, t53_tonemap_program_, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f))) return false;
    CmdBindTexture(0u, scene_tex, TextureDim::Tex2D);     // → group2 binding0
    CmdBindTexture(1u, exposure_tex, TextureDim::Tex2D);  // → group2 binding2
    CmdDrawIndexed(6, 0, 0);
    CmdEndRenderPass();

    // copy color RT → 回读缓冲（随帧提交）。
    const RenderTargetEntry* rt = FindRenderTarget(t53_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const TextureEntry* color = FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT53RtBytes);
    t53_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickHDRSelfTestReadback() {
    if (!t53_rb_pixels_) return;
    auto* ctx = new HDRSelfTestCtx();
    ctx->rb_pixels = t53_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t53_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT53RtBytes, OnT53PixelsMapped, ctx);
}

// ============================================================================
// Task 5 Subtask 4（T5-4）：IBL（BRDF LUT + irradiance + prefilter env + PBR 环境项）离屏自检。
// ============================================================================
bool WebGPURhiDevice::RecordIBLSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    // RT：BRDF LUT（64×64）、irradiance（1×1）、prefilter（1×1）、PBR color（64×64，CopySrc）。
    auto make_rt = [&](unsigned int& rt, uint32_t dim) {
        if (rt) return;
        RenderTargetDesc d;
        d.width = static_cast<int>(dim);
        d.height = static_cast<int>(dim);
        d.has_color = true;
        d.has_depth = false;
        rt = CreateRenderTarget(d);
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
        t54_pso_ = CreatePipelineState(d);
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
        t54_brdf_program_ = CreateShaderProgram(kBrdfWGSL, "");
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
        t54_irr_program_ = CreateShaderProgram(kIrrWGSL, "");
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
        t54_pref_program_ = CreateShaderProgram(kPrefWGSL, "");
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
        t54_pbr_program_ = CreateShaderProgram(kPbrWGSL, "");
    }
    // 全屏 quad（pos.xy + uv，stride 16；BRDF LUT 趟用 uv，其余趟接收但不读）。
    if (!t54_quad_vbo_) {
        const float fsq[4 * 4] = {
            -1.0f, -1.0f, 0.0f, 0.0f,   1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,  -1.0f,  1.0f, 0.0f, 1.0f,
        };
        t54_quad_vbo_ = CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t54_quad_ibo_ = CreateBuffer(sizeof(idx), idx, false, true);
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
        CmdBeginRenderPass(rp);
        if (!cur_pass_) return false;
        CmdSetViewport(0, 0, static_cast<int>(dim), static_cast<int>(dim));
        CmdBindPipeline(GetGraphicsPipeline(t54_pso_, program));
        CmdBindVertexBuffer(0, t54_quad_vbo_, 16, attrs, VertexInputRate::PerVertex);
        CmdBindIndexBuffer(t54_quad_ibo_, IndexType::UInt32);
        return true;
    };

    // ①BRDF LUT 趟。
    if (!begin_quad(t54_brdf_rt_, kT54LutDim, t54_brdf_program_)) return false;
    CmdDrawIndexed(6, 0, 0);
    CmdEndRenderPass();
    // ②irradiance 趟。
    if (!begin_quad(t54_irr_rt_, 1, t54_irr_program_)) return false;
    CmdDrawIndexed(6, 0, 0);
    CmdEndRenderPass();
    // ③prefilter 趟。
    if (!begin_quad(t54_pref_rt_, 1, t54_pref_program_)) return false;
    CmdDrawIndexed(6, 0, 0);
    CmdEndRenderPass();

    // ④PBR 环境项趟：绑 LUT/irr/pref 三纹理，split-sum 合成渲到 64×64 color RT。
    const unsigned int brdf_tex = GetRenderTargetColorTexture(t54_brdf_rt_, 0);
    const unsigned int irr_tex  = GetRenderTargetColorTexture(t54_irr_rt_, 0);
    const unsigned int pref_tex = GetRenderTargetColorTexture(t54_pref_rt_, 0);
    if (!brdf_tex || !irr_tex || !pref_tex) {
        DEBUG_LOG_ERROR("WebGPU[T5-4] 取 LUT/irr/pref 纹理失败，跳过");
        return false;
    }
    if (!begin_quad(t54_color_rt_, kT54RtSize, t54_pbr_program_)) return false;
    CmdBindTexture(0u, brdf_tex, TextureDim::Tex2D);  // → group2 binding0
    CmdBindTexture(1u, irr_tex,  TextureDim::Tex2D);  // → group2 binding2
    CmdBindTexture(2u, pref_tex, TextureDim::Tex2D);  // → group2 binding4
    CmdDrawIndexed(6, 0, 0);
    CmdEndRenderPass();

    // copy color RT → 回读缓冲（随帧提交）。
    const RenderTargetEntry* rt = FindRenderTarget(t54_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const TextureEntry* color = FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT54RtBytes);
    t54_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickIBLSelfTestReadback() {
    if (!t54_rb_pixels_) return;
    auto* ctx = new IBLSelfTestCtx();
    ctx->rb_pixels = t54_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t54_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT54RtBytes, OnT54PixelsMapped, ctx);
}

// ============================================================================
// Task 5 Subtask 5（T5-5）：WBOIT（accum/reveal MRT + resolve）离屏自检。
// ============================================================================
bool WebGPURhiDevice::RecordWBOITSelfTest() {
    if (!device_ || !frame_encoder_ || cur_pass_ || cur_compute_pass_) return false;

    // accum/reveal MRT（2 个 RGBA16Float 颜色附件，均 TextureBinding 可采样）。
    if (!t55_mrt_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT55RtSize);
        d.height = static_cast<int>(kT55RtSize);
        d.has_color = true;
        d.has_depth = false;
        d.color_attachment_count = 2;
        t55_mrt_rt_ = CreateRenderTarget(d);
    }
    // 离屏 color RT（RGBA16Float + CopySrc）。
    if (!t55_color_rt_) {
        RenderTargetDesc d;
        d.width = static_cast<int>(kT55RtSize);
        d.height = static_cast<int>(kT55RtSize);
        d.has_color = true;
        d.has_depth = false;
        t55_color_rt_ = CreateRenderTarget(d);
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
        t55_geom_program_ = CreateShaderProgram(kGeomWGSL, "");
    }
    if (!t55_geom_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t55_geom_pso_ = CreatePipelineState(d);
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
        t55_resolve_program_ = CreateShaderProgram(kResolveWGSL, "");
    }
    if (!t55_resolve_pso_) {
        PipelineStateDesc d;
        d.blend_enabled = false;
        d.depth_test_enabled = false;
        d.depth_write_enabled = false;
        d.culling_enabled = false;
        d.cull_face = CullFace::None;
        d.topology = PrimitiveTopology::TriangleList;
        t55_resolve_pso_ = CreatePipelineState(d);
    }
    // 全屏 quad（pos.xy，stride 8）。
    if (!t55_quad_vbo_) {
        const float fsq[4 * 2] = {
            -1.0f, -1.0f,   1.0f, -1.0f,
             1.0f,  1.0f,  -1.0f,  1.0f,
        };
        t55_quad_vbo_ = CreateBuffer(sizeof(fsq), fsq, false, false);
        const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
        t55_quad_ibo_ = CreateBuffer(sizeof(idx), idx, false, true);
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
        CmdBeginRenderPass(rp);
        if (!cur_pass_) return false;
        CmdSetViewport(0, 0, static_cast<int>(kT55RtSize), static_cast<int>(kT55RtSize));
        CmdBindPipeline(GetGraphicsPipeline(t55_geom_pso_, t55_geom_program_));
        CmdBindVertexBuffer(0, t55_quad_vbo_, 8, attrs, VertexInputRate::PerVertex);
        CmdBindIndexBuffer(t55_quad_ibo_, IndexType::UInt32);
        CmdDrawIndexed(6, 0, 0);
        CmdEndRenderPass();
    }

    // ②resolve 趟：绑 accum slot0 + reveal slot1，合成渲到 64×64 color RT。
    const unsigned int accum_tex  = GetRenderTargetColorTexture(t55_mrt_rt_, 0);
    const unsigned int reveal_tex = GetRenderTargetColorTexture(t55_mrt_rt_, 1);
    if (!accum_tex || !reveal_tex) {
        DEBUG_LOG_ERROR("WebGPU[T5-5] 取 accum/reveal 纹理失败，跳过");
        return false;
    }
    {
        RenderPassDesc rp;
        rp.render_target = t55_color_rt_;
        rp.clear_color_enabled = true;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        CmdBeginRenderPass(rp);
        if (!cur_pass_) return false;
        CmdSetViewport(0, 0, static_cast<int>(kT55RtSize), static_cast<int>(kT55RtSize));
        CmdBindPipeline(GetGraphicsPipeline(t55_resolve_pso_, t55_resolve_program_));
        CmdBindVertexBuffer(0, t55_quad_vbo_, 8, attrs, VertexInputRate::PerVertex);
        CmdBindIndexBuffer(t55_quad_ibo_, IndexType::UInt32);
        CmdBindTexture(0u, accum_tex,  TextureDim::Tex2D);  // → group2 binding0
        CmdBindTexture(1u, reveal_tex, TextureDim::Tex2D);  // → group2 binding2
        CmdDrawIndexed(6, 0, 0);
        CmdEndRenderPass();
    }

    // copy color RT → 回读缓冲（随帧提交）。
    const RenderTargetEntry* rt = FindRenderTarget(t55_color_rt_);
    if (!rt || rt->color_textures.empty()) return false;
    const TextureEntry* color = FindTexture(rt->color_textures[0]);
    if (!color || !color->texture) return false;
    WGPUBufferDescriptor bd{};
    bd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    bd.size = AlignUp4(kT55RtBytes);
    t55_rb_pixels_ = wgpuDeviceCreateBuffer(device_, &bd);
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
    wgpuCommandEncoderCopyTextureToBuffer(frame_encoder_, &src, &dst, &ext);
    return true;
}

void WebGPURhiDevice::KickWBOITSelfTestReadback() {
    if (!t55_rb_pixels_) return;
    auto* ctx = new WBOITSelfTestCtx();
    ctx->rb_pixels = t55_rb_pixels_;  // 所有权转移给 ctx（回调内释放），避免 Shutdown 二次释放。
    t55_rb_pixels_ = nullptr;
    wgpuBufferMapAsync(ctx->rb_pixels, WGPUMapMode_Read, 0, kT55RtBytes, OnT55PixelsMapped, ctx);
}

// --- 设备级 Cmd*：绑定状态累积 ---

void WebGPURhiDevice::CmdBindPipeline(unsigned int graphics_pipeline_handle) {
    const GraphicsPipelineDesc* gp = GetGraphicsPipelineDesc(graphics_pipeline_handle);
    if (!gp) return;
    cur_pso_handle_ = gp->pso_state;
    if (gp->program != 0) cur_program_ = gp->program;  // program==0：仅应用 PSO，保留已绑 program
}

void WebGPURhiDevice::CmdBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                                          const std::vector<VertexAttr>& attrs, VertexInputRate rate) {
    if (slot >= cur_vbs_.size()) cur_vbs_.resize(slot + 1);
    VbBinding& vb = cur_vbs_[slot];
    vb.handle = buffer_handle;
    vb.stride = stride;
    vb.attrs = attrs;
    vb.rate = rate;
    vb.set = true;
}

void WebGPURhiDevice::CmdBindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    cur_ib_handle_ = buffer_handle;
    cur_ib_format_ = (type == IndexType::UInt32) ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16;
}

void WebGPURhiDevice::CmdBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    cur_texs_[slot] = TexBinding{texture_handle, dim};
}

void WebGPURhiDevice::CmdBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                           uint32_t offset, uint32_t size) {
    cur_ubos_[slot] = UboBinding{buffer_handle, offset, size};
}

void WebGPURhiDevice::CmdBindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                           uint32_t offset, uint32_t size) {
    cur_ssbos_[slot] = SsboBinding{buffer_handle, offset, size};
}

void WebGPURhiDevice::CmdPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) {
    if (!data || size == 0) return;
    auto write = [&](std::vector<uint8_t>& buf) {
        if (buf.size() < static_cast<size_t>(offset) + size) buf.resize(static_cast<size_t>(offset) + size, 0);
        std::memcpy(buf.data() + offset, data, size);
    };
    if (stage & ShaderStage::Vertex)   write(cur_vs_push_);
    if (stage & ShaderStage::Fragment) write(cur_fs_push_);
}

// --- 设备级 Cmd*：绘制 ---

void WebGPURhiDevice::CmdDraw(uint32_t vertex_count, uint32_t first_vertex) {
    IssueDraw(false, vertex_count, 1, first_vertex, 0, 0);
}

void WebGPURhiDevice::CmdDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    IssueDraw(true, index_count, 1, first_index, base_vertex, 0);
}

void WebGPURhiDevice::CmdDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                              uint32_t first_index, int32_t base_vertex,
                                              uint32_t first_instance) {
    IssueDraw(true, index_count, instance_count, first_index, base_vertex, first_instance);
}

// --- bring-up 自检：经 Cmd* 把渐变×棋盘纹理画到 backbuffer，验证整条录制链路 ---

void WebGPURhiDevice::EnsureSelfTestResources() {
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
    selftest_program_ = CreateShaderProgram(kSelfTestWGSL, "");

    PipelineStateDesc d;
    d.blend_enabled = false;
    d.depth_test_enabled = false;
    d.depth_write_enabled = false;
    d.culling_enabled = false;
    d.cull_face = CullFace::None;
    d.topology = PrimitiveTopology::TriangleList;
    selftest_pso_ = CreatePipelineState(d);

    // 全屏 quad（两三角形，pos.xy + uv.xy，stride 16）。
    const float quad[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };
    selftest_vbo_ = CreateBuffer(sizeof(quad), quad, false, false);

    const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    selftest_ubo_ = CreateBuffer(sizeof(tint), tint, false, false);

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
    selftest_tex_ = CreateTexture2D(8, 8, checker, false);
}

void WebGPURhiDevice::RunBringUpSelfTest() {
    EnsureSelfTestResources();
    if (!selftest_program_ || !selftest_pso_ || !selftest_vbo_ || !selftest_ubo_ || !selftest_tex_) return;

    RenderPassDesc rp;
    rp.render_target = 0;
    rp.clear_color_enabled = true;
    rp.clear_color = glm::vec4(0.05f, 0.05f, 0.08f, 1.0f);
    CmdBeginRenderPass(rp);
    if (!cur_pass_) return;
    CmdSetViewport(0, 0, width_, height_);

    const unsigned int pipe = GetGraphicsPipeline(selftest_pso_, selftest_program_);
    CmdBindPipeline(pipe);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0, 2, 0},  // pos.xy
        VertexAttr{1, 2, 8},  // uv.xy
    };
    CmdBindVertexBuffer(0, selftest_vbo_, 16, attrs, VertexInputRate::PerVertex);
    CmdBindUniformBuffer(0, selftest_ubo_, 0, 16);
    CmdBindTexture(0, selftest_tex_, TextureDim::Tex2D);
    CmdDraw(6, 0);
    CmdEndRenderPass();
}

glm::mat4 WebGPURhiDevice::GetProjectionCorrection() const {
    // WebGPU NDC：Y-up、Z∈[0,1]（同 D3D12/Metal）。从引擎默认 GL 约定（Z∈[-1,1]）
    // 重映射 Z 到 [0,1]；Y 不翻转（WebGPU 帧缓冲 Y-up）。
    glm::mat4 m(1.0f);
    m[2][2] = 0.5f;
    m[3][2] = 0.5f;
    return m;
}

glm::mat4 WebGPURhiDevice::GetShadowSampleCorrection() const {
    // 与投影矫正同源但不含 Z 重映射（着色器内统一把 Z 从 [-1,1] 映到 [0,1]）。
    return glm::mat4(1.0f);
}

} // namespace render
} // namespace dse

#endif // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
