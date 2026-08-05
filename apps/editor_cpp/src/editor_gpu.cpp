#include "editor_gpu.h"

#include "editor_imgui_backend.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

namespace dse::editor {

namespace {
dse::render::RhiDevice* g_rhi = nullptr;
ImGuiBackend* g_imgui_backend = nullptr;
}  // namespace

void SetEditorRhiDevice(dse::render::RhiDevice* device) { g_rhi = device; }
void SetEditorImGuiBackend(ImGuiBackend* backend) { g_imgui_backend = backend; }

dse::render::RhiDevice* EditorRhi() { return g_rhi; }

std::uint64_t EditorImGuiTextureId(unsigned int handle) {
    return g_imgui_backend
        ? static_cast<std::uint64_t>(g_imgui_backend->GetTextureId(handle))
        : 0;
}

unsigned int EditorCreateTexture2D(int width, int height, const uint8_t* rgba8,
                                   bool linear, bool clamp) {
    if (!g_rhi || width <= 0 || height <= 0) return 0;
    // RHI 共享类型（rhi_types.h）定义在全局命名空间。
    TextureSamplerDesc sampler;
    sampler.filter = linear ? TextureFilter::Linear : TextureFilter::Nearest;
    sampler.wrap = clamp ? TextureWrap::ClampToEdge : TextureWrap::Repeat;
    return g_rhi->CreateTexture2D(width, height, rgba8, sampler).raw();
}

void EditorDeleteTexture(unsigned int handle) {
    if (handle == 0) return;
    if (g_imgui_backend) g_imgui_backend->ReleaseTexture(handle);
    if (g_rhi) {
        g_rhi->DeleteTexture(dse::render::TextureHandle::from_raw(handle));
    }
}

// ── RenderTarget / Blit（多视口）────────────────────────────────────────────

unsigned int EditorCreateBlitTarget(int width, int height) {
    if (!g_rhi || width <= 0 || height <= 0) return 0;
    RenderTargetDesc desc;  // RHI 共享类型（rhi_types.h）定义在全局命名空间
    desc.width = width;
    desc.height = height;
    desc.has_color = true;
    desc.has_depth = false;
    desc.msaa_samples = 1;  // 单采样目标：显示与后续采样可直接使用
    return g_rhi->CreateRenderTarget(desc).raw();
}

void EditorBlitRenderTarget(unsigned int src_rt, unsigned int dst_rt) {
    if (!g_rhi || src_rt == 0 || dst_rt == 0) return;
    g_rhi->BlitRenderTarget(dse::render::RenderTargetHandle::from_raw(src_rt),
                            dse::render::RenderTargetHandle::from_raw(dst_rt));
}

unsigned int EditorRenderTargetColorTexture(unsigned int rt) {
    if (!g_rhi || rt == 0) return 0;
    return g_rhi->GetRenderTargetColorTexture(
        dse::render::RenderTargetHandle::from_raw(rt)).raw();
}

void EditorDeleteBlitTarget(unsigned int rt) {
    if (rt == 0) return;
    if (g_rhi) {
        g_rhi->DeleteRenderTarget(dse::render::RenderTargetHandle::from_raw(rt));
    }
}

}  // namespace dse::editor
