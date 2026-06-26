/**
 * @file webgpu_draw_executor.h
 * @brief WebGPU 绘制/命令录制执行器（manager 拆分：依赖 ctx/res/pso/shader）。
 *
 * 持有命令录制全部瞬态：当前 pass 态、当前绘制绑定（VBO/UBO/纹理/SSBO/compute 视图）、
 * 渲染/计算管线缓存、push-constant 模拟池、本帧 BindGroup、mega VAO 登记、GPU-driven /
 * 点光 cube 阴影回退等。缓存同名稳定句柄 device_/queue_；每帧瞬态经 ctx_ live 访问器读取，
 * 版本环经 res_ 访问器读取，PBR 程序/PSO 经 shader_/pso_ 访问器读取。
 *
 * 仅在 Emscripten + DSE_ENABLE_WEBGPU 下编入（与各 webgpu 实现文件一致）。
 */

#ifndef DSE_WEBGPU_DRAW_EXECUTOR_H
#define DSE_WEBGPU_DRAW_EXECUTOR_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/webgpu/webgpu_common.h"
#include "engine/render/rhi/webgpu/webgpu_context.h"
#include "engine/render/rhi/webgpu/webgpu_resource_manager.h"
#include "engine/render/rhi/webgpu/webgpu_pipeline_state_manager.h"
#include "engine/render/rhi/webgpu/webgpu_shader_manager.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace render {

/**
 * @class WebGPUDrawExecutor
 * @brief 命令录制 / 绘制 / compute 派发的执行器与瞬态持有者。
 */
class WebGPUDrawExecutor {
private:
    // --- 公有方法签名引用的嵌套类型须前置定义（管线缓存条目）---
    /// 管线缓存条目：管线 + 其 explicit 布局所用的 4 组 BGL（BindGroup 须用同一 BGL 以保证兼容）。
    struct PipelineCacheEntry {
        WGPURenderPipeline pipeline = nullptr;
        WGPUPipelineLayout layout = nullptr;
        WGPUBindGroupLayout bgls[4] = {nullptr, nullptr, nullptr, nullptr};
    };
    /// compute 管线缓存条目：管线 + explicit 布局所用 4 组 BGL。
    struct ComputePipelineCacheEntry {
        WGPUComputePipeline pipeline = nullptr;
        WGPUPipelineLayout layout = nullptr;
        WGPUBindGroupLayout bgls[4] = {nullptr, nullptr, nullptr, nullptr};
    };

public:
    /// 注入依赖：ctx（设备/帧编码器/句柄）+ res（资源表/版本环）+ pso（PSO 表）+ shader（PBR/compute 着色器）。
    void Init(WebGPUContext* ctx, WebGPUResourceManager* res, WebGPUPipelineStateManager* pso,
              WebGPUShaderManager* shader, RhiDevice* owner, RenderStats* stats) {
        ctx_ = ctx; res_ = res; pso_ = pso; shader_ = shader; owner_ = owner; stats_ = stats;
    }
    /// 同名稳定句柄缓存：device 生命周期内不变（AcquireDevice 成功时设，Shutdown 时以空清）。
    void OnDeviceAcquired(WGPUDevice device, WGPUQueue queue) { device_ = device; queue_ = queue; }
    /// orchestrator Shutdown 调用：释放管线缓存 / push 池 / 本帧 BindGroup / compute mip 视图 / 回退资源。
    void Shutdown();

    // --- 当前 pass 活动态访问器（res 的 BeginGpuReadback 门控读取）---
    WGPURenderPassEncoder cur_pass() const { return cur_pass_; }
    WGPUComputePassEncoder cur_compute_pass() const { return cur_compute_pass_; }

    // --- orchestrator 每帧编排钩子（迁自 device BeginFrame/EndFrame 中的录制态部分）---
    bool backbuffer_drawn() const { return backbuffer_drawn_; }
    /// BeginFrame：复位录制瞬态（push 池游标 / 自检触发标志 / 当前绘制绑定）+ 确保阴影回退就绪。
    void BeginFrameReset() {
        backbuffer_drawn_ = false;
        push_pool_used_ = 0;
        ResetDrawState();
        EnsureShadowDepthFallback();
        EnsurePointShadowFallback();
    }
    /// EndFrame：提交后统一释放本帧创建的 BindGroup（push 缓冲跨帧复用，不在此释放）。
    void EndFrameReleaseBindGroups() {
        for (WGPUBindGroup bg : frame_bindgroups_) if (bg) wgpuBindGroupRelease(bg);
        frame_bindgroups_.clear();
    }

    // --- 迁自 device 的绘制 / 命令录制 / compute 方法（机械抽取，共 61 个）---
    void EnsureShadowDepthFallback();
    void EnsurePointShadowFallback();
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only);
    void SetComputeImageViewExplicit(uint32_t binding, WGPUTextureView view, WGPUTextureFormat format, WGPUTextureViewDimension view_dim, bool read_only);
    void InvalidateComputeMipViews(unsigned int texture_handle);
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle, int mip_level, bool read_only, bool r32f);
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle);
    size_t GetOrCreateComputeNamedOffset(const char* name, size_t data_size);
    void WriteComputeNamedStaging(size_t offset, const void* data, size_t size);
    void SetComputeUniformInt(unsigned int shader, const char* name, int value);
    void SetComputeUniformFloat(unsigned int shader, const char* name, float value);
    void SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y);
    void SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y);
    void SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z);
    void SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z);
    void SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w);
    void SetComputeUniformMat4(unsigned int shader, const char* name, const float* data);
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point);
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable);
    VertexArrayHandle CreateVertexArray();
    void DeleteVertexArray(VertexArrayHandle handle);
    void ResetDrawState();
    void ReleasePassViews();
    std::vector<BindingInfo> CollectGroupBindings(uint32_t group);
    const PipelineCacheEntry* GetOrCreateRenderPipeline();
    void BuildAndSetBindGroups(const PipelineCacheEntry& entry);
    bool BindPassDrawState(bool indexed, const PipelineCacheEntry*& pe_out);
    void IssueDraw(bool indexed, uint32_t count, uint32_t instance_count, uint32_t first, int32_t base_vertex, uint32_t first_instance);
    void CmdBeginRenderPass(const RenderPassDesc& desc);
    void CmdEndRenderPass();
    void CmdSetViewport(int x, int y, int width, int height);
    void CmdClearColor(const glm::vec4& color);
    void CmdBindGlobalShadowMap(unsigned int index, unsigned int texture_handle);
    void CmdBindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle);
    void CmdBindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle);
    void CmdDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset);
    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride, size_t byte_offset);
    VertexArrayHandle CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes, BufferHandle& out_vbo, BufferHandle& out_ibo);
    void UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data);
    void UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data);
    void DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo);
    void BindMegaVAO(VertexArrayHandle vao);
    void UnbindVAO();
    void SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camera_pos, const glm::vec3& light_dir, const glm::vec3& light_color, float light_intensity, float ambient_intensity, float shadow_strength);
    void BindGPUDrivenTextures(unsigned int albedo, unsigned int normal, unsigned int metallic_roughness, unsigned int emissive, unsigned int occlusion);
    void CmdDispatchComputePass(const ComputeDispatch& dispatch);
    void BeginComputePass();
    void EndComputePass();
    std::vector<BindingInfo> CollectComputeGroupBindings(uint32_t group, const ComputeShaderEntry& sh);
    const ComputePipelineCacheEntry* GetOrCreateComputePipeline(unsigned int shader_handle);
    void DispatchCompute(unsigned int shader_handle, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z);
    void CmdBindPipeline(unsigned int graphics_pipeline_handle);
    void CmdBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride, const std::vector<VertexAttr>& attrs, VertexInputRate rate);
    void CmdBindIndexBuffer(unsigned int buffer_handle, IndexType type);
    void CmdBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim);
    void CmdBindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size);
    void CmdBindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size);
    void CmdPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size);
    void CmdDraw(uint32_t vertex_count, uint32_t first_vertex);
    void CmdDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex);
    void CmdDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance);

private:
    // ctx 转发的同名 shim（迁移方法体内 bare 调用文本不变）。
    bool EnsureInitialized() { return ctx_->EnsureInitialized(); }
    unsigned int NextHandle() { return ctx_->NextHandle(); }
    // owner（device 基类）转发的同名 shim：图形管线解析 + 全局阴影状态写入。
    const GraphicsPipelineDesc* GetGraphicsPipelineDesc(unsigned int handle) const { return owner_->GetGraphicsPipelineDesc(handle); }
    void SetGlobalShadowMap(unsigned int index, unsigned int handle) { owner_->SetGlobalShadowMap(index, handle); }
    void SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) { owner_->SetGlobalSpotShadowMap(index, handle); }
    void SetGlobalPointShadowMap(unsigned int index, unsigned int handle) { owner_->SetGlobalPointShadowMap(index, handle); }
    /// 为纹理条目创建单层单 mip 2D 视图（cube/2D-array 逐面附件用）：转发 common.h 自由函数。
    WGPUTextureView MakeFaceView(const TextureEntry& e, int face) {
        return MakeFaceViewImpl(device_, e, face);
    }

    WebGPUContext* ctx_ = nullptr;
    WebGPUResourceManager* res_ = nullptr;
    WebGPUPipelineStateManager* pso_ = nullptr;
    WebGPUShaderManager* shader_ = nullptr;
    RhiDevice* owner_ = nullptr;       ///< 拥有者 device（基类）：解析图形管线 / 写全局阴影状态。
    RenderStats* stats_ = nullptr;     ///< 指向 orchestrator 的 last_frame_stats_（draw/pass 计数累加）。
    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;

    // --- 命令录制状态与缓存（迁自 device，纳入 draw 自治）---

    /// 每 slot 顶点流绑定（含布局 + 步进频率），Draw* 时组装 WGPUVertexBufferLayout。
    struct VbBinding {
        unsigned int handle = 0;
        uint32_t stride = 0;
        std::vector<VertexAttr> attrs;
        VertexInputRate rate = VertexInputRate::PerVertex;
        bool set = false;
    };
    struct UboBinding  { unsigned int handle = 0; uint32_t offset = 0; uint32_t size = 0; };
    struct TexBinding  { unsigned int handle = 0; TextureDim dim = TextureDim::Tex2D; };
    struct SsboBinding { unsigned int handle = 0; uint32_t offset = 0; uint32_t size = 0; };

    // --- 录制：当前 pass 状态 ---
    WGPURenderPassEncoder cur_pass_ = nullptr;
    bool cur_pass_is_backbuffer_ = false;
    std::vector<WGPUTextureFormat> cur_color_formats_;
    WGPUTextureFormat cur_depth_format_ = WGPUTextureFormat_Undefined;
    uint32_t cur_sample_count_ = 1;
    uint32_t cur_rt_width_ = 0;   ///< 当前 pass 渲染目标宽（视口裁剪用）
    uint32_t cur_rt_height_ = 0;  ///< 当前 pass 渲染目标高
    std::vector<WGPUTextureView> cur_pass_views_;  ///< 本 pass 临时创建的面视图，pass 结束释放
    std::vector<unsigned int> cur_pass_attachment_texs_;  ///< 当前 pass 颜色+深度附件句柄（读写危险检测）

    // --- 录制：当前绘制绑定（跨 Draw 持续，BeginRenderPass 时重置）---
    unsigned int cur_pso_handle_ = 0;
    unsigned int cur_program_   = 0;
    std::vector<VbBinding> cur_vbs_;
    unsigned int cur_ib_handle_ = 0;
    WGPUIndexFormat cur_ib_format_ = WGPUIndexFormat_Uint16;
    std::map<uint32_t, UboBinding>  cur_ubos_;
    std::map<uint32_t, TexBinding>  cur_texs_;
    std::map<uint32_t, SsboBinding> cur_ssbos_;
    std::map<uint32_t, unsigned int> cur_compute_images_;    ///< compute storage image（binding→纹理句柄）
    std::map<uint32_t, unsigned int> cur_compute_textures_;  ///< compute 只读采样纹理（binding→纹理句柄）
    /// compute 显式纹理视图绑定（per-mip）：同纹理不同 mip 作采样读/storage 写（Hi-Z 金字塔）。
    struct ComputeViewBind {
        WGPUTextureView         view     = nullptr;
        WGPUTextureFormat       format   = WGPUTextureFormat_Undefined;
        WGPUTextureViewDimension view_dim = WGPUTextureViewDimension_2D;
    };
    std::map<uint32_t, ComputeViewBind> cur_compute_image_views_;    ///< storage 写显式视图
    std::map<uint32_t, ComputeViewBind> cur_compute_texture_views_;  ///< 采样读显式视图
    std::map<uint64_t, WGPUTextureView> compute_mip_views_;  ///< (句柄<<16|mip) → 单层单 mip 视图缓存
    static constexpr uint32_t kComputeNamedUboBinding = 8;
    static constexpr uint32_t kComputeStorageBindingBase = 8;
    std::vector<uint8_t> compute_named_staging_;
    std::unordered_map<std::string, size_t> compute_named_offsets_;
    size_t compute_named_next_ = 0;
    // 当前 dispatch 命名 uniform 块在 UBO 版本环内的切片（DispatchCompute 填，CollectComputeGroupBindings 读）。
    WGPUBuffer cur_compute_named_buffer_ = nullptr;
    uint64_t cur_compute_named_offset_ = 0;
    uint64_t cur_compute_named_size_ = 0;
    std::vector<uint8_t> cur_vs_push_;
    std::vector<uint8_t> cur_fs_push_;

    bool backbuffer_drawn_ = false;  ///< 本帧是否有真实绘制落到 backbuffer（决定是否跑自检 pass）

    std::unordered_map<std::string, PipelineCacheEntry> pipeline_cache_;
    std::vector<WGPUBuffer> push_pool_;       ///< push-constant 模拟用 uniform 缓冲池（跨帧复用）
    size_t push_pool_used_ = 0;
    std::vector<WGPUBindGroup> frame_bindgroups_;  ///< 本帧创建的 BindGroup，提交后统一释放

    // --- compute 管线缓存 / 当前 compute pass ---
    std::unordered_map<std::string, ComputePipelineCacheEntry> compute_pipeline_cache_;
    WGPUComputePassEncoder cur_compute_pass_ = nullptr;  ///< 当前 compute pass（BeginComputePass 起）

    std::set<unsigned int> logged_incomplete_programs_;  ///< 已告警过的缺绑定程序（去重日志）

    // --- CSM shadow atlas（slot11）恒亮 Depth32 回退纹理 ---
    unsigned int shadow_fallback_rt_ = 0;          ///< 1×1 depth-only RT（懒建）
    unsigned int shadow_fallback_depth_tex_ = 0;   ///< 其 Depth32 纹理句柄（slot11 回退）
    bool shadow_fallback_cleared_ = false;         ///< 是否已清深=1.0（一次性）
    // 点光源 cube 阴影 1×1 Depth32 回退（恒亮，距离=1→无遮挡）。
    unsigned int point_shadow_fallback_rt_ = 0;        ///< 1×1×6 depth-only cube RT（懒建）
    unsigned int point_shadow_fallback_tex_ = 0;       ///< 其 Depth32 cube 纹理句柄（slot16-19 回退）
    bool point_shadow_fallback_cleared_ = false;       ///< 6 面是否已清深=1.0（一次性）
    WGPUSampler nonfilter_sampler_ = nullptr;          ///< 非过滤采样器（深度 cube 阴影采样所需）

    // --- Mega VAO 句柄记录（WebGPU 无 VAO 对象，仅记 VBO/IBO 句柄）---
    struct MegaVaoEntry { unsigned int vbo = 0; unsigned int ibo = 0; };
    std::unordered_map<unsigned int, MegaVaoEntry> mega_vaos_;  ///< VAO 句柄 → {vbo,ibo}
    unsigned int next_mega_vao_id_ = 1;                         ///< VAO 句柄发号器（0 表无效）
};

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif  // DSE_WEBGPU_DRAW_EXECUTOR_H
