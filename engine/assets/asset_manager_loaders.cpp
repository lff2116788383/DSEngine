/**
 * @file asset_manager_loaders.cpp
 * @brief AssetManager asset loading methods (texture, cubemap, shader, audio, mesh).
 */

#include "engine/assets/asset_manager.h"
#include "engine/assets/dds_parser.h"
#include "engine/assets/dtex.h"
#include "engine/assets/bundle_packer.h"
#include "engine/assets/pak_reader.h"
#include "engine/assets/native_file_system.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"
#include "engine/core/job_system.h"
#include "engine/core/event_bus.h"
#include "engine/core/memory/memory.h"
#include <utility>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <cstdint>
#include <rapidjson/document.h>
#include "bundle/bundle.h"
#include <stb/stb_image.h>

namespace {
std::string NormalizePath(const std::string& path) {
    std::filesystem::path p(path);
    return p.make_preferred().lexically_normal().string();
}
} // anonymous namespace

std::shared_ptr<TextureAsset> AssetManager::LoadTexture(const std::string& path) {
    return LoadTexture(path, TextureSamplerDesc{});
}

std::shared_ptr<TextureAsset> AssetManager::LoadTexture(const std::string& path,
                                                        const TextureSamplerDesc& sampler) {
    const std::string logical_path = NormalizeAssetPath(path);
    const std::string resolved_path = ResolveAssetPath(path);
    const std::string base_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(path) : NormalizePath(resolved_path)) : logical_path;
    // é‡‡æ ·æè¿°å¹¶å…¥ç¼“å­˜é”®ï¼šåŒä¸€å›¾ä»¥ä¸åŒ filter/wrap åŠ è½½åº”å¾—åˆ°å„è‡ªçš„ GPU çº¹ç†ã€‚
    // é»˜è®¤ {Linear, Repeat} ä¸åŠ åŽç¼€ï¼Œä¿æŒæ—§ç¼“å­˜é”®ä¸å˜ï¼ˆå‘åŽå…¼å®¹ï¼‰ã€‚
    const bool default_sampler = (sampler.filter == TextureFilter::Linear && sampler.wrap == TextureWrap::Repeat);
    const std::string cache_key = default_sampler
        ? base_key
        : base_key + "|s=" + std::to_string(static_cast<int>(sampler.filter))
                   + std::to_string(static_cast<int>(sampler.wrap));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = textures_.find(cache_key);
        if (it != textures_.end()) {
            return it->second;
        }
    }

    std::vector<uint8_t> file_data;
    if (!LoadFileToMemory(path, file_data)) {
        DEBUG_LOG_ERROR("Failed to read texture file: {}", path);
        return nullptr;
    }

    const bool is_dds = dse::assets::HasDdsExtension(path);
    const bool is_dtex = dse::assets::HasDtexExtension(path);
    if (is_dds || is_dtex) {
        CompressedTextureFormat fmt;
        std::vector<CompressedMipLevel> mips;
        int comp_w, comp_h;
        const bool parsed = is_dds
            ? dse::assets::ParseDds(file_data, fmt, mips, comp_w, comp_h)
            : dse::assets::ParseDtex(file_data, fmt, mips, comp_w, comp_h);
        if (!parsed) {
            DEBUG_LOG_ERROR("Failed to parse compressed texture file: {}", path);
            return nullptr;
        }

        unsigned int handle = 0;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            if (rhi_device_) {
                handle = rhi_device_->CreateCompressedTexture2D(fmt, mips, sampler.filter == TextureFilter::Linear);
            }
        }
        if (handle == 0) {
            DEBUG_LOG_ERROR("Failed to create compressed texture via RHI: {}", path);
            return nullptr;
        }

        auto tex = std::make_shared<TextureAsset>(path, handle, comp_w, comp_h, 4);
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            textures_[cache_key] = tex;
            gpu_texture_handles_.insert(handle);
        }
        size_t total = 0;
        for (auto& m : mips) total += m.size;
        TouchLru(cache_key, total);
        return tex;
    }

    int width, height, channels;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        stbi_set_flip_vertically_on_load(rhi_device_ ? rhi_device_->NeedsTextureYFlip() : true);
    }
    unsigned char* data = stbi_load_from_memory(file_data.data(), file_data.size(), &width, &height, &channels, 4);
    
    if (!data) {
        DEBUG_LOG_ERROR("Failed to decode texture: {}", path);
        return nullptr;
    }

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTexture2D(width, height, data, sampler);
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
        gpu_texture_handles_.insert(handle);
    }
    TouchLru(cache_key, static_cast<std::size_t>(width) * height * 4u);
    return tex;
}

std::string AssetManager::FindTexturePathByHandle(unsigned int handle) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    for (const auto& [key, tex] : textures_) {
        if (tex && tex->GetHandle() == handle) {
            return tex->GetPath();
        }
    }
    return {};
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
        gpu_cubemap_handles_.insert(handle);
    }
    TouchLru(cache_key, static_cast<std::size_t>(width) * height * 4u * 6u);
    return cubemap;
}

std::shared_ptr<CubemapAsset> AssetManager::LoadCubemapPanorama(const std::string& image_path, int face_size) {
    const std::string logical_path = NormalizeAssetPath(image_path);
    const std::string resolved_path = ResolveAssetPath(image_path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(image_path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cubemaps_.find(cache_key);
        if (it != cubemaps_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    // Load the equirectangular panorama image
    std::vector<unsigned char> pano_pixels;
    int pano_w = 0, pano_h = 0, pano_ch = 0;
    if (!LoadImageRgba(image_path, pano_pixels, pano_w, pano_h, pano_ch)) {
        DEBUG_LOG_ERROR("Failed to load panorama image: {}", image_path);
        return nullptr;
    }

    if (face_size <= 0) face_size = 512;

    // Allocate 6 faces
    const size_t face_bytes = static_cast<size_t>(face_size) * face_size * 4;
    std::vector<std::vector<unsigned char>> faces(6);
    for (auto& f : faces) f.resize(face_bytes);

    // Equirectangular â†’ cubemap conversion (CPU)
    // Face order: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    const float half = static_cast<float>(face_size) * 0.5f;
    const float inv_half = 1.0f / half;
    const float pi = 3.14159265358979f;
    const float two_pi = 2.0f * pi;

    for (int face = 0; face < 6; ++face) {
        unsigned char* dst = faces[face].data();
        for (int y = 0; y < face_size; ++y) {
            for (int x = 0; x < face_size; ++x) {
                // Normalized face coordinates [-1, +1]
                const float fx = (static_cast<float>(x) + 0.5f) * inv_half - 1.0f;
                const float fy = 1.0f - (static_cast<float>(y) + 0.5f) * inv_half;

                // Compute 3D direction for this face/pixel
                float dx = 0.0f, dy = 0.0f, dz = 0.0f;
                switch (face) {
                    case 0: dx =  1.0f; dy = fy; dz = -fx; break; // +X
                    case 1: dx = -1.0f; dy = fy; dz =  fx; break; // -X
                    case 2: dx =  fx;   dy =  1.0f; dz = -fy; break; // +Y
                    case 3: dx =  fx;   dy = -1.0f; dz =  fy; break; // -Y
                    case 4: dx =  fx;   dy = fy; dz =  1.0f; break; // +Z
                    case 5: dx = -fx;   dy = fy; dz = -1.0f; break; // -Z
                }

                // Direction â†’ spherical â†’ equirectangular UV
                const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
                const float nx = dx / len;
                const float ny = dy / len;
                const float nz = dz / len;

                // theta: azimuth angle, phi: elevation angle
                const float theta = std::atan2(nz, nx); // [-pi, pi]
                const float phi = std::asin(std::clamp(ny, -1.0f, 1.0f)); // [-pi/2, pi/2]

                // UV in equirectangular image
                float u = (theta + pi) / two_pi; // [0, 1]
                float v = 0.5f - phi / pi;       // [0, 1], top=0

                // Bilinear sample from panorama
                const float px_f = u * static_cast<float>(pano_w) - 0.5f;
                const float py_f = v * static_cast<float>(pano_h) - 0.5f;
                const int px0 = static_cast<int>(std::floor(px_f));
                const int py0 = static_cast<int>(std::floor(py_f));
                const float fx_frac = px_f - static_cast<float>(px0);
                const float fy_frac = py_f - static_cast<float>(py0);

                auto sample = [&](int sx, int sy) -> const unsigned char* {
                    sx = ((sx % pano_w) + pano_w) % pano_w;
                    sy = std::clamp(sy, 0, pano_h - 1);
                    return &pano_pixels[(static_cast<size_t>(sy) * pano_w + sx) * 4];
                };

                const unsigned char* s00 = sample(px0, py0);
                const unsigned char* s10 = sample(px0 + 1, py0);
                const unsigned char* s01 = sample(px0, py0 + 1);
                const unsigned char* s11 = sample(px0 + 1, py0 + 1);

                const size_t dst_offset = (static_cast<size_t>(y) * face_size + x) * 4;
                for (int c = 0; c < 4; ++c) {
                    const float top = static_cast<float>(s00[c]) * (1.0f - fx_frac) + static_cast<float>(s10[c]) * fx_frac;
                    const float bot = static_cast<float>(s01[c]) * (1.0f - fx_frac) + static_cast<float>(s11[c]) * fx_frac;
                    const float val = top * (1.0f - fy_frac) + bot * fy_frac;
                    dst[dst_offset + c] = static_cast<unsigned char>(std::clamp(val + 0.5f, 0.0f, 255.0f));
                }
            }
        }
    }

    const unsigned char* face_ptrs[6] = {
        faces[0].data(), faces[1].data(), faces[2].data(),
        faces[3].data(), faces[4].data(), faces[5].data()
    };

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTextureCube(face_size, face_size, face_ptrs, true);
        }
    }

    if (handle == 0) {
        DEBUG_LOG_ERROR("Failed to create cubemap from panorama via RHI: {}", image_path);
        return nullptr;
    }

    auto cubemap = std::make_shared<CubemapAsset>(image_path, handle, face_size, face_size);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cubemaps_[cache_key] = cubemap;
        gpu_cubemap_handles_.insert(handle);
    }
    TouchLru(cache_key, static_cast<std::size_t>(face_size) * face_size * 4u * 6u);
    DEBUG_LOG_INFO("Loaded panorama skybox: {} ({}x{} per face)", image_path, face_size, face_size);
    return cubemap;
}

std::shared_ptr<CubemapAsset> AssetManager::LoadCubemapCross(const std::string& image_path) {
    const std::string logical_path = NormalizeAssetPath(image_path);
    const std::string resolved_path = ResolveAssetPath(image_path);
    const std::string cache_key = logical_path.empty() ? (resolved_path.empty() ? NormalizePath(image_path) : NormalizePath(resolved_path)) : logical_path;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cubemaps_.find(cache_key);
        if (it != cubemaps_.end()) {
            if (auto shared = it->second.lock()) {
                return shared;
            }
        }
    }

    // Load the cross layout image
    std::vector<unsigned char> cross_pixels;
    int cross_w = 0, cross_h = 0, cross_ch = 0;
    if (!LoadImageRgba(image_path, cross_pixels, cross_w, cross_h, cross_ch)) {
        DEBUG_LOG_ERROR("Failed to load cross layout image: {}", image_path);
        return nullptr;
    }

    // Horizontal cross: 4 columns Ã— 3 rows
    const int face_w = cross_w / 4;
    const int face_h = cross_h / 3;
    if (face_w <= 0 || face_h <= 0) {
        DEBUG_LOG_ERROR("Cross layout image too small: {}x{}", cross_w, cross_h);
        return nullptr;
    }

    // Extract 6 faces from the horizontal cross layout:
    //   Row 0:          [top]
    //   Row 1: [right] [back] [left] [front]
    //   Row 2:          [bottom]
    // OpenGL cubemap face order: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    // KF layout (from mesh_manager.cpp UV mapping):
    //   col=0,row=1 â†’ right  (+X)
    //   col=1,row=1 â†’ back   (-Z)
    //   col=2,row=1 â†’ left   (-X)
    //   col=3,row=1 â†’ front  (+Z)
    //   col=1,row=0 â†’ top    (+Y)
    //   col=1,row=2 â†’ bottom (-Y)
    struct FaceRegion { int col; int row; };
    const FaceRegion regions[6] = {
        {0, 1}, // +X (right)
        {2, 1}, // -X (left)
        {1, 0}, // +Y (top)
        {1, 2}, // -Y (bottom)
        {3, 1}, // +Z (front)
        {1, 1}, // -Z (back)
    };

    const size_t face_bytes = static_cast<size_t>(face_w) * face_h * 4;
    std::vector<std::vector<unsigned char>> faces(6);
    for (int face = 0; face < 6; ++face) {
        faces[face].resize(face_bytes);
        const int src_x0 = regions[face].col * face_w;
        const int src_y0 = regions[face].row * face_h;
        // +Y (face 2) and -Y (face 3) need 180Â° rotation to match OpenGL cubemap convention
        const bool rotate_180 = (face == 2 || face == 3);
        for (int y = 0; y < face_h; ++y) {
            const size_t src_offset = (static_cast<size_t>(src_y0 + y) * cross_w + src_x0) * 4;
            if (!rotate_180) {
                const size_t dst_offset = static_cast<size_t>(y) * face_w * 4;
                std::memcpy(faces[face].data() + dst_offset, cross_pixels.data() + src_offset, static_cast<size_t>(face_w) * 4);
            } else {
                // 180Â° rotation: dst(x,y) = src(w-1-x, h-1-y)
                const int dst_y = face_h - 1 - y;
                for (int x = 0; x < face_w; ++x) {
                    const int dst_x = face_w - 1 - x;
                    const size_t si = src_offset + static_cast<size_t>(x) * 4;
                    const size_t di = (static_cast<size_t>(dst_y) * face_w + dst_x) * 4;
                    std::memcpy(faces[face].data() + di, cross_pixels.data() + si, 4);
                }
            }
        }
    }

    const unsigned char* face_ptrs[6] = {
        faces[0].data(), faces[1].data(), faces[2].data(),
        faces[3].data(), faces[4].data(), faces[5].data()
    };

    unsigned int handle = 0;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (rhi_device_) {
            handle = rhi_device_->CreateTextureCube(face_w, face_h, face_ptrs, true);
        }
    }

    if (handle == 0) {
        DEBUG_LOG_ERROR("Failed to create cubemap from cross layout via RHI: {}", image_path);
        return nullptr;
    }

    auto cubemap = std::make_shared<CubemapAsset>(image_path, handle, face_w, face_h);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cubemaps_[cache_key] = cubemap;
        gpu_cubemap_handles_.insert(handle);
    }
    TouchLru(cache_key, face_bytes * 6u);
    DEBUG_LOG_INFO("Loaded cross layout skybox: {} ({}x{} per face)", image_path, face_w, face_h);
    return cubemap;
}

std::shared_ptr<CubemapAsset> AssetManager::LoadCubemap(const std::string& path) {
    const std::string resolved = ResolveAssetPath(path);
    const std::string& check_path = resolved.empty() ? path : resolved;

    // If path is a directory â†’ 6-face loading
    if (std::filesystem::exists(check_path) && std::filesystem::is_directory(check_path)) {
        return LoadCubemapDirectory(path);
    }
    // If path is a file â†’ detect layout by aspect ratio
    if (std::filesystem::exists(check_path) && std::filesystem::is_regular_file(check_path)) {
        // Probe image dimensions to detect cross layout (4:3) vs panorama (2:1)
        std::vector<uint8_t> file_data;
        int probe_w = 0, probe_h = 0, probe_ch = 0;
        if (LoadFileToMemory(check_path, file_data)) {
            stbi_info_from_memory(file_data.data(), static_cast<int>(file_data.size()), &probe_w, &probe_h, &probe_ch);
        }
        // Horizontal cross: width/height â‰ˆ 4/3 (1.33), tolerance 1.2~1.5
        if (probe_w > 0 && probe_h > 0) {
            const float aspect = static_cast<float>(probe_w) / static_cast<float>(probe_h);
            if (aspect >= 1.2f && aspect <= 1.5f) {
                return LoadCubemapCross(path);
            }
        }
        return LoadCubemapPanorama(path);
    }

    DEBUG_LOG_ERROR("Cubemap path not found (neither directory nor file): {}", path);
    return nullptr;
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
        gpu_shader_handles_.insert(handle);
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

    const std::size_t clip_bytes = file_data.size();
    auto clip = std::make_shared<AudioClipAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        audio_clips_[cache_key] = clip;
    }
    TouchLru(cache_key, clip_bytes);
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

    const std::size_t dmesh_bytes = file_data.size();
    auto dmesh = std::make_shared<DmeshAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        dmeshes_[cache_key] = dmesh;
    }
    TouchLru(cache_key, dmesh_bytes);
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

    const std::size_t danim_bytes = file_data.size();
    auto danim = std::make_shared<DanimAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        danims_[cache_key] = danim;
    }
    TouchLru(cache_key, danim_bytes);
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

    const std::size_t dskel_bytes = file_data.size();
    auto dskel = std::make_shared<DskelAsset>(load_path, std::move(file_data));
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        dskels_[cache_key] = dskel;
    }
    TouchLru(cache_key, dskel_bytes);
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
            auto shared = it->second;
            if (callback) callback(shared);
            PublishResourceLoaded(GetEventBus(), cache_key, true);
            return;
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
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            stbi_set_flip_vertically_on_load(rhi_device_ ? rhi_device_->NeedsTextureYFlip() : true);
        }
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

