/**
 * @file asset_manager.h
 * @brief 资产管理器，负责加载、缓存和生命周期管理(如纹理、音频、预制体)
 */

#ifndef DSE_ASSET_MANAGER_H
#define DSE_ASSET_MANAGER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <vector>
#include <deque>
#include <cstddef>
#include <glm/glm.hpp>
class RhiDevice;

/**
 * @class TextureAsset
 * @brief 纹理资源封装，存储纹理在 RHI 层分配的句柄及尺寸信息
 */
class TextureAsset {
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

private:
    std::string path_;
    unsigned int handle_;
    int width_;
    int height_;
    int channels_;
};

/**
 * @class ShaderAsset
 * @brief 着色器资源封装，存储着色器程序在 RHI 层的句柄
 */
class ShaderAsset {
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
class MaterialAsset {
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
class AssetManager {
public:
    /**
     * @brief 注入 RhiDevice 实例，用于底层硬件资源的分配
     * @param rhi_device 渲染硬件接口实例
     */
    void SetRhiDevice(RhiDevice* rhi_device);
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

    // Async load texture using JobSystem
    /**
     * @brief 执行 LoadTextureAsync 操作
     * @param path 参数说明
     * @param std::function<void(std::shared_ptr<TextureAsset> 参数说明
     * @example
     * // AssetManager::LoadTextureAsync(...);
     */
    void LoadTextureAsync(const std::string& path, std::function<void(std::shared_ptr<TextureAsset>)> callback);
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

    AssetManager() = default;
    ~AssetManager() = default;

private:
    
    std::unordered_map<std::string, std::weak_ptr<TextureAsset>> textures_;
    std::unordered_map<std::string, std::weak_ptr<ShaderAsset>> shaders_;
    std::unordered_map<std::string, std::weak_ptr<AudioClipAsset>> audio_clips_;
    std::unordered_map<std::string, std::weak_ptr<DmeshAsset>> dmeshes_;
    std::unordered_map<std::string, std::weak_ptr<DanimAsset>> danims_;
    std::unordered_map<std::string, std::weak_ptr<DskelAsset>> dskels_;
    std::unordered_map<unsigned int, std::weak_ptr<MaterialAsset>> materials_;
    
    // Virtual File System for Bundles
    std::unordered_map<std::string, std::vector<uint8_t>> vfs_files_;

    unsigned int next_texture_handle_ = 410000;
    unsigned int next_shader_handle_ = 420000;
    unsigned int next_material_id_ = 430000;
    std::string data_root_ = "data";
    std::mutex cache_mutex_;
    std::mutex callback_mutex_;
    mutable std::mutex config_mutex_;
    RhiDevice* rhi_device_ = nullptr;
    std::deque<std::function<void()>> pending_main_thread_callbacks_;
    std::size_t pending_callbacks_high_watermark_ = 0;
    bool callback_backlog_warned_ = false;
};

#endif
