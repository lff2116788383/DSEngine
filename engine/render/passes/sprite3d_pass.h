/**
 * @file sprite3d_pass.h
 * @brief HD-2D Sprite3D pass: extract/sort/issue the 3D billboard sprites.
 *
 * This pass is invoked by Gameplay2DModule::RenderScene2D immediately before
 * the original 2D sprite renderer, which in turn is invoked from
 * ForwardScenePass after all 3D opaque renderers. That preserves the M1
 * ordering requirement without splitting the existing scene render pass.
 */

#ifndef DSE_RENDER_SPRITE3D_PASS_H
#define DSE_RENDER_SPRITE3D_PASS_H

#include <cstddef>
#include <vector>

#include "engine/render/frame_context.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/sprite_batch_renderer.h"

class World;

namespace dse {
namespace render {

class RhiDevice;
class CommandBuffer;

class Sprite3DPass {
public:
    void SetRhiDevice(RhiDevice* device) { rhi_device_ = device; }

    /// Phase 1 (main thread): extract ECS Sprite3D components into SpriteDrawItem
    /// and sort by (depth_bucket, texture, blend); smaller bucket/bias is in front.
    void ExtractFrameRenderData(World& world);

    /// Phase 2 (render thread): issue the sorted Sprite3D batch through the
    /// shared SpriteBatchRenderer 3D path.
    void Render(CommandBuffer& cmd, const FrameContext& frame);

    void Shutdown();

    std::size_t sprite_count() const { return frame_items_.size(); }

private:
    RhiDevice* rhi_device_ = nullptr;
    SpriteBatchRenderer batch_;
    std::vector<SpriteDrawItem> frame_items_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_SPRITE3D_PASS_H
