#ifndef DSE_PHASE1_ASSET_MANAGER_H
#define DSE_PHASE1_ASSET_MANAGER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

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

class Phase1AssetManager {
public:
    static Phase1AssetManager& Instance();

    std::shared_ptr<TextureAsset> LoadTexture(const std::string& path);
    std::shared_ptr<ShaderAsset> LoadShader(const std::string& name, const std::string& vert_src, const std::string& frag_src);
    
    // Async load texture using JobSystem
    void LoadTextureAsync(const std::string& path, std::function<void(std::shared_ptr<TextureAsset>)> callback);

    void UnloadUnused();

private:
    Phase1AssetManager() = default;
    ~Phase1AssetManager() = default;
    
    std::unordered_map<std::string, std::weak_ptr<TextureAsset>> textures_;
    std::unordered_map<std::string, std::weak_ptr<ShaderAsset>> shaders_;
    unsigned int next_texture_handle_ = 410000;
    unsigned int next_shader_handle_ = 420000;
};

#endif
