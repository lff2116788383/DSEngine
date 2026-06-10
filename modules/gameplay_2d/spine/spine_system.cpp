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
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <cstdio>

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

struct AtlasDeleter {
    void operator()(spine::Atlas* atlas) const {
        delete atlas;
    }
};

struct SkeletonDataDeleter {
    void operator()(spine::SkeletonData* skeleton_data) const {
        delete skeleton_data;
    }
};

struct SkeletonDeleter {
    void operator()(spine::Skeleton* skeleton) const {
        delete skeleton;
    }
};

struct AnimationStateDataDeleter {
    void operator()(spine::AnimationStateData* state_data) const {
        delete state_data;
    }
};

struct AnimationStateDeleter {
    void operator()(spine::AnimationState* animation_state) const {
        delete animation_state;
    }
};

using AtlasPtr = std::unique_ptr<spine::Atlas, AtlasDeleter>;
using SkeletonDataPtr = std::unique_ptr<spine::SkeletonData, SkeletonDataDeleter>;
using SkeletonPtr = std::unique_ptr<spine::Skeleton, SkeletonDeleter>;
using AnimationStateDataPtr = std::unique_ptr<spine::AnimationStateData, AnimationStateDataDeleter>;
using AnimationStatePtr = std::unique_ptr<spine::AnimationState, AnimationStateDeleter>;

struct SpineRuntimeHandle final : SpineRendererComponent::RuntimeHandle {
    AtlasPtr atlas;
    SkeletonDataPtr skeleton_data;
    SkeletonPtr skeleton;
    AnimationStateDataPtr animation_state_data;
    AnimationStatePtr animation_state;

    void ResetWithDiagnostics(const char* reason) {
        DEBUG_LOG_INFO("[SpineReset] begin reason={} runtime={} state={} state_data={} skeleton={} skeleton_data={} atlas={}",
                       reason ? reason : "(null)",
                       static_cast<const void*>(this),
                       static_cast<const void*>(animation_state.get()),
                       static_cast<const void*>(animation_state_data.get()),
                       static_cast<const void*>(skeleton.get()),
                       static_cast<const void*>(skeleton_data.get()),
                       static_cast<const void*>(atlas.get()));

        DEBUG_LOG_INFO("[SpineReset] release animation_state runtime={} ptr={}",
                       static_cast<const void*>(this),
                       static_cast<const void*>(animation_state.get()));
        animation_state.reset();
        DEBUG_LOG_INFO("[SpineReset] released animation_state runtime={}", static_cast<const void*>(this));

        DEBUG_LOG_INFO("[SpineReset] release skeleton runtime={} ptr={}",
                       static_cast<const void*>(this),
                       static_cast<const void*>(skeleton.get()));
        skeleton.reset();
        DEBUG_LOG_INFO("[SpineReset] released skeleton runtime={}", static_cast<const void*>(this));

        DEBUG_LOG_INFO("[SpineReset] release animation_state_data runtime={} ptr={}",
                       static_cast<const void*>(this),
                       static_cast<const void*>(animation_state_data.get()));
        animation_state_data.reset();
        DEBUG_LOG_INFO("[SpineReset] released animation_state_data runtime={}", static_cast<const void*>(this));

        DEBUG_LOG_INFO("[SpineReset] release skeleton_data runtime={} ptr={}",
                       static_cast<const void*>(this),
                       static_cast<const void*>(skeleton_data.get()));
        skeleton_data.reset();
        DEBUG_LOG_INFO("[SpineReset] released skeleton_data runtime={}", static_cast<const void*>(this));

        DEBUG_LOG_INFO("[SpineReset] release atlas runtime={} ptr={}",
                       static_cast<const void*>(this),
                       static_cast<const void*>(atlas.get()));
        atlas.reset();
        DEBUG_LOG_INFO("[SpineReset] released atlas runtime={} end", static_cast<const void*>(this));
    }

    void Reset() {
        ResetWithDiagnostics("unspecified");
    }

    ~SpineRuntimeHandle() override {
        ResetWithDiagnostics("SpineRuntimeHandle::~SpineRuntimeHandle");
    }
};

using RuntimeHandlePtr = std::shared_ptr<SpineRuntimeHandle>;

class EngineTextureLoader : public TextureLoader {
public:
    std::vector<std::shared_ptr<TextureAsset>>* current_textures = nullptr;
    AssetManager* asset_manager = nullptr;

    void load(AtlasPage& page, const String& path) override {
        if (!current_textures) {
            return;
        }

        auto tex = RequireAssetManager(asset_manager).LoadTexture(path.buffer());
        if (tex) {
            page.texture = reinterpret_cast<void*>(static_cast<uintptr_t>(tex->GetHandle()));
            page.width = tex->GetWidth();
            page.height = tex->GetHeight();
            current_textures->push_back(tex);
        } else {
            DEBUG_LOG_ERROR("Spine failed to load texture: {}", path.buffer());
        }
    }

    void unload(void* texture) override {
        (void)texture;
        // 纹理由 shared_ptr 生命周期托管。
    }
};

SpineRuntimeHandle* GetRuntime(const SpineRendererComponent& comp) {
    return static_cast<SpineRuntimeHandle*>(comp.runtime.get());
}

RuntimeHandlePtr BuildRuntime(SpineRendererComponent& comp, AssetManager& asset_manager) {
    auto runtime = std::make_shared<SpineRuntimeHandle>();

    EngineTextureLoader texture_loader;
    texture_loader.current_textures = &comp.textures;
    texture_loader.asset_manager = &asset_manager;

    std::vector<uint8_t> atlas_data;
    if (!asset_manager.LoadFileToMemory(comp.atlas_path, atlas_data)) {
        DEBUG_LOG_WARN("[SpineUpdate] atlas file missing: {}", comp.atlas_path);
        return runtime;
    }

    String atlas_str(reinterpret_cast<const char*>(atlas_data.data()), atlas_data.size());
    runtime->atlas.reset(new spine::Atlas(atlas_str, &texture_loader, true));

    std::vector<uint8_t> skeleton_file_data;
    if (!asset_manager.LoadFileToMemory(comp.skeleton_data_path, skeleton_file_data)) {
        DEBUG_LOG_WARN("[SpineUpdate] skeleton file missing: {}", comp.skeleton_data_path);
        runtime->ResetWithDiagnostics("BuildRuntime missing skeleton file");
        comp.textures.clear();
        return runtime;
    }

    const bool is_binary = comp.skeleton_data_path.find(".skel") != std::string::npos;
    if (is_binary) {
        SkeletonBinary binary(runtime->atlas.get());
        runtime->skeleton_data.reset(binary.readSkeletonData(
            reinterpret_cast<const unsigned char*>(skeleton_file_data.data()),
            static_cast<int>(skeleton_file_data.size())));
    } else {
        SkeletonJson json(runtime->atlas.get());
        runtime->skeleton_data.reset(json.readSkeletonData(
            reinterpret_cast<const char*>(skeleton_file_data.data())));
    }

    if (!runtime->skeleton_data) {
        DEBUG_LOG_ERROR("[SpineUpdate] failed to parse skeleton data: {}", comp.skeleton_data_path);
        runtime->ResetWithDiagnostics("BuildRuntime parse skeleton failed");
        comp.textures.clear();
        return runtime;
    }

    runtime->skeleton.reset(new spine::Skeleton(runtime->skeleton_data.get()));
    runtime->animation_state_data.reset(new spine::AnimationStateData(runtime->skeleton_data.get()));
    runtime->animation_state.reset(new spine::AnimationState(runtime->animation_state_data.get()));
    return runtime;
}

} // namespace

SpineSystem::~SpineSystem() {
    DEBUG_LOG_TRACE("[spine-system] ~SpineSystem this={} asset_manager={}",
                static_cast<void*>(this), static_cast<void*>(asset_manager_));
}

void SpineSystem::CleanupComponent(SpineRendererComponent& comp) {
    auto* runtime = GetRuntime(comp);
    DEBUG_LOG_INFO("[SpineCleanup] begin comp={} runtime={} anim_state={} anim_state_data={} skeleton={} skeleton_data={} atlas={} textures={}",
                   static_cast<const void*>(&comp),
                   static_cast<const void*>(runtime),
                   runtime ? static_cast<const void*>(runtime->animation_state.get()) : nullptr,
                   runtime ? static_cast<const void*>(runtime->animation_state_data.get()) : nullptr,
                   runtime ? static_cast<const void*>(runtime->skeleton.get()) : nullptr,
                   runtime ? static_cast<const void*>(runtime->skeleton_data.get()) : nullptr,
                   runtime ? static_cast<const void*>(runtime->atlas.get()) : nullptr,
                   comp.textures.size());

    DEBUG_LOG_INFO("[SpineCleanup] release comp.runtime comp={} runtime={}",
                   static_cast<const void*>(&comp),
                   static_cast<const void*>(runtime));
    comp.runtime.reset();
    DEBUG_LOG_INFO("[SpineCleanup] released comp.runtime comp={}", static_cast<const void*>(&comp));
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
    DEBUG_LOG_TRACE("[spine-update] begin this={} dt={}", static_cast<void*>(this), static_cast<double>(dt));
    auto view = registry.view<SpineRendererComponent>();
    for (auto entity : view) {
        auto& comp = view.get<SpineRendererComponent>(entity);

        const bool has_complete_paths = !comp.skeleton_data_path.empty() && !comp.atlas_path.empty();
        auto* runtime = GetRuntime(comp);
        const bool has_runtime = runtime && runtime->animation_state && runtime->skeleton;
        const bool needs_spine_assets = has_complete_paths && !has_runtime;
        DEBUG_LOG_TRACE("[spine-update] entity={} has_paths={} has_runtime={} needs_assets={} anim={} dirty={} visible={}",
                    static_cast<unsigned>(entity), has_complete_paths ? 1 : 0, has_runtime ? 1 : 0,
                    needs_spine_assets ? 1 : 0, comp.current_animation, comp.dirty_animation ? 1 : 0, comp.visible ? 1 : 0);

        if (needs_spine_assets) {
            auto& asset_manager = RequireAssetManager(asset_manager_);
            CleanupComponent(comp);
            comp.dirty_animation = !comp.current_animation.empty();

            try {
                RuntimeHandlePtr new_runtime = BuildRuntime(comp, asset_manager);
                if (new_runtime->animation_state && new_runtime->skeleton) {
                    comp.runtime = std::move(new_runtime);
                    runtime = GetRuntime(comp);
                } else {
                    comp.runtime.reset();
                }
            } catch (const std::exception&) {
                comp.runtime.reset();
                comp.textures.clear();
                continue;
            } catch (...) {
                comp.runtime.reset();
                comp.textures.clear();
                continue;
            }
        }

        runtime = GetRuntime(comp);
        DEBUG_LOG_TRACE("[spine-update] post-build entity={} runtime={}",
                    static_cast<unsigned>(entity), static_cast<void*>(runtime));
    }
    DEBUG_LOG_TRACE("[spine-update] end this={}", static_cast<void*>(this));
}

void SpineSystem::Render(World& world, CommandBuffer& cmd_buffer) {
    auto view = world.registry().view<TransformComponent, SpineRendererComponent>();
    std::vector<MeshDrawItem> batch_items;

    for (auto entity : view) {
        auto [transform, comp] = view.get<TransformComponent, SpineRendererComponent>(entity);
        auto* runtime = GetRuntime(comp);
        if (!comp.visible || !runtime || !runtime->skeleton) {
            continue;
        }

        auto* skeleton = runtime->skeleton.get();

        for (int i = 0; i < skeleton->getSlots().size(); ++i) {
            Slot* slot = skeleton->getDrawOrder()[i];
            Attachment* attachment = slot->getAttachment();
            if (!attachment) {
                continue;
            }

            MeshDrawItem item;
            item.model = transform.local_to_world;
            item.sorting_layer = comp.sorting_layer;
            item.order_in_layer = comp.order_in_layer;
            item.blend_mode = 0;

            if (attachment->getRTTI().isExactly(RegionAttachment::rtti)) {
                auto* region = static_cast<RegionAttachment*>(attachment);
                auto* page = static_cast<AtlasPage*>(static_cast<AtlasRegion*>(region->getRegion())->page);
                if (page) {
                    item.texture_handle = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(page->texture));
                }

                item.vertices.resize(4);
                item.indices = {0, 1, 2, 2, 3, 0};

                float vertices[8];
                region->computeWorldVertices(*slot, vertices, 0, 2);
                for (int v = 0; v < 4; ++v) {
                    item.vertices[v].pos = glm::vec3(vertices[v * 2], vertices[v * 2 + 1], 0.0f);
                    item.vertices[v].color = glm::vec4(
                        skeleton->getColor().r * slot->getColor().r * region->getColor().r,
                        skeleton->getColor().g * slot->getColor().g * region->getColor().g,
                        skeleton->getColor().b * slot->getColor().b * region->getColor().b,
                        skeleton->getColor().a * slot->getColor().a * region->getColor().a);
                    item.vertices[v].uv = glm::vec2(region->getUVs()[v * 2], region->getUVs()[v * 2 + 1]);
                }
                batch_items.push_back(std::move(item));
            } else if (attachment->getRTTI().isExactly(MeshAttachment::rtti)) {
                auto* mesh = static_cast<MeshAttachment*>(attachment);
                auto* page = static_cast<AtlasPage*>(static_cast<AtlasRegion*>(mesh->getRegion())->page);
                if (page) {
                    item.texture_handle = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(page->texture));
                }

                const size_t num_vertices = mesh->getWorldVerticesLength() / 2;
                item.vertices.resize(num_vertices);
                std::vector<float> vertices(mesh->getWorldVerticesLength());
                mesh->computeWorldVertices(*slot, 0, mesh->getWorldVerticesLength(), vertices.data(), 0, 2);

                for (size_t v = 0; v < num_vertices; ++v) {
                    item.vertices[v].pos = glm::vec3(vertices[v * 2], vertices[v * 2 + 1], 0.0f);
                    item.vertices[v].color = glm::vec4(
                        skeleton->getColor().r * slot->getColor().r * mesh->getColor().r,
                        skeleton->getColor().g * slot->getColor().g * mesh->getColor().g,
                        skeleton->getColor().b * slot->getColor().b * mesh->getColor().b,
                        skeleton->getColor().a * slot->getColor().a * mesh->getColor().a);
                    item.vertices[v].uv = glm::vec2(mesh->getUVs()[v * 2], mesh->getUVs()[v * 2 + 1]);
                }

                auto& indices = mesh->getTriangles();
                item.indices.assign(indices.buffer(), indices.buffer() + indices.size());
                batch_items.push_back(std::move(item));
            }
        }
    }

    if (!batch_items.empty()) {
        cmd_buffer.DrawMeshBatch(batch_items);
    }
}

} // namespace gameplay2d
} // namespace dse
