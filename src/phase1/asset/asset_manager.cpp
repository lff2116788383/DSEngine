#include "phase1/asset/asset_manager.h"
#include "phase1/rhi/rhi_device.h"
#include "utils/debug.h"
#include "core/job_system.h"
#include <filesystem>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace {
std::string NormalizePath(const std::string& path) {
    std::filesystem::path p(path);
    return p.make_preferred().lexically_normal().string();
}

std::string ResolveTexturePath(const std::string& path, const std::string& data_root) {
    std::filesystem::path input(path);
    if (input.is_absolute()) {
        if (std::filesystem::exists(input)) {
            return NormalizePath(input.string());
        }
        return "";
    }

    std::filesystem::path normalized_input = input.lexically_normal();
    if (normalized_input.empty()) {
        return "";
    }
    if (std::filesystem::exists(normalized_input)) {
        return NormalizePath(normalized_input.string());
    }

    std::filesystem::path relative = normalized_input;
    const std::filesystem::path data_prefix = std::filesystem::path("data");
    const std::filesystem::path bin_data_prefix = std::filesystem::path("bin") / "data";
    auto starts_with = [](const std::filesystem::path& value, const std::filesystem::path& prefix) {
        auto it_value = value.begin();
        auto it_prefix = prefix.begin();
        for (; it_prefix != prefix.end(); ++it_prefix, ++it_value) {
            if (it_value == value.end() || *it_value != *it_prefix) {
                return false;
            }
        }
        return true;
    };
    if (starts_with(relative, data_prefix)) {
        relative = relative.lexically_relative(data_prefix);
    } else if (starts_with(relative, bin_data_prefix)) {
        relative = relative.lexically_relative(bin_data_prefix);
    }
    if (relative.empty()) {
        return "";
    }

    std::filesystem::path resolved = std::filesystem::path(data_root) / relative;
    if (std::filesystem::exists(resolved)) {
        return NormalizePath(resolved.string());
    }
    return "";
}

}

TextureAsset::TextureAsset(const std::string& path, unsigned int handle, int width, int height, int channels)
    : path_(path), handle_(handle), width_(width), height_(height), channels_(channels) {
}

TextureAsset::~TextureAsset() {
}

ShaderAsset::ShaderAsset(const std::string& name, unsigned int handle)
    : name_(name), handle_(handle) {
}

ShaderAsset::~ShaderAsset() {
}

Phase1MaterialAsset::Phase1MaterialAsset(unsigned int id, const std::string& name)
    : id_(id), name_(name) {
}

Phase1AssetManager& Phase1AssetManager::Instance() {
    static Phase1AssetManager instance;
    return instance;
}

void Phase1AssetManager::SetRhiDevice(RhiDevice* rhi_device) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    rhi_device_ = rhi_device;
}

void Phase1AssetManager::ConfigureDataRoot(const std::string& data_root) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (data_root.empty()) {
        return;
    }
    data_root_ = NormalizePath(data_root);
}

std::string Phase1AssetManager::GetDataRoot() {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return data_root_;
}

std::shared_ptr<TextureAsset> Phase1AssetManager::LoadTexture(const std::string& path) {
    const std::string data_root = GetDataRoot();
    const std::string resolved_path = ResolveTexturePath(path, data_root);
    const std::string cache_key = resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = textures_.find(cache_key);
        if (it != textures_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    unsigned char* data = stbi_load(load_path.c_str(), &width, &height, &channels, 4);
    
    if (!data) {
        DEBUG_LOG_ERROR("Failed to load texture: {}, resolved: {}", path, load_path);
        return nullptr;
    }

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTexture2D(width, height, data, true);
        }
    }
    if (handle == 0) {
        stbi_image_free(data);
        DEBUG_LOG_ERROR("Failed to create texture via RHI: {}", load_path);
        return nullptr;
    }

    stbi_image_free(data);

    auto tex = std::make_shared<TextureAsset>(load_path, handle, width, height, channels);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        textures_[cache_key] = tex;
    }
    return tex;
}

std::shared_ptr<ShaderAsset> Phase1AssetManager::LoadShader(const std::string& name, const std::string& vert_src, const std::string& frag_src) {
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = shaders_.find(name);
        if (it != shaders_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateShaderProgram(vert_src, frag_src);
        }
    }
    if (handle == 0) {
        DEBUG_LOG_ERROR("Failed to create shader via RHI: {}", name);
        return nullptr;
    }

    auto shader = std::make_shared<ShaderAsset>(name, handle);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        shaders_[name] = shader;
    }
    return shader;
}

void Phase1AssetManager::LoadTextureAsync(const std::string& path, std::function<void(std::shared_ptr<TextureAsset>)> callback) {
    const std::string data_root = GetDataRoot();
    const std::string resolved_path = ResolveTexturePath(path, data_root);
    const std::string cache_key = resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = textures_.find(cache_key);
        if (it != textures_.end()) {
            if (auto shared = it->second.lock()) {
                if (callback) callback(shared);
                return;
            }
        }
    }

    core::JobSystem::Execute([this, path, resolved_path, cache_key, callback]() {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(true);
        const std::string load_path = resolved_path.empty() ? path : resolved_path;
        unsigned char* data = stbi_load(load_path.c_str(), &width, &height, &channels, 4);
        
        if (!data) {
            DEBUG_LOG_ERROR("Failed to async load texture: {}, resolved: {}", path, load_path);
            if (callback) {
                std::lock_guard<std::mutex> callback_lock(callback_mutex_);
                pending_main_thread_callbacks_.push_back([callback]() {
                    callback(nullptr);
                });
                pending_callbacks_high_watermark_ = std::max(pending_callbacks_high_watermark_, pending_main_thread_callbacks_.size());
                if (!callback_backlog_warned_ && pending_main_thread_callbacks_.size() >= 1024) {
                    callback_backlog_warned_ = true;
                    DEBUG_LOG_WARN("Phase1 async callback backlog is high: {}", pending_main_thread_callbacks_.size());
                }
            }
            return;
        }

        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        pending_main_thread_callbacks_.push_back([this, callback, cache_key, load_path, width, height, channels, data]() {
            unsigned int handle = 0;
            {
                std::lock_guard<std::mutex> config_lock(config_mutex_);
                if (rhi_device_) {
                    handle = rhi_device_->CreateTexture2D(width, height, data, true);
                }
            }
            stbi_image_free(data);
            if (handle == 0) {
                if (callback) {
                    callback(nullptr);
                }
                return;
            }
            auto tex = std::make_shared<TextureAsset>(load_path, handle, width, height, channels);
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                textures_[cache_key] = tex;
            }
            if (callback) {
                callback(tex);
            }
        });
        pending_callbacks_high_watermark_ = std::max(pending_callbacks_high_watermark_, pending_main_thread_callbacks_.size());
        if (!callback_backlog_warned_ && pending_main_thread_callbacks_.size() >= 1024) {
            callback_backlog_warned_ = true;
            DEBUG_LOG_WARN("Phase1 async callback backlog is high: {}", pending_main_thread_callbacks_.size());
        }
    });
}

std::shared_ptr<Phase1MaterialAsset> Phase1AssetManager::CreateMaterialInstance(const std::string& name) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    unsigned int material_id = next_material_id_++;
    auto material = std::make_shared<Phase1MaterialAsset>(material_id, name);
    materials_[material_id] = material;
    return material;
}

std::shared_ptr<Phase1MaterialAsset> Phase1AssetManager::GetMaterialInstance(unsigned int material_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = materials_.find(material_id);
    if (it == materials_.end()) {
        return nullptr;
    }
    return it->second.lock();
}

std::vector<unsigned int> Phase1AssetManager::ListMaterialInstanceIds() {
    std::vector<unsigned int> ids;
    std::lock_guard<std::mutex> lock(cache_mutex_);
    ids.reserve(materials_.size());
    for (const auto& pair : materials_) {
        if (!pair.second.expired()) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}

void Phase1AssetManager::UnloadUnused() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    for (auto it = textures_.begin(); it != textures_.end(); ) {
        if (it->second.expired()) {
            it = textures_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = shaders_.begin(); it != shaders_.end(); ) {
        if (it->second.expired()) {
            it = shaders_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = materials_.begin(); it != materials_.end(); ) {
        if (it->second.expired()) {
            it = materials_.erase(it);
        } else {
            ++it;
        }
    }
}

void Phase1AssetManager::PumpMainThreadCallbacks(std::size_t max_callbacks) {
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        std::size_t consume_count = std::min(max_callbacks, pending_main_thread_callbacks_.size());
        callbacks.reserve(consume_count);
        for (std::size_t i = 0; i < consume_count; ++i) {
            callbacks.push_back(std::move(pending_main_thread_callbacks_.front()));
            pending_main_thread_callbacks_.pop_front();
        }
        if (callback_backlog_warned_ && pending_main_thread_callbacks_.size() < 256) {
            callback_backlog_warned_ = false;
        }
    }
    for (auto& callback : callbacks) {
        callback();
    }
}

std::size_t Phase1AssetManager::PendingMainThreadCallbacks() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    return pending_main_thread_callbacks_.size();
}

std::size_t Phase1AssetManager::PendingMainThreadCallbacksHighWatermark() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    return pending_callbacks_high_watermark_;
}
