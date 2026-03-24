#ifndef DSE_PHASE1_ASSET_MANAGER_H
#define DSE_PHASE1_ASSET_MANAGER_H

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

class TextureAsset {
public:
    TextureAsset(const std::string& path, unsigned int handle, int width, int height, int channels);
    ~TextureAsset();

    unsigned int GetHandle() const { return handle_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    int GetChannels() const { return channels_; }

private:
    std::string path_;
    unsigned int handle_;
    int width_;
    int height_;
    int channels_;
};

class ShaderAsset {
public:
    ShaderAsset(const std::string& name, unsigned int handle);
    ~ShaderAsset();

    unsigned int GetHandle() const { return handle_; }

private:
    std::string name_;
    unsigned int handle_;
};

enum class Phase1MaterialBlendMode {
    Alpha = 0,
    Additive = 1,
    Multiply = 2
};

class Phase1MaterialAsset {
public:
    Phase1MaterialAsset(unsigned int id, const std::string& name);

    unsigned int GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    const std::string& GetShaderVariant() const { return shader_variant_; }
    unsigned int GetTextureHandle() const { return texture_handle_; }
    const glm::vec4& GetTint() const { return tint_; }
    const glm::vec4& GetUvRect() const { return uv_rect_; }
    Phase1MaterialBlendMode GetBlendMode() const { return blend_mode_; }

    void SetName(const std::string& name) { name_ = name; }
    void SetShaderVariant(const std::string& shader_variant) { shader_variant_ = shader_variant; }
    void SetTextureHandle(unsigned int texture_handle) { texture_handle_ = texture_handle; }
    void SetTint(const glm::vec4& tint) { tint_ = tint; }
    void SetUvRect(const glm::vec4& uv_rect) { uv_rect_ = uv_rect; }
    void SetBlendMode(Phase1MaterialBlendMode blend_mode) { blend_mode_ = blend_mode; }

private:
    unsigned int id_ = 0;
    std::string name_;
    std::string shader_variant_ = "SPRITE_UNLIT";
    unsigned int texture_handle_ = 0;
    glm::vec4 tint_ = glm::vec4(1.0f);
    glm::vec4 uv_rect_ = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    Phase1MaterialBlendMode blend_mode_ = Phase1MaterialBlendMode::Alpha;
};

class AudioClipAsset {
public:
    AudioClipAsset(const std::string& path) : path_(path) {}
    ~AudioClipAsset() = default;

    const std::string& GetPath() const { return path_; }

private:
    std::string path_;
};

class Phase1AssetManager {
public:
    static Phase1AssetManager& Instance();
    void SetRhiDevice(RhiDevice* rhi_device);
    void ConfigureDataRoot(const std::string& data_root);
    std::string GetDataRoot();

    std::shared_ptr<TextureAsset> LoadTexture(const std::string& path);
    std::shared_ptr<ShaderAsset> LoadShader(const std::string& name, const std::string& vert_src, const std::string& frag_src);
    std::shared_ptr<AudioClipAsset> LoadAudioClip(const std::string& path);
    
    // Async load texture using JobSystem
    void LoadTextureAsync(const std::string& path, std::function<void(std::shared_ptr<TextureAsset>)> callback);
    void PumpMainThreadCallbacks(std::size_t max_callbacks = static_cast<std::size_t>(-1));
    std::size_t PendingMainThreadCallbacks();
    std::size_t PendingMainThreadCallbacksHighWatermark();
    std::shared_ptr<Phase1MaterialAsset> CreateMaterialInstance(const std::string& name);
    std::shared_ptr<Phase1MaterialAsset> GetMaterialInstance(unsigned int material_id);
    std::vector<unsigned int> ListMaterialInstanceIds();

    void UnloadUnused();

private:
    Phase1AssetManager() = default;
    ~Phase1AssetManager() = default;
    
    std::unordered_map<std::string, std::weak_ptr<TextureAsset>> textures_;
    std::unordered_map<std::string, std::weak_ptr<ShaderAsset>> shaders_;
    std::unordered_map<std::string, std::weak_ptr<AudioClipAsset>> audio_clips_;
    std::unordered_map<unsigned int, std::weak_ptr<Phase1MaterialAsset>> materials_;
    unsigned int next_texture_handle_ = 410000;
    unsigned int next_shader_handle_ = 420000;
    unsigned int next_material_id_ = 430000;
    std::string data_root_ = "data";
    std::mutex cache_mutex_;
    std::mutex callback_mutex_;
    std::mutex config_mutex_;
    RhiDevice* rhi_device_ = nullptr;
    std::deque<std::function<void()>> pending_main_thread_callbacks_;
    std::size_t pending_callbacks_high_watermark_ = 0;
    bool callback_backlog_warned_ = false;
};

#endif
