/**
 * @file asset_manager.h
 * @brief 资产管理器，负责加载、缓存和生命周期管理(如纹理、音频、预制体)
 */

#ifndef DSE_ASSET_MANAGER_H
#define DSE_ASSET_MANAGER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <mutex>
#include <vector>
#include <deque>
#include <cstddef>
#include <atomic>
#include <chrono>
#include <thread>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"
namespace dse::render { class RhiDevice; }
using dse::render::RhiDevice;
namespace dse::core {
class EventBus;
class JobSystem;
}
namespace dse::pak {
class PakReader;
}
namespace dse::assets {
class FileSystem;
}

/**
 * @class TextureAsset
 * @brief 纹理资源封装，存储纹理在 RHI 层分配的句柄及尺寸信息
 */
class DSE_EXPORT TextureAsset {
public:
    TextureAsset(const std::string& path, unsigned int handle, int width, int height, int channels);
    ~TextureAsset();

    /**
     * @brief 获取 RHI 纹理句柄
     * @return 纹理句柄(unsigned int)
     */
    unsigned int GetHandle() const { return handle_; }
    /**
     * @brief 获取纹理宽度
     * @return 宽度(像素)
     */
    int GetWidth() const { return width_; }
    /**
     * @brief 获取纹理高度
     * @return 高度(像素)
     */
    int GetHeight() const { return height_; }
    /**
     * @brief 获取纹理通道数
     * @return 通道数(例如 4 代表 RGBA)
     */
    int GetChannels() const { return channels_; }
    const std::string& GetPath() const { return path_; }

private:
    std::string path_;
    unsigned int handle_;
    int width_;
    int height_;
    int channels_;
};

/**
 * @class CubemapAsset
 * @brief 立方体贴图资源，当前用于 SkyboxComponent 的六面天空盒纹理。
 */
class DSE_EXPORT CubemapAsset {
public:
    CubemapAsset(const std::string& path, unsigned int handle, int width, int height);
    ~CubemapAsset();

    unsigned int GetHandle() const { return handle_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    const std::string& GetPath() const { return path_; }

private:
    std::string path_;
    unsigned int handle_;
    int width_;
    int height_;
};

/**
 * @class ShaderAsset
 * @brief 着色器资源封装，存储着色器程序在 RHI 层的句柄
 */
class DSE_EXPORT ShaderAsset {
public:
    ShaderAsset(const std::string& name, unsigned int handle);
    ~ShaderAsset();

    /**
     * @brief 获取 RHI 着色器句柄
     * @return 着色器句柄
     */
    unsigned int GetHandle() const { return handle_; }

private:
    std::string name_;
    unsigned int handle_;
};

/**
 * @class DmeshAsset
 * @brief 运行时 Mesh 资产，对应 .dmesh 二进制数据
 */
class DmeshAsset {
public:
    DmeshAsset(const std::string& path, std::vector<uint8_t> data) 
        : path_(path), data_(std::move(data)) {}
    
    const std::string& GetPath() const { return path_; }
    const std::vector<uint8_t>& GetData() const { return data_; }

private:
    std::string path_;
    std::vector<uint8_t> data_;
};

/**
 * @class DanimAsset
 * @brief 运行时动画资产，对应 .danim 二进制数据
 */
class DanimAsset {
public:
    DanimAsset(const std::string& path, std::vector<uint8_t> data) 
        : path_(path), data_(std::move(data)) {}
    
    const std::string& GetPath() const { return path_; }
    const std::vector<uint8_t>& GetData() const { return data_; }

private:
    std::string path_;
    std::vector<uint8_t> data_;
};

/**
 * @class DskelAsset
 * @brief 运行时骨架资产，对应 .dskel 二进制数据
 */
class DskelAsset {
public:
    DskelAsset(const std::string& path, std::vector<uint8_t> data) 
        : path_(path), data_(std::move(data)) {}
    
    const std::string& GetPath() const { return path_; }
    const std::vector<uint8_t>& GetData() const { return data_; }

private:
    std::string path_;
    std::vector<uint8_t> data_;
};

enum class MaterialBlendMode {
    Alpha = 0,
    Additive = 1,
    Multiply = 2,
    Opaque = 3
};

/**
 * @class MaterialAsset
 * @brief 材质资源，组合了着色器变体、纹理、颜色和混合模式
 */
class DSE_EXPORT MaterialAsset {
public:
    struct TextureSlots {
        unsigned int albedo = 0;
        unsigned int normal = 0;
        unsigned int metallic_roughness = 0;
        unsigned int emissive = 0;
        unsigned int occlusion = 0;
    };

    struct ScalarOverrides {
        float metallic = 0.0f;
        float roughness = 0.5f;
        float ao = 1.0f;
        float normal_strength = 1.0f;
        float alpha_cutoff = 0.5f;
        bool alpha_test = false;
        float sss_strength = 0.0f;
        glm::vec3 sss_tint = glm::vec3(0.0f);
        float clear_coat = 0.0f;
        float clear_coat_roughness = 0.1f;
        float anisotropy = 0.0f;
        float pom_height_scale = 0.0f;
    };

    struct RasterOverrides {
        bool double_sided = false;
    };

    MaterialAsset(unsigned int id, const std::string& name);

    unsigned int GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    const std::string& GetShaderVariant() const { return shader_variant_; }
    unsigned int GetTextureHandle() const { return texture_handle_; }
    const glm::vec4& GetTint() const { return tint_; }
    const glm::vec4& GetUvRect() const { return uv_rect_; }
    const glm::vec4& GetBaseColor() const { return base_color_; }
    const glm::vec3& GetEmissiveColor() const { return emissive_color_; }
    const TextureSlots& GetTextureSlots() const { return texture_slots_; }
    const ScalarOverrides& GetScalarOverrides() const { return scalar_overrides_; }
    const RasterOverrides& GetRasterOverrides() const { return raster_overrides_; }
    /**
     * @brief 获取混合模式
     * @return 材质当前的混合模式枚举
     */
    MaterialBlendMode GetBlendMode() const { return blend_mode_; }

    /**
     * @brief 设置材质名称
     * @param name 材质新名称
     */
    void SetName(const std::string& name) { name_ = name; }
    /**
     * @brief 设置着色器变体
     * @param shader_variant 变体名称
     */
    void SetShaderVariant(const std::string& shader_variant) { shader_variant_ = shader_variant; }
    /**
     * @brief 设置纹理句柄
     * @param texture_handle 新的纹理句柄
     */
    void SetTextureHandle(unsigned int texture_handle) { texture_handle_ = texture_handle; }
    /**
     * @brief 设置材质染色(Tint)
     * @param tint 颜色向量(RGBA)
     */
    void SetTint(const glm::vec4& tint) { tint_ = tint; }
    /**
     * @brief 设置纹理 UV 区域
     * @param uv_rect UV 矩形区域 (x, y, w, h)
     */
    void SetUvRect(const glm::vec4& uv_rect) { uv_rect_ = uv_rect; }
    void SetBaseColor(const glm::vec4& base_color) { base_color_ = base_color; }
    void SetEmissiveColor(const glm::vec3& emissive_color) { emissive_color_ = emissive_color; }
    void SetTextureSlots(const TextureSlots& texture_slots) { texture_slots_ = texture_slots; }
    void SetScalarOverrides(const ScalarOverrides& scalar_overrides) { scalar_overrides_ = scalar_overrides; }
    void SetRasterOverrides(const RasterOverrides& raster_overrides) { raster_overrides_ = raster_overrides; }
    /**
     * @brief 设置混合模式
     * @param blend_mode 混合模式枚举
     */
    void SetBlendMode(MaterialBlendMode blend_mode) { blend_mode_ = blend_mode; }

private:
    unsigned int id_ = 0;
    std::string name_;
    std::string shader_variant_ = "SPRITE_UNLIT";
    unsigned int texture_handle_ = 0;
    glm::vec4 tint_ = glm::vec4(1.0f);
    glm::vec4 uv_rect_ = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 base_color_ = glm::vec4(1.0f);
    glm::vec3 emissive_color_ = glm::vec3(0.0f);
    TextureSlots texture_slots_;
    ScalarOverrides scalar_overrides_;
    RasterOverrides raster_overrides_;
    MaterialBlendMode blend_mode_ = MaterialBlendMode::Alpha;
};

/**
 * @class AudioClipAsset
 * @brief 音频切片资源，封装音频文件的路径以供音频引擎加载
 */
class AudioClipAsset {
public:
    AudioClipAsset(const std::string& path, std::vector<uint8_t> data) : path_(path), data_(std::move(data)) {}
    ~AudioClipAsset() = default;

    /**
     * @brief 获取音频文件路径
     * @return 路径字符串
     */
    const std::string& GetPath() const { return path_; }

    const std::vector<uint8_t>& GetData() const { return data_; }

private:
    std::string path_;
    std::vector<uint8_t> data_;
};

/**
 * @class AssetManager
 * @brief 资产管理器，负责所有资源（纹理、材质、音频等）的统一加载、缓存和生命周期管理。
 */
class DSE_EXPORT AssetManager {
public:
    /**
     * @brief 注入 RhiDevice 实例，用于底层硬件资源的分配
     * @param rhi_device 渲染硬件接口实例
     */
    void SetRhiDevice(RhiDevice* rhi_device);
    RhiDevice* rhi_device() const { return rhi_device_; }
    void SetEventBus(dse::core::EventBus* event_bus);
    void SetJobSystem(dse::core::JobSystem* job_system);
    dse::core::EventBus* GetEventBus() const;
    dse::core::JobSystem* GetJobSystem() const;
    void SetFileSystem(dse::assets::FileSystem* file_system);
    dse::assets::FileSystem* GetFileSystem() const;
    /**
     * @brief 配置数据根目录
     * @param data_root 资源文件的基础路径
     */
    void ConfigureDataRoot(const std::string& data_root);
    /**
     * @brief 获取当前的数据根目录
     * @return 数据根目录路径
     */
    std::string GetDataRoot() const;

    /**
     * @brief 执行 LoadTexture 操作
     * @param path 参数说明
     * @return std::shared_ptr<TextureAsset> 返回值说明
     * @example
     * // AssetManager::LoadTexture(...);
     */
    std::shared_ptr<TextureAsset> LoadTexture(const std::string& path);
    std::string FindTexturePathByHandle(unsigned int handle) const;
    bool LoadImageRgba(const std::string& path, std::vector<unsigned char>& out_pixels, int& out_width, int& out_height, int& out_channels);
    /**
     * @brief 从目录加载六面天空盒立方体贴图。
     * @param directory_path 包含 px/nx/py/ny/pz/nz 六个同尺寸图片的目录
     * @return 成功时返回 CubemapAsset；任一面缺失、尺寸不一致或 RHI 未就绪时返回 nullptr
     */
    std::shared_ptr<CubemapAsset> LoadCubemapDirectory(const std::string& directory_path);
    /**
     * @brief 从单张全景图(equirectangular)加载天空盒立方体贴图。
     * @param image_path 等距柱状投影全景图路径 (宽高比 2:1 最佳, 如 .jpg/.png/.hdr)
     * @param face_size  生成的每面分辨率 (默认 512, 即 512×512×6)
     * @return 成功时返回 CubemapAsset；图片加载失败或 RHI 未就绪时返回 nullptr
     */
    std::shared_ptr<CubemapAsset> LoadCubemapPanorama(const std::string& image_path, int face_size = 512);
    /**
     * @brief 从水平十字展开图(horizontal cross)加载天空盒立方体贴图。
     * @param image_path 4×3 网格的十字展开图路径 (宽高比 4:3)
     * @return 成功时返回 CubemapAsset；图片加载失败或 RHI 未就绪时返回 nullptr
     */
    std::shared_ptr<CubemapAsset> LoadCubemapCross(const std::string& image_path);
    /**
     * @brief 智能加载天空盒: 目录→6面, 4:3文件→十字展开, 其他文件→全景图。
     * @param path 目录路径或单张图片路径
     * @return CubemapAsset 或 nullptr
     */
    std::shared_ptr<CubemapAsset> LoadCubemap(const std::string& path);
    /**
     * @brief 执行 LoadShader 操作
     * @param name 参数说明
     * @param vert_src 参数说明
     * @param frag_src 参数说明
     * @return std::shared_ptr<ShaderAsset> 返回值说明
     * @example
     * // AssetManager::LoadShader(...);
     */
    std::shared_ptr<ShaderAsset> LoadShader(const std::string& name, const std::string& vert_src, const std::string& frag_src);
    /**
     * @brief 执行 LoadAudioClip 操作
     * @param path 参数说明
     * @return std::shared_ptr<AudioClipAsset> 返回值说明
     * @example
     * // AssetManager::LoadAudioClip(...);
     */
    std::shared_ptr<AudioClipAsset> LoadAudioClip(const std::string& path);
    
    /**
     * @brief 加载 .dmesh 资产
     * @param path dmesh 文件路径
     */
    std::shared_ptr<DmeshAsset> LoadDmesh(const std::string& path);

    /**
     * @brief 加载 .danim 动画资产
     * @param path danim 文件路径
     */
    std::shared_ptr<DanimAsset> LoadDanim(const std::string& path);

    /**
     * @brief 加载 .dskel 骨架资产
     * @param path dskel 文件路径
     */
    std::shared_ptr<DskelAsset> LoadDskel(const std::string& path);
    
    // Asset Bundle API
    /**
     * @brief Pack a directory into an encrypted asset bundle (.bun)
     * @param input_dir Directory containing assets to pack
     * @param output_bundle Output bundle file path
     * @param aes_key 16-byte AES encryption key (empty for no encryption)
     * @return true if successful
     */
    bool PackBundle(const std::string& input_dir, const std::string& output_bundle, const std::string& aes_key);

    /**
     * @brief Mount an asset bundle to the VFS
     * @param bundle_path Path to the .bun file
     * @param aes_key 16-byte AES encryption key used during packing
     * @return true if successful
     */
    bool MountBundle(const std::string& bundle_path, const std::string& aes_key);

    /**
     * @brief 将资源路径规范化为相对于 data root 的逻辑路径
     * @param path 原始路径，可为逻辑路径、带 data/ 前缀路径或绝对路径
     * @return 规范化后的逻辑路径；失败时返回空字符串
     */
    std::string NormalizeAssetPath(const std::string& path) const;

    /**
     * @brief 解析资源到磁盘绝对/相对可访问路径
     * @param path 原始路径，可为逻辑路径、带 data/ 前缀路径或绝对路径
     * @return 可访问的磁盘路径；失败时返回空字符串
     */
    std::string ResolveAssetPath(const std::string& path) const;

    /**
     * @brief Load a file into memory, checking mounted bundles first, then disk
     */
    bool LoadFileToMemory(const std::string& path, std::vector<uint8_t>& out_data);

    // Async load using JobSystem (disk I/O on worker, GPU upload on main thread via PumpMainThreadCallbacks)
    /**
     * @brief 异步加载纹理，磁盘 IO 在工作线程完成，GPU 上传在主线程通过 PumpMainThreadCallbacks 调度
     * @param path 纹理文件路径
     * @param callback 加载完成后的回调，在主线程执行
     */
    void LoadTextureAsync(const std::string& path, std::function<void(std::shared_ptr<TextureAsset>)> callback);
    /**
     * @brief 异步加载 .dmesh 资产
     * @param path dmesh 文件路径
     * @param callback 加载完成后的回调，在主线程执行
     */
    void LoadDmeshAsync(const std::string& path, std::function<void(std::shared_ptr<DmeshAsset>)> callback);
    /**
     * @brief 异步加载 .danim 动画资产
     * @param path danim 文件路径
     * @param callback 加载完成后的回调，在主线程执行
     */
    void LoadDanimAsync(const std::string& path, std::function<void(std::shared_ptr<DanimAsset>)> callback);
    /**
     * @brief 异步加载 .dskel 骨架资产
     * @param path dskel 文件路径
     * @param callback 加载完成后的回调，在主线程执行
     */
    void LoadDskelAsync(const std::string& path, std::function<void(std::shared_ptr<DskelAsset>)> callback);
    /**
     * @brief 异步加载音频切片资产
     * @param path 音频文件路径
     * @param callback 加载完成后的回调，在主线程执行
     */
    void LoadAudioClipAsync(const std::string& path, std::function<void(std::shared_ptr<AudioClipAsset>)> callback);
    /**
     * @brief 异步加载 .dmat 材质资产
     * @param dmat_path dmat 文件路径
     * @param material_index 材质索引
     * @param callback 加载完成后的回调，在主线程执行
     */
    void LoadMaterialAsync(const std::string& dmat_path, std::size_t material_index, std::function<void(std::shared_ptr<MaterialAsset>)> callback);
    /**
     * @brief 执行 PumpMainThreadCallbacks 操作
     * @param static_cast<std::size_t>(-1 参数说明
     */
    void PumpMainThreadCallbacks(std::size_t max_callbacks = static_cast<std::size_t>(-1));
    /**
     * @brief 执行 PendingMainThreadCallbacks 操作
     * @return std::size_t 返回值说明
     */
    std::size_t PendingMainThreadCallbacks();
    /**
     * @brief 执行 PendingMainThreadCallbacksHighWatermark 操作
     * @return std::size_t 返回值说明
     */
    std::size_t PendingMainThreadCallbacksHighWatermark();
    /**
     * @brief 执行 CreateMaterialInstance 操作
     * @param name 参数说明
     * @return std::shared_ptr<MaterialAsset> 返回值说明
     * @example
     * // AssetManager::CreateMaterialInstance(...);
     */
    std::shared_ptr<MaterialAsset> CreateMaterialInstance(const std::string& name);
    std::shared_ptr<MaterialAsset> LoadMaterialInstanceFromDmat(const std::string& dmat_path, std::size_t material_index = 0);
    /**
     * @brief 执行 GetMaterialInstance 操作
     * @param material_id 参数说明
     * @return std::shared_ptr<MaterialAsset> 返回值说明
     */
    std::shared_ptr<MaterialAsset> GetMaterialInstance(unsigned int material_id);
    std::vector<unsigned int> ListMaterialInstanceIds();

    /**
     * @brief 执行 UnloadUnused 操作
     */
    void UnloadUnused();
    void ReleaseGpuResources();

    // --- LRU 淘汰与内存预算 ---
    /**
     * @brief 设置资源内存预算（字节），超出时按 LRU 淘汰最久未访问的资源
     * @param budget_bytes 预算字节数，0 表示不限制
     */
    void SetMemoryBudget(std::size_t budget_bytes);
    /**
     * @brief 获取当前追踪的资源内存占用估算（字节）
     */
    std::size_t EstimatedMemoryUsage() const;
    /**
     * @brief 按 LRU 策略淘汰资源，直到内存占用降至预算以内
     * @return 本次淘汰的资源数量
     */
    std::size_t EvictLRU();

    // --- 热重载（文件监听）---
    /**
     * @brief 启动文件监听线程，监听 data root 目录的文件变更
     */
    void StartFileWatcher();
    /**
     * @brief 停止文件监听线程
     */
    void StopFileWatcher();
    /**
     * @brief 在主线程中处理待热重载的资源（应在 PumpMainThreadCallbacks 附近调用）
     * @return 本次重载的资源数量
     */
    std::size_t PumpHotReloads();

    AssetManager();
    ~AssetManager();

    /// Mount a .dpak archive. Assets found in pak take priority over loose files.
    bool MountPak(const std::string& pak_path);

    /// Unmount all pak archives.
    void UnmountAllPaks();

    /// Check if any pak is mounted.
    bool HasMountedPak() const;

private:
    
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> textures_;
    std::unordered_map<std::string, std::weak_ptr<CubemapAsset>> cubemaps_;
    std::unordered_map<std::string, std::weak_ptr<ShaderAsset>> shaders_;
    std::unordered_set<unsigned int> gpu_texture_handles_;
    std::unordered_set<unsigned int> gpu_cubemap_handles_;
    std::unordered_set<unsigned int> gpu_shader_handles_;
    std::unordered_map<std::string, std::weak_ptr<AudioClipAsset>> audio_clips_;
    std::unordered_map<std::string, std::weak_ptr<DmeshAsset>> dmeshes_;
    std::unordered_map<std::string, std::weak_ptr<DanimAsset>> danims_;
    std::unordered_map<std::string, std::weak_ptr<DskelAsset>> dskels_;
    std::unordered_map<unsigned int, std::weak_ptr<MaterialAsset>> materials_;
    std::unordered_map<unsigned int, std::string> material_names_;
    
    // Virtual File System for Bundles
    std::unordered_map<std::string, std::vector<uint8_t>> vfs_files_;

    unsigned int next_texture_handle_ = 410000;
    unsigned int next_shader_handle_ = 420000;
    unsigned int next_material_id_ = 430000;
    std::string data_root_ = "data";
    mutable std::mutex cache_mutex_;
    std::mutex callback_mutex_;
    mutable std::mutex config_mutex_;
    RhiDevice* rhi_device_ = nullptr;
    dse::core::EventBus* event_bus_ = nullptr;
    dse::core::JobSystem* job_system_ = nullptr;
    dse::assets::FileSystem* file_system_ = nullptr;
    std::deque<std::function<void()>> pending_main_thread_callbacks_;
    std::size_t pending_callbacks_high_watermark_ = 0;
    bool callback_backlog_warned_ = false;

    // --- LRU 淘汰 ---
    struct LruEntry {
        std::string cache_key;
        std::size_t estimated_bytes = 0;
        std::chrono::steady_clock::time_point last_access;
    };
    std::unordered_map<std::string, LruEntry> lru_entries_;
    std::size_t memory_budget_bytes_ = 0;
    std::size_t estimated_memory_usage_ = 0;
    void TouchLru(const std::string& cache_key, std::size_t estimated_bytes);
    void RemoveLru(const std::string& cache_key);

    // --- 热重载 ---
    std::thread file_watcher_thread_;
    std::atomic<bool> file_watcher_running_{false};
    std::mutex hot_reload_mutex_;
    std::vector<std::string> pending_hot_reloads_;
    void FileWatcherLoop();

    // --- Pak archives ---
    std::vector<std::shared_ptr<dse::pak::PakReader>> mounted_paks_;
    bool ReadFromPak(const std::string& relative_path, std::vector<uint8_t>& out_data) const;
};

#endif
