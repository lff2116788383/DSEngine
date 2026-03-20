#include "phase1/systems/sprite_render_system.h"
#include <algorithm>

void SpriteRenderSystem::Render(Phase1World& world, CommandBuffer& cmd_buffer) {
    std::vector<Phase1SpriteDrawItem> items;
    auto view = world.registry().view<SpriteRendererComponent, TransformComponent>();
    items.reserve(view.size_hint());
    
    for (auto entity : view) {
        auto& sprite = view.get<SpriteRendererComponent>(entity);
        if (!sprite.visible) {
            continue;
        }
        auto& transform = view.get<TransformComponent>(entity);
        
        Phase1SpriteDrawItem item;
        item.texture_handle = sprite.texture_handle;
        item.model = transform.local_to_world;
        item.color = sprite.color;
        item.uv = sprite.uv;
        item.sorting_layer = sprite.sorting_layer;
        item.order_in_layer = sprite.order_in_layer;
        items.push_back(item);
    }
    
    std::sort(items.begin(), items.end(), [](const Phase1SpriteDrawItem& a, const Phase1SpriteDrawItem& b) {
        if (a.sorting_layer != b.sorting_layer) {
            return a.sorting_layer < b.sorting_layer;
        }
        return a.order_in_layer < b.order_in_layer;
    });
    cmd_buffer.DrawSpriteBatch(items);
}
