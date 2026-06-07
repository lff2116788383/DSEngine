/**
 * @file scene_manager.cpp
 * @brief 场景管理器实现
 */

#include "engine/scene/scene_manager.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/base/debug.h"
#include <fstream>
#include <sstream>

namespace scene {

SceneManager::SceneManager() = default;

SceneManager::~SceneManager() {
    UnloadAll();
}

void SceneManager::SetWorld(World* world) { world_ = world; }
void SceneManager::SetAssetManager(AssetManager* asset_manager) { asset_manager_ = asset_manager; }
void SceneManager::SetEventBus(dse::core::EventBus* event_bus) { event_bus_ = event_bus; }
void SceneManager::SetJobSystem(dse::core::JobSystem* job_system) { job_system_ = job_system; }

void SceneManager::LoadSubSceneAsync(const std::string& path) {
    if (!world_ || !asset_manager_) {
        DEBUG_LOG_ERROR("SceneManager::LoadSubSceneAsync: world or asset_manager not set");
        return;
    }
    if (sub_scenes_.find(path) != sub_scenes_.end()) {
        DEBUG_LOG_WARN("SceneManager::LoadSubSceneAsync: {} already loaded", path);
        return;
    }
    if (loading_paths_.find(path) != loading_paths_.end()) {
        DEBUG_LOG_WARN("SceneManager::LoadSubSceneAsync: {} already loading", path);
        return;
    }

    // 如果有 JobSystem，在工作线程读取文件
    if (job_system_) {
        loading_paths_.insert(path);
        auto shared_path = std::make_shared<std::string>(path);
        auto shared_pending = std::make_shared<PendingLoad>();
        shared_pending->path = path;

        job_system_->Submit([shared_path, shared_pending, this]() {
            std::ifstream in(*shared_path);
            if (in.is_open()) {
                std::stringstream buf;
                buf << in.rdbuf();
                shared_pending->json_data = buf.str();
                shared_pending->success = true;
            } else {
                shared_pending->success = false;
            }
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_loads_.push_back(std::move(*shared_pending));
        });
    } else {
        // 回退：同步加载
        LoadSubScene(path);
    }
}

bool SceneManager::LoadSubScene(const std::string& path) {
    if (!world_ || !asset_manager_) {
        return false;
    }
    if (sub_scenes_.find(path) != sub_scenes_.end()) {
        return false;
    }

    auto sub = std::make_unique<SubScene>();
    if (!sub->Load(*world_, *asset_manager_, path)) {
        return false;
    }

    auto& loaded_sub = *(sub_scenes_[path] = std::move(sub));
    IndexSubSceneUuids(loaded_sub);
    WarmMeshes(loaded_sub);

    if (event_bus_) {
        event_bus_->Publish<dse::core::SubSceneLoadedEvent>(path);
    }
    return true;
}

void SceneManager::UnloadSubScene(const std::string& path) {
    auto it = sub_scenes_.find(path);
    if (it == sub_scenes_.end()) {
        return;
    }
    RemoveSubSceneUuids(*it->second);
    if (world_) {
        it->second->Unload(*world_);
    }
    sub_scenes_.erase(it);

    if (event_bus_) {
        event_bus_->Publish<dse::core::SubSceneUnloadedEvent>(path);
    }
}

void SceneManager::UnloadAll() {
    if (world_) {
        for (auto& pair : sub_scenes_) {
            pair.second->Unload(*world_);
        }
    }
    sub_scenes_.clear();
    uuid_index_.clear();
}

void SceneManager::Update(float dt) {
    // Phase 3: 处理场景切换状态机
    if (transition_state_ != TransitionState::Idle) {
        UpdateTransition(dt);
    }

    std::vector<PendingLoad> completed;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        completed.swap(pending_loads_);
    }

    for (auto& pending : completed) {
        loading_paths_.erase(pending.path);

        if (!pending.success || pending.json_data.empty()) {
            DEBUG_LOG_ERROR("SceneManager::Update: async load failed for {}", pending.path);
            if (event_bus_) {
                event_bus_->Publish<dse::core::SubSceneLoadFailedEvent>(pending.path);
            }
            continue;
        }
        if (sub_scenes_.find(pending.path) != sub_scenes_.end()) {
            continue;
        }

        auto sub = std::make_unique<SubScene>();
        if (sub->LoadFromJson(*world_, *asset_manager_, pending.json_data, pending.path)) {
            auto& loaded_sub = *(sub_scenes_[pending.path] = std::move(sub));
            IndexSubSceneUuids(loaded_sub);
            WarmMeshes(loaded_sub);
            if (event_bus_) {
                event_bus_->Publish<dse::core::SubSceneLoadedEvent>(pending.path);
            }
        } else {
            DEBUG_LOG_ERROR("SceneManager::Update: deserialize failed for {}", pending.path);
            if (event_bus_) {
                event_bus_->Publish<dse::core::SubSceneLoadFailedEvent>(pending.path);
            }
        }
    }
}

std::vector<std::string> SceneManager::GetLoadedSubScenes() const {
    std::vector<std::string> result;
    result.reserve(sub_scenes_.size());
    for (const auto& pair : sub_scenes_) {
        result.push_back(pair.first);
    }
    return result;
}

bool SceneManager::IsSubSceneLoaded(const std::string& path) const {
    return sub_scenes_.find(path) != sub_scenes_.end();
}

size_t SceneManager::LoadedCount() const {
    return sub_scenes_.size();
}

size_t SceneManager::PendingCount() const {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_loads_.size();
}

const SubScene* SceneManager::GetSubScene(const std::string& path) const {
    auto it = sub_scenes_.find(path);
    return it != sub_scenes_.end() ? it->second.get() : nullptr;
}

// ========== Phase 4: 跨场景 Entity 引用（哈希索引） ==========

Entity SceneManager::ResolveReference(uint64_t uuid) const {
    if (uuid == 0) {
        return entt::null;
    }
    auto it = uuid_index_.find(uuid);
    if (it != uuid_index_.end()) {
        if (world_ && world_->registry().valid(it->second)) {
            return it->second;
        }
    }
    return entt::null;
}

// ========== UUID 索引管理 ==========

void SceneManager::IndexSubSceneUuids(const SubScene& sub) {
    if (!world_) return;
    auto& reg = world_->registry();
    for (auto entity : sub.GetEntities()) {
        if (reg.valid(entity) && reg.all_of<UUIDComponent>(entity)) {
            uint64_t uuid = reg.get<UUIDComponent>(entity).uuid;
            if (uuid != 0) {
                uuid_index_[uuid] = entity;
            }
        }
    }
}

void SceneManager::RemoveSubSceneUuids(const SubScene& sub) {
    if (!world_) return;
    auto& reg = world_->registry();
    for (auto entity : sub.GetEntities()) {
        if (reg.valid(entity) && reg.all_of<UUIDComponent>(entity)) {
            uint64_t uuid = reg.get<UUIDComponent>(entity).uuid;
            if (uuid != 0) {
                uuid_index_.erase(uuid);
            }
        }
    }
}

// ========== Mesh 异步预热 ==========

void SceneManager::WarmMeshes(const SubScene& sub) {
    if (!world_ || !asset_manager_) return;
    auto& reg = world_->registry();
    for (auto entity : sub.GetEntities()) {
        if (!reg.valid(entity)) continue;
        if (!reg.all_of<dse::MeshRendererComponent>(entity)) continue;
        const auto& mr = reg.get<dse::MeshRendererComponent>(entity);
        if (mr.mesh_path.empty()) continue;
        asset_manager_->LoadDmeshAsync(mr.mesh_path, [](std::shared_ptr<DmeshAsset>) {
            // 预热完成，资源已进入 AssetManager 缓存
        });
    }
}

// ========== Phase 3: 场景切换 ==========

void SceneManager::TransitionTo(const std::string& path, TransitionMode mode, float fade_duration) {
    if (transition_state_ != TransitionState::Idle) {
        DEBUG_LOG_WARN("SceneManager::TransitionTo: transition already in progress");
        return;
    }

    pending_transition_path_ = path;
    pending_transition_mode_ = mode;
    fade_duration_ = fade_duration;

    switch (mode) {
    case TransitionMode::Instant: {
        // 卸载旧场景，加载新场景
        std::string old_path = active_scene_path_;
        if (!old_path.empty()) {
            UnloadSubScene(old_path);
        }
        LoadSubScene(path);
        active_scene_path_ = path;
        break;
    }
    case TransitionMode::Additive: {
        // 叠加加载，不卸载旧场景
        LoadSubScene(path);
        active_scene_path_ = path;
        break;
    }
    case TransitionMode::Fade: {
        // 开始淡出
        previous_scene_path_ = active_scene_path_;
        transition_state_ = TransitionState::FadingOut;
        fade_timer_ = 0.0f;
        fade_progress_ = 0.0f;
        break;
    }
    }
}

void SceneManager::UpdateTransition(float dt) {
    switch (transition_state_) {
    case TransitionState::FadingOut: {
        fade_timer_ += dt;
        fade_progress_ = (fade_duration_ > 0.0f) ? std::min(fade_timer_ / fade_duration_, 1.0f) : 1.0f;
        if (fade_progress_ >= 1.0f) {
            // 淡出完成 → 卸载旧场景 → 加载新场景
            if (!previous_scene_path_.empty()) {
                UnloadSubScene(previous_scene_path_);
            }
            transition_state_ = TransitionState::Loading;
            fade_timer_ = 0.0f;
            fade_progress_ = 0.0f;
        }
        break;
    }
    case TransitionState::Loading: {
        LoadSubScene(pending_transition_path_);
        active_scene_path_ = pending_transition_path_;
        // 加载完成 → 开始淡入
        transition_state_ = TransitionState::FadingIn;
        fade_timer_ = 0.0f;
        fade_progress_ = 0.0f;
        break;
    }
    case TransitionState::FadingIn: {
        fade_timer_ += dt;
        fade_progress_ = (fade_duration_ > 0.0f) ? std::min(fade_timer_ / fade_duration_, 1.0f) : 1.0f;
        if (fade_progress_ >= 1.0f) {
            // 淡入完成
            transition_state_ = TransitionState::Idle;
            fade_progress_ = 0.0f;
            fade_timer_ = 0.0f;
            pending_transition_path_.clear();
            previous_scene_path_.clear();
        }
        break;
    }
    default:
        break;
    }
}

} // namespace scene
