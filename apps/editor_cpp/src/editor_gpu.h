#pragma once

// 编辑器侧 GPU 资源访问 —— 走引擎 RHI（render/rhi/RhiDevice），不再直接调用 OpenGL。
//
// 编辑器的主场景渲染早已经由引擎 RHI 完成（pipeline()->GetSceneTextureId()）；本模块把
// 编辑器自己创建的零散 GPU 资源（资产缩略图等）也收敛到 RHI 抽象上，作为编辑器“走 RHI”的
// 统一接入点。后端无关：在 OpenGL 后端下返回的句柄即 GL 纹理 id，可直接喂给 ImGui::Image。
//
// 用法：引擎初始化完成后，由 editor_app 调用一次 SetEditorRhiDevice(...) 注入设备；其余编辑器
// 代码通过 EditorCreateTexture2D / EditorDeleteTexture 申请/释放纹理。

#include <cstdint>

namespace dse::render { class RhiDevice; }

namespace dse::editor {

/// 注入引擎 RHI 设备（生命周期由引擎持有；编辑器仅借用，关闭时置空）。
void SetEditorRhiDevice(dse::render::RhiDevice* device);

/// 当前编辑器使用的 RHI 设备，未注入时为 nullptr。
dse::render::RhiDevice* EditorRhi();

/// 经 RHI 创建一张 RGBA8 2D 纹理；linear=线性过滤，clamp=边缘钳制（否则平铺）。
/// 返回纹理句柄（OpenGL 后端即 GL 纹理 id）；设备未就绪返回 0。
unsigned int EditorCreateTexture2D(int width, int height, const uint8_t* rgba8,
                                   bool linear, bool clamp);

/// 经 RHI 释放 EditorCreateTexture2D 返回的纹理句柄。
void EditorDeleteTexture(unsigned int handle);

}  // namespace dse::editor
