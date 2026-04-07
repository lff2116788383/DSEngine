/**
 * @file spine_system.cpp
 * @brief Spine 2D 系统实现
 */

#include "spine_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
#include <spine/spine.h>
#include <spine/Extension.h>
#include <stdexcept>

using namespace spine;

namespace dse {
namespace gameplay2d {

namespace {
AssetManager& RequireAssetManager(AssetManager* asset_manager) {
    if (asset_manager) {
        return *asset_manager;
    }
    throw std::runtime_error("SpineSystem requires an injected AssetManager");
}
}

class EngineTextureLoader : public TextureLoader {
public:
    std::vector<std::shared_ptr<TextureAsset>>* current_textures = nullptr;
    AssetManager* asset_manager = nullptr;

    void load(AtlasPage& page, const String& path) override {
        if (!current_textures) return;
        auto tex = RequireAssetManager(asset_manager).LoadTexture(path.buffer());
        if (tex) {
            page.texture = (void*)(uintptr_t)tex->GetHandle();
            page.width = tex->GetWidth();
            page.height = tex->GetHeight();
            current_textures->push_back(tex);
        } else {
            DEBUG_LOG_ERROR("Spine failed to load texture: {}", path.buffer());
        }
    }

    void unload(void* texture) override {
        // Handled by shared_ptr
    }
};

SpineSystem::~SpineSystem() {
}

void SpineSystem::CleanupComponent(SpineRendererComponent& comp) {
    DEBUG_LOG_INFO("[SpineCleanup] begin comp={} anim_state={} anim_state_data={} skeleton={} skeleton_data={} atlas={} textures={}",
                   static_cast<const void*>(&comp),
                   comp.animation_state,
                   comp.animation_state_data,
                   comp.skeleton,
                   comp.skeleton_data,
                   comp.atlas,
                   comp.textures.size());

    if (comp.animation_state) {
        DEBUG_LOG_INFO("[SpineCleanup] deleting AnimationState ptr={}", comp.animation_state);
        delete static_cast<spine::AnimationState*>(comp.animation_state);
        comp.animation_state = nullptr;
        DEBUG_LOG_INFO("[SpineCleanup] deleted AnimationState");
    }
    if (comp.animation_state_data) {
        DEBUG_LOG_INFO("[SpineCleanup] deleting AnimationStateData ptr={}", comp.animation_state_data);
        delete static_cast<spine::AnimationStateData*>(comp.animation_state_data);
        comp.animation_state_data = nullptr;
        DEBUG_LOG_INFO("[SpineCleanup] deleted AnimationStateData");
    }

    DEBUG_LOG_INFO("[SpineCleanup] skip deleting Skeleton/SkeletonData/Atlas in diagnostic mode to isolate heap corruption path");
    comp.skeleton = nullptr;
    comp.skeleton_data = nullptr;
    comp.atlas = nullptr;
    comp.textures.clear();
    comp.dirty_animation = false;
    DEBUG_LOG_INFO("[SpineCleanup] end comp={}", static_cast<const void*>(&comp));
}

void SpineSystem::Shutdown(entt::registry& registry) {
    auto view = registry.view<SpineRendererComponent>();
    for (auto entity : view) {
        auto& comp = view.get<SpineRendererComponent>(entity);
        CleanupComponent(comp);
    }
}

void SpineSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

void SpineSystem::Update(entt::registry& registry, float dt) {
    auto view = registry.view<SpineRendererComponent>();
    for (auto entity : view) {
        auto& comp = view.get<SpineRendererComponent>(entity);

        const bool needs_spine_assets = !comp.skeleton_data && !comp.skeleton_data_path.empty() && !comp.atlas_path.empty();
        if (needs_spine_assets) {
            auto& asset_manager = RequireAssetManager(asset_manager_);
            EngineTextureLoader texture_loader;
            texture_loader.current_textures = &comp.textures;
            texture_loader.asset_manager = &asset_manager;

            // Load atlas
            std::vector<uint8_t> atlas_data;
            if (asset_manager.LoadFileToMemory(comp.atlas_path, atlas_data)) {
                String atlas_str((const char*)atlas_data.data(), atlas_data.size());
                Atlas* atlas = new Atlas(atlas_str, &texture_loader, true);
                comp.atlas = atlas;

                std::vector<uint8_t> skel_data;
                if (asset_manager.LoadFileToMemory(comp.skeleton_data_path, skel_data)) {
                    bool is_binary = comp.skeleton_data_path.find(".skel") != std::string::npos;
                    SkeletonData* skeletonData = nullptr;
                    if (is_binary) {
                        SkeletonBinary binary(atlas);
                        skeletonData = binary.readSkeletonData((const unsigned char*)skel_data.data(), skel_data.size());
                    } else {
                        SkeletonJson json(atlas);
                        skeletonData = json.readSkeletonData((const char*)skel_data.data());
                    }

                    if (skeletonData) {
                        comp.skeleton_data = skeletonData;
                        comp.skeleton = new spine::Skeleton(skeletonData);
                        spine::AnimationStateData* stateData = new spine::AnimationStateData(skeletonData);
                        comp.animation_state_data = stateData;
                        comp.animation_state = new spine::AnimationState(stateData);
                    } else {
                        DEBUG_LOG_ERROR("Failed to read Spine skeleton data: {}", comp.skeleton_data_path);
                        if (comp.atlas) {
                            delete static_cast<spine::Atlas*>(comp.atlas);
                            comp.atlas = nullptr;
                        }
                        comp.textures.clear();
                    }
                }
            }
        }

        if (comp.animation_state && comp.skeleton) {
            spine::AnimationState* state = static_cast<spine::AnimationState*>(comp.animation_state);
            spine::Skeleton* skeleton = static_cast<spine::Skeleton*>(comp.skeleton);

            if (comp.dirty_animation && !comp.current_animation.empty()) {
                state->setAnimation(0, String(comp.current_animation.c_str()), comp.loop);
                comp.dirty_animation = false;
            }

            state->update(dt * comp.time_scale);
            state->apply(*skeleton);
            skeleton->updateWorldTransform(spine::Physics_Update);
        }
    }
}

void SpineSystem::Render(World& world, CommandBuffer& cmd_buffer) {
    auto view = world.registry().view<TransformComponent, SpineRendererComponent>();
    std::vector<MeshDrawItem> batch_items;

    for (auto entity : view) {
        auto [transform, comp] = view.get<TransformComponent, SpineRendererComponent>(entity);
        if (!comp.visible || !comp.skeleton) continue;

        spine::Skeleton* skeleton = static_cast<spine::Skeleton*>(comp.skeleton);

        for (int i = 0; i < skeleton->getSlots().size(); ++i) {
            Slot* slot = skeleton->getDrawOrder()[i];
            Attachment* attachment = slot->getAttachment();
            if (!attachment) continue;

            MeshDrawItem item;
            item.model = transform.local_to_world;
            item.sorting_layer = comp.sorting_layer;
            item.order_in_layer = comp.order_in_layer;
            item.blend_mode = 0;

            if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
                RegionAttachment* region = (RegionAttachment*)attachment;
                auto* page = (AtlasPage*)((AtlasRegion*)region->getRegion())->page;
                if (page) item.texture_handle = (unsigned int)(uintptr_t)page->texture;

                item.vertices.resize(4);
                item.indices = {0, 1, 2, 2, 3, 0};

                float vertices[8];
                region->computeWorldVertices(*slot, vertices, 0, 2);
                for (int v = 0; v < 4; ++v) {
                    item.vertices[v].pos = glm::vec3(vertices[v*2], vertices[v*2+1], 0.0f);
                    item.vertices[v].color = glm::vec4(skeleton->getColor().r * slot->getColor().r * region->getColor().r,
                                                       skeleton->getColor().g * slot->getColor().g * region->getColor().g,
                                                       skeleton->getColor().b * slot->getColor().b * region->getColor().b,
                                                       skeleton->getColor().a * slot->getColor().a * region->getColor().a);
                    item.vertices[v].uv = glm::vec2(region->getUVs()[v*2], region->getUVs()[v*2+1]);
                }
                batch_items.push_back(item);
            } else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
                MeshAttachment* mesh = (MeshAttachment*)attachment;
                auto* page = (AtlasPage*)((AtlasRegion*)mesh->getRegion())->page;
                if (page) item.texture_handle = (unsigned int)(uintptr_t)page->texture;

                size_t num_vertices = mesh->getWorldVerticesLength() / 2;
                item.vertices.resize(num_vertices);
                std::vector<float> vertices(mesh->getWorldVerticesLength());
                mesh->computeWorldVertices(*slot, 0, mesh->getWorldVerticesLength(), vertices.data(), 0, 2);

                for (size_t v = 0; v < num_vertices; ++v) {
                    item.vertices[v].pos = glm::vec3(vertices[v*2], vertices[v*2+1], 0.0f);
                    item.vertices[v].color = glm::vec4(skeleton->getColor().r * slot->getColor().r * mesh->getColor().r,
                                                       skeleton->getColor().g * slot->getColor().g * mesh->getColor().g,
                                                       skeleton->getColor().b * slot->getColor().b * mesh->getColor().b,
                                                       skeleton->getColor().a * slot->getColor().a * mesh->getColor().a);
                    item.vertices[v].uv = glm::vec2(mesh->getUVs()[v*2], mesh->getUVs()[v*2+1]);
                }
                
                auto& indices = mesh->getTriangles();
                item.indices.assign(indices.buffer(), indices.buffer() + indices.size());
                
                batch_items.push_back(item);
            }
        }
    }

    if (!batch_items.empty()) {
        cmd_buffer.DrawMeshBatch(batch_items);
    }
}

} // namespace gameplay2d
} // namespace dse
