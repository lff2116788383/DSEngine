/**
 * @file webgpu_draw_executor.cpp
 * @brief WebGPUDrawExecutor 实现（机械抽自 webgpu_rhi_device.cpp）。
 *
 * 方法体经 out/split_tools/gen.py 机械抽取（两步 sed + live 转发 + 跨界访问器/调用限定改写）；
 * 仅 Shutdown 为手写编排片段（释放本 manager 持有的录制缓存/池/瞬态，资源表/环归 res）。
 */

#include "engine/render/rhi/webgpu/webgpu_draw_executor.h"

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/base/debug.h"
#include "engine/render/rhi/draw_executor_common.h"
#include "engine/render/rhi/gpu_scene_types.h"

#include <glm/glm.hpp>

#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dse {
namespace render {

void WebGPUDrawExecutor::Shutdown() {
    // 录制缓存 / 池 / 瞬态：管线缓存（pipeline+layout+4×BGL）、compute 管线缓存、push 缓冲池、
    // 本帧 BindGroup、当前 compute pass、单 mip 视图缓存、临时面视图。资源表/环/着色器 module 归 res/shader。
    for (auto& [key, e] : pipeline_cache_) {
        (void)key;
        if (e.pipeline) wgpuRenderPipelineRelease(e.pipeline);
        if (e.layout)   wgpuPipelineLayoutRelease(e.layout);
        for (WGPUBindGroupLayout bgl : e.bgls) if (bgl) wgpuBindGroupLayoutRelease(bgl);
    }
    pipeline_cache_.clear();
    for (auto& [key, e] : compute_pipeline_cache_) {
        (void)key;
        if (e.pipeline) wgpuComputePipelineRelease(e.pipeline);
        if (e.layout)   wgpuPipelineLayoutRelease(e.layout);
        for (WGPUBindGroupLayout bgl : e.bgls) if (bgl) wgpuBindGroupLayoutRelease(bgl);
    }
    compute_pipeline_cache_.clear();
    if (cur_compute_pass_) { wgpuComputePassEncoderRelease(cur_compute_pass_); cur_compute_pass_ = nullptr; }
    for (auto& [k, v] : compute_mip_views_) if (v) wgpuTextureViewRelease(v);
    compute_mip_views_.clear();
    for (WGPUBuffer b : push_pool_) if (b) wgpuBufferRelease(b);
    push_pool_.clear();
    push_pool_used_ = 0;
    for (WGPUBindGroup bg : frame_bindgroups_) if (bg) wgpuBindGroupRelease(bg);
    frame_bindgroups_.clear();
    ReleasePassViews();
}

void WebGPUDrawExecutor::EnsureShadowDepthFallback() {
    // 懒建 1×1 Depth32 回退纹理，并在无活动 pass 时一次性清深=1.0（恒无遮挡）。
    if (!device_) return;
    if (!shadow_fallback_rt_) {
        RenderTargetDesc d{};
        d.width = 1; d.height = 1;
        d.has_color = false;
        d.has_depth = true;
        shadow_fallback_rt_ = res_->CreateRenderTarget(d);
        shadow_fallback_depth_tex_ = res_->GetRenderTargetDepthTexture(shadow_fallback_rt_);
        shadow_fallback_cleared_ = false;
    }
    if (shadow_fallback_cleared_ || !ctx_->frame_encoder() || cur_pass_) return;
    const TextureEntry* fb = res_->FindTexture(shadow_fallback_depth_tex_);
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
    WGPURenderPassEncoder p = wgpuCommandEncoderBeginRenderPass(ctx_->frame_encoder(), &pd);
    if (p) {
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
        shadow_fallback_cleared_ = true;
    }
}

void WebGPUDrawExecutor::EnsurePointShadowFallback() {
    // Final-Feat-8：点光源 cube 阴影回退。懒建 1×1×6 Depth32 cube 纹理 + 非过滤采样器，并在无活动
    //   pass 时逐面一次性清深=1.0（恒无遮挡）。前向 WGSL 把 slot16-19 声明为 texture_depth_cube，缺图
    //   或读写危险时换用此 cube 回退；其采样器须为 NonFiltering（Filtering 采样深度纹理会被 WebGPU 拒绝）。
    if (!device_) return;
    if (!nonfilter_sampler_) {
        WGPUSamplerDescriptor sd{};
        sd.addressModeU = WGPUAddressMode_ClampToEdge;
        sd.addressModeV = WGPUAddressMode_ClampToEdge;
        sd.addressModeW = WGPUAddressMode_ClampToEdge;
        sd.magFilter = WGPUFilterMode_Nearest;
        sd.minFilter = WGPUFilterMode_Nearest;
        sd.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        sd.lodMinClamp = 0.0f;
        sd.lodMaxClamp = 32.0f;
        sd.maxAnisotropy = 1;
        sd.compare = WGPUCompareFunction_Undefined;
        nonfilter_sampler_ = wgpuDeviceCreateSampler(device_, &sd);
    }
    if (!point_shadow_fallback_rt_) {
        RenderTargetDesc d{};
        d.width = 1; d.height = 1;
        d.has_color = false;
        d.has_depth = true;
        d.cube_map = true;
        point_shadow_fallback_rt_ = res_->CreateRenderTarget(d);
        point_shadow_fallback_tex_ = res_->GetRenderTargetDepthTexture(point_shadow_fallback_rt_);
        point_shadow_fallback_cleared_ = false;
    }
    if (point_shadow_fallback_cleared_ || !ctx_->frame_encoder() || cur_pass_) return;
    const TextureEntry* fb = res_->FindTexture(point_shadow_fallback_tex_);
    if (!fb || !fb->texture) return;
    // 逐面建 2D depth 视图并清深=1.0（cube 默认视图为 Cube，不能作深度附件）。
    bool all_ok = true;
    for (int face = 0; face < 6; ++face) {
        WGPUTextureView fv = MakeFaceView(*fb, face);
        if (!fv) { all_ok = false; break; }
        WGPURenderPassDepthStencilAttachment ds{};
        ds.view = fv;
        ds.depthLoadOp = WGPULoadOp_Clear;
        ds.depthStoreOp = WGPUStoreOp_Store;
        ds.depthClearValue = 1.0f;           // 深度=1 → PointShadow 恒判无遮挡
        ds.stencilLoadOp = WGPULoadOp_Undefined;
        ds.stencilStoreOp = WGPUStoreOp_Undefined;
        WGPURenderPassDescriptor pd{};
        pd.colorAttachmentCount = 0;
        pd.colorAttachments = nullptr;
        pd.depthStencilAttachment = &ds;
        WGPURenderPassEncoder p = wgpuCommandEncoderBeginRenderPass(ctx_->frame_encoder(), &pd);
        if (p) {
            wgpuRenderPassEncoderEnd(p);
            wgpuRenderPassEncoderRelease(p);
        } else {
            all_ok = false;
        }
        wgpuTextureViewRelease(fv);
        if (!all_ok) break;
    }
    if (all_ok) point_shadow_fallback_cleared_ = true;
}

void WebGPUDrawExecutor::SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) {
    // read_only=false：可写 storage image（textureStore，WriteOnly 访问）→ cur_compute_images_。
    // read_only=true：只读采样纹理（compute textureLoad，无 sampler）→ cur_compute_textures_。
    // 二者按声明的绑定槽分别收集，使同一 group2 可混合「采样读 src + storage 写 dst」（Hi-Z 下采样模式）。
    if (read_only) {
        if (texture_handle) cur_compute_textures_[binding] = texture_handle;
        else                cur_compute_textures_.erase(binding);
    } else {
        if (texture_handle) cur_compute_images_[binding] = texture_handle;
        else                cur_compute_images_.erase(binding);
    }
}

void WebGPUDrawExecutor::SetComputeImageViewExplicit(uint32_t binding, WGPUTextureView view,
                                                  WGPUTextureFormat format,
                                                  WGPUTextureViewDimension view_dim, bool read_only) {
    // 直接绑定显式纹理视图（如 Hi-Z 金字塔的单 mip 视图），绕开「句柄→默认全 mip 视图」。
    // read_only=true→采样读（textureLoad）；false→storage 写（textureStore）。
    auto& m = read_only ? cur_compute_texture_views_ : cur_compute_image_views_;
    if (view) m[binding] = ComputeViewBind{view, format, view_dim};
    else      m.erase(binding);
}

void WebGPUDrawExecutor::InvalidateComputeMipViews(unsigned int texture_handle) {
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

void WebGPUDrawExecutor::SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                                int mip_level, bool read_only, bool r32f) {
    // 引擎 Hi-Z build 真实绑定面——按句柄 + mip 级绑定单层单 mip 视图到 compute group2 槽。
    // 同一槽可在相邻 dispatch 间在「采样读 / storage 写」间切换（Hi-Z：先写 mip0，再逐级读 mip[k-1]
    // 写 mip[k]），且无 DispatchCompute 间自动 ResetDrawState，故先从「读/写」两映射均擦除该槽，
    // 再按本次 read_only 路由，避免上一次 dispatch 的陈旧绑定残留同槽。
    cur_compute_texture_views_.erase(binding);
    cur_compute_image_views_.erase(binding);
    if (!texture_handle) return;

    const TextureEntry* e = res_->FindTexture(texture_handle);
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

void WebGPUDrawExecutor::SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
    // 引擎 compute 采样面（Hi-Z/GPU-driven 剔除读 Hi-Z/深度纹理）。WebGPU 路由到只读采样
    //   绑定（group2 binding=unit，texture_2d<f32>，手译 WGSL 用 textureLoad 取代 textureLod）。
    //   同槽先清显式视图 / storage image 绑定，避免上一次 dispatch 的陈旧绑定残留同槽。
    cur_compute_image_views_.erase(unit);
    cur_compute_texture_views_.erase(unit);
    cur_compute_images_.erase(unit);
    if (!texture_handle) { cur_compute_textures_.erase(unit); return; }
    cur_compute_textures_[unit] = texture_handle;
}

size_t WebGPUDrawExecutor::GetOrCreateComputeNamedOffset(const char* name, size_t data_size) {
    auto it = compute_named_offsets_.find(name);
    if (it != compute_named_offsets_.end()) return it->second;
    const size_t offset = (compute_named_next_ + 15) & ~size_t(15);
    compute_named_offsets_[name] = offset;
    compute_named_next_ = offset + data_size;
    return offset;
}

void WebGPUDrawExecutor::WriteComputeNamedStaging(size_t offset, const void* data, size_t size) {
    if (compute_named_staging_.size() < offset + size) compute_named_staging_.resize(offset + size, 0);
    std::memcpy(compute_named_staging_.data() + offset, data, size);
}

void WebGPUDrawExecutor::SetComputeUniformInt(unsigned int shader, const char* name, int value) {
    (void)shader;
    if (!name) return;
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(int)), &value, sizeof(int));
}

void WebGPUDrawExecutor::SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
    (void)shader;
    if (!name) return;
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(float)), &value, sizeof(float));
}

void WebGPUDrawExecutor::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
    (void)shader;
    if (!name) return;
    const int d[2]{x, y};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPUDrawExecutor::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
    (void)shader;
    if (!name) return;
    const float d[2]{x, y};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPUDrawExecutor::SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) {
    (void)shader;
    if (!name) return;
    const float d[3]{x, y, z};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPUDrawExecutor::SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) {
    (void)shader;
    if (!name) return;
    const int d[3]{x, y, z};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPUDrawExecutor::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
    (void)shader;
    if (!name) return;
    const float d[4]{x, y, z, w};
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, sizeof(d)), d, sizeof(d));
}

void WebGPUDrawExecutor::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    (void)shader;
    if (!name || !data) return;
    WriteComputeNamedStaging(GetOrCreateComputeNamedOffset(name, 64), data, 64);
}

void WebGPUDrawExecutor::BindGpuBuffer(BufferHandle handle, uint32_t binding_point) {
    // 路由到 group3 SSBO 槽位图（与 CmdBindStorageBuffer 一致）。size=0 → 整缓冲（见 CollectComputeGroupBindings）。
    cur_ssbos_[binding_point] = SsboBinding{handle.raw(), 0, 0};
}

void WebGPUDrawExecutor::BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) {
    (void)writable;  // WebGPU compute storage 统一 read_write，writable 无意义
    BindGpuBuffer(handle, binding_point);
}

VertexArrayHandle WebGPUDrawExecutor::CreateVertexArray() {
    return VertexArrayHandle{NextHandle()};
}

void WebGPUDrawExecutor::DeleteVertexArray(VertexArrayHandle handle) {
    (void)handle;
}

void WebGPUDrawExecutor::ResetDrawState() {
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

void WebGPUDrawExecutor::ReleasePassViews() {
    for (WGPUTextureView v : cur_pass_views_) {
        if (v) wgpuTextureViewRelease(v);
    }
    cur_pass_views_.clear();
}

std::vector<BindingInfo> WebGPUDrawExecutor::CollectGroupBindings(uint32_t group) {
    // 各组遍历顺序在 BGL（GetOrCreateRenderPipeline）与 BindGroup（BuildAndSetBindGroups）间共用，
    // 杜绝二者发散。group0=push（uniform 池）、group1=UBO、group2=texture+sampler、group3=SSBO。
    // std::map 保证按 binding 升序稳定遍历。
    std::vector<BindingInfo> out;
    const WGPUShaderStageFlags kVsFs = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    // 仅纳入当前 WGSL 程序实际声明的绑定：引擎可能绑定多于着色器所需的资源（如 ForwardShaded
    // 绑 8 UBO/20 纹理槽），全量纳入会超 per-stage 采样上限并使 layout 与着色器用量不符。
    const std::set<uint32_t>* used = nullptr;
    {
        if (const ShaderEntry* se = shader_->FindShader(cur_program_)) used = &se->wgsl_bindings;
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
                const BufferEntry* be = res_->FindBuffer(u.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Uniform; b.visibility = kVsFs;
                // 优先用当帧最近一次 UBO 版本切片（无范围偏移绑定时）：使逐 draw 材质数据互不覆盖。
                const UboVersion* vit = (u.offset == 0) ? res_->FindUboVersion(u.handle) : nullptr;
                if (vit && vit->buffer) {
                    b.buffer = vit->buffer;
                    b.buf_offset = vit->offset;
                    b.buf_size = vit->size;
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
                const TextureEntry* te = res_->FindTexture(t.handle);
                if (!te || !te->view) continue;
                WGPUTextureView      use_view = te->view;
                WGPUSampler          use_smp  = te->sampler;
                WGPUTextureSampleType use_st   = IsDepthFormat(te->format)
                                                     ? WGPUTextureSampleType_Depth
                                                     : WGPUTextureSampleType_Float;
                TextureDim           use_dim  = t.dim;
                // 5.1b / Final-Feat-8：阴影深度槽须恒以 Depth 采样。前向 WGSL 把
                //   slot11=CSM atlas、slot12-15=聚光灯、slot16-19=点光源 分别声明为 texture_depth_2d /
                //   texture_depth_cube。两种情形换用恒亮 Depth32 回退纹理（2D / cube）：
                //   (a) 消费方在无 shadow map 时绑「白 RGBA8/白 cube」回退（Float）→ 统一 sampleType 为 Depth；
                //   (b) 读写危险：当前 pass 把真深度图作可写附件（depth-only 阴影 pass 仍复用 forward PSO，
                //       逐 draw 绑该槽），WebGPU 不允许同一同步作用域内既可写附件又被采样 → 换只读回退纹理。
                //   点光 cube 深度还须配「非过滤」采样器：RT 自带 Filtering 采样器采样深度纹理会触发 WebGPU
                //   校验错误（TextureSampleType::Depth 配 Filtering），故无论真假纹理都改用 nonfilter_sampler_。
                const bool is_shadow_2d   = (slot == 11u) || (slot >= 12u && slot <= 15u);
                const bool is_shadow_cube = (slot >= 16u && slot <= 19u);
                if (is_shadow_2d || is_shadow_cube) {
                    const bool not_depth = (use_st != WGPUTextureSampleType_Depth);
                    const bool rw_hazard =
                        std::find(cur_pass_attachment_texs_.begin(),
                                  cur_pass_attachment_texs_.end(), t.handle)
                        != cur_pass_attachment_texs_.end();
                    if (not_depth || rw_hazard) {
                        const unsigned int fbh = is_shadow_cube ? point_shadow_fallback_tex_
                                                                : shadow_fallback_depth_tex_;
                        const TextureEntry* fb = fbh ? res_->FindTexture(fbh) : nullptr;
                        if (fb && fb->view) {
                            use_view = fb->view;
                            use_smp  = fb->sampler;
                            use_st   = WGPUTextureSampleType_Depth;
                            use_dim  = is_shadow_cube ? TextureDim::TexCube : TextureDim::Tex2D;
                        }
                    }
                    if (is_shadow_cube && nonfilter_sampler_) use_smp = nonfilter_sampler_;
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
                sb.sampler_nonfiltering = is_shadow_cube;  // 深度 cube 阴影：BGL 端声明 NonFiltering
                out.push_back(sb);
            }
            break;
        }
        case 3: {
            for (const auto& [slot, s] : cur_ssbos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = res_->FindBuffer(s.handle);
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

const WebGPUDrawExecutor::PipelineCacheEntry* WebGPUDrawExecutor::GetOrCreateRenderPipeline() {
    // 无 WGSL module 的程序（引擎 GLSL，无离线转译）：返回 nullptr，调用方跳过该 draw。
    const ShaderEntry* se = shader_->FindShader(cur_program_);
    if (!se || !se->module) return nullptr;
    const ShaderEntry& sh = *se;

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
    const PipelineStateDesc* pso = pso_->FindPipelineState(cur_pso_handle_);
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
                    // Final-Feat-8：深度纹理（点光 cube 阴影）须配非过滤采样器，否则 WebGPU 校验报
                    //   「TextureSampleType::Depth 配 Filtering 采样器」。普通纹理仍用 Filtering。
                    e.sampler.type = b.sampler_nonfiltering ? WGPUSamplerBindingType_NonFiltering
                                                            : WGPUSamplerBindingType_Filtering; break;
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

void WebGPUDrawExecutor::BuildAndSetBindGroups(const PipelineCacheEntry& entry) {
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

bool WebGPUDrawExecutor::BindPassDrawState(bool indexed, const PipelineCacheEntry*& pe_out) {
    const PipelineCacheEntry* pe = GetOrCreateRenderPipeline();
    if (!pe || !pe->pipeline) return false;  // 无 WGSL module / 组装失败 → 优雅跳过

    wgpuRenderPassEncoderSetPipeline(cur_pass_, pe->pipeline);
    for (size_t i = 0; i < cur_vbs_.size(); ++i) {
        const VbBinding& vb = cur_vbs_[i];
        if (!vb.set) continue;
        // 当帧若有该顶点缓冲的版本切片（每 draw 重写共享 vbo_），改绑版本切片避免合并覆盖。
        const UboVersion* git = res_->FindGeomVersion(vb.handle);
        if (git && git->buffer) {
            wgpuRenderPassEncoderSetVertexBuffer(cur_pass_, static_cast<uint32_t>(i),
                                                 git->buffer, git->offset, git->size);
            continue;
        }
        const BufferEntry* be = res_->FindBuffer(vb.handle);
        if (!be || !be->buffer) continue;
        wgpuRenderPassEncoderSetVertexBuffer(cur_pass_, static_cast<uint32_t>(i), be->buffer, 0, be->size);
    }
    BuildAndSetBindGroups(*pe);

    if (indexed) {
        // 同顶点：优先当帧索引版本切片（每 draw 重写共享 ibo_），否则原索引缓冲。
        const UboVersion* git = res_->FindGeomVersion(cur_ib_handle_);
        if (git && git->buffer) {
            wgpuRenderPassEncoderSetIndexBuffer(cur_pass_, git->buffer, cur_ib_format_,
                                                git->offset, git->size);
        } else {
            const BufferEntry* ib = res_->FindBuffer(cur_ib_handle_);
            if (!ib || !ib->buffer) return false;
            wgpuRenderPassEncoderSetIndexBuffer(cur_pass_, ib->buffer, cur_ib_format_, 0, ib->size);
        }
    }
    pe_out = pe;
    return true;
}

void WebGPUDrawExecutor::IssueDraw(bool indexed, uint32_t count, uint32_t instance_count,
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
    stats_->draw_calls += 1;
}

void WebGPUDrawExecutor::CmdBeginRenderPass(const RenderPassDesc& desc) {
    if (!ctx_->frame_encoder()) { cur_pass_ = nullptr; return; }
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
        if (!ctx_->backbuffer_view()) { cur_pass_ = nullptr; return; }
        cur_rt_width_ = static_cast<uint32_t>(ctx_->width() > 0 ? ctx_->width() : 1);
        cur_rt_height_ = static_cast<uint32_t>(ctx_->height() > 0 ? ctx_->height() : 1);
        WGPURenderPassColorAttachment ca{};
        ca.view = ctx_->backbuffer_view();
        ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        ca.loadOp = load;
        ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = clear;
        color_atts.push_back(ca);
        cur_color_formats_.push_back(ctx_->swapchain_format());
    } else {
        const RenderTargetEntry* rt = res_->FindRenderTarget(desc.render_target);
        if (!rt) { cur_pass_ = nullptr; return; }
        cur_sample_count_ = static_cast<uint32_t>(rt->msaa_samples > 1 ? rt->msaa_samples : 1);
        cur_rt_width_ = rt->width > 0 ? rt->width : 1;
        cur_rt_height_ = rt->height > 0 ? rt->height : 1;
        for (unsigned int th : rt->color_textures) {
            const TextureEntry* te = res_->FindTexture(th);
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
            const TextureEntry* de = res_->FindTexture(rt->depth_texture);
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
                depth_att.depthLoadOp = WGPULoadOp_Clear;  // 深度恒清
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

    cur_pass_ = wgpuCommandEncoderBeginRenderPass(ctx_->frame_encoder(), &pd);
    stats_->render_passes += 1;
}

void WebGPUDrawExecutor::CmdEndRenderPass() {
    if (cur_pass_) {
        wgpuRenderPassEncoderEnd(cur_pass_);
        wgpuRenderPassEncoderRelease(cur_pass_);
        cur_pass_ = nullptr;
    }
    ReleasePassViews();
}

void WebGPUDrawExecutor::CmdSetViewport(int x, int y, int width, int height) {
    if (!cur_pass_ || width <= 0 || height <= 0) return;
    // 裁剪到当前 pass 渲染目标范围：WebGPU 要求视口完全落在目标内，否则整个命令缓冲失效。
    // 无 WGSL module 的 pass 绘制虽被跳过，但其 SetViewport 仍会录制，故须在此防御性裁剪。
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

void WebGPUDrawExecutor::CmdClearColor(const glm::vec4& color) { (void)color; }

void WebGPUDrawExecutor::CmdBindGlobalShadowMap(unsigned int index, unsigned int texture_handle) {
    SetGlobalShadowMap(index, texture_handle);
}

void WebGPUDrawExecutor::CmdBindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) {
    SetGlobalSpotShadowMap(index, texture_handle);
}

void WebGPUDrawExecutor::CmdBindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) {
    SetGlobalPointShadowMap(index, texture_handle);
}

void WebGPUDrawExecutor::CmdDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset) {
    if (!cur_pass_) return;
    const BufferEntry* ind = res_->FindBuffer(indirect_buffer);
    if (!ind || !ind->buffer) return;
    const PipelineCacheEntry* pe = nullptr;
    if (!BindPassDrawState(/*indexed=*/true, pe)) return;
    wgpuRenderPassEncoderDrawIndexedIndirect(cur_pass_, ind->buffer, byte_offset);
    if (cur_pass_is_backbuffer_) backbuffer_drawn_ = true;
    stats_->draw_calls += 1;
}

void WebGPUDrawExecutor::MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count,
                                               size_t stride, size_t byte_offset) {
    if (!cur_pass_ || draw_count <= 0) return;
    const BufferEntry* ind = res_->FindBuffer(indirect_buffer);
    if (!ind || !ind->buffer) return;
    const PipelineCacheEntry* pe = nullptr;
    if (!BindPassDrawState(/*indexed=*/true, pe)) return;
    for (int i = 0; i < draw_count; ++i) {
        const uint64_t off = static_cast<uint64_t>(byte_offset) +
                             static_cast<uint64_t>(i) * static_cast<uint64_t>(stride);
        wgpuRenderPassEncoderDrawIndexedIndirect(cur_pass_, ind->buffer, off);
    }
    if (cur_pass_is_backbuffer_) backbuffer_drawn_ = true;
    stats_->draw_calls += static_cast<uint32_t>(draw_count);
}

VertexArrayHandle WebGPUDrawExecutor::CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                                 BufferHandle& out_vbo, BufferHandle& out_ibo) {
    if (vbo_size_bytes == 0 || ibo_size_bytes == 0) { out_vbo = {}; out_ibo = {}; return {}; }
    GpuBufferDesc vd; vd.size = vbo_size_bytes; vd.usage = GpuBufferUsage::kVertex;
    GpuBufferDesc id; id.size = ibo_size_bytes; id.usage = GpuBufferUsage::kIndex;
    BufferHandle vbo = res_->CreateGpuBuffer(vd, nullptr);
    BufferHandle ibo = res_->CreateGpuBuffer(id, nullptr);
    if (!vbo || !ibo) {
        if (vbo) res_->DeleteGpuBuffer(vbo);
        if (ibo) res_->DeleteGpuBuffer(ibo);
        out_vbo = {}; out_ibo = {};
        return {};
    }
    const unsigned int vao_id = next_mega_vao_id_++;
    mega_vaos_[vao_id] = MegaVaoEntry{vbo.raw(), ibo.raw()};
    out_vbo = vbo;
    out_ibo = ibo;
    return VertexArrayHandle{vao_id};
}

void WebGPUDrawExecutor::UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) {
    if (!vbo || size == 0 || !data) return;
    res_->UpdateGpuBuffer(vbo, offset, size, data);
}

void WebGPUDrawExecutor::UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) {
    if (!ibo || size == 0 || !data) return;
    res_->UpdateGpuBuffer(ibo, offset, size, data);
}

void WebGPUDrawExecutor::DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) {
    if (vao) mega_vaos_.erase(vao.raw());
    if (vbo) res_->DeleteGpuBuffer(vbo);
    if (ibo) res_->DeleteGpuBuffer(ibo);
}

void WebGPUDrawExecutor::BindMegaVAO(VertexArrayHandle vao) {
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

void WebGPUDrawExecutor::UnbindVAO() {
    // WebGPU 无 VAO 对象——清除已绑顶点/索引 draw state（等价 glBindVertexArray(0)）。
    cur_ib_handle_ = 0;
    if (!cur_vbs_.empty()) cur_vbs_[0].set = false;
}

void WebGPUDrawExecutor::SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                              const glm::vec3& camera_pos,
                                              const glm::vec3& light_dir, const glm::vec3& light_color,
                                              float light_intensity, float ambient_intensity,
                                              float shadow_strength) {
    if (!shader_->EnsureGpuDrivenPBRShader()) return;
    cur_program_    = shader_->gpu_driven_pbr_program();
    cur_pso_handle_ = shader_->gpu_driven_pbr_pso();

    PerFrameUBO per_frame{};
    per_frame.vp = proj * view;
    per_frame.view = view;
    per_frame.camera_pos = glm::vec4(camera_pos, 0.0f);
    res_->UpdateGpuBuffer(shader_->gpu_driven_perframe_ubo(), 0, sizeof(per_frame), &per_frame);

    PerSceneUBO per_scene{};
    per_scene.light_dir_and_enabled   = glm::vec4(light_dir, 1.0f);
    per_scene.light_color_and_ambient = glm::vec4(light_color, ambient_intensity);
    const float receive_shadow = (shadow_strength > 0.0f) ? 1.0f : 0.0f;
    per_scene.light_params = glm::vec4(light_intensity, shadow_strength, receive_shadow, 0.0f);
    res_->UpdateGpuBuffer(shader_->gpu_driven_perscene_ubo(), 0, sizeof(per_scene), &per_scene);

    CmdBindUniformBuffer(0, shader_->gpu_driven_perframe_ubo().raw(), 0, sizeof(per_frame));
    CmdBindUniformBuffer(1, shader_->gpu_driven_perscene_ubo().raw(), 0, sizeof(per_scene));
}

void WebGPUDrawExecutor::BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                                            unsigned int metallic_roughness,
                                            unsigned int emissive, unsigned int occlusion) {
    if (!shader_->white_texture()) shader_->EnsureGpuDrivenPBRShader();
    const unsigned int w = shader_->white_texture();
    CmdBindTexture(0, albedo             ? albedo             : w, TextureDim::Tex2D);
    CmdBindTexture(1, normal             ? normal             : w, TextureDim::Tex2D);
    CmdBindTexture(2, metallic_roughness ? metallic_roughness : w, TextureDim::Tex2D);
    CmdBindTexture(3, emissive           ? emissive           : w, TextureDim::Tex2D);
    CmdBindTexture(4, occlusion          ? occlusion          : w, TextureDim::Tex2D);
}

void WebGPUDrawExecutor::CmdDispatchComputePass(const ComputeDispatch& dispatch) { (void)dispatch; }

void WebGPUDrawExecutor::BeginComputePass() {
    // 不可与 render pass 嵌套；同一时刻仅一个 compute pass。
    if (!ctx_->frame_encoder() || cur_compute_pass_ || cur_pass_) return;
    cur_compute_pass_ = wgpuCommandEncoderBeginComputePass(ctx_->frame_encoder(), nullptr);
}

void WebGPUDrawExecutor::EndComputePass() {
    if (!cur_compute_pass_) return;
    wgpuComputePassEncoderEnd(cur_compute_pass_);
    wgpuComputePassEncoderRelease(cur_compute_pass_);
    cur_compute_pass_ = nullptr;
}

std::vector<BindingInfo>
WebGPUDrawExecutor::CollectComputeGroupBindings(uint32_t group, const ComputeShaderEntry& sh) {
    // 对齐 render 路径的 group 约定，compute 仅接 group1=UBO、group3=SSBO（可见性 Compute）；
    // group0（push）compute 不用；group2（texture/sampler/storage image）在下方 case 2 收集。
    std::vector<BindingInfo> out;
    const WGPUShaderStageFlags kCs = WGPUShaderStage_Compute;
    auto declared = [&](uint32_t binding) -> bool {
        return sh.wgsl_bindings.count((group << 16) | binding) != 0;
    };
    switch (group) {
        case 1: {
            for (const auto& [slot, u] : cur_ubos_) {
                if (!declared(slot)) continue;
                const BufferEntry* be = res_->FindBuffer(u.handle);
                if (!be || !be->buffer) continue;
                BindingInfo b;
                b.binding = slot; b.kind = BindingInfo::Kind::Uniform; b.visibility = kCs;
                b.buffer = be->buffer; b.buf_offset = u.offset;
                b.buf_size = u.size ? u.size : be->size;
                out.push_back(b);
            }
            // 命名 uniform 块（SetComputeUniform* 累积）→ group1 保留 binding。仅当着色器
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
            // compute 只读采样纹理（texture_2d<f32>/texture_depth_2d，textureLoad，无 sampler）。
            // r32float 为 unfilterable-float，深度格式为 depth sampleType，需相应声明。@binding=slot。
            for (const auto& [slot, tex_handle] : cur_compute_textures_) {
                if (!declared(slot)) continue;
                const TextureEntry* te = res_->FindTexture(tex_handle);
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
            // compute storage image（texture_storage_2d<...,write>）。@binding=slot（冲突时错开）。
            for (const auto& [slot, tex_handle] : cur_compute_images_) {
                const uint32_t bnd = storage_binding(slot);
                if (!declared(bnd)) continue;
                const TextureEntry* te = res_->FindTexture(tex_handle);
                if (!te || !te->view) continue;
                BindingInfo b;
                b.binding = bnd; b.kind = BindingInfo::Kind::StorageTexture; b.visibility = kCs;
                b.view = te->view; b.tex_format = te->format; b.view_dim = te->view_dim;
                out.push_back(b);
            }
            // compute 显式视图绑定（per-mip Hi-Z 金字塔）。采样读视图（textureLoad，无 sampler）。
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
            // storage 写显式视图（texture_storage_2d<...,write>，绑定单 mip 视图）。冲突时错开。
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
                const BufferEntry* be = res_->FindBuffer(s.handle);
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

const WebGPUDrawExecutor::ComputePipelineCacheEntry*
WebGPUDrawExecutor::GetOrCreateComputePipeline(unsigned int shader_handle) {
    const ComputeShaderEntry* sit = shader_->FindComputeShader(shader_handle);
    if (!sit || !sit->module) return nullptr;
    const ComputeShaderEntry& sh = *sit;

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
                    // compute 写 storage image（rgba8unorm 仅 WriteOnly 访问）。
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

void WebGPUDrawExecutor::DispatchCompute(unsigned int shader_handle,
                                      unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
    if (!cur_compute_pass_ || groups_x == 0 || groups_y == 0 || groups_z == 0) return;
    const ComputeShaderEntry* it = shader_->FindComputeShader(shader_handle);
    if (!it || !it->module) return;

    // 命名 uniform 暂存（SetComputeUniform* 按调用序 16B 对齐累积）→ 经 UBO 版本环分配独立
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
        const uint64_t off = res_->AllocUboVersion(compute_named_staging_.data(), compute_named_staging_.size());
        if (off != UINT64_MAX) {
            cur_compute_named_buffer_ = res_->ubo_ring();
            cur_compute_named_offset_ = off;
            cur_compute_named_size_   = compute_named_staging_.size();
        }
    }

    const ComputePipelineCacheEntry* pe = GetOrCreateComputePipeline(shader_handle);
    if (!pe || !pe->pipeline) return;

    wgpuComputePassEncoderSetPipeline(cur_compute_pass_, pe->pipeline);
    for (uint32_t g = 0; g < 4; ++g) {
        const std::vector<BindingInfo> bs = CollectComputeGroupBindings(g, *it);
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

    // 每次 dispatch 自带一组命名 uniform——上传后清空暂存与定位表（与 GL/DX11 同义）。
    compute_named_staging_.clear();
    compute_named_offsets_.clear();
    compute_named_next_ = 0;
}

void WebGPUDrawExecutor::CmdBindPipeline(unsigned int graphics_pipeline_handle) {
    const GraphicsPipelineDesc* gp = GetGraphicsPipelineDesc(graphics_pipeline_handle);
    if (!gp) return;
    cur_pso_handle_ = gp->pso_state;
    if (gp->program != 0) cur_program_ = gp->program;  // program==0：仅应用 PSO，保留已绑 program
}

void WebGPUDrawExecutor::CmdBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                                          const std::vector<VertexAttr>& attrs, VertexInputRate rate) {
    if (slot >= cur_vbs_.size()) cur_vbs_.resize(slot + 1);
    VbBinding& vb = cur_vbs_[slot];
    vb.handle = buffer_handle;
    vb.stride = stride;
    vb.attrs = attrs;
    vb.rate = rate;
    vb.set = true;
}

void WebGPUDrawExecutor::CmdBindIndexBuffer(unsigned int buffer_handle, IndexType type) {
    cur_ib_handle_ = buffer_handle;
    cur_ib_format_ = (type == IndexType::UInt32) ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16;
}

void WebGPUDrawExecutor::CmdBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    cur_texs_[slot] = TexBinding{texture_handle, dim};
}

void WebGPUDrawExecutor::CmdBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                           uint32_t offset, uint32_t size) {
    cur_ubos_[slot] = UboBinding{buffer_handle, offset, size};
}

void WebGPUDrawExecutor::CmdBindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                           uint32_t offset, uint32_t size) {
    cur_ssbos_[slot] = SsboBinding{buffer_handle, offset, size};
}

void WebGPUDrawExecutor::CmdPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) {
    if (!data || size == 0) return;
    auto write = [&](std::vector<uint8_t>& buf) {
        if (buf.size() < static_cast<size_t>(offset) + size) buf.resize(static_cast<size_t>(offset) + size, 0);
        std::memcpy(buf.data() + offset, data, size);
    };
    if (stage & ShaderStage::Vertex)   write(cur_vs_push_);
    if (stage & ShaderStage::Fragment) write(cur_fs_push_);
}

void WebGPUDrawExecutor::CmdDraw(uint32_t vertex_count, uint32_t first_vertex) {
    IssueDraw(false, vertex_count, 1, first_vertex, 0, 0);
}

void WebGPUDrawExecutor::CmdDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex) {
    IssueDraw(true, index_count, 1, first_index, base_vertex, 0);
}

void WebGPUDrawExecutor::CmdDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                              uint32_t first_index, int32_t base_vertex,
                                              uint32_t first_instance) {
    IssueDraw(true, index_count, instance_count, first_index, base_vertex, first_instance);
}

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
