/**
 * @file asset_manager.cpp
 * @brief 资产管理器，负责加载、缓存和生命周期管理(如纹理、音频、预制体)
 */

#include "engine/assets/asset_manager.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include "engine/core/job_system.h"
#include "engine/core/event_bus.h"
#include <utility>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <rapidjson/document.h>
#include "bundle/bundle.h"
extern "C" {
#include "aes.h"
}

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace {
std::string NormalizePath(const std::string& path) {
    std::filesystem::path p(path);
    return p.make_preferred().lexically_normal().string();
}

std::string ResolveAssetPathImpl(const std::string& path, const std::string& data_root) {
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

AssetManager::AssetManager() = default;

AssetManager::~AssetManager() {
    // 在成员容器析构前主动释放，避免静态 CRT Debug 堆在 DLL 卸载/测试进程退出阶段
    // 再处理跨模块分配过的 STL 节点时触发 debug_heap 链表断言。
    ReleaseGpuResources();
    {
        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        pending_main_thread_callbacks_.clear();
    }
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        textures_.clear();
        cubemaps_.clear();
        shaders_.clear();
        audio_clips_.clear();
        dmeshes_.clear();
        danims_.clear();
        dskels_.clear();
        materials_.clear();
        vfs_files_.clear();
    }
}

TextureAsset::TextureAsset(const std::string& path, unsigned int handle, int width, int height, int channels)
    : path_(path), handle_(handle), width_(width), height_(height), channels_(channels) {
}

TextureAsset::~TextureAsset() {
}

CubemapAsset::CubemapAsset(const std::string& path, unsigned int handle, int width, int height)
    : path_(path), handle_(handle), width_(width), height_(height) {
}

CubemapAsset::~CubemapAsset() {
}

ShaderAsset::ShaderAsset(const std::string& name, unsigned int handle)
    : name_(name), handle_(handle) {
}

ShaderAsset::~ShaderAsset() {
}

MaterialAsset::MaterialAsset(unsigned int id, const std::string& name)
    : id_(id), name_(name) {
    if (name.find("mesh") != std::string::npos || name.find("pbr") != std::string::npos) {
        shader_variant_ = "MESH_PBR";
        blend_mode_ = MaterialBlendMode::Opaque;
    }
}

void AssetManager::SetRhiDevice(RhiDevice* rhi_device) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    rhi_device_ = rhi_device;
}

void AssetManager::SetEventBus(dse::core::EventBus* event_bus) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    event_bus_ = event_bus;
}

void AssetManager::SetJobSystem(dse::core::JobSystem* job_system) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    job_system_ = job_system;
}

dse::core::EventBus* AssetManager::GetEventBus() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return event_bus_;
}

dse::core::JobSystem* AssetManager::GetJobSystem() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return job_system_;
}

void AssetManager::ConfigureDataRoot(const std::string& data_root) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    if (data_root.empty()) {
        return;
    }
    data_root_ = NormalizePath(data_root);
}

std::string AssetManager::GetDataRoot() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return data_root_;
}

std::string AssetManager::NormalizeAssetPath(const std::string& path) const {
    if (path.empty()) {
        return "";
    }

    std::filesystem::path normalized = std::filesystem::path(path).lexically_normal();
    if (normalized.empty()) {
        return "";
    }

    const std::filesystem::path data_root = GetDataRoot();
    if (normalized.is_absolute()) {
        if (!data_root.empty()) {
            const std::filesystem::path normalized_data_root = std::filesystem::path(data_root).lexically_normal();
            std::error_code ec;
            const std::filesystem::path relative = normalized.lexically_relative(normalized_data_root);
            const std::wstring relative_native = relative.native();
            if (!relative.empty() && relative_native.rfind(L"..", 0) != 0) {
                std::string logical = relative.generic_string();
                return logical == "." ? "" : logical;
            }
        }
        return normalized.generic_string();
    }

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

    if (starts_with(normalized, data_prefix)) {
        normalized = normalized.lexically_relative(data_prefix);
    } else if (starts_with(normalized, bin_data_prefix)) {
        normalized = normalized.lexically_relative(bin_data_prefix);
    }

    const std::string logical = normalized.generic_string();
    return logical == "." ? "" : logical;
}

std::string AssetManager::ResolveAssetPath(const std::string& path) const {
    return ResolveAssetPathImpl(path, GetDataRoot());
}

bool AssetManager::PackBundle(const std::string& input_dir, const std::string& output_bundle, const std::string& aes_key) {
    if (!std::filesystem::exists(input_dir)) return false;
    bundle::archive pak;
    int idx = 0;
    for (auto const& entry : std::filesystem::recursive_directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary | std::ios::ate);
            if (!file) continue;
            std::streamsize size = file.tellg();
            if (size < 0) {
                continue;
            }
            file.seekg(0, std::ios::beg);
            std::string content(static_cast<std::size_t>(size), '\0');
            if (size == 0 || file.read(content.data(), size)) {
                pak.resize(idx + 1);
                std::string rel_path = std::filesystem::relative(entry.path(), input_dir).generic_string();
                std::replace(rel_path.begin(), rel_path.end(), '\\', '/');
                pak[idx]["name"] = rel_path;
                pak[idx]["data"] = content;
                idx++;
            }
        }
    }
    
    std::string bin = pak.zip(60); // level 60
    
    if (!aes_key.empty() && aes_key.size() >= 16) {
        struct AES_ctx ctx;
        uint8_t iv[16] = {0}; // Fixed IV for simplicity
        AES_init_ctx_iv(&ctx, (const uint8_t*)aes_key.c_str(), iv);
        AES_CTR_xcrypt_buffer(&ctx, (uint8_t*)bin.data(), bin.size());
    }
    
    std::ofstream out(output_bundle, std::ios::binary);
    if (!out) return false;
    out.write(bin.data(), bin.size());
    return true;
}

bool AssetManager::MountBundle(const std::string& bundle_path, const std::string& aes_key) {
    std::ifstream file(bundle_path, std::ios::binary | std::ios::ate);
    if (!file) {
        DEBUG_LOG_ERROR("Failed to open bundle: {}", bundle_path);
        return false;
    }
    std::streamsize size = file.tellg();
    if (size < 0) {
        DEBUG_LOG_ERROR("Failed to query bundle size: {}", bundle_path);
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::string bin(static_cast<std::size_t>(size), '\0');
    if (size > 0 && !file.read(bin.data(), size)) return false;
    
    if (!aes_key.empty() && aes_key.size() >= 16) {
        struct AES_ctx ctx;
        uint8_t iv[16] = {0};
        AES_init_ctx_iv(&ctx, (const uint8_t*)aes_key.c_str(), iv);
        AES_CTR_xcrypt_buffer(&ctx, (uint8_t*)bin.data(), bin.size());
    }
    
    bundle::archive pak;
    pak.zip(bin);
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    for (size_t i = 0; i < pak.size(); ++i) {
        const std::string& name = pak[i]["name"];
        const std::string& data = pak[i]["data"];
        vfs_files_[name] = std::vector<uint8_t>(data.begin(), data.end());
    }
    DEBUG_LOG_INFO("Mounted bundle: {} with {} files", bundle_path, pak.size());
    return true;
}

bool AssetManager::LoadFileToMemory(const std::string& path, std::vector<uint8_t>& out_data) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string search_path = resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path);

    // Convert to generic relative path for VFS check
    std::string vfs_key = logical_path.empty() ? search_path : logical_path;
    std::replace(vfs_key.begin(), vfs_key.end(), '\\', '/');
    
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = vfs_files_.find(vfs_key);
        if (it != vfs_files_.end()) {
            out_data = it->second;
            return true;
        }
    }
    
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    std::ifstream file(load_path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    std::streamsize size = file.tellg();
    if (size < 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    out_data.resize(static_cast<std::size_t>(size));
    if (size == 0 || file.read(reinterpret_cast<char*>(out_data.data()), size)) {
        return true;
    }
    return false;
}


std::shared_ptr<TextureAsset> AssetManager::LoadTexture(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = textures_.find(cache_key);
        if (it != textures_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read texture file: {}", path);
        return nullptr;
    }

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, 4);
    
    if (!data) {
        DEBUG_LOG_ERROR("Failed to decode texture: {}", path);
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
        DEBUG_LOG_ERROR("Failed to create texture via RHI: {}", path);
        return nullptr;
    }

    stbi_image_free(data);

    auto tex = std::make_shared<TextureAsset>(path, handle, width, height, channels);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        textures_[cache_key] = tex;
    }
    return tex;
}

std::shared_ptr<CubemapAsset> AssetManager::LoadCubemapDirectory(const std::string& directory_path) {
    const std::string logical_path = NormalizeAssetPath(directory_path);
    const std::string resolved_path = ResolveAssetPath(directory_path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(directory_path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cubemaps_.find(cache_key);
        if (it != cubemaps_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    const std::string base_path = resolved_path.empty() ? directory_path : resolved_path;
    const std::filesystem::path base_dir = std::filesystem::path(base_path);
    if (!std::filesystem::exists(base_dir) || !std::filesystem::is_directory(base_dir)) {
        DEBUG_LOG_ERROR("Cubemap directory does not exist: {}", directory_path);
        return nullptr;
    }

    const char* face_names[6] = {"px", "nx", "py", "ny", "pz", "nz"};
    const char* extensions[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".ppm"};
    std::vector<std::string> face_paths(6);
    for (int face = 0; face < 6; ++face) {
        for (const char* ext : extensions) {
            const std::filesystem::path candidate = base_dir / (std::string(face_names[face]) + ext);
            if (std::filesystem::exists(candidate)) {
                face_paths[face] = candidate.string();
                break;
            }
        }
        if (face_paths[face].empty()) {
            DEBUG_LOG_ERROR("Cubemap face is missing: {} ({})", directory_path, face_names[face]);
            return nullptr;
        }
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char*> face_pixels(6, nullptr);
    auto cleanup_faces = [&]() {
        for (auto* pixels : face_pixels) {
            if (pixels) {
                stbi_image_free(pixels);
            }
        }
    };

    stbi_set_flip_vertically_on_load(false);
    for (int face = 0; face < 6; ++face) {
        std::vector<uint8_t> file_data;
        if (!LoadFileToMemory(face_paths[face], file_data)) {
            DEBUG_LOG_ERROR("Failed to read cubemap face: {}", face_paths[face]);
            cleanup_faces();
            return nullptr;
        }

        int face_width = 0;
        int face_height = 0;
        int face_channels = 0;
        face_pixels[face] = stbi_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &face_width, &face_height, &face_channels, 4);
        if (!face_pixels[face]) {
            DEBUG_LOG_ERROR("Failed to decode cubemap face: {}", face_paths[face]);
            cleanup_faces();
            return nullptr;
        }

        if (face == 0) {
            width = face_width;
            height = face_height;
            channels = face_channels;
        } else if (face_width != width || face_height != height) {
            DEBUG_LOG_ERROR("Cubemap face size mismatch in directory: {}", directory_path);
            cleanup_faces();
            return nullptr;
        }
    }

    const unsigned char* face_ptrs[6] = {
        face_pixels[0], face_pixels[1], face_pixels[2], face_pixels[3], face_pixels[4], face_pixels[5]
    };

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTextureCube(width, height, face_ptrs, true);
        }
    }

    cleanup_faces();

    if (handle == 0) {
        DEBUG_LOG_ERROR("Failed to create cubemap via RHI: {}", directory_path);
        return nullptr;
    }

    auto cubemap = std::make_shared<CubemapAsset>(base_dir.generic_string(), handle, width, height);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cubemaps_[cache_key] = cubemap;
    }
    return cubemap;
}

std::shared_ptr<ShaderAsset> AssetManager::LoadShader(const std::string& name, const std::string& vert_src, const std::string& frag_src) {
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

std::shared_ptr<AudioClipAsset> AssetManager::LoadAudioClip(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    const std::string cache_key = logical_path.empty() ? NormalizePath(load_path) : logical_path;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = audio_clips_.find(cache_key);
        if (it != audio_clips_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read audio file: {}", path);
        return nullptr;
    }

    auto clip = std::make_shared<AudioClipAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        audio_clips_[cache_key] = clip;
    }
    return clip;
}

std::shared_ptr<DmeshAsset> AssetManager::LoadDmesh(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    const std::string cache_key = logical_path.empty() ? NormalizePath(load_path) : logical_path;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = dmeshes_.find(cache_key);
        if (it != dmeshes_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read dmesh file: {}", path);
        return nullptr;
    }

    auto dmesh = std::make_shared<DmeshAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        dmeshes_[cache_key] = dmesh;
    }
    return dmesh;
}

std::shared_ptr<DanimAsset> AssetManager::LoadDanim(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    const std::string cache_key = logical_path.empty() ? NormalizePath(load_path) : logical_path;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = danims_.find(cache_key);
        if (it != danims_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read danim file: {}", path);
        return nullptr;
    }

    auto danim = std::make_shared<DanimAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        danims_[cache_key] = danim;
    }
    return danim;
}

std::shared_ptr<DskelAsset> AssetManager::LoadDskel(const std::string& path) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string load_path = resolved_path.empty() ? path : resolved_path;
    const std::string cache_key = logical_path.empty() ? NormalizePath(load_path) : logical_path;

    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = dskels_.find(cache_key);
        if (it != dskels_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read dskel file: {}", path);
        return nullptr;
    }

    auto dskel = std::make_shared<DskelAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        dskels_[cache_key] = dskel;
    }
    return dskel;
}

namespace {
void PublishResourceLoaded(dse::core::EventBus* event_bus, const std::string& path, bool success) {
    if (!event_bus) {
        return;
    }
    event_bus->Publish<dse::core::ResourceLoadedEvent>(path, success);
}
}

void AssetManager::LoadTextureAsync(const std::string& path, std::function<void(std::shared_ptr<TextureAsset>)> callback) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = textures_.find(cache_key);
        if (it != textures_.end()) {
            if (auto shared = it->second.lock()) {
                if (callback) callback(shared);
                PublishResourceLoaded(GetEventBus(), cache_key, true);
                return;
            }
        }
    }

    dse::core::EventBus* event_bus = GetEventBus();
    dse::core::JobSystem* job_system = GetJobSystem();
    auto worker = [this, path, resolved_path, cache_key, callback, event_bus]() {
        std::vector<uint8_t> file_data;
        if (!LoadFileToMemory(path, file_data)) {
            DEBUG_LOG_ERROR("Failed to read texture file async: {}", path);
            if (callback) {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                pending_main_thread_callbacks_.push_back([callback]() { callback(nullptr); });
            }
            PublishResourceLoaded(event_bus, cache_key, false);
            return;
        }

        int width, height, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, 4);
        
        if (!data) {
            DEBUG_LOG_ERROR("Failed to async load texture: {}, resolved: {}", path, resolved_path);
            if (callback) {
                std::lock_guard<std::mutex> callback_lock(callback_mutex_);
                pending_main_thread_callbacks_.push_back([callback]() {
                    callback(nullptr);
                });
                pending_main_thread_callbacks_.push_back([resolved_path, event_bus]() {
                    PublishResourceLoaded(event_bus, resolved_path, false);
                });
                pending_callbacks_high_watermark_ = std::max(pending_callbacks_high_watermark_, pending_main_thread_callbacks_.size());
                if (!callback_backlog_warned_ && pending_main_thread_callbacks_.size() >= 1024) {
                    callback_backlog_warned_ = true;
                    DEBUG_LOG_WARN("Async callback backlog is high: {}", pending_main_thread_callbacks_.size());
                }
            }
            return;
        }

        std::lock_guard<std::mutex> callback_lock(callback_mutex_);
        pending_main_thread_callbacks_.push_back([this, callback, cache_key, resolved_path, width, height, channels, data, event_bus]() {
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
                PublishResourceLoaded(event_bus, resolved_path, false);
                return;
            }
            auto tex = std::make_shared<TextureAsset>(resolved_path, handle, width, height, channels);
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                textures_[cache_key] = tex;
            }
            if (callback) {
                callback(tex);
            }
            PublishResourceLoaded(event_bus, resolved_path, true);
        });
        pending_callbacks_high_watermark_ = std::max(pending_callbacks_high_watermark_, pending_main_thread_callbacks_.size());
        if (!callback_backlog_warned_ && pending_main_thread_callbacks_.size() >= 1024) {
            callback_backlog_warned_ = true;
            DEBUG_LOG_WARN("Async callback backlog is high: {}", pending_main_thread_callbacks_.size());
        }
    };

    if (job_system) {
        job_system->Execute(worker);
    } else {
        worker();
    }
}

std::shared_ptr<MaterialAsset> AssetManager::CreateMaterialInstance(const std::string& name) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    unsigned int material_id = next_material_id_++;
    auto material = std::make_shared<MaterialAsset>(material_id, name);
    materials_[material_id] = material;
    return material;
}

std::shared_ptr<MaterialAsset> AssetManager::LoadMaterialInstanceFromDmat(const std::string& dmat_path, std::size_t material_index) {
    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(dmat_path, file_data)) {
        return nullptr;
    }

    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(file_data.data()), file_data.size());
    if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("materials") || !doc["materials"].IsArray()) {
        return nullptr;
    }

    const auto& materials = doc["materials"];
    if (material_index >= materials.Size()) {
        return nullptr;
    }

    const auto& mat = materials[static_cast<rapidjson::SizeType>(material_index)];
    if (!mat.IsObject()) {
        return nullptr;
    }

    const std::string name = (mat.HasMember("name") && mat["name"].IsString())
        ? mat["name"].GetString()
        : (std::filesystem::path(dmat_path).stem().string() + "_" + std::to_string(material_index));
    auto material = CreateMaterialInstance(name);
    if (!material) {
        return nullptr;
    }

    if (mat.HasMember("base_color") && mat["base_color"].IsArray() && mat["base_color"].Size() >= 4) {
        material->SetBaseColor(glm::vec4(
            mat["base_color"][0].GetFloat(),
            mat["base_color"][1].GetFloat(),
            mat["base_color"][2].GetFloat(),
            mat["base_color"][3].GetFloat()));
    }
    if (mat.HasMember("emissive") && mat["emissive"].IsArray() && mat["emissive"].Size() >= 3) {
        material->SetEmissiveColor(glm::vec3(
            mat["emissive"][0].GetFloat(),
            mat["emissive"][1].GetFloat(),
            mat["emissive"][2].GetFloat()));
    }
    MaterialAsset::RasterOverrides raster = material->GetRasterOverrides();
    if (mat.HasMember("double_sided") && mat["double_sided"].IsBool()) {
        raster.double_sided = mat["double_sided"].GetBool();
    }
    material->SetRasterOverrides(raster);
    material->SetBlendMode(MaterialBlendMode::Opaque);

    MaterialAsset::ScalarOverrides scalars = material->GetScalarOverrides();
    if (mat.HasMember("metallic") && mat["metallic"].IsNumber()) {
        scalars.metallic = mat["metallic"].GetFloat();
    }
    if (mat.HasMember("roughness") && mat["roughness"].IsNumber()) {
        scalars.roughness = mat["roughness"].GetFloat();
    }
    if (mat.HasMember("occlusion_strength") && mat["occlusion_strength"].IsNumber()) {
        scalars.ao = mat["occlusion_strength"].GetFloat();
    }
    if (mat.HasMember("normal_scale") && mat["normal_scale"].IsNumber()) {
        scalars.normal_strength = mat["normal_scale"].GetFloat();
    }
    if (mat.HasMember("alpha_cutoff") && mat["alpha_cutoff"].IsNumber()) {
        scalars.alpha_cutoff = mat["alpha_cutoff"].GetFloat();
    }
    if (mat.HasMember("alpha_test") && mat["alpha_test"].IsBool()) {
        scalars.alpha_test = mat["alpha_test"].GetBool();
    }
    material->SetScalarOverrides(scalars);

    MaterialAsset::TextureSlots slots = material->GetTextureSlots();
    auto try_load_texture = [this](const rapidjson::Value& object, const char* key) -> unsigned int {
        if (!object.HasMember(key) || !object[key].IsString()) {
            return 0;
        }
        auto texture = LoadTexture(object[key].GetString());
        return texture ? texture->GetHandle() : 0;
    };
    slots.albedo = try_load_texture(mat, "base_color_texture");
    slots.normal = try_load_texture(mat, "normal_texture");
    slots.metallic_roughness = try_load_texture(mat, "metallic_roughness_texture");
    slots.emissive = try_load_texture(mat, "emissive_texture");
    slots.occlusion = try_load_texture(mat, "occlusion_texture");
    material->SetTextureSlots(slots);
    if (slots.albedo != 0) {
        material->SetTextureHandle(slots.albedo);
    }

    return material;
}

std::shared_ptr<MaterialAsset> AssetManager::GetMaterialInstance(unsigned int material_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = materials_.find(material_id);
    if (it == materials_.end()) {
        return nullptr;
    }
    return it->second.lock();
}

std::vector<unsigned int> AssetManager::ListMaterialInstanceIds() {
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

void AssetManager::UnloadUnused() {
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
    for (auto it = audio_clips_.begin(); it != audio_clips_.end(); ) {
        if (it->second.expired()) {
            it = audio_clips_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = dmeshes_.begin(); it != dmeshes_.end(); ) {
        if (it->second.expired()) {
            it = dmeshes_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = danims_.begin(); it != danims_.end(); ) {
        if (it->second.expired()) {
            it = danims_.erase(it);
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

void AssetManager::ReleaseGpuResources() {
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);

    RhiDevice* device = nullptr;
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        device = rhi_device_;
    }

    if (!device) {
        textures_.clear();
        cubemaps_.clear();
        shaders_.clear();
        materials_.clear();
        return;
    }

    for (auto it = textures_.begin(); it != textures_.end(); ++it) {
        if (auto texture = it->second.lock()) {
            const unsigned int handle = texture->GetHandle();
            if (handle != 0) {
                device->DeleteTexture(handle);
            }
        }
    }
    textures_.clear();

    for (auto it = cubemaps_.begin(); it != cubemaps_.end(); ++it) {
        if (auto cubemap = it->second.lock()) {
            const unsigned int handle = cubemap->GetHandle();
            if (handle != 0) {
                device->DeleteTexture(handle);
            }
        }
    }
    cubemaps_.clear();

    for (auto it = shaders_.begin(); it != shaders_.end(); ++it) {
        if (auto shader = it->second.lock()) {
            const unsigned int handle = shader->GetHandle();
            if (handle != 0) {
                device->DeleteShaderProgram(handle);
            }
        }
    }
    shaders_.clear();
    materials_.clear();
}

void AssetManager::PumpMainThreadCallbacks(std::size_t max_callbacks) {
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

std::size_t AssetManager::PendingMainThreadCallbacks() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    return pending_main_thread_callbacks_.size();
}

std::size_t AssetManager::PendingMainThreadCallbacksHighWatermark() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    return pending_callbacks_high_watermark_;
}
