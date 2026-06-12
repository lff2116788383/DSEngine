#include "editor_gpu.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"

namespace dse::editor {

namespace {
dse::render::RhiDevice* g_rhi = nullptr;
}  // namespace

void SetEditorRhiDevice(dse::render::RhiDevice* device) { g_rhi = device; }

dse::render::RhiDevice* EditorRhi() { return g_rhi; }

unsigned int EditorCreateTexture2D(int width, int height, const uint8_t* rgba8,
                                   bool linear, bool clamp) {
    if (!g_rhi || width <= 0 || height <= 0) return 0;
    // RHI 共享类型（rhi_types.h）定义在全局命名空间。
    TextureSamplerDesc sampler;
    sampler.filter = linear ? TextureFilter::Linear : TextureFilter::Nearest;
    sampler.wrap = clamp ? TextureWrap::ClampToEdge : TextureWrap::Repeat;
    return g_rhi->CreateTexture2D(width, height, rgba8, sampler);
}

void EditorDeleteTexture(unsigned int handle) {
    if (g_rhi && handle != 0) g_rhi->DeleteTexture(handle);
}

}  // namespace dse::editor
